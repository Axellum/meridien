// Horloge materielle du Tab5 : RX8130CE en 0x32 (UTC, supercondensateur de secours). Implementation de board_rtc_* (board.h).
// Les conversions de la trame et TOUTE la politique (quand reprendre la puce, quand l'ecrire, d'ou vient l'heure) sont dans le noyau
// pur ecran::rtc, teste sur le PC avec mutations. Ce fichier lit et ecrit les registres, lit l'horloge du systeme, et applique.
//
// Registres (composant `rx8130` d'ESPHome 2026.9.0, lu le 07/11/2026 ; fiche Epson RX8130CE) : 0x10..0x16 secondes a annee (BCD),
// 0x1C extension (minuteries : mises a 0), 0x1D drapeaux, 0x1E commande 0 (bit 6 = STOP : l'ecriture de l'heure se fait puce
// arretee), 0x1F commande 1 (0x30 = charge du supercondensateur et commutation sur pile), 0x30 decalage numerique (0).
// DIFFERENCE avec ESPHome : il efface les drapeaux (0x1D) a chaque demarrage, donc perd l'information « la puce a perdu son
// alimentation ». Ici on LIT le drapeau d'abord et on ne l'efface qu'apres avoir ecrit une heure venue du reseau.
// Position du bit « perte de tension » (VLF) dans 0x1D : bit 1 (pilote Linux rtc-rx8130 ; NON VERIFIE sur la fiche ici) : la valeur
// brute est journalisee au demarrage et la plausibilite de l'heure protege de toute facon (une puce qui a perdu sa pile repart
// avant 2026, donc n'est jamais reprise).
//
// Qui regle l'heure du systeme ? Le SNTP de l'application (main/reseau.cpp, main/sntp_rapide.cpp) appelle settimeofday ; le
// rappel SNTP d'ESP-IDF est pris par l'application, on ne s'y accroche donc pas. On OBSERVE : toutes les 5 s, un saut de l'horloge
// de plus de 2 s par rapport a sa lancee, ou son passage de « absurde » a « plausible » que nous n'avons pas pose, vaut « le
// reseau a regle l'heure » (source_apres). L'application peut aussi le dire (board_rtc_note_network_time).
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "board.h"
#include "ecran/horloge_materielle.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tab5_internal.h"

namespace {

namespace rtc = ecran::rtc;

constexpr char TAG[] = "tab5_rtc";

constexpr uint8_t kRegHeure = 0x10; // 7 octets
constexpr uint8_t kRegExten = 0x1C;
constexpr uint8_t kRegFlag = 0x1D;
constexpr uint8_t kRegCtrl0 = 0x1E;
constexpr uint8_t kRegCtrl1 = 0x1F;
constexpr uint8_t kRegDigOffset = 0x30;
constexpr uint8_t kCtrl0Stop = 0x40;
constexpr uint8_t kCtrl1Batterie = 0x30;
constexpr uint8_t kFlagVlf = 0x02;
constexpr uint64_t kPeriodeObservationUs = 5ULL * 1000 * 1000;

i2c_master_dev_handle_t s_dev = nullptr;
StaticSemaphore_t s_verrou_stockage;
SemaphoreHandle_t s_verrou = nullptr;
esp_timer_handle_t s_timer = nullptr;

board_rtc_info_t s_info; // copie lisible depuis board_rtc_get_info()
rtc::SourceHeure s_source = rtc::SourceHeure::Aucune;
bool s_plausible_avant = false;
bool s_reseau_signale = false;
int64_t s_dernier_systeme_s = 0;
int64_t s_dernier_mono_us = 0;
int64_t s_derniere_ecriture_mono_us = -1;

int64_t systeme_s() { return static_cast<int64_t>(time(nullptr)); }

void formater(int64_t epoch_s, char *sortie, size_t taille) {
    time_t t = static_cast<time_t>(epoch_s);
    struct tm tm;
    gmtime_r(&t, &tm);
    // 23 caractères pour une année de 4 chiffres, dans un tampon de 32 : la coupe à taille - 1 octets de snprintf ne joue pas. On lit
    // le retour pour ne jamais laisser un texte à moitié écrit si snprintf échoue.
    if (snprintf(sortie, taille, "%04d-%02d-%02d %02d:%02d:%02d UTC", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec) < 0) {
        sortie[0] = '\0';
    }
}

board_time_source_t vers_board(rtc::SourceHeure s) {
    switch (s) {
    case rtc::SourceHeure::Rtc:
        return BOARD_TIME_SOURCE_RTC;
    case rtc::SourceHeure::Reseau:
        return BOARD_TIME_SOURCE_NETWORK;
    case rtc::SourceHeure::Aucune:
        break;
    }
    return BOARD_TIME_SOURCE_NONE;
}

// Lecture de la trame sous verrou. Rend l'erreur du bus, ou ESP_ERR_INVALID_RESPONSE si la trame est incoherente.
esp_err_t lire(int64_t *epoch_s) {
    if (s_dev == nullptr) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    rtc::Trame t;
    xSemaphoreTake(s_verrou, portMAX_DELAY);
    const esp_err_t err = tab5_i2c_read_reg(s_dev, kRegHeure, t.octet, sizeof(t.octet));
    xSemaphoreGive(s_verrou);
    if (err != ESP_OK) {
        return err;
    }
    int64_t valeur = 0;
    if (rtc::decoder(t, valeur) != rtc::Erreur::Aucune) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *epoch_s = valeur;
    return ESP_OK;
}

// Ecriture d'une trame : puce arretee pendant l'ecriture (comme ESPHome), puis redemarree.
esp_err_t ecrire_trame(const uint8_t trame[7], bool effacer_perte) {
    if (s_dev == nullptr) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    xSemaphoreTake(s_verrou, portMAX_DELAY);
    esp_err_t err = tab5_i2c_write_u8(s_dev, kRegCtrl0, kCtrl0Stop);
    if (err == ESP_OK) {
        err = tab5_i2c_write_reg(s_dev, kRegHeure, trame, 7);
    }
    const esp_err_t err_redemarrage = tab5_i2c_write_u8(s_dev, kRegCtrl0, 0x00); // toujours relancer la puce
    if (err == ESP_OK) {
        err = err_redemarrage;
    }
    if (err == ESP_OK && effacer_perte) {
        err = tab5_i2c_write_u8(s_dev, kRegFlag, 0x00);
    }
    xSemaphoreGive(s_verrou);
    return err;
}

void passe_observation(void *) {
    const int64_t mono = esp_timer_get_time();
    const int64_t maintenant = systeme_s();
    const int64_t attendu = s_dernier_systeme_s + (mono - s_dernier_mono_us + 500000) / 1000000;
    rtc::Observation o;
    o.source_avant = s_source;
    o.systeme_plausible_avant = s_plausible_avant;
    o.systeme_plausible_maintenant = rtc::instant_plausible(maintenant);
    o.saut_detecte = rtc::valeur_absolue(maintenant - attendu) > rtc::kEcartTolereS || s_reseau_signale;
    s_reseau_signale = false;
    s_source = rtc::source_apres(o);
    s_plausible_avant = o.systeme_plausible_maintenant;
    s_dernier_systeme_s = maintenant;
    s_dernier_mono_us = mono;
    s_info.source = vers_board(s_source);

    if (s_source != rtc::SourceHeure::Reseau) {
        return; // rien a ecrire : la puce n'est jamais reglee depuis elle-meme ni depuis une heure douteuse
    }
    rtc::EntreePeriodique e;
    e.source = s_source;
    e.systeme_s = maintenant;
    int64_t rtc_s = 0;
    e.rtc_lue = lire(&rtc_s) == ESP_OK;
    e.rtc_s = rtc_s;
    e.depuis_derniere_ecriture_s =
        s_derniere_ecriture_mono_us < 0 ? -1 : (mono - s_derniere_ecriture_mono_us) / 1000000;
    if (rtc::periodique(e) == rtc::ActionPeriodique::EcrireRtc) {
        const esp_err_t err = board_rtc_write(maintenant);
        char texte[32];
        formater(maintenant, texte, sizeof(texte));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "horloge matérielle réglée sur l'heure du réseau : %s (écart avant : %lld s)", texte,
                     e.rtc_lue ? static_cast<long long>(rtc_s - maintenant) : 0LL);
        } else {
            ESP_LOGW(TAG, "horloge matérielle non réglée : %s", esp_err_to_name(err));
        }
    }
}

} // namespace

extern "C" {

esp_err_t tab5_rtc_init(void) {
    s_verrou = xSemaphoreCreateMutexStatic(&s_verrou_stockage);
    if (!tab5_i2c_probe(TAB5_I2C_ADDR_RX8130)) {
        ESP_LOGW(TAG, "horloge matérielle : aucune puce à l'adresse 0x%02X, l'heure viendra du réseau seulement",
                 TAB5_I2C_ADDR_RX8130);
        return ESP_OK;
    }
    esp_err_t err = tab5_i2c_attach(TAB5_I2C_ADDR_RX8130, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "horloge matérielle : attache impossible : %s", esp_err_to_name(err));
        s_dev = nullptr;
        return ESP_OK;
    }
    s_info.present = true;

    // 1. Drapeaux AVANT de toucher a quoi que ce soit : on veut savoir si la puce a perdu son alimentation.
    uint8_t flag = 0;
    if (tab5_i2c_read_u8(s_dev, kRegFlag, &flag) != ESP_OK) {
        ESP_LOGW(TAG, "horloge matérielle : drapeaux illisibles, on suppose une perte d'alimentation");
        flag = kFlagVlf;
    }
    s_info.flag_at_boot = flag;
    s_info.power_lost = (flag & kFlagVlf) != 0;

    // 2. Configuration de ESPHome, sauf l'effacement des drapeaux : pas de decalage, pas de minuterie, pas d'interruption, charge
    // du supercondensateur et commutation sur pile. Une erreur ici n'empeche pas la lecture de l'heure.
    const uint8_t zero = 0x00;
    bool ok = tab5_i2c_write_reg(s_dev, kRegDigOffset, &zero, 1) == ESP_OK;
    ok = tab5_i2c_write_reg(s_dev, kRegExten, &zero, 1) == ESP_OK && ok;
    ok = tab5_i2c_write_reg(s_dev, kRegCtrl0, &zero, 1) == ESP_OK && ok;
    ok = tab5_i2c_write_u8(s_dev, kRegCtrl1, kCtrl1Batterie) == ESP_OK && ok;
    if (!ok) {
        ESP_LOGW(TAG, "horloge matérielle : configuration incomplète (bus I2C)");
    }

    // 3. Lecture, decision (noyau pur), reprise eventuelle de l'horloge du systeme.
    int64_t rtc_s = 0;
    err = lire(&rtc_s);
    const int64_t systeme = systeme_s();
    rtc::EntreeDemarrage entree;
    entree.rtc_lue = err == ESP_OK;
    entree.rtc_perdue = s_info.power_lost;
    entree.rtc_s = rtc_s;
    entree.systeme_s = systeme;
    const rtc::ActionDemarrage action = rtc::au_demarrage(entree);
    char texte_rtc[32] = "illisible";
    if (err == ESP_OK) {
        formater(rtc_s, texte_rtc, sizeof(texte_rtc));
    }
    if (action == rtc::ActionDemarrage::ReprendreRtc) {
        const struct timeval tv = {.tv_sec = static_cast<time_t>(rtc_s), .tv_usec = 0};
        settimeofday(&tv, nullptr);
        s_source = rtc::SourceHeure::Rtc;
        s_info.resumed_from_rtc = true;
        ESP_LOGI(TAG, "heure reprise de l'horloge matérielle : %s (drapeaux 0x%02X)", texte_rtc, flag);
    } else {
        ESP_LOGI(
            TAG,
            "horloge matérielle : %s, drapeaux 0x%02X (perte d'alimentation : %s) ; heure du système %s, rien repris",
            texte_rtc, flag, s_info.power_lost ? "oui" : "non",
            rtc::instant_plausible(systeme) ? "plausible" : "absente");
    }
    s_info.source = vers_board(s_source);

    // 4. Surveillance : un seul minuteur logiciel, pas de tache en plus.
    s_plausible_avant = rtc::instant_plausible(systeme_s());
    s_dernier_systeme_s = systeme_s();
    s_dernier_mono_us = esp_timer_get_time();
    esp_timer_create_args_t args = {}; // champs non cites a zero : minuteur appele depuis la tache esp_timer
    args.callback = passe_observation;
    args.name = "rtc";
    err = esp_timer_create(&args, &s_timer);
    if (err == ESP_OK) {
        err = esp_timer_start_periodic(s_timer, kPeriodeObservationUs);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "surveillance de l'heure non démarrée : %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

esp_err_t tab5_rtc_raw_read(uint8_t frame[7]) {
    if (s_dev == nullptr) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    xSemaphoreTake(s_verrou, portMAX_DELAY);
    const esp_err_t err = tab5_i2c_read_reg(s_dev, kRegHeure, frame, 7);
    xSemaphoreGive(s_verrou);
    return err;
}

esp_err_t tab5_rtc_raw_write(const uint8_t frame[7]) { return ecrire_trame(frame, false); }

esp_err_t board_rtc_read(int64_t *epoch_s) {
    if (epoch_s == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return lire(epoch_s);
}

esp_err_t board_rtc_write(int64_t epoch_s) {
    rtc::Trame t;
    if (rtc::encoder(epoch_s, t) != rtc::Erreur::Aucune) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t err = ecrire_trame(t.octet, true);
    if (err == ESP_OK) {
        s_info.writes++;
        s_info.last_write_epoch_s = epoch_s;
        s_derniere_ecriture_mono_us = esp_timer_get_time();
    }
    return err;
}

void board_rtc_get_info(board_rtc_info_t *info) {
    if (info != nullptr) {
        *info = s_info;
    }
}

void board_rtc_note_network_time(void) { s_reseau_signale = true; }

} // extern "C"
