// Partie PURE de la couche carte de la Waveshare ESP32-P4-WIFI6-Touch-LCD-7B : broches, minutages de la dalle, table
// d'initialisation de l'EK79007, courbe de volume de l'ES8311, rotation d'un point tactile, plan d'extinction de la dalle.
//
// Pourquoi ce fichier existe : tout ce qui est calcul ou table de valeurs est ICI, sans aucun include d'ESP-IDF, pour etre
// compile et EXECUTE sur le PC (test/host/test_carte_7b.cpp, avec AddressSanitizer et UBSan) sans la carte. Les fichiers
// ws7b_*.c n'ont plus qu'a executer ces valeurs : ce qui est teste est ce qui tourne.
//
// Contraintes : C99 ET C++17 (ce fichier est inclus par du C et par les tests C++) -> pas d'initialiseurs designes, pas de
// litteraux composes, uniquement des constantes, des tableaux `static const` et des fonctions `static inline`. Aucun include
// autre que <stdint.h> / <stddef.h>.
//
// Sources des valeurs (lues le 07/11/2026, rien de memoire ; « non verifie » = pas lu dans une source) :
//   [S]  schema Waveshare lu page par page, audit materiel de la 7B n 1 du 16/10/2026 (notes locales de l'auteur, hors depot)
//   [B]  BSP Waveshare waveshare/esp32_p4_wifi6_touch_lcd_7b 3.0.1 (registre des composants) : include/bsp/*.h, *.c
//   [E]  composant Espressif esp_lcd_ek79007 2.0.2 : esp_lcd_ek79007.c, include/esp_lcd_ek79007.h
//   [H]  firmware ESPHome en service sur cette carte depuis le 15/10/2026 : Tab5/paquets/carte-waveshare-7b.yaml et le modele
//        mipi_dsi WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B d'ESPHome 2026.9.1
//   [M]  memoires d'Axel waveshare-7b-materiel-c6 et es8311-volume-gain-7b (mesures sur cette carte, 16 au 19/10/2026)
//   [D]  codec : esp_codec_dev 1.5.11, device/es8311/es8311.c
#ifndef WS7B_PUR_H
#define WS7B_PUR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Un tableau `static const` defini dans un en-tete n'est pas toujours utilise par chaque fichier qui l'inclut.
#if defined(__GNUC__)
#define WS7B_UNUSED __attribute__((unused))
#else
#define WS7B_UNUSED
#endif

// En C++ (les tests du PC, test/host/test_carte_7b.cpp) les fonctions et tables sont `constexpr` : le cadre de test les evalue a la
// COMPILATION par des static_assert (voir test/host/mini.hpp), ce qui permet de les verifier meme sans pouvoir lancer de programme.
// En C (les fichiers ws7b_*.c de la carte) ce sont de simples `static inline` et `static const`.
#ifdef __cplusplus
#define WS7B_FN static constexpr
#define WS7B_TABLE static constexpr
#else
#define WS7B_FN static inline
#define WS7B_TABLE static const
#endif

// ============================================================================================================================
// 1. Broches (numeros de GPIO du P4). Une seule definition ici ; ws7b_internal.h les convertit en gpio_num_t.
// ============================================================================================================================
#define WS7B_PIN_I2C_SDA 7      // [S][B] tactile GT911, ES8311, ES7210, connecteur I2C
#define WS7B_PIN_I2C_SCL 8      // [S][B]
#define WS7B_PIN_I2S_DOUT 9     // [S][B] vers l'ES8311 (DSDIN)
#define WS7B_PIN_I2S_LRCK 10    // [S][B]
#define WS7B_PIN_I2S_DIN 11     // [S][B] depuis l'ES7210 (ASDOUT)
#define WS7B_PIN_I2S_BCLK 12    // [S][B]
#define WS7B_PIN_I2S_MCLK 13    // [S][B]
#define WS7B_PIN_C6_SDIO_D0 14  // [S][H] SDIO 4 bits vers l'ESP32-C6
#define WS7B_PIN_C6_SDIO_D1 15  // [S][H]
#define WS7B_PIN_C6_SDIO_D2 16  // [S][H]
#define WS7B_PIN_C6_SDIO_D3 17  // [S][H]
#define WS7B_PIN_C6_SDIO_CLK 18 // [S][H]
#define WS7B_PIN_C6_SDIO_CMD 19 // [S][H]
#define WS7B_PIN_VBAT_ADC 20    // [S] tension de la batterie (pont 200k/100k, /3) : non utilisee par ce BSP
#define WS7B_PIN_TOUCH_RST 23   // [S] RESET_TP par R54 ; confirme par [H] (reset_pin: GPIO23)
#define WS7B_PIN_BACKLIGHT 32   // [S][B][H] BL_CTRL -> filtre RC -> AP3032 : gradation ANALOGIQUE
#define WS7B_PIN_LCD_RST 33     // [S][B][H] RESET_LCD
#define WS7B_PIN_AMP_CTRL 53    // [S][B][H] CTRL de l'ampli NS4150B (haut = actif)
#define WS7B_PIN_C6_RESET 54    // [S][H] CHIP_PU du C6 par R33 (0 ohm)
// Broches qui existent sur la carte et que ce BSP NE touche PAS (a ne jamais reaffecter par erreur) :
#define WS7B_PIN_USB_DM 24   // [S] USB 1.1 Serial/JTAG du P4 (le port de flash et de journal)
#define WS7B_PIN_USB_DP 25   // [S]
#define WS7B_PIN_UART0_TX 37 // [S] pont CH343P
#define WS7B_PIN_UART0_RX 38 // [S]
#define WS7B_PIN_BOOT 35     // [S] bouton BOOT
#define WS7B_PIN_SD_PWR 45   // [S] alimentation de la microSD (bas = allumee) : non utilisee

// Le tactile GT911 a son INT relie au seul point de test TP1, PAS au P4 [S] : lecture par interrogation periodique.
#define WS7B_TOUCH_INT_CONNECTED 0

// ============================================================================================================================
// 2. Dalle : EK79007, 1024 x 600, MIPI-DSI 2 voies. Minutages DPI : [E] (macro EK79007_1024_600_PANEL_60HZ_CONFIG_CF),
//    identiques dans [B] et [H] (modele ESPHome WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B).
// ============================================================================================================================
#define WS7B_LCD_H_RES 1024
#define WS7B_LCD_V_RES 600
#define WS7B_DSI_LANES 2
#define WS7B_DPI_CLOCK_MHZ 52
#define WS7B_DPI_HSYNC_PULSE 10
#define WS7B_DPI_HSYNC_BACK_PORCH 160
#define WS7B_DPI_HSYNC_FRONT_PORCH 160
#define WS7B_DPI_VSYNC_PULSE 1
#define WS7B_DPI_VSYNC_BACK_PORCH 23
#define WS7B_DPI_VSYNC_FRONT_PORCH 12
// Debit de ligne DSI. Trois sources, deux valeurs : 900 Mbit/s dans [E] (macro EK79007_PANEL_BUS_DSI_2CH_CONFIG) ET dans
// [H] (la valeur qui fait tourner cette carte depuis le 15/10/2026) ; 1000 Mbit/s dans [B]. Choix : 900. A essayer : 1000, seulement
// si l'ecran reste noir ou scintille.
#define WS7B_DSI_LANE_MBPS 900
#define WS7B_DSI_PHY_LDO_CHANNEL 3 // LDO_VO3 -> VDD_MIPI_DPHY [B]
#define WS7B_DSI_PHY_LDO_MV 2500   // [B]
#define WS7B_RGB565_BITS_PER_PIXEL 16

// Cadence de rafraichissement en milli-hertz (52 MHz / (1024+10+160+160) / (600+1+23+12)) : doit etre ~60 Hz.
WS7B_FN uint32_t ws7b_refresh_mhz(void) {
    const uint32_t h_total =
        WS7B_LCD_H_RES + WS7B_DPI_HSYNC_PULSE + WS7B_DPI_HSYNC_BACK_PORCH + WS7B_DPI_HSYNC_FRONT_PORCH;
    const uint32_t v_total =
        WS7B_LCD_V_RES + WS7B_DPI_VSYNC_PULSE + WS7B_DPI_VSYNC_BACK_PORCH + WS7B_DPI_VSYNC_FRONT_PORCH;
    return (uint32_t)(((uint64_t)WS7B_DPI_CLOCK_MHZ * 1000000ULL * 1000ULL) / ((uint64_t)h_total * v_total));
}

// Debit utile des pixels (bit/s) et debit total offert par les voies DSI : le second doit depasser le premier avec de la marge.
WS7B_FN uint64_t ws7b_pixel_bps(void) { return (uint64_t)WS7B_DPI_CLOCK_MHZ * 1000000ULL * WS7B_RGB565_BITS_PER_PIXEL; }
WS7B_FN uint64_t ws7b_dsi_bps(void) { return (uint64_t)WS7B_DSI_LANES * WS7B_DSI_LANE_MBPS * 1000000ULL; }

// Table d'initialisation propre a la dalle, envoyee en commandes DCS APRES la commande B2 (nombre de voies) que le composant
// esp_lcd_ek79007 emet lui-meme en tete [E] (0x10 = 2 voies, 0x00 = 4 voies). Valeurs : [E] vendor_specific_init_default,
// identiques a la sequence d'ESPHome [H] (qui envoie B2 = 0x10 en dernier : meme ensemble de registres). La derniere commande est
// « sleep out » (0x11), suivie de 120 ms.
typedef struct {
    uint8_t cmd;
    uint8_t nargs; // 0 ou 1
    uint8_t arg0;
    uint16_t delay_ms;
} ws7b_dcs_t;

#define WS7B_DCS_SLEEP_IN 0x10
#define WS7B_DCS_SLEEP_OUT 0x11
#define WS7B_DCS_DISPLAY_OFF 0x28
#define WS7B_DCS_DISPLAY_ON 0x29
#define WS7B_DCS_LANE_CONTROL 0xB2 // emis par le composant
#define WS7B_DCS_LANE_CONTROL_2_LANES 0x10

WS7B_TABLE ws7b_dcs_t WS7B_EK79007_INIT[] WS7B_UNUSED = {
    {0x80, 1, 0x8B, 0}, {0x81, 1, 0x78, 0}, {0x82, 1, 0x84, 0}, {0x83, 1, 0x88, 0},
    {0x84, 1, 0xA8, 0}, {0x85, 1, 0xE3, 0}, {0x86, 1, 0x88, 0}, {WS7B_DCS_SLEEP_OUT, 0, 0, 120},
};
#define WS7B_EK79007_INIT_COUNT (sizeof(WS7B_EK79007_INIT) / sizeof(WS7B_EK79007_INIT[0]))

// ============================================================================================================================
// 3. Orientation. La dalle est NATIVEMENT en paysage 1024 x 600. Deux montages seulement sont pris en charge :
//      0   = image telle que la dalle la balaie. C'est la 7B d'Axel, MONTEE A L'ENVERS par rapport au sens des demos Waveshare
//            (memoire waveshare-7b-materiel-c6 : « ses builds = carte_rotation 0 » depuis le 16/10/2026) ;
//      180 = montage « normal » : le sens des demos Waveshare ([B] bsp_display_start : ESP_LV_ADAPTER_ROTATE_180) et le defaut
//            public du firmware ESPHome ([H] carte_rotation: 180).
//    La rotation est LOGICIELLE (LVGL + accelerateur PPA, comme le Tab5) : le balayage materiel par le registre MADCTL de
//    l'EK79007 existe dans le composant [E] mais n'a JAMAIS ete essaye sur cette carte (ESPHome le desactive : no_transform).
// ============================================================================================================================
#define WS7B_ROTATION_AS_SCANNED 0
#define WS7B_ROTATION_FLIPPED 180

WS7B_FN int ws7b_rotation_valid(int degrees) {
    return degrees == WS7B_ROTATION_AS_SCANNED || degrees == WS7B_ROTATION_FLIPPED;
}

// Taille LOGIQUE de l'interface selon la rotation (90/270 echangent largeur et hauteur ; non pris en charge par le BSP mais
// la fonction est juste pour eux, et le test le dit).
WS7B_FN void ws7b_ui_size(int degrees, int *width, int *height) {
    const int tourne = (degrees == 90 || degrees == 270);
    *width = tourne ? WS7B_LCD_V_RES : WS7B_LCD_H_RES;
    *height = tourne ? WS7B_LCD_H_RES : WS7B_LCD_V_RES;
}

// Point de l'interface (apres rotation) vers le point BRUT de la dalle, et inversement : pour 0 et 180 la rotation de 180 est son
// propre inverse. Convention de LVGL pour lv_display_set_rotation() : 180 -> (W-1-x, H-1-y). Sert a l'auto-test du tactile
// (injecter un appui brut dont on connait le point logique attendu) et a la trace « brut -> logique ».
WS7B_FN void ws7b_rotate_point(int degrees, int x, int y, int panel_w, int panel_h, int *ox, int *oy) {
    switch (degrees) {
    case 90: // x_logique = hauteur - 1 - y_brut ; y_logique = x_brut (meme formule que le Tab5)
        *ox = panel_h - 1 - y;
        *oy = x;
        break;
    case 180:
        *ox = panel_w - 1 - x;
        *oy = panel_h - 1 - y;
        break;
    case 270:
        *ox = y;
        *oy = panel_w - 1 - x;
        break;
    default:
        *ox = x;
        *oy = y;
        break;
    }
}

// ============================================================================================================================
// 4. Tactile GT911. Adresses I2C 7 bits : 0x5D par defaut, 0x14 si l'INT etait haute au reset [S][B]. Reponse du registre
//    d'identifiant produit (0x8140, 4 octets) : les caracteres ASCII « 9 », « 1 », « 1 » puis 0x00 [driver gt911 1.2.1].
//    Le BSP Waveshare [B] ET ESPHome [H] inversent X et Y (le GT911 est monte a l'envers de la dalle) : sans cela « 1 appui
//    reconnu en 5 min » ([H], essai du 14/10/2026). Ces deux inversions sont GARDEES telles quelles ; la rotation de l'interface
//    est ensuite faite par LVGL sur le point deja cale, comme sur le Tab5.
// ============================================================================================================================
#define WS7B_GT911_ADDR 0x5D
#define WS7B_GT911_ADDR_BACKUP 0x14
#define WS7B_GT911_MIRROR_X 1
#define WS7B_GT911_MIRROR_Y 1
#define WS7B_GT911_SWAP_XY 0
#define WS7B_GT911_PRODUCT_ID_REG 0x8140

WS7B_FN int ws7b_gt911_product_id_ok(const uint8_t id[4]) {
    return id[0] == '9' && id[1] == '1' && id[2] == '1' && id[3] == 0;
}

// Borne un nombre dans [lo, hi]. Sert au point tactile : apres le miroir du pilote, le GT911 peut rendre 1024 (hors dalle) ou -1
// au bord ; on le ramene dans la dalle avant de le donner a LVGL (le Tab5 n'en a pas besoin, sa puce tactile ne deborde pas).
WS7B_FN int ws7b_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Les cinq appuis de l'auto-test (points LOGIQUES de l'interface 1024 x 600, quelle que soit la rotation) : les quatre coins et le
// centre. Meme jeu que le Tab5 (six appuis, le sixieme sur un bouton de l'ecran principal, propre a la taille de son ecran).
#define WS7B_AUTOTEST_POINTS 5
WS7B_TABLE int WS7B_AUTOTEST_X[WS7B_AUTOTEST_POINTS] WS7B_UNUSED = {0, WS7B_LCD_H_RES - 1, 0, WS7B_LCD_H_RES - 1,
                                                                    WS7B_LCD_H_RES / 2};
WS7B_TABLE int WS7B_AUTOTEST_Y[WS7B_AUTOTEST_POINTS] WS7B_UNUSED = {0, 0, WS7B_LCD_V_RES - 1, WS7B_LCD_V_RES - 1,
                                                                    WS7B_LCD_V_RES / 2};

// ============================================================================================================================
// 5. Retroeclairage. LEDC 10 bits, 5 kHz, sortie INVERSEE (une sortie a 0 % de rapport cyclique = ecran a pleine lumiere
//    sinon) [B][H]. 0 % ne l'eteint PAS tout a fait (analogique, constate par Axel le 16/10/2026 [M]) : voir le plan d'extinction.
// ============================================================================================================================
#define WS7B_BACKLIGHT_HZ 5000
#define WS7B_BACKLIGHT_BITS 10

WS7B_FN uint32_t ws7b_backlight_duty(int percent, int bits) {
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t max_duty = (1U << bits) - 1U;
    return max_duty * (uint32_t)percent / 100U;
}

// ============================================================================================================================
// 6. Extinction reelle de la dalle (« ecran eteint = vraiment noir »). Le retroeclairage analogique ne s'eteint pas : recette
//    validee sur cette carte par Axel le 17/10/2026 sous ESPHome (« ecran bien noir, il se rallume quand je le touche » [M]) :
//    voile noir, « display off » (0x28), « sleep in » (0x10) ; au reveil « sleep out » (0x11), 120 ms, « display on » (0x29),
//    voile retire. 0x28 seul avait ete essaye le 16/10 sans effet visible. Delai de la norme DCS : 120 ms entre deux
//    changements de veille (0x10 <-> 0x11).
// ============================================================================================================================
#define WS7B_PANEL_SLEEP_SPACING_MS 120
#define WS7B_PANEL_VEIL_SETTLE_MS 40

typedef enum {
    WS7B_STEP_NONE = 0,
    WS7B_STEP_VEIL_ON,  // objet noir plein ecran sur la couche systeme de LVGL, dessine tout de suite
    WS7B_STEP_VEIL_OFF, // retire
    WS7B_STEP_DCS,      // commande DCS sans parametre : le champ `dcs`
    WS7B_STEP_WAIT,     // attente de `ms` millisecondes
    WS7B_STEP_SPACE     // attendre ce qui reste de WS7B_PANEL_SLEEP_SPACING_MS depuis le dernier 0x10/0x11
} ws7b_step_kind_t;

typedef struct {
    uint8_t kind; // ws7b_step_kind_t
    uint8_t dcs;
    uint16_t ms;
} ws7b_step_t;

#define WS7B_PLAN_MAX 10

// Remplit `plan` (au moins WS7B_PLAN_MAX cases) avec les etapes pour passer de `is_on` a `want_on` ; rend leur nombre (0 si
// rien a faire). Ne fait rien d'autre : l'execution (et le chrono) est dans ws7b_io.c.
WS7B_FN size_t ws7b_panel_plan(int is_on, int want_on, ws7b_step_t plan[WS7B_PLAN_MAX]) {
    size_t n = 0;
    if (!!is_on == !!want_on) {
        return 0;
    }
    if (!want_on) {
        plan[n].kind = WS7B_STEP_VEIL_ON, plan[n].dcs = 0, plan[n].ms = 0, n++;
        // Le voile est envoye a la dalle par un transfert DMA2D asynchrone : on lui laisse le temps de partir avant « display off »
        // (ajout du BSP natif, la recette d'ESPHome enchaine sans attendre ; 40 ms ne coutent rien a l'extinction).
        plan[n].kind = WS7B_STEP_WAIT, plan[n].dcs = 0, plan[n].ms = WS7B_PANEL_VEIL_SETTLE_MS, n++;
        plan[n].kind = WS7B_STEP_DCS, plan[n].dcs = WS7B_DCS_DISPLAY_OFF, plan[n].ms = 0, n++;
        plan[n].kind = WS7B_STEP_SPACE, plan[n].dcs = 0, plan[n].ms = 0, n++;
        plan[n].kind = WS7B_STEP_DCS, plan[n].dcs = WS7B_DCS_SLEEP_IN, plan[n].ms = 0, n++;
    } else {
        plan[n].kind = WS7B_STEP_SPACE, plan[n].dcs = 0, plan[n].ms = 0, n++;
        plan[n].kind = WS7B_STEP_DCS, plan[n].dcs = WS7B_DCS_SLEEP_OUT, plan[n].ms = 0, n++;
        plan[n].kind = WS7B_STEP_WAIT, plan[n].dcs = 0, plan[n].ms = WS7B_PANEL_SLEEP_SPACING_MS, n++;
        plan[n].kind = WS7B_STEP_DCS, plan[n].dcs = WS7B_DCS_DISPLAY_ON, plan[n].ms = 0, n++;
        plan[n].kind = WS7B_STEP_VEIL_OFF, plan[n].dcs = 0, plan[n].ms = 0, n++;
    }
    return n;
}

// Ce qui reste a attendre (ms) pour respecter `spacing_ms` depuis `last_ms` ; 0 si c'est ecoule. Compte modulo 2^32 comme millis().
WS7B_FN uint32_t ws7b_wait_remaining(uint32_t now_ms, uint32_t last_ms, uint32_t spacing_ms) {
    const uint32_t elapsed = now_ms - last_ms; // modulo 2^32 voulu
    return elapsed >= spacing_ms ? 0U : spacing_ms - elapsed;
}

// ============================================================================================================================
// 7. Audio. ES8311 (sortie mono, ampli NS4150B) en I2C 0x18 ; ES7210 (entrees) en I2C 0x40 [S].
//    Volume de l'ES8311 : registre 0x32, pas de 0,5 dB, 0x00 = -95,5 dB, 0xBF = 0 dB, 0xFF = +32 dB [D][M]. A +32 dB le
//    haut-parleur SATURE (essai d'Axel a l'oreille le 17/10/2026 : 100 % sature, 80 % encore un peu, 75 % propre [M]).
//    Regles reprises du firmware ESPHome en service : le plafond est 0 dB (0xBF = 75 % de l'echelle du registre) ; le plancher
//    d'un volume NON NUL est 40 % de l'echelle (0x66 = -44,5 dB) parce que dessous « Axel n'entend RIEN » (18/10/2026 [M]) ; le
//    volume 0 coupe. Entre les deux, interpolation lineaire en pourcentage utilisateur.
// ============================================================================================================================
#define WS7B_ES8311_I2C_ADDR 0x18
#define WS7B_ES7210_I2C_ADDR 0x40
#define WS7B_ES8311_REG_VOLUME 0x32
#define WS7B_ES8311_REG_0DB 0xBF
#define WS7B_ES8311_REG_FLOOR 0x66 // 40 % de 255
#define WS7B_ES8311_REG_MAX_ALLOWED WS7B_ES8311_REG_0DB

// Volume utilisateur 0..100 (%) -> valeur du registre. 0 -> 0x00 (coupe) ; 1..100 -> 0x66..0xBF, arrondi au plus proche.
WS7B_FN uint8_t ws7b_volume_reg(int percent) {
    if (percent <= 0) {
        return 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    const int span = WS7B_ES8311_REG_0DB - WS7B_ES8311_REG_FLOOR; // 89
    return (uint8_t)(WS7B_ES8311_REG_FLOOR + (percent * span + 50) / 100);
}

// Gain en dixiemes de decibel d'une valeur du registre (0xBF -> 0, 0x66 -> -445, 0x00 -> -955, 0xFF -> +320).
WS7B_FN int ws7b_reg_to_decidb(uint8_t reg) { return ((int)reg - WS7B_ES8311_REG_0DB) * 5; }

// ============================================================================================================================
// 8. Memoire. Tampon de dessin LVGL : N lignes de la largeur de la dalle, en RAM interne (DMA), en double tampon.
//    Le Tab5 prend 50 lignes de 720 px (72 Ko, 144 Ko en double). A 1024 px de large, 50 lignes feraient 100 Ko (200 Ko en double) :
//    trop pour la memoire interne (jalon 1b, budget de memoire interne). La valeur par defaut est donc plus petite (Kconfig).
// ============================================================================================================================
// Hauteur par defaut du tampon de dessin (lignes de 1024 px). 25 lignes x 1024 px x 2 octets x 2 tampons = 100 Ko en RAM interne :
// moins que les 144 Ko (2 x 72 000 octets) du Tab5, ce que verifie test_carte_7b.cpp.
#define WS7B_DRAW_BUFFER_LINES_DEFAULT 25

WS7B_FN uint32_t ws7b_draw_buffer_bytes(int lines) {
    return (uint32_t)WS7B_LCD_H_RES * (uint32_t)lines * (WS7B_RGB565_BITS_PER_PIXEL / 8);
}

#ifdef __cplusplus
}
#endif

#endif // WS7B_PUR_H
