// Scène « auto » : l'accueil (main/ui_principal.cpp, compilé tel quel) dans ses états du MODE AUTOMATIQUE (ADR-0015, ADR-0023) : la
// pastille « Auto · … » à côté du lieu, la carte pluie qui recouvre les cinq jours, le bandeau d'alerte agrandi (orange ou rouge) avec sa
// tuile de pluie et les cinq jours réduits à une bande, le profil épinglé.
//
// Les variantes « roue-profil-* » ajoutent la roue d'appui long (main/ui_roue.cpp) avec sa famille « Profil ▸ » (main/ui_roue_profil.h,
// le même fichier que l'appareil) : le second anneau offre « Auto » et « Météo », le profil en vigueur marqué ; la variante
// « parcours-profil » rejoue le geste (appui long, Profil, Météo, puis Auto) et s'arrête en ECHEC si l'accueil ne suit pas.
//
// La décision n'est pas écrite à la main : chaque variante passe par le VRAI pilote (accueil_pilote.hpp), avec les mêmes mesures que le
// branchement de l'appareil (main/ecran_principal.cpp), donc ce que ce banc montre est ce que le noyau décide pour ces entrées.
//
// À rendre avec -Instant 2026-11-08T07:45:00Z (dimanche 08 h 45 en France), l'heure de l'esquisse. Fixtures inventées, pas des mesures.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_meteo_icones.cpp main/horloge.cpp main/ui_roue.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: auto
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "banc.h"
#include "ecran/accueil_epinglage.hpp"
#include "ecran/accueil_pilote.hpp"
#include "ecran/roue_entrees.hpp"
#include "ecran/visuel_textes.hpp"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_principal.h"
#include "ui_roue.h"
#include "ui_roue_profil.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");
extern const uint8_t mdi_roue_start[] asm("_binary_mdi_roue_ttf_start");
extern const uint8_t mdi_roue_end[] asm("_binary_mdi_roue_ttf_end");

namespace {

using namespace donnees;
using ecran::vigie::Niveau;
using ecran::visuel::VUE_QUARTS_MAX;

// Les quarts d'heure d'une pluie qui commence dans `dans_min` minutes, comme l'appareil les garde (ADR-0027) : le quart écoulé d'abord, neuf en
// tout. À partir du début : 0,2 / 0,4 / 0,8 / 1,2 / 0,9 / 0,5 / 0,2 / 0,1 mm par quart d'heure (de faible à fort, puis ça s'éteint).
// Un quart d'heure est pluvieux en entier ou pas du tout (l'appareil lit « il pleut » dès que le quart en cours l'est, meteo_riche.hpp) :
// la pluie commence donc au premier début de quart d'heure à `dans_min` minutes ou plus. Rend les minutes réelles jusqu'à ce début (0 : il
// pleut), pour que `pluie_dans_min` dise la même chose que les quarts, comme sur l'appareil.
int avec_quarts(VueMeteo &v, int64_t maintenant_s, int dans_min) {
    static const int dix[VUE_QUARTS_MAX] = {2, 4, 8, 12, 9, 5, 2, 1, 0};
    v.nb_quarts = VUE_QUARTS_MAX;
    v.quarts_debut_s = (maintenant_s / 900) * 900 - 900;
    const int64_t debut_pluie_s = maintenant_s + static_cast<int64_t>(dans_min) * 60;
    const int64_t premier_s = dans_min == 0 ? (maintenant_s / 900) * 900 : ((debut_pluie_s + 899) / 900) * 900;
    for (int i = 0; i < VUE_QUARTS_MAX; ++i) {
        const int64_t t0 = v.quarts_debut_s + static_cast<int64_t>(i) * 900;
        const int k = static_cast<int>((t0 - premier_s) / 900);
        v.quarts_dix_mm[i] = t0 < premier_s ? 0 : (k < VUE_QUARTS_MAX ? dix[k] : 0);
    }
    return premier_s <= maintenant_s ? 0 : static_cast<int>((premier_s - maintenant_s + 59) / 60);
}

// La météo de l'esquisse, avec de la pluie qui commence dans `dans_min` minutes : à partir de l'heure courante 0,3 / 0,8 / 1,6 / 3,1 /
// 1,2 / 0,4 mm (un orage qui monte puis s'éteint), puis plus rien.
VueMeteo avec_pluie_dans(int64_t maintenant_s, int dans_min) {
    static const int mm[12] = {3, 8, 16, 31, 12, 4, 0, 0, 0, 0, 0, 0};
    VueMeteo v = vue_esquisse(maintenant_s);
    int total = 0;
    for (int i = 0; i < v.nb_heures; ++i) {
        const int dix = i < 12 ? mm[i] : 0;
        v.heures[i].pluie_dix_mm = dix;
        v.heures[i].proba_pct = dix > 0 ? 85 : 5;
        v.heures[i].code = dix >= 20 ? 81 : (dix > 0 ? 61 : 3);
        total += dix;
    }
    const int reel_min =
        avec_quarts(v, maintenant_s, dans_min); // les quarts décident (arrondi au quart d'heure suivant)
    v.pluie_dans_min = reel_min;
    v.pluie_dix_mm = mm[0];
    v.code = reel_min == 0 ? 61 : 3;
    v.proba_pluie_pct = 85;
    v.jours[0].pluie_dix_mm = total;
    v.jours[0].proba_pct = 85;
    v.jours[0].code = 81;
    return v;
}

// Une alerte officielle de `niveau`, de 14 h à 22 h ce jour-là (heures de France, hiver : UTC + 1), `autres` autres en vigueur.
ecran::vigie::Bandeau alerte(int64_t maintenant_s, Niveau niveau, const char *titre, int autres = 0) {
    ecran::vigie::Bandeau b = alerte_vent(maintenant_s, autres);
    b.niveau = niveau;
    std::snprintf(b.texte, sizeof b.texte, "%s", titre);
    return b;
}

[[noreturn]] void echec(const char *quoi) {
    std::fprintf(stderr, "ECHEC de la scène auto : %s\n", quoi);
    std::exit(1);
}

void exiger(bool condition, const char *quoi) {
    if (!condition) {
        echec(quoi);
    }
}

// Le pilote, avec les mêmes mesures que main/ecran_principal.cpp. `epingle` : un profil Météo épinglé (le réglage « Profil »).
ecran::EntreesPrincipal decider(ecran::EntreesPrincipal e, bool epingle = false) {
    ecran::accueil::Pilote pilote;
    ecran::accueil::Mesures m;
    m.maintenant_s = e.maintenant_s;
    m.fuseau = e.fuseau;
    m.fuseau_connu = e.fuseau_connu;
    m.position_connue = e.position_connue;
    m.latitude_deg = e.latitude_deg;
    m.longitude_deg = e.longitude_deg;
    m.lieu = ecran::accueil::cle_du_lieu(e.meteo.ville, e.latitude_deg, e.longitude_deg);
    m.meteo_connue = e.meteo.connue;
    m.meteo_perimee = e.meteo.perimee;
    m.pluie_dans_min = e.meteo.pluie_dans_min;
    m.niveau_alerte = e.bandeau.present ? e.bandeau.niveau : Niveau::Inconnu;
    m.epingle = epingle;
    m.profil_epingle = ecran::accueil::Profil::Meteo;
    e.mode_auto = pilote.avancer(m);
    return e;
}

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

// Rien de proche : « Auto · Météo », l'accueil d'avant (la pluie de 13 h est dite par la puce du bandeau).
void defaut(const banc::Contexte &c) { afficher(c, decider(entrees(c, 0, vue_esquisse(c.instant_utc_s)))); }

// La pluie arrive dans 12 minutes ou plus, au prochain début de quart d'heure (15 min à 08 h 45) : « Auto · pluie proche », et la bande de
// pluie dit « Pluie dans 15 min ».
void pluie_proche(const banc::Contexte &c) {
    afficher(c, decider(entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 12))));
}

// Il pleut déjà : « Pluie en cours ».
void pluie_en_cours(const banc::Contexte &c) {
    afficher(c, decider(entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 0))));
}

// Les quarts d'heure RÉELS d'Open-Meteo pour la maison le 09/11/2026 à 12 h 18 (voir docs/mesures/bande-pluie.md) : le quart écoulé 1,9 mm,
// puis 0,7 / 0,1 / 0,1 mm, puis sec. À rendre avec -Instant 2026-11-09T11:18:00Z.
void bande_reelle(const banc::Contexte &c) {
    VueMeteo v = avec_pluie_dans(c.instant_utc_s, 0);
    static const int reel[VUE_QUARTS_MAX] = {19, 7, 1, 1, 0, 0, 0, 0, 0};
    for (int i = 0; i < VUE_QUARTS_MAX; ++i) {
        v.quarts_dix_mm[i] = reel[i];
    }
    afficher(c, decider(entrees(c, 0, v)));
}

// La pluie ne commence que dans 95 minutes (dans la bande de deux heures, mais pas « dans l'heure ») : « Pluie dès … », pas de carte.
void bande_plus_tard(const banc::Contexte &c) {
    afficher(c, decider(entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 95))));
}

// Il pleut jusqu'au bout de la bande : pas de « fin dans N min » (la fin n'est pas dans les données).
void bande_longue(const banc::Contexte &c) {
    VueMeteo v = avec_pluie_dans(c.instant_utc_s, 0);
    for (int i = 0; i < VUE_QUARTS_MAX; ++i) {
        v.quarts_dix_mm[i] = 3 + (i % 3);
    }
    afficher(c, decider(entrees(c, 0, v)));
}

// Une alerte jaune (pas agrandie) et la pluie proche : la bande de pluie, l'alerte garde la moitié droite du bandeau.
void jaune_et_pluie(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 12));
    e.bandeau = alerte(c.instant_utc_s, Niveau::Jaune, "Vent violent");
    afficher(c, decider(e));
}

// Alerte orange, pluie seulement plus tard : « Auto · alerte orange », grande carte d'alerte, tuile « Pluie dès 13 h ».
void alerte_orange(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau = alerte(c.instant_utc_s, Niveau::Orange, "Vent violent");
    afficher(c, decider(e));
}

// Alerte orange sans aucune pluie à dire : la grande carte prend toute la largeur.
void alerte_orange_sans_pluie(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, sans_pluie(vue_esquisse(c.instant_utc_s)));
    e.bandeau = alerte(c.instant_utc_s, Niveau::Orange, "Vent violent");
    afficher(c, decider(e));
}

// Alerte rouge, deux autres alertes en vigueur ET de la pluie au prochain quart d'heure : les deux règles sont vraies, l'alerte passe avant.
void alerte_rouge_et_pluie(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 12));
    e.bandeau = alerte(c.instant_utc_s, Niveau::Rouge, "Orages violents", 2);
    afficher(c, decider(e));
}

// Un titre d'émetteur très long : deux lignes au plus, coupé par « … ».
void alerte_titre_long(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau =
        alerte(c.instant_utc_s, Niveau::Orange,
               "Vigilance orange vents violents : rafales de 110 km/h attendues sur le littoral du Morbihan et du "
               "Finistère, risque de chutes d'arbres et de coupures d'électricité en fin de journée");
    afficher(c, decider(e));
}

// Alerte orange et météo périmée : la grande carte reste, la tuile dit « Données de 07:52 · périmées ».
void alerte_perimee(const banc::Contexte &c) {
    VueMeteo v = vue_esquisse(c.instant_utc_s);
    v.perimee = true;
    v.instant_s = c.instant_utc_s - 53 * 60;
    ecran::EntreesPrincipal e = entrees(c, 0, v);
    e.bandeau = alerte(c.instant_utc_s, Niveau::Orange, "Vent violent");
    afficher(c, decider(e));
}

// Profil épinglé et pluie proche : « Météo · épinglé » + « Auto en pause », pas de carte pluie.
void epingle(const banc::Contexte &c) {
    afficher(c, decider(entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 12)), true));
}

// Épinglé ET alerte orange : le bandeau agrandi reste (une alerte officielle ne cède jamais sa place), la pastille dit « épinglé ».
void epingle_et_alerte(const banc::Contexte &c) {
    ecran::EntreesPrincipal e = entrees(c, 0, vue_esquisse(c.instant_utc_s));
    e.bandeau = alerte(c.instant_utc_s, Niveau::Orange, "Vent violent");
    afficher(c, decider(e, true));
}

// Un nom de lieu très long : la pastille garde sa place (le lieu est coupé avant).
void ville_longue(const banc::Contexte &c) {
    VueMeteo v = avec_pluie_dans(c.instant_utc_s, 12);
    std::snprintf(v.ville, sizeof v.ville, "Villeneuve-d'Ascq-Lez-Marcq-en-Bar\xC5\x93l");
    afficher(c, decider(entrees(c, 0, v)));
}

// Un lieu court et « Exemple » (météo du bouchon) : les deux indications de la barre côte à côte.
void exemple(const banc::Contexte &c) {
    VueMeteo v = avec_pluie_dans(c.instant_utc_s, 12);
    v.exemple = true;
    afficher(c, decider(entrees(c, 0, v)));
}

// ---- la roue d'appui long et sa famille « Profil » ----------------------------------------------------------------------------------

using ecran::reglages::ProfilAccueil;

// Le réglage du banc : ce que « Auto » et « Météo » écrivent ici (sur l'appareil : le réglage « Profil » en NVS).
ProfilAccueil g_profil = ProfilAccueil::Auto;
int g_choix_profil = 0;
void choisir_auto() {
    g_profil = ProfilAccueil::Auto;
    ++g_choix_profil;
}
void choisir_meteo() {
    g_profil = ProfilAccueil::Meteo;
    ++g_choix_profil;
}
void rien() {}

// Le premier anneau de l'appareil (main/ecran_principal.cpp, composer_entrees) : QUELLES entrées et dans quel ordre vient du même noyau
// pur (ecran/roue_entrees.hpp) : « Réglages », « Détails », « Alertes » quand une alerte est en vigueur, « Mer » quand un spot est choisi,
// puis la famille « Profil ». Les quatre premières ne font rien ici : la scène juge le second anneau.
int composer(ecran::EntreeRoue *sortie, bool alertes, bool mer, ecran::ChoixRoue *choix) {
    ecran::ActionsProfil actions;
    actions.choisir[static_cast<std::size_t>(ProfilAccueil::Auto)] = choisir_auto;
    actions.choisir[static_cast<std::size_t>(ProfilAccueil::Meteo)] = choisir_meteo;
    const int nb_profil = ecran::composer_choix_profil(g_profil, actions, choix);
    const ecran::roue::Liste liste =
        ecran::roue::entrees_du_premier_anneau({alertes, mer, ecran::roue::profil_a_proposer(nb_profil)});
    int n = 0;
    for (int i = 0; i < liste.n; ++i) {
        switch (liste.entree[i]) {
        case ecran::roue::Entree::Reglages:
            sortie[n++] = {ecran::visuel::texte_c("roue|Réglages"), ecran::ICONE_REGLAGES, rien, nullptr, 0};
            break;
        case ecran::roue::Entree::Details:
            sortie[n++] = {ecran::visuel::texte_c("Détails"), ecran::ICONE_DETAILS, rien, nullptr, 0};
            break;
        case ecran::roue::Entree::Alertes:
            sortie[n++] = {ecran::visuel::texte_c("roue|Alertes"), ecran::ICONE_ALERTES, rien, nullptr, 0};
            break;
        case ecran::roue::Entree::Mer:
            sortie[n++] = {ecran::visuel::texte_c("Mer"), ecran::ICONE_MER, rien, nullptr, 0};
            break;
        case ecran::roue::Entree::Profil:
            sortie[n++] = ecran::entree_profil(choix, nb_profil);
            break;
        }
    }
    return n;
}

// L'accueil que la décision du noyau donne pour le réglage `g_profil`, avec ou sans alerte orange.
ecran::EntreesPrincipal accueil_du_profil(const banc::Contexte &c, bool alerte_orange) {
    ecran::EntreesPrincipal e = entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 12));
    if (alerte_orange) {
        e.bandeau = alerte(c.instant_utc_s, Niveau::Orange, "Vent violent");
    }
    return decider(e, ecran::accueil::epinglage_de(g_profil).epingle);
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

void tourner() {
    for (int i = 0; i < 3; ++i) {
        for (lv_timer_t *t = lv_timer_get_next(nullptr); t != nullptr; t = lv_timer_get_next(t)) {
            lv_timer_ready(t);
        }
        lv_timer_handler();
    }
}

// Ouvre la roue au point (fx, fy) en millièmes de l'écran, puis, si `deplier`, touche « Profil » comme le doigt. Rend le nombre d'entrées.
int ouvrir_roue(const banc::Contexte &c, int fx, int fy, bool alertes, bool mer, bool deplier) {
    ecran::EntreeRoue entrees_roue[ecran::ENTREES_ROUE_MAX] = {};
    ecran::ChoixRoue choix[ecran::reglages::NOMBRE_PROFILS_ACCUEIL] = {};
    const int n = composer(entrees_roue, alertes, mer, choix);
    const int32_t x = c.largeur * fx / 1000;
    const int32_t y = c.hauteur * fy / 1000;
    exiger(ecran::ui_roue_ouvrir(contexte_roue(c), entrees_roue, n, x, y), "la roue n'a pas pu s'ouvrir");
    if (deplier) {
        int32_t bx = 0;
        int32_t by = 0;
        exiger(ecran::ui_roue_centre_bouton(n - 1, &bx, &by), "le bouton « Profil » n'a pas de centre");
        exiger(ecran::ui_roue_simuler_toucher(bx, by), "toucher « Profil » ferme la roue au lieu de la déplier");
    }
    ecran::PositionRoue position = {};
    exiger(ecran::ui_roue_position(&position), "la roue ouverte n'a pas de position");
    std::fprintf(stderr, "roue-profil %dx%d, doigt en (%d, %d), %d entrée(s), %s : %s\n", static_cast<int>(c.largeur),
                 static_cast<int>(c.hauteur), static_cast<int>(x), static_cast<int>(y), n,
                 deplier ? "dépliée" : "repliée", position.tient ? "tient" : "NE TIENT PAS");
    return n;
}

void roue_profil(const banc::Contexte &c, ProfilAccueil profil, int fx, int fy, bool alertes, bool mer, bool deplier) {
    g_profil = profil;
    afficher(c, accueil_du_profil(c, alertes));
    ouvrir_roue(c, fx, fy, alertes, mer, deplier);
}

// Le premier anneau : Réglages, Détails, Profil ▸ (aucune alerte). La famille est repliée.
void roue_profil_replie(const banc::Contexte &c) { roue_profil(c, ProfilAccueil::Auto, 500, 500, false, false, false); }
// Le second anneau : Auto (marqué) et Météo.
void roue_profil_auto(const banc::Contexte &c) { roue_profil(c, ProfilAccueil::Auto, 500, 500, false, false, true); }
// Météo épinglé : l'accueil dit « Météo · épinglé », sans carte pluie, et le second anneau marque Météo.
void roue_profil_epingle(const banc::Contexte &c) {
    roue_profil(c, ProfilAccueil::Meteo, 500, 500, false, false, true);
}
// Avec une alerte orange : quatre boutons au premier anneau.
void roue_profil_alerte(const banc::Contexte &c) { roue_profil(c, ProfilAccueil::Auto, 500, 500, true, false, true); }
// Près d'un coin et d'un bord : le second anneau pivote pour tenir.
void roue_profil_coin(const banc::Contexte &c) { roue_profil(c, ProfilAccueil::Meteo, 950, 930, true, false, true); }
void roue_profil_bord(const banc::Contexte &c) { roue_profil(c, ProfilAccueil::Auto, 50, 500, false, false, true); }
// Le plus plein : alerte en vigueur ET spot de mer choisi, soit cinq boutons au premier anneau (Réglages, Détails, Alertes, Mer, Profil).
void roue_profil_complet(const banc::Contexte &c) { roue_profil(c, ProfilAccueil::Auto, 500, 500, true, true, true); }
// Les cinq boutons, repliés : le profil épinglé n'apparaît qu'au second anneau.
void roue_profil_complet_replie(const banc::Contexte &c) {
    roue_profil(c, ProfilAccueil::Meteo, 500, 500, true, true, false);
}
// Les cinq boutons près du coin bas droit.
void roue_profil_complet_coin(const banc::Contexte &c) {
    roue_profil(c, ProfilAccueil::Meteo, 950, 930, true, true, true);
}

// Le geste de bout en bout : appui long (la roue), « Profil », « Météo » ferme la roue et épingle ; l'accueil que le noyau décide ne montre plus
// la carte pluie ; la roue se rouvre avec Météo marqué ; « Auto » dépingle et la carte pluie revient tout de suite.
void parcours_profil(const banc::Contexte &c) {
    g_profil = ProfilAccueil::Auto;
    g_choix_profil = 0;
    ecran::EntreesPrincipal e = accueil_du_profil(c, false);
    exiger(!e.mode_auto.epingle && e.mode_auto.carte_pluie, "Auto avec de la pluie proche doit montrer la carte pluie");
    afficher(c, e);

    int32_t x = 0;
    int32_t y = 0;
    ecran::EntreeRoue entrees_roue[ecran::ENTREES_ROUE_MAX] = {};
    ecran::ChoixRoue choix[ecran::reglages::NOMBRE_PROFILS_ACCUEIL] = {};
    int n = composer(entrees_roue, false, false, choix);
    exiger(n == 3 && entrees_roue[n - 1].nb_choix == 2,
           "le premier anneau n'a pas « Profil » avec deux choix à la suite");
    n = composer(entrees_roue, true, true, choix);
    exiger(n == 5 && entrees_roue[n - 1].nb_choix == 2 && entrees_roue[n - 2].nb_choix == 0,
           "avec une alerte et un spot de mer, « Profil » n'est pas le cinquième bouton, après « Mer »");
    exiger(choix[0].actif && !choix[1].actif, "« Auto » n'est pas marqué quand rien n'est épinglé");

    ouvrir_roue(c, 500, 500, false, false, true);
    exiger(ecran::ui_roue_centre_choix(1, &x, &y), "le second anneau n'a pas de second choix (Météo)");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Météo » laisse la roue ouverte");
    tourner();
    exiger(g_choix_profil == 1 && g_profil == ProfilAccueil::Meteo,
           "toucher « Météo » n'écrit pas le profil exactement une fois");
    exiger(ecran::ui_roue_cout().objets == 0, "la roue fermée garde un objet LVGL");
    e = accueil_du_profil(c, false);
    exiger(e.mode_auto.epingle && !e.mode_auto.carte_pluie,
           "épinglé : l'accueil doit dire « épinglé » et ne plus montrer la carte pluie");
    afficher(c, e);

    n = composer(entrees_roue, false, false, choix);
    exiger(!choix[0].actif && choix[1].actif, "« Météo » n'est pas marqué une fois épinglé");
    ouvrir_roue(c, 500, 500, false, false, true);
    exiger(ecran::ui_roue_centre_choix(0, &x, &y), "le second anneau n'a pas de premier choix (Auto)");
    exiger(!ecran::ui_roue_simuler_toucher(x, y), "toucher « Auto » laisse la roue ouverte");
    tourner();
    exiger(g_choix_profil == 2 && g_profil == ProfilAccueil::Auto, "toucher « Auto » n'écrit pas le profil");
    e = accueil_du_profil(c, false);
    exiger(!e.mode_auto.epingle && e.mode_auto.carte_pluie,
           "dépinglé : les règles reprennent tout de suite (carte pluie)");

    // Une alerte orange reste affichée, épinglé ou non : l'alerte officielle ne cède jamais sa place.
    g_profil = ProfilAccueil::Meteo;
    e = accueil_du_profil(c, true);
    exiger(e.mode_auto.epingle && e.mode_auto.bandeau_agrandi, "épinglé : le bandeau d'alerte agrandi doit rester");
    afficher(c, e);
    g_profil = ProfilAccueil::Auto;
    (void)n;
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"defaut", "Auto · Météo (rien de proche)", defaut},
    {"pluie-proche", "Auto · pluie proche : la bande de pluie", pluie_proche},
    {"pluie-en-cours", "Il pleut : la bande de pluie", pluie_en_cours},
    {"bande-reelle", "La bande avec les quarts d'heure réels du 09/11 à 12 h 18", bande_reelle},
    {"bande-plus-tard", "La bande, pluie dans 95 min", bande_plus_tard},
    {"bande-longue", "La bande, il pleut jusqu'au bout (pas de fin)", bande_longue},
    {"jaune-et-pluie", "Alerte jaune + pluie proche : la bande sur la moitié gauche", jaune_et_pluie},
    {"alerte-orange", "Auto · alerte orange : bandeau agrandi", alerte_orange},
    {"alerte-orange-sans-pluie", "Alerte orange, aucune pluie à dire", alerte_orange_sans_pluie},
    {"alerte-rouge-et-pluie", "Alerte rouge + 2 autres + pluie au prochain quart d'heure", alerte_rouge_et_pluie},
    {"alerte-titre-long", "Alerte orange, titre très long", alerte_titre_long},
    {"alerte-perimee", "Alerte orange, météo périmée", alerte_perimee},
    {"epingle", "Météo · épinglé + Auto en pause", epingle},
    {"epingle-et-alerte", "Épinglé + alerte orange", epingle_et_alerte},
    {"ville-longue", "Lieu très long et pastille", ville_longue},
    {"exemple", "Pastille et « Exemple »", exemple},
    {"roue-profil", "Roue : Réglages, Détails, Profil ▸ (repliée)", roue_profil_replie},
    {"roue-profil-auto", "Roue : second anneau de Profil, Auto marqué", roue_profil_auto},
    {"roue-profil-epingle", "Roue : second anneau de Profil, Météo marqué, accueil épinglé", roue_profil_epingle},
    {"roue-profil-alerte", "Roue : quatre boutons (alerte en vigueur), second anneau de Profil", roue_profil_alerte},
    {"roue-profil-coin", "Roue : second anneau de Profil près du coin bas droit", roue_profil_coin},
    {"roue-profil-bord", "Roue : second anneau de Profil près du bord gauche", roue_profil_bord},
    {"roue-profil-complet", "Roue : cinq boutons (alerte et mer), second anneau de Profil", roue_profil_complet},
    {"roue-profil-complet-replie", "Roue : cinq boutons repliés (alerte et mer, Météo épinglé)",
     roue_profil_complet_replie},
    {"roue-profil-complet-coin", "Roue : cinq boutons, second anneau de Profil près du coin bas droit",
     roue_profil_complet_coin},
    {"parcours-profil", "Parcours : appui long, Profil, Météo (épinglé), Auto (dépinglé)", parcours_profil},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
