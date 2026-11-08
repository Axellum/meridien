// Noyau pur : ce que l'écran principal reçoit de la météo, dans une forme que le dessin lit sans rien convertir. Un champ inconnu
// vaut INCONNU (-1) ou INCONNU_DIX (-32768) ; l'écran cache ce qu'il ne connaît pas, il n'invente jamais une valeur (une
// température « 0 » n'est pas une température inconnue).
//
// `vue_depuis_releve()` remplit la vue à partir du relevé Open-Meteo du client de l'appareil (ecran/meteo_openmeteo.hpp) : la
// seule conversion qui compte (unités, jour courant, lever et coucher du jour). Ce que le relevé n'a pas (prévision heure par
// heure, cumul de pluie, tendance de la pression, « pluie dans N minutes ») reste inconnu : voir
// side-session-notes/…/ecran-visuel_besoins-meteo.md, la demande faite à la session météo.
// C++17 standard, aucun include ESP-IDF ni LVGL ; testé sur le PC (test_visuel_vue.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/meteo_openmeteo.hpp"
#include "ecran/visuel_echelles.hpp"

namespace ecran::visuel {

inline constexpr int VUE_HEURES_MAX = 24;
inline constexpr int VUE_JOURS_MAX = 7;
inline constexpr std::size_t VUE_VILLE_MAX = 40; // octets, zéro final compris

struct HeurePrevue {
    std::int64_t instant_s = 0; // début de l'heure, instant UNIX UTC
    int temp_dix = INCONNU_DIX;
    int code = INCONNU; // code WMO
    bool jour = true;
    int proba_pct = INCONNU;
    int pluie_dix_mm = INCONNU_DIX;
};

struct JourPrevu {
    std::int64_t date_s = 0; // minuit local du jour, instant UNIX UTC
    int code = INCONNU;
    int min_dix = INCONNU_DIX;
    int max_dix = INCONNU_DIX;
    int proba_pct = INCONNU;
    int pluie_dix_mm = INCONNU_DIX;
    int uv_dix = INCONNU_DIX;       // indice UV maximal du jour
    int vent_dix_kmh = INCONNU_DIX; // vent maximal du jour, dixièmes de km/h (MET Norway et NWS : la force seule)
    int direction_deg =
        INCONNU; // d'où vient le vent dominant du jour ; INCONNU quand le fournisseur ne la donne pas (flèche cachée)
};

struct VueMeteo {
    bool connue = false;  // une météo exploitable existe ; faux : « Météo indisponible »
    bool perimee = false; // reçue il y a trop longtemps : l'écran la garde et le dit
    bool exemple = false; // valeurs d'un bouchon : l'écran le dit
    std::int64_t instant_s = 0;
    int decalage_utc_s = 0;
    char ville[VUE_VILLE_MAX] = {};
    // maintenant
    int code = INCONNU;
    bool jour = true;
    int temp_dix = INCONNU_DIX;
    int ressenti_dix = INCONNU_DIX;
    int min_dix = INCONNU_DIX; // minimum du jour
    int max_dix = INCONNU_DIX; // maximum du jour
    int vent_dix_kmh = INCONNU_DIX;
    int rafales_dix_kmh = INCONNU_DIX;
    int direction_deg = INCONNU; // d'où vient le vent
    int humidite_pct = INCONNU;
    int pression_dix_hpa = INCONNU_DIX;
    int tendance_dix_hpa_3h = INCONNU_DIX;
    int uv_dix = INCONNU_DIX; // indice UV (du moment si connu, sinon maximum du jour : `uv_est_max`)
    bool uv_est_max = false;
    int proba_pluie_pct = INCONNU;
    int pluie_dix_mm = INCONNU_DIX;
    int pluie_dans_min = -2; // contrat de pluie_proche() : 0 il pleut, -1 rien dans 2 h, -2 inconnu
    std::int64_t lever_s = 0;
    std::int64_t coucher_s = 0;
    // prévisions
    int nb_heures = 0;
    HeurePrevue heures[VUE_HEURES_MAX] = {};
    int nb_jours = 0;
    JourPrevu jours[VUE_JOURS_MAX] = {};
};

namespace detail {

constexpr void copier_ville(char *dest, const char *source) noexcept {
    std::size_t i = 0;
    if (source != nullptr) {
        for (; source[i] != '\0' && i + 1 < VUE_VILLE_MAX; ++i)
            dest[i] = source[i];
    }
    dest[i] = '\0';
}

} // namespace detail

// Relevé du client -> vue. `maintenant_s` : l'instant du moment (UTC), pour choisir le jour en cours parmi les jours prévus.
// `ville` : nom à afficher (peut être nul ou vide). Les champs du relevé hors plage ne sont pas recopiés (inconnus).
constexpr VueMeteo vue_depuis_releve(const meteo::Releve &r, std::int64_t maintenant_s,
                                     const char *ville = nullptr) noexcept {
    VueMeteo v{};
    v.connue = true;
    v.instant_s = r.instant_s;
    v.decalage_utc_s = r.decalage_utc_s;
    detail::copier_ville(v.ville, ville);
    v.code = r.code_wmo;
    v.jour = r.jour;
    v.temp_dix = r.temperature_dixiemes;
    v.ressenti_dix = r.ressentie_dixiemes;
    v.vent_dix_kmh = r.vent_dixiemes_kmh;
    v.rafales_dix_kmh = r.rafales_dixiemes_kmh;
    v.direction_deg = (r.direction_vent_deg >= 0 && r.direction_vent_deg <= 360) ? r.direction_vent_deg : INCONNU;
    v.humidite_pct = (r.humidite_pct >= 0 && r.humidite_pct <= 100) ? r.humidite_pct : INCONNU;
    v.pression_dix_hpa = (r.pression_dixiemes_hpa > 0) ? r.pression_dixiemes_hpa : INCONNU_DIX;
    v.proba_pluie_pct = (r.proba_precip_pct >= 0 && r.proba_precip_pct <= 100) ? r.proba_precip_pct : INCONNU;
    const int nb = r.nb_jours < VUE_JOURS_MAX ? static_cast<int>(r.nb_jours) : VUE_JOURS_MAX;
    for (int i = 0; i < nb; ++i) {
        const meteo::Jour &j = r.jours[i];
        JourPrevu &p = v.jours[i];
        p.date_s = j.date_s;
        p.code = j.code_wmo;
        p.min_dix = j.min_dixiemes;
        p.max_dix = j.max_dixiemes;
        p.proba_pct = (j.proba_precip_max >= 0 && j.proba_precip_max <= 100) ? j.proba_precip_max : INCONNU;
        p.uv_dix = j.uv_max_dixiemes;
    }
    v.nb_jours = nb;
    const int auj = meteo::index_du_jour(r, maintenant_s);
    if (auj >= 0 && auj < nb) {
        const meteo::Jour &j = r.jours[auj];
        v.min_dix = j.min_dixiemes;
        v.max_dix = j.max_dixiemes;
        v.lever_s = j.sans_soleil() ? 0 : j.lever_s;
        v.coucher_s = j.sans_soleil() ? 0 : j.coucher_s;
        v.uv_dix = j.uv_max_dixiemes;
        v.uv_est_max = true;
    }
    return v;
}

} // namespace ecran::visuel
