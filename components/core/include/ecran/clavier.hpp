// Noyau pur : DISPOSITIONS DU CLAVIER à l'écran pour les sept langues (ADR-0007, point 4). Données seulement : le composant clavier
// de LVGL (main/ui_clavier.cpp) en tire ses cartes de boutons. Une disposition = quelques lignes de touches, chaque touche un
// mot séparé par une espace ; une touche de commande commence par « # » et fait partie d'une liste fermée (CONTROLES).
//
// Les dispositions suivent le clavier physique de chaque langue (AZERTY, QWERTZ, QWERTY, Q turc) avec une ligne d'accents quand la
// langue en a besoin. Les majuscules sont écrites en dur et non déduites des minuscules : en turc, le « i » donne « İ » et le « ı »
// donne « I » (la règle ASCII ferait « I » pour les deux). Un test du PC vérifie que chaque lettre de chaque alphabet est
// atteignable en minuscule et en majuscule, que chaque touche est UN caractère, et que minuscules et majuscules ont la même forme.
//
// Ce que ce fichier ne dit PAS : que la police embarquée dessine chaque caractère (components/core/outils/verifier_glyphes.py et
// test/host/controle_reglages.py le lisent dans la table `cmap`), ni que les touches soient assez grandes pour un doigt (à
// constater sur la carte, ADR-0007 « à prouver »).
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/i18n.hpp"

namespace ecran::clavier {

// Les trois cartes d'une langue. Les chiffres sont sur la première ligne des deux premières (un mot de passe en a souvent besoin),
// les signes dans la troisième, commune aux sept langues.
enum class Mode : std::uint8_t { Minuscules, Majuscules, Symboles };
inline constexpr std::size_t NOMBRE_MODES = 3;
inline constexpr std::size_t LIGNES_MAX = 6;

enum class Controle : std::uint8_t {
    Aucun,    // une touche qui écrit son caractère
    Maj,      // « #maj » : majuscule pour UN caractère (puis retour aux minuscules), ou retour aux minuscules
    Symboles, // « #sym » : carte des signes
    Lettres,  // « #abc » : retour aux lettres
    Efface,   // « #eff » : efface le caractère avant le curseur
    Valider,  // « #ok » : valide la saisie
    Espace,   // « #esp » : une espace
};

inline constexpr std::string_view CONTROLES[] = {"#maj", "#sym", "#abc", "#eff", "#ok", "#esp"};

constexpr Controle controle(std::string_view touche) noexcept {
    if (touche == "#maj")
        return Controle::Maj;
    if (touche == "#sym")
        return Controle::Symboles;
    if (touche == "#abc")
        return Controle::Lettres;
    if (touche == "#eff")
        return Controle::Efface;
    if (touche == "#ok")
        return Controle::Valider;
    if (touche == "#esp")
        return Controle::Espace;
    return Controle::Aucun;
}

struct Disposition {
    std::string_view lignes[LIGNES_MAX];
    std::size_t nb;
};

namespace detail {

// Les signes, communs aux sept langues (de quoi écrire une adresse, un mot de passe, un nom de réseau).
inline constexpr Disposition SYMBOLES = {{"1 2 3 4 5 6 7 8 9 0", "@ # $ % & * + - = /", "( ) [ ] { } < > ! ?",
                                          "\" ' ` ^ ~ | \\ _ : ; #eff", "#abc , #esp . #ok", ""},
                                         5};

inline constexpr std::string_view CHIFFRES = "1 2 3 4 5 6 7 8 9 0";
inline constexpr std::string_view BAS = "#sym , #esp . #ok";

// Français (AZERTY) + ligne d'accents.
inline constexpr Disposition FR_MIN = {{CHIFFRES, "a z e r t y u i o p", "q s d f g h j k l m",
                                        "#maj w x c v b n ' #eff", "é è ê ë à â ù û ç ô î ï œ", BAS},
                                       6};
inline constexpr Disposition FR_MAJ = {{CHIFFRES, "A Z E R T Y U I O P", "Q S D F G H J K L M",
                                        "#maj W X C V B N ' #eff", "É È Ê Ë À Â Ù Û Ç Ô Î Ï Œ", BAS},
                                       6};
// Anglais (QWERTY).
inline constexpr Disposition EN_MIN = {
    {CHIFFRES, "q w e r t y u i o p", "a s d f g h j k l", "#maj z x c v b n m #eff", BAS, ""}, 5};
inline constexpr Disposition EN_MAJ = {
    {CHIFFRES, "Q W E R T Y U I O P", "A S D F G H J K L", "#maj Z X C V B N M #eff", BAS, ""}, 5};
// Allemand (QWERTZ), ä ö ü ß sur le clavier.
inline constexpr Disposition DE_MIN = {
    {CHIFFRES, "q w e r t z u i o p ü", "a s d f g h j k l ö ä", "#maj y x c v b n m ß #eff", BAS, ""}, 5};
inline constexpr Disposition DE_MAJ = {
    {CHIFFRES, "Q W E R T Z U I O P Ü", "A S D F G H J K L Ö Ä", "#maj Y X C V B N M ẞ #eff", BAS, ""}, 5};
// Néerlandais (QWERTY) + accents.
inline constexpr Disposition NL_MIN = {
    {CHIFFRES, "q w e r t y u i o p", "a s d f g h j k l", "#maj z x c v b n m #eff", "é è ê ë ï ö ü ä", BAS}, 6};
inline constexpr Disposition NL_MAJ = {
    {CHIFFRES, "Q W E R T Y U I O P", "A S D F G H J K L", "#maj Z X C V B N M #eff", "É È Ê Ë Ï Ö Ü Ä", BAS}, 6};
// Espagnol (QWERTY) avec ñ + accents et signes ouvrants.
inline constexpr Disposition ES_MIN = {
    {CHIFFRES, "q w e r t y u i o p", "a s d f g h j k l ñ", "#maj z x c v b n m #eff", "á é í ó ú ü ¿ ¡", BAS}, 6};
inline constexpr Disposition ES_MAJ = {
    {CHIFFRES, "Q W E R T Y U I O P", "A S D F G H J K L Ñ", "#maj Z X C V B N M #eff", "Á É Í Ó Ú Ü ¿ ¡", BAS}, 6};
// Italien (QWERTY) + accents.
inline constexpr Disposition IT_MIN = {{
                                           CHIFFRES,
                                           "q w e r t y u i o p",
                                           "a s d f g h j k l",
                                           "#maj z x c v b n m #eff",
                                           "à è é ì ò ù",
                                           BAS,
                                       },
                                       6};
inline constexpr Disposition IT_MAJ = {{
                                           CHIFFRES,
                                           "Q W E R T Y U I O P",
                                           "A S D F G H J K L",
                                           "#maj Z X C V B N M #eff",
                                           "À È É Ì Ò Ù",
                                           BAS,
                                       },
                                       6};
// Turc (clavier Q) : ı sans point et i avec point sont deux lettres ; majuscules I et İ.
inline constexpr Disposition TR_MIN = {{
                                           CHIFFRES,
                                           "q w e r t y u ı o p ğ ü",
                                           "a s d f g h j k l ş i",
                                           "#maj z x c v b n m ö ç #eff",
                                           BAS,
                                           "",
                                       },
                                       5};
inline constexpr Disposition TR_MAJ = {{
                                           CHIFFRES,
                                           "Q W E R T Y U I O P Ğ Ü",
                                           "A S D F G H J K L Ş İ",
                                           "#maj Z X C V B N M Ö Ç #eff",
                                           BAS,
                                           "",
                                       },
                                       5};

} // namespace detail

constexpr Disposition disposition(i18n::Langue langue, Mode mode) noexcept {
    if (mode == Mode::Symboles)
        return detail::SYMBOLES;
    const bool maj = mode == Mode::Majuscules;
    switch (langue) {
    case i18n::Langue::Fr:
        return maj ? detail::FR_MAJ : detail::FR_MIN;
    case i18n::Langue::En:
        return maj ? detail::EN_MAJ : detail::EN_MIN;
    case i18n::Langue::De:
        return maj ? detail::DE_MAJ : detail::DE_MIN;
    case i18n::Langue::Nl:
        return maj ? detail::NL_MAJ : detail::NL_MIN;
    case i18n::Langue::Es:
        return maj ? detail::ES_MAJ : detail::ES_MIN;
    case i18n::Langue::It:
        return maj ? detail::IT_MAJ : detail::IT_MIN;
    case i18n::Langue::Tr:
        return maj ? detail::TR_MAJ : detail::TR_MIN;
    }
    return detail::EN_MIN;
}

// ---- lecture d'une ligne ------------------------------------------------------------------------------------------------

// Nombre de touches d'une ligne (les mots séparés par une espace).
constexpr std::size_t compter_touches(std::string_view ligne) noexcept {
    std::size_t n = 0;
    bool dans_un_mot = false;
    for (char c : ligne) {
        if (c == ' ') {
            dans_un_mot = false;
        } else if (!dans_un_mot) {
            dans_un_mot = true;
            ++n;
        }
    }
    return n;
}

// La `rang`-ième touche d'une ligne (0 = la première), ou vide si elle n'existe pas.
constexpr std::string_view touche(std::string_view ligne, std::size_t rang) noexcept {
    std::size_t vu = 0;
    std::size_t debut = 0;
    while (debut < ligne.size()) {
        while (debut < ligne.size() && ligne[debut] == ' ')
            ++debut;
        if (debut >= ligne.size())
            break;
        std::size_t fin = debut;
        while (fin < ligne.size() && ligne[fin] != ' ')
            ++fin;
        if (vu == rang)
            return ligne.substr(debut, fin - debut);
        ++vu;
        debut = fin;
    }
    return {};
}

// Nombre de caractères Unicode d'un texte UTF-8 valide.
constexpr std::size_t nombre_de_caracteres(std::string_view s) noexcept {
    std::size_t n = 0;
    for (char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u)
            ++n;
    }
    return n;
}

} // namespace ecran::clavier
