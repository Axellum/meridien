// La famille « Profil » de la roue d'appui long (ADR-0023, complément « le profil épinglé ») : un second anneau qui offre « Auto » et les
// profils que l'appareil sait dessiner (accueil::profil_offert : aujourd'hui « Météo »), le profil en vigueur marqué.
//
// LVGL PUR et sans état : ce fichier ne dépend ni d'ESP-IDF ni de la NVS. L'appareil (main/ecran_principal.cpp) et le banc de rendu PC
// (tools/rendu/scenes/scene_auto.cpp) le compilent tel quel, de sorte que le rendu montre la famille que la carte construit. Ce que
// « choisir » fait (écrire le réglage) est donné par l'appelant : une fonction par profil.
#pragma once

#include <cstddef>

#include "ecran/accueil_epinglage.hpp"
#include "ecran/reglages_valeurs.hpp"
#include "ecran/visuel_textes.hpp"
#include "ui_roue.h"

namespace ecran {

// Ce que fait le toucher d'un profil, dans l'ordre de reglages::ProfilAccueil (Auto, Météo...). Une case nulle : le profil n'est pas
// proposé, quoi qu'en dise accueil::profil_offert.
struct ActionsProfil {
    void (*choisir[reglages::NOMBRE_PROFILS_ACCUEIL])() = {};
};

// Le mot du profil : la clé française, traduite par visuel::texte_c (table de traduction de l'écran).
inline const char *mot_du_profil(reglages::ProfilAccueil p) noexcept {
    switch (p) {
    case reglages::ProfilAccueil::Meteo:
        return visuel::texte_c("Météo");
    case reglages::ProfilAccueil::Auto:
        break;
    }
    return visuel::texte_c("Auto");
}

// Le glyphe du profil (ui_roue_icones.inc) : jamais nul, une icône n'est jamais un rectangle vide.
inline const char *icone_du_profil(reglages::ProfilAccueil p) noexcept {
    switch (p) {
    case reglages::ProfilAccueil::Meteo:
        return ICONE_PROFIL_METEO;
    case reglages::ProfilAccueil::Auto:
        break;
    }
    return ICONE_PROFIL_AUTO;
}

// Remplit `sortie` (au moins reglages::NOMBRE_PROFILS_ACCUEIL cases) avec les choix de la famille : « Auto », puis chaque profil épinglable
// offert. `actuel` est marqué (`actif`). Rend le nombre de choix ; `sortie` doit vivre jusqu'à ui_roue_ouvrir (qui la copie).
inline int composer_choix_profil(reglages::ProfilAccueil actuel, const ActionsProfil &actions,
                                 ChoixRoue *sortie) noexcept {
    int n = 0;
    for (std::size_t i = 0; i < reglages::NOMBRE_PROFILS_ACCUEIL; ++i) {
        const auto p = static_cast<reglages::ProfilAccueil>(i);
        if (actions.choisir[i] == nullptr || !accueil::profil_offert(p)) {
            continue;
        }
        sortie[n++] = {mot_du_profil(p), icone_du_profil(p), actions.choisir[i], p == actuel};
    }
    return n;
}

// L'entrée du premier anneau : la famille « Profil ». À ne poser que si `n` >= 2 (un second anneau d'un seul choix n'offre rien) :
// `ajouter_famille_profil` le fait.
inline EntreeRoue entree_profil(const ChoixRoue *choix, int n) noexcept {
    return {visuel::texte_c("Profil"), ICONE_PROFIL, nullptr, choix, n};
}

// Pose « Profil ▸ » à la suite de `sortie[n]` quand au moins deux choix existent (Auto et un profil épinglable) et qu'il reste une place au
// premier anneau. Rend le nouveau nombre d'entrées.
inline int ajouter_famille_profil(EntreeRoue *sortie, int n, reglages::ProfilAccueil actuel,
                                  const ActionsProfil &actions, ChoixRoue *choix) noexcept {
    const int nb = composer_choix_profil(actuel, actions, choix);
    if (nb >= 2 && n < ENTREES_ROUE_MAX) {
        sortie[n++] = entree_profil(choix, nb);
    }
    return n;
}

} // namespace ecran
