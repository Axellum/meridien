// Scène « alertes » : la pastille de l'alerte officielle de Vigie dans la barre de l'écran principal (main/ui_principal.cpp) et la page
// que son appui ouvre (main/ui_alertes.cpp), compilées telles quelles.
//
// Les alertes ne sont PAS inventées : ce sont de VRAIES réponses de Vigie captées le 2026-11-07 (test/host/donnees/vigie_*.inc,
// test/host/donnees/LISEZMOI_vigie.md), lues par le même noyau que l'appareil (vigie::analyser, composer, composer_bandeau). Seules la
// météo (scene_principal_donnees.hpp, inventée) et l'heure de l'écran sont des fixtures : l'horloge de l'écran est calée sur l'instant de
// la capture, pour que l'alerte soit « en vigueur » comme elle l'était, et la réponse est tenue pour « reçue à cet instant ».
// Les réponses ont été demandées en français et en anglais (`lang=fr,en`) : sur un écran en allemand ou en turc, l'appareil demande
// `lang=de,en` ou `lang=tr,en`, ce que ce banc ne peut pas rejouer ; l'émetteur est ici montré dans les deux langues captées.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_principal_outils.cpp main/ui_principal_donnees.cpp main/ui_principal_disposition.cpp main/ui_principal_barre.cpp main/ui_principal_horloge.cpp main/ui_principal_voiles.cpp main/ui_principal_carte_heure.cpp main/ui_principal_heures.cpp main/ui_principal_bandeau.cpp main/ui_principal_mode_auto.cpp main/ui_principal_jours.cpp main/ui_principal_dessin.cpp main/ui_principal_toucher.cpp main/ui_principal_ciel.cpp main/ui_meteo_icones.cpp main/ui_alertes.cpp main/horloge.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: alertes
//
// La 7B n'a pas été essayée : le rendu 1024x600 n'est qu'une simulation de sa taille. Ce rendu n'est pas une photo de la carte.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "banc.h"
#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_bandeau.hpp"
#include "ecran/vigie_vue.hpp"
#include "horloge.h"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_alertes.h"
#include "ui_principal.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using namespace donnees;
using sv = std::string_view;
namespace vigie = ecran::vigie;

// Réponses réelles de Vigie (octets du fil, décompressés).
constexpr sv OSLO =
#include "../../../test/host/donnees/vigie_oslo.inc"
    ;
constexpr sv LA_PALMA =
#include "../../../test/host/donnees/vigie_la_palma.inc"
    ;
constexpr sv EBERSWALDE =
#include "../../../test/host/donnees/vigie_eberswalde.inc"
    ;

constexpr int64_t kOsloS = 1794041558;       // server_utc de la capture : l'avertissement jaune y est en vigueur
constexpr int64_t kEberswaldeS = 1794041555; // idem
constexpr int64_t kLaPalmaS =
    1794086000; // trois avertissements orange et un jaune sont en vigueur (voir test_vigie_bandeau.cpp)

vigie::Alertes g_alertes; // ≈ 70 Ko : hors pile

struct Lu {
    bool ok = false;
    vigie::Vue vue;
    int64_t maintenant_s = 0;
};

Lu lire(const banc::Contexte &c, sv json, int64_t maintenant_s, vigie::NiveauMin minimum = vigie::NiveauMin::Jaune) {
    Lu l;
    l.maintenant_s = maintenant_s;
    l.ok = vigie::analyser(json, ecran::i18n::code(c.langue), g_alertes) == vigie::Statut::Ok;
    if (l.ok) {
        l.vue = vigie::composer(g_alertes, maintenant_s, maintenant_s, minimum);
    }
    return l;
}

void ecran_principal(const banc::Contexte &c, const Lu &l, bool perimee = false, bool exemple = false) {
    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(ecran_lvgl, ressources, c.largeur, c.hauteur)) {
        return;
    }
    VueMeteo v = vue(SOLEIL, l.maintenant_s);
    v.perimee = perimee;
    v.exemple = exemple;
    ecran::EntreesPrincipal e = entrees(c, l.maintenant_s - c.instant_utc_s, v);
    if (l.ok) {
        e.bandeau = vigie::composer_bandeau(g_alertes, l.vue, c.langue);
    }
    ecran::ui_principal_mettre_a_jour(e);
    lv_screen_load(ecran_lvgl);
}

void page(const banc::Contexte &c, const Lu &l) {
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteAlertes ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = c.largeur;
    ctx.hauteur = c.hauteur;
    ctx.heure_locale = ecran::horloge_heure_locale;
    ecran::DonneesAlertes d;
    d.alertes = &g_alertes;
    d.vue = &l.vue;
    d.recu_s = l.maintenant_s;
    if (!l.ok || !ecran::ui_alertes_creer(ctx, d)) {
        return;
    }
    lv_screen_load(racine);
}

// L'écran principal : une seule alerte jaune (Oslo), puis plusieurs (La Palma : la plus grave, « +3 »), puis un avertissement allemand (Eberswalde).
void pastille_oslo(const banc::Contexte &c) { ecran_principal(c, lire(c, OSLO, kOsloS)); }
void pastille_la_palma(const banc::Contexte &c) { ecran_principal(c, lire(c, LA_PALMA, kLaPalmaS)); }
void pastille_eberswalde(const banc::Contexte &c) { ecran_principal(c, lire(c, EBERSWALDE, kEberswaldeS)); }
// Pire cas de place : la pastille la plus longue (La Palma) avec la mention « Exemple » et la météo périmée dans la même barre.
void pastille_et_etats(const banc::Contexte &c) { ecran_principal(c, lire(c, LA_PALMA, kLaPalmaS), true, true); }
// Rien n'est en vigueur (le niveau minimal est trop haut pour La Palma) : l'écran principal est celui des autres scènes, sans pastille.
void sans_alerte(const banc::Contexte &c) { ecran_principal(c, lire(c, LA_PALMA, kLaPalmaS, vigie::NiveauMin::Rouge)); }

// La page des alertes.
void page_oslo(const banc::Contexte &c) { page(c, lire(c, OSLO, kOsloS)); }
void page_la_palma(const banc::Contexte &c) { page(c, lire(c, LA_PALMA, kLaPalmaS)); }
void page_eberswalde(const banc::Contexte &c) { page(c, lire(c, EBERSWALDE, kEberswaldeS)); }
// La page ouverte au moment où la dernière alerte vient de finir : « Aucune alerte en vigueur » et la carte des sources.
void page_vide(const banc::Contexte &c) { page(c, lire(c, OSLO, kOsloS + 30 * 86400)); }

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"pastille-oslo", "Pastille : un avertissement jaune (Oslo, réponse réelle)", pastille_oslo},
    {"pastille-la-palma", "Pastille : la plus grave de quatre (La Palma, réponse réelle)", pastille_la_palma},
    {"pastille-eberswalde",
     "Pastille : gel allemand, textes de l'émetteur en français et en anglais (Eberswalde, réponse réelle)",
     pastille_eberswalde},
    {"pastille-et-etats", "Pastille avec « Exemple » et météo périmée", pastille_et_etats},
    {"sans-alerte", "Rien en vigueur : aucune pastille", sans_alerte},
    {"page-oslo", "Page des alertes : un avertissement (Oslo)", page_oslo},
    {"page-la-palma", "Page des alertes : quatre avertissements (La Palma)", page_la_palma},
    {"page-eberswalde", "Page des alertes : Eberswalde", page_eberswalde},
    {"page-vide", "Page des alertes : rien en vigueur", page_vide},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
