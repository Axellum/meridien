// Audio de la Waveshare 7B : ES8311 (sortie mono -> ampli NS4150B -> haut-parleur) et ES7210 (entrees des microphones) sur un meme
// bus I2S, via esp_codec_dev 1.5.11. board.h n'a pas (encore) d'audio : ce fichier est un module de la couche carte, appele par
// l'essai matiere (WS7B_ESSAI_AUDIO) et prevu pour la future couche audio commune (proposition a la session materiel-tab5 :
// docs/carte-7b.md, section « Ce qui manque a board.h »).
//
// Regles de securite du son, reprises du firmware ESPHome en service (memoire d'Axel es8311-volume-gain-7b, 17 et 18/10/2026) :
//  - le volume est ecrit DIRECTEMENT dans le registre 0x32 de l'ES8311 par ws7b_volume_reg() (include/ws7b_pur.h, testee sur le
//    PC) : jamais de gain positif (le plafond est 0 dB = 0xBF), plancher audible a 40 % de l'echelle, 0 = coupe ;
//  - l'ampli n'est allume que le temps d'un son (GPIO53), pour eviter les craquements et la consommation a vide ;
//  - l'ES8311 est commande par son adresse I2C 7 bits 0x18 ; esp_codec_dev attend la forme 8 bits (0x30), d'ou le decalage.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ws7b_internal.h"

static const char *TAG = "ws7b_audio";

#define AUDIO_RATE_HZ 16000
#define AUDIO_BITS 16
#define TONE_AMPLITUDE 12000   // sur 32767 : environ -8,7 dBFS ; le volume du codec decide du niveau reel
#define TONE_CHUNK_SAMPLES 320 // 20 ms
#define AMP_SETTLE_MS 30       // laisse le codec se stabiliser avant d'allumer l'ampli, et l'ampli avant de couper
#define SELFTEST_VOLUME_PERCENT                                                                                        \
    60 // 0xBF = 75 % de l'echelle du registre : 60 % utilisateur = -17,5 dB, bien en dessous du plafond
#define SELFTEST_TONE_HZ 440
#define SELFTEST_TONE_MS 300
#define SELFTEST_MIC_MS 500

_Static_assert((ES8311_CODEC_DEFAULT_ADDR >> 1) == WS7B_ES8311_I2C_ADDR,
               "adresse ES8311 : forme 8 bits d'esp_codec_dev != 0x18");
_Static_assert((ES7210_CODEC_DEFAULT_ADDR >> 1) == WS7B_ES7210_I2C_ADDR,
               "adresse ES7210 : forme 8 bits d'esp_codec_dev != 0x40");

static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static const audio_codec_data_if_t *s_data_if;
static const audio_codec_ctrl_if_t *s_ctrl_es8311;
static esp_codec_dev_handle_t s_speaker;
static esp_codec_dev_handle_t s_mic;
static int s_volume_percent = 0;

static esp_err_t i2s_start(void) {
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.auto_clear = true; // pas de reste de donnees dans les tampons DMA
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, &s_tx, &s_rx), TAG, "canaux I2S");

    const i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .mclk = WS7B_PIN_I2S_MCLK,
                .bclk = WS7B_PIN_I2S_BCLK,
                .ws = WS7B_PIN_I2S_LRCK,
                .dout = WS7B_PIN_I2S_DOUT,
                .din = WS7B_PIN_I2S_DIN,
                .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
            },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std), TAG, "I2S sortie");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std), TAG, "I2S entree");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "I2S sortie active");
    return i2s_channel_enable(s_rx);
}

esp_err_t ws7b_audio_init(void) {
    if (s_speaker != NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(i2s_start(), TAG, "I2S");

    audio_codec_i2s_cfg_t i2s_cfg = {.port = I2S_NUM_0, .rx_handle = s_rx, .tx_handle = s_tx};
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(s_data_if != NULL, ESP_FAIL, TAG, "interface de donnees I2S");

    // Sortie : ES8311. L'ampli est commande ici (pa_pin = -1 : esp_codec_dev n'y touche pas).
    audio_codec_i2c_cfg_t i2c_8311 = {
        .port = WS7B_I2C_PORT, .addr = ES8311_CODEC_DEFAULT_ADDR, .bus_handle = ws7b_i2c_bus()};
    s_ctrl_es8311 = audio_codec_new_i2c_ctrl(&i2c_8311);
    ESP_RETURN_ON_FALSE(s_ctrl_es8311 != NULL, ESP_FAIL, TAG, "controle I2C de l'ES8311");
    es8311_codec_cfg_t es8311 = {
        .ctrl_if = s_ctrl_es8311,
        .gpio_if = audio_codec_new_gpio(),
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = -1,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = {.pa_voltage = 5.0f, .codec_dac_voltage = 3.3f}, // valeurs du BSP Waveshare
    };
    const audio_codec_if_t *dac = es8311_codec_new(&es8311);
    ESP_RETURN_ON_FALSE(dac != NULL, ESP_FAIL, TAG, "ES8311 (codec absent ou muet sur le bus I2C ?)");
    esp_codec_dev_cfg_t spk_cfg = {.dev_type = ESP_CODEC_DEV_TYPE_OUT, .codec_if = dac, .data_if = s_data_if};
    s_speaker = esp_codec_dev_new(&spk_cfg);
    ESP_RETURN_ON_FALSE(s_speaker != NULL, ESP_FAIL, TAG, "peripherique de sortie");

    // Entrees : ES7210, microphones 1 et 2 ; en I2S mono seule la voie gauche (micro 1) est lue.
    audio_codec_i2c_cfg_t i2c_7210 = {
        .port = WS7B_I2C_PORT, .addr = ES7210_CODEC_DEFAULT_ADDR, .bus_handle = ws7b_i2c_bus()};
    const audio_codec_ctrl_if_t *ctrl_7210 = audio_codec_new_i2c_ctrl(&i2c_7210);
    ESP_RETURN_ON_FALSE(ctrl_7210 != NULL, ESP_FAIL, TAG, "controle I2C de l'ES7210");
    es7210_codec_cfg_t es7210 = {
        .ctrl_if = ctrl_7210,
        .master_mode = false,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2,
        .mclk_src = ES7210_MCLK_FROM_PAD,
        .mclk_div = 0,
    };
    const audio_codec_if_t *adc = es7210_codec_new(&es7210);
    ESP_RETURN_ON_FALSE(adc != NULL, ESP_FAIL, TAG, "ES7210 (codec absent ou muet sur le bus I2C ?)");
    esp_codec_dev_cfg_t mic_cfg = {.dev_type = ESP_CODEC_DEV_TYPE_IN, .codec_if = adc, .data_if = s_data_if};
    s_mic = esp_codec_dev_new(&mic_cfg);
    ESP_RETURN_ON_FALSE(s_mic != NULL, ESP_FAIL, TAG, "peripherique d'entree");
    ESP_LOGI(TAG, "audio pret : %d Hz, %d bits, mono", AUDIO_RATE_HZ, AUDIO_BITS);
    return ESP_OK;
}

esp_err_t ws7b_audio_set_volume_percent(int percent) {
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    s_volume_percent = percent;
    if (s_ctrl_es8311 == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    // Ecriture directe du registre 0x32 : la valeur vient de ws7b_volume_reg(), plafonnee a 0 dB. Le registre n'a un effet qu'une
    // fois le codec ouvert (esp_codec_dev_open le remet a sa valeur par defaut) : ws7b_audio_play_tone() l'ecrit apres l'ouverture.
    uint8_t reg = ws7b_volume_reg(percent); // non const : write_reg() prend un `void *`
    const int err = s_ctrl_es8311->write_reg(s_ctrl_es8311, WS7B_ES8311_REG_VOLUME, 1, &reg, 1);
    return err == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t ws7b_audio_play_tone(int freq_hz, int duration_ms) {
    ESP_RETURN_ON_ERROR(ws7b_audio_init(), TAG, "audio");
    const esp_codec_dev_sample_info_t fs = {.bits_per_sample = AUDIO_BITS,
                                            .channel = 1,
                                            .channel_mask = 0,
                                            .sample_rate = AUDIO_RATE_HZ,
                                            .mclk_multiple = 0};
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_speaker, (esp_codec_dev_sample_info_t *)&fs) == ESP_CODEC_DEV_OK, ESP_FAIL,
                        TAG, "ouverture de la sortie");
    esp_codec_dev_set_out_mute(s_speaker, s_volume_percent == 0);
    const esp_err_t vol_err = ws7b_audio_set_volume_percent(s_volume_percent); // APRES l'ouverture (voir plus haut)

    int16_t *buf = malloc(TONE_CHUNK_SAMPLES * sizeof(int16_t));
    esp_err_t err = vol_err;
    if (buf == NULL) {
        err = ESP_ERR_NO_MEM;
    }
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(AMP_SETTLE_MS));
        ws7b_amp_set(true);
        const int total = (AUDIO_RATE_HZ / 1000) * duration_ms;
        int done = 0;
        double phase = 0.0;
        const double step = 2.0 * M_PI * (double)freq_hz / (double)AUDIO_RATE_HZ;
        while (done < total && err == ESP_OK) {
            const int n = (total - done) < TONE_CHUNK_SAMPLES ? (total - done) : TONE_CHUNK_SAMPLES;
            for (int i = 0; i < n; i++) {
                buf[i] = (int16_t)(TONE_AMPLITUDE * sin(phase));
                phase += step;
            }
            if (esp_codec_dev_write(s_speaker, buf, n * (int)sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
                err = ESP_FAIL;
            }
            done += n;
        }
        vTaskDelay(pdMS_TO_TICKS(AMP_SETTLE_MS));
        ws7b_amp_set(false);
    }
    free(buf);
    esp_codec_dev_close(s_speaker);
    return err;
}

esp_err_t ws7b_audio_mic_level(int32_t *rms, int32_t *peak, int duration_ms) {
    ESP_RETURN_ON_ERROR(ws7b_audio_init(), TAG, "audio");
    const esp_codec_dev_sample_info_t fs = {.bits_per_sample = AUDIO_BITS,
                                            .channel = 1,
                                            .channel_mask = 0,
                                            .sample_rate = AUDIO_RATE_HZ,
                                            .mclk_multiple = 0};
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_mic, (esp_codec_dev_sample_info_t *)&fs) == ESP_CODEC_DEV_OK, ESP_FAIL,
                        TAG, "ouverture de l'entree");
    int16_t *buf = malloc(TONE_CHUNK_SAMPLES * sizeof(int16_t));
    esp_err_t err = (buf == NULL) ? ESP_ERR_NO_MEM : ESP_OK;
    uint64_t sum = 0;
    int32_t max_abs = 0;
    int count = 0;
    const int total = (AUDIO_RATE_HZ / 1000) * duration_ms;
    while (err == ESP_OK && count < total) {
        if (esp_codec_dev_read(s_mic, buf, TONE_CHUNK_SAMPLES * (int)sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            err = ESP_FAIL;
            break;
        }
        for (int i = 0; i < TONE_CHUNK_SAMPLES; i++) {
            const int32_t v = buf[i];
            const int32_t a = v < 0 ? -v : v;
            sum += (uint64_t)((int64_t)v * v);
            if (a > max_abs) {
                max_abs = a;
            }
        }
        count += TONE_CHUNK_SAMPLES;
    }
    free(buf);
    esp_codec_dev_close(s_mic);
    if (err == ESP_OK && count > 0) {
        *rms = (int32_t)sqrt((double)sum / (double)count);
        *peak = max_abs;
    }
    return err;
}

void ws7b_audio_selftest(void) {
    esp_err_t err = ws7b_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESSAI AUDIO : initialisation refusee (%s)", esp_err_to_name(err));
        return;
    }
    ws7b_audio_set_volume_percent(SELFTEST_VOLUME_PERCENT);
    ESP_LOGI(TAG, "ESSAI AUDIO : tonalite %d Hz pendant %d ms au volume %d %% (registre 0x%02x = %d.%d dB)",
             SELFTEST_TONE_HZ, SELFTEST_TONE_MS, SELFTEST_VOLUME_PERCENT, ws7b_volume_reg(SELFTEST_VOLUME_PERCENT),
             ws7b_reg_to_decidb(ws7b_volume_reg(SELFTEST_VOLUME_PERCENT)) / 10,
             abs(ws7b_reg_to_decidb(ws7b_volume_reg(SELFTEST_VOLUME_PERCENT))) % 10);
    err = ws7b_audio_play_tone(SELFTEST_TONE_HZ, SELFTEST_TONE_MS);
    ESP_LOGI(TAG, "ESSAI AUDIO : sortie %s",
             err == ESP_OK ? "OK (envoyee ; l'ecoute reste a faire par une personne)" : esp_err_to_name(err));
    int32_t rms = 0;
    int32_t peak = 0;
    err = ws7b_audio_mic_level(&rms, &peak, SELFTEST_MIC_MS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ESSAI AUDIO : micro (voie gauche) pendant %d ms : RMS %d, crete %d sur 32767", SELFTEST_MIC_MS,
                 (int)rms, (int)peak);
    } else {
        ESP_LOGE(TAG, "ESSAI AUDIO : micro refuse (%s)", esp_err_to_name(err));
    }
}
