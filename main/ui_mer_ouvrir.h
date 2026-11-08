// Ouvre la page mer (ui_mer.h) sur la mer courante de l'appareil (mer_etat.h). C'est la colle ESP-IDF de la page : l'écran de la carte,
// l'horloge, le réseau et le registre des spots ; la page elle-même (ui_mer.cpp) n'en sait rien, pour que le rendu PC la compile telle
// quelle. Elle est appelée par l'entrée « Mer » de la roue d'appui long (main/ecran_principal.cpp, ADR-0024), quand un spot est choisi.
#pragma once

#include "ecran/visuel_themes.hpp"

namespace ecran {

// Montre la page mer par-dessus l'écran affiché, dans le thème `theme` (celui de l'écran principal), et revient à cet écran d'un coup à
// « Fermer ». La mer montrée est celle du spot choisi dans le registre ; si la mer en mémoire est celle d'un autre spot, ou s'il n'y en a
// pas, la page s'ouvre quand même et dit « Mer : données indisponibles » (jamais la mer d'un autre lieu). L'heure de l'appareil n'est donnée
// à la page que si elle est vraie (réseau synchronisé ou horloge matérielle plausible) : sans elle, ni âge ni prochaine marée.
// Rend faux, sans rien afficher, si la page est déjà ouverte, si l'écran de la carte est inconnu, si le fuseau du lieu n'a pas pu être lu
// (jamais l'heure d'un autre fuseau) ou si une police n'a pas pu être créée.
bool ui_mer_ouvrir(const visuel::ThemePrincipal &theme);

// La page mer est-elle affichée ?
bool ui_mer_ouverte();

} // namespace ecran
