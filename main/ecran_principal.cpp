#include "ecran_principal.h"

#include <cstdint>
#include <cstring>
#include <ctime>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "horloge.h"
#include "meteo_etat.h"
#include "reseau.h"
#include "textes_principal.h"
#include "ui_bonjour.h"
#include "ui_principal.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf, EMBED_FILES de main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace ecran {

namespace {

const char *TAG = "principal";

constexpr uint32_t kPeriodeMs = 500;
constexpr int kJournalToutesLesN = 120; // une minute à 500 ms

lv_obj_t *g_ecran_principal = nullptr;
lv_obj_t *g_ecran_diagnostic = nullptr;
int g_compteur = 0;
bool g_heure_journalisee = false;

void vers_diagnostic() {
    if (g_ecran_diagnostic != nullptr) {
        lv_screen_load(g_ecran_diagnostic);
    }
}

void vers_principal() {
    if (g_ecran_principal != nullptr) {
        lv_screen_load(g_ecran_principal);
    }
}

// Appelé par LVGL (dans sa tâche, verrou pris) deux fois par seconde.
void rafraichir(lv_timer_t *) {
    const EtatReseau etat = reseau_etat();

    EntreesPrincipal entrees = {};
    TexteHorloge horloge = {};
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    entrees.heure_connue = etat.heure_synchronisee && horloge_formater(instant, horloge) &&
                           horloge_hhmm(instant, entrees.heure, sizeof entrees.heure);
    entrees.date = horloge.date;
    entrees.date_attente = texte_principal::kDateAttente;

    if (!meteo_etat_lire(&entrees.meteo)) {
        entrees.meteo = MeteoEtat{}; // valide = faux : le bloc dit « météo indisponible »
    }
#ifdef MERIDIEN_METEO_BOUCHON
    entrees.meteo_exemple = true; // valeurs d'exemple du bouchon (main/bouchons/) : l'écran le dit
#endif

    ui_principal_mettre_a_jour(entrees);

    // Trace de ce qui est affiché : à la première heure valide puis chaque minute (l'écran n'est pas photographié).
    if (entrees.heure_connue && (!g_heure_journalisee || g_compteur % kJournalToutesLesN == 0)) {
        g_heure_journalisee = true;
        ESP_LOGI(TAG, "affichage : « %s » / « %s » ; météo %s ; utc=%lld", entrees.heure, entrees.date,
                 entrees.meteo.valide ? (entrees.meteo.perime ? "périmée" : "à jour") : "indisponible",
                 static_cast<long long>(instant));
    }
    ++g_compteur;
}

} // namespace

void ecran_principal_creer(const BootInfo &boot) {
    const board_info_t *carte = board_get_info();
    lv_display_t *affichage = board_get_display();

    // Écran de diagnostic : l'écran « Bonjour » des phases 1a et 1b (cause du dernier redémarrage, état du Wi-Fi, point tactile,
    // bouton Redémarrer). Il garde l'écran actif par défaut ; l'écran principal est créé à part puis chargé.
    ui_bonjour_creer(boot);
    g_ecran_diagnostic = lv_display_get_screen_active(affichage);
    ui_bonjour_sur_appui_long(vers_principal);

    g_ecran_principal = lv_obj_create(nullptr);
    const size_t libre_interne_avant = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (!ui_principal_creer(g_ecran_principal, roboto_700_start, static_cast<size_t>(roboto_700_end - roboto_700_start),
                            carte->width, carte->height)) {
        ESP_LOGE(TAG, "écran principal non créé (police TrueType) : l'écran de diagnostic reste affiché");
        lv_obj_delete(g_ecran_principal);
        g_ecran_principal = nullptr; // l'appui long de l'écran de diagnostic ne charge alors rien
        return;
    }
    ui_principal_sur_appui_long(vers_diagnostic);

    // Une première mise à jour avant d'afficher : jamais d'écran vide, même une demi-seconde.
    rafraichir(nullptr);
    lv_screen_load(g_ecran_principal);
    lv_timer_create(rafraichir, kPeriodeMs, nullptr);

    ESP_LOGI(
        TAG,
        "écran principal %dx%d chargé ; appui long (400 ms) : écran de diagnostic ; mémoire interne libre %u -> %u o",
        static_cast<int>(carte->width), static_cast<int>(carte->height), static_cast<unsigned>(libre_interne_avant),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
}

} // namespace ecran
