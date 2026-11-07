// Tests du noyau pur « surf » : la lecture des deux réponses d'Open-Meteo (surf_donnees.hpp) sur des réponses FABRIQUÉES, courtes, dont
// chaque valeur est choisie : null, bornes, arrondis, unités, nombre d'heures, fuseau. Rapides : évalués à la compilation
// (static_assert) ET à l'exécution (voir mini.hpp). Les réponses RÉELLES sont dans test_surf_reponses.cpp.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/surf_donnees.hpp"
#include "mini.hpp"
#include "surf_outils.hpp"

using namespace ecran::surf;
using namespace surf_test;

namespace {

// ---- ce qui n'est pas du JSON, ce qui est une erreur de l'API ---------------------------------------------------------------

constexpr int une_reponse_qui_n_est_pas_du_json_ou_pas_un_objet_est_illisible() {
    VERIFIER(refuse("", Erreur::JsonInvalide, ""));
    VERIFIER(refuse("pas du json", Erreur::JsonInvalide, ""));
    VERIFIER(refuse("[1,2,3]", Erreur::JsonInvalide, ""));
    VERIFIER(refuse("{\"utc_offset_seconds\":0,", Erreur::JsonInvalide, ""));
    VERIFIER(refuse_vent("", Erreur::JsonInvalide, "") && refuse_vent("[]", Erreur::JsonInvalide, ""));
    const Json coupee = reponse_minimale(10);
    VERIFIER(refuse(coupee.vue().substr(0, coupee.n - 3), Erreur::JsonInvalide, "")); // coupée en route
    return 0;
}

constexpr int une_reponse_d_erreur_de_l_api_est_reconnue() {
    constexpr sv E = R"({"error":true,"reason":"Latitude must be in range of -90 to 90°. Given: 91.0."})";
    VERIFIER(refuse(E, Erreur::ReponseRefusee, "error"));
    VERIFIER(refuse_vent(E, Erreur::ReponseRefusee, "error"));
    // "error":false n'est pas une erreur : la réponse est lue (ici, faute de fuseau, refusée pour une autre cause)
    VERIFIER(refuse(R"({"error":false,"hourly":{"time":[1794031200]}})", Erreur::ChampAbsent, "utc_offset_seconds"));
    VERIFIER(analyser_marine(R"({"error":false,"utc_offset_seconds":0,"hourly":{"time":[1794031200]}})").ok());
    return 0;
}

// ---- fuseau et heures ----------------------------------------------------------------------------------------------------

constexpr int le_fuseau_horaire_doit_etre_present_entier_et_plausible() {
    VERIFIER(analyser_marine(reponse_minimale(3, 3600, 0).vue()).marine.decalage_utc_s == 0);
    VERIFIER(analyser_marine(reponse_minimale(3, 3600, 20700).vue()).marine.decalage_utc_s == 20700); // Népal : +5 h 45
    VERIFIER(analyser_marine(reponse_minimale(3, 3600, -34200).vue()).marine.decalage_utc_s ==
             -34200); // Marquises : -9 h 30
    VERIFIER(analyser_marine(reponse_minimale(3, 3600, 50400).vue()).ok() &&
             analyser_marine(reponse_minimale(3, 3600, -43200).vue()).ok());
    VERIFIER(refuse(reponse_minimale(3, 3600, 50401).vue(), Erreur::HorsPlage, "utc_offset_seconds"));
    VERIFIER(refuse(reponse_minimale(3, 3600, -43201).vue(), Erreur::HorsPlage, "utc_offset_seconds"));
    VERIFIER(refuse(R"({"hourly":{"time":[1794031200]}})", Erreur::ChampAbsent, "utc_offset_seconds"));
    VERIFIER(refuse(R"({"utc_offset_seconds":null,"hourly":{"time":[1794031200]}})", Erreur::ChampAbsent,
                    "utc_offset_seconds"));
    VERIFIER(refuse(R"({"utc_offset_seconds":"0","hourly":{"time":[1794031200]}})", Erreur::MauvaisType,
                    "utc_offset_seconds"));
    VERIFIER(refuse_vent(R"({"hourly":{"time":[1794031200]}})", Erreur::ChampAbsent, "utc_offset_seconds"));
    return 0;
}

constexpr int la_liste_des_heures_doit_exister_etre_faite_d_entiers_plausibles() {
    VERIFIER(refuse(R"({"utc_offset_seconds":0})", Erreur::ChampAbsent, "hourly"));
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":5})", Erreur::MauvaisType, "hourly"));
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{}})", Erreur::ChampAbsent, "hourly.time"));
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{"time":5}})", Erreur::MauvaisType, "hourly.time"));
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{"time":["x"]}})", Erreur::MauvaisType, "hourly.time"));
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{"time":[10]}})", Erreur::HorsPlage, "hourly.time"));
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{"time":[4200000000]}})", Erreur::HorsPlage, "hourly.time"));
    VERIFIER(analyser_marine(R"({"utc_offset_seconds":0,"hourly":{"time":[946684800]}})").ok()); // 2000-01-01 00:00 UTC
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{"time":[946684799]}})", Erreur::HorsPlage, "hourly.time"));
    VERIFIER(
        analyser_marine(R"({"utc_offset_seconds":0,"hourly":{"time":[4102444800]}})").ok()); // 2100-01-01 00:00 UTC
    VERIFIER(refuse(R"({"utc_offset_seconds":0,"hourly":{"time":[4102444801]}})", Erreur::HorsPlage, "hourly.time"));
    return 0;
}

constexpr int une_reponse_minimale_est_lue_et_toutes_les_variables_restent_absentes() {
    const Json j = reponse_minimale(3, 3600, -34200);
    const AnalyseMarine a = analyser_marine(j.vue());
    VERIFIER(a.ok() && a.marine.nb == 3 && a.marine.decalage_utc_s == -34200);
    VERIFIER(!a.marine.a_vagues && !a.marine.a_houle && !a.marine.a_maree && !a.marine.a_temperature);
    VERIFIER(a.marine.heures[2].debut_s == 1794031200LL + 7200 && a.marine.heures[2].vagues_cm == ABSENT);
    const AnalyseVent v = analyser_vent(j.vue());
    VERIFIER(v.ok() && v.vent.nb == 3 && v.vent.heures[0].vitesse_dixiemes_kmh == ABSENT);
    const Json w = reponse_minimale(1, 3600, 0, true);
    const AnalyseMarine b = analyser_marine(w.vue());
    VERIFIER(b.ok() && b.marine.nb == 1 && b.marine.a_vagues && b.marine.heures[0].vagues_cm == 50);
    return 0;
}

constexpr int soixante_heures_passent_soixante_et_une_sont_refusees_pas_tronquees() {
    VERIFIER(HEURES_MAX == 60);
    VERIFIER(analyser_marine(reponse_minimale(60, 3600, 0, true).vue()).ok());
    VERIFIER(analyser_marine(reponse_minimale(60, 3600, 0, true).vue()).marine.nb == 60);
    VERIFIER(refuse(reponse_minimale(61, 3600, 0, true).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    VERIFIER(analyser_vent(reponse_minimale(60).vue()).ok() && analyser_vent(reponse_minimale(60).vue()).vent.nb == 60);
    VERIFIER(refuse_vent(reponse_minimale(61).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    VERIFIER(refuse(reponse_minimale(0).vue(), Erreur::TableauxIncoherents, "hourly.time")); // aucune heure
    return 0;
}

constexpr int des_heures_qui_ne_se_suivent_pas_d_une_heure_sont_refusees() {
    VERIFIER(refuse(reponse_minimale(5, 1800).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    VERIFIER(refuse(reponse_minimale(5, 7200).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    VERIFIER(refuse(reponse_minimale(5, 0).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    VERIFIER(refuse(reponse_minimale(5, -3600).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    VERIFIER(analyser_marine(reponse_minimale(1, 7200).vue()).ok()); // une seule heure : pas d'intervalle à contrôler
    VERIFIER(refuse_vent(reponse_minimale(5, 1800).vue(), Erreur::TableauxIncoherents, "hourly.time"));
    return 0;
}

// ---- les valeurs : null, bornes, arrondis -----------------------------------------------------------------------------------

constexpr int un_null_est_pas_de_donnee_jamais_zero() {
    const Json j = reponse_avec(3, R"("wave_height":[0.5,null,0.0],"swell_wave_height":[null,null,null])");
    const AnalyseMarine a = analyser_marine(j.vue());
    VERIFIER(a.ok() && a.marine.a_vagues &&
             !a.marine.a_houle); // des null partout = pas de houle ; 0,0 m = une mer plate mesurée
    VERIFIER(a.marine.heures[0].vagues_cm == 50 && a.marine.heures[1].vagues_cm == ABSENT &&
             a.marine.heures[2].vagues_cm == 0);
    VERIFIER(a.marine.heures[0].houle_cm == ABSENT && a.marine.heures[1].houle_cm == ABSENT);
    // une variable annoncée `null` en bloc (au lieu d'un tableau) est une variable absente
    const AnalyseMarine b = analyser_marine(reponse_avec(2, R"("wave_height":null)").vue());
    VERIFIER(b.ok() && !b.marine.a_vagues && b.marine.heures[0].vagues_cm == ABSENT);
    return 0;
}

constexpr int la_hauteur_est_lue_en_centimetres_bornes_0_et_30_m() {
    const AnalyseMarine a =
        analyser_marine(reponse_avec(3, R"("wave_height":[0.0,30.0,1.23],"swell_wave_height":[0.01,29.99,5])").vue());
    VERIFIER(a.ok() && a.marine.heures[0].vagues_cm == 0 && a.marine.heures[1].vagues_cm == 3000 &&
             a.marine.heures[2].vagues_cm == 123);
    VERIFIER(a.marine.heures[0].houle_cm == 1 && a.marine.heures[1].houle_cm == 2999 &&
             a.marine.heures[2].houle_cm == 500);
    VERIFIER(HAUTEUR_VAGUES_MAX_CM == 3000);
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":[30.01,0.5])").vue(), Erreur::HorsPlage, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":[-0.01,0.5])").vue(), Erreur::HorsPlage, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("swell_wave_height":[0.5,31])").vue(), Erreur::HorsPlage,
                    "hourly.swell_wave_height"));
    VERIFIER(
        refuse(reponse_avec(2, R"("wind_wave_height":[-1,0])").vue(), Erreur::HorsPlage, "hourly.wind_wave_height"));
    return 0;
}

constexpr int les_arrondis_se_font_au_plus_proche_la_moitie_s_eloigne_de_zero() {
    const AnalyseMarine a = analyser_marine(
        reponse_avec(4, R"("wave_height":[0.004,0.005,1.234,1.235],"wave_period":[5.04,5.05,5.54,5.55])").vue());
    VERIFIER(a.ok());
    VERIFIER(a.marine.heures[0].vagues_cm == 0 && a.marine.heures[1].vagues_cm == 1 &&
             a.marine.heures[2].vagues_cm == 123 && a.marine.heures[3].vagues_cm == 124);
    VERIFIER(a.marine.heures[0].vagues_periode_ds == 50 && a.marine.heures[1].vagues_periode_ds == 51 &&
             a.marine.heures[2].vagues_periode_ds == 55 && a.marine.heures[3].vagues_periode_ds == 56);
    return 0;
}

constexpr int la_periode_est_lue_en_dixiemes_de_seconde_bornes_0_et_40_s() {
    const AnalyseMarine a = analyser_marine(
        reponse_avec(3, R"("wave_period":[0,40.0,13.4],"swell_wave_period":[8.1,12,0.1],"wind_wave_period":[2,3,4])")
            .vue());
    VERIFIER(a.ok() && a.marine.heures[0].vagues_periode_ds == 0 && a.marine.heures[1].vagues_periode_ds == 400 &&
             a.marine.heures[2].vagues_periode_ds == 134);
    VERIFIER(a.marine.heures[0].houle_periode_ds == 81 && a.marine.heures[1].houle_periode_ds == 120 &&
             a.marine.heures[2].houle_periode_ds == 1);
    VERIFIER(a.marine.heures[2].mer_vent_periode_ds == 40);
    VERIFIER(PERIODE_MAX_DS == 400);
    VERIFIER(refuse(reponse_avec(2, R"("wave_period":[40.1,5])").vue(), Erreur::HorsPlage, "hourly.wave_period"));
    VERIFIER(refuse(reponse_avec(2, R"("swell_wave_period":[-0.1,5])").vue(), Erreur::HorsPlage,
                    "hourly.swell_wave_period"));
    VERIFIER(
        refuse(reponse_avec(2, R"("wind_wave_period":[5,41])").vue(), Erreur::HorsPlage, "hourly.wind_wave_period"));
    return 0;
}

constexpr int les_directions_vont_de_0_a_360_et_360_degres_font_zero() {
    const AnalyseMarine a = analyser_marine(
        reponse_avec(
            4,
            R"("wave_direction":[0,359,360,270],"swell_wave_direction":[1,null,180,90],"wind_wave_direction":[360,0,359,1])")
            .vue());
    VERIFIER(a.ok());
    VERIFIER(a.marine.heures[0].vagues_direction_deg == 0 && a.marine.heures[1].vagues_direction_deg == 359 &&
             a.marine.heures[2].vagues_direction_deg == 0 && a.marine.heures[3].vagues_direction_deg == 270);
    VERIFIER(a.marine.heures[1].houle_direction_deg == ABSENT && a.marine.heures[2].houle_direction_deg == 180);
    VERIFIER(a.marine.heures[0].mer_vent_direction_deg == 0 && a.marine.heures[2].mer_vent_direction_deg == 359);
    VERIFIER(refuse(reponse_avec(2, R"("wave_direction":[361,0])").vue(), Erreur::HorsPlage, "hourly.wave_direction"));
    VERIFIER(refuse(reponse_avec(2, R"("swell_wave_direction":[0,-1])").vue(), Erreur::HorsPlage,
                    "hourly.swell_wave_direction"));
    VERIFIER(refuse(reponse_avec(2, R"("wind_wave_direction":[0,400])").vue(), Erreur::HorsPlage,
                    "hourly.wind_wave_direction"));
    return 0;
}

constexpr int la_temperature_de_la_mer_va_de_moins_5_a_45_degres() {
    const AnalyseMarine a =
        analyser_marine(reponse_avec(4, R"("sea_surface_temperature":[-5.0,45.0,17.26,null])").vue());
    VERIFIER(a.ok() && a.marine.a_temperature);
    VERIFIER(a.marine.heures[0].temperature_mer_dixiemes == -50 && a.marine.heures[1].temperature_mer_dixiemes == 450 &&
             a.marine.heures[2].temperature_mer_dixiemes == 173 &&
             a.marine.heures[3].temperature_mer_dixiemes == ABSENT);
    VERIFIER(refuse(reponse_avec(2, R"("sea_surface_temperature":[-5.1,0])").vue(), Erreur::HorsPlage,
                    "hourly.sea_surface_temperature"));
    VERIFIER(refuse(reponse_avec(2, R"("sea_surface_temperature":[0,45.1])").vue(), Erreur::HorsPlage,
                    "hourly.sea_surface_temperature"));
    return 0;
}

constexpr int le_niveau_de_la_mer_va_de_moins_20_a_plus_20_m() {
    const AnalyseMarine a = analyser_marine(reponse_avec(4, R"("sea_level_height_msl":[-20.0,20.0,-1.54,0])").vue());
    VERIFIER(a.ok() && a.marine.a_maree);
    VERIFIER(a.marine.heures[0].maree_cm == -2000 && a.marine.heures[1].maree_cm == 2000 &&
             a.marine.heures[2].maree_cm == -154 && a.marine.heures[3].maree_cm == 0);
    VERIFIER(NIVEAU_MER_MAX_CM == 2000);
    VERIFIER(refuse(reponse_avec(2, R"("sea_level_height_msl":[-20.01,0])").vue(), Erreur::HorsPlage,
                    "hourly.sea_level_height_msl"));
    VERIFIER(refuse(reponse_avec(2, R"("sea_level_height_msl":[0,20.01])").vue(), Erreur::HorsPlage,
                    "hourly.sea_level_height_msl"));
    return 0;
}

constexpr int le_vent_est_lu_en_dixiemes_de_km_h_et_en_degres() {
    const AnalyseVent a = analyser_vent(
        reponse_avec(
            4,
            R"("wind_speed_10m":[0,19.8,500,null],"wind_gusts_10m":[0.04,0.05,null,30.25],"wind_direction_10m":[0,360,359,null])")
            .vue());
    VERIFIER(a.ok());
    VERIFIER(a.vent.heures[0].vitesse_dixiemes_kmh == 0 && a.vent.heures[1].vitesse_dixiemes_kmh == 198 &&
             a.vent.heures[2].vitesse_dixiemes_kmh == 5000 && a.vent.heures[3].vitesse_dixiemes_kmh == ABSENT);
    VERIFIER(a.vent.heures[0].rafale_dixiemes_kmh == 0 && a.vent.heures[1].rafale_dixiemes_kmh == 1 &&
             a.vent.heures[2].rafale_dixiemes_kmh == ABSENT && a.vent.heures[3].rafale_dixiemes_kmh == 303);
    VERIFIER(a.vent.heures[0].direction_deg == 0 && a.vent.heures[1].direction_deg == 0 &&
             a.vent.heures[2].direction_deg == 359 && a.vent.heures[3].direction_deg == ABSENT);
    VERIFIER(VITESSE_VENT_MAX_DIXIEMES_KMH == 5000);
    VERIFIER(refuse_vent(reponse_avec(2, R"("wind_speed_10m":[500.1,0])").vue(), Erreur::HorsPlage,
                         "hourly.wind_speed_10m"));
    VERIFIER(
        refuse_vent(reponse_avec(2, R"("wind_gusts_10m":[0,-0.1])").vue(), Erreur::HorsPlage, "hourly.wind_gusts_10m"));
    VERIFIER(refuse_vent(reponse_avec(2, R"("wind_direction_10m":[0,361])").vue(), Erreur::HorsPlage,
                         "hourly.wind_direction_10m"));
    return 0;
}

constexpr int une_valeur_d_un_autre_type_ou_une_serie_de_mauvaise_longueur_refuse_la_reponse() {
    VERIFIER(
        refuse(reponse_avec(2, R"("wave_height":[0.5,"haute"])").vue(), Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":[0.5,true])").vue(), Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":[0.5,[1]])").vue(), Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":5)").vue(), Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":"m")").vue(), Erreur::MauvaisType, "hourly.wave_height"));
    VERIFIER(
        refuse(reponse_avec(3, R"("wave_height":[0.5,0.5])").vue(), Erreur::TableauxIncoherents, "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":[0.5,0.5,0.5])").vue(), Erreur::TableauxIncoherents,
                    "hourly.wave_height"));
    VERIFIER(refuse(reponse_avec(2, R"("wave_height":[])").vue(), Erreur::TableauxIncoherents, "hourly.wave_height"));
    VERIFIER(refuse_vent(reponse_avec(2, R"("wind_speed_10m":[1])").vue(), Erreur::TableauxIncoherents,
                         "hourly.wind_speed_10m"));
    VERIFIER(refuse_vent(reponse_avec(2, R"("wind_gusts_10m":["x",1])").vue(), Erreur::MauvaisType,
                         "hourly.wind_gusts_10m"));
    return 0;
}

// ---- unités --------------------------------------------------------------------------------------------------------------

constexpr int les_unites_annoncees_doivent_etre_celles_demandees() {
    constexpr sv BONNES =
        R"("wave_height":"m","wave_period":"s","wave_direction":"°","sea_surface_temperature":"°C","sea_level_height_msl":"m")";
    constexpr sv SERIES =
        R"("wave_height":[0.5],"wave_period":[8],"wave_direction":[270],"sea_surface_temperature":[15],"sea_level_height_msl":[1])";
    VERIFIER(analyser_marine(reponse_avec(1, SERIES, BONNES).vue()).ok());
    VERIFIER(
        refuse(reponse_avec(1, SERIES, R"("wave_height":"ft")").vue(), Erreur::UniteInattendue, "hourly.wave_height"));
    VERIFIER(
        refuse(reponse_avec(1, SERIES, R"("wave_period":"min")").vue(), Erreur::UniteInattendue, "hourly.wave_period"));
    VERIFIER(refuse(reponse_avec(1, SERIES, R"("wave_direction":"rad")").vue(), Erreur::UniteInattendue,
                    "hourly.wave_direction"));
    VERIFIER(refuse(reponse_avec(1, SERIES, R"("sea_surface_temperature":"°F")").vue(), Erreur::UniteInattendue,
                    "hourly.sea_surface_temperature"));
    VERIFIER(refuse(reponse_avec(1, SERIES, R"("sea_level_height_msl":"cm")").vue(), Erreur::UniteInattendue,
                    "hourly.sea_level_height_msl"));
    // une unité absente de la réponse n'est pas refusée : on ne refuse que ce qu'on SAIT être faux ; une unité sans série non plus
    VERIFIER(analyser_marine(reponse_avec(1, SERIES).vue()).ok());
    VERIFIER(analyser_marine(reponse_avec(1, SERIES, R"("autre":"m")").vue()).ok());
    VERIFIER(analyser_marine(reponse_avec(1, R"("wave_period":[8])", R"("wave_height":"ft")").vue())
                 .ok()); // la hauteur n'est pas lue
    const sv vent_series = R"("wind_speed_10m":[10],"wind_gusts_10m":[20],"wind_direction_10m":[90])";
    VERIFIER(analyser_vent(reponse_avec(1, vent_series,
                                        R"("wind_speed_10m":"km/h","wind_gusts_10m":"km/h","wind_direction_10m":"°")")
                               .vue())
                 .ok());
    VERIFIER(refuse_vent(reponse_avec(1, vent_series, R"("wind_speed_10m":"mph")").vue(), Erreur::UniteInattendue,
                         "hourly.wind_speed_10m"));
    VERIFIER(refuse_vent(reponse_avec(1, vent_series, R"("wind_gusts_10m":"kn")").vue(), Erreur::UniteInattendue,
                         "hourly.wind_gusts_10m"));
    VERIFIER(refuse_vent(reponse_avec(1, vent_series, R"("wind_direction_10m":"rad")").vue(), Erreur::UniteInattendue,
                         "hourly.wind_direction_10m"));
    return 0;
}

constexpr int chaque_erreur_a_une_phrase_courte_et_aucune_n_est_vide() {
    VERIFIER(message_court(Erreur::Aucune).empty());
    for (const Erreur e : {Erreur::JsonInvalide, Erreur::ReponseRefusee, Erreur::ChampAbsent, Erreur::MauvaisType,
                           Erreur::HorsPlage, Erreur::TableauxIncoherents, Erreur::UniteInattendue}) {
        VERIFIER(!message_court(e).empty() && message_court(e).size() < 40);
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(une_reponse_qui_n_est_pas_du_json_ou_pas_un_objet_est_illisible)                                                 \
    X(une_reponse_d_erreur_de_l_api_est_reconnue)                                                                      \
    X(le_fuseau_horaire_doit_etre_present_entier_et_plausible)                                                         \
    X(la_liste_des_heures_doit_exister_etre_faite_d_entiers_plausibles)                                                \
    X(une_reponse_minimale_est_lue_et_toutes_les_variables_restent_absentes)                                           \
    X(soixante_heures_passent_soixante_et_une_sont_refusees_pas_tronquees)                                             \
    X(des_heures_qui_ne_se_suivent_pas_d_une_heure_sont_refusees)                                                      \
    X(un_null_est_pas_de_donnee_jamais_zero)                                                                           \
    X(la_hauteur_est_lue_en_centimetres_bornes_0_et_30_m)                                                              \
    X(les_arrondis_se_font_au_plus_proche_la_moitie_s_eloigne_de_zero)                                                 \
    X(la_periode_est_lue_en_dixiemes_de_seconde_bornes_0_et_40_s)                                                      \
    X(les_directions_vont_de_0_a_360_et_360_degres_font_zero)                                                          \
    X(la_temperature_de_la_mer_va_de_moins_5_a_45_degres)                                                              \
    X(le_niveau_de_la_mer_va_de_moins_20_a_plus_20_m)                                                                  \
    X(le_vent_est_lu_en_dixiemes_de_km_h_et_en_degres)                                                                 \
    X(une_valeur_d_un_autre_type_ou_une_serie_de_mauvaise_longueur_refuse_la_reponse)                                  \
    X(les_unites_annoncees_doivent_etre_celles_demandees)                                                              \
    X(chaque_erreur_a_une_phrase_courte_et_aucune_n_est_vide)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
