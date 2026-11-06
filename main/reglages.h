// Réglages que la page de réglages modifie : langue de l'écran, langue de l'assistant, thème, luminosité (NVS, espace
// « reglages »), et la ville avec son complément d'affichage (région, pays), au-dessus de meteo_zone.h qui garde le lieu lui-même.
//
// Contrat avec l'interface (`ecran-principal`) : `reglages_affichage()` donne les valeurs courantes ; `reglages_definir_rappel()`
// enregistre une fonction appelée APRÈS chaque enregistrement réussi, dans la tâche du serveur de la page (prendre le verrou
// d'affichage avant de toucher à LVGL). La langue courante de `ecran::i18n` et le rétroéclairage sont déjà appliqués par ce module.
// Ce fichier est PUR (aucun en-tête ESP-IDF).
#pragma once

#include "ecran/reglages_valeurs.hpp"

namespace ecran {

// Lit la NVS, applique la langue de l'écran à `ecran::i18n` (la luminosité s'applique à l'allumage : voir main.cpp). À appeler
// une fois, après nvs_flash_init() (boot_info_load) et avant de créer l'interface. Sans NVS lisible : valeurs par défaut
// (français, thème auto, luminosité 75 %).
void reglages_demarrer();

// Copie des valeurs courantes (sûre depuis n'importe quelle tâche).
reglages::Affichage reglages_affichage();

// Enregistre en NVS puis applique : rien n'est appliqué si l'écriture échoue (rend faux, l'ancien état reste en service).
// Refuse une luminosité hors de 5..100 (un écran noir depuis un téléphone n'est pas récupérable).
bool reglages_enregistrer_affichage(const reglages::Affichage &nouveau);

using RappelReglages = void (*)(const reglages::Affichage &nouveau);
void reglages_definir_rappel(RappelReglages rappel);

// Ville enregistrée avec sa région et son pays (le lieu lui-même — nom, coordonnées, fuseau — est celui de meteo_zone.h).
// `reglages_zone_lire` rend faux seulement si la lecture est impossible ; la région et le pays sont vides si la ville a été
// enregistrée sans eux ou par un autre chemin que la page.
bool reglages_zone_lire(reglages::Zone &zone);
// Rend faux si meteo_zone.h refuse la zone (rien n'est écrit). Le complément (région, pays) est enregistré à part : s'il
// échoue, la ville est quand même enregistrée.
bool reglages_zone_enregistrer(const reglages::Zone &zone);

} // namespace ecran
