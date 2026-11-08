// Scène « auto » : l'accueil (main/ui_principal.cpp, compilé tel quel) dans ses états du MODE AUTOMATIQUE (ADR-0015, ADR-0023) : la
// pastille « Auto · … » à côté du lieu, la carte pluie qui recouvre les cinq jours, le bandeau d'alerte agrandi (orange ou rouge) avec sa
// tuile de pluie et les cinq jours réduits à une bande, le profil épinglé.
//
// La décision n'est pas écrite à la main : chaque variante passe par le VRAI pilote (accueil_pilote.hpp), avec les mêmes mesures que le
// branchement de l'appareil (main/ecran_principal.cpp), donc ce que ce banc montre est ce que le noyau décide pour ces entrées.
//
// À rendre avec -Instant 2026-11-08T07:45:00Z (dimanche 08 h 45 en France), l'heure de l'esquisse. Fixtures inventées, pas des mesures.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_meteo_icones.cpp main/horloge.cpp
// rendu-langues: fr en de nl es it tr
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 1024x600 720x1280
// rendu-sequence: auto
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "banc.h"
#include "ecran/accueil_pilote.hpp"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_principal.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using namespace donnees;
using ecran::vigie::Niveau;

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
    v.pluie_dans_min = dans_min;
    v.pluie_dix_mm = mm[0];
    v.code = dans_min == 0 ? 61 : 3;
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

// Le pilote, avec les mêmes mesures que main/ecran_principal.cpp. `epingle` : un profil Météo épinglé (le réglage « Profil » de la suite).
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

// La pluie arrive dans 12 minutes : « Auto · pluie proche », la carte pluie recouvre les cinq jours.
void pluie_proche(const banc::Contexte &c) {
    afficher(c, decider(entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 12))));
}

// Il pleut déjà : « Pluie en cours ».
void pluie_en_cours(const banc::Contexte &c) {
    afficher(c, decider(entrees(c, 0, avec_pluie_dans(c.instant_utc_s, 0))));
}

// Une alerte jaune (pas agrandie) et la pluie proche : la carte pluie, l'alerte garde sa bande en pleine largeur.
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

// Alerte rouge, deux autres alertes en vigueur ET de la pluie dans 12 minutes : les deux règles sont vraies, l'alerte passe avant.
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

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"defaut", "Auto · Météo (rien de proche)", defaut},
    {"pluie-proche", "Auto · pluie proche : la carte pluie", pluie_proche},
    {"pluie-en-cours", "Il pleut : la carte pluie", pluie_en_cours},
    {"jaune-et-pluie", "Alerte jaune + pluie proche", jaune_et_pluie},
    {"alerte-orange", "Auto · alerte orange : bandeau agrandi", alerte_orange},
    {"alerte-orange-sans-pluie", "Alerte orange, aucune pluie à dire", alerte_orange_sans_pluie},
    {"alerte-rouge-et-pluie", "Alerte rouge + 2 autres + pluie dans 12 min", alerte_rouge_et_pluie},
    {"alerte-titre-long", "Alerte orange, titre très long", alerte_titre_long},
    {"alerte-perimee", "Alerte orange, météo périmée", alerte_perimee},
    {"epingle", "Météo · épinglé + Auto en pause", epingle},
    {"epingle-et-alerte", "Épinglé + alerte orange", epingle_et_alerte},
    {"ville-longue", "Lieu très long et pastille", ville_longue},
    {"exemple", "Pastille et « Exemple »", exemple},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
