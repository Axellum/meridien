// Page des alertes officielles (ADR-0009) : ce qu'un appui sur la pastille de l'écran principal ouvre. Le phénomène, le niveau, la
// période, l'émetteur et son texte, ce qu'il demande de faire, puis, toujours, qui fournit ces données (Vigie et ses sources) et la
// phrase fixe qui dit que ce n'est pas un service d'alerte officiel.
//
// LVGL PUR, comme ui_principal.h : aucun en-tête ESP-IDF, FreeRTOS ni journal. La page reçoit tout ce qu'elle montre par paramètre (la
// réponse lue, l'instant, la fonction qui met un instant à l'heure locale) pour que le rendu PC (tools/rendu) la compile telle quelle.
// Les mots de l'ÉMETTEUR sont montrés tels quels ; les étiquettes sont celles de ecran/vigie_textes.hpp, en sept langues.
//
// La page est une IMAGE de la réponse au moment de l'ouverture : elle ne se redessine pas si une réponse plus récente arrive pendant
// qu'elle est ouverte (on la ferme et on la rouvre). Pas de fondu : l'écran change d'un coup (AGENTS.md, préférences du produit).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/heure_fr.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_vue.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran {

struct ContexteAlertes {
    lv_obj_t *racine = nullptr;        // l'écran LVGL (vide) qui reçoit la page, de la taille de l'écran
    const std::uint8_t *ttf = nullptr; // Roboto Bold embarquée (main/assets/roboto_700.ttf)
    std::size_t ttf_taille = 0;
    int32_t largeur = 0; // taille LOGIQUE de l'écran (rotation comprise)
    int32_t hauteur = 0;
    const visuel::ThemePrincipal *theme = &visuel::kThemeCiel;
    void (*fermer)() = nullptr; // « Fermer » : rend la main à l'écran principal ; peut être nul (rendu PC)
    // Met un instant UTC à l'heure locale de l'appareil (horloge_heure_locale) ; nul ou faux : l'instant est dit « non communiqué ».
    bool (*heure_locale)(std::int64_t instant_s, heure::HeureLocale &sortie) = nullptr;
};

// Ce que la page montre : la réponse lue, quelles alertes sont en vigueur (vigie::composer), quand la réponse est arrivée. Les pointeurs
// ne servent que pendant l'appel : tout est copié dans les objets de la page.
struct DonneesAlertes {
    const vigie::Alertes *alertes = nullptr;
    const vigie::Vue *vue = nullptr;
    std::int64_t recu_s = 0; // instant UTC où la réponse est arrivée (0 : inconnu)
};

// Construit la page sur `ctx.racine`. Rend faux si une police n'a pas pu être créée ou si les données manquent.
bool ui_alertes_creer(const ContexteAlertes &ctx, const DonneesAlertes &donnees);
// Les trois polices de la page sont créées à la première ouverture et gardées (TinyTTF se libère mal : même choix que la page des
// réglages) ; la page ne rend donc rien à « Fermer » à part ses objets, qui partent avec la suppression de `racine`.

} // namespace ecran
