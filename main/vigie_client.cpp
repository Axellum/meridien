#include "vigie_client.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>
#include <string_view>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ecran/i18n.hpp"
#include "ecran/reglages_alertes.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_planning.hpp"
#include "ecran/vigie_requete.hpp"
#include "http_commun.h"
#include "meteo_zone.h"
#include "reglages_alertes.h"
#include "reseau.h"
#include "tache_donnees.h"
#include "vigie_etat.h"

// Client des alertes officielles de Vigie. L'aller-retour HTTPS passe par le client commun (main/http_commun.h : tampons en PSRAM,
// gzip avec CRC-32 vérifié, verrou TLS, redirections refusées, taille maximale d'une réponse). Priorité d'arrière-plan, délai de 15 s.

namespace ecran {

namespace {

const char *TAG = "vigie";
// Ligne de résultat que le banc de test cherche : « VIGIE … » à chaque réponse lue.
const char *TAG_RESULTAT = "VIGIE";

// Plus grande réponse acceptée, sur le fil comme décompressée : 64 Ko (deux tampons en PSRAM pris par le client commun et rendus
// après chaque essai). Pourquoi 64 Ko : les réponses réelles du 07/11/2026 font 426 à 19 238 octets décompressés
// (test/host/donnees/vigie_*.json ; La Palma, neuf avertissements, est la plus grande ; 26 190 octets quand l'adresse ne filtre pas
// les langues) et 426 à 1 883 octets sur le fil. Une réponse plus grande est REFUSÉE (`Issue::TropGrand`), jamais lue à moitié.
constexpr size_t kTailleMax = 64 * 1024;
constexpr const char *kAgentUtilisateur = "Meridien/ESP32-P4 (alertes)";
constexpr uint32_t kAttenteVerrouTlsMs = 40000; // une recherche de ville tient le verrou 8 s au plus (geocodage.cpp)
constexpr int kDelaiHttpMs = 15000;             // connexion, TLS et lecture : au-delà, l'essai est un échec
constexpr uint32_t kPasAttenteMs = 2000;        // contrôle de l'IP et de l'heure avant le premier essai
constexpr int64_t kAntiRebondS = 3;             // plusieurs changements de réglage d'affilée = une seule requête
constexpr int64_t kPasSondageS = 5;             // les réglages sont relus au moins toutes les 5 s pendant l'attente
constexpr int64_t kInstantMin = 946684800;      // 1er janvier 2000 : en dessous, l'horloge n'est pas réglée
// Les requêtes passent par LA tâche réseau des données (tache_donnees.h : pile de 12 Ko, priorité d'arrière-plan), un pas à la fois.

// ---- mémoire -------------------------------------------------------------------------------------------------------

void journaliser_memoire(const char *quand, const client_http::Memoire &m) {
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

// Ce que la lecture du corps reçoit et rend (client_http::LireCorps).
struct Lecture {
    std::string_view langue;
    vigie::Alertes *sortie = nullptr;
    int duree_us = 0;
};

bool lire_corps(std::string_view corps, void *utilisateur) {
    auto *l = static_cast<Lecture *>(utilisateur);
    return lire_reponse(corps, l->langue, *l->sortie, l->duree_us);
}

// Les issues du client commun, avec une seule différence : Vigie demande aussi de ralentir par un 503 (comme un 429).
Issue issue_de(const client_http::Reponse &rep) {
    switch (rep.issue) {
    case client_http::Issue::Ok:
        return Issue::Ok;
    case client_http::Issue::Memoire:
        return Issue::Memoire;
    case client_http::Issue::Requete:
        return Issue::Requete;
    case client_http::Issue::Reseau:
        return Issue::Reseau;
    case client_http::Issue::Http:
        return rep.statut_http == 503 ? Issue::TropDeRequetes : Issue::Http;
    case client_http::Issue::TropDeRequetes:
        return Issue::TropDeRequetes;
    case client_http::Issue::TropGrand:
        return Issue::TropGrand;
    case client_http::Issue::Illisible:
        break;
    }
    return Issue::Illisible;
}

// Un aller-retour complet par le client commun (main/http_commun.h). La réponse lue est écrite dans le tampon de travail de
// vigie_etat.h (seulement si l'issue est `Ok`) : le publier est l'affaire de l'appelant.
Essai interroger(const Reglage &r, vigie::Alertes &sortie) {
    Essai e;
    char url[vigie::TAILLE_REQUETE_MAX];
    if (vigie::construire_requete(r.adresse, r.latitude_e5, r.longitude_e5, r.langue, url, sizeof(url)) == 0) {
        e.issue = Issue::Requete;
        return e;
    }

    const client_http::Memoire avant = client_http::mesurer_memoire();
    journaliser_memoire("avant la requête", avant);

    Lecture lecture;
    lecture.langue = r.langue;
    lecture.sortie = &sortie;
    client_http::Requete requete;
    requete.adresse = url;
    requete.user_agent = kAgentUtilisateur;
    requete.accept = "application/json";
    requete.delai_ms = kDelaiHttpMs;
    requete.attente_verrou_tls_ms = kAttenteVerrouTlsMs;
    requete.taille_max = kTailleMax;
    const client_http::Reponse rep = client_http::obtenir(requete, lire_corps, &lecture);
    if (rep.connecte) {
        journaliser_memoire("une fois la connexion TLS établie", rep.apres_connexion);
    }
    e.issue = issue_de(rep);
    e.statut_http = rep.statut_http;
    e.octets_fil = rep.octets_fil;
    e.octets_texte = rep.octets;
    e.gzip = rep.gzip;
    e.duree_ms = rep.duree_ms;
    e.duree_inflate_us = rep.duree_decompression_us;
    e.duree_lecture_us = lecture.duree_us;

    const client_http::Memoire apres = client_http::mesurer_memoire();
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

// Le sommeil d'un pas, au moins 1 s ; un réveil anticipé (vigie_client_a_change) le raccourcit.
int64_t dormir_ms(int64_t secondes) { return (secondes < 1 ? 1 : secondes) * 1000; }

// Ce que l'ancienne boucle de la tâche « vigie » gardait entre deux tours. Lu et écrit par la seule tâche réseau des données.
struct Rythme {
    int echecs = 0;                // essais ratés de suite depuis la dernière réponse lue
    bool trop_de_requetes = false; // le dernier échec était un 429 ou un 503
    Reglage derniere; // la requête à laquelle correspondent la réponse publiée et la date du prochain essai
    bool a_une_derniere = false;
    bool publiee = false;               // une réponse de CETTE requête est en mémoire
    int64_t dernier_essai_s = -1000000; // début du dernier essai (secondes depuis le démarrage)
    int64_t prochain_s = 0;
    bool premier = true;
};

Rythme *g_rythme = nullptr; // créé par vigie_client_demarrer() (tache_donnees.h : creer_etat_de_pas)

// Un pas de la tâche réseau des données (tache_donnees.h) : un tour de l'ancienne boucle de la tâche « vigie », à l'identique. Là où
// elle dormait, le pas rend la durée du sommeil ; après un essai, il rend 0 (le tour suivant tout de suite, comme avant). Un réveil
// (vigie_client_a_change) fait passer le tour suivant tout de suite : les réglages sont relus à chaque tour.
int64_t pas_vigie(bool) {
    Rythme &v = *g_rythme;
    if (!reseau_et_heure_pretes()) {
        return dormir_ms(kPasAttenteMs / 1000);
    }
    if (v.premier) {
        // Pas de rafale au démarrage : le Wi-Fi, l'heure et la météo passent d'abord (60 à 120 s d'attente).
        v.premier = false;
        v.prochain_s = maintenant_s() + vigie::delai_premier_essai_s(alea());
        ESP_LOGI(TAG, "premier essai dans %lld s", static_cast<long long>(v.prochain_s - maintenant_s()));
    }

    // Les réglages sont relus à chaque tour : un changement de lieu, de langue, d'adresse ou de période se voit dans les
    // 5 secondes, sans que les modules qui les écrivent aient à connaître ce client (vigie_client_a_change() l'accélère).
    const Reglage r = lire_reglage();
    if (!r.actif) {
        if (v.publiee) {
            ESP_LOGI(TAG, "alertes désactivées : plus de requête, plus de bandeau");
            vigie_etat_oublier();
            v.publiee = false;
        }
        v.derniere = r;
        v.a_une_derniere = true;
        return dormir_ms(kPasSondageS);
    }
    if (!v.a_une_derniere) {
        v.derniere = r; // premier tour : le premier essai est déjà planifié
        v.a_une_derniere = true;
    } else if (!r.meme_requete(v.derniere)) {
        // Lieu, adresse, langue ou activation changés : l'ancienne réponse ne vaut plus. Une seule requête, après un court
        // délai (plusieurs changements de suite = une seule requête) et une minute au moins après la précédente.
        if (v.publiee) {
            vigie_etat_oublier();
            v.publiee = false;
        }
        v.echecs = 0;
        v.trop_de_requetes = false;
        const int64_t au_plus_tot = v.dernier_essai_s + vigie::ESPACEMENT_MIN_S;
        const int64_t apres_rebond = maintenant_s() + kAntiRebondS;
        v.prochain_s = au_plus_tot > apres_rebond ? au_plus_tot : apres_rebond;
        v.derniere = r;
        ESP_LOGI(TAG, "requête changée (lieu, langue, adresse ou activation) : nouvel essai dans %lld s",
                 static_cast<long long>(v.prochain_s - maintenant_s()));
    } else if (r.periode_min != v.derniere.periode_min) {
        // Seule la période a changé : on replanifie depuis le dernier essai, sans redemander plus tôt qu'une minute après.
        const int64_t delai_s =
            vigie::delai_avant_essai_s(v.echecs, v.trop_de_requetes, vigie::periode_s(r.periode_min), alea());
        const int64_t depuis_dernier = v.dernier_essai_s + delai_s;
        const int64_t au_plus_tot = maintenant_s() + kAntiRebondS;
        v.prochain_s = depuis_dernier > au_plus_tot ? depuis_dernier : au_plus_tot;
        v.derniere = r;
        ESP_LOGI(TAG, "période changée (%d min) : prochain essai dans %lld s", r.periode_min,
                 static_cast<long long>(v.prochain_s - maintenant_s()));
    }

    const int64_t reste_s = v.prochain_s - maintenant_s();
    if (reste_s > 0) {
        return dormir_ms(reste_s < kPasSondageS ? reste_s : kPasSondageS);
    }

    // ---- un essai ----
    v.dernier_essai_s = maintenant_s();
    ESP_LOGI(TAG, "requête Vigie %s (lat %ld e5, lon %ld e5, langue %s ; %d échec(s) de suite jusqu'ici)", r.adresse,
             static_cast<long>(r.latitude_e5), static_cast<long>(r.longitude_e5), r.langue, v.echecs);
    vigie::Alertes *travail = vigie_etat_travail();
    const Essai e = travail != nullptr ? interroger(r, *travail) : Essai{Issue::Memoire};
    tache_donnees_journaliser_pile(TAG);

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
            v.publiee = true;
            v.echecs = 0;
            v.trop_de_requetes = false;
        }
    } else {
        ++v.echecs;
        v.trop_de_requetes = (e.issue == Issue::TropDeRequetes);
        ESP_LOGW(TAG,
                 "essai raté : %s (HTTP %d) ; %d échec(s) de suite ; la réponse précédente, s'il y en a une, reste "
                 "en place",
                 nom_issue(e.issue), e.statut_http, v.echecs);
    }
    if (!ecartee) {
        const int64_t delai_s =
            vigie::delai_avant_essai_s(v.echecs, v.trop_de_requetes, vigie::periode_s(r.periode_min), alea());
        v.prochain_s = maintenant_s() + delai_s;
        ESP_LOGI(TAG, "prochain essai dans %lld s (plus tôt si le lieu, la langue ou l'adresse change)",
                 static_cast<long long>(delai_s));
    }
    return 0;
}

} // namespace

void vigie_client_a_change() { tache_donnees_reveiller(ordonnanceur::Source::Vigie); }

void vigie_client_demarrer() {
    reglages_alertes_demarrer(); // la NVS est ouverte à ce stade (après reglages_demarrer())
    // Les tailles réelles sur le RISC-V (le PC en annonce d'autres : alignement des entiers 64 bits).
    ESP_LOGI(TAG,
             "tailles : réponse lue %u o (2 tampons en PSRAM), réponse HTTP %u o au plus (client commun : tampons en "
             "PSRAM, rendus après chaque essai)",
             static_cast<unsigned>(sizeof(vigie::Alertes)), static_cast<unsigned>(kTailleMax));
    g_rythme = creer_etat_de_pas<Rythme>();
    if (g_rythme == nullptr) {
        ESP_LOGE(TAG, "pas de place pour l'état du client (%u octets) : pas d'alertes",
                 static_cast<unsigned>(sizeof(Rythme)));
        return;
    }
    tache_donnees_inscrire(ordonnanceur::Source::Vigie, pas_vigie);
    tache_donnees_reveiller(ordonnanceur::Source::Vigie);
}

} // namespace ecran
