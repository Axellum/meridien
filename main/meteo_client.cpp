#include "meteo_client.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string_view>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "ecran/meteo_cache.hpp"
#include "ecran/meteo_planning.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_synthese.hpp"
#include "horloge.h"
#include "meteo_etat.h"
#include "meteo_interne.h"
#include "reseau.h"

namespace ecran {

namespace {

const char *TAG = "meteo";
// Ligne de résultat que le banc de test cherche : « METEO … » à chaque réponse lue.
const char *TAG_RESULTAT = "METEO";

constexpr const char *kEspaceNvs = "meteo";
constexpr const char *kCleCache = "cache";
constexpr size_t kTailleTampon =
    16 * 1024; // réponse mesurée : 2 826 à 2 960 octets ; tampon en PSRAM, libéré après chaque essai
constexpr int kDelaiHttpMs = 15000;          // connexion, TLS et lecture : au-delà, l'essai est un échec
constexpr uint32_t kPasAttenteMs = 2000;     // contrôle de l'IP et de l'heure avant le premier essai
constexpr uint32_t kAntiRebondZoneMs = 1500; // plusieurs changements de zone d'affilée = une seule requête
constexpr int64_t kInstantMin = 946684800;   // 1er janvier 2000 : en dessous, l'horloge n'est pas réglée
constexpr uint32_t kPileOctets = 12288;      // la prise de contact TLS demande plusieurs Ko de pile

TaskHandle_t g_tache = nullptr;

// ---- mémoire -------------------------------------------------------------------------------------------------------

struct Memoire {
    size_t interne;      // octets libres en RAM interne
    size_t interne_bloc; // plus grand bloc libre en RAM interne
    size_t interne_min;  // minimum de RAM interne libre depuis le démarrage
    size_t psram;        // octets libres en PSRAM
    size_t psram_bloc;   // plus grand bloc libre en PSRAM
};

Memoire mesurer_memoire() {
    Memoire m;
    m.interne = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    m.interne_bloc = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    m.interne_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    m.psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    m.psram_bloc = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    return m;
}

void journaliser_memoire(const char *quand, const Memoire &m) {
    ESP_LOGI(TAG,
             "mémoire libre %s : interne %u o (plus grand bloc %u o, minimum depuis le démarrage %u o), PSRAM %u o "
             "(plus grand bloc %u o)",
             quand, static_cast<unsigned>(m.interne), static_cast<unsigned>(m.interne_bloc),
             static_cast<unsigned>(m.interne_min), static_cast<unsigned>(m.psram), static_cast<unsigned>(m.psram_bloc));
}

// ---- une requête ---------------------------------------------------------------------------------------------------

struct Reception {
    uint8_t *donnees;
    size_t capacite;
    size_t n;
    bool trop;
    bool connecte;
    Memoire
        apres_connexion; // mesurée une fois la connexion TLS établie : c'est là que la RAM interne est la plus sollicitée
};

esp_err_t sur_evenement_http(esp_http_client_event_t *evt) {
    auto *rec = static_cast<Reception *>(evt->user_data);
    if (rec == nullptr) {
        return ESP_OK;
    }
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        rec->connecte = true;
        rec->apres_connexion = mesurer_memoire();
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0 && evt->data != nullptr) {
            const size_t len = static_cast<size_t>(evt->data_len);
            if (rec->n + len > rec->capacite) {
                rec->trop = true; // réponse plus grande que prévu : on ne la lit pas à moitié
            } else {
                std::memcpy(rec->donnees + rec->n, evt->data, len);
                rec->n += len;
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

enum class Issue : uint8_t {
    Ok,
    Memoire,        // pas de place en PSRAM pour le tampon
    Requete,        // adresse non construite (position invraisemblable : ne devrait pas arriver)
    Reseau,         // DNS, connexion, TLS ou délai dépassé
    Http,           // le serveur a répondu autre chose que 200
    TropDeRequetes, // HTTP 429
    TropGrand,      // réponse au-delà du tampon
    Illisible,      // 200, mais la réponse n'est pas celle qu'on attend
};

struct Essai {
    Issue issue = Issue::Reseau;
    int statut_http = 0;
    size_t octets = 0;
    int duree_ms = 0;
    meteo::Releve releve{};
};

// Un aller-retour complet. Le tampon de réception est pris en PSRAM et rendu avant de sortir.
Essai interroger(const PositionE5 &position) {
    Essai e;
    char url[meteo::TAILLE_REQUETE_MAX];
    if (meteo::construire_requete(position.latitude_e5, position.longitude_e5, url, sizeof(url)) == 0) {
        e.issue = Issue::Requete;
        return e;
    }

    const Memoire avant = mesurer_memoire();
    journaliser_memoire("avant la requête", avant);

    auto *tampon = static_cast<uint8_t *>(heap_caps_malloc(kTailleTampon, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (tampon == nullptr) {
        e.issue = Issue::Memoire;
        return e;
    }
    Reception rec = {};
    rec.donnees = tampon;
    rec.capacite = kTailleTampon;

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = kDelaiHttpMs;
    config.event_handler = sur_evenement_http;
    config.user_data = &rec;
    config.crt_bundle_attach =
        esp_crt_bundle_attach;    // certificats racine du paquet d'ESP-IDF (Mozilla) : le serveur est vérifié
    config.buffer_size = 2048;    // en-têtes de réponse
    config.buffer_size_tx = 1024; // ligne de requête (479 octets) + en-têtes : 512 par défaut ne suffit pas
    config.keep_alive_enable = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        heap_caps_free(tampon);
        e.issue = Issue::Reseau;
        return e;
    }
    esp_http_client_set_header(client, "Accept", "application/json");

    const int64_t debut_us = esp_timer_get_time();
    const esp_err_t err = esp_http_client_perform(client);
    e.duree_ms = static_cast<int>((esp_timer_get_time() - debut_us) / 1000);
    e.statut_http = esp_http_client_get_status_code(client);
    const int erreur_socket = esp_http_client_get_errno(client);
    esp_http_client_cleanup(client);
    e.octets = rec.n;

    if (rec.connecte) {
        journaliser_memoire("une fois la connexion TLS établie", rec.apres_connexion);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "requête échouée : %s (errno %d) après %d ms", esp_err_to_name(err), erreur_socket, e.duree_ms);
        e.issue = Issue::Reseau;
    } else if (e.statut_http == 429) {
        e.issue = Issue::TropDeRequetes;
    } else if (e.statut_http != 200) {
        e.issue = Issue::Http;
    } else if (rec.trop) {
        e.issue = Issue::TropGrand;
    } else {
        const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
        const meteo::Analyse a =
            meteo::analyser(std::string_view(reinterpret_cast<const char *>(tampon), rec.n), maintenant);
        if (a.ok()) {
            e.releve = a.releve;
            e.issue = Issue::Ok;
        } else {
            ESP_LOGW(TAG, "réponse refusée : %.*s (champ %.*s)",
                     static_cast<int>(meteo::message_court(a.diag.erreur).size()),
                     meteo::message_court(a.diag.erreur).data(), static_cast<int>(a.diag.champ.size()),
                     a.diag.champ.data());
            e.issue = Issue::Illisible;
        }
    }
    heap_caps_free(tampon);

    const Memoire apres = mesurer_memoire();
    journaliser_memoire("après la requête", apres);
    ESP_LOGI(TAG, "écart de mémoire libre sur la requête : interne %+d o, PSRAM %+d o",
             static_cast<int>(apres.interne) - static_cast<int>(avant.interne),
             static_cast<int>(apres.psram) - static_cast<int>(avant.psram));
    return e;
}

// ---- cache NVS -----------------------------------------------------------------------------------------------------

void enregistrer_cache(const meteo::Releve &releve, const PositionE5 &position) {
    uint8_t brut[meteo::TAILLE_CACHE_ZONE_MAX];
    const size_t n = meteo::serialiser_cache(releve, position.latitude_e5, position.longitude_e5, brut, sizeof(brut));
    if (n == 0) {
        ESP_LOGE(TAG, "cache non écrit : relevé ou position refusés par le noyau");
        return;
    }
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kEspaceNvs, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, kCleCache, brut, n);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cache non écrit en NVS : %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "cache écrit en NVS (%u octets)", static_cast<unsigned>(n));
    }
}

void relire_cache() {
    PositionE5 position = {};
    zone_lire_e5(nullptr, &position);
    nvs_handle_t h = 0;
    const esp_err_t ouvert = nvs_open(kEspaceNvs, NVS_READONLY, &h);
    if (ouvert != ESP_OK) {
        ESP_LOGI(TAG, "pas de cache météo (NVS : %s)", esp_err_to_name(ouvert));
        return;
    }
    uint8_t brut[meteo::TAILLE_CACHE_ZONE_MAX];
    size_t taille = sizeof(brut);
    const esp_err_t err = nvs_get_blob(h, kCleCache, brut, &taille);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "pas de cache météo enregistré");
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cache météo non lu : %s", esp_err_to_name(err));
        return;
    }
    const meteo::LectureCache lu = meteo::deserialiser_cache(brut, taille);
    if (!lu.ok()) {
        ESP_LOGW(TAG, "cache météo refusé : %.*s (%.*s) ; il sera remplacé à la prochaine réponse",
                 static_cast<int>(meteo::message_court(lu.diag.erreur).size()),
                 meteo::message_court(lu.diag.erreur).data(), static_cast<int>(lu.diag.champ.size()),
                 lu.diag.champ.data());
        return;
    }
    if (!(PositionE5{lu.latitude_e5, lu.longitude_e5} == position)) {
        ESP_LOGI(TAG, "cache météo d'un autre lieu que la zone actuelle : ignoré");
        return;
    }
    meteo_etat_publier(lu.releve);
    char t[12];
    meteo::ecrire_dixiemes(lu.releve.temperature_dixiemes, t, sizeof(t));
    const std::string_view libelle = meteo::decrire(lu.releve.code_wmo, lu.releve.jour).libelle;
    ESP_LOGI(TAG, "cache relu : %s °C, %.*s, reçu à l'instant UNIX %lld (fraîcheur : voir la ligne contrat suivante)",
             t, static_cast<int>(libelle.size()), libelle.data(), static_cast<long long>(lu.releve.recu_a_s));
}

// ---- journal du résultat -------------------------------------------------------------------------------------------

void journaliser_releve(const ZoneMeteo &zone, const Essai &e) {
    const meteo::Releve &r = e.releve;
    const int64_t maintenant = r.recu_a_s;
    const meteo::Synthese s = meteo::synthetiser(r, maintenant, 0);
    char temp[12], ress[12], mini[12], maxi[12], vent[12], pression[12];
    meteo::ecrire_dixiemes(r.temperature_dixiemes, temp, sizeof(temp));
    meteo::ecrire_dixiemes(r.ressentie_dixiemes, ress, sizeof(ress));
    meteo::ecrire_dixiemes(s.min_dixiemes, mini, sizeof(mini));
    meteo::ecrire_dixiemes(s.max_dixiemes, maxi, sizeof(maxi));
    meteo::ecrire_dixiemes(r.vent_dixiemes_kmh, vent, sizeof(vent));
    meteo::ecrire_dixiemes(r.pression_dixiemes_hpa, pression, sizeof(pression));
    char pluie[8];
    if (r.proba_precip_pct == meteo::PROBA_INCONNUE) {
        std::snprintf(pluie, sizeof(pluie), "?");
    } else {
        std::snprintf(pluie, sizeof(pluie), "%d", static_cast<int>(r.proba_precip_pct));
    }
    TexteHorloge texte = {};
    const bool horloge_ok = horloge_formater(maintenant, texte);
    ESP_LOGI(
        TAG_RESULTAT,
        "METEO %s : %s °C (ressentie %s), %.*s, %s ; min %s / max %s du jour%s ; vent %s km/h, direction %d° ; "
        "pression %s hPa ; "
        "humidité %d %% ; pluie %s %% ; mesure UNIX %lld, reçue UNIX %lld (%s, %s) ; %u jours ; %u octets en %d ms",
        zone.nom, temp, ress, static_cast<int>(s.libelle.size()), s.libelle.data(), r.jour ? "jour" : "nuit", mini,
        maxi, s.minmax_connus ? "" : " (prévision du jour absente)", vent, static_cast<int>(r.direction_vent_deg),
        pression, static_cast<int>(r.humidite_pct), pluie, static_cast<long long>(r.instant_s),
        static_cast<long long>(r.recu_a_s), horloge_ok ? texte.date : "-", horloge_ok ? texte.heure : "-",
        static_cast<unsigned>(r.nb_jours), static_cast<unsigned>(e.octets), e.duree_ms);
}

// Relit la météo PAR LE CONTRAT (meteo_etat_lire), exactement comme le fera l'interface, et la met au journal : c'est la preuve
// sur la carte que ce que l'écran recevra est la météo réelle, avec son indicateur « périmé » et son libellé.
void journaliser_contrat(const char *quand) {
    MeteoEtat e = {};
    if (!meteo_etat_lire(&e)) {
        ESP_LOGI(TAG_RESULTAT, "contrat (%s) : meteo_etat_lire -> valide=0 (aucune météo connue pour la zone actuelle)",
                 quand);
        return;
    }
    char temp[12], mini[12], maxi[12];
    meteo::ecrire_dixiemes(std::lroundf(e.temperature_c * 10.0F), temp, sizeof(temp));
    meteo::ecrire_dixiemes(std::lroundf(e.min_c * 10.0F), mini, sizeof(mini));
    meteo::ecrire_dixiemes(std::lroundf(e.max_c * 10.0F), maxi, sizeof(maxi));
    ESP_LOGI(TAG_RESULTAT,
             "contrat (%s) : meteo_etat_lire -> valide=%d perime=%d maj=UNIX %lld temperature=%s °C min=%s max=%s (%s) "
             "code=%d "
             "jour=%d libellé=« %s »",
             quand, e.valide ? 1 : 0, e.perime ? 1 : 0, static_cast<long long>(e.maj_epoch_s), temp, mini, maxi,
             e.minmax_connus ? "du jour" : "prévision du jour absente", e.code_wmo, e.jour ? 1 : 0, e.libelle);
}

const char *nom_issue(Issue i) {
    switch (i) {
    case Issue::Ok:
        return "réussi";
    case Issue::Memoire:
        return "pas de place en PSRAM pour le tampon";
    case Issue::Requete:
        return "adresse non construite";
    case Issue::Reseau:
        return "réseau (DNS, connexion, TLS ou délai)";
    case Issue::Http:
        return "le serveur n'a pas répondu 200";
    case Issue::TropDeRequetes:
        return "trop de requêtes (HTTP 429)";
    case Issue::TropGrand:
        return "réponse plus grande que le tampon";
    case Issue::Illisible:
        return "réponse illisible";
    }
    return "?";
}

// ---- la tâche ------------------------------------------------------------------------------------------------------

bool reseau_et_heure_pretes() {
    const EtatReseau r = reseau_etat();
    // SNTP doit avoir répondu : sans heure juste, TLS refuse le certificat du serveur (date de validité).
    return r.ip_obtenue && r.heure_synchronisee && static_cast<int64_t>(std::time(nullptr)) > kInstantMin;
}

// Attend `delai_s` secondes, en tranches de 30 s pour signaler au journal le moment où la météo devient périmée. Rend vrai si un
// changement de zone a réveillé la tâche avant la fin.
bool attendre(int64_t delai_s) {
    constexpr int64_t kTrancheS = 30;
    const int64_t fin_s = esp_timer_get_time() / 1000000 + delai_s;
    MeteoEtat e = {};
    bool dernier_perime = meteo_etat_lire(&e) && e.perime;
    for (;;) {
        const int64_t reste_s = fin_s - esp_timer_get_time() / 1000000;
        if (reste_s <= 0) {
            return false;
        }
        const int64_t tranche_s = reste_s < kTrancheS ? reste_s : kTrancheS;
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(static_cast<uint32_t>(tranche_s) * 1000U)) != 0) {
            return true;
        }
        if (meteo_etat_lire(&e) && e.perime != dernier_perime) {
            dernier_perime = e.perime;
            journaliser_contrat(e.perime ? "elle vient de devenir périmée" : "elle n'est plus périmée");
        }
    }
}

void tache_meteo(void *) {
    bool trop_de_requetes = false;
    for (;;) {
        while (!reseau_et_heure_pretes()) {
            ulTaskNotifyTake(pdTRUE,
                             pdMS_TO_TICKS(kPasAttenteMs)); // un changement de zone réveille aussi, sans importance ici
        }

        journaliser_contrat("réseau et heure prêts, avant la requête");
        ZoneMeteo zone = {};
        PositionE5 position = {};
        zone_lire_e5(&zone, &position);
        ESP_LOGI(TAG, "requête Open-Meteo pour « %s » (%d échec(s) de suite jusqu'ici)", zone.nom, meteo_etat_echecs());
        const Essai e = interroger(position);

        bool immediat = false;
        if (e.issue == Issue::Ok) {
            PositionE5 maintenant = {};
            zone_lire_e5(nullptr, &maintenant);
            if (!(maintenant == position)) {
                ESP_LOGI(TAG, "la zone a changé pendant la requête : réponse écartée, nouvelle requête");
                immediat = true;
            } else {
                meteo_etat_publier(e.releve);
                enregistrer_cache(e.releve, position);
                journaliser_releve(zone, e);
                journaliser_contrat("après la réponse");
                trop_de_requetes = false;
            }
        } else {
            meteo_etat_compter_echec();
            trop_de_requetes = (e.issue == Issue::TropDeRequetes);
            ESP_LOGW(TAG, "essai raté : %s (HTTP %d) ; %d échec(s) de suite", nom_issue(e.issue), e.statut_http,
                     meteo_etat_echecs());
        }

        if (!immediat) {
            const int64_t delai_s = meteo::delai_avant_essai_s(meteo_etat_echecs(), trop_de_requetes,
                                                               static_cast<uint8_t>(esp_random() & 0xFFU));
            ESP_LOGI(TAG, "prochain essai dans %lld s (ou tout de suite si la zone change)",
                     static_cast<long long>(delai_s));
            if (!attendre(delai_s)) {
                continue; // le délai est écoulé
            }
        }
        // Réveil par un changement de zone : on laisse passer une rafale de changements avant de demander.
        vTaskDelay(pdMS_TO_TICKS(kAntiRebondZoneMs));
        ulTaskNotifyTake(pdTRUE, 0);
    }
}

} // namespace

void meteo_client_zone_changee() {
    meteo_etat_oublier();
    if (g_tache != nullptr) {
        xTaskNotifyGive(g_tache);
    }
}

void meteo_client_demarrer() {
    relire_cache();
    journaliser_contrat("au démarrage, depuis le cache");
    if (xTaskCreate(tache_meteo, "meteo", kPileOctets, nullptr, 3, &g_tache) != pdPASS) {
        ESP_LOGE(TAG, "tâche météo non créée");
    }
}

} // namespace ecran
