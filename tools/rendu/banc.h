// Banc de rendu hors carte (tools/rendu/, docs/outils.md) : ce que le banc offre aux adaptateurs de scène (scene_*.cpp).
//
// Une scène construit l'interface du firmware (main/ui_*.cpp, jamais modifié) avec des données fixées à la place du temps réel.
// Le banc fournit l'affichage LVGL en mémoire (RGB565 comme la dalle du Tab5), les bouchons de l'interface commune des cartes
// (board.h) et l'instant fixé.
#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

namespace banc {

struct Contexte {
    int32_t largeur; // pixels de l'affichage simulé
    int32_t hauteur;
    int64_t instant_utc_s; // l'instant fixé (std::time(nullptr) le rend aussi : voir banc.cpp)
    lv_display_t *affichage;
};

struct Scene {
    const char *variante;                 // « connecte », « attente »... : nom du fichier PNG
    void (*construire)(const Contexte &); // construit l'écran sur contexte.affichage
};

// Définies par l'adaptateur de la scène.
extern const Scene kScenes[];
extern const std::size_t kNombreScenes;

// Identité de la carte que board_get_info() rend à l'interface (défaut : Tab5 ST7121, chaînes de components/bsp_tab5).
void definir_carte(const char *nom, const char *controleur_ecran, const char *controleur_tactile);

// Durée de démarrage affichée par certains écrans (board_get_boot_timeline()->lvgl_ready_ms).
void definir_duree_demarrage_ms(int32_t ms);

} // namespace banc
