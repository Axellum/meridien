// Scène « principal-themes » : les pistes de dessin de l'écran principal côte à côte (docs/design/), le même écran (main/ui_principal.cpp)
// avec chacun des quatre thèmes de visuel_themes.hpp, sur trois temps (soleil, pluie, nuit froide).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_meteo_icones.cpp main/horloge.cpp
// rendu-langues: fr de tr
// rendu-langues-defaut: fr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: principal-themes
//
// Les quatre pistes :
//   ciel     « verre sur ciel » : fond en dégradé qui suit l'heure et le temps, cartes translucides. Tout l'écran d'un coup d'oeil.
//   tableau  « tableau de bord » : fond uni anthracite, cartes pleines, couleurs par grandeur. Même composition que « ciel ».
//   horloge  « horloge d'abord » : l'heure en géant, la météo du moment et la semaine. Ni grandeurs ni heure par heure.
//   clair    « jour » : fond clair, texte sombre, pour une pièce très éclairée. Même composition que « ciel ».
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "banc.h"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_principal.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using namespace donnees;
using ecran::visuel::ThemePrincipal;

constexpr int64_t kDouzeHeures = 12 * 3600;

void afficher(const banc::Contexte &contexte, const ThemePrincipal &theme, const Cas &cas, int64_t decalage) {
    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    ecran::ui_principal_differer_verre(std::getenv("BANC_VERRE_DIFFERE") != nullptr); // voir scene_principal.cpp
    if (!ecran::ui_principal_creer(ecran_lvgl, ressources, contexte.largeur, contexte.hauteur, theme)) {
        return;
    }
    ecran::ui_principal_mettre_a_jour(entrees(contexte, decalage, vue(cas, contexte.instant_utc_s + decalage)));
    lv_screen_load(ecran_lvgl);
}

// Bascule à chaud (ui_principal_changer_theme) : l'écran est construit et rendu avec `depart`, puis passe à `arrivee`. Le PNG doit
// être identique, pixel pour pixel, à celui de `arrivee` construit directement (tools/rendu/verifier_bascule.py le contrôle).
void bascule(const banc::Contexte &contexte, const ThemePrincipal &depart, const ThemePrincipal &arrivee) {
    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(ecran_lvgl, ressources, contexte.largeur, contexte.hauteur, depart)) {
        return;
    }
    const ecran::EntreesPrincipal e = entrees(contexte, 0, vue(SOLEIL, contexte.instant_utc_s));
    ecran::ui_principal_mettre_a_jour(e);
    lv_screen_load(ecran_lvgl);
    lv_refr_now(contexte.affichage); // le premier état est vraiment dessiné avant la bascule
    ecran::ui_principal_changer_theme(arrivee);
    ecran::ui_principal_mettre_a_jour(e);
}

void bascule_ciel_clair(const banc::Contexte &c) { bascule(c, ecran::visuel::kThemeCiel, ecran::visuel::kThemeClair); }
void bascule_clair_ciel(const banc::Contexte &c) { bascule(c, ecran::visuel::kThemeClair, ecran::visuel::kThemeCiel); }
void bascule_ciel_tableau(const banc::Contexte &c) {
    bascule(c, ecran::visuel::kThemeCiel, ecran::visuel::kThemeTableauSombre);
}
void bascule_tableau_ciel(const banc::Contexte &c) {
    bascule(c, ecran::visuel::kThemeTableauSombre, ecran::visuel::kThemeCiel);
}
// Vers « horloge » la bascule est refusée (autre disposition) : l'écran doit rester celui du départ.
void bascule_refusee(const banc::Contexte &c) { bascule(c, ecran::visuel::kThemeCiel, ecran::visuel::kThemeHorloge); }

// Huit temps par thème : de quoi juger le même thème sur un ciel dégagé, de la pluie, de la neige, un orage, du brouillard, la
// nuit, une canicule et les textes les plus longs (essayer avec -Langue de tr).
#define VARIANTES(nom, theme)                                                                                          \
    void nom##_soleil(const banc::Contexte &c) { afficher(c, theme, SOLEIL, 0); }                                      \
    void nom##_pluie(const banc::Contexte &c) { afficher(c, theme, PLUIE, 0); }                                        \
    void nom##_neige(const banc::Contexte &c) { afficher(c, theme, NEIGE, 0); }                                        \
    void nom##_orage(const banc::Contexte &c) { afficher(c, theme, ORAGE, 0); }                                        \
    void nom##_brouillard(const banc::Contexte &c) { afficher(c, theme, BROUILLARD, 0); }                              \
    void nom##_nuit(const banc::Contexte &c) { afficher(c, theme, NUIT, kDouzeHeures); }                               \
    void nom##_chaleur(const banc::Contexte &c) { afficher(c, theme, CHALEUR, 0); }                                    \
    void nom##_pires(const banc::Contexte &c) { afficher(c, theme, PIRES_CAS, 0); }

VARIANTES(ciel, ecran::visuel::kThemeCiel)
VARIANTES(tableau, ecran::visuel::kThemeTableauSombre)
VARIANTES(horloge, ecran::visuel::kThemeHorloge)
VARIANTES(clair, ecran::visuel::kThemeClair)

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"ciel-soleil", "Ciel : soleil", ciel_soleil},
    {"ciel-pluie", "Ciel : pluie", ciel_pluie},
    {"ciel-neige", "Ciel : neige", ciel_neige},
    {"ciel-orage", "Ciel : orage", ciel_orage},
    {"ciel-brouillard", "Ciel : brouillard", ciel_brouillard},
    {"ciel-nuit", "Ciel : nuit froide", ciel_nuit},
    {"ciel-chaleur", "Ciel : canicule", ciel_chaleur},
    {"ciel-pires", "Ciel : textes les plus longs", ciel_pires},
    {"tableau-soleil", "Tableau : soleil", tableau_soleil},
    {"tableau-pluie", "Tableau : pluie", tableau_pluie},
    {"tableau-neige", "Tableau : neige", tableau_neige},
    {"tableau-orage", "Tableau : orage", tableau_orage},
    {"tableau-brouillard", "Tableau : brouillard", tableau_brouillard},
    {"tableau-nuit", "Tableau : nuit froide", tableau_nuit},
    {"tableau-chaleur", "Tableau : canicule", tableau_chaleur},
    {"tableau-pires", "Tableau : textes les plus longs", tableau_pires},
    {"horloge-soleil", "Horloge : soleil", horloge_soleil},
    {"horloge-pluie", "Horloge : pluie", horloge_pluie},
    {"horloge-neige", "Horloge : neige", horloge_neige},
    {"horloge-orage", "Horloge : orage", horloge_orage},
    {"horloge-brouillard", "Horloge : brouillard", horloge_brouillard},
    {"horloge-nuit", "Horloge : nuit froide", horloge_nuit},
    {"horloge-chaleur", "Horloge : canicule", horloge_chaleur},
    {"horloge-pires", "Horloge : textes les plus longs", horloge_pires},
    {"clair-soleil", "Clair : soleil", clair_soleil},
    {"clair-pluie", "Clair : pluie", clair_pluie},
    {"clair-neige", "Clair : neige", clair_neige},
    {"clair-orage", "Clair : orage", clair_orage},
    {"clair-brouillard", "Clair : brouillard", clair_brouillard},
    {"clair-nuit", "Clair : nuit froide", clair_nuit},
    {"clair-chaleur", "Clair : canicule", clair_chaleur},
    {"clair-pires", "Clair : textes les plus longs", clair_pires},
    {"bascule-ciel-clair", "Bascule à chaud : ciel vers clair", bascule_ciel_clair},
    {"bascule-clair-ciel", "Bascule à chaud : clair vers ciel", bascule_clair_ciel},
    {"bascule-ciel-tableau", "Bascule à chaud : ciel vers tableau", bascule_ciel_tableau},
    {"bascule-tableau-ciel", "Bascule à chaud : tableau vers ciel", bascule_tableau_ciel},
    {"bascule-refusee", "Bascule refusée : ciel vers horloge (reste ciel)", bascule_refusee},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
