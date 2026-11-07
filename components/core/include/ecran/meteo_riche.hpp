// Noyau pur : la réponse « riche » d'Open-Meteo (courant complet, 24 heures, 7 jours, quarts d'heure de pluie), son contrôle,
// son cache binaire versionné pour la NVS et ce que l'écran en tire (heure en cours, pluie des prochaines minutes). C++17
// standard, aucun include ESP-IDF, aucun flottant : tout est en entiers (dixièmes de degré, de km/h, de mm…). La conversion en
// `float` du contrat `main/meteo_etat.h` se fait dans `main/meteo_etat.cpp`.
//
// Ce fichier AJOUTE au lecteur de base `meteo_openmeteo.hpp` (qui lit le courant et les 7 jours essentiels dans un `Releve`) ; il
// ne le modifie pas. `analyser_riche()` lance d'abord `analyser()` (même réponse, mêmes règles strictes), puis lit les ajouts :
//   - strictement, avec le courant : `current.precipitation` et `current.cloud_cover` (absents = réponse refusée) ;
//   - section par section, sans faire échouer le reste : `hourly` (24 heures), `daily` (compléments des 7 jours),
//     `minutely_15` (quarts d'heure de pluie). Une section absente, incomplète ou invraisemblable est VIDÉE (nb_… = 0) et dit
//     pourquoi dans `diag_heures`, `diag_jours` ou `diag_quarts` : la météo de base reste utilisable, jamais de valeur inventée.
//
// Requête attendue : voir `meteo_requete.hpp` (noms des variables vérifiés par appels réels le 07/11/2026, voir
// docs/mesures/meteo.md).
//
// CONVENTION D'OPEN-METEO SUR LES CUMULS (documentée pour l'horaire, recoupée sur l'API le 07/11/2026 pour le quart d'heure) :
// les précipitations, la probabilité de précipitations et l'indice UV moyen d'une ÉTIQUETTE `t` portent sur l'intervalle qui
// PRÉCÈDE `t` ((t - 1 h, t] ou (t - 15 min, t]). Le noyau recale ces valeurs sur l'intervalle qu'elles décrivent : `heures[i]`
// est l'heure [debut_s, debut_s + 3600) avec, pour les cumuls, la valeur de l'étiquette suivante ; `quarts_dixiemes[i]` est le
// quart d'heure [quarts_debut_s + 900 i, + 900) avec `quarts_debut_s` = première étiquette - 900 s. D'où l'API demandée avec une
// heure et un quart d'heure de plus que ce qu'on garde.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/meteo_openmeteo.hpp"
#include "ecran/meteo_requete.hpp"

namespace ecran::meteo {

inline constexpr int HEURES_MAX = 24;
inline constexpr int QUARTS_MAX = 9;
inline constexpr std::int64_t PAS_HEURE_S = 3600;
inline constexpr std::int64_t PAS_QUART_S = 900;
// Un quart d'heure « pluvieux » : au moins 0,1 mm (précision de l'API), bruine comprise.
inline constexpr std::int16_t SEUIL_PLUIE_DIXIEMES = 1;
// Au-delà, les quarts d'heure ne sont plus un « dans N minutes » sûr (ils restent lisibles).
inline constexpr std::int64_t DELAI_QUARTS_PERIMES_S = 30 * 60;

// Une heure de prévision. Les valeurs instantanées sont celles du DÉBUT de l'heure, les cumuls et la moyenne UV celles de
// l'heure entière (recalées, voir plus haut).
struct HeureRiche {
    std::int64_t debut_s = 0;
    std::int16_t temperature_dixiemes = 0;
    std::int16_t ressentie_dixiemes = 0;
    std::int16_t humidite_pct = 0;
    std::int16_t proba_pct = PROBA_INCONNUE;
    std::int16_t precip_dixiemes_mm = 0;
    std::int16_t code_wmo = 0;
    std::int16_t nuages_pct = 0;
    std::int16_t vent_dixiemes_kmh = 0;
    std::int16_t rafales_dixiemes_kmh = 0;
    std::int16_t direction_deg = 0;
    std::int16_t uv_dixiemes = 0;
    bool jour = false;
};

// Compléments d'un jour de `Releve::jours` (même rang). Le reste (code, min, max, lever, coucher, UV max, probabilité) est dans
// `Jour`.
struct JourRiche {
    std::int16_t ressentie_min_dixiemes = 0;
    std::int16_t ressentie_max_dixiemes = 0;
    std::int16_t ensoleillement_min = 0; // minutes d'ensoleillement prévues
    std::int16_t precip_dixiemes_mm = 0;
    std::int16_t neige_dixiemes_cm = 0;
    std::int16_t heures_precip = 0;
    std::int16_t vent_max_dixiemes_kmh = 0;
    std::int16_t rafales_max_dixiemes_kmh = 0;
    std::int16_t direction_dominante_deg = 0;
};

struct Riche {
    std::int16_t precip_courante_dixiemes = 0; // current.precipitation : le dernier quart d'heure écoulé
    std::int16_t nuages_pct = 0;               // current.cloud_cover
    std::uint8_t nb_heures = 0;
    HeureRiche heures[HEURES_MAX] = {};
    std::uint8_t nb_jours = 0; // 0 ou Releve::nb_jours
    JourRiche jours[JOURS_MAX] = {};
    std::uint8_t nb_quarts = 0;
    std::int64_t quarts_debut_s = 0;
    std::int16_t quarts_dixiemes[QUARTS_MAX] = {};
};

// Ce que l'appareil garde : le relevé de base et ses compléments.
struct Meteo {
    Releve releve{};
    Riche riche{};
};

struct AnalyseRiche {
    Meteo meteo{};
    Diagnostic diag{};        // réponse de base : si faux, rien n'est utilisable
    Diagnostic diag_heures{}; // pourquoi `heures` est vide (Aucune : tout va bien)
    Diagnostic diag_jours{};  // pourquoi les compléments des jours sont vides
    Diagnostic diag_quarts{}; // pourquoi les quarts d'heure sont vides
    constexpr bool ok() const noexcept { return diag.ok(); }
};

namespace detail {

constexpr bool dans(std::int64_t v, std::int64_t bas, std::int64_t haut) noexcept { return v >= bas && v <= haut; }

// ---- contrôle des compléments ----------------------------------------------------------------------------------------

constexpr Diagnostic verifier_courant_riche(const Riche &r) noexcept {
    if (!dans(r.precip_courante_dixiemes, 0, 1000))
        return echec(Erreur::HorsPlage, "current.precipitation");
    if (!dans(r.nuages_pct, 0, 100))
        return echec(Erreur::HorsPlage, "current.cloud_cover");
    return Diagnostic{};
}

constexpr Diagnostic verifier_heures_riche(const Riche &r) noexcept {
    if (r.nb_heures > HEURES_MAX)
        return echec(Erreur::HorsPlage, "hourly");
    for (std::size_t i = 0; i < r.nb_heures; ++i) {
        const HeureRiche &h = r.heures[i];
        if (!dans(h.debut_s, INSTANT_MIN, INSTANT_MAX))
            return echec(Erreur::HorsPlage, "hourly.time");
        if (i > 0 && h.debut_s - r.heures[i - 1].debut_s != PAS_HEURE_S)
            return echec(Erreur::TableauxIncoherents, "hourly.time");
        if (!dans(h.temperature_dixiemes, -900, 600))
            return echec(Erreur::HorsPlage, "hourly.temperature_2m");
        if (!dans(h.ressentie_dixiemes, -1200, 800))
            return echec(Erreur::HorsPlage, "hourly.apparent_temperature");
        if (!dans(h.humidite_pct, 0, 100))
            return echec(Erreur::HorsPlage, "hourly.relative_humidity_2m");
        if (!dans(h.proba_pct, PROBA_INCONNUE, 100))
            return echec(Erreur::HorsPlage, "hourly.precipitation_probability");
        if (!dans(h.precip_dixiemes_mm, 0, 2000))
            return echec(Erreur::HorsPlage, "hourly.precipitation");
        if (!dans(h.code_wmo, 0, 99))
            return echec(Erreur::HorsPlage, "hourly.weather_code");
        if (!dans(h.nuages_pct, 0, 100))
            return echec(Erreur::HorsPlage, "hourly.cloud_cover");
        if (!dans(h.vent_dixiemes_kmh, 0, 5000))
            return echec(Erreur::HorsPlage, "hourly.wind_speed_10m");
        if (!dans(h.rafales_dixiemes_kmh, 0, 5000))
            return echec(Erreur::HorsPlage, "hourly.wind_gusts_10m");
        if (!dans(h.direction_deg, 0, 360))
            return echec(Erreur::HorsPlage, "hourly.wind_direction_10m");
        if (!dans(h.uv_dixiemes, 0, 250))
            return echec(Erreur::HorsPlage, "hourly.uv_index");
    }
    return Diagnostic{};
}

constexpr Diagnostic verifier_jours_riche(const Releve &base, const Riche &r) noexcept {
    if (r.nb_jours != 0 && r.nb_jours != base.nb_jours)
        return echec(Erreur::TableauxIncoherents, "daily");
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        const JourRiche &j = r.jours[i];
        if (!dans(j.ressentie_min_dixiemes, -1200, 800) || !dans(j.ressentie_max_dixiemes, -1200, 800))
            return echec(Erreur::HorsPlage, "daily.apparent_temperature");
        if (j.ressentie_min_dixiemes > j.ressentie_max_dixiemes)
            return echec(Erreur::HorsPlage, "daily.apparent_temperature_min");
        if (!dans(j.ensoleillement_min, 0, 1500))
            return echec(Erreur::HorsPlage, "daily.sunshine_duration");
        if (!dans(j.precip_dixiemes_mm, 0, 10000))
            return echec(Erreur::HorsPlage, "daily.precipitation_sum");
        if (!dans(j.neige_dixiemes_cm, 0, 10000))
            return echec(Erreur::HorsPlage, "daily.snowfall_sum");
        if (!dans(j.heures_precip, 0, 24))
            return echec(Erreur::HorsPlage, "daily.precipitation_hours");
        if (!dans(j.vent_max_dixiemes_kmh, 0, 5000))
            return echec(Erreur::HorsPlage, "daily.wind_speed_10m_max");
        if (!dans(j.rafales_max_dixiemes_kmh, 0, 5000))
            return echec(Erreur::HorsPlage, "daily.wind_gusts_10m_max");
        if (!dans(j.direction_dominante_deg, 0, 360))
            return echec(Erreur::HorsPlage, "daily.wind_direction_10m_dominant");
    }
    return Diagnostic{};
}

constexpr Diagnostic verifier_quarts_riche(const Riche &r) noexcept {
    if (r.nb_quarts > QUARTS_MAX)
        return echec(Erreur::HorsPlage, "minutely_15");
    if (r.nb_quarts > 0 && !dans(r.quarts_debut_s, INSTANT_MIN, INSTANT_MAX))
        return echec(Erreur::HorsPlage, "minutely_15.time");
    for (std::size_t i = 0; i < r.nb_quarts; ++i) {
        if (!dans(r.quarts_dixiemes[i], 0, 1000))
            return echec(Erreur::HorsPlage, "minutely_15.precipitation");
    }
    return Diagnostic{};
}

// ---- lecture d'une section en colonnes ---------------------------------------------------------------------------------

// Parcourt les colonnes d'un bloc (« hourly », « daily », « minutely_15 ») : chaque colonne doit exister, être un tableau et
// avoir la longueur de la première lue.
struct Colonnes {
    Diagnostic diag{};
    std::size_t n = 0;
    bool premiere = true;

    constexpr json::Valeur prendre(const json::Valeur &bloc, std::string_view nom_court,
                                   std::string_view nom_complet) noexcept {
        const json::Valeur v = bloc.champ(nom_court);
        if (!diag.ok())
            return v;
        if (!v.existe() || v.est_nul()) {
            diag = echec(Erreur::ChampAbsent, nom_complet);
        } else if (v.genre() != json::Genre::Tableau) {
            diag = echec(Erreur::MauvaisType, nom_complet);
        } else if (premiere) {
            n = v.taille();
            premiere = false;
        } else if (v.taille() != n) {
            diag = echec(Erreur::TableauxIncoherents, nom_complet);
        }
        return v;
    }
};

// Le bloc lui-même : absent -> ChampAbsent ; pas un objet -> MauvaisType.
constexpr Diagnostic bloc_present(const json::Valeur &bloc, std::string_view nom) noexcept {
    if (!bloc.existe() || bloc.est_nul())
        return echec(Erreur::ChampAbsent, nom);
    if (bloc.genre() != json::Genre::Objet)
        return echec(Erreur::MauvaisType, nom);
    return Diagnostic{};
}

// Durée en secondes (Open-Meteo écrit « 7609.29 », deux décimales au plus) -> minutes, à la plus proche (la demi-minute exacte
// monte). On lit des CENTIÈMES de seconde pour que l'arrondi soit celui de la valeur écrite, pas celui d'une valeur déjà arrondie.
// Refuse l'absence, un type inattendu et une durée de plus de 100 000 s (plus d'un jour).
constexpr std::int16_t minutes_depuis_secondes(Lecteur &l, const json::Valeur &v, std::string_view nom) noexcept {
    if (!v.existe() || v.est_nul()) {
        l.rater(Erreur::ChampAbsent, nom);
        return 0;
    }
    if (v.genre() != json::Genre::Nombre) {
        l.rater(Erreur::MauvaisType, nom);
        return 0;
    }
    std::int64_t centiemes = 0;
    if (!v.en_fixe(2, centiemes) || centiemes < 0 || centiemes > 100000 * 100) {
        l.rater(Erreur::HorsPlage, nom);
        return 0;
    }
    return static_cast<std::int16_t>((centiemes + 3000) / 6000);
}

constexpr void lire_heures(const json::Valeur &racine, Riche &r, Diagnostic &diag) noexcept {
    const json::Valeur bloc = racine.champ("hourly");
    diag = bloc_present(bloc, "hourly");
    if (!diag.ok())
        return;
    Colonnes c;
    const json::Valeur temps = c.prendre(bloc, "time", "hourly.time");
    const json::Valeur temp = c.prendre(bloc, "temperature_2m", "hourly.temperature_2m");
    const json::Valeur ress = c.prendre(bloc, "apparent_temperature", "hourly.apparent_temperature");
    const json::Valeur hum = c.prendre(bloc, "relative_humidity_2m", "hourly.relative_humidity_2m");
    const json::Valeur pluie = c.prendre(bloc, "precipitation", "hourly.precipitation");
    const json::Valeur code = c.prendre(bloc, "weather_code", "hourly.weather_code");
    const json::Valeur nuages = c.prendre(bloc, "cloud_cover", "hourly.cloud_cover");
    const json::Valeur vent = c.prendre(bloc, "wind_speed_10m", "hourly.wind_speed_10m");
    const json::Valeur raf = c.prendre(bloc, "wind_gusts_10m", "hourly.wind_gusts_10m");
    const json::Valeur dir = c.prendre(bloc, "wind_direction_10m", "hourly.wind_direction_10m");
    const json::Valeur uv = c.prendre(bloc, "uv_index", "hourly.uv_index");
    const json::Valeur jour = c.prendre(bloc, "is_day", "hourly.is_day");
    // La probabilité de précipitations est FACULTATIVE (comme dans analyser()) : absente ou nulle, toutes les heures la
    // donnent « inconnue » ; présente, elle doit avoir la longueur des autres colonnes.
    const json::Valeur proba = bloc.champ("precipitation_probability");
    const bool proba_presente = proba.existe() && !proba.est_nul();
    if (c.diag.ok() && proba_presente) {
        if (proba.genre() != json::Genre::Tableau)
            c.diag = echec(Erreur::MauvaisType, "hourly.precipitation_probability");
        else if (proba.taille() != c.n)
            c.diag = echec(Erreur::TableauxIncoherents, "hourly.precipitation_probability");
    }
    if (!c.diag.ok()) {
        diag = c.diag;
        return;
    }
    // Il faut une étiquette de plus que d'heures gardées : les cumuls de l'heure [t, t + 1 h) sont à l'étiquette t + 1 h.
    if (c.n < 2) {
        diag = echec(Erreur::TableauxIncoherents, "hourly.time");
        return;
    }
    const std::size_t nb =
        (c.n - 1 < static_cast<std::size_t>(HEURES_MAX)) ? c.n - 1 : static_cast<std::size_t>(HEURES_MAX);
    Lecteur l;
    Riche essai;
    essai.nb_heures = static_cast<std::uint8_t>(nb);
    for (std::size_t i = 0; i < nb; ++i) {
        HeureRiche &h = essai.heures[i];
        h.debut_s = l.entier(temps.element(i), "hourly.time");
        h.temperature_dixiemes = l.fixe16(temp.element(i), "hourly.temperature_2m", 1);
        h.ressentie_dixiemes = l.fixe16(ress.element(i), "hourly.apparent_temperature", 1);
        h.humidite_pct = l.fixe16(hum.element(i), "hourly.relative_humidity_2m", 0);
        h.code_wmo = l.entier16(code.element(i), "hourly.weather_code");
        h.nuages_pct = l.fixe16(nuages.element(i), "hourly.cloud_cover", 0);
        h.vent_dixiemes_kmh = l.fixe16(vent.element(i), "hourly.wind_speed_10m", 1);
        h.rafales_dixiemes_kmh = l.fixe16(raf.element(i), "hourly.wind_gusts_10m", 1);
        h.direction_deg = l.fixe16(dir.element(i), "hourly.wind_direction_10m", 0);
        const std::int64_t j = l.entier(jour.element(i), "hourly.is_day");
        if (j != 0 && j != 1)
            l.rater(Erreur::HorsPlage, "hourly.is_day");
        h.jour = (j == 1);
        // Cumuls et moyenne : étiquette suivante (heure qui PRÉCÈDE l'étiquette).
        h.precip_dixiemes_mm = l.fixe16(pluie.element(i + 1), "hourly.precipitation", 1);
        h.uv_dixiemes = l.fixe16(uv.element(i + 1), "hourly.uv_index", 1);
        const json::Valeur p = proba_presente ? proba.element(i + 1) : json::Valeur{};
        h.proba_pct = (!p.existe() || p.est_nul()) ? static_cast<std::int16_t>(PROBA_INCONNUE)
                                                   : l.fixe16(p, "hourly.precipitation_probability", 0);
    }
    if (!l.diag.ok()) {
        diag = l.diag;
        return;
    }
    diag = verifier_heures_riche(essai);
    if (diag.ok()) {
        r.nb_heures = essai.nb_heures;
        for (std::size_t i = 0; i < nb; ++i)
            r.heures[i] = essai.heures[i];
    }
}

constexpr void lire_jours(const json::Valeur &racine, const Releve &base, Riche &r, Diagnostic &diag) noexcept {
    const json::Valeur bloc = racine.champ("daily");
    diag = bloc_present(bloc, "daily");
    if (!diag.ok())
        return;
    // analyser() a déjà refusé un `daily` sans jour : ici le relevé de base a au moins un jour.
    Colonnes c;
    c.prendre(bloc, "time", "daily.time");
    const json::Valeur rmax = c.prendre(bloc, "apparent_temperature_max", "daily.apparent_temperature_max");
    const json::Valeur rmin = c.prendre(bloc, "apparent_temperature_min", "daily.apparent_temperature_min");
    const json::Valeur soleil = c.prendre(bloc, "sunshine_duration", "daily.sunshine_duration");
    const json::Valeur pluie = c.prendre(bloc, "precipitation_sum", "daily.precipitation_sum");
    const json::Valeur neige = c.prendre(bloc, "snowfall_sum", "daily.snowfall_sum");
    const json::Valeur hpluie = c.prendre(bloc, "precipitation_hours", "daily.precipitation_hours");
    const json::Valeur vent = c.prendre(bloc, "wind_speed_10m_max", "daily.wind_speed_10m_max");
    const json::Valeur raf = c.prendre(bloc, "wind_gusts_10m_max", "daily.wind_gusts_10m_max");
    const json::Valeur dir = c.prendre(bloc, "wind_direction_10m_dominant", "daily.wind_direction_10m_dominant");
    if (!c.diag.ok()) {
        diag = c.diag;
        return;
    }
    // `daily.time` est la même colonne que celle que analyser() a lue pour les jours de base : mêmes dates, `c.n` >= nb_jours.
    Lecteur l;
    Riche essai;
    essai.nb_jours = base.nb_jours;
    for (std::size_t i = 0; i < base.nb_jours; ++i) {
        JourRiche &j = essai.jours[i];
        j.ressentie_max_dixiemes = l.fixe16(rmax.element(i), "daily.apparent_temperature_max", 1);
        j.ressentie_min_dixiemes = l.fixe16(rmin.element(i), "daily.apparent_temperature_min", 1);
        j.ensoleillement_min = minutes_depuis_secondes(l, soleil.element(i), "daily.sunshine_duration");
        j.precip_dixiemes_mm = l.fixe16(pluie.element(i), "daily.precipitation_sum", 1);
        j.neige_dixiemes_cm = l.fixe16(neige.element(i), "daily.snowfall_sum", 1);
        j.heures_precip = l.fixe16(hpluie.element(i), "daily.precipitation_hours", 0);
        j.vent_max_dixiemes_kmh = l.fixe16(vent.element(i), "daily.wind_speed_10m_max", 1);
        j.rafales_max_dixiemes_kmh = l.fixe16(raf.element(i), "daily.wind_gusts_10m_max", 1);
        j.direction_dominante_deg = l.fixe16(dir.element(i), "daily.wind_direction_10m_dominant", 0);
    }
    if (!l.diag.ok()) {
        diag = l.diag;
        return;
    }
    diag = verifier_jours_riche(base, essai);
    if (diag.ok()) {
        r.nb_jours = essai.nb_jours;
        for (std::size_t i = 0; i < base.nb_jours; ++i)
            r.jours[i] = essai.jours[i];
    }
}

constexpr void lire_quarts(const json::Valeur &racine, Riche &r, Diagnostic &diag) noexcept {
    const json::Valeur bloc = racine.champ("minutely_15");
    diag = bloc_present(bloc, "minutely_15");
    if (!diag.ok())
        return;
    Colonnes c;
    const json::Valeur temps = c.prendre(bloc, "time", "minutely_15.time");
    const json::Valeur pluie = c.prendre(bloc, "precipitation", "minutely_15.precipitation");
    if (!c.diag.ok()) {
        diag = c.diag;
        return;
    }
    if (c.n == 0) {
        diag = echec(Erreur::TableauxIncoherents, "minutely_15.time");
        return;
    }
    const std::size_t nb = (c.n < static_cast<std::size_t>(QUARTS_MAX)) ? c.n : static_cast<std::size_t>(QUARTS_MAX);
    Lecteur l;
    Riche essai;
    essai.nb_quarts = static_cast<std::uint8_t>(nb);
    std::int64_t precedent = 0;
    for (std::size_t i = 0; i < nb; ++i) {
        const std::int64_t t = l.entier(temps.element(i), "minutely_15.time");
        if (i == 0) {
            essai.quarts_debut_s = t - PAS_QUART_S; // l'étiquette décrit le quart d'heure qui la précède
        } else if (t - precedent != PAS_QUART_S) {
            l.rater(Erreur::TableauxIncoherents, "minutely_15.time");
        }
        precedent = t;
        essai.quarts_dixiemes[i] = l.fixe16(pluie.element(i), "minutely_15.precipitation", 1);
    }
    if (!l.diag.ok()) {
        diag = l.diag;
        return;
    }
    diag = verifier_quarts_riche(essai);
    if (diag.ok()) {
        r.nb_quarts = essai.nb_quarts;
        r.quarts_debut_s = essai.quarts_debut_s;
        for (std::size_t i = 0; i < nb; ++i)
            r.quarts_dixiemes[i] = essai.quarts_dixiemes[i];
    }
}

} // namespace detail

// Vérifie un `Meteo` déjà construit (plages et cohérence) : le relevé de base, le courant, puis chaque section. Une section
// invraisemblable est refusée avec son champ ; une section VIDE (nb_… = 0) est acceptée.
constexpr Diagnostic verifier_meteo(const Meteo &m) noexcept {
    Diagnostic d = verifier(m.releve);
    if (!d.ok())
        return d;
    d = detail::verifier_courant_riche(m.riche);
    if (!d.ok())
        return d;
    d = detail::verifier_heures_riche(m.riche);
    if (!d.ok())
        return d;
    d = detail::verifier_jours_riche(m.releve, m.riche);
    if (!d.ok())
        return d;
    return detail::verifier_quarts_riche(m.riche);
}

// Analyse la réponse JSON riche. `recu_a_s` : heure (UNIX) de réception, lue par l'appelant sur son horloge réglée.
constexpr AnalyseRiche analyser_riche(std::string_view texte, std::int64_t recu_a_s) noexcept {
    AnalyseRiche a;
    const Analyse base = analyser(texte, recu_a_s);
    a.meteo.releve = base.releve;
    a.diag = base.diag;
    if (!a.diag.ok())
        return a;
    const json::Resultat lu = json::lire(texte); // analyser() l'a déjà accepté
    const json::Valeur racine = lu.valeur;

    detail::Lecteur l;
    const json::Valeur cur = racine.champ("current");
    a.meteo.riche.precip_courante_dixiemes = l.fixe16(cur.champ("precipitation"), "current.precipitation", 1);
    a.meteo.riche.nuages_pct = l.fixe16(cur.champ("cloud_cover"), "current.cloud_cover", 0);
    if (!l.diag.ok()) {
        a.diag = l.diag;
        return a;
    }
    a.diag = detail::verifier_courant_riche(a.meteo.riche);
    if (!a.diag.ok())
        return a;

    detail::lire_heures(racine, a.meteo.riche, a.diag_heures);
    detail::lire_jours(racine, a.meteo.releve, a.meteo.riche, a.diag_jours);
    detail::lire_quarts(racine, a.meteo.riche, a.diag_quarts);
    return a;
}

// ---- ce que l'écran en tire --------------------------------------------------------------------------------------------

// Rang de l'heure de `riche.heures` qui contient `maintenant_s`, ou -1.
constexpr int heure_courante(const Riche &r, std::int64_t maintenant_s) noexcept {
    for (int i = 0; i < static_cast<int>(r.nb_heures); ++i) {
        if (maintenant_s >= r.heures[i].debut_s && maintenant_s < r.heures[i].debut_s + PAS_HEURE_S)
            return i;
    }
    return -1;
}

// Rang du quart d'heure qui contient `maintenant_s`, ou -1.
constexpr int quart_courant(const Riche &r, std::int64_t maintenant_s) noexcept {
    for (int i = 0; i < static_cast<int>(r.nb_quarts); ++i) {
        const std::int64_t debut = r.quarts_debut_s + PAS_QUART_S * i;
        if (maintenant_s >= debut && maintenant_s < debut + PAS_QUART_S)
            return i;
    }
    return -1;
}

// Probabilité de précipitations de l'heure en cours (celle des heures recalées), ou PROBA_INCONNUE.
constexpr int proba_courante(const Riche &r, std::int64_t maintenant_s) noexcept {
    const int i = heure_courante(r, maintenant_s);
    return i < 0 ? PROBA_INCONNUE : r.heures[i].proba_pct;
}

struct PluieProche {
    bool connue = false;   // des quarts d'heure existent ET l'heure de l'appareil les couvre
    bool perimee = false;  // reçus il y a plus de DELAI_QUARTS_PERIMES_S
    bool en_cours = false; // le quart d'heure en cours est pluvieux
    int dans_min = -1;     // minutes avant le début de la prochaine pluie, 0 si en cours, -1 si aucune dans la fenêtre
    int fin_dans_min = -1; // minutes avant la fin de cette pluie ; -1 si aucune ou si elle dépasse la fenêtre
    int fenetre_min = 0;   // minutes couvertes devant `maintenant_s`
    int cumul_dixiemes = 0;
    int max_quart_dixiemes = 0;
};

namespace detail {
constexpr int minutes_au_plus_pres_sup(std::int64_t secondes) noexcept {
    return secondes <= 0 ? 0 : static_cast<int>((secondes + 59) / 60);
}
} // namespace detail

// Pluie des prochaines minutes à l'heure `maintenant_s` de l'appareil. Le quart d'heure en cours compte EN ENTIER (on ne sait
// pas à quel moment du quart d'heure il pleut). `recu_a_s` sert à juger l'âge. Horloge non réglée (avant l'an 2000) : aucun
// quart d'heure ne la contient ; horloge en retard sur la réception de plus de TOLERANCE_HORLOGE_S : `connue` est faux. Jamais
// une réponse au hasard.
constexpr PluieProche pluie_proche(const Riche &r, std::int64_t recu_a_s, std::int64_t maintenant_s) noexcept {
    PluieProche p;
    if (r.nb_quarts == 0 || maintenant_s + TOLERANCE_HORLOGE_S < recu_a_s)
        return p;
    const int q = quart_courant(r, maintenant_s);
    if (q < 0)
        return p; // tous les quarts sont passés (ou pas encore commencés) : rien à dire
    p.connue = true;
    p.perimee = (maintenant_s - recu_a_s) > DELAI_QUARTS_PERIMES_S;
    const int n = static_cast<int>(r.nb_quarts);
    const std::int64_t fin_fenetre = r.quarts_debut_s + PAS_QUART_S * n;
    p.fenetre_min = detail::minutes_au_plus_pres_sup(fin_fenetre - maintenant_s);
    p.en_cours = r.quarts_dixiemes[q] >= SEUIL_PLUIE_DIXIEMES;
    int debut_pluie = -1;
    for (int i = q; i < n; ++i) {
        p.cumul_dixiemes += r.quarts_dixiemes[i];
        if (r.quarts_dixiemes[i] > p.max_quart_dixiemes)
            p.max_quart_dixiemes = r.quarts_dixiemes[i];
        if (debut_pluie < 0 && r.quarts_dixiemes[i] >= SEUIL_PLUIE_DIXIEMES)
            debut_pluie = i;
    }
    if (debut_pluie < 0)
        return p;
    const std::int64_t debut_s = r.quarts_debut_s + PAS_QUART_S * debut_pluie;
    p.dans_min = debut_pluie == q ? 0 : detail::minutes_au_plus_pres_sup(debut_s - maintenant_s);
    int fin = debut_pluie;
    while (fin < n && r.quarts_dixiemes[fin] >= SEUIL_PLUIE_DIXIEMES)
        ++fin;
    if (fin < n) // la pluie s'arrête dans la fenêtre : au début du premier quart sec
        p.fin_dans_min = detail::minutes_au_plus_pres_sup(r.quarts_debut_s + PAS_QUART_S * fin - maintenant_s);
    return p;
}

// ---- cache binaire versionné (NVS) -------------------------------------------------------------------------------------
//
// Format v2, petit-boutiste : 'Z' 'K' | version(2) | latitude_e5(i32) | longitude_e5(i32) | longueur du relevé(u16) |
// `serialiser()` du relevé (format de meteo_openmeteo.hpp, avec sa marque, sa version et sa somme de contrôle) | 'M' 'R' |
// version du bloc riche(1) | précipitation courante(i16) nuages(u8) | nb_heures(u8) et, si > 0, première étiquette(u32) puis par
// heure : temp(i16) ressentie(i16) humidité(u8) proba(i8) pluie(i16) code(u8) nuages(u8) vent(i16) rafales(i16) direction(i16)
// uv(i16) jour(u8) | nb_jours(u8) et par jour : ressentie min(i16) max(i16) ensoleillement(i16) pluie(i16) neige(i16) heures(u8)
// vent(i16) rafales(i16) direction(i16) | nb_quarts(u8) et, si > 0, début(u32) puis par quart : pluie(i16) | CRC-32 de tout ce
// qui précède (4 octets).
//
// Un cache d'une AUTRE version (le v1 'Z' 'K' 1 d'avant, ou un futur v3) est refusé par `Erreur::VersionInconnue` AVANT toute
// lecture de son contenu : jamais lu de travers. Le client le remplace à la prochaine réponse.
inline constexpr std::uint8_t VERSION_CACHE_RICHE = 2;
inline constexpr std::size_t OCTETS_PAR_HEURE_CACHE = 19;
inline constexpr std::size_t OCTETS_PAR_JOUR_CACHE = 17;
inline constexpr std::size_t TAILLE_CACHE_RICHE_MAX = 3 + 4 + 4 + 2 + TAILLE_CACHE_MAX + 3 + 3 + 1 + 4 +
                                                      OCTETS_PAR_HEURE_CACHE * HEURES_MAX + 1 +
                                                      OCTETS_PAR_JOUR_CACHE * JOURS_MAX + 1 + 4 + 2 * QUARTS_MAX + 4;

constexpr std::size_t serialiser_cache_meteo(const Meteo &m, std::int32_t latitude_e5, std::int32_t longitude_e5,
                                             std::uint8_t *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || !verifier_meteo(m).ok())
        return 0;
    if (latitude_e5 < -LATITUDE_E5_MAX || latitude_e5 > LATITUDE_E5_MAX || longitude_e5 < -LONGITUDE_E5_MAX ||
        longitude_e5 > LONGITUDE_E5_MAX) {
        return 0; // une position invraisemblable ne se met pas en cache
    }
    constexpr std::size_t ENTETE = 3 + 4 + 4 + 2;
    if (capacite < ENTETE + 4)
        return 0;
    detail::Ecrivain w{sortie, capacite};
    w.u8('Z');
    w.u8('K');
    w.u8(VERSION_CACHE_RICHE);
    w.u32(static_cast<std::uint32_t>(latitude_e5));
    w.u32(static_cast<std::uint32_t>(longitude_e5));
    const std::size_t taille_releve = serialiser(m.releve, sortie + ENTETE, capacite - ENTETE - 4);
    if (taille_releve == 0)
        return 0;
    w.u16(static_cast<std::uint32_t>(taille_releve));
    w.n += taille_releve;
    const Riche &r = m.riche;
    w.u8('M');
    w.u8('R');
    w.u8(1);
    w.u16(static_cast<std::uint16_t>(r.precip_courante_dixiemes));
    w.u8(static_cast<std::uint8_t>(r.nuages_pct));
    w.u8(r.nb_heures);
    if (r.nb_heures > 0)
        w.u32(static_cast<std::uint32_t>(r.heures[0].debut_s));
    for (std::size_t i = 0; i < r.nb_heures; ++i) {
        const HeureRiche &h = r.heures[i];
        w.u16(static_cast<std::uint16_t>(h.temperature_dixiemes));
        w.u16(static_cast<std::uint16_t>(h.ressentie_dixiemes));
        w.u8(static_cast<std::uint8_t>(h.humidite_pct));
        w.u8(static_cast<std::uint8_t>(h.proba_pct));
        w.u16(static_cast<std::uint16_t>(h.precip_dixiemes_mm));
        w.u8(static_cast<std::uint8_t>(h.code_wmo));
        w.u8(static_cast<std::uint8_t>(h.nuages_pct));
        w.u16(static_cast<std::uint16_t>(h.vent_dixiemes_kmh));
        w.u16(static_cast<std::uint16_t>(h.rafales_dixiemes_kmh));
        w.u16(static_cast<std::uint16_t>(h.direction_deg));
        w.u16(static_cast<std::uint16_t>(h.uv_dixiemes));
        w.u8(h.jour ? 1U : 0U);
    }
    w.u8(r.nb_jours);
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        const JourRiche &j = r.jours[i];
        w.u16(static_cast<std::uint16_t>(j.ressentie_min_dixiemes));
        w.u16(static_cast<std::uint16_t>(j.ressentie_max_dixiemes));
        w.u16(static_cast<std::uint16_t>(j.ensoleillement_min));
        w.u16(static_cast<std::uint16_t>(j.precip_dixiemes_mm));
        w.u16(static_cast<std::uint16_t>(j.neige_dixiemes_cm));
        w.u8(static_cast<std::uint8_t>(j.heures_precip));
        w.u16(static_cast<std::uint16_t>(j.vent_max_dixiemes_kmh));
        w.u16(static_cast<std::uint16_t>(j.rafales_max_dixiemes_kmh));
        w.u16(static_cast<std::uint16_t>(j.direction_dominante_deg));
    }
    w.u8(r.nb_quarts);
    if (r.nb_quarts > 0)
        w.u32(static_cast<std::uint32_t>(r.quarts_debut_s));
    for (std::size_t i = 0; i < r.nb_quarts; ++i)
        w.u16(static_cast<std::uint16_t>(r.quarts_dixiemes[i]));
    if (w.trop)
        return 0;
    w.u32(detail::crc32(sortie, w.n));
    return w.trop ? 0 : w.n;
}

struct LectureCacheMeteo {
    Meteo meteo{};
    std::int32_t latitude_e5 = 0;
    std::int32_t longitude_e5 = 0;
    Diagnostic diag{};
    constexpr bool ok() const noexcept { return diag.ok(); }
};

constexpr LectureCacheMeteo deserialiser_cache_meteo(const std::uint8_t *donnees, std::size_t taille) noexcept {
    constexpr std::size_t ENTETE = 3 + 4 + 4 + 2;
    LectureCacheMeteo out;
    if (donnees == nullptr || taille < ENTETE + 4) {
        out.diag = detail::echec(Erreur::Corrompu, "cache");
        return out;
    }
    if (donnees[0] != 'Z' || donnees[1] != 'K') {
        out.diag = detail::echec(Erreur::Corrompu, "cache.marque");
        return out;
    }
    if (donnees[2] != VERSION_CACHE_RICHE) { // avant toute autre lecture : un autre format n'est jamais interprété
        out.diag = detail::echec(Erreur::VersionInconnue, "cache.version");
        return out;
    }
    const std::uint32_t attendu = static_cast<std::uint32_t>(donnees[taille - 4]) |
                                  (static_cast<std::uint32_t>(donnees[taille - 3]) << 8) |
                                  (static_cast<std::uint32_t>(donnees[taille - 2]) << 16) |
                                  (static_cast<std::uint32_t>(donnees[taille - 1]) << 24);
    if (detail::crc32(donnees, taille - 4) != attendu) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.crc");
        return out;
    }
    detail::Lecteur32 rd{donnees, taille - 4};
    rd.n = 3;
    const std::int32_t lat = rd.i32();
    const std::int32_t lon = rd.i32();
    if (lat < -LATITUDE_E5_MAX || lat > LATITUDE_E5_MAX || lon < -LONGITUDE_E5_MAX || lon > LONGITUDE_E5_MAX) {
        out.diag = detail::echec(Erreur::HorsPlage, "cache.position");
        return out;
    }
    const std::size_t taille_releve = rd.u16();
    if (rd.trop || taille_releve == 0 || ENTETE + taille_releve > taille - 4) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.releve");
        return out;
    }
    const Lecture lu = deserialiser(donnees + ENTETE, taille_releve);
    if (!lu.ok()) {
        out.diag = lu.diag;
        return out;
    }
    out.meteo.releve = lu.releve;
    rd.n = ENTETE + taille_releve;
    if (rd.u8() != 'M' || rd.u8() != 'R') {
        out.diag = detail::echec(Erreur::Corrompu, "cache.riche.marque");
        return out;
    }
    if (rd.u8() != 1) {
        out.diag = detail::echec(Erreur::VersionInconnue, "cache.riche.version");
        return out;
    }
    Riche &r = out.meteo.riche;
    r.precip_courante_dixiemes = rd.i16();
    r.nuages_pct = static_cast<std::int16_t>(rd.u8());
    const std::uint32_t nb_h = rd.u8();
    if (nb_h > static_cast<std::uint32_t>(HEURES_MAX)) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.heures");
        return out;
    }
    r.nb_heures = static_cast<std::uint8_t>(nb_h);
    std::int64_t premiere = 0;
    if (nb_h > 0)
        premiere = rd.u32();
    for (std::uint32_t i = 0; i < nb_h; ++i) {
        HeureRiche &h = r.heures[i];
        h.debut_s = premiere + PAS_HEURE_S * static_cast<std::int64_t>(i);
        h.temperature_dixiemes = rd.i16();
        h.ressentie_dixiemes = rd.i16();
        h.humidite_pct = static_cast<std::int16_t>(rd.u8());
        h.proba_pct = static_cast<std::int16_t>(rd.i8());
        h.precip_dixiemes_mm = rd.i16();
        h.code_wmo = static_cast<std::int16_t>(rd.u8());
        h.nuages_pct = static_cast<std::int16_t>(rd.u8());
        h.vent_dixiemes_kmh = rd.i16();
        h.rafales_dixiemes_kmh = rd.i16();
        h.direction_deg = rd.i16();
        h.uv_dixiemes = rd.i16();
        h.jour = rd.u8() != 0;
    }
    const std::uint32_t nb_j = rd.u8();
    if (nb_j > static_cast<std::uint32_t>(JOURS_MAX)) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.jours");
        return out;
    }
    r.nb_jours = static_cast<std::uint8_t>(nb_j);
    for (std::uint32_t i = 0; i < nb_j; ++i) {
        JourRiche &j = r.jours[i];
        j.ressentie_min_dixiemes = rd.i16();
        j.ressentie_max_dixiemes = rd.i16();
        j.ensoleillement_min = rd.i16();
        j.precip_dixiemes_mm = rd.i16();
        j.neige_dixiemes_cm = rd.i16();
        j.heures_precip = static_cast<std::int16_t>(rd.u8());
        j.vent_max_dixiemes_kmh = rd.i16();
        j.rafales_max_dixiemes_kmh = rd.i16();
        j.direction_dominante_deg = rd.i16();
    }
    const std::uint32_t nb_q = rd.u8();
    if (nb_q > static_cast<std::uint32_t>(QUARTS_MAX)) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.quarts");
        return out;
    }
    r.nb_quarts = static_cast<std::uint8_t>(nb_q);
    if (nb_q > 0)
        r.quarts_debut_s = rd.u32();
    for (std::uint32_t i = 0; i < nb_q; ++i)
        r.quarts_dixiemes[i] = rd.i16();
    if (rd.trop || rd.n != taille - 4) { // trop court, ou octets en trop avant la somme
        out.diag = detail::echec(Erreur::Corrompu, "cache.taille");
        return out;
    }
    out.diag = verifier_meteo(out.meteo);
    if (out.diag.ok()) {
        out.latitude_e5 = lat;
        out.longitude_e5 = lon;
    }
    return out;
}

} // namespace ecran::meteo
