#include "vigie_client.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>
#include <string_view>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "miniz.h" // tinfl_decompress : ROM de l'ESP32-P4 (esp32p4.rom.ld), aucun code de décompression dans la flash

#include "ecran/gzip_flux.hpp"
#include "ecran/i18n.hpp"
#include "ecran/reglages_alertes.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_planning.hpp"
#include "ecran/vigie_requete.hpp"
#include "meteo_zone.h"
#include "reglages_alertes.h"
#include "reseau.h"
#include "verrou_tls.h"
#include "vigie_etat.h"

// Client des alertes officielles de Vigie. Même schéma que le client météo (main/meteo_client.cpp), écrit à part : tampons en PSRAM,
// un seul TLS à la fois (verrou_tls.h), priorité d'arrière-plan, délai de 15 s. Il ne dépend pas du client météo.

namespace ecran {

namespace {

const char *TAG = "vigie";
// Ligne de résultat que le banc de test cherche : « VIGIE … » à chaque réponse lue.
const char *TAG_RESULTAT = "VIGIE";

// Tampons de réception et de texte : 64 Ko chacun, en PSRAM, rendus après chaque essai. Pourquoi 64 Ko : les réponses réelles du
// 07/11/2026 font 426 à 19 238 octets décompressés (test/host/donnees/vigie_*.json ; La Palma, neuf avertissements, est la plus
// grande ; 26 190 octets quand l'adresse ne filtre pas les langues) et 426 à 1 883 octets sur le fil. Une réponse plus grande est
// REFUSÉE (`Issue::TropGrand`), jamais lue à moitié. Pourquoi en PSRAM : un tampon de plusieurs Ko en RAM interne mangerait le budget
// de la prise de contact TLS (jalon 1b).
constexpr size_t kTailleRecu = 64 * 1024;
constexpr size_t kTailleTexte = 64 * 1024;
constexpr uint32_t kAttenteVerrouTlsMs = 40000; // une recherche de ville tient le verrou 8 s au plus (geocodage.cpp)
constexpr int kDelaiHttpMs = 15000;             // connexion, TLS et lecture : au-delà, l'essai est un échec
constexpr uint32_t kPasAttenteMs = 2000;        // contrôle de l'IP et de l'heure avant le premier essai
constexpr int64_t kAntiRebondS = 3;             // plusieurs changements de réglage d'affilée = une seule requête
constexpr int64_t kPasSondageS = 5;             // les réglages sont relus au moins toutes les 5 s pendant l'attente
constexpr int64_t kInstantMin = 946684800;      // 1er janvier 2000 : en dessous, l'horloge n'est pas réglée
constexpr uint32_t kPileOctets = 12288;         // la prise de contact TLS demande plusieurs Ko de pile
// Priorité d'ARRIÈRE-PLAN (voir meteo_client.cpp : la prise de contact TLS calcule plusieurs secondes sans jamais s'endormir ; au
// dessus de la priorité de la tâche d'inactivité, le chien de garde des tâches se déclenche).
constexpr UBaseType_t kPrioriteVigie = tskIDLE_PRIORITY;

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

// ---- ce qui part dans la requête -------------------------------------------------------------------------------------

// Tout ce qui décide de la requête. Si l'un de ces champs change, l'ancienne réponse ne vaut plus pour le nouveau lieu, la nouvelle
// adresse ou la nouvelle langue : on l'oublie et on redemande. `periode_min` change seule : on replanifie sans redemander.
struct Reglage {
    bool actif = false;
    char adresse[vigie::ADRESSE_MAX + 1] = {}; // normalisée (https://, sans « / » final), ou vide : instance publique
    int periode_min = vigie::PERIODE_DEFAUT_MIN;
    int32_t latitude_e5 = 0;
    int32_t longitude_e5 = 0;
    char langue[4] = {}; // « fr »

    bool meme_requete(const Reglage &autre) const {
        return actif == autre.actif && latitude_e5 == autre.latitude_e5 && longitude_e5 == autre.longitude_e5 &&
               std::strcmp(adresse, autre.adresse) == 0 && std::strcmp(langue, autre.langue) == 0;
    }
};

// Degrés décimaux (float de meteo_zone.h) -> centièmes de millième de degré. Même arrondi que le client météo.
int32_t en_e5(float degres) { return static_cast<int32_t>(std::lround(static_cast<double>(degres) * 100000.0)); }

Reglage lire_reglage() {
    Reglage r;
    const reglages::Alertes a = reglages_alertes();
    r.actif = a.actives;
    r.periode_min = a.periode_min;
    // L'adresse des réglages (déjà normalisée à l'enregistrement), ou l'instance publique si elle est vide.
    reglages::adresse_utilisee(a, r.adresse, sizeof r.adresse);
    ZoneMeteo zone = {};
    zone_lire(&zone); // la zone enregistrée, ou Lyon par défaut : la même que celle de la météo
    r.latitude_e5 = en_e5(zone.latitude);
    r.longitude_e5 = en_e5(zone.longitude);
    const std::string_view code = i18n::code(i18n::langue_courante());
    std::snprintf(r.langue, sizeof r.langue, "%.*s", static_cast<int>(code.size()), code.data());
    return r;
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
    Memoire,        // pas de place en PSRAM pour les tampons
    Requete,        // adresse non construite (position ou adresse invraisemblables : ne devrait pas arriver)
    Reseau,         // DNS, connexion, TLS ou délai dépassé
    Http,           // le serveur a répondu autre chose que 200
    TropDeRequetes, // HTTP 429 ou 503 : l'instance publique nous demande de ralentir
    TropGrand,      // réponse (ou texte décompressé) au-delà du tampon
    Illisible,      // 200, mais la réponse n'est pas celle qu'on attend
};

struct Essai {
    Issue issue = Issue::Reseau;
    int statut_http = 0;
    size_t octets_fil = 0;   // octets reçus sur le fil
    size_t octets_texte = 0; // octets du JSON lu (après décompression)
    bool gzip = false;
    int duree_ms = 0;         // aller-retour réseau
    int duree_inflate_us = 0; // décompression
    int duree_lecture_us = 0; // lecture du JSON par le noyau
};

// Décompresse le gzip reçu dans `texte`. Jamais « en ligne » : le décompresseur de la ROM pèse ≈ 11 Ko, pris en PSRAM, et ses
// variables restent hors de la fonction qui tient les tampons. Rend l'issue ; `texte_n` reçoit la taille du texte.
__attribute__((noinline)) Issue decompresser(const uint8_t *fil, size_t n, uint8_t *texte, size_t capacite,
                                             size_t &texte_n, int &duree_us) {
    const size_t debut = gzip_flux::longueur_entete(fil, n);
    if (debut == 0) {
        ESP_LOGW(TAG, "réponse gzip refusée : en-tête invalide ou coupé (%u octets reçus)", static_cast<unsigned>(n));
        return Issue::Illisible;
    }
    auto *tinfl = static_cast<tinfl_decompressor *>(
        heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (tinfl == nullptr) {
        ESP_LOGE(TAG, "pas de place en PSRAM pour le décompresseur (%u octets)",
                 static_cast<unsigned>(sizeof(tinfl_decompressor)));
        return Issue::Memoire;
    }
    tinfl_init(tinfl);
    // Flux DEFLATE BRUT : pas d'en-tête zlib (TINFL_FLAG_PARSE_ZLIB_HEADER absent), toute l'entrée d'un coup (HAS_MORE_INPUT absent),
    // sortie d'un seul tenant assez grande pour tout le flux (NON_WRAPPING). Le pied gzip (8 octets) n'est pas dans l'entrée.
    size_t entree = n - debut - gzip_flux::FIN;
    size_t ecrit = capacite;
    const int64_t t0 = esp_timer_get_time();
    const tinfl_status etat =
        tinfl_decompress(tinfl, fil + debut, &entree, texte, texte, &ecrit, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    duree_us = static_cast<int>(esp_timer_get_time() - t0);
    heap_caps_free(tinfl);
    texte_n = ecrit;
    if (etat == TINFL_STATUS_HAS_MORE_OUTPUT) {
        ESP_LOGW(TAG, "réponse gzip refusée : le texte dépasse le tampon de %u octets",
                 static_cast<unsigned>(capacite));
        return Issue::TropGrand;
    }
    if (etat != TINFL_STATUS_DONE) {
        ESP_LOGW(TAG, "réponse gzip refusée : flux DEFLATE invalide ou incomplet (statut tinfl %d)",
                 static_cast<int>(etat));
        return Issue::Illisible;
    }
    // Le pied annonce la taille du texte (modulo 2^32) : elle doit être celle que `tinfl` a rendue.
    if (gzip_flux::taille_annoncee(fil, n) != static_cast<int64_t>(static_cast<uint32_t>(ecrit))) {
        ESP_LOGW(TAG, "réponse gzip refusée : taille annoncée %lld, taille obtenue %u",
                 static_cast<long long>(gzip_flux::taille_annoncee(fil, n)), static_cast<unsigned>(ecrit));
        return Issue::Illisible;
    }
    return Issue::Ok;
}

const char *nom_statut(vigie::Statut s) {
    switch (s) {
    case vigie::Statut::Ok:
        return "ok";
    case vigie::Statut::Illisible:
        return "JSON illisible";
    case vigie::Statut::VersionInconnue:
        return "version inconnue";
    case vigie::Statut::Incoherent:
        return "réponse incohérente";
    }
    return "?";
}

// Lit le JSON dans `sortie`. Jamais « en ligne » : l'analyse n'existe sur la pile que le temps de la lecture, pas pendant la prise de
// contact TLS, qui est le pic de pile de la tâche.
__attribute__((noinline)) bool lire_reponse(std::string_view texte, std::string_view langue, vigie::Alertes &sortie,
                                            int &duree_us) {
    const int64_t t0 = esp_timer_get_time();
    const vigie::Statut s = vigie::analyser(texte, langue, sortie);
    duree_us = static_cast<int>(esp_timer_get_time() - t0);
    if (s != vigie::Statut::Ok) {
        ESP_LOGW(TAG, "réponse refusée : %s", nom_statut(s));
        return false;
    }
    return true;
}

// Un aller-retour complet. Les deux tampons sont pris en PSRAM et rendus avant de sortir. La réponse lue est écrite dans le tampon
// de travail de vigie_etat.h (seulement si l'issue est `Ok`) : le publier est l'affaire de l'appelant.
Essai interroger(const Reglage &r, vigie::Alertes &sortie) {
    Essai e;
    char url[vigie::TAILLE_REQUETE_MAX];
    if (vigie::construire_requete(r.adresse, r.latitude_e5, r.longitude_e5, r.langue, url, sizeof(url)) == 0) {
        e.issue = Issue::Requete;
        return e;
    }

    const Memoire avant = mesurer_memoire();
    journaliser_memoire("avant la requête", avant);

    auto *recu = static_cast<uint8_t *>(heap_caps_malloc(kTailleRecu, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto *texte = static_cast<uint8_t *>(heap_caps_malloc(kTailleTexte, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (recu == nullptr || texte == nullptr) {
        heap_caps_free(recu);
        heap_caps_free(texte);
        e.issue = Issue::Memoire;
        return e;
    }
    Reception rec = {};
    rec.donnees = recu;
    rec.capacite = kTailleRecu;

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = kDelaiHttpMs;
    config.event_handler = sur_evenement_http;
    config.user_data = &rec;
    config.crt_bundle_attach =
        esp_crt_bundle_attach;    // certificats racine du paquet d'ESP-IDF (Mozilla) : le serveur est vérifié
    config.buffer_size = 2048;    // en-têtes de réponse
    config.buffer_size_tx = 1024; // ligne de requête : 512 par défaut ne suffit pas pour une adresse longue
    config.keep_alive_enable = false;

    // Un seul TLS à la fois dans tout le firmware (verrou_tls.h) : la poignée de main demande ≈ 65 Ko de RAM interne, et la carte
    // n'en a pas assez pour deux. Le verrou couvre exactement esp_http_client_init -> esp_http_client_cleanup ; il n'est pas tenu
    // pendant la décompression ni la lecture du JSON.
    if (!verrou_tls_prendre(kAttenteVerrouTlsMs)) {
        ESP_LOGW(TAG, "verrou TLS occupé depuis plus de %u ms : essai reporté",
                 static_cast<unsigned>(kAttenteVerrouTlsMs));
        heap_caps_free(recu);
        heap_caps_free(texte);
        e.issue = Issue::Reseau;
        return e;
    }
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        verrou_tls_rendre();
        heap_caps_free(recu);
        heap_caps_free(texte);
        e.issue = Issue::Reseau;
        return e;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    // Le serveur ne compresse que si on le demande (corps de plus de ≈ 500 octets) : dix fois moins d'octets pour neuf avertissements.
    esp_http_client_set_header(client, "Accept-Encoding", "gzip");
    esp_http_client_set_header(client, "User-Agent", "Meridien/ESP32-P4 (alertes)");

    const int64_t debut_us = esp_timer_get_time();
    const esp_err_t err = esp_http_client_perform(client);
    e.duree_ms = static_cast<int>((esp_timer_get_time() - debut_us) / 1000);
    e.statut_http = esp_http_client_get_status_code(client);
    const int erreur_socket = esp_http_client_get_errno(client);
    esp_http_client_cleanup(client);
    verrou_tls_rendre();
    e.octets_fil = rec.n;

    if (rec.connecte) {
        journaliser_memoire("une fois la connexion TLS établie", rec.apres_connexion);
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "requête échouée : %s (errno %d) après %d ms", esp_err_to_name(err), erreur_socket, e.duree_ms);
        e.issue = Issue::Reseau;
    } else if (e.statut_http == 429 || e.statut_http == 503) {
        e.issue = Issue::TropDeRequetes;
    } else if (e.statut_http != 200) {
        e.issue = Issue::Http;
    } else if (rec.trop) {
        e.issue = Issue::TropGrand;
    } else {
        const uint8_t *corps = recu;
        size_t corps_n = rec.n;
        Issue lu = Issue::Ok;
        if (gzip_flux::est_gzip(recu, rec.n)) {
            e.gzip = true;
            size_t texte_n = 0;
            lu = decompresser(recu, rec.n, texte, kTailleTexte, texte_n, e.duree_inflate_us);
            corps = texte;
            corps_n = texte_n;
        }
        if (lu != Issue::Ok) {
            e.issue = lu;
        } else {
            e.octets_texte = corps_n;
            const bool ok = lire_reponse(std::string_view(reinterpret_cast<const char *>(corps), corps_n), r.langue,
                                         sortie, e.duree_lecture_us);
            e.issue = ok ? Issue::Ok : Issue::Illisible;
        }
    }
    heap_caps_free(recu);
    heap_caps_free(texte);

    const Memoire apres = mesurer_memoire();
    journaliser_memoire("après la requête", apres);
    ESP_LOGI(TAG, "écart de mémoire libre sur la requête : interne %+d o, PSRAM %+d o",
             static_cast<int>(apres.interne) - static_cast<int>(avant.interne),
             static_cast<int>(apres.psram) - static_cast<int>(avant.psram));
    return e;
}

const char *nom_issue(Issue i) {
    switch (i) {
    case Issue::Ok:
        return "réussi";
    case Issue::Memoire:
        return "pas de place en PSRAM pour les tampons";
    case Issue::Requete:
        return "adresse non construite";
    case Issue::Reseau:
        return "réseau (DNS, connexion, TLS ou délai)";
    case Issue::Http:
        return "le serveur n'a pas répondu 200";
    case Issue::TropDeRequetes:
        return "trop de requêtes (HTTP 429 ou 503)";
    case Issue::TropGrand:
        return "réponse plus grande que le tampon";
    case Issue::Illisible:
        return "réponse illisible";
    }
    return "?";
}

const char *nom_niveau(vigie::Niveau n) {
    switch (n) {
    case vigie::Niveau::Vert:
        return "vert";
    case vigie::Niveau::Jaune:
        return "jaune";
    case vigie::Niveau::Orange:
        return "orange";
    case vigie::Niveau::Rouge:
        return "rouge";
    case vigie::Niveau::Inconnu:
        break;
    }
    return "inconnu";
}

// La réponse lue, au journal : de quoi vérifier sur la carte ce que l'écran montrera (aucun secret : des textes publics).
void journaliser_reponse(const vigie::Alertes &a, const Essai &e, const Reglage &r) {
    ESP_LOGI(TAG_RESULTAT,
             "VIGIE %s ; %s : %u alerte(s) gardée(s) sur %u lues (%u doublon(s), %u omise(s), %u ignorée(s)) ; "
             "couverture %s ; %u octets "
             "sur le fil%s, %u octets de JSON ; réseau %d ms, décompression %d µs, lecture %d µs",
             r.langue, r.adresse, static_cast<unsigned>(a.nombre), static_cast<unsigned>(a.lues),
             static_cast<unsigned>(a.doublons), static_cast<unsigned>(a.omises), static_cast<unsigned>(a.ignorees),
             a.couverture.complete() ? "complète" : "INCOMPLÈTE", static_cast<unsigned>(e.octets_fil),
             e.gzip ? " (gzip)" : "", static_cast<unsigned>(e.octets_texte), e.duree_ms, e.duree_inflate_us,
             e.duree_lecture_us);
    for (std::size_t i = 0; i < a.nombre; ++i) {
        const vigie::Alerte &x = a.alertes[i];
        ESP_LOGI(TAG_RESULTAT,
                 "  alerte %u : niveau %s (« %s »), phénomène « %s », %u texte(s), émetteur « %s », début UNIX %lld, "
                 "fin UNIX %lld",
                 static_cast<unsigned>(i), nom_niveau(x.niveau), x.niveau_original, x.phenomene_original,
                 static_cast<unsigned>(x.nb_textes), x.emetteur, static_cast<long long>(x.debut.value_or(-1)),
                 static_cast<long long>(x.fin ? *x.fin : x.expire.value_or(-1)));
    }
}

// ---- la tâche ------------------------------------------------------------------------------------------------------

bool reseau_et_heure_pretes() {
    const EtatReseau r = reseau_etat();
    // SNTP doit avoir répondu : sans heure juste, TLS refuse le certificat du serveur (date de validité).
    return r.ip_obtenue && r.heure_synchronisee && static_cast<int64_t>(std::time(nullptr)) > kInstantMin;
}

int64_t maintenant_s() { return esp_timer_get_time() / 1000000; }

uint8_t alea() { return static_cast<uint8_t>(esp_random() & 0xFFU); }

// Dort au plus `secondes` secondes ; un réveil anticipé (vigie_client_a_change) rend la main tout de suite.
void dormir(int64_t secondes) {
    if (secondes < 1) {
        secondes = 1;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(static_cast<uint32_t>(secondes) * 1000U));
}

void tache_vigie(void *) {
    int echecs = 0;                // essais ratés de suite depuis la dernière réponse lue
    bool trop_de_requetes = false; // le dernier échec était un 429 ou un 503
    Reglage derniere; // la requête à laquelle correspondent la réponse publiée et la date du prochain essai
    bool a_une_derniere = false;
    bool publiee = false;               // une réponse de CETTE requête est en mémoire
    int64_t dernier_essai_s = -1000000; // début du dernier essai (secondes depuis le démarrage)
    int64_t prochain_s = 0;
    bool premier = true;
    for (;;) {
        while (!reseau_et_heure_pretes()) {
            dormir(kPasAttenteMs / 1000);
        }
        if (premier) {
            // Pas de rafale au démarrage : le Wi-Fi, l'heure et la météo passent d'abord (60 à 120 s d'attente).
            premier = false;
            prochain_s = maintenant_s() + vigie::delai_premier_essai_s(alea());
            ESP_LOGI(TAG, "premier essai dans %lld s", static_cast<long long>(prochain_s - maintenant_s()));
        }

        // Les réglages sont relus à chaque tour : un changement de lieu, de langue, d'adresse ou de période se voit dans les
        // 5 secondes, sans que les modules qui les écrivent aient à connaître ce client (vigie_client_a_change() l'accélère).
        const Reglage r = lire_reglage();
        if (!r.actif) {
            if (publiee) {
                ESP_LOGI(TAG, "alertes désactivées : plus de requête, plus de bandeau");
                vigie_etat_oublier();
                publiee = false;
            }
            derniere = r;
            a_une_derniere = true;
            dormir(kPasSondageS);
            continue;
        }
        if (!a_une_derniere) {
            derniere = r; // premier tour : le premier essai est déjà planifié
            a_une_derniere = true;
        } else if (!r.meme_requete(derniere)) {
            // Lieu, adresse, langue ou activation changés : l'ancienne réponse ne vaut plus. Une seule requête, après un court
            // délai (plusieurs changements de suite = une seule requête) et une minute au moins après la précédente.
            if (publiee) {
                vigie_etat_oublier();
                publiee = false;
            }
            echecs = 0;
            trop_de_requetes = false;
            const int64_t au_plus_tot = dernier_essai_s + vigie::ESPACEMENT_MIN_S;
            const int64_t apres_rebond = maintenant_s() + kAntiRebondS;
            prochain_s = au_plus_tot > apres_rebond ? au_plus_tot : apres_rebond;
            derniere = r;
            ESP_LOGI(TAG, "requête changée (lieu, langue, adresse ou activation) : nouvel essai dans %lld s",
                     static_cast<long long>(prochain_s - maintenant_s()));
        } else if (r.periode_min != derniere.periode_min) {
            // Seule la période a changé : on replanifie depuis le dernier essai, sans redemander plus tôt qu'une minute après.
            const int64_t delai_s =
                vigie::delai_avant_essai_s(echecs, trop_de_requetes, vigie::periode_s(r.periode_min), alea());
            const int64_t depuis_dernier = dernier_essai_s + delai_s;
            const int64_t au_plus_tot = maintenant_s() + kAntiRebondS;
            prochain_s = depuis_dernier > au_plus_tot ? depuis_dernier : au_plus_tot;
            derniere = r;
            ESP_LOGI(TAG, "période changée (%d min) : prochain essai dans %lld s", r.periode_min,
                     static_cast<long long>(prochain_s - maintenant_s()));
        }

        const int64_t reste_s = prochain_s - maintenant_s();
        if (reste_s > 0) {
            dormir(reste_s < kPasSondageS ? reste_s : kPasSondageS);
            continue;
        }

        // ---- un essai ----
        dernier_essai_s = maintenant_s();
        ESP_LOGI(TAG, "requête Vigie %s (lat %ld e5, lon %ld e5, langue %s ; %d échec(s) de suite jusqu'ici)",
                 r.adresse, static_cast<long>(r.latitude_e5), static_cast<long>(r.longitude_e5), r.langue, echecs);
        vigie::Alertes *travail = vigie_etat_travail();
        const Essai e = travail != nullptr ? interroger(r, *travail) : Essai{Issue::Memoire};
        ESP_LOGI(TAG, "pile de la tâche : %u octets jamais utilisés sur %u",
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileOctets));

        bool ecartee = false;
        if (e.issue == Issue::Ok) {
            if (!lire_reglage().meme_requete(r)) {
                // Le lieu, l'adresse ou la langue a changé pendant la requête : la réponse est celle de l'ancien lieu. Écartée ;
                // le tour suivant voit le changement et planifie la bonne requête.
                ESP_LOGI(TAG, "le lieu, l'adresse ou la langue a changé pendant la requête : réponse écartée");
                ecartee = true;
            } else {
                journaliser_reponse(*travail, e, r); // avant la publication : elle échange les deux tampons
                vigie_etat_publier(static_cast<int64_t>(std::time(nullptr)));
                publiee = true;
                echecs = 0;
                trop_de_requetes = false;
            }
        } else {
            ++echecs;
            trop_de_requetes = (e.issue == Issue::TropDeRequetes);
            ESP_LOGW(TAG,
                     "essai raté : %s (HTTP %d) ; %d échec(s) de suite ; la réponse précédente, s'il y en a une, reste "
                     "en place",
                     nom_issue(e.issue), e.statut_http, echecs);
        }
        if (!ecartee) {
            const int64_t delai_s =
                vigie::delai_avant_essai_s(echecs, trop_de_requetes, vigie::periode_s(r.periode_min), alea());
            prochain_s = maintenant_s() + delai_s;
            ESP_LOGI(TAG, "prochain essai dans %lld s (plus tôt si le lieu, la langue ou l'adresse change)",
                     static_cast<long long>(delai_s));
        }
    }
}

} // namespace

void vigie_client_a_change() {
    if (g_tache != nullptr) {
        xTaskNotifyGive(g_tache);
    }
}

void vigie_client_demarrer() {
    reglages_alertes_demarrer(); // la NVS est ouverte à ce stade (après reglages_demarrer())
    // Les tailles réelles sur le RISC-V (le PC en annonce d'autres : alignement des entiers 64 bits).
    ESP_LOGI(TAG,
             "tailles : réponse lue %u o (2 tampons en PSRAM), tampons de réception %u o + texte %u o (PSRAM, rendus "
             "après chaque essai)",
             static_cast<unsigned>(sizeof(vigie::Alertes)), static_cast<unsigned>(kTailleRecu),
             static_cast<unsigned>(kTailleTexte));
    if (xTaskCreate(tache_vigie, "vigie", kPileOctets, nullptr, kPrioriteVigie, &g_tache) != pdPASS) {
        ESP_LOGE(TAG, "tâche Vigie non créée");
    }
}

} // namespace ecran
