// Bus audio I2S du Tab5 : UN seul port, maitre, en duplex (emission vers l'ES8388, reception depuis l'ES7210). Les deux canaux
// partagent MCLK / BCLK / WS : meme cadence, meme format (Philips, stereo, 16 bits), obligatoirement. En C parce que les macros de
// configuration d'ESP-IDF (initialiseurs designes imbriques) sont ecrites pour le C.
//
// Quand une direction est desactivee, les horloges continuent tant que l'autre tourne ; quand les deux le sont, elles s'arretent
// (le CAN ES7210 et le DAC ES8388 sont esclaves). Le bus s'ouvre une fois (tab5_i2s_open) et reste ouvert : les tampons DMA
// prennent de la RAM INTERNE (obligatoire pour le DMA), on les garde petits : 4 tampons de 160 trames par direction, soit
// 4 x 160 x 4 octets = 2,5 Kio chacune, plus les descripteurs. La mesure exacte est rendue a l'ouverture (internal_ram_taken).
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_i2s";

#define I2S_DMA_DESC 4
#define I2S_DMA_FRAMES 160

static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static bool s_tx_on;
static bool s_rx_on;
static uint32_t s_rate;
static volatile uint32_t s_tx_overflows;
static volatile uint32_t s_rx_overflows;

// Appeles depuis l'interruption du DMA : seulement un compteur.
static bool tx_overflow_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user) {
    (void)handle;
    (void)event;
    (void)user;
    s_tx_overflows++;
    return false;
}

static bool rx_overflow_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user) {
    (void)handle;
    (void)event;
    (void)user;
    s_rx_overflows++;
    return false;
}

bool tab5_i2s_is_open(void) { return s_tx != NULL; }

uint32_t tab5_i2s_rate(void) { return s_rate; }

esp_err_t tab5_i2s_open(uint32_t rate, uint32_t *ram_taken) {
    if (s_tx != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t avant = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan.dma_desc_num = I2S_DMA_DESC;
    chan.dma_frame_num = I2S_DMA_FRAMES;
    chan.auto_clear_after_cb =
        true; // sans donnees a envoyer, le DMA envoie du silence (et non la fin du dernier tampon)
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, &s_tx, &s_rx), TAG, "i2s_new_channel");

    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = TAB5_PIN_I2S_MCLK,
                .bclk = TAB5_PIN_I2S_BCLK,
                .ws = TAB5_PIN_I2S_WS,
                .dout = TAB5_PIN_I2S_DOUT,
                .din = TAB5_PIN_I2S_DIN,
            },
    };
    std.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256; // les deux codecs sont regles pour MCLK = 256 x fs
    esp_err_t err = i2s_channel_init_std_mode(s_tx, &std);
    if (err == ESP_OK) {
        err = i2s_channel_init_std_mode(s_rx, &std);
    }
    if (err == ESP_OK) {
        const i2s_event_callbacks_t tx_cb = {.on_send_q_ovf = tx_overflow_cb};
        const i2s_event_callbacks_t rx_cb = {.on_recv_q_ovf = rx_overflow_cb};
        err = i2s_channel_register_event_callback(s_tx, &tx_cb, NULL);
        if (err == ESP_OK) {
            err = i2s_channel_register_event_callback(s_rx, &rx_cb, NULL);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialisation du bus I2S : %s", esp_err_to_name(err));
        i2s_del_channel(s_tx);
        i2s_del_channel(s_rx);
        s_tx = NULL;
        s_rx = NULL;
        return err;
    }
    s_rate = rate;
    s_tx_on = false;
    s_rx_on = false;
    const size_t apres = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (ram_taken != NULL) {
        *ram_taken = avant > apres ? (uint32_t)(avant - apres) : 0;
    }
    return ESP_OK;
}

// Met chaque direction dans l'etat demande (activer ou desactiver seulement si besoin).
// La RECEPTION SEULE ne marche pas : une lecture avec la seule voie de reception activee expire sans rien rendre (constate sur
// carte le 07/11/2026 : ESP_ERR_TIMEOUT, 0 trame ; cause non elucidee, probablement les horloges maitres portees par la voie
// d'emission). Demander la reception active donc aussi l'emission ; elle envoie du silence tant qu'on ne lui donne rien
// (auto_clear_after_cb), le DAC reste en sourdine et l'amplificateur eteint.
esp_err_t tab5_i2s_enable(bool tx, bool rx) {
    if (s_tx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    tx = tx || rx;
    if (rx != s_rx_on && !rx) {
        ESP_RETURN_ON_ERROR(i2s_channel_disable(s_rx), TAG, "rx off");
        s_rx_on = false;
    }
    if (tx != s_tx_on && !tx) {
        ESP_RETURN_ON_ERROR(i2s_channel_disable(s_tx), TAG, "tx off");
        s_tx_on = false;
    }
    if (rx && !s_rx_on) {
        ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "rx on");
        s_rx_on = true;
    }
    if (tx && !s_tx_on) {
        ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "tx on");
        s_tx_on = true;
    }
    return ESP_OK;
}

esp_err_t tab5_i2s_write(const void *data, size_t bytes, size_t *written, uint32_t timeout_ms) {
    if (s_tx == NULL || !s_tx_on) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_write(s_tx, data, bytes, written, timeout_ms);
}

esp_err_t tab5_i2s_read(void *data, size_t bytes, size_t *read, uint32_t timeout_ms) {
    if (s_rx == NULL || !s_rx_on) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_read(s_rx, data, bytes, read, timeout_ms);
}

// Vide la file de reception (ce qui s'y est accumule depuis la mise en route) : coupure puis remise en route de la seule reception.
esp_err_t tab5_i2s_flush_rx(void) {
    if (s_rx == NULL || !s_rx_on) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_disable(s_rx), TAG, "rx off");
    return i2s_channel_enable(s_rx);
}

void tab5_i2s_counters(uint32_t *tx_overflows, uint32_t *rx_overflows) {
    if (tx_overflows != NULL) {
        *tx_overflows = s_tx_overflows;
    }
    if (rx_overflows != NULL) {
        *rx_overflows = s_rx_overflows;
    }
}

esp_err_t tab5_i2s_close(void) {
    if (s_tx == NULL) {
        return ESP_OK;
    }
    (void)tab5_i2s_enable(false, false);
    esp_err_t err = i2s_del_channel(s_tx);
    const esp_err_t err_rx = i2s_del_channel(s_rx);
    s_tx = NULL;
    s_rx = NULL;
    s_rate = 0;
    return err != ESP_OK ? err : err_rx;
}
