// Scène « principal » : l'écran principal (main/ui_principal.cpp), compilé tel quel, avec le thème par défaut (« Ciel »). Grande
// horloge, date, météo du moment, grandeurs (vent, pluie, humidité, pression, UV, soleil, lune), heure par heure, sept jours,
// alertes. Données fixées à la place du temps réel et de la météo (scene_principal_donnees.hpp).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_meteo_icones.cpp main/horloge.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: principal
//
// L'écran est dessiné pour 1280x720 (paysage) ou 720x1280 (portrait) et mis à l'échelle sur les autres tailles (1024x600 de la 7B) :
// la 7B n'a pas été essayée, ce rendu n'est qu'une simulation de sa taille.
//
// Fixtures (inventées, pas des mesures). « donnees-minimales » montre ce que l'appareil reçoit aujourd'hui du client météo (ni
// heure par heure, ni tendance, ni cumul de pluie) ; les autres variantes, les données complètes demandées à la session météo.
#include <cstddef>
#include <cstdint>

#include "banc.h"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_principal.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using namespace donnees;

constexpr int64_t kDouzeHeures = 12 * 3600;

void afficher(const banc::Contexte &contexte, const ecran::EntreesPrincipal &e) {
    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(ecran_lvgl, ressources, contexte.largeur, contexte.hauteur)) {
        return; // police non créée : l'écran reste vide, le PNG le montrera
    }
    ecran::ui_principal_mettre_a_jour(e);
    lv_screen_load(ecran_lvgl);
}

void depuis(const banc::Contexte &c, const Cas &cas, int64_t decalage = 0) {
    afficher(c, entrees(c, decalage, vue(cas, c.instant_utc_s + decalage)));
}

void soleil(const banc::Contexte &c) { depuis(c, SOLEIL); }
void pluie(const banc::Contexte &c) { depuis(c, PLUIE); }
void neige(const banc::Contexte &c) { depuis(c, NEIGE); }
void orage(const banc::Contexte &c) { depuis(c, ORAGE); }
void brouillard(const banc::Contexte &c) { depuis(c, BROUILLARD); }
void nuit(const banc::Contexte &c) { depuis(c, NUIT, kDouzeHeures); } // 23 h 40 locale
void chaleur(const banc::Contexte &c) { depuis(c, CHALEUR); }

// Ce que l'appareil reçoit aujourd'hui : pas d'heure par heure, pas de tendance de pression, pas de cumul ni de minutage de la pluie.
void donnees_minimales(const banc::Contexte &c) { afficher(c, entrees(c, 0, vue(SOLEIL, c.instant_utc_s, false))); }

// La dernière météo reçue est trop ancienne : l'écran la garde et le dit.
void perimee(const banc::Contexte &c) {
    VueMeteo v = vue(PLUIE, c.instant_utc_s);
    v.perimee = true;
    afficher(c, entrees(c, 0, v));
}

// Ce que l'appareil montre tant que la météo vient du bouchon du contrat : l'écran le dit.
void exemple(const banc::Contexte &c) {
    VueMeteo v = vue(SOLEIL, c.instant_utc_s, false);
    v.exemple = true;
    afficher(c, entrees(c, 0, v));
}

// Aucune météo reçue (pas de zone choisie, ou réseau absent) : « Météo indisponible ».
void meteo_absente(const banc::Contexte &c) { afficher(c, entrees(c, 0, VueMeteo{})); }

// Les premières secondes : pas d'heure du réseau (« --:-- » et phrase d'attente), pas de météo.
void attente(const banc::Contexte &c) { afficher(c, entrees(c, 0, VueMeteo{}, false)); }

// Pires cas de largeur : ville très longue, températures négatives à deux chiffres, libellé le plus long, météo périmée.
void pires_cas(const banc::Contexte &c) {
    VueMeteo v = vue(PIRES_CAS, c.instant_utc_s);
    v.perimee = true;
    afficher(c, entrees(c, 0, v));
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"soleil", "Soleil, ciel clair", soleil},
    {"pluie", "Pluie, vent, pression qui baisse", pluie},
    {"neige", "Neige, gel", neige},
    {"orage", "Orage, rafales", orage},
    {"brouillard", "Brouillard", brouillard},
    {"nuit", "Nuit, froid, lune", nuit},
    {"chaleur", "Forte chaleur, UV extrême", chaleur},
    {"donnees-minimales", "Données reçues aujourd'hui (sans heure par heure)", donnees_minimales},
    {"perimee", "Météo périmée", perimee},
    {"exemple", "Météo d'exemple (bouchon)", exemple},
    {"meteo-absente", "Aucune météo reçue", meteo_absente},
    {"attente", "Au démarrage : heure en attente", attente},
    {"pires-cas", "Pires cas de largeur", pires_cas},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
