// Noyau pur : QUELLES entrées le premier anneau de la roue d'appui long montre, et dans quel ordre (ADR-0024). Entre la décision de
// l'appareil (main/ecran_principal.cpp : une page des alertes existe-t-elle, un spot de mer est-il choisi) et le dessin de la roue
// (main/ui_roue.cpp), il reste une règle qui se teste en nombres et que l'appareil et le banc de rendu du PC partagent : sans elle, la
// table de l'appareil et celle de la scène « roue » se recopient et finissent par se contredire.
//
//   - « Réglages » est TOUJOURS le premier bouton et « Détails » le second : ils sont atteignables quoi qu'il arrive, et ne bougent
//     pas quand une entrée conditionnelle apparaît ou disparaît (ADR-0021, point 4) ;
//   - « Alertes » vient ensuite quand la page des alertes existe ;
//   - « Mer » vient ensuite quand un spot de mer est choisi : aucune autre entrée ne change de rang à son arrivée (ADR-0024) ;
//   - « Profil » (la famille Auto / profil épinglé, ADR-0023) vient en DERNIER quand l'appareil offre au moins deux choix : il ne
//     déplace aucune autre entrée, et le rang de « Mer » ne change pas à son arrivée.
// Aucun texte, aucune icône, aucune action : des identifiants. Ce que chacun montre et fait est dans main/ecran_principal.cpp.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation, aucune allocation.
#pragma once

#include <cstdint>

#include "ecran/roue_geometrie.hpp"

namespace ecran::roue {

enum class Entree : std::uint8_t { Reglages, Details, Alertes, Mer, Profil };

// Ce que l'appareil sait au moment d'ouvrir la roue.
struct Presences {
    bool alertes = false; // la page des alertes existe (alertes activées, heure sûre, une réponse de Vigie en mémoire)
    bool mer = false;     // un spot de mer est choisi dans le registre (ADR-0019)
    bool profil = false;  // la famille « Profil » a de quoi choisir (profil_a_proposer)
};

struct Liste {
    Entree entree[BOUTONS_MAX] = {};
    int n = 0;
};

// Le plus grand nombre d'entrées possible doit tenir au premier anneau de la géométrie.
inline constexpr int ENTREES_MAX = 5;
static_assert(ENTREES_MAX <= BOUTONS_MAX, "le premier anneau de la roue ne peut pas porter toutes les entrées");

// Le second anneau de « Profil » offre « Auto » et les profils que l'appareil sait dessiner ; avec un seul choix il n'offrirait rien :
// la famille n'est proposée qu'à partir de deux.
constexpr bool profil_a_proposer(int nombre_de_choix) noexcept { return nombre_de_choix >= 2; }

constexpr Liste entrees_du_premier_anneau(const Presences &p) noexcept {
    Liste liste;
    liste.entree[liste.n++] = Entree::Reglages;
    liste.entree[liste.n++] = Entree::Details;
    if (p.alertes) {
        liste.entree[liste.n++] = Entree::Alertes;
    }
    if (p.mer) {
        liste.entree[liste.n++] = Entree::Mer;
    }
    if (p.profil) {
        liste.entree[liste.n++] = Entree::Profil;
    }
    return liste;
}

} // namespace ecran::roue
