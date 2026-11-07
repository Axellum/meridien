// Audio du Tab5 (implementation de board_audio_* de board.h) : sortie haut-parleur / casque par l'ES8388, entree des microphones
// par l'ES7210, sur le bus I2S partage. Les CALCULS (courbe de volume perçu, niveaux en dBFS, Goertzel, sinus de test) sont dans le
// noyau pur (ecran::volume, ecran::audio), teste sur le PC avec mutations ; ce fichier pilote le materiel et mesure.
//
// Choix :
//   - UNE cadence pour tout le bus : 16 000 Hz, stereo, 16 bits. Lecture et enregistrement d'un meme port I2S partagent MCLK, BCLK
//     et WS : pas de cadences differentes en meme temps (le firmware ESPHome du Tab5, lui, joue a 48 kHz et ecoute a 16 kHz, d'ou
//     son « pas de duplex »). Les codecs savent aussi 48 kHz ; la couche carte ne l'expose pas encore (tab5_i2s_open(rate)).
//   - Ouverture PARESSEUSE : rien n'est configure, aucune RAM prise tant qu'une fonction audio n'est pas appelee. Une fois ouvert,
//     le bus reste ouvert ; les horloges ne tournent que pendant un son ou une ecoute, et l'amplificateur n'est allume que
//     pendant un son (SPEAKER_EN, expandeur 0x43 bit 1).
//   - PSRAM pour tout ce qui est gros (tampon d'enregistrement, table du sinus) ; seuls les tampons DMA du bus (petits, voir
//     tab5_i2s.c) sont en RAM interne, car le DMA l'exige.
//   - Volume PERÇU : voir ecran::volume. Le plafond du Tab5 (-6 dB) est une ESTIME de prudence, a regler a l'oreille : le haut-parleur
//     est a quelques centimetres du micro et l'amplificateur est celui d'une tablette de table.
#include <string.h>

#include "board.h"
#include "ecran/niveau_audio.hpp"
#include "ecran/volume.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tab5_internal.h"

namespace {

namespace aud = ecran::audio;
namespace vol = ecran::volume;

constexpr char TAG[] = "tab5_audio";

constexpr uint32_t kCadence = 16000;
constexpr int kPlafondDbX10 = -60; // -6 dB
constexpr int kNiveauInitial = 30;
constexpr size_t kBloc = 160;          // trames par ecriture ou lecture (10 ms)
constexpr uint32_t kRampeTrames = 160; // 10 ms de montee et de descente : pas de claquement
constexpr uint32_t kPeriodeMaxTrames = 8000;
constexpr uint32_t kRejetMs = 500; // stabilisation du micro apres la mise en route des horloges
constexpr uint32_t kDureeMicMaxMs = 5000;
constexpr uint32_t kDureeSonMaxMs = 10000;

StaticSemaphore_t s_verrou_stockage;
bool s_ouvert = false;
bool s_ampli = false;
bool s_en_lecture = false;
int s_niveau = kNiveauInitial;
uint32_t s_ram_prise = 0;

// Un seul appel audio a la fois (le bus est un seul port). Le verrou est cree au premier usage (statique local : l'initialisation
// est protegee par le compilateur si deux taches arrivent ensemble).
SemaphoreHandle_t poignee_verrou() {
    static const SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&s_verrou_stockage);
    return h;
}

struct Verrou {
    Verrou() { xSemaphoreTake(poignee_verrou(), portMAX_DELAY); }
    ~Verrou() { xSemaphoreGive(poignee_verrou()); }
    Verrou(const Verrou &) = delete;
    Verrou &operator=(const Verrou &) = delete;
};

void *tampon_psram(size_t octets) {
    void *p = heap_caps_malloc(octets, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p != nullptr ? p : heap_caps_malloc(octets, MALLOC_CAP_8BIT);
}

constexpr uint32_t pgcd(uint32_t a, uint32_t b) { return b == 0 ? a : pgcd(b, a % b); }

esp_err_t appliquer_volume(bool muet) {
    return tab5_dac_set_volume(vol::pas_pour_niveau(s_niveau, kPlafondDbX10), muet || vol::est_muet(s_niveau));
}

esp_err_t ampli(bool on) {
    const esp_err_t err = tab5_expander_output(TAB5_EXPANDER_ECRAN, TAB5_EXPANDER_SPEAKER_EN, on ? 1 : 0);
    if (err == ESP_OK) {
        s_ampli = on;
    }
    return err;
}

// Ouverture (sous verrou) : bus I2S, horloges en marche le temps de configurer les codecs, DAC en sourdine, amplificateur eteint.
esp_err_t ouvrir() {
    if (s_ouvert) {
        return ESP_OK;
    }
    esp_err_t err = tab5_i2s_open(kCadence, &s_ram_prise);
    if (err != ESP_OK) {
        return err;
    }
    err = tab5_i2s_enable(true, true); // les codecs veulent leurs horloges pour leur mise en route
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
        err = tab5_codec_open(kCadence);
    }
    uint8_t id1 = 0;
    uint8_t id2 = 0;
    if (err == ESP_OK && tab5_adc_read_ids(&id1, &id2) == ESP_OK) {
        ESP_LOGI(TAG, "ES7210 : registres d'identité 0x3D = 0x%02X, 0x3E = 0x%02X", id1, id2);
    }
    (void)tab5_i2s_enable(false, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "audio non ouvert : %s", esp_err_to_name(err));
        (void)tab5_i2s_close();
        return err;
    }
    (void)ampli(false);
    (void)appliquer_volume(true);
    s_ouvert = true;
    ESP_LOGI(TAG, "audio ouvert : %u Hz, stéréo 16 bits, RAM interne prise par le bus : %u octets", (unsigned)kCadence,
             (unsigned)s_ram_prise);
    return ESP_OK;
}

struct Flux {
    uint32_t ecrites = 0;
    uint32_t lues = 0;
    uint32_t bloc_le_plus_lent_ms = 0;
};

// Emet un sinus (rampe d'entree et de sortie) et, si `rx` n'est pas nul, lit le micro en meme temps, un bloc contre un bloc. Sous
// verrou, horloges et amplificateur deja en marche. `rx` doit pouvoir contenir `trames + 4 * kBloc` trames stereo.
esp_err_t emettre_ton(uint32_t hz, int32_t crete_dbfs_x10, uint32_t trames, int16_t *rx, Flux &flux) {
    const uint32_t periode = kCadence / pgcd(hz, kCadence);
    if (hz == 0 || hz * 2 >= kCadence || periode > kPeriodeMaxTrames) {
        return ESP_ERR_INVALID_ARG;
    }
    const int32_t amplitude = aud::amplitude_pour_dbfs_x10(crete_dbfs_x10);
    int16_t *table = static_cast<int16_t *>(tampon_psram(periode * sizeof(int16_t)));
    if (table == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    for (uint32_t i = 0; i < periode; i++) {
        table[i] = aud::echantillon_sinus(hz, kCadence, i, amplitude);
    }
    int16_t bloc[kBloc * 2];
    int16_t lecture[kBloc * 2];
    esp_err_t err = ESP_OK;
    const uint32_t blocs = (trames + kBloc - 1) / kBloc + 4; // 4 blocs de silence a la fin : le DMA vide ce qui reste
    for (uint32_t b = 0; b < blocs && err == ESP_OK; b++) {
        for (uint32_t i = 0; i < kBloc; i++) {
            const uint32_t n = b * kBloc + i;
            int32_t v = 0;
            if (n < trames) {
                const uint32_t reste = trames - 1 - n;
                const uint32_t proche = n < reste ? n : reste;
                const int32_t gain = proche >= kRampeTrames ? 256 : static_cast<int32_t>(proche * 256U / kRampeTrames);
                v = (table[n % periode] * gain) / 256;
            }
            bloc[2 * i] = static_cast<int16_t>(v);
            bloc[2 * i + 1] = static_cast<int16_t>(v);
        }
        const int64_t t0 = esp_timer_get_time();
        size_t ecrit = 0;
        err = tab5_i2s_write(bloc, sizeof(bloc), &ecrit, 300);
        flux.ecrites += static_cast<uint32_t>(ecrit / 4);
        if (err == ESP_OK && rx != nullptr) {
            size_t lu = 0;
            err = tab5_i2s_read(lecture, sizeof(lecture), &lu, 300);
            if (err == ESP_OK) {
                memcpy(rx + static_cast<size_t>(flux.lues) * 2, lecture, lu);
                flux.lues += static_cast<uint32_t>(lu / 4);
            }
        }
        const uint32_t ms = static_cast<uint32_t>((esp_timer_get_time() - t0) / 1000);
        if (ms > flux.bloc_le_plus_lent_ms) {
            flux.bloc_le_plus_lent_ms = ms;
        }
    }
    heap_caps_free(table);
    return err;
}

// Niveau d'une voie d'un tampon stereo entrelace de `trames` trames, hors la phase de stabilisation.
aud::Niveau niveau_voie(const int16_t *stereo, uint32_t trames, int voie) {
    const uint32_t rejet = kCadence * kRejetMs / 1000;
    if (trames <= rejet) {
        return aud::Niveau{};
    }
    return aud::mesurer(stereo + rejet * 2 + voie, trames - rejet, 2);
}

int32_t ton_voie(const int16_t *stereo, uint32_t trames, int voie, uint32_t hz) {
    const uint32_t rejet = kCadence * kRejetMs / 1000;
    if (hz == 0 || trames <= rejet + 64) {
        return aud::kSilence_x10;
    }
    return aud::goertzel_dbfs_x10(stereo + rejet * 2 + voie, trames - rejet, hz, kCadence, 2);
}

// Ecoute `duration_ms` dans un tampon PSRAM rendu dans `tampon` (a liberer avec heap_caps_free), `lues` trames stereo. Sous verrou.
esp_err_t ecouter(uint32_t duration_ms, int16_t *&tampon, uint32_t &lues) {
    tampon = nullptr;
    lues = 0;
    if (duration_ms <= kRejetMs + 100 || duration_ms > kDureeMicMaxMs) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ouvrir();
    if (err != ESP_OK) {
        return err;
    }
    const uint32_t trames = kCadence * duration_ms / 1000;
    tampon = static_cast<int16_t *>(tampon_psram(static_cast<size_t>(trames) * 4));
    if (tampon == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    err = tab5_i2s_enable(false, true);
    while (err == ESP_OK && lues < trames) {
        size_t lu = 0;
        const uint32_t veut = trames - lues < kBloc ? trames - lues : kBloc;
        err = tab5_i2s_read(tampon + static_cast<size_t>(lues) * 2, veut * 4, &lu, 300);
        lues += static_cast<uint32_t>(lu / 4);
        if (err == ESP_OK && lu == 0) {
            err = ESP_ERR_TIMEOUT;
        }
    }
    (void)tab5_i2s_enable(false, false);
    return err;
}

} // namespace

extern "C" {

esp_err_t board_audio_init(void) {
    Verrou v;
    return ouvrir();
}

void board_audio_get_info(board_audio_info_t *info) {
    if (info == nullptr) {
        return;
    }
    memset(info, 0, sizeof(*info));
    info->codec_ready = s_ouvert;
    info->amplifier_on = s_ampli;
    info->volume_level = s_niveau;
    info->volume_db_x10 = vol::est_muet(s_niveau) ? vol::kDbX10Plancher : vol::db_x10(s_niveau, kPlafondDbX10);
    info->sample_rate = tab5_i2s_is_open() ? tab5_i2s_rate() : 0;
    info->internal_ram_taken = s_ram_prise;
    int casque = 0;
    if (tab5_expander_input(TAB5_EXPANDER_ECRAN, TAB5_EXPANDER_HEADPHONE_DETECT, &casque) == ESP_OK) {
        info->headphones = casque != 0;
    }
}

esp_err_t board_audio_set_volume(int level) {
    Verrou v;
    s_niveau = vol::borner_niveau(level);
    if (!s_ouvert) {
        return ESP_OK; // retenu, applique a l'ouverture
    }
    return appliquer_volume(!s_en_lecture);
}

esp_err_t board_audio_play_tone(uint32_t hz, uint32_t duration_ms, int32_t peak_dbfs_x10) {
    if (duration_ms == 0 || duration_ms > kDureeSonMaxMs || peak_dbfs_x10 > 0 || peak_dbfs_x10 < -600) {
        return ESP_ERR_INVALID_ARG;
    }
    Verrou v;
    esp_err_t err = ouvrir();
    if (err != ESP_OK) {
        return err;
    }
    err = tab5_i2s_enable(true, false);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    s_en_lecture = true;
    (void)appliquer_volume(false);
    (void)ampli(true);
    Flux flux;
    err = emettre_ton(hz, peak_dbfs_x10, kCadence * duration_ms / 1000, nullptr, flux);
    (void)ampli(false);
    s_en_lecture = false;
    (void)appliquer_volume(true);
    (void)tab5_i2s_enable(false, false);
    return err;
}

esp_err_t tab5_audio_mic_levels(uint32_t duration_ms, uint32_t tone_hz, int32_t *left_rms_x10, int32_t *right_rms_x10,
                                int32_t *left_tone_x10, int32_t *right_tone_x10) {
    Verrou v;
    int16_t *tampon = nullptr;
    uint32_t lues = 0;
    const esp_err_t err = ecouter(duration_ms, tampon, lues);
    if (err == ESP_OK) {
        if (left_rms_x10 != nullptr) {
            *left_rms_x10 = niveau_voie(tampon, lues, 0).rms_dbfs_x10;
        }
        if (right_rms_x10 != nullptr) {
            *right_rms_x10 = niveau_voie(tampon, lues, 1).rms_dbfs_x10;
        }
        if (left_tone_x10 != nullptr) {
            *left_tone_x10 = ton_voie(tampon, lues, 0, tone_hz);
        }
        if (right_tone_x10 != nullptr) {
            *right_tone_x10 = ton_voie(tampon, lues, 1, tone_hz);
        }
    }
    heap_caps_free(tampon);
    return err;
}

esp_err_t board_audio_mic_level(uint32_t duration_ms, uint32_t tone_hz, board_audio_level_t *level) {
    if (level == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    Verrou v;
    int16_t *tampon = nullptr;
    uint32_t lues = 0;
    const esp_err_t err = ecouter(duration_ms, tampon, lues);
    if (err == ESP_OK) {
        const aud::Niveau g = niveau_voie(tampon, lues, 0);
        const aud::Niveau d = niveau_voie(tampon, lues, 1);
        const int voie = g.rms_dbfs_x10 >= d.rms_dbfs_x10 ? 0 : 1; // le micro le plus fort des deux
        const aud::Niveau &n = voie == 0 ? g : d;
        level->rms_dbfs_x10 = n.rms_dbfs_x10;
        level->peak_dbfs_x10 = n.crete_dbfs_x10;
        level->clipped = n.ecretes;
        level->dc = n.continu;
        level->tone_dbfs_x10 = ton_voie(tampon, lues, voie, tone_hz);
    }
    heap_caps_free(tampon);
    return err;
}

esp_err_t board_audio_play_pcm(const int16_t *stereo, size_t frames, uint32_t timeout_ms) {
    if (stereo == nullptr || frames == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    Verrou v;
    esp_err_t err = ouvrir();
    if (err != ESP_OK) {
        return err;
    }
    err = tab5_i2s_enable(true, false);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    s_en_lecture = true;
    (void)appliquer_volume(false);
    (void)ampli(true);
    size_t fait = 0;
    while (err == ESP_OK && fait < frames) {
        size_t ecrit = 0;
        const size_t veut = frames - fait < kBloc ? frames - fait : kBloc;
        err = tab5_i2s_write(stereo + fait * 2, veut * 4, &ecrit, timeout_ms);
        fait += ecrit / 4;
    }
    int16_t silence[kBloc * 2] = {};
    for (int i = 0; i < 4 && err == ESP_OK; i++) { // le DMA vide ce qui reste
        size_t ecrit = 0;
        err = tab5_i2s_write(silence, sizeof(silence), &ecrit, timeout_ms);
    }
    (void)ampli(false);
    s_en_lecture = false;
    (void)appliquer_volume(true);
    (void)tab5_i2s_enable(false, false);
    return err;
}

esp_err_t board_audio_record_pcm(int16_t *stereo, size_t frames, size_t *recorded, uint32_t timeout_ms) {
    if (stereo == nullptr || frames == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    Verrou v;
    esp_err_t err = ouvrir();
    if (err != ESP_OK) {
        return err;
    }
    err = tab5_i2s_enable(false, true);
    size_t fait = 0;
    while (err == ESP_OK && fait < frames) {
        size_t lu = 0;
        const size_t veut = frames - fait < kBloc ? frames - fait : kBloc;
        err = tab5_i2s_read(stereo + fait * 2, veut * 4, &lu, timeout_ms);
        fait += lu / 4;
        if (err == ESP_OK && lu == 0) {
            err = ESP_ERR_TIMEOUT;
        }
    }
    (void)tab5_i2s_enable(false, false);
    if (recorded != nullptr) {
        *recorded = fait;
    }
    return err;
}

// --- essais sur carte ----------------------------------------------------------------------------------------------------

esp_err_t tab5_audio_set_line(tab5_dac_line_t line) {
    Verrou v;
    esp_err_t err = ouvrir();
    if (err == ESP_OK) {
        err = tab5_dac_set_output(line);
    }
    return err;
}

esp_err_t tab5_audio_close(void) {
    Verrou v;
    if (!s_ouvert) {
        return ESP_OK;
    }
    (void)ampli(false);
    (void)appliquer_volume(true);
    const esp_err_t err = tab5_i2s_close();
    s_ouvert = false;
    s_ram_prise = 0;
    return err;
}

// Lecture ET enregistrement en meme temps sur le meme port : un sinus au haut-parleur pendant que les microphones sont lus, un bloc
// de 10 ms contre un bloc de 10 ms, depuis une seule tache. Compte les depassements de file du DMA dans les deux sens et le bloc le
// plus lent ; le caller compare le niveau du ton entendu au bruit mesure sans son.
esp_err_t tab5_audio_duplex_probe(const tab5_duplex_cfg_t *cfg, tab5_duplex_result_t *result) {
    if (cfg == nullptr || result == nullptr || cfg->duration_ms <= kRejetMs + 200 ||
        cfg->duration_ms > kDureeMicMaxMs) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(result, 0, sizeof(*result));
    Verrou v;
    esp_err_t err = ouvrir();
    if (err != ESP_OK) {
        return err;
    }
    const uint32_t trames = kCadence * cfg->duration_ms / 1000;
    const size_t capacite =
        static_cast<size_t>((trames + kBloc - 1) / kBloc + 4) * kBloc; // blocs entiers, comme emettre_ton
    int16_t *rx = static_cast<int16_t *>(tampon_psram(capacite * 4));
    if (rx == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    const int niveau_avant = s_niveau;
    s_niveau = vol::borner_niveau(cfg->volume_level);
    err = tab5_i2s_enable(true, true);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
        err = tab5_i2s_flush_rx(); // ce qui s'est accumule pendant la mise en route ne compte pas
    }
    if (err == ESP_OK) {
        uint32_t tx0 = 0;
        uint32_t rx0 = 0;
        tab5_i2s_counters(&tx0, &rx0);
        s_en_lecture = true;
        (void)appliquer_volume(false);
        (void)ampli(true);
        result->speaker_en_pin = -1;
        (void)tab5_expander_peek(TAB5_EXPANDER_ECRAN, TAB5_EXPANDER_SPEAKER_EN, &result->speaker_en_pin);
        Flux flux;
        const int64_t debut = esp_timer_get_time();
        err = emettre_ton(cfg->tone_hz, cfg->tone_peak_dbfs_x10, trames, rx, flux);
        result->elapsed_ms = static_cast<uint32_t>((esp_timer_get_time() - debut) / 1000);
        uint32_t tx1 = 0;
        uint32_t rx1 = 0;
        tab5_i2s_counters(&tx1, &rx1);
        result->frames_written = flux.ecrites;
        result->frames_read = flux.lues;
        result->tx_overflows = tx1 - tx0;
        result->rx_overflows = rx1 - rx0;
        result->slowest_block_ms = flux.bloc_le_plus_lent_ms;
        (void)ampli(false);
        s_en_lecture = false;
        (void)appliquer_volume(true);
        if (flux.lues > kCadence * kRejetMs / 1000 + 64) {
            result->left_rms_dbfs_x10 = niveau_voie(rx, flux.lues, 0).rms_dbfs_x10;
            result->right_rms_dbfs_x10 = niveau_voie(rx, flux.lues, 1).rms_dbfs_x10;
            result->left_tone_dbfs_x10 = ton_voie(rx, flux.lues, 0, cfg->tone_hz);
            result->right_tone_dbfs_x10 = ton_voie(rx, flux.lues, 1, cfg->tone_hz);
        }
    }
    (void)tab5_i2s_enable(false, false);
    s_niveau = niveau_avant;
    heap_caps_free(rx);
    return err;
}

} // extern "C"
