// Noyau pur : du réglage « Profil » (reglages::ProfilAccueil : Auto ou un profil épinglé, ADR-0023) à ce que le pilote du mode
// automatique attend (Mesures::epingle et Mesures::profil_epingle, accueil_pilote.hpp). Aucun include ESP-IDF ni LVGL.
//
// Trois règles, testées sur le PC (test/host/test_accueil_epinglage.cpp) :
//   1. « Auto » n'épingle rien : les règles de l'ADR-0015 décident.
//   2. Un choix hors table (une NVS abîmée qui aurait laissé passer un octet) n'épingle rien non plus : jamais un profil épinglé par
//      accident.
//   3. Le réglage n'offre que les profils que l'appareil sait dessiner (PROFILS_DISPONIBLES du pilote) : `profil_offert` le dit, c'est
//      ce que la roue d'appui long et la liste des réglages proposent.
#pragma once

#include <cstdint>

#include "ecran/accueil_pilote.hpp"
#include "ecran/reglages_valeurs.hpp"

namespace ecran::accueil {

// Ce que le pilote reçoit : `epingle` faux = pas d'épinglage (`profil` ne compte alors pas).
struct Epinglage {
    bool epingle = false;
    Profil profil = Profil::Meteo;
};

constexpr bool operator==(const Epinglage &a, const Epinglage &b) noexcept {
    return a.epingle == b.epingle && (!a.epingle || a.profil == b.profil);
}

constexpr Epinglage epinglage_de(reglages::ProfilAccueil choix) noexcept {
    switch (choix) {
    case reglages::ProfilAccueil::Meteo:
        return {true, Profil::Meteo};
    case reglages::ProfilAccueil::Auto:
        break;
    }
    return {};
}

// Le choix est-il proposé à la personne ? « Auto » toujours ; un profil épinglé seulement si l'appareil le dessine.
constexpr bool profil_offert(reglages::ProfilAccueil choix) noexcept {
    const Epinglage e = epinglage_de(choix);
    if (choix == reglages::ProfilAccueil::Auto)
        return true;
    return e.epingle && profil_disponible(e.profil);
}

} // namespace ecran::accueil
