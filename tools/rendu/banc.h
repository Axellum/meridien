// Banc de rendu hors carte (tools/rendu/, docs/outils.md) : ce que le banc offre aux adaptateurs de scène (scenes/scene_*.cpp).
//
// Une scène construit l'interface du firmware (main/ui_*.cpp, jamais modifié) avec des données fixées à la place du temps réel.
// Le banc fournit l'affichage LVGL en mémoire (RGB565 comme la dalle du Tab5), les bouchons de l'interface commune des cartes
// (board.h), l'instant fixé, la taille de l'écran et la langue (tr() du noyau pur).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/i18n.hpp"
#include "lvgl.h"

namespace banc {

struct Contexte {
    int32_t largeur; // pixels de l'affichage simulé (taille logique demandée : 1280x720, 720x1280, 1024x600...)
    int32_t hauteur;
    int64_t instant_utc_s; // l'instant fixé (std::time(nullptr) le rend aussi : voir banc.cpp)
    lv_display_t *affichage;
    ecran::i18n::Langue langue; // déjà posée par le banc (ecran::i18n::definir_langue) : tr() la suit
};

struct Scene {
    const char *variante;                 // « connecte », « etape-3 »... : nom du fichier PNG (sans espace ni accent)
    const char *legende;                  // légende française de la planche : « Étape 3 : Lieu »
    void (*construire)(const Contexte &); // construit l'écran sur contexte.affichage
};

// Définies par le fichier de la scène (un seul fichier par scène : tools/rendu/scenes/scene_<nom>.cpp).
extern const Scene kScenes[];
extern const std::size_t kNombreScenes;

// Identité de la carte que board_get_info() rend à l'interface. Par défaut le banc la déduit de la taille : 1024x600 = Waveshare
// 7B (docs/compatibilite.md), sinon Tab5 ST7121 (chaînes de components/bsp_tab5).
void definir_carte(const char *nom, const char *controleur_ecran, const char *controleur_tactile);

// Durée de démarrage affichée par certains écrans (board_get_boot_timeline()->lvgl_ready_ms).
void definir_duree_demarrage_ms(int32_t ms);

// La zone que lit main/horloge.cpp (meteo_zone.h, zone_lire) : le LIEU dont l'écran montre l'heure (ADR-0012). Par défaut la zone
// d'usine de l'appareil (Lyon, Europe/Paris), donc les scènes qui ne l'appellent pas rendent comme avant. `fuseau_iana` : un nom
// de la table du noyau (« Asia/Tokyo »). À appeler au début de la scène, avant de construire l'écran.
void definir_zone(const char *nom, float latitude, float longitude, const char *fuseau_iana);

} // namespace banc
