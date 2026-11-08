// Tests du noyau pur : la page mer composée (mer_page.hpp). Une mer d'exemple écrite à la main (quatre activités, quatre marées sur des
// demi-heures exactes), vue à des instants et avec des fraîcheurs différents ; aucune trame, rien du réseau.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_page.hpp"
#include "mini.hpp"

using namespace ecran::mer;
using ecran::i18n::Langue;

namespace {

constexpr std::int64_t MINUIT = 1794096000; // 2026-11-08 00:00:00 UTC
constexpr std::int64_t H(int h, int m = 0) { return MINUIT + h * 3600 + m * 60; }

template <typename F> constexpr bool dit(F f, std::string_view attendu) {
    char b[200] = {};
    for (char &c : b)
        c = 'X';
    const std::size_t n = f(b, sizeof b);
    return n == attendu.size() && std::string_view(b, n) == attendu && b[n] == '\0';
}

template <typename F> constexpr bool se_tait(F f) {
    char b[200] = {};
    for (char &c : b)
        c = 'X';
    const std::size_t n = f(b, sizeof b);
    return n == 0 && b[0] == '\0';
}

constexpr Critere_ligne critere(Critere id, std::int32_t valeur_d, Unite u, Verdict v, int points = -1, int max = -1) {
    Critere_ligne c;
    c.id = id;
    c.valeur_d = valeur_d;
    c.unite = u;
    c.verdict = v;
    c.points = static_cast<std::int8_t>(points);
    c.points_max = static_cast<std::int8_t>(max);
    return c;
}

constexpr Maree maree(bool haute, std::int64_t instant, std::int32_t cm) {
    Maree m;
    m.haute = haute;
    m.instant_s = instant;
    m.hauteur_cm = cm;
    return m;
}

// Une mer d'exemple : 24 heures depuis minuit UTC, surf bon (trois critères dont la vigilance), kite possible, bateau déconseillé pour
// la houle, pêche à pied qu'on ne peut pas juger ici ; marée basse 02:00 (1 m), haute 08:30 (5 m), basse 15:00, haute 21:30.
constexpr Mer mer_exemple() {
    Mer m;
    m.valide = true;
    m.genere_s = H(7);
    m.premiere_heure_s = MINUIT;
    m.nb_marees = 4;
    m.marees[0] = maree(false, H(2), 100);
    m.marees[1] = maree(true, H(8, 30), 500);
    m.marees[2] = maree(false, H(15), 100);
    m.marees[3] = maree(true, H(21, 30), 500);
    m.nb_coefficients = 2;
    m.coefficients[0] = Coefficient{H(8, 30), 92};
    m.coefficients[1] = Coefficient{H(21, 30), 95};

    Prevision &surf = m.activites[0];
    surf.presente = true;
    surf.disponible = true;
    surf.verdict = Verdict::Bon;
    surf.nb_criteres = 4;
    surf.criteres[0] = critere(Critere::HauteurHoule, 18, Unite::Metres, Verdict::Bon, 2, 2);
    // Une vigilance à valeur connue : c'est son identifiant, pas sa valeur, qui la tient hors des rangées d'une carte.
    surf.criteres[1] = critere(Critere::Vigilance, 20, Unite::Aucune, Verdict::Bon);
    surf.criteres[2] =
        critere(Critere::PeriodeHoule, INCONNU, Unite::Secondes, Verdict::Bon, 1, 2); // valeur inconnue : pas montrée
    surf.criteres[3] = critere(Critere::Vent, 90, Unite::KmH, Verdict::Possible, 1, 2);
    for (int h = 0; h < HEURES; ++h) {
        surf.heures[h] = h >= 8 && h < 12 ? Verdict::Bon : Verdict::Possible;
        surf.lumiere[h] = (h < 7 || h >= 18) ? Lumiere::Nuit : Lumiere::Jour;
    }

    Prevision &kite = m.activites[1];
    kite.presente = true;
    kite.disponible = true;
    kite.verdict = Verdict::Possible;
    kite.raison_critere = Critere::Vent;
    kite.raison_etat = Etat::UnPeuFaible;

    Prevision &bateau = m.activites[2];
    bateau.presente = true;
    bateau.disponible = true;
    bateau.verdict = Verdict::Deconseille;
    bateau.raison_critere = Critere::ForceRafales;
    bateau.raison_etat = Etat::CoupDeVentOuPlus;
    bateau.nb_criteres = 1;
    bateau.criteres[0] = critere(Critere::ForceRafales, 80, Unite::Beaufort, Verdict::Deconseille);

    Prevision &pied = m.activites[3];
    pied.presente = true;
    pied.disponible = false;     // pas de modèle de marée : on ne peut pas la juger ici
    pied.verdict = Verdict::Bon; // même si la ligne disait « bon » : la carte ne le montre pas
    return m;
}

constexpr bool teinte_est(Teinte a, Teinte b) { return a == b; }

// ---- les cartes ----------------------------------------------------------------------------------------------------------------

constexpr int les_verdicts_ne_se_montrent_que_pour_une_mer_fraiche_ou_de_quelques_minutes() {
    VERIFIER(verdicts_montres(Fraicheur::Fraiche));
    VERIFIER(verdicts_montres(Fraicheur::Vieillie));
    VERIFIER(!verdicts_montres(Fraicheur::Expiree));
    VERIFIER(!verdicts_montres(Fraicheur::Rien));
    return 0;
}

constexpr int une_carte_jugee_montre_son_verdict_et_s_ouvre_si_elle_a_des_criteres() {
    constexpr Mer m = mer_exemple();
    constexpr VueCarte surf = vue_carte(m, 0, Fraicheur::Fraiche);
    VERIFIER(surf.etat == EtatCarte::Jugee && surf.verdict == Verdict::Bon && surf.ouvrable && !surf.terne);
    constexpr VueCarte kite = vue_carte(m, 1, Fraicheur::Fraiche);
    VERIFIER(kite.etat == EtatCarte::Jugee && kite.verdict == Verdict::Possible);
    VERIFIER(!kite.ouvrable); // aucun critère : rien à ouvrir
    constexpr VueCarte bateau = vue_carte(m, 2, Fraicheur::Fraiche);
    VERIFIER(bateau.etat == EtatCarte::Jugee && bateau.verdict == Verdict::Deconseille && bateau.ouvrable);
    return 0;
}

constexpr int des_donnees_de_quelques_minutes_ternissent_la_carte() {
    constexpr Mer m = mer_exemple();
    const VueCarte v = vue_carte(m, 0, Fraicheur::Vieillie);
    VERIFIER(v.etat == EtatCarte::Jugee && v.terne && v.ouvrable);
    VERIFIER(!vue_carte(m, 0, Fraicheur::Fraiche).terne);
    return 0;
}

constexpr int une_activite_qu_on_ne_juge_pas_ici_n_a_jamais_de_verdict() {
    constexpr Mer m = mer_exemple();
    const VueCarte v = vue_carte(m, 3, Fraicheur::Fraiche);
    VERIFIER(v.etat == EtatCarte::Indisponible);
    VERIFIER(v.verdict == Verdict::Inconnu && !v.ouvrable);
    return 0;
}

constexpr int une_mer_absente_ou_trop_vieille_ne_montre_aucun_verdict() {
    constexpr Mer m = mer_exemple();
    for (int i = 0; i < NB_ACTIVITES; ++i) {
        VERIFIER(vue_carte(m, i, Fraicheur::Expiree).etat == EtatCarte::SansVerdict);
        VERIFIER(vue_carte(m, i, Fraicheur::Rien).etat == EtatCarte::SansVerdict);
        VERIFIER(vue_carte(m, i, Fraicheur::Expiree).verdict == Verdict::Inconnu);
        VERIFIER(!vue_carte(m, i, Fraicheur::Expiree).ouvrable);
    }
    return 0;
}

constexpr int une_ligne_absente_ou_sans_verdict_n_est_pas_jugee() {
    Mer m = mer_exemple();
    m.activites[1].presente = false;
    VERIFIER(vue_carte(m, 1, Fraicheur::Fraiche).etat == EtatCarte::SansVerdict);
    m.activites[1].presente = true;
    m.activites[1].verdict = Verdict::Inconnu;
    VERIFIER(vue_carte(m, 1, Fraicheur::Fraiche).etat == EtatCarte::SansVerdict);
    // Une activité jamais reçue n'est pas « indisponible » non plus : elle n'est pas là.
    m.activites[1].presente = false;
    m.activites[1].disponible = false;
    VERIFIER(vue_carte(m, 1, Fraicheur::Fraiche).etat == EtatCarte::SansVerdict);
    return 0;
}

constexpr int une_activite_hors_plage_donne_une_carte_vide() {
    constexpr Mer m = mer_exemple();
    VERIFIER(vue_carte(m, -1, Fraicheur::Fraiche).etat == EtatCarte::SansVerdict);
    VERIFIER(vue_carte(m, NB_ACTIVITES, Fraicheur::Fraiche).etat == EtatCarte::SansVerdict);
    return 0;
}

constexpr int la_ligne_de_la_carte_dit_pourquoi() {
    constexpr Mer m = mer_exemple();
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 1, Fraicheur::Fraiche, Langue::Fr, b, c); },
            "Vent : un peu faible"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 1, Fraicheur::Vieillie, Langue::En, b, c); },
            "Wind: a bit light"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 2, Fraicheur::Fraiche, Langue::Fr, b, c); },
            "Rafales : coup de vent ou plus"));
    // Le surf est bon et n'a aucune raison à donner : la ligne reste vide.
    VERIFIER(se_tait(
        [&](char *b, std::size_t c) { return formater_texte_carte(m, 0, Fraicheur::Fraiche, Langue::Fr, b, c); }));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 3, Fraicheur::Fraiche, Langue::Fr, b, c); },
            "Indisponible ici"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 0, Fraicheur::Expiree, Langue::Fr, b, c); },
            "Données de mer trop anciennes"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 3, Fraicheur::Expiree, Langue::Fr, b, c); },
            "Données de mer trop anciennes"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_texte_carte(m, 0, Fraicheur::Rien, Langue::Fr, b, c); },
                 "Mer : données indisponibles"));
    VERIFIER(se_tait(
        [&](char *b, std::size_t c) { return formater_texte_carte(m, 4, Fraicheur::Fraiche, Langue::Fr, b, c); }));
    VERIFIER(se_tait(
        [&](char *b, std::size_t c) { return formater_texte_carte(m, -1, Fraicheur::Rien, Langue::Fr, b, c); }));
    return 0;
}

constexpr int la_ligne_d_une_activite_jugee_sans_raison_connue_reste_vide() {
    Mer m = mer_exemple();
    m.activites[1].raison_critere = Critere::Inconnu;
    VERIFIER(se_tait(
        [&](char *b, std::size_t c) { return formater_texte_carte(m, 1, Fraicheur::Fraiche, Langue::Fr, b, c); }));
    // Une raison hors du vocabulaire du contrat : rien d'inventé.
    m.activites[1].raison_critere = Critere::Maree;
    m.activites[1].raison_etat = Etat::Faible;
    VERIFIER(se_tait(
        [&](char *b, std::size_t c) { return formater_texte_carte(m, 1, Fraicheur::Fraiche, Langue::Fr, b, c); }));
    return 0;
}

// ---- les barres ----------------------------------------------------------------------------------------------------------------

constexpr int une_barre_prend_la_teinte_de_son_verdict_le_jour_et_reste_neutre_la_nuit() {
    constexpr Mer m = mer_exemple();
    const Prevision &s = m.activites[0];
    VERIFIER(teinte_est(teinte_barre(s, 9), Teinte::Bon));       // jour, bon
    VERIFIER(teinte_est(teinte_barre(s, 13), Teinte::Possible)); // jour, possible
    VERIFIER(teinte_est(teinte_barre(s, 3), Teinte::Neutre));    // nuit : sombre, quel que soit le verdict
    VERIFIER(teinte_est(teinte_barre(s, 20), Teinte::Neutre));
    VERIFIER(teinte_est(teinte_barre(s, 6), Teinte::Neutre));    // dernière heure de nuit
    VERIFIER(teinte_est(teinte_barre(s, 7), Teinte::Possible));  // première heure de jour
    VERIFIER(teinte_est(teinte_barre(s, 17), Teinte::Possible)); // dernière heure de jour
    VERIFIER(teinte_est(teinte_barre(s, 18), Teinte::Neutre));
    return 0;
}

constexpr int une_barre_au_verdict_inconnu_ou_hors_plage_est_neutre() {
    Mer m = mer_exemple();
    m.activites[0].heures[10] = Verdict::Inconnu;
    VERIFIER(teinte_est(teinte_barre(m.activites[0], 10), Teinte::Neutre));
    VERIFIER(teinte_est(teinte_barre(m.activites[0], -1), Teinte::Neutre));
    VERIFIER(teinte_est(teinte_barre(m.activites[0], HEURES), Teinte::Neutre));
    m.activites[0].lumiere[10] = Lumiere::Inconnue; // lumière inconnue : le verdict s'applique
    m.activites[0].heures[10] = Verdict::Deconseille;
    VERIFIER(teinte_est(teinte_barre(m.activites[0], 10), Teinte::Deconseille));
    return 0;
}

constexpr int chaque_verdict_a_sa_teinte() {
    VERIFIER(teinte_est(teinte_verdict(Verdict::Bon), Teinte::Bon));
    VERIFIER(teinte_est(teinte_verdict(Verdict::Possible), Teinte::Possible));
    VERIFIER(teinte_est(teinte_verdict(Verdict::Deconseille), Teinte::Deconseille));
    VERIFIER(teinte_est(teinte_verdict(Verdict::Inconnu), Teinte::Neutre));
    return 0;
}

constexpr int le_repere_maintenant_suit_l_heure_et_disparait_avec_les_verdicts() {
    constexpr Mer m = mer_exemple();
    VERIFIER(heure_courante(m, Fraicheur::Fraiche, H(9, 30)) == 9);
    VERIFIER(heure_courante(m, Fraicheur::Vieillie, H(0, 0)) == 0);
    VERIFIER(heure_courante(m, Fraicheur::Fraiche, H(23, 59)) == 23);
    VERIFIER(heure_courante(m, Fraicheur::Fraiche, H(24, 0)) == -1);
    VERIFIER(heure_courante(m, Fraicheur::Fraiche, H(0) - 1) == -1);
    VERIFIER(heure_courante(m, Fraicheur::Fraiche, 0) == -1);
    VERIFIER(heure_courante(m, Fraicheur::Expiree, H(9)) == -1);
    VERIFIER(heure_courante(m, Fraicheur::Rien, H(9)) == -1);
    return 0;
}

constexpr int l_axe_demande_des_barres_montrees_et_une_heure_de_depart() {
    Mer m = mer_exemple();
    VERIFIER(axe_montre(m, Fraicheur::Fraiche));
    VERIFIER(axe_montre(m, Fraicheur::Vieillie));
    VERIFIER(!axe_montre(m, Fraicheur::Expiree));
    VERIFIER(!axe_montre(m, Fraicheur::Rien));
    m.premiere_heure_s = 0;
    VERIFIER(!axe_montre(m, Fraicheur::Fraiche));
    return 0;
}

// ---- les critères --------------------------------------------------------------------------------------------------------------

constexpr int une_carte_montre_les_premiers_criteres_connus_sans_la_vigilance() {
    constexpr Mer m = mer_exemple();
    int idx[RANGEES_CARTE] = {-1, -1, -1};
    // Hauteur de houle (0), vigilance (1) écartée, période (2) à valeur inconnue écartée, vent (3).
    VERIFIER(criteres_de_carte(m.activites[0], idx, RANGEES_CARTE) == 2);
    VERIFIER(idx[0] == 0 && idx[1] == 3 && idx[2] == -1);
    VERIFIER(criteres_de_carte(m.activites[1], idx, RANGEES_CARTE) == 0);
    int idx2[RANGEES_CARTE] = {};
    VERIFIER(criteres_de_carte(m.activites[2], idx2, RANGEES_CARTE) == 1 && idx2[0] == 0);
    return 0;
}

constexpr int une_carte_ne_montre_pas_plus_de_criteres_qu_on_en_demande() {
    Prevision a;
    a.nb_criteres = CRITERES_MAX;
    for (int k = 0; k < CRITERES_MAX; ++k)
        a.criteres[k] = critere(Critere::Vent, 100 + k, Unite::KmH, Verdict::Bon);
    int idx[CRITERES_MAX] = {};
    VERIFIER(criteres_de_carte(a, idx, RANGEES_CARTE) == RANGEES_CARTE);
    VERIFIER(idx[0] == 0 && idx[1] == 1 && idx[2] == 2);
    VERIFIER(criteres_de_carte(a, idx, 1) == 1);
    VERIFIER(criteres_de_carte(a, idx, 0) == 0);
    VERIFIER(criteres_de_carte(a, idx, CRITERES_MAX) == CRITERES_MAX);
    // Un nombre de critères supérieur à la capacité (donnée abîmée) ne sort pas du tableau.
    a.nb_criteres = 200;
    VERIFIER(criteres_de_carte(a, idx, CRITERES_MAX) == CRITERES_MAX);
    return 0;
}

constexpr int un_critere_inconnu_ou_d_unite_inconnue_n_est_pas_montre() {
    Prevision a;
    a.nb_criteres = 3;
    a.criteres[0] = critere(Critere::Inconnu, 10, Unite::Metres, Verdict::Bon);
    a.criteres[1] = critere(Critere::Vent, 10, Unite::Inconnue, Verdict::Bon);
    a.criteres[2] = critere(Critere::Coefficient, 920, Unite::Aucune, Verdict::Bon); // sans unité mais lisible
    int idx[RANGEES_CARTE] = {};
    VERIFIER(criteres_de_carte(a, idx, RANGEES_CARTE) == 1 && idx[0] == 2);
    VERIFIER(valeur_montrable(a.criteres[2]));
    VERIFIER(!valeur_montrable(a.criteres[1]));
    VERIFIER(!valeur_montrable(critere(Critere::Vent, INCONNU, Unite::KmH, Verdict::Bon)));
    return 0;
}

constexpr int le_detail_dessine_des_blocs_une_pastille_ou_rien() {
    // Surf : 2 sur 2 -> deux blocs pleins ; 1 sur 2 -> un plein sur deux.
    MarqueCritere m = marque_critere(critere(Critere::HauteurHoule, 18, Unite::Metres, Verdict::Bon, 2, 2));
    VERIFIER(m.marque == Marque::Blocs && m.plein == 2 && m.total == 2 && m.verdict == Verdict::Bon);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Possible, 1, 2));
    VERIFIER(m.marque == Marque::Blocs && m.plein == 1 && m.total == 2 && m.verdict == Verdict::Possible);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Deconseille, 0, 2));
    VERIFIER(m.marque == Marque::Blocs && m.plein == 0 && m.total == 2);
    // Sans points : la pastille de son verdict ; sans verdict non plus : rien (un tiret).
    m = marque_critere(critere(Critere::ForceVent, 40, Unite::Beaufort, Verdict::Possible));
    VERIFIER(m.marque == Marque::Pastille && m.verdict == Verdict::Possible && m.total == 0);
    m = marque_critere(critere(Critere::ForceVent, 40, Unite::Beaufort, Verdict::Inconnu));
    VERIFIER(m.marque == Marque::Rien);
    return 0;
}

constexpr int des_points_incoherents_ne_donnent_pas_de_blocs() {
    // Inconnus (-1), maximum nul, ou plus de points que le maximum : on retombe sur la pastille ou sur rien, jamais sur des blocs inventés.
    MarqueCritere m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, -1, 2));
    VERIFIER(m.marque == Marque::Pastille);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 1, -1));
    VERIFIER(m.marque == Marque::Pastille);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 0, 0));
    VERIFIER(m.marque == Marque::Pastille);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 3, 2));
    VERIFIER(m.marque == Marque::Pastille);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Inconnu, 3, 2));
    VERIFIER(m.marque == Marque::Rien);
    return 0;
}

constexpr int les_blocs_ne_depassent_pas_le_maximum_dessine() {
    MarqueCritere m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 6, 6));
    VERIFIER(m.marque == Marque::Blocs && m.total == BLOCS_MAX && m.plein == BLOCS_MAX);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 3, 6));
    VERIFIER(m.total == BLOCS_MAX && m.plein == 3);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 4, 4));
    VERIFIER(m.total == 4 && m.plein == 4);
    m = marque_critere(critere(Critere::Vent, 90, Unite::KmH, Verdict::Bon, 5, 6));
    VERIFIER(m.total == BLOCS_MAX && m.plein == BLOCS_MAX);
    return 0;
}

// ---- la marée ------------------------------------------------------------------------------------------------------------------

constexpr int la_courbe_couvre_ce_que_la_trame_donne_et_va_de_la_basse_a_la_haute() {
    constexpr Mer m = mer_exemple();
    constexpr CourbeMaree c = courbe_maree(m, H(5));
    VERIFIER(c.presente);
    VERIFIER(c.premier == 4 && c.dernier == 43); // 02:00 et 21:30, sur les demi-heures de la bande
    VERIFIER(c.y_pm[4] == 1000);                 // basse mer en bas
    VERIFIER(c.y_pm[17] == 0);                   // pleine mer (08:30) en haut
    VERIFIER(c.y_pm[30] == 1000);                // basse mer (15:00)
    VERIFIER(c.y_pm[43] == 0);                   // pleine mer (21:30)
    return 0;
}

constexpr int la_courbe_monte_puis_descend_sans_a_coup() {
    constexpr Mer m = mer_exemple();
    constexpr CourbeMaree c = courbe_maree(m, H(5));
    for (int k = 4; k < 17; ++k)
        VERIFIER(c.y_pm[k + 1] <= c.y_pm[k]); // la mer monte : l'ordonnée (0 en haut) diminue
    for (int k = 17; k < 30; ++k)
        VERIFIER(c.y_pm[k + 1] >= c.y_pm[k]);
    for (int k = 4; k <= 43; ++k)
        VERIFIER(c.y_pm[k] >= 0 && c.y_pm[k] <= 1000);
    // À mi-marée (05:00, 3 h sur 6 h 30 de montée) : un peu en dessous du milieu, d'après la demi-cosinusoïde.
    VERIFIER(c.y_pm[10] >= 550 && c.y_pm[10] <= 570);
    // Symétrie de la demi-cosinusoïde : deux points à égale distance du milieu de la montée valent à peu près 1000 de somme.
    VERIFIER(c.y_pm[8] + c.y_pm[13] >= 995 && c.y_pm[8] + c.y_pm[13] <= 1005);
    return 0;
}

constexpr int le_repere_est_sur_la_courbe_a_l_heure_de_maintenant() {
    constexpr Mer m = mer_exemple();
    constexpr CourbeMaree c = courbe_maree(m, H(5));
    VERIFIER(c.repere);
    VERIFIER(c.x_pm == 208); // 5 h sur 24 h : 208,3 millièmes
    VERIFIER(c.y_repere_pm >= 550 && c.y_repere_pm <= 570);
    // À la pleine mer exacte, le repère est en haut ; à la basse mer, en bas.
    VERIFIER(courbe_maree(m, H(8, 30)).y_repere_pm == 0);
    VERIFIER(courbe_maree(m, H(15)).y_repere_pm == 1000);
    VERIFIER(courbe_maree(m, H(15)).x_pm == 625);
    return 0;
}

constexpr int pas_de_repere_hors_des_24_heures_ni_hors_de_la_couverture() {
    constexpr Mer m = mer_exemple();
    VERIFIER(!courbe_maree(m, 0).repere);         // pas d'horloge
    VERIFIER(!courbe_maree(m, H(0) - 1).repere);  // avant la bande
    VERIFIER(!courbe_maree(m, H(24) + 1).repere); // après la bande
    VERIFIER(!courbe_maree(m, H(1)).repere);      // avant la première marée : la trame ne dit rien
    VERIFIER(!courbe_maree(m, H(23)).repere);     // après la dernière
    VERIFIER(courbe_maree(m, H(1)).presente);     // la courbe reste tracée sans repère
    // Même quand la trame dit la marée au-delà des 24 heures : « maintenant » n'est pas sur la courbe de la bande, qui s'arrête à 24 h.
    Mer longue = mer_exemple();
    longue.nb_marees = 6;
    longue.marees[4] = maree(false, H(27, 30), 100);
    longue.marees[5] = maree(true, H(34), 500);
    VERIFIER(courbe_maree(longue, H(23)).repere); // la marée est maintenant connue après 21:30
    VERIFIER(courbe_maree(longue, H(24)).repere); // la dernière seconde de la bande
    VERIFIER(!courbe_maree(longue, H(24) + 1).repere);
    VERIFIER(!courbe_maree(longue, H(28)).repere);
    return 0;
}

constexpr int sans_assez_de_marees_ni_heure_de_depart_il_n_y_a_pas_de_courbe() {
    Mer m = mer_exemple();
    m.nb_marees = 0;
    VERIFIER(!courbe_maree(m, H(5)).presente && !courbe_maree(m, H(5)).repere);
    m.nb_marees = 1;
    VERIFIER(!courbe_maree(m, H(5)).presente);
    m = mer_exemple();
    m.premiere_heure_s = 0;
    VERIFIER(!courbe_maree(m, H(5)).presente);
    // Même si les marées de la trame tombent juste après l'instant 0 : sans heure de départ, rien ne place la courbe dans le temps.
    for (int k = 0; k < m.nb_marees; ++k)
        m.marees[k].instant_s -= MINUIT;
    VERIFIER(!courbe_maree(m, 5 * 3600).presente);
    m = mer_exemple();
    m.valide = false;
    VERIFIER(!courbe_maree(m, H(5)).presente);
    return 0;
}

constexpr int une_maree_sans_hauteur_coupe_la_courbe_sans_pont() {
    Mer m = mer_exemple();
    m.marees[2].hauteur_cm = INCONNU; // la seconde montée -> descente n'est plus calculable
    const CourbeMaree c = courbe_maree(m, H(5));
    VERIFIER(c.presente);
    VERIFIER(c.premier == 4 &&
             c.dernier < 17 + 13); // elle s'arrête avec le premier tronçon : rien après la basse mer de 15:00
    VERIFIER(c.dernier >= 17);
    // Un trou au milieu : la courbe garde le premier tronçon et ne fait pas de pont vers les marées d'après (le trou est la marée de 06:00).
    Mer trou;
    trou.valide = true;
    trou.premiere_heure_s = MINUIT;
    trou.nb_marees = 6;
    trou.marees[0] = maree(false, H(0), 100);
    trou.marees[1] = maree(true, H(3), 500);
    trou.marees[2] = maree(false, H(6), INCONNU);
    trou.marees[3] = maree(true, H(9), 500);
    trou.marees[4] = maree(false, H(12), 100);
    trou.marees[5] = maree(true, H(15), 500);
    const CourbeMaree t = courbe_maree(trou, H(1));
    VERIFIER(t.presente && t.premier == 0 && t.dernier == 6); // 00:00 à 03:00, rien après
    VERIFIER(t.y_pm[6] == 0);                                 // la pleine mer de 03:00 en haut
    VERIFIER(t.y_pm[18] == 0 &&
             t.y_pm[24] == 0); // rien n'est tracé au-delà : ni 09:00 (pleine mer) ni 12:00 (basse mer)
    return 0;
}

constexpr int une_maree_plate_ne_s_etire_pas_sur_toute_la_hauteur() {
    Mer m = mer_exemple();
    m.marees[0].hauteur_cm = 300;
    m.marees[1].hauteur_cm = 304; // 4 cm d'amplitude : traité comme 10 cm
    m.marees[2].hauteur_cm = 300;
    m.marees[3].hauteur_cm = 304;
    const CourbeMaree c = courbe_maree(m, H(5));
    VERIFIER(c.presente);
    // Le haut du tracé est la pleine mer (304 cm : 0) ; la basse mer, 4 cm plus bas, ne descend qu'aux 4/10 de la hauteur (amplitude de 10).
    VERIFIER(c.y_pm[17] == 0);
    VERIFIER(c.y_pm[4] == 400);
    VERIFIER(c.y_pm[30] == 400);
    return 0;
}

constexpr int les_prochaines_marees_sont_les_deux_premieres_qui_ne_sont_pas_passees() {
    constexpr Mer m = mer_exemple();
    int idx[2] = {-1, -1};
    VERIFIER(prochaines_marees(m, H(5), idx) == 2 && idx[0] == 1 && idx[1] == 2);
    VERIFIER(prochaines_marees(m, H(1), idx) == 2 && idx[0] == 0 && idx[1] == 1);
    VERIFIER(prochaines_marees(m, H(8, 30), idx) == 2 && idx[0] == 1); // une marée à l'instant même n'est pas passée
    VERIFIER(prochaines_marees(m, H(8, 31), idx) == 2 && idx[0] == 2);
    VERIFIER(prochaines_marees(m, H(16), idx) == 1 && idx[0] == 3); // une seule reste
    VERIFIER(prochaines_marees(m, H(22), idx) == 0);                // plus aucune
    VERIFIER(prochaines_marees(m, 0, idx) == 0); // sans horloge, on ne sait pas laquelle est la prochaine
    Mer vide = mer_exemple();
    vide.nb_marees = 0;
    VERIFIER(prochaines_marees(vide, H(5), idx) == 0);
    return 0;
}

constexpr int le_coefficient_est_celui_de_la_prochaine_pleine_mer() {
    constexpr Mer m = mer_exemple();
    VERIFIER(coefficient_a_montrer(m, H(5)) == 0);
    VERIFIER(coefficient_a_montrer(m, H(9)) == 1);
    VERIFIER(coefficient_a_montrer(m, H(22)) == -1);
    VERIFIER(coefficient_a_montrer(m, 0) == -1); // sans horloge : jamais celui qui se trouve en tête
    return 0;
}

// ---- le bandeau ----------------------------------------------------------------------------------------------------------------

constexpr int le_style_du_bandeau_ne_peint_jamais_des_vigilances_illisibles_comme_un_calme() {
    using N = ecran::vigie::Niveau;
    const StyleBandeau orange = style_bandeau(Bandeau::Vigilance, N::Orange);
    VERIFIER(orange.niveau == 3 && orange.teinte);
    const StyleBandeau rouge = style_bandeau(Bandeau::Vigilance, N::Rouge);
    VERIFIER(rouge.niveau == 4 && rouge.teinte);
    const StyleBandeau jaune = style_bandeau(Bandeau::Vigilance, N::Jaune);
    VERIFIER(jaune.niveau == 2 && jaune.teinte);
    // Illisibles : jaune, sans teinte pleine, quel que soit le niveau qu'on lui passe.
    const StyleBandeau illisible = style_bandeau(Bandeau::VigilanceIllisible, N::Inconnu);
    VERIFIER(illisible.niveau == 2 && !illisible.teinte);
    VERIFIER(style_bandeau(Bandeau::VigilanceIllisible, N::Rouge).niveau == 2);
    // Un état (âge, absence) ou rien : neutre.
    VERIFIER(style_bandeau(Bandeau::Etat, N::Rouge).niveau == 0 && !style_bandeau(Bandeau::Etat, N::Rouge).teinte);
    VERIFIER(style_bandeau(Bandeau::Rien, N::Orange).niveau == 0 && !style_bandeau(Bandeau::Rien, N::Orange).teinte);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_verdicts_ne_se_montrent_que_pour_une_mer_fraiche_ou_de_quelques_minutes)                                     \
    X(une_carte_jugee_montre_son_verdict_et_s_ouvre_si_elle_a_des_criteres)                                            \
    X(des_donnees_de_quelques_minutes_ternissent_la_carte)                                                             \
    X(une_activite_qu_on_ne_juge_pas_ici_n_a_jamais_de_verdict)                                                        \
    X(une_mer_absente_ou_trop_vieille_ne_montre_aucun_verdict)                                                         \
    X(une_ligne_absente_ou_sans_verdict_n_est_pas_jugee)                                                               \
    X(une_activite_hors_plage_donne_une_carte_vide)                                                                    \
    X(la_ligne_de_la_carte_dit_pourquoi)                                                                               \
    X(la_ligne_d_une_activite_jugee_sans_raison_connue_reste_vide)                                                     \
    X(une_barre_prend_la_teinte_de_son_verdict_le_jour_et_reste_neutre_la_nuit)                                        \
    X(une_barre_au_verdict_inconnu_ou_hors_plage_est_neutre)                                                           \
    X(chaque_verdict_a_sa_teinte)                                                                                      \
    X(le_repere_maintenant_suit_l_heure_et_disparait_avec_les_verdicts)                                                \
    X(l_axe_demande_des_barres_montrees_et_une_heure_de_depart)                                                        \
    X(une_carte_montre_les_premiers_criteres_connus_sans_la_vigilance)                                                 \
    X(une_carte_ne_montre_pas_plus_de_criteres_qu_on_en_demande)                                                       \
    X(un_critere_inconnu_ou_d_unite_inconnue_n_est_pas_montre)                                                         \
    X(le_detail_dessine_des_blocs_une_pastille_ou_rien)                                                                \
    X(des_points_incoherents_ne_donnent_pas_de_blocs)                                                                  \
    X(les_blocs_ne_depassent_pas_le_maximum_dessine)                                                                   \
    X(la_courbe_couvre_ce_que_la_trame_donne_et_va_de_la_basse_a_la_haute)                                             \
    X(la_courbe_monte_puis_descend_sans_a_coup)                                                                        \
    X(le_repere_est_sur_la_courbe_a_l_heure_de_maintenant)                                                             \
    X(pas_de_repere_hors_des_24_heures_ni_hors_de_la_couverture)                                                       \
    X(sans_assez_de_marees_ni_heure_de_depart_il_n_y_a_pas_de_courbe)                                                  \
    X(une_maree_sans_hauteur_coupe_la_courbe_sans_pont)                                                                \
    X(une_maree_plate_ne_s_etire_pas_sur_toute_la_hauteur)                                                             \
    X(les_prochaines_marees_sont_les_deux_premieres_qui_ne_sont_pas_passees)                                           \
    X(le_coefficient_est_celui_de_la_prochaine_pleine_mer)                                                             \
    X(le_style_du_bandeau_ne_peint_jamais_des_vigilances_illisibles_comme_un_calme)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
