// État « météo » de l'appareil : la dernière météo Open-Meteo reçue (ou relue du cache NVS) — relevé de base et compléments
// (24 heures, 7 jours, quarts d'heure de pluie) —, jugée à chaque lecture contre l'heure du moment. Tout le calcul est dans le
// noyau pur (ecran/meteo_synthese.hpp pour le contrat v1, ecran/meteo_detail.hpp pour le v2, testés sur le PC) ; ce fichier ne
// fait que garder la météo sous verrou et appeler ces deux conversions.
//
// Mémoire : la météo (1 312 octets sur le PC) est en PSRAM, allouée à la première publication et jamais libérée (repli en RAM interne si la
// PSRAM manque). En RAM interne il ne reste que le verrou (une centaine d'octets), quelques entiers et l'historique des pressions
// (~170 octets) : la RAM interne est rare.
// Verrou : un mutex FreeRTOS, pas une section critique : la conversion (2 088 octets écrits) ne doit pas couper les interruptions. Les
// lecteurs sont des tâches (jamais une interruption) ; le seul écrivain est la tâche météo (et `meteo_client_demarrer`, avant
// qu'elle existe).
#include "meteo_etat.h"

#include <atomic>
#include <cstring>
#include <ctime>
#include <limits>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ecran/meteo_detail.hpp"
#include "ecran/meteo_synthese.hpp"
#include "meteo_interne.h"

namespace {

const char *TAG = "meteo";

// Tout ce qui est protégé par `verrou`, sauf `version_visible` que l'interface lit sans verrou. Construit avant `app_main`.
struct Etat {
    StaticSemaphore_t tampon_verrou;
    SemaphoreHandle_t verrou;
    ecran::meteo::Meteo *meteo;            // PSRAM ; nul tant que rien n'a été publié
    bool a_meteo;                          // une météo est connue pour la zone actuelle
    int echecs;                            // essais ratés de suite
    uint32_t compteur;                     // numéro de la dernière publication, jamais 0
    std::atomic<uint32_t> version_visible; // = compteur si une météo est connue, sinon 0
    ecran::meteo::HistoPression histo;     // pressions mesurées, pour la tendance sur 3 h (~170 octets, RAM interne)

    Etat()
        : tampon_verrou(), verrou(xSemaphoreCreateMutexStatic(&tampon_verrou)), meteo(nullptr), a_meteo(false),
          echecs(0), compteur(0), version_visible(0), histo() {}
};

Etat g_etat;

// Tient le verrou le temps d'un bloc.
class Verrou {
  public:
    Verrou() { xSemaphoreTake(g_etat.verrou, portMAX_DELAY); }
    ~Verrou() { xSemaphoreGive(g_etat.verrou); }
    Verrou(const Verrou &) = delete;
    Verrou &operator=(const Verrou &) = delete;
};

void mettre_a_zero(MeteoDetail *sortie) {
    std::memset(sortie, 0, sizeof(*sortie));
    sortie->heure_courante = -1;
    sortie->jour_courant = -1;
    sortie->quart_courant = -1;
    sortie->courant.tendance_pression_3h_hpa = std::numeric_limits<float>::quiet_NaN(); // « inconnue », même sans météo
}

} // namespace

namespace ecran {

bool meteo_etat_publier(const meteo::Meteo &meteo) {
    Verrou v;
    if (g_etat.meteo == nullptr) {
        void *place = heap_caps_calloc(1, sizeof(meteo::Meteo), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (place == nullptr) {
            place = heap_caps_calloc(1, sizeof(meteo::Meteo), MALLOC_CAP_8BIT);
            if (place != nullptr) {
                ESP_LOGW(TAG, "PSRAM indisponible : la météo (%u octets) est gardée en RAM interne",
                         static_cast<unsigned>(sizeof(meteo::Meteo)));
            }
        }
        if (place == nullptr) {
            return false; // rien n'est publié ; l'état précédent (s'il y en a un) reste lisible
        }
        g_etat.meteo = new (place) meteo::Meteo();
    }
    *g_etat.meteo = meteo;
    ecran::meteo::tendance_noter(g_etat.histo, meteo.releve.instant_s, meteo.releve.pression_dixiemes_hpa);
    g_etat.a_meteo = true;
    g_etat.echecs = 0;
    if (++g_etat.compteur == 0) {
        g_etat.compteur = 1; // 0 veut dire « aucune météo » : on ne le réutilise pas après le débordement
    }
    g_etat.version_visible.store(g_etat.compteur, std::memory_order_release);
    return true;
}

void meteo_etat_compter_echec() {
    Verrou v;
    if (g_etat.echecs < 1000000) { // saturé : le compteur ne déborde jamais
        ++g_etat.echecs;
    }
}

void meteo_etat_oublier() {
    Verrou v;
    g_etat.a_meteo = false;
    g_etat.echecs = 0;
    g_etat.histo = ecran::meteo::HistoPression{}; // les pressions d'un autre lieu ne disent rien de la tendance d'ici
    // La version repasse à 0 : l'interface voit un changement et relit (valide = faux). La publication suivante prend un numéro
    // jamais utilisé, donc aussi différent de celui d'avant.
    g_etat.version_visible.store(0, std::memory_order_release);
}

int meteo_etat_echecs() {
    Verrou v;
    return g_etat.echecs;
}

} // namespace ecran

uint32_t meteo_detail_version() { return g_etat.version_visible.load(std::memory_order_acquire); }

bool meteo_detail_lire(MeteoDetail *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    {
        Verrou v;
        if (g_etat.a_meteo) {
            // La conversion se fait sous verrou, directement depuis la PSRAM vers `sortie` : pas de copie intermédiaire de 2 Ko
            // sur la pile. Elle dure quelques dizaines de microsecondes ; personne d'autre n'écrit qu'une fois par quart d'heure.
            ecran::meteo::remplir_detail(*g_etat.meteo, g_etat.compteur, maintenant, g_etat.echecs, g_etat.histo,
                                         *sortie);
            return true;
        }
    }
    mettre_a_zero(sortie);
    return false;
}

bool meteo_etat_lire(MeteoEtat *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    std::memset(sortie, 0, sizeof(*sortie));

    ecran::meteo::Releve releve = {};
    int echecs = 0;
    {
        Verrou v;
        if (!g_etat.a_meteo) {
            return false;
        }
        releve = g_etat.meteo->releve; // ~400 octets : copie sous verrou, le calcul se fait dehors
        echecs = g_etat.echecs;
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
