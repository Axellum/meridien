// Noyau pur : la réponse de MET Norway « Locationforecast 2.0 / complete » -> la météo du noyau (`AnalyseRiche`, comme le lecteur
// d'Open-Meteo). C++17 standard, aucun include ESP-IDF, aucun flottant dans la lecture des données (entiers en dixièmes) ; seul
// le calcul du soleil (soleil_calcule.hpp) emploie des `double`. Décision et mesures : ADR-0014, docs/mesures/fournisseurs-meteo.md.
//
// Requête : meteo_requete_fournisseurs.hpp (https://api.met.no/weatherapi/locationforecast/2.0/complete?lat=…&lon=…, quatre
// décimales, User-Agent qui identifie l'application). Réponse : GeoJSON, `properties.timeseries[]`, un élément par instant UTC
// (« 2026-11-07T12:00:00Z »), un pas d'une heure pendant 54 à 60 heures puis de six heures jusqu'à dix jours (relevé le
// 07/11/2026). Chaque élément a `data.instant.details` (valeurs à cet instant) et des périodes `next_1_hours`, `next_6_hours`,
// `next_12_hours` (symbole, cumul de pluie, probabilité, minimum et maximum) qui COMMENCENT à cet instant.
//
// CE QUE LA SOURCE DONNE, ET CE QU'ELLE NE DONNE PAS (aucune valeur n'est inventée : « inconnu » = INCONNU_DIX / INCONNU_ENTIER) :
//   - température, température ressentie (`apparent_air_temperature`, absente => inconnue), humidité, pression au niveau de la
//     mer, nuages, vent, direction : à chaque instant. Vent en m/s converti en km/h (× 3,6 sur le texte, arrondi au dixième).
//   - rafales et probabilité de précipitations : seulement dans la zone « nordique » de MET Norway. Relevé le 07/11/2026 :
//     rafales à Oslo, Helsinki, Tromsø, Longyearbyen ; probabilité à Oslo, Helsinki, Tromsø mais PAS à Longyearbyen ; ni l'une ni
//     l'autre à Reykjavik, Londres, Tokyo, Sydney. Absentes : inconnues.
//   - indice UV : MET Norway ne donne que l'UV « ciel dégagé » (`ultraviolet_index_clear_sky`), pas l'UV prévu avec les nuages.
//     Le montrer sous le nom « UV » serait faux : l'UV est INCONNU (décision d'Axel à confirmer, ADR-0014).
//   - lever et coucher du soleil : non fournis, calculés ici (soleil_calcule.hpp).
//   - pluie par quarts d'heure, neige (cm), durée d'ensoleillement, direction dominante du vent d'un jour : non fournis.
//   - cumul de pluie d'un jour et nombre d'heures de pluie : seulement pour un jour dont chaque heure (ou bloc de six heures
//     tombant exactement dans le jour) est connue ; sinon inconnus. Le premier jour n'est jamais complet : la réponse commence à
//     l'heure en cours.
//   - minimum et maximum du jour : sur les instants et les blocs de six heures de la réponse ; le premier jour ne couvre que
//     le reste de la journée (la réponse ne remonte pas avant l'heure en cours) : limite connue, ADR-0014.
//
// SYMBOLES : `symbol_code` (« partlycloudy_night », « lightrainshowers_day »…) -> code WMO du projet (les 29 codes de
// meteo_wmo.hpp). La liste des 41 symboles est celle du dépôt officiel metno/weathericons (relevée le 07/11/2026) ; la
// correspondance est la nôtre (« sleet » = neige fondue n'existe pas parmi nos 29 codes : famille de la neige). Un symbole
// absent de la table rend le code inconnu : l'heure, le jour ou le courant qui en dépend est refusé, pas deviné.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/iso8601.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/meteo_natif_commun.hpp"
#include "ecran/meteo_riche.hpp"
#include "ecran/parcours_json.hpp"
#include "ecran/soleil_calcule.hpp"

namespace ecran::meteo::metno {

// Le plus grand nombre d'éléments lus (la réponse du 07/11/2026 en a 85 à 91) ; les suivants sont ignorés.
inline constexpr std::size_t ENTREES_MAX = 128;

// ---- symboles ---------------------------------------------------------------------------------------------------------

struct Symbole {
    std::string_view nom; // sans _day, _night ni _polartwilight
    std::int16_t code;    // code WMO du projet
};

// clang-format off
inline constexpr Symbole SYMBOLES[] = {
    {"clearsky", 0}, {"fair", 1}, {"partlycloudy", 2}, {"cloudy", 3}, {"fog", 45},
    {"lightrain", 61}, {"rain", 63}, {"heavyrain", 65},
    {"lightrainshowers", 80}, {"rainshowers", 81}, {"heavyrainshowers", 82},
    {"lightsleet", 71}, {"sleet", 73}, {"heavysleet", 75},
    {"lightsleetshowers", 85}, {"sleetshowers", 85}, {"heavysleetshowers", 86},
    {"lightsnow", 71}, {"snow", 73}, {"heavysnow", 75},
    {"lightsnowshowers", 85}, {"snowshowers", 85}, {"heavysnowshowers", 86},
    {"lightrainandthunder", 95}, {"rainandthunder", 95}, {"heavyrainandthunder", 97},
    {"lightrainshowersandthunder", 95}, {"rainshowersandthunder", 95}, {"heavyrainshowersandthunder", 97},
    {"lightsleetandthunder", 95}, {"sleetandthunder", 95}, {"heavysleetandthunder", 97},
    {"lightssleetshowersandthunder", 95}, {"sleetshowersandthunder", 95}, {"heavysleetshowersandthunder", 97},
    {"lightsnowandthunder", 95}, {"snowandthunder", 95}, {"heavysnowandthunder", 97},
    {"lightssnowshowersandthunder", 95}, {"snowshowersandthunder", 95}, {"heavysnowshowersandthunder", 97},
    // Orthographe corrigée des deux symboles « lightss… » (le dépôt officiel annonce la correction pour la prochaine version)
    {"lightsleetshowersandthunder", 95}, {"lightsnowshowersandthunder", 95},
};
// clang-format on

namespace detail {

constexpr bool finit_par(std::string_view t, std::string_view fin) noexcept {
    return t.size() >= fin.size() && t.substr(t.size() - fin.size()) == fin;
}

} // namespace detail

// « partlycloudy_night » -> « partlycloudy » ; rien d'autre n'est retiré.
constexpr std::string_view sans_variante(std::string_view symbole) noexcept {
    for (const std::string_view suffixe :
         {std::string_view("_day"), std::string_view("_night"), std::string_view("_polartwilight")}) {
        if (detail::finit_par(symbole, suffixe))
            return symbole.substr(0, symbole.size() - suffixe.size());
    }
    return symbole;
}

// Code WMO du projet pour un `symbol_code`, ou -1 s'il n'est pas dans la table.
constexpr int code_projet(std::string_view symbole) noexcept {
    const std::string_view base = sans_variante(symbole);
    for (const Symbole &s : SYMBOLES) {
        if (s.nom == base)
            return s.code;
    }
    return -1;
}

// ---- une réponse lue élément par élément --------------------------------------------------------------------------------

struct Instant {
    std::int16_t temperature = INCONNU_DIX;
    std::int16_t ressentie = INCONNU_DIX;
    std::int16_t humidite = INCONNU_ENTIER;
    std::int16_t pression = INCONNU_DIX;
    std::int16_t nuages = INCONNU_ENTIER;
    std::int16_t vent = INCONNU_DIX; // km/h, dixièmes
    std::int16_t rafales = INCONNU_DIX;
    std::int16_t direction = INCONNU_ENTIER;
};

struct Periode {
    bool presente = false;
    std::int16_t code = -1; // code WMO du projet ; -1 : symbole absent ou inconnu
    std::int16_t precip = INCONNU_DIX;
    std::int16_t proba = INCONNU_ENTIER;
    std::int16_t tmin = INCONNU_DIX;
    std::int16_t tmax = INCONNU_DIX;
};

struct Entree {
    std::int64_t t = 0;
    Instant instant{};
    Periode h1{};  // next_1_hours
    Periode h6{};  // next_6_hours
    Periode h12{}; // next_12_hours
};

// Espace de travail de `analyser_metno` (≈ 8 Ko) : à fournir par l'appelant (sur la carte, en PSRAM, pas sur la pile).
struct Serie {
    Entree e[ENTREES_MAX] = {};
    std::size_t n = 0;
};

namespace detail {

constexpr std::int16_t vitesse_kmh(const json::Valeur &v, std::string_view nom, Diagnostic &diag) noexcept {
    std::int64_t ms_dix = 0;
    switch (natif::lire_fixe(v, 1, ms_dix)) {
    case natif::Lecture::Absente:
        return INCONNU_DIX;
    case natif::Lecture::Invalide:
        natif::signaler(diag, Erreur::MauvaisType, nom);
        return INCONNU_DIX;
    case natif::Lecture::Valeur:
        break;
    }
    const std::int64_t kmh = natif::kmh_dix_depuis_ms_dix(ms_dix);
    if (ms_dix < 0 || kmh > INT16_MAX) {
        natif::signaler(diag, Erreur::HorsPlage, nom);
        return INCONNU_DIX;
    }
    return static_cast<std::int16_t>(kmh);
}

constexpr Instant lire_instant(const json::Valeur &data, Diagnostic &diag) noexcept {
    const json::Valeur d = data.trouver("instant.details");
    Instant i;
    i.temperature = natif::fixe_ou_inconnu(d.champ("air_temperature"), 1, "air_temperature", diag);
    i.ressentie = natif::fixe_ou_inconnu(d.champ("apparent_air_temperature"), 1, "apparent_air_temperature", diag);
    i.humidite = natif::pourcentage_ou_inconnu(d.champ("relative_humidity"), "relative_humidity", diag);
    i.pression = natif::fixe_ou_inconnu(d.champ("air_pressure_at_sea_level"), 1, "air_pressure_at_sea_level", diag);
    i.nuages = natif::pourcentage_ou_inconnu(d.champ("cloud_area_fraction"), "cloud_area_fraction", diag);
    i.vent = vitesse_kmh(d.champ("wind_speed"), "wind_speed", diag);
    i.rafales = vitesse_kmh(d.champ("wind_speed_of_gust"), "wind_speed_of_gust", diag);
    i.direction = natif::entier_ou_inconnu(d.champ("wind_from_direction"), "wind_from_direction", diag);
    return i;
}

constexpr Periode lire_periode(const json::Valeur &data, std::string_view cle, Diagnostic &diag) noexcept {
    Periode p;
    const json::Valeur bloc = data.champ(cle);
    if (bloc.genre() != json::Genre::Objet)
        return p;
    p.presente = true;
    const json::Valeur symbole = bloc.trouver("summary.symbol_code");
    if (symbole.genre() == json::Genre::Chaine) {
        char tampon[64] = {};
        bool tronque = false;
        const std::size_t n = symbole.copier_texte(tampon, sizeof tampon, &tronque);
        if (!tronque)
            p.code = static_cast<std::int16_t>(code_projet(std::string_view(tampon, n)));
    }
    const json::Valeur d = bloc.champ("details");
    p.precip = natif::fixe_ou_inconnu(d.champ("precipitation_amount"), 1, "precipitation_amount", diag);
    p.proba =
        natif::pourcentage_ou_inconnu(d.champ("probability_of_precipitation"), "probability_of_precipitation", diag);
    p.tmin = natif::fixe_ou_inconnu(d.champ("air_temperature_min"), 1, "air_temperature_min", diag);
    p.tmax = natif::fixe_ou_inconnu(d.champ("air_temperature_max"), 1, "air_temperature_max", diag);
    return p;
}

// Lit `properties.timeseries` dans `s`. Faux, avec le diagnostic, si la liste manque, si un instant est illisible ou ne croît pas.
constexpr bool lire_serie(const json::Valeur &racine, Serie &s, Diagnostic &diag) noexcept {
    const json::Valeur serie = racine.trouver("properties.timeseries");
    if (serie.genre() != json::Genre::Tableau) {
        natif::signaler(diag, serie.existe() ? Erreur::MauvaisType : Erreur::ChampAbsent, "properties.timeseries");
        return false;
    }
    json::Parcours parcours(serie);
    json::Valeur el;
    s.n = 0;
    while (s.n < ENTREES_MAX && parcours.suivant(el)) {
        Entree &e = s.e[s.n];
        const json::Valeur temps = el.champ("time");
        char tampon[40] = {};
        bool tronque = false;
        const std::size_t n =
            temps.genre() == json::Genre::Chaine ? temps.copier_texte(tampon, sizeof tampon, &tronque) : 0;
        if (n == 0 || tronque || !iso::instant(std::string_view(tampon, n), e.t)) {
            natif::signaler(diag, Erreur::MauvaisType, "properties.timeseries.time");
            return false;
        }
        if (s.n > 0 && e.t <= s.e[s.n - 1].t) {
            natif::signaler(diag, Erreur::TableauxIncoherents, "properties.timeseries.time");
            return false;
        }
        const json::Valeur data = el.champ("data");
        if (data.genre() != json::Genre::Objet) {
            natif::signaler(diag, Erreur::ChampAbsent, "properties.timeseries.data");
            return false;
        }
        e.instant = lire_instant(data, diag);
        e.h1 = lire_periode(data, "next_1_hours", diag);
        e.h6 = lire_periode(data, "next_6_hours", diag);
        e.h12 = lire_periode(data, "next_12_hours", diag);
        if (!diag.ok())
            return false;
        ++s.n;
    }
    if (s.n == 0) {
        natif::signaler(diag, Erreur::ChampAbsent, "properties.timeseries");
        return false;
    }
    return true;
}

// Le premier code connu parmi les périodes d'un élément (la plus courte d'abord), ou -1.
constexpr std::int16_t code_de(const Entree &e) noexcept {
    if (e.h1.code >= 0)
        return e.h1.code;
    if (e.h6.code >= 0)
        return e.h6.code;
    return e.h12.code;
}

constexpr std::int64_t valeur_absolue(std::int64_t v) noexcept { return v < 0 ? -v : v; }

} // namespace detail

// Analyse la réponse. `recu_a_s` : heure (UNIX) de réception. `latitude_e5`, `longitude_e5` : la position demandée (pour le
// soleil). `iana` : le fuseau de la ville (« Europe/Oslo ») : la réponse n'en donne pas, et les jours se découpent en heure
// locale ; un nom inconnu de fuseaux.hpp fait refuser la réponse. `travail` : voir `Serie`.
//
// Même contrat que `analyser_riche` : `diag` dit si la météo de base est utilisable ; `diag_heures`, `diag_jours` et `diag_quarts`
// disent pourquoi une section est vide (jamais une valeur inventée à la place).
constexpr AnalyseRiche analyser_metno(std::string_view texte, std::int64_t recu_a_s, std::int32_t latitude_e5,
                                      std::int32_t longitude_e5, std::string_view iana, Serie &travail) noexcept {
    AnalyseRiche a;
    Releve &r = a.meteo.releve;
    Riche &q = a.meteo.riche;

    const json::Resultat lu = json::lire(texte);
    if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet) {
        a.diag = Diagnostic{Erreur::JsonInvalide, "json"};
        return a;
    }
    const std::string_view posix = fuseaux::posix_pour(iana);
    const heure::AnalyseTz tz = heure::analyser_tz(posix);
    if (posix.empty() || !tz.ok() || !natif::copier_fuseau(r, iana)) {
        a.diag = Diagnostic{Erreur::ChampAbsent, "fuseau"};
        return a;
    }
    Serie &s = travail;
    if (!detail::lire_serie(lu.valeur, s, a.diag))
        return a;

    // ---- l'élément de l'heure en cours : le dernier dont l'instant n'est pas après la réception ----
    std::size_t i0 = 0;
    for (std::size_t i = 0; i < s.n; ++i) {
        if (s.e[i].t <= recu_a_s)
            i0 = i;
    }
    const Entree &c = s.e[i0];
    r.recu_a_s = recu_a_s;
    r.instant_s = c.t;
    r.decalage_utc_s = heure::en_heure_locale(recu_a_s, tz.fuseau).decalage_s;
    if (c.instant.temperature == INCONNU_DIX) {
        a.diag = Diagnostic{Erreur::ChampAbsent, "air_temperature"};
        return a;
    }
    const std::int16_t code_courant = detail::code_de(c);
    if (code_courant < 0) {
        a.diag = Diagnostic{Erreur::ChampAbsent, "symbol_code"};
        return a;
    }
    r.temperature_dixiemes = c.instant.temperature;
    r.ressentie_dixiemes = c.instant.ressentie;
    r.code_wmo = code_courant;
    r.jour = astro::soleil_leve(c.t, latitude_e5, longitude_e5);
    r.vent_dixiemes_kmh = c.instant.vent;
    r.rafales_dixiemes_kmh = c.instant.rafales;
    r.direction_vent_deg = c.instant.direction;
    r.pression_dixiemes_hpa = c.instant.pression;
    r.humidite_pct = static_cast<std::int8_t>(c.instant.humidite);
    r.proba_precip_pct = static_cast<std::int8_t>(c.h1.proba);
    r.nb_jours = 0;
    q.precip_courante_dixiemes = INCONNU_DIX; // « le dernier quart d'heure écoulé » : MET Norway ne le donne pas
    q.nuages_pct = c.instant.nuages;
    a.diag = verifier(a.meteo.releve);
    if (!a.diag.ok())
        return a;
    a.diag = ecran::meteo::detail::verifier_courant_riche(q);
    if (!a.diag.ok())
        return a;

    // ---- 24 heures : des éléments d'une heure exactement, chacun avec sa période d'une heure et un symbole connu ----
    {
        bool bon = i0 + static_cast<std::size_t>(HEURES_MAX) <= s.n;
        for (int k = 0; bon && k < HEURES_MAX; ++k) {
            const Entree &e = s.e[i0 + static_cast<std::size_t>(k)];
            bon =
                e.t == c.t + PAS_HEURE_S * k && e.instant.temperature != INCONNU_DIX && e.h1.presente && e.h1.code >= 0;
        }
        if (!bon) {
            a.diag_heures = Diagnostic{Erreur::TableauxIncoherents, "hourly"};
        } else {
            for (int k = 0; k < HEURES_MAX; ++k) {
                const Entree &e = s.e[i0 + static_cast<std::size_t>(k)];
                HeureRiche &h = q.heures[k];
                h.debut_s = e.t;
                h.temperature_dixiemes = e.instant.temperature;
                h.ressentie_dixiemes = e.instant.ressentie;
                h.humidite_pct = e.instant.humidite;
                h.proba_pct = e.h1.proba;
                h.precip_dixiemes_mm = e.h1.precip;
                h.code_wmo = e.h1.code;
                h.nuages_pct = e.instant.nuages;
                h.vent_dixiemes_kmh = e.instant.vent;
                h.rafales_dixiemes_kmh = e.instant.rafales;
                h.direction_deg = e.instant.direction;
                h.uv_dixiemes = INCONNU_DIX; // MET Norway ne donne que l'UV par ciel dégagé
                h.jour = astro::soleil_leve(e.t, latitude_e5, longitude_e5);
            }
            q.nb_heures = static_cast<std::uint8_t>(HEURES_MAX);
            a.diag_heures = ecran::meteo::detail::verifier_heures_riche(q);
            if (!a.diag_heures.ok())
                q.nb_heures = 0;
        }
    }

    // ---- jusqu'à sept jours locaux ----
    {
        const natif::Jours jl = natif::jours_locaux(tz.fuseau, recu_a_s);
        std::uint8_t nj = 0;
        for (int k = 0; k < JOURS_MAX; ++k) {
            const std::int64_t d0 = jl.debut[k];
            const std::int64_t d1 = jl.debut[k + 1];
            natif::Extremes temp, ress, vent, raf;
            int proba = INCONNU_ENTIER;
            std::int16_t code = -1;
            std::int64_t ecart_midi = INT64_MAX;
            for (std::size_t i = 0; i < s.n; ++i) {
                const Entree &e = s.e[i];
                if (e.t < d0 || e.t >= d1)
                    continue;
                temp.noter(e.instant.temperature);
                ress.noter(e.instant.ressentie);
                vent.noter(e.instant.vent);
                raf.noter(e.instant.rafales);
                if (e.h1.proba > proba)
                    proba = e.h1.proba;
                const bool bloc6_dans_le_jour = e.h6.presente && e.t + 6 * 3600 <= d1;
                if (bloc6_dans_le_jour) {
                    temp.noter(e.h6.tmin);
                    temp.noter(e.h6.tmax);
                    if (e.h6.proba > proba)
                        proba = e.h6.proba;
                }
                // le symbole du jour : celui de l'élément le plus proche de midi (période de six heures, sinon douze, sinon une)
                const std::int16_t cand = e.h6.code >= 0 ? e.h6.code : (e.h12.code >= 0 ? e.h12.code : e.h1.code);
                const std::int64_t ecart = detail::valeur_absolue(e.t - (d0 + 43200));
                if (cand >= 0 && ecart < ecart_midi) {
                    ecart_midi = ecart;
                    code = cand;
                }
            }
            if (temp.vide() || code < 0)
                break; // pas de jour sans température ni symbole : on s'arrête au dernier jour complet
            // cumul de pluie : des blocs qui se suivent sans trou de l'instant `d0` à `d1`
            std::int64_t curseur = d0;
            std::int64_t total = 0;
            int heures_pluie = 0;
            bool complet = true;
            bool que_des_heures = true;
            while (curseur < d1 && complet) {
                const Entree *trouve = nullptr;
                for (std::size_t i = 0; i < s.n && trouve == nullptr; ++i) {
                    if (s.e[i].t == curseur)
                        trouve = &s.e[i];
                }
                if (trouve == nullptr) {
                    complet = false;
                } else if (trouve->h1.presente && trouve->h1.precip != INCONNU_DIX) {
                    total += trouve->h1.precip;
                    if (trouve->h1.precip >= SEUIL_PLUIE_DIXIEMES)
                        ++heures_pluie;
                    curseur += 3600;
                } else if (trouve->h6.presente && trouve->h6.precip != INCONNU_DIX && curseur + 6 * 3600 <= d1) {
                    total += trouve->h6.precip;
                    que_des_heures = false;
                    curseur += 6 * 3600;
                } else {
                    complet = false;
                }
            }

            Jour &j = r.jours[k];
            j.date_s = d0;
            j.code_wmo = code;
            j.min_dixiemes = temp.min;
            j.max_dixiemes = temp.max;
            natif::remplir_soleil(j, d1, latitude_e5, longitude_e5);
            j.uv_max_dixiemes = INCONNU_DIX;
            j.proba_precip_max = static_cast<std::int8_t>(proba);
            JourRiche &cj = q.jours[k];
            cj.ressentie_min_dixiemes = ress.min;
            cj.ressentie_max_dixiemes = ress.max;
            cj.ensoleillement_min = INCONNU_ENTIER;
            cj.precip_dixiemes_mm = (complet && total <= INT16_MAX) ? static_cast<std::int16_t>(total) : INCONNU_DIX;
            cj.neige_dixiemes_cm = INCONNU_DIX;
            cj.heures_precip = (complet && que_des_heures) ? static_cast<std::int16_t>(heures_pluie) : INCONNU_ENTIER;
            cj.vent_max_dixiemes_kmh = vent.max;
            cj.rafales_max_dixiemes_kmh = raf.max;
            cj.direction_dominante_deg = INCONNU_ENTIER;
            ++nj;
        }
        r.nb_jours = nj;
        q.nb_jours = nj;
        a.diag_jours = verifier(r);
        if (a.diag_jours.ok())
            a.diag_jours = ecran::meteo::detail::verifier_jours_riche(r, q);
        if (!a.diag_jours.ok()) {
            r.nb_jours = 0;
            q.nb_jours = 0;
        } else if (nj == 0) {
            a.diag_jours = Diagnostic{Erreur::ChampAbsent, "daily"};
        }
    }

    // MET Norway n'a pas de pluie par quarts d'heure : la section reste vide, et le dit.
    a.diag_quarts = Diagnostic{Erreur::ChampAbsent, "minutely_15"};
    return a;
}

} // namespace ecran::meteo::metno
