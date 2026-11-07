// Tests de la partie PURE de la couche carte de la Waveshare 7B (components/bsp_waveshare_7b/include/ws7b_pur.h) : broches, minutages
// de la dalle, table d'initialisation de l'EK79007, rotation d'un point tactile, rapport cyclique du retroeclairage, plan
// d'extinction de la dalle, courbe de volume de l'ES8311.
//
// Ce qui est teste ICI est ce qui TOURNE sur la carte : les fichiers ws7b_*.c incluent le meme en-tete (voir son en-tete). Ce que ces
// tests ne prouvent PAS : que les valeurs sont celles de la carte (elles viennent de sources lues, listees dans l'en-tete, et ne
// sont « constatees » qu'une fois essayees sur la 7B : docs/mesures/7b-*.md), ni qu'un son ou une image sortent.
#include <cstddef>
#include <cstdint>

#include "mini.hpp"
#include "ws7b_pur.h"

namespace {

// ---- broches ------------------------------------------------------------------------------------------------------------------

constexpr int kBrochesUtilisees[] = {
    WS7B_PIN_I2C_SDA,    WS7B_PIN_I2C_SCL,     WS7B_PIN_I2S_DOUT,    WS7B_PIN_I2S_LRCK,   WS7B_PIN_I2S_DIN,
    WS7B_PIN_I2S_BCLK,   WS7B_PIN_I2S_MCLK,    WS7B_PIN_C6_SDIO_D0,  WS7B_PIN_C6_SDIO_D1, WS7B_PIN_C6_SDIO_D2,
    WS7B_PIN_C6_SDIO_D3, WS7B_PIN_C6_SDIO_CLK, WS7B_PIN_C6_SDIO_CMD, WS7B_PIN_TOUCH_RST,  WS7B_PIN_BACKLIGHT,
    WS7B_PIN_LCD_RST,    WS7B_PIN_AMP_CTRL,    WS7B_PIN_C6_RESET,
};
// Broches qui existent sur la carte et que le BSP ne doit jamais prendre.
constexpr int kBrochesInterdites[] = {WS7B_PIN_USB_DM,   WS7B_PIN_USB_DP, WS7B_PIN_UART0_TX,
                                      WS7B_PIN_UART0_RX, WS7B_PIN_BOOT,   WS7B_PIN_SD_PWR};
constexpr int kNbUtilisees = sizeof(kBrochesUtilisees) / sizeof(kBrochesUtilisees[0]);
constexpr int kNbInterdites = sizeof(kBrochesInterdites) / sizeof(kBrochesInterdites[0]);
constexpr int kGpioMax = 54; // l'ESP32-P4 a les GPIO 0 a 54

constexpr int broches_valides_et_distinctes() {
    for (int i = 0; i < kNbUtilisees; i++) {
        VERIFIER(kBrochesUtilisees[i] >= 0 && kBrochesUtilisees[i] <= kGpioMax);
        for (int j = i + 1; j < kNbUtilisees; j++) {
            VERIFIER(kBrochesUtilisees[i] != kBrochesUtilisees[j]); // deux fonctions sur une meme broche
        }
    }
    return 0;
}

constexpr int aucune_broche_reservee_n_est_prise() {
    for (int i = 0; i < kNbUtilisees; i++) {
        for (int j = 0; j < kNbInterdites; j++) {
            VERIFIER(kBrochesUtilisees[i] != kBrochesInterdites[j]);
        }
    }
    // Broches de demarrage (« strapping ») du P4 : GPIO34 a 38. Le BSP n'en utilise aucune.
    for (int i = 0; i < kNbUtilisees; i++) {
        VERIFIER(kBrochesUtilisees[i] < 34 || kBrochesUtilisees[i] > 38);
    }
    return 0;
}

constexpr int sdio_du_c6_sur_la_fente_1() {
    // Fente 1 du P4 : D0..D3 consecutives de 14 a 17, puis CLK 18 et CMD 19. Le C6 se remet a zero par GPIO54.
    VERIFIER(WS7B_PIN_C6_SDIO_D0 == 14 && WS7B_PIN_C6_SDIO_D1 == 15 && WS7B_PIN_C6_SDIO_D2 == 16 &&
             WS7B_PIN_C6_SDIO_D3 == 17);
    VERIFIER(WS7B_PIN_C6_SDIO_CLK == 18 && WS7B_PIN_C6_SDIO_CMD == 19);
    VERIFIER(WS7B_PIN_C6_RESET == 54);
    return 0;
}

constexpr int i2c_et_i2s_valeurs_connues() {
    VERIFIER(WS7B_PIN_I2C_SDA == 7 && WS7B_PIN_I2C_SCL == 8);
    VERIFIER(WS7B_PIN_I2S_MCLK == 13 && WS7B_PIN_I2S_BCLK == 12 && WS7B_PIN_I2S_LRCK == 10);
    VERIFIER(WS7B_PIN_I2S_DOUT == 9 && WS7B_PIN_I2S_DIN == 11);
    VERIFIER(WS7B_PIN_BACKLIGHT == 32 && WS7B_PIN_LCD_RST == 33 && WS7B_PIN_AMP_CTRL == 53 && WS7B_PIN_TOUCH_RST == 23);
    VERIFIER(WS7B_TOUCH_INT_CONNECTED == 0); // INT du GT911 non relie au P4 : le tactile est interroge
    VERIFIER(WS7B_ES8311_I2C_ADDR == 0x18 && WS7B_ES7210_I2C_ADDR == 0x40);
    VERIFIER(WS7B_GT911_ADDR == 0x5D && WS7B_GT911_ADDR_BACKUP == 0x14);
    return 0;
}

// ---- dalle --------------------------------------------------------------------------------------------------------------------

constexpr int cadence_de_rafraichissement_proche_de_60_hz() {
    const uint32_t mhz = ws7b_refresh_mhz();
    VERIFIER(mhz > 59000 && mhz < 61000); // 52 MHz / (1354 x 636) = 60,38 Hz
    return 0;
}

constexpr int les_voies_dsi_ont_de_la_marge() {
    VERIFIER(ws7b_pixel_bps() == 832000000ULL);          // 52 MHz x 16 bits
    VERIFIER(ws7b_dsi_bps() == 1800000000ULL);           // 2 voies x 900 Mbit/s
    VERIFIER(ws7b_dsi_bps() > ws7b_pixel_bps() * 3 / 2); // au moins 50 % de marge sur le debit des pixels
    return 0;
}

constexpr int geometrie_de_la_dalle() {
    VERIFIER(WS7B_LCD_H_RES == 1024 && WS7B_LCD_V_RES == 600);
    VERIFIER(WS7B_LCD_H_RES > WS7B_LCD_V_RES); // paysage natif
    VERIFIER(WS7B_DSI_LANES == 2);
    VERIFIER(WS7B_DSI_PHY_LDO_CHANNEL == 3 && WS7B_DSI_PHY_LDO_MV == 2500);
    return 0;
}

constexpr int table_d_initialisation_de_l_ek79007() {
    VERIFIER(WS7B_EK79007_INIT_COUNT == 8);
    // 0x80 a 0x86, chacun avec son octet, puis « sleep out » (0x11) suivi de 120 ms.
    constexpr uint8_t valeurs[7] = {0x8B, 0x78, 0x84, 0x88, 0xA8, 0xE3, 0x88};
    for (int i = 0; i < 7; i++) {
        VERIFIER(WS7B_EK79007_INIT[i].cmd == 0x80 + i);
        VERIFIER(WS7B_EK79007_INIT[i].nargs == 1);
        VERIFIER(WS7B_EK79007_INIT[i].arg0 == valeurs[i]);
        VERIFIER(WS7B_EK79007_INIT[i].delay_ms == 0);
    }
    VERIFIER(WS7B_EK79007_INIT[7].cmd == WS7B_DCS_SLEEP_OUT);
    VERIFIER(WS7B_EK79007_INIT[7].nargs == 0);
    VERIFIER(WS7B_EK79007_INIT[7].delay_ms == 120);
    return 0;
}

constexpr int la_table_n_emet_ni_b2_ni_extinction() {
    // B2 (nombre de voies) est emise par le composant esp_lcd_ek79007 ; 0x28, 0x29 et 0x10 sont reserves au plan d'extinction.
    for (std::size_t i = 0; i < WS7B_EK79007_INIT_COUNT; i++) {
        VERIFIER(WS7B_EK79007_INIT[i].cmd != WS7B_DCS_LANE_CONTROL);
        VERIFIER(WS7B_EK79007_INIT[i].cmd != WS7B_DCS_DISPLAY_OFF);
        VERIFIER(WS7B_EK79007_INIT[i].cmd != WS7B_DCS_DISPLAY_ON);
        VERIFIER(WS7B_EK79007_INIT[i].cmd != WS7B_DCS_SLEEP_IN);
        VERIFIER(WS7B_EK79007_INIT[i].cmd != 0x36); // MADCTL : pas de balayage materiel (rotation logicielle)
        VERIFIER(WS7B_EK79007_INIT[i].nargs <= 1);
    }
    VERIFIER(WS7B_DCS_LANE_CONTROL_2_LANES == 0x10);
    return 0;
}

// ---- rotation et tactile ------------------------------------------------------------------------------------------------------

constexpr int rotations_prises_en_charge() {
    VERIFIER(ws7b_rotation_valid(0));
    VERIFIER(ws7b_rotation_valid(180));
    VERIFIER(!ws7b_rotation_valid(90)); // la dalle est deja en paysage : ni 90 ni 270
    VERIFIER(!ws7b_rotation_valid(270));
    VERIFIER(!ws7b_rotation_valid(-180));
    VERIFIER(!ws7b_rotation_valid(360));
    return 0;
}

constexpr int taille_logique_de_l_interface() {
    int w = 0;
    int h = 0;
    ws7b_ui_size(0, &w, &h);
    VERIFIER(w == 1024 && h == 600);
    ws7b_ui_size(180, &w, &h);
    VERIFIER(w == 1024 && h == 600);
    ws7b_ui_size(90, &w, &h);
    VERIFIER(w == 600 && h == 1024);
    ws7b_ui_size(270, &w, &h);
    VERIFIER(w == 600 && h == 1024);
    return 0;
}

constexpr int rotation_de_180_degres_des_coins_et_du_centre() {
    int x = -1;
    int y = -1;
    ws7b_rotate_point(180, 0, 0, 1024, 600, &x, &y);
    VERIFIER(x == 1023 && y == 599);
    ws7b_rotate_point(180, 1023, 599, 1024, 600, &x, &y);
    VERIFIER(x == 0 && y == 0);
    ws7b_rotate_point(180, 1023, 0, 1024, 600, &x, &y);
    VERIFIER(x == 0 && y == 599);
    ws7b_rotate_point(180, 512, 300, 1024, 600, &x, &y);
    VERIFIER(x == 511 && y == 299);
    return 0;
}

constexpr int rotation_de_0_degre_ne_change_rien() {
    int x = -1;
    int y = -1;
    ws7b_rotate_point(0, 123, 456, 1024, 600, &x, &y);
    VERIFIER(x == 123 && y == 456);
    return 0;
}

constexpr int rotation_de_180_est_son_propre_inverse_sur_toute_la_dalle() {
    for (int px = 0; px < 1024; px += 37) {
        for (int py = 0; py < 600; py += 29) {
            int ax = 0, ay = 0, bx = 0, by = 0;
            ws7b_rotate_point(180, px, py, 1024, 600, &ax, &ay);
            VERIFIER(ax >= 0 && ax < 1024 && ay >= 0 && ay < 600); // reste dans la dalle
            ws7b_rotate_point(180, ax, ay, 1024, 600, &bx, &by);
            VERIFIER(bx == px && by == py);
        }
    }
    return 0;
}

constexpr int rotations_de_90_et_270_restent_dans_l_interface_tournee() {
    // Non utilisees par le BSP, mais la fonction doit etre juste : un point de la dalle 1024 x 600 tombe dans 600 x 1024.
    const int degres[2] = {90, 270};
    for (int d = 0; d < 2; d++) {
        for (int px = 0; px < 1024; px += 41) {
            for (int py = 0; py < 600; py += 31) {
                int ox = -1, oy = -1;
                ws7b_rotate_point(degres[d], px, py, 1024, 600, &ox, &oy);
                VERIFIER(ox >= 0 && ox < 600 && oy >= 0 && oy < 1024);
            }
        }
    }
    int x = 0, y = 0;
    ws7b_rotate_point(90, 0, 0, 1024, 600, &x, &y); // meme formule que le Tab5 : x = hauteur - 1 - y_brut, y = x_brut
    VERIFIER(x == 599 && y == 0);
    ws7b_rotate_point(270, 0, 0, 1024, 600, &x, &y);
    VERIFIER(x == 0 && y == 1023);
    return 0;
}

constexpr int points_de_l_autotest_dans_l_interface_et_retrouves_apres_rotation() {
    // Pour chaque rotation prise en charge : le point BRUT injecte (rotation du point voulu) redonne le point voulu quand LVGL
    // applique sa propre rotation (qui est la meme fonction, 0 et 180 etant leur propre inverse).
    const int degres[2] = {0, 180};
    for (int d = 0; d < 2; d++) {
        for (int k = 0; k < WS7B_AUTOTEST_POINTS; k++) {
            VERIFIER(WS7B_AUTOTEST_X[k] >= 0 && WS7B_AUTOTEST_X[k] < WS7B_LCD_H_RES);
            VERIFIER(WS7B_AUTOTEST_Y[k] >= 0 && WS7B_AUTOTEST_Y[k] < WS7B_LCD_V_RES);
            int rx = 0, ry = 0, lx = 0, ly = 0;
            ws7b_rotate_point(degres[d], WS7B_AUTOTEST_X[k], WS7B_AUTOTEST_Y[k], 1024, 600, &rx, &ry);
            ws7b_rotate_point(degres[d], rx, ry, 1024, 600, &lx, &ly);
            VERIFIER(lx == WS7B_AUTOTEST_X[k] && ly == WS7B_AUTOTEST_Y[k]);
        }
    }
    // Les cinq points couvrent les quatre coins et le centre, tous distincts.
    for (int a = 0; a < WS7B_AUTOTEST_POINTS; a++) {
        for (int b = a + 1; b < WS7B_AUTOTEST_POINTS; b++) {
            VERIFIER(WS7B_AUTOTEST_X[a] != WS7B_AUTOTEST_X[b] || WS7B_AUTOTEST_Y[a] != WS7B_AUTOTEST_Y[b]);
        }
    }
    return 0;
}

constexpr int identifiant_produit_du_gt911() {
    constexpr uint8_t bon[4] = {'9', '1', '1', 0};
    constexpr uint8_t autre_puce[4] = {'9', '1', '2', 0};
    constexpr uint8_t trop_court[4] = {'9', '1', 0, 0};
    constexpr uint8_t sans_fin[4] = {'9', '1', '1', '1'};
    constexpr uint8_t vide[4] = {0, 0, 0, 0};
    VERIFIER(ws7b_gt911_product_id_ok(bon));
    VERIFIER(!ws7b_gt911_product_id_ok(autre_puce));
    VERIFIER(!ws7b_gt911_product_id_ok(trop_court));
    VERIFIER(!ws7b_gt911_product_id_ok(sans_fin));
    VERIFIER(!ws7b_gt911_product_id_ok(vide));
    VERIFIER(WS7B_GT911_PRODUCT_ID_REG == 0x8140);
    return 0;
}

constexpr int miroirs_du_gt911_gardes_et_pas_d_echange() {
    VERIFIER(WS7B_GT911_MIRROR_X == 1 && WS7B_GT911_MIRROR_Y == 1 && WS7B_GT911_SWAP_XY == 0);
    return 0;
}

constexpr int le_point_tactile_est_ramene_dans_la_dalle() {
    VERIFIER(ws7b_clamp(-1, 0, WS7B_LCD_H_RES - 1) == 0);
    VERIFIER(ws7b_clamp(0, 0, WS7B_LCD_H_RES - 1) == 0);
    VERIFIER(ws7b_clamp(500, 0, WS7B_LCD_H_RES - 1) == 500);
    VERIFIER(ws7b_clamp(WS7B_LCD_H_RES - 1, 0, WS7B_LCD_H_RES - 1) == WS7B_LCD_H_RES - 1);
    VERIFIER(ws7b_clamp(WS7B_LCD_H_RES, 0, WS7B_LCD_H_RES - 1) == WS7B_LCD_H_RES - 1);
    VERIFIER(ws7b_clamp(100000, 0, WS7B_LCD_V_RES - 1) == WS7B_LCD_V_RES - 1);
    // Cas reel : le miroir du pilote calcule x_max - x ; une puce qui rend x = 1024 donne -1, ramene a 0.
    VERIFIER(ws7b_clamp((WS7B_LCD_H_RES - 1) - 1024, 0, WS7B_LCD_H_RES - 1) == 0);
    return 0;
}

// ---- retroeclairage -----------------------------------------------------------------------------------------------------------

constexpr int rapport_cyclique_du_retroeclairage() {
    VERIFIER(ws7b_backlight_duty(0, WS7B_BACKLIGHT_BITS) == 0);
    VERIFIER(ws7b_backlight_duty(100, WS7B_BACKLIGHT_BITS) == 1023);
    VERIFIER(ws7b_backlight_duty(50, WS7B_BACKLIGHT_BITS) == 511);
    VERIFIER(ws7b_backlight_duty(100, 8) == 255);
    VERIFIER(ws7b_backlight_duty(-5, WS7B_BACKLIGHT_BITS) == 0);     // negatif -> 0
    VERIFIER(ws7b_backlight_duty(250, WS7B_BACKLIGHT_BITS) == 1023); // au-dessus de 100 -> 100
    VERIFIER(WS7B_BACKLIGHT_HZ == 5000 && WS7B_BACKLIGHT_BITS == 10);
    return 0;
}

constexpr int rapport_cyclique_croissant_et_borne() {
    uint32_t precedent = 0;
    for (int p = 0; p <= 100; p++) {
        const uint32_t duty = ws7b_backlight_duty(p, WS7B_BACKLIGHT_BITS);
        VERIFIER(duty >= precedent);
        VERIFIER(duty <= 1023);
        precedent = duty;
    }
    return 0;
}

// ---- extinction et reveil de la dalle -----------------------------------------------------------------------------------------

constexpr int rien_a_faire_si_l_etat_est_deja_le_bon() {
    ws7b_step_t plan[WS7B_PLAN_MAX] = {};
    VERIFIER(ws7b_panel_plan(1, 1, plan) == 0);
    VERIFIER(ws7b_panel_plan(0, 0, plan) == 0);
    VERIFIER(ws7b_panel_plan(5, 2, plan) == 0); // toute valeur non nulle vaut « allume »
    return 0;
}

constexpr int plan_d_extinction_voile_puis_0x28_puis_0x10() {
    ws7b_step_t plan[WS7B_PLAN_MAX] = {};
    const std::size_t n = ws7b_panel_plan(1, 0, plan);
    VERIFIER(n == 5);
    VERIFIER(plan[0].kind == WS7B_STEP_VEIL_ON);
    VERIFIER(plan[1].kind == WS7B_STEP_WAIT && plan[1].ms == WS7B_PANEL_VEIL_SETTLE_MS);
    VERIFIER(plan[2].kind == WS7B_STEP_DCS && plan[2].dcs == 0x28);
    VERIFIER(plan[3].kind == WS7B_STEP_SPACE);
    VERIFIER(plan[4].kind == WS7B_STEP_DCS && plan[4].dcs == 0x10);
    return 0;
}

constexpr int plan_de_reveil_0x11_120_ms_0x29_puis_voile_retire() {
    ws7b_step_t plan[WS7B_PLAN_MAX] = {};
    const std::size_t n = ws7b_panel_plan(0, 1, plan);
    VERIFIER(n == 5);
    VERIFIER(plan[0].kind == WS7B_STEP_SPACE);
    VERIFIER(plan[1].kind == WS7B_STEP_DCS && plan[1].dcs == 0x11);
    VERIFIER(plan[2].kind == WS7B_STEP_WAIT && plan[2].ms == 120);
    VERIFIER(plan[3].kind == WS7B_STEP_DCS && plan[3].dcs == 0x29);
    VERIFIER(plan[4].kind == WS7B_STEP_VEIL_OFF);
    return 0;
}

constexpr int le_voile_couvre_toute_la_veille() {
    // Extinction : le voile est en place AVANT « display off » ; reveil : il n'est retire qu'APRES « display on ».
    ws7b_step_t off[WS7B_PLAN_MAX] = {};
    ws7b_step_t on[WS7B_PLAN_MAX] = {};
    const std::size_t n_off = ws7b_panel_plan(1, 0, off);
    const std::size_t n_on = ws7b_panel_plan(0, 1, on);
    VERIFIER(off[0].kind == WS7B_STEP_VEIL_ON);
    VERIFIER(on[n_on - 1].kind == WS7B_STEP_VEIL_OFF);
    // Aucune etape inconnue, et le plan tient dans le tableau.
    VERIFIER(n_off <= WS7B_PLAN_MAX && n_on <= WS7B_PLAN_MAX);
    for (std::size_t i = 0; i < n_off; i++) {
        VERIFIER(off[i].kind != WS7B_STEP_NONE);
    }
    for (std::size_t i = 0; i < n_on; i++) {
        VERIFIER(on[i].kind != WS7B_STEP_NONE);
    }
    return 0;
}

constexpr int deux_changements_de_veille_sont_toujours_espaces() {
    // Un « sleep in » / « sleep out » est toujours precede d'un SPACE (la norme DCS veut 120 ms entre les deux).
    ws7b_step_t plan[2][WS7B_PLAN_MAX] = {};
    const std::size_t n[2] = {ws7b_panel_plan(1, 0, plan[0]), ws7b_panel_plan(0, 1, plan[1])};
    for (int k = 0; k < 2; k++) {
        for (std::size_t i = 0; i < n[k]; i++) {
            if (plan[k][i].kind == WS7B_STEP_DCS &&
                (plan[k][i].dcs == WS7B_DCS_SLEEP_IN || plan[k][i].dcs == WS7B_DCS_SLEEP_OUT)) {
                VERIFIER(i > 0 && plan[k][i - 1].kind == WS7B_STEP_SPACE);
            }
        }
    }
    return 0;
}

constexpr int attente_restante_de_l_espacement() {
    VERIFIER(ws7b_wait_remaining(1000, 1000, 120) == 120);
    VERIFIER(ws7b_wait_remaining(1050, 1000, 120) == 70);
    VERIFIER(ws7b_wait_remaining(1119, 1000, 120) == 1);
    VERIFIER(ws7b_wait_remaining(1120, 1000, 120) == 0);
    VERIFIER(ws7b_wait_remaining(5000, 1000, 120) == 0);
    return 0;
}

constexpr int attente_restante_sur_le_passage_a_zero_du_compteur() {
    // Le compteur de millisecondes (32 bits) repasse par zero toutes les 49,7 jours : l'ecart se calcule modulo 2^32.
    VERIFIER(ws7b_wait_remaining(0x00000010u, 0xFFFFFFF0u, 120) == 88); // 32 ms ecoulees
    VERIFIER(ws7b_wait_remaining(0x00000100u, 0xFFFFFFF0u, 120) == 0);  // 272 ms ecoulees
    return 0;
}

// ---- audio --------------------------------------------------------------------------------------------------------------------

constexpr int volume_zero_coupe_et_cent_vaut_zero_db() {
    VERIFIER(ws7b_volume_reg(0) == 0x00);
    VERIFIER(ws7b_volume_reg(-10) == 0x00);
    VERIFIER(ws7b_volume_reg(100) == 0xBF);
    VERIFIER(ws7b_volume_reg(1000) == 0xBF); // au-dessus de 100 : plafonne
    VERIFIER(ws7b_reg_to_decidb(ws7b_volume_reg(100)) == 0);
    return 0;
}

constexpr int le_volume_ne_depasse_jamais_zero_db() {
    // Le registre a +32 dB (0xFF) SATURE le haut-parleur (essai d'Axel du 17/10/2026) : aucune valeur d'entree ne doit y mener.
    // Valeurs ECRITES EN DUR (pas la constante testee) : 0xBF = 0 dB est un fait du composant ES8311 (esp_codec_dev, es8311.c),
    // pas un choix du BSP. Un plafond remonte a 0xFF ferait passer un test ecrit avec la meme constante.
    for (int p = -1000; p <= 1000; p++) {
        VERIFIER(ws7b_volume_reg(p) <= 0xBF);
        VERIFIER((static_cast<int>(ws7b_volume_reg(p)) - 0xBF) * 5 <= 0); // gain en dixiemes de dB, au plus 0
    }
    return 0;
}

constexpr int plancher_audible_a_40_pour_cent_de_l_echelle() {
    VERIFIER(WS7B_ES8311_REG_FLOOR * 100 == 40 * 255); // 0x66 = 40 % de 255
    VERIFIER(WS7B_ES8311_REG_0DB * 100 >= 74 * 255);   // 0xBF ~ 75 % de 255
    VERIFIER(WS7B_ES8311_REG_0DB * 100 < 76 * 255);
    VERIFIER(ws7b_volume_reg(1) >= WS7B_ES8311_REG_FLOOR);    // un volume non nul n'est jamais sous le plancher
    VERIFIER(ws7b_reg_to_decidb(ws7b_volume_reg(1)) >= -445); // -44,5 dB
    VERIFIER(ws7b_reg_to_decidb(WS7B_ES8311_REG_FLOOR) == -445);
    return 0;
}

constexpr int volume_croissant_sans_trou_d_un_pas() {
    uint8_t precedent = ws7b_volume_reg(1);
    for (int p = 2; p <= 100; p++) {
        const uint8_t reg = ws7b_volume_reg(p);
        VERIFIER(reg >= precedent);
        VERIFIER(reg - precedent <= 2); // pas de saut brutal d'un pourcent au suivant
        precedent = reg;
    }
    return 0;
}

constexpr int echelle_du_registre_de_l_es8311() {
    VERIFIER(ws7b_reg_to_decidb(0x00) == -955);
    VERIFIER(ws7b_reg_to_decidb(0xBF) == 0);
    VERIFIER(ws7b_reg_to_decidb(0xFF) == 320);
    VERIFIER(ws7b_reg_to_decidb(0xC0) == 5);
    VERIFIER(WS7B_ES8311_REG_VOLUME == 0x32);
    return 0;
}

// ---- memoire ------------------------------------------------------------------------------------------------------------------

constexpr int tampon_de_dessin_dans_le_budget_du_tab5() {
    VERIFIER(ws7b_draw_buffer_bytes(1) == 2048);
    VERIFIER(ws7b_draw_buffer_bytes(25) == 51200);
    // Deux tampons de la hauteur par defaut : pas plus de RAM interne que les 2 x 72 000 octets du Tab5.
    VERIFIER(2 * ws7b_draw_buffer_bytes(WS7B_DRAW_BUFFER_LINES_DEFAULT) <= 144000);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(broches_valides_et_distinctes)                                                                                   \
    X(aucune_broche_reservee_n_est_prise)                                                                              \
    X(sdio_du_c6_sur_la_fente_1)                                                                                       \
    X(i2c_et_i2s_valeurs_connues)                                                                                      \
    X(cadence_de_rafraichissement_proche_de_60_hz)                                                                     \
    X(les_voies_dsi_ont_de_la_marge)                                                                                   \
    X(geometrie_de_la_dalle)                                                                                           \
    X(table_d_initialisation_de_l_ek79007)                                                                             \
    X(la_table_n_emet_ni_b2_ni_extinction)                                                                             \
    X(rotations_prises_en_charge)                                                                                      \
    X(taille_logique_de_l_interface)                                                                                   \
    X(rotation_de_180_degres_des_coins_et_du_centre)                                                                   \
    X(rotation_de_0_degre_ne_change_rien)                                                                              \
    X(rotation_de_180_est_son_propre_inverse_sur_toute_la_dalle)                                                       \
    X(rotations_de_90_et_270_restent_dans_l_interface_tournee)                                                         \
    X(points_de_l_autotest_dans_l_interface_et_retrouves_apres_rotation)                                               \
    X(identifiant_produit_du_gt911)                                                                                    \
    X(miroirs_du_gt911_gardes_et_pas_d_echange)                                                                        \
    X(le_point_tactile_est_ramene_dans_la_dalle)                                                                       \
    X(rapport_cyclique_du_retroeclairage)                                                                              \
    X(rapport_cyclique_croissant_et_borne)                                                                             \
    X(rien_a_faire_si_l_etat_est_deja_le_bon)                                                                          \
    X(plan_d_extinction_voile_puis_0x28_puis_0x10)                                                                     \
    X(plan_de_reveil_0x11_120_ms_0x29_puis_voile_retire)                                                               \
    X(le_voile_couvre_toute_la_veille)                                                                                 \
    X(deux_changements_de_veille_sont_toujours_espaces)                                                                \
    X(attente_restante_de_l_espacement)                                                                                \
    X(attente_restante_sur_le_passage_a_zero_du_compteur)                                                              \
    X(volume_zero_coupe_et_cent_vaut_zero_db)                                                                          \
    X(le_volume_ne_depasse_jamais_zero_db)                                                                             \
    X(plancher_audible_a_40_pour_cent_de_l_echelle)                                                                    \
    X(volume_croissant_sans_trou_d_un_pas)                                                                             \
    X(echelle_du_registre_de_l_es8311)                                                                                 \
    X(tampon_de_dessin_dans_le_budget_du_tab5)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
