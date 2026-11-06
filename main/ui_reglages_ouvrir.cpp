// La colle entre les écrans de réglages (ui_reglages.cpp, ui_installation.cpp : LVGL pur, rendus aussi sur le PC) et l'appareil :
// l'écran de la carte, la police embarquée, l'hôte (mémoire permanente, Wi-Fi, lieu : hote_appareil_esp.cpp).
//
// Les réglages et l'installation guidée s'ouvrent sur un ÉCRAN À PART (lv_obj_create(nullptr)) : l'écran principal, ses minuteries
// et son contenu restent dessous, intacts. « Retour » sur le premier écran recharge l'écran qui était affiché et supprime celui-ci
// avec toute sa pile. Transitions instantanées, sans fondu (AGENTS.md, préférences du produit).
//
// Appelé dans la tâche LVGL, verrou d'affichage déjà pris (bouton « Réglages » de l'écran principal).
#include "ui_reglages.h"

#include <cstddef>
#include <cstdint>

#include "board.h"
#include "esp_log.h"
#include "lvgl.h"

#include "hote_appareil_esp.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf, EMBED_FILES de main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

const char *TAG = "reglages_ui";

lv_obj_t *g_ecran = nullptr;     // l'écran à part des réglages ; nul quand rien n'est ouvert
lv_obj_t *g_precedent = nullptr; // l'écran qui était affiché à l'ouverture

// « Retour » sur le premier écran des réglages : on rend l'écran d'avant. La pile d'écrans est supprimée d'abord (elle appartient
// à g_ecran), puis g_ecran lui-même, plus tard, parce qu'on est encore dans le rappel d'un bouton qu'il contient.
void fermer() {
    ecran::ui_reglages_liberer();
    if (g_precedent != nullptr) {
        lv_screen_load(g_precedent);
    }
    if (g_ecran != nullptr) {
        lv_obj_delete_async(g_ecran);
    }
    g_ecran = nullptr;
    g_precedent = nullptr;
}

// Crée l'écran à part, le charge et rend le contexte à remplir. Faux : déjà ouvert (un second appui ne fait rien).
bool ouvrir_ecran(ecran::ContexteReglages &ctx) {
    if (g_ecran != nullptr) {
        return false;
    }
    const board_info_t *carte = board_get_info();
    g_precedent = lv_screen_active();
    g_ecran = lv_obj_create(nullptr);
    lv_obj_remove_flag(g_ecran, LV_OBJ_FLAG_SCROLLABLE);
    ctx.racine = g_ecran;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = carte->width; // taille LOGIQUE (rotation comprise) : 1280 x 720 sur le Tab5, 1024 x 600 sur la 7B
    ctx.hauteur = carte->height;
    ctx.hote = &ecran::hote_de_l_appareil();
    ctx.fermer = fermer;
    return true;
}

} // namespace

void ui_reglages_ouvrir() {
    ecran::ContexteReglages ctx;
    if (!ouvrir_ecran(ctx)) {
        return;
    }
    ecran::ui_reglages_creer(ctx);
    lv_screen_load(g_ecran);
    ESP_LOGI(TAG, "réglages ouverts (%dx%d)", static_cast<int>(ctx.largeur), static_cast<int>(ctx.hauteur));
}

void ui_installation_ouvrir_si_necessaire() {
    if (ecran::hote_de_l_appareil().progression().terminee) {
        return;
    }
    ecran::ContexteReglages ctx;
    if (!ouvrir_ecran(ctx)) {
        return;
    }
    ecran::ui_installation_creer(ctx); // reprend à l'étape que l'hôte donne
    lv_screen_load(g_ecran);
    ESP_LOGI(TAG, "installation guidée ouverte à l'étape %d",
             static_cast<int>(ecran::hote_de_l_appareil().progression().etape));
}
