#include "ecran_principal.h"

#include <cstdint>
#include <cstring>
#include <ctime>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ecran/horloge_materielle.hpp"
#include "ecran/i18n.hpp"
#include "ecran/meteo_synthese.hpp"
#include "ecran/vigie_bandeau.hpp"
#include "ecran/vigie_vue.hpp"
#include "ecran/visuel_vue.hpp"
#include "horloge.h"
#include "meteo_etat.h"
#include "meteo_interne.h"
#include "meteo_zone.h"
#include "reglages.h"
#include "reglages_alertes.h"
#include "reseau.h"
#include "textes_principal.h"
#include "ui_alertes.h"
#include "ui_bonjour.h"
#include "ui_principal.h"
#include "ui_reglages.h"
#include "ui_roue.h"
#include "vigie_etat.h"
#include "vue_depuis_detail.h"

// Police Roboto Bold embarquée (main/assets/roboto_700.ttf) et jeu d'icônes météo (main/assets/icones_meteo.bin, lu par
// ui_meteo_icones.cpp) : EMBED_FILES de main/CMakeLists.txt.
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");
// Sous-ensemble de la police d'icônes Material Design Icons pour la roue (main/assets/mdi_roue.ttf, target_add_binary_data).
extern const uint8_t mdi_roue_start[] asm("_binary_mdi_roue_ttf_start");
extern const uint8_t mdi_roue_end[] asm("_binary_mdi_roue_ttf_end");

namespace ecran {

namespace {

const char *TAG = "principal";

constexpr uint32_t kPeriodeMs = 500;
constexpr int kJournalToutesLesN = 120; // une minute à 500 ms

lv_obj_t *g_ecran_principal = nullptr;
lv_obj_t *g_ecran_diagnostic = nullptr;
lv_obj_t *g_diag_retour = nullptr; // l'écran d'où le diagnostic a été ouvert (les réglages), pour y revenir
int g_compteur = 0;
bool g_heure_journalisee = false;
char g_ville[sizeof(ZoneMeteo::nom)] = {};

void vers_principal() {
    if (g_ecran_principal != nullptr) {
        lv_screen_load(g_ecran_principal);
    }
}

// Appui long sur l'écran de diagnostic : on rend l'écran d'où l'on venait (les réglages, ouverts dessous et intacts), sinon l'écran
// principal. Sans cela le diagnostic rendrait l'écran principal et laisserait les réglages « déjà ouverts » sans écran pour les fermer.
void revenir_du_diagnostic() {
    lv_obj_t *cible = (g_diag_retour != nullptr && lv_obj_is_valid(g_diag_retour)) ? g_diag_retour : g_ecran_principal;
    g_diag_retour = nullptr;
    if (cible != nullptr) {
        lv_screen_load(cible);
    }
}

lv_obj_t *g_ecran_alertes =
    nullptr; // la page des alertes officielles (ADR-0009), créée à l'appui sur la pastille, supprimée à « Fermer »

// Le réglage « thème » de la page de réglages (reglages.h) choisit l'habillage de l'écran principal : « auto » = ciel (suit l'heure
// et le temps), « clair » = clair, « sombre » = tableau sombre. Le thème « horloge » (heure géante) n'a pas de valeur dans ce
// réglage : il faudrait une quatrième valeur au registre des réglages (session reglages-ecran), donc il n'est pas atteignable.
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

// « Fermer » de la page des alertes : l'écran principal revient d'un coup, la page est supprimée (ses polices sont gardées pour la
// prochaine ouverture : ui_alertes.cpp).
void fermer_alertes() {
    vers_principal();
    if (g_ecran_alertes != nullptr) {
        lv_obj_delete_async(g_ecran_alertes);
        g_ecran_alertes = nullptr;
    }
}

// Appui sur la pastille de l'alerte officielle : la page des alertes, bâtie sur la réponse de Vigie au moment de l'appui (une image :
// elle ne se met pas à jour si une réponse plus récente arrive pendant qu'elle est ouverte).
void ouvrir_alertes() {
    const board_info_t *carte = board_get_info();
    if (g_ecran_alertes != nullptr || g_ecran_principal == nullptr || carte == nullptr) {
        return;
    }
    const reglages::Alertes ra = reglages_alertes();
    const VigieLecture lecture;
    if (!lecture.valide()) {
        return; // la réponse a été oubliée entre le dernier dessin et l'appui : la pastille disparaît au prochain passage
    }
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    const vigie::Vue vue = vigie::composer(lecture.alertes(), instant, lecture.recu_s(), ra.niveau_min);
    lv_obj_t *ecran = lv_obj_create(nullptr);
    ContexteAlertes ctx;
    ctx.racine = ecran;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = carte->width;
    ctx.hauteur = carte->height;
    ctx.theme = &theme_du_reglage();
    ctx.fermer = fermer_alertes;
    ctx.heure_locale = horloge_heure_locale;
    DonneesAlertes donnees;
    donnees.alertes = &lecture.alertes();
    donnees.vue = &vue;
    donnees.recu_s = lecture.recu_s();
    if (!ui_alertes_creer(ctx, donnees)) {
        ESP_LOGE(TAG, "page des alertes non créée (police TrueType) : l'écran principal reste affiché");
        lv_obj_delete(ecran);
        return;
    }
    g_ecran_alertes = ecran;
    lv_screen_load(ecran);
    ESP_LOGI(TAG, "page des alertes ouverte : %u alerte(s) en vigueur", static_cast<unsigned>(vue.nombre));
}

// La page des alertes existe-t-elle à cet instant ? Comme la pastille : alertes activées, heure sûre, une réponse de Vigie en mémoire.
bool alertes_atteignables() {
    if (!reglages_alertes().actives) {
        return false;
    }
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    if (!rtc::heure_affichable(reseau_etat().heure_synchronisee, instant)) {
        return false;
    }
    const VigieLecture lecture;
    return lecture.valide();
}

// LE PREMIER ANNEAU de la roue d'appui long (ADR-0021) : « Réglages » et, quand la page des alertes existe, « Alertes ». Rien d'autre
// au premier anneau (ni Profil, ni Affichage, ni Assistant). Pour une entrée de plus, par exemple « Détails » (ui_details_ouvrir(), session
// meridien-details) : UNE ligne ici, et son glyphe dans tools/icones/roue_glyphes.json (puis régénérer, voir docs/outils.md).
// Les chaînes sont des littéraux ou la table de traduction : elles vivent aussi longtemps que la roue est ouverte.
int composer_entrees(EntreeRoue *sortie) {
    int n = 0;
    sortie[n++] = {texte_principal::t(texte_principal::kReglages), ICONE_REGLAGES, ui_reglages_ouvrir, nullptr, 0};
    if (alertes_atteignables()) {
        sortie[n++] = {visuel::texte_c("Alertes"), ICONE_ALERTES, ouvrir_alertes, nullptr, 0};
    }
    return n;
}

// Appui long sur l'écran principal : la roue s'ouvre au point d'appui (x, y), en pixels de l'écran.
void ouvrir_roue(int32_t x, int32_t y) {
    const board_info_t *carte = board_get_info();
    if (carte == nullptr || g_ecran_principal == nullptr || ui_roue_ouverte()) {
        return;
    }
    EntreeRoue entrees[ENTREES_ROUE_MAX] = {};
    const int n = composer_entrees(entrees);
    ContexteRoue ctx;
    ctx.couche = lv_display_get_layer_top(board_get_display());
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<size_t>(roboto_700_end - roboto_700_start);
    ctx.mdi = mdi_roue_start;
    ctx.mdi_taille = static_cast<size_t>(mdi_roue_end - mdi_roue_start);
    ctx.largeur = carte->width;
    ctx.hauteur = carte->height;
    ctx.theme = &theme_du_reglage();
    if (!ui_roue_ouvrir(ctx, entrees, n, x, y)) {
        // Les réglages doivent rester atteignables même si la roue ne peut pas se construire (police ou mémoire) : on les ouvre tout de suite.
        ESP_LOGE(TAG, "roue non ouverte en (%d, %d) : les réglages s'ouvrent directement", static_cast<int>(x),
                 static_cast<int>(y));
        ui_reglages_ouvrir();
        return;
    }
    ESP_LOGI(TAG, "roue ouverte en (%d, %d) : %d entrée(s)", static_cast<int>(x), static_cast<int>(y), n);
}

// Appelé par LVGL (dans sa tâche, verrou pris) deux fois par seconde.
void rafraichir(lv_timer_t *) {
    ui_principal_changer_theme(theme_du_reglage()); // rien si le thème n'a pas changé depuis la page de réglages
    ui_roue_changer_theme(theme_du_reglage());      // la roue, si elle est ouverte, suit le thème
    const EtatReseau etat = reseau_etat();

    EntreesPrincipal entrees = {};
    const int64_t instant = static_cast<int64_t>(std::time(nullptr));
    // L'heure est vraie dès que le réseau l'a donnée OU que celle du système est plausible (reprise de l'horloge matérielle au
    // démarrage, ou gardée à travers un redémarrage logiciel) : l'écran n'attend plus SNTP (≈ 13 s) pour l'heure et le ciel.
    const bool heure_vraie = rtc::heure_affichable(etat.heure_synchronisee, instant);
    entrees.maintenant_s = heure_vraie ? instant : 0;
    // Le LIEU choisi (ADR-0012) : relu à chaque passage, de sorte qu'une autre ville choisie sur l'écran ou sur la page web change
    // l'heure, la date, les colonnes « heure par heure » et le ciel au passage suivant (500 ms), sans notification. zone_lire copie la
    // zone gardée en RAM par meteo_zone.cpp (la NVS n'est lue qu'au premier appel).
    ZoneMeteo zone = {};
    const bool zone_connue = zone_lire(&zone);
    entrees.fuseau_connu = horloge_fuseau(entrees.fuseau);
    entrees.position_connue = zone_connue;
    entrees.latitude_deg = zone.latitude;
    entrees.longitude_deg = zone.longitude;
    entrees.heure_connue = heure_vraie && horloge_hhmm(instant, entrees.heure, sizeof entrees.heure);
    char date[48] = {};
    if (entrees.heure_connue && !horloge_date_localisee(instant, i18n::langue_courante(), date, sizeof date)) {
        entrees.heure_connue = false;
    }
    entrees.date = date; // copiée par l'écran pendant l'appel
    entrees.date_attente = texte_principal::kDateAttente;

    // La ville : celle de la zone enregistrée, lue plus haut.
    if (zone_connue) {
        std::memcpy(g_ville, zone.nom, sizeof g_ville);
        g_ville[sizeof g_ville - 1] = '\0';
    }

    // La météo : d'abord la météo riche (contrat v2 : courant complet, 24 heures, 7 jours, pluie proche, tendance de pression),
    // sinon le relevé v1 (repli tant que la v2 n'est pas lue : cache absent ou première requête pas encore faite).
    // `detail` pèse ≈ 2 Ko : static plutôt que la pile de la tâche LVGL, qui est seule à passer ici.
    static MeteoDetail detail;
    if (meteo_detail_lire(&detail) && detail.valide) {
        entrees.meteo = visuel::vue_depuis_detail(detail, g_ville);
    } else {
        meteo::Releve releve = {};
        int echecs = 0;
        if (meteo_etat_lire_releve(&releve, &echecs)) {
            entrees.meteo = visuel::vue_depuis_releve(releve, instant, g_ville);
            entrees.meteo.perimee = meteo::est_perime(releve, instant, echecs);
        }
    }
#ifdef MERIDIEN_METEO_BOUCHON
    entrees.meteo.exemple = true; // valeurs d'exemple du bouchon (main/bouchons/) : l'écran le dit
#endif

    // L'alerte officielle de Vigie (ADR-0009) : la dernière réponse lue, ce qui est en vigueur à cet instant, la pastille. Aucune
    // pastille quand les alertes sont désactivées, que l'heure n'est pas sûre (on ne sait pas ce qui est « en vigueur »), qu'aucune
    // réponse n'est en mémoire ou que rien n'est en vigueur : jamais de bandeau permanent.
    const reglages::Alertes alertes = reglages_alertes();
    if (alertes.actives && heure_vraie) {
        const VigieLecture lecture;
        if (lecture.valide()) {
            const vigie::Vue vue = vigie::composer(lecture.alertes(), instant, lecture.recu_s(), alertes.niveau_min);
            entrees.bandeau = vigie::composer_bandeau(lecture.alertes(), vue, i18n::langue_courante());
        }
    }

    ui_principal_mettre_a_jour(entrees);

    // Trace de ce qui est affiché : à la première heure valide puis chaque minute (l'écran n'est pas photographié).
    if (entrees.heure_connue && (!g_heure_journalisee || g_compteur % kJournalToutesLesN == 0)) {
        g_heure_journalisee = true;
        ESP_LOGI(TAG, "affichage : « %s » / « %s » ; fuseau %s ; météo %s ; utc=%lld", entrees.heure, entrees.date,
                 entrees.fuseau_connu ? zone.fuseau : "inconnu",
                 entrees.meteo.connue ? (entrees.meteo.perimee ? "périmée" : "à jour") : "indisponible",
                 static_cast<long long>(instant));
    }
    ++g_compteur;
}

} // namespace

void ecran_principal_ouvrir_diagnostic() {
    if (g_ecran_diagnostic == nullptr || lv_screen_active() == g_ecran_diagnostic) {
        return;
    }
    g_diag_retour = lv_screen_active(); // les réglages, d'où la ligne « Diagnostic » a été touchée
    lv_screen_load(g_ecran_diagnostic);
}

void ecran_principal_creer(const BootInfo &boot) {
    const board_info_t *carte = board_get_info();
    lv_display_t *affichage = board_get_display();

    // Écran de diagnostic : l'écran « Bonjour » des phases 1a et 1b (cause du dernier redémarrage, état du Wi-Fi, point tactile,
    // bouton Redémarrer). Il garde l'écran actif par défaut ; l'écran principal est créé à part puis chargé. On l'atteint par
    // Réglages > Appareil > Diagnostic (ecran_principal_ouvrir_diagnostic) ; son appui long rend l'écran d'où l'on venait.
    ui_bonjour_creer(boot);
    g_ecran_diagnostic = lv_display_get_screen_active(affichage);
    ui_bonjour_sur_appui_long(revenir_du_diagnostic);

    g_ecran_principal = lv_obj_create(nullptr);
    const size_t libre_interne_avant = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const RessourcesPrincipal ressources = {roboto_700_start, static_cast<size_t>(roboto_700_end - roboto_700_start)};
    if (!ui_principal_creer(g_ecran_principal, ressources, carte->width, carte->height, theme_du_reglage())) {
        ESP_LOGE(TAG, "écran principal non créé (police TrueType) : l'écran de diagnostic reste affiché");
        lv_obj_delete(g_ecran_principal);
        g_ecran_principal = nullptr; // l'appui long de l'écran de diagnostic ne charge alors rien
        return;
    }
    ui_principal_sur_appui_long(
        ouvrir_roue); // ADR-0021 : l'appui long ouvre la roue, d'où l'on atteint les réglages (ADR-0007)
    ui_principal_sur_alertes(ouvrir_alertes); // ADR-0009 : la pastille de l'alerte officielle ouvre la page des alertes

    // Une première mise à jour avant d'afficher : jamais d'écran vide, même une demi-seconde.
    rafraichir(nullptr);
    lv_screen_load(g_ecran_principal);
    // Carte neuve ou redémarrage au milieu de l'installation guidée : elle reprend ici, par-dessus l'écran principal (demande de
    // reglages-ecran ; sans cet appel les réglages marchent mais l'installation ne reprend pas au démarrage).
    ui_installation_ouvrir_si_necessaire();
    lv_timer_create(rafraichir, kPeriodeMs, nullptr);

    const CoutEcran cout = ui_principal_cout();
    const CoutRoue cout_roue = ui_roue_cout();
    ESP_LOGI(TAG,
             "écran principal %dx%d chargé (%d objets LVGL, %d polices) ; appui long (400 ms) : roue (1 objet LVGL "
             "tant qu'elle est "
             "ouverte, %u o de RAM statique) ; mémoire interne libre %u -> %u o",
             static_cast<int>(carte->width), static_cast<int>(carte->height), cout.objets, cout.polices,
             static_cast<unsigned>(cout_roue.ram_statique), static_cast<unsigned>(libre_interne_avant),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
}

} // namespace ecran
