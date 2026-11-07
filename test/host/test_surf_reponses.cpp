// Tests du noyau pur « surf » sur des réponses RÉELLES d'Open-Meteo (quinze positions du monde, capturées le 07/11/2026 avec les
// adresses exactes du firmware, voir donnees/LISEZMOI_surf.md) : lecture des deux réponses, marée, vent classé, note heure par
// heure, meilleur créneau d'aujourd'hui et de demain, évaluation et clé de classement. Chaque cas est comparé aux valeurs écrites par
// un SECOND programme, test/host/controle_python_surf.py (lecture de la réponse avec decimal, cosinus de math.cos, marée trouvée
// par une autre méthode, barème réécrit depuis l'ADR) : donnees/surf_cas.inc. Quand un test échoue, il rend 10000 + le rang de la
// première valeur qui diffère ; `python test/host/controle_python_surf.py --expliquer <lieu> <rang>` dit laquelle.
//
// Puis des réponses réelles ABÎMÉES exprès (une seule chose cassée à la fois) : l'analyseur doit refuser la réponse entière avec
// l'erreur et le champ attendus, ou la lire telle quelle quand le défaut est toléré (null, variable absente, unité absente).
//
// Ce que ces tests ne disent pas : si la note est JUSTE pour un spot précis (voir docs/mesures/surf.md, « non vérifié »).
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/surf_note.hpp"
#include "mini.hpp"
#include "surf_outils.hpp"

using namespace ecran::surf;
using namespace surf_test;

namespace {

struct CasSurf {
    const char *id;
    int latitude_e5;
    int longitude_e5;
    int orientation;
    std::string_view mer;
    std::string_view vent;
    const long long *attendu;
    std::size_t nb_attendu;
};

#include "donnees/surf_cas.inc"

constexpr std::size_t NB_CAS = sizeof(SURF_CAS) / sizeof(SURF_CAS[0]);

// ---- aplatir : la liste des valeurs à comparer, dans l'ordre de controle_python_surf.py ---------------------------------------

struct Plat {
    long long v[2400] = {};
    std::size_t n = 0;
    bool trop = false;
    constexpr void ajouter(long long x) noexcept {
        if (n < sizeof(v) / sizeof(v[0]))
            v[n++] = x;
        else
            trop = true;
    }
};

constexpr Plat aplatir(const Spot &spot, const Marine &mer, const Vent &vent, std::int64_t maintenant) noexcept {
    Plat p;
    // A. la mer
    p.ajouter(mer.decalage_utc_s);
    p.ajouter(mer.nb);
    p.ajouter(mer.a_vagues);
    p.ajouter(mer.a_houle);
    p.ajouter(mer.a_maree);
    p.ajouter(mer.a_temperature);
    for (std::size_t i = 0; i < mer.nb; ++i) {
        const MarineHeure &h = mer.heures[i];
        p.ajouter(h.debut_s);
        p.ajouter(h.vagues_cm);
        p.ajouter(h.vagues_periode_ds);
        p.ajouter(h.vagues_direction_deg);
        p.ajouter(h.houle_cm);
        p.ajouter(h.houle_periode_ds);
        p.ajouter(h.houle_direction_deg);
        p.ajouter(h.mer_vent_cm);
        p.ajouter(h.mer_vent_periode_ds);
        p.ajouter(h.mer_vent_direction_deg);
        p.ajouter(h.temperature_mer_dixiemes);
        p.ajouter(h.maree_cm);
    }
    // B. le vent
    p.ajouter(vent.decalage_utc_s);
    p.ajouter(vent.nb);
    for (std::size_t i = 0; i < vent.nb; ++i) {
        const VentHeure &h = vent.heures[i];
        p.ajouter(h.debut_s);
        p.ajouter(h.vitesse_dixiemes_kmh);
        p.ajouter(h.rafale_dixiemes_kmh);
        p.ajouter(h.direction_deg);
    }
    // C. la marée
    const Maree m = extraire_maree(mer);
    p.ajouter(m.notable);
    p.ajouter(m.nb);
    p.ajouter(m.marnage_cm);
    p.ajouter(m.serie_debut_s);
    p.ajouter(m.serie_fin_s);
    for (std::size_t k = 0; k < m.nb; ++k) {
        p.ajouter(m.extrema[k].instant_s);
        p.ajouter(m.extrema[k].niveau_cm);
        p.ajouter(m.extrema[k].pleine);
    }
    p.ajouter(static_cast<int>(tendance_a(m, maintenant)));
    const Extremum *pleine = prochain_extremum(m, true, maintenant);
    const Extremum *basse = prochain_extremum(m, false, maintenant);
    p.ajouter(pleine != nullptr ? pleine->instant_s : 0);
    p.ajouter(basse != nullptr ? basse->instant_s : 0);
    // D. et E. le vent classé et la note, heure par heure
    for (std::size_t i = 0; i < mer.nb; ++i) {
        const Conditions c = conditions_a(mer, vent, i);
        const VentSurf vs =
            classer_vent(c.vent_dixiemes_kmh, c.rafale_dixiemes_kmh, c.vent_direction_deg, spot.orientation_deg);
        p.ajouter(static_cast<int>(vs.classe));
        p.ajouter(vs.composante_dixiemes_kmh);
        p.ajouter(vs.effectif_dixiemes_kmh);
        const Note n = noter(spot, c);
        p.ajouter(n.disponible);
        p.ajouter(n.points);
        p.ajouter(n.points_hauteur);
        p.ajouter(n.points_periode);
        p.ajouter(n.points_vent);
        p.ajouter(n.note);
        p.ajouter(static_cast<int>(n.hauteur));
        p.ajouter(static_cast<int>(n.limitant));
    }
    // F. le meilleur créneau d'aujourd'hui puis de demain
    for (int jour = 0; jour <= 1; ++jour) {
        const Creneau c = meilleur_creneau(spot, mer, vent, maintenant, decalage_a_retenir(mer, vent), jour);
        p.ajouter(c.trouve);
        p.ajouter(c.debut_s);
        p.ajouter(c.fin_s);
        p.ajouter(c.heures);
        p.ajouter(c.points);
        p.ajouter(c.note);
    }
    // G. l'évaluation de l'heure en cours et la clé de classement
    const EvaluationSpot e = evaluer_spot(spot, mer, vent, maintenant);
    p.ajouter(e.donnees);
    p.ajouter(e.maintenant.disponible);
    p.ajouter(e.maintenant.points);
    p.ajouter(e.cle);
    return p;
}

constexpr Spot spot_du_cas(const CasSurf &c) noexcept {
    Spot s{};
    for (std::size_t i = 0; c.id[i] != '\0' && i + 1 < sizeof(s.nom); ++i)
        s.nom[i] = c.id[i];
    s.latitude_e5 = c.latitude_e5;
    s.longitude_e5 = c.longitude_e5;
    s.orientation_deg = static_cast<std::int16_t>(c.orientation);
    return s; // la fourchette de hauteur est celle par défaut, comme dans le script Python
}

constexpr bool egal_texte(const char *a, const char *b) noexcept {
    std::size_t i = 0;
    for (; a[i] != '\0' && b[i] != '\0'; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return a[i] == b[i];
}

// 0 : identique. 1 à 4 : une étape préalable a échoué. 10000 + r : la valeur de rang r diffère. 20000 + r : le nombre de valeurs diffère.
constexpr int jouer(const CasSurf &c) noexcept {
    const AnalyseMarine am = analyser_marine(c.mer);
    if (!am.ok())
        return 1;
    const AnalyseVent av = analyser_vent(c.vent);
    if (!av.ok())
        return 2;
    const Spot spot = spot_du_cas(c);
    if (!spot_valide(spot))
        return 3;
    const Plat p = aplatir(spot, am.marine, av.vent, SURF_MAINTENANT_S);
    if (p.trop)
        return 4;
    const std::size_t commun = p.n < c.nb_attendu ? p.n : c.nb_attendu;
    for (std::size_t i = 0; i < commun; ++i) {
        if (p.v[i] != c.attendu[i])
            return 10000 + static_cast<int>(i);
    }
    return p.n == c.nb_attendu ? 0 : 20000 + static_cast<int>(commun);
}

constexpr Marine mer_du(const CasSurf &c) noexcept { return analyser_marine(c.mer).marine; }
constexpr Vent vent_du(const CasSurf &c) noexcept { return analyser_vent(c.vent).vent; }

// Un test par position, comparé aux valeurs du second programme. L'ordre des cas est celui de surf_manifeste.json.
#define UN_CAS(RANG, ID)                                                                                               \
    constexpr int cas_reel_##ID() {                                                                                    \
        VERIFIER(egal_texte(SURF_CAS[RANG].id, #ID));                                                                  \
        return jouer(SURF_CAS[RANG]);                                                                                  \
    }
UN_CAS(0, hossegor)
UN_CAS(1, hawai_pipeline)
UN_CAS(2, gold_coast)
UN_CAS(3, nazare)
UN_CAS(4, jeffreys_bay)
UN_CAS(5, uluwatu)
UN_CAS(6, le_cap)
UN_CAS(7, pichilemu)
UN_CAS(8, mentawai)
UN_CAS(9, irlande_lahinch)
UN_CAS(10, baltique_warnemunde)
UN_CAS(11, mediterranee_marseille)
UN_CAS(12, saint_malo)
UN_CAS(13, michigan)
UN_CAS(14, paris)

// ---- la table de cosinus -------------------------------------------------------------------------------------------------

constexpr int la_table_de_cosinus_est_celle_de_math_cos() {
    for (int d = 0; d <= 90; ++d)
        VERIFIER(cosinus_pm(d) == SURF_COSINUS_PM[d]);
    for (int d = -360; d <= 360; ++d) {
        const int e = ecart_angulaire(d, 0); // 0..180
        const int attendu = e <= 90 ? SURF_COSINUS_PM[e] : -SURF_COSINUS_PM[180 - e];
        VERIFIER(cosinus_pm(d) == attendu);
    }
    return 0;
}

// ---- ce que les quinze positions montrent --------------------------------------------------------------------------------

constexpr int quinze_positions_et_la_capture_se_correspondent() {
    VERIFIER(NB_CAS == 15);
    for (std::size_t k = 0; k < NB_CAS; ++k) {
        const AnalyseMarine am = analyser_marine(SURF_CAS[k].mer);
        const AnalyseVent av = analyser_vent(SURF_CAS[k].vent);
        VERIFIER(am.ok() && av.ok());
        VERIFIER(am.marine.nb == HEURES_ATTENDUES &&
                 av.vent.nb == HEURES_ATTENDUES); // 6 h de passé + 48 h de futur, comme demandé
        VERIFIER(am.marine.heures[0].debut_s == SURF_PREMIERE_HEURE_S &&
                 av.vent.heures[0].debut_s == SURF_PREMIERE_HEURE_S);
        VERIFIER(heure_de(am.marine, SURF_MAINTENANT_S) == PASSE_HEURES); // « maintenant » tombe dans l'heure n° 6
        VERIFIER(spot_valide(spot_du_cas(SURF_CAS[k])));
    }
    return 0;
}

constexpr int paris_pas_de_mer_toutes_les_valeurs_sont_absentes_jamais_zero() {
    const CasSurf &c = SURF_CAS[14];
    const Marine m = mer_du(c);
    VERIFIER(m.nb == 54 && !m.a_vagues && !m.a_houle && !m.a_maree && !m.a_temperature);
    for (std::size_t i = 0; i < m.nb; ++i) {
        const MarineHeure &h = m.heures[i];
        VERIFIER(h.vagues_cm == ABSENT && h.houle_cm == ABSENT && h.houle_periode_ds == ABSENT && h.maree_cm == ABSENT);
        VERIFIER(h.temperature_mer_dixiemes == ABSENT && h.houle_direction_deg == ABSENT && h.mer_vent_cm == ABSENT);
    }
    const Vent v = vent_du(c);
    VERIFIER(v.nb == 54 && v.heures[0].vitesse_dixiemes_kmh != ABSENT); // le vent, lui, existe à Paris
    const EvaluationSpot e = evaluer_spot(spot_du_cas(c), m, v, SURF_MAINTENANT_S);
    VERIFIER(!e.donnees && !e.maintenant.disponible && !e.creneau.trouve && e.cle == -1);
    VERIFIER(!e.maree.notable && e.tendance == Tendance::Inconnue);
    VERIFIER(!noter(spot_du_cas(c), conditions_a(m, v, 6)).disponible);
    return 0;
}

constexpr int michigan_un_lac_des_vagues_mais_ni_maree_ni_temperature() {
    const Marine m = mer_du(SURF_CAS[13]);
    VERIFIER(m.a_vagues && !m.a_maree && !m.a_temperature);
    const EvaluationSpot e = evaluer_spot(spot_du_cas(SURF_CAS[13]), m, vent_du(SURF_CAS[13]), SURF_MAINTENANT_S);
    VERIFIER(e.donnees && !e.maree.notable && e.tendance == Tendance::Inconnue && e.prochaine_pleine_s == 0);
    return 0;
}

constexpr int amplitude_du_niveau_de_mer_cm(const Marine &m) noexcept {
    int haut = -100000;
    int bas = 100000;
    bool une = false;
    for (std::size_t i = 0; i < m.nb; ++i) {
        if (!present(m.heures[i].maree_cm))
            continue;
        une = true;
        haut = m.heures[i].maree_cm > haut ? m.heures[i].maree_cm : haut;
        bas = m.heures[i].maree_cm < bas ? m.heures[i].maree_cm : bas;
    }
    return une ? haut - bas : -1;
}

// C'est ce test qui justifie le seuil de 30 cm de surf_maree.hpp : sur 54 heures, la Baltique (20 cm) et la Méditerranée (27 cm) varient
// SANS marée ; la plus faible marée mesurée (Oahu) varie de 68 cm. Les neuf autres littoraux ont une marée notable.
constexpr int le_seuil_de_30_cm_separe_les_mers_sans_maree_des_marees_mesurees() {
    const Marine baltique = mer_du(SURF_CAS[10]);
    const Marine marseille = mer_du(SURF_CAS[11]);
    const Marine oahu = mer_du(SURF_CAS[1]);
    VERIFIER(amplitude_du_niveau_de_mer_cm(baltique) == 20 && amplitude_du_niveau_de_mer_cm(marseille) == 27);
    VERIFIER(amplitude_du_niveau_de_mer_cm(oahu) == 68);
    VERIFIER(27 < MAREE_AMPLITUDE_MIN_CM && MAREE_AMPLITUDE_MIN_CM < 68);
    VERIFIER(!extraire_maree(baltique).notable && !extraire_maree(marseille).notable);
    VERIFIER(extraire_maree(oahu).notable && extraire_maree(oahu).marnage_cm == 68);
    // les neuf autres littoraux de mer ouverte : hossegor, gold_coast, nazare, jeffreys_bay, uluwatu, le_cap, pichilemu, mentawai,
    // irlande_lahinch et saint_malo (dix en tout avec Oahu mis à part)
    for (const std::size_t k : {std::size_t{0}, std::size_t{2}, std::size_t{3}, std::size_t{4}, std::size_t{5},
                                std::size_t{6}, std::size_t{7}, std::size_t{8}, std::size_t{9}, std::size_t{12}}) {
        const Maree m = extraire_maree(mer_du(SURF_CAS[k]));
        VERIFIER(m.notable && m.marnage_cm >= 68 && m.nb >= 3);
    }
    // sans niveau de mer (lac, terre) : pas de marée, et ce n'est pas la même chose que « marée plate »
    VERIFIER(amplitude_du_niveau_de_mer_cm(mer_du(SURF_CAS[13])) == -1 &&
             amplitude_du_niveau_de_mer_cm(mer_du(SURF_CAS[14])) == -1);
    return 0;
}

constexpr int saint_malo_a_la_plus_grande_maree_et_la_tendance_est_cherchee_a_l_heure_de_la_capture() {
    const Maree sm = extraire_maree(mer_du(SURF_CAS[12]));
    VERIFIER(sm.notable &&
             sm.marnage_cm >= 900); // la baie du Mont-Saint-Michel : le plus fort marnage d'Europe continentale
    for (std::size_t k = 0; k < NB_CAS; ++k)
        VERIFIER(extraire_maree(mer_du(SURF_CAS[k])).marnage_cm <= sm.marnage_cm);
    const Tendance t = tendance_a(sm, SURF_MAINTENANT_S);
    VERIFIER(t == Tendance::Montante || t == Tendance::Descendante || t == Tendance::Etale);
    return 0;
}

// Les invariants d'un créneau, sur chaque position et chaque jour : heures entières, 6 h à 21 h LOCALES du bon jour, de 2 à 6 heures.
constexpr int invariants_du_creneau(const Creneau &c, std::int64_t maintenant, std::int32_t decalage,
                                    int jour) noexcept {
    if (!c.trouve)
        return c.heures == 0 && c.points == 0 && c.note == 0 && c.debut_s == 0 && c.fin_s == 0 ? 0 : 1;
    if (c.heures < CRENEAU_HEURES_MIN || c.heures > CRENEAU_HEURES_MAX)
        return 2;
    if (c.fin_s - c.debut_s != static_cast<std::int64_t>(c.heures) * 3600)
        return 3;
    if (c.fin_s <= maintenant) // un créneau entièrement écoulé n'en est plus un ; l'heure en cours est permise
        return 4;
    const std::int64_t jour_local = division_plancher(c.debut_s + decalage, 86400);
    if (jour_local != division_plancher(maintenant + decalage, 86400) + jour)
        return 5;
    const std::int64_t debut_du_jour = c.debut_s + decalage - jour_local * 86400;
    if (debut_du_jour < CRENEAU_DEBUT_H * 3600 || debut_du_jour + c.fin_s - c.debut_s > CRENEAU_FIN_H * 3600)
        return 6;
    if (c.points > 100 || c.note != (c.points + 5) / 10)
        return 7;
    return 0;
}

constexpr int les_creneaux_reels_respectent_la_fenetre_locale_et_la_duree() {
    for (std::size_t k = 0; k < NB_CAS; ++k) {
        const Marine m = mer_du(SURF_CAS[k]);
        const Vent v = vent_du(SURF_CAS[k]);
        const Spot s = spot_du_cas(SURF_CAS[k]);
        const std::int32_t d = decalage_a_retenir(m, v);
        for (int jour = 0; jour <= 1; ++jour)
            VERIFIER(invariants_du_creneau(meilleur_creneau(s, m, v, SURF_MAINTENANT_S, d, jour), SURF_MAINTENANT_S, d,
                                           jour) == 0);
    }
    return 0;
}

constexpr int a_gold_coast_et_uluwatu_il_est_trop_tard_pour_aujourd_hui() {
    // 12 h 51 UTC le 07/11/2026 : 23 h 51 à Gold Coast (UTC+11), 20 h 51 à Uluwatu (UTC+8). Plus aucune heure entière de la fenêtre
    // de 6 h à 21 h n'est à venir ; demain, si.
    for (const std::size_t k : {std::size_t{2}, std::size_t{5}}) {
        const Marine m = mer_du(SURF_CAS[k]);
        const Vent v = vent_du(SURF_CAS[k]);
        const std::int32_t d = decalage_a_retenir(m, v);
        VERIFIER(d == (k == 2 ? 39600 : 28800));
        VERIFIER(!meilleur_creneau(spot_du_cas(SURF_CAS[k]), m, v, SURF_MAINTENANT_S, d, 0).trouve);
        VERIFIER(meilleur_creneau(spot_du_cas(SURF_CAS[k]), m, v, SURF_MAINTENANT_S, d, 1).trouve);
    }
    // À Hossegor (UTC+1, 13 h 51 locales) il reste de la journée.
    const Marine m = mer_du(SURF_CAS[0]);
    const Vent v = vent_du(SURF_CAS[0]);
    VERIFIER(meilleur_creneau(spot_du_cas(SURF_CAS[0]), m, v, SURF_MAINTENANT_S, decalage_a_retenir(m, v), 0).trouve);
    return 0;
}

constexpr int le_classement_des_quinze_positions_met_le_sans_donnee_en_dernier_et_reste_stable() {
    std::int16_t cles[NB_CAS] = {};
    std::uint8_t ordre[NB_CAS] = {};
    for (std::size_t k = 0; k < NB_CAS; ++k)
        cles[k] =
            evaluer_spot(spot_du_cas(SURF_CAS[k]), mer_du(SURF_CAS[k]), vent_du(SURF_CAS[k]), SURF_MAINTENANT_S).cle;
    classer(cles, NB_CAS, ordre);
    VERIFIER(cles[14] == -1); // Paris : pas de mer
    VERIFIER(ordre[NB_CAS - 1] == 14);
    for (std::size_t r = 0; r + 1 < NB_CAS; ++r) {
        const std::int16_t a = cles[ordre[r]];
        const std::int16_t b = cles[ordre[r + 1]];
        VERIFIER(a >= b);
        VERIFIER(a != b || ordre[r] < ordre[r + 1]); // à clés égales, l'ordre de saisie
    }
    for (std::size_t k = 0; k < NB_CAS - 1; ++k)
        VERIFIER(cles[k] >= 0 && cles[k] <= 100); // toutes les autres positions ont une clé
    return 0;
}

// ---- réponses réelles abîmées exprès ------------------------------------------------------------------------------------

constexpr int la_mer_hossegor_reelle_est_lue_telle_quelle() {
    const AnalyseMarine a = analyser_marine(SURF_MER_HOSSEGOR);
    VERIFIER(a.ok() && a.diag.champ.empty());
    const Marine &m = a.marine;
    VERIFIER(m.decalage_utc_s == 3600 && m.nb == 54 && m.a_vagues && m.a_houle && m.a_maree && m.a_temperature);
    VERIFIER(m.heures[0].debut_s == 1794031200 && m.heures[0].vagues_cm == 66 &&
             m.heures[0].vagues_direction_deg == 303);
    VERIFIER(m.heures[0].maree_cm == -154 && m.heures[1].maree_cm == -187); // -1,54 m puis -1,87 m
    VERIFIER(m.heures[0].houle_cm == 58);
    return 0;
}

constexpr int le_fuseau_horaire_doit_etre_present_entier_et_plausible() {
    const sv ancien = "\"utc_offset_seconds\":3600";
    VERIFIER(
        refuse(remplacer(SURF_MER_HOSSEGOR, ancien, "\"x\":3600").vue(), Erreur::ChampAbsent, "utc_offset_seconds"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":null").vue(), Erreur::ChampAbsent,
                    "utc_offset_seconds"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":\"3600\"").vue(), Erreur::MauvaisType,
                    "utc_offset_seconds"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":50401").vue(), Erreur::HorsPlage,
                    "utc_offset_seconds"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":-43201").vue(), Erreur::HorsPlage,
                    "utc_offset_seconds"));
    VERIFIER(analyser_marine(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":50400").vue()).ok());
    VERIFIER(analyser_marine(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":-43200").vue()).ok());
    VERIFIER(analyser_marine(remplacer(SURF_MER_HOSSEGOR, ancien, "\"utc_offset_seconds\":20700").vue())
                 .marine.decalage_utc_s == 20700);
    VERIFIER(refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "\"utc_offset_seconds\":3600", "\"y\":3600").vue(),
                         Erreur::ChampAbsent, "utc_offset_seconds"));
    return 0;
}

constexpr int les_heures_doivent_etre_presentes_entieres_et_se_suivre_d_une_heure() {
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"hourly\":{", "\"houly\":{").vue(), Erreur::ChampAbsent, "hourly"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"hourly\":{\"time\":[", "\"hourly\":{\"temps\":[").vue(),
                    Erreur::ChampAbsent, "hourly.time"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "1794031200", "\"x\"").vue(), Erreur::MauvaisType, "hourly.time"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "1794031200", "10").vue(), Erreur::HorsPlage, "hourly.time"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "1794034800", "1794034801").vue(), Erreur::TableauxIncoherents,
                    "hourly.time"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "1794034800", "1794031200").vue(), Erreur::TableauxIncoherents,
                    "hourly.time"));
    VERIFIER(refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "1794034800", "1794038400").vue(), Erreur::TableauxIncoherents,
                         "hourly.time"));
    VERIFIER(
        refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "1794031200", "4200000000").vue(), Erreur::HorsPlage, "hourly.time"));
    return 0;
}

constexpr int une_unite_inattendue_refuse_la_reponse() {
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":\"m\"", "\"wave_height\":\"ft\"").vue(),
                    Erreur::UniteInattendue, "hourly.wave_height"));
    VERIFIER(
        refuse(remplacer(SURF_MER_HOSSEGOR, "\"sea_level_height_msl\":\"m\"", "\"sea_level_height_msl\":\"cm\"").vue(),
               Erreur::UniteInattendue, "hourly.sea_level_height_msl"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_period\":\"s\"", "\"wave_period\":\"min\"").vue(),
                    Erreur::UniteInattendue, "hourly.wave_period"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR,
                              "\"sea_surface_temperature\":\"\xC2\xB0"
                              "C\"",
                              "\"sea_surface_temperature\":\"\xC2\xB0"
                              "F\"")
                        .vue(),
                    Erreur::UniteInattendue, "hourly.sea_surface_temperature"));
    VERIFIER(refuse(
        remplacer(SURF_MER_HOSSEGOR, "\"swell_wave_direction\":\"\xC2\xB0\"", "\"swell_wave_direction\":\"rad\"").vue(),
        Erreur::UniteInattendue, "hourly.swell_wave_direction"));
    VERIFIER(
        refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_speed_10m\":\"km/h\"", "\"wind_speed_10m\":\"mph\"").vue(),
                    Erreur::UniteInattendue, "hourly.wind_speed_10m"));
    VERIFIER(
        refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_gusts_10m\":\"km/h\"", "\"wind_gusts_10m\":\"m/s\"").vue(),
                    Erreur::UniteInattendue, "hourly.wind_gusts_10m"));
    // Une unité absente de la réponse n'est pas refusée : on ne refuse que ce qu'on SAIT être faux.
    VERIFIER(
        analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":\"m\"", "\"wave_heightx\":\"m\"").vue()).ok());
    VERIFIER(analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"hourly_units\":{", "\"unites\":{").vue()).ok());
    return 0;
}

constexpr int une_serie_de_longueur_differente_une_valeur_invraisemblable_ou_d_un_autre_type_refuse_la_reponse() {
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[0.66,0.66,").vue(),
                    Erreur::TableauxIncoherents, "hourly.wave_height")); // 55 valeurs pour 54 heures
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[").vue(),
                    Erreur::TableauxIncoherents, "hourly.wave_height")); // 53
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[-0.66,").vue(),
                    Erreur::HorsPlage, "hourly.wave_height"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[31.0,").vue(),
                    Erreur::HorsPlage,
                    "hourly.wave_height")); // 31 m : au-delà de 30 m
    VERIFIER(
        analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[30.0,").vue()).ok());
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[\"haute\",").vue(),
                    Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[true,").vue(),
                    Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":5,").vue(),
                    Erreur::JsonInvalide, ""));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_direction\":[303,", "\"wave_direction\":[400,").vue(),
                    Erreur::HorsPlage, "hourly.wave_direction"));
    VERIFIER(refuse(remplacer(SURF_MER_HOSSEGOR, "\"wave_direction\":[303,", "\"wave_direction\":[-1,").vue(),
                    Erreur::HorsPlage, "hourly.wave_direction"));
    VERIFIER(refuse(
        remplacer(SURF_MER_HOSSEGOR, "\"sea_level_height_msl\":[-1.54,", "\"sea_level_height_msl\":[21.0,").vue(),
        Erreur::HorsPlage, "hourly.sea_level_height_msl"));
    VERIFIER(refuse(
        remplacer(SURF_MER_HOSSEGOR, "\"sea_level_height_msl\":[-1.54,", "\"sea_level_height_msl\":[-21.0,").vue(),
        Erreur::HorsPlage, "hourly.sea_level_height_msl"));
    VERIFIER(refuse(
        remplacer(SURF_MER_HOSSEGOR, "\"sea_surface_temperature\":[", "\"sea_surface_temperature\":[99.9,").vue(),
        Erreur::TableauxIncoherents, "hourly.sea_surface_temperature"));
    VERIFIER(refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_speed_10m\":[19.8,", "\"wind_speed_10m\":[-0.1,").vue(),
                         Erreur::HorsPlage, "hourly.wind_speed_10m"));
    VERIFIER(refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_speed_10m\":[19.8,", "\"wind_speed_10m\":[500.1,").vue(),
                         Erreur::HorsPlage, "hourly.wind_speed_10m"));
    VERIFIER(
        analyser_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_speed_10m\":[19.8,", "\"wind_speed_10m\":[500.0,").vue())
            .ok());
    VERIFIER(
        refuse_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_direction_10m\":[109,", "\"wind_direction_10m\":[361,").vue(),
                    Erreur::HorsPlage, "hourly.wind_direction_10m"));
    return 0;
}

constexpr int un_null_est_pas_de_donnee_jamais_zero_et_360_degres_font_zero() {
    const AnalyseMarine a =
        analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"wave_height\":[0.66,", "\"wave_height\":[null,").vue());
    VERIFIER(a.ok() && a.marine.a_vagues);
    VERIFIER(a.marine.heures[0].vagues_cm == ABSENT &&
             a.marine.heures[1].vagues_cm == 66); // la 1re heure seule est absente
    const AnalyseMarine b =
        analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"wave_direction\":[303,", "\"wave_direction\":[360,").vue());
    VERIFIER(b.ok() && b.marine.heures[0].vagues_direction_deg == 0); // 360° = 0° : une seule écriture du nord
    const AnalyseMarine c =
        analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"wave_direction\":[303,", "\"wave_direction\":[0,").vue());
    VERIFIER(c.ok() && c.marine.heures[0].vagues_direction_deg == 0);
    const AnalyseVent d = analyser_vent(
        remplacer(SURF_VENT_HOSSEGOR, "\"wind_direction_10m\":[109,", "\"wind_direction_10m\":[360,").vue());
    VERIFIER(d.ok() && d.vent.heures[0].direction_deg == 0);
    const AnalyseVent e =
        analyser_vent(remplacer(SURF_VENT_HOSSEGOR, "\"wind_speed_10m\":[19.8,", "\"wind_speed_10m\":[null,").vue());
    VERIFIER(e.ok() && e.vent.heures[0].vitesse_dixiemes_kmh == ABSENT && e.vent.heures[1].vitesse_dixiemes_kmh == 218);
    // une note sur une heure dont le vent est null est indisponible, pas calculée sur un vent nul
    const AnalyseMarine m = analyser_marine(SURF_MER_HOSSEGOR);
    VERIFIER(!noter(spot_ouest(), conditions_a(m.marine, e.vent, 0)).disponible);
    VERIFIER(noter(spot_ouest(), conditions_a(m.marine, e.vent, 1)).disponible);
    return 0;
}

constexpr int une_variable_absente_de_la_reponse_est_toleree_et_la_houle_retombe_sur_les_vagues() {
    const AnalyseMarine a =
        analyser_marine(remplacer(SURF_MER_HOSSEGOR, "\"swell_wave_height\":[", "\"swell_wave_heightx\":[").vue());
    VERIFIER(a.ok() && !a.marine.a_houle && a.marine.a_vagues);
    for (std::size_t i = 0; i < a.marine.nb; ++i)
        VERIFIER(a.marine.heures[i].houle_cm == ABSENT);
    const AnalyseVent v = analyser_vent(SURF_VENT_HOSSEGOR);
    const Conditions c = conditions_a(a.marine, v.vent, 0);
    // sans hauteur de houle, la hauteur ET la période sont celles des vagues totales (66 cm, 10,8 s), pas la période de houle (9,9 s)
    VERIFIER(c.hauteur_cm == 66 && c.periode_ds == 108 && a.marine.heures[0].houle_periode_ds == 99);
    // sans niveau de mer, plus de marée
    const AnalyseMarine b = analyser_marine(
        remplacer(SURF_MER_HOSSEGOR, "\"sea_level_height_msl\":[", "\"sea_level_height_mslx\":[").vue());
    VERIFIER(b.ok() && !b.marine.a_maree && !extraire_maree(b.marine).notable);
    // sans vent (vitesse, direction, rafales renommées), la réponse est lue mais aucune note n'existe
    const AnalyseVent w =
        analyser_vent(remplacer(remplacer(SURF_VENT_HOSSEGOR, "\"wind_speed_10m\":[", "\"wind_speed_10mx\":[").vue(),
                                "\"wind_direction_10m\":[", "\"wind_direction_10mx\":[")
                          .vue());
    VERIFIER(w.ok() && w.vent.heures[0].vitesse_dixiemes_kmh == ABSENT && w.vent.heures[0].direction_deg == ABSENT);
    const AnalyseMarine m = analyser_marine(SURF_MER_HOSSEGOR);
    for (std::size_t i = 0; i < m.marine.nb; ++i)
        VERIFIER(!noter(spot_ouest(), conditions_a(m.marine, w.vent, i)).disponible);
    return 0;
}

// Ces tests relisent les quinze réponses réelles ou les abîment une à une. Ils ne sont PAS évalués à la compilation :
// le faire demande environ six minutes de compilation (mesuré, voir docs/mesures/surf.md). Ils tournent à l'exécution : le
// lanceur doit être en mode « complet » (compilé + exécuté), pas en mode « syntaxe ». Les tests de lecture sur des réponses
// fabriquées, rapides et évalués aussi à la compilation, sont dans test_surf_lecture.cpp.
#define LISTE(X)                                                                                                       \
    X(cas_reel_hossegor)                                                                                               \
    X(cas_reel_hawai_pipeline)                                                                                         \
    X(cas_reel_gold_coast)                                                                                             \
    X(cas_reel_nazare)                                                                                                 \
    X(cas_reel_jeffreys_bay)                                                                                           \
    X(cas_reel_uluwatu)                                                                                                \
    X(cas_reel_le_cap)                                                                                                 \
    X(cas_reel_pichilemu)                                                                                              \
    X(cas_reel_mentawai)                                                                                               \
    X(cas_reel_irlande_lahinch)                                                                                        \
    X(cas_reel_baltique_warnemunde)                                                                                    \
    X(cas_reel_mediterranee_marseille)                                                                                 \
    X(cas_reel_saint_malo)                                                                                             \
    X(cas_reel_michigan)                                                                                               \
    X(cas_reel_paris)                                                                                                  \
    X(la_table_de_cosinus_est_celle_de_math_cos)                                                                       \
    X(quinze_positions_et_la_capture_se_correspondent)                                                                 \
    X(paris_pas_de_mer_toutes_les_valeurs_sont_absentes_jamais_zero)                                                   \
    X(michigan_un_lac_des_vagues_mais_ni_maree_ni_temperature)                                                         \
    X(le_seuil_de_30_cm_separe_les_mers_sans_maree_des_marees_mesurees)                                                \
    X(saint_malo_a_la_plus_grande_maree_et_la_tendance_est_cherchee_a_l_heure_de_la_capture)                           \
    X(les_creneaux_reels_respectent_la_fenetre_locale_et_la_duree)                                                     \
    X(a_gold_coast_et_uluwatu_il_est_trop_tard_pour_aujourd_hui)                                                       \
    X(le_classement_des_quinze_positions_met_le_sans_donnee_en_dernier_et_reste_stable)                                \
    X(la_mer_hossegor_reelle_est_lue_telle_quelle)                                                                     \
    X(le_fuseau_horaire_doit_etre_present_entier_et_plausible)                                                         \
    X(les_heures_doivent_etre_presentes_entieres_et_se_suivre_d_une_heure)                                             \
    X(une_unite_inattendue_refuse_la_reponse)                                                                          \
    X(une_serie_de_longueur_differente_une_valeur_invraisemblable_ou_d_un_autre_type_refuse_la_reponse)                \
    X(un_null_est_pas_de_donnee_jamais_zero_et_360_degres_font_zero)                                                   \
    X(une_variable_absente_de_la_reponse_est_toleree_et_la_houle_retombe_sur_les_vagues)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
