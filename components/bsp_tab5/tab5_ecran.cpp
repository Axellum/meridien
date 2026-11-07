// Luminosite PERCUE et veille de l'ecran du Tab5 (implementation de board.h : board_set_backlight_percent, board_sleep_*,
// board_screen_*). Toute la DECISION est dans le noyau pur, teste sur le PC avec mutations :
//   - ecran::luminosite : niveau 1..100 -> registre du PWM (loi de puissance 2,5, plancher utilisable, plafond de la carte) ;
//   - ecran::veille : allume / attenue / eteint d'apres l'inactivite, l'heure locale et les plages horaires.
// Ce fichier ne fait que : lire l'inactivite de LVGL, demander l'heure locale a l'application, appliquer la decision.
//
// Mecanisme : un seul timer LVGL de 100 ms (pas de tache en plus, pas de RAM interne en plus que son objet). Il tourne dans la
// tache LVGL, verrou pris : on peut donc y toucher aux objets LVGL sans autre precaution. Les fonctions publiques appelees d'une
// autre tache prennent le verrou de LVGL (board_display_lock, recursif).
//
// ECRAN ETEINT = retroeclairage a 0 ET un voile noir opaque sur la couche systeme de LVGL. Le voile sert a une chose : le toucher
// qui REVEILLE l'ecran ne doit pas declencher le bouton qui se trouve dessous (on ne sait pas ce qu'on touche dans le noir). Au
// reveil le voile devient transparent mais reste en place, toujours cliquable, jusqu'au RELACHEMENT du doigt, puis il est supprime :
// ni appui ni relachement ne traversent. Il n'y a PAS de mise en veille de la dalle par commande DCS (0x28/0x10) : non essayee sur
// ST7121 / ST7123 (TDDI, le tactile y est lie a la dalle), et le retroeclairage a 0 suffit a l'oeil. A reprendre avec un essai
// sur carte si la consommation de la dalle eteinte devient un sujet.
//
// Apres un redemarrage l'etat est « allume » et le niveau de l'application « 0 » : c'est l'application qui rallume le
// retroeclairage (board_set_backlight_percent) une fois sa premiere image dessinee, comme avant. Rien n'est garde en memoire
// d'un demarrage a l'autre ici : l'ecran ne peut donc pas rester eteint apres un redemarrage a cause de ce fichier.
#include "board.h"
#include "ecran/luminosite.hpp"
#include "ecran/veille.hpp"
#include "esp_log.h"
#include "tab5_internal.h"

namespace {

namespace lum = ecran::luminosite;
namespace vei = ecran::veille;

constexpr char TAG[] = "tab5_ecran";

// Courbe du Tab5 : plancher 2 % (20 pour mille) et plafond 100 %. Le plancher est une ESTIME a valider a l'oeil (clignotement ?
// allumage franc ?), jamais mesuree sans camera : docs/carte-tab5.md. Le plafond de 100 % est celui d'avant ; un Tab5 sur un port
// USB faible a deja redemarre (« brownout ») a 100 % sous ESPHome le 08/10/2026 : si cela se reproduit ici, baisser duty_max_pm
// ici (une seule ligne) plutot que dans l'application.
constexpr lum::Courbe kCourbeTab5{20, 1000};
constexpr uint32_t kPeriodeMs = 100;

lv_display_t *s_disp = nullptr;
lv_timer_t *s_timer = nullptr;
lv_obj_t *s_voile = nullptr;

int s_niveau_app = 0; // ce que l'application a demande (0 = eteint tant qu'elle n'a rien demande)
vei::Reglage s_reglage;
vei::Etat s_etat = vei::Etat::Allume;
bool s_occupe = false;
board_local_time_cb_t s_heure_locale = nullptr;

const char *nom_etat(vei::Etat e) {
    switch (e) {
    case vei::Etat::Allume:
        return "allumé";
    case vei::Etat::Attenue:
        return "atténué";
    case vei::Etat::Eteint:
        return "éteint";
    }
    return "?";
}

const char *nom_cause(vei::Cause c) {
    switch (c) {
    case vei::Cause::Occupe:
        return "occupé";
    case vei::Cause::Activite:
        return "activité";
    case vei::Cause::Inactivite:
        return "inactivité";
    case vei::Cause::PlageHoraire:
        return "plage horaire";
    case vei::Cause::Aucune:
        break;
    }
    return "-";
}

// Niveau percu effectivement applique au retroeclairage selon l'etat de veille.
int niveau_effectif() {
    switch (s_etat) {
    case vei::Etat::Allume:
        return s_niveau_app;
    case vei::Etat::Attenue:
        return lum::niveau_plafonne(s_niveau_app, s_reglage.niveau_attenue);
    case vei::Etat::Eteint:
        return 0;
    }
    return 0;
}

void appliquer_retroeclairage() {
    const uint32_t duty = lum::duty_pour_bits(niveau_effectif(), TAB5_BACKLIGHT_BITS, kCourbeTab5);
    const esp_err_t err = tab5_backlight_set_duty(duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rétroéclairage non écrit : %s", esp_err_to_name(err));
    }
}

bool doigt_pose() {
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            return true;
        }
    }
    return false;
}

// Voile noir : couleur technique (absence de lumiere), pas un role de palette de l'interface.
void voile_poser() {
    if (s_voile == nullptr) {
        s_voile = lv_obj_create(lv_layer_sys());
        lv_obj_remove_flag(s_voile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_voile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(s_voile, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_radius(s_voile, 0, 0);
        lv_obj_set_style_border_width(s_voile, 0, 0);
        lv_obj_set_style_pad_all(s_voile, 0, 0);
        lv_obj_set_style_bg_color(s_voile, lv_color_black(), 0);
    }
    lv_obj_set_style_bg_opa(s_voile, LV_OPA_COVER, 0);
}

// Reveil : le voile reste (transparent, cliquable) jusqu'au relachement, voir voile_retirer_si_libre().
void voile_rendre_transparent() {
    if (s_voile != nullptr) {
        lv_obj_set_style_bg_opa(s_voile, LV_OPA_TRANSP, 0);
    }
}

void voile_retirer_si_libre() {
    if (s_voile != nullptr && s_etat != vei::Etat::Eteint && !doigt_pose()) {
        lv_obj_delete(s_voile);
        s_voile = nullptr;
    }
}

vei::Entree lire_entree() {
    vei::Entree e;
    e.inactif_s = lv_display_get_inactive_time(s_disp) / 1000U;
    e.occupe = s_occupe;
    int minute = 0;
    int jour = 0;
    e.heure_connue = s_heure_locale != nullptr && s_heure_locale(&minute, &jour) && minute >= 0 &&
                     minute < vei::kMinutesParJour && jour >= 0 && jour <= 6;
    e.minute_du_jour = minute;
    e.jour_semaine = jour;
    return e;
}

void passer_a(vei::Etat nouveau, vei::Cause cause, uint32_t inactif_s) {
    const vei::Etat ancien = s_etat;
    s_etat = nouveau;
    if (nouveau == vei::Etat::Eteint) {
        voile_poser();
    } else {
        voile_rendre_transparent();
    }
    appliquer_retroeclairage();
    ESP_LOGI(TAG, "écran %s → %s (cause : %s, sans toucher depuis %u s)", nom_etat(ancien), nom_etat(nouveau),
             nom_cause(cause), (unsigned)inactif_s);
}

void tick(lv_timer_t *) {
    const vei::Entree e = lire_entree();
    const vei::Decision d = vei::decider(s_reglage, e);
    if (d.etat != s_etat) {
        passer_a(d.etat, d.cause, e.inactif_s);
    }
    voile_retirer_si_libre();
}

} // namespace

extern "C" {

esp_err_t tab5_ecran_init(lv_display_t *disp) {
    s_disp = disp;
    s_timer = lv_timer_create(tick, kPeriodeMs, nullptr);
    return s_timer != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t board_set_backlight_percent(int percent) {
    const int niveau = lum::borner_niveau(percent);
    const bool pris = s_disp != nullptr && board_display_lock(0);
    s_niveau_app = niveau;
    appliquer_retroeclairage();
    if (pris) {
        board_display_unlock();
    }
    return ESP_OK;
}

int board_get_backlight_percent(void) { return s_niveau_app; }

uint32_t board_get_backlight_duty_permille(void) {
    const uint32_t maximum = (1U << TAB5_BACKLIGHT_BITS) - 1U;
    return (tab5_backlight_get_duty() * 1000U + maximum / 2U) / maximum;
}

esp_err_t board_sleep_configure(const board_sleep_config_t *cfg) {
    if (cfg == nullptr || cfg->window_count < 0 || cfg->window_count > BOARD_SLEEP_WINDOWS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    vei::Reglage r;
    r.attenuer_apres_s = cfg->dim_after_s;
    r.eteindre_apres_s = cfg->off_after_s;
    r.rendormir_apres_s = cfg->relight_after_s;
    r.niveau_attenue = cfg->dim_level;
    r.nombre_plages = cfg->window_count;
    for (int i = 0; i < cfg->window_count; i++) {
        r.plages[i].debut_min = cfg->windows[i].start_min;
        r.plages[i].fin_min = cfg->windows[i].end_min;
        r.plages[i].jours = cfg->windows[i].days;
    }
    if (!vei::reglage_valide(r)) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool pris = s_disp != nullptr && board_display_lock(0);
    s_reglage = r;
    if (pris) {
        board_display_unlock();
    }
    return ESP_OK;
}

void board_sleep_set_local_time_source(board_local_time_cb_t cb) { s_heure_locale = cb; }

void board_screen_set_busy(bool busy) { s_occupe = busy; }

board_screen_state_t board_screen_get_state(void) {
    switch (s_etat) {
    case vei::Etat::Allume:
        return BOARD_SCREEN_ON;
    case vei::Etat::Attenue:
        return BOARD_SCREEN_DIMMED;
    case vei::Etat::Eteint:
        return BOARD_SCREEN_OFF;
    }
    return BOARD_SCREEN_ON;
}

void board_screen_wake(void) {
    if (s_disp == nullptr) {
        return;
    }
    const bool pris = board_display_lock(0);
    lv_display_trigger_activity(s_disp);
    if (pris) {
        board_display_unlock();
    }
}

} // extern "C"
