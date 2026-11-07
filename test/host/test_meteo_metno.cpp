// Tests du noyau pur : le lecteur de MET Norway Locationforecast 2.0 « complete » (meteo_metno.hpp).
//
// Huit RÉPONSES RÉELLES, capturées le 07/11/2026 avec l'adresse exacte du firmware (test/host/capturer_fournisseurs_meteo.py) :
// Oslo, Reykjavik, Helsinki, Tromsø, Longyearbyen (nuit polaire), Londres, Tokyo, Sydney. Les valeurs attendues sont recalculées
// par Python, qui relit les réponses sans rien du noyau (test/host/controle_python_fournisseurs.py, lecture_fournisseurs.py) et
// qui écrit donnees/attendu_metno_*.inc : lever, coucher et « jour » sont comparés à quelques minutes près (autre algorithme du
// soleil), tout le reste à l'unité. Les cas limites sont de petites réponses écrites à la main.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../main/meteo_etat.h"
#include "aplatir_fournisseurs.hpp"
#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/meteo_detail.hpp"
#include "ecran/meteo_metno.hpp"
#include "ecran/meteo_riche.hpp"
#include "mini.hpp"

using namespace ecran::meteo;
using namespace fournisseurs_test;

namespace {

constexpr sv P_OSLO[] = {
#include "donnees/metno_oslo.inc"
};
constexpr auto J_OSLO = joindre<longueur_totale(P_OSLO)>(P_OSLO);
constexpr sv OSLO = J_OSLO.vue();
constexpr sv P_REYKJAVIK[] = {
#include "donnees/metno_reykjavik.inc"
};
constexpr auto J_REYKJAVIK = joindre<longueur_totale(P_REYKJAVIK)>(P_REYKJAVIK);
constexpr sv REYKJAVIK = J_REYKJAVIK.vue();
constexpr sv P_HELSINKI[] = {
#include "donnees/metno_helsinki.inc"
};
constexpr auto J_HELSINKI = joindre<longueur_totale(P_HELSINKI)>(P_HELSINKI);
constexpr sv HELSINKI = J_HELSINKI.vue();
constexpr sv P_TROMSO[] = {
#include "donnees/metno_tromso.inc"
};
constexpr auto J_TROMSO = joindre<longueur_totale(P_TROMSO)>(P_TROMSO);
constexpr sv TROMSO = J_TROMSO.vue();
constexpr sv P_LONGYEARBYEN[] = {
#include "donnees/metno_longyearbyen.inc"
};
constexpr auto J_LONGYEARBYEN = joindre<longueur_totale(P_LONGYEARBYEN)>(P_LONGYEARBYEN);
constexpr sv LONGYEARBYEN = J_LONGYEARBYEN.vue();
constexpr sv P_LONDRES[] = {
#include "donnees/metno_londres.inc"
};
constexpr auto J_LONDRES = joindre<longueur_totale(P_LONDRES)>(P_LONDRES);
constexpr sv LONDRES = J_LONDRES.vue();
constexpr sv P_TOKYO[] = {
#include "donnees/metno_tokyo.inc"
};
constexpr auto J_TOKYO = joindre<longueur_totale(P_TOKYO)>(P_TOKYO);
constexpr sv TOKYO = J_TOKYO.vue();
constexpr sv P_SYDNEY[] = {
#include "donnees/metno_sydney.inc"
};
constexpr auto J_SYDNEY = joindre<longueur_totale(P_SYDNEY)>(P_SYDNEY);
constexpr sv SYDNEY = J_SYDNEY.vue();

constexpr long long ATT_OSLO[][2] = {
#include "donnees/attendu_metno_oslo.inc"
};
constexpr long long ATT_REYKJAVIK[][2] = {
#include "donnees/attendu_metno_reykjavik.inc"
};
constexpr long long ATT_HELSINKI[][2] = {
#include "donnees/attendu_metno_helsinki.inc"
};
constexpr long long ATT_TROMSO[][2] = {
#include "donnees/attendu_metno_tromso.inc"
};
constexpr long long ATT_LONGYEARBYEN[][2] = {
#include "donnees/attendu_metno_longyearbyen.inc"
};
constexpr long long ATT_LONDRES[][2] = {
#include "donnees/attendu_metno_londres.inc"
};
constexpr long long ATT_TOKYO[][2] = {
#include "donnees/attendu_metno_tokyo.inc"
};
constexpr long long ATT_SYDNEY[][2] = {
#include "donnees/attendu_metno_sydney.inc"
};

// {nom, reçu à, latitude e5, longitude e5, fuseau}, dans l'ordre de LIEUX_METNO (controle_python_fournisseurs.py).
struct Lieu {
    sv nom;
    long long recu;
    int lat;
    int lon;
    sv fuseau;
};
constexpr Lieu LIEUX[] = {
#include "donnees/lieux_metno.inc"
};
constexpr std::size_t I_OSLO = 0, I_REYKJAVIK = 1, I_HELSINKI = 2, I_TROMSO = 3, I_LONGYEARBYEN = 4, I_LONDRES = 5,
                      I_TOKYO = 6, I_SYDNEY = 7;
static_assert(sizeof LIEUX / sizeof LIEUX[0] == 8, "lieux_metno.inc : huit lieux");

constexpr AnalyseRiche lire(sv texte, const Lieu &l) {
    metno::Serie travail;
    return metno::analyser_metno(texte, l.recu, l.lat, l.lon, l.fuseau, travail);
}

constexpr AnalyseRiche R_OSLO = lire(OSLO, LIEUX[I_OSLO]);
constexpr AnalyseRiche R_REYKJAVIK = lire(REYKJAVIK, LIEUX[I_REYKJAVIK]);
constexpr AnalyseRiche R_HELSINKI = lire(HELSINKI, LIEUX[I_HELSINKI]);
constexpr AnalyseRiche R_TROMSO = lire(TROMSO, LIEUX[I_TROMSO]);
constexpr AnalyseRiche R_LONGYEARBYEN = lire(LONGYEARBYEN, LIEUX[I_LONGYEARBYEN]);
constexpr AnalyseRiche R_LONDRES = lire(LONDRES, LIEUX[I_LONDRES]);
constexpr AnalyseRiche R_TOKYO = lire(TOKYO, LIEUX[I_TOKYO]);
constexpr AnalyseRiche R_SYDNEY = lire(SYDNEY, LIEUX[I_SYDNEY]);

// ---- réponses réelles -----------------------------------------------------------------------------------------------------

constexpr int oslo_identique_a_python() { return comparer(R_OSLO, ATT_OSLO); }
constexpr int reykjavik_identique_a_python() { return comparer(R_REYKJAVIK, ATT_REYKJAVIK); }
constexpr int helsinki_identique_a_python() { return comparer(R_HELSINKI, ATT_HELSINKI); }
constexpr int tromso_identique_a_python() { return comparer(R_TROMSO, ATT_TROMSO); }
constexpr int longyearbyen_nuit_polaire_identique_a_python() { return comparer(R_LONGYEARBYEN, ATT_LONGYEARBYEN); }
constexpr int londres_identique_a_python() { return comparer(R_LONDRES, ATT_LONDRES); }
constexpr int tokyo_identique_a_python() { return comparer(R_TOKYO, ATT_TOKYO); }
constexpr int sydney_identique_a_python() { return comparer(R_SYDNEY, ATT_SYDNEY); }

constexpr int ce_que_la_source_ne_donne_pas_reste_inconnu() {
    for (const AnalyseRiche *a :
         {&R_OSLO, &R_REYKJAVIK, &R_HELSINKI, &R_TROMSO, &R_LONGYEARBYEN, &R_LONDRES, &R_TOKYO, &R_SYDNEY}) {
        const Meteo &m = a->meteo;
        VERIFIER(m.releve.nb_jours >= 6);
        VERIFIER(m.riche.precip_courante_dixiemes == INCONNU_DIX);
        VERIFIER(m.riche.nb_quarts == 0);
        for (std::size_t i = 0; i < m.releve.nb_jours; ++i) {
            VERIFIER(m.releve.jours[i].uv_max_dixiemes == INCONNU_DIX); // l'UV « ciel dégagé » n'est pas l'UV prévu
            VERIFIER(m.riche.jours[i].neige_dixiemes_cm == INCONNU_DIX);
            VERIFIER(m.riche.jours[i].ensoleillement_min == INCONNU_ENTIER);
            VERIFIER(m.riche.jours[i].direction_dominante_deg == INCONNU_ENTIER);
        }
        for (std::size_t i = 0; i < m.riche.nb_heures; ++i)
            VERIFIER(m.riche.heures[i].uv_dixiemes == INCONNU_DIX);
    }
    return 0;
}

constexpr int les_rafales_et_la_probabilite_n_existent_que_dans_la_zone_nordique() {
    // Relevé du 07/11/2026 : Oslo, Helsinki, Tromsø ont les deux ; Longyearbyen les rafales seulement ; Reykjavik, Londres, Tokyo,
    // Sydney aucune.
    for (const AnalyseRiche *a : {&R_OSLO, &R_HELSINKI, &R_TROMSO, &R_LONGYEARBYEN})
        VERIFIER(a->meteo.releve.rafales_dixiemes_kmh != INCONNU_DIX);
    for (const AnalyseRiche *a : {&R_OSLO, &R_HELSINKI, &R_TROMSO})
        VERIFIER(a->meteo.releve.proba_precip_pct != INCONNU_ENTIER);
    VERIFIER(R_LONGYEARBYEN.meteo.releve.proba_precip_pct == INCONNU_ENTIER);
    for (const AnalyseRiche *a : {&R_REYKJAVIK, &R_LONDRES, &R_TOKYO, &R_SYDNEY}) {
        VERIFIER(a->meteo.releve.rafales_dixiemes_kmh == INCONNU_DIX);
        VERIFIER(a->meteo.releve.proba_precip_pct == INCONNU_ENTIER);
        VERIFIER(a->meteo.riche.heures[0].rafales_dixiemes_kmh == INCONNU_DIX);
        VERIFIER(a->meteo.riche.heures[0].proba_pct == INCONNU_ENTIER);
        VERIFIER(a->meteo.releve.jours[0].proba_precip_max == INCONNU_ENTIER);
        VERIFIER(a->meteo.riche.jours[0].rafales_max_dixiemes_kmh == INCONNU_DIX);
    }
    // La pression, elle, est toujours donnée.
    VERIFIER(R_TOKYO.meteo.releve.pression_dixiemes_hpa != INCONNU_DIX);
    return 0;
}

constexpr int la_nuit_polaire_de_longyearbyen_a_lever_et_coucher_a_minuit() {
    const Releve &r = R_LONGYEARBYEN.meteo.releve;
    VERIFIER(r.nb_jours >= 6);
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        VERIFIER(r.jours[i].lever_s == r.jours[i].date_s && r.jours[i].coucher_s == r.jours[i].date_s);
        VERIFIER(r.jours[i].sans_soleil());
    }
    VERIFIER(!r.jour); // 07/11/2026 : le soleil ne se lève pas à Longyearbyen
    return 0;
}

constexpr int le_fuseau_et_le_decalage_viennent_de_l_appelant() {
    // La réponse ne donne pas de fuseau : il est celui de la ville (Sydney : UTC+11 en heure d'été, Londres : UTC+0).
    const Releve &sydney = R_SYDNEY.meteo.releve;
    VERIFIER(sydney.decalage_utc_s == 39600);
    VERIFIER(sydney.fuseau[0] == 'A' && sydney.fuseau[9] == '/' && sydney.fuseau[10] == 'S');
    VERIFIER(R_LONDRES.meteo.releve.decalage_utc_s == 0);
    VERIFIER(R_OSLO.meteo.releve.decalage_utc_s == 3600);
    return 0;
}

constexpr int les_sept_jours_sont_locaux() {
    // Sydney : le jour 0 commence à minuit local (UTC+11) = 13:00 UTC la veille.
    const Releve &r = R_SYDNEY.meteo.releve;
    VERIFIER(r.nb_jours == 7);
    for (std::size_t i = 0; i < 7; ++i)
        VERIFIER(r.jours[i].date_s % 3600 == 0 && (i == 0 || r.jours[i].date_s - r.jours[i - 1].date_s == 86400));
    VERIFIER(r.jours[0].date_s == 1793970000); // 2026-11-07 00:00 à Sydney
    return 0;
}

// ---- symboles ---------------------------------------------------------------------------------------------------------------

struct Symbole {
    sv nom;
    int code;
};
constexpr Symbole OFFICIELS[] = {
#include "donnees/attendu_symboles_metno.inc"
};

constexpr int les_41_symboles_officiels_et_leurs_variantes() {
    VERIFIER(sizeof OFFICIELS / sizeof OFFICIELS[0] == 41 * 4);
    for (const Symbole &s : OFFICIELS) {
        VERIFIER(s.code >= 0);
        VERIFIER(metno::code_projet(s.nom) == s.code);
    }
    return 0;
}

constexpr int symboles_inconnus_ou_mal_ecrits() {
    VERIFIER(metno::code_projet("") == -1);
    VERIFIER(metno::code_projet("Clearsky") == -1);
    VERIFIER(metno::code_projet("clearsky_") == -1);
    VERIFIER(metno::code_projet("clearsky_dusk") == -1);
    VERIFIER(metno::code_projet("clearsky_night_day") == -1);
    VERIFIER(metno::code_projet("rainy") == -1);
    VERIFIER(metno::code_projet("clearsky_night") == 0 && metno::code_projet("clearsky") == 0);
    VERIFIER(metno::code_projet("heavyrainshowersandthunder_day") == 97);
    VERIFIER(metno::code_projet("lightssleetshowersandthunder_night") == 95); // orthographe de la légende officielle
    VERIFIER(metno::code_projet("lightsleetshowersandthunder_night") == 95);  // et la version corrigée
    VERIFIER(metno::sans_variante("partlycloudy_polartwilight") == "partlycloudy");
    VERIFIER(metno::sans_variante("partlycloudy") == "partlycloudy");
    VERIFIER(metno::sans_variante("_day") == "");
    return 0;
}

// ---- ce que la source ne donne pas traverse le cache et le contrat de l'écran sans devenir un zéro ----------------------------

constexpr bool est_nan(float x) noexcept { return __builtin_isnan(x); }

constexpr int le_cache_garde_les_valeurs_inconnues() {
    for (const AnalyseRiche *a : {&R_OSLO, &R_REYKJAVIK, &R_LONGYEARBYEN, &R_SYDNEY}) {
        std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
        const std::size_t n = serialiser_cache_meteo(a->meteo, 5991273, 1074609, tampon, sizeof tampon);
        VERIFIER(n != 0 && n <= TAILLE_CACHE_RICHE_MAX);
        const LectureCacheMeteo l = deserialiser_cache_meteo(tampon, n);
        VERIFIER(l.ok());
        const Plat avant = aplatir(a->meteo);
        const Plat apres = aplatir(l.meteo);
        VERIFIER(!avant.trop && avant.n == apres.n);
        for (std::size_t i = 0; i < avant.n; ++i)
            VERIFIER(avant.v[i] == apres.v[i]);
    }
    return 0;
}

constexpr MeteoDetail convertir(const Meteo &m, const HistoPression &histo = HistoPression{}) {
    MeteoDetail d{};
    remplir_detail(m, 1, m.releve.recu_a_s, 0, histo, d);
    return d;
}

constexpr int le_contrat_de_l_ecran_recoit_nan_et_moins_un_pour_ce_que_la_source_ne_donne_pas() {
    const MeteoDetail d = convertir(R_OSLO.meteo);
    VERIFIER(d.valide && !d.perime);
    VERIFIER(est_nan(
        d.courant.precipitation_mm)); // « le dernier quart d'heure » : MET Norway ne le donne pas, ce n'est pas un zéro
    VERIFIER(!est_nan(d.courant.ressentie_c)); // MET Norway donne apparent_air_temperature
    VERIFIER(!est_nan(d.courant.temperature_c) && !est_nan(d.courant.pression_hpa) && !est_nan(d.courant.vent_kmh));
    VERIFIER(!est_nan(d.courant.rafales_kmh) &&
             d.courant.proba_precip_pct != METEO_PROBA_INCONNUE); // Oslo : zone nordique
    VERIFIER(d.nb_heures == 24 && d.nb_jours >= 6);
    for (int i = 0; i < d.nb_heures; ++i)
        VERIFIER(est_nan(d.heures[i].uv) && !est_nan(d.heures[i].ressentie_c) && !est_nan(d.heures[i].temperature_c));
    for (int i = 0; i < d.nb_jours; ++i) {
        const MeteoJour &j = d.jours[i];
        VERIFIER(est_nan(j.uv_max) && est_nan(j.ensoleillement_h) && est_nan(j.neige_cm));
        VERIFIER(j.direction_vent_dominante_deg == -1);
        VERIFIER(!est_nan(j.min_c) && !est_nan(j.max_c) && !est_nan(j.duree_jour_h));
    }
    // le cumul d'un jour n'existe que si des pas entiers couvrent ses 24 heures (docs/mesures/fournisseurs-meteo.md) : le jour 0
    // a commencé avant la réponse (inconnu), le jour 1 est couvert heure par heure (zéro mesuré, pas inconnu), les suivants sont
    // en pas de 6 heures qui ne tombent pas sur le minuit local d'Oslo (inconnus)
    VERIFIER(est_nan(d.jours[0].precipitation_mm));
    VERIFIER(!est_nan(d.jours[1].precipitation_mm) && d.jours[1].precipitation_mm == 0.0F);
    VERIFIER(est_nan(d.jours[2].precipitation_mm));
    VERIFIER(d.jours[0].heures_precipitation == -1 && d.jours[1].heures_precipitation == 0 &&
             d.jours[2].heures_precipitation == -1);
    VERIFIER(!d.pluie.connue && d.nb_quarts == 0);
    // hors de la zone nordique : ni rafales ni probabilité
    const MeteoDetail r = convertir(R_REYKJAVIK.meteo);
    VERIFIER(est_nan(r.courant.rafales_kmh) && r.courant.proba_precip_pct == METEO_PROBA_INCONNUE);
    VERIFIER(est_nan(r.heures[0].rafales_kmh) && r.heures[0].proba_precip_pct == METEO_PROBA_INCONNUE);
    VERIFIER(est_nan(r.jours[0].rafales_max_kmh) && r.jours[0].proba_precip_pct == METEO_PROBA_INCONNUE);
    return 0;
}

constexpr int la_tendance_de_pression_vient_de_la_pression_donnee_par_la_source() {
    // MET Norway donne la pression : l'historique de l'appareil fait une tendance, comme pour Open-Meteo
    HistoPression h;
    tendance_noter(h, R_OSLO.meteo.releve.instant_s - 3 * 3600, R_OSLO.meteo.releve.pression_dixiemes_hpa - 25);
    tendance_noter(h, R_OSLO.meteo.releve.instant_s, R_OSLO.meteo.releve.pression_dixiemes_hpa);
    VERIFIER(convertir(R_OSLO.meteo, h).courant.tendance_pression_3h_hpa == 2.5F);
    return 0;
}

// ---- les pays que la table des sources annonce sont ceux dont une réponse RÉELLE est rejouée ici ---------------------------

struct PaysVu {
    const AnalyseRiche *reponse;
    sv pays;
};

constexpr int les_pays_annonces_par_la_table_sont_ceux_des_captures() {
    const fournisseurs::Fournisseur *f = fournisseurs::trouver("metnorway");
    VERIFIER(f != nullptr && f->famille == fournisseurs::Famille::MetNo);
    const PaysVu vus[] = {
        {&R_OSLO, "NO"}, {&R_TROMSO, "NO"}, {&R_HELSINKI, "FI"}, {&R_REYKJAVIK, "IS"}, {&R_LONGYEARBYEN, "SJ"}};
    for (const PaysVu &v : vus) {
        VERIFIER(v.reponse->ok());                            // la réponse réelle de ce pays est lue sans refus
        VERIFIER(fournisseurs::concerne_le_pays(*f, v.pays)); // et la table annonce ce pays
    }
    for (std::size_t i = 0; i < f->pays.size();
         i += 3) { // chaque pays de la table a sa capture : jamais un code deviné
        bool vu = false;
        for (const PaysVu &v : vus)
            vu = vu || v.pays == f->pays.substr(i, 2);
        VERIFIER(vu);
    }
    // Londres, Tokyo et Sydney sont lues aussi (MET Norway couvre le monde) mais ne sont pas des pays où ce service est « national »
    for (const sv hors : {sv("GB"), sv("JP"), sv("AU"), sv("SE"), sv("DK"), sv("EE")})
        VERIFIER(!fournisseurs::concerne_le_pays(*f, hors));
    VERIFIER(R_LONDRES.ok() && R_TOKYO.ok() && R_SYDNEY.ok());
    return 0;
}

// ---- petites réponses écrites à la main ---------------------------------------------------------------------------------------

using Doc = Texte<4096>;

// Une réponse de deux instants (12:00 et 13:00 UTC le 07/11/2026). `detail` : le contenu de instant.details du premier instant.
constexpr Doc mini(sv detail, sv symbole = "cloudy", sv t2 = "2026-11-07T13:00:00Z") {
    Doc d;
    d +=
        R"({"type":"Feature","properties":{"timeseries":[{"time":"2026-11-07T12:00:00Z","data":{"instant":{"details":{)";
    d += detail;
    d += R"(}},"next_1_hours":{"summary":{"symbol_code":")";
    d += symbole;
    d += R"("},"details":{"precipitation_amount":0.4}}}},{"time":")";
    d += t2;
    d += R"(","data":{"instant":{"details":{"air_temperature":4.0}},"next_1_hours":{"summary":{"symbol_code":"rain"},)"
         R"("details":{"precipitation_amount":1.2}}}}]}})";
    return d;
}

constexpr sv BASE = R"("air_temperature":3.0,"relative_humidity":80,"wind_speed":3.5,"wind_from_direction":200,)"
                    R"("air_pressure_at_sea_level":1013.24,"cloud_area_fraction":50.0)";

constexpr Doc base_avec_vent(sv vent, sv rafales = "") {
    Doc d;
    d += R"("air_temperature":3.0,"relative_humidity":80,"wind_speed":)";
    d += vent;
    if (!rafales.empty()) {
        d += R"(,"wind_speed_of_gust":)";
        d += rafales;
    }
    d += R"(,"wind_from_direction":200,"air_pressure_at_sea_level":1013.24,"cloud_area_fraction":50.0)";
    return mini(d.vue());
}

constexpr AnalyseRiche lire_mini(const Doc &d, sv fuseau = "Europe/Oslo") {
    metno::Serie travail;
    return metno::analyser_metno(d.vue(), 1794052800 + 600, 5991273, 1074609, fuseau, travail);
}

constexpr int une_reponse_minimale_donne_le_courant_et_dit_pourquoi_le_reste_manque() {
    const AnalyseRiche a = lire_mini(mini(BASE));
    VERIFIER(a.ok());
    const Releve &r = a.meteo.releve;
    VERIFIER(r.instant_s == 1794052800 && r.temperature_dixiemes == 30 && r.code_wmo == 3);
    VERIFIER(r.humidite_pct == 80 && r.direction_vent_deg == 200 && r.pression_dixiemes_hpa == 10132);
    VERIFIER(a.meteo.riche.nuages_pct == 50);
    VERIFIER(r.ressentie_dixiemes == INCONNU_DIX); // pas de apparent_air_temperature : inconnue, jamais la température
    VERIFIER(r.rafales_dixiemes_kmh == INCONNU_DIX);
    VERIFIER(r.proba_precip_pct == INCONNU_ENTIER);
    VERIFIER(!a.diag_heures.ok() && a.meteo.riche.nb_heures == 0); // 24 heures consécutives : il n'y en a que deux
    VERIFIER(a.diag_heures.erreur == Erreur::TableauxIncoherents);
    VERIFIER(!a.diag_quarts.ok());
    VERIFIER(a.meteo.riche.nb_quarts == 0);
    return 0;
}

constexpr int le_vent_passe_de_m_s_a_km_h() {
    // 10,0 m/s = 36,0 km/h ; 0,9 m/s = 3,24 km/h -> 3,2 ; 0,0 m/s est un vent nul CONNU, pas un vent inconnu.
    VERIFIER(lire_mini(base_avec_vent("10.0", "15.5")).meteo.releve.vent_dixiemes_kmh == 360);
    VERIFIER(lire_mini(base_avec_vent("10.0", "15.5")).meteo.releve.rafales_dixiemes_kmh == 558);
    VERIFIER(lire_mini(base_avec_vent("0.9")).meteo.releve.vent_dixiemes_kmh == 32);
    VERIFIER(lire_mini(base_avec_vent("0.0")).meteo.releve.vent_dixiemes_kmh == 0);
    VERIFIER(lire_mini(base_avec_vent("0.0")).meteo.releve.rafales_dixiemes_kmh == INCONNU_DIX);
    VERIFIER(lire_mini(base_avec_vent("0.05")).meteo.releve.vent_dixiemes_kmh ==
             4); // 0,05 -> 0,1 m/s -> 0,36 -> 0,4 km/h
    VERIFIER(lire_mini(base_avec_vent("3.5", "null")).meteo.releve.rafales_dixiemes_kmh == INCONNU_DIX);
    return 0;
}

constexpr int un_vent_negatif_ou_enorme_est_refuse() {
    const AnalyseRiche a = lire_mini(base_avec_vent("-1.0"));
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::HorsPlage && a.diag.champ == "wind_speed");
    const AnalyseRiche b = lire_mini(base_avec_vent("9999.0"));
    VERIFIER(!b.ok() && b.diag.erreur == Erreur::HorsPlage);
    const AnalyseRiche c = lire_mini(base_avec_vent("\"fort\""));
    VERIFIER(!c.ok() && c.diag.erreur == Erreur::MauvaisType && c.diag.champ == "wind_speed");
    return 0;
}

constexpr int la_temperature_est_obligatoire() {
    const AnalyseRiche a = lire_mini(mini(R"("relative_humidity":80)"));
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::ChampAbsent && a.diag.champ == "air_temperature");
    const AnalyseRiche b = lire_mini(mini(R"("air_temperature":null)"));
    VERIFIER(!b.ok() && b.diag.champ == "air_temperature");
    return 0;
}

constexpr int le_symbole_du_courant_est_obligatoire_et_doit_etre_connu() {
    const AnalyseRiche a = lire_mini(mini(BASE, "mystery_day"));
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::ChampAbsent && a.diag.champ == "symbol_code");
    const AnalyseRiche b = lire_mini(mini(BASE, "rain_day"));
    VERIFIER(b.ok() && b.meteo.releve.code_wmo == 63);
    return 0;
}

constexpr int une_humidite_au_dela_de_100_est_refusee() {
    const AnalyseRiche a = lire_mini(mini(R"("air_temperature":3.0,"relative_humidity":140)"));
    VERIFIER(!a.ok());
    const AnalyseRiche b = lire_mini(mini(R"("air_temperature":3.0,"relative_humidity":-5)"));
    VERIFIER(!b.ok() && b.diag.champ == "relative_humidity"); // -5 n'est pas « inconnu » (-1 l'est) : réponse refusée
    // 306 % ne doit pas se retrouver à 50 % en tenant dans un octet (306 - 256)
    const AnalyseRiche c = lire_mini(mini(R"("air_temperature":3.0,"relative_humidity":306)"));
    VERIFIER(!c.ok() && c.diag.erreur == Erreur::HorsPlage && c.diag.champ == "relative_humidity");
    const AnalyseRiche d = lire_mini(mini(R"("air_temperature":3.0,"cloud_area_fraction":300)"));
    VERIFIER(!d.ok() && d.diag.champ == "cloud_area_fraction");
    return 0;
}

constexpr int des_instants_qui_ne_croissent_pas_sont_refuses() {
    const AnalyseRiche a = lire_mini(mini(BASE, "cloudy", "2026-11-07T11:00:00Z"));
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::TableauxIncoherents);
    const AnalyseRiche b = lire_mini(mini(BASE, "cloudy", "2026-11-07T12:00:00Z"));
    VERIFIER(!b.ok() && b.diag.erreur == Erreur::TableauxIncoherents);
    const AnalyseRiche c = lire_mini(mini(BASE, "cloudy", "demain"));
    VERIFIER(!c.ok() && c.diag.erreur == Erreur::MauvaisType);
    return 0;
}

constexpr int un_fuseau_inconnu_ou_un_texte_qui_n_est_pas_du_json_sont_refuses() {
    VERIFIER(!lire_mini(mini(BASE), "Mars/Olympus").ok());
    VERIFIER(lire_mini(mini(BASE), "Mars/Olympus").diag.champ == "fuseau");
    VERIFIER(!lire_mini(mini(BASE), "").ok());
    metno::Serie travail;
    const AnalyseRiche a = metno::analyser_metno("{\"properties\":", 1794052800, 0, 0, "Europe/Oslo", travail);
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::JsonInvalide);
    const AnalyseRiche b = metno::analyser_metno("[]", 1794052800, 0, 0, "Europe/Oslo", travail);
    VERIFIER(!b.ok() && b.diag.erreur == Erreur::JsonInvalide);
    const AnalyseRiche c = metno::analyser_metno("{\"properties\":{}}", 1794052800, 0, 0, "Europe/Oslo", travail);
    VERIFIER(!c.ok() && c.diag.erreur == Erreur::ChampAbsent && c.diag.champ == "properties.timeseries");
    const AnalyseRiche d =
        metno::analyser_metno("{\"properties\":{\"timeseries\":[]}}", 1794052800, 0, 0, "Europe/Oslo", travail);
    VERIFIER(!d.ok());
    const AnalyseRiche e =
        metno::analyser_metno("{\"properties\":{\"timeseries\":{}}}", 1794052800, 0, 0, "Europe/Oslo", travail);
    VERIFIER(!e.ok() && e.diag.erreur == Erreur::MauvaisType);
    return 0;
}

constexpr int une_reponse_trop_longue_est_tronquee_sans_deborder() {
    // Plus de ENTREES_MAX éléments : on lit les premiers, pas un octet au-delà du tableau.
    static_assert(metno::ENTREES_MAX == 128);
    Texte<1 << 16> d;
    d += R"({"properties":{"timeseries":[)";
    for (int i = 0; i < 200; ++i) {
        // « 2026-11-07T12:00:00Z » + i heures : 200 heures = 8 jours 8 heures, on reste en novembre.
        const int depuis_minuit = 12 + i;
        const int jour = 7 + depuis_minuit / 24;
        const int h = depuis_minuit % 24;
        const char c[] = {static_cast<char>('0' + jour / 10), static_cast<char>('0' + jour % 10), 'T',
                          static_cast<char>('0' + h / 10), static_cast<char>('0' + h % 10)};
        d += i == 0 ? R"({"time":"2026-11-)" : R"(,{"time":"2026-11-)";
        d += sv(c, 5);
        d +=
            R"(:00:00Z","data":{"instant":{"details":{"air_temperature":3.0}},"next_1_hours":{"summary":{"symbol_code":"cloudy"}}}})";
    }
    d += "]}}";
    VERIFIER(!d.trop);
    metno::Serie travail;
    const AnalyseRiche a = metno::analyser_metno(d.vue(), 1794052800 + 600, 5991273, 1074609, "Europe/Oslo", travail);
    VERIFIER(a.ok());
    VERIFIER(travail.n == metno::ENTREES_MAX);
    return 0;
}

// Trois instants d'une heure (12:00, 13:00 et 14:00 UTC le 07/11/2026), chacun avec le contenu de instant.details donné et un symbole.
constexpr Doc trois_instants(sv detail1, sv detail2, sv detail3) {
    Doc d;
    d += R"({"properties":{"timeseries":[{"time":"2026-11-07T12:00:00Z","data":{"instant":{"details":{)";
    d += detail1;
    d += R"(}},"next_1_hours":{"summary":{"symbol_code":"cloudy"},"details":{"precipitation_amount":0.4}}}},)"
         R"({"time":"2026-11-07T13:00:00Z","data":{"instant":{"details":{)";
    d += detail2;
    d += R"(}},"next_1_hours":{"summary":{"symbol_code":"cloudy"},"details":{"precipitation_amount":0.4}}}},)"
         R"({"time":"2026-11-07T14:00:00Z","data":{"instant":{"details":{)";
    d += detail3;
    d += R"(}},"next_1_hours":{"summary":{"symbol_code":"cloudy"},"details":{"precipitation_amount":0.4}}}}]}})";
    return d;
}

constexpr int les_extremes_d_un_jour_gardent_un_zero_et_ignorent_l_inconnu() {
    // 12:00 : 5,0 °C, ressentie 4,0, vent 2 m/s, rafales 8 m/s. 13:00 : 0,0 °C (un VRAI zéro), ni ressentie, ni vent, ni rafales.
    // 14:00 : 2,0 °C, ressentie 1,0, vent 5 m/s, rafales 6 m/s. L'inconnu ne doit effacer aucun extrême, le zéro doit compter.
    const Doc d = trois_instants(
        R"("air_temperature":5.0,"apparent_air_temperature":4.0,"wind_speed":2.0,"wind_speed_of_gust":8.0)",
        R"("air_temperature":0.0)",
        R"("air_temperature":2.0,"apparent_air_temperature":1.0,"wind_speed":5.0,"wind_speed_of_gust":6.0)");
    const AnalyseRiche a = lire_mini(d);
    VERIFIER(a.ok() && a.diag_jours.ok() && a.meteo.releve.nb_jours >= 1);
    const Jour &j = a.meteo.releve.jours[0];
    const JourRiche &jr = a.meteo.riche.jours[0];
    VERIFIER(j.min_dixiemes == 0 && j.max_dixiemes == 50); // le zéro est le minimum
    VERIFIER(jr.ressentie_min_dixiemes == 10 &&
             jr.ressentie_max_dixiemes == 40);    // l'instant sans ressentie ne l'efface pas
    VERIFIER(jr.vent_max_dixiemes_kmh == 180);    // 5 m/s = 18 km/h ; l'instant sans vent ne l'efface pas
    VERIFIER(jr.rafales_max_dixiemes_kmh == 288); // 8 m/s = 28,8 km/h
    // Un jour dont aucun instant ne donne de ressentie, de vent ni de rafales : inconnus, pas zéro.
    const AnalyseRiche b =
        lire_mini(trois_instants(R"("air_temperature":5.0)", R"("air_temperature":0.0)", R"("air_temperature":2.0)"));
    VERIFIER(b.ok() && b.diag_jours.ok());
    VERIFIER(b.meteo.riche.jours[0].ressentie_min_dixiemes == INCONNU_DIX &&
             b.meteo.riche.jours[0].ressentie_max_dixiemes == INCONNU_DIX);
    VERIFIER(b.meteo.riche.jours[0].vent_max_dixiemes_kmh == INCONNU_DIX &&
             b.meteo.riche.jours[0].rafales_max_dixiemes_kmh == INCONNU_DIX);
    VERIFIER(b.meteo.releve.jours[0].min_dixiemes == 0);
    return 0;
}

constexpr int un_nom_de_fuseau_qui_ne_tient_pas_dans_le_releve_est_refuse() {
    Releve r;
    char nom[FUSEAU_MAX + 1] = {};
    for (std::size_t i = 0; i < FUSEAU_MAX; ++i)
        nom[i] = 'a';
    VERIFIER(!natif::copier_fuseau(r, sv(nom, FUSEAU_MAX)));     // 48 caractères : plus de place pour le zéro final
    VERIFIER(!natif::copier_fuseau(r, sv(nom, FUSEAU_MAX + 1))); // 49
    VERIFIER(!natif::copier_fuseau(r, sv()));                    // vide
    VERIFIER(natif::copier_fuseau(r, sv(nom, FUSEAU_MAX - 1)));  // 47 : tient, zéro final compris
    VERIFIER(r.fuseau[0] == 'a' && r.fuseau[FUSEAU_MAX - 2] == 'a' && r.fuseau[FUSEAU_MAX - 1] == '\0');
    return 0;
}

// Une série d'un élément par heure à partir de 12:00 UTC le 07/11/2026 ; l'élément `mauvais` (s'il est >= 0) a le symbole `symbole`.
constexpr Texte<16384> serie_horaire(int n, int mauvais, sv symbole) {
    Texte<16384> d;
    d += R"({"properties":{"timeseries":[)";
    for (int i = 0; i < n; ++i) {
        const int depuis_minuit = 12 + i;
        const int jour = 7 + depuis_minuit / 24;
        const int h = depuis_minuit % 24;
        const char c[] = {static_cast<char>('0' + jour / 10), static_cast<char>('0' + jour % 10), 'T',
                          static_cast<char>('0' + h / 10), static_cast<char>('0' + h % 10)};
        d += i == 0 ? R"({"time":"2026-11-)" : R"(,{"time":"2026-11-)";
        d += sv(c, 5);
        d +=
            R"(:00:00Z","data":{"instant":{"details":{"air_temperature":3.0}},"next_1_hours":{"summary":{"symbol_code":")";
        d += i == mauvais ? symbole : sv("cloudy");
        d += R"("}}}})";
    }
    d += "]}}";
    return d;
}

constexpr int vingt_quatre_heures_de_suite_avec_chacune_un_symbole_connu() {
    metno::Serie travail;
    const Texte<16384> bonne = serie_horaire(30, -1, "");
    VERIFIER(!bonne.trop);
    const AnalyseRiche a =
        metno::analyser_metno(bonne.vue(), 1794052800 + 600, 5991273, 1074609, "Europe/Oslo", travail);
    VERIFIER(a.ok() && a.diag_heures.ok() && a.meteo.riche.nb_heures == 24);
    // Un symbole inconnu parmi les 24 heures (la 2e, la 12e, la 24e) : la section des heures est refusée, pas devinée ; le courant reste.
    for (const int mauvais : {1, 11, 23}) {
        const Texte<16384> d = serie_horaire(30, mauvais, "mystery");
        VERIFIER(!d.trop);
        const AnalyseRiche b =
            metno::analyser_metno(d.vue(), 1794052800 + 600, 5991273, 1074609, "Europe/Oslo", travail);
        VERIFIER(b.ok() && !b.diag_heures.ok() && b.meteo.riche.nb_heures == 0);
        // refusé pour ce qu'il est (un symbole inconnu), pas par la vérification de plage qui viendrait ensuite sur un code -1
        VERIFIER(b.diag_heures.erreur == Erreur::TableauxIncoherents && b.diag_heures.champ == "hourly");
    }
    // Hors des 24 heures (la 25e) : sans effet sur elles.
    const Texte<16384> hors = serie_horaire(30, 24, "mystery");
    const AnalyseRiche c =
        metno::analyser_metno(hors.vue(), 1794052800 + 600, 5991273, 1074609, "Europe/Oslo", travail);
    VERIFIER(c.ok() && c.diag_heures.ok() && c.meteo.riche.nb_heures == 24);
    return 0;
}

constexpr int un_jour_sans_symbole_n_est_pas_garde() {
    // Le courant a son symbole ; le lendemain n'a qu'une température (aucune période, donc aucun symbole) : la liste des jours
    // s'arrête au dernier jour complet au lieu de garder un jour au code météo inconnu.
    constexpr sv doc =
        R"({"properties":{"timeseries":[{"time":"2026-11-07T12:00:00Z","data":{"instant":{"details":{"air_temperature":3.0}},)"
        R"("next_1_hours":{"summary":{"symbol_code":"cloudy"}}}},)"
        R"({"time":"2026-11-08T12:00:00Z","data":{"instant":{"details":{"air_temperature":4.0}}}}]}})";
    metno::Serie travail;
    const AnalyseRiche a = metno::analyser_metno(doc, 1794052800 + 600, 5991273, 1074609, "Europe/Oslo", travail);
    VERIFIER(a.ok() && a.diag_jours.ok());
    VERIFIER(a.meteo.releve.nb_jours == 1 && a.meteo.releve.jours[0].code_wmo == 3);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(oslo_identique_a_python)                                                                                         \
    X(reykjavik_identique_a_python)                                                                                    \
    X(helsinki_identique_a_python)                                                                                     \
    X(tromso_identique_a_python)                                                                                       \
    X(longyearbyen_nuit_polaire_identique_a_python)                                                                    \
    X(londres_identique_a_python)                                                                                      \
    X(tokyo_identique_a_python)                                                                                        \
    X(sydney_identique_a_python)                                                                                       \
    X(ce_que_la_source_ne_donne_pas_reste_inconnu)                                                                     \
    X(les_rafales_et_la_probabilite_n_existent_que_dans_la_zone_nordique)                                              \
    X(la_nuit_polaire_de_longyearbyen_a_lever_et_coucher_a_minuit)                                                     \
    X(le_fuseau_et_le_decalage_viennent_de_l_appelant)                                                                 \
    X(les_sept_jours_sont_locaux)                                                                                      \
    X(les_41_symboles_officiels_et_leurs_variantes)                                                                    \
    X(symboles_inconnus_ou_mal_ecrits)                                                                                 \
    X(les_pays_annonces_par_la_table_sont_ceux_des_captures)                                                           \
    X(le_cache_garde_les_valeurs_inconnues)                                                                            \
    X(le_contrat_de_l_ecran_recoit_nan_et_moins_un_pour_ce_que_la_source_ne_donne_pas)                                 \
    X(la_tendance_de_pression_vient_de_la_pression_donnee_par_la_source)                                               \
    X(une_reponse_minimale_donne_le_courant_et_dit_pourquoi_le_reste_manque)                                           \
    X(le_vent_passe_de_m_s_a_km_h)                                                                                     \
    X(un_vent_negatif_ou_enorme_est_refuse)                                                                            \
    X(la_temperature_est_obligatoire)                                                                                  \
    X(le_symbole_du_courant_est_obligatoire_et_doit_etre_connu)                                                        \
    X(une_humidite_au_dela_de_100_est_refusee)                                                                         \
    X(des_instants_qui_ne_croissent_pas_sont_refuses)                                                                  \
    X(un_fuseau_inconnu_ou_un_texte_qui_n_est_pas_du_json_sont_refuses)                                                \
    X(une_reponse_trop_longue_est_tronquee_sans_deborder)                                                              \
    X(les_extremes_d_un_jour_gardent_un_zero_et_ignorent_l_inconnu)                                                    \
    X(un_nom_de_fuseau_qui_ne_tient_pas_dans_le_releve_est_refuse)                                                     \
    X(vingt_quatre_heures_de_suite_avec_chacune_un_symbole_connu)                                                      \
    X(un_jour_sans_symbole_n_est_pas_garde)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
