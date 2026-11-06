// Écrans de réglages et installation guidée (ADR-0007) : ce que l'extérieur appelle. Le détail (pile d'écrans, rangées, éditeurs) est
// dans ui_reglages_interne.h et reste privé. LVGL pur : le rendu PC (tools/rendu) compile ces fichiers tels quels.
#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

#include "hote_reglages.h"

namespace ecran {

// Tout ce qu'il faut pour dessiner : le conteneur plein écran qui reçoit les écrans, la police, la taille LOGIQUE de l'écran
// (rotation comprise : 1280x720, 720x1280, 1024x600...), l'hôte (mémoire permanente, Wi-Fi, lieu) et ce qu'il faut faire quand la
// personne quitte l'interface par « Retour » sur le premier écran.
struct ContexteReglages {
    lv_obj_t *racine = nullptr;   // conteneur sans disposition (ni flex ni grille), de la taille de l'écran
    const uint8_t *ttf = nullptr; // Roboto Bold embarquée (main/assets/roboto_700.ttf)
    std::size_t ttf_taille = 0;
    int32_t largeur = 0;
    int32_t hauteur = 0;
    HoteReglages *hote = nullptr;
    void (*fermer)() = nullptr; // rend la main à l'écran principal ; peut être nul (rendu PC)
};

// La liste des réglages (six sections). Efface ce que l'interface avait dessiné avant.
void ui_reglages_creer(const ContexteReglages &ctx);

// L'installation guidée en sept étapes, à l'étape que `hote->progression()` donne (carte neuve : la langue). Chaque « Continuer »
// écrit la progression auprès de l'hôte, un redémarrage au milieu reprend donc au même endroit.
void ui_installation_creer(const ContexteReglages &ctx);

// Ouvre un écran précis (essais et rendu PC ; l'appareil n'en a pas besoin). Un chemin = une base, puis éventuellement « / » et un
// écran ouvert par-dessus :
//   bases : « liste », « section:<n>[:<onglet>] » (n = 0 à 5), « etape:<n> » (installation, n = 1 à 7) ;
//   par-dessus : « wifi », « wifi-mdp:<ssid> », « connexion:<ssid>[:<mot de passe>] », « ville », « editeur:<identifiant du réglage> ».
// Exemples : « section:4:1 », « etape:2/wifi », « section:0/connexion:Maison:secret123 ».
void ui_aller(const char *chemin);

// Comme si la personne venait de toucher sa langue à la première étape (« Continuer » devient possible). Rendu PC et essais.
void ui_simuler_langue_choisie();

// Pose `texte` dans le champ de l'éditeur affiché, comme si on l'avait tapé ; `valider` touche ensuite « OK » (rendu PC et essais).
void ui_simuler_saisie(const char *texte, bool valider = false);

} // namespace ecran

// Appelé par l'écran principal (bouton « Réglages ») : ouvre la liste des réglages par-dessus. Défini dans ui_reglages_ouvrir.cpp
// (colle du firmware : afficheur de la carte, hôte, police), jamais compilé par le rendu PC.
void ui_reglages_ouvrir();
