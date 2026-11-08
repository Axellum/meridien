// Branchement de l'écran principal sur l'appareil : écrans (principal + diagnostic), minuteur LVGL, heure du réseau, météo.
// Les dessins sont dans ui_principal.* (LVGL pur, sans ESP-IDF) ; ce fichier-ci est le seul à connaître l'heure et la météo.
#pragma once

#include "boot_info.h"

namespace ecran {

// Construit l'écran de diagnostic (ui_bonjour) et l'écran principal, charge le principal et lance le minuteur de mise à jour.
// À appeler après board_init(), verrou de LVGL pris.
void ecran_principal_creer(const BootInfo &boot);

// Charge l'écran de diagnostic (cause du dernier redémarrage, Wi-Fi, point tactile, Redémarrer). Appelé par la ligne « Diagnostic » de
// Réglages > Appareil ; un appui long sur le diagnostic rend l'écran d'où l'on venait. Sans effet si le diagnostic est déjà affiché.
// À appeler dans la tâche LVGL, verrou pris.
void ecran_principal_ouvrir_diagnostic();

} // namespace ecran
