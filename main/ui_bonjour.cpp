#include "ui_bonjour.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "palette.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf, EMBED_FILES de main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace ecran {

namespace {

const char *TAG = "ui_bonjour";
const Palette &kCouleurs = kPaletteSombre;

// Textes de l'écran (français, accents compris). La table de traduction du dépôt Tab5 (le texte français est la clé) remplacera
// ces constantes avec le noyau pur, en phase 2.
namespace texte {
constexpr const char *kBonjour = "Bonjour";
constexpr const char *kSousTitre = "Écran autonome";
constexpr const char *kCarte = "Carte : %s, %d × %d";
constexpr const char *kControleur = "Contrôleur d'écran : %s";
constexpr const char *kTactile = "Tactile : %s";
constexpr const char *kDemarrage = "Démarrage n° %u";
constexpr const char *kDemarrageNonGarde = "Démarrage n° %u (compteur non enregistré)";
constexpr const char *kCause = "Dernier redémarrage : %s";
constexpr const char *kDuree = "Carte prête après %d ms";
constexpr const char *kToucherAucun = "Toucher : aucun";
constexpr const char *kToucher = "Toucher : x = %d, y = %d";
constexpr const char *kRedemarrer = "Redémarrer";
} // namespace texte

constexpr int32_t kDiametrePoint = 96;
constexpr int32_t kLargeurTexte = 640;
constexpr int kJournalToutesLesN = 20;

lv_obj_t *g_point = nullptr;
lv_obj_t *g_libelle_toucher = nullptr;
int g_compteur_appui = 0;
int32_t g_x_min = INT32_MAX, g_x_max = INT32_MIN, g_y_min = INT32_MAX, g_y_max = INT32_MIN;

lv_color_t couleur(uint32_t rgb) { return lv_color_hex(rgb); }

lv_obj_t *creer_libelle(lv_obj_t *parent, const lv_font_t *police, uint32_t couleur_rgb) {
    lv_obj_t *libelle = lv_label_create(parent);
    lv_obj_set_width(libelle, kLargeurTexte);
    lv_label_set_long_mode(libelle, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(libelle, police, 0);
    lv_obj_set_style_text_color(libelle, couleur(couleur_rgb), 0);
    lv_obj_set_style_text_align(libelle, LV_TEXT_ALIGN_CENTER, 0);
    return libelle;
}

// Mire de mesure du décalage entre le tactile et l'image (phase 1a) : un cadre sur les bords de l'écran (il montre si l'image
// est coupée ou décalée) et cinq cibles à des positions connues (coins, centre) dont on compare le centre aux appuis du journal.
constexpr int32_t kCoteCible = 64;
constexpr int32_t kEpaisseurTrait = 2;
constexpr int32_t kMargeCible = 60;
constexpr int32_t kEpaisseurCadre = 4;

lv_obj_t *creer_trait(lv_obj_t *parent, int32_t x, int32_t y, int32_t largeur, int32_t hauteur, uint32_t couleur_rgb) {
    lv_obj_t *trait = lv_obj_create(parent);
    lv_obj_remove_style_all(trait);
    lv_obj_remove_flag(trait, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(trait, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(trait, x, y);
    lv_obj_set_size(trait, largeur, hauteur);
    lv_obj_set_style_bg_opa(trait, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(trait, couleur(couleur_rgb), 0);
    return trait;
}

void creer_cible(lv_obj_t *parent, int32_t centre_x, int32_t centre_y) {
    lv_obj_t *cible = lv_obj_create(parent);
    lv_obj_remove_style_all(cible);
    lv_obj_remove_flag(cible, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(cible, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(cible, kCoteCible, kCoteCible);
    lv_obj_set_pos(cible, centre_x - kCoteCible / 2, centre_y - kCoteCible / 2);
    lv_obj_set_style_radius(cible, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_outline_width(cible, kEpaisseurTrait, 0);
    lv_obj_set_style_outline_color(cible, couleur(kCouleurs.accent), 0);
    creer_trait(cible, 0, kCoteCible / 2 - 1, kCoteCible, kEpaisseurTrait, kCouleurs.texte);
    creer_trait(cible, kCoteCible / 2 - 1, 0, kEpaisseurTrait, kCoteCible, kCouleurs.texte);
}

void creer_mire(lv_obj_t *couche, int32_t largeur, int32_t hauteur) {
    lv_obj_t *cadre = lv_obj_create(couche);
    lv_obj_remove_style_all(cadre);
    lv_obj_remove_flag(cadre, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(cadre, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(cadre, 0, 0);
    lv_obj_set_size(cadre, largeur, hauteur);
    lv_obj_set_style_border_width(cadre, kEpaisseurCadre, 0);
    lv_obj_set_style_border_color(cadre, couleur(kCouleurs.accent), 0);

    creer_cible(couche, kMargeCible, kMargeCible);
    creer_cible(couche, largeur - kMargeCible, kMargeCible);
    creer_cible(couche, largeur / 2, hauteur / 2);
    creer_cible(couche, kMargeCible, hauteur - kMargeCible);
    creer_cible(couche, largeur - kMargeCible, hauteur - kMargeCible);
}

void placer_point(int32_t x, int32_t y) { lv_obj_set_pos(g_point, x - kDiametrePoint / 2, y - kDiametrePoint / 2); }

void sur_toucher(lv_event_t *evenement) {
    const lv_event_code_t code = lv_event_get_code(evenement);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) {
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_RELEASED) {
        // Le point reste là où le doigt s'est levé, en couleur "relâché".
        lv_obj_set_style_bg_color(g_point, couleur(kCouleurs.accent_relache), 0);
        ESP_LOGI(TAG, "tactile : relâché en (%d, %d) ; course x [%d..%d], y [%d..%d]", (int)p.x, (int)p.y, (int)g_x_min,
                 (int)g_x_max, (int)g_y_min, (int)g_y_max);
        g_x_min = INT32_MAX;
        g_x_max = INT32_MIN;
        g_y_min = INT32_MAX;
        g_y_max = INT32_MIN;
        return;
    }

    g_x_min = std::min(g_x_min, p.x);
    g_x_max = std::max(g_x_max, p.x);
    g_y_min = std::min(g_y_min, p.y);
    g_y_max = std::max(g_y_max, p.y);
    lv_obj_remove_flag(g_point, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(g_point, couleur(kCouleurs.accent), 0);
    placer_point(p.x, p.y);
    lv_label_set_text_fmt(g_libelle_toucher, texte::kToucher, (int)p.x, (int)p.y);

    if (code == LV_EVENT_PRESSED) {
        g_compteur_appui = 0;
        ESP_LOGI(TAG, "tactile : appui en (%d, %d)", (int)p.x, (int)p.y);
    } else if (++g_compteur_appui % kJournalToutesLesN == 0) {
        ESP_LOGI(TAG, "tactile : doigt en (%d, %d)", (int)p.x, (int)p.y);
    }
}

void sur_bouton_redemarrer(lv_event_t *evenement) {
    if (lv_event_get_code(evenement) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGW(TAG, "redémarrage logiciel demandé par l'écran (esp_rom_software_reset_system)");
    vTaskDelay(pdMS_TO_TICKS(200)); // laisser partir le journal sur l'USB
    board_restart();
}

} // namespace

void ui_bonjour_creer(const BootInfo &boot) {
    const board_info_t *carte = board_get_info();
    const board_boot_timeline_t *jalons = board_get_boot_timeline();

    lv_font_t *police_titre = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 128);
    lv_font_t *police_texte = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 38);
    lv_font_t *police_petite = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 30);
    if (police_titre == nullptr || police_texte == nullptr || police_petite == nullptr) {
        ESP_LOGE(TAG, "police TrueType non créée (TinyTTF)");
        return;
    }

    lv_obj_t *ecran = lv_display_get_screen_active(board_get_display());
    lv_obj_set_style_bg_color(ecran, couleur(kCouleurs.fond), 0);
    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_add_flag(ecran, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE); // un glissement ne doit pas faire défiler l'écran
    lv_obj_set_flex_flow(ecran, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ecran, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(ecran, 90, 0);
    lv_obj_set_style_pad_row(ecran, 22, 0);
    lv_obj_add_event_cb(ecran, sur_toucher, LV_EVENT_ALL, nullptr);

    lv_label_set_text(creer_libelle(ecran, police_titre, kCouleurs.accent), texte::kBonjour);
    lv_label_set_text(creer_libelle(ecran, police_texte, kCouleurs.texte_discret), texte::kSousTitre);

    lv_obj_t *libelle = creer_libelle(ecran, police_texte, kCouleurs.texte);
    lv_obj_set_style_pad_top(libelle, 50, 0);
    lv_label_set_text_fmt(libelle, texte::kControleur, carte->display_controller);

    lv_label_set_text_fmt(creer_libelle(ecran, police_petite, kCouleurs.texte_discret), texte::kTactile,
                          carte->touch_controller);
    lv_label_set_text_fmt(creer_libelle(ecran, police_petite, kCouleurs.texte_discret), texte::kCarte,
                          carte->board_name, (int)carte->width, (int)carte->height);

    libelle = creer_libelle(ecran, police_texte, kCouleurs.texte);
    lv_obj_set_style_pad_top(libelle, 40, 0);
    lv_label_set_text_fmt(libelle, boot.counter_persistent ? texte::kDemarrage : texte::kDemarrageNonGarde,
                          static_cast<unsigned>(boot.boot_count));
    lv_label_set_text_fmt(creer_libelle(ecran, police_petite, kCouleurs.texte_discret), texte::kCause,
                          boot.reset_reason);
    lv_label_set_text_fmt(creer_libelle(ecran, police_petite, kCouleurs.texte_discret), texte::kDuree,
                          (int)jalons->lvgl_ready_ms);

    g_libelle_toucher = creer_libelle(ecran, police_texte, kCouleurs.texte);
    lv_obj_set_style_pad_top(g_libelle_toucher, 60, 0);
    lv_label_set_text(g_libelle_toucher, texte::kToucherAucun);

    lv_obj_t *bouton = lv_button_create(ecran);
    lv_obj_set_size(bouton, 420, 110);
    lv_obj_set_style_bg_color(bouton, couleur(kCouleurs.surface), 0);
    lv_obj_set_style_radius(bouton, 24, 0);
    lv_obj_add_event_cb(bouton, sur_bouton_redemarrer, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *texte_bouton = lv_label_create(bouton);
    lv_label_set_text(texte_bouton, texte::kRedemarrer);
    lv_obj_set_style_text_font(texte_bouton, police_texte, 0);
    lv_obj_set_style_text_color(texte_bouton, couleur(kCouleurs.bouton_texte), 0);
    lv_obj_center(texte_bouton);

    // Couche supérieure de l'affichage : sans remplissage ni mise en page, son origine est le coin de l'écran. Le point et la
    // mire y sont posés en coordonnées d'écran exactes (sur l'écran lui-même, le remplissage haut de 90 px décalait le point).
    lv_obj_t *couche = lv_display_get_layer_top(board_get_display());
    creer_mire(couche, carte->width, carte->height);

    // Le point suit le doigt, à moitié transparent pour laisser voir la cible dessous ; jamais cliquable (les appuis passent
    // à l'écran).
    g_point = lv_obj_create(couche);
    lv_obj_remove_style_all(g_point);
    lv_obj_set_size(g_point, kDiametrePoint, kDiametrePoint);
    lv_obj_set_style_radius(g_point, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_point, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(g_point, couleur(kCouleurs.accent), 0);
    // Un appel par drapeau : en C++ le "|" de deux lv_obj_flag_t rend un int, que LVGL refuse.
    lv_obj_add_flag(g_point, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_point, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g_point, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace ecran
