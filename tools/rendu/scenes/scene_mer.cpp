// Scène « mer » : la page mer d'un spot (main/ui_mer.cpp, ADR-0022), compilée telle quelle, sur le même noyau que l'appareil
// (mer::decoder, mer::composer_bandeau, mer::courbe_maree...).
//
// Les premières variantes ne sont PAS inventées : ce sont les SIX VRAIES trames de Vigie captées le 08/11/2026 vers 13 h 15 UTC
// (test/host/donnees/mer_*.inc, test/host/donnees/LISEZMOI_mer.md), lues par le décodeur de l'appareil. L'instant montré est celui de la
// trame, plus quelques minutes : les données sont fraîches comme elles l'étaient. Le décalage horaire est celui de la France en novembre
// (UTC+1), le seul du banc : la trame n'a pas le fuseau du spot et l'appareil le prend de son horloge.
//
// Les variantes d'ÉTAT (vigilance orange ou rouge, vigilances illisibles, données vieillies, gardées, anciennes, absentes, sans marée,
// sans horloge, surf « bon ») partent d'une vraie trame RÉÉCRITE à la main dans la scène (aucune capture du jour ne les porte : au moment
// de la capture, aucun avertissement et aucun surf « bon »). Elles montrent chaque état que la page sait dessiner ; elles ne disent rien
// de ce que Vigie rendra.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_mer.cpp main/ui_meteo_icones.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: mer
//
// La 7B n'a pas été essayée : le rendu 1024x600 n'est qu'une simulation de sa taille. Ce rendu n'est pas une photo de la carte.
// Les textes allemands, néerlandais, espagnols, italiens et turcs de la page sont écrits avec une IA, pas relus par des locuteurs.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "banc.h"
#include "ecran/mer_trame.hpp"
#include "lvgl.h"
#include "ui_mer.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using sv = std::string_view;
namespace mer = ecran::mer;
namespace vigie = ecran::vigie;

// Les vraies trames (octets du fil).
constexpr sv HOSSEGOR =
#include "../../../test/host/donnees/mer_hossegor.inc"
    ;
constexpr sv LA_TORCHE =
#include "../../../test/host/donnees/mer_la_torche.inc"
    ;
constexpr sv ROCHEBONNE =
#include "../../../test/host/donnees/mer_rochebonne.inc"
    ;
constexpr sv PORT_VIEUX =
#include "../../../test/host/donnees/mer_port_vieux.inc"
    ;
constexpr sv GOVELLE =
#include "../../../test/host/donnees/mer_govelle.inc"
    ;
constexpr sv ONDARRAITZ =
#include "../../../test/host/donnees/mer_ondarraitz.inc"
    ;

constexpr std::int32_t DECALAGE_S = 3600; // UTC+1 : la France en novembre
constexpr std::int64_t APRES_S = 180;     // la page est ouverte 3 minutes après la trame

mer::Mer g_mer; // ≈ 1,5 Ko : hors pile, et la page en garde sa propre copie

const mer::Mer &lire(sv trame) {
    g_mer = mer::Mer{};
    mer::decoder(trame, g_mer);
    return g_mer;
}

void page(const banc::Contexte &c, const mer::Mer &m, std::int64_t maintenant_s, int detail = -1) {
    lv_obj_t *racine = lv_obj_create(nullptr);
    ecran::ContexteMer ctx;
    ctx.racine = racine;
    ctx.ttf = roboto_700_start;
    ctx.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    ctx.largeur = c.largeur;
    ctx.hauteur = c.hauteur;
    ecran::DonneesMer d;
    d.mer = &m;
    d.maintenant_s = maintenant_s;
    d.decalage_s = DECALAGE_S;
    if (!ecran::ui_mer_creer(ctx, d)) {
        return;
    }
    if (detail >= 0) {
        ecran::ui_mer_detail(detail);
    }
    lv_screen_load(racine);
}

// Une trame fraîche, comme à l'ouverture.
void fraiche(const banc::Contexte &c, sv trame, int detail = -1) {
    const mer::Mer &m = lire(trame);
    page(c, m, m.genere_s + APRES_S, detail);
}

// ---- les vraies trames --------------------------------------------------------------------------------------------------------------------
void hossegor(const banc::Contexte &c) { fraiche(c, HOSSEGOR); }
void la_torche(const banc::Contexte &c) { fraiche(c, LA_TORCHE); }
void rochebonne(const banc::Contexte &c) { fraiche(c, ROCHEBONNE); }
void port_vieux(const banc::Contexte &c) { fraiche(c, PORT_VIEUX); }
void govelle(const banc::Contexte &c) { fraiche(c, GOVELLE); }
void ondarraitz(const banc::Contexte &c) { fraiche(c, ONDARRAITZ); }

// Le détail d'une carte (un appui) sur de vraies trames.
void detail_surf(const banc::Contexte &c) { fraiche(c, LA_TORCHE, 0); }
void detail_kite(const banc::Contexte &c) { fraiche(c, LA_TORCHE, 1); }
void detail_bateau(const banc::Contexte &c) { fraiche(c, HOSSEGOR, 2); }

// ---- les états, sur une vraie trame réécrite --------------------------------------------------------------------------------------------------

// Donne à l'activité un verdict « bon » aux heures de jour : un surf comme on le voudrait, avec une note pleine.
void rendre_bonne(mer::Prevision &a, std::int64_t premiere_heure_s) {
    a.verdict = mer::Verdict::Bon;
    for (int h = 0; h < mer::HEURES; ++h) {
        if (a.lumiere[h] == mer::Lumiere::Jour) {
            a.heures[h] = (h % 5 == 4) ? mer::Verdict::Possible : mer::Verdict::Bon;
        }
    }
    a.creneau_debut_s = premiere_heure_s + 9 * 3600;
    a.creneau_fin_s = premiere_heure_s + 13 * 3600;
    a.raison_critere = mer::Critere::Inconnu;
    a.raison_etat = mer::Etat::Inconnu;
    for (int i = 0; i < a.nb_criteres; ++i) {
        a.criteres[i].verdict = mer::Verdict::Bon;
        if (a.criteres[i].points_max > 0) {
            a.criteres[i].points = a.criteres[i].points_max;
        }
    }
    a.points = a.sur;
}

void surf_bon(const banc::Contexte &c) {
    mer::Mer m = lire(LA_TORCHE);
    rendre_bonne(m.activites[0], m.premiere_heure_s);
    page(c, m, m.genere_s + APRES_S);
}

void surf_bon_detail(const banc::Contexte &c) {
    mer::Mer m = lire(LA_TORCHE);
    rendre_bonne(m.activites[0], m.premiere_heure_s);
    page(c, m, m.genere_s + APRES_S, 0);
}

void avec_vigilance(const banc::Contexte &c, vigie::Niveau niveau, bool lues) {
    mer::Mer m = lire(HOSSEGOR);
    m.vigilances_lues = lues;
    m.vigilance_presente = lues;
    m.vigilance_niveau = lues ? niveau : vigie::Niveau::Inconnu;
    m.vigilance_fin_s = lues ? m.premiere_heure_s + 15 * 3600 : 0;
    std::strncpy(m.vigilance_source, "Météo-France", sizeof m.vigilance_source - 1);
    page(c, m, m.genere_s + APRES_S);
}
void vigilance_orange(const banc::Contexte &c) { avec_vigilance(c, vigie::Niveau::Orange, true); }
void vigilance_rouge(const banc::Contexte &c) { avec_vigilance(c, vigie::Niveau::Rouge, true); }
void vigilances_illisibles(const banc::Contexte &c) { avec_vigilance(c, vigie::Niveau::Inconnu, false); }

// Ni vigilance ni état à dire : le bandeau montre la commune la plus proche, discrètement.
void commune(const banc::Contexte &c) {
    mer::Mer m = lire(HOSSEGOR);
    std::strncpy(m.lieu, "Soorts-Hossegor", sizeof m.lieu - 1);
    page(c, m, m.genere_s + APRES_S);
}

void donnees_vieillies(const banc::Contexte &c) {
    const mer::Mer &m = lire(HOSSEGOR);
    page(c, m, m.genere_s + 22 * 60);
}

void donnees_gardees(const banc::Contexte &c) {
    mer::Mer m = lire(HOSSEGOR);
    m.donnees_gardees = true;
    page(c, m, m.genere_s + 22 * 60);
}

void donnees_anciennes(const banc::Contexte &c) {
    const mer::Mer &m = lire(HOSSEGOR);
    page(c, m, m.genere_s + 7 * 3600);
}

void sans_donnees(const banc::Contexte &c) {
    g_mer = mer::Mer{};
    page(c, g_mer, c.instant_utc_s);
}

void sans_horloge(const banc::Contexte &c) {
    const mer::Mer &m = lire(HOSSEGOR);
    page(c, m, 0);
}

// Aucun modèle de marée près du spot : le panneau le dit, et la pêche à pied (qui s'en sert) ne se juge pas ici.
void sans_maree(const banc::Contexte &c) {
    mer::Mer m = lire(HOSSEGOR);
    m.nb_marees = 0;
    m.nb_coefficients = 0;
    mer::Prevision &peche = m.activites[3];
    peche.disponible = false;
    peche.verdict = mer::Verdict::Inconnu;
    peche.nb_criteres = 0;
    for (int h = 0; h < mer::HEURES; ++h) {
        peche.heures[h] = mer::Verdict::Inconnu;
    }
    page(c, m, m.genere_s + APRES_S);
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"hossegor", "Soorts-Hossegor : trame réelle du 08/11 (surf déconseillé, grande marée)", hossegor},
    {"la-torche", "La Torche : trame réelle (surf et kite possibles)", la_torche},
    {"rochebonne", "Saint-Malo : trame réelle (très grand marnage)", rochebonne},
    {"port-vieux", "Biarritz : trame réelle (plage abritée)", port_vieux},
    {"govelle", "Le Pouliguen : trame réelle", govelle},
    {"ondarraitz", "Hendaye : trame réelle", ondarraitz},
    {"detail-surf", "Détail du surf (La Torche, trame réelle)", detail_surf},
    {"detail-kite", "Détail du kite (La Torche, trame réelle)", detail_kite},
    {"detail-bateau", "Détail du bateau (Hossegor, trame réelle)", detail_bateau},
    {"surf-bon", "Surf « bon » (trame réécrite)", surf_bon},
    {"surf-bon-detail", "Détail du surf « bon » (trame réécrite)", surf_bon_detail},
    {"vigilance-orange", "Vigilance côtière orange (trame réécrite)", vigilance_orange},
    {"vigilance-rouge", "Vigilance côtière rouge (trame réécrite)", vigilance_rouge},
    {"vigilances-illisibles", "Vigilances illisibles : jamais un « tout va bien » (trame réécrite)",
     vigilances_illisibles},
    {"commune", "Rien à dire : la commune la plus proche (trame réécrite)", commune},
    {"donnees-vieillies", "Données de 22 minutes : cartes atténuées, âge dit", donnees_vieillies},
    {"donnees-gardees", "Un fournisseur a servi sa dernière réponse", donnees_gardees},
    {"donnees-anciennes", "Données de 7 heures : plus aucun verdict", donnees_anciennes},
    {"sans-donnees", "Aucune donnée de mer", sans_donnees},
    {"sans-horloge", "Heure non réglée : ni âge, ni prochaine marée, ni repère", sans_horloge},
    {"sans-maree", "Aucun modèle de marée ici : pêche à pied indisponible", sans_maree},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
