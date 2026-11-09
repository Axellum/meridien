#include "meteo_client.h"

#include <atomic>
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
#include "ecran/meteo_metno.hpp"
#include "ecran/meteo_nws.hpp"
#include "ecran/meteo_planning.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_requete_fournisseurs.hpp"
#include "ecran/meteo_riche.hpp"
#include "ecran/meteo_rythme_natif.hpp"
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
// Le service qui a donné le relevé gardé (une valeur de `Famille`, un octet) : écrit avec lui, dans la même validation.
constexpr const char *kCleOrigine = "origine";
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
bool g_cache_relu = false; // le cache NVS a déjà été relu (tâche principale seulement, voir meteo_client_relire_cache)

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
    int64_t expires_s = 0; // en-tête `Expires` de la réponse (UNIX) ; 0 : absent ou illisible
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

// Espaces de travail des analyseurs des services lus directement : ≈ 8 Ko pour MET Norway, ≈ 25 Ko pour le NWS. En PSRAM, alloués à
// la première utilisation (jamais pour Open-Meteo seul), hors de la pile de 12 Ko.
struct Natif {
    meteo::metno::Serie metno;
    meteo::nws::Serie nws;
};

Natif *g_natif = nullptr;

Natif *natif() {
    if (g_natif == nullptr) {
        void *place = heap_caps_calloc(1, sizeof(Natif), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (place == nullptr) {
            ESP_LOGE(TAG, "pas de place en PSRAM pour l'espace de travail des services directs (%u octets)",
                     static_cast<unsigned>(sizeof(Natif)));
            return nullptr;
        }
        g_natif = new (place) Natif();
    }
    return g_natif;
}

// L'adresse de grille du NWS (réponse de /points) pour la position actuelle, gardée en RAM 24 h (meteo_rythme_natif.hpp). Lue et écrite
// par la seule tâche météo.
struct MailleGardee {
    bool valide = false;
    PositionE5 position = {};
    int64_t obtenue_s = 0;
    meteo::nws::Maille maille;
};

MailleGardee g_maille;

// Pas de requête au service direct avant cet instant (UNIX) : posé après un échec, pour que le repli sur Open-Meteo ne fasse pas
// frapper le service en panne à chaque essai. Remis à zéro quand la zone ou la source change (autre lieu, autre service).
std::atomic<int64_t> g_natif_pas_avant_s{0};

// Accepte (ou refuse) le résultat d'un analyseur : journalise ce qu'il a écarté et recopie la météo dans `sortie`. Fonction à part et
// jamais « en ligne » : le résultat de l'analyse (≈ 1,3 Ko) n'existe sur la pile que le temps de la lecture, pas pendant la prise de
// contact TLS, qui est le pic de pile de la tâche.
__attribute__((noinline)) bool accepter_analyse(const meteo::AnalyseRiche &a, meteo::Meteo &sortie) {
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

// Open-Meteo.
__attribute__((noinline)) bool lire_reponse(std::string_view texte, int64_t maintenant, meteo::Meteo &sortie) {
    return accepter_analyse(meteo::analyser_riche(texte, maintenant), sortie);
}

// MET Norway : la réponse ne donne pas le fuseau, c'est celui de la ville choisie.
__attribute__((noinline)) bool lire_reponse_metno(std::string_view texte, int64_t maintenant,
                                                  const PositionE5 &position, std::string_view iana,
                                                  meteo::Meteo &sortie) {
    Natif *n = natif();
    if (n == nullptr) {
        return false;
    }
    return accepter_analyse(
        meteo::metno::analyser_metno(texte, maintenant, position.latitude_e5, position.longitude_e5, iana, n->metno),
        sortie);
}

// NWS, grille : le fuseau vient de la maille (réponse de /points).
__attribute__((noinline)) bool lire_reponse_nws(std::string_view texte, int64_t maintenant, const PositionE5 &position,
                                                const meteo::nws::Maille &maille, meteo::Meteo &sortie) {
    Natif *n = natif();
    if (n == nullptr) {
        return false;
    }
    return accepter_analyse(
        meteo::nws::analyser_nws(texte, maille, maintenant, position.latitude_e5, position.longitude_e5, n->nws),
        sortie);
}

// NWS, /points : la maille (bureau, x, y, fuseau). Écrite dans `nouvelle` seulement si la réponse est lue sans refus.
__attribute__((noinline)) bool lire_reponse_points(std::string_view texte, meteo::nws::Maille &nouvelle) {
    meteo::nws::Maille lue;
    const meteo::Diagnostic d = meteo::nws::analyser_points(texte, lue);
    if (!d.ok()) {
        ESP_LOGW(TAG, "réponse /points refusée : %.*s (champ %.*s)",
                 static_cast<int>(meteo::message_court(d.erreur).size()), meteo::message_court(d.erreur).data(),
                 static_cast<int>(d.champ.size()), d.champ.data());
        return false;
    }
    nouvelle = lue;
    return true;
}

// Ce que les fonctions de lecture ont besoin : où écrire la météo, et ce qui n'est pas dans la réponse.
struct Lecture {
    meteo::Meteo *sortie = nullptr;
    PositionE5 position = {};
    std::string_view iana;                         // MET Norway : fuseau de la ville
    const meteo::nws::Maille *maille = nullptr;    // NWS, grille : la maille demandée
    meteo::nws::Maille *maille_a_ecrire = nullptr; // NWS, /points : où écrire la maille lue
};

// Appelées par le client commun avec le texte DÉCOMPRESSÉ de la réponse 200, une fois la connexion fermée (hors du verrou TLS).
bool lire_corps(std::string_view corps, void *utilisateur) {
    auto *lecture = static_cast<Lecture *>(utilisateur);
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    return lire_reponse(corps, maintenant, *lecture->sortie);
}

bool lire_corps_metno(std::string_view corps, void *utilisateur) {
    auto *lecture = static_cast<Lecture *>(utilisateur);
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    return lire_reponse_metno(corps, maintenant, lecture->position, lecture->iana, *lecture->sortie);
}

bool lire_corps_points(std::string_view corps, void *utilisateur) {
    auto *lecture = static_cast<Lecture *>(utilisateur);
    return lire_reponse_points(corps, *lecture->maille_a_ecrire);
}

bool lire_corps_grille(std::string_view corps, void *utilisateur) {
    auto *lecture = static_cast<Lecture *>(utilisateur);
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    return lire_reponse_nws(corps, maintenant, lecture->position, *lecture->maille, *lecture->sortie);
}

// Un aller-retour HTTPS : l'adresse, qui parle (`user_agent`), le type attendu, la plus grande réponse acceptée, la fonction de lecture.
// Les tampons sont pris en PSRAM par le client commun (http_commun.h) et rendus avant de sortir. L'adresse n'est jamais journalisée.
Essai aller_retour(const char *url, const char *user_agent, const char *accept, size_t taille_max,
                   client_http::LireCorps lire, Lecture &lecture) {
    Essai e;
    const Memoire avant = mesurer_memoire();
    journaliser_memoire("avant la requête", avant);

    client_http::Requete requete;
    requete.adresse = url;
    requete.user_agent = user_agent;
    requete.accept = accept;
    requete.delai_ms = kDelaiHttpMs;
    requete.attente_verrou_tls_ms = kAttenteVerrouTlsMs;
    requete.taille_max = taille_max;
    const client_http::Reponse r = client_http::obtenir(requete, lire, &lecture);
    e.issue = r.issue;
    e.statut_http = r.statut_http;
    e.octets = r.octets; // le texte lu (décompressé) ; les octets du fil sont au journal du client commun
    e.duree_ms = r.duree_ms;
    e.expires_s = r.expires_s;

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

// Open-Meteo. `modeles` : la valeur de `models=` de la source choisie (vide pour « Automatique » : l'adresse est alors celle d'avant
// ce réglage, à l'octet près ; c'est aussi l'adresse du repli). La première ligne de la requête fait 885 octets au plus avec le plus
// long modèle de la table (test_meteo_appareil.cpp) ; esp_http_client envoie les en-têtes par morceaux, seule cette ligne et le
// premier en-tête doivent tenir dans `buffer_size_tx` (1024 octets, http_commun.cpp).
Essai interroger(const PositionE5 &position, std::string_view modeles, meteo::Meteo &sortie) {
    char url[meteo::TAILLE_REQUETE_MAX];
    if (meteo::construire_requete(position.latitude_e5, position.longitude_e5, url, sizeof(url), modeles) == 0) {
        Essai e;
        e.issue = Issue::Requete;
        return e;
    }
    Lecture lecture;
    lecture.sortie = &sortie;
    return aller_retour(url, kAgentUtilisateur, "application/json", client_http::kTailleTampon, lire_corps, lecture);
}

// MET Norway : une requête, 60 à 91 Ko décompressés. Le User-Agent porte le contact exigé par le service.
Essai interroger_metno(const PositionE5 &position, const char *fuseau, meteo::Meteo &sortie) {
    char url[meteo::fournisseurs::TAILLE_REQUETE_METNO_MAX];
    if (meteo::fournisseurs::construire_requete_metno(position.latitude_e5, position.longitude_e5, url, sizeof(url)) ==
        0) {
        Essai e;
        e.issue = Issue::Requete;
        return e;
    }
    Lecture lecture;
    lecture.sortie = &sortie;
    lecture.position = position;
    lecture.iana = fuseau;
    return aller_retour(url, meteo::fournisseurs::USER_AGENT.data(), meteo::fournisseurs::ACCEPT_METNO,
                        meteo::fournisseurs::TAILLE_REPONSE_METNO_MAX, lire_corps_metno, lecture);
}

// NWS, première requête : la maille de la position (bureau, x, y, fuseau), ou HTTP 404 hors de la couverture.
Essai demander_maille(const PositionE5 &position, meteo::nws::Maille &nouvelle) {
    char url[meteo::fournisseurs::TAILLE_REQUETE_NWS_POINTS_MAX];
    if (meteo::fournisseurs::construire_requete_nws_points(position.latitude_e5, position.longitude_e5, url,
                                                           sizeof(url)) == 0) {
        Essai e;
        e.issue = Issue::Requete;
        return e;
    }
    Lecture lecture;
    lecture.maille_a_ecrire = &nouvelle;
    return aller_retour(url, meteo::fournisseurs::USER_AGENT.data(), meteo::fournisseurs::ACCEPT_NWS,
                        meteo::fournisseurs::TAILLE_REPONSE_NWS_POINTS_MAX, lire_corps_points, lecture);
}

// NWS, seconde requête : la grille de cette maille, 120 à 240 Ko décompressés. L'adresse est reconstruite à partir de trois valeurs
// contrôlées (meteo_requete_fournisseurs.hpp), jamais copiée dans la réponse précédente.
Essai demander_grille(const PositionE5 &position, const meteo::nws::Maille &maille, meteo::Meteo &sortie) {
    char url[meteo::fournisseurs::TAILLE_REQUETE_NWS_GRILLE_MAX];
    if (meteo::fournisseurs::construire_requete_nws_grille(std::string_view(maille.bureau), maille.x, maille.y, url,
                                                           sizeof(url)) == 0) {
        Essai e;
        e.issue = Issue::Requete;
        return e;
    }
    Lecture lecture;
    lecture.sortie = &sortie;
    lecture.position = position;
    lecture.maille = &maille;
    return aller_retour(url, meteo::fournisseurs::USER_AGENT.data(), meteo::fournisseurs::ACCEPT_NWS,
                        meteo::fournisseurs::TAILLE_REPONSE_NWS_GRILLE_MAX, lire_corps_grille, lecture);
}

// NWS : la maille gardée si elle sert encore (même position, moins de 24 h), sinon /points ; puis la grille. Si la grille échoue, la
// maille gardée est oubliée (le bureau ou la maille a pu changer) ; un 404 sur une maille GARDÉE est retenté une fois avec une maille neuve.
Essai interroger_nws(const PositionE5 &position, meteo::Meteo &sortie) {
    const int64_t maintenant = static_cast<int64_t>(std::time(nullptr));
    const bool gardee = g_maille.valide && g_maille.position == position &&
                        meteo::fournisseurs::maille_nws_valable(g_maille.obtenue_s, maintenant);
    if (!gardee) {
        meteo::nws::Maille nouvelle;
        const Essai p = demander_maille(position, nouvelle);
        if (p.issue != Issue::Ok) {
            g_maille.valide = false;
            return p;
        }
        g_maille = MailleGardee{true, position, maintenant, nouvelle};
    }
    Essai e = demander_grille(position, g_maille.maille, sortie);
    if (e.issue == Issue::Ok) {
        return e;
    }
    g_maille.valide = false;
    if (gardee && e.issue == Issue::Http && e.statut_http == 404) {
        meteo::nws::Maille nouvelle;
        const Essai p = demander_maille(position, nouvelle);
        if (p.issue != Issue::Ok) {
            return p;
        }
        g_maille = MailleGardee{true, position, maintenant, nouvelle};
        e = demander_grille(position, g_maille.maille, sortie);
        if (e.issue != Issue::Ok) {
            g_maille.valide = false;
        }
    }
    return e;
}

// Le service direct demandé par la source choisie.
Essai interroger_natif(meteo::fournisseurs::Famille famille, const PositionE5 &position, const char *fuseau,
                       meteo::Meteo &sortie) {
    return famille == meteo::fournisseurs::Famille::MetNo ? interroger_metno(position, fuseau, sortie)
                                                          : interroger_nws(position, sortie);
}

// ---- cache NVS -----------------------------------------------------------------------------------------------------

// Le cache est VERSIONNÉ (format v2, ecran/meteo_riche.hpp) : un cache d'un autre format (le v1 écrit par les firmwares d'avant la
// météo riche) est reconnu à sa version avant toute lecture, ignoré, et remplacé à la première réponse.
void enregistrer_cache(const meteo::Meteo &meteo, const PositionE5 &position, meteo::fournisseurs::Famille origine) {
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
            err = nvs_set_u8(h, kCleOrigine, static_cast<uint8_t>(origine)); // avec le relevé, dans la même validation
        }
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
    // Le service qui a donné ce relevé. Absent (relevé écrit avant ce réglage) ou inconnu : Open-Meteo, la seule source d'alors.
    uint8_t origine_brute = 0;
    if (nvs_get_u8(h, kCleOrigine, &origine_brute) != ESP_OK ||
        origine_brute > static_cast<uint8_t>(meteo::fournisseurs::Famille::Nws)) {
        origine_brute = static_cast<uint8_t>(meteo::fournisseurs::Famille::OpenMeteo);
    }
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
    if (!meteo_etat_publier(lu.meteo, static_cast<meteo::fournisseurs::Famille>(origine_brute))) {
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

// Une valeur que la source ne donne pas (MET Norway et le NWS ne donnent pas tout) s'écrit « ? » : jamais « -3276,8 », jamais « 0 ».
void dixiemes_ou_inconnu(int32_t dixiemes, char *sortie, size_t n) {
    if (dixiemes == meteo::INCONNU_DIX) {
        std::snprintf(sortie, n, "?");
    } else {
        meteo::ecrire_dixiemes(dixiemes, sortie, n);
    }
}

// Un entier du noyau ou du contrat v2 (humidité, nuages, direction, heures) : -1 veut dire « inconnu ».
void entier_ou_inconnu(int valeur, char *sortie, size_t n) {
    if (valeur < 0) {
        std::snprintf(sortie, n, "?");
    } else {
        std::snprintf(sortie, n, "%d", valeur);
    }
}

void journaliser_releve(const ZoneMeteo &zone, const meteo::Releve &r, const Essai &e,
                        meteo::fournisseurs::Famille origine) {
    const int64_t maintenant = r.recu_a_s;
    const meteo::Synthese s = meteo::synthetiser(r, maintenant, 0);
    char temp[12], ress[12], mini[12], maxi[12], vent[12], pression[12], direction[8], humidite[8];
    dixiemes_ou_inconnu(r.temperature_dixiemes, temp, sizeof(temp));
    dixiemes_ou_inconnu(r.ressentie_dixiemes, ress, sizeof(ress));
    dixiemes_ou_inconnu(s.min_dixiemes, mini, sizeof(mini));
    dixiemes_ou_inconnu(s.max_dixiemes, maxi, sizeof(maxi));
    dixiemes_ou_inconnu(r.vent_dixiemes_kmh, vent, sizeof(vent));
    dixiemes_ou_inconnu(r.pression_dixiemes_hpa, pression, sizeof(pression));
    entier_ou_inconnu(static_cast<int>(r.direction_vent_deg), direction, sizeof(direction));
    entier_ou_inconnu(static_cast<int>(r.humidite_pct), humidite, sizeof(humidite));
    char pluie[8];
    if (r.proba_precip_pct == meteo::PROBA_INCONNUE) {
        std::snprintf(pluie, sizeof(pluie), "?");
    } else {
        std::snprintf(pluie, sizeof(pluie), "%d", static_cast<int>(r.proba_precip_pct));
    }
    const std::string_view nom_source = meteo::fournisseurs::attribution_de(origine).nom;
    TexteHorloge texte = {};
    const bool horloge_ok = horloge_formater(maintenant, texte);
    ESP_LOGI(
        TAG_RESULTAT,
        "METEO %s : %s °C (ressentie %s), %.*s, %s ; min %s / max %s du jour%s ; vent %s km/h, direction %s° ; "
        "pression %s hPa ; "
        "humidité %s %% ; pluie %s %% ; mesure UNIX %lld, reçue UNIX %lld (%s, %s) ; %u jours ; %u octets en %d ms ; "
        "source %.*s",
        zone.nom, temp, ress, static_cast<int>(s.libelle.size()), s.libelle.data(), r.jour ? "jour" : "nuit", mini,
        maxi, s.minmax_connus ? "" : " (prévision du jour absente)", vent, direction, pression, humidite, pluie,
        static_cast<long long>(r.instant_s), static_cast<long long>(r.recu_a_s), horloge_ok ? texte.date : "-",
        horloge_ok ? texte.heure : "-", static_cast<unsigned>(r.nb_jours), static_cast<unsigned>(e.octets), e.duree_ms,
        static_cast<int>(nom_source.size()), nom_source.data());
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

// Un nombre du contrat (exact au dixième) en texte « -0,5 » : pas de printf("%f") dans les journaux. NaN (valeur que la source ne
// donne pas) s'écrit « ? ».
void dixiemes_de(float valeur, char *sortie, size_t n) {
    if (std::isnan(valeur)) {
        std::snprintf(sortie, n, "?");
        return;
    }
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
    char humidite[8], nuages[8], direction[8], pression[12];
    entier_ou_inconnu(static_cast<int>(d.courant.humidite_pct), humidite, sizeof(humidite));
    entier_ou_inconnu(static_cast<int>(d.courant.nuages_pct), nuages, sizeof(nuages));
    entier_ou_inconnu(static_cast<int>(d.courant.direction_vent_deg), direction, sizeof(direction));
    if (std::isnan(d.courant.pression_hpa)) {
        std::snprintf(pression, sizeof(pression), "?");
    } else {
        std::snprintf(pression, sizeof(pression), "%d", static_cast<int>(std::lroundf(d.courant.pression_hpa)));
    }
    char tendance[12];
    if (std::isnan(d.courant.tendance_pression_3h_hpa)) {
        std::snprintf(tendance, sizeof(tendance),
                      "?"); // moins de 3 h de relevés depuis le démarrage ou le changement de zone
    } else {
        dixiemes_de(d.courant.tendance_pression_3h_hpa, tendance, sizeof(tendance));
    }
    ESP_LOGI(TAG_RESULTAT,
             "  courant : %s °C (ressentie %s), « %s », humidité %s %%, nuages %s %%, pluie %s mm, probabilité %s %%, "
             "vent %s "
             "km/h (rafales %s) direction %s°, pression %s hPa (tendance sur 3 h : %s hPa)",
             t_c, res, d.courant.libelle, humidite, nuages, pluie_mm, proba, vent, raf, direction, pression, tendance);

    if (d.heure_courante >= 0) {
        const MeteoHeure &h = d.heures[d.heure_courante];
        char ht[12], hp[12], uv[12], hproba[8];
        dixiemes_de(h.temperature_c, ht, sizeof(ht));
        dixiemes_de(h.precipitation_mm, hp, sizeof(hp));
        dixiemes_de(h.uv, uv, sizeof(uv));
        entier_ou_inconnu(static_cast<int>(h.proba_precip_pct), hproba, sizeof(hproba));
        ESP_LOGI(TAG_RESULTAT,
                 "  heure en cours (début UNIX %lld) : %s °C, pluie %s mm, probabilité %s %%, UV %s, code %d",
                 static_cast<long long>(h.debut_epoch_s), ht, hp, hproba, uv, static_cast<int>(h.code_wmo));
    }
    if (d.jour_courant >= 0) {
        const MeteoJour &j = d.jours[d.jour_courant];
        char mn[12], mx[12], duree[12], soleil[12], cumul[12], heures_pluie[8];
        entier_ou_inconnu(static_cast<int>(j.heures_precipitation), heures_pluie, sizeof(heures_pluie));
        dixiemes_de(j.min_c, mn, sizeof(mn));
        dixiemes_de(j.max_c, mx, sizeof(mx));
        dixiemes_de(j.duree_jour_h, duree, sizeof(duree));
        dixiemes_de(j.ensoleillement_h, soleil, sizeof(soleil));
        dixiemes_de(j.precipitation_mm, cumul, sizeof(cumul));
        ESP_LOGI(
            TAG_RESULTAT,
            "  jour en cours : min %s / max %s °C, jour de %s h%s, ensoleillement %s h, pluie %s mm sur %s h, code %d",
            mn, mx, duree, j.sans_soleil ? " (nuit polaire)" : "", soleil, cumul, heures_pluie,
            static_cast<int>(j.code_wmo));
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
        const meteo::fournisseurs::Famille choisie = meteo::fournisseurs::famille_de(source);
        const std::string_view modeles = meteo::fournisseurs::modeles_de(source); // vide pour un service lu directement
        ESP_LOGI(TAG, "requête pour « %s », source « %s » (%d échec(s) de suite jusqu'ici)", zone.nom, source,
                 meteo_etat_echecs());
        Travail *tr = travail();
        Essai e = Essai{Issue::Memoire};
        // Le service qui donne VRAIMENT la météo : la source choisie, ou Open-Meteo quand le service direct a échoué (repli, ADR-0014).
        meteo::fournisseurs::Famille origine = meteo::fournisseurs::Famille::OpenMeteo;
        if (tr != nullptr) {
            if (choisie != meteo::fournisseurs::Famille::OpenMeteo) {
                const int64_t maintenant_s = static_cast<int64_t>(std::time(nullptr));
                const int64_t pause_s = g_natif_pas_avant_s.load() - maintenant_s;
                if (pause_s <= 0) {
                    e = interroger_natif(choisie, position, zone.fuseau, tr->meteo);
                    if (e.issue == Issue::Ok) {
                        origine = choisie;
                        g_natif_pas_avant_s.store(0);
                    } else {
                        ESP_LOGW(TAG,
                                 "service direct : essai raté (%s, HTTP %d) ; repli sur Open-Meteo « Automatique »",
                                 nom_issue(e.issue), e.statut_http);
                        // Pas de nouvel essai au service avant l'écart minimum entre deux requêtes : le repli ne doit pas le faire
                        // frapper à chaque essai quand il est en panne.
                        g_natif_pas_avant_s.store(maintenant_s + meteo::fournisseurs::plancher_s(choisie));
                    }
                } else {
                    ESP_LOGI(TAG, "service direct en pause après un échec (encore %lld s) : Open-Meteo « Automatique »",
                             static_cast<long long>(pause_s));
                }
            }
            if (origine == meteo::fournisseurs::Famille::OpenMeteo) {
                e = interroger(position, modeles, tr->meteo);
            }
        }
        // Pile : marge restante la plus basse depuis le début de la tâche (le pic est la prise de contact TLS, juste faite).
        ESP_LOGI(TAG, "pile de la tâche : %u octets jamais utilisés sur %u",
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileOctets));

        bool immediat = false;
        bool direct_reussi = false;
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
            } else if (!meteo_etat_publier(tr->meteo, origine)) {
                ESP_LOGE(TAG, "réponse lue mais non publiée : pas de place pour l'état de la météo");
                meteo_etat_compter_echec();
            } else {
                enregistrer_cache(tr->meteo, position, origine);
                journaliser_releve(zone, tr->meteo.releve, e, origine);
                journaliser_contrat("après la réponse");
                journaliser_detail("après la réponse");
                trop_de_requetes = false;
                direct_reussi = origine != meteo::fournisseurs::Famille::OpenMeteo;
            }
        } else {
            meteo_etat_compter_echec();
            trop_de_requetes = (e.issue == Issue::TropDeRequetes);
            ESP_LOGW(TAG, "essai raté : %s (HTTP %d) ; %d échec(s) de suite", nom_issue(e.issue), e.statut_http,
                     meteo_etat_echecs());
        }

        if (!immediat) {
            const uint8_t alea = static_cast<uint8_t>(esp_random() & 0xFFU);
            int64_t delai_s = 0;
            if (direct_reussi) {
                // Un service lu directement : pas avant son plancher ni son `Expires`, loin de l'heure ronde (meteo_rythme_natif.hpp).
                const int64_t maintenant_s = static_cast<int64_t>(std::time(nullptr));
                delai_s = meteo::fournisseurs::delai_apres_succes_s(origine, maintenant_s, e.expires_s, alea);
                if (e.expires_s > 0) {
                    ESP_LOGI(TAG, "en-tête Expires de la réponse : dans %lld s",
                             static_cast<long long>(e.expires_s - maintenant_s));
                } else {
                    ESP_LOGI(TAG,
                             "en-tête Expires de la réponse : absent ou illisible, le plancher du service s'applique");
                }
            } else {
                delai_s = meteo::delai_avant_essai_s(meteo_etat_echecs(), trop_de_requetes, alea);
            }
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
    g_natif_pas_avant_s.store(
        0); // un autre lieu (ou une autre source) : le service direct est de nouveau permis tout de suite
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
        nvs_erase_key(h, kCleOrigine); // l'origine n'a de sens qu'avec son relevé
        if (err == ESP_OK) {
            nvs_commit(h);
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "relevé gardé non effacé après le changement de source : %s", esp_err_to_name(err));
        }
        nvs_close(h);
    }
    meteo_client_zone_changee();
}

void meteo_client_relire_cache() {
    if (g_cache_relu) {
        return;
    }
    g_cache_relu = true; // même si la relecture ne trouve rien : une seule tentative par démarrage
    relire_cache();
}

void meteo_client_demarrer() {
    // Les tailles réelles sur le RISC-V (le PC en annonce d'autres : alignement des entiers 64 bits).
    ESP_LOGI(
        TAG,
        "tailles : météo %u o, contrat v2 %u o, cache NVS au plus %u o, tampon de réception %u o (PSRAM), espace de "
        "travail des services directs %u o (PSRAM, à la première utilisation)",
        static_cast<unsigned>(sizeof(meteo::Meteo)), static_cast<unsigned>(sizeof(MeteoDetail)),
        static_cast<unsigned>(meteo::TAILLE_CACHE_RICHE_MAX), static_cast<unsigned>(client_http::kTailleTampon),
        static_cast<unsigned>(sizeof(Natif)));
    meteo_client_relire_cache(); // déjà fait avant la première image par main.cpp ; sinon (autre appelant) fait ici
    journaliser_contrat("au démarrage, depuis le cache");
    journaliser_detail("au démarrage, depuis le cache");
    if (xTaskCreate(tache_meteo, "meteo", kPileOctets, nullptr, kPrioriteMeteo, &g_tache) != pdPASS) {
        ESP_LOGE(TAG, "tâche météo non créée");
    }
}

} // namespace ecran
