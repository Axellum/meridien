// Bus DSI et dalle du Tab5. Seul le ST7121 est pris en charge en phase 1a (voir docs/mesures/1a.md, "Contrôleurs").
//
// Valeurs du ST7121 et leur provenance (lues le 06/11/2026) :
//   - horloge de pixels 70 MHz et marges 40/2/40 (horizontal), 24/20/200 (vertical) : composant esp_lcd_st7121 1.0.1
//     (macro ST7121_1280_720_PANEL_60HZ_DPI_CONFIG_CF), BSP esp-bsp m5stack_tab5 1.3.2, espp, ESPHome (M5STACK-TAB5-ST7121) :
//     quatre sources concordantes ;
//   - debit de ligne DSI : 965 Mbit/s dans le BSP esp-bsp ET dans le modele ESPHome qui fait tourner cette tablette ; 900 dans espp
//     (valeur de M5GFX) ; 1300 dans la macro ST7121_PANEL_BUS_DSI_2CH_CONFIG du composant. Choix : 965 (la valeur qui tourne
//     sur cette tablette). Les deux autres sont a essayer si l'ecran reste noir ;
//   - sequence d'initialisation : celle par defaut du composant esp_lcd_st7121 (identique a celle du BSP esp-bsp, sauf les
//     temporisations SLPOUT 120 ms / DISPON 0 ms au lieu de 80 / 800 ms).
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_st7121.h"
#include "esp_log.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_display";

#define ST7121_DSI_LANE_BITRATE_MBPS 965
#define ST7121_DPI_CLOCK_MHZ 70

static esp_ldo_channel_handle_t s_phy_ldo;

static esp_err_t create_st7121(esp_lcd_dsi_bus_handle_t dsi_bus, esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t *panel)
{
    esp_lcd_dpi_panel_config_t dpi = {0};
    dpi.virtual_channel = 0;
    dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi.dpi_clock_freq_mhz = ST7121_DPI_CLOCK_MHZ;
    dpi.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi.num_fbs = 1;  // un seul tampon d'image : LVGL dessine dans des tampons partiels et la dalle est mise a jour par copie
    dpi.video_timing.h_size = TAB5_LCD_H_RES;
    dpi.video_timing.v_size = TAB5_LCD_V_RES;
    dpi.video_timing.hsync_back_porch = 40;
    dpi.video_timing.hsync_pulse_width = 2;
    dpi.video_timing.hsync_front_porch = 40;
    dpi.video_timing.vsync_back_porch = 24;
    dpi.video_timing.vsync_pulse_width = 20;
    dpi.video_timing.vsync_front_porch = 200;
    dpi.flags.use_dma2d = true;  // copie des tampons LVGL vers le tampon d'image par DMA2D (comme le BSP d'Espressif)

    st7121_vendor_config_t vendor = {0};
    vendor.init_cmds = NULL;  // sequence par defaut du composant
    vendor.init_cmds_size = 0;
    vendor.mipi_config.dsi_bus = dsi_bus;
    vendor.mipi_config.dpi_config = &dpi;

    esp_lcd_panel_dev_config_t panel_cfg = {0};
    panel_cfg.reset_gpio_num = -1;  // reset materiel fait par l'expandeur (tab5_lcd_reset_pulse) ; ici reset logiciel (SWRESET)
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.bits_per_pixel = 16;
    panel_cfg.vendor_config = &vendor;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7121(io, &panel_cfg, panel), TAG, "esp_lcd_new_panel_st7121");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel), TAG, "reset de la dalle");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), TAG, "initialisation de la dalle");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(*panel, false), TAG, "inversion");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(*panel, false, false), TAG, "miroir (orientation native)");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*panel, true), TAG, "affichage");
    return ESP_OK;
}

esp_err_t tab5_display_create(tab5_display_controller_t controller, esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io)
{
    if (controller != TAB5_DISPLAY_ST7121) {
        // Detecte, mais pas encore porte : aucun exemplaire a essayer en phase 1a (le ST7123 est en SAV, aucun ILI9881C).
        ESP_LOGE(TAG, "contrôleur %s détecté mais pas pris en charge dans cette version (seul le ST7121 l'est)",
                 tab5_display_controller_name(controller));
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = TAB5_DSI_PHY_LDO_CHANNEL,
        .voltage_mv = TAB5_DSI_PHY_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &s_phy_ldo), TAG, "alimentation de la PHY DSI");

    esp_lcd_dsi_bus_config_t bus_cfg = {0};
    bus_cfg.bus_id = 0;
    bus_cfg.num_data_lanes = TAB5_DSI_LANES;
    bus_cfg.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
    bus_cfg.lane_bit_rate_mbps = ST7121_DSI_LANE_BITRATE_MBPS;
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus), TAG, "bus DSI");

    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, io), TAG, "E/S DBI");

    ESP_LOGI(TAG, "ST7121 : DSI %d voies à %d Mbit/s, pixels à %d MHz, %dx%d RGB565", TAB5_DSI_LANES,
             ST7121_DSI_LANE_BITRATE_MBPS, ST7121_DPI_CLOCK_MHZ, TAB5_LCD_H_RES, TAB5_LCD_V_RES);
    return create_st7121(dsi_bus, *io, panel);
}
