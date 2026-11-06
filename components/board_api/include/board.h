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
#include <stddef.h>
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

// --- Luminosite et veille de l'ecran -----------------------------------------------------------------------------------------
// Le NIVEAU est une luminosite PERCUE de 0 a 100, pas un rapport cyclique : 0 = eteint, 1 = le plus bas encore utilisable de la
// carte, 100 = son plafond ; entre les deux, une courbe de puissance 2,5 (components/core, ecran::luminosite) fait que 50 « parait
// a moitie ». Les bornes sont propres a chaque carte (components/bsp_<carte>). Apres un demarrage le niveau est 0 : l'application
// rallume le retroeclairage une fois sa premiere image dessinee.
esp_err_t
board_set_backlight_percent(int percent); // ce que demande l'application ; la veille peut l'abaisser sans le changer
int board_get_backlight_percent(void);    // dernier niveau demande par l'application (avant la veille)
// Rapport cyclique REELLEMENT applique, relu dans le materiel, en pour mille (0..1000) : pour les essais et le diagnostic.
uint32_t board_get_backlight_duty_permille(void);

// Veille : decision pure (components/core, ecran::veille), teste sur le PC. Apres board_init() rien n'est arme : l'ecran reste
// allume jusqu'a board_sleep_configure(). Le toucher qui reveille un ecran eteint n'atteint PAS l'interface (voile cliquable sur
// la couche systeme de LVGL, retire au relachement). La carte ne connait pas le fuseau horaire : l'heure LOCALE est fournie par
// l'application (board_sleep_set_local_time_source) ; sans elle les plages horaires sont ignorees.
#define BOARD_SLEEP_WINDOWS_MAX 4
typedef struct {
    int16_t start_min; // debut en minutes depuis minuit (heure locale), 0..1439
    int16_t end_min;   // fin exclue, 0..1440 ; fin < debut = la plage passe minuit (elle appartient au jour de depart)
    uint8_t days;      // bit 0 = dimanche ... bit 6 = samedi ; 0x7F = tous les jours
} board_sleep_window_t;
typedef struct {
    uint32_t
        dim_after_s; // secondes sans toucher avant d'attenuer (0 = jamais) ; doit etre < off_after_s si les deux sont non nuls
    uint32_t off_after_s; // secondes sans toucher avant d'eteindre (0 = jamais)
    uint32_t
        relight_after_s; // dans une plage horaire, un toucher rallume l'ecran puis il se reteint apres ce delai (15 conseille)
    int dim_level;    // niveau percu (1..100) d'un ecran attenue
    int window_count; // 0..BOARD_SLEEP_WINDOWS_MAX
    board_sleep_window_t
        windows[BOARD_SLEEP_WINDOWS_MAX]; // dans une plage : ecran eteint (sauf toucher recent), quel que soit le reste
} board_sleep_config_t;
typedef enum { BOARD_SCREEN_ON = 0, BOARD_SCREEN_DIMMED, BOARD_SCREEN_OFF } board_screen_state_t;
// Rend la minute du jour (0..1439) et le jour de la semaine (0 = dimanche) en heure LOCALE ; false = heure inconnue.
typedef bool (*board_local_time_cb_t)(int *minute_of_day, int *weekday);

esp_err_t board_sleep_configure(const board_sleep_config_t *cfg); // ESP_ERR_INVALID_ARG si le reglage est incoherent
void board_sleep_set_local_time_source(board_local_time_cb_t cb);
void board_screen_set_busy(
    bool busy); // vrai pendant que l'assistant parle, qu'un reveil sonne, qu'une mise a jour tourne
board_screen_state_t board_screen_get_state(void);
void board_screen_wake(void); // equivalent d'un toucher, sans toucher : repart du temps zero

// --- Horloge materielle (RTC) -------------------------------------------------------------------------------------------------
// Garde l'heure UTC sans reseau (supercondensateur). La couche carte la reprend d'elle-meme au demarrage si le systeme n'a pas
// d'heure plausible, et la remet a l'heure depuis une heure venue du RESEAU seulement (jamais depuis elle-meme), au premier
// reglage puis au plus toutes les 6 h. Regles et tests : components/core, ecran::rtc. Elle n'ecrase jamais une heure du reseau.
typedef enum { BOARD_TIME_SOURCE_NONE = 0, BOARD_TIME_SOURCE_RTC, BOARD_TIME_SOURCE_NETWORK } board_time_source_t;
typedef struct {
    bool present;          // la puce repond
    bool power_lost;       // elle a signale une perte d'alimentation avant ce demarrage (son heure n'etait pas fiable)
    uint8_t flag_at_boot;  // registre d'etat brut lu au demarrage (diagnostic)
    bool resumed_from_rtc; // le systeme a repris l'heure de la puce a ce demarrage
    board_time_source_t source; // qui a regle l'heure du systeme depuis (deduit d'un saut de l'horloge)
    uint32_t writes;            // ecritures de la puce depuis le demarrage
    int64_t last_write_epoch_s; // heure ecrite la derniere fois (0 si jamais)
} board_rtc_info_t;
esp_err_t
board_rtc_read(int64_t *epoch_s); // secondes depuis 1970 UTC ; ESP_ERR_INVALID_RESPONSE si la trame est incoherente
esp_err_t
board_rtc_write(int64_t epoch_s); // ecriture directe (essais, reglage manuel) ; ESP_ERR_INVALID_ARG hors 2026..2099
void board_rtc_get_info(board_rtc_info_t *info);
// A appeler apres un reglage de l'heure du systeme par le reseau (SNTP) : la prochaine passe ecrit la puce sans attendre le
// saut detecte. Facultatif : la couche carte detecte aussi seule un saut de plus de 2 s.
void board_rtc_note_network_time(void);

// --- Audio -----------------------------------------------------------------------------------------------------------------
// Sortie (haut-parleur, casque) et entree (microphones). L'initialisation des codecs et du bus I2S se fait au premier appel et
// coupe l'amplificateur entre deux sons. Le VOLUME est percu (0 = muet, 1..100) : voir components/core, ecran::volume. Une carte
// qui n'a pas une fonction rend ESP_ERR_NOT_SUPPORTED. Les tampons des enregistrements sont ceux de l'appelant (PSRAM conseillee).
typedef struct {
    bool codec_ready;            // codecs configures
    bool amplifier_on;           // amplificateur du haut-parleur allume a cet instant
    bool headphones;             // fiche casque inseree
    int volume_level;            // 0..100
    int volume_db_x10;           // attenuation correspondante en dixiemes de dB (muet = -960)
    uint32_t sample_rate;        // cadence du bus (Hz), 0 si le bus n'est pas ouvert
    uint32_t internal_ram_taken; // octets de RAM interne pris par le bus audio ouvert (mesure a l'ouverture)
} board_audio_info_t;
typedef struct {
    int32_t rms_dbfs_x10;  // niveau efficace, dixiemes de dBFS (-1200 = silence)
    int32_t peak_dbfs_x10; // niveau de crete
    uint32_t clipped;      // echantillons a pleine echelle
    int32_t dc;            // composante continue (echantillons)
    int32_t tone_dbfs_x10; // niveau de la frequence demandee (board_audio_mic_level), -1200 si aucune
} board_audio_level_t;
esp_err_t board_audio_init(void);
void board_audio_get_info(board_audio_info_t *info);
esp_err_t board_audio_set_volume(int level); // 0..100, applique tout de suite
// Sinus de test, bloquant : monte l'amplificateur, joue, le coupe. `peak_dbfs_x10` = niveau de crete du signal (-200 = -20 dBFS).
esp_err_t board_audio_play_tone(uint32_t hz, uint32_t duration_ms, int32_t peak_dbfs_x10);
// Ecoute le micro `duration_ms` (la premiere demi-seconde est jetee : stabilisation) et rend les niveaux ; `tone_hz` > 0 mesure aussi
// le niveau de cette frequence (detection d'un son joue par le haut-parleur).
esp_err_t board_audio_mic_level(uint32_t duration_ms, uint32_t tone_hz, board_audio_level_t *level);
// Lecture et enregistrement de blocs bruts, 16 bits, STEREO entrelaces (G, D) a la cadence du bus (16 000 Hz). `frames` = nombre de
// paires. Le micro rend les deux microphones de la carte. Bloquants ; le bus I2S est un seul port : voir docs/carte-tab5.md pour
// la simultaneite lecture + enregistrement.
esp_err_t board_audio_play_pcm(const int16_t *stereo, size_t frames, uint32_t timeout_ms);
esp_err_t board_audio_record_pcm(int16_t *stereo, size_t frames, size_t *recorded, uint32_t timeout_ms);

// --- Alimentation ----------------------------------------------------------------------------------------------------------
// Moniteur de courant du connecteur de BATTERIE et etat du chargeur. Le Tab5 d'Axel n'a PAS de batterie : sa tension lue n'a de
// sens que chargeur coupe (le firmware coupe le chargeur au demarrage et ne l'allume jamais). Une batterie est presente seulement
// si elle depasse 5,5 V chargeur coupe. Aucune puce ne dit si le port USB-C est alimente.
typedef enum {
    BOARD_BATTERY_UNKNOWN = 0,
    BOARD_BATTERY_NONE,
    BOARD_BATTERY_IDLE,
    BOARD_BATTERY_CHARGING,
    BOARD_BATTERY_DISCHARGING
} board_battery_state_t;
typedef struct {
    bool monitor_present; // le moniteur repond et son identifiant est celui attendu
    int32_t bus_mv;       // tension du connecteur de batterie (millivolts)
    int32_t current_ma;   // courant, NEGATIF = charge, positif = decharge
    board_battery_state_t battery;
    bool charger_enabled;   // sortie CHG_EN (toujours false : le firmware ne l'allume pas)
    bool charge_status_pin; // entree CHG_STAT : sans batterie elle annonce « en charge » (constate), ne pas s'y fier
} board_power_info_t;
esp_err_t board_power_get(board_power_info_t *info);

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
