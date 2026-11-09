// Adaptateur « contrat météo v2 (MeteoDetail) -> VueMeteo de l'écran principal » (écrit par la session ecran-visuel, posé à
// l'intégration du 07/11/2026). Il vit dans main/ et non dans le noyau pur parce qu'il inclut main/meteo_etat.h.
#pragma once

#include <cstdint>

#include "ecran/visuel_vue.hpp"
#include "meteo_etat.h"

namespace ecran::visuel {

namespace detail_v2 {

// Un flottant « exact au dixième » du contrat -> entier en dixièmes ; INCONNU_DIX si NaN.
inline int dix(float x) noexcept {
    if (x != x)
        return INCONNU_DIX;
    return static_cast<int>(x * 10.0F + (x >= 0.0F ? 0.5F : -0.5F));
}
inline int pct(int p) noexcept { return (p >= 0 && p <= 100) ? p : INCONNU; }

} // namespace detail_v2

inline VueMeteo vue_depuis_detail(const MeteoDetail &d, const char *ville) noexcept {
    using namespace detail_v2;
    VueMeteo v{};
    v.connue = d.valide;
    if (!d.valide)
        return v;
    v.perimee = d.perime;
    v.instant_s = d.maj_epoch_s;
    v.decalage_utc_s = d.decalage_utc_s;
    detail::copier_ville(v.ville, ville);

    const MeteoCourant &c = d.courant;
    v.code = c.code_wmo;
    v.jour = c.jour;
    v.temp_dix = dix(c.temperature_c);
    v.ressenti_dix = dix(c.ressentie_c);
    v.vent_dix_kmh = dix(c.vent_kmh);
    v.rafales_dix_kmh = dix(c.rafales_kmh);
    v.direction_deg = (c.direction_vent_deg >= 0 && c.direction_vent_deg <= 360) ? c.direction_vent_deg : INCONNU;
    v.humidite_pct = pct(c.humidite_pct);
    v.pression_dix_hpa = c.pression_hpa > 0.0F ? dix(c.pression_hpa) : INCONNU_DIX;
    v.proba_pluie_pct = pct(c.proba_precip_pct); // METEO_PROBA_INCONNUE (-1) -> INCONNU (-1)
    // Tendance de la pression sur 3 h (hPa, positive si elle monte), mesurée par l'appareil : NaN pendant les ~3 premières heures
    // après chaque démarrage et après un changement de zone -> dix() rend INCONNU_DIX et l'écran cache la flèche.
    v.tendance_dix_hpa_3h = dix(c.tendance_pression_3h_hpa);

    // La pluie proche : « il pleut », « dans N min », « rien dans 2 h » ; des quarts d'heure périmés ne sont plus un « dans N
    // minutes » sûr -> inconnue.
    if (d.pluie.connue && !d.pluie.perimee) {
        v.pluie_dans_min = d.pluie.en_cours ? 0 : (d.pluie.dans_min >= 0 ? d.pluie.dans_min : -1);
    }
    const bool jour_ok = d.jour_courant >= 0 && d.jour_courant < d.nb_jours;
    if (jour_ok) {
        const MeteoJour &j = d.jours[d.jour_courant];
        v.min_dix = dix(j.min_c);
        v.max_dix = dix(j.max_c);
        v.lever_s = j.sans_soleil ? 0 : j.lever_epoch_s;
        v.coucher_s = j.sans_soleil ? 0 : j.coucher_epoch_s;
    }
    // Le cumul de pluie de la tuile « pluie » (affiché seulement quand la pluie proche est inconnue) est celui de l'heure en cours,
    // comme la probabilité qui l'accompagne ; l'indice UV aussi. Sans heure couvrant le moment : la valeur du moment pour la
    // pluie, le maximum du jour pour l'UV.
    const bool heure_ok = d.heure_courante >= 0 && d.heure_courante < d.nb_heures;
    if (heure_ok) {
        v.pluie_dix_mm = dix(d.heures[d.heure_courante].precipitation_mm);
        v.uv_dix = dix(d.heures[d.heure_courante].uv);
        v.uv_est_max = false;
    } else {
        v.pluie_dix_mm = dix(c.precipitation_mm);
        if (jour_ok) {
            v.uv_dix = dix(d.jours[d.jour_courant].uv_max);
            v.uv_est_max = true;
        }
    }

    const int nh = d.nb_heures < VUE_HEURES_MAX ? d.nb_heures : VUE_HEURES_MAX;
    for (int i = 0; i < nh; ++i) {
        const MeteoHeure &h = d.heures[i];
        HeurePrevue &p = v.heures[i];
        p.instant_s = h.debut_epoch_s;
        p.temp_dix = dix(h.temperature_c);
        p.code = h.code_wmo;
        p.jour = h.jour;
        p.proba_pct = pct(h.proba_precip_pct);
        p.pluie_dix_mm = dix(h.precipitation_mm);
    }
    v.nb_heures = nh;
    const int nj = d.nb_jours < VUE_JOURS_MAX ? d.nb_jours : VUE_JOURS_MAX;
    for (int i = 0; i < nj; ++i) {
        const MeteoJour &j = d.jours[i];
        JourPrevu &p = v.jours[i];
        p.date_s = j.date_epoch_s;
        p.code = j.code_wmo;
        p.min_dix = dix(j.min_c);
        p.max_dix = dix(j.max_c);
        p.proba_pct = pct(j.proba_precip_pct);
        p.pluie_dix_mm = dix(j.precipitation_mm);
        p.uv_dix = dix(j.uv_max);
        p.vent_dix_kmh = dix(j.vent_max_kmh);
        p.direction_deg = (j.direction_vent_dominante_deg >= 0 && j.direction_vent_dominante_deg <= 360)
                              ? j.direction_vent_dominante_deg
                              : INCONNU;
    }
    v.nb_jours = nj;
    // La pluie par quart d'heure, telle que le contrat la donne (le premier est en général le quart écoulé) : la bande de pluie de
    // l'accueil choisit elle-même le quart en cours. Un quart sans valeur reste INCONNU_DIX, jamais zéro.
    const int nq = d.nb_quarts < VUE_QUARTS_MAX ? d.nb_quarts : VUE_QUARTS_MAX;
    if (nq > 0 && d.quarts_debut_epoch_s > 0) {
        v.nb_quarts = nq;
        v.quarts_debut_s = d.quarts_debut_epoch_s;
        for (int i = 0; i < nq; ++i)
            v.quarts_dix_mm[i] = dix(d.quarts_precipitation_mm[i]);
    }
    return v;
}

} // namespace ecran::visuel
