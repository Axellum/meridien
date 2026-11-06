// Palette par rôles (AGENTS.md, règle 5 : aucune couleur écrite en dur dans l'interface). Le code de l'écran ne cite que des
// rôles ; les valeurs n'existent qu'ici. Palette provisoire de la phase 1a (un thème sombre) : les thèmes du dépôt Tab5
// (Palette, 21 thèmes) arrivent avec le noyau pur en phase 2.
#pragma once

#include <cstdint>

namespace ecran {

struct Palette {
    uint32_t fond;           // fond de l'écran
    uint32_t surface;        // fond des cartes et des boutons
    uint32_t texte;          // texte principal
    uint32_t texte_discret;  // texte secondaire
    uint32_t accent;         // point tactile pendant l'appui, titres
    uint32_t accent_relache; // point tactile après le relâchement
    uint32_t bouton_texte;   // texte sur un bouton
};

inline constexpr Palette kPaletteSombre = {
    .fond = 0x0F1419,
    .surface = 0x1E2630,
    .texte = 0xF2F5F7,
    .texte_discret = 0x9AA7B4,
    .accent = 0x3DDC97,
    .accent_relache = 0x4D6B88,
    .bouton_texte = 0xF2F5F7,
};

} // namespace ecran
