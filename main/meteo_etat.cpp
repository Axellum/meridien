// État « météo actuelle » de l'appareil : le dernier relevé Open-Meteo reçu (ou relu du cache NVS), jugé à chaque lecture contre
// l'heure du moment. Tout le calcul (libellé français, min/max du jour, règle « périmé ») est dans le noyau pur
// (ecran/meteo_synthese.hpp, testé sur le PC) ; ce fichier ne fait que garder le relevé sous verrou et remplir MeteoEtat.
#include "meteo_etat.h"

#include <cstring>
#include <ctime>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "ecran/meteo_synthese.hpp"
#include "meteo_interne.h"

namespace {

portMUX_TYPE g_verrou = portMUX_INITIALIZER_UNLOCKED;
ecran::meteo::Releve g_releve = {};
bool g_a_releve = false;
int g_echecs = 0;

} // namespace

namespace ecran {

void meteo_etat_publier(const meteo::Releve &releve) {
    portENTER_CRITICAL(&g_verrou);
    g_releve = releve;
    g_a_releve = true;
    g_echecs = 0;
    portEXIT_CRITICAL(&g_verrou);
}

void meteo_etat_compter_echec() {
    portENTER_CRITICAL(&g_verrou);
    if (g_echecs < 1000000) { // saturé : le compteur ne déborde jamais
        ++g_echecs;
    }
    portEXIT_CRITICAL(&g_verrou);
}

void meteo_etat_oublier() {
    portENTER_CRITICAL(&g_verrou);
    g_a_releve = false;
    g_echecs = 0;
    portEXIT_CRITICAL(&g_verrou);
}

int meteo_etat_echecs() {
    portENTER_CRITICAL(&g_verrou);
    const int n = g_echecs;
    portEXIT_CRITICAL(&g_verrou);
    return n;
}

} // namespace ecran

// PROVISOIRE : le contrat v2 (meteo_etat.h) est publié avant son implémentation pour que l'interface puisse se compiler et se
// rendre. Tant que ce stub est là, aucune météo détaillée n'est produite : valide = faux, indices à -1.
bool meteo_detail_lire(MeteoDetail *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    std::memset(sortie, 0, sizeof(*sortie));
    sortie->heure_courante = -1;
    sortie->jour_courant = -1;
    sortie->quart_courant = -1;
    return false;
}

uint32_t meteo_detail_version() { return 0; }
bool meteo_etat_lire(MeteoEtat *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    std::memset(sortie, 0, sizeof(*sortie));

    ecran::meteo::Releve releve = {};
    int echecs = 0;
    bool a_releve = false;
    portENTER_CRITICAL(&g_verrou);
    a_releve = g_a_releve;
    if (a_releve) {
        releve = g_releve; // ~400 octets : copie atomique, le calcul se fait hors du verrou
        echecs = g_echecs;
    }
    portEXIT_CRITICAL(&g_verrou);
    if (!a_releve) {
        return false;
    }

    // Tant que SNTP n'a rien reçu, time() vaut environ zéro : le noyau juge alors la météo « périmée » (horloge incertaine).
    const ecran::meteo::Synthese s =
        ecran::meteo::synthetiser(releve, static_cast<int64_t>(std::time(nullptr)), echecs);
    if (s.libelle.size() >= sizeof(sortie->libelle)) {
        return false; // ne devrait pas arriver (test du noyau : tous les libellés font moins de 40 octets) ; jamais tronqué
    }
    sortie->valide = true;
    sortie->perime = s.perime;
    sortie->maj_epoch_s = s.maj_s;
    sortie->temperature_c = static_cast<float>(s.temperature_dixiemes) / 10.0F;
    sortie->code_wmo = s.code_wmo;
    sortie->jour = s.jour;
    sortie->min_c = static_cast<float>(s.min_dixiemes) / 10.0F;
    sortie->max_c = static_cast<float>(s.max_dixiemes) / 10.0F;
    std::memcpy(sortie->libelle, s.libelle.data(), s.libelle.size());
    sortie->libelle[s.libelle.size()] = '\0';
    sortie->minmax_connus = s.minmax_connus;
    return true;
}
