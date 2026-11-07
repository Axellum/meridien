// Tests du noyau pur « surf » : la note de session, le classement des spots, le meilleur créneau du jour et l'évaluation complète d'un
// spot (surf_note.hpp), sur des données synthétiques dont chaque heure est choisie (les réponses réelles sont dans
// test_surf_reponses.cpp). Le barème est maison : ces tests fixent ce que CE barème fait, pas ce qu'est un bon jour de surf. Chaque
// test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "ecran/surf_note.hpp"
#include "mini.hpp"
#include "surf_outils.hpp"

using namespace ecran::surf;
using namespace surf_test;

namespace {

// ---- fabriques -----------------------------------------------------------------------------------------------------------

constexpr Conditions cond(int hauteur_cm, int periode_ds, int vent, int direction, int rafale = ABSENT) noexcept {
    Conditions c{};
    c.hauteur_cm = static_cast<std::int16_t>(hauteur_cm);
    c.periode_ds = static_cast<std::int16_t>(periode_ds);
    c.vent_dixiemes_kmh = static_cast<std::int16_t>(vent);
    c.rafale_dixiemes_kmh = static_cast<std::int16_t>(rafale);
    c.vent_direction_deg = static_cast<std::int16_t>(direction);
    return c;
}

// Conditions parfaites partout sauf ce qu'on règle : houle 1,50 m (dans la fourchette 1,00 à 2,50 m), 13,0 s, vent nul.
constexpr Note note_avec(int hauteur_cm, int periode_ds, int vent, int direction, int rafale = ABSENT) noexcept {
    return noter(spot_ouest(), cond(hauteur_cm, periode_ds, vent, direction, rafale));
}

// Une hauteur de houle qui, avec 13,0 s et un vent calme, fait exactement `points` points au total (60 à 100) sur le spot de test :
// 25 (période) + 35 (vent) + la rampe de hauteur entre 40 cm (0 point) et 100 cm (40 points).
constexpr int hauteur_pour_points(int points) noexcept {
    for (int h = 40; h <= 100; ++h) {
        if (rampe(h, 40, 0, 100, 40) == points - 60)
            return h;
    }
    return -1;
}

// Une mer de 54 heures dont chaque heure vaut `defaut` points (60 à 100) avec le vent calme de `vent_calme()` ; `fixer` change une heure
// (0 : houle absente, donc heure sans note).
constexpr Marine mer_a_points(int defaut, std::int64_t debut = MINUIT, std::int32_t decalage = 0) noexcept {
    Marine m = mer_vide(54, debut, decalage);
    for (std::size_t i = 0; i < 54; ++i)
        regler_houle(m, i, hauteur_pour_points(defaut), 130);
    return m;
}
constexpr void fixer(Marine &m, std::size_t heure, int points) noexcept {
    if (points == 0) {
        m.heures[heure].houle_cm = ABSENT;
        m.heures[heure].houle_periode_ds = ABSENT;
        m.heures[heure].vagues_cm = ABSENT;
        m.heures[heure].vagues_periode_ds = ABSENT;
    } else {
        regler_houle(m, heure, hauteur_pour_points(points), 130);
    }
}
constexpr Vent vent_calme(std::int64_t debut = MINUIT, std::int32_t decalage = 0) noexcept {
    return vent_uniforme(54, 0, 0, 270, debut, decalage);
}

// ---- la note : sommes, arrondis, indisponibilité -------------------------------------------------------------------------

constexpr int le_bareme_fait_cent_points_et_la_mer_parfaite_dix() {
    VERIFIER(POINTS_HAUTEUR + POINTS_PERIODE + POINTS_VENT == 100);
    const Note n = note_avec(150, 130, 0, 270);
    VERIFIER(n.disponible && n.points == 100 && n.note == 10);
    VERIFIER(n.points_hauteur == 40 && n.points_periode == 25 && n.points_vent == 35);
    VERIFIER(n.hauteur == PositionHauteur::Dans && n.vent.classe == ClasseVent::Calme && n.limitant == Facteur::Aucun);
    VERIFIER(verdict_de(n) == Verdict::Excellent);
    return 0;
}

constexpr int le_pire_cas_fait_zero_point_mais_reste_disponible() {
    // 0 cm de houle est une MESURE (mer plate), pas une absence ; 0 s de période aussi ; vent de mer de 50 km/h.
    const Note n = note_avec(0, 0, 500, 270);
    VERIFIER(n.disponible && n.points == 0 && n.note == 0);
    VERIFIER(n.hauteur == PositionHauteur::Sous && n.vent.classe == ClasseVent::Onshore);
    VERIFIER(verdict_de(n) == Verdict::Mauvais);
    return 0;
}

constexpr int une_grandeur_absente_rend_la_note_indisponible_jamais_zero() {
    for (const Note &n :
         {noter(spot_ouest(), cond(ABSENT, 130, 0, 270)), noter(spot_ouest(), cond(150, ABSENT, 0, 270)),
          noter(spot_ouest(), cond(150, 130, ABSENT, 270)), noter(spot_ouest(), cond(150, 130, 0, ABSENT)),
          noter(spot_ouest(), cond(ABSENT, ABSENT, ABSENT, ABSENT))}) {
        VERIFIER(!n.disponible && n.points == 0 && n.note == 0 && n.limitant == Facteur::Aucun);
        VERIFIER(verdict_de(n) == Verdict::Indisponible);
    }
    // La rafale, elle, peut manquer : le vent effectif est alors le vent moyen.
    const Note sans_rafale = note_avec(150, 130, 200, 270);
    VERIFIER(sans_rafale.disponible && sans_rafale.vent.effectif_dixiemes_kmh == 200);
    return 0;
}

constexpr int la_note_est_les_points_arrondis_sur_dix() {
    VERIFIER(note_de_points(0) == 0 && note_de_points(4) == 0 && note_de_points(5) == 1 && note_de_points(14) == 1);
    VERIFIER(note_de_points(15) == 2 && note_de_points(94) == 9 && note_de_points(95) == 10 &&
             note_de_points(100) == 10);
    VERIFIER(note_de_points(104) == 10 && note_de_points(500) == 10); // plafonnée
    VERIFIER(note_de_points(-5) == 0 && note_de_points(-60) == 0);    // plancher
    return 0;
}

constexpr Note note_de(int note, bool dispo = true) noexcept {
    Note n{};
    n.disponible = dispo;
    n.note = static_cast<std::uint8_t>(note);
    return n;
}

constexpr int les_verdicts_suivent_les_seuils_nommes() {
    VERIFIER(NOTE_EXCELLENT_MIN == 9 && NOTE_BON_MIN == 7 && NOTE_CORRECT_MIN == 5 && NOTE_MOYEN_MIN == 3);
    VERIFIER(verdict_de(note_de(10)) == Verdict::Excellent && verdict_de(note_de(9)) == Verdict::Excellent);
    VERIFIER(verdict_de(note_de(8)) == Verdict::Bon && verdict_de(note_de(7)) == Verdict::Bon);
    VERIFIER(verdict_de(note_de(6)) == Verdict::Correct && verdict_de(note_de(5)) == Verdict::Correct);
    VERIFIER(verdict_de(note_de(4)) == Verdict::Moyen && verdict_de(note_de(3)) == Verdict::Moyen);
    VERIFIER(verdict_de(note_de(2)) == Verdict::Mauvais && verdict_de(note_de(0)) == Verdict::Mauvais);
    VERIFIER(verdict_de(note_de(10, false)) == Verdict::Indisponible); // une note 10 non disponible ne dit rien
    return 0;
}

// ---- la hauteur ----------------------------------------------------------------------------------------------------------

constexpr int hauteur_dans_la_fourchette_40_points_dessous_et_dessus_une_rampe() {
    // Fourchette 100 à 250 cm : zéro point à 40 cm (40 % de 100) et à 400 cm (160 % de 250).
    const auto ph = [](int h) { return note_avec(h, 130, 0, 270).points_hauteur; };
    VERIFIER(ph(100) == 40 && ph(150) == 40 && ph(250) == 40);
    VERIFIER(ph(70) == 20 && ph(55) == 10 && ph(99) == 39 && ph(41) == 1 && ph(40) == 0 && ph(39) == 0 && ph(0) == 0);
    VERIFIER(ph(251) == 40 && ph(325) == 20 && ph(399) == 0 && ph(400) == 0 && ph(401) == 0 && ph(2000) == 0);
    VERIFIER(note_avec(99, 130, 0, 270).hauteur == PositionHauteur::Sous);
    VERIFIER(note_avec(100, 130, 0, 270).hauteur == PositionHauteur::Dans);
    VERIFIER(note_avec(250, 130, 0, 270).hauteur == PositionHauteur::Dans);
    VERIFIER(note_avec(251, 130, 0, 270).hauteur == PositionHauteur::Sur);
    return 0;
}

constexpr int la_fourchette_du_spot_decale_les_rampes() {
    Spot s = spot_ouest();
    s.hauteur_min_cm = 50;
    s.hauteur_max_cm = 120; // zéro point à 20 cm et à 192 cm
    const auto ph = [&s](int h) { return noter(s, cond(h, 130, 0, 270)).points_hauteur; };
    VERIFIER(ph(50) == 40 && ph(120) == 40 && ph(35) == 20 && ph(20) == 0 && ph(156) == 20 && ph(192) == 0);
    // Les bornes de zéro point sont arrondies au centimètre : 40 % de 103 = 41,2 -> 41 ; 160 % de 253 = 404,8 -> 405.
    s.hauteur_min_cm = 103;
    s.hauteur_max_cm = 253;
    VERIFIER(noter(s, cond(41, 130, 0, 270)).points_hauteur == 0);
    VERIFIER(noter(s, cond(42, 130, 0, 270)).points_hauteur == 1);
    VERIFIER(noter(s, cond(403, 130, 0, 270)).points_hauteur == 1); // 405 et non 404 : 40 × (405 - 403) / 152 = 0,53
    VERIFIER(noter(s, cond(404, 130, 0, 270)).points_hauteur == 0);
    VERIFIER(noter(s, cond(405, 130, 0, 270)).points_hauteur == 0);
    return 0;
}

constexpr int la_hauteur_est_monotone_unimodale_et_sans_saut_de_plus_d_un_point() {
    int precedent = noter(spot_ouest(), cond(0, 130, 0, 270)).points_hauteur;
    for (int h = 1; h <= 600; ++h) {
        const int p = noter(spot_ouest(), cond(h, 130, 0, 270)).points_hauteur;
        const int ecart = p - precedent;
        VERIFIER(ecart >= -1 && ecart <= 1); // aucun saut
        if (h <= 100)
            VERIFIER(ecart >= 0); // monte
        else if (h > 250)
            VERIFIER(ecart <= 0); // descend
        else
            VERIFIER(ecart == 0); // plateau
        precedent = p;
    }
    return 0;
}

// ---- la période ----------------------------------------------------------------------------------------------------------

constexpr int periode_de_5_s_a_13_s_lineaire_et_plate_aux_bouts() {
    const auto pp = [](int p) { return note_avec(150, p, 0, 270).points_periode; };
    VERIFIER(PERIODE_NULLE_DS == 50 && PERIODE_PLEINE_DS == 130);
    VERIFIER(pp(0) == 0 && pp(49) == 0 && pp(50) == 0 && pp(130) == 25 && pp(131) == 25 && pp(400) == 25);
    VERIFIER(pp(90) == 13 && pp(100) == 16 && pp(120) == 22 && pp(70) == 6 && pp(51) == 0 && pp(52) == 1);
    return 0;
}

constexpr int la_periode_est_monotone_et_sans_saut() {
    int precedent = note_avec(150, 0, 0, 270).points_periode;
    for (int p = 1; p <= 200; ++p) {
        const int v = note_avec(150, p, 0, 270).points_periode;
        VERIFIER(v - precedent >= 0 && v - precedent <= 1);
        precedent = v;
    }
    return 0;
}

// ---- le vent -------------------------------------------------------------------------------------------------------------

constexpr int vent_de_terre_plein_jusqu_a_25_km_h_puis_zero_a_70() {
    const auto pv = [](int v) {
        return note_avec(150, 130, v, 90).points_vent;
    }; // vent d'est, plage à l'ouest : de terre
    VERIFIER(note_avec(150, 130, 300, 90).vent.classe == ClasseVent::Offshore);
    VERIFIER(pv(99) == 35 && pv(100) == 35 && pv(250) == 35 && pv(260) == 34 && pv(475) == 18 && pv(700) == 0 &&
             pv(900) == 0);
    return 0;
}

constexpr int vent_de_travers_de_10_a_45_km_h() {
    const auto pv = [](int v) {
        return note_avec(150, 130, v, 0).points_vent;
    }; // vent du nord, plage à l'ouest : travers
    VERIFIER(note_avec(150, 130, 300, 0).vent.classe == ClasseVent::Travers);
    VERIFIER(pv(99) == 35 && pv(100) == 35 && pv(275) == 18 && pv(450) == 0 && pv(451) == 0 && pv(800) == 0);
    return 0;
}

constexpr int vent_de_mer_de_10_a_30_km_h() {
    const auto pv = [](int v) { return note_avec(150, 130, v, 270).points_vent; }; // vent d'ouest : de mer
    VERIFIER(note_avec(150, 130, 300, 270).vent.classe == ClasseVent::Onshore);
    VERIFIER(pv(99) == 35 && pv(100) == 35 && pv(145) == 27 && pv(200) == 18 && pv(300) == 0 && pv(301) == 0);
    return 0;
}

constexpr int la_meme_force_vaut_moins_de_points_selon_la_classe() {
    // 20 km/h : de terre 35, de travers 25, de mer 18 (les trois classes ont des pentes différentes).
    VERIFIER(note_avec(150, 130, 200, 90).points_vent == 35);
    VERIFIER(note_avec(150, 130, 200, 0).points_vent == 25); // 35 × (450 - 200) / 350 = 25
    VERIFIER(note_avec(150, 130, 200, 270).points_vent == 18);
    // et la même direction vaut autre chose sur une plage tournée de l'autre côté
    Spot est = spot_ouest();
    est.orientation_deg = 90;
    VERIFIER(noter(est, cond(150, 130, 200, 270)).vent.classe == ClasseVent::Offshore);
    VERIFIER(noter(est, cond(150, 130, 200, 270)).points_vent == 35);
    return 0;
}

constexpr int sous_10_km_h_effectifs_toutes_les_classes_font_35() {
    for (const int d : {0, 45, 90, 135, 180, 225, 270, 315})
        VERIFIER(note_avec(150, 130, 99, d).points_vent == 35 &&
                 note_avec(150, 130, 99, d).vent.classe == ClasseVent::Calme);
    return 0;
}

constexpr int une_rafale_forte_compte_dans_le_vent_effectif() {
    // 10 km/h de vent de mer avec des rafales à 50 : effectif 30 km/h -> 0 point (les rafales comptent à moitié).
    const Note n = note_avec(150, 130, 100, 270, 500);
    VERIFIER(n.vent.effectif_dixiemes_kmh == 300 && n.points_vent == 0);
    // 8 km/h avec rafales à 30 : sort du calme (effectif 19 km/h, 19 points sur 35 pour un vent de mer)
    const Note m = note_avec(150, 130, 80, 270, 300);
    VERIFIER(m.vent.classe == ClasseVent::Onshore && m.vent.effectif_dixiemes_kmh == 190 && m.points_vent == 19);
    // le même vent moyen sans rafale connue reste calme : 35 points
    VERIFIER(note_avec(150, 130, 80, 270).points_vent == 35);
    return 0;
}

constexpr int le_vent_est_monotone_et_sans_saut_dans_chaque_direction() {
    for (const int d : {270, 0, 90, 315, 45, 180}) {
        int precedent = note_avec(150, 130, 0, d).points_vent;
        for (int v = 1; v <= 800; ++v) {
            const int p = note_avec(150, 130, v, d).points_vent;
            VERIFIER(p <= precedent && precedent - p <= 1);
            precedent = p;
        }
    }
    return 0;
}

// ---- le facteur limitant -------------------------------------------------------------------------------------------------

constexpr int le_facteur_limitant_est_la_part_qui_manque_le_plus_a_partir_de_8_points() {
    VERIFIER(LIMITANT_SEUIL_POINTS == 8);
    VERIFIER(note_avec(150, 130, 0, 270).limitant == Facteur::Aucun);
    // vent de mer à 14,5 km/h : 27 points sur 35, il manque 8 -> limitant ; à 14 km/h : 28, il manque 7 -> aucun
    VERIFIER(note_avec(150, 130, 145, 270).points_vent == 27 &&
             note_avec(150, 130, 145, 270).limitant == Facteur::Vent);
    VERIFIER(note_avec(150, 130, 140, 270).points_vent == 28 &&
             note_avec(150, 130, 140, 270).limitant == Facteur::Aucun);
    // houle trop petite : la hauteur
    VERIFIER(note_avec(50, 130, 0, 270).limitant == Facteur::Hauteur);
    // période courte : la période
    VERIFIER(note_avec(150, 60, 0, 270).limitant == Facteur::Periode);
    return 0;
}

constexpr int a_egalite_le_vent_puis_la_hauteur_puis_la_periode() {
    // vent et période manquent 25 points chacun -> le vent
    VERIFIER(note_avec(150, 50, 243, 270).points_vent == 10 && note_avec(150, 50, 243, 270).points_periode == 0);
    VERIFIER(note_avec(150, 50, 243, 270).limitant == Facteur::Vent);
    // hauteur et période manquent 25 points chacune (hauteur 62 cm = 15 points) et le vent est calme -> la hauteur
    VERIFIER(note_avec(62, 50, 0, 270).points_hauteur == 15 && note_avec(62, 50, 0, 270).limitant == Facteur::Hauteur);
    // vent et hauteur manquent chacun 25 -> le vent
    VERIFIER(note_avec(62, 130, 243, 270).limitant == Facteur::Vent);
    return 0;
}

// ---- les deux réponses mises ensemble ------------------------------------------------------------------------------------

constexpr int la_houle_passe_avant_les_vagues_totales() {
    Marine m = mer_vide(3);
    m.heures[0].houle_cm = 120;
    m.heures[0].houle_periode_ds = 110;
    m.heures[0].vagues_cm = 300; // des vagues totales plus grosses : le clapot du vent, pas la houle
    m.heures[0].vagues_periode_ds = 50;
    const Vent v = vent_uniforme(3, 100, 120, 90);
    const Conditions c = conditions_a(m, v, 0);
    VERIFIER(c.hauteur_cm == 120 && c.periode_ds == 110 && c.debut_s == MINUIT);
    VERIFIER(c.vent_dixiemes_kmh == 100 && c.rafale_dixiemes_kmh == 120 && c.vent_direction_deg == 90);
    return 0;
}

constexpr int la_periode_de_houle_manquante_retombe_sur_celle_des_vagues() {
    Marine m = mer_vide(2);
    m.heures[0].houle_cm = 120;
    m.heures[0].vagues_periode_ds = 95;
    const Conditions c = conditions_a(m, vent_uniforme(2, 100, 120, 90), 0);
    VERIFIER(c.hauteur_cm == 120 && c.periode_ds == 95);
    return 0;
}

constexpr int sans_houle_on_retombe_sur_les_vagues_totales() {
    Marine m = mer_vide(2);
    m.heures[0].vagues_cm = 140;
    m.heures[0].vagues_periode_ds = 85;
    m.heures[0].houle_periode_ds = 120; // une période de houle sans hauteur de houle ne sert pas
    const Conditions c = conditions_a(m, vent_uniforme(2, 100, 120, 90), 0);
    VERIFIER(c.hauteur_cm == 140 && c.periode_ds == 85);
    const Conditions rien = conditions_a(mer_vide(2), vent_uniforme(2, 100, 120, 90), 1);
    VERIFIER(rien.hauteur_cm == ABSENT && rien.periode_ds == ABSENT && rien.vent_dixiemes_kmh == 100);
    return 0;
}

constexpr int une_houle_de_zero_centimetre_est_une_mesure_pas_une_absence() {
    Marine m = mer_vide(2);
    m.heures[0].houle_cm = 0;
    m.heures[0].houle_periode_ds = 80;
    m.heures[0].vagues_cm = 90; // ne doit PAS prendre le relais
    const Conditions c = conditions_a(m, vent_uniforme(2, 0, 0, 270), 0);
    VERIFIER(c.hauteur_cm == 0 && c.periode_ds == 80);
    return 0;
}

constexpr int la_jointure_se_fait_par_l_instant_pas_par_le_rang() {
    Marine m = mer_uniforme(6, 150, 130, MINUIT);
    Vent v = vent_uniforme(6, 100, 100, 270, MINUIT + 2 * HEURE); // le vent commence deux heures plus tard
    for (std::size_t k = 0; k < 6; ++k)
        v.heures[k].vitesse_dixiemes_kmh = static_cast<std::int16_t>(100 + 10 * k); // 10,0 11,0 12,0 ...
    VERIFIER(conditions_a(m, v, 0).vent_dixiemes_kmh == ABSENT && conditions_a(m, v, 1).vent_dixiemes_kmh == ABSENT);
    VERIFIER(conditions_a(m, v, 2).vent_dixiemes_kmh == 100 && conditions_a(m, v, 3).vent_dixiemes_kmh == 110);
    VERIFIER(conditions_a(m, v, 5).vent_dixiemes_kmh == 130);
    // la note d'une heure sans vent est indisponible, pas calculée sur un vent nul
    VERIFIER(!noter(spot_ouest(), conditions_a(m, v, 0)).disponible &&
             noter(spot_ouest(), conditions_a(m, v, 2)).disponible);
    // hors de la série : des conditions vides
    VERIFIER(conditions_a(m, v, 6).hauteur_cm == ABSENT && conditions_a(m, v, 6).debut_s == 0);
    return 0;
}

constexpr int la_direction_de_la_houle_ne_change_pas_la_note_limite_ecrite() {
    // LIMITE : la direction de la houle par rapport à la plage n'est pas modélisée. Une houle qui vient de derrière la plage est
    // notée comme une houle de face. Ce test fixe cette limite (un jour qu'elle sera levée, il devra changer, exprès).
    Marine a = mer_uniforme(3, 150, 130);
    Marine b = a;
    for (std::size_t i = 0; i < 3; ++i) {
        a.heures[i].houle_direction_deg = 270; // de face (plage à l'ouest)
        b.heures[i].houle_direction_deg = 90;  // de dos
    }
    const Vent v = vent_calme();
    const Note na = noter(spot_ouest(), conditions_a(a, v, 1));
    const Note nb = noter(spot_ouest(), conditions_a(b, v, 1));
    VERIFIER(na.disponible && na.points == nb.points && na.note == nb.note && na.points == 100);
    b.heures[1].houle_direction_deg = ABSENT;
    VERIFIER(noter(spot_ouest(), conditions_a(b, v, 1)).points ==
             100); // et son absence ne rend pas la note indisponible
    return 0;
}

constexpr int heure_de_rend_le_rang_de_l_heure_qui_contient_l_instant() {
    const Marine m = mer_vide(5);
    VERIFIER(heure_de(m, MINUIT) == 0 && heure_de(m, MINUIT + 3599) == 0 && heure_de(m, MINUIT + 3600) == 1);
    VERIFIER(heure_de(m, MINUIT + 4 * HEURE + 3599) == 4);
    VERIFIER(heure_de(m, MINUIT - 1) == -1 && heure_de(m, MINUIT + 5 * HEURE) == -1);
    VERIFIER(heure_de(Marine{}, MINUIT) == -1);
    return 0;
}

constexpr int le_decalage_retenu_est_celui_du_vent_sinon_celui_de_la_mer() {
    Marine m = mer_vide(3, MINUIT, 0); // au large : fuseau Etc/GMT
    Vent v = vent_uniforme(3, 100, 100, 270, MINUIT, 3600);
    VERIFIER(decalage_a_retenir(m, v) == 3600);
    v.nb = 0; // pas de réponse vent exploitable
    m.decalage_utc_s = -36000;
    VERIFIER(decalage_a_retenir(m, v) == -36000);
    return 0;
}

// ---- classement ----------------------------------------------------------------------------------------------------------

constexpr int le_classement_range_par_cle_decroissante_et_garde_l_ordre_de_saisie_a_egalite() {
    const std::int16_t cles[] = {50, 70, 70, -1, 90};
    std::uint8_t ordre[5] = {};
    classer(cles, 5, ordre);
    VERIFIER(ordre[0] == 4 && ordre[1] == 1 && ordre[2] == 2 && ordre[3] == 0 && ordre[4] == 3);
    const std::int16_t egaux[] = {60, 60, 60, 60};
    std::uint8_t o2[4] = {};
    classer(egaux, 4, o2);
    VERIFIER(o2[0] == 0 && o2[1] == 1 && o2[2] == 2 && o2[3] == 3); // stable
    return 0;
}

constexpr int un_spot_sans_donnee_passe_apres_tous_les_autres_jamais_devant_sur_un_zero() {
    const std::int16_t cles[] = {-1, 0, ABSENT, 0, 5};
    std::uint8_t ordre[5] = {};
    classer(cles, 5, ordre);
    VERIFIER(ordre[0] == 4 && ordre[1] == 1 && ordre[2] == 3); // le 5, puis les deux VRAIS zéros dans l'ordre de saisie
    VERIFIER(ordre[3] == 0 && ordre[4] == 2); // puis les deux « pas de donnée », dans l'ordre de saisie
    const std::int16_t tous_absents[] = {-1, ABSENT, -1};
    std::uint8_t o2[3] = {};
    classer(tous_absents, 3, o2);
    VERIFIER(o2[0] == 0 && o2[1] == 1 && o2[2] == 2);
    return 0;
}

constexpr int le_classement_tient_les_cas_limites() {
    std::uint8_t o0[1] = {7};
    classer(nullptr, 0, o0); // rien à ranger : ne touche à rien
    VERIFIER(o0[0] == 7);
    const std::int16_t un[] = {42};
    std::uint8_t o1[1] = {};
    classer(un, 1, o1);
    VERIFIER(o1[0] == 0);
    // douze spots (le maximum), clés croissantes -> ordre inversé
    std::int16_t croissantes[SPOTS_MAX] = {};
    std::uint8_t o12[SPOTS_MAX] = {};
    for (std::size_t i = 0; i < SPOTS_MAX; ++i)
        croissantes[i] = static_cast<std::int16_t>(10 * i);
    classer(croissantes, SPOTS_MAX, o12);
    for (std::size_t i = 0; i < SPOTS_MAX; ++i)
        VERIFIER(static_cast<std::size_t>(o12[i]) == SPOTS_MAX - 1 - i);
    return 0;
}

// ---- le meilleur créneau -------------------------------------------------------------------------------------------------

constexpr int la_fabrique_de_test_donne_exactement_les_points_demandes() {
    const Vent v = vent_calme();
    Marine m = mer_a_points(60);
    for (int p = 60; p <= 100; ++p) {
        fixer(m, 10, p);
        VERIFIER(noter(spot_ouest(), conditions_a(m, v, 10)).points == p);
    }
    fixer(m, 10, 0);
    VERIFIER(!noter(spot_ouest(), conditions_a(m, v, 10)).disponible);
    return 0;
}

constexpr Creneau creneau(const Marine &m, std::int64_t maintenant_s = MINUIT, std::int32_t decalage = 0, int jour = 0,
                          const Vent *v = nullptr) noexcept {
    const Vent par_defaut = vent_calme(m.heures[0].debut_s, decalage);
    return meilleur_creneau(spot_ouest(), m, v != nullptr ? *v : par_defaut, maintenant_s, decalage, jour);
}

constexpr int une_seule_bonne_heure_donne_un_creneau_de_deux_heures_le_plus_tot_a_egalite() {
    Marine m = mer_a_points(60);
    fixer(m, 10, 100);
    const Creneau c = creneau(m);
    VERIFIER(c.trouve && c.heures == 2 && c.debut_s == MINUIT + 9 * HEURE && c.fin_s == MINUIT + 11 * HEURE);
    VERIFIER(c.points == 80 && c.note == 8); // (60 + 100) / 2
    return 0;
}

constexpr int le_creneau_s_elargit_tant_que_les_heures_voisines_sont_dans_la_tolerance() {
    VERIFIER(CRENEAU_TOLERANCE_POINTS == 10 && CRENEAU_HEURES_MIN == 2 && CRENEAU_HEURES_MAX == 6);
    Marine m = mer_a_points(60);
    fixer(m, 10, 100);
    fixer(m, 11, 100);
    fixer(m, 12, 95);
    fixer(m, 13, 90); // 90 = moyenne des deux meilleures (100) moins 10 : inclus
    const Creneau c = creneau(m);
    VERIFIER(c.trouve && c.debut_s == MINUIT + 10 * HEURE && c.fin_s == MINUIT + 14 * HEURE && c.heures == 4);
    VERIFIER(c.points == 96 && c.note == 10); // 385 / 4 = 96,25
    fixer(m, 13, 89);                         // un point de moins : exclu
    const Creneau d = creneau(m);
    VERIFIER(d.heures == 3 && d.fin_s == MINUIT + 13 * HEURE && d.points == 98); // 295 / 3 = 98,33
    // Le même seuil de l'autre côté : une voisine de GAUCHE à 90 (100 + 100 - 20 : égalité exacte) est incluse, à 89 elle ne l'est pas.
    Marine g = mer_a_points(60);
    fixer(g, 10, 100);
    fixer(g, 11, 100);
    fixer(g, 9, 90);
    fixer(g, 12, 89);
    const Creneau e = creneau(g);
    VERIFIER(e.heures == 3 && e.debut_s == MINUIT + 9 * HEURE && e.fin_s == MINUIT + 12 * HEURE &&
             e.points == 97); // 290 / 3 = 96,67
    fixer(g, 9, 89);
    const Creneau f = creneau(g);
    VERIFIER(f.heures == 2 && f.debut_s == MINUIT + 10 * HEURE && f.fin_s == MINUIT + 12 * HEURE && f.points == 100);
    return 0;
}

constexpr int la_tolerance_se_mesure_sur_la_moyenne_des_deux_meilleures_heures() {
    Marine m = mer_a_points(60);
    fixer(m, 10, 100);
    fixer(m, 11,
          99); // somme de départ : 199, moyenne 99,5 -> une voisine doit valoir au moins 90 (89,5 arrondi vers le haut)
    fixer(m, 12, 90);
    fixer(m, 9, 89);
    const Creneau c = creneau(m);
    VERIFIER(c.debut_s == MINUIT + 10 * HEURE && c.fin_s == MINUIT + 13 * HEURE && c.heures == 3); // 90 oui, 89 non
    return 0;
}

constexpr int le_creneau_ne_depasse_pas_six_heures() {
    Marine m = mer_a_points(60);
    for (std::size_t h = 8; h <= 16; ++h)
        fixer(m, h, 100);
    const Creneau c = creneau(m);
    VERIFIER(c.heures == 6 && c.debut_s == MINUIT + 8 * HEURE && c.fin_s == MINUIT + 14 * HEURE && c.points == 100 &&
             c.note == 10);
    return 0;
}

constexpr int la_meilleure_voisine_passe_avant_et_a_egalite_c_est_la_gauche() {
    Marine m = mer_a_points(60);
    fixer(m, 9, 95);
    fixer(m, 10, 100);
    fixer(m, 11, 100);
    fixer(m, 12, 99);
    const Creneau c = creneau(m);
    VERIFIER(c.debut_s == MINUIT + 9 * HEURE && c.fin_s == MINUIT + 13 * HEURE && c.heures == 4 &&
             c.points == 99); // 394 / 4
    // égalité : deux heures à 95 à gauche, trois à droite ; la gauche d'abord donne 8..13, la droite d'abord donnerait 9..14
    Marine n = mer_a_points(60);
    fixer(n, 8, 95);
    fixer(n, 9, 95);
    fixer(n, 10, 100);
    fixer(n, 11, 100);
    fixer(n, 12, 95);
    fixer(n, 13, 95);
    fixer(n, 14, 95);
    const Creneau d = creneau(n);
    VERIFIER(d.heures == 6 && d.debut_s == MINUIT + 8 * HEURE && d.fin_s == MINUIT + 14 * HEURE);
    return 0;
}

constexpr int une_heure_sans_note_coupe_le_creneau() {
    Marine m = mer_a_points(60);
    fixer(m, 10, 100);
    fixer(m, 11, 0); // houle absente
    fixer(m, 12, 100);
    fixer(m, 13, 100);
    const Creneau c = creneau(m);
    VERIFIER(c.trouve && c.debut_s == MINUIT + 12 * HEURE && c.fin_s == MINUIT + 14 * HEURE &&
             c.heures == 2); // pas 10 à 13
    return 0;
}

constexpr int aucun_creneau_si_aucune_paire_d_heures_n_a_de_note() {
    Marine m = mer_a_points(60);
    for (std::size_t h = 0; h < 54; h += 2)
        fixer(m, h, 0); // une heure sur deux sans note : jamais deux heures de suite
    VERIFIER(!creneau(m).trouve);
    // une seule heure notée dans la fenêtre du jour
    Marine u = mer_vide(54);
    for (std::size_t h = 0; h < 54; ++h)
        fixer(u, h, 0);
    fixer(u, 10, 100);
    const Creneau c = creneau(u);
    VERIFIER(!c.trouve && c.heures == 0 && c.debut_s == 0 && c.fin_s == 0 && c.points == 0 && c.note == 0);
    return 0;
}

constexpr int la_fenetre_est_de_6_h_a_21_h_locales_les_heures_entieres() {
    VERIFIER(CRENEAU_DEBUT_H == 6 && CRENEAU_FIN_H == 21);
    Marine m = mer_a_points(60);
    fixer(m, 4, 100);
    fixer(m, 5, 100); // 4 h et 5 h : avant 6 h
    const Creneau a = creneau(m);
    // Journée plate à 60 points : le créneau est le plus tôt possible de la fenêtre et s'étend jusqu'à six heures (toutes les
    // voisines sont dans la tolérance) ; les heures de 4 h et 5 h, meilleures, ne comptent pas.
    VERIFIER(a.debut_s == MINUIT + 6 * HEURE && a.fin_s == MINUIT + 12 * HEURE && a.heures == 6 && a.points == 60);
    Marine n = mer_a_points(60);
    fixer(n, 20, 100);
    fixer(n, 21, 100); // 20 h oui (finit à 21 h), 21 h non
    const Creneau b = creneau(n);
    VERIFIER(b.debut_s == MINUIT + 19 * HEURE && b.fin_s == MINUIT + 21 * HEURE && b.points == 80);
    Marine o = mer_a_points(60);
    fixer(o, 5, 100);
    fixer(o, 6, 100); // 5 h non, 6 h oui : la fenêtre commence à 6 h exactement
    const Creneau d = creneau(o);
    VERIFIER(d.debut_s == MINUIT + 6 * HEURE && d.heures == 2 && d.points == 80);
    return 0;
}

constexpr int le_jour_local_suit_le_decalage_de_la_reponse() {
    Marine m = mer_a_points(60, MINUIT, 3600);
    fixer(m, 4, 100);
    fixer(m, 5, 100);
    // UTC+1 : 5 h UTC = 6 h locales (dans la fenêtre), 4 h UTC = 5 h locales (hors fenêtre)
    const Creneau a = creneau(m, MINUIT, 3600);
    VERIFIER(a.debut_s == MINUIT + 5 * HEURE && a.fin_s == MINUIT + 7 * HEURE && a.points == 80);
    // UTC-10 (Hawaï) : le jour local de MINUIT + 12 h UTC est le jour même, 6 h à 21 h locales = 16 h à 31 h UTC
    Marine h = mer_a_points(60, MINUIT, -36000);
    fixer(h, 22, 100);
    fixer(h, 23, 100);
    fixer(h, 4, 100); // 18 h locales la VEILLE : un autre jour
    fixer(h, 5, 100);
    const Creneau b = creneau(h, MINUIT + 12 * HEURE, -36000);
    VERIFIER(b.debut_s == MINUIT + 22 * HEURE && b.fin_s == MINUIT + 24 * HEURE && b.points == 100);
    // et demain à Hawaï : 6 h à 21 h locales du lendemain = 40 h à 55 h UTC, la série s'arrête à 54 h
    Marine h2 = mer_a_points(60, MINUIT, -36000);
    fixer(h2, 50, 100);
    fixer(h2, 51, 100);
    const Creneau c = creneau(h2, MINUIT + 12 * HEURE, -36000, 1);
    VERIFIER(c.trouve && c.debut_s == MINUIT + 50 * HEURE && c.fin_s == MINUIT + 52 * HEURE);
    // « Aujourd'hui » est aussi un jour LOCAL : à 5 h UTC, il est 19 h la veille à Hawaï, donc le jour demandé est celui de la veille
    // en UTC. Les heures écoulées (jusqu'à 4 h UTC) sont écartées ; 5 h et 6 h UTC (19 h et 20 h locales) sont les deux dernières
    // heures de la fenêtre, 7 h UTC (21 h locales) est déjà dehors.
    Marine h3 = mer_a_points(60, MINUIT, -36000);
    fixer(h3, 5, 100);
    fixer(h3, 6, 100);
    const Creneau d = creneau(h3, MINUIT + 5 * HEURE, -36000);
    VERIFIER(d.trouve && d.debut_s == MINUIT + 5 * HEURE && d.fin_s == MINUIT + 7 * HEURE && d.points == 100);
    return 0;
}

constexpr int les_decalages_d_une_demi_heure_et_de_trois_quarts_d_heure_sont_respectes() {
    // UTC+5:30 (Inde) : les heures commencent à :30 locales. 0 h UTC = 5 h 30 locales (hors fenêtre, avant 6 h) ;
    // 1 h UTC = 6 h 30 (dans) ; 14 h UTC = 19 h 30 (dans, finit à 20 h 30) ; 15 h UTC = 20 h 30 (finirait à 21 h 30 : hors).
    {
        Marine m = mer_a_points(60, MINUIT, 19800);
        fixer(m, 0, 100);
        fixer(m, 15, 100);
        const Creneau a = creneau(m, MINUIT, 19800);
        VERIFIER(a.trouve && a.debut_s == MINUIT + 1 * HEURE && a.points == 60); // 0 h et 15 h ne comptent pas
        fixer(m, 14, 100);
        const Creneau b = creneau(m, MINUIT, 19800);
        VERIFIER(b.debut_s == MINUIT + 13 * HEURE && b.fin_s == MINUIT + 15 * HEURE && b.points == 80);
    }
    // UTC+5:45 (Népal) : 0 h UTC = 5 h 45 locales (hors) ; 14 h UTC = 19 h 45 (finit à 20 h 45 : dans) ; 15 h UTC = 20 h 45 (hors)
    {
        Marine m = mer_a_points(60, MINUIT, 20700);
        fixer(m, 0, 100);
        fixer(m, 14, 100);
        fixer(m, 15, 100);
        const Creneau a = creneau(m, MINUIT, 20700);
        VERIFIER(a.debut_s == MINUIT + 13 * HEURE && a.fin_s == MINUIT + 15 * HEURE && a.points == 80);
    }
    // UTC-9:30 (Marquises) : MINUIT + 1 h UTC est la veille 15 h 30 locales ; le jour local est celui d'avant MINUIT UTC
    {
        Marine m = mer_a_points(60, MINUIT, -34200);
        fixer(m, 5, 100);
        fixer(m, 6, 100);
        const Creneau a = creneau(m, MINUIT + 1 * HEURE, -34200);
        VERIFIER(a.trouve && a.debut_s == MINUIT + 4 * HEURE && a.fin_s == MINUIT + 6 * HEURE && a.points == 80);
    }
    return 0;
}

constexpr int les_heures_ecoulees_sont_ecartees_l_heure_en_cours_est_gardee() {
    Marine m = mer_a_points(60);
    fixer(m, 8, 100);
    fixer(m, 9, 100);
    fixer(m, 12, 80);
    fixer(m, 13, 80);
    VERIFIER(creneau(m, MINUIT).debut_s == MINUIT + 8 * HEURE); // (8, 9) = 200
    VERIFIER(creneau(m, MINUIT + 9 * HEURE).debut_s ==
             MINUIT + 9 * HEURE); // 8 h est terminée ; (9, 10) = 160 = (12, 13), le plus tôt
    VERIFIER(creneau(m, MINUIT + 9 * HEURE + 1800).debut_s == MINUIT + 9 * HEURE); // 9 h est en cours : gardée
    VERIFIER(creneau(m, MINUIT + 10 * HEURE - 1).debut_s == MINUIT + 9 * HEURE);   // à une seconde de la fin de 9 h
    const Creneau apres = creneau(m, MINUIT + 10 * HEURE);                         // 9 h est terminée
    VERIFIER(apres.debut_s == MINUIT + 12 * HEURE && apres.fin_s == MINUIT + 14 * HEURE && apres.points == 80);
    return 0;
}

constexpr int le_soir_trop_tardif_il_n_y_a_plus_de_creneau_aujourd_hui_mais_il_y_en_a_un_demain() {
    Marine m = mer_a_points(80);
    fixer(m, 40, 100);
    fixer(m, 41, 100);
    VERIFIER(!creneau(m, MINUIT + 21 * HEURE).trouve); // 20 h (la dernière) est terminée à 21 h
    VERIFIER(!creneau(m, MINUIT + 22 * HEURE).trouve);
    const Creneau demain = creneau(m, MINUIT + 22 * HEURE, 0, 1);
    VERIFIER(demain.trouve && demain.debut_s == MINUIT + 40 * HEURE && demain.points == 100);
    VERIFIER(!creneau(m, MINUIT + 22 * HEURE, 0, 2).trouve); // après-demain : hors de la série
    return 0;
}

constexpr int sans_vent_ou_sans_mer_il_n_y_a_pas_de_creneau() {
    const Marine m = mer_a_points(100);
    const Vent sans_vent{};
    VERIFIER(!meilleur_creneau(spot_ouest(), m, sans_vent, MINUIT, 0, 0).trouve);
    VERIFIER(!meilleur_creneau(spot_ouest(), Marine{}, vent_calme(), MINUIT, 0, 0).trouve);
    VERIFIER(!meilleur_creneau(spot_ouest(), mer_a_points(100), vent_calme(MINUIT + 40 * HEURE), MINUIT, 0, 0)
                  .trouve); // vents d'un autre jour
    return 0;
}

constexpr int le_vent_pese_dans_le_creneau() {
    // Mer parfaite toute la journée ; le vent de mer se lève à 12 h : le créneau est le matin.
    const Marine m = mer_uniforme(54, 150, 130);
    Vent v = vent_calme();
    for (std::size_t h = 12; h < 54; ++h) {
        v.heures[h].vitesse_dixiemes_kmh = 300;
        v.heures[h].rafale_dixiemes_kmh = 300;
        v.heures[h].direction_deg = 270; // de mer
    }
    const Creneau c = meilleur_creneau(spot_ouest(), m, v, MINUIT, 0, 0);
    VERIFIER(c.trouve && c.debut_s == MINUIT + 6 * HEURE && c.fin_s == MINUIT + 12 * HEURE && c.heures == 6 &&
             c.points == 100);
    return 0;
}

// ---- l'évaluation complète d'un spot -------------------------------------------------------------------------------------

constexpr int evaluer_un_spot_complet_note_creneau_cle() {
    const Marine m = mer_a_points(90);
    const Vent v = vent_calme();
    const EvaluationSpot e = evaluer_spot(spot_ouest(), m, v, MINUIT + 10 * HEURE + 300);
    VERIFIER(e.donnees && e.maintenant.disponible && e.maintenant.points == 90 && e.maintenant.note == 9);
    VERIFIER(e.creneau.trouve && e.creneau.points == 90 && e.cle == 90);
    VERIFIER(e.tendance == Tendance::Inconnue && e.prochaine_pleine_s == 0 &&
             e.prochaine_basse_s == 0); // pas de marée dans cette mer
    return 0;
}

constexpr int la_cle_retombe_sur_la_note_de_l_heure_en_cours_puis_sur_moins_un() {
    const Marine m = mer_a_points(80);
    const Vent v = vent_calme();
    // 22 h : plus de créneau aujourd'hui, mais l'heure en cours a une note
    const EvaluationSpot soir = evaluer_spot(spot_ouest(), m, v, MINUIT + 22 * HEURE + 100);
    VERIFIER(soir.donnees && !soir.creneau.trouve && soir.maintenant.disponible && soir.cle == 80);
    // hors de la série : ni créneau, ni heure en cours
    const EvaluationSpot loin = evaluer_spot(spot_ouest(), m, v, MINUIT + 60 * HEURE);
    VERIFIER(loin.donnees && !loin.maintenant.disponible && !loin.creneau.trouve && loin.cle == -1);
    // demain : la clé suit le créneau de demain
    const EvaluationSpot demain = evaluer_spot(spot_ouest(), m, v, MINUIT + 22 * HEURE + 100, 1);
    VERIFIER(demain.creneau.trouve && demain.cle == demain.creneau.points && demain.maintenant.points == 80);
    return 0;
}

constexpr int pas_de_vague_ou_pas_de_vent_pas_de_donnees() {
    const EvaluationSpot sans_mer = evaluer_spot(spot_ouest(), mer_vide(54), vent_calme(), MINUIT + 10 * HEURE);
    VERIFIER(!sans_mer.donnees && !sans_mer.maintenant.disponible && !sans_mer.creneau.trouve && sans_mer.cle == -1);
    const EvaluationSpot sans_vent = evaluer_spot(spot_ouest(), mer_a_points(100), Vent{}, MINUIT + 10 * HEURE);
    VERIFIER(!sans_vent.donnees && !sans_vent.creneau.trouve && sans_vent.cle == -1);
    return 0;
}

constexpr int la_maree_entre_dans_l_evaluation() {
    Marine m = mer_a_points(100);
    constexpr int NIVEAU[] = {0, 100, 180, 220, 180, 100, 0, -100, -180, -220, -180, -100, 0, 100, 180, 220, 180};
    for (std::size_t i = 0; i < 54; ++i)
        m.heures[i].maree_cm = static_cast<std::int16_t>(NIVEAU[i < 16 ? i : 16]);
    m.a_maree = true;
    const EvaluationSpot e = evaluer_spot(spot_ouest(), m, vent_calme(), MINUIT + 4 * HEURE + 60);
    VERIFIER(e.maree.notable && e.maree.nb == 3);
    VERIFIER(e.tendance == Tendance::Descendante);
    VERIFIER(e.prochaine_basse_s == MINUIT + 9 * HEURE && e.prochaine_pleine_s == MINUIT + 15 * HEURE);
    // après la dernière pleine mer de la série : plus de pleine annoncée (0 = inconnue)
    const EvaluationSpot f = evaluer_spot(spot_ouest(), m, vent_calme(), MINUIT + 16 * HEURE);
    VERIFIER(f.prochaine_pleine_s == 0 && f.prochaine_basse_s == 0);
    return 0;
}

constexpr int le_spot_oriente_change_la_note_pas_la_mer() {
    const Marine m = mer_uniforme(54, 150, 130);
    const Vent v = vent_uniforme(54, 200, 200, 270); // vent d'ouest de 20 km/h
    Spot ouest = spot_ouest();
    Spot est = spot_ouest();
    est.orientation_deg = 90;
    const EvaluationSpot a = evaluer_spot(ouest, m, v, MINUIT + 10 * HEURE);
    const EvaluationSpot b = evaluer_spot(est, m, v, MINUIT + 10 * HEURE);
    VERIFIER(a.maintenant.vent.classe == ClasseVent::Onshore && b.maintenant.vent.classe == ClasseVent::Offshore);
    VERIFIER(b.cle > a.cle && b.cle == 100 && a.cle == 100 - 17); // 18 points de vent sur 35 d'un côté, 35 de l'autre
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(le_bareme_fait_cent_points_et_la_mer_parfaite_dix)                                                               \
    X(le_pire_cas_fait_zero_point_mais_reste_disponible)                                                               \
    X(une_grandeur_absente_rend_la_note_indisponible_jamais_zero)                                                      \
    X(la_note_est_les_points_arrondis_sur_dix)                                                                         \
    X(les_verdicts_suivent_les_seuils_nommes)                                                                          \
    X(hauteur_dans_la_fourchette_40_points_dessous_et_dessus_une_rampe)                                                \
    X(la_fourchette_du_spot_decale_les_rampes)                                                                         \
    X(la_hauteur_est_monotone_unimodale_et_sans_saut_de_plus_d_un_point)                                               \
    X(periode_de_5_s_a_13_s_lineaire_et_plate_aux_bouts)                                                               \
    X(la_periode_est_monotone_et_sans_saut)                                                                            \
    X(vent_de_terre_plein_jusqu_a_25_km_h_puis_zero_a_70)                                                              \
    X(vent_de_travers_de_10_a_45_km_h)                                                                                 \
    X(vent_de_mer_de_10_a_30_km_h)                                                                                     \
    X(la_meme_force_vaut_moins_de_points_selon_la_classe)                                                              \
    X(sous_10_km_h_effectifs_toutes_les_classes_font_35)                                                               \
    X(une_rafale_forte_compte_dans_le_vent_effectif)                                                                   \
    X(le_vent_est_monotone_et_sans_saut_dans_chaque_direction)                                                         \
    X(le_facteur_limitant_est_la_part_qui_manque_le_plus_a_partir_de_8_points)                                         \
    X(a_egalite_le_vent_puis_la_hauteur_puis_la_periode)                                                               \
    X(la_houle_passe_avant_les_vagues_totales)                                                                         \
    X(la_periode_de_houle_manquante_retombe_sur_celle_des_vagues)                                                      \
    X(sans_houle_on_retombe_sur_les_vagues_totales)                                                                    \
    X(une_houle_de_zero_centimetre_est_une_mesure_pas_une_absence)                                                     \
    X(la_jointure_se_fait_par_l_instant_pas_par_le_rang)                                                               \
    X(la_direction_de_la_houle_ne_change_pas_la_note_limite_ecrite)                                                    \
    X(heure_de_rend_le_rang_de_l_heure_qui_contient_l_instant)                                                         \
    X(le_decalage_retenu_est_celui_du_vent_sinon_celui_de_la_mer)                                                      \
    X(le_classement_range_par_cle_decroissante_et_garde_l_ordre_de_saisie_a_egalite)                                   \
    X(un_spot_sans_donnee_passe_apres_tous_les_autres_jamais_devant_sur_un_zero)                                       \
    X(le_classement_tient_les_cas_limites)                                                                             \
    X(la_fabrique_de_test_donne_exactement_les_points_demandes)                                                        \
    X(une_seule_bonne_heure_donne_un_creneau_de_deux_heures_le_plus_tot_a_egalite)                                     \
    X(le_creneau_s_elargit_tant_que_les_heures_voisines_sont_dans_la_tolerance)                                        \
    X(la_tolerance_se_mesure_sur_la_moyenne_des_deux_meilleures_heures)                                                \
    X(le_creneau_ne_depasse_pas_six_heures)                                                                            \
    X(la_meilleure_voisine_passe_avant_et_a_egalite_c_est_la_gauche)                                                   \
    X(une_heure_sans_note_coupe_le_creneau)                                                                            \
    X(aucun_creneau_si_aucune_paire_d_heures_n_a_de_note)                                                              \
    X(la_fenetre_est_de_6_h_a_21_h_locales_les_heures_entieres)                                                        \
    X(le_jour_local_suit_le_decalage_de_la_reponse)                                                                    \
    X(les_decalages_d_une_demi_heure_et_de_trois_quarts_d_heure_sont_respectes)                                        \
    X(les_heures_ecoulees_sont_ecartees_l_heure_en_cours_est_gardee)                                                   \
    X(le_soir_trop_tardif_il_n_y_a_plus_de_creneau_aujourd_hui_mais_il_y_en_a_un_demain)                               \
    X(sans_vent_ou_sans_mer_il_n_y_a_pas_de_creneau)                                                                   \
    X(le_vent_pese_dans_le_creneau)                                                                                    \
    X(evaluer_un_spot_complet_note_creneau_cle)                                                                        \
    X(la_cle_retombe_sur_la_note_de_l_heure_en_cours_puis_sur_moins_un)                                                \
    X(pas_de_vague_ou_pas_de_vent_pas_de_donnees)                                                                      \
    X(la_maree_entre_dans_l_evaluation)                                                                                \
    X(le_spot_oriente_change_la_note_pas_la_mer)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
