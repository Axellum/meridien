// Page « mer » (ADR-0022) : la mer d'un spot en un coup d'œil. Quatre cartes d'activité (surf, kite, bateau, pêche à pied) avec leur verdict,
// leur meilleur créneau et leurs 24 heures en barres, le bandeau d'une vigilance côtière ou de l'âge des données, la courbe de la marée du
// jour et le soleil ; un appui sur une carte ouvre le détail critère par critère. C'est le port, sur Meridien, de la page mer de Vigie
// (vigie-display) : mêmes règles de lecture, mêmes mots, sans l'écran « Eau » (la mer n'en donne pas la température).
//
// LVGL PUR, comme ui_alertes.h : aucun en-tête ESP-IDF, FreeRTOS ni journal. La page reçoit tout ce qu'elle montre par paramètre (la mer
// lue, l'instant, le décalage horaire du lieu) pour que le rendu PC (tools/rendu, scène « mer ») la compile telle quelle. Les règles de
// lecture sont dans le noyau pur (ecran/mer_page.hpp : quoi montrer ; ecran/mer_textes.hpp : les mots en sept langues ;
// ecran/mer_geometrie.hpp : où), TESTÉS sur le PC ; ce fichier-ci ne fait que dessiner ce qu'ils décident.
//
// La page est une IMAGE de la mer au moment de l'ouverture : elle ne se redessine pas si une trame plus récente arrive pendant qu'elle est
// ouverte (on la ferme et on la rouvre), comme la page des alertes. Pas de fondu : l'écran change d'un coup (AGENTS.md, préférences du
// produit). Elle s'ouvre par la roue d'appui long (entrée « Mer » du premier anneau, quand un spot est choisi : ADR-0024).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/mer_trame.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran {

struct ContexteMer {
    lv_obj_t *racine = nullptr;        // l'écran LVGL (vide) qui reçoit la page, de la taille de l'écran
    const std::uint8_t *ttf = nullptr; // Roboto Bold embarquée (main/assets/roboto_700.ttf)
    std::size_t ttf_taille = 0;
    int32_t largeur = 0; // taille LOGIQUE de l'écran (rotation comprise)
    int32_t hauteur = 0;
    const visuel::ThemePrincipal *theme = &visuel::kThemeCiel;
    void (*fermer)() = nullptr; // « Fermer » : rend la main à l'écran qui l'a ouverte ; peut être nul (rendu PC)
};

// Ce que la page montre. La mer est COPIÉE (une page garde sa propre image) ; `maintenant_s` nul veut dire « pas d'horloge » : la page ne
// dit alors ni âge, ni prochaine marée, ni coefficient, ni repère « maintenant ». `decalage_s` : l'UTC du lieu en secondes, pour écrire
// les heures (la trame est en UTC et ne porte pas le fuseau du spot) ; l'appelant le tire de l'horloge de l'appareil.
struct DonneesMer {
    const mer::Mer *mer = nullptr;
    std::int64_t maintenant_s = 0;
    std::int32_t decalage_s = 0;
};

// Construit la page sur `ctx.racine`. Rend faux si une police n'a pas pu être créée, si les données manquent ou si l'écran est trop petit.
// Les quatre polices sont créées à la première ouverture (et à chaque changement de taille d'écran) et gardées : TinyTTF se libère mal,
// même choix que les pages des alertes et des réglages.
bool ui_mer_creer(const ContexteMer &ctx, const DonneesMer &donnees);

// Ouvre le détail de l'activité `activite` (0 à 3) comme le ferait un appui sur sa carte, ou le referme (-1). Sert au rendu PC, qui n'a
// pas de doigt ; rend faux si la page n'est pas construite ou si cette carte n'a rien à détailler.
bool ui_mer_detail(int activite);

} // namespace ecran
