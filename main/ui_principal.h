// Écran principal : grande horloge, date, météo du moment, grandeurs (vent, pluie, humidité, pression, UV, soleil, lune), prévision
// heure par heure et sur sept jours, alertes. Fond en dégradé qui suit l'heure du jour et le temps. Les réglages s'ouvrent par la roue
// d'appui long (ui_roue.h).
//
// LVGL PUR (contrat d'interface n° 3) : ce fichier et ses voisins (ui_principal.cpp, ui_meteo_*.cpp) n'incluent AUCUN en-tête
// ESP-IDF, ni FreeRTOS, ni journal. Ils reçoivent leurs données par paramètre, la police et les icônes par leurs octets, afin que
// le rendu sur le PC (tools/rendu/) les compile tels quels. Le branchement sur l'appareil (minuteur, heure du réseau, lecture de la
// météo) est dans ecran_principal.cpp.
//
// Tout ce qui est calculable sans LVGL est dans le noyau pur (components/core/include/ecran/visuel_*.hpp, testé sur le PC) ; les
// couleurs, opacités et dispositions sont dans les thèmes (visuel_themes.hpp). L'écran est dessiné pour un écran de référence de
// 1280 x 720 (paysage) ou 720 x 1280 (portrait) et mis à l'échelle sur les autres tailles (1024 x 600 de la 7B).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/heure_fr.hpp"
#include "ecran/vigie_bandeau.hpp"
#include "ecran/visuel_themes.hpp"
#include "ecran/visuel_vue.hpp"
#include "lvgl.h"

namespace ecran {

// Ce que l'écran affiche, à chaque mise à jour. Les chaînes sont copiées pendant l'appel (rien n'est gardé).
struct EntreesPrincipal {
    bool heure_connue; // faux : « --:-- » et la date est remplacée par `date_attente`
    char heure[8];     // « 11:40 » (24 h, deux chiffres)
    const char *date;  // « vendredi 6 novembre 2026 » dans la langue de l'écran (utilisée seulement si heure_connue)
    const char
        *date_attente; // phrase d'attente quand l'heure du réseau n'est pas encore là (nul : la phrase de l'écran)
    std::int64_t maintenant_s; // instant UTC du moment : position du soleil, lune, colonne « maintenant » (0 : inconnu)
    visuel::VueMeteo meteo;    // la météo ; `connue` faux : « Météo indisponible »
    // Le LIEU choisi (ADR-0012). `fuseau` : les règles de son fuseau (décalage et changements d'heure), qui convertissent CHAQUE
    // instant affiché en heure locale : étiquette de chaque colonne « heure par heure », lever et coucher du soleil, noms des jours,
    // « aujourd'hui ». Un seul décalage pour toute la fenêtre serait faux dès qu'elle contient un changement d'heure.
    // `fuseau_connu` faux : le nom du fuseau n'est pas dans la table ; l'écran garde alors le décalage de la réponse météo (fixe) et,
    // sans météo, ne devine pas le ciel (nuit). `latitude_deg` / `longitude_deg` (`position_connue`) : le ciel (jour ou nuit) tant que la
    // météo n'est pas lue est calculé au lever et au coucher du soleil du lieu (ecran/soleil.hpp), plus avec une plage d'heures fixe.
    ecran::heure::Fuseau fuseau;
    bool fuseau_connu;
    bool position_connue;
    double latitude_deg;
    double longitude_deg;
    // L'alerte officielle de Vigie en vigueur (ADR-0009) : `present` faux = rien n'est dessiné à sa place (pas de bandeau permanent).
    // Quand elle est présente, sa pastille remplace celle de l'alerte déduite de la météo, dans la barre du haut.
    vigie::Bandeau bandeau;
};

// Données du jeu d'icônes et de la police : octets embarqués, qui doivent vivre aussi longtemps que l'écran.
struct RessourcesPrincipal {
    const std::uint8_t *ttf; // Roboto Bold
    std::size_t ttf_taille;
};

// Construit l'écran sur `ecran` (un objet d'écran LVGL vide, de `largeur` x `hauteur` pixels logiques) avec le thème `theme`.
// Rend faux si une police n'a pas pu être créée. Les icônes (ui_meteo_icones.cpp) sont lues dans le binaire embarqué.
bool ui_principal_creer(lv_obj_t *ecran, const RessourcesPrincipal &ressources, int32_t largeur, int32_t hauteur,
                        const visuel::ThemePrincipal &theme = visuel::kThemeCiel);

// Met à jour les textes et les dessins (sans redessiner ce qui n'a pas changé). À appeler dans la tâche LVGL, verrou pris.
void ui_principal_mettre_a_jour(const EntreesPrincipal &entrees);

// Change de thème sur l'écran déjà construit (couleurs, fond), tout est redessiné au passage suivant. Les thèmes qui ont la même
// composition (« ciel », « tableau sombre », « clair ») se remplacent à chaud ; passer de ou vers « horloge » demande une autre
// taille de police et une autre disposition : rend faux sans rien changer, il faut alors reconstruire l'écran. Rend vrai si le
// thème demandé est maintenant celui de l'écran. À appeler dans la tâche LVGL, verrou pris. Le thème doit vivre aussi longtemps
// que l'écran (les thèmes de visuel_themes.hpp sont des constantes).
bool ui_principal_changer_theme(const visuel::ThemePrincipal &theme);

// Appui long (400 ms) sur l'écran : `rappel(x, y)` est appelé dans la tâche LVGL, avec le point où le doigt est (pixels de l'écran).
// Un doigt qui a glissé de plus que le seuil de glissement depuis qu'il s'est posé n'est pas un appui long : rien n'est appelé.
// nullptr : rien. Sert à ouvrir la roue (ui_roue.h, ADR-0019), d'où l'on atteint les réglages : il n'y a plus de bouton « Réglages »
// sur cet écran.
void ui_principal_sur_appui_long(void (*rappel)(int32_t x, int32_t y));

// Pastille de l'alerte officielle (ADR-0009) : `rappel` est appelé dans la tâche LVGL sur un appui court sur la pastille, qui n'est
// tactile que tant qu'une alerte est en vigueur (EntreesPrincipal::bandeau.present). Le branchement sur la page des alertes est dans
// ecran_principal.cpp. nullptr : la pastille est dessinée mais ne fait rien.
void ui_principal_sur_alertes(void (*rappel)());

// Pour les essais : nombre d'objets LVGL de l'écran (racine comprise) et de polices créées, afin de mesurer ce que l'écran coûte
// en mémoire interne (chaque objet est alloué en RAM interne).
struct CoutEcran {
    int objets;
    int polices;
};
CoutEcran ui_principal_cout();

} // namespace ecran
