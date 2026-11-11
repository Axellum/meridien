// Lecture de la liste des spots de Vigie côté appareil (contrat main/mer_liste_client.h).
//
// Ce fichier ne décide de rien : la requête, la lecture de la réponse, la recherche et la règle « faut-il relire ? » sont dans le noyau pur
// ecran/mer_liste.hpp, testé sur le PC. Ici : la tâche, l'aller-retour HTTPS (client commun) et le verrou.
#include "mer_liste_client.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ecran/mer_liste.hpp"
#include "ecran/reglages_alertes.hpp"
#include "http_commun.h"
#include "reglages_alertes.h"
#include "reseau.h"
#include "tache_donnees.h"

namespace ecran {

namespace {

const char *TAG = "mer_liste";

// Un nom fixe, sans version ni identifiant (rien de variable envoyé), comme le client de la mer (mer_client.cpp).
constexpr const char *kAgentUtilisateur = "Meridien/ESP32-P4 (mer)";
constexpr const char *kAccept = "application/json";
// 51 spots = 20 405 octets bruts (08/11/2026) ; le noyau en garde 64 au plus. 96 Ko laissent près de cinq fois la taille d'aujourd'hui : au-delà,
// la réponse est REFUSÉE (`Issue::TropGrand`), jamais lue à moitié. Deux tampons de cette taille en PSRAM le temps de la requête.
constexpr size_t kTailleMax = 96 * 1024;
constexpr uint32_t kAttenteVerrouTlsMs = 40000; // une recherche de ville tient le verrou 8 s au plus (geocodage.cpp)
constexpr int kDelaiHttpMs = 15000;             // connexion, TLS et lecture : au-delà, l'essai est un échec
// La lecture passe par LA tâche réseau des données (tache_donnees.h : pile de 12 Ko, priorité d'arrière-plan), en priorité sur la
// météo, les alertes et la mer : une personne attend la liste devant l'écran.

struct Contexte {
    SemaphoreHandle_t verrou = nullptr; // protège tout sauf `travail`
    ListeMer etat = ListeMer::Inactive;
    bool tache_active = false;
    bool liste_gardee = false; // `liste` vient d'une lecture réussie
    int64_t fin_liste_us = 0;  // quand elle a été lue
    int64_t fin_essai_us = 0;  // quand le dernier essai s'est fini (0 : aucun)
    mer::liste::Liste liste;   // ce que l'écran copie
    mer::liste::Liste travail; // écrit par la tâche seule, recopié dans `liste` à la fin d'un essai réussi
};

Contexte *creer() {
    void *memoire = heap_caps_calloc(1, sizeof(Contexte), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (memoire == nullptr) {
        ESP_LOGE(TAG, "mémoire PSRAM non obtenue (%u octets) : pas de liste de spots",
                 static_cast<unsigned>(sizeof(Contexte)));
        return nullptr;
    }
    Contexte *c = new (memoire) Contexte();
    c->verrou = xSemaphoreCreateMutex();
    if (c->verrou == nullptr) {
        ESP_LOGE(TAG, "sémaphore non créé : pas de liste de spots");
        return nullptr; // le contexte reste alloué : il ne sera jamais utilisé, et l'erreur est rare
    }
    return c;
}

// Créé au premier appel (l'initialisation d'une variable locale statique est protégée contre deux fils).
Contexte *contexte() {
    static Contexte *c = creer();
    return c;
}

// ---- l'aller-retour -------------------------------------------------------------------------------------------------------

struct Lecture {
    mer::liste::Liste *travail = nullptr;
    mer::liste::Issue issue = mer::liste::Issue::Illisible;
};

// Appelée une seule fois, pour une réponse 200 complète, hors du verrou TLS. Une liste sans aucun spot utilisable est une réponse valable
// (l'écran dit « Vigie ne propose aucun spot ») ; seule une réponse qui n'est pas une liste est refusée.
bool lire_corps(std::string_view corps, void *utilisateur) {
    auto *l = static_cast<Lecture *>(utilisateur);
    l->issue = mer::liste::analyser(corps, *l->travail);
    if (l->issue == mer::liste::Issue::Illisible) {
        ESP_LOGW(TAG, "réponse refusée (ce n'est pas un objet avec une liste « spots ») : %u octets",
                 static_cast<unsigned>(corps.size()));
    }
    return l->issue != mer::liste::Issue::Illisible;
}

void journaliser_memoire(const char *quand, const client_http::Memoire &m) {
    ESP_LOGI(TAG,
             "mémoire libre %s : interne %u o (plus grand bloc %u o, minimum depuis le démarrage %u o), PSRAM %u o",
             quand, static_cast<unsigned>(m.interne), static_cast<unsigned>(m.interne_bloc),
             static_cast<unsigned>(m.interne_min), static_cast<unsigned>(m.psram));
}

// Un aller-retour complet. `c->travail` reçoit la liste ; le résultat dit ce que l'écran doit montrer.
ListeMer lire(Contexte *c) {
    if (!reseau_etat().ip_obtenue) {
        return ListeMer::SansConnexion;
    }
    char adresse[vigie::ADRESSE_MAX + 1] = {};
    if (reglages::adresse_utilisee(reglages_alertes(), adresse, sizeof adresse) == 0) {
        return ListeMer::Echec; // adresse inutilisable : ne devrait pas arriver
    }
    char url[mer::liste::TAILLE_REQUETE_LISTE_MAX] = {};
    if (mer::liste::construire_requete_liste(adresse, url, sizeof url) == 0) {
        return ListeMer::Echec;
    }

    const client_http::Memoire avant = client_http::mesurer_memoire();
    journaliser_memoire("avant la requête", avant);
    Lecture lecture;
    lecture.travail = &c->travail;
    client_http::Requete requete;
    requete.adresse = url;
    requete.user_agent = kAgentUtilisateur;
    requete.accept = kAccept;
    requete.delai_ms = kDelaiHttpMs;
    requete.attente_verrou_tls_ms = kAttenteVerrouTlsMs;
    requete.taille_max = kTailleMax;
    const client_http::Reponse rep = client_http::obtenir(requete, lire_corps, &lecture);
    if (rep.connecte) {
        journaliser_memoire("une fois la connexion TLS établie", rep.apres_connexion);
    }
    journaliser_memoire("après la requête", client_http::mesurer_memoire());
    // Jamais l'adresse au journal : seulement les tailles, le statut et la durée.
    ESP_LOGI(TAG, "requête finie en %d ms : issue %d, HTTP %d, %u octets sur le fil%s, %u octets lus", rep.duree_ms,
             static_cast<int>(rep.issue), rep.statut_http, static_cast<unsigned>(rep.octets_fil),
             rep.gzip ? " (gzip)" : "", static_cast<unsigned>(rep.octets));

    switch (rep.issue) {
    case client_http::Issue::Ok:
        ESP_LOGI(TAG, "%u spot(s) lu(s)%s", static_cast<unsigned>(c->travail.nombre),
                 c->travail.tronquee ? " (liste tronquée)" : "");
        return lecture.issue == mer::liste::Issue::Ok ? ListeMer::Terminee : ListeMer::Vide;
    case client_http::Issue::Reseau:
        return ListeMer::SansConnexion; // DNS, connexion, TLS, délai ou verrou TLS occupé trop longtemps
    default:
        return ListeMer::
            Echec; // refus du serveur, trop de requêtes, réponse trop grande ou illisible, pas de place en PSRAM
    }
}

// Un pas de la tâche réseau des données : une lecture, puis plus rien jusqu'à la prochaine demande de l'écran.
int64_t pas_liste(bool) {
    Contexte *c = contexte();
    if (c == nullptr) {
        return ordonnanceur::kPlusRien;
    }
    const ListeMer fin = lire(c);
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    if (fin == ListeMer::Terminee) {
        c->liste = c->travail; // une seule copie, sous le verrou : l'écran ne voit jamais une liste à moitié écrite
        c->liste_gardee = true;
        c->fin_liste_us = esp_timer_get_time();
    }
    c->fin_essai_us = esp_timer_get_time();
    c->etat = fin;
    c->tache_active = false;
    xSemaphoreGive(c->verrou);
    return ordonnanceur::kPlusRien;
}

} // namespace

void mer_liste_lecture_lancer() {
    Contexte *c = contexte();
    if (c == nullptr) {
        return;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    if (c->etat == ListeMer::EnCours) {
        xSemaphoreGive(c->verrou);
        return;
    }
    const int64_t maintenant = esp_timer_get_time();
    const int64_t age_liste_s = c->liste_gardee ? (maintenant - c->fin_liste_us) / 1000000 : -1;
    const int64_t age_essai_s = c->fin_essai_us != 0 ? (maintenant - c->fin_essai_us) / 1000000 : -1;
    const bool dernier_echec =
        c->etat == ListeMer::Vide || c->etat == ListeMer::SansConnexion || c->etat == ListeMer::Echec;
    if (!mer::liste::relire_la_liste(c->liste_gardee, age_liste_s, dernier_echec, age_essai_s)) {
        // Rien à relire : la liste gardée est encore bonne (on la remontre), ou on est dans la pause qui suit un échec.
        if (c->liste_gardee && age_liste_s >= 0 && age_liste_s < mer::liste::LISTE_VALABLE_S) {
            c->etat = ListeMer::Terminee;
        }
        xSemaphoreGive(c->verrou);
        return;
    }
    c->etat = ListeMer::EnCours;
    c->tache_active = true;
    tache_donnees_inscrire(ordonnanceur::Source::ListeSpots, pas_liste);
    if (!tache_donnees_reveiller(ordonnanceur::Source::ListeSpots)) {
        ESP_LOGE(TAG, "tâche réseau des données absente : pas de liste de spots");
        c->etat = ListeMer::Echec;
        c->tache_active = false;
        c->fin_essai_us = maintenant;
    }
    xSemaphoreGive(c->verrou);
}

ListeMer mer_liste_lecture_etat() {
    Contexte *c = contexte();
    if (c == nullptr) {
        return ListeMer::Echec;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const ListeMer etat = c->etat;
    xSemaphoreGive(c->verrou);
    return etat;
}

bool mer_liste_lecture_copier(mer::liste::Liste &sortie) {
    Contexte *c = contexte();
    if (c == nullptr) {
        return false;
    }
    xSemaphoreTake(c->verrou, portMAX_DELAY);
    const bool prete = c->etat == ListeMer::Terminee && c->liste_gardee;
    if (prete) {
        sortie = c->liste;
    }
    xSemaphoreGive(c->verrou);
    return prete;
}

} // namespace ecran
