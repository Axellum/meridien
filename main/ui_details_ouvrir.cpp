// La colle entre la page « Détails » (ui_details.cpp : LVGL pur, rendue aussi sur le PC) et l'appareil : l'écran de la carte, la police
// embarquée, la lecture de la météo, de l'heure, du lieu et du réglage de thème, le minuteur.
//
// La page s'ouvre sur un ÉCRAN À PART (lv_obj_create(nullptr)), comme les réglages et la page des alertes : l'accueil, ses minuteries et
// son contenu restent dessous, intacts. « Fermer » recharge l'écran qui était affiché à l'ouverture et supprime celui-ci. Transitions
// instantanées, sans fondu (AGENTS.md, préférences du produit).
//
// Appelé dans la tâche LVGL, verrou d'affichage déjà pris. RIEN ne l'appelle encore : la roue d'appui long (session meridien-roue-lvgl)
// y branchera « Pages ▸ Détails ».
#include "ui_details.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <new>

#include "board.h"
#include "esp_log.h"
#include "lvgl.h"
#include "src/draw/lv_draw_buf_private.h" // la structure des gestionnaires de tampons (LVGL ne l'ouvre qu'avec LV_USE_PRIVATE_API)

#include "ecran/grandeurs.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/horloge_materielle.hpp"
#include "ecran/i18n.hpp"
#include "ecran/meteo_synthese.hpp"
#include "ecran/visuel_vue.hpp"
#include "horloge.h"
#include "meteo_etat.h"
#include "meteo_interne.h"
#include "meteo_zone.h"
#include "reglages.h"
#include "reseau.h"
#include "vue_depuis_detail.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf, EMBED_FILES de main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace ecran {

namespace {

const char *TAG = "details";

constexpr uint32_t kPeriodeMs = 500; // comme l'accueil : la page suit l'heure et la météo au demi-seconde près

lv_obj_t *g_ecran = nullptr;      // l'écran à part de la page ; nul quand rien n'est ouvert
lv_obj_t *g_precedent = nullptr;  // l'écran qui était affiché à l'ouverture
lv_timer_t *g_minuteur = nullptr; // la mise à jour de 500 ms, le temps de l'ouverture
MeteoDetail *g_detail =
    nullptr; // la météo riche lue à chaque passage (≈ 2 Ko) : en PSRAM le temps de l'ouverture, pas en RAM interne

// Le réglage « thème » de la page de réglages choisit l'habillage, comme pour l'accueil (ecran_principal.cpp : même correspondance,
// copiée ici pour ne pas toucher à un fichier que les autres sessions modifient ; à factorer).
const visuel::ThemePrincipal &theme_du_reglage() {
    switch (reglages_affichage().theme) {
    case reglages::Theme::Clair:
        return visuel::kThemeClair;
    case reglages::Theme::Sombre:
        return visuel::kThemeTableauSombre;
    case reglages::Theme::Auto:
        break;
    }
    return visuel::kThemeCiel;
}

// Les bitmaps de glyphes de TinyTTF vont en PSRAM : indispensable aux chiffres de 76 px de cette page (un tampon A8 par lettre et par
// taille, gardés en cache), sinon la mémoire INTERNE, la seule qui manque, les paie (mesure du 07/11/2026 : ui_reglages_ouvrir.cpp).
// Même gestionnaire que celui de ui_reglages_ouvrir.cpp (global : toutes les polices TinyTTF de l'appareil) ; copié, à factorer.
void *glyphe_en_psram(size_t octets, lv_color_format_t) {
    const size_t avec_marge = octets + LV_DRAW_BUF_ALIGN - 1;
    // lv_malloc va en PSRAM d'abord, la mémoire interne si elle est pleine (main/lvgl_memoire.cpp) : la même place qu'un
    // heap_caps_malloc en PSRAM, mais le bloc est compté avec ceux de LVGL (commande « memoire »), qui le rend par lv_free().
    return lv_malloc(avec_marge);
}

void glyphes_en_psram() {
    static bool fait = false;
    if (!fait) {
        fait = true;
        lv_draw_buf_get_font_handlers()->buf_malloc_cb = glyphe_en_psram;
    }
}

// Ce que la page montre maintenant : la météo (riche d'abord, relevé v1 en repli, comme l'accueil), l'heure (vraie seulement si le
// réseau l'a donnée ou que celle du système est plausible), le fuseau et le lieu choisis, la langue de l'écran. Une grandeur que la
// météo ne donne pas reste « — » (ecran/grandeurs.hpp), jamais inventée.
grandeurs::Page composer_page() {
    const EtatReseau etat = reseau_etat();
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    const bool heure_vraie = rtc::heure_affichable(etat.heure_synchronisee, instant);
    const int64_t maintenant_s = heure_vraie ? instant : 0;

    ZoneMeteo zone = {};
    const bool zone_connue = zone_lire(&zone);
    zone.nom[sizeof zone.nom - 1] = '\0';
    const char *ville = zone_connue ? zone.nom : "";

    visuel::VueMeteo vue = {};
    if (g_detail != nullptr && meteo_detail_lire(g_detail) && g_detail->valide) {
        vue = visuel::vue_depuis_detail(*g_detail, ville);
    } else {
        meteo::Releve releve = {};
        int echecs = 0;
        if (meteo_etat_lire_releve(&releve, &echecs)) {
            vue = visuel::vue_depuis_releve(releve, instant, ville);
            vue.perimee = meteo::est_perime(releve, instant, echecs);
        }
    }

    // Les règles du fuseau du lieu ; si son nom n'est pas dans la table, le décalage fixe de la réponse météo, comme l'accueil.
    heure::Fuseau fuseau = {};
    if (!horloge_fuseau(fuseau)) {
        fuseau = heure::fuseau_fixe(vue.connue ? vue.decalage_utc_s : 0);
    }
    return grandeurs::composer(vue, maintenant_s, fuseau, i18n::langue_courante());
}

// Appelé par LVGL (dans sa tâche, verrou pris) deux fois par seconde : ne redessine que si l'empreinte de la page a changé.
void rafraichir(lv_timer_t *) { ui_details_mettre_a_jour(composer_page()); }

} // namespace

} // namespace ecran

void ui_details_ouvrir() {
    using namespace ecran;
    const board_info_t *carte = board_get_info();
    if (g_ecran != nullptr || carte == nullptr) {
        return; // déjà ouverte : un second appel ne fait rien
    }
    glyphes_en_psram();
    void *place = lv_malloc(sizeof(MeteoDetail));
    if (place == nullptr) {
        ESP_LOGE(TAG, "page Détails non ouverte : mémoire insuffisante (%u o)",
                 static_cast<unsigned>(sizeof(MeteoDetail)));
        return;
    }
    g_detail = new (place) MeteoDetail{};

    lv_obj_t *racine = lv_obj_create(nullptr);
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);
    ContexteDetails ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = carte->width; // taille LOGIQUE (rotation comprise) : 1280 x 720 sur le Tab5, 1024 x 600 sur la 7B
    ctx.hauteur = carte->height;
    ctx.theme = &theme_du_reglage();
    ctx.fermer = ui_details_fermer;
    if (!ui_details_creer(ctx, composer_page())) {
        ESP_LOGE(TAG, "page Détails non créée (police TrueType ou mémoire) : l'écran d'avant reste affiché");
        lv_obj_delete(racine);
        lv_free(g_detail);
        g_detail = nullptr;
        return;
    }
    g_ecran = racine;
    g_precedent = lv_screen_active();
    lv_screen_load(g_ecran);
    g_minuteur = lv_timer_create(rafraichir, kPeriodeMs, nullptr);
    ESP_LOGI(TAG, "page Détails ouverte (%dx%d)", static_cast<int>(ctx.largeur), static_cast<int>(ctx.hauteur));
}

// « Fermer » : l'écran d'avant revient d'un coup. L'écran de la page est supprimé plus tard (lv_obj_delete_async), parce qu'on est encore
// dans le rappel d'un bouton qu'il contient.
void ui_details_fermer() {
    using namespace ecran;
    if (g_ecran == nullptr) {
        return;
    }
    if (g_minuteur != nullptr) {
        lv_timer_delete(g_minuteur);
        g_minuteur = nullptr;
    }
    if (g_precedent != nullptr) {
        lv_screen_load(g_precedent);
    }
    lv_obj_delete_async(g_ecran);
    g_ecran = nullptr;
    g_precedent = nullptr;
    if (g_detail != nullptr) {
        lv_free(g_detail); // MeteoDetail est trivialement destructible
        g_detail = nullptr;
    }
    ESP_LOGI(TAG, "page Détails fermée");
}
