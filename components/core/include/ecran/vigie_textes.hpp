// Noyau pur : les mots PROPRES à l'écran des alertes, en sept langues (fr, en, de, nl, es, it, tr), et la mise en forme des instants.
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation (test/host/test_vigie_textes.cpp).
//
// À ne pas confondre avec les mots de l'ÉMETTEUR : titre, description, consigne, niveau et phénomène d'origine arrivent de Vigie dans les
// langues de l'émetteur et ne passent jamais par ce fichier. Ici : les étiquettes de la page (« Début », « Fin »…), les mots que l'écran
// dérive du niveau et du phénomène de Vigie quand l'émetteur n'a rien dit de dessinable, et les phrases fixes (avertissement, couverture).
//
// Le texte FRANÇAIS est la clé de la table commune (i18n/*.yaml, ADR-0006 ; la table propre à ce fichier y a été ramenée le
// 11/11/2026) : repli sur l'anglais puis sur le français. NON RELUS par un locuteur natif.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/heure_fr.hpp"
#include "ecran/i18n.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/visuel_dates.hpp"

namespace ecran::vigie {

// Un mot de l'écran : sa clé française dans la table commune (i18n/*.yaml, section « Écran : la page des alertes »).
using Mot = std::string_view;

// Le mot `m` dans `langue` : la table commune (repli : anglais, puis le français). Une clé inconnue est rendue telle quelle.
constexpr std::string_view traduction(Mot m, i18n::Langue langue) noexcept { return i18n::tr(m, langue); }

// Le mot de clé `francais` dans `langue` (même chose, nom gardé pour l'écran des alertes).
constexpr std::string_view mot(std::string_view francais, i18n::Langue langue) noexcept {
    return i18n::tr(francais, langue);
}

// Pour printf et LVGL : `mot` rend un `string_view` sur un littéral terminé par un zéro.
inline const char *mot_c(const char *francais) noexcept { return i18n::tr_c(francais); }

constexpr std::string_view cle_phenomene(Phenomene p) noexcept {
    switch (p) {
    case Phenomene::Vent:
        return "Vent";
    case Phenomene::Pluie:
        return "Pluie";
    case Phenomene::Orage:
        return "Orage";
    case Phenomene::NeigeVerglas:
        return "Neige et verglas";
    case Phenomene::Chaleur:
        return "Chaleur";
    case Phenomene::Froid:
        return "Froid";
    case Phenomene::Crue:
        return "Crue";
    case Phenomene::Avalanche:
        return "Avalanche";
    case Phenomene::Cotier:
        return "Littoral";
    case Phenomene::Incendie:
        return "Incendie";
    case Phenomene::Brouillard:
        return "Brouillard";
    case Phenomene::Autre:
        return "Alerte";
    case Phenomene::Inconnu:
        break;
    }
    return "Alerte météo";
}

constexpr std::string_view cle_niveau(Niveau n) noexcept {
    switch (n) {
    case Niveau::Jaune:
        return "Jaune";
    case Niveau::Orange:
        return "Orange";
    case Niveau::Rouge:
        return "Rouge";
    case Niveau::Vert:
    case Niveau::Inconnu:
        break;
    }
    return {};
}

// Les mots de Vigie pour une alerte, quand l'émetteur n'a rien dit de dessinable : « Pluie · Orange » (le niveau n'est écrit que s'il est
// connu). Rend le nombre d'octets écrits (sans le zéro) ; 0 et un tampon vidé si le tampon est trop petit.
constexpr std::size_t libelle_derive(Phenomene phenomene, Niveau niveau_alerte, i18n::Langue langue, char *sortie,
                                     std::size_t capacite) noexcept {
    visuel::detail::Plume p{sortie, capacite};
    p.texte(mot(cle_phenomene(phenomene), langue));
    const std::string_view niveau = cle_niveau(niveau_alerte);
    if (!niveau.empty()) {
        p.texte(" \xC2\xB7 "); // « · »
        p.texte(mot(niveau, langue));
    }
    return p.finir();
}

// « 05:59 · samedi 8 novembre » (sans l'année) dans la langue de l'écran, d'après l'heure locale déjà calculée. L'heure vient EN
// PREMIER : le moteur de texte de l'écran coupe une ligne après un « : », donc « samedi 8 novembre · 05:59 » pouvait finir sur « … · 05: »
// puis « 59 » (vu sur le rendu PC) ; au début d'une ligne, l'heure ne peut pas être coupée. Rend 0 et un tampon vidé si la date est
// invalide ou le tampon trop petit.
constexpr std::size_t formater_instant(i18n::Langue langue, const heure::HeureLocale &h, char *sortie,
                                       std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    if (h.heure < 0 || h.heure > 23 || h.minute < 0 || h.minute > 59) {
        sortie[0] = '\0';
        return 0;
    }
    visuel::detail::Plume p{sortie, capacite};
    p.car(static_cast<char>('0' + h.heure / 10));
    p.car(static_cast<char>('0' + h.heure % 10));
    p.car(':');
    p.car(static_cast<char>('0' + h.minute / 10));
    p.car(static_cast<char>('0' + h.minute % 10));
    p.texte(" \xC2\xB7 ");
    const std::size_t n = visuel::formater_date(langue, h, sortie + p.n, capacite - p.n, false);
    if (n == 0) {
        sortie[0] = '\0';
        return 0;
    }
    return p.n + n;
}

} // namespace ecran::vigie
