// Alimentation du Tab5 : moniteur de courant INA226 (I2C 0x41, shunt 5 mohm, sur le connecteur de BATTERIE) et chargeur (expandeur
// 0x44 : bit 5 nCHG_QC_EN actif bas, bit 6 CHG_STAT en entree, bit 7 CHG_EN). Implemente board_power_get() de board.h.
//
// Les conversions de registres et le classement de l'etat sont dans le noyau pur (ecran::alimentation, teste sur le PC avec
// mutations) ; ce fichier lit les registres et pilote les broches.
//
// Particularites du Tab5 d'Axel, a ne pas oublier en lisant les chiffres :
//   - IL N'A PAS DE BATTERIE. La tension lue est celle du connecteur de batterie, pas celle de l'USB-C : sans batterie, chargeur coupe,
//     le firmware ESPHome voisin y a releve 1,83 a 1,94 V ; chargeur allume, 4,2 V, 5,7 V ou une oscillation 4,2 <-> 8,4 V. D'ou la
//     regle : « batterie presente » seulement au-dessus de 5,5 V CHARGEUR COUPE.
//   - Aucune puce ne dit si le port USB-C est alimente : ce n'est pas mesurable par cette carte et l'API ne pretend pas le faire.
//   - Ce firmware ne CHARGE jamais : au demarrage CHG_EN est force bas et la charge rapide (nCHG_QC_EN) forcee a l'arret (si elle ne l'est pas deja), comme le
//     firmware ESPHome du depot Tab5 (tab5-sensors-diagnostics.yaml, « CHG_EN coupe au setup »). Sans batterie, charger dans le vide
//     faisait un pic de consommation et les boucles de redemarrage la chargeaient en continu (incident du 11/10/2026 cote ESPHome).
//     Brancher une batterie plus tard est une decision a prendre (ADR) : cette couche n'allume jamais le chargeur d'elle-meme.
//   - L'etat « tel que trouve » des broches du chargeur est journalise AVANT de les forcer : c'est la seule trace de ce que la
//     carte fait toute seule apres la remise a zero de l'expandeur.
#include <string.h>

#include "board.h"
#include "ecran/alimentation.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tab5_internal.h"

namespace {

namespace ali = ecran::alimentation;

constexpr char TAG[] = "tab5_alim";

constexpr int32_t kCourantMaxMa = 8192; // plage du moniteur : LSB de 250 uA
constexpr int32_t kShuntMohm = 5;
// Configuration : moyenne sur 16 mesures (bits 11..9 = 010), conversions de 1,1 ms (bits 8..6 et 5..3 = 100), continu tension de
// shunt et du bus (bits 2..0 = 111), bit 14 a 1 comme le prescrit la fiche (0x4000). 16 x 1,1 ms x 2 = ~35 ms par mesure.
constexpr uint16_t kConfig = 0x4000 | (2u << 9) | (4u << 6) | (4u << 3) | 7u;
constexpr uint16_t kConfigReset = 0x8000;

i2c_master_dev_handle_t s_ina = nullptr;
bool s_ina_repond = false;
bool s_ina_identifie = false; // registres 0xFE / 0xFF conformes a la fiche
bool s_chargeur_coupe = false;
int32_t s_lsb_ua = 0;

esp_err_t lire16(uint8_t reg, uint16_t *valeur) {
    uint8_t octets[2] = {};
    const esp_err_t err = tab5_i2c_read_reg(s_ina, reg, octets, 2);
    if (err == ESP_OK) {
        *valeur = static_cast<uint16_t>((octets[0] << 8) | octets[1]); // l'INA226 envoie l'octet de poids fort d'abord
    }
    return err;
}

esp_err_t ecrire16(uint8_t reg, uint16_t valeur) {
    const uint8_t octets[2] = {static_cast<uint8_t>(valeur >> 8), static_cast<uint8_t>(valeur & 0xFF)};
    return tab5_i2c_write_reg(s_ina, reg, octets, 2);
}

board_battery_state_t vers_etat_api(ali::Etat e) {
    switch (e) {
    case ali::Etat::SansBatterie:
        return BOARD_BATTERY_NONE;
    case ali::Etat::Repos:
        return BOARD_BATTERY_IDLE;
    case ali::Etat::EnCharge:
        return BOARD_BATTERY_CHARGING;
    case ali::Etat::EnDecharge:
        return BOARD_BATTERY_DISCHARGING;
    case ali::Etat::Inconnu:
        break;
    }
    return BOARD_BATTERY_UNKNOWN;
}

// Chargeur : journalise l'etat trouve, puis CHG_EN bas et charge rapide a l'arret.
void arreter_chargeur() {
    if (!tab5_wifi_power_available()) {
        ESP_LOGW(TAG, "expandeur 0x44 absent : le chargeur n'est pas pilotable ici (état inconnu)");
        return;
    }
    // L'expandeur vient d'etre remis a zero par son pilote : toutes les broches sont en entree. CHG_STAT avec tirage vers le bas,
    // comme le firmware ESPHome (sans cela, la broche flotte quand le chargeur ne la tire pas).
    (void)tab5_expander_pull_down(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_CHG_STAT);
    int stat = -1;
    int chg_en = -1;
    int qc = -1;
    (void)tab5_expander_input(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_CHG_STAT, &stat);
    (void)tab5_expander_input(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_CHG_EN, &chg_en);
    (void)tab5_expander_input(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_NCHG_QC_EN, &qc);
    ESP_LOGI(TAG, "chargeur tel que trouvé (broches en entrée) : CHG_STAT=%d CHG_EN=%d nCHG_QC_EN=%d", stat, chg_en,
             qc);
    // CHG_EN toujours pilote a 0. nCHG_QC_EN (actif bas) seulement s'il n'est pas deja a l'arret (1) : une sortie qui prend un instant
    // le niveau 0 du registre de sortie avant le niveau voulu allumerait la charge rapide quelques dizaines de microsecondes pour rien.
    const esp_err_t e1 = tab5_expander_output(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_CHG_EN, 0);
    const esp_err_t e2 = qc == 1 ? ESP_OK : tab5_expander_output(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_NCHG_QC_EN, 1);
    s_chargeur_coupe = e1 == ESP_OK && e2 == ESP_OK;
    if (!s_chargeur_coupe) {
        ESP_LOGE(TAG, "chargeur non coupé : CHG_EN %s, nCHG_QC_EN %s", esp_err_to_name(e1), esp_err_to_name(e2));
    }
}

} // namespace

extern "C" {

esp_err_t tab5_alim_init(void) {
    arreter_chargeur();

    if (!tab5_i2c_probe(TAB5_I2C_ADDR_INA226)) {
        ESP_LOGW(TAG, "INA226 absent en 0x%02X", TAB5_I2C_ADDR_INA226);
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err = tab5_i2c_attach(TAB5_I2C_ADDR_INA226, &s_ina);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "INA226 non rattaché : %s", esp_err_to_name(err));
        return err;
    }
    uint16_t fabricant = 0;
    uint16_t puce = 0;
    uint16_t config_trouvee = 0;
    uint16_t etalonnage_trouve = 0;
    uint16_t tension_brute = 0;
    err = lire16(ali::kRegFabricant, &fabricant);
    if (err == ESP_OK) {
        err = lire16(ali::kRegPuce, &puce);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "INA226 ne répond pas à la lecture : %s", esp_err_to_name(err));
        return err;
    }
    s_ina_repond = true;
    (void)lire16(ali::kRegConfig, &config_trouvee);
    (void)lire16(ali::kRegEtalonnage, &etalonnage_trouve);
    (void)lire16(ali::kRegTension, &tension_brute);
    s_ina_identifie = fabricant == ali::kFabricantTi && puce == ali::kPuceIna226;
    ESP_LOGI(TAG, "INA226 : fabricant 0x%04X (attendu 0x%04X), puce 0x%04X (attendu 0x%04X) -> %s", fabricant,
             ali::kFabricantTi, puce, ali::kPuceIna226, s_ina_identifie ? "identifié" : "IDENTIFIANT DIFFÉRENT");
    ESP_LOGI(TAG, "INA226 tel que trouvé : config=0x%04X étalonnage=0x%04X tension brute=0x%04X (%d mV)",
             config_trouvee, etalonnage_trouve, tension_brute, static_cast<int>(ali::tension_mv(tension_brute)));

    s_lsb_ua = ali::lsb_courant_ua(kCourantMaxMa);
    const int32_t etalonnage = ali::etalonnage(s_lsb_ua, kShuntMohm);
    err = ecrire16(ali::kRegConfig, kConfigReset);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = ecrire16(ali::kRegConfig, kConfig);
    }
    if (err == ESP_OK) {
        err = ecrire16(ali::kRegEtalonnage, static_cast<uint16_t>(etalonnage));
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "INA226 non configuré : %s", esp_err_to_name(err));
        s_ina_repond = false;
        return err;
    }
    ESP_LOGI(TAG, "INA226 configuré : LSB %d uA, étalonnage %d (shunt %d mΩ, %d mA max)", static_cast<int>(s_lsb_ua),
             static_cast<int>(etalonnage), static_cast<int>(kShuntMohm), static_cast<int>(kCourantMaxMa));
    return ESP_OK;
}

esp_err_t board_power_get(board_power_info_t *info) {
    if (info == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(info, 0, sizeof(*info));
    info->battery = BOARD_BATTERY_UNKNOWN;
    info->charger_enabled = false; // ce firmware ne l'allume jamais (arreter_chargeur)

    int stat = 0;
    if (tab5_expander_input(TAB5_EXPANDER_ALIM, TAB5_EXPANDER_POWER_CHG_STAT, &stat) == ESP_OK) {
        info->charge_status_pin =
            stat != 0; // 1 = « en charge » selon M5Unified ; sans batterie, constate a 1 sur ESPHome
    }
    if (!s_ina_repond) {
        return ESP_ERR_NOT_FOUND;
    }
    uint16_t tension = 0;
    uint16_t courant = 0;
    esp_err_t err = lire16(ali::kRegTension, &tension);
    if (err == ESP_OK) {
        err = lire16(ali::kRegCourant, &courant);
    }
    if (err != ESP_OK) {
        return err;
    }
    info->monitor_present = s_ina_identifie;
    info->bus_mv = ali::tension_mv(tension);
    info->current_ma = ali::courant_ma(courant, s_lsb_ua);
    info->battery = vers_etat_api(ali::classer(s_ina_identifie, info->bus_mv, info->current_ma, s_chargeur_coupe));
    return ESP_OK;
}

} // extern "C"
