#include "ui_bonjour.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "ecran/i18n.hpp"

#include "horloge.h"
#include "palette.h"
#include "reseau.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf, EMBED_FILES de main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace ecran {

namespace {

const char *TAG = "ui_bonjour";
const Palette &kCouleurs = kPaletteSombre;

// Textes de l'écran de diagnostic : la clé française de la table commune (i18n/*.yaml, sections « Écran : « Bonjour » » et
// « Écran : diagnostic »), traduite à l'affichage par `T()`. test/host/controle_i18n.py vérifie que chaque constante est une clé.
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
// Heure, date et réseau (ex-main/textes_1b.h, ramenés dans la table le 11/11/2026).
constexpr const char *kHeureAbsente = "-- h --";
constexpr const char *kDateAbsente = "Heure du réseau en attente";
constexpr const char *kWifiReglagesAbsents = "Wi-Fi : réglages absents du firmware";
constexpr const char *kWifiCoprocesseurAbsent = "Wi-Fi : coprocesseur absent";
constexpr const char *kWifiCoprocesseurDemarre = "Wi-Fi : coprocesseur en démarrage";
constexpr const char *kWifiConnexion = "Wi-Fi : connexion en cours";
constexpr const char *kWifiAdresse = "Wi-Fi : connecté, adresse en cours";
constexpr const char *kWifiConnecte = "Wi-Fi : connecté";
constexpr const char *kHeureEnAttente = "Heure : en attente du réseau";
constexpr const char *kHeureRecue = "Heure : reçue du réseau";
} // namespace texte

constexpr int32_t kDiametrePoint = 96;
constexpr int32_t kLargeurTexte = 580; // une colonne : deux colonnes de 600 px dans les 1280 px du paysage
constexpr int32_t kLargeurColonne = 600;
constexpr int kJournalToutesLesN = 20;
constexpr uint32_t kPeriodeRafraichissementMs = 500;
constexpr int kJournalHorlogeToutesLesN = 120; // une minute à 500 ms

// La mire de la phase 1a (cadre et cinq cibles) a servi à mesurer le décalage tactile ; retirée avec le passage en paysage
// (elle était cachée depuis 1b). Le point qui suit le doigt reste, pour voir que le tactile répond.

const char *TAG_HORLOGE = "horloge";

lv_obj_t *g_point = nullptr;
lv_obj_t *g_libelle_toucher = nullptr;
lv_obj_t *g_libelle_heure = nullptr;
lv_obj_t *g_libelle_date = nullptr;
lv_obj_t *g_libelle_wifi = nullptr;
lv_obj_t *g_libelle_source = nullptr;
int g_compteur_appui = 0;
void (*g_appui_long)() = nullptr;
bool g_appui_long_fait = false;
int32_t g_x_min = INT32_MAX, g_x_max = INT32_MIN, g_y_min = INT32_MAX, g_y_max = INT32_MIN;
int g_compteur_rafraichissement = 0;
bool g_heure_deja_journalisee = false;

lv_color_t couleur(uint32_t rgb) { return lv_color_hex(rgb); }

// Le texte dans la langue de l'écran (table commune, repli anglais puis français).
const char *T(const char *francais) { return ecran::i18n::tr_c(francais); }

lv_obj_t *creer_colonne(lv_obj_t *parent) {
    lv_obj_t *colonne = lv_obj_create(parent);
    lv_obj_remove_style_all(colonne);
    lv_obj_remove_flag(colonne, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(colonne, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(colonne, kLargeurColonne, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(colonne, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(colonne, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(colonne, 10, 0);
    return colonne;
}

lv_obj_t *creer_libelle(lv_obj_t *parent, const lv_font_t *police, uint32_t couleur_rgb) {
    lv_obj_t *libelle = lv_label_create(parent);
    lv_obj_set_width(libelle, kLargeurTexte);
    lv_label_set_long_mode(libelle, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(libelle, police, 0);
    lv_obj_set_style_text_color(libelle, couleur(couleur_rgb), 0);
    lv_obj_set_style_text_align(libelle, LV_TEXT_ALIGN_CENTER, 0);
    return libelle;
}

// Ne redessine que si le texte a changé (le minuteur passe deux fois par seconde).
void changer_texte(lv_obj_t *libelle, const char *texte_neuf) {
    const char *actuel = lv_label_get_text(libelle);
    if (actuel == nullptr || std::strcmp(actuel, texte_neuf) != 0) {
        lv_label_set_text(libelle, texte_neuf);
    }
}

void placer_point(int32_t x, int32_t y) { lv_obj_set_pos(g_point, x - kDiametrePoint / 2, y - kDiametrePoint / 2); }

void sur_toucher(lv_event_t *evenement) {
    const lv_event_code_t code = lv_event_get_code(evenement);
    if (code == LV_EVENT_LONG_PRESSED) {
        // Appui long sur le fond de l'écran : retour à l'écran principal (si l'application l'a demandé). Le doigt est encore posé
        // et LVGL continue de lui envoyer les événements après le changement d'écran : le point (couche supérieure, donc
        // visible sur l'autre écran) est caché et ignoré jusqu'au relâchement.
        if (g_appui_long != nullptr) {
            g_appui_long_fait = true;
            lv_obj_add_flag(g_point, LV_OBJ_FLAG_HIDDEN);
            g_appui_long();
        }
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) {
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (code == LV_EVENT_PRESSED) {
        g_appui_long_fait = false;
    }
    if (g_appui_long_fait && code != LV_EVENT_RELEASED) {
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        g_appui_long_fait = false;
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
    lv_label_set_text_fmt(g_libelle_toucher, T(texte::kToucher), (int)p.x, (int)p.y);

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

const char *texte_wifi(const EtatReseau &etat) {
    if (!etat.identifiants_presents) {
        return T(texte::kWifiReglagesAbsents);
    }
    if (etat.coprocesseur_echec) {
        return T(texte::kWifiCoprocesseurAbsent);
    }
    if (!etat.coprocesseur_pret) {
        return T(texte::kWifiCoprocesseurDemarre);
    }
    if (!etat.wifi_connecte) {
        return T(texte::kWifiConnexion);
    }
    return T(etat.ip_obtenue ? texte::kWifiConnecte : texte::kWifiAdresse);
}

// Appelé par LVGL (dans sa tâche, verrou pris) deux fois par seconde : heure, date et état du Wi-Fi.
void rafraichir(lv_timer_t *) {
    const EtatReseau etat = reseau_etat();

    TexteHorloge horloge = {};
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    const bool heure_connue = etat.heure_synchronisee && horloge_formater(instant, horloge);

    // En français, l'heure et la date de horloge_formater (« 14 h 05 », « mercredi 11 novembre 2026 ») ; dans les autres langues,
    // « 14:05 » et la date de la langue (les mêmes fonctions que l'écran principal).
    const char *heure_affichee = horloge.heure;
    const char *date_affichee = horloge.date;
    char heure_langue[8] = {};
    char date_langue[sizeof horloge.date] = {};
    if (heure_connue && ecran::i18n::langue_courante() != ecran::i18n::Langue::Fr) {
        if (horloge_hhmm(instant, heure_langue, sizeof heure_langue))
            heure_affichee = heure_langue;
        if (horloge_date_localisee(instant, ecran::i18n::langue_courante(), date_langue, sizeof date_langue))
            date_affichee = date_langue;
    }
    changer_texte(g_libelle_heure, heure_connue ? heure_affichee : T(texte::kHeureAbsente));
    changer_texte(g_libelle_date, heure_connue ? date_affichee : T(texte::kDateAbsente));
    changer_texte(g_libelle_wifi, texte_wifi(etat));
    changer_texte(g_libelle_source, T(etat.heure_synchronisee ? texte::kHeureRecue : texte::kHeureEnAttente));

    // Journal : le texte exact posé sur l'écran, à la première heure valide puis chaque minute. Le banc compare l'instant UTC
    // à l'horloge du PC (l'écran n'est pas photographié : c'est la trace de ce qui a été affiché).
    if (heure_connue && (!g_heure_deja_journalisee || g_compteur_rafraichissement % kJournalHorlogeToutesLesN == 0)) {
        g_heure_deja_journalisee = true;
        ESP_LOGI(TAG_HORLOGE, "affichage : « %s » / « %s » ; utc=%lld", horloge.heure, horloge.date,
                 static_cast<long long>(instant));
    }
    ++g_compteur_rafraichissement;
}

} // namespace

void ui_bonjour_creer(const BootInfo &boot) {
    const board_info_t *carte = board_get_info();
    const board_boot_timeline_t *jalons = board_get_boot_timeline();

    // « Bonjour » passe de 128 à 72 px en 1b pour faire de la place à l'heure (128 px) : sinon l'écran déborde de 1280 px.
    lv_font_t *police_titre = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 72);
    lv_font_t *police_heure = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 128);
    lv_font_t *police_date = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 44);
    lv_font_t *police_texte = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 38);
    lv_font_t *police_petite = lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, 30);
    if (police_titre == nullptr || police_heure == nullptr || police_date == nullptr || police_texte == nullptr ||
        police_petite == nullptr) {
        ESP_LOGE(TAG, "police TrueType non créée (TinyTTF)");
        return;
    }

    lv_obj_t *ecran = lv_display_get_screen_active(board_get_display());
    lv_obj_set_style_bg_color(ecran, couleur(kCouleurs.fond), 0);
    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_add_flag(ecran, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE); // un glissement ne doit pas faire défiler l'écran
    // Paysage (1280 x 720) : deux colonnes côte à côte. À gauche l'heure, la date et le réseau ; à droite l'identité de la
    // carte, le démarrage et le tactile.
    lv_obj_set_flex_flow(ecran, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ecran, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(ecran, 24, 0);
    lv_obj_add_event_cb(ecran, sur_toucher, LV_EVENT_ALL, nullptr);
    lv_obj_t *colonne_gauche = creer_colonne(ecran);
    lv_obj_t *colonne_droite = creer_colonne(ecran);

    lv_label_set_text(creer_libelle(colonne_gauche, police_titre, kCouleurs.accent), T(texte::kBonjour));
    lv_label_set_text(creer_libelle(colonne_gauche, police_texte, kCouleurs.texte_discret), T(texte::kSousTitre));

    // Phase 1b : l'heure et la date locales (reçues du réseau), puis l'état du Wi-Fi. Rafraîchis par le minuteur de fin de fonction.
    g_libelle_heure = creer_libelle(colonne_gauche, police_heure, kCouleurs.accent);
    lv_obj_set_style_pad_top(g_libelle_heure, 20, 0);
    lv_label_set_text(g_libelle_heure, T(texte::kHeureAbsente));
    g_libelle_date = creer_libelle(colonne_gauche, police_date, kCouleurs.texte);
    lv_label_set_text(g_libelle_date, T(texte::kDateAbsente));
    g_libelle_wifi = creer_libelle(colonne_gauche, police_petite, kCouleurs.texte_discret);
    lv_label_set_text(g_libelle_wifi, T(texte::kWifiCoprocesseurDemarre));
    g_libelle_source = creer_libelle(colonne_gauche, police_petite, kCouleurs.texte_discret);
    lv_label_set_text(g_libelle_source, T(texte::kHeureEnAttente));

    lv_obj_t *libelle = creer_libelle(colonne_droite, police_texte, kCouleurs.texte);
    lv_obj_set_style_pad_top(libelle, 30, 0);
    lv_label_set_text_fmt(libelle, T(texte::kControleur), carte->display_controller);

    lv_label_set_text_fmt(creer_libelle(colonne_droite, police_petite, kCouleurs.texte_discret), T(texte::kTactile),
                          carte->touch_controller);
    lv_label_set_text_fmt(creer_libelle(colonne_droite, police_petite, kCouleurs.texte_discret), T(texte::kCarte),
                          carte->board_name, (int)carte->width, (int)carte->height);

    libelle = creer_libelle(colonne_droite, police_texte, kCouleurs.texte);
    lv_obj_set_style_pad_top(libelle, 30, 0);
    lv_label_set_text_fmt(libelle, T(boot.counter_persistent ? texte::kDemarrage : texte::kDemarrageNonGarde),
                          static_cast<unsigned>(boot.boot_count));
    lv_label_set_text_fmt(creer_libelle(colonne_droite, police_petite, kCouleurs.texte_discret), T(texte::kCause),
                          T(boot.reset_reason));
    lv_label_set_text_fmt(creer_libelle(colonne_droite, police_petite, kCouleurs.texte_discret), T(texte::kDuree),
                          (int)jalons->lvgl_ready_ms);

    g_libelle_toucher = creer_libelle(colonne_droite, police_texte, kCouleurs.texte);
    lv_obj_set_style_pad_top(g_libelle_toucher, 40, 0);
    lv_label_set_text(g_libelle_toucher, T(texte::kToucherAucun));

    lv_obj_t *bouton = lv_button_create(colonne_droite);
    lv_obj_set_size(bouton, 420, 110);
    lv_obj_set_style_bg_color(bouton, couleur(kCouleurs.surface), 0);
    lv_obj_set_style_radius(bouton, 24, 0);
    lv_obj_add_event_cb(bouton, sur_bouton_redemarrer, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *texte_bouton = lv_label_create(bouton);
    lv_label_set_text(texte_bouton, T(texte::kRedemarrer));
    lv_obj_set_style_text_font(texte_bouton, police_texte, 0);
    lv_obj_set_style_text_color(texte_bouton, couleur(kCouleurs.bouton_texte), 0);
    lv_obj_center(texte_bouton);

    // Couche supérieure de l'affichage : sans remplissage ni mise en page, son origine est le coin de l'écran. Le point et la
    // mire y sont posés en coordonnées d'écran exactes (sur l'écran lui-même, le remplissage haut de 90 px décalait le point).
    lv_obj_t *couche = lv_display_get_layer_top(board_get_display());
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

    lv_timer_create(rafraichir, kPeriodeRafraichissementMs, nullptr);
}

void ui_bonjour_sur_appui_long(void (*rappel)()) { g_appui_long = rappel; }

} // namespace ecran
