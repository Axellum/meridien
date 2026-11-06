// Interface commune des cartes (ADR-0005). Le code applicatif n'inclut que ce fichier : il ne sait pas sur quelle carte il
// tourne. Chaque carte (components/bsp_<carte>) fournit ces fonctions ; boards/<carte>/ porte ses reglages de construction.
//
// Convention : l'interface se dessine a partir de board_get_info()->width / height, jamais en coordonnees absolues. Ces deux
// valeurs sont la taille LOGIQUE, celle que LVGL voit APRES la rotation que la carte applique pour que l'image soit droite
// dans le sens voulu (paysage). La taille NATIVE de la dalle est a part (panel_width / panel_height). Une carte dont la dalle
// est deja en paysage (la 7B, 1024 x 600) rend rotation_degrees = 0 et width / height = panel_width / panel_height ; le tactile
// est alors livre a LVGL dans les memes coordonnees logiques (c'est la couche carte qui s'en charge, jamais l'application).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_rom_sys.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *board_name; // "M5Stack Tab5"
    const char
        *display_controller;      // detecte a l'execution, avant d'initialiser le DSI : "ST7121", "ST7123", "ILI9881C"
    const char *touch_controller; // puce tactile lue, avec sa version : "Sitronix (version 1)"
    int32_t width;                // pixels, taille LOGIQUE de l'interface (apres rotation) : 1280 sur le Tab5
    int32_t height;               // 720 sur le Tab5
    int32_t panel_width;          // pixels, taille NATIVE de la dalle (avant rotation) : 720 sur le Tab5
    int32_t panel_height;         // 1280 sur le Tab5
    int32_t rotation_degrees; // rotation que LVGL applique (0, 90, 180, 270), convention de lv_display_set_rotation() :
    // 90 sur le Tab5, equivalent du `rotation: 270` d'ESPHome (voir docs/mesures/orientation.md)
} board_info_t;

// Jalons du demarrage, en millisecondes depuis le lancement de l'application (esp_timer), pour mesurer avant de sacrifier.
typedef struct {
    int32_t init_start_ms;          // entree dans board_init()
    int32_t expander_ready_ms;      // attente de l'expandeur ecoulee et expandeur initialise
    int32_t controller_detected_ms; // controleur d'ecran identifie (apres le reset du LCD et du tactile)
    int32_t panel_ready_ms;         // DSI + dalle initialises
    int32_t lvgl_ready_ms;          // LVGL, affichage et tactile enregistres
} board_boot_timeline_t;

// Alimentations, reset de l'ecran et du tactile, detection du controleur, DSI, dalle, LVGL, tactile.
// Le retroeclairage reste eteint : l'application l'allume (board_set_backlight_percent) une fois sa premiere image dessinee.
esp_err_t board_init(void);

const board_info_t *board_get_info(void);
const board_boot_timeline_t *board_get_boot_timeline(void);

// Ecran LVGL cree par board_init(). LVGL tourne dans sa propre tache : verrouiller (board_display_lock) avant d'y toucher.
lv_display_t *board_get_display(void);

// Verrou de LVGL. timeout_ms = 0 : attendre sans limite. Rend false si le verrou n'a pas pu etre pris.
bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);

esp_err_t board_set_backlight_percent(int percent);

// --- Wi-Fi par le coprocesseur radio (ADR-0005) ---
// Sur le Tab5 la radio est un ESP32-C6 relie en SDIO (esp_hosted, version epinglee par la couche carte). board_init() n'allume
// que l'alimentation du coprocesseur. board_wifi_link_up() etablit le lien (remise a zero du coprocesseur, SDIO) et lit sa
// version ; ensuite le code applicatif utilise l'API esp_wifi / esp_netif standard (esp_wifi_remote la route vers le
// coprocesseur). Cette couche ne connait ni SSID ni mot de passe. Le firmware du coprocesseur n'est jamais ecrit (decision
// d'Axel du 16/10/2026) : lecture de sa version seulement.
typedef struct {
    bool link_up;                 // transport actif entre le P4 et le coprocesseur
    bool version_known;           // version du coprocesseur lue (RPC) ; false si elle n'a pas repondu
    char coprocessor_version[24]; // "1.4.1" ; vide si inconnue
    char host_version[24];        // pilote esp_hosted cote P4, par exemple "2.12.12"
    int32_t power_on_ms; // alimentation du coprocesseur allumee (ms depuis le lancement de l'application), -1 si non
    int32_t link_up_ms;  // lien etabli (meme origine), -1 tant qu'il ne l'est pas
    int32_t attempts;    // 1 = lien au premier essai ; >1 = l'alimentation du coprocesseur a du etre coupee puis rendue
    uint32_t transport_failures; // evenements ESP_HOSTED_EVENT_TRANSPORT_FAILURE vus depuis le demarrage
} board_wifi_info_t;

// Etablit le lien avec le coprocesseur. Bloque jusqu'a timeout_ms au plus (ESP_ERR_TIMEOUT sinon). Un premier echec provoque
// une coupure puis une remise de l'alimentation du coprocesseur et un second essai ; board_wifi_get_info()->attempts le dit.
// A appeler avant esp_wifi_init(). Une panne du transport en marche (TRANSPORT_FAILURE) redemarre la carte par board_restart().
esp_err_t board_wifi_link_up(uint32_t timeout_ms);
void board_wifi_get_info(board_wifi_info_t *info);

// Redemarrage logiciel. Sur ESP32-P4, esp_restart() ne remet a zero que les coeurs (boucle du lien C6 vue sur la 7B) :
// la seule forme autorisee dans ce projet est la remise a zero du systeme par la ROM.
static inline void __attribute__((noreturn)) board_restart(void) {
    esp_rom_software_reset_system();
    for (;;) {
    }
}

#ifdef __cplusplus
}
#endif
