#include "ui_mer_ouvrir.h"

#include <cstdint>
#include <cstring>
#include <ctime>

#include "board.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ecran/heure_fr.hpp"
#include "ecran/horloge_materielle.hpp"
#include "ecran/mer_spots.hpp"
#include "ecran/mer_trame.hpp"
#include "horloge.h"
#include "mer_etat.h"
#include "reglages_mer.h"
#include "reseau.h"
#include "ui_mer.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf) : EMBED_FILES de main/CMakeLists.txt, la même que celle de l'écran principal.
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace ecran {

namespace {

const char *TAG = "mer";

lv_obj_t *g_page = nullptr;   // l'écran de la page, supprimé à « Fermer »
lv_obj_t *g_retour = nullptr; // l'écran qui était affiché quand elle s'est ouverte
const mer::Mer g_vide{};      // « aucune mer » : la page le dit en toutes lettres

// « Fermer » : l'écran d'avant revient d'un coup, la page est supprimée (ses polices sont gardées pour la prochaine ouverture : ui_mer.cpp).
void fermer() {
    if (g_retour != nullptr) {
        lv_screen_load(g_retour);
        g_retour = nullptr;
    }
    if (g_page != nullptr) {
        lv_obj_delete_async(g_page);
        g_page = nullptr;
    }
}

} // namespace

bool ui_mer_ouverte() { return g_page != nullptr; }

bool ui_mer_ouvrir(const visuel::ThemePrincipal &theme) {
    const board_info_t *carte = board_get_info();
    if (g_page != nullptr || carte == nullptr) {
        return false;
    }
    // L'heure des barres, de la marée et du soleil est celle du lieu : sans son fuseau on n'ouvre pas, plutôt que d'écrire l'heure d'un autre.
    heure::Fuseau fuseau;
    if (!horloge_fuseau(fuseau)) {
        ESP_LOGW(TAG, "page mer non ouverte : fuseau du lieu illisible");
        return false;
    }
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    const bool heure_vraie = rtc::heure_affichable(reseau_etat().heure_synchronisee, instant);

    // La mer en mémoire n'est montrée que si elle est celle du spot choisi : jamais la mer d'un autre lieu sous le nom du bon.
    const mer::DemandeMer demande = reglages_mer_demande();
    const MerLecture lecture;
    const bool bonne_mer = lecture.valide() && demande.presente && std::strcmp(lecture.spot(), demande.spot) == 0;

    lv_obj_t *racine = lv_obj_create(nullptr);
    ContexteMer ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = carte->width;
    ctx.hauteur = carte->height;
    ctx.theme = &theme;
    ctx.fermer = fermer;
    DonneesMer donnees;
    donnees.mer = bonne_mer ? &lecture.mer() : &g_vide;
    donnees.maintenant_s = heure_vraie ? instant : 0;
    donnees.decalage_s = horloge_decalage_s(instant);
    if (!ui_mer_creer(ctx, donnees)) {
        ESP_LOGE(TAG, "page mer non créée (police TrueType ou écran trop petit) : l'écran affiché reste");
        lv_obj_delete(racine);
        return false;
    }
    g_retour = lv_screen_active();
    g_page = racine;
    lv_screen_load(racine);
    ESP_LOGI(TAG, "page mer ouverte : %s, heure %s", bonne_mer ? "mer du spot choisi" : "aucune mer pour ce spot",
             heure_vraie ? "vraie" : "inconnue");
    return true;
}

} // namespace ecran
