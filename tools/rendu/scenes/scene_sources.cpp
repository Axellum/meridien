// Scène « sources » : la page « Sources » de la roue d'appui long (main/ui_sources.cpp, ADR-0029), compilée telle quelle. Les mentions sont
// celles que le noyau pur décide (ecran/credits.hpp : credits::composer), les mêmes que l'appareil ; le banc ne fait que poser ce que
// l'appareil saurait (quelle météo est montrée et de quel service, alertes activées, normales du mois en mémoire, spot de mer choisi).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_sources.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: sources
//
// La 7B n'a pas été essayée : le rendu 1024x600 n'est qu'une simulation de sa taille. Ce rendu n'est pas une photo de la carte.
#include <cstddef>
#include <cstdint>

#include "banc.h"
#include "ecran/credits.hpp"
#include "lvgl.h"
#include "ui_sources.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

namespace credits = ecran::credits;
using ecran::meteo::fournisseurs::Famille;

void page(const banc::Contexte &c, const credits::Presences &presences) {
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteSources ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = c.largeur;
    ctx.hauteur = c.hauteur;
    if (!ecran::ui_sources_creer(ctx, credits::composer(presences))) {
        return;
    }
    lv_screen_load(racine);
}

// Tout ce que l'écran peut montrer : météo d'Open-Meteo, alertes, normales de novembre (fiche éditée le 06/11/2026, comme la réponse
// réelle de Lorient), spot de mer choisi.
void complet(const banc::Contexte &c) {
    credits::Presences p;
    p.meteo = true;
    p.alertes = true;
    p.normales = true;
    p.edition = "2026-11-06";
    p.mer = true;
    page(c, p);
}

// Le cas le plus chargé : la météo vient du NWS (une ligne de plus, l'avertissement « ni approuvé ni parrainé »).
void nws(const banc::Contexte &c) {
    credits::Presences p;
    p.meteo = true;
    p.origine = Famille::Nws;
    p.alertes = true;
    p.normales = true;
    p.edition = "2026-11-06";
    p.mer = true;
    page(c, p);
}

void met_norway(const banc::Contexte &c) {
    credits::Presences p;
    p.meteo = true;
    p.origine = Famille::MetNo;
    page(c, p);
}

// Les normales sans date d'édition (la fiche ne l'a pas donnée) : la phrase n'en invente pas.
void normales_sans_date(const banc::Contexte &c) {
    credits::Presences p;
    p.meteo = true;
    p.alertes = true;
    p.normales = true;
    page(c, p);
}

// Au démarrage, ou hors de France sans alertes ni mer : seulement ce qui est toujours montré (lieux, polices) et la météo.
void minimal(const banc::Contexte &c) {
    credits::Presences p;
    p.meteo = true;
    page(c, p);
}

// Rien n'est encore lu (météo absente) : les lieux et les polices, jamais une page vide.
void rien_lu(const banc::Contexte &c) { page(c, credits::Presences{}); }

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"complet", "Sources : météo, lieux, alertes, normales, mer, spots, polices", complet},
    {"nws", "Sources : météo du NWS (avec « ni approuvé ni parrainé »), tout le reste", nws},
    {"met-norway", "Sources : météo de MET Norway (licences CC BY 4.0 et NLOD 2.0)", met_norway},
    {"normales-sans-date", "Sources : normales sans date d'édition", normales_sans_date},
    {"minimal", "Sources : météo, lieux et polices seulement", minimal},
    {"rien-lu", "Sources : aucune météo lue", rien_lu},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
