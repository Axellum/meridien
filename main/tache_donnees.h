// LA tâche réseau des données : météo (meteo_client.cpp), alertes de Vigie (vigie_client.cpp), mer du spot (mer_client.cpp) et liste
// des spots (mer_liste_client.cpp). Une seule pile de 12 Ko en RAM interne au lieu de quatre (trois permanentes, une à la demande) :
// les quatre clients ne faisaient déjà jamais deux requêtes à la fois (verrou TLS commun, verrou_tls.h).
//
// Chaque client garde son rythme, sa relance après échec et sa réaction aux réglages : il fournit un « pas » (un essai, ou un simple
// contrôle de ses réglages) qui rend le délai avant son pas suivant. QUI passe et QUAND est décidé par le noyau pur
// (ecran/ordonnanceur_reseau.hpp, testé sur le PC) : la plus en retard d'abord, la liste des spots avant les autres (une personne
// attend devant l'écran). La recherche de ville (geocodage.cpp) garde sa propre tâche de passage : elle ne fait jamais la queue
// derrière ces quatre-là, seulement derrière le verrou TLS, comme avant.
//
// Priorité d'ARRIÈRE-PLAN (tskIDLE_PRIORITY), comme les quatre tâches d'avant : la prise de contact TLS calcule plusieurs secondes sans
// jamais s'endormir ; au-dessus de la priorité de la tâche d'inactivité, le chien de garde des tâches se déclencherait.
#pragma once

#include <cstdint>
#include <new>

#include "esp_heap_caps.h"

#include "ecran/ordonnanceur_reseau.hpp"

namespace ecran {

// Pile de la tâche : celle de chacune des tâches d'avant (le pic est la prise de contact TLS ; ≈ 6 Ko utilisés par la météo,
// mesure du 11/11/2026 sur le Tab5). Pile en RAM interne : la météo écrit la NVS (un `assert` d'ESP-IDF refuse une pile en PSRAM).
constexpr std::uint32_t kPileTacheDonnees = 12288;

// Un pas d'une source. `reveillee` : un réveil (tache_donnees_reveiller) est arrivé depuis son pas précédent. Rend le délai en
// millisecondes avant son pas suivant (0 : dès que possible), ou ordonnanceur::kPlusRien (rien tant qu'on ne la réveille pas).
using PasDonnees = std::int64_t (*)(bool reveillee);

// Donne le pas de `source`. Sans effet sur l'ordonnancement : une source ne passe qu'après un réveil.
void tache_donnees_inscrire(ordonnanceur::Source source, PasDonnees pas);

// Réveille `source` (depuis n'importe quelle tâche, jamais d'une interruption) : elle passe après son anti-rebond. Faux si la tâche
// n'existe pas (création ratée au démarrage) : le réveil est gardé, mais personne ne le traitera.
bool tache_donnees_reveiller(ordonnanceur::Source source);

// Crée la tâche (une seule fois). À appeler avant les `*_client_demarrer`.
void tache_donnees_demarrer();

// L'état qu'un client garde entre deux pas (ce que son ancienne tâche gardait sur sa pile), construit en PSRAM (repli en RAM interne) :
// pas en variable statique, qui irait en RAM interne. nullptr si aucune place.
template <typename T> T *creer_etat_de_pas() {
    void *place = heap_caps_calloc(1, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (place == nullptr) {
        place = heap_caps_calloc(1, sizeof(T), MALLOC_CAP_8BIT);
    }
    return place != nullptr ? new (place) T() : nullptr;
}

// Journalise la marge de pile la plus basse depuis le démarrage de la tâche (à appeler depuis un pas, après une requête).
void tache_donnees_journaliser_pile(const char *tag);

} // namespace ecran
