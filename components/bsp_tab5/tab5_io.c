// Bus I2C, expandeurs PI4IOE5V6408 (0x43 : ecran et tactile ; 0x44 : alimentation du coprocesseur Wi-Fi), retroeclairage, reset
// de l'ecran et du tactile.
#include "board.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_io_expander_pi4ioe5v6408.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "tab5_internal.h"

static const char *TAG = "tab5_io";

#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_1
#define BACKLIGHT_LEDC_BITS TAB5_BACKLIGHT_BITS // 10 : LEDC_TIMER_10_BIT, voir tab5_internal.h
// 1 kHz comme le firmware ESPHome en service (ledc, tab5-hardware.yaml) ; le BSP d'Espressif prend 5 kHz.
#define BACKLIGHT_LEDC_HZ 1000

static i2c_master_bus_handle_t s_bus;
static esp_io_expander_handle_t
    s_expander; // 0x43 : resets de l'ecran et du tactile, ampli du haut-parleur, prise casque
static esp_io_expander_handle_t s_expander_power; // 0x44 : coprocesseur Wi-Fi (bit 0), chargeur (bits 5 a 7)

// Verrou commun a toutes les ecritures et lectures des deux expandeurs depuis les taches de fonctionnement (Wi-Fi, audio,
// alimentation) : le pilote de l'expandeur lit, modifie puis ecrit ses registres sans protection. Statique : pas de tas.
static StaticSemaphore_t s_expander_lock_storage;
static SemaphoreHandle_t s_expander_lock;

static esp_io_expander_handle_t expander_handle(tab5_expander_t which) {
    return which == TAB5_EXPANDER_ALIM ? s_expander_power : s_expander;
}

esp_err_t tab5_expander_output(tab5_expander_t which, uint32_t pin_mask, int level) {
    esp_io_expander_handle_t expander = expander_handle(which);
    if (expander == NULL || s_expander_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_expander_lock, portMAX_DELAY);
    // Direction D'ABORD : le pilote refuse d'ecrire le niveau d'une broche en entree (« can't set level in input mode », constate
    // sur carte le 07/11/2026). Consequence : a la PREMIERE sortie d'une broche, elle prend un instant le niveau du registre de
    // sortie de la puce (0 apres la remise a zero) avant le niveau voulu : sans importance pour SPEAKER_EN et CHG_EN (0 = arret),
    // et tab5_alim.cpp ne pilote pas nCHG_QC_EN quand elle est deja a l'arret.
    esp_err_t err = esp_io_expander_set_dir(expander, pin_mask, IO_EXPANDER_OUTPUT);
    if (err == ESP_OK) {
        err = esp_io_expander_set_level(expander, pin_mask, level ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = esp_io_expander_set_output_mode(expander, pin_mask, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
    }
    xSemaphoreGive(s_expander_lock);
    return err;
}

// Etat de la broche SANS changer sa direction (une sortie reste une sortie) : pour verifier ce qu'on a ecrit.
esp_err_t tab5_expander_peek(tab5_expander_t which, uint32_t pin_mask, int *level) {
    esp_io_expander_handle_t expander = expander_handle(which);
    if (expander == NULL || s_expander_lock == NULL || level == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_expander_lock, portMAX_DELAY);
    uint32_t value = 0;
    const esp_err_t err = esp_io_expander_get_level(expander, pin_mask, &value);
    xSemaphoreGive(s_expander_lock);
    if (err == ESP_OK) {
        *level = (value & pin_mask) ? 1 : 0;
    }
    return err;
}

esp_err_t tab5_expander_input(tab5_expander_t which, uint32_t pin_mask, int *level) {
    esp_io_expander_handle_t expander = expander_handle(which);
    if (expander == NULL || s_expander_lock == NULL || level == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_expander_lock, portMAX_DELAY);
    esp_err_t err = esp_io_expander_set_dir(expander, pin_mask, IO_EXPANDER_INPUT);
    uint32_t value = 0;
    if (err == ESP_OK) {
        err = esp_io_expander_get_level(expander, pin_mask, &value);
    }
    xSemaphoreGive(s_expander_lock);
    if (err == ESP_OK) {
        *level = (value & pin_mask) ? 1 : 0;
    }
    return err;
}

// Tirage vers le bas d'une entree (CHG_STAT, comme le firmware ESPHome : `mode: INPUT_PULLDOWN`).
esp_err_t tab5_expander_pull_down(tab5_expander_t which, uint32_t pin_mask) {
    esp_io_expander_handle_t expander = expander_handle(which);
    if (expander == NULL || s_expander_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_expander_lock, portMAX_DELAY);
    esp_err_t err = esp_io_expander_set_dir(expander, pin_mask, IO_EXPANDER_INPUT);
    if (err == ESP_OK) {
        err = esp_io_expander_set_pullupdown(expander, pin_mask, IO_EXPANDER_PULL_DOWN);
    }
    xSemaphoreGive(s_expander_lock);
    return err;
}

i2c_master_bus_handle_t tab5_i2c_bus(void) { return s_bus; }

static esp_err_t backlight_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_BITS,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer retroeclairage");
    ledc_channel_config_t channel = {
        .gpio_num = TAB5_PIN_BACKLIGHT,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0, // eteint tant que l'application n'a pas dessine sa premiere image
        .hpoint = 0,
    };
    return ledc_channel_config(&channel);
}

// Registre brut du PWM. La courbe de luminosite percue (niveau 0..100 -> registre) est dans tab5_ecran.cpp (noyau pur
// ecran::luminosite, teste sur le PC) : ici on ne fait qu'ecrire et relire le materiel.
esp_err_t tab5_backlight_set_duty(uint32_t duty) {
    const uint32_t max_duty = (1U << BACKLIGHT_LEDC_BITS) - 1U;
    if (duty > max_duty) {
        duty = max_duty;
    }
    ESP_RETURN_ON_ERROR(ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty), TAG, "duty");
    return ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
}

uint32_t tab5_backlight_get_duty(void) { return ledc_get_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL); }

// --- Reprise sur un expandeur qui ne repond pas au demarrage -----------------------------------------------------------------
// Constate le 06/11/2026 (Tab5 ST7121, demarrage n° 65, binaire c1fa7f6) : « pi4ioe5v6408: reset(252): Chip reset failed » puis
// « carte non initialisee ; arret (l'ecran reste eteint) » : une seule tentative, puis ecran noir definitif. On fait jusqu'a
// EXPANDEUR_ESSAIS essais espaces (avec une remise a zero du bus I2C entre deux), une ligne de journal par essai ; si tout echoue,
// la carte redemarre (au plus REDEMARRAGES_MAX fois de suite, compteur en memoire RTC non initialisee) avant de s'arreter avec un
// message lisible. Le compteur revient a zero des qu'un demarrage reussit.
#define EXPANDEUR_ESSAIS 5
#define EXPANDEUR_PAUSE_MS 150
#define REDEMARRAGES_MAX 3
#define REPRISE_MAGIC                                                                                                  \
    0x45585031u // « EXP1 » : distingue un compteur ecrit par nous d'un contenu aleatoire (mise sous tension)

// Essai sur carte du chemin de reprise : nombre d'essais a faire echouer artificiellement (0 = binaire normal). Option CMake
// TAB5_AUTOTEST_EXPANDEUR (voir CMakeLists.txt). 99 : tous les essais echouent, donc redemarrages puis arret.
#ifndef TAB5_AUTOTEST_EXPANDEUR
#define TAB5_AUTOTEST_EXPANDEUR 0
#endif

typedef struct {
    uint32_t magic;
    uint32_t redemarrages;
} reprise_t;

static RTC_NOINIT_ATTR reprise_t s_reprise;

static esp_err_t expandeur_principal_creer(void) {
    if (s_reprise.magic != REPRISE_MAGIC) {
        s_reprise.magic = REPRISE_MAGIC;
        s_reprise.redemarrages = 0;
    }
    esp_err_t err = ESP_FAIL;
    for (int essai = 1; essai <= EXPANDEUR_ESSAIS; essai++) {
        if (essai <= TAB5_AUTOTEST_EXPANDEUR) {
            err = ESP_ERR_TIMEOUT; // echec simule (binaire d'essai)
        } else {
            err =
                esp_io_expander_new_i2c_pi4ioe5v6408(s_bus, ESP_IO_EXPANDER_I2C_PI4IOE5V6408_ADDRESS_LOW, &s_expander);
        }
        if (err == ESP_OK) {
            if (essai > 1 || s_reprise.redemarrages > 0) {
                ESP_LOGW(TAG, "expandeur 0x43 : répond à l'essai %d/%d (après %u redémarrage(s) de reprise)", essai,
                         EXPANDEUR_ESSAIS, (unsigned)s_reprise.redemarrages);
            }
            s_reprise.redemarrages = 0;
            return ESP_OK;
        }
        ESP_LOGW(TAG, "expandeur 0x43 : essai %d/%d, erreur %s", essai, EXPANDEUR_ESSAIS, esp_err_to_name(err));
        if (essai < EXPANDEUR_ESSAIS) {
            const esp_err_t bus_err = i2c_master_bus_reset(s_bus);
            if (bus_err != ESP_OK) {
                ESP_LOGW(TAG, "remise à zéro du bus I2C refusée : %s", esp_err_to_name(bus_err));
            }
            vTaskDelay(pdMS_TO_TICKS(EXPANDEUR_PAUSE_MS));
        }
    }
    if (s_reprise.redemarrages < REDEMARRAGES_MAX) {
        s_reprise.redemarrages++;
        ESP_LOGE(TAG, "expandeur 0x43 muet après %d essais : redémarrage de reprise %u/%d dans 1 s", EXPANDEUR_ESSAIS,
                 (unsigned)s_reprise.redemarrages, REDEMARRAGES_MAX);
        vTaskDelay(pdMS_TO_TICKS(1000));
        board_restart();
    }
    ESP_LOGE(TAG,
             "expandeur 0x43 muet après %d essais et %d redémarrages de reprise : l'écran reste éteint (bus I2C ou "
             "alimentation "
             "de l'écran à vérifier ; débrancher puis rebrancher l'USB)",
             EXPANDEUR_ESSAIS, REDEMARRAGES_MAX);
    s_reprise.redemarrages = 0; // le prochain démarrage (appui sur reset, USB) repart d'un compteur propre
    return err;
}

esp_err_t tab5_io_init(void) {
    // Retroeclairage a 0 % d'abord : la broche flotte tant que le LEDC n'est pas configure.
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "retroeclairage");

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TAB5_I2C_PORT,
        .sda_io_num = TAB5_PIN_I2C_SDA,
        .scl_io_num = TAB5_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "bus I2C");
    s_expander_lock = xSemaphoreCreateMutexStatic(&s_expander_lock_storage);

    // Attention : creer le pilote de l'expandeur le REMET A ZERO (registre 0x01, puis tous les registres par defaut :
    // sorties a bas, haute impedance). Au demarrage c'est l'etat voulu ; ce n'est pas un appel a refaire en marche.
    // Plusieurs essais, puis redemarrage limite : un premier echec (« Chip reset failed », boot n° 65 du 06/11/2026, ecran
    // reste noir jusqu'a l'action de l'auteur) ne doit plus laisser la carte eteinte (voir expandeur_principal_creer).
    ESP_RETURN_ON_ERROR(expandeur_principal_creer(), TAG, "expandeur 0x43");

    // Alimentation du coprocesseur Wi-Fi. Une erreur ici ne doit pas empecher l'ecran de demarrer : on la journalise et
    // l'application affichera "Wi-Fi absent". Creer ce pilote remet aussi l'expandeur 0x44 a ses valeurs par defaut ; le seul
    // bit ensuite piloté est WIFI_EN (les autres sorties restent en haute impedance, comme apres une mise sous tension).
    esp_err_t err =
        esp_io_expander_new_i2c_pi4ioe5v6408(s_bus, ESP_IO_EXPANDER_I2C_PI4IOE5V6408_ADDRESS_HIGH, &s_expander_power);
    if (err == ESP_OK) {
        err = tab5_wifi_power_set(true);
    }
    if (err == ESP_OK) {
        tab5_wifi_note_power_on_ms((int32_t)(esp_timer_get_time() / 1000));
        ESP_LOGI(TAG, "alimentation du coprocesseur Wi-Fi allumée (expandeur 0x44, bit 0)");
    } else {
        s_expander_power = NULL;
        ESP_LOGE(TAG, "alimentation du coprocesseur Wi-Fi non allumée : %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

bool tab5_wifi_power_available(void) { return s_expander_power != NULL; }

// WIFI_EN (expandeur 0x44, bit 0). Allumer : niveau haut ecrit AVANT de piloter la broche (elle sort de la haute impedance deja
// a l'etat haut, sans impulsion basse). Eteindre : niveau bas, broche toujours pilotee.
esp_err_t tab5_wifi_power_set(bool on) {
    esp_io_expander_handle_t expander = s_expander_power;
    if (expander == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    // Meme verrou que tab5_expander_output() : le chargeur (bits 5 a 7 du meme expandeur) est pilote depuis une autre tache.
    // Avant que le verrou existe (creation de l'expandeur dans tab5_io_init), personne d'autre n'ecrit.
    const bool locked = s_expander_lock != NULL && xSemaphoreTake(s_expander_lock, portMAX_DELAY) == pdTRUE;
    esp_err_t err = esp_io_expander_set_dir(expander, TAB5_EXPANDER_POWER_WIFI_EN, IO_EXPANDER_OUTPUT);
    if (err == ESP_OK) {
        err = esp_io_expander_set_level(expander, TAB5_EXPANDER_POWER_WIFI_EN, on ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = esp_io_expander_set_output_mode(expander, TAB5_EXPANDER_POWER_WIFI_EN, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
    }
    if (locked) {
        xSemaphoreGive(s_expander_lock);
    }
    return err;
}

// Reset de la dalle. La ligne LCD_RST est tenue a bas en push-pull, puis rendue en ENTREE a pull-up (drain ouvert) pour la
// remonter : la laisser en sortie haute rend l'I2C instable (docs/recherche/01-pile-et-qualite.md ; meme sequence que le BSP
// d'Espressif, bsp_feature_en.c, cas BSP_FEATURE_LCD).
esp_err_t tab5_lcd_reset_pulse(void) {
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_OUTPUT), TAG, "dir");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_LCD_RST, 0), TAG, "niveau bas");
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL), TAG,
        "push-pull");
    vTaskDelay(pdMS_TO_TICKS(10)); // espp : 10 ms bas ; ESPHome : 5 ms
    ESP_RETURN_ON_ERROR(esp_io_expander_set_pullupdown(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_PULL_UP), TAG,
                        "pull-up");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_INPUT), TAG, "entree");
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_LCD_RST, IO_EXPANDER_OUTPUT_MODE_OPEN_DRAIN), TAG,
        "drain ouvert");
    vTaskDelay(pdMS_TO_TICKS(120)); // le controleur n'accepte SLPOUT que ~120 ms apres le reset
    return ESP_OK;
}

esp_err_t tab5_touch_enable(void) {
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT), TAG, "dir");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_TOUCH_RST, 1), TAG, "niveau haut");
    return esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
}

esp_err_t tab5_touch_reset_pulse(void) {
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT), TAG, "dir");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_TOUCH_RST, 0), TAG, "niveau bas");
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_output_mode(s_expander, TAB5_EXPANDER_TOUCH_RST, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL), TAG,
        "push-pull");
    vTaskDelay(pdMS_TO_TICKS(20)); // espp : 20 ms bas puis 50 ms avant de parler a la puce
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_expander, TAB5_EXPANDER_TOUCH_RST, 1), TAG, "niveau haut");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}
