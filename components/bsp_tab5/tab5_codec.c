// Codecs audio du Tab5 : ES8388 (DAC, adresse 0x10) pour le haut-parleur et le casque, ES7210 (CAN, adresse 0x40) pour les
// microphones. Pilotes MINIMAUX ecrits ici plutot que le composant `esp_codec_dev` : ce composant n'est pas dans ce depot, l'ajouter
// change `dependencies.lock` (fichier partage avec la session du jalon 1b) et pese plus que ce qu'il faut ici (un DAC a regler, un
// CAN a mettre en route). Les sequences d'initialisation sont celles du firmware ESPHome du Tab5, qui tourne sur CETTE carte
// (composants `es8388` et `es7210` d'ESPHome 2026.9.0, lus le 07/11/2026) ; les noms de registres suivent leurs en-tetes.
//
// Les deux puces sont ESCLAVES du bus I2S : l'ESP32-P4 fournit MCLK = 256 x fs, BCLK et WS. Les registres s'ecrivent sans horloge,
// mais la machine d'etat interne de l'ES8388 (CHIPPOWER) et l'ES7210 (RESET) veulent les horloges en marche : tab5_codec_open() se
// fait donc bus I2S actif (voir tab5_audio.cpp).
//
// Ce qui n'est PAS verifie ici (rien ne le prouve sans mesure sur carte) : la ligne de sortie de l'ES8388 qui alimente le
// haut-parleur (LINE1, LINE2 ou les deux : l'essai sur carte la mesure avec le micro), et les registres d'identite de l'ES7210
// (0x3D / 0x3E, simplement journalises).
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_codec";

static i2c_master_dev_handle_t s_dac;
static i2c_master_dev_handle_t s_adc;

// --- ES8388 ----------------------------------------------------------------------------------------------------------------
#define ES8388_CONTROL1 0x00
#define ES8388_CONTROL2 0x01
#define ES8388_CHIPPOWER 0x02
#define ES8388_ADCPOWER 0x03
#define ES8388_DACPOWER 0x04
#define ES8388_MASTERMODE 0x08
#define ES8388_ADCCONTROL1 0x09
#define ES8388_ADCCONTROL3 0x0b
#define ES8388_ADCCONTROL4 0x0c
#define ES8388_ADCCONTROL5 0x0d
#define ES8388_ADCCONTROL7 0x0f
#define ES8388_ADCCONTROL8 0x10
#define ES8388_ADCCONTROL9 0x11
#define ES8388_ADCCONTROL10 0x12
#define ES8388_ADCCONTROL11 0x13
#define ES8388_ADCCONTROL12 0x14
#define ES8388_ADCCONTROL13 0x15
#define ES8388_ADCCONTROL14 0x16
#define ES8388_DACCONTROL1 0x17
#define ES8388_DACCONTROL2 0x18
#define ES8388_DACCONTROL3 0x19
#define ES8388_DACCONTROL4 0x1a // volume gauche, 0 = 0 dB ... 192 = -96 dB par pas de 0,5 dB
#define ES8388_DACCONTROL5 0x1b // volume droit
#define ES8388_DACCONTROL16 0x26
#define ES8388_DACCONTROL17 0x27
#define ES8388_DACCONTROL20 0x2a
#define ES8388_DACCONTROL21 0x2b
#define ES8388_DACCONTROL23 0x2d
#define ES8388_DACCONTROL24 0x2e // volume LOUT1
#define ES8388_DACCONTROL25 0x2f // ROUT1
#define ES8388_DACCONTROL26 0x30 // LOUT2
#define ES8388_DACCONTROL27 0x31 // ROUT2
#define ES8388_DAC_MUTE 0x04     // bit 2 de DACCONTROL3
#define ES8388_OUT_LINE1 0x30
#define ES8388_OUT_LINE2 0x0C
#define ES8388_OUT_BOTH 0x3C
#define ES8388_OUT_NONE 0xC0

static esp_err_t dac_w(uint8_t reg, uint8_t value) { return tab5_i2c_write_u8(s_dac, reg, value); }

#define DAC_W(reg, value) ESP_RETURN_ON_ERROR(dac_w((reg), (value)), TAG, "ES8388 reg 0x%02x", (reg))

static esp_err_t es8388_init(void) {
    // Sequence de ESPHome (es8388.cpp, setup()) : esclave, DAC en I2S 16 bits, rapport d'horloge 256, melangeur LIN1/RIN1 a
    // 0 dB, CAN et DAC partagent LRCK, machine d'etat relancee, puis CAN alimente (comme ESPHome : le DAC prend LRCK du CAN).
    ESP_RETURN_ON_ERROR(tab5_i2c_update_u8(s_dac, ES8388_DACCONTROL3, ES8388_DAC_MUTE, ES8388_DAC_MUTE), TAG, "mute");
    DAC_W(ES8388_MASTERMODE, 0x00);
    DAC_W(ES8388_CONTROL2, 0x50);
    DAC_W(ES8388_CHIPPOWER, 0x00);
    DAC_W(ES8388_CONTROL1, 0x12);
    DAC_W(ES8388_DACCONTROL1, 0x18);
    DAC_W(ES8388_DACCONTROL2, 0x02);
    DAC_W(ES8388_DACCONTROL16, 0x00);
    DAC_W(ES8388_DACCONTROL17, 0x90);
    DAC_W(ES8388_DACCONTROL20, 0x90);
    DAC_W(ES8388_DACCONTROL21, 0x80);
    DAC_W(ES8388_DACCONTROL23, 0x00);
    DAC_W(ES8388_ADCPOWER, 0xFF);
    DAC_W(ES8388_ADCCONTROL1, 0x00);
    DAC_W(ES8388_ADCCONTROL3, 0x02);
    DAC_W(ES8388_ADCCONTROL4, 0x0d);
    DAC_W(ES8388_ADCCONTROL5, 0x02);
    DAC_W(ES8388_ADCCONTROL8, 0x00);
    DAC_W(ES8388_ADCCONTROL9, 0x00);
    DAC_W(ES8388_ADCCONTROL10, 0xe2);
    DAC_W(ES8388_ADCCONTROL11, 0xa0);
    DAC_W(ES8388_ADCCONTROL12, 0x12);
    DAC_W(ES8388_ADCCONTROL13, 0x06);
    DAC_W(ES8388_ADCCONTROL14, 0xc3);
    DAC_W(ES8388_DACCONTROL21, 0x80);
    DAC_W(ES8388_CHIPPOWER, 0xF0);
    vTaskDelay(pdMS_TO_TICKS(2));
    DAC_W(ES8388_CHIPPOWER, 0x00);
    DAC_W(ES8388_ADCCONTROL7, 0x60);
    DAC_W(ES8388_DACCONTROL3,
          0x24); // rampe douce + sourdine : la sourdine ne se leve que pour jouer (tab5_dac_set_volume)
    // Volume au plus bas AVANT de lever la sourdine (le volume par defaut de la puce est 0 dB).
    DAC_W(ES8388_DACCONTROL4, 192);
    DAC_W(ES8388_DACCONTROL5, 192);
    DAC_W(ES8388_ADCPOWER, 0x09);
    return ESP_OK;
}

esp_err_t tab5_dac_set_output(tab5_dac_line_t line) {
    uint8_t out1 = 0;
    uint8_t out2 = 0;
    uint8_t power = ES8388_OUT_NONE;
    switch (line) {
    case TAB5_DAC_LINE1:
        out1 = 0x1E; // 0x00 = -30 dB, 0x1E = 0 dB (ESPHome)
        power = ES8388_OUT_LINE1;
        break;
    case TAB5_DAC_LINE2:
        out2 = 0x1E;
        power = ES8388_OUT_LINE2;
        break;
    case TAB5_DAC_BOTH:
        out1 = 0x1E;
        out2 = 0x1E;
        power = ES8388_OUT_BOTH;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    DAC_W(ES8388_DACCONTROL24, out1);
    DAC_W(ES8388_DACCONTROL25, out1);
    DAC_W(ES8388_DACCONTROL26, out2);
    DAC_W(ES8388_DACCONTROL27, out2);
    DAC_W(ES8388_DACPOWER, power);
    return ESP_OK;
}

// Volume en pas de 0,5 dB (0..192) et sourdine du DAC. La sourdine est un bit de DACCONTROL3 (les autres bits sont conserves).
esp_err_t tab5_dac_set_volume(int step, bool mute) {
    if (step < 0) {
        step = 0;
    }
    if (step > 192) {
        step = 192;
    }
    DAC_W(ES8388_DACCONTROL4, (uint8_t)step);
    DAC_W(ES8388_DACCONTROL5, (uint8_t)step);
    return tab5_i2c_update_u8(s_dac, ES8388_DACCONTROL3, ES8388_DAC_MUTE, mute ? ES8388_DAC_MUTE : 0);
}

esp_err_t tab5_dac_read_reg(uint8_t reg, uint8_t *value) { return tab5_i2c_read_u8(s_dac, reg, value); }

// --- ES7210 ----------------------------------------------------------------------------------------------------------------
static esp_err_t adc_w(uint8_t reg, uint8_t value) { return tab5_i2c_write_u8(s_adc, reg, value); }

#define ADC_W(reg, value) ESP_RETURN_ON_ERROR(adc_w((reg), (value)), TAG, "ES7210 reg 0x%02x", (reg))

typedef struct {
    uint32_t mclk;
    uint32_t lrclk;
    uint8_t adc_div;
    uint8_t dll;
    uint8_t doubler;
    uint8_t osr;
    uint8_t lrck_h;
    uint8_t lrck_l;
} es7210_coeff_t;

// Lignes de la table d'ESPHome (es7210_const.h) pour MCLK = 256 x fs : seules cadences utilisables avec ce BSP.
static const es7210_coeff_t s_coeff[] = {
    {4096000, 16000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
    {12288000, 48000, 0x01, 0x01, 0x01, 0x20, 0x01, 0x00},
};

static esp_err_t es7210_init(uint32_t rate, uint8_t gain_reg) {
    const es7210_coeff_t *c = NULL;
    for (size_t i = 0; i < sizeof(s_coeff) / sizeof(s_coeff[0]); i++) {
        if (s_coeff[i].lrclk == rate) {
            c = &s_coeff[i];
        }
    }
    if (c == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    ADC_W(0x00, 0xff); // reset logiciel
    ADC_W(0x00, 0x32);
    ADC_W(0x01, 0x3f);
    ADC_W(0x09, 0x30); // temps d'initialisation
    ADC_W(0x0a, 0x30);
    ADC_W(0x23, 0x2a); // filtres passe-haut
    ADC_W(0x22, 0x0a);
    ADC_W(0x20, 0x0a);
    ADC_W(0x21, 0x2a);
    ESP_RETURN_ON_ERROR(tab5_i2c_update_u8(s_adc, 0x08, 0x01, 0x00), TAG, "mode esclave");
    ADC_W(0x40, 0xC3); // alimentation analogique
    ADC_W(0x41, 0x70); // polarisation des micros
    ADC_W(0x42, 0x70);
    ADC_W(0x11, 0x60); // I2S 16 bits
    ADC_W(0x12, 0x00); // micros 1 et 2 sur SDOUT1 (pas de TDM)
    ADC_W(0x02, (uint8_t)(c->adc_div | (c->doubler << 6) | (c->dll << 7)));
    ADC_W(0x07, c->osr);
    ADC_W(0x04, c->lrck_h);
    ADC_W(0x05, c->lrck_l);
    for (uint8_t i = 0; i < 4; i++) {
        ESP_RETURN_ON_ERROR(tab5_i2c_update_u8(s_adc, (uint8_t)(0x43 + i), 0x10, 0x00), TAG, "gain");
    }
    ADC_W(0x4B, 0xff);
    ADC_W(0x4C, 0xff);
    for (uint8_t i = 0; i < 4; i++) { // micros 1 a 4, comme ESPHome : horloge, alimentation du groupe, gain
        ESP_RETURN_ON_ERROR(tab5_i2c_update_u8(s_adc, 0x01, 0x0b, 0x00), TAG, "horloge micro");
        ADC_W(i < 2 ? 0x4B : 0x4C, 0x00);
        ESP_RETURN_ON_ERROR(tab5_i2c_update_u8(s_adc, (uint8_t)(0x43 + i), 0x10, 0x10), TAG, "gain on");
        ESP_RETURN_ON_ERROR(tab5_i2c_update_u8(s_adc, (uint8_t)(0x43 + i), 0x0f, gain_reg), TAG, "gain");
    }
    ADC_W(0x47, 0x08); // alimentation des micros
    ADC_W(0x48, 0x08);
    ADC_W(0x49, 0x08);
    ADC_W(0x4A, 0x08);
    ADC_W(0x06, 0x04); // DLL coupee
    ADC_W(0x4B, 0x0F);
    ADC_W(0x4C, 0x0F);
    ADC_W(0x00, 0x71); // mise en route
    ADC_W(0x00, 0x41);
    return ESP_OK;
}

esp_err_t tab5_adc_read_ids(uint8_t *id1, uint8_t *id2) {
    ESP_RETURN_ON_ERROR(tab5_i2c_read_u8(s_adc, 0x3D, id1), TAG, "id1");
    return tab5_i2c_read_u8(s_adc, 0x3E, id2);
}

esp_err_t tab5_adc_read_reg(uint8_t reg, uint8_t *value) { return tab5_i2c_read_u8(s_adc, reg, value); }

// --- ensemble ------------------------------------------------------------------------------------------------------------------
esp_err_t tab5_codec_open(uint32_t rate) {
    if (s_dac == NULL) {
        ESP_RETURN_ON_ERROR(tab5_i2c_attach(TAB5_I2C_ADDR_ES8388, &s_dac), TAG, "ES8388 absent du bus");
    }
    if (s_adc == NULL) {
        ESP_RETURN_ON_ERROR(tab5_i2c_attach(TAB5_I2C_ADDR_ES7210, &s_adc), TAG, "ES7210 absent du bus");
    }
    ESP_RETURN_ON_ERROR(es8388_init(), TAG, "ES8388");
    // Gain des micros : 30 dB, valeur du firmware ESPHome en service (registre = 30,5 / 3 = 10).
    ESP_RETURN_ON_ERROR(es7210_init(rate, 10), TAG, "ES7210");
    return tab5_dac_set_output(TAB5_DAC_BOTH);
}
