// Tests du noyau pur : conversion du relevé du client Open-Meteo en vue de l'écran principal (unités, jour courant, inconnus).
#include <string_view>

#include "ecran/visuel_vue.hpp"
#include "mini.hpp"

using namespace ecran::visuel;
using ecran::meteo::Jour;
using ecran::meteo::Releve;

namespace {

// Vendredi 6 novembre 2026, 10 h 40 UTC+1 : minuit local du jour = 2026-11-05T23:00:00Z.
constexpr std::int64_t MINUIT_J0 = 1793919600; // 2026-11-05T23:00:00Z
constexpr std::int64_t MAINTENANT = MINUIT_J0 + 10 * 3600 + 40 * 60;

constexpr Releve un_releve() {
    Releve r{};
    r.recu_a_s = MAINTENANT - 60;
    r.instant_s = MAINTENANT - 600;
    r.decalage_utc_s = 3600;
    r.temperature_dixiemes = 124;
    r.ressentie_dixiemes = 98;
    r.code_wmo = 2;
    r.jour = true;
    r.vent_dixiemes_kmh = 183;
    r.rafales_dixiemes_kmh = 412;
    r.direction_vent_deg = 250;
    r.pression_dixiemes_hpa = 10132;
    r.humidite_pct = 71;
    r.proba_precip_pct = 35;
    r.nb_jours = 3;
    for (int i = 0; i < 3; ++i) {
        Jour &j = r.jours[i];
        j.date_s = MINUIT_J0 + i * 86400;
        j.code_wmo = static_cast<std::int16_t>(i == 0 ? 2 : 61);
        j.min_dixiemes = static_cast<std::int16_t>(60 + i * 10);
        j.max_dixiemes = static_cast<std::int16_t>(146 + i * 10);
        j.lever_s = j.date_s + 7 * 3600 + 20 * 60;
        j.coucher_s = j.date_s + 17 * 3600 + 5 * 60;
        j.uv_max_dixiemes = static_cast<std::int16_t>(21 + i);
        j.proba_precip_max = static_cast<std::int8_t>(10 + 20 * i);
    }
    return r;
}

constexpr int les_champs_du_moment_sont_recopies_dans_les_bonnes_unites() {
    const Releve r = un_releve();
    const VueMeteo v = vue_depuis_releve(r, MAINTENANT, "Rennes");
    VERIFIER(v.connue && !v.perimee && !v.exemple);
    VERIFIER(v.temp_dix == 124 && v.ressenti_dix == 98 && v.code == 2 && v.jour);
    VERIFIER(v.vent_dix_kmh == 183 && v.rafales_dix_kmh == 412 && v.direction_deg == 250);
    VERIFIER(v.pression_dix_hpa == 10132 && v.humidite_pct == 71 && v.proba_pluie_pct == 35);
    VERIFIER(v.decalage_utc_s == 3600 && v.instant_s == r.instant_s);
    VERIFIER(std::string_view(v.ville) == "Rennes");
    return 0;
}

constexpr int le_minimum_le_maximum_et_le_soleil_sont_ceux_du_jour_en_cours() {
    const VueMeteo v = vue_depuis_releve(un_releve(), MAINTENANT);
    VERIFIER(v.min_dix == 60 && v.max_dix == 146);
    VERIFIER(v.lever_s == MINUIT_J0 + 7 * 3600 + 20 * 60 && v.coucher_s == MINUIT_J0 + 17 * 3600 + 5 * 60);
    VERIFIER(v.uv_dix == 21 && v.uv_est_max);
    // le lendemain : les valeurs du deuxième jour
    const VueMeteo w = vue_depuis_releve(un_releve(), MAINTENANT + 86400);
    VERIFIER(w.min_dix == 70 && w.max_dix == 156 && w.uv_dix == 22);
    VERIFIER(v.nb_jours == 3 && v.jours[1].code == 61 && v.jours[2].proba_pct == 50);
    return 0;
}

constexpr int un_releve_trop_ancien_ne_donne_ni_minimum_ni_soleil() {
    // 5 jours plus tard : plus aucun jour prévu ne contient l'instant, on n'invente pas celui d'hier
    const VueMeteo v = vue_depuis_releve(un_releve(), MAINTENANT + 5 * 86400);
    VERIFIER(!connu(v.min_dix) && !connu(v.max_dix) && v.lever_s == 0 && v.coucher_s == 0);
    VERIFIER(!connu(v.uv_dix) && !v.uv_est_max);
    return 0;
}

constexpr int les_valeurs_hors_plage_restent_inconnues() {
    Releve r = un_releve();
    r.direction_vent_deg = -1;
    r.humidite_pct = -1;
    r.pression_dixiemes_hpa = 0;
    r.proba_precip_pct = ecran::meteo::PROBA_INCONNUE;
    const VueMeteo v = vue_depuis_releve(r, MAINTENANT);
    VERIFIER(v.direction_deg == INCONNU && v.humidite_pct == INCONNU);
    VERIFIER(!connu(v.pression_dix_hpa) && v.proba_pluie_pct == INCONNU);
    // ce que le client ne sait pas fournir reste inconnu, jamais un zéro
    VERIFIER(!connu(v.tendance_dix_hpa_3h) && !connu(v.pluie_dix_mm) && v.pluie_dans_min == -2 && v.nb_heures == 0);
    return 0;
}

constexpr int la_nuit_polaire_n_a_pas_de_lever_ni_de_coucher() {
    Releve r = un_releve();
    for (int i = 0; i < 3; ++i) {
        r.jours[i].lever_s = r.jours[i].date_s;
        r.jours[i].coucher_s = r.jours[i].date_s;
    }
    const VueMeteo v = vue_depuis_releve(r, MAINTENANT);
    VERIFIER(v.lever_s == 0 && v.coucher_s == 0);
    return 0;
}

constexpr int la_ville_est_tronquee_sans_deborder_et_toujours_terminee() {
    const char *longue = "Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch-sur-mer";
    const VueMeteo v = vue_depuis_releve(un_releve(), MAINTENANT, longue);
    VERIFIER(std::string_view(v.ville).size() == VUE_VILLE_MAX - 1);
    const VueMeteo w = vue_depuis_releve(un_releve(), MAINTENANT, nullptr);
    VERIFIER(w.ville[0] == '\0');
    return 0;
}

constexpr int plus_de_sept_jours_sont_coupes_au_maximum_de_la_vue() {
    Releve r = un_releve();
    r.nb_jours = static_cast<std::uint8_t>(ecran::meteo::JOURS_MAX);
    const VueMeteo v = vue_depuis_releve(r, MAINTENANT);
    VERIFIER(v.nb_jours <= VUE_JOURS_MAX);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_champs_du_moment_sont_recopies_dans_les_bonnes_unites)                                                       \
    X(le_minimum_le_maximum_et_le_soleil_sont_ceux_du_jour_en_cours)                                                   \
    X(un_releve_trop_ancien_ne_donne_ni_minimum_ni_soleil)                                                             \
    X(les_valeurs_hors_plage_restent_inconnues)                                                                        \
    X(la_nuit_polaire_n_a_pas_de_lever_ni_de_coucher)                                                                  \
    X(la_ville_est_tronquee_sans_deborder_et_toujours_terminee)                                                        \
    X(plus_de_sept_jours_sont_coupes_au_maximum_de_la_vue)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
