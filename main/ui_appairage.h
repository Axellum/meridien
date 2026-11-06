// Bandeau provisoire du bas de l'écran : l'adresse de la page de réglages et le code d'appairage à taper. Posé sur la couche
// supérieure de LVGL (il passe devant l'écran principal sans le modifier). `ecran-principal` placera ensuite ces informations dans
// son écran et retirera le bandeau. Aucun en-tête ESP-IDF (rendu sur PC possible).
#pragma once

#include "lvgl.h"

namespace ecran {

// Crée le bandeau au premier appel, puis met à jour et montre. À appeler avec le verrou d'affichage pris.
//   adresse : « 192.168.1.50 » (sans « http:// ») ; nom : « ecran-f6b6 » (sans « .local ») ; code : 6 chiffres.
// Un texte vide n'est pas affiché (le bandeau reste utile avec l'adresse seule).
void ui_appairage_afficher(lv_display_t *affichage, const char *adresse, const char *nom, const char *code);

} // namespace ecran
