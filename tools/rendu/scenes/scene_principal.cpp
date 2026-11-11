// Scène « principal » : l'écran principal (main/ui_principal.cpp), compilé tel quel, avec le thème par défaut (« Ciel »). L'accueil :
// lieu, grande horloge, date, météo du moment, courbe des 12 heures, bandeau (pluie à venir, alerte officielle), cinq jours.
// Données fixées à la place du temps réel et de la météo (scene_principal_donnees.hpp). Les variantes « esquisse », « calme »,
// « alerte-* » et « esquisse-perimee » reprennent les états de l'esquisse retenue (Lorient, dimanche 08 h 45) : les rendre avec
// -Instant 2026-11-08T07:45:00Z.
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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

// BANC_VOILES=1 : les images des voiles ne fuient pas. 40 mises à jour qui changent la météo (donc les voiles, recuits à chaque fois
// avec d'autres tailles et formats), un repeint après chacune, deux reconstructions de l'écran ; il ne doit jamais rester plus de
// deux images. Code de sortie 4 sinon. L'état final est celui de la variante : le PNG ne change pas.
void verifier_voiles(const banc::Contexte &c, const ecran::EntreesPrincipal &final) {
    if (std::getenv("BANC_VOILES") == nullptr) {
        return;
    }
    const Cas *const cas[] = {&SOLEIL, &PLUIE, &NEIGE, &ORAGE, &BROUILLARD, &CHALEUR, &PIRES_CAS};
    int pire = 0;
    for (int i = 0; i < 40; ++i) {
        if (i == 13 || i == 27) {
            afficher(c, final); // reconstruction : ui_principal_creer rend les images de la précédente
        }
        ecran::ui_principal_mettre_a_jour(entrees(c, 0, vue(*cas[i % 7], c.instant_utc_s)));
        lv_refr_now(c.affichage);
        pire = std::max(pire, ecran::ui_principal_images_voile());
    }
    ecran::ui_principal_mettre_a_jour(final);
    lv_refr_now(c.affichage);
    std::fprintf(stderr, "voiles : au plus %d images vivantes sur 40 mises à jour, %d à la fin\n", pire,
                 ecran::ui_principal_images_voile());
    if (pire > 2) {
        std::exit(4);
    }
}

void depuis(const banc::Contexte &c, const Cas &cas, int64_t decalage = 0) {
    const ecran::EntreesPrincipal e = entrees(c, decalage, vue(cas, c.instant_utc_s + decalage));
    afficher(c, e);
    verifier_voiles(c, e);
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

// L'esquisse « croquis aéré » (voir scene_principal_donnees.hpp) : à rendre avec -Instant 2026-11-08T07:45:00Z (dimanche 08 h 45).
void esquisse(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau = alerte_vent(c.instant_utc_s);
    afficher(c, e);
}

// La même, tout juste périmée : le relevé date de 07 h 52 (« Données de 07:52 · périmées »), l'alerte officielle reste en vigueur.
void esquisse_perimee(const banc::Contexte &c) {
    VueMeteo v = vue_esquisse(c.instant_utc_s);
    v.perimee = true;
    v.instant_s = c.instant_utc_s - 53 * 60;
    ecran::EntreesPrincipal e = entrees(c, 0, v);
    e.bandeau = alerte_vent(c.instant_utc_s);
    afficher(c, e);
}

// Ni pluie ni alerte : une seule puce calme, la place gagnée reste vide.
void calme(const banc::Contexte &c) { afficher(c, entrees(c, 0, sans_pluie(vue_esquisse(c.instant_utc_s)))); }

// Une alerte officielle sans pluie, puis une alerte avec deux autres en vigueur et de la pluie.
void alerte_seule(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, sans_pluie(vue_esquisse(c.instant_utc_s)));
    e.bandeau = alerte_vent(c.instant_utc_s);
    afficher(c, e);
}
void alerte_et_autres(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau = alerte_vent(c.instant_utc_s, 2);
    afficher(c, e);
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
    {"esquisse", "Esquisse : Lorient, dimanche 08 h 45 (-Instant 2026-11-08T07:45:00Z)", esquisse},
    {"esquisse-perimee", "Esquisse, météo périmée", esquisse_perimee},
    {"calme", "Rien à signaler (ni pluie ni alerte)", calme},
    {"alerte-seule", "Alerte officielle sans pluie", alerte_seule},
    {"alerte-et-autres", "Alerte officielle + 2 autres et pluie", alerte_et_autres},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
