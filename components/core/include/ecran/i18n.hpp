// Noyau pur : traduction de l'écran en sept langues (ADR-0006). `tr("texte français")` : le texte FRANÇAIS écrit dans
// le code est la clé (façon gettext, comme sur le Tab5) ; chaque langue est un fichier i18n/<code>.yaml, que
// components/core/outils/generer_i18n.py transforme en i18n_donnees.inc (table triée, une ligne par clé).
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation (les tests de test/host/ s'en servent).
//
// Repli : langue demandée, puis anglais, puis le texte français lui-même (la clé). Une clé inconnue est rendue telle
// quelle. L'INDICE d'une langue ne change jamais (il est enregistré en NVS) : fr 0, en 1, de 2, nl 3, es 4, it 5, tr 6,
// comme le sélecteur « Langue » du Tab5 ; une langue nouvelle prend le numéro suivant, à la fin.
//
// Les textes sont en UTF-8 : sous MSVC, compiler avec /utf-8. Les `string_view` rendus pointent soit dans la table
// (littéraux, terminés par un zéro), soit dans le texte reçu : `tr_c` rend un `const char*` pour printf/LVGL.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ecran::i18n {

enum class Langue : std::uint8_t { Fr = 0, En = 1, De = 2, Nl = 3, Es = 4, It = 5, Tr = 6 };

inline constexpr std::size_t NOMBRE_LANGUES = 7;

// Codes (ISO 639-1) et noms dans leur propre langue, dans l'ordre des indices.
inline constexpr std::string_view CODES[NOMBRE_LANGUES] = {"fr", "en", "de", "nl", "es", "it", "tr"};
inline constexpr std::string_view NOMS[NOMBRE_LANGUES] = {"Français", "English",  "Deutsch", "Nederlands",
                                                          "Español",  "Italiano", "Türkçe"};

constexpr int indice(Langue langue) noexcept { return static_cast<int>(langue); }

// Indice lu en NVS -> langue. Une valeur hors de 0..6 (NVS vide, abîmée, écrite par une version plus récente) donne
// `defaut` (le français) : jamais d'accès hors tableau.
constexpr Langue langue_depuis_indice(int i, Langue defaut = Langue::Fr) noexcept {
    return (i >= 0 && i < static_cast<int>(NOMBRE_LANGUES)) ? static_cast<Langue>(i) : defaut;
}

constexpr std::string_view code(Langue langue) noexcept { return CODES[indice(langue)]; }
constexpr std::string_view nom(Langue langue) noexcept { return NOMS[indice(langue)]; }

// « de » -> Langue::De ; vide si le code est inconnu (la casse compte : « DE » n'existe pas).
constexpr std::optional<Langue> langue_depuis_code(std::string_view c) noexcept {
    for (std::size_t i = 0; i < NOMBRE_LANGUES; ++i) {
        if (CODES[i] == c)
            return static_cast<Langue>(i);
    }
    return std::nullopt;
}

// Une ligne de la table : la clé (texte français) et ses traductions dans l'ordre en, de, nl, es, it, tr.
// Une case vide = pas de traduction dans cette langue.
struct Entree {
    std::string_view cle;
    std::string_view t[NOMBRE_LANGUES - 1];
};

inline constexpr Entree TABLE[] = {
#include "i18n_donnees.inc"
};
inline constexpr std::size_t NOMBRE_CLES = sizeof(TABLE) / sizeof(TABLE[0]);

namespace detail {

// Recherche dichotomique d'une clé dans une table triée par octets (comparaison d'octets non signés, comme Python).
constexpr const Entree *chercher(const Entree *table, std::size_t n, std::string_view cle) noexcept {
    std::size_t bas = 0;
    std::size_t haut = n;
    while (bas < haut) {
        const std::size_t milieu = bas + (haut - bas) / 2;
        const int c = table[milieu].cle.compare(cle);
        if (c == 0)
            return &table[milieu];
        if (c < 0) {
            bas = milieu + 1;
        } else {
            haut = milieu;
        }
    }
    return nullptr;
}

} // namespace detail

// Traduction propre à `langue`, sans repli : vide si la clé est inconnue, si la case est vide, ou si la langue est le
// français (le français n'a pas de traduction, il EST la clé).
constexpr std::string_view propre_dans(const Entree *table, std::size_t n, std::string_view francais,
                                       Langue langue) noexcept {
    if (langue == Langue::Fr)
        return {};
    const Entree *e = detail::chercher(table, n, francais);
    return e != nullptr ? e->t[indice(langue) - 1] : std::string_view{};
}

// Traduction avec repli : langue demandée, sinon anglais, sinon le texte français reçu.
constexpr std::string_view tr_dans(const Entree *table, std::size_t n, std::string_view francais,
                                   Langue langue) noexcept {
    if (langue == Langue::Fr)
        return francais;
    const Entree *e = detail::chercher(table, n, francais);
    if (e == nullptr)
        return francais;
    if (!e->t[indice(langue) - 1].empty())
        return e->t[indice(langue) - 1];
    if (!e->t[indice(Langue::En) - 1].empty())
        return e->t[indice(Langue::En) - 1];
    return francais;
}

constexpr std::string_view propre(std::string_view francais, Langue langue) noexcept {
    return propre_dans(TABLE, NOMBRE_CLES, francais, langue);
}

constexpr std::string_view tr(std::string_view francais, Langue langue) noexcept {
    return tr_dans(TABLE, NOMBRE_CLES, francais, langue);
}

// Le texte français est-il une clé de la table ?
constexpr bool est_une_cle(std::string_view francais) noexcept {
    return detail::chercher(TABLE, NOMBRE_CLES, francais) != nullptr;
}

// ---- langue courante (celle de l'écran) ----

namespace detail {
inline std::atomic<std::uint8_t> &courante() noexcept {
    static std::atomic<std::uint8_t> valeur{0}; // français tant que rien n'est réglé
    return valeur;
}
} // namespace detail

inline void definir_langue(Langue langue) noexcept { detail::courante().store(static_cast<std::uint8_t>(langue)); }

inline Langue langue_courante() noexcept { return langue_depuis_indice(detail::courante().load()); }

inline std::string_view tr(std::string_view francais) noexcept { return tr(francais, langue_courante()); }

// Pour printf et LVGL : le texte rendu est terminé par un zéro quand `francais` est un littéral (cas normal).
inline const char *tr_c(const char *francais) noexcept { return tr(std::string_view(francais)).data(); }

} // namespace ecran::i18n
