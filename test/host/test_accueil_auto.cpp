// Tests du mode automatique de l'accueil : accueil_auto.hpp (ADR-0015).
//
// Aucune donnée réelle : la fonction ne lit que des entrées déjà calculées (booléens, minutes, dixièmes de km/h, instants). Chaque
// cas est écrit à la main, son résultat attendu est ce que dit l'ADR (pas ce que rend le code) ; deux tests relisent la règle autrement
// (une table de toutes les combinaisons d'entrées, une suite d'une journée à la minute) pour que l'oubli d'un croisement se voie.
// Les instants sont ceux du dimanche 8 novembre 2026 à Paris (CET, UTC+1) : valeurs d'exemple, pas un lever de soleil constaté.
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "ecran/accueil_auto.hpp"
#include "mini.hpp"

using namespace ecran::accueil;
using ecran::vigie::Niveau;

namespace {

// 2026-11-08 00:00 à Paris = 2026-11-07 23:00 UTC.
constexpr std::int64_t MINUIT = 1'794'092'400;
constexpr std::int32_t PARIS_HIVER = 3600;

constexpr std::int64_t heure(int h, int m = 0, int s = 0) { return MINUIT + h * 3600 + m * 60 + s; }

constexpr std::int64_t LEVER = heure(8, 0);
constexpr std::int64_t COUCHER = heure(17, 30);

// Une maison à Paris, 9 h : dans le créneau du matin (8 h à 10 h), rien d'autre de reçu ni de réglé.
constexpr Entrees base() {
    Entrees e;
    e.maintenant_s = heure(9, 0);
    e.decalage_local_s = PARIS_HIVER;
    e.lever_s = LEVER;
    e.coucher_s = COUCHER;
    return e;
}

// Tout ce qu'il faut pour Surf, et seulement lui.
constexpr Entrees surf_possible() {
    Entrees e = base();
    e.spot_surf = true;
    e.activite_surf = true;
    e.fraicheur_mer = Fraicheur::Fraiche;
    return e;
}

// Tout ce qu'il faut pour Vent·Bateau, et seulement lui : seuil de 25 km/h, vent de 30 km/h.
constexpr Entrees vent_possible() {
    Entrees e = base();
    e.activite_vent_bateau = true;
    e.seuil_vent_dixiemes_kmh = 250;
    e.vent_dixiemes_kmh = 300;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    return e;
}

// Tout ce qu'il faut pour Côte, et seulement lui : basse mer dans 1 h 30, hors créneau de surf (12 h).
constexpr Entrees cote_possible() {
    Entrees e = base();
    e.maintenant_s = heure(12, 0);
    e.spot_peche_a_pied = true;
    e.basse_mer_dans_min = 90;
    e.fraicheur_mer = Fraicheur::Fraiche;
    return e;
}

constexpr bool meme_etat(const EtatAuto &a, const EtatAuto &b) {
    return a.profil == b.profil && a.changement_connu == b.changement_connu && a.change_a_s == b.change_a_s;
}

// ---- les constantes -----------------------------------------------------------------------------------------------------------

constexpr int les_seuils_publics_sont_ceux_de_l_adr() {
    VERIFIER(ALERTE_NIVEAU_MIN == Niveau::Orange);
    VERIFIER(PLUIE_PROCHE_MAX_MIN == 60);
    VERIFIER(CRENEAU_MATIN_FIN_S == 10 * 3600 && CRENEAU_SOIR_DEBUT_S == 16 * 3600);
    VERIFIER(BASSE_MER_MAX_MIN == 180);
    VERIFIER(CHANGEMENT_ECART_MIN_S == 1800);
    VERIFIER(JOUR_S == 86400);
    VERIFIER(VALEUR_INCONNUE == -1);
    VERIFIER(NOMBRE_PROFILS == 4 && static_cast<int>(Profil::Cote) == 3);
    // Les valeurs des profils seront enregistrées en NVS (profil épinglé) : elles ne bougent pas.
    VERIFIER(static_cast<int>(Profil::Meteo) == 0 && static_cast<int>(Profil::Surf) == 1);
    VERIFIER(static_cast<int>(Profil::VentBateau) == 2);
    VERIFIER(NOMBRE_RAISONS == static_cast<std::size_t>(Raison::ChangementDiffere) + 1);
    return 0;
}

// ---- entrées vides, entrées inconnues -----------------------------------------------------------------------------------------

constexpr int entrees_vides_donnent_meteo_sans_rien_d_autre() {
    const Decision d = decider(Entrees{});
    VERIFIER(d.profil == Profil::Meteo);
    VERIFIER(d.raison == Raison::Defaut && d.raison_profil == Raison::Defaut);
    VERIFIER(!d.bandeau_agrandi && !d.carte_pluie);
    VERIFIER(meme_etat(d.etat, EtatAuto{}));
    return 0;
}

constexpr int rien_d_inconnu_ne_declenche_une_bascule() {
    // Surf : chacune des données dont il dépend, une à une, inconnue.
    Entrees e = surf_possible();
    VERIFIER(decider(e).profil == Profil::Surf);
    e = surf_possible();
    e.fraicheur_mer = Fraicheur::Inconnue;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = surf_possible();
    e.lever_s = 0;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = surf_possible();
    e.coucher_s = 0;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = surf_possible();
    e.maintenant_s = 0;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    // Vent·Bateau : vent inconnu, seuil non réglé, prévisions de fraîcheur inconnue.
    e = vent_possible();
    VERIFIER(decider(e).profil == Profil::VentBateau);
    e.vent_dixiemes_kmh = VALEUR_INCONNUE;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = vent_possible();
    e.seuil_vent_dixiemes_kmh = 0;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = vent_possible();
    e.fraicheur_meteo = Fraicheur::Inconnue;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    // Côte : basse mer inconnue, données de mer de fraîcheur inconnue.
    e = cote_possible();
    VERIFIER(decider(e).profil == Profil::Cote);
    e.basse_mer_dans_min = VALEUR_INCONNUE;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = cote_possible();
    e.fraicheur_mer = Fraicheur::Inconnue;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    // Alerte et pluie : niveau inconnu, minutes inconnues.
    e = base();
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.niveau_alerte = Niveau::Inconnu;
    e.pluie_dans_min = VALEUR_INCONNUE;
    VERIFIER(!decider(e).bandeau_agrandi && !decider(e).carte_pluie);
    return 0;
}

// ---- règle 1 : l'alerte officielle ----------------------------------------------------------------------------------------------

constexpr int alerte_orange_et_rouge_agrandissent_le_bandeau_pas_en_dessous() {
    Entrees e = base();
    e.niveau_alerte = Niveau::Orange;
    Decision d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.raison == Raison::AlerteOrange);
    e.niveau_alerte = Niveau::Rouge;
    d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.raison == Raison::AlerteRouge);
    for (Niveau n : {Niveau::Inconnu, Niveau::Vert, Niveau::Jaune}) {
        e.niveau_alerte = n;
        d = decider(e);
        VERIFIER(!d.bandeau_agrandi && d.raison == Raison::Defaut);
    }
    VERIFIER(alerte_agrandit_le_bandeau(Niveau::Orange) && alerte_agrandit_le_bandeau(Niveau::Rouge));
    VERIFIER(!alerte_agrandit_le_bandeau(Niveau::Jaune) && !alerte_agrandit_le_bandeau(Niveau::Vert));
    VERIFIER(!alerte_agrandit_le_bandeau(Niveau::Inconnu));
    return 0;
}

constexpr int une_alerte_n_est_pas_un_changement_de_profil() {
    Entrees e = surf_possible();
    e.niveau_alerte = Niveau::Orange;
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison_profil == Raison::Surf); // le profil que les règles 3 à 6 voulaient
    VERIFIER(d.bandeau_agrandi && d.raison == Raison::AlerteOrange);       // la pastille, elle, dit l'alerte
    e = base();
    e.niveau_alerte = Niveau::Rouge;
    d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison_profil == Raison::Defaut);
    return 0;
}

constexpr int l_alerte_agrandit_meme_epinglee_retenue_ou_pendant_un_toucher() {
    // Épinglé : le profil épinglé et la pastille d'épinglage, le bandeau agrandi quand même.
    Entrees e = base();
    e.niveau_alerte = Niveau::Orange;
    e.epingle = true;
    e.profil_epingle = Profil::Cote;
    Decision d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.profil == Profil::Cote && d.raison == Raison::Epingle);
    // Pendant un toucher : le profil attend, pas le bandeau.
    e = surf_possible();
    e.niveau_alerte = Niveau::Orange;
    e.toucher_en_cours = true;
    d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.profil == Profil::Meteo && d.raison_profil == Raison::ChangementDiffere);
    VERIFIER(d.raison == Raison::AlerteOrange);
    // Pendant le délai de 30 min : idem.
    e = surf_possible();
    e.niveau_alerte = Niveau::Rouge;
    e.etat = EtatAuto{Profil::Meteo, true, e.maintenant_s - 60};
    d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.profil == Profil::Meteo && d.raison == Raison::AlerteRouge);
    // Données périmées : l'alerte vient de Vigie, elle a sa propre fraîcheur (déjà jugée par l'appelant).
    e = surf_possible();
    e.niveau_alerte = Niveau::Orange;
    e.fraicheur_mer = Fraicheur::Perimee;
    VERIFIER(decider(e).bandeau_agrandi);
    // Un profil autre que Météo est montré (état rendu par l'appel précédent) : le bandeau agrandi ne dépend pas de lui.
    for (Profil p : {Profil::Surf, Profil::VentBateau, Profil::Cote}) {
        e = base();
        e.niveau_alerte = Niveau::Rouge;
        e.etat = EtatAuto{p, true, e.maintenant_s - 60};
        d = decider(e);
        VERIFIER(d.bandeau_agrandi && d.raison == Raison::AlerteRouge);
    }
    return 0;
}

// ---- règle 2 : la pluie proche ---------------------------------------------------------------------------------------------------

constexpr int pluie_a_59_minutes_oui_a_60_et_61_non() {
    Entrees e = base();
    e.fraicheur_meteo = Fraicheur::Fraiche;
    for (int m : {0, 1, 12, 58, 59}) {
        e.pluie_dans_min = m;
        const Decision d = decider(e);
        VERIFIER(d.carte_pluie && d.raison == Raison::PluieProche);
        VERIFIER(d.profil == Profil::Meteo && d.raison_profil == Raison::Defaut);
    }
    for (int m : {60, 61, 90, 1000, -1, -2, -100}) {
        e.pluie_dans_min = m;
        const Decision d = decider(e);
        VERIFIER(!d.carte_pluie && d.raison == Raison::Defaut);
    }
    VERIFIER(pluie_proche(Fraicheur::Fraiche, 59) && !pluie_proche(Fraicheur::Fraiche, 60));
    return 0;
}

constexpr int la_carte_pluie_veut_des_previsions_fraiches() {
    Entrees e = base();
    e.pluie_dans_min = 10;
    VERIFIER(!decider(e).carte_pluie); // fraîcheur inconnue
    e.fraicheur_meteo = Fraicheur::Perimee;
    VERIFIER(!decider(e).carte_pluie && decider(e).raison == Raison::Defaut);
    e.fraicheur_meteo = Fraicheur::Fraiche;
    VERIFIER(decider(e).carte_pluie);
    return 0;
}

constexpr int un_profil_epingle_n_a_pas_de_carte_pluie() {
    Entrees e = base();
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.pluie_dans_min = 5;
    e.epingle = true;
    e.profil_epingle = Profil::Surf;
    const Decision d = decider(e);
    VERIFIER(!d.carte_pluie && d.profil == Profil::Surf && d.raison == Raison::Epingle);
    return 0;
}

constexpr int la_carte_pluie_recouvre_le_profil_sans_le_changer() {
    Entrees e = surf_possible();
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.pluie_dans_min = 20;
    const Decision d = decider(e);
    VERIFIER(d.carte_pluie && d.profil == Profil::Surf);
    VERIFIER(d.raison == Raison::PluieProche && d.raison_profil == Raison::Surf);
    VERIFIER(d.etat.profil == Profil::Surf && d.etat.changement_connu); // le profil continue dessous
    return 0;
}

constexpr int l_alerte_passe_avant_la_pluie_dans_la_pastille() {
    Entrees e = base();
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.pluie_dans_min = 20;
    e.niveau_alerte = Niveau::Orange;
    const Decision d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.carte_pluie); // les deux s'ajoutent à l'écran
    VERIFIER(d.raison == Raison::AlerteOrange);   // la pastille dit la plus forte
    return 0;
}

// ---- règle 3 : surf et créneau de sortie -----------------------------------------------------------------------------------------

constexpr int le_creneau_du_matin_va_du_lever_a_10_h_bornes_a_la_seconde() {
    const auto dans = [](std::int64_t t) { return creneau_de_sortie(t, PARIS_HIVER, LEVER, COUCHER); };
    VERIFIER(!dans(LEVER - 1));
    VERIFIER(dans(LEVER));
    VERIFIER(dans(heure(9, 0)));
    VERIFIER(dans(heure(9, 59, 59)));
    VERIFIER(!dans(heure(10, 0, 0)));
    VERIFIER(!dans(heure(10, 0, 1)));
    return 0;
}

constexpr int le_creneau_du_soir_va_de_16_h_au_coucher_bornes_a_la_seconde() {
    const auto dans = [](std::int64_t t) { return creneau_de_sortie(t, PARIS_HIVER, LEVER, COUCHER); };
    VERIFIER(!dans(heure(15, 59, 59)));
    VERIFIER(dans(heure(16, 0, 0)));
    VERIFIER(dans(heure(17, 0)));
    VERIFIER(dans(COUCHER - 1));
    VERIFIER(!dans(COUCHER));
    VERIFIER(!dans(COUCHER + 1));
    return 0;
}

constexpr int hors_creneau_midi_nuit_et_avant_le_lever() {
    const auto dans = [](std::int64_t t) { return creneau_de_sortie(t, PARIS_HIVER, LEVER, COUCHER); };
    VERIFIER(!dans(heure(12, 0)));
    VERIFIER(!dans(heure(14, 30)));
    VERIFIER(!dans(heure(22, 0)));
    VERIFIER(!dans(heure(23, 59, 59)));
    VERIFIER(!dans(heure(3, 0))); // avant 10 h mais avant le lever : pas de créneau
    VERIFIER(!dans(heure(0, 0)));
    return 0;
}

constexpr int le_creneau_veut_un_lever_et_un_coucher_du_jour_local() {
    const std::int64_t t = heure(9, 0);
    VERIFIER(creneau_de_sortie(t, PARIS_HIVER, LEVER, COUCHER));
    // Inconnus (jour ou nuit polaire, soleil non calculé) : pas de créneau.
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, 0, COUCHER));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, LEVER, 0));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, -5, COUCHER));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, LEVER, -5));
    // Heure inconnue.
    VERIFIER(!creneau_de_sortie(0, PARIS_HIVER, LEVER, COUCHER));
    VERIFIER(!creneau_de_sortie(-3600, PARIS_HIVER, LEVER, COUCHER));
    // Un lever ou un coucher d'un autre jour est inconnu (un lever d'hier n'ouvre pas le créneau de 3 h du matin).
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, LEVER - JOUR_S, COUCHER));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, LEVER, COUCHER + JOUR_S));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, LEVER, COUCHER - JOUR_S));
    // Les deux sont exigés du jour local, même quand seul l'autre sert à la fenêtre du moment (le soir ne lit pas le lever).
    VERIFIER(creneau_de_sortie(heure(16, 30), PARIS_HIVER, LEVER, COUCHER));
    VERIFIER(!creneau_de_sortie(heure(16, 30), PARIS_HIVER, LEVER + JOUR_S, COUCHER));
    VERIFIER(!creneau_de_sortie(heure(16, 30), PARIS_HIVER, LEVER - JOUR_S, COUCHER));
    VERIFIER(!creneau_de_sortie(heure(3, 0), PARIS_HIVER, LEVER - JOUR_S, COUCHER - JOUR_S));
    // Les bornes du jour local : minuit pile est du jour, minuit du lendemain n'en est pas.
    VERIFIER(creneau_de_sortie(t, PARIS_HIVER, MINUIT, COUCHER));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, MINUIT - 1, COUCHER));
    VERIFIER(creneau_de_sortie(t, PARIS_HIVER, LEVER, MINUIT + JOUR_S - 1));
    VERIFIER(!creneau_de_sortie(t, PARIS_HIVER, LEVER, MINUIT + JOUR_S));
    return 0;
}

constexpr int le_creneau_lit_les_heures_dans_le_decalage_du_lieu() {
    // Le même instant (9 h 30 à Paris l'hiver) est 10 h 30 à Paris l'été : hors créneau.
    const std::int64_t t = heure(9, 30);
    VERIFIER(creneau_de_sortie(t, 3600, LEVER, COUCHER));
    VERIFIER(!creneau_de_sortie(t, 7200, LEVER, COUCHER));
    // New York l'hiver (UTC-5, décalage négatif) : minuit local = 05:00 UTC.
    const std::int64_t ny = MINUIT + 6 * 3600; // 2026-11-08 00:00 EST = 05:00 UTC
    const std::int64_t lever_ny = ny + 6 * 3600 + 50 * 60;
    const std::int64_t coucher_ny = ny + 16 * 3600 + 45 * 60;
    VERIFIER(creneau_de_sortie(ny + 7 * 3600, -18000, lever_ny, coucher_ny));            // 7 h : matin
    VERIFIER(!creneau_de_sortie(ny + 10 * 3600, -18000, lever_ny, coucher_ny));          // 10 h : fini
    VERIFIER(creneau_de_sortie(ny + 16 * 3600 + 10 * 60, -18000, lever_ny, coucher_ny)); // 16 h 10 : soir
    VERIFIER(!creneau_de_sortie(ny + 15 * 3600, -18000, lever_ny, coucher_ny));          // 15 h : entre les deux
    // Le même instant lu avec le décalage de Paris n'est pas dans le jour du lever de New York : pas de créneau.
    VERIFIER(!creneau_de_sortie(ny + 7 * 3600, 3600, lever_ny, coucher_ny));
    // Horloge à peine réglée (quelques secondes après 1970) avec un décalage négatif : le jour local est la veille au soir, le
    // reste de la division est négatif et doit être ramené dans la journée (19 h 16 min 40 s le 31/12/1969, lever à 19 h 01).
    VERIFIER(creneau_de_sortie(1000, -18000, 100, 4600));
    // Lever, coucher et heure à 0 ou moins sont « inconnus » même quand le calcul du jour local les placerait dans la journée
    // (avec ces décalages, minuit local tombe avant l'an 1970 : le jour local contient 0 et les petits négatifs).
    VERIFIER(creneau_de_sortie(1000, 5000, 100, 4000));   // témoin : matin, tout est connu
    VERIFIER(!creneau_de_sortie(1000, 5000, 0, 4000));    // lever inconnu
    VERIFIER(!creneau_de_sortie(1000, 5000, 100, 0));     // coucher inconnu
    VERIFIER(!creneau_de_sortie(1000, -18000, -5, 4600)); // lever négatif
    VERIFIER(!creneau_de_sortie(-1000, 60000, 100,
                                4000)); // heure négative : inconnue, même si 16 h 23 locales tombent dans le soir
    VERIFIER(!creneau_de_sortie(1000, -18000, 100, 900)); // coucher déjà passé (900 s < 1000 s)
    return 0;
}

constexpr int surf_demande_spot_activite_donnees_fraiches_et_creneau() {
    Entrees e = surf_possible();
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf && d.raison_profil == Raison::Surf);
    VERIFIER(d.etat.profil == Profil::Surf && d.etat.changement_connu && d.etat.change_a_s == e.maintenant_s);
    VERIFIER(!d.bandeau_agrandi && !d.carte_pluie);
    // Le soir aussi.
    e.maintenant_s = heure(16, 30);
    VERIFIER(decider(e).profil == Profil::Surf);
    // Un seul élément en moins : plus de Surf.
    e = surf_possible();
    e.spot_surf = false;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = surf_possible();
    e.activite_surf = false;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = surf_possible();
    e.maintenant_s = heure(12, 0); // hors créneau
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    return 0;
}

constexpr int une_maison_sans_spot_ne_bascule_jamais_vers_surf_ni_cote() {
    // Tout le reste est là, y compris l'autre activité et des données fraîches.
    Entrees e = surf_possible();
    e.spot_surf = false;
    e.spot_peche_a_pied = false;
    e.basse_mer_dans_min = 30;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    VERIFIER(decider(e).profil == Profil::Meteo);
    e.maintenant_s = heure(12, 0);
    VERIFIER(decider(e).profil == Profil::Meteo);
    // Vent·Bateau n'a pas besoin de spot.
    e = vent_possible();
    e.spot_surf = false;
    e.spot_peche_a_pied = false;
    VERIFIER(decider(e).profil == Profil::VentBateau);
    return 0;
}

// ---- règle 4 : vent et bateau ---------------------------------------------------------------------------------------------------

constexpr int vent_bateau_veut_un_vent_strictement_au_dessus_du_seuil() {
    Entrees e = vent_possible(); // seuil 250, vent 300
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::VentBateau && d.raison == Raison::VentBateau && d.raison_profil == Raison::VentBateau);
    e.vent_dixiemes_kmh = 251;
    VERIFIER(decider(e).profil == Profil::VentBateau);
    e.vent_dixiemes_kmh = 250; // égal au seuil : pas au-dessus
    VERIFIER(decider(e).profil == Profil::Meteo);
    e.vent_dixiemes_kmh = 249;
    VERIFIER(decider(e).profil == Profil::Meteo);
    e.vent_dixiemes_kmh = 0;
    VERIFIER(decider(e).profil == Profil::Meteo);
    VERIFIER(vent_au_dessus_du_seuil(251, 250) && !vent_au_dessus_du_seuil(250, 250));
    return 0;
}

constexpr int vent_bateau_veut_l_activite_cochee_et_un_seuil_regle() {
    Entrees e = vent_possible();
    e.activite_vent_bateau = false;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    for (std::int32_t seuil : {0, -1, -250}) { // non réglé : jamais « tout vent est au-dessus »
        e = vent_possible();
        e.seuil_vent_dixiemes_kmh = seuil;
        e.vent_dixiemes_kmh = 900;
        VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    }
    e = vent_possible();
    e.seuil_vent_dixiemes_kmh = 1; // le plus petit seuil réglé : un vent de 0,2 km/h le dépasse
    e.vent_dixiemes_kmh = 2;
    VERIFIER(decider(e).profil == Profil::VentBateau);
    e.vent_dixiemes_kmh = -1;
    VERIFIER(decider(e).profil == Profil::Meteo);
    return 0;
}

// ---- règle 5 : côte et basse mer ----------------------------------------------------------------------------------------------

constexpr int cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises() {
    Entrees e = cote_possible();
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Cote && d.raison == Raison::Cote && d.raison_profil == Raison::Cote);
    for (int m : {0, 1, 179, 180}) {
        e.basse_mer_dans_min = m;
        VERIFIER(decider(e).profil == Profil::Cote);
    }
    for (int m : {181, 182, 600, -1, -30}) {
        e.basse_mer_dans_min = m;
        VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    }
    VERIFIER(basse_mer_proche(180) && !basse_mer_proche(181) && basse_mer_proche(0) && !basse_mer_proche(-1));
    return 0;
}

constexpr int cote_veut_un_spot_de_peche_a_pied_pas_d_activite() {
    Entrees e = cote_possible();
    e.spot_peche_a_pied = false;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = cote_possible();
    e.activite_surf = false; // aucune activité cochée : Côte n'en demande pas
    e.activite_vent_bateau = false;
    VERIFIER(decider(e).profil == Profil::Cote);
    return 0;
}

// ---- l'ordre des règles 3 à 6 ----------------------------------------------------------------------------------------------------

constexpr int surf_puis_vent_bateau_puis_cote_puis_meteo() {
    Entrees e = surf_possible();
    e.activite_vent_bateau = true;
    e.seuil_vent_dixiemes_kmh = 250;
    e.vent_dixiemes_kmh = 300;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.spot_peche_a_pied = true;
    e.basse_mer_dans_min = 90;
    // Les trois conviennent : Surf.
    VERIFIER(decider(e).profil == Profil::Surf);
    // Plus de Surf : Vent·Bateau.
    e.spot_surf = false;
    VERIFIER(decider(e).profil == Profil::VentBateau && decider(e).raison == Raison::VentBateau);
    // Plus de vent : Côte.
    e.vent_dixiemes_kmh = 100;
    VERIFIER(decider(e).profil == Profil::Cote && decider(e).raison == Raison::Cote);
    // Plus de basse mer : Météo.
    e.basse_mer_dans_min = 600;
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    return 0;
}

constexpr int surf_passe_avant_cote_et_vent_bateau_passe_avant_cote_deux_a_deux() {
    Entrees e = surf_possible();
    e.spot_peche_a_pied = true;
    e.basse_mer_dans_min = 10;
    VERIFIER(decider(e).profil == Profil::Surf);
    e = vent_possible();
    e.spot_peche_a_pied = true;
    e.basse_mer_dans_min = 10;
    e.fraicheur_mer = Fraicheur::Fraiche;
    VERIFIER(decider(e).profil == Profil::VentBateau);
    e = surf_possible();
    e.activite_vent_bateau = true;
    e.seuil_vent_dixiemes_kmh = 250;
    e.vent_dixiemes_kmh = 300;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    VERIFIER(decider(e).profil == Profil::Surf);
    return 0;
}

constexpr int toutes_les_regles_croisees_alerte_pluie_et_surf() {
    Entrees e = surf_possible();
    e.niveau_alerte = Niveau::Rouge;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.pluie_dans_min = 15;
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison_profil == Raison::Surf);
    VERIFIER(d.bandeau_agrandi && d.carte_pluie && d.raison == Raison::AlerteRouge);
    return 0;
}

// ---- donnée périmée -----------------------------------------------------------------------------------------------------------------

constexpr int une_donnee_de_mer_perimee_ramene_a_meteo_tout_de_suite() {
    Entrees e = surf_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.etat = EtatAuto{Profil::Surf, true,
                      e.maintenant_s - 300}; // Surf choisi il y a 5 min : le délai de 30 min ne protège pas
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison == Raison::DonneesPerimees &&
             d.raison_profil == Raison::DonneesPerimees);
    VERIFIER(d.etat.profil == Profil::Meteo && d.etat.changement_connu && d.etat.change_a_s == e.maintenant_s);
    return 0;
}

constexpr int une_donnee_meteo_perimee_ramene_vent_bateau_a_meteo() {
    Entrees e = vent_possible();
    e.fraicheur_meteo = Fraicheur::Perimee;
    e.etat = EtatAuto{Profil::VentBateau, true, e.maintenant_s - 60};
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison == Raison::DonneesPerimees);
    return 0;
}

constexpr int une_donnee_de_mer_perimee_ramene_cote_a_meteo() {
    Entrees e = cote_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.etat = EtatAuto{Profil::Cote, true, e.maintenant_s - 60};
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison == Raison::DonneesPerimees);
    return 0;
}

constexpr int inconnue_n_est_pas_perimee() {
    Entrees e = surf_possible();
    e.fraicheur_mer = Fraicheur::Inconnue;
    e.etat =
        EtatAuto{Profil::Surf, true, e.maintenant_s - 4000}; // délai écoulé : le retour à Météo se fait, par défaut
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison == Raison::Defaut && d.raison_profil == Raison::Defaut);
    return 0;
}

constexpr int perimee_sans_les_autres_conditions_n_est_qu_un_defaut() {
    // La donnée est périmée, mais le créneau est fini : ce n'est pas elle qui empêche Surf.
    Entrees e = surf_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.maintenant_s = heure(12, 0);
    VERIFIER(decider(e).profil == Profil::Meteo && decider(e).raison == Raison::Defaut);
    e = surf_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.spot_surf = false;
    VERIFIER(decider(e).raison == Raison::Defaut);
    e = vent_possible();
    e.fraicheur_meteo = Fraicheur::Perimee;
    e.vent_dixiemes_kmh = 100; // plus au-dessus du seuil
    VERIFIER(decider(e).raison == Raison::Defaut);
    return 0;
}

constexpr int une_donnee_perimee_laisse_la_place_au_profil_suivant_dont_la_donnee_est_fraiche() {
    // Surf voudrait de la houle fraîche (périmée), Vent·Bateau lit les prévisions (fraîches) : Vent·Bateau, pas Météo.
    Entrees e = surf_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.activite_vent_bateau = true;
    e.seuil_vent_dixiemes_kmh = 250;
    e.vent_dixiemes_kmh = 300;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    VERIFIER(decider(e).profil == Profil::VentBateau && decider(e).raison == Raison::VentBateau);
    return 0;
}

constexpr int la_peremption_attend_la_fin_du_toucher() {
    Entrees e = surf_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.etat = EtatAuto{Profil::Surf, true, e.maintenant_s - 300};
    e.toucher_en_cours = true;
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison_profil == Raison::ChangementDiffere);
    e.toucher_en_cours = false;
    d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison == Raison::DonneesPerimees);
    return 0;
}

constexpr int la_peremption_d_une_famille_ne_touche_pas_l_autre() {
    // Les données de mer périmées ne chassent pas Vent·Bateau, qui lit les prévisions.
    Entrees e = vent_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.spot_surf = true;
    e.activite_surf = true;
    e.etat = EtatAuto{Profil::VentBateau, true, e.maintenant_s - 60};
    VERIFIER(decider(e).profil == Profil::VentBateau);
    // Et les prévisions périmées ne chassent pas Côte (marée), ni Surf.
    e = cote_possible();
    e.fraicheur_meteo = Fraicheur::Perimee;
    VERIFIER(decider(e).profil == Profil::Cote);
    e = surf_possible();
    e.fraicheur_meteo = Fraicheur::Perimee;
    VERIFIER(decider(e).profil == Profil::Surf);
    return 0;
}

// ---- garde-fou : un changement au plus toutes les 30 minutes ---------------------------------------------------------------------------

constexpr int pas_de_changement_a_29_min_59_s_changement_a_30_min() {
    Entrees e = surf_possible();
    e.etat = EtatAuto{Profil::Meteo, true, e.maintenant_s - 1799};
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison_profil == Raison::ChangementDiffere &&
             d.raison == Raison::ChangementDiffere);
    VERIFIER(meme_etat(d.etat, e.etat)); // rien n'est noté : le prochain changement part toujours de l'ancien
    e.etat.change_a_s = e.maintenant_s - 1800;
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf);
    VERIFIER(meme_etat(d.etat, EtatAuto{Profil::Surf, true, e.maintenant_s}));
    e.etat.change_a_s = e.maintenant_s - 1801;
    VERIFIER(decider(e).profil == Profil::Surf);
    e.etat.change_a_s = e.maintenant_s - 1;
    VERIFIER(decider(e).profil == Profil::Meteo);
    e.etat.change_a_s = e.maintenant_s; // à la même seconde
    VERIFIER(decider(e).profil == Profil::Meteo);
    return 0;
}

constexpr int le_premier_choix_est_libre() {
    Entrees e = surf_possible(); // état par défaut : aucun changement noté
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf);
    VERIFIER(meme_etat(d.etat, EtatAuto{Profil::Surf, true, e.maintenant_s}));
    return 0;
}

constexpr int le_meme_profil_ne_renouvelle_pas_l_horodatage() {
    Entrees e = surf_possible();
    e.etat = EtatAuto{Profil::Surf, true, e.maintenant_s - 100};
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf); // pas « changement différé » : rien n'est différé
    VERIFIER(meme_etat(d.etat, e.etat));
    return 0;
}

constexpr int un_changement_en_attente_est_pris_des_la_fin_du_delai() {
    // Météo -> Surf à 9 h 00 ; à 9 h 10 Vent·Bateau convient mieux : attendre 9 h 30. À 9 h 20, retour au défaut voulu : attendre aussi.
    Entrees e = surf_possible();
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf);
    e.etat = d.etat;
    e.maintenant_s = heure(9, 10);
    e.spot_surf = false;
    e.activite_vent_bateau = true;
    e.seuil_vent_dixiemes_kmh = 250;
    e.vent_dixiemes_kmh = 300;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison_profil == Raison::ChangementDiffere);
    e.etat = d.etat;
    e.maintenant_s = heure(9, 29, 59);
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf);
    e.etat = d.etat;
    e.maintenant_s = heure(9, 30, 0);
    d = decider(e);
    VERIFIER(d.profil == Profil::VentBateau && d.etat.change_a_s == heure(9, 30, 0));
    // Le changement suivant attend 10 h 00.
    e.etat = d.etat;
    e.activite_vent_bateau = false;
    e.maintenant_s = heure(9, 59, 59);
    VERIFIER(decider(e).profil == Profil::VentBateau);
    e.maintenant_s = heure(10, 0, 0);
    VERIFIER(decider(e).profil == Profil::Meteo);
    return 0;
}

constexpr int une_horloge_revenue_en_arriere_ne_bloque_pas_les_changements() {
    Entrees e = surf_possible();
    e.etat = EtatAuto{Profil::Meteo, true, e.maintenant_s + 5000}; // le dernier changement serait dans le futur
    VERIFIER(decider(e).profil == Profil::Surf);
    VERIFIER(delai_ecoule(e.etat, e.maintenant_s));
    VERIFIER(!delai_ecoule(EtatAuto{Profil::Meteo, true, 1000}, 1000 + 1799));
    VERIFIER(delai_ecoule(EtatAuto{Profil::Meteo, true, 1000}, 1000 + 1800));
    VERIFIER(delai_ecoule(EtatAuto{Profil::Meteo, false, 1000}, 1000));
    return 0;
}

constexpr int sans_heure_le_profil_ne_change_pas() {
    Entrees e = vent_possible();
    e.maintenant_s = 0;
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison_profil == Raison::ChangementDiffere);
    VERIFIER(meme_etat(d.etat, EtatAuto{}));
    e.maintenant_s = -1;
    VERIFIER(decider(e).profil == Profil::Meteo);
    // Un profil déjà en place reste, sans heure aussi.
    e.maintenant_s = 0;
    e.etat = EtatAuto{Profil::VentBateau, true, 100};
    d = decider(e);
    VERIFIER(d.profil == Profil::VentBateau && d.raison == Raison::VentBateau);
    // Même la péremption attend l'heure : sans elle, on ne note rien.
    e.fraicheur_meteo = Fraicheur::Perimee;
    VERIFIER(decider(e).profil == Profil::VentBateau);
    return 0;
}

constexpr int un_retour_a_meteo_ordinaire_attend_les_30_minutes_comme_tout_changement() {
    // Surf choisi à 9 h 55 ; à 10 h le créneau est fini : les règles veulent Météo (par défaut, sans donnée périmée), mais le
    // dernier changement date de 5 minutes. Seule la péremption passe devant le délai.
    Entrees e = surf_possible();
    e.maintenant_s = heure(10, 0);
    e.etat = EtatAuto{Profil::Surf, true, heure(9, 55)};
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison_profil == Raison::ChangementDiffere);
    VERIFIER(meme_etat(d.etat, e.etat));
    e.maintenant_s = heure(10, 25);
    d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison == Raison::Defaut);
    VERIFIER(meme_etat(d.etat, EtatAuto{Profil::Meteo, true, heure(10, 25)}));
    return 0;
}

// ---- garde-fou : jamais pendant un toucher --------------------------------------------------------------------------------------------

constexpr int le_profil_ne_change_jamais_pendant_un_toucher() {
    Entrees e = surf_possible();
    e.toucher_en_cours = true;
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.raison_profil == Raison::ChangementDiffere);
    VERIFIER(meme_etat(d.etat, EtatAuto{}));
    // Le doigt se lève : le changement se fait au prochain appel, sans nouveau délai.
    e.toucher_en_cours = false;
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf);
    // Le délai de 30 min écoulé ne suffit pas pendant un toucher.
    e = surf_possible();
    e.etat = EtatAuto{Profil::Meteo, true, e.maintenant_s - 7200};
    e.toucher_en_cours = true;
    VERIFIER(decider(e).profil == Profil::Meteo);
    return 0;
}

constexpr int un_toucher_ne_gele_ni_le_bandeau_ni_la_carte_pluie_ni_un_profil_inchange() {
    Entrees e = base();
    e.toucher_en_cours = true;
    e.niveau_alerte = Niveau::Orange;
    e.fraicheur_meteo = Fraicheur::Fraiche;
    e.pluie_dans_min = 30;
    Decision d = decider(e);
    VERIFIER(d.bandeau_agrandi && d.carte_pluie);
    // Le profil voulu est déjà celui qui est montré : aucune raison de dire « différé ».
    e = surf_possible();
    e.etat = EtatAuto{Profil::Surf, true, e.maintenant_s - 100};
    e.toucher_en_cours = true;
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf);
    return 0;
}

// ---- l'épinglage --------------------------------------------------------------------------------------------------------------------------

constexpr int un_profil_epingle_gagne_sur_toutes_les_regles() {
    for (Profil p : {Profil::Meteo, Profil::Surf, Profil::VentBateau, Profil::Cote}) {
        Entrees e = surf_possible(); // les règles voudraient Surf
        e.fraicheur_meteo = Fraicheur::Fraiche;
        e.pluie_dans_min = 10;
        e.epingle = true;
        e.profil_epingle = p;
        const Decision d = decider(e);
        VERIFIER(d.profil == p && d.raison == Raison::Epingle && d.raison_profil == Raison::Epingle);
        VERIFIER(!d.carte_pluie && !d.bandeau_agrandi);
        VERIFIER(d.etat.profil == p && !d.etat.changement_connu);
        // Le profil enregistré ne compte que si l'épinglage est actif : sans lui, les règles reprennent.
        e.epingle = false;
        VERIFIER(decider(e).profil == Profil::Surf && decider(e).raison != Raison::Epingle);
    }
    return 0;
}

constexpr int l_epinglage_ignore_le_delai_le_toucher_et_la_peremption() {
    Entrees e = surf_possible();
    e.fraicheur_mer = Fraicheur::Perimee;
    e.etat = EtatAuto{Profil::Surf, true, e.maintenant_s - 10};
    e.epingle = true;
    e.profil_epingle = Profil::Cote;
    e.toucher_en_cours = true; // la personne vient de choisir : elle a le doigt sur l'écran
    const Decision d = decider(e);
    VERIFIER(d.profil == Profil::Cote && d.raison == Raison::Epingle);
    // Épinglé sur un profil sans spot ni donnée : la personne l'a voulu.
    e = base();
    e.epingle = true;
    e.profil_epingle = Profil::Surf;
    VERIFIER(decider(e).profil == Profil::Surf);
    return 0;
}

constexpr int le_desepinglage_reprend_les_regles_tout_de_suite() {
    Entrees e = surf_possible();
    e.etat = EtatAuto{Profil::Surf, true, e.maintenant_s - 600};
    e.epingle = true;
    e.profil_epingle = Profil::Cote;
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Cote);
    // Une minute plus tard on désépingle : Surf revient sans attendre 30 min.
    e.etat = d.etat;
    e.epingle = false;
    e.maintenant_s += 60;
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf);
    VERIFIER(meme_etat(d.etat, EtatAuto{Profil::Surf, true, e.maintenant_s}));
    // Et si les règles voulaient le même profil que l'épinglé, rien ne bouge (aucune heure de changement inventée).
    e = surf_possible();
    e.epingle = true;
    e.profil_epingle = Profil::Surf;
    d = decider(e);
    e.etat = d.etat;
    e.epingle = false;
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf && !d.etat.changement_connu);
    return 0;
}

constexpr int une_valeur_hors_table_est_ignoree_jamais_une_erreur() {
    // Épinglage invalide : comme s'il n'y en avait pas.
    Entrees e = surf_possible();
    e.epingle = true;
    e.profil_epingle = static_cast<Profil>(7);
    Decision d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.raison == Raison::Surf);
    // État précédent invalide : on repart de Météo, sans changement noté.
    e = surf_possible();
    e.etat = EtatAuto{static_cast<Profil>(200), true, e.maintenant_s};
    d = decider(e);
    VERIFIER(d.profil == Profil::Surf && d.etat.profil == Profil::Surf);
    e = base();
    e.etat = EtatAuto{static_cast<Profil>(4), true, e.maintenant_s};
    d = decider(e);
    VERIFIER(d.profil == Profil::Meteo && d.etat.profil == Profil::Meteo && !d.etat.changement_connu);
    VERIFIER(profil_valide(Profil::Cote) && !profil_valide(static_cast<Profil>(4)));
    return 0;
}

// ---- deux relectures de la règle, autrement -------------------------------------------------------------------------------------------

// Toutes les combinaisons des entrées qui comptent, état neuf, heure connue, pas de toucher, rien d'épinglé : la table que dit
// l'ADR, relue ici sans en recopier le code (un tableau de conditions, de haut en bas).
constexpr int toutes_les_combinaisons_d_entrees_suivent_la_table_de_l_adr() {
    constexpr Fraicheur F[3] = {Fraicheur::Inconnue, Fraicheur::Fraiche, Fraicheur::Perimee};
    for (unsigned bits = 0; bits < 512; ++bits) {
        const bool spot_surf = (bits & 1u) != 0;
        const bool act_surf = (bits & 2u) != 0;
        const bool creneau = (bits & 4u) != 0;
        const bool act_vent = (bits & 8u) != 0;
        const bool vent_haut = (bits & 16u) != 0;
        const bool spot_peche = (bits & 32u) != 0;
        const bool basse = (bits & 64u) != 0;
        const bool pluie = (bits & 128u) != 0;
        const bool alerte = (bits & 256u) != 0;
        for (const Fraicheur mer : F) {
            for (const Fraicheur meteo : F) {
                Entrees e = base();
                e.maintenant_s = creneau ? heure(9, 0) : heure(12, 0);
                e.spot_surf = spot_surf;
                e.activite_surf = act_surf;
                e.activite_vent_bateau = act_vent;
                e.seuil_vent_dixiemes_kmh = 250;
                e.vent_dixiemes_kmh = vent_haut ? 251 : 250;
                e.spot_peche_a_pied = spot_peche;
                e.basse_mer_dans_min = basse ? 180 : 181;
                e.pluie_dans_min = pluie ? 59 : 60;
                e.niveau_alerte = alerte ? Niveau::Orange : Niveau::Jaune;
                e.fraicheur_mer = mer;
                e.fraicheur_meteo = meteo;

                const bool surf_ok = spot_surf && act_surf && creneau;
                const bool vent_ok = act_vent && vent_haut;
                const bool cote_ok = spot_peche && basse;
                Profil attendu = Profil::Meteo;
                Raison raison_attendue = Raison::Defaut;
                if (surf_ok && mer == Fraicheur::Fraiche) {
                    attendu = Profil::Surf;
                    raison_attendue = Raison::Surf;
                } else if (vent_ok && meteo == Fraicheur::Fraiche) {
                    attendu = Profil::VentBateau;
                    raison_attendue = Raison::VentBateau;
                } else if (cote_ok && mer == Fraicheur::Fraiche) {
                    attendu = Profil::Cote;
                    raison_attendue = Raison::Cote;
                } else if ((surf_ok && mer == Fraicheur::Perimee) || (vent_ok && meteo == Fraicheur::Perimee) ||
                           (cote_ok && mer == Fraicheur::Perimee)) {
                    raison_attendue = Raison::DonneesPerimees;
                }
                const bool carte = pluie && meteo == Fraicheur::Fraiche;

                const Decision d = decider(e);
                VERIFIER(d.profil == attendu);
                VERIFIER(d.raison_profil == raison_attendue);
                VERIFIER(d.bandeau_agrandi == alerte);
                VERIFIER(d.carte_pluie == carte);
                VERIFIER(d.raison == (alerte ? Raison::AlerteOrange : (carte ? Raison::PluieProche : raison_attendue)));
                VERIFIER(d.etat.profil == attendu);
                // Une maison sans aucun spot ne va jamais vers Surf ni Côte.
                if (!spot_surf && !spot_peche) {
                    VERIFIER(d.profil == Profil::Meteo || d.profil == Profil::VentBateau);
                }
                // La même entrée épinglée : le profil épinglé, jamais de carte pluie, le bandeau inchangé.
                e.epingle = true;
                e.profil_epingle = Profil::Cote;
                const Decision p = decider(e);
                VERIFIER(p.profil == Profil::Cote && p.raison == Raison::Epingle && !p.carte_pluie);
                VERIFIER(p.bandeau_agrandi == alerte);
            }
        }
    }
    return 0;
}

// Petit générateur congruentiel : toujours la même suite, donc le même test à chaque compilation.
struct Alea {
    std::uint64_t etat = 20261108;
    constexpr unsigned prochain(unsigned bornes) {
        etat = etat * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<unsigned>((etat >> 33) % bornes);
    }
};

// Une journée entière à la minute, des entrées qui changent au hasard (vent, marée, pluie, houle, doigt posé) : sans donnée
// périmée, deux changements de profil ne sont jamais à moins de 30 min, aucun n'a lieu pendant un toucher, et le profil rendu est
// toujours celui de l'état rendu.
constexpr int sur_une_journee_au_plus_un_changement_par_30_minutes_et_aucun_pendant_un_toucher() {
    Alea alea;
    Entrees e = base();
    e.spot_surf = true;
    e.activite_surf = true;
    e.activite_vent_bateau = true;
    e.seuil_vent_dixiemes_kmh = 250;
    e.spot_peche_a_pied = true;
    int changements = 0;
    int differes = 0;
    std::int64_t dernier = -1;
    Profil avant = Profil::Meteo;
    for (int minute = 0; minute < 24 * 60; ++minute) {
        e.maintenant_s = MINUIT + minute * 60;
        if (alea.prochain(8) == 0)
            e.vent_dixiemes_kmh = alea.prochain(2) == 0 ? 300 : 100;
        if (alea.prochain(8) == 0)
            e.basse_mer_dans_min = alea.prochain(2) == 0 ? 60 : 400;
        if (alea.prochain(8) == 0)
            e.fraicheur_mer = alea.prochain(2) == 0 ? Fraicheur::Fraiche : Fraicheur::Inconnue;
        if (alea.prochain(8) == 0)
            e.fraicheur_meteo = alea.prochain(2) == 0 ? Fraicheur::Fraiche : Fraicheur::Inconnue;
        e.pluie_dans_min = alea.prochain(2) == 0 ? 30 : VALEUR_INCONNUE;
        e.toucher_en_cours = alea.prochain(6) == 0;
        const Decision d = decider(e);
        VERIFIER(d.etat.profil == d.profil);
        if (d.raison_profil == Raison::ChangementDiffere)
            ++differes;
        if (d.profil != avant) {
            VERIFIER(!e.toucher_en_cours);
            VERIFIER(dernier < 0 || e.maintenant_s - dernier >= CHANGEMENT_ECART_MIN_S);
            VERIFIER(d.etat.changement_connu && d.etat.change_a_s == e.maintenant_s);
            dernier = e.maintenant_s;
            ++changements;
        } else {
            VERIFIER(meme_etat(d.etat, EtatAuto{d.profil, e.etat.changement_connu, e.etat.change_a_s}));
        }
        avant = d.profil;
        e.etat = d.etat;
    }
    // Le test n'est pas vide : la suite fait changer le profil plusieurs fois ET le garde-fou a retenu des changements.
    VERIFIER(changements >= 5);
    VERIFIER(differes >= 5);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_seuils_publics_sont_ceux_de_l_adr)                                                                           \
    X(entrees_vides_donnent_meteo_sans_rien_d_autre)                                                                   \
    X(rien_d_inconnu_ne_declenche_une_bascule)                                                                         \
    X(alerte_orange_et_rouge_agrandissent_le_bandeau_pas_en_dessous)                                                   \
    X(une_alerte_n_est_pas_un_changement_de_profil)                                                                    \
    X(l_alerte_agrandit_meme_epinglee_retenue_ou_pendant_un_toucher)                                                   \
    X(pluie_a_59_minutes_oui_a_60_et_61_non)                                                                           \
    X(la_carte_pluie_veut_des_previsions_fraiches)                                                                     \
    X(un_profil_epingle_n_a_pas_de_carte_pluie)                                                                        \
    X(la_carte_pluie_recouvre_le_profil_sans_le_changer)                                                               \
    X(l_alerte_passe_avant_la_pluie_dans_la_pastille)                                                                  \
    X(le_creneau_du_matin_va_du_lever_a_10_h_bornes_a_la_seconde)                                                      \
    X(le_creneau_du_soir_va_de_16_h_au_coucher_bornes_a_la_seconde)                                                    \
    X(hors_creneau_midi_nuit_et_avant_le_lever)                                                                        \
    X(le_creneau_veut_un_lever_et_un_coucher_du_jour_local)                                                            \
    X(le_creneau_lit_les_heures_dans_le_decalage_du_lieu)                                                              \
    X(surf_demande_spot_activite_donnees_fraiches_et_creneau)                                                          \
    X(une_maison_sans_spot_ne_bascule_jamais_vers_surf_ni_cote)                                                        \
    X(vent_bateau_veut_un_vent_strictement_au_dessus_du_seuil)                                                         \
    X(vent_bateau_veut_l_activite_cochee_et_un_seuil_regle)                                                            \
    X(cote_veut_une_basse_mer_dans_les_trois_heures_bornes_comprises)                                                  \
    X(cote_veut_un_spot_de_peche_a_pied_pas_d_activite)                                                                \
    X(surf_puis_vent_bateau_puis_cote_puis_meteo)                                                                      \
    X(surf_passe_avant_cote_et_vent_bateau_passe_avant_cote_deux_a_deux)                                               \
    X(toutes_les_regles_croisees_alerte_pluie_et_surf)                                                                 \
    X(une_donnee_de_mer_perimee_ramene_a_meteo_tout_de_suite)                                                          \
    X(une_donnee_meteo_perimee_ramene_vent_bateau_a_meteo)                                                             \
    X(une_donnee_de_mer_perimee_ramene_cote_a_meteo)                                                                   \
    X(inconnue_n_est_pas_perimee)                                                                                      \
    X(perimee_sans_les_autres_conditions_n_est_qu_un_defaut)                                                           \
    X(une_donnee_perimee_laisse_la_place_au_profil_suivant_dont_la_donnee_est_fraiche)                                 \
    X(la_peremption_attend_la_fin_du_toucher)                                                                          \
    X(la_peremption_d_une_famille_ne_touche_pas_l_autre)                                                               \
    X(pas_de_changement_a_29_min_59_s_changement_a_30_min)                                                             \
    X(le_premier_choix_est_libre)                                                                                      \
    X(le_meme_profil_ne_renouvelle_pas_l_horodatage)                                                                   \
    X(un_retour_a_meteo_ordinaire_attend_les_30_minutes_comme_tout_changement)                                         \
    X(un_changement_en_attente_est_pris_des_la_fin_du_delai)                                                           \
    X(une_horloge_revenue_en_arriere_ne_bloque_pas_les_changements)                                                    \
    X(sans_heure_le_profil_ne_change_pas)                                                                              \
    X(le_profil_ne_change_jamais_pendant_un_toucher)                                                                   \
    X(un_toucher_ne_gele_ni_le_bandeau_ni_la_carte_pluie_ni_un_profil_inchange)                                        \
    X(un_profil_epingle_gagne_sur_toutes_les_regles)                                                                   \
    X(l_epinglage_ignore_le_delai_le_toucher_et_la_peremption)                                                         \
    X(le_desepinglage_reprend_les_regles_tout_de_suite)                                                                \
    X(une_valeur_hors_table_est_ignoree_jamais_une_erreur)                                                             \
    X(toutes_les_combinaisons_d_entrees_suivent_la_table_de_l_adr)                                                     \
    X(sur_une_journee_au_plus_un_changement_par_30_minutes_et_aucun_pendant_un_toucher)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
