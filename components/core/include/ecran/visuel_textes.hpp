// Noyau pur : les textes de l'écran principal, de la page Détails et de la roue. Le texte FRANÇAIS est la clé de la table commune
// (i18n/*.yaml → i18n_donnees.inc, ADR-0006) ; la table propre à ce fichier y a été ramenée le 11/11/2026 (sections « Écran : accueil,
// grandeurs et roue »). Ce fichier ne garde que les noms que l'écran appelle (`visuel::texte`, `texte_c`) : un seul chemin de
// traduction, `i18n::tr`. Les phrases courtes sont voulues : ce sont des étiquettes posées sur de petites cartes.
//
// NON RELUES par un locuteur natif (comme le reste de la table). Tests : test/host/test_visuel_textes.cpp.
#pragma once

#include <string_view>

#include "ecran/i18n.hpp"

namespace ecran::visuel {

// Texte dans la langue : la table commune (repli : anglais, puis français). Le résultat est terminé par un zéro quand `francais`
// est un littéral (cas normal).
constexpr std::string_view texte(std::string_view francais, i18n::Langue langue) noexcept {
    return i18n::tr(francais, langue);
}

inline std::string_view texte(std::string_view francais) noexcept { return i18n::tr(francais); }

inline const char *texte_c(const char *francais) noexcept { return i18n::tr_c(francais); }

} // namespace ecran::visuel
