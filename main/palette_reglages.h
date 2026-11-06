// Palette par rôles des écrans de réglages et de l'installation guidée (AGENTS.md, règle 5 : aucune couleur écrite en dur dans
// l'interface). Les valeurs n'existent qu'ici ; le code de l'interface ne cite que des rôles. Volontairement indépendante de
// palette.h (qui appartient à l'écran principal) : cette palette porte les rôles dont les réglages ont besoin en plus (champ de
// saisie, touche du clavier, action destructive).
//
// Le thème « Auto » est provisoirement sombre : la bascule selon l'heure ou la lumière ambiante n'existe pas encore.
#pragma once

#include <cstdint>

namespace ecran {

struct PaletteReglages {
    uint32_t fond;             // fond de l'écran
    uint32_t surface;          // fond des lignes, des boutons et des cartes
    uint32_t surface_haute;    // fond d'une ligne appuyée, d'un champ de saisie
    uint32_t separateur;       // trait fin entre deux zones
    uint32_t texte;            // texte principal
    uint32_t texte_discret;    // texte secondaire, aide, valeur d'une ligne
    uint32_t accent;           // choix en cours, bouton principal, étape en cours
    uint32_t texte_sur_accent; // texte posé sur l'accent
    uint32_t touche;           // touche de lettre du clavier
    uint32_t touche_commande;  // touche de commande du clavier (majuscule, effacer, valider)
    uint32_t danger;           // action destructive (réinitialiser)
    uint32_t texte_sur_danger;
    uint32_t succes;        // « Connecté »
    uint32_t avertissement; // « Plus tard », échec récupérable
    uint32_t desactive;     // texte d'un bouton inactif
};

inline constexpr PaletteReglages kReglagesSombre = {
    .fond = 0x0F1419,
    .surface = 0x1E2630,
    .surface_haute = 0x2A3441,
    .separateur = 0x34404E,
    .texte = 0xF2F5F7,
    .texte_discret = 0x9AA7B4,
    .accent = 0x3DDC97,
    .texte_sur_accent = 0x0F1419,
    .touche = 0x2A3441,
    .touche_commande = 0x3B4A5C,
    .danger = 0xE5594F,
    .texte_sur_danger = 0xFFFFFF,
    .succes = 0x3DDC97,
    .avertissement = 0xF2B84B,
    .desactive = 0x5E6B78,
};

inline constexpr PaletteReglages kReglagesClair = {
    .fond = 0xF3F5F7,
    .surface = 0xFFFFFF,
    .surface_haute = 0xE3E8ED,
    .separateur = 0xC9D1D9,
    .texte = 0x14202B,
    .texte_discret = 0x586674,
    .accent = 0x0F8F5F,
    .texte_sur_accent = 0xFFFFFF,
    .touche = 0xFFFFFF,
    .touche_commande = 0xD5DCE3,
    .danger = 0xC53A30,
    .texte_sur_danger = 0xFFFFFF,
    .succes = 0x0F8F5F,
    .avertissement = 0xA66A00,
    .desactive = 0x9AA5B0,
};

// Indice du réglage « affichage.theme » (0 auto, 1 clair, 2 sombre) → palette.
constexpr const PaletteReglages &palette_du_theme(int theme) noexcept {
    return theme == 1 ? kReglagesClair : kReglagesSombre;
}

} // namespace ecran
