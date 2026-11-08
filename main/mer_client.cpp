#include "mer_client.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string_view>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ecran/mer_requete.hpp"
#include "ecran/mer_spots.hpp"
#include "ecran/mer_trame.hpp"
#include "ecran/reglages_alertes.hpp"
#include "http_commun.h"
#include "mer_etat.h"
#include "reglages_alertes.h"
#include "reglages_mer.h"
#include "reseau.h"

// Client de la mer d'un spot. Même schéma que le client des alertes (main/vigie_client.cpp) mais l'aller-retour HTTPS passe par le client
// commun (main/http_commun.h : tampons en PSRAM, gzip, verrou TLS, taille maximale d'une réponse). Priorité d'arrière-plan, délai de 15 s.

namespace ecran {

namespace {

const char *TAG = "mer";
// Ligne de résultat que le banc de test cherche : « MER … » à chaque trame lue.
const char *TAG_RESULTAT = "MER";

// User-Agent : un nom fixe, sans version ni identifiant (rien de variable envoyé), comme les autres clients de Meridien.
constexpr const char *kAgentUtilisateur = "Meridien/ESP32-P4 (mer)";
constexpr const char *kAccept = "text/plain";
// La trame ne dépasse pas 8 Kio pour les zones surveillées par défaut (vigie_api/docs/embedded-frame.md) ; 4 992 à 5 022 octets sur les
// six trames de test/host/donnees/. 16 Kio laissent le double de marge si l'instance surveille plus de zones : au-delà, la réponse est
// REFUSÉE (`Issue::TropGrand`), jamais lue à moitié. Deux tampons de cette taille en PSRAM le temps de la requête.
constexpr size_t kTailleMax = 16 * 1024;
constexpr uint32_t kAttenteVerrouTlsMs = 40000; // une recherche de ville tient le verrou 8 s au plus (geocodage.cpp)
constexpr int kDelaiHttpMs = 15000;             // connexion, TLS et lecture : au-delà, l'essai est un échec
constexpr uint32_t kPasAttenteMs = 2000;        // contrôle de l'IP et de l'heure avant le premier essai
constexpr int64_t kPasSondageS = 5;             // les réglages sont relus au moins toutes les 5 s pendant l'attente
constexpr int64_t kInstantMin = 946684800;      // 1er janvier 2000 : en dessous, l'horloge n'est pas réglée
constexpr uint32_t kPileOctets = 12288;         // la prise de contact TLS demande plusieurs Ko de pile
// Priorité d'ARRIÈRE-PLAN (voir meteo_client.cpp : la prise de contact TLS calcule plusieurs secondes sans jamais s'endormir ; au-dessus
// de la priorité de la tâche d'inactivité, le chien de garde des tâches se déclenche).
constexpr UBaseType_t kPrioriteMer = tskIDLE_PRIORITY;

TaskHandle_t g_tache = nullptr;
// La tâche (12 Ko de pile en RAM interne) n'existe que si un spot est choisi : un appareil neuf, au registre vide, garde tout son budget
// de mémoire interne (docs/mesures/memoire-interne.md). Une fois créée elle reste, même si le registre se vide (elle dort alors 5 s à la fois).
std::atomic<bool> g_demarree{false};

using client_http::Memoire;
using client_http::mesurer_memoire;

void journaliser_memoire(const char *quand, const Memoire &m) {
    ESP_LOGI(TAG,
             "mémoire libre %s : interne %u o (plus grand bloc %u o, minimum depuis le démarrage %u o), PSRAM %u o "
             "(plus grand bloc %u o)",
             quand, static_cast<unsigned>(m.interne), static_cast<unsigned>(m.interne_bloc),
             static_cast<unsigned>(m.interne_min), static_cast<unsigned>(m.psram), static_cast<unsigned>(m.psram_bloc));
}

// ---- ce qui part dans la requête -------------------------------------------------------------------------------------

// Tout ce qui décide de la requête. Si l'un de ces champs change, l'ancienne mer ne vaut plus pour le nouveau spot, la nouvelle plage de
// vent ou la nouvelle adresse : on l'oublie et on redemande.
struct Reglage {
    mer::DemandeMer demande;                   // vide : aucun spot choisi, rien n'est demandé
    char adresse[vigie::ADRESSE_MAX + 1] = {}; // celle des réglages des alertes (normalisée), ou l'instance publique

    bool meme(const Reglage &autre) const {
        if (!mer::meme_demande(demande, autre.demande))
            return false;
        return !demande.presente || std::strcmp(adresse, autre.adresse) == 0; // sans spot, l'adresse ne sert à rien
    }
};

Reglage lire_reglage() {
    Reglage r;
    r.demande = reglages_mer_demande();
    if (r.demande.presente) {
        const reglages::Alertes a = reglages_alertes();
        if (reglages::adresse_utilisee(a, r.adresse, sizeof r.adresse) == 0) {
            r.demande = mer::DemandeMer{}; // adresse inutilisable : ne devrait pas arriver, mieux vaut ne rien demander
        }
    }
    return r;
}

// ---- une requête ---------------------------------------------------------------------------------------------------

using Issue = client_http::Issue;

struct Essai {
    Issue issue = Issue::Reseau;
    int statut_http = 0;
    size_t octets_fil = 0; // sur le fil (compressés ou non)
    size_t octets = 0;     // le texte lu (décompressé)
    bool gzip = false;
    int duree_ms = 0;
    mer::MiseAJour maj = mer::MiseAJour::Refusee;
};

// Ce que le client commun rend à `lire_corps` : où décoder, et ce que le noyau en a fait.
struct Lecture {
    MerPubliee *travail = nullptr;
    mer::MiseAJour maj = mer::MiseAJour::Refusee;
};

// Appelée une seule fois, pour une réponse 200 complète, hors du verrou TLS. Le texte n'existe que pendant l'appel.
bool lire_corps(std::string_view corps, void *utilisateur) {
    auto *l = static_cast<Lecture *>(utilisateur);
    l->maj = mer::mettre_a_jour(corps, l->travail->mer);
    if (l->maj == mer::MiseAJour::Refusee) {
        ESP_LOGW(TAG, "trame refusée (CRC faux, ligne END absente ou ligne SEA illisible) : %u octets",
                 static_cast<unsigned>(corps.size()));
    }
    return l->maj != mer::MiseAJour::Refusee;
}

// Un aller-retour complet. La mer lue est écrite dans le tampon de travail de mer_etat.h (seulement si la trame est celle d'un bloc
// mer) : la publier est l'affaire de l'appelant.
Essai interroger(const Reglage &r, MerPubliee &travail) {
    Essai e;
    char url[mer::TAILLE_REQUETE_MAX];
    if (mer::requete_de_la_demande(r.adresse, r.demande, url, sizeof(url)) == 0) {
        e.issue = Issue::Requete;
        return e;
    }

    const Memoire avant = mesurer_memoire();
    journaliser_memoire("avant la requête", avant);

    Lecture lecture;
    lecture.travail = &travail;
    client_http::Requete requete;
    requete.adresse = url;
    requete.user_agent = kAgentUtilisateur;
    requete.accept = kAccept;
    requete.delai_ms = kDelaiHttpMs;
    requete.attente_verrou_tls_ms = kAttenteVerrouTlsMs;
    requete.taille_max = kTailleMax;
    const client_http::Reponse rep = client_http::obtenir(requete, lire_corps, &lecture);
    e.issue = rep.issue;
    e.statut_http = rep.statut_http;
    e.octets_fil = rep.octets_fil;
    e.octets = rep.octets;
    e.gzip = rep.gzip;
    e.duree_ms = rep.duree_ms;
    e.maj = lecture.maj;

    if (rep.connecte) {
        journaliser_memoire("une fois la connexion TLS établie", rep.apres_connexion);
    }
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

const char *nom_verdict(mer::Verdict v) {
    switch (v) {
    case mer::Verdict::Bon:
        return "bon";
    case mer::Verdict::Possible:
        return "possible";
    case mer::Verdict::Deconseille:
        return "déconseillé";
    case mer::Verdict::Inconnu:
        break;
    }
    return "inconnu";
}

const char *nom_activite(int a) {
    switch (static_cast<mer::Activite>(a)) {
    case mer::Activite::Surf:
        return "surf";
    case mer::Activite::Kite:
        return "kite";
    case mer::Activite::Bateau:
        return "bateau";
    case mer::Activite::PecheAPied:
        return "pêche à pied";
    case mer::Activite::Nombre:
        break;
    }
    return "?";
}

// La mer lue, au journal : de quoi vérifier sur la carte ce que la page montrera (aucun secret : des textes publics).
void journaliser_mer(const MerPubliee &p, const Essai &e) {
    const mer::Mer &m = p.mer;
    ESP_LOGI(TAG_RESULTAT,
             "MER %s : « %s » (%s) ; générée UNIX %lld ; %u marée(s), %u coefficient(s) ; vigilances côtières %s%s ; "
             "%u octets sur le fil%s, %u octets de trame ; réseau %d ms",
             p.spot, m.nom, m.lieu, static_cast<long long>(m.genere_s), static_cast<unsigned>(m.nb_marees),
             static_cast<unsigned>(m.nb_coefficients),
             m.vigilances_lues ? "lues" : "ILLISIBLES (pas « rien à craindre »)",
             m.vigilance_presente ? ", une est en cours" : "", static_cast<unsigned>(e.octets_fil),
             e.gzip ? " (gzip)" : "", static_cast<unsigned>(e.octets), e.duree_ms);
    for (int a = 0; a < mer::NB_ACTIVITES; ++a) {
        const mer::Prevision &pr = m.activites[a];
        if (!pr.presente) {
            ESP_LOGI(TAG_RESULTAT, "  %s : pas de ligne dans la trame", nom_activite(a));
            continue;
        }
        ESP_LOGI(TAG_RESULTAT, "  %s : %s%s, créneau UNIX %lld à %lld", nom_activite(a), nom_verdict(pr.verdict),
                 pr.disponible ? "" : " (non jugeable ici)", static_cast<long long>(pr.creneau_debut_s),
                 static_cast<long long>(pr.creneau_fin_s));
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

// Dort au plus `secondes` secondes ; un réveil anticipé (mer_client_a_change) rend la main tout de suite.
void dormir(int64_t secondes) {
    if (secondes < 1) {
        secondes = 1;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(static_cast<uint32_t>(secondes) * 1000U));
}

// `au_demarrage` (non nul) : créée par mer_client_demarrer() avant que le réseau soit là ; nul : créée plus tard parce qu'un spot vient
// d'être choisi, le réseau est déjà là et la personne attend sa mer.
void tache_mer(void *au_demarrage) {
    int echecs = 0;                // essais ratés de suite depuis la dernière trame lue
    bool trop_de_requetes = false; // le dernier échec était un 429 ou un 503
    Reglage derniere;              // la requête à laquelle correspondent la mer publiée et la date du prochain essai
    bool a_une_derniere = false;
    bool publiee = false;               // une mer de CETTE requête est en mémoire
    int64_t dernier_essai_s = -1000000; // début du dernier essai (secondes depuis le démarrage)
    int64_t prochain_s = 0;
    bool premier = au_demarrage != nullptr;
    bool depart_tardif = !premier;
    for (;;) {
        while (!reseau_et_heure_pretes()) {
            dormir(kPasAttenteMs / 1000);
        }
        if (premier) {
            // Pas de rafale au démarrage : le Wi-Fi, l'heure, la météo et les alertes passent d'abord (120 à 180 s d'attente).
            premier = false;
            prochain_s = maintenant_s() + mer::delai_premier_essai_s(alea());
        } else if (depart_tardif) {
            // Tâche née du choix d'un spot : même délai que pour un changement de spot (quelques secondes), pas les deux minutes du démarrage.
            depart_tardif = false;
            prochain_s = mer::prochain_essai_apres_changement_s(dernier_essai_s, maintenant_s());
        }

        // Les réglages sont relus à chaque tour : un changement de spot, de plage de vent ou d'adresse se voit dans les 5 secondes, sans que
        // les modules qui les écrivent aient à connaître ce client (mer_client_a_change() l'accélère).
        const Reglage r = lire_reglage();
        if (!r.demande.presente) {
            if (publiee) {
                ESP_LOGI(TAG, "plus de spot choisi : plus de requête, plus de mer");
                mer_etat_oublier();
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
            ESP_LOGI(TAG, "premier essai de la mer dans %lld s (spot %s)",
                     static_cast<long long>(prochain_s - maintenant_s()), r.demande.spot);
        } else if (!r.meme(derniere)) {
            // Spot, plage de vent ou adresse changés : l'ancienne mer ne vaut plus. Une seule requête, après un court délai (plusieurs
            // changements de suite = une seule requête) et une minute au moins après la précédente.
            if (publiee) {
                mer_etat_oublier();
                publiee = false;
            }
            echecs = 0;
            trop_de_requetes = false;
            prochain_s = mer::prochain_essai_apres_changement_s(dernier_essai_s, maintenant_s());
            derniere = r;
            ESP_LOGI(TAG, "requête changée (spot %s, vent, adresse ou nouveau choix) : nouvel essai dans %lld s",
                     r.demande.spot, static_cast<long long>(prochain_s - maintenant_s()));
        }

        const int64_t reste_s = prochain_s - maintenant_s();
        if (reste_s > 0) {
            dormir(reste_s < kPasSondageS ? reste_s : kPasSondageS);
            continue;
        }

        // ---- un essai ----
        dernier_essai_s = maintenant_s();
        ESP_LOGI(TAG, "requête de la mer, spot %s (vent du kite %d à %d nœuds ; %d échec(s) de suite jusqu'ici)",
                 r.demande.spot, r.demande.vent_min_kn, r.demande.vent_max_kn, echecs);
        MerPubliee *travail = mer_etat_travail();
        const Essai e = travail != nullptr ? interroger(r, *travail) : Essai{Issue::Memoire};
        ESP_LOGI(TAG, "pile de la tâche : %u octets jamais utilisés sur %u",
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), static_cast<unsigned>(kPileOctets));

        bool ecartee = false;
        if (e.issue == Issue::Ok) {
            if (!lire_reglage().meme(r)) {
                // Le spot, la plage de vent ou l'adresse a changé pendant la requête : la mer est celle de l'ancien spot. Écartée ; le
                // tour suivant voit le changement et planifie la bonne requête.
                ESP_LOGI(TAG, "le spot, le vent ou l'adresse a changé pendant la requête : mer écartée");
                ecartee = true;
            } else {
                echecs = 0;
                trop_de_requetes = false;
                if (e.maj == mer::MiseAJour::Remplacee) {
                    std::memcpy(travail->spot, r.demande.spot, sizeof travail->spot);
                    journaliser_mer(*travail, e); // avant la publication : elle échange les deux tampons
                    mer_etat_publier(static_cast<int64_t>(std::time(nullptr)));
                    publiee = true;
                } else {
                    // Trame bonne sans bloc mer (la houle n'a pas pu être lue côté Vigie) : on ne change rien, la mer publiée s'il y en a
                    // une vieillit (mer_vue.hpp) ; la prochaine requête est à la période habituelle.
                    ESP_LOGW(TAG, "trame de Vigie sans bloc mer pour le spot %s (%u octets) : rien n'est publié",
                             r.demande.spot, static_cast<unsigned>(e.octets));
                }
            }
        } else {
            ++echecs;
            trop_de_requetes = (e.issue == Issue::TropDeRequetes);
            ESP_LOGW(
                TAG,
                "essai raté : %s (HTTP %d) ; %d échec(s) de suite ; la mer précédente, s'il y en a une, reste en place",
                nom_issue(e.issue), e.statut_http, echecs);
            if (e.statut_http == 404 || e.statut_http == 422) {
                ESP_LOGW(TAG, "Vigie ne connaît pas le spot %s ou refuse la plage de vent : le registre est à corriger",
                         r.demande.spot);
            }
        }
        if (!ecartee) {
            const int64_t delai_s = mer::delai_avant_essai_s(echecs, trop_de_requetes, alea());
            prochain_s = maintenant_s() + delai_s;
            ESP_LOGI(TAG, "prochain essai dans %lld s (plus tôt si le spot, le vent ou l'adresse change)",
                     static_cast<long long>(delai_s));
        }
    }
}

// Crée la tâche, une seule fois quoi qu'il arrive (deux appels en même temps n'en créent qu'une).
void creer_la_tache(bool au_demarrage) {
    bool attendu = false;
    if (!g_demarree.compare_exchange_strong(attendu, true)) {
        return;
    }
    if (xTaskCreate(tache_mer, "mer", kPileOctets, au_demarrage ? &g_demarree : nullptr, kPrioriteMer, &g_tache) !=
        pdPASS) {
        g_demarree = false;
        ESP_LOGE(TAG, "tâche mer non créée");
    }
}

} // namespace

void mer_client_a_change() {
    if (g_tache != nullptr) {
        xTaskNotifyGive(g_tache);
    } else if (reglages_mer_demande().presente) {
        creer_la_tache(false); // le premier spot vient d'être choisi
    }
}

void mer_client_demarrer() {
    reglages_mer_demarrer(); // la NVS est ouverte à ce stade (après reglages_demarrer())
    ESP_LOGI(TAG,
             "tailles : mer lue %u o (2 tampons en PSRAM), tampons de réception %u o + texte %u o (PSRAM, rendus après "
             "chaque essai)",
             static_cast<unsigned>(sizeof(MerPubliee)), static_cast<unsigned>(kTailleMax),
             static_cast<unsigned>(kTailleMax));
    if (reglages_mer_demande().presente) {
        creer_la_tache(true);
    } else {
        ESP_LOGI(TAG, "aucun spot choisi : pas de tâche, pas de requête à Vigie (elle sera créée au premier choix)");
    }
}

} // namespace ecran
