// Icônes de l'écran principal : dessin des pièces A8 de main/assets/icones_meteo.bin (voir tools/icones/generer_icones.py).
//
// LVGL PUR (comme ui_principal.h) : aucun en-tête ESP-IDF. Les pixels sont dans le binaire embarqué (EMBED_FILES, donc en flash :
// zéro octet de RAM interne) ; les descripteurs LVGL de chaque image sont des constantes en flash aussi (ui_meteo_icones_table.inc,
// générée avec le binaire). Une icône météo est un empilement de 1 à 3 pièces teintées par les rôles du thème.
#pragma once

#include <cstdint>

#include "ecran/visuel_icones.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"

namespace ecran::icones {

// Côté (en pixels) de l'image réellement dessinée pour une pièce quand on demande `cote` : la plus grande taille générée qui
// tient, sinon la plus petite. 0 si la pièce est inconnue.
int taille_dessinee(visuel::Piece piece, int cote);

// Dessine une pièce dans le carré (x, y, cote, cote), centrée, teinte `rgb`, opacité `opa`.
void piece(lv_layer_t *couche, visuel::Piece piece, int cote, int x, int y, std::uint32_t rgb,
           std::uint8_t opa = LV_OPA_COVER);

// Dessine une icône météo (ses couches teintées par les rôles du thème) dans le carré (x, y, cote, cote).
void meteo(lv_layer_t *couche, visuel::IdIcone icone, int cote, int x, int y, const visuel::ThemePrincipal &theme,
           std::uint8_t opa = LV_OPA_COVER);

// Phase de lune `indice` (0 à 7) dans le carré (x, y, cote, cote).
void lune(lv_layer_t *couche, int indice, int cote, int x, int y, std::uint32_t rgb);

// Nombre d'images du binaire (pour les tests de cohérence du banc).
int nombre_images();

} // namespace ecran::icones
