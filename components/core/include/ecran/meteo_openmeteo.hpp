// Noyau pur : réponse JSON d'Open-Meteo -> structure `Releve` contrôlée, cache binaire pour la NVS, logique « périmé ».
//
// Requête attendue (recherche 03, 2 839 octets mesurés ; paramètres exacts dans docs/recherche/03-...) :
//   current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,wind_direction_10m,wind_gusts_10m,
//           pressure_msl,relative_humidity_2m
//   hourly=temperature_2m,weather_code,precipitation_probability,uv_index&forecast_hours=48
//   daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,precipitation_probability_max
//         &forecast_days=7&timezone=auto&timeformat=unixtime
//
// Règles : jamais de valeur inventée. Un champ de `current` absent, nul, d'un autre type ou invraisemblable rend une
// erreur (avec le nom du champ). Le bloc `daily` est facultatif (0 jour) mais, s'il est là, il doit être complet et
// cohérent. Le bloc `hourly` ne sert qu'à lire la probabilité de précipitations de l'heure en cours ; s'il manque ou
// ne couvre pas l'instant, la valeur est PROBA_INCONNUE (-1), pas 0. L'API ne donne PAS de hauteur de précipitations
// dans cette requête : « précipitations » veut dire ici « probabilité en % ».
// Unités : températures, vent, rafales, pression et UV en DIXIÈMES (116 = 11,6 °C), entiers : pas de flottant sur
// l'appareil, pas de printf("%f"). Instants en secondes UNIX (UTC). Horodatage de réception INJECTÉ par l'appelant
// (`recu_a_s`) : aucune horloge système dans le noyau.
//
// Lever/coucher : en nuit polaire, Open-Meteo rend lever == coucher == minuit local du jour (constaté le 06/11/2026
// à Longyearbyen). `Jour::sans_soleil()` le dit. Le jour polaire (soleil de minuit) : comportement NON VÉRIFIÉ.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/json_mini.hpp"

namespace ecran::meteo {

inline constexpr int JOURS_MAX = 7;
inline constexpr int PROBA_INCONNUE = -1;
inline constexpr std::size_t FUSEAU_MAX = 48;  // octets, zéro final compris (le plus long nom IANA fait 32)

struct Jour {
    std::int64_t date_s = 0;          // minuit local du jour, en instant UNIX
    std::int16_t code_wmo = 0;
    std::int16_t min_dixiemes = 0;
    std::int16_t max_dixiemes = 0;
    std::int64_t lever_s = 0;
    std::int64_t coucher_s = 0;
    std::int16_t uv_max_dixiemes = 0;
    std::int8_t proba_precip_max = 0;  // %

    constexpr bool sans_soleil() const noexcept { return lever_s == coucher_s; }  // nuit polaire
};

struct Releve {
    std::int64_t recu_a_s = 0;        // quand l'appareil a reçu la réponse (injecté)
    std::int64_t instant_s = 0;       // current.time : heure de la mesure
    std::int32_t decalage_utc_s = 0;  // utc_offset_seconds au moment de la réponse
    char fuseau[FUSEAU_MAX] = {};     // nom IANA (« Europe/Paris »), vide si la réponse n'en donne pas
    std::int16_t temperature_dixiemes = 0;
    std::int16_t ressentie_dixiemes = 0;
    std::int16_t code_wmo = 0;
    bool jour = false;                // is_day
    std::int16_t vent_dixiemes_kmh = 0;
    std::int16_t rafales_dixiemes_kmh = 0;
    std::int16_t direction_vent_deg = 0;  // d'où vient le vent, 0..360
    std::int16_t pression_dixiemes_hpa = 0;
    std::int8_t humidite_pct = 0;
    std::int8_t proba_precip_pct = PROBA_INCONNUE;  // heure en cours, % ou PROBA_INCONNUE
    std::uint8_t nb_jours = 0;
    Jour jours[JOURS_MAX] = {};
};

enum class Erreur : std::uint8_t {
    Aucune,
    JsonInvalide,         // le texte n'est pas du JSON
    ChampAbsent,          // champ manquant ou nul
    MauvaisType,          // texte au lieu d'un nombre, décimal là où un entier est exigé…
    HorsPlage,            // valeur invraisemblable
    TableauxIncoherents,  // les tableaux de `daily` n'ont pas la même longueur, dates non consécutives…
    Corrompu,             // cache binaire : taille, marque ou somme de contrôle fausse
    VersionInconnue,      // cache binaire d'un autre format
};

// Phrase courte pour l'écran (jamais un code ni un état brut).
constexpr std::string_view message_court(Erreur e) noexcept {
    switch (e) {
        case Erreur::Aucune: return "Météo à jour";
        case Erreur::JsonInvalide: return "Réponse météo illisible";
        case Erreur::ChampAbsent: return "Donnée météo manquante";
        case Erreur::MauvaisType: return "Donnée météo inattendue";
        case Erreur::HorsPlage: return "Donnée météo invraisemblable";
        case Erreur::TableauxIncoherents: return "Prévisions incohérentes";
        case Erreur::Corrompu: return "Cache météo corrompu";
        case Erreur::VersionInconnue: return "Cache météo trop ancien";
    }
    return "Météo indisponible";
}

struct Diagnostic {
    Erreur erreur = Erreur::Aucune;
    std::string_view champ{};  // pour le journal : « current.temperature_2m » (texte statique)
    constexpr bool ok() const noexcept { return erreur == Erreur::Aucune; }
};

struct Analyse {
    Releve releve{};
    Diagnostic diag{};
    constexpr bool ok() const noexcept { return diag.ok(); }
};

namespace detail {

constexpr std::int64_t INSTANT_MIN = 946684800;   // 2000-01-01
constexpr std::int64_t INSTANT_MAX = 4102444800;  // 2100-01-01 (tient dans 32 bits non signés)

constexpr Diagnostic echec(Erreur e, std::string_view champ) noexcept { return Diagnostic{e, champ}; }

// Cohérence d'un relevé : plages de chaque champ et liens entre champs. Sert après l'analyse JSON ET après la
// lecture du cache binaire (un cache altéré mais de bonne somme de contrôle reste refusé s'il est invraisemblable).
constexpr Diagnostic verifier_champs(const Releve& r) noexcept {
    const auto hors = [](std::int64_t v, std::int64_t bas, std::int64_t haut) { return v < bas || v > haut; };
    if (hors(r.recu_a_s, INSTANT_MIN, INSTANT_MAX)) return echec(Erreur::HorsPlage, "recu_a_s");
    if (hors(r.instant_s, INSTANT_MIN, INSTANT_MAX)) return echec(Erreur::HorsPlage, "current.time");
    if (hors(r.decalage_utc_s, -43200, 50400)) return echec(Erreur::HorsPlage, "utc_offset_seconds");
    bool fin_vue = false;  // un nom IANA : lettres, chiffres, « / _ - + », terminé par un zéro
    for (std::size_t i = 0; i < FUSEAU_MAX; ++i) {
        const char c = r.fuseau[i];
        if (fin_vue) {
            if (c != '\0') return echec(Erreur::HorsPlage, "timezone");
        } else if (c == '\0') {
            fin_vue = true;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
                     c == '_' || c == '-' || c == '+')) {
            return echec(Erreur::HorsPlage, "timezone");
        }
    }
    if (!fin_vue) return echec(Erreur::HorsPlage, "timezone");
    if (hors(r.temperature_dixiemes, -900, 600)) return echec(Erreur::HorsPlage, "current.temperature_2m");
    if (hors(r.ressentie_dixiemes, -1200, 800)) return echec(Erreur::HorsPlage, "current.apparent_temperature");
    if (hors(r.code_wmo, 0, 99)) return echec(Erreur::HorsPlage, "current.weather_code");
    if (hors(r.vent_dixiemes_kmh, 0, 5000)) return echec(Erreur::HorsPlage, "current.wind_speed_10m");
    if (hors(r.rafales_dixiemes_kmh, 0, 5000)) return echec(Erreur::HorsPlage, "current.wind_gusts_10m");
    if (hors(r.direction_vent_deg, 0, 360)) return echec(Erreur::HorsPlage, "current.wind_direction_10m");
    if (hors(r.pression_dixiemes_hpa, 8000, 11000)) return echec(Erreur::HorsPlage, "current.pressure_msl");
    if (hors(r.humidite_pct, 0, 100)) return echec(Erreur::HorsPlage, "current.relative_humidity_2m");
    if (hors(r.proba_precip_pct, PROBA_INCONNUE, 100)) return echec(Erreur::HorsPlage, "hourly.precipitation_probability");
    if (r.nb_jours > JOURS_MAX) return echec(Erreur::HorsPlage, "daily");
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        const Jour& j = r.jours[i];
        if (hors(j.date_s, INSTANT_MIN, INSTANT_MAX)) return echec(Erreur::HorsPlage, "daily.time");
        if (hors(j.code_wmo, 0, 99)) return echec(Erreur::HorsPlage, "daily.weather_code");
        if (hors(j.max_dixiemes, -900, 600)) return echec(Erreur::HorsPlage, "daily.temperature_2m_max");
        if (hors(j.min_dixiemes, -900, 600)) return echec(Erreur::HorsPlage, "daily.temperature_2m_min");
        if (j.min_dixiemes > j.max_dixiemes) return echec(Erreur::HorsPlage, "daily.temperature_2m_min");
        if (hors(j.lever_s, INSTANT_MIN, INSTANT_MAX)) return echec(Erreur::HorsPlage, "daily.sunrise");
        if (hors(j.coucher_s, INSTANT_MIN, INSTANT_MAX)) return echec(Erreur::HorsPlage, "daily.sunset");
        if (j.lever_s > j.coucher_s) return echec(Erreur::HorsPlage, "daily.sunset");
        if (hors(j.uv_max_dixiemes, 0, 250)) return echec(Erreur::HorsPlage, "daily.uv_index_max");
        if (hors(j.proba_precip_max, 0, 100)) return echec(Erreur::HorsPlage, "daily.precipitation_probability_max");
        if (i > 0) {  // un jour dure 23, 24 ou 25 heures (changement d'heure)
            const std::int64_t ecart = j.date_s - r.jours[i - 1].date_s;
            if (ecart < 82800 || ecart > 90000) return echec(Erreur::TableauxIncoherents, "daily.time");
        }
    }
    return Diagnostic{};
}

// ---- lecture d'un champ JSON ------------------------------------------------------------------------------------

struct Lecteur {
    Diagnostic diag{};

    constexpr void rater(Erreur e, std::string_view champ) noexcept {
        if (diag.ok()) diag = Diagnostic{e, champ};
    }

    // Nombre décimal en dixièmes (ou entier brut si decimales == 0), qui doit tenir dans int16.
    constexpr std::int16_t fixe16(const json::Valeur& v, std::string_view nom, int decimales) noexcept {
        if (!v.existe() || v.est_nul()) {
            rater(Erreur::ChampAbsent, nom);
            return 0;
        }
        std::int64_t n = 0;
        if (v.genre() != json::Genre::Nombre) {
            rater(Erreur::MauvaisType, nom);
            return 0;
        }
        if (!v.en_fixe(decimales, n) || n < INT16_MIN || n > INT16_MAX) {
            rater(Erreur::HorsPlage, nom);
            return 0;
        }
        return static_cast<std::int16_t>(n);
    }

    // Entier écrit comme tel dans le JSON (« 3 », pas « 3.0 »).
    constexpr std::int64_t entier(const json::Valeur& v, std::string_view nom) noexcept {
        if (!v.existe() || v.est_nul()) {
            rater(Erreur::ChampAbsent, nom);
            return 0;
        }
        std::int64_t n = 0;
        if (v.genre() != json::Genre::Nombre || !v.en_entier(n)) {
            rater(Erreur::MauvaisType, nom);
            return 0;
        }
        return n;
    }

    // Entier strict qui doit tenir dans int16 (codes météo).
    constexpr std::int16_t entier16(const json::Valeur& v, std::string_view nom) noexcept {
        const std::int64_t n = entier(v, nom);
        if (n < INT16_MIN || n > INT16_MAX) {
            rater(Erreur::HorsPlage, nom);
            return 0;
        }
        return static_cast<std::int16_t>(n);
    }
};

// Probabilité de précipitations (%) de la case horaire de `hourly` qui contient `instant_s`. Facultative : PROBA_INCONNUE
// si `hourly` ou sa probabilité manque, est nulle à cet endroit, ou ne couvre pas l'instant ; erreur si les tableaux
// sont incohérents ou la valeur invraisemblable.
constexpr std::int8_t proba_a(const json::Valeur& racine, std::int64_t instant_s, Lecteur& l) noexcept {
    const json::Valeur horaire = racine.champ("hourly");
    if (!horaire.existe() || horaire.est_nul()) return PROBA_INCONNUE;
    if (horaire.genre() != json::Genre::Objet) {
        l.rater(Erreur::MauvaisType, "hourly");
        return PROBA_INCONNUE;
    }
    const json::Valeur temps = horaire.champ("time");
    const json::Valeur proba = horaire.champ("precipitation_probability");
    if (!proba.existe() || proba.est_nul()) return PROBA_INCONNUE;
    if (temps.genre() != json::Genre::Tableau || proba.genre() != json::Genre::Tableau) {
        l.rater(Erreur::MauvaisType, "hourly.precipitation_probability");
        return PROBA_INCONNUE;
    }
    if (temps.taille() != proba.taille()) {
        l.rater(Erreur::TableauxIncoherents, "hourly.precipitation_probability");
        return PROBA_INCONNUE;
    }
    const std::size_t n = temps.taille();
    for (std::size_t i = 0; i < n; ++i) {
        std::int64_t t = 0;
        if (!temps.element(i).en_entier(t)) {
            l.rater(Erreur::MauvaisType, "hourly.time");
            return PROBA_INCONNUE;
        }
        if (t <= instant_s && instant_s < t + 3600) {  // la case horaire qui contient l'instant
            const json::Valeur p = proba.element(i);
            if (!p.existe() || p.est_nul()) return PROBA_INCONNUE;
            const std::int16_t v = l.fixe16(p, "hourly.precipitation_probability", 0);
            return static_cast<std::int8_t>(v < 0 || v > 100 ? -2 : v);  // -2 : refusé par verifier
        }
    }
    return PROBA_INCONNUE;
}

}  // namespace detail

// Analyse la réponse JSON. `recu_a_s` : heure (UNIX) de réception, lue par l'appelant sur son horloge réglée.
constexpr Analyse analyser(std::string_view texte, std::int64_t recu_a_s) noexcept {
    Analyse a;
    const json::Resultat lu = json::lire(texte);
    if (!lu.ok()) {
        a.diag = detail::echec(Erreur::JsonInvalide, "json");
        return a;
    }
    const json::Valeur racine = lu.valeur;
    if (racine.genre() != json::Genre::Objet) {
        a.diag = detail::echec(Erreur::JsonInvalide, "json");
        return a;
    }
    detail::Lecteur l;
    Releve& r = a.releve;
    r.recu_a_s = recu_a_s;
    const std::int64_t decalage = l.entier(racine.champ("utc_offset_seconds"), "utc_offset_seconds");
    if (decalage < INT32_MIN || decalage > INT32_MAX) {  // avant de rétrécir : 2^32 + 3600 ne doit pas devenir 3600
        l.rater(Erreur::HorsPlage, "utc_offset_seconds");
    } else {
        r.decalage_utc_s = static_cast<std::int32_t>(decalage);
    }

    const json::Valeur tz = racine.champ("timezone");
    if (tz.existe() && !tz.est_nul()) {
        bool tronque = false;
        if (tz.genre() != json::Genre::Chaine) {
            l.rater(Erreur::MauvaisType, "timezone");
        } else {
            tz.copier_texte(r.fuseau, FUSEAU_MAX, &tronque);
            if (tronque) l.rater(Erreur::HorsPlage, "timezone");
        }
    }

    const json::Valeur cur = racine.champ("current");
    if (!cur.existe() || cur.genre() != json::Genre::Objet) {
        l.rater(cur.existe() ? Erreur::MauvaisType : Erreur::ChampAbsent, "current");
    } else {
        r.instant_s = l.entier(cur.champ("time"), "current.time");
        r.temperature_dixiemes = l.fixe16(cur.champ("temperature_2m"), "current.temperature_2m", 1);
        r.ressentie_dixiemes = l.fixe16(cur.champ("apparent_temperature"), "current.apparent_temperature", 1);
        const std::int64_t jour = l.entier(cur.champ("is_day"), "current.is_day");
        if (jour != 0 && jour != 1) l.rater(Erreur::HorsPlage, "current.is_day");
        r.jour = (jour == 1);
        r.code_wmo = l.entier16(cur.champ("weather_code"), "current.weather_code");
        r.vent_dixiemes_kmh = l.fixe16(cur.champ("wind_speed_10m"), "current.wind_speed_10m", 1);
        r.direction_vent_deg = l.fixe16(cur.champ("wind_direction_10m"), "current.wind_direction_10m", 0);
        r.rafales_dixiemes_kmh = l.fixe16(cur.champ("wind_gusts_10m"), "current.wind_gusts_10m", 1);
        r.pression_dixiemes_hpa = l.fixe16(cur.champ("pressure_msl"), "current.pressure_msl", 1);
        const std::int16_t hum = l.fixe16(cur.champ("relative_humidity_2m"), "current.relative_humidity_2m", 0);
        r.humidite_pct = static_cast<std::int8_t>(hum < 0 || hum > 100 ? -1 : hum);  // -1 : refusé par verifier
    }

    r.proba_precip_pct = detail::proba_a(racine, r.instant_s, l);

    // Prévisions par jour (facultatives, mais complètes si elles sont là).
    const json::Valeur jours = racine.champ("daily");
    if (jours.existe() && !jours.est_nul()) {
        if (jours.genre() != json::Genre::Objet) {
            l.rater(Erreur::MauvaisType, "daily");
        } else {
            struct Colonne {
                std::string_view nom;
                json::Valeur tableau;
            };
            Colonne cols[7] = {{"daily.time", jours.champ("time")},
                               {"daily.weather_code", jours.champ("weather_code")},
                               {"daily.temperature_2m_max", jours.champ("temperature_2m_max")},
                               {"daily.temperature_2m_min", jours.champ("temperature_2m_min")},
                               {"daily.sunrise", jours.champ("sunrise")},
                               {"daily.sunset", jours.champ("sunset")},
                               {"daily.uv_index_max", jours.champ("uv_index_max")}};
            const json::Valeur col_proba = jours.champ("precipitation_probability_max");
            std::size_t n = 0;
            bool structure_ok = true;
            for (std::size_t c = 0; c < 7 && structure_ok; ++c) {
                if (!cols[c].tableau.existe() || cols[c].tableau.est_nul()) {
                    l.rater(Erreur::ChampAbsent, cols[c].nom);
                    structure_ok = false;
                } else if (cols[c].tableau.genre() != json::Genre::Tableau) {
                    l.rater(Erreur::MauvaisType, cols[c].nom);
                    structure_ok = false;
                } else if (c == 0) {
                    n = cols[0].tableau.taille();
                } else if (cols[c].tableau.taille() != n) {
                    l.rater(Erreur::TableauxIncoherents, cols[c].nom);
                    structure_ok = false;
                }
            }
            if (structure_ok && (!col_proba.existe() || col_proba.est_nul())) {
                l.rater(Erreur::ChampAbsent, "daily.precipitation_probability_max");
                structure_ok = false;
            } else if (structure_ok && (col_proba.genre() != json::Genre::Tableau || col_proba.taille() != n)) {
                l.rater(col_proba.genre() != json::Genre::Tableau ? Erreur::MauvaisType : Erreur::TableauxIncoherents,
                        "daily.precipitation_probability_max");
                structure_ok = false;
            }
            if (structure_ok && n == 0) {
                l.rater(Erreur::HorsPlage, "daily.time");
                structure_ok = false;
            }
            if (structure_ok) {
                const std::size_t gardes = n < static_cast<std::size_t>(JOURS_MAX) ? n : static_cast<std::size_t>(JOURS_MAX);
                r.nb_jours = static_cast<std::uint8_t>(gardes);
                for (std::size_t i = 0; i < gardes; ++i) {
                    Jour& j = r.jours[i];
                    j.date_s = l.entier(cols[0].tableau.element(i), "daily.time");
                    j.code_wmo = l.entier16(cols[1].tableau.element(i), "daily.weather_code");
                    j.max_dixiemes = l.fixe16(cols[2].tableau.element(i), "daily.temperature_2m_max", 1);
                    j.min_dixiemes = l.fixe16(cols[3].tableau.element(i), "daily.temperature_2m_min", 1);
                    j.lever_s = l.entier(cols[4].tableau.element(i), "daily.sunrise");
                    j.coucher_s = l.entier(cols[5].tableau.element(i), "daily.sunset");
                    j.uv_max_dixiemes = l.fixe16(cols[6].tableau.element(i), "daily.uv_index_max", 1);
                    const std::int16_t p = l.fixe16(col_proba.element(i), "daily.precipitation_probability_max", 0);
                    j.proba_precip_max = static_cast<std::int8_t>(p < 0 || p > 100 ? -1 : p);  // -1 : refusé
                }
            }
        }
    }

    if (!l.diag.ok()) {
        a.diag = l.diag;
        return a;
    }
    a.diag = detail::verifier_champs(r);
    return a;
}

// Vérifie un relevé déjà construit (plages et cohérence). Même contrôle que celui de `analyser`.
constexpr Diagnostic verifier(const Releve& r) noexcept { return detail::verifier_champs(r); }

// ---- fraîcheur -----------------------------------------------------------------------------------------------------

enum class Fraicheur : std::uint8_t {
    Frais,       // reçu depuis peu : à afficher normalement
    Perime,      // plus de `delai_perime_s` : à afficher avec la marque « périmé »
    TresPerime,  // plus de `delai_abandon_s` : à ne plus présenter comme la météo actuelle
    Incertain,   // horloge non réglée ou revenue en arrière : on ne sait pas juger
};

// Valeurs proposées (à valider) : rafraîchissement toutes les 15 à 30 min -> « périmé » après 2 échecs ≈ 1 h.
inline constexpr std::int64_t DELAI_PERIME_S = 3600;
inline constexpr std::int64_t DELAI_ABANDON_S = 6 * 3600;
inline constexpr std::int64_t TOLERANCE_HORLOGE_S = 300;

constexpr Fraicheur fraicheur(const Releve& r, std::int64_t maintenant_s, std::int64_t delai_perime_s = DELAI_PERIME_S,
                              std::int64_t delai_abandon_s = DELAI_ABANDON_S) noexcept {
    if (maintenant_s < detail::INSTANT_MIN || r.recu_a_s < detail::INSTANT_MIN) return Fraicheur::Incertain;
    if (maintenant_s + TOLERANCE_HORLOGE_S < r.recu_a_s) return Fraicheur::Incertain;
    const std::int64_t age = maintenant_s > r.recu_a_s ? maintenant_s - r.recu_a_s : 0;
    if (age <= delai_perime_s) return Fraicheur::Frais;
    if (age <= delai_abandon_s) return Fraicheur::Perime;
    return Fraicheur::TresPerime;
}

// Rang (dans `jours`) du jour qui contient `maintenant_s`, ou -1 (relevé trop ancien ou sans prévisions). Le dernier
// jour couvre 24 h après son minuit local (±1 h les jours de changement d'heure : non vérifié).
constexpr int index_du_jour(const Releve& r, std::int64_t maintenant_s) noexcept {
    for (int i = 0; i < static_cast<int>(r.nb_jours); ++i) {
        const std::int64_t debut = r.jours[i].date_s;
        const std::int64_t fin = (i + 1 < static_cast<int>(r.nb_jours)) ? r.jours[i + 1].date_s : debut + 86400;
        if (maintenant_s >= debut && maintenant_s < fin) return i;
    }
    return -1;
}

// ---- cache binaire (NVS) -------------------------------------------------------------------------------------------
//
// Format v1, petit-boutiste : 'M' 'W' | version(1) | charge utile | CRC-32 (4 octets, sur tout ce qui précède).
// Charge utile : recu_a(u32) instant(u32) decalage(i32) longueur_fuseau(u8) fuseau, puis température, ressentie (i16),
// code(u8), jour(u8), vent, rafales, direction, pression (i16), humidité(u8), proba(i8), nb_jours(u8) et, par jour :
// date(u32) code(u8) min(i16) max(i16) lever(u32) coucher(u32) uv(i16) proba(u8).
inline constexpr std::size_t TAILLE_CACHE_MAX = 3 + 4 + 4 + 4 + 1 + (FUSEAU_MAX - 1) + 22 + 1 + 20 * JOURS_MAX + 4;

namespace detail {

constexpr std::uint32_t crc32(const std::uint8_t* donnees, std::size_t n) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= donnees[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1U) ? (crc >> 1) ^ 0xEDB88320U : crc >> 1;
    }
    return ~crc;
}

struct Ecrivain {
    std::uint8_t* p;
    std::size_t capacite;
    std::size_t n = 0;
    bool trop = false;

    constexpr void u8(std::uint32_t v) noexcept {
        if (n >= capacite) {
            trop = true;
            return;
        }
        p[n++] = static_cast<std::uint8_t>(v & 0xFFU);
    }
    constexpr void u16(std::uint32_t v) noexcept {
        u8(v);
        u8(v >> 8);
    }
    constexpr void u32(std::uint32_t v) noexcept {
        u16(v);
        u16(v >> 16);
    }
};

struct Lecteur32 {
    const std::uint8_t* p;
    std::size_t taille;
    std::size_t n = 0;
    bool trop = false;

    constexpr std::uint32_t u8() noexcept {
        if (n >= taille) {
            trop = true;
            return 0;
        }
        return p[n++];
    }
    constexpr std::uint32_t u16() noexcept {
        const std::uint32_t a = u8();
        return a | (u8() << 8);
    }
    constexpr std::uint32_t u32() noexcept {
        const std::uint32_t a = u16();
        return a | (u16() << 16);
    }
    constexpr std::int16_t i16() noexcept { return static_cast<std::int16_t>(static_cast<std::uint16_t>(u16())); }
    constexpr std::int8_t i8() noexcept { return static_cast<std::int8_t>(static_cast<std::uint8_t>(u8())); }
    constexpr std::int32_t i32() noexcept { return static_cast<std::int32_t>(u32()); }
};

}  // namespace detail

// Écrit le relevé dans `sortie`. Rend la taille écrite, ou 0 si le relevé est invraisemblable ou si la place manque.
constexpr std::size_t serialiser(const Releve& r, std::uint8_t* sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || !verifier(r).ok()) return 0;
    detail::Ecrivain w{sortie, capacite};
    w.u8('M');
    w.u8('W');
    w.u8(1);
    w.u32(static_cast<std::uint32_t>(r.recu_a_s));
    w.u32(static_cast<std::uint32_t>(r.instant_s));
    w.u32(static_cast<std::uint32_t>(r.decalage_utc_s));
    std::size_t longueur = 0;
    while (longueur < FUSEAU_MAX && r.fuseau[longueur] != '\0') ++longueur;
    w.u8(static_cast<std::uint32_t>(longueur));
    for (std::size_t i = 0; i < longueur; ++i) w.u8(static_cast<std::uint8_t>(r.fuseau[i]));
    w.u16(static_cast<std::uint16_t>(r.temperature_dixiemes));
    w.u16(static_cast<std::uint16_t>(r.ressentie_dixiemes));
    w.u8(static_cast<std::uint32_t>(r.code_wmo));
    w.u8(r.jour ? 1U : 0U);
    w.u16(static_cast<std::uint16_t>(r.vent_dixiemes_kmh));
    w.u16(static_cast<std::uint16_t>(r.rafales_dixiemes_kmh));
    w.u16(static_cast<std::uint16_t>(r.direction_vent_deg));
    w.u16(static_cast<std::uint16_t>(r.pression_dixiemes_hpa));
    w.u8(static_cast<std::uint8_t>(r.humidite_pct));
    w.u8(static_cast<std::uint8_t>(r.proba_precip_pct));
    w.u8(r.nb_jours);
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        const Jour& j = r.jours[i];
        w.u32(static_cast<std::uint32_t>(j.date_s));
        w.u8(static_cast<std::uint32_t>(j.code_wmo));
        w.u16(static_cast<std::uint16_t>(j.min_dixiemes));
        w.u16(static_cast<std::uint16_t>(j.max_dixiemes));
        w.u32(static_cast<std::uint32_t>(j.lever_s));
        w.u32(static_cast<std::uint32_t>(j.coucher_s));
        w.u16(static_cast<std::uint16_t>(j.uv_max_dixiemes));
        w.u8(static_cast<std::uint8_t>(j.proba_precip_max));
    }
    const std::uint32_t somme = detail::crc32(sortie, w.n > capacite ? capacite : w.n);
    w.u32(somme);
    return w.trop ? 0 : w.n;
}

struct Lecture {
    Releve releve{};
    Diagnostic diag{};
    constexpr bool ok() const noexcept { return diag.ok(); }
};

// Relit un cache écrit par `serialiser`. Refuse (sans rien inventer) une taille, une marque, une version, une somme de
// contrôle ou des valeurs invraisemblables.
constexpr Lecture deserialiser(const std::uint8_t* donnees, std::size_t taille) noexcept {
    Lecture out;
    if (donnees == nullptr || taille < 3 + 4) {
        out.diag = detail::echec(Erreur::Corrompu, "cache");
        return out;
    }
    if (donnees[0] != 'M' || donnees[1] != 'W') {
        out.diag = detail::echec(Erreur::Corrompu, "cache.marque");
        return out;
    }
    if (donnees[2] != 1) {
        out.diag = detail::echec(Erreur::VersionInconnue, "cache.version");
        return out;
    }
    detail::Lecteur32 rd{donnees, taille - 4};
    const std::uint32_t attendu = static_cast<std::uint32_t>(donnees[taille - 4]) |
                                  (static_cast<std::uint32_t>(donnees[taille - 3]) << 8) |
                                  (static_cast<std::uint32_t>(donnees[taille - 2]) << 16) |
                                  (static_cast<std::uint32_t>(donnees[taille - 1]) << 24);
    if (detail::crc32(donnees, taille - 4) != attendu) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.crc");
        return out;
    }
    rd.n = 3;
    Releve& r = out.releve;
    r.recu_a_s = rd.u32();
    r.instant_s = rd.u32();
    r.decalage_utc_s = rd.i32();
    const std::uint32_t longueur = rd.u8();
    if (longueur >= FUSEAU_MAX) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.fuseau");
        return out;
    }
    for (std::uint32_t i = 0; i < longueur; ++i) r.fuseau[i] = static_cast<char>(rd.u8());
    r.temperature_dixiemes = rd.i16();
    r.ressentie_dixiemes = rd.i16();
    r.code_wmo = static_cast<std::int16_t>(rd.u8());
    r.jour = rd.u8() != 0;
    r.vent_dixiemes_kmh = rd.i16();
    r.rafales_dixiemes_kmh = rd.i16();
    r.direction_vent_deg = rd.i16();
    r.pression_dixiemes_hpa = rd.i16();
    r.humidite_pct = rd.i8();
    r.proba_precip_pct = rd.i8();
    const std::uint32_t nb = rd.u8();
    if (nb > static_cast<std::uint32_t>(JOURS_MAX)) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.jours");
        return out;
    }
    r.nb_jours = static_cast<std::uint8_t>(nb);
    for (std::uint32_t i = 0; i < nb; ++i) {
        Jour& j = r.jours[i];
        j.date_s = rd.u32();
        j.code_wmo = static_cast<std::int16_t>(rd.u8());
        j.min_dixiemes = rd.i16();
        j.max_dixiemes = rd.i16();
        j.lever_s = rd.u32();
        j.coucher_s = rd.u32();
        j.uv_max_dixiemes = rd.i16();
        j.proba_precip_max = rd.i8();
    }
    if (rd.trop || rd.n != taille - 4) {  // trop court, ou octets en trop avant la somme
        out.diag = detail::echec(Erreur::Corrompu, "cache.taille");
        return out;
    }
    out.diag = verifier(r);
    return out;
}

}  // namespace ecran::meteo
