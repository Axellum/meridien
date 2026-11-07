// Écran principal : grande horloge, date, météo du moment, grandeurs (vent, pluie, humidité, pression, UV, soleil, lune), prévision
// heure par heure et sur sept jours, alertes, bouton Réglages. Fond en dégradé qui suit l'heure du jour et le temps.
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
    // Décalage à l'UTC du fuseau de l'horloge, en secondes. Sert au ciel (jour ou nuit) tant que la météo n'est pas lue : sans lui
    // le ciel jugeait l'heure en UTC et restait « nuit » une heure ou deux de trop au démarrage. Ignoré dès que la météo est connue.
    std::int32_t decalage_secours_s;
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

// Appui long (400 ms) sur l'écran : `rappel` est appelé dans la tâche LVGL. nullptr : rien. Sert à ouvrir l'écran de diagnostic.
void ui_principal_sur_appui_long(void (*rappel)());

// Bouton « Réglages » (ADR-0007), en haut à droite : `rappel` est appelé dans la tâche LVGL sur un appui court. nullptr : le bouton
// est dessiné mais ne fait rien. Le branchement sur ui_reglages_ouvrir() est dans ecran_principal.cpp.
void ui_principal_sur_reglages(void (*rappel)());

// Pour les essais : nombre d'objets LVGL de l'écran (racine comprise) et de polices créées, afin de mesurer ce que l'écran coûte
// en mémoire interne (chaque objet est alloué en RAM interne).
struct CoutEcran {
    int objets;
    int polices;
};
CoutEcran ui_principal_cout();

} // namespace ecran
