// Scène « ouvertures » : le chronomètre de l'ouverture des pages, sur le PC. L'accueil (main/ui_principal.cpp) est construit et rendu,
// puis chaque page s'ouvre par-dessus, deux fois de suite, comme sur l'appareil (écran neuf chargé d'un coup, « Fermer » recharge
// l'accueil et supprime la page) : Détails (main/ui_details.cpp), Mer (main/ui_mer.cpp), Réglages (main/ui_reglages.cpp), Alertes
// (main/ui_alertes.cpp), tous compilés tels quels. L'image écrite est l'accueil à la fin du parcours (il doit être revenu intact).
//
// Avec BANC_CHRONO=1, la scène écrit sur stderr, pour chaque ouverture : le temps de construction de la page, celui de son premier rendu
// complet, et la mémoire des images de glyphes (tampons A8 de TinyTTF, ceux que l'appareil met en PSRAM) ; puis, par police partagée
// (main/ui_ressources.h), le nombre de glyphes différents qu'elle a servis (ce qui dimensionne son cache). Durées du PC : elles comparent
// deux versions du dessin entre elles, elles ne disent rien de la vitesse de la carte (docs/outils.md).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_meteo_icones.cpp main/horloge.cpp main/ui_details.cpp main/ui_mer.cpp main/ui_alertes.cpp main/ui_installation.cpp main/ui_reglages.cpp main/ui_reglages_mer.cpp main/ui_clavier.cpp main/ui_commun.cpp main/hote_memoire.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: ouvertures
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string_view>

#include "parcours_outils.h"

#include "ecran/grandeurs.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/mer_trame.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_vue.hpp"
#include "horloge.h"
#include "scenes/scene_principal_donnees.hpp"
#include "src/draw/lv_draw_buf_private.h"
#include "ui_alertes.h"
#include "ui_details.h"
#include "ui_mer.h"
#include "ui_principal.h"
#include "ui_ressources.h"

namespace {

using namespace donnees;
using horloge_pc = std::chrono::steady_clock;

constexpr std::string_view HOSSEGOR =
#include "../../../test/host/donnees/mer_hossegor.inc"
    ;
constexpr std::string_view OSLO =
#include "../../../test/host/donnees/vigie_oslo.inc"
    ;
constexpr int64_t kOsloS = 1794041558; // server_utc de la capture : l'avertissement jaune y est en vigueur

const banc::Contexte *g_c = nullptr;
bool g_chrono = false;
lv_obj_t *g_accueil = nullptr;
ecran::mer::Mer g_mer;
ecran::vigie::Alertes g_alertes; // ≈ 70 Ko : hors pile
ecran::vigie::Vue g_vue_alertes;

[[noreturn]] void echec(const char *quoi) {
    std::fprintf(stderr, "ECHEC de la scène ouvertures : %s\n", quoi);
    std::exit(1);
}

// ---- la mémoire des images de glyphes : le gestionnaire de tampons des polices, compté (l'appareil le détourne vers la PSRAM) -----

std::map<void *, std::size_t> g_tampons;
std::size_t g_octets = 0;
lv_draw_buf_malloc_cb_t g_malloc_lvgl = nullptr;
lv_draw_buf_free_cb_t g_free_lvgl = nullptr;

void *compter_malloc(std::size_t octets, lv_color_format_t format) {
    void *p = g_malloc_lvgl(octets, format);
    if (p != nullptr) {
        g_tampons[p] = octets;
        g_octets += octets;
    }
    return p;
}

void compter_free(void *p) {
    const auto it = g_tampons.find(p);
    if (it != g_tampons.end()) {
        g_octets -= it->second;
        g_tampons.erase(it);
    }
    g_free_lvgl(p);
}

void compter_les_tampons() {
    lv_draw_buf_handlers_t *h = lv_draw_buf_get_font_handlers();
    g_malloc_lvgl = h->buf_malloc_cb;
    g_free_lvgl = h->buf_free_cb;
    h->buf_malloc_cb = compter_malloc;
    h->buf_free_cb = compter_free;
}

// ---- les glyphes servis par chaque police partagée : la fonction de description de chaque police est enveloppée -----------------

struct Suivi {
    bool (*origine)(const lv_font_t *, lv_font_glyph_dsc_t *, uint32_t, uint32_t) = nullptr;
    std::set<uint32_t> glyphes;
};
std::map<const lv_font_t *, Suivi> g_suivis;

bool decrire_compte(const lv_font_t *f, lv_font_glyph_dsc_t *dsc, uint32_t lettre, uint32_t suivante) {
    Suivi &s = g_suivis[f];
    s.glyphes.insert(lettre);
    return s.origine(f, dsc, lettre, suivante);
}

void suivre_les_nouvelles_polices() {
    for (int i = 0; i < ecran::ressources::nombre_polices(); ++i) {
        lv_font_t *f = ecran::ressources::police_n(i).f;
        if (f != nullptr && g_suivis.find(f) == g_suivis.end()) {
            g_suivis[f].origine = f->get_glyph_dsc;
            f->get_glyph_dsc = decrire_compte;
        }
    }
}

// ---- chrono ---------------------------------------------------------------------------------------------------------------------

long long us(horloge_pc::time_point a, horloge_pc::time_point b) {
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(b - a).count());
}

// Ouvre la page (construire), la charge, la rend en entier ; dit les durées ; puis la ferme et rend l'accueil revenu.
void mesurer(const char *nom, int fois, lv_obj_t *(*construire)(), void (*liberer)()) {
    const std::size_t avant = g_octets;
    const auto t0 = horloge_pc::now();
    lv_obj_t *page = construire();
    lv_screen_load(page);
    const auto t1 = horloge_pc::now();
    suivre_les_nouvelles_polices(); // hors chrono : seulement pour compter les glyphes servis au rendu
    const auto t2 = horloge_pc::now();
    lv_refr_now(nullptr);
    const auto t3 = horloge_pc::now();
    if (g_chrono) {
        std::fprintf(
            stderr,
            "chrono ouverture %-8s %d : construction %6lld us, premier rendu %6lld us, glyphes +%zu o (%zu o)\n", nom,
            fois, us(t0, t1), us(t2, t3), g_octets - avant, g_octets);
    }
    if (liberer != nullptr) {
        liberer();
    }
    lv_screen_load(g_accueil);
    lv_obj_delete(page);
    lv_refr_now(nullptr);
}

// ---- les pages, construites comme les colles de l'appareil (main/ui_*_ouvrir.cpp) les construisent -------------------------------

lv_obj_t *details() {
    ecran::heure::Fuseau fuseau{};
    ecran::horloge_fuseau(fuseau);
    const ecran::grandeurs::Page page =
        ecran::grandeurs::composer(vue_esquisse(g_c->instant_utc_s), g_c->instant_utc_s, fuseau, g_c->langue);
    lv_obj_t *racine = lv_obj_create(nullptr);
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);
    ecran::ContexteDetails ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = g_c->largeur;
    ctx.hauteur = g_c->hauteur;
    ctx.theme = &ecran::visuel::kThemeCiel;
    if (!ecran::ui_details_creer(ctx, page)) {
        echec("page Détails non créée");
    }
    return racine;
}

lv_obj_t *mer() {
    g_mer = ecran::mer::Mer{};
    ecran::mer::decoder(HOSSEGOR, g_mer);
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteMer ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = g_c->largeur;
    ctx.hauteur = g_c->hauteur;
    ctx.theme = &ecran::visuel::kThemeCiel;
    ecran::DonneesMer donnees;
    donnees.mer = &g_mer;
    donnees.maintenant_s = g_mer.genere_s + 180;
    donnees.decalage_s = 3600;
    if (!ecran::ui_mer_creer(ctx, donnees)) {
        echec("page Mer non créée");
    }
    return racine;
}

lv_obj_t *reglages() {
    lv_obj_t *racine = lv_obj_create(nullptr);
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);
    ecran::ContexteReglages c;
    c.racine = racine;
    c.ttf = roboto_700_start;
    c.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    c.largeur = g_c->largeur;
    c.hauteur = g_c->hauteur;
    c.hote = &parcours::g_hote;
    c.fermer = parcours::fermer_interface;
    ecran::ui_reglages_creer(c);
    return racine;
}

lv_obj_t *alertes() {
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteAlertes ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = g_c->largeur;
    ctx.hauteur = g_c->hauteur;
    ctx.heure_locale = ecran::horloge_heure_locale;
    ecran::DonneesAlertes d;
    d.alertes = &g_alertes;
    d.vue = &g_vue_alertes;
    d.recu_s = kOsloS;
    if (!ecran::ui_alertes_creer(ctx, d)) {
        echec("page Alertes non créée");
    }
    return racine;
}

void ouvertures(const banc::Contexte &c) {
    g_c = &c;
    g_chrono = std::getenv("BANC_CHRONO") != nullptr;
    compter_les_tampons();

    parcours::g_hote.fixer_horloge(1000000);
    parcours::g_hote.meteo.disponible = true;
    parcours::g_hote.meteo.temperature_c = 12.0F;
    std::snprintf(parcours::g_hote.meteo.libelle, sizeof parcours::g_hote.meteo.libelle, "%s",
                  parcours::meteo_dans(c.langue));
    parcours::g_hote.definir_nombre("langue.ecran", ecran::i18n::indice(c.langue));
    parcours::g_hote.definir_nombre("langue.assistant", ecran::i18n::indice(c.langue));
    parcours::g_hote.simuler_connecte("Maison_5G", "192.168.1.52");
    if (ecran::vigie::analyser(OSLO, ecran::i18n::code(c.langue), g_alertes) != ecran::vigie::Statut::Ok) {
        echec("réponse Vigie d'Oslo illisible");
    }
    g_vue_alertes = ecran::vigie::composer(g_alertes, kOsloS, kOsloS, ecran::vigie::NiveauMin::Jaune);

    // L'accueil, construit et rendu une fois (ses polices sont alors chaudes, comme sur l'appareil après le démarrage).
    const std::size_t avant = g_octets;
    const auto t0 = horloge_pc::now();
    g_accueil = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(g_accueil, ressources, c.largeur, c.hauteur)) {
        echec("accueil non créé");
    }
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau = alerte_vent(c.instant_utc_s);
    ecran::ui_principal_mettre_a_jour(e);
    lv_screen_load(g_accueil);
    const auto t1 = horloge_pc::now();
    suivre_les_nouvelles_polices();
    const auto t2 = horloge_pc::now();
    lv_refr_now(nullptr);
    const auto t3 = horloge_pc::now();
    if (g_chrono) {
        std::fprintf(
            stderr,
            "chrono ouverture %-8s %d : construction %6lld us, premier rendu %6lld us, glyphes +%zu o (%zu o)\n",
            "accueil", 1, us(t0, t1), us(t2, t3), g_octets - avant, g_octets);
    }

    for (int fois = 1; fois <= 2; ++fois) {
        mesurer("details", fois, details, nullptr);
    }
    for (int fois = 1; fois <= 2; ++fois) {
        mesurer("mer", fois, mer, nullptr);
    }
    for (int fois = 1; fois <= 2; ++fois) {
        mesurer("reglages", fois, reglages, ecran::ui_reglages_liberer);
    }
    for (int fois = 1; fois <= 2; ++fois) {
        mesurer("alertes", fois, alertes, nullptr);
    }

    if (g_chrono) {
        std::fprintf(stderr, "chrono polices : %d instance(s) partagée(s)\n", ecran::ressources::nombre_polices());
        for (int i = 0; i < ecran::ressources::nombre_polices(); ++i) {
            const ecran::ressources::PoliceCreee p = ecran::ressources::police_n(i);
            const auto it = g_suivis.find(p.f);
            std::fprintf(stderr, "chrono police %3d px : %zu glyphe(s) différent(s)\n", static_cast<int>(p.px),
                         it != g_suivis.end() ? it->second.glyphes.size() : static_cast<std::size_t>(0));
        }
    }
}

} // namespace

namespace banc {
const Scene kScenes[] = {
    {"ouvertures", "Accueil revenu après Détails, Mer, Réglages et Alertes ouverts deux fois", ouvertures},
};
const std::size_t kNombreScenes = sizeof kScenes / sizeof kScenes[0];
} // namespace banc
