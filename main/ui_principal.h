// Écran principal (phase 2) : grande horloge, date en français, bloc météo. Thème sombre.
//
// LVGL PUR (contrat d'interface n° 3) : ce fichier et ui_principal.cpp n'incluent AUCUN en-tête ESP-IDF, ni FreeRTOS, ni
// journal. Ils reçoivent leurs données par paramètre et la police par ses octets, afin que le rendu sur le PC (session
// rendu-pc) les compile tels quels. Le branchement sur l'appareil (minuteur, heure du réseau, lecture de la météo) est dans
// ecran_principal.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"
#include "meteo_etat.h" // MeteoEtat : contrat de la session meteo-appareil

namespace ecran {

// Ce que l'écran affiche, à chaque mise à jour. Les chaînes sont copiées pendant l'appel (rien n'est gardé).
struct EntreesPrincipal {
    bool heure_connue;        // faux : « --:-- » et la date est remplacée par `date_attente`
    char heure[8];            // « 11:40 » (24 h, deux chiffres)
    const char *date;         // « vendredi 6 novembre 2026 » (utilisée seulement si heure_connue)
    const char *date_attente; // phrase d'attente quand l'heure du réseau n'est pas encore là
    MeteoEtat meteo;          // copie de meteo_etat_lire()
    bool meteo_exemple;       // vrai : les valeurs météo sont un exemple (bouchon), le bloc le dit
};

// Construit l'écran sur `ecran` (un objet d'écran LVGL vide, de `largeur` x `hauteur` pixels logiques). `ttf` : octets d'une
// police TrueType (Roboto Bold), qui doit vivre aussi longtemps que l'écran. Rend faux si une police n'a pas pu être créée.
bool ui_principal_creer(lv_obj_t *ecran, const uint8_t *ttf, std::size_t ttf_taille, int32_t largeur, int32_t hauteur);

// Met à jour les textes (sans redessiner ceux qui n'ont pas changé). À appeler dans la tâche LVGL, verrou pris.
void ui_principal_mettre_a_jour(const EntreesPrincipal &entrees);

// Appui long (400 ms) sur l'écran : `rappel` est appelé dans la tâche LVGL. nullptr : rien. Sert à ouvrir l'écran de diagnostic.
void ui_principal_sur_appui_long(void (*rappel)());

// Bouton « Réglages » (ADR-0007), discret, en bas à droite : `rappel` est appelé dans la tâche LVGL sur un appui court. nullptr :
// le bouton est dessiné mais ne fait rien. Le branchement sur ui_reglages_ouvrir() est dans ecran_principal.cpp.
void ui_principal_sur_reglages(void (*rappel)());

} // namespace ecran
