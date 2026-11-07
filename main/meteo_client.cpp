#include "meteo_client.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>
#include <string_view>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/meteo_cache.hpp"
#include "ecran/meteo_planning.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_riche.hpp"
#include "ecran/meteo_synthese.hpp"
#include "horloge.h"
#include "http_commun.h"
#include "meteo_etat.h"
#include "meteo_interne.h"
#include "reglages.h"
#include "reseau.h"

namespace ecran {

namespace {

const char *TAG = "meteo";
// Ligne de résultat que le banc de test cherche : « METEO … » à chaque réponse lue.
const char *TAG_RESULTAT = "METEO";

constexpr const char *kEspaceNvs = "meteo";
constexpr const char *kCleCache = "cache";
// L'aller-retour HTTPS (tampons en PSRAM, gzip, verrou TLS, taille maximale d'une réponse) est dans main/http_commun.{h,cpp}.
// User-Agent : un nom fixe, sans version ni identifiant (rien à formater sur la pile de 12 Ko, rien de variable envoyé) ; Open-Meteo
// n'exige pas de contact dans le User-Agent (MET Norway l'exige : son appelant donnera le sien).
constexpr const char *kAgentUtilisateur = "Meridien/ESP32-P4 (open-meteo-client)";
constexpr uint32_t kAttenteVerrouTlsMs = 40000; // une recherche de ville tient le verrou 8 s au plus (geocodage.cpp)
constexpr int kDelaiHttpMs = 15000;             // connexion, TLS et lecture : au-delà, l'essai est un échec
constexpr uint32_t kPasAttenteMs = 2000;        // contrôle de l'IP et de l'heure avant le premier essai
constexpr uint32_t kAntiRebondZoneMs = 1500;    // plusieurs changements de zone d'affilée = une seule requête
constexpr int64_t kInstantMin = 946684800;      // 1er janvier 2000 : en dessous, l'horloge n'est pas réglée
constexpr uint32_t kPileOctets = 12288;         // la prise de contact TLS demande plusieurs Ko de pile
// Priorité d'ARRIÈRE-PLAN : la prise de contact TLS calcule plusieurs secondes sans jamais s'endormir. À une priorité au-dessus de
// celle de la tâche d'inactivité (0), la tâche IDLE du cœur qui l'exécute ne tourne plus et le chien de garde des tâches (5 s) se
// déclenche. À la priorité 0 elle partage le cœur avec IDLE tranche par tranche (1 ms) : IDLE nourrit le chien de garde.
constexpr UBaseType_t kPrioriteMeteo = tskIDLE_PRIORITY;

TaskHandle_t g_tache = nullptr;

// ---- mémoire -------------------------------------------------------------------------------------------------------

using client_http::Memoire;
using client_http::mesurer_memoire;

void journaliser_memoire(const char *quand, const Memoire &m) {
    ESP_LOGI(TAG,
             "mémoire libre %s : interne %u o (plus grand bloc %u o, minimum depuis le démarrage %u o), PSRAM %u o "
             "(plus grand bloc %u o)",
             quand, static_cast<unsigned>(m.interne), static_cast<unsigned>(m.interne_bloc),
             static_cast<unsigned>(m.interne_min), static_cast<unsigned>(m.psram), static_cast<unsigned>(m.psram_bloc));
}

// ---- une requête ---------------------------------------------------------------------------------------------------

// Les issues d'un aller-retour sont celles du client commun (http_commun.h) ; `Requete` y est aussi « adresse non construite ».
using Issue = client_http::Issue;

struct Essai {
    Issue issue = Issue::Reseau;
    int statut_http = 0;
    size_t octets = 0;
    int duree_ms = 0;
};

// Zone de travail en PSRAM, allouée une fois : la météo lue (1 312 octets sur le PC) et la copie du contrat v2 (2 088 octets) qui sert au journal.
// Hors de la pile (12 Ko, dont le pic est la prise de contact TLS) et hors de la RAM interne, qui est rare.
struct Travail {
    meteo::Meteo meteo;
    MeteoDetail detail;
};

Travail *g_travail = nullptr;

Travail *travail() {
    if (g_travail == nullptr) {
        void *place = heap_caps_calloc(1, sizeof(Travail), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (place == nullptr) {
            ESP_LOGE(TAG, "pas de place en PSRAM pour la zone de travail (%u octets)",
                     static_cast<unsigned>(sizeof(Travail)));
            return nullptr;
        }
        g_travail = new (place) Travail();
    }
    return g_travail;
}

// Lit la réponse JSON dans `sortie`. Fonction à part et jamais « en ligne » : l'analyse (≈ 1,3 Ko de résultat) n'existe sur la pile
// que le temps de la lecture, pas pendant la prise de contact TLS, qui est le pic de pile de la tâche.
__attribute__((noinline)) bool lire_reponse(std::string_view texte, int64_t maintenant, meteo::Meteo &sortie) {
    const meteo::AnalyseRiche a = meteo::analyser_riche(texte, maintenant);
    if (!a.ok()) {
        ESP_LOGW(
            TAG, "réponse refusée : %.*s (champ %.*s)", static_cast<int>(meteo::message_court(a.diag.erreur).size()),
            meteo::message_court(a.diag.erreur).data(), static_cast<int>(a.diag.champ.size()), a.diag.champ.data());
        return false;
    }
    // Une section riche cassée est vidée (le reste reste utilisable) : on le dit au journal, avec le champ en cause.
    const struct {
        const char *nom;
        const meteo::Diagnostic &diag;
    } sections[] = {{"heures", a.diag_heures}, {"jours", a.diag_jours}, {"quarts d'heure", a.diag_quarts}};
    for (const auto &s : sections) {
        if (!s.diag.ok()) {
            ESP_LOGW(TAG, "section « %s » de la réponse écartée : %.*s (champ %.*s)", s.nom,
                     static_cast<int>(meteo::message_court(s.diag.erreur).size()),
                     meteo::message_court(s.diag.erreur).data(), static_cast<int>(s.diag.champ.size()),
                     s.diag.champ.data());
        }
    }
    sortie = a.meteo;
    return true;
}

// Ce que `lire_corps` a besoin pour lire la réponse : où écrire la météo.
struct Lecture {
    meteo::Meteo *sortie;
};

// Appelée par le client commun avec le texte DÉCOMPRESSÉ de la réponse 200, une fois la connexion fermée (hors du verrou TLS).
bool lire_corps(std::string_view corps, void *utilisateur) {
    auto *lecture = static_cast<Lecture *>(utilisateur);
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    return lire_reponse(corps, maintenant, *lecture->sortie);
}

// Un aller-retour complet. Les tampons sont pris en PSRAM par le client commun (http_commun.h) et rendus avant de sortir. La météo
// lue est écrite dans `sortie` (seulement si l'issue est `Ok`). `modeles` : la valeur de `models=` de la source choisie (vide pour
// « Automatique » : l'adresse est alors celle d'avant ce réglage, à l'octet près). La première ligne de la requête fait 885 octets au
// plus avec le plus long modèle de la table (test_meteo_appareil.cpp) ; esp_http_client envoie les en-têtes par morceaux, seule cette
// ligne et le premier en-tête doivent tenir dans `buffer_size_tx` (1024 octets, http_commun.cpp).
Essai interroger(const PositionE5 &position, std::string_view modeles, meteo::Meteo &sortie) {
    Essai e;
    char url[meteo::TAILLE_REQUETE_MAX];
    if (meteo::construire_requete(position.latitude_e5, position.longitude_e5, url, sizeof(url), modeles) == 0) {
        e.issue = Issue::Requete;
        return e;
    }

    const Memoire avant = mesurer_memoire();
    journaliser_memoire("avant la requête", avant);

    client_http::Requete requete;
    requete.adresse = url;
    requete.user_agent = kAgentUtilisateur;
    requete.accept = "application/json";
    requete.delai_ms = kDelaiHttpMs;
    requete.attente_verrou_tls_ms = kAttenteVerrouTlsMs;
    Lecture lecture = {&sortie};
    const client_http::Reponse r = client_http::obtenir(requete, lire_corps, &lecture);
    e.issue = r.issue;
    e.statut_http = r.statut_http;
    e.octets = r.octets; // le texte lu (décompressé) ; les octets du fil sont au journal du client commun
    e.duree_ms = r.duree_ms;

    if (r.connecte) {
        journaliser_memoire("une fois la connexion TLS établie", r.apres_connexion);
    }

    const Memoire apres = mesurer_memoire();
    journaliser_memoire("après la requête", apres);
    ESP_LOGI(TAG, "écart de mémoire libre sur la requête : interne %+d o, PSRAM %+d o",
             static_cast<int>(apres.interne) - static_cast<int>(avant.interne),
             static_cast<int>(apres.psram) - static_cast<int>(avant.psram));
    return e;
}

// ---- cache NVS -----------------------------------------------------------------------------------------------------

// Le cache est VERSIONNÉ (format v2, ecran/meteo_riche.hpp) : un cache d'un autre format (le v1 écrit par les firmwares d'avant la
// météo riche) est reconnu à sa version avant toute lecture, ignoré, et remplacé à la première réponse.
void enregistrer_cache(const meteo::Meteo &meteo, const PositionE5 &position) {
    uint8_t brut[meteo::TAILLE_CACHE_RICHE_MAX]; // ≈ 860 octets
    const size_t n =
        meteo::serialiser_cache_meteo(meteo, position.latitude_e5, position.longitude_e5, brut, sizeof(brut));
    if (n == 0) {
        ESP_LOGE(TAG, "cache non écrit : météo ou position refusées par le noyau");
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

// Jamais « en ligne » : le résultat de la lecture (≈ 1,3 Ko) n'occupe la pile d'`app_main` que le temps de la relecture.
__attribute__((noinline)) void relire_cache() {
    PositionE5 position = {};
    zone_lire_e5(nullptr, &position);
    nvs_handle_t h = 0;
    const esp_err_t ouvert = nvs_open(kEspaceNvs, NVS_READONLY, &h);
    if (ouvert != ESP_OK) {
        ESP_LOGI(TAG, "pas de cache météo (NVS : %s)", esp_err_to_name(ouvert));
        return;
    }
    uint8_t brut[meteo::TAILLE_CACHE_RICHE_MAX];
    size_t taille = sizeof(brut);
    const esp_err_t err = nvs_get_blob(h, kCleCache, brut, &taille);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "pas de cache météo enregistré");
        return;
    }
    if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        // Plus grand que le plus grand cache v2 : ce n'est pas un cache que ce firmware sait lire.
        ESP_LOGI(TAG, "cache météo d'un format inconnu (trop grand), ignoré ; il sera remplacé à la prochaine réponse");
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cache météo non lu : %s", esp_err_to_name(err));
        return;
    }
    const meteo::LectureCacheMeteo lu = meteo::deserialiser_cache_meteo(brut, taille);
    if (!lu.ok()) {
        if (lu.diag.erreur == meteo::Erreur::VersionInconnue) {
            ESP_LOGI(TAG, "cache météo d'un ancien format, ignoré ; il sera remplacé à la prochaine réponse");
        } else {
            ESP_LOGW(TAG, "cache météo refusé : %.*s (%.*s) ; il sera remplacé à la prochaine réponse",
                     static_cast<int>(meteo::message_court(lu.diag.erreur).size()),
                     meteo::message_court(lu.diag.erreur).data(), static_cast<int>(lu.diag.champ.size()),
                     lu.diag.champ.data());
        }
        return;
    }
    if (!(PositionE5{lu.latitude_e5, lu.longitude_e5} == position)) {
        ESP_LOGI(TAG, "cache météo d'un autre lieu que la zone actuelle : ignoré");
        return;
    }
    if (!meteo_etat_publier(lu.meteo)) {
        ESP_LOGE(TAG, "cache relu mais non publié : pas de place pour l'état de la météo");
        return;
    }
    const meteo::Releve &r = lu.meteo.releve;
    char t[12];
    meteo::ecrire_dixiemes(r.temperature_dixiemes, t, sizeof(t));
    const std::string_view libelle = meteo::decrire(r.code_wmo, r.jour).libelle;
    ESP_LOGI(TAG,
             "cache relu : %s °C, %.*s, reçu à l'instant UNIX %lld ; %u heures, %u jours, %u quarts d'heure (fraîcheur "
             ": voir "
             "la ligne contrat suivante)",
             t, static_cast<int>(libelle.size()), libelle.data(), static_cast<long long>(r.recu_a_s),
             static_cast<unsigned>(lu.meteo.riche.nb_heures), static_cast<unsigned>(lu.meteo.riche.nb_jours),
             static_cast<unsigned>(lu.meteo.riche.nb_quarts));
}

// ---- journal du résultat -------------------------------------------------------------------------------------------

void journaliser_releve(const ZoneMeteo &zone, const meteo::Releve &r, const Essai &e) {
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

// Un nombre du contrat (exact au dixième) en texte « -0,5 » : pas de printf("%f") dans les journaux.
void dixiemes_de(float valeur, char *sortie, size_t n) {
    meteo::ecrire_dixiemes(static_cast<int32_t>(std::lroundf(valeur * 10.0F)), sortie, n);
}

// Relit la météo détaillée PAR LE CONTRAT v2 (meteo_detail_lire), exactement comme le fera l'écran « très visuel », et la met au
// journal : c'est la preuve sur la carte que ce que l'écran recevra (courant, heures, jours, pluie proche) est la réponse réelle.
void journaliser_detail(const char *quand) {
    Travail *t = travail();
    if (t == nullptr) {
        return;
    }
    const MeteoDetail &d = t->detail;
    if (!meteo_detail_lire(&t->detail)) {
        ESP_LOGI(TAG_RESULTAT,
                 "contrat v2 (%s) : meteo_detail_lire -> valide=0 (aucune météo connue pour la zone actuelle)", quand);
        return;
    }
    ESP_LOGI(TAG_RESULTAT,
             "contrat v2 (%s) : meteo_detail_lire -> valide=1 perime=%d version=%u fuseau=%s décalage=%d s ; %d heures "
             "(en cours : %d), %d jours (en cours : %d), %d quarts d'heure (en cours : %d)",
             quand, d.perime ? 1 : 0, static_cast<unsigned>(d.version), d.fuseau, static_cast<int>(d.decalage_utc_s),
             d.nb_heures, d.heure_courante, d.nb_jours, d.jour_courant, d.nb_quarts, d.quart_courant);

    char t_c[12], res[12], vent[12], raf[12], pluie_mm[12];
    dixiemes_de(d.courant.temperature_c, t_c, sizeof(t_c));
    dixiemes_de(d.courant.ressentie_c, res, sizeof(res));
    dixiemes_de(d.courant.vent_kmh, vent, sizeof(vent));
    dixiemes_de(d.courant.rafales_kmh, raf, sizeof(raf));
    dixiemes_de(d.courant.precipitation_mm, pluie_mm, sizeof(pluie_mm));
    char proba[8];
    if (d.courant.proba_precip_pct == METEO_PROBA_INCONNUE) {
        std::snprintf(proba, sizeof(proba), "?");
    } else {
        std::snprintf(proba, sizeof(proba), "%d", static_cast<int>(d.courant.proba_precip_pct));
    }
    char tendance[12];
    if (std::isnan(d.courant.tendance_pression_3h_hpa)) {
        std::snprintf(tendance, sizeof(tendance),
                      "?"); // moins de 3 h de relevés depuis le démarrage ou le changement de zone
    } else {
        dixiemes_de(d.courant.tendance_pression_3h_hpa, tendance, sizeof(tendance));
    }
    ESP_LOGI(TAG_RESULTAT,
             "  courant : %s °C (ressentie %s), « %s », humidité %d %%, nuages %d %%, pluie %s mm, probabilité %s %%, "
             "vent %s "
             "km/h (rafales %s) direction %d°, pression %d hPa (tendance sur 3 h : %s hPa)",
             t_c, res, d.courant.libelle, static_cast<int>(d.courant.humidite_pct),
             static_cast<int>(d.courant.nuages_pct), pluie_mm, proba, vent, raf,
             static_cast<int>(d.courant.direction_vent_deg), static_cast<int>(std::lroundf(d.courant.pression_hpa)),
             tendance);

    if (d.heure_courante >= 0) {
        const MeteoHeure &h = d.heures[d.heure_courante];
        char ht[12], hp[12], uv[12];
        dixiemes_de(h.temperature_c, ht, sizeof(ht));
        dixiemes_de(h.precipitation_mm, hp, sizeof(hp));
        dixiemes_de(h.uv, uv, sizeof(uv));
        ESP_LOGI(TAG_RESULTAT,
                 "  heure en cours (début UNIX %lld) : %s °C, pluie %s mm, probabilité %d %%, UV %s, code %d",
                 static_cast<long long>(h.debut_epoch_s), ht, hp, static_cast<int>(h.proba_precip_pct), uv,
                 static_cast<int>(h.code_wmo));
    }
    if (d.jour_courant >= 0) {
        const MeteoJour &j = d.jours[d.jour_courant];
        char mn[12], mx[12], duree[12], soleil[12], cumul[12];
        dixiemes_de(j.min_c, mn, sizeof(mn));
        dixiemes_de(j.max_c, mx, sizeof(mx));
        dixiemes_de(j.duree_jour_h, duree, sizeof(duree));
        dixiemes_de(j.ensoleillement_h, soleil, sizeof(soleil));
        dixiemes_de(j.precipitation_mm, cumul, sizeof(cumul));
        ESP_LOGI(
            TAG_RESULTAT,
            "  jour en cours : min %s / max %s °C, jour de %s h%s, ensoleillement %s h, pluie %s mm sur %d h, code %d",
            mn, mx, duree, j.sans_soleil ? " (nuit polaire)" : "", soleil, cumul,
            static_cast<int>(j.heures_precipitation), static_cast<int>(j.code_wmo));
    }
    const MeteoPluieProche &p = d.pluie;
    char cumul[12], fort[12];
    dixiemes_de(p.cumul_mm, cumul, sizeof(cumul));
    dixiemes_de(p.max_quart_mm, fort, sizeof(fort));
    if (!p.connue) {
        ESP_LOGI(TAG_RESULTAT, "  pluie proche : inconnue (aucun quart d'heure ne couvre l'heure actuelle)");
    } else if (p.en_cours) {
        if (p.fin_dans_min >= 0) {
            ESP_LOGI(TAG_RESULTAT,
                     "  pluie proche : il pleut, jusqu'à dans %d min ; %s mm sur %d min (plus fort quart : %s mm)%s",
                     p.fin_dans_min, cumul, p.fenetre_min, fort, p.perimee ? " [périmée]" : "");
        } else {
            ESP_LOGI(TAG_RESULTAT,
                     "  pluie proche : il pleut, sur toute la fenêtre de %d min ; %s mm (plus fort quart : %s mm)%s",
                     p.fenetre_min, cumul, fort, p.perimee ? " [périmée]" : "");
        }
    } else if (p.dans_min >= 0) {
        ESP_LOGI(TAG_RESULTAT,
                 "  pluie proche : de la pluie dans %d min ; %s mm sur %d min (plus fort quart : %s mm)%s", p.dans_min,
                 cumul, p.fenetre_min, fort, p.perimee ? " [périmée]" : "");
    } else {
        ESP_LOGI(TAG_RESULTAT, "  pluie proche : pas de pluie dans les %d min qui viennent%s", p.fenetre_min,
                 p.perimee ? " [périmée]" : "");
    }
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
        journaliser_detail("réseau et heure prêts, avant la requête");
        ZoneMeteo zone = {};
        PositionE5 position = {};
        zone_lire_e5(&zone, &position);
        // La source choisie, relue à chaque requête : changer de source (écran ou page) agit à la requête d'après, ou tout de suite.
        char source[16] = {};
        reglages_source_meteo(source, sizeof(source));
        const std::string_view modeles = meteo::fournisseurs::modeles_de(source);
        ESP_LOGI(TAG, "requête Open-Meteo pour « %s », source « %s » (%d échec(s) de suite jusqu'ici)", zone.nom,
                 source, meteo_etat_echecs());
        Travail *tr = travail();
        const Essai e = tr != nullptr ? interroger(position, modeles, tr->meteo) : Essai{Issue::Memoire};
        // Pile : marge restante la plus basse depuis le début de la tâche (le pic est la prise de contact TLS, juste faite).
        ESP_LOGI(TAG, "pile de la tâche : %u octets jamais utilisés sur %u",
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileOctets));

        bool immediat = false;
        if (e.issue == Issue::Ok) {
            PositionE5 maintenant = {};
            zone_lire_e5(nullptr, &maintenant);
            char source_apres[16] = {};
            reglages_source_meteo(source_apres, sizeof(source_apres));
            if (!(maintenant == position)) {
                ESP_LOGI(TAG, "la zone a changé pendant la requête : réponse écartée, nouvelle requête");
                immediat = true;
            } else if (std::strcmp(source, source_apres) != 0) {
                // Une réponse de l'ancien modèle ne doit jamais s'afficher sous la nouvelle source.
                ESP_LOGI(TAG,
                         "la source a changé pendant la requête (« %s » -> « %s ») : réponse écartée, nouvelle requête",
                         source, source_apres);
                immediat = true;
            } else if (!meteo_etat_publier(tr->meteo)) {
                ESP_LOGE(TAG, "réponse lue mais non publiée : pas de place pour l'état de la météo");
                meteo_etat_compter_echec();
            } else {
                enregistrer_cache(tr->meteo, position);
                journaliser_releve(zone, tr->meteo.releve, e);
                journaliser_contrat("après la réponse");
                journaliser_detail("après la réponse");
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

void meteo_client_source_changee() {
    // Le relevé gardé en NVS ne connaît que la position : il montrerait la météo de l'ancienne source (au prochain démarrage, avant
    // la première réponse). On l'efface ; la tâche en redemande un à la nouvelle source.
    nvs_handle_t h = 0;
    if (nvs_open(kEspaceNvs, NVS_READWRITE, &h) == ESP_OK) {
        const esp_err_t err = nvs_erase_key(h, kCleCache);
        if (err == ESP_OK) {
            nvs_commit(h);
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "relevé gardé non effacé après le changement de source : %s", esp_err_to_name(err));
        }
        nvs_close(h);
    }
    meteo_client_zone_changee();
}

void meteo_client_demarrer() {
    // Les tailles réelles sur le RISC-V (le PC en annonce d'autres : alignement des entiers 64 bits).
    ESP_LOGI(TAG, "tailles : météo %u o, contrat v2 %u o, cache NVS au plus %u o, tampon de réception %u o (PSRAM)",
             static_cast<unsigned>(sizeof(meteo::Meteo)), static_cast<unsigned>(sizeof(MeteoDetail)),
             static_cast<unsigned>(meteo::TAILLE_CACHE_RICHE_MAX), static_cast<unsigned>(client_http::kTailleTampon));
    relire_cache();
    journaliser_contrat("au démarrage, depuis le cache");
    journaliser_detail("au démarrage, depuis le cache");
    if (xTaskCreate(tache_meteo, "meteo", kPileOctets, nullptr, kPrioriteMeteo, &g_tache) != pdPASS) {
        ESP_LOGE(TAG, "tâche météo non créée");
    }
}

} // namespace ecran
