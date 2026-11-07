// Bus DSI et dalle EK79007 (1024 x 600, 2 voies) de la Waveshare 7B.
//
// Les valeurs (minutages, debit de ligne, table d'initialisation) sont dans include/ws7b_pur.h avec leur provenance ; ce fichier
// les transmet au composant Espressif esp_lcd_ek79007 2.0.2. Sequence, calquee sur le BSP Waveshare 3.0.1 (bsp_display_new_with_handles)
// et sur le firmware ESPHome en service :
//   1. alimenter la PHY DSI (LDO canal 3, 2,5 V) ;  2. bus DSI ;  3. canal DBI (commandes DCS) ;
//   4. dalle EK79007 (commande B2 « 2 voies » emise par le composant, puis WS7B_EK79007_INIT) ;
//   5. reset materiel de la dalle par GPIO33 (esp_lcd_panel_reset) ;  6. initialisation (esp_lcd_panel_init).
//
// Detection du controleur a l'execution : non faite, et c'est voulu. La 7B n'a qu'un seul modele de dalle, et une lecture
// d'identifiant DCS (retour de bus DSI) n'a jamais ete essayee sur cette carte. Le controleur est donc annonce « EK79007 » par
// ws7b_board.c, ce qui est un FAIT DE CONCEPTION et non une mesure.
#include "esp_check.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"

#include "ws7b_internal.h"

static const char *TAG = "ws7b_display";

static esp_ldo_channel_handle_t s_phy_ldo;

// La table du composant doit etre `static` et hors fonction (en-tete esp_lcd_ek79007.h) : on la remplit une fois depuis
// WS7B_EK79007_INIT, la table testee sur le PC. Les arguments (un octet au plus) sont gardes dans un tableau a part.
static ek79007_lcd_init_cmd_t s_init_cmds[WS7B_EK79007_INIT_COUNT];
static uint8_t s_init_args[WS7B_EK79007_INIT_COUNT];

static void build_init_table(void) {
    for (size_t i = 0; i < WS7B_EK79007_INIT_COUNT; i++) {
        const ws7b_dcs_t *src = &WS7B_EK79007_INIT[i];
        s_init_args[i] = src->arg0;
        s_init_cmds[i].cmd = src->cmd;
        s_init_cmds[i].data = (src->nargs > 0) ? (const void *)&s_init_args[i] : NULL;
        s_init_cmds[i].data_bytes = src->nargs;
        s_init_cmds[i].delay_ms = src->delay_ms;
    }
}

esp_err_t ws7b_display_create(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io) {
    build_init_table();

    // 1. Alimentation de la PHY DSI (sans elle le bus ne demarre pas).
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = WS7B_DSI_PHY_LDO_CHANNEL,
        .voltage_mv = WS7B_DSI_PHY_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &s_phy_ldo), TAG, "alimentation de la PHY DSI");

    // 2. Bus DSI.
    esp_lcd_dsi_bus_config_t bus_cfg = {0};
    bus_cfg.bus_id = 0;
    bus_cfg.num_data_lanes = WS7B_DSI_LANES;
    // 0 = le pilote choisit selon la revision MINIMALE de puce du build (XTAL des v3.0, PLL_F20M avant). Ne pas ecrire
    // MIPI_DSI_PHY_CLK_SRC_DEFAULT : c'est PLL_F20M, « reserve aux puces < v3.0 » ; sur la 7B (puce v3.2, build >= v3.1)
    // le pilote fait abort() dans mipi_dsi_ll_set_phy_pllref_clock_source. Constate sur la carte le 07/11/2026 : 11 demarrages
    // sur 11 plantes la, juste apres « apres la NVS et les reglages » (docs/mesures/7b-materiel.md).
    bus_cfg.phy_clk_src = 0;
    bus_cfg.lane_bit_rate_mbps = WS7B_DSI_LANE_MBPS;
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus), TAG, "bus DSI");

    // 3. Canal DBI : les commandes DCS (init, veille).
    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, io), TAG, "E/S DBI");

    // 4. Dalle. Minutages : ws7b_pur.h section 2 (macro EK79007_1024_600_PANEL_60HZ_CONFIG_CF du composant).
    esp_lcd_dpi_panel_config_t dpi = {0};
    dpi.virtual_channel = 0;
    dpi.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi.dpi_clock_freq_mhz = WS7B_DPI_CLOCK_MHZ;
    dpi.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi.num_fbs =
        1; // un seul tampon d'image : LVGL dessine par bandes et la dalle est mise a jour par copie (comme le Tab5)
    dpi.video_timing.h_size = WS7B_LCD_H_RES;
    dpi.video_timing.v_size = WS7B_LCD_V_RES;
    dpi.video_timing.hsync_pulse_width = WS7B_DPI_HSYNC_PULSE;
    dpi.video_timing.hsync_back_porch = WS7B_DPI_HSYNC_BACK_PORCH;
    dpi.video_timing.hsync_front_porch = WS7B_DPI_HSYNC_FRONT_PORCH;
    dpi.video_timing.vsync_pulse_width = WS7B_DPI_VSYNC_PULSE;
    dpi.video_timing.vsync_back_porch = WS7B_DPI_VSYNC_BACK_PORCH;
    dpi.video_timing.vsync_front_porch = WS7B_DPI_VSYNC_FRONT_PORCH;
    dpi.flags.use_dma2d = true; // copie des tampons LVGL vers le tampon d'image par DMA2D

    ek79007_vendor_config_t vendor = {0};
    vendor.init_cmds = s_init_cmds;
    vendor.init_cmds_size = (uint16_t)WS7B_EK79007_INIT_COUNT;
    vendor.mipi_config.dsi_bus = dsi_bus;
    vendor.mipi_config.dpi_config = &dpi;
    vendor.mipi_config.lane_num = WS7B_DSI_LANES;

    esp_lcd_panel_dev_config_t panel_cfg = {0};
    panel_cfg.reset_gpio_num = WS7B_PIN_LCD_RST; // reset materiel fait par le composant (esp_lcd_panel_reset)
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.bits_per_pixel = WS7B_RGB565_BITS_PER_PIXEL;
    panel_cfg.vendor_config = &vendor;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ek79007(*io, &panel_cfg, panel), TAG, "esp_lcd_new_panel_ek79007");
    // 5. et 6. Reset puis initialisation. Le composant n'a pas de commande « display on » (0x29) : la dalle sort du reset
    // allumee (comme dans le BSP Waveshare, qui ne l'envoie jamais) ; 0x29 n'est envoye qu'au reveil, par ws7b_io.c.
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel), TAG, "reset de la dalle");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), TAG, "initialisation de la dalle");

    ESP_LOGI(TAG, "EK79007 : DSI %d voies a %d Mbit/s, pixels a %d MHz (%u mHz de rafraichissement), %dx%d RGB565",
             WS7B_DSI_LANES, WS7B_DSI_LANE_MBPS, WS7B_DPI_CLOCK_MHZ, (unsigned)ws7b_refresh_mhz(), WS7B_LCD_H_RES,
             WS7B_LCD_V_RES);
    return ESP_OK;
}
