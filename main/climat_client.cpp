#include "climat_client.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string_view>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "climat_etat.h"
#include "ecran/climat_normales.hpp"
#include "ecran/climat_planning.hpp"
#include "ecran/climat_requete.hpp"
#include "ecran/reglages_alertes.hpp"
#include "horloge.h"
#include "http_commun.h"
#include "meteo_zone.h"
#include "reglages_alertes.h"
#include "reseau.h"
#include "tache_donnees.h"

// Client des normales climatiques (ADR-0029). Le modèle est vigie_client.cpp, en plus simple : une seule requête par jour, une réponse de
// 2 Ko (≈ 830 octets sur le fil, mesuré le 21/11/2026), aucun tampon en PSRAM à garder (une structure de ≈ 40 octets, climat_etat.h).

namespace ecran {

namespace {

const char *TAG = "climat";
constexpr const char *kAgentUtilisateur = "Meridien/ESP32-P4 (normales)";

// Plus grande réponse acceptée, sur le fil comme décompressée : 16 Ko (deux tampons en PSRAM, rendus après chaque essai). Les réponses
// réelles du 21/11/2026 (France métropolitaine, La Réunion, hors couverture) font 702 à 2 073 octets décompressés.
constexpr size_t kTailleMax = 16 * 1024;
constexpr uint32_t kAttenteVerrouTlsMs = 40000;
constexpr int kDelaiHttpMs = 15000;
constexpr uint32_t kPasAttenteMs = 2000; // contrôle de l'IP et de l'heure avant le premier essai
constexpr int64_t kPasSondageS = 15; // les réglages et le mois sont relus au moins toutes les 15 s pendant l'attente
constexpr int64_t kAntiRebondS = 5;  // plusieurs changements de réglage d'affilée = une seule requête
constexpr int64_t kInstantMin = 946684800; // 1er janvier 2000 : en dessous, l'horloge n'est pas réglée

// ---- ce qui part dans la requête -------------------------------------------------------------------------------------

// Tout ce qui décide de la requête. Si l'un de ces champs change, l'ancienne réponse ne vaut plus pour le nouveau lieu ou la nouvelle adresse.
struct Reglage {
    bool actif = false;
    char adresse[vigie::ADRESSE_MAX + 1] = {};
    int32_t latitude_e5 = 0;
    int32_t longitude_e5 = 0;

    bool meme_requete(const Reglage &autre) const {
        return actif == autre.actif && latitude_e5 == autre.latitude_e5 && longitude_e5 == autre.longitude_e5 &&
               std::strcmp(adresse, autre.adresse) == 0;
    }
};

// Degrés décimaux (float de meteo_zone.h) -> centièmes de millième de degré. Même arrondi que la météo et les alertes.
int32_t en_e5(float degres) { return static_cast<int32_t>(std::lround(static_cast<double>(degres) * 100000.0)); }

Reglage lire_reglage() {
    Reglage r;
    const reglages::Alertes a = reglages_alertes();
    r.actif = a.actives;
    reglages::adresse_utilisee(a, r.adresse, sizeof r.adresse);
    ZoneMeteo zone = {};
    zone_lire(&zone); // la zone enregistrée, ou Lyon par défaut : la même que celle de la météo
    r.latitude_e5 = en_e5(zone.latitude);
    r.longitude_e5 = en_e5(zone.longitude);
    return r;
}

// ---- une requête -----------------------------------------------------------------------------------------------------

enum class Issue : uint8_t {
    Ok,             // réponse lue, couverte
    HorsCouverture, // réponse lue : aucune fiche à moins de 50 km, rien à montrer
    Reseau,         // mémoire, adresse, DNS, connexion, TLS, délai, taille
    Http,           // le serveur a répondu autre chose que 200
    TropDeRequetes, // HTTP 429 ou 503
    PasDeRoute,     // HTTP 404 : cette instance de Vigie n'a pas la route (instance plus ancienne)
    Illisible,      // 200, mais pas la réponse attendue
};

const char *nom_statut(climat::Statut s) {
    switch (s) {
    case climat::Statut::Illisible:
        return "JSON illisible";
    case climat::Statut::Incoherent:
        return "réponse incohérente";
    case climat::Statut::HorsCouverture:
        return "hors couverture";
    case climat::Statut::Ok:
        break;
    }
    return "ok";
}

struct Lecture {
    climat::Normales sortie;
    climat::Statut statut = climat::Statut::Illisible;
};

bool lire_corps(std::string_view corps, void *utilisateur) {
    auto *l = static_cast<Lecture *>(utilisateur);
    l->statut = climat::analyser(corps, l->sortie);
    if (l->statut == climat::Statut::Illisible || l->statut == climat::Statut::Incoherent) {
        ESP_LOGW(TAG, "réponse refusée : %s", nom_statut(l->statut));
        return false;
    }
    return true;
}

struct Essai {
    Issue issue = Issue::Reseau;
    int statut_http = 0;
    size_t octets_fil = 0;
    size_t octets_texte = 0;
    bool gzip = false;
    int duree_ms = 0;
};

Essai interroger(const Reglage &r, Lecture &lecture) {
    Essai e;
    char url[climat::TAILLE_REQUETE_MAX];
    if (climat::construire_requete(r.adresse, r.latitude_e5, r.longitude_e5, url, sizeof url) == 0) {
        return e; // adresse ou position invraisemblables : traité comme un échec réseau, avec attente
    }
    client_http::Requete requete;
    requete.adresse = url;
    requete.user_agent = kAgentUtilisateur;
    requete.accept = "application/json";
    requete.delai_ms = kDelaiHttpMs;
    requete.attente_verrou_tls_ms = kAttenteVerrouTlsMs;
    requete.taille_max = kTailleMax;
    const client_http::Reponse rep = client_http::obtenir(requete, lire_corps, &lecture);
    e.statut_http = rep.statut_http;
    e.octets_fil = rep.octets_fil;
    e.octets_texte = rep.octets;
    e.gzip = rep.gzip;
    e.duree_ms = rep.duree_ms;
    switch (rep.issue) {
    case client_http::Issue::Ok:
        e.issue = lecture.statut == climat::Statut::HorsCouverture ? Issue::HorsCouverture : Issue::Ok;
        break;
    case client_http::Issue::Http:
        e.issue =
            rep.statut_http == 404 ? Issue::PasDeRoute : (rep.statut_http == 503 ? Issue::TropDeRequetes : Issue::Http);
        break;
    case client_http::Issue::TropDeRequetes:
        e.issue = Issue::TropDeRequetes;
        break;
    case client_http::Issue::Illisible:
        e.issue = Issue::Illisible;
        break;
    case client_http::Issue::Memoire:
    case client_http::Issue::Requete:
    case client_http::Issue::Reseau:
    case client_http::Issue::TropGrand:
        e.issue = Issue::Reseau;
        break;
    }
    return e;
}

const char *nom_issue(Issue i) {
    switch (i) {
    case Issue::Ok:
        return "réussi";
    case Issue::HorsCouverture:
        return "réussi, hors couverture";
    case Issue::Reseau:
        return "réseau (mémoire, DNS, connexion, TLS, délai ou taille)";
    case Issue::Http:
        return "le serveur n'a pas répondu 200";
    case Issue::TropDeRequetes:
        return "trop de requêtes (HTTP 429 ou 503)";
    case Issue::PasDeRoute:
        return "route absente (HTTP 404)";
    case Issue::Illisible:
        return "réponse illisible";
    }
    return "?";
}

// ---- la tâche --------------------------------------------------------------------------------------------------------

bool reseau_et_heure_pretes() {
    const EtatReseau r = reseau_etat();
    return r.ip_obtenue && r.heure_synchronisee && static_cast<int64_t>(std::time(nullptr)) > kInstantMin;
}

int64_t maintenant_s() { return esp_timer_get_time() / 1000000; }
uint8_t alea() { return static_cast<uint8_t>(esp_random() & 0xFFU); }
int64_t dormir_ms(int64_t secondes) { return (secondes < 1 ? 1 : secondes) * 1000; }

// Le mois du lieu à cet instant (1 à 12), ou 0 si l'horloge locale ne le dit pas.
int mois_local() {
    heure::HeureLocale h;
    return horloge_heure_locale(static_cast<int64_t>(std::time(nullptr)), h) ? h.mois : 0;
}

struct Rythme {
    int echecs = 0;
    bool trop_de_requetes = false;
    bool pas_de_route = false;
    Reglage derniere;
    bool a_une_derniere = false;
    bool publiee = false; // une réponse de CETTE requête est en mémoire (couverte ou non)
    bool couverte = false;
    int mois_publie = 0;
    int64_t dernier_essai_s = -1000000;
    int64_t prochain_s = 0;
    bool premier = true;
};

Rythme *g_rythme = nullptr;

void oublier(Rythme &v) {
    if (v.publiee) {
        climat_etat_oublier();
    }
    v.publiee = false;
    v.couverte = false;
    v.mois_publie = 0;
}

int64_t pas_climat(bool) {
    Rythme &v = *g_rythme;
    if (!reseau_et_heure_pretes()) {
        return dormir_ms(kPasAttenteMs / 1000);
    }
    if (v.premier) {
        v.premier = false;
        v.prochain_s = maintenant_s() + climat::delai_premier_essai_s(alea());
        ESP_LOGI(TAG, "premier essai dans %lld s", static_cast<long long>(v.prochain_s - maintenant_s()));
    }

    const Reglage r = lire_reglage();
    if (!r.actif) {
        if (v.publiee) {
            ESP_LOGI(TAG, "alertes désactivées : plus de requête, plus de pastille de normale");
            oublier(v);
        }
        v.derniere = r;
        v.a_une_derniere = true;
        return dormir_ms(kPasSondageS);
    }
    if (!v.a_une_derniere) {
        v.derniere = r; // premier tour : le premier essai est déjà planifié
        v.a_une_derniere = true;
    } else if (!r.meme_requete(v.derniere)) {
        // Lieu, adresse ou activation changés : l'ancienne réponse ne vaut plus. Une seule requête, après un court délai et cinq minutes
        // au moins après la précédente.
        oublier(v);
        v.echecs = 0;
        v.trop_de_requetes = false;
        v.pas_de_route = false;
        const int64_t au_plus_tot = v.dernier_essai_s + climat::ESPACEMENT_MIN_S;
        const int64_t apres_rebond = maintenant_s() + kAntiRebondS;
        v.prochain_s = au_plus_tot > apres_rebond ? au_plus_tot : apres_rebond;
        v.derniere = r;
        ESP_LOGI(TAG, "requête changée (lieu, adresse ou activation) : nouvel essai dans %lld s",
                 static_cast<long long>(v.prochain_s - maintenant_s()));
    } else if (v.couverte && v.mois_publie != 0) {
        // Le jour de la bascule de mois : la réponse en mémoire décrit le mois d'avant. On en redemande une, au plus une par heure.
        const int mois = mois_local();
        if (mois != 0 && mois != v.mois_publie) {
            const int64_t t =
                climat::prochain_apres_changement_de_mois_s(maintenant_s(), v.dernier_essai_s, v.prochain_s);
            if (t < v.prochain_s) {
                ESP_LOGI(TAG, "mois changé (%d -> %d) : nouvel essai dans %lld s", v.mois_publie, mois,
                         static_cast<long long>(t - maintenant_s() > 0 ? t - maintenant_s() : 0));
                v.prochain_s = t;
            }
        }
    }

    const int64_t reste_s = v.prochain_s - maintenant_s();
    if (reste_s > 0) {
        return dormir_ms(reste_s < kPasSondageS ? reste_s : kPasSondageS);
    }

    // ---- un essai ----
    v.dernier_essai_s = maintenant_s();
    ESP_LOGI(TAG, "requête Vigie (normales ; lat %ld e5, lon %ld e5 ; %d échec(s) de suite jusqu'ici)",
             static_cast<long>(r.latitude_e5), static_cast<long>(r.longitude_e5), v.echecs);
    Lecture lecture;
    const Essai e = interroger(r, lecture);
    tache_donnees_journaliser_pile(TAG);

    bool ecartee = false;
    if (e.issue == Issue::Ok || e.issue == Issue::HorsCouverture) {
        if (!lire_reglage().meme_requete(r)) {
            ESP_LOGI(TAG, "le lieu ou l'adresse a changé pendant la requête : réponse écartée");
            ecartee = true;
        } else {
            if (e.issue == Issue::Ok) {
                climat_etat_publier(lecture.sortie, static_cast<int64_t>(std::time(nullptr)));
                v.publiee = true;
                v.couverte = true;
                v.mois_publie = lecture.sortie.mois;
            } else {
                oublier(
                    v); // aucune fiche à moins de 50 km : pas de pastille, et rien à redemander avant un changement de lieu
                v.publiee = true;
            }
            v.echecs = 0;
            v.trop_de_requetes = false;
            v.pas_de_route = false;
            ESP_LOGI("CLIMAT",
                     "CLIMAT %s : mois %d, normale du max %d (dixièmes), record chaud %d, record froid %d, station %s, "
                     "édition « %s » ; "
                     "%u octets sur le fil%s, %u octets de JSON ; réseau %d ms",
                     e.issue == Issue::Ok ? "couvert" : "hors couverture", lecture.sortie.mois,
                     lecture.sortie.tmax_normale_dix.value_or(-9999), lecture.sortie.tmax_record_dix.value_or(-9999),
                     lecture.sortie.tmin_record_dix.value_or(-9999),
                     lecture.sortie.station_active ? "active" : "arrêtée", lecture.sortie.edition,
                     static_cast<unsigned>(e.octets_fil), e.gzip ? " (gzip)" : "",
                     static_cast<unsigned>(e.octets_texte), e.duree_ms);
        }
    } else {
        ++v.echecs;
        v.trop_de_requetes = (e.issue == Issue::TropDeRequetes);
        v.pas_de_route = (e.issue == Issue::PasDeRoute);
        ESP_LOGW(
            TAG,
            "essai raté : %s (HTTP %d) ; %d échec(s) de suite ; la réponse précédente, s'il y en a une, reste en place",
            nom_issue(e.issue), e.statut_http, v.echecs);
    }
    if (!ecartee) {
        // Une instance sans la route (404) n'est pas redemandée avant demain : inutile de la harceler toutes les heures.
        const int echecs = v.pas_de_route ? 0 : v.echecs;
        const int64_t delai_s = climat::delai_avant_essai_s(echecs, v.trop_de_requetes, alea());
        v.prochain_s = maintenant_s() + delai_s;
        ESP_LOGI(TAG, "prochain essai dans %lld s (plus tôt si le lieu ou l'adresse change)",
                 static_cast<long long>(delai_s));
    }
    return 0;
}

} // namespace

void climat_client_a_change() { tache_donnees_reveiller(ordonnanceur::Source::Climat); }

void climat_client_demarrer() {
    g_rythme = creer_etat_de_pas<Rythme>();
    if (g_rythme == nullptr) {
        ESP_LOGE(TAG, "pas de place pour l'état du client (%u octets) : pas de normales",
                 static_cast<unsigned>(sizeof(Rythme)));
        return;
    }
    tache_donnees_inscrire(ordonnanceur::Source::Climat, pas_climat);
    tache_donnees_reveiller(ordonnanceur::Source::Climat);
}

} // namespace ecran
