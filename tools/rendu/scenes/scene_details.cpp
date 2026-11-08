// Scène « details » : la page « Détails » (main/ui_details.cpp), compilée telle quelle, qui rend les sept grandeurs retirées de l'accueil
// (vent, pluie, humidité, pression, UV, soleil, lune). La page est composée par le noyau pur (ecran/grandeurs.hpp) à partir des mêmes
// temps inventés que l'écran principal (scene_principal_donnees.hpp) : normal, trois thèmes, pluie, nuit, grandeur absente, météo
// absente, météo périmée, valeurs d'exemple, pires cas de largeur, mise à jour à chaud, réouverture.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_details.cpp main/ui_meteo_icones.cpp main/horloge.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: details
//
// La 7B (1024x600) n'a pas été essayée : le rendu à cette taille n'est qu'une simulation. Ce rendu n'est pas une photo de la carte.
#include <cstddef>
#include <cstdint>

#include "banc.h"
#include "ecran/grandeurs.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/visuel_themes.hpp"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_details.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using namespace donnees;
using ecran::visuel::INCONNU;
using ecran::visuel::INCONNU_DIX;
using ecran::visuel::ThemePrincipal;
namespace gr = ecran::grandeurs;

constexpr int64_t kDouzeHeures = 12 * 3600;

gr::Page composer(const banc::Contexte &c, const VueMeteo &v, int64_t maintenant_s) {
    ecran::heure::Fuseau fuseau{};
    ecran::horloge_fuseau(fuseau); // la zone posée par le banc (Lyon, Europe/Paris)
    return gr::composer(v, maintenant_s, fuseau, c.langue);
}

lv_obj_t *construire(const banc::Contexte &c, const ThemePrincipal &theme, const gr::Page &page) {
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteDetails ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = c.largeur;
    ctx.hauteur = c.hauteur;
    ctx.theme = &theme;
    if (!ecran::ui_details_creer(ctx, page)) {
        lv_obj_delete(racine);
        return nullptr; // police non créée : l'écran reste vide, le PNG le montrera
    }
    return racine;
}

void afficher(const banc::Contexte &c, const ThemePrincipal &theme, const VueMeteo &v, int64_t decalage = 0) {
    lv_obj_t *racine = construire(c, theme, composer(c, v, c.instant_utc_s + decalage));
    if (racine != nullptr) {
        lv_screen_load(racine);
    }
}

void depuis(const banc::Contexte &c, const ThemePrincipal &theme, const Cas &cas, int64_t decalage = 0) {
    afficher(c, theme, vue(cas, c.instant_utc_s + decalage), decalage);
}

// ---- les trois thèmes sur un temps calme ----
void soleil(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeCiel, SOLEIL); }
void sombre(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeTableauSombre, SOLEIL); }
void clair(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeClair, SOLEIL); }

// ---- d'autres temps ----
// Pluie en cours, vent fort, tendance en baisse, UV nul.
void pluie(const banc::Contexte &c) {
    VueMeteo v = vue(PLUIE, c.instant_utc_s);
    v.pluie_dans_min = 0;
    afficher(c, ecran::visuel::kThemeCiel, v);
}
// Pluie annoncée dans 25 minutes, sur le thème sombre.
void pluie_proche_sombre(const banc::Contexte &c) {
    VueMeteo v = vue(PLUIE, c.instant_utc_s);
    v.pluie_dans_min = 25;
    afficher(c, ecran::visuel::kThemeTableauSombre, v);
}
// 23 h 40 locale : le soleil est couché (pas de position), la lune se lit.
void nuit(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeCiel, NUIT, kDouzeHeures); }
// Orage, rafales violentes, UV élevé en Ciel.
void orage(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeCiel, ORAGE); }
// UV extrême, chaleur, vent faible.
void chaleur(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeCiel, CHALEUR); }
// Pires cas de largeur : 103 km/h, rafales à 138, 978 hPa, indice UV 123, humidité 100, ville très longue.
void pires_cas(const banc::Contexte &c) { depuis(c, ecran::visuel::kThemeCiel, PIRES_CAS); }

// ---- données absentes ----
// Le vent, l'humidité, l'UV, la pression et le soleil ne sont pas fournis : des tirets, sans unité ni détail ; la pluie et la lune restent.
void grandeurs_absentes(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s);
    v.vent_dix_kmh = INCONNU_DIX;
    v.rafales_dix_kmh = INCONNU_DIX;
    v.direction_deg = INCONNU;
    v.humidite_pct = INCONNU;
    v.pression_dix_hpa = INCONNU_DIX;
    v.tendance_dix_hpa_3h = INCONNU_DIX;
    v.uv_dix = INCONNU_DIX;
    v.lever_s = 0;
    v.coucher_s = 0;
    afficher(c, ecran::visuel::kThemeCiel, v);
}
void grandeurs_absentes_sombre(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s);
    v.vent_dix_kmh = INCONNU_DIX;
    v.rafales_dix_kmh = INCONNU_DIX;
    v.direction_deg = INCONNU;
    v.humidite_pct = INCONNU;
    v.pression_dix_hpa = INCONNU_DIX;
    v.tendance_dix_hpa_3h = INCONNU_DIX;
    v.uv_dix = INCONNU_DIX;
    v.lever_s = 0;
    v.coucher_s = 0;
    afficher(c, ecran::visuel::kThemeTableauSombre, v);
}
// Ce que l'appareil reçoit aujourd'hui du client météo : pas de tendance de pression, pas de cumul, pas de « pluie dans N minutes ».
void donnees_minimales(const banc::Contexte &c) {
    afficher(c, ecran::visuel::kThemeCiel, vue(SOLEIL, c.instant_utc_s, false));
}
// Pas de météo du tout : tirets partout sauf la lune, qui se calcule sur l'appareil.
void sans_meteo(const banc::Contexte &c) { afficher(c, ecran::visuel::kThemeCiel, VueMeteo{}); }
void sans_meteo_sombre(const banc::Contexte &c) { afficher(c, ecran::visuel::kThemeTableauSombre, VueMeteo{}); }
// L'heure de l'appareil n'est pas sûre (SNTP pas encore là) : pas de position du soleil, pas de lune.
void sans_heure(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s);
    lv_obj_t *racine = construire(c, ecran::visuel::kThemeCiel, composer(c, v, 0));
    if (racine != nullptr) {
        lv_screen_load(racine);
    }
}

// ---- météo périmée : les dernières valeurs, grisées, avec l'heure du relevé ----
void perime(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s);
    v.perimee = true;
    v.instant_s = c.instant_utc_s - 3 * 3600; // le relevé date de trois heures
    afficher(c, ecran::visuel::kThemeCiel, v);
}
void perime_sombre(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s);
    v.perimee = true;
    v.instant_s = c.instant_utc_s - 3 * 3600;
    afficher(c, ecran::visuel::kThemeTableauSombre, v);
}
// Les valeurs d'exemple du bouchon du contrat, en plus de la météo périmée : la barre la plus chargée.
void exemple_perime(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s);
    v.perimee = true;
    v.exemple = true;
    v.instant_s = c.instant_utc_s - 3 * 3600;
    afficher(c, ecran::visuel::kThemeCiel, v);
}

// ---- la vie de la page ----
// La page ouverte sur un temps calme puis mise à jour à chaud avec la pluie, de nuit : le ciel, les valeurs et le soleil changent.
void mise_a_jour(const banc::Contexte &c) {
    const int64_t maintenant = c.instant_utc_s;
    lv_obj_t *racine = construire(c, ecran::visuel::kThemeCiel, composer(c, vue(SOLEIL, maintenant), maintenant));
    if (racine == nullptr) {
        return;
    }
    lv_screen_load(racine);
    VueMeteo v = vue(PLUIE, maintenant + kDouzeHeures);
    v.pluie_dans_min = 0;
    ecran::ui_details_mettre_a_jour(composer(c, v, maintenant + kDouzeHeures));
}
// Deux ouvertures de suite sans laisser l'ancienne page se supprimer d'abord : l'état de chacune est rendu à sa suppression, la
// seconde page n'en est pas touchée. Le PNG montre la seconde.
void reouverture(const banc::Contexte &c) {
    const int64_t maintenant = c.instant_utc_s;
    lv_obj_t *premiere = construire(c, ecran::visuel::kThemeCiel, composer(c, vue(SOLEIL, maintenant), maintenant));
    if (premiere == nullptr) {
        return;
    }
    lv_screen_load(premiere);
    VueMeteo v = vue(PLUIE, maintenant);
    v.pluie_dans_min = 0;
    lv_obj_t *seconde = construire(c, ecran::visuel::kThemeTableauSombre, composer(c, v, maintenant));
    if (seconde == nullptr) {
        return;
    }
    lv_screen_load(seconde);
    lv_obj_delete(premiere);
    ecran::ui_details_mettre_a_jour(composer(c, vue(ORAGE, maintenant), maintenant)); // la page vivante est la seconde
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"soleil", "Détails : un temps calme, thème Ciel", soleil},
    {"sombre", "Détails : le même temps, thème tableau sombre", sombre},
    {"clair", "Détails : le même temps, thème clair", clair},
    {"pluie", "Détails : il pleut, vent fort, pression en baisse", pluie},
    {"pluie-proche-sombre", "Détails : pluie dans 25 minutes, thème sombre", pluie_proche_sombre},
    {"nuit", "Détails : 23 h 40, soleil couché, la lune", nuit},
    {"orage", "Détails : orage, rafales violentes", orage},
    {"chaleur", "Détails : forte chaleur, UV très élevé", chaleur},
    {"pires-cas", "Détails : les valeurs les plus larges", pires_cas},
    {"grandeurs-absentes", "Détails : vent, humidité, UV, pression et soleil absents (tirets)", grandeurs_absentes},
    {"grandeurs-absentes-sombre", "Détails : mêmes tirets, thème sombre", grandeurs_absentes_sombre},
    {"donnees-minimales", "Détails : ce que le client météo fournit aujourd'hui", donnees_minimales},
    {"sans-meteo", "Détails : pas de météo, la lune seule", sans_meteo},
    {"sans-meteo-sombre", "Détails : pas de météo, thème sombre", sans_meteo_sombre},
    {"sans-heure", "Détails : heure de l'appareil pas sûre", sans_heure},
    {"perime", "Détails : météo périmée de trois heures, grisée", perime},
    {"perime-sombre", "Détails : météo périmée, thème sombre", perime_sombre},
    {"exemple-perime", "Détails : valeurs d'exemple et météo périmée", exemple_perime},
    {"mise-a-jour", "Détails : mise à jour à chaud (calme puis pluie de nuit)", mise_a_jour},
    {"reouverture", "Détails : réouverture avant la suppression de la première page", reouverture},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
