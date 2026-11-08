// Scène « roue » : la roue d'appui long (main/ui_roue.cpp, ADR-0021) ouverte par-dessus l'écran principal (main/ui_principal.cpp), tous
// deux compilés tels quels. Le premier anneau porte « Réglages », « Détails » et « Alertes » comme sur l'appareil ; les variantes « famille-* »
// ajoutent une entrée de DÉMONSTRATION qui déplie un second anneau (choix inventés, glyphes d'emprunt : le firmware n'a pas encore de
// famille). La roue s'ouvre à un point de l'écran, exprimé en millièmes de la largeur et de la hauteur : le centre, près d'un coin,
// près d'un bord. Les variantes « parcours-* » rejouent des gestes et s'arrêtent en ECHEC (code de sortie non nul) si l'effet n'est
// pas celui attendu : c'est ainsi que « les réglages sont toujours atteignables » est vérifié sur le PC (pas sur la carte).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_meteo_icones.cpp main/horloge.cpp main/ui_roue.cpp main/ui_details.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr
// rendu-tailles: 1280x720 1024x600
// rendu-sequence: roue
//
// Ce n'est PAS une photo de l'écran : rendu LVGL logiciel, pas de dalle, pas de doigt réel. La 7B n'a pas été essayée : le rendu à
// 1024x600 n'est qu'une simulation de sa taille.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "banc.h"
#include "ecran/grandeurs.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/visuel_textes.hpp"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_details.h"
#include "ui_principal.h"
#include "ui_roue.h"

// Les polices du firmware (symboles posés par le banc, comme EMBED_FILES et target_add_binary_data dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");
extern const uint8_t mdi_roue_start[] asm("_binary_mdi_roue_ttf_start");
extern const uint8_t mdi_roue_end[] asm("_binary_mdi_roue_ttf_end");

namespace {

using namespace donnees;
using ecran::ChoixRoue;
using ecran::EntreeRoue;

const char *g_variante = "";

[[noreturn]] void echec(const char *quoi) {
    std::fprintf(stderr, "ECHEC de la scène roue « %s » : %s\n", g_variante, quoi);
    std::exit(1);
}

void exiger(bool condition, const char *quoi) {
    if (!condition) {
        echec(quoi);
    }
}

void tourner() {
    for (int i = 0; i < 3; ++i) {
        for (lv_timer_t *t = lv_timer_get_next(nullptr); t != nullptr; t = lv_timer_get_next(t)) {
            lv_timer_ready(t);
        }
        lv_timer_handler();
    }
}

// Ce que les actions font ici : elles se comptent.
int g_reglages = 0;
int g_alertes = 0;
int g_choix = 0;
int g_details = 0;
void compter_reglages() { ++g_reglages; }
void compter_alertes() { ++g_alertes; }
void compter_choix() { ++g_choix; }

// Les choix de la famille de démonstration : des mots du produit (Profil, Affichage...) posés sur des glyphes d'emprunt, pour juger la
// place et le rendu d'un second anneau plein (huit choix). Textes en français seulement : le rendu juge la forme, pas la traduction.
const ChoixRoue FAMILLE_HUIT[8] = {
    {"Profil", ecran::ICONE_MOYEU, compter_choix},        {"Affichage", ecran::ICONE_REGLAGES, compter_choix},
    {"Assistant", ecran::ICONE_ALERTES, compter_choix},   {"Écran", ecran::ICONE_MOYEU, compter_choix},
    {"Luminosité", ecran::ICONE_REGLAGES, compter_choix}, {"Langue", ecran::ICONE_ALERTES, compter_choix},
    {"Réseau", ecran::ICONE_MOYEU, compter_choix},        {"À propos", ecran::ICONE_REGLAGES, compter_choix},
};

lv_obj_t *g_principal = nullptr;
const banc::Contexte *g_contexte = nullptr;

// « Détails » : ce que fait ui_details_ouvrir() (main/ui_details_ouvrir.cpp, la colle de l'appareil, qui ne tourne PAS sur le PC) : la
// page est construite sur un écran neuf, l'écran actif est gardé comme « précédent », la page est chargée d'un coup ; « Fermer » recharge
// le précédent et supprime la page plus tard (lv_obj_delete_async). La page elle-même (main/ui_details.cpp) est celle de l'appareil.
lv_obj_t *g_page_details = nullptr;
lv_obj_t *g_avant_details = nullptr;

void fermer_details() {
    if (g_page_details == nullptr) {
        return;
    }
    lv_screen_load(g_avant_details);
    lv_obj_delete_async(g_page_details);
    g_page_details = nullptr;
    g_avant_details = nullptr;
}

void ouvrir_details() {
    ++g_details;
    if (g_page_details != nullptr || g_contexte == nullptr) {
        return; // déjà ouverte : un second appel ne fait rien (comme ui_details_ouvrir)
    }
    const banc::Contexte &c = *g_contexte;
    ecran::heure::Fuseau fuseau{};
    ecran::horloge_fuseau(fuseau); // la zone posée par le banc (Lyon, Europe/Paris)
    const ecran::grandeurs::Page page =
        ecran::grandeurs::composer(vue_esquisse(c.instant_utc_s), c.instant_utc_s, fuseau, c.langue);
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteDetails ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = c.largeur;
    ctx.hauteur = c.hauteur;
    ctx.theme = &ecran::visuel::kThemeCiel;
    ctx.fermer = fermer_details;
    if (!ecran::ui_details_creer(ctx, page)) {
        echec("la page Détails n'a pas pu être créée (police)");
    }
    g_page_details = racine;
    g_avant_details = lv_screen_active();
    lv_screen_load(racine);
}

// La table du premier anneau de l'appareil (ecran_principal.cpp, composer_entrees) puis, si demandé, la famille de démonstration.
int composer(EntreeRoue *sortie, bool alertes, int choix_demo) {
    int n = 0;
    sortie[n++] = {ecran::visuel::texte_c("Réglages"), ecran::ICONE_REGLAGES, compter_reglages, nullptr, 0};
    sortie[n++] = {ecran::visuel::texte_c("Détails"), ecran::ICONE_DETAILS, ouvrir_details, nullptr, 0};
    if (alertes) {
        sortie[n++] = {ecran::visuel::texte_c("Alertes"), ecran::ICONE_ALERTES, compter_alertes, nullptr, 0};
    }
    if (choix_demo > 0) {
        sortie[n++] = {"Affichage", ecran::ICONE_MOYEU, nullptr, FAMILLE_HUIT, choix_demo};
    }
    return n;
}

void afficher_principal(const banc::Contexte &c) {
    g_contexte = &c;
    g_principal = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(g_principal, ressources, c.largeur, c.hauteur)) {
        echec("l'écran principal n'a pas pu être créé (police)");
    }
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau = alerte_vent(c.instant_utc_s);
    ecran::ui_principal_mettre_a_jour(e);
    lv_screen_load(g_principal);
}

ecran::ContexteRoue contexte_roue(const banc::Contexte &c) {
    ecran::ContexteRoue ctx;
    ctx.couche = lv_display_get_layer_top(c.affichage);
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.mdi = mdi_roue_start;
    ctx.mdi_taille = static_cast<std::size_t>(mdi_roue_end - mdi_roue_start);
    ctx.largeur = c.largeur;
    ctx.hauteur = c.hauteur;
    ctx.theme = &ecran::visuel::kThemeCiel;
    return ctx;
}

// Ouvre la roue au point (fx, fy) en millièmes de l'écran. `choix_demo` > 0 : ajoute la famille de démonstration et la déplie.
void ouvrir_la_roue(const banc::Contexte &c, int fx, int fy, bool alertes, int choix_demo) {
    EntreeRoue entrees_roue[ecran::ENTREES_ROUE_MAX] = {};
    const int n = composer(entrees_roue, alertes, choix_demo);
    const int32_t x = c.largeur * fx / 1000;
    const int32_t y = c.hauteur * fy / 1000;
    exiger(ecran::ui_roue_ouvrir(contexte_roue(c), entrees_roue, n, x, y), "la roue n'a pas pu s'ouvrir");
    if (choix_demo > 0) {
        int32_t bx = 0;
        int32_t by = 0;
        exiger(ecran::ui_roue_centre_bouton(n - 1, &bx, &by), "le bouton de la famille n'a pas de centre");
        exiger(ecran::ui_roue_simuler_toucher(bx, by), "toucher la famille ferme la roue au lieu de la déplier");
    }
    const ecran::CoutRoue cout = ecran::ui_roue_cout();
    ecran::PositionRoue position = {};
    exiger(ecran::ui_roue_position(&position), "la roue ouverte n'a pas de position");
    std::fprintf(stderr,
                 "roue « %s » %dx%d, doigt en (%d, %d), moyeu en (%d, %d) à %d px du doigt, %s, %s : %d objet(s) LVGL, "
                 "%d minuteur(s), %d police(s), %u o de RAM statique, écran principal : %d objets\n",
                 g_variante, static_cast<int>(c.largeur), static_cast<int>(c.hauteur), static_cast<int>(x),
                 static_cast<int>(y), static_cast<int>(position.x), static_cast<int>(position.y),
                 static_cast<int>(position.ecart), position.dessous ? "boutons dessous" : "boutons dessus",
                 position.tient ? "tient" : "NE TIENT PAS", cout.objets, cout.minuteurs, cout.polices,
                 static_cast<unsigned>(cout.ram_statique), ecran::ui_principal_cout().objets);
}

void ouvrir(const char *variante, const banc::Contexte &c, int fx, int fy, bool alertes, int choix_demo) {
    g_variante = variante;
    afficher_principal(c);
    ouvrir_la_roue(c, fx, fy, alertes, choix_demo);
}

// ---- les rendus ---------------------------------------------------------------------------------------------------------------------

void centre(const banc::Contexte &c) { ouvrir("centre", c, 500, 500, true, 0); }
void centre_sans_alertes(const banc::Contexte &c) { ouvrir("centre-sans-alertes", c, 500, 500, false, 0); }
void coin_haut_gauche(const banc::Contexte &c) { ouvrir("coin-haut-gauche", c, 50, 70, true, 0); }
void coin_bas_droit(const banc::Contexte &c) { ouvrir("coin-bas-droit", c, 950, 930, true, 0); }
void bord_droit(const banc::Contexte &c) { ouvrir("bord-droit", c, 975, 500, true, 0); }
void bord_haut(const banc::Contexte &c) { ouvrir("bord-haut", c, 500, 60, true, 0); }
void bord_bas(const banc::Contexte &c) { ouvrir("bord-bas", c, 500, 940, true, 0); }

void famille_centre(const banc::Contexte &c) { ouvrir("famille-centre", c, 500, 500, true, 8); }
void famille_quatre_centre(const banc::Contexte &c) { ouvrir("famille-quatre-centre", c, 500, 500, true, 4); }
void famille_coin(const banc::Contexte &c) { ouvrir("famille-coin", c, 50, 70, true, 8); }
void famille_bord_droit(const banc::Contexte &c) { ouvrir("famille-bord-droit", c, 975, 500, true, 8); }
void famille_bord_bas(const banc::Contexte &c) { ouvrir("famille-bord-bas", c, 500, 940, true, 6); }

// ---- les parcours : des gestes, des constats --------------------------------------------------------------------------------------

// Le geste de l'appareil, de bout en bout : un appui long sur l'écran principal appelle le rappel branché (ecran_principal.cpp y met
// l'ouverture de la roue), la roue s'ouvre, toucher « Réglages » la ferme et ouvre les réglages, une fois. Le point d'appui est celui que
// le rappel reçoit (le banc n'a pas de doigt : (0, 0)).
int g_appuis_longs = 0;
void sur_appui_long(int32_t x, int32_t y) {
    ++g_appuis_longs;
    EntreeRoue entrees_roue[ecran::ENTREES_ROUE_MAX] = {};
    const int n = composer(entrees_roue, true, 0);
    exiger(ecran::ui_roue_ouvrir(contexte_roue(*g_contexte), entrees_roue, n, x, y),
           "la roue n'a pas pu s'ouvrir sur l'appui long");
}

void parcours_reglages(const banc::Contexte &c) {
    g_variante = "parcours-reglages";
    g_reglages = g_alertes = g_appuis_longs = 0;
    afficher_principal(c);
    ecran::ui_principal_sur_appui_long(sur_appui_long);
    exiger(!ecran::ui_roue_ouverte(), "la roue est ouverte avant tout appui");

    // 1. Appui long -> roue -> Réglages.
    lv_obj_send_event(g_principal, LV_EVENT_LONG_PRESSED, nullptr);
    exiger(g_appuis_longs == 1 && ecran::ui_roue_ouverte(), "un appui long n'ouvre pas la roue");
    exiger(ecran::ui_roue_cout().objets == 1, "la roue ouverte ne compte pas un seul objet LVGL");
    int32_t x = 0;
    int32_t y = 0;
    exiger(ecran::ui_roue_centre_bouton(0, &x, &y), "pas de premier bouton");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Réglages » laisse la roue ouverte");
    tourner();
    exiger(g_reglages == 1 && g_alertes == 0 && g_details == 0,
           "toucher « Réglages » n'ouvre pas les réglages exactement une fois");
    exiger(ecran::ui_roue_cout().objets == 0, "la roue fermée garde un objet LVGL");

    // 2. Appui long -> roue -> Alertes (le troisième bouton : Réglages, Détails, Alertes).
    lv_obj_send_event(g_principal, LV_EVENT_LONG_PRESSED, nullptr);
    exiger(ecran::ui_roue_ouverte(), "la roue ne se rouvre pas au second appui long");
    exiger(ecran::ui_roue_centre_bouton(2, &x, &y), "pas de troisième bouton (Alertes)");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Alertes » laisse la roue ouverte");
    tourner();
    exiger(g_alertes == 1 && g_reglages == 1, "toucher « Alertes » n'ouvre pas les alertes exactement une fois");

    // 3. Un toucher dans le vide ferme la roue sans rien ouvrir.
    lv_obj_send_event(g_principal, LV_EVENT_LONG_PRESSED, nullptr);
    exiger(ecran::ui_roue_ouverte(), "la roue ne se rouvre pas pour le toucher dans le vide");
    exiger(!ecran::ui_roue_simuler_toucher(c.largeur - 4, c.hauteur - 4),
           "un toucher dans le vide laisse la roue ouverte");
    tourner();
    exiger(g_reglages == 1 && g_alertes == 1, "un toucher dans le vide a ouvert quelque chose");

    // 4. Elle se rouvre une quatrième fois, puis se ferme sans rien laisser.
    lv_obj_send_event(g_principal, LV_EVENT_LONG_PRESSED, nullptr);
    exiger(ecran::ui_roue_ouverte() && g_appuis_longs == 4, "la roue ne se rouvre pas après une fermeture");
    ecran::ui_roue_fermer();
    tourner();
    exiger(!ecran::ui_roue_ouverte() && ecran::ui_roue_cout().objets == 0, "fermer la roue laisse un objet LVGL");
}

// « Détails » : appui long -> roue -> toucher « Détails » ouvre la page exactement une fois (et la roue est fermée), « Fermer » ramène à
// l'accueil et supprime la page, puis la roue se rouvre et « Réglages » reste atteignable.
void parcours_details(const banc::Contexte &c) {
    g_variante = "parcours-details";
    g_reglages = g_alertes = g_details = g_appuis_longs = 0;
    afficher_principal(c);
    ecran::ui_principal_sur_appui_long(sur_appui_long);

    lv_obj_send_event(g_principal, LV_EVENT_LONG_PRESSED, nullptr);
    exiger(ecran::ui_roue_ouverte(), "un appui long n'ouvre pas la roue");
    int32_t x = 0;
    int32_t y = 0;
    exiger(ecran::ui_roue_centre_bouton(1, &x, &y), "pas de second bouton (Détails)");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Détails » laisse la roue ouverte");
    tourner();
    exiger(g_details == 1 && g_reglages == 0 && g_alertes == 0,
           "toucher « Détails » n'ouvre pas la page exactement une fois");
    exiger(ecran::ui_roue_cout().objets == 0, "la roue fermée garde un objet LVGL");
    exiger(g_page_details != nullptr && lv_screen_active() == g_page_details,
           "la page Détails n'est pas l'écran actif après le toucher");
    exiger(g_avant_details == g_principal, "la page Détails ne garde pas l'accueil comme écran précédent");

    // « Fermer » est le seul objet cliquable de la page (ui_details.cpp).
    lv_obj_t *page = g_page_details;
    lv_obj_t *fermer = nullptr;
    for (uint32_t i = 0; i < lv_obj_get_child_count(page); ++i) {
        lv_obj_t *o = lv_obj_get_child(page, static_cast<int32_t>(i));
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)) {
            exiger(fermer == nullptr, "la page Détails a plusieurs objets cliquables");
            fermer = o;
        }
    }
    exiger(fermer != nullptr, "la page Détails n'a pas de bouton « Fermer »");
    lv_obj_send_event(fermer, LV_EVENT_CLICKED, nullptr);
    tourner();
    exiger(lv_screen_active() == g_principal, "« Fermer » ne ramène pas à l'accueil");
    exiger(g_page_details == nullptr && !lv_obj_is_valid(page), "« Fermer » laisse la page Détails en mémoire");

    // Les réglages restent atteignables par la roue après la visite de la page.
    lv_obj_send_event(g_principal, LV_EVENT_LONG_PRESSED, nullptr);
    exiger(ecran::ui_roue_ouverte() && g_appuis_longs == 2, "la roue ne se rouvre pas après « Fermer »");
    exiger(ecran::ui_roue_centre_bouton(0, &x, &y), "pas de premier bouton (Réglages)");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Réglages » laisse la roue ouverte");
    tourner();
    exiger(g_reglages == 1 && g_details == 1, "« Réglages » n'est plus atteignable après la page Détails");
    exiger(lv_screen_active() == g_principal, "toucher « Réglages » a changé d'écran dans le banc");
}

// Une famille : toucher son bouton déplie le second anneau, toucher un choix ferme la roue et lance l'action du choix ; avec la famille
// dépliée, « Réglages » reste atteignable.
void parcours_famille(const banc::Contexte &c) {
    g_variante = "parcours-famille";
    g_choix = g_reglages = 0;
    afficher_principal(c);
    ouvrir_la_roue(c, 500, 500, true, 5);
    int32_t x = 0;
    int32_t y = 0;
    exiger(ecran::ui_roue_centre_choix(2, &x, &y), "la famille dépliée n'a pas de troisième choix");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher un choix laisse la roue ouverte");
    tourner();
    exiger(g_choix == 1 && g_reglages == 0,
           "toucher un choix de la famille ne lance pas son action exactement une fois");

    ouvrir_la_roue(c, 500, 500, true, 5);
    exiger(ecran::ui_roue_centre_bouton(0, &x, &y), "pas de premier bouton");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Réglages » famille dépliée laisse la roue ouverte");
    tourner();
    exiger(g_reglages == 1 && g_choix == 1, "« Réglages » n'est pas atteignable quand la famille est dépliée");

    ouvrir_la_roue(c, 500, 500, true, 5);
    exiger(ecran::ui_roue_simuler_toucher(c.largeur - 4, c.hauteur - 4),
           "un toucher dans le vide, famille dépliée, doit d'abord replier le second anneau et non fermer la roue");
    tourner();
    int32_t sx = 0;
    int32_t sy = 0;
    exiger(!ecran::ui_roue_centre_choix(0, &sx, &sy), "le second anneau est encore déplié après le repli");
    exiger(!ecran::ui_roue_simuler_toucher(c.largeur - 4, c.hauteur - 4),
           "un second toucher dans le vide ferme la roue");
    tourner();
    exiger(g_reglages == 1 && g_choix == 1 && ecran::ui_roue_cout().objets == 0, "la fermeture a lancé une action");
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"centre", "Au centre, Réglages, Détails et Alertes", centre},
    {"centre-sans-alertes", "Au centre, Réglages et Détails (aucune alerte)", centre_sans_alertes},
    {"coin-haut-gauche", "Près du coin haut gauche", coin_haut_gauche},
    {"coin-bas-droit", "Près du coin bas droit", coin_bas_droit},
    {"bord-droit", "Près du bord droit", bord_droit},
    {"bord-haut", "Près du bord haut", bord_haut},
    {"bord-bas", "Près du bord bas", bord_bas},
    {"famille-centre", "Famille de démonstration, 8 choix, au centre", famille_centre},
    {"famille-quatre-centre", "Famille de démonstration, 4 choix, au centre", famille_quatre_centre},
    {"famille-coin", "Famille de démonstration, 8 choix, près d'un coin", famille_coin},
    {"famille-bord-droit", "Famille de démonstration, 8 choix, près du bord droit", famille_bord_droit},
    {"famille-bord-bas", "Famille de démonstration, 6 choix, près du bord bas", famille_bord_bas},
    {"parcours-reglages", "Parcours : appui long, Réglages, Alertes, toucher dans le vide", parcours_reglages},
    {"parcours-details", "Parcours : appui long, Détails, Fermer, Réglages encore atteignable", parcours_details},
    {"parcours-famille", "Parcours : famille dépliée, Réglages encore atteignable", parcours_famille},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
