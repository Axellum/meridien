// Tests du noyau pur « météo riche » (meteo_riche.hpp) : lecture de la réponse d'Open-Meteo (courant complet, 24 heures, compléments
// des 7 jours, quarts d'heure de pluie), contrôle, indices de l'heure en cours, pluie des prochaines minutes et cache binaire v2.
//
// Les valeurs viennent de VRAIES réponses capturées le 07/11/2026 à 00 h 12 (heure de Paris) avec l'adresse exacte du firmware
// (donnees/openmeteo_riche_*.inc) ; les valeurs attendues sont calculées par Python, sans reprendre une ligne du noyau
// (donnees/attendu_riche_*.inc, test/host/controle_python_riche.py). Les cas limites sont écrits à la main, ou obtenus en
// cassant UNE chose dans une vraie réponse (`remplacer`).
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_cache.hpp"
#include "ecran/meteo_openmeteo.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_riche.hpp"
#include "mini.hpp"

using namespace ecran::meteo;
using sv = std::string_view;

namespace {

constexpr sv LYON =
#include "donnees/openmeteo_riche_lyon.inc"
    ;
constexpr sv LONGYEARBYEN =
#include "donnees/openmeteo_riche_longyearbyen.inc"
    ;
constexpr sv SEATTLE =
#include "donnees/openmeteo_riche_seattle.inc"
    ;
constexpr sv SINGAPOUR =
#include "donnees/openmeteo_riche_singapour.inc"
    ;
constexpr sv SYDNEY =
#include "donnees/openmeteo_riche_sydney.inc"
    ;
constexpr sv KATMANDOU =
#include "donnees/openmeteo_riche_katmandou.inc"
    ;
constexpr sv TERRENEUVE =
#include "donnees/openmeteo_riche_terreneuve.inc"
    ;

constexpr long long ATT_LYON[] = {
#include "donnees/attendu_riche_lyon.inc"
};
constexpr long long ATT_LONGYEARBYEN[] = {
#include "donnees/attendu_riche_longyearbyen.inc"
};
constexpr long long ATT_SEATTLE[] = {
#include "donnees/attendu_riche_seattle.inc"
};
constexpr long long ATT_SINGAPOUR[] = {
#include "donnees/attendu_riche_singapour.inc"
};
constexpr long long ATT_SYDNEY[] = {
#include "donnees/attendu_riche_sydney.inc"
};
constexpr long long ATT_KATMANDOU[] = {
#include "donnees/attendu_riche_katmandou.inc"
};
constexpr long long ATT_TERRENEUVE[] = {
#include "donnees/attendu_riche_terreneuve.inc"
};

// Toutes les captures ont le même `current.time` (le même quart d'heure) ; l'appareil les « reçoit » 100 s plus tard.
constexpr std::int64_t T0 = 1794006000;
constexpr std::int64_t RECU = T0 + 100;

constexpr AnalyseRiche R_LYON = analyser_riche(LYON, RECU);
constexpr AnalyseRiche R_LONGYEARBYEN = analyser_riche(LONGYEARBYEN, RECU);
constexpr AnalyseRiche R_SEATTLE = analyser_riche(SEATTLE, RECU);
constexpr AnalyseRiche R_SINGAPOUR = analyser_riche(SINGAPOUR, RECU);
constexpr AnalyseRiche R_SYDNEY = analyser_riche(SYDNEY, RECU);
constexpr AnalyseRiche R_KATMANDOU = analyser_riche(KATMANDOU, RECU);
constexpr AnalyseRiche R_TERRENEUVE = analyser_riche(TERRENEUVE, RECU);

// ---- outils -------------------------------------------------------------------------------------------------------------

// Le `Meteo` en une liste de nombres, dans l'ORDRE du fichier attendu (controle_python_riche.py, `valeurs_attendues`).
struct Plat {
    long long v[600] = {};
    std::size_t n = 0;
    bool trop = false;
    constexpr void ajouter(long long x) noexcept {
        if (n < 600)
            v[n++] = x;
        else
            trop = true;
    }
};

constexpr Plat aplatir(const Meteo &m) noexcept {
    Plat p;
    const Releve &r = m.releve;
    p.ajouter(r.instant_s);
    p.ajouter(r.decalage_utc_s);
    p.ajouter(r.temperature_dixiemes);
    p.ajouter(r.ressentie_dixiemes);
    p.ajouter(r.code_wmo);
    p.ajouter(r.jour ? 1 : 0);
    p.ajouter(r.vent_dixiemes_kmh);
    p.ajouter(r.rafales_dixiemes_kmh);
    p.ajouter(r.direction_vent_deg);
    p.ajouter(r.pression_dixiemes_hpa);
    p.ajouter(r.humidite_pct);
    p.ajouter(r.proba_precip_pct);
    p.ajouter(r.nb_jours);
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        const Jour &j = r.jours[i];
        p.ajouter(j.date_s);
        p.ajouter(j.code_wmo);
        p.ajouter(j.min_dixiemes);
        p.ajouter(j.max_dixiemes);
        p.ajouter(j.lever_s);
        p.ajouter(j.coucher_s);
        p.ajouter(j.uv_max_dixiemes);
        p.ajouter(j.proba_precip_max);
    }
    const Riche &q = m.riche;
    p.ajouter(q.precip_courante_dixiemes);
    p.ajouter(q.nuages_pct);
    p.ajouter(q.nb_heures);
    for (std::size_t i = 0; i < q.nb_heures; ++i) {
        const HeureRiche &h = q.heures[i];
        p.ajouter(h.debut_s);
        p.ajouter(h.temperature_dixiemes);
        p.ajouter(h.ressentie_dixiemes);
        p.ajouter(h.humidite_pct);
        p.ajouter(h.proba_pct);
        p.ajouter(h.precip_dixiemes_mm);
        p.ajouter(h.code_wmo);
        p.ajouter(h.nuages_pct);
        p.ajouter(h.vent_dixiemes_kmh);
        p.ajouter(h.rafales_dixiemes_kmh);
        p.ajouter(h.direction_deg);
        p.ajouter(h.uv_dixiemes);
        p.ajouter(h.jour ? 1 : 0);
    }
    p.ajouter(q.nb_jours);
    for (std::size_t i = 0; i < q.nb_jours; ++i) {
        const JourRiche &j = q.jours[i];
        p.ajouter(j.ressentie_min_dixiemes);
        p.ajouter(j.ressentie_max_dixiemes);
        p.ajouter(j.ensoleillement_min);
        p.ajouter(j.precip_dixiemes_mm);
        p.ajouter(j.neige_dixiemes_cm);
        p.ajouter(j.heures_precip);
        p.ajouter(j.vent_max_dixiemes_kmh);
        p.ajouter(j.rafales_max_dixiemes_kmh);
        p.ajouter(j.direction_dominante_deg);
    }
    p.ajouter(q.nb_quarts);
    p.ajouter(q.quarts_debut_s);
    for (std::size_t i = 0; i < q.nb_quarts; ++i)
        p.ajouter(q.quarts_dixiemes[i]);
    return p;
}

// Rend 0 si tout concorde, sinon 10000 + le rang de la première valeur fausse (voir le fichier attendu) ou 1 à 4 pour une
// section refusée.
template <std::size_t N> constexpr int comparer(const AnalyseRiche &a, const long long (&attendu)[N]) noexcept {
    if (!a.ok())
        return 1;
    if (!a.diag_heures.ok())
        return 2;
    if (!a.diag_jours.ok())
        return 3;
    if (!a.diag_quarts.ok())
        return 4;
    const Plat p = aplatir(a.meteo);
    if (p.trop || p.n != N)
        return 5;
    for (std::size_t i = 0; i < N; ++i) {
        if (p.v[i] != attendu[i])
            return 10000 + static_cast<int>(i);
    }
    return verifier_meteo(a.meteo).ok() ? 0 : 6;
}

// Un texte modifiable à la compilation : sert à casser UNE chose dans une vraie réponse.
struct Texte {
    char d[6000] = {};
    std::size_t n = 0;
    constexpr sv vue() const noexcept { return sv(d, n); }
};

// Copie `source` en remplaçant la `rang`-ième occurrence (de 0) de `ancien` par `nouveau`. n == 0 : occurrence introuvable
// ou résultat trop grand (le test qui s'en sert doit le refuser).
constexpr Texte remplacer(sv source, sv ancien, sv nouveau, int rang = 0) noexcept {
    Texte t;
    std::size_t pos = 0;
    for (int i = 0; i <= rang; ++i) {
        pos = source.find(ancien, i == 0 ? 0 : pos + 1);
        if (pos == sv::npos)
            return t;
    }
    if (source.size() - ancien.size() + nouveau.size() > sizeof(t.d))
        return t;
    for (std::size_t i = 0; i < pos; ++i)
        t.d[t.n++] = source[i];
    for (const char c : nouveau)
        t.d[t.n++] = c;
    for (std::size_t i = pos + ancien.size(); i < source.size(); ++i)
        t.d[t.n++] = source[i];
    return t;
}

constexpr Texte coller(sv a, sv b, sv c, sv d) noexcept {
    Texte t;
    for (const sv morceau : {a, b, c, d}) {
        for (const char x : morceau) {
            if (t.n >= sizeof(t.d))
                return Texte{};
            t.d[t.n++] = x;
        }
    }
    return t;
}

// Un cache v2 dont la somme de contrôle est refaite : sert à fabriquer un cache ALTÉRÉ mais bien scellé.
constexpr void resceller(std::uint8_t *tampon, std::size_t n) noexcept {
    const std::uint32_t crc = detail::crc32(tampon, n - 4);
    for (int i = 0; i < 4; ++i)
        tampon[n - 4 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((crc >> (8 * i)) & 0xFFU);
}

// ---- réponses réelles ---------------------------------------------------------------------------------------------------

constexpr int lyon_identique_a_python() { return comparer(R_LYON, ATT_LYON); }
constexpr int nuit_polaire_de_longyearbyen_identique_a_python() { return comparer(R_LONGYEARBYEN, ATT_LONGYEARBYEN); }
constexpr int pluie_en_cours_de_seattle_identique_a_python() { return comparer(R_SEATTLE, ATT_SEATTLE); }
constexpr int singapour_identique_a_python() { return comparer(R_SINGAPOUR, ATT_SINGAPOUR); }
constexpr int sydney_identique_a_python() { return comparer(R_SYDNEY, ATT_SYDNEY); }
constexpr int katmandou_identique_a_python() { return comparer(R_KATMANDOU, ATT_KATMANDOU); }
constexpr int terre_neuve_identique_a_python() { return comparer(R_TERRENEUVE, ATT_TERRENEUVE); }

constexpr int les_cumuls_sont_decales_d_une_etiquette_sur_seattle() {
    // Étiquette 0 de la réponse : précipitations 1,7 mm, étiquette 1 : 1,0 mm, étiquette 2 : 0,2 mm, probabilités 62, 48, 35 %,
    // UV 0,1 puis 0,05. L'heure [T0, T0 + 1 h) porte la valeur de l'étiquette SUIVANTE (Open-Meteo : intervalle qui précède).
    const Riche &r = R_SEATTLE.meteo.riche;
    VERIFIER(r.nb_heures == 24);
    VERIFIER(r.heures[0].debut_s == T0);
    VERIFIER(r.heures[0].precip_dixiemes_mm == 10 && r.heures[1].precip_dixiemes_mm == 2);
    VERIFIER(r.heures[0].proba_pct == 48 && r.heures[1].proba_pct == 35);
    VERIFIER(r.heures[0].uv_dixiemes == 1 &&
             r.heures[1].uv_dixiemes == 0); // 0,05 -> 0,1 (moitié loin de zéro) puis 0,0
    // Les valeurs instantanées, elles, restent à leur étiquette.
    VERIFIER(r.heures[0].temperature_dixiemes == R_SEATTLE.meteo.releve.temperature_dixiemes);
    return 0;
}

constexpr int les_quarts_d_heure_sont_recales_sur_l_intervalle_qui_precede() {
    // Seattle : 0,8 0,3 0,3 0,2 0,2 0,1 0,0 0,1 0,0 mm, première étiquette = T0. Le quart [T0 - 15 min, T0) a donc 0,8 mm.
    const Riche &r = R_SEATTLE.meteo.riche;
    VERIFIER(r.nb_quarts == 9 && r.quarts_debut_s == T0 - 900);
    constexpr std::int16_t ATTENDU[9] = {8, 3, 3, 2, 2, 1, 0, 1, 0};
    for (std::size_t i = 0; i < 9; ++i)
        VERIFIER(r.quarts_dixiemes[i] == ATTENDU[i]);
    VERIFIER(r.precip_courante_dixiemes == 8); // current.precipitation = le même quart (étiquette T0)
    return 0;
}

constexpr int les_fuseaux_a_decalage_fractionnaire_commencent_avant_ou_apres_l_heure_courante() {
    // Constaté sur l'API le 07/11/2026 : Katmandou (+5 h 45) commence 45 min AVANT current.time, Terre-Neuve (-3 h 30) 30 min
    // APRÈS. Le noyau accepte les deux et ne prétend pas couvrir ce que l'API ne donne pas.
    VERIFIER(R_KATMANDOU.meteo.riche.heures[0].debut_s == T0 - 2700);
    VERIFIER(R_KATMANDOU.meteo.riche.quarts_debut_s == T0 - 2700 - 900);
    VERIFIER(R_TERRENEUVE.meteo.riche.heures[0].debut_s == T0 + 1800);
    VERIFIER(R_TERRENEUVE.meteo.riche.quarts_debut_s == T0 + 1800 - 900);
    VERIFIER(R_TERRENEUVE.meteo.releve.decalage_utc_s == -12600); // -3 h 30 : heure d'hiver de Terre-Neuve
    VERIFIER(R_KATMANDOU.meteo.releve.decalage_utc_s == 20700);
    const sv kat = R_KATMANDOU.meteo.releve.fuseau;
    const sv terre = R_TERRENEUVE.meteo.releve.fuseau;
    VERIFIER(kat == "Asia/Kathmandu" && terre == "America/St_Johns");
    return 0;
}

constexpr int la_nuit_polaire_n_a_pas_de_soleil_ni_d_ensoleillement() {
    const Meteo &m = R_LONGYEARBYEN.meteo;
    VERIFIER(m.releve.nb_jours == 7 && m.riche.nb_jours == 7);
    for (std::size_t i = 0; i < 7; ++i) {
        VERIFIER(m.releve.jours[i].sans_soleil());
        VERIFIER(m.riche.jours[i].ensoleillement_min == 0);
    }
    VERIFIER(m.riche.jours[0].neige_dixiemes_cm == 4); // 0,35 cm -> 0,4 (moitié loin de zéro)
    return 0;
}

constexpr int l_ensoleillement_est_arrondi_a_la_minute_la_plus_proche() {
    VERIFIER(R_SYDNEY.meteo.riche.jours[0].ensoleillement_min == 694);    // 41641.93 s = 694,03 min
    VERIFIER(R_SYDNEY.meteo.riche.jours[1].ensoleillement_min == 780);    // 46800.0 s pile
    VERIFIER(R_TERRENEUVE.meteo.riche.jours[2].ensoleillement_min == 30); // 1815.49 s = 30,26 min
    return 0;
}

// ---- une section cassée ne coûte pas le reste ---------------------------------------------------------------------------

constexpr int sans_hourly_le_reste_reste_utilisable() {
    const Texte t = remplacer(LYON, "\"hourly\":{", "\"hourlyX\":{");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(!a.diag_heures.ok() && a.diag_heures.erreur == Erreur::ChampAbsent && a.diag_heures.champ == "hourly");
    VERIFIER(a.meteo.riche.nb_heures == 0);
    VERIFIER(a.diag_jours.ok() && a.meteo.riche.nb_jours == 7);
    VERIFIER(a.diag_quarts.ok() && a.meteo.riche.nb_quarts == 9);
    VERIFIER(a.meteo.releve.proba_precip_pct == PROBA_INCONNUE);
    return 0;
}

constexpr int sans_minutely_15_les_heures_restent() {
    const Texte t = remplacer(LYON, "\"minutely_15\":{", "\"minutely_15X\":{");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok() && a.diag_jours.ok());
    VERIFIER(a.diag_quarts.erreur == Erreur::ChampAbsent && a.diag_quarts.champ == "minutely_15");
    VERIFIER(a.meteo.riche.nb_quarts == 0 && a.meteo.riche.nb_heures == 24);
    return 0;
}

constexpr int sans_ensoleillement_les_jours_riches_sont_vides_mais_pas_les_jours_de_base() {
    const Texte t = remplacer(LYON, "\"sunshine_duration\":[", "\"sunshine_durationX\":[");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok() && a.diag_quarts.ok());
    VERIFIER(a.diag_jours.erreur == Erreur::ChampAbsent && a.diag_jours.champ == "daily.sunshine_duration");
    VERIFIER(a.meteo.riche.nb_jours == 0 && a.meteo.releve.nb_jours == 7);
    return 0;
}

constexpr int une_colonne_horaire_trop_longue_vide_les_heures() {
    const Texte t = remplacer(LYON, "\"is_day\":[", "\"is_day\":[7,"); // la première occurrence est celle de `hourly`
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_heures.erreur == Erreur::TableauxIncoherents && a.diag_heures.champ == "hourly.is_day");
    VERIFIER(a.meteo.riche.nb_heures == 0);
    return 0;
}

constexpr int une_temperature_horaire_invraisemblable_vide_les_heures() {
    const Texte t = remplacer(LYON, "\"temperature_2m\":[7.4,", "\"temperature_2m\":[99.4,");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_heures.erreur == Erreur::HorsPlage && a.diag_heures.champ == "hourly.temperature_2m");
    VERIFIER(a.meteo.riche.nb_heures == 0);
    return 0;
}

constexpr int une_valeur_horaire_texte_est_refusee() {
    const Texte t = remplacer(LYON, "\"relative_humidity_2m\":[98,", "\"relative_humidity_2m\":[\"98\",");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_heures.erreur == Erreur::MauvaisType && a.diag_heures.champ == "hourly.relative_humidity_2m");
    return 0;
}

constexpr int le_code_horaire_doit_etre_entier() {
    const Texte t = remplacer(LYON, "\"weather_code\":[45,",
                              "\"weather_code\":[45.0,"); // la 1re occurrence est celle de `hourly`, avant `daily`
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_heures.erreur == Erreur::MauvaisType && a.diag_heures.champ == "hourly.weather_code");
    return 0;
}

constexpr int sans_probabilite_toutes_les_heures_sont_inconnues_mais_valides() {
    const Texte t = remplacer(LYON, "\"precipitation_probability\":[", "\"precipitation_probabilityX\":[");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok());
    VERIFIER(a.meteo.riche.nb_heures == 24);
    for (std::size_t i = 0; i < 24; ++i)
        VERIFIER(a.meteo.riche.heures[i].proba_pct == PROBA_INCONNUE);
    return 0;
}

constexpr int une_probabilite_nulle_est_inconnue_sans_gener_les_autres() {
    const Texte t = remplacer(LYON, "\"precipitation_probability\":[0,0,", "\"precipitation_probability\":[0,null,");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok());
    VERIFIER(a.meteo.riche.heures[0].proba_pct == PROBA_INCONNUE); // heures[0] lit l'étiquette 1
    VERIFIER(a.meteo.riche.heures[1].proba_pct == 0);
    return 0;
}

constexpr int des_quarts_pas_a_quinze_minutes_sont_refuses() {
    const Texte t = remplacer(LYON, "1794007800,", "1794007801,"); // la 3e étiquette de minutely_15 (la 1re occurrence)
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_quarts.erreur == Erreur::TableauxIncoherents && a.diag_quarts.champ == "minutely_15.time");
    VERIFIER(a.meteo.riche.nb_quarts == 0);
    return 0;
}

constexpr int une_pluie_negative_est_refusee() {
    const Texte t = remplacer(LYON, "\"precipitation\":[0.00,0.00,", "\"precipitation\":[0.00,-0.50,"); // minutely_15
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_quarts.erreur == Erreur::HorsPlage && a.diag_quarts.champ == "minutely_15.precipitation");
    VERIFIER(a.meteo.riche.nb_quarts == 0 && a.meteo.riche.nb_heures == 24 && a.meteo.riche.nb_jours == 7);
    return 0;
}

constexpr int une_section_du_mauvais_type_est_refusee() {
    const Texte t = remplacer(LYON, "\"minutely_15\":{", "\"minutely_15\":5,\"zz\":{");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_quarts.erreur == Erreur::MauvaisType && a.diag_quarts.champ == "minutely_15");
    return 0;
}

constexpr int sans_precipitation_courante_toute_la_reponse_est_refusee() {
    const Texte t = remplacer(LYON, "\"precipitation\":0.00", "\"precipitationX\":0.00");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(!a.ok());
    VERIFIER(a.diag.erreur == Erreur::ChampAbsent && a.diag.champ == "current.precipitation");
    return 0;
}

constexpr int une_couverture_nuageuse_de_101_pour_cent_est_refusee() {
    const Texte t = remplacer(LYON, "\"cloud_cover\":48,", "\"cloud_cover\":101,");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(!a.ok());
    VERIFIER(a.diag.erreur == Erreur::HorsPlage && a.diag.champ == "current.cloud_cover");
    return 0;
}

constexpr int une_reponse_de_base_fausse_reste_refusee_sans_toucher_aux_ajouts() {
    const Texte t = remplacer(LYON, "\"temperature_2m\":7.4,", "\"temperature_2m\":99.4,");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::HorsPlage);
    VERIFIER(a.meteo.riche.nb_heures == 0 && a.meteo.riche.nb_quarts == 0 && a.meteo.riche.nb_jours == 0);
    VERIFIER(!analyser_riche("", RECU).ok());
    VERIFIER(!analyser_riche("[]", RECU).ok());
    VERIFIER(!analyser_riche("{", RECU).ok());
    return 0;
}

constexpr int une_neige_negative_vide_les_jours_riches_sans_toucher_aux_jours_de_base() {
    const Texte t = remplacer(LYON, "\"snowfall_sum\":[0.00,", "\"snowfall_sum\":[-1.00,");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok() && a.diag_quarts.ok());
    VERIFIER(a.diag_jours.erreur == Erreur::HorsPlage && a.diag_jours.champ == "daily.snowfall_sum");
    VERIFIER(a.meteo.riche.nb_jours == 0 && a.meteo.releve.nb_jours == 7);
    return 0;
}

// ---- une petite réponse écrite à la main (comptes courts, valeurs lisibles) ---------------------------------------------

constexpr sv MINI_DEBUT =
    R"({"utc_offset_seconds":3600,"timezone":"Europe/Paris","current":{"time":1794006000,"interval":900,"temperature_2m":7.4,)"
    R"("apparent_temperature":6.4,"is_day":0,"precipitation":0.20,"weather_code":45,"cloud_cover":48,"wind_speed_10m":1.5,)"
    R"("wind_direction_10m":346,"wind_gusts_10m":1.8,"pressure_msl":1024.9,"relative_humidity_2m":98})";
constexpr sv MINI_HEURES_3 =
    R"(,"hourly":{"time":[1794006000,1794009600,1794013200],"temperature_2m":[7.4,6.6,6.2],"apparent_temperature":[6.4,5.5,4.9],)"
    R"("relative_humidity_2m":[98,99,95],"precipitation_probability":[10,20,30],"precipitation":[0.0,1.2,0.4],)"
    R"("weather_code":[45,61,61],"cloud_cover":[48,90,100],"wind_speed_10m":[1.5,2.5,3.5],"wind_direction_10m":[346,10,20],)"
    R"("wind_gusts_10m":[1.8,2.8,3.8],"uv_index":[0.0,0.0,0.4],"is_day":[0,0,1]})";
constexpr sv MINI_QUARTS_2 = R"(,"minutely_15":{"time":[1794006000,1794006900],"precipitation":[0.2,0.0]})";

constexpr int une_petite_reponse_donne_moins_d_heures_et_de_quarts() {
    const Texte t = coller(MINI_DEBUT, MINI_HEURES_3, MINI_QUARTS_2, "}");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok() && a.diag_quarts.ok());
    VERIFIER(!a.diag_jours.ok() && a.diag_jours.erreur == Erreur::ChampAbsent); // pas de `daily` du tout
    const Riche &r = a.meteo.riche;
    VERIFIER(r.precip_courante_dixiemes == 2 && r.nuages_pct == 48);
    VERIFIER(r.nb_heures == 2); // 3 étiquettes -> 2 heures complètes
    VERIFIER(r.heures[0].debut_s == 1794006000 && r.heures[1].debut_s == 1794009600);
    VERIFIER(r.heures[0].precip_dixiemes_mm == 12 && r.heures[1].precip_dixiemes_mm == 4);
    VERIFIER(r.heures[0].proba_pct == 20 && r.heures[1].proba_pct == 30);
    VERIFIER(r.heures[0].uv_dixiemes == 0 && r.heures[1].uv_dixiemes == 4);
    VERIFIER(r.heures[0].temperature_dixiemes == 74 && r.heures[1].temperature_dixiemes == 66);
    VERIFIER(r.heures[0].code_wmo == 45 && r.heures[1].code_wmo == 61);
    VERIFIER(r.heures[0].jour == false && r.heures[1].jour == false); // is_day des étiquettes 0 et 1
    VERIFIER(r.nb_quarts == 2 && r.quarts_debut_s == 1794006000 - 900);
    VERIFIER(r.quarts_dixiemes[0] == 2 && r.quarts_dixiemes[1] == 0);
    return 0;
}

constexpr int une_seule_etiquette_horaire_ne_fait_pas_une_heure() {
    constexpr sv UNE_HEURE =
        R"(,"hourly":{"time":[1794006000],"temperature_2m":[7.4],"apparent_temperature":[6.4],"relative_humidity_2m":[98],)"
        R"("precipitation_probability":[10],"precipitation":[0.0],"weather_code":[45],"cloud_cover":[48],"wind_speed_10m":[1.5],)"
        R"("wind_direction_10m":[346],"wind_gusts_10m":[1.8],"uv_index":[0.0],"is_day":[0]})";
    const Texte t = coller(MINI_DEBUT, UNE_HEURE, MINI_QUARTS_2, "}");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag_heures.erreur == Erreur::TableauxIncoherents && a.meteo.riche.nb_heures == 0);
    VERIFIER(a.diag_quarts.ok() && a.meteo.riche.nb_quarts == 2);
    return 0;
}

constexpr int plus_de_vingt_cinq_etiquettes_sont_tronquees_a_vingt_quatre_heures() {
    // 26 étiquettes (comme si l'API en rendait trop) : on garde 24 heures, pas 25.
    char horaire[2048] = {};
    std::size_t n = 0;
    const auto ajouter = [&](sv s) {
        for (const char c : s)
            horaire[n++] = c;
    };
    const auto colonne = [&](sv nom, sv valeur, bool premiere) {
        if (!premiere)
            ajouter(",");
        ajouter("\"");
        ajouter(nom);
        ajouter("\":[");
        for (int i = 0; i < 26; ++i) {
            if (i > 0)
                ajouter(",");
            ajouter(valeur);
        }
        ajouter("]");
    };
    ajouter(",\"hourly\":{");
    ajouter("\"time\":[");
    for (int i = 0; i < 26; ++i) {
        if (i > 0)
            ajouter(",");
        const long long t = 1794006000LL + 3600LL * i;
        char chiffres[16] = {};
        int k = 0;
        long long x = t;
        while (x > 0) {
            chiffres[k++] = static_cast<char>('0' + x % 10);
            x /= 10;
        }
        while (k > 0)
            horaire[n++] = chiffres[--k];
    }
    ajouter("]");
    colonne("temperature_2m", "5.0", false);
    colonne("apparent_temperature", "4.0", false);
    colonne("relative_humidity_2m", "50", false);
    colonne("precipitation_probability", "10", false);
    colonne("precipitation", "0.1", false);
    colonne("weather_code", "3", false);
    colonne("cloud_cover", "10", false);
    colonne("wind_speed_10m", "1.0", false);
    colonne("wind_direction_10m", "90", false);
    colonne("wind_gusts_10m", "2.0", false);
    colonne("uv_index", "0.0", false);
    colonne("is_day", "1", false);
    ajouter("}");
    const Texte t = coller(MINI_DEBUT, sv(horaire, n), MINI_QUARTS_2, "}");
    VERIFIER(t.n != 0);
    const AnalyseRiche a = analyser_riche(t.vue(), RECU);
    VERIFIER(a.ok() && a.diag_heures.ok());
    VERIFIER(a.meteo.riche.nb_heures == 24);
    VERIFIER(a.meteo.riche.heures[23].debut_s == 1794006000LL + 3600LL * 23);
    return 0;
}

// ---- l'heure en cours ---------------------------------------------------------------------------------------------------

constexpr int heure_courante_aux_bornes_de_lyon() {
    const Riche &r = R_LYON.meteo.riche;
    VERIFIER(heure_courante(r, T0) == 0);
    VERIFIER(heure_courante(r, T0 + 3599) == 0);
    VERIFIER(heure_courante(r, T0 + 3600) == 1);
    VERIFIER(heure_courante(r, T0 - 1) == -1);
    VERIFIER(heure_courante(r, T0 + 23 * 3600 + 3599) == 23);
    VERIFIER(heure_courante(r, T0 + 24 * 3600) == -1);
    VERIFIER(heure_courante(Riche{}, T0) == -1);
    return 0;
}

constexpr int heure_courante_des_fuseaux_a_decalage_fractionnaire() {
    // Katmandou : la liste commence 45 min avant l'heure courante ; Terre-Neuve : 30 min après (l'heure en cours n'y est pas).
    VERIFIER(heure_courante(R_KATMANDOU.meteo.riche, T0) == 0);
    VERIFIER(heure_courante(R_KATMANDOU.meteo.riche, T0 + 899) == 0);
    VERIFIER(heure_courante(R_KATMANDOU.meteo.riche, T0 + 900) == 1);
    VERIFIER(heure_courante(R_TERRENEUVE.meteo.riche, T0) == -1);
    VERIFIER(heure_courante(R_TERRENEUVE.meteo.riche, T0 + 1799) == -1);
    VERIFIER(heure_courante(R_TERRENEUVE.meteo.riche, T0 + 1800) == 0);
    return 0;
}

constexpr int quart_courant_aux_bornes() {
    const Riche &r = R_LYON.meteo.riche; // quarts_debut_s = T0 - 900, 9 quarts : jusqu'à T0 + 7200 exclu
    VERIFIER(quart_courant(r, T0 - 900) == 0);
    VERIFIER(quart_courant(r, T0 - 901) == -1);
    VERIFIER(quart_courant(r, T0 - 1) == 0);
    VERIFIER(quart_courant(r, T0) == 1);
    VERIFIER(quart_courant(r, T0 + 899) == 1);
    VERIFIER(quart_courant(r, T0 + 900) == 2);
    VERIFIER(quart_courant(r, T0 + 7199) == 8);
    VERIFIER(quart_courant(r, T0 + 7200) == -1);
    VERIFIER(quart_courant(Riche{}, T0) == -1);
    return 0;
}

constexpr int probabilite_courante_est_celle_de_l_heure_recalee() {
    VERIFIER(proba_courante(R_SEATTLE.meteo.riche, T0) == 48);
    VERIFIER(proba_courante(R_SEATTLE.meteo.riche, T0 + 3600) == 35);
    VERIFIER(proba_courante(R_SEATTLE.meteo.riche, T0 - 1) == PROBA_INCONNUE);
    VERIFIER(proba_courante(R_TERRENEUVE.meteo.riche, T0) ==
             PROBA_INCONNUE); // l'heure en cours n'est pas dans la liste
    return 0;
}

// ---- pluie des prochaines minutes ---------------------------------------------------------------------------------------

// Quarts d'heure écrits à la main : le premier commence à D, les suivants toutes les 15 min.
constexpr std::int64_t D = T0 - 900;
template <std::size_t N> constexpr Riche avec_quarts(const std::int16_t (&dixiemes)[N]) noexcept {
    Riche r;
    r.nb_quarts = static_cast<std::uint8_t>(N);
    r.quarts_debut_s = D;
    for (std::size_t i = 0; i < N; ++i)
        r.quarts_dixiemes[i] = dixiemes[i];
    return r;
}

constexpr int pluie_proche_sans_quarts_n_est_pas_connue() {
    const PluieProche p = pluie_proche(Riche{}, T0, T0 + 30);
    VERIFIER(!p.connue && !p.en_cours && !p.perimee && p.dans_min == -1 && p.fin_dans_min == -1 && p.fenetre_min == 0);
    VERIFIER(p.cumul_dixiemes == 0 && p.max_quart_dixiemes == 0);
    return 0;
}

constexpr int pluie_proche_qui_commence_dans_le_quart_suivant() {
    // Quarts : sec, sec, 0,3 mm, 0,3 mm, sec, sec, 0,1 mm, sec, sec ; on est 30 s après le début du 2e quart (sec).
    constexpr std::int16_t Q[9] = {0, 0, 3, 3, 0, 0, 1, 0, 0};
    const Riche r = avec_quarts(Q);
    const std::int64_t maintenant = D + 900 + 30;
    const PluieProche p = pluie_proche(r, maintenant - 60, maintenant);
    VERIFIER(p.connue && !p.perimee && !p.en_cours);
    VERIFIER(p.dans_min == 15);     // début à D + 1800 : dans 870 s = 14,5 min -> 15
    VERIFIER(p.fin_dans_min == 45); // fin à D + 3600 : dans 2670 s = 44,5 min -> 45
    VERIFIER(p.fenetre_min == 120); // fin du dernier quart à D + 8100 : dans 7170 s = 119,5 min -> 120
    VERIFIER(p.cumul_dixiemes == 7 && p.max_quart_dixiemes == 3); // du quart en cours (inclus) à la fin
    return 0;
}

constexpr int pluie_proche_deja_en_cours() {
    constexpr std::int16_t Q[9] = {0, 0, 3, 3, 0, 0, 1, 0, 0};
    const Riche r = avec_quarts(Q);
    const std::int64_t maintenant = D + 1800; // le début EXACT du 3e quart appartient au 3e quart
    const PluieProche p = pluie_proche(r, maintenant, maintenant);
    VERIFIER(p.connue && p.en_cours && p.dans_min == 0);
    VERIFIER(p.fin_dans_min == 30); // fin à D + 3600
    VERIFIER(p.cumul_dixiemes == 7);
    const PluieProche q = pluie_proche(r, maintenant, D + 1799); // une seconde avant : encore sec, début dans 1 min
    VERIFIER(q.connue && !q.en_cours && q.dans_min == 1);
    return 0;
}

constexpr int pluie_proche_sans_fin_dans_la_fenetre() {
    constexpr std::int16_t Q[9] = {0, 0, 0, 0, 0, 2, 5, 5, 5};
    const Riche r = avec_quarts(Q);
    const PluieProche p = pluie_proche(r, T0, T0); // T0 = début du 2e quart
    VERIFIER(p.connue && !p.en_cours);
    VERIFIER(p.dans_min == 60);     // le 6e quart commence à D + 4500 : dans 3600 s
    VERIFIER(p.fin_dans_min == -1); // la pluie dure jusqu'au bout de la fenêtre : pas de fin connue
    VERIFIER(p.cumul_dixiemes == 17 && p.max_quart_dixiemes == 5);
    VERIFIER(p.fenetre_min == 120);
    return 0;
}

constexpr int pluie_proche_sur_un_ciel_sec() {
    constexpr std::int16_t Q[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    const PluieProche p = pluie_proche(avec_quarts(Q), T0, T0 + 10);
    VERIFIER(p.connue && !p.en_cours && p.dans_min == -1 && p.fin_dans_min == -1);
    VERIFIER(p.cumul_dixiemes == 0 && p.max_quart_dixiemes == 0 && p.fenetre_min == 120);
    return 0;
}

constexpr int pluie_proche_le_seuil_est_un_dixieme_de_millimetre() {
    constexpr std::int16_t Q[3] = {0, 1, 0};
    const PluieProche p = pluie_proche(avec_quarts(Q), D, D); // le quart 0 est sec, le quart 1 a 0,1 mm
    VERIFIER(p.connue && !p.en_cours);
    VERIFIER(p.dans_min == 15 && p.fin_dans_min == 30);
    VERIFIER(p.fenetre_min == 45);
    const PluieProche e = pluie_proche(avec_quarts(Q), D, D + 900); // dans le quart de 0,1 mm
    VERIFIER(e.connue && e.en_cours && e.dans_min == 0 && e.fin_dans_min == 15);
    return 0;
}

constexpr int pluie_proche_aux_bornes_de_la_fenetre() {
    constexpr std::int16_t Q[9] = {0, 0, 0, 0, 0, 0, 0, 0, 4};
    const Riche r = avec_quarts(Q);
    const PluieProche dernier = pluie_proche(r, D + 8099, D + 8099); // une seconde avant la fin du dernier quart
    VERIFIER(dernier.connue && dernier.en_cours && dernier.dans_min == 0);
    VERIFIER(dernier.fin_dans_min == -1); // elle touche la fin de la fenêtre : on ne sait pas si elle s'arrête
    VERIFIER(dernier.fenetre_min == 1);
    VERIFIER(!pluie_proche(r, D + 8100, D + 8100).connue); // tous les quarts sont passés
    VERIFIER(!pluie_proche(r, D - 1, D - 1).connue);       // pas encore commencés
    return 0;
}

constexpr int pluie_proche_horloge_non_reglee_ou_en_retard_n_est_pas_connue() {
    constexpr std::int16_t Q[9] = {0, 0, 3, 3, 0, 0, 1, 0, 0};
    const Riche r = avec_quarts(Q);
    VERIFIER(!pluie_proche(r, T0, 0).connue);         // horloge non réglée
    VERIFIER(!pluie_proche(r, T0, 946684799).connue); // avant l'an 2000
    VERIFIER(!pluie_proche(r, T0 + 301, T0).connue);  // réception « dans le futur » de plus de 5 min
    VERIFIER(pluie_proche(r, T0 + 300, T0).connue);   // exactement la tolérance : encore accepté
    return 0;
}

constexpr int pluie_proche_perimee_apres_trente_minutes() {
    constexpr std::int16_t Q[9] = {0, 0, 3, 3, 0, 0, 1, 0, 0};
    const Riche r = avec_quarts(Q);
    VERIFIER(!pluie_proche(r, T0, T0 + 1800).perimee);
    VERIFIER(pluie_proche(r, T0, T0 + 1801).perimee);
    VERIFIER(pluie_proche(r, T0, T0 + 1801).connue); // périmée mais lisible
    return 0;
}

constexpr int pluie_proche_reelle_de_seattle() {
    // Quarts de Seattle (0,8 0,3 0,3 0,2 0,2 0,1 0,0 0,1 0,0) : on est 60 s après T0, dans le 2e quart.
    const PluieProche p = pluie_proche(R_SEATTLE.meteo.riche, RECU, T0 + 60);
    VERIFIER(p.connue && !p.perimee && p.en_cours && p.dans_min == 0);
    VERIFIER(p.fin_dans_min == 74);   // le 7e quart (sec) commence à T0 + 4500 : dans 4440 s
    VERIFIER(p.cumul_dixiemes == 12); // 0,3 0,3 0,2 0,2 0,1 0,0 0,1 0,0 : le quart écoulé (0,8) ne compte plus
    VERIFIER(p.max_quart_dixiemes == 3);
    VERIFIER(p.fenetre_min == 119);
    const PluieProche s = pluie_proche(R_LYON.meteo.riche, RECU, T0 + 60);
    VERIFIER(s.connue && !s.en_cours && s.dans_min == -1 && s.cumul_dixiemes == 0);
    const PluieProche k = pluie_proche(R_TERRENEUVE.meteo.riche, RECU, T0); // les quarts commencent après : rien à dire
    VERIFIER(!k.connue);
    return 0;
}

// ---- cache binaire v2 ----------------------------------------------------------------------------------------------------

constexpr std::int32_t LAT = 4574906;
constexpr std::int32_t LON = 484789;

constexpr int aller_retour(const Meteo &m) {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(m, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0 && n <= TAILLE_CACHE_RICHE_MAX);
    const LectureCacheMeteo l = deserialiser_cache_meteo(tampon, n);
    VERIFIER(l.ok() && l.latitude_e5 == LAT && l.longitude_e5 == LON);
    const Plat a = aplatir(m);
    const Plat b = aplatir(l.meteo);
    VERIFIER(!a.trop && a.n == b.n);
    for (std::size_t i = 0; i < a.n; ++i)
        VERIFIER(a.v[i] == b.v[i]);
    VERIFIER(l.meteo.releve.recu_a_s == m.releve.recu_a_s);
    VERIFIER(sv(l.meteo.releve.fuseau) == sv(m.releve.fuseau));
    return 0;
}

constexpr int cache_v2_aller_retour_de_chaque_reponse_reelle() {
    VERIFIER(aller_retour(R_LYON.meteo) == 0);
    VERIFIER(aller_retour(R_LONGYEARBYEN.meteo) == 0);
    VERIFIER(aller_retour(R_SEATTLE.meteo) == 0);
    VERIFIER(aller_retour(R_SINGAPOUR.meteo) == 0);
    VERIFIER(aller_retour(R_SYDNEY.meteo) == 0);
    VERIFIER(aller_retour(R_KATMANDOU.meteo) == 0);
    VERIFIER(aller_retour(R_TERRENEUVE.meteo) == 0);
    return 0;
}

constexpr int cache_v2_aller_retour_avec_des_sections_vides() {
    Meteo m = R_LYON.meteo;
    m.riche.nb_heures = 0;
    m.riche.nb_quarts = 0;
    m.riche.quarts_debut_s = 0; // sans quart d'heure, le début n'est pas gardé
    m.riche.nb_jours = 0;
    VERIFIER(aller_retour(m) == 0);
    m.riche = Riche{};
    VERIFIER(aller_retour(m) == 0);
    return 0;
}

constexpr int cache_v2_garde_les_probabilites_inconnues() {
    Meteo m = R_LYON.meteo;
    for (std::size_t i = 0; i < m.riche.nb_heures; ++i)
        m.riche.heures[i].proba_pct = static_cast<std::int16_t>(PROBA_INCONNUE);
    VERIFIER(aller_retour(m) == 0);
    return 0;
}

constexpr int cache_v2_refuse_tout_octet_altere() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(R_SEATTLE.meteo, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    for (std::size_t i = 0; i < n; ++i) {
        tampon[i] = static_cast<std::uint8_t>(tampon[i] ^ 0x01U);
        VERIFIER(!deserialiser_cache_meteo(tampon, n).ok());
        tampon[i] = static_cast<std::uint8_t>(tampon[i] ^ 0x01U);
    }
    VERIFIER(deserialiser_cache_meteo(tampon, n).ok()); // remis en état
    return 0;
}

constexpr int cache_v2_refuse_toute_troncature_et_les_entrees_nulles() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(R_LYON.meteo, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    for (std::size_t i = 0; i < n; ++i)
        VERIFIER(!deserialiser_cache_meteo(tampon, i).ok());
    VERIFIER(!deserialiser_cache_meteo(nullptr, n).ok());
    VERIFIER(!deserialiser_cache_meteo(tampon, 0).ok());
    return 0;
}

constexpr int cache_v1_d_avant_est_ignore_proprement() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache(R_LYON.meteo.releve, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    const LectureCacheMeteo l = deserialiser_cache_meteo(tampon, n);
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::VersionInconnue && l.diag.champ == "cache.version");
    VERIFIER(l.latitude_e5 == 0 && l.longitude_e5 == 0 && l.meteo.releve.nb_jours == 0); // rien n'est rendu
    return 0;
}

constexpr int un_ancien_firmware_refuse_le_cache_v2() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(R_LYON.meteo, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    const LectureCache l = deserialiser_cache(tampon, n); // le lecteur v1
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::VersionInconnue);
    return 0;
}

constexpr int cache_v2_d_une_version_future_est_refuse() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(R_LYON.meteo, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    tampon[2] = 3;
    resceller(tampon, n); // même scellé, une version inconnue n'est jamais interprétée
    const LectureCacheMeteo l = deserialiser_cache_meteo(tampon, n);
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::VersionInconnue);
    return 0;
}

// Décalage de la partie « riche » dans un cache v2 (voir le format dans meteo_riche.hpp) : en-tête de 13 octets (marque, version,
// position, longueur du relevé), le relevé, puis 'M' 'R' version(1) précipitation(2) nuages(1) nb_heures(1) ...
constexpr std::size_t decalage_riche(const std::uint8_t *t) noexcept {
    const std::size_t taille_releve = static_cast<std::size_t>(t[11]) | (static_cast<std::size_t>(t[12]) << 8);
    return 13 + taille_releve;
}

constexpr int cache_v2_scelle_mais_invraisemblable_est_refuse() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(R_LYON.meteo, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    const std::size_t riche = decalage_riche(tampon);
    VERIFIER(tampon[riche] == 'M' && tampon[riche + 1] == 'R' && tampon[riche + 2] == 1);
    const std::size_t nb_heures = riche + 3 + 2 + 1;
    VERIFIER(tampon[nb_heures] == 24);

    std::uint8_t copie[TAILLE_CACHE_RICHE_MAX] = {};
    for (std::size_t i = 0; i < n; ++i)
        copie[i] = tampon[i];
    copie[nb_heures] = 25; // plus d'heures que le contrat n'en porte
    resceller(copie, n);
    LectureCacheMeteo l = deserialiser_cache_meteo(copie, n);
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::Corrompu && l.diag.champ == "cache.heures");

    for (std::size_t i = 0; i < n; ++i)
        copie[i] = tampon[i];
    copie[nb_heures + 1 + 4 + 2 + 2] = 200; // humidité de la 1re heure (après température et ressentie) : 200 %
    resceller(copie, n);
    l = deserialiser_cache_meteo(copie, n);
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::HorsPlage && l.diag.champ == "hourly.relative_humidity_2m");

    for (std::size_t i = 0; i < n; ++i)
        copie[i] = tampon[i];
    copie[3 + 3] = 0x7FU; // latitude : octet de poids fort (e5) -> bien au-delà de 90 degrés
    resceller(copie, n);
    l = deserialiser_cache_meteo(copie, n);
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::HorsPlage && l.diag.champ == "cache.position");

    std::uint8_t trop[TAILLE_CACHE_RICHE_MAX] = {};
    for (std::size_t i = 0; i < n - 4; ++i)
        trop[i] = tampon[i];
    trop[n - 4] = 0xAA; // deux octets en trop avant la somme (qui se refait, à la fin du tampon de n + 2 octets)
    trop[n - 3] = 0xBB;
    resceller(trop, n + 2);
    VERIFIER(deserialiser_cache_meteo(trop, n + 2).diag.erreur == Erreur::Corrompu);
    VERIFIER(deserialiser_cache_meteo(trop, n + 2).diag.champ == "cache.taille");
    return 0;
}

constexpr int cache_v2_refuse_une_place_insuffisante_et_une_entree_invraisemblable() {
    std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
    const std::size_t n = serialiser_cache_meteo(R_SEATTLE.meteo, LAT, LON, tampon, sizeof(tampon));
    VERIFIER(n != 0);
    VERIFIER(serialiser_cache_meteo(R_SEATTLE.meteo, LAT, LON, tampon, n) == n);     // juste assez
    VERIFIER(serialiser_cache_meteo(R_SEATTLE.meteo, LAT, LON, tampon, n - 1) == 0); // un octet de moins
    VERIFIER(serialiser_cache_meteo(R_SEATTLE.meteo, LAT, LON, tampon, 10) == 0);
    VERIFIER(serialiser_cache_meteo(R_SEATTLE.meteo, LAT, LON, nullptr, 500) == 0);
    VERIFIER(serialiser_cache_meteo(R_SEATTLE.meteo, 9000001, LON, tampon, sizeof(tampon)) == 0);
    VERIFIER(serialiser_cache_meteo(R_SEATTLE.meteo, LAT, -18000001, tampon, sizeof(tampon)) == 0);
    Meteo faux = R_SEATTLE.meteo;
    faux.riche.heures[3].humidite_pct = 101; // un relevé invraisemblable ne va pas en NVS
    VERIFIER(serialiser_cache_meteo(faux, LAT, LON, tampon, sizeof(tampon)) == 0);
    faux = R_SEATTLE.meteo;
    faux.riche.nb_jours = 3; // des jours riches qui ne suivent pas les jours de base
    VERIFIER(serialiser_cache_meteo(faux, LAT, LON, tampon, sizeof(tampon)) == 0);
    return 0;
}

constexpr int verifier_meteo_refuse_les_incoherences() {
    VERIFIER(verifier_meteo(R_LYON.meteo).ok());
    Meteo m = R_LYON.meteo;
    m.riche.nuages_pct = 101;
    VERIFIER(verifier_meteo(m).erreur == Erreur::HorsPlage);
    m = R_LYON.meteo;
    m.riche.heures[5].debut_s += 1; // une heure qui ne suit plus la précédente de 3600 s
    VERIFIER(verifier_meteo(m).erreur == Erreur::TableauxIncoherents);
    m = R_LYON.meteo;
    m.riche.heures[0].proba_pct = 101;
    VERIFIER(verifier_meteo(m).erreur == Erreur::HorsPlage);
    m = R_LYON.meteo;
    m.riche.jours[2].ressentie_min_dixiemes = static_cast<std::int16_t>(m.riche.jours[2].ressentie_max_dixiemes + 1);
    VERIFIER(verifier_meteo(m).erreur == Erreur::HorsPlage);
    m = R_LYON.meteo;
    m.riche.quarts_dixiemes[4] = 1001;
    VERIFIER(verifier_meteo(m).erreur == Erreur::HorsPlage);
    m = R_LYON.meteo;
    m.riche.nb_quarts = 10; // plus que QUARTS_MAX
    VERIFIER(verifier_meteo(m).erreur == Erreur::HorsPlage);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(lyon_identique_a_python)                                                                                         \
    X(nuit_polaire_de_longyearbyen_identique_a_python)                                                                 \
    X(pluie_en_cours_de_seattle_identique_a_python)                                                                    \
    X(singapour_identique_a_python)                                                                                    \
    X(sydney_identique_a_python)                                                                                       \
    X(katmandou_identique_a_python)                                                                                    \
    X(terre_neuve_identique_a_python)                                                                                  \
    X(les_cumuls_sont_decales_d_une_etiquette_sur_seattle)                                                             \
    X(les_quarts_d_heure_sont_recales_sur_l_intervalle_qui_precede)                                                    \
    X(les_fuseaux_a_decalage_fractionnaire_commencent_avant_ou_apres_l_heure_courante)                                 \
    X(la_nuit_polaire_n_a_pas_de_soleil_ni_d_ensoleillement)                                                           \
    X(l_ensoleillement_est_arrondi_a_la_minute_la_plus_proche)                                                         \
    X(sans_hourly_le_reste_reste_utilisable)                                                                           \
    X(sans_minutely_15_les_heures_restent)                                                                             \
    X(sans_ensoleillement_les_jours_riches_sont_vides_mais_pas_les_jours_de_base)                                      \
    X(une_colonne_horaire_trop_longue_vide_les_heures)                                                                 \
    X(une_temperature_horaire_invraisemblable_vide_les_heures)                                                         \
    X(une_valeur_horaire_texte_est_refusee)                                                                            \
    X(le_code_horaire_doit_etre_entier)                                                                                \
    X(sans_probabilite_toutes_les_heures_sont_inconnues_mais_valides)                                                  \
    X(une_probabilite_nulle_est_inconnue_sans_gener_les_autres)                                                        \
    X(des_quarts_pas_a_quinze_minutes_sont_refuses)                                                                    \
    X(une_pluie_negative_est_refusee)                                                                                  \
    X(une_section_du_mauvais_type_est_refusee)                                                                         \
    X(une_neige_negative_vide_les_jours_riches_sans_toucher_aux_jours_de_base)                                         \
    X(sans_precipitation_courante_toute_la_reponse_est_refusee)                                                        \
    X(une_couverture_nuageuse_de_101_pour_cent_est_refusee)                                                            \
    X(une_reponse_de_base_fausse_reste_refusee_sans_toucher_aux_ajouts)                                                \
    X(une_petite_reponse_donne_moins_d_heures_et_de_quarts)                                                            \
    X(une_seule_etiquette_horaire_ne_fait_pas_une_heure)                                                               \
    X(plus_de_vingt_cinq_etiquettes_sont_tronquees_a_vingt_quatre_heures)                                              \
    X(heure_courante_aux_bornes_de_lyon)                                                                               \
    X(heure_courante_des_fuseaux_a_decalage_fractionnaire)                                                             \
    X(quart_courant_aux_bornes)                                                                                        \
    X(probabilite_courante_est_celle_de_l_heure_recalee)                                                               \
    X(pluie_proche_sans_quarts_n_est_pas_connue)                                                                       \
    X(pluie_proche_qui_commence_dans_le_quart_suivant)                                                                 \
    X(pluie_proche_deja_en_cours)                                                                                      \
    X(pluie_proche_sans_fin_dans_la_fenetre)                                                                           \
    X(pluie_proche_sur_un_ciel_sec)                                                                                    \
    X(pluie_proche_le_seuil_est_un_dixieme_de_millimetre)                                                              \
    X(pluie_proche_aux_bornes_de_la_fenetre)                                                                           \
    X(pluie_proche_horloge_non_reglee_ou_en_retard_n_est_pas_connue)                                                   \
    X(pluie_proche_perimee_apres_trente_minutes)                                                                       \
    X(pluie_proche_reelle_de_seattle)                                                                                  \
    X(cache_v2_aller_retour_de_chaque_reponse_reelle)                                                                  \
    X(cache_v2_aller_retour_avec_des_sections_vides)                                                                   \
    X(cache_v2_garde_les_probabilites_inconnues)                                                                       \
    X(cache_v2_refuse_tout_octet_altere)                                                                               \
    X(cache_v2_refuse_toute_troncature_et_les_entrees_nulles)                                                          \
    X(cache_v1_d_avant_est_ignore_proprement)                                                                          \
    X(un_ancien_firmware_refuse_le_cache_v2)                                                                           \
    X(cache_v2_d_une_version_future_est_refuse)                                                                        \
    X(cache_v2_scelle_mais_invraisemblable_est_refuse)                                                                 \
    X(cache_v2_refuse_une_place_insuffisante_et_une_entree_invraisemblable)                                            \
    X(verifier_meteo_refuse_les_incoherences)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
