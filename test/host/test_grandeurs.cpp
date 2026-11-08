// Tests du noyau pur : la page « Détails » (grandeurs.hpp) — les sept grandeurs de l'ancien accueil, leur état manquant, la mise en page.
//
// D'où viennent les valeurs de référence : les exemples sont ceux des essais de visuel_echelles (catégories UV, tendance de la pression,
// secteurs du vent) et les écritures de l'ancien accueil (main/ui_principal.cpp avant ca84569 : « Rafales 26 », « 2 Faible », « 1024 hPa ») ;
// les heures du soleil sont calculées à la main (07:12 + 09:58 de jour, midi à 48 % de l'arc). Aucune n'est produite par le code testé.
#include <string_view>

#include "ecran/grandeurs.hpp"
#include "mini.hpp"

using namespace ecran::grandeurs;
using ecran::i18n::Langue;
using ecran::visuel::INCONNU;
using ecran::visuel::INCONNU_DIX;

namespace {

constexpr std::string_view sv(const char *s) { return s; }

// Un temps ordinaire (Rennes, à midi) : tout est connu.
constexpr ecran::heure::Fuseau CET_FIXE =
    ecran::heure::fuseau_fixe(3600);                     // UTC+1, sans heure d'été : 07:12 locale = 06:12 UTC
constexpr std::int64_t MINUIT_UTC = 1793750400;          // 2026-11-05 00:00 UTC, minuit local = 23:00 UTC la veille
constexpr std::int64_t MINUIT_LOCAL = MINUIT_UTC - 3600; // 2026-11-05 00:00 locale
constexpr std::int64_t LEVER = MINUIT_LOCAL + 7 * 3600 + 12 * 60;    // 07:12
constexpr std::int64_t COUCHER = MINUIT_LOCAL + 17 * 3600 + 10 * 60; // 17:10
constexpr std::int64_t MIDI = MINUIT_LOCAL + 12 * 3600;              // 12:00

constexpr ecran::visuel::VueMeteo ordinaire() {
    ecran::visuel::VueMeteo m{};
    m.connue = true;
    m.instant_s = MIDI - 600;
    m.vent_dix_kmh = 142;
    m.rafales_dix_kmh = 261;
    m.direction_deg = 225;
    m.humidite_pct = 58;
    m.pression_dix_hpa = 10243;
    m.tendance_dix_hpa_3h = 12;
    m.uv_dix = 24;
    m.proba_pluie_pct = 5;
    m.pluie_dix_mm = 0;
    m.pluie_dans_min = -1;
    m.lever_s = LEVER;
    m.coucher_s = COUCHER;
    m.jour = true;
    return m;
}

constexpr bool egal(const char *a, const char *b) { return sv(a) == sv(b); }

// ---- textes ----

constexpr int les_deux_phrases_de_la_page_ont_leurs_six_traductions() {
    for (const ecran::visuel::Texte &x : TEXTES) {
        VERIFIER(!x.fr.empty());
        for (std::string_view t : x.t) {
            VERIFIER(!t.empty());
        }
    }
    VERIFIER(NOMBRE_TEXTES == 2);
    return 0;
}

constexpr int les_phrases_de_la_page_dans_chaque_langue() {
    VERIFIER(texte("Détails", Langue::Fr) == "Détails");
    VERIFIER(texte("Détails", Langue::En) == "Details");
    VERIFIER(texte("Détails", Langue::Es) == "Detalles");
    VERIFIER(texte("Détails", Langue::It) == "Dettagli");
    VERIFIER(texte("Détails", Langue::Tr) == "Ayrıntılar");
    VERIFIER(texte("Fermer", Langue::Fr) == "Fermer");
    VERIFIER(texte("Fermer", Langue::En) == "Close");
    VERIFIER(texte("Fermer", Langue::De) == "Schließen");
    VERIFIER(texte("Fermer", Langue::Nl) == "Sluiten");
    return 0;
}

constexpr int les_mots_des_grandeurs_viennent_de_la_table_de_l_ecran_principal() {
    VERIFIER(texte("Humidité", Langue::De) == "Feuchte");
    VERIFIER(texte("Pleine lune", Langue::En) == "Full moon");
    VERIFIER(texte("Texte qui n'existe nulle part", Langue::De) == "Texte qui n'existe nulle part");
    return 0;
}

// ---- vent ----

constexpr int le_vent_ordinaire() {
    const Tuile t = tuile_vent(ordinaire(), Langue::Fr);
    VERIFIER(t.grandeur == ecran::visuel::Metrique::VENT);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "Vent"));
    VERIFIER(egal(t.valeur, "14"));
    VERIFIER(egal(t.unite, "km/h"));
    VERIFIER(t.fleche_deg == 45); // d'où il vient : 225° (sud-ouest), il souffle vers 45° (nord-est)
    VERIFIER(egal(t.ligne1, "SO"));
    VERIFIER(egal(t.ligne2, "Rafales 26"));
    return 0;
}

constexpr int le_vent_dans_les_autres_langues() {
    const Tuile en = tuile_vent(ordinaire(), Langue::En);
    VERIFIER(egal(en.libelle, "Wind"));
    VERIFIER(egal(en.ligne1, "SW"));
    VERIFIER(egal(en.ligne2, "Gusts 26"));
    const Tuile de = tuile_vent(ordinaire(), Langue::De);
    VERIFIER(egal(de.ligne2, "Böen 26"));
    const Tuile tr = tuile_vent(ordinaire(), Langue::Tr);
    VERIFIER(egal(tr.libelle, "Rüzgâr"));
    VERIFIER(egal(tr.ligne1, "GB"));
    return 0;
}

constexpr int le_vent_s_arrondit_a_l_entier_le_plus_proche_moitie_vers_le_haut() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.vent_dix_kmh = 144;
    VERIFIER(egal(tuile_vent(m, Langue::Fr).valeur, "14"));
    m.vent_dix_kmh = 145;
    VERIFIER(egal(tuile_vent(m, Langue::Fr).valeur, "15"));
    m.vent_dix_kmh = 1034; // 103,4 km/h
    VERIFIER(egal(tuile_vent(m, Langue::Fr).valeur, "103"));
    return 0;
}

constexpr int un_vent_nul_est_une_valeur_pas_un_manque() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.vent_dix_kmh = 0;
    m.rafales_dix_kmh = INCONNU_DIX;
    const Tuile t = tuile_vent(m, Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.valeur, "0"));
    VERIFIER(egal(t.unite, "km/h"));
    return 0;
}

constexpr int les_rafales_ne_sont_dites_que_cinq_km_h_au_dessus_du_vent() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.vent_dix_kmh = 200;
    m.rafales_dix_kmh = 250; // exactement 5 km/h de plus : pas encore
    VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne2, ""));
    m.rafales_dix_kmh = 251;
    VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne2, "Rafales 25"));
    m.rafales_dix_kmh = INCONNU_DIX;
    VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne2, ""));
    return 0;
}

constexpr int sans_vitesse_pas_de_rafales_ni_d_unite() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.vent_dix_kmh = INCONNU_DIX; // les rafales restent connues : elles ne se disent pas sans le vent
    const Tuile t = tuile_vent(m, Langue::Fr);
    VERIFIER(!t.connue);
    VERIFIER(egal(t.valeur, "—"));
    VERIFIER(egal(t.unite, ""));
    VERIFIER(egal(t.ligne2, ""));
    m.vent_dix_kmh = -30; // une vitesse négative n'existe pas : inconnue
    VERIFIER(egal(tuile_vent(m, Langue::Fr).valeur, "—"));
    return 0;
}

constexpr int la_direction_inconnue_cache_la_fleche_et_le_secteur() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.direction_deg = INCONNU;
    const Tuile t = tuile_vent(m, Langue::Fr);
    VERIFIER(t.fleche_deg == INCONNU);
    VERIFIER(egal(t.ligne1, ""));
    VERIFIER(egal(t.valeur, "14"));
    m.direction_deg = 361; // hors plage : pas une direction
    VERIFIER(tuile_vent(m, Langue::Fr).fleche_deg == INCONNU);
    return 0;
}

constexpr int les_huit_secteurs_et_la_fleche_aux_bornes() {
    ecran::visuel::VueMeteo m = ordinaire();
    constexpr const char *ATTENDU[8] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    for (int s = 0; s < 8; ++s) {
        m.direction_deg = s * 45;
        VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne1, ATTENDU[s]));
    }
    m.direction_deg = 360; // le nord aussi
    VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne1, "N"));
    VERIFIER(tuile_vent(m, Langue::Fr).fleche_deg == 180);
    m.direction_deg = 0;
    VERIFIER(tuile_vent(m, Langue::Fr).fleche_deg == 180); // d'où il vient : le nord ; il souffle vers le sud
    m.direction_deg = 337;                                 // 337° se range encore au nord-ouest, 338° au nord
    VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne1, "NO"));
    m.direction_deg = 338;
    VERIFIER(egal(tuile_vent(m, Langue::Fr).ligne1, "N"));
    return 0;
}

// ---- pluie ----

constexpr int la_pluie_ordinaire() {
    const Tuile t = tuile_pluie(ordinaire(), Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "Pluie"));
    VERIFIER(egal(t.valeur, "5"));
    VERIFIER(egal(t.unite, "%"));
    VERIFIER(egal(t.ligne1, "Sec 2 h"));
    return 0;
}

constexpr int la_pluie_proche_dit_ce_qu_il_en_est() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.pluie_dans_min = 0;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "Il pleut"));
    VERIFIER(egal(tuile_pluie(m, Langue::En).ligne1, "Raining"));
    m.pluie_dans_min = 25;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "dans 25 min"));
    VERIFIER(egal(tuile_pluie(m, Langue::De).ligne1, "in 25 Min."));
    VERIFIER(egal(tuile_pluie(m, Langue::Tr).ligne1, "25 dk sonra"));
    m.pluie_dans_min = -1;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "Sec 2 h"));
    return 0;
}

constexpr int sans_pluie_proche_le_cumul_de_l_heure_prend_la_place() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.pluie_dans_min = -2;
    m.pluie_dix_mm = 4;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "0,4 mm"));
    VERIFIER(egal(tuile_pluie(m, Langue::En).ligne1, "0.4 mm")); // seul l'anglais écrit un point
    m.pluie_dix_mm = 100;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "10 mm"));
    m.pluie_dix_mm = 125;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "12,5 mm"));
    m.pluie_dix_mm = 0; // « 0 mm » est un fait : il ne pleut pas
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, "0 mm"));
    m.pluie_dix_mm = INCONNU_DIX;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).ligne1, ""));
    return 0;
}

constexpr int une_probabilite_inconnue_n_est_pas_zero() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.proba_pluie_pct = INCONNU;
    const Tuile t = tuile_pluie(m, Langue::Fr);
    VERIFIER(!t.connue);
    VERIFIER(egal(t.valeur, "—"));
    VERIFIER(egal(t.unite, ""));
    m.proba_pluie_pct = 0; // zéro pour cent est une valeur
    const Tuile z = tuile_pluie(m, Langue::Fr);
    VERIFIER(z.connue);
    VERIFIER(egal(z.valeur, "0"));
    VERIFIER(egal(z.unite, "%"));
    m.proba_pluie_pct = 101;
    VERIFIER(!tuile_pluie(m, Langue::Fr).connue);
    m.proba_pluie_pct = 100;
    VERIFIER(egal(tuile_pluie(m, Langue::Fr).valeur, "100"));
    return 0;
}

// ---- humidité ----

constexpr int l_humidite_et_sa_jauge() {
    const Tuile t = tuile_humidite(ordinaire(), Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "Humidité"));
    VERIFIER(egal(t.valeur, "58"));
    VERIFIER(egal(t.unite, "%"));
    VERIFIER(t.jauge_pm == 580);
    ecran::visuel::VueMeteo m = ordinaire();
    m.humidite_pct = 0;
    VERIFIER(egal(tuile_humidite(m, Langue::Fr).valeur, "0"));
    VERIFIER(tuile_humidite(m, Langue::Fr).jauge_pm == 0);
    m.humidite_pct = 100;
    VERIFIER(tuile_humidite(m, Langue::Fr).jauge_pm == 1000);
    return 0;
}

constexpr int une_humidite_inconnue_ou_absurde_est_un_tiret_sans_jauge() {
    ecran::visuel::VueMeteo m = ordinaire();
    for (const int pct : {INCONNU, 101, -5}) {
        m.humidite_pct = pct;
        const Tuile t = tuile_humidite(m, Langue::Fr);
        VERIFIER(!t.connue);
        VERIFIER(egal(t.valeur, "—"));
        VERIFIER(egal(t.unite, ""));
        VERIFIER(t.jauge_pm == INCONNU);
    }
    return 0;
}

// ---- pression ----

constexpr int la_pression_et_sa_tendance() {
    const Tuile t = tuile_pression(ordinaire(), Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "Pression"));
    VERIFIER(egal(t.valeur, "1024"));
    VERIFIER(egal(t.unite, "hPa"));
    VERIFIER(t.tendance == ecran::visuel::TendancePression::HAUSSE);
    VERIFIER(egal(t.ligne1, "En hausse"));
    ecran::visuel::VueMeteo m = ordinaire();
    m.tendance_dix_hpa_3h = -12;
    VERIFIER(egal(tuile_pression(m, Langue::Fr).ligne1, "En baisse"));
    VERIFIER(egal(tuile_pression(m, Langue::En).ligne1, "Falling"));
    m.tendance_dix_hpa_3h = 9; // sous 1 hPa en trois heures : stable
    VERIFIER(egal(tuile_pression(m, Langue::Fr).ligne1, "Stable"));
    m.tendance_dix_hpa_3h = 10; // la borne : 1,0 hPa est déjà une hausse
    VERIFIER(egal(tuile_pression(m, Langue::Fr).ligne1, "En hausse"));
    m.tendance_dix_hpa_3h = -10;
    VERIFIER(egal(tuile_pression(m, Langue::Fr).ligne1, "En baisse"));
    return 0;
}

constexpr int la_pression_s_arrondit_comme_le_vent() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.pression_dix_hpa = 10244;
    VERIFIER(egal(tuile_pression(m, Langue::Fr).valeur, "1024"));
    m.pression_dix_hpa = 10245;
    VERIFIER(egal(tuile_pression(m, Langue::Fr).valeur, "1025"));
    m.pression_dix_hpa = 9787; // 978,7 : la tempête de l'ancien pire cas
    VERIFIER(egal(tuile_pression(m, Langue::Fr).valeur, "979"));
    return 0;
}

constexpr int la_tendance_est_inconnue_sans_mesure_et_sans_pression() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.tendance_dix_hpa_3h = INCONNU_DIX; // l'appareil n'a pas encore trois heures de mesures
    const Tuile t = tuile_pression(m, Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(t.tendance == ecran::visuel::TendancePression::INCONNUE);
    VERIFIER(egal(t.ligne1, ""));
    m = ordinaire(); // une tendance sans pression ne se dit pas
    m.pression_dix_hpa = INCONNU_DIX;
    const Tuile s = tuile_pression(m, Langue::Fr);
    VERIFIER(!s.connue);
    VERIFIER(egal(s.valeur, "—"));
    VERIFIER(egal(s.unite, ""));
    VERIFIER(egal(s.ligne1, ""));
    VERIFIER(s.tendance == ecran::visuel::TendancePression::INCONNUE);
    m.pression_dix_hpa = 0;
    VERIFIER(!tuile_pression(m, Langue::Fr).connue);
    return 0;
}

// ---- UV ----

constexpr int l_indice_uv_et_son_niveau() {
    const Tuile t = tuile_uv(ordinaire(), Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "UV"));
    VERIFIER(egal(t.valeur, "2"));
    VERIFIER(egal(t.unite, ""));
    VERIFIER(egal(t.ligne1, "Faible"));
    VERIFIER(t.segments == 1);
    VERIFIER(t.categorie_uv == ecran::visuel::CategorieUv::FAIBLE);
    return 0;
}

constexpr int les_cinq_niveaux_de_l_oms_aux_bornes() {
    ecran::visuel::VueMeteo m = ordinaire();
    struct Cas {
        int dix;
        const char *valeur;
        const char *mot;
        int segments;
    };
    constexpr Cas CAS[] = {{0, "0", "Faible", 1},      {29, "3", "Faible", 1},       {30, "3", "Modéré", 2},
                           {59, "6", "Modéré", 2},     {60, "6", "Élevé", 3},        {79, "8", "Élevé", 3},
                           {80, "8", "Très élevé", 4}, {109, "11", "Très élevé", 4}, {110, "11", "Extrême", 5},
                           {1234, "123", "Extrême", 5}};
    for (const Cas &c : CAS) {
        m.uv_dix = c.dix;
        const Tuile t = tuile_uv(m, Langue::Fr);
        VERIFIER(egal(t.valeur, c.valeur));
        VERIFIER(egal(t.ligne1, c.mot));
        VERIFIER(t.segments == c.segments);
    }
    m.uv_dix = 80;
    VERIFIER(egal(tuile_uv(m, Langue::En).ligne1, "Very high"));
    VERIFIER(egal(tuile_uv(m, Langue::Tr).ligne1, "Çok yüksek"));
    return 0;
}

constexpr int un_uv_inconnu_ou_negatif_est_un_tiret_sans_segment() {
    ecran::visuel::VueMeteo m = ordinaire();
    for (const int dix : {INCONNU_DIX, -5}) {
        m.uv_dix = dix;
        const Tuile t = tuile_uv(m, Langue::Fr);
        VERIFIER(!t.connue);
        VERIFIER(egal(t.valeur, "—"));
        VERIFIER(egal(t.ligne1, ""));
        VERIFIER(t.segments == 0);
        VERIFIER(t.categorie_uv == ecran::visuel::CategorieUv::INCONNU);
    }
    return 0;
}

// ---- soleil ----

constexpr int le_soleil_ordinaire() {
    const Tuile t = tuile_soleil(ordinaire(), MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "Soleil"));
    VERIFIER(egal(t.valeur, "07:12"));
    VERIFIER(egal(t.valeur2, "17:10"));
    VERIFIER(egal(t.ligne1, "Lever"));
    VERIFIER(egal(t.ligne2, "Coucher"));
    VERIFIER(t.position_pm == 481); // 4 h 48 sur 9 h 58 : 17 280 / 35 880
    const Tuile en = tuile_soleil(ordinaire(), MIDI, CET_FIXE, Langue::En);
    VERIFIER(egal(en.ligne1, "Sunrise"));
    VERIFIER(egal(en.ligne2, "Sunset"));
    return 0;
}

constexpr int le_soleil_hors_de_son_arc_n_a_pas_de_position() {
    VERIFIER(tuile_soleil(ordinaire(), LEVER - 1, CET_FIXE, Langue::Fr).position_pm == INCONNU); // avant le lever
    VERIFIER(tuile_soleil(ordinaire(), LEVER, CET_FIXE, Langue::Fr).position_pm == 0);
    VERIFIER(tuile_soleil(ordinaire(), COUCHER, CET_FIXE, Langue::Fr).position_pm == 1000);
    VERIFIER(tuile_soleil(ordinaire(), COUCHER + 1, CET_FIXE, Langue::Fr).position_pm == INCONNU); // après le coucher
    const Tuile nuit = tuile_soleil(ordinaire(), COUCHER + 3600, CET_FIXE, Langue::Fr);
    VERIFIER(nuit.connue); // les heures du jour restent dites la nuit
    VERIFIER(egal(nuit.valeur, "07:12"));
    return 0;
}

constexpr int sans_heure_sure_les_heures_du_soleil_restent_mais_pas_sa_position() {
    const Tuile t = tuile_soleil(ordinaire(), 0, CET_FIXE, Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.valeur, "07:12"));
    VERIFIER(egal(t.valeur2, "17:10"));
    VERIFIER(t.position_pm == INCONNU);
    return 0;
}

constexpr int un_soleil_absent_ou_incoherent_est_deux_tirets() {
    ecran::visuel::VueMeteo m = ordinaire();
    const auto verifier = [](const ecran::visuel::VueMeteo &v) {
        const Tuile t = tuile_soleil(v, MIDI, CET_FIXE, Langue::Fr);
        VERIFIER(!t.connue);
        VERIFIER(egal(t.valeur, "—"));
        VERIFIER(egal(t.valeur2, "—"));
        VERIFIER(egal(t.ligne1, ""));
        VERIFIER(egal(t.ligne2, ""));
        VERIFIER(t.position_pm == INCONNU);
        return 0;
    };
    m.lever_s = 0; // nuit polaire ou donnée absente
    m.coucher_s = 0;
    if (const int ligne = verifier(m))
        return ligne;
    m = ordinaire();
    m.coucher_s = m.lever_s; // égaux : jour ou nuit polaire
    if (const int ligne = verifier(m))
        return ligne;
    m = ordinaire();
    m.coucher_s = m.lever_s - 60; // dans le mauvais ordre
    return verifier(m);
}

constexpr int les_heures_d_hier_ne_sont_pas_celles_d_aujourd_hui() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.lever_s = LEVER - 86400;
    m.coucher_s = COUCHER - 86400;
    const Tuile t = tuile_soleil(m, MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(!t.connue);
    VERIFIER(egal(t.valeur, "—"));
    VERIFIER(egal(t.valeur2, "—"));
    // à 00:30 locale le lendemain, le jour local a changé : les heures de la veille ne servent plus
    VERIFIER(!tuile_soleil(ordinaire(), MINUIT_LOCAL + 86400 + 1800, CET_FIXE, Langue::Fr).connue);
    // 23:59 locale : le même jour local encore
    VERIFIER(tuile_soleil(ordinaire(), MINUIT_LOCAL + 86400 - 60, CET_FIXE, Langue::Fr).connue);
    return 0;
}

constexpr int le_soleil_se_lit_a_l_heure_du_lieu_pas_a_celle_de_l_utc() {
    // Kolkata, UTC+5:30 : le lever de 06:15 locale est à 00:45 UTC ; les deux instants tombent le même jour local.
    const ecran::heure::Fuseau kolkata = ecran::heure::fuseau_fixe(5 * 3600 + 1800);
    const std::int64_t minuit = MINUIT_UTC - (5 * 3600 + 1800);
    ecran::visuel::VueMeteo m = ordinaire();
    m.lever_s = minuit + 6 * 3600 + 15 * 60;
    m.coucher_s = minuit + 17 * 3600 + 45 * 60;
    const Tuile t = tuile_soleil(m, minuit + 12 * 3600, kolkata, Langue::Fr);
    VERIFIER(egal(t.valeur, "06:15"));
    VERIFIER(egal(t.valeur2, "17:45"));
    return 0;
}

// ---- lune ----

constexpr int la_lune_a_la_nouvelle_lune_de_reference() {
    const Tuile t = tuile_lune(ecran::visuel::LUNE_NOUVELLE_REFERENCE_S, Langue::Fr);
    VERIFIER(t.connue);
    VERIFIER(egal(t.libelle, "Lune"));
    VERIFIER(t.phase == 0);
    VERIFIER(egal(t.valeur, "0"));
    VERIFIER(egal(t.unite, "%"));
    VERIFIER(egal(t.ligne1, "Nouvelle lune"));
    return 0;
}

constexpr int la_pleine_lune_est_a_la_moitie_du_cycle() {
    const std::int64_t pleine = ecran::visuel::LUNE_NOUVELLE_REFERENCE_S + ecran::visuel::LUNE_MOIS_SYNODIQUE_S / 2;
    const Tuile t = tuile_lune(pleine, Langue::Fr);
    VERIFIER(t.phase == 4);
    VERIFIER(egal(t.valeur, "100"));
    VERIFIER(egal(t.ligne1, "Pleine lune"));
    VERIFIER(egal(tuile_lune(pleine, Langue::En).ligne1, "Full moon"));
    VERIFIER(egal(tuile_lune(pleine, Langue::De).ligne1, "Vollmond"));
    // un quart de cycle : premier quartier (indice 2), à moitié éclairée
    const std::int64_t quartier = ecran::visuel::LUNE_NOUVELLE_REFERENCE_S + ecran::visuel::LUNE_MOIS_SYNODIQUE_S / 4;
    const Tuile q = tuile_lune(quartier, Langue::Fr);
    VERIFIER(q.phase == 2);
    VERIFIER(egal(q.ligne1, "Premier quartier"));
    return 0;
}

constexpr int les_huit_phases_ont_un_nom_chacune() {
    for (int i = 0; i < ecran::visuel::LUNE_NB_PHASES; ++i) {
        const std::int64_t instant =
            ecran::visuel::LUNE_NOUVELLE_REFERENCE_S + i * ecran::visuel::LUNE_MOIS_SYNODIQUE_S / 8;
        const Tuile t = tuile_lune(instant, Langue::Fr);
        VERIFIER(t.phase == i);
        VERIFIER(t.ligne1[0] != '\0');
        for (int j = 0; j < i; ++j) {
            VERIFIER(!egal(t.ligne1, tuile_lune(ecran::visuel::LUNE_NOUVELLE_REFERENCE_S +
                                                    j * ecran::visuel::LUNE_MOIS_SYNODIQUE_S / 8,
                                                Langue::Fr)
                                         .ligne1));
        }
    }
    return 0;
}

constexpr int sans_heure_sure_la_lune_est_un_tiret_et_n_a_pas_d_image() {
    const Tuile t = tuile_lune(0, Langue::Fr);
    VERIFIER(!t.connue);
    VERIFIER(egal(t.valeur, "—"));
    VERIFIER(egal(t.unite, ""));
    VERIFIER(egal(t.ligne1, ""));
    VERIFIER(t.phase == INCONNU);
    VERIFIER(!tuile_lune(-5, Langue::Fr).connue);
    return 0;
}

// ---- la page ----

constexpr int la_page_ordinaire_a_sept_tuiles_dans_l_ordre_des_metriques() {
    const Page p = composer(ordinaire(), MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(NB_TUILES == 7);
    VERIFIER(egal(p.titre, "Détails"));
    VERIFIER(egal(p.fermer, "Fermer"));
    VERIFIER(egal(p.mention, ""));
    VERIFIER(egal(p.exemple, ""));
    VERIFIER(p.meteo_connue);
    VERIFIER(!p.perimee);
    for (int i = 0; i < NB_TUILES; ++i) {
        VERIFIER(static_cast<int>(p.tuile[i].grandeur) == i);
        VERIFIER(p.tuile[i].libelle[0] != '\0');
        VERIFIER(p.tuile[i].connue);
        VERIFIER(!p.tuile[i].grisee);
    }
    VERIFIER(egal(p.tuile[0].libelle, "Vent"));
    VERIFIER(egal(p.tuile[1].libelle, "Pluie"));
    VERIFIER(egal(p.tuile[2].libelle, "Humidité"));
    VERIFIER(egal(p.tuile[3].libelle, "Pression"));
    VERIFIER(egal(p.tuile[4].libelle, "UV"));
    VERIFIER(egal(p.tuile[5].libelle, "Soleil"));
    VERIFIER(egal(p.tuile[6].libelle, "Lune"));
    return 0;
}

constexpr int la_ville_et_la_mention_exemple_suivent_la_vue() {
    ecran::visuel::VueMeteo m = ordinaire();
    const char *ville = "Rennes";
    for (std::size_t i = 0; ville[i] != '\0'; ++i)
        m.ville[i] = ville[i];
    m.exemple = true;
    const Page p = composer(m, MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(egal(p.ville, "Rennes"));
    VERIFIER(egal(p.exemple, "Exemple"));
    VERIFIER(egal(composer(m, MIDI, CET_FIXE, Langue::De).exemple, "Beispiel"));
    m.connue = false; // sans météo il n'y a rien à marquer « Exemple »
    VERIFIER(egal(composer(m, MIDI, CET_FIXE, Langue::Fr).exemple, ""));
    return 0;
}

constexpr int une_meteo_perimee_garde_ses_valeurs_grisees_et_dit_l_heure_du_releve() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.perimee = true;
    m.instant_s = MINUIT_LOCAL + 7 * 3600 + 52 * 60; // le relevé date de 07:52
    const Page p = composer(m, MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(p.perimee);
    VERIFIER(egal(p.mention, "Données de 07:52 · périmées"));
    VERIFIER(egal(p.tuile[0].valeur,
                  "14")); // la dernière valeur reste lisible : comme l'accueil, elle est grisée, pas effacée
    VERIFIER(egal(p.tuile[3].valeur, "1024"));
    for (int i = 0; i < NB_TUILES; ++i) {
        const bool lune = i == static_cast<int>(ecran::visuel::Metrique::LUNE);
        VERIFIER(p.tuile[i].grisee == !lune); // la lune se calcule sur l'appareil : jamais périmée
    }
    VERIFIER(egal(composer(m, MIDI, CET_FIXE, Langue::En).mention, "Data from 07:52 · outdated"));
    VERIFIER(egal(composer(m, MIDI, CET_FIXE, Langue::De).mention, "Daten von 07:52 · veraltet"));
    VERIFIER(egal(composer(m, MIDI, CET_FIXE, Langue::Tr).mention, "07:52 verileri · eski"));
    m.instant_s = 0; // l'heure du relevé est perdue : la mention le dit sans l'heure
    VERIFIER(egal(composer(m, MIDI, CET_FIXE, Langue::Fr).mention, "Données périmées"));
    return 0;
}

constexpr int sans_meteo_toutes_les_grandeurs_meteo_sont_des_tirets_et_la_lune_reste() {
    const Page p = composer(ecran::visuel::VueMeteo{}, MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(!p.meteo_connue);
    VERIFIER(!p.perimee);
    VERIFIER(egal(p.mention, "Météo indisponible"));
    for (int i = 0; i < NB_TUILES - 1; ++i) {
        const Tuile &t = p.tuile[i];
        VERIFIER(!t.connue);
        VERIFIER(egal(t.valeur, "—"));
        VERIFIER(egal(t.valeur2, i == 5 ? "—" : ""));
        VERIFIER(egal(t.unite, ""));
        VERIFIER(egal(t.ligne2, ""));
        VERIFIER(!t.grisee);
        VERIFIER(t.fleche_deg == INCONNU);
        VERIFIER(t.jauge_pm == INCONNU);
        VERIFIER(t.segments == 0);
        VERIFIER(t.position_pm == INCONNU);
        VERIFIER(t.phase == INCONNU);
    }
    // aucune ligne de détail sous un tiret : « Sec 2 h » sous « — » dirait ce que l'on ne sait pas
    VERIFIER(egal(p.tuile[1].ligne1, ""));
    VERIFIER(p.tuile[6].connue); // la lune ne vient pas de la météo
    VERIFIER(egal(composer(ecran::visuel::VueMeteo{}, MIDI, CET_FIXE, Langue::En).mention, "Weather unavailable"));
    // Un drapeau « périmée » sans météo n'a pas de sens : pas de grisé, pas d'heure de relevé.
    ecran::visuel::VueMeteo vieille{};
    vieille.perimee = true;
    const Page v = composer(vieille, MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(!v.perimee);
    VERIFIER(egal(v.mention, "Météo indisponible"));
    VERIFIER(!v.tuile[0].grisee);
    return 0;
}

// Aucun chiffre n'apparaît dans une tuile dont la grandeur est inconnue : ni 0 ni valeur de repli.
constexpr bool a_un_chiffre(const char *s) {
    for (; *s != '\0'; ++s) {
        if (*s >= '0' && *s <= '9')
            return true;
    }
    return false;
}

constexpr int un_manque_n_ecrit_jamais_un_chiffre() {
    for (int l = 0; l < 7; ++l) {
        const Page p = composer(ecran::visuel::VueMeteo{}, 0, CET_FIXE, static_cast<Langue>(l));
        for (const Tuile &t : p.tuile) {
            VERIFIER(!a_un_chiffre(t.valeur));
            VERIFIER(!a_un_chiffre(t.valeur2));
            VERIFIER(!a_un_chiffre(t.unite));
            VERIFIER(!a_un_chiffre(t.ligne1));
            VERIFIER(!a_un_chiffre(t.ligne2));
            VERIFIER(!t.connue);
        }
        VERIFIER(!a_un_chiffre(p.mention));
    }
    return 0;
}

// Le texte le plus long de chaque langue doit tenir dans les tampons de la page : un texte trop long est effacé, jamais coupé.
constexpr int les_pires_textes_tiennent_dans_chaque_langue() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.vent_dix_kmh = 1034;
    m.rafales_dix_kmh = 1387;
    m.direction_deg = 315;
    m.pluie_dans_min = 118;
    m.pression_dix_hpa = 9787;
    m.tendance_dix_hpa_3h = -61;
    m.uv_dix = 1234;
    m.perimee = true;
    m.exemple = true;
    m.instant_s = MIDI - 3600;
    const std::int64_t pire_lune = ecran::visuel::LUNE_NOUVELLE_REFERENCE_S +
                                   5 * ecran::visuel::LUNE_MOIS_SYNODIQUE_S / 8; // gibbeuse décroissante
    for (int l = 0; l < 7; ++l) {
        const Langue langue = static_cast<Langue>(l);
        const Page p = composer(m, MIDI, CET_FIXE, langue);
        VERIFIER(p.titre[0] != '\0');
        VERIFIER(p.fermer[0] != '\0');
        VERIFIER(p.mention[0] != '\0');
        VERIFIER(p.exemple[0] != '\0');
        for (const Tuile &t : p.tuile) {
            VERIFIER(t.libelle[0] != '\0');
            VERIFIER(t.valeur[0] != '\0');
        }
        VERIFIER(p.tuile[0].ligne1[0] != '\0'); // secteur
        VERIFIER(p.tuile[0].ligne2[0] != '\0'); // rafales
        VERIFIER(p.tuile[1].ligne1[0] != '\0'); // dans 118 min
        VERIFIER(p.tuile[3].ligne1[0] != '\0'); // tendance
        VERIFIER(p.tuile[4].ligne1[0] != '\0'); // « Très élevé » / « Extrême »
        VERIFIER(p.tuile[5].ligne1[0] != '\0' && p.tuile[5].ligne2[0] != '\0');
        VERIFIER(tuile_lune(pire_lune, langue).ligne1[0] != '\0');
    }
    return 0;
}

constexpr int deux_pages_identiques_ont_la_meme_empreinte_et_tout_changement_la_change() {
    const Page base = composer(ordinaire(), MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(empreinte(base) == empreinte(composer(ordinaire(), MIDI, CET_FIXE, Langue::Fr)));
    ecran::visuel::VueMeteo m = ordinaire();
    m.vent_dix_kmh = 150;
    VERIFIER(empreinte(composer(m, MIDI, CET_FIXE, Langue::Fr)) != empreinte(base)); // une valeur
    m = ordinaire();
    m.direction_deg = 230; // même secteur (SO), même texte : seule la flèche change de cinq degrés
    VERIFIER(empreinte(composer(m, MIDI, CET_FIXE, Langue::Fr)) != empreinte(base)); // la flèche
    m = ordinaire();
    m.ville[0] = 'R'; // la ville est dans l'en-tête
    VERIFIER(empreinte(composer(m, MIDI, CET_FIXE, Langue::Fr)) != empreinte(base));
    m = ordinaire();
    m.perimee = true;
    VERIFIER(empreinte(composer(m, MIDI, CET_FIXE, Langue::Fr)) != empreinte(base)); // le grisé
    VERIFIER(empreinte(composer(ordinaire(), MIDI + 3600, CET_FIXE, Langue::Fr)) !=
             empreinte(base));                                                                 // la place du soleil
    VERIFIER(empreinte(composer(ordinaire(), MIDI, CET_FIXE, Langue::En)) != empreinte(base)); // la langue
    // La nuit, une minute de plus ne change rien de ce qui se voit : l'heure elle-même n'entre pas dans l'empreinte.
    VERIFIER(empreinte(composer(ordinaire(), COUCHER + 3600, CET_FIXE, Langue::Fr)) ==
             empreinte(composer(ordinaire(), COUCHER + 3660, CET_FIXE, Langue::Fr)));
    m = ordinaire();
    m.code = 61; // seul le ciel du fond change (jour, mais pluvieux) : la page se redessine aussi
    VERIFIER(empreinte(composer(m, MIDI, CET_FIXE, Langue::Fr)) != empreinte(base));
    m = ordinaire();
    m.humidite_pct = 59;
    VERIFIER(empreinte(composer(m, MIDI, CET_FIXE, Langue::Fr)) != empreinte(base)); // la jauge (le texte change aussi)
    return 0;
}

// ---- le ciel ----

constexpr int le_ciel_suit_le_soleil_et_le_temps_comme_l_accueil() {
    ecran::visuel::VueMeteo m = ordinaire();
    m.code = 0; // ciel dégagé
    VERIFIER(ciel_de(m, MIDI).moment == ecran::visuel::MomentJour::JOUR);
    VERIFIER(ciel_de(m, MIDI).ambiance == ecran::visuel::Ambiance::CLAIR);
    VERIFIER(ciel_de(m, LEVER).moment == ecran::visuel::MomentJour::AUBE);
    VERIFIER(ciel_de(m, COUCHER).moment == ecran::visuel::MomentJour::CREPUSCULE);
    VERIFIER(ciel_de(m, COUCHER + 3600).moment == ecran::visuel::MomentJour::NUIT);
    VERIFIER(ciel_de(m, LEVER - 3600).moment == ecran::visuel::MomentJour::NUIT);
    m.code = 61; // pluie
    VERIFIER(ciel_de(m, MIDI).ambiance == ecran::visuel::Ambiance::PLUIE);
    VERIFIER(composer(m, MIDI, CET_FIXE, Langue::Fr).ciel.ambiance ==
             ecran::visuel::Ambiance::PLUIE); // la page porte ce ciel
    VERIFIER(composer(m, MIDI, CET_FIXE, Langue::Fr).ciel.moment == ecran::visuel::MomentJour::JOUR);
    m.code = 95; // orage
    VERIFIER(ciel_de(m, MIDI).ambiance == ecran::visuel::Ambiance::ORAGE);
    // sans lever ni coucher, le drapeau « jour » de la source décide
    m.lever_s = 0;
    m.coucher_s = 0;
    m.jour = true;
    VERIFIER(ciel_de(m, MIDI).moment == ecran::visuel::MomentJour::JOUR);
    m.jour = false;
    VERIFIER(ciel_de(m, MIDI).moment == ecran::visuel::MomentJour::NUIT);
    return 0;
}

constexpr int sans_meteo_le_ciel_est_une_nuit_couverte() {
    const Ciel c = ciel_de(ecran::visuel::VueMeteo{}, MIDI);
    VERIFIER(c.moment == ecran::visuel::MomentJour::NUIT);
    VERIFIER(c.ambiance == ecran::visuel::Ambiance::COUVERT);
    const Page p = composer(ecran::visuel::VueMeteo{}, MIDI, CET_FIXE, Langue::Fr);
    VERIFIER(p.ciel.moment == c.moment && p.ciel.ambiance == c.ambiance);
    // une vue vide mais « jour » et « code 0 » ne donne pas un ciel clair : sans météo connue on ne lit rien d'elle
    ecran::visuel::VueMeteo v{};
    v.jour = true;
    v.code = 0;
    v.lever_s = LEVER;
    v.coucher_s = COUCHER;
    VERIFIER(ciel_de(v, MIDI).moment == ecran::visuel::MomentJour::NUIT);
    VERIFIER(ciel_de(v, MIDI).ambiance == ecran::visuel::Ambiance::COUVERT);
    return 0;
}

// ---- mise en page ----

constexpr bool dedans(const Rect &r, int w, int h) {
    return r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 && r.x + r.w <= w && r.y + r.h <= h;
}

constexpr bool se_recouvrent(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

constexpr int toute_mise_en_page_tient_et_ne_se_recouvre_pas() {
    struct Taille {
        int w, h;
        bool portrait;
    };
    // 1280 x 720 (Tab5), 1280 x 750 (la 7B de 1024 x 600 ramenée à l'échelle 0,8), 720 x 1280 (portrait), 1280 x 800 (un écran plus haut)
    constexpr Taille TAILLES[] = {{1280, 720, false}, {1280, 750, false}, {720, 1280, true}, {1280, 800, false}};
    for (const Taille &t : TAILLES) {
        const Zones z = disposer(t.w, t.h, t.portrait);
        VERIFIER(dedans(z.barre, t.w, t.h));
        VERIFIER(dedans(z.fermer, t.w, t.h));
        VERIFIER(!se_recouvrent(z.barre, z.fermer));
        for (int i = 0; i < NB_TUILES; ++i) {
            VERIFIER(dedans(z.tuile[i], t.w, t.h));
            VERIFIER(!se_recouvrent(z.tuile[i], z.barre));
            VERIFIER(!se_recouvrent(z.tuile[i], z.fermer));
            for (int j = i + 1; j < NB_TUILES; ++j) {
                VERIFIER(!se_recouvrent(z.tuile[i], z.tuile[j]));
            }
        }
    }
    return 0;
}

constexpr int le_paysage_a_deux_rangees_alignees_sur_les_bords() {
    const Zones z = disposer(1280, 720, false);
    constexpr int V = 0, P = 1, H = 2, R = 3, U = 4, S = 5, L = 6;
    VERIFIER(z.tuile[V].x == 32 && z.tuile[V].y == GRILLE_DEPUIS);
    VERIFIER(z.tuile[V].w == 292 && z.tuile[V].h == 274);
    VERIFIER(z.tuile[P].x == 340 && z.tuile[H].x == 648);
    VERIFIER(z.tuile[R].x + z.tuile[R].w == 1280 - 32); // la dernière touche la marge droite
    VERIFIER(z.tuile[U].x == z.tuile[V].x && z.tuile[U].y == z.tuile[V].y + z.tuile[V].h + 16);
    VERIFIER(z.tuile[S].x == z.tuile[P].x && z.tuile[S].w == 600); // le soleil prend deux colonnes
    VERIFIER(z.tuile[L].x == z.tuile[R].x && z.tuile[L].w == z.tuile[R].w);
    VERIFIER(z.tuile[S].x + z.tuile[S].w + 16 == z.tuile[L].x);
    VERIFIER(z.tuile[U].y + z.tuile[U].h == 720 - 32); // la rangée du bas touche la marge basse
    VERIFIER(z.fermer.x + z.fermer.w == 1280 - 32);
    return 0;
}

constexpr int la_7b_donne_ses_pixels_en_plus_a_la_hauteur_des_tuiles() {
    const Zones z = disposer(1280, 750, false);
    VERIFIER(z.extra_h == 30);
    VERIFIER(z.tuile[0].h > 274);
    VERIFIER(z.tuile[4].y + z.tuile[4].h == 750 - 32);
    return 0;
}

constexpr int le_portrait_a_deux_colonnes_et_le_soleil_sur_toute_la_largeur() {
    const Zones z = disposer(720, 1280, true);
    constexpr int V = 0, P = 1, H = 2, R = 3, U = 4, S = 5, L = 6;
    VERIFIER(z.tuile[V].x == 28 && z.tuile[P].x > z.tuile[V].x + z.tuile[V].w);
    VERIFIER(z.tuile[H].y == z.tuile[V].y + z.tuile[V].h + 16);
    VERIFIER(z.tuile[H].x == z.tuile[V].x && z.tuile[R].x == z.tuile[P].x);
    VERIFIER(z.tuile[U].x == z.tuile[V].x && z.tuile[L].x == z.tuile[P].x);
    VERIFIER(z.tuile[S].x == 28 && z.tuile[S].x + z.tuile[S].w == 720 - 28);
    VERIFIER(z.tuile[S].y + z.tuile[S].h == 1280 - 28);
    VERIFIER(z.tuile[P].x + z.tuile[P].w == 720 - 28);
    return 0;
}

constexpr int chaque_tuile_a_la_place_de_son_contenu() {
    // Le contenu le plus haut d'une tuile : marge, ligne de l'icône, valeur, deux lignes de détail, marge. Les lignes de police
    // mesurent 1,2 fois leur taille (Roboto : 1,17), c'est une borne large ; le rendu le montre pour de vrai.
    constexpr int ligne_valeur = POLICE_VALEUR * 6 / 5;
    constexpr int ligne_texte = POLICE_TEXTE * 6 / 5;
    constexpr int besoin =
        TUILE_MARGE + TUILE_ICONE + TUILE_ESPACE + ligne_valeur + TUILE_ESPACE + 2 * ligne_texte + TUILE_MARGE;
    for (const bool portrait : {false, true}) {
        const Zones z = portrait ? disposer(720, 1280, true) : disposer(1280, 720, false);
        for (const Rect &r : z.tuile) {
            VERIFIER(r.h >= besoin);
            VERIFIER(r.w >= 260);
        }
    }
    return 0;
}

constexpr int aucun_texte_n_est_plus_petit_que_vingt_quatre_pixels_reels() {
    VERIFIER(TEXTE_MIN == 24);
    VERIFIER(ECHELLE_MIN_PM == 800); // la 7B : 1024 / 1280
    // en pixels RÉELS sur la plus petite carte
    VERIFIER(POLICE_VALEUR * ECHELLE_MIN_PM / 1000 >= TEXTE_MIN);
    VERIFIER(POLICE_TITRE * ECHELLE_MIN_PM / 1000 >= TEXTE_MIN);
    VERIFIER(POLICE_TEXTE * ECHELLE_MIN_PM / 1000 >= TEXTE_MIN);
    VERIFIER(POLICE_VALEUR > POLICE_TITRE && POLICE_TITRE > POLICE_TEXTE); // la valeur se lit avant tout le reste
    return 0;
}

constexpr int le_bouton_fermer_est_assez_grand_pour_un_doigt() {
    const Zones z = disposer(1280, 750, false);
    // à l'échelle 0,8 de la 7B : au moins 64 pixels réels de haut et 140 de large
    VERIFIER(z.fermer.h * 8 / 10 >= 64);
    VERIFIER(z.fermer.w * 8 / 10 >= 140);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_deux_phrases_de_la_page_ont_leurs_six_traductions)                                                           \
    X(les_phrases_de_la_page_dans_chaque_langue)                                                                       \
    X(les_mots_des_grandeurs_viennent_de_la_table_de_l_ecran_principal)                                                \
    X(le_vent_ordinaire)                                                                                               \
    X(le_vent_dans_les_autres_langues)                                                                                 \
    X(le_vent_s_arrondit_a_l_entier_le_plus_proche_moitie_vers_le_haut)                                                \
    X(un_vent_nul_est_une_valeur_pas_un_manque)                                                                        \
    X(les_rafales_ne_sont_dites_que_cinq_km_h_au_dessus_du_vent)                                                       \
    X(sans_vitesse_pas_de_rafales_ni_d_unite)                                                                          \
    X(la_direction_inconnue_cache_la_fleche_et_le_secteur)                                                             \
    X(les_huit_secteurs_et_la_fleche_aux_bornes)                                                                       \
    X(la_pluie_ordinaire)                                                                                              \
    X(la_pluie_proche_dit_ce_qu_il_en_est)                                                                             \
    X(sans_pluie_proche_le_cumul_de_l_heure_prend_la_place)                                                            \
    X(une_probabilite_inconnue_n_est_pas_zero)                                                                         \
    X(l_humidite_et_sa_jauge)                                                                                          \
    X(une_humidite_inconnue_ou_absurde_est_un_tiret_sans_jauge)                                                        \
    X(la_pression_et_sa_tendance)                                                                                      \
    X(la_pression_s_arrondit_comme_le_vent)                                                                            \
    X(la_tendance_est_inconnue_sans_mesure_et_sans_pression)                                                           \
    X(l_indice_uv_et_son_niveau)                                                                                       \
    X(les_cinq_niveaux_de_l_oms_aux_bornes)                                                                            \
    X(un_uv_inconnu_ou_negatif_est_un_tiret_sans_segment)                                                              \
    X(le_soleil_ordinaire)                                                                                             \
    X(le_soleil_hors_de_son_arc_n_a_pas_de_position)                                                                   \
    X(sans_heure_sure_les_heures_du_soleil_restent_mais_pas_sa_position)                                               \
    X(un_soleil_absent_ou_incoherent_est_deux_tirets)                                                                  \
    X(les_heures_d_hier_ne_sont_pas_celles_d_aujourd_hui)                                                              \
    X(le_soleil_se_lit_a_l_heure_du_lieu_pas_a_celle_de_l_utc)                                                         \
    X(la_lune_a_la_nouvelle_lune_de_reference)                                                                         \
    X(la_pleine_lune_est_a_la_moitie_du_cycle)                                                                         \
    X(les_huit_phases_ont_un_nom_chacune)                                                                              \
    X(sans_heure_sure_la_lune_est_un_tiret_et_n_a_pas_d_image)                                                         \
    X(la_page_ordinaire_a_sept_tuiles_dans_l_ordre_des_metriques)                                                      \
    X(la_ville_et_la_mention_exemple_suivent_la_vue)                                                                   \
    X(une_meteo_perimee_garde_ses_valeurs_grisees_et_dit_l_heure_du_releve)                                            \
    X(sans_meteo_toutes_les_grandeurs_meteo_sont_des_tirets_et_la_lune_reste)                                          \
    X(un_manque_n_ecrit_jamais_un_chiffre)                                                                             \
    X(les_pires_textes_tiennent_dans_chaque_langue)                                                                    \
    X(deux_pages_identiques_ont_la_meme_empreinte_et_tout_changement_la_change)                                        \
    X(le_ciel_suit_le_soleil_et_le_temps_comme_l_accueil)                                                              \
    X(sans_meteo_le_ciel_est_une_nuit_couverte)                                                                        \
    X(toute_mise_en_page_tient_et_ne_se_recouvre_pas)                                                                  \
    X(le_paysage_a_deux_rangees_alignees_sur_les_bords)                                                                \
    X(la_7b_donne_ses_pixels_en_plus_a_la_hauteur_des_tuiles)                                                          \
    X(le_portrait_a_deux_colonnes_et_le_soleil_sur_toute_la_largeur)                                                   \
    X(chaque_tuile_a_la_place_de_son_contenu)                                                                          \
    X(aucun_texte_n_est_plus_petit_que_vingt_quatre_pixels_reels)                                                      \
    X(le_bouton_fermer_est_assez_grand_pour_un_doigt)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
