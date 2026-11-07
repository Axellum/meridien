// Noyau pur : les thèmes de l'écran principal et le dégradé du ciel. Une SEULE place pour les couleurs, les dispositions et les
// opacités de l'écran (AGENTS.md, règle 5) ; main/ui_principal.cpp ne cite que des rôles. C++17 standard, aucun include ESP-IDF
// ni LVGL, évaluable à la compilation : test/host/test_visuel_themes.cpp MESURE le contraste de chaque texte et de chaque icône
// contre ce qui est derrière (rapport WCAG), pour chaque ciel possible.
//
// Quatre thèmes, trois dispositions :
//   * CIEL    : fond en dégradé qui suit l'heure du jour et le temps (ciel_dynamique), cartes de verre sombre. Thème par défaut.
//   * TABLEAU : cartes pleines, une couleur par grandeur (vent, pluie...). Deux thèmes : sombre et clair.
//   * HORLOGE : noir pur, l'heure géante, presque rien d'autre.
// Un thème est un tableau de constantes : en ajouter un ne touche pas au dessin. Le choix du thème par l'utilisateur est une
// affaire de la page de réglages (hors de ce lot) ; en attendant, main/ecran_principal.cpp prend kThemeCiel.
#pragma once

#include <cstdint>

#include "ecran/visuel_astres.hpp"
#include "ecran/visuel_echelles.hpp"
#include "ecran/visuel_icones.hpp"

namespace ecran::visuel {

enum class Disposition : std::uint8_t { CIEL, TABLEAU, HORLOGE };

// Les grandeurs de la bande d'information : une couleur chacune (thème TABLEAU surtout, discrète dans les autres).
enum class Metrique : std::uint8_t { VENT, PLUIE, HUMIDITE, PRESSION, UV, SOLEIL, LUNE, NOMBRE };

inline constexpr int NB_ROLES = static_cast<int>(Role::NOMBRE);
inline constexpr int NB_METRIQUES = static_cast<int>(Metrique::NOMBRE);

struct ThemePrincipal {
    const char *nom; // nom technique, jamais affiché
    Disposition disposition;
    bool ciel_dynamique;             // vrai : le fond est le dégradé du ciel (`ciel()`), `fond` ne sert pas
    std::uint32_t fond;              // fond plein de l'écran
    std::uint32_t carte;             // teinte des cartes
    std::uint8_t carte_opa;          // opacité des cartes sur le fond (0 transparent, 255 plein)
    std::uint32_t texte;             // texte principal
    std::uint32_t texte_2;           // texte secondaire
    std::uint32_t texte_3;           // texte discret (libellés, unités)
    std::uint32_t filet;             // traits fins (séparateurs, rails des barres)
    std::uint32_t accent;            // point sur l'arc du soleil, curseurs, anneau tactile
    std::uint32_t avertissement;     // « Données périmées », « Exemple »
    std::uint32_t texte_sur_couleur; // texte posé sur une pastille colorée (alertes)
    std::uint32_t role[NB_ROLES]; // teinte des couches d'icône, indexée par Role (l'indice 0, AUCUN, n'est pas utilisé)
    std::uint32_t metrique[NB_METRIQUES]; // couleur de chaque grandeur
    std::uint32_t niveau_uv[5];           // faible, modéré, élevé, très élevé, extrême
    std::uint32_t alerte[6];              // indexée par Alerte (l'indice 0, AUCUNE, n'est pas utilisé)
    std::uint32_t
        officielle[5];   // alerte officielle de Vigie, indexée par son niveau : inconnu, vert, jaune, orange, rouge
                         // (le vert n'est jamais montré ; l'inconnu est un neutre : on ne devine pas la gravité)
    Rampe rampe;         // température -> couleur
    std::uint32_t pluie; // barres de pluie
};

// ---- les quatre thèmes ----

// Ordre des rôles : AUCUN, soleil, lune, nuage, nuage_sombre, pluie, neige, eclair, brume.
inline constexpr ThemePrincipal kThemeCiel = {
    "ciel",
    Disposition::CIEL,
    true,
    0x14244A,
    0x0A1220,
    0x70,
    0xF6F8FB,
    0xD3DCE6,
    0xB4C2D2,
    0x5E738C,
    0xFFC857,
    0xFFCF66,
    0x10151C,
    {0, 0xFFC83D, 0xEFE9CF, 0xE6EDF5, 0xA9B7CB, 0x62BCFF, 0xFFFFFF, 0xFFD93D, 0xB4C0CE},
    {0x8FD6FF, 0x66B8FF, 0x7FE0A8, 0xC3A6FF, 0xFFD25C, 0xFFB454, 0xE9E4CC},
    {0x7FE0A8, 0xFFD25C, 0xFFA24D, 0xFF6B5E, 0xD79BFF},
    {0, 0x8FD6FF, 0xFF9E73, 0xFFD25C, 0x66B8FF, 0xD79BFF},
    {0xB4C2D2, 0x7FE0A8, 0xFFD25C, 0xFFA24D, 0xFF9A90},
    {{0x8FC4FF, 0x62D8FF, 0x84E8A0, 0xFFCB52, 0xFF7555}},
    0x5CB8FF,
};

inline constexpr ThemePrincipal kThemeTableauSombre = {
    "tableau-sombre",
    Disposition::TABLEAU,
    false,
    0x0C1117,
    0x19232E,
    0xFF,
    0xF4F7FA,
    0xCBD5DF,
    0xA3B1C0,
    0x2C3947,
    0x3DDC97,
    0xFFCF66,
    0x10151C,
    {0, 0xFFC83D, 0xEFE9CF, 0xE3EAF2, 0x8F9DB2, 0x58B4FF, 0xFFFFFF, 0xFFD93D, 0xAAB6C4},
    {0x7FD1FF, 0x58B4FF, 0x6FE3B0, 0xC3A6FF, 0xFFD25C, 0xFFB454, 0xE9E4CC},
    {0x6FE3B0, 0xFFD25C, 0xFFA24D, 0xFF6B5E, 0xD79BFF},
    {0, 0x7FD1FF, 0xFF8A5C, 0xFFD25C, 0x58B4FF, 0xD79BFF},
    {0xA3B1C0, 0x6FE3B0, 0xFFD25C, 0xFFA24D, 0xFF6B5E},
    {{0x7FB2FF, 0x5CD6FF, 0x7BE495, 0xFFC94A, 0xFF6B4A}},
    0x58B4FF,
};

inline constexpr ThemePrincipal kThemeHorloge = {
    "horloge",
    Disposition::HORLOGE,
    false,
    0x000000,
    0x0E1114,
    0xFF,
    0xFFFFFF,
    0xD0D6DD,
    0xA6AFBA,
    0x2A2F36,
    0xFFB347,
    0xFFCF66,
    0x0A0D10,
    {0, 0xFFC83D, 0xEFE9CF, 0xE3EAF2, 0x8F9DB2, 0x58B4FF, 0xFFFFFF, 0xFFD93D, 0xAAB6C4},
    {0x7FD1FF, 0x58B4FF, 0x6FE3B0, 0xC3A6FF, 0xFFD25C, 0xFFB454, 0xE9E4CC},
    {0x6FE3B0, 0xFFD25C, 0xFFA24D, 0xFF6B5E, 0xD79BFF},
    {0, 0x7FD1FF, 0xFF8A5C, 0xFFD25C, 0x58B4FF, 0xD79BFF},
    {0xA3B1C0, 0x6FE3B0, 0xFFD25C, 0xFFA24D, 0xFF6B5E},
    {{0x7FB2FF, 0x5CD6FF, 0x7BE495, 0xFFC94A, 0xFF6B4A}},
    0x58B4FF,
};

inline constexpr ThemePrincipal kThemeClair = {
    "clair",
    Disposition::TABLEAU,
    false,
    0xEEF1F4,
    0xFFFFFF,
    0xFF,
    0x12171C,
    0x37424E,
    0x4F5C6A,
    0xD5DBE2,
    0x0E8F5F,
    0x8A5A00,
    0xFFFFFF,
    {0, 0xCF6D00, 0x93792A, 0x6A839F, 0x4C5A6C, 0x1E78D6, 0x3F86B3, 0xB07800, 0x72849C},
    {0x0F78B0, 0x1E6FD0, 0x1B8A5A, 0x7A4FC2, 0xB06A00, 0xC25A00, 0x7A6A22},
    {0x1B8A5A, 0xB06A00, 0xC25A00, 0xC22F2F, 0x8E3FC0},
    {0, 0x0F78B0, 0xB85200, 0x8A5A00, 0x1E6FD0, 0x8E3FC0},
    {0x4F5C6A, 0x1B8A5A, 0x8A6400, 0xB45309, 0xC22F2F},
    {{0x2F6FDB, 0x0A88B8, 0x2E9E55, 0xC77A00, 0xD0391E}},
    0x1E78D6,
};

inline constexpr const ThemePrincipal *THEMES[] = {&kThemeCiel, &kThemeTableauSombre, &kThemeHorloge, &kThemeClair};
inline constexpr int NB_THEMES = 4;

// ---- accès par rôle ----

constexpr std::uint32_t teinte_role(const ThemePrincipal &t, Role r) noexcept { return t.role[static_cast<int>(r)]; }
constexpr std::uint32_t couleur_metrique(const ThemePrincipal &t, Metrique m) noexcept {
    return t.metrique[static_cast<int>(m)];
}
constexpr std::uint32_t couleur_uv(const ThemePrincipal &t, CategorieUv c) noexcept {
    return c == CategorieUv::INCONNU ? t.texte_3 : t.niveau_uv[static_cast<int>(c) - 1];
}
constexpr std::uint32_t couleur_alerte(const ThemePrincipal &t, Alerte a) noexcept {
    return t.alerte[static_cast<int>(a)];
}
// Couleur d'une alerte officielle de Vigie : `niveau` = 0 inconnu, 1 vert, 2 jaune, 3 orange, 4 rouge (ecran::vigie::Niveau) ; hors de
// cette plage, le neutre de l'inconnu.
constexpr std::uint32_t couleur_officielle(const ThemePrincipal &t, int niveau) noexcept {
    return t.officielle[niveau >= 0 && niveau <= 4 ? niveau : 0];
}

// Couleur d'une carte posée sur `fond_rgb` (mélange de la teinte de la carte et du fond selon l'opacité du thème).
constexpr std::uint32_t carte_sur(const ThemePrincipal &t, std::uint32_t fond_rgb) noexcept {
    return en_hex(melanger(depuis_hex(fond_rgb), depuis_hex(t.carte), t.carte_opa * 1000 / 255));
}

// ---- ciel ----

struct Degrade {
    std::uint32_t haut;
    std::uint32_t bas;
};

// Dégradé de base de chaque moment (haut, bas) : clair le jour, mais assez sombre pour qu'un texte blanc garde 4,5:1 partout
// (le test le mesure pour les 28 ciels).
inline constexpr Degrade CIEL_BASE[4] = {
    {0x050A1A, 0x14244A}, // nuit
    {0x1B2754, 0x7A4A5A}, // aube
    {0x123F7D, 0x2A5A8F}, // jour
    {0x241F5C, 0x7F3F46}, // crépuscule
};

struct TeinteTemps {
    std::uint32_t gris;
    int force_pm; // part de gris mélangée au dégradé de base, en millièmes
};

// Une teinte par ambiance (Ambiance::CLAIR ... ORAGE) : le ciel se grise, se voile, s'assombrit.
inline constexpr TeinteTemps CIEL_TEMPS[static_cast<int>(Ambiance::NOMBRE)] = {
    {0x000000, 0},   // clair : le dégradé de base
    {0x3E4F68, 300}, // nuageux
    {0x3A4656, 600}, // couvert
    {0x4A5663, 650}, // brume
    {0x27374A, 650}, // pluie
    {0x55677C, 550}, // neige
    {0x1A1E2E, 750}, // orage
};

constexpr Degrade ciel(MomentJour moment, Ambiance temps) noexcept {
    const Degrade base = CIEL_BASE[static_cast<int>(moment)];
    const TeinteTemps tt = CIEL_TEMPS[static_cast<int>(temps)];
    if (tt.force_pm == 0)
        return base;
    // La nuit, le gris des nuages est lui-même assombri : un ciel couvert de nuit n'est pas gris clair.
    const Rgb gris =
        moment == MomentJour::NUIT ? melanger(depuis_hex(tt.gris), Rgb{0, 0, 0}, 550) : depuis_hex(tt.gris);
    return {en_hex(melanger(depuis_hex(base.haut), gris, tt.force_pm)),
            en_hex(melanger(depuis_hex(base.bas), gris, tt.force_pm))};
}

} // namespace ecran::visuel
