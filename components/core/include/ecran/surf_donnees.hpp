// Noyau pur « surf » : lecture des deux réponses JSON d'Open-Meteo (mer et vent), tolérante pour ce qui peut manquer, stricte pour
// ce qui serait faux. C++17 standard, aucun include ESP-IDF, aucun flottant : hauteurs en CENTIMÈTRES, périodes en DIXIÈMES de
// seconde, vitesses en DIXIÈMES de km/h, températures en DIXIÈMES de degré, directions en degrés entiers. Instants en secondes UNIX
// (UTC) : la requête demande `timeformat=unixtime` (surf_requete.hpp). Aucune heure locale n'est formatée ici.
//
// Règles (les mêmes que meteo_openmeteo.hpp : jamais de valeur inventée) :
//   - une valeur `null` ou une variable absente de la réponse = « pas de donnée » (`ABSENT`, surf_base.hpp), JAMAIS 0. C'est le cas
//     normal d'une position sans mer (Paris : les onze variables sont `null`, constaté le 07/11/2026) et d'un lac (Michigan : les
//     vagues sont là, la température de l'eau et la marée sont `null`). La réponse reste valide ; `a_vagues`, `a_maree`… le disent ;
//   - une valeur présente mais invraisemblable (hauteur négative, direction 400°…), d'un autre type (texte), un tableau dont la
//     longueur n'est pas celle de `time`, des heures qui ne se suivent pas de 3 600 s, ou une UNITÉ qui n'est pas celle demandée
//     (« ft » au lieu de « m ») : la réponse entière est REFUSÉE avec le nom du champ, pas corrigée en silence ;
//   - une réponse d'erreur de l'API (`{"error":true,"reason":"…"}`, HTTP 400) est reconnue comme telle ;
//   - plus de HEURES_MAX (60) heures : refusée, pas tronquée.
//
// Ce que ces données SONT : des prévisions de modèles de 5 à 25 km de maille. La case lue peut être à plusieurs kilomètres du spot
// et ne voit ni le banc de sable ni le récif : une hauteur est celle du LARGE, une estimation. Voir l'ADR-0013.
//
// `decalage_utc_s` : `utc_offset_seconds` de la réponse, fixé au moment de la requête (la page d'Open-Meteo le dit : il ne suit pas
// un changement d'heure survenant pendant les 48 h).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/surf_base.hpp"
#include "ecran/surf_requete.hpp"

namespace ecran::surf {

enum class Erreur : std::uint8_t {
    Aucune,
    JsonInvalide,        // le texte n'est pas du JSON
    ReponseRefusee,      // l'API a répondu {"error":true,…} (coordonnée invalide, variable inconnue…)
    ChampAbsent,         // `hourly`, `hourly.time` ou `utc_offset_seconds` manquent
    MauvaisType,         // texte au lieu d'un nombre, nombre là où un tableau est attendu…
    HorsPlage,           // valeur invraisemblable
    TableauxIncoherents, // longueur différente de `time`, heures qui ne se suivent pas, trop d'heures
    UniteInattendue,     // `hourly_units` annonce une autre unité que celle demandée (ft, mph…)
};

// Phrase courte pour l'écran (jamais un code ni un état brut) ; vide s'il n'y a pas d'erreur.
constexpr std::string_view message_court(Erreur e) noexcept {
    switch (e) {
    case Erreur::Aucune:
        return "";
    case Erreur::JsonInvalide:
        return "Réponse illisible";
    case Erreur::ReponseRefusee:
        return "Service des vagues indisponible";
    case Erreur::ChampAbsent:
        return "Donnée de mer manquante";
    case Erreur::MauvaisType:
        return "Donnée de mer inattendue";
    case Erreur::HorsPlage:
        return "Donnée de mer invraisemblable";
    case Erreur::TableauxIncoherents:
        return "Prévisions incohérentes";
    case Erreur::UniteInattendue:
        return "Unité de mesure inattendue";
    }
    return "Données de mer indisponibles";
}

struct Diagnostic {
    Erreur erreur = Erreur::Aucune;
    std::string_view champ{}; // pour le journal : « hourly.wave_height » (texte statique)
    constexpr bool ok() const noexcept { return erreur == Erreur::Aucune; }
};

// Une heure de la réponse MER. `debut_s` est l'étiquette de l'heure ; toutes les autres valeurs sont celles de l'étiquette (pour
// les vagues et la marée, Open-Meteo donne des valeurs instantanées à l'heure juste, pas des cumuls).
struct MarineHeure {
    std::int64_t debut_s = 0;
    std::int16_t vagues_cm = ABSENT; // wave_height : hauteur significative des vagues totales (houle + vagues de vent)
    std::int16_t vagues_periode_ds = ABSENT;
    std::int16_t vagues_direction_deg = ABSENT; // d'où viennent les vagues
    std::int16_t houle_cm = ABSENT;             // swell_wave_height
    std::int16_t houle_periode_ds = ABSENT;
    std::int16_t houle_direction_deg = ABSENT;
    std::int16_t mer_vent_cm = ABSENT; // wind_wave_height : le clapot fait localement par le vent
    std::int16_t mer_vent_periode_ds = ABSENT;
    std::int16_t mer_vent_direction_deg = ABSENT;
    std::int16_t temperature_mer_dixiemes = ABSENT; // sea_surface_temperature, en dixièmes de °C
    std::int16_t maree_cm =
        ABSENT; // sea_level_height_msl : niveau de la mer par rapport au niveau moyen GLOBAL, marée comprise
};

struct Marine {
    std::int32_t decalage_utc_s = 0;
    std::uint8_t nb = 0;
    MarineHeure heures[HEURES_MAX] = {};
    bool a_vagues = false;      // au moins une hauteur de vagues dans la réponse (faux : pas de mer ici)
    bool a_houle = false;       // au moins une hauteur de houle
    bool a_maree = false;       // au moins un niveau de mer
    bool a_temperature = false; // au moins une température de mer
};

struct VentHeure {
    std::int64_t debut_s = 0;
    std::int16_t vitesse_dixiemes_kmh = ABSENT; // wind_speed_10m
    std::int16_t rafale_dixiemes_kmh = ABSENT;  // wind_gusts_10m
    std::int16_t direction_deg = ABSENT;        // wind_direction_10m : D'OÙ VIENT le vent
};

struct Vent {
    std::int32_t decalage_utc_s = 0;
    std::uint8_t nb = 0;
    VentHeure heures[HEURES_MAX] = {};
};

struct AnalyseMarine {
    Marine marine{};
    Diagnostic diag{};
    constexpr bool ok() const noexcept { return diag.ok(); }
};

struct AnalyseVent {
    Vent vent{};
    Diagnostic diag{};
    constexpr bool ok() const noexcept { return diag.ok(); }
};

namespace detail {

constexpr std::int64_t INSTANT_MIN = 946684800;  // 2000-01-01
constexpr std::int64_t INSTANT_MAX = 4102444800; // 2100-01-01
constexpr std::int64_t PAS_S = 3600;

constexpr Diagnostic echec(Erreur e, std::string_view champ) noexcept { return Diagnostic{e, champ}; }

// Contrôles communs : JSON valide, pas une réponse d'erreur, `utc_offset_seconds`, `hourly.time`. Rend le diagnostic ; remplit
// `racine`, `hourly`, `decalage`, `nb` et `temps`.
struct Entete {
    json::Valeur hourly{};
    json::Valeur units{};
    std::int32_t decalage = 0;
    std::size_t nb = 0;
    json::Valeur temps{};
};

constexpr Diagnostic lire_entete(std::string_view texte, Entete &e) noexcept {
    const json::Resultat r = json::lire(texte);
    if (!r.ok() || r.valeur.genre() != json::Genre::Objet)
        return echec(Erreur::JsonInvalide, "");
    bool refusee = false;
    if (r.valeur.champ("error").en_booleen(refusee) && refusee)
        return echec(Erreur::ReponseRefusee, "error");
    const json::Valeur decalage = r.valeur.champ("utc_offset_seconds");
    if (!decalage.existe() || decalage.est_nul())
        return echec(Erreur::ChampAbsent, "utc_offset_seconds");
    std::int64_t d = 0;
    if (!decalage.en_entier(d))
        return echec(Erreur::MauvaisType, "utc_offset_seconds");
    if (d < -43200 || d > 50400)
        return echec(Erreur::HorsPlage, "utc_offset_seconds");
    e.decalage = static_cast<std::int32_t>(d);
    e.hourly = r.valeur.champ("hourly");
    if (e.hourly.genre() != json::Genre::Objet)
        return echec(e.hourly.existe() ? Erreur::MauvaisType : Erreur::ChampAbsent, "hourly");
    e.units = r.valeur.champ("hourly_units");
    e.temps = e.hourly.champ("time");
    if (e.temps.genre() != json::Genre::Tableau)
        return echec(e.temps.existe() ? Erreur::MauvaisType : Erreur::ChampAbsent, "hourly.time");
    e.nb = e.temps.taille();
    if (e.nb == 0)
        return echec(Erreur::TableauxIncoherents, "hourly.time");
    if (e.nb > HEURES_MAX)
        return echec(Erreur::TableauxIncoherents, "hourly.time");
    return Diagnostic{};
}

// Lit `hourly.time` : entiers dans la plage, espacés d'exactement une heure.
template <typename Ecrire> constexpr Diagnostic lire_temps(const Entete &e, Ecrire &&ecrire) noexcept {
    std::int64_t precedent = 0;
    for (std::size_t i = 0; i < e.nb; ++i) {
        std::int64_t t = 0;
        if (!e.temps.element(i).en_entier(t))
            return echec(Erreur::MauvaisType, "hourly.time");
        if (t < INSTANT_MIN || t > INSTANT_MAX)
            return echec(Erreur::HorsPlage, "hourly.time");
        if (i > 0 && t - precedent != PAS_S)
            return echec(Erreur::TableauxIncoherents, "hourly.time");
        ecrire(i, t);
        precedent = t;
    }
    return Diagnostic{};
}

// Une variable horaire : `nom` (ex. « wave_height »), unité attendue, échelle (décimales), plage acceptée. Absente de la réponse :
// toutes les heures restent ABSENT. Présente : tableau de la longueur de `time`, chaque case `null` (ABSENT) ou un nombre dans la
// plage. `champ` est le nom complet pour le journal (« hourly.wave_height »). Rend vrai si au moins une valeur est présente.
template <typename Ecrire>
constexpr Diagnostic lire_serie(const Entete &e, std::string_view nom, std::string_view champ, std::string_view unite,
                                int decimales, std::int64_t bas, std::int64_t haut, bool &une_valeur,
                                Ecrire &&ecrire) noexcept {
    const json::Valeur serie = e.hourly.champ(nom);
    if (!serie.existe() || serie.est_nul())
        return Diagnostic{};
    const json::Valeur u = e.units.champ(nom);
    if (u.existe() && !u.texte_egal(unite))
        return echec(Erreur::UniteInattendue, champ);
    if (serie.genre() != json::Genre::Tableau)
        return echec(Erreur::MauvaisType, champ);
    if (serie.taille() != e.nb)
        return echec(Erreur::TableauxIncoherents, champ);
    for (std::size_t i = 0; i < e.nb; ++i) {
        const json::Valeur v = serie.element(i);
        if (v.est_nul())
            continue; // pas de donnée : la case reste ABSENT
        std::int64_t x = 0;
        if (!v.en_fixe(decimales, x))
            return echec(v.genre() == json::Genre::Nombre ? Erreur::HorsPlage : Erreur::MauvaisType, champ);
        if (x < bas || x > haut)
            return echec(Erreur::HorsPlage, champ);
        ecrire(i, static_cast<std::int16_t>(x));
        une_valeur = true;
    }
    return Diagnostic{};
}

// Une direction en degrés : 0..360 accepté (360 = 0), rendu en 0..359.
constexpr std::int16_t direction_vers_0_359(std::int16_t d) noexcept { return d == 360 ? std::int16_t{0} : d; }

} // namespace detail

// Plages acceptées, en unités entières (cm, dixièmes de s, dixièmes de °C, degrés). Au-delà : réponse refusée.
//   hauteur  0 à 30 m ; période 0 à 40 s ; direction 0 à 360° ; température de mer -5 à 45 °C ; niveau de mer -20 à +20 m
//   (le plus grand marnage du monde, la baie de Fundy, fait un peu plus de 16 m crête à crête).
inline constexpr std::int64_t HAUTEUR_VAGUES_MAX_CM = 3000;
inline constexpr std::int64_t PERIODE_MAX_DS = 400;
inline constexpr std::int64_t TEMPERATURE_MER_MIN_DIXIEMES = -50;
inline constexpr std::int64_t TEMPERATURE_MER_MAX_DIXIEMES = 450;
inline constexpr std::int64_t NIVEAU_MER_MAX_CM = 2000;
inline constexpr std::int64_t VITESSE_VENT_MAX_DIXIEMES_KMH = 5000; // 500 km/h

// Lit la réponse de l'API Marine. Les `null` sont des « pas de donnée » ; voir l'en-tête du fichier pour le reste.
constexpr AnalyseMarine analyser_marine(std::string_view texte) noexcept {
    AnalyseMarine a{};
    detail::Entete e{};
    a.diag = detail::lire_entete(texte, e);
    if (!a.diag.ok())
        return a;
    Marine &m = a.marine;
    m.decalage_utc_s = e.decalage;
    m.nb = static_cast<std::uint8_t>(e.nb);
    a.diag = detail::lire_temps(e, [&m](std::size_t i, std::int64_t t) { m.heures[i].debut_s = t; });
    if (!a.diag.ok())
        return a;
    using detail::lire_serie;
    const auto h = [&m](auto membre) {
        return [&m, membre](std::size_t i, std::int16_t v) { m.heures[i].*membre = v; };
    };
    const auto dir = [&m](auto membre) {
        return [&m, membre](std::size_t i, std::int16_t v) { m.heures[i].*membre = detail::direction_vers_0_359(v); };
    };
    struct Variable {
        std::string_view nom;
        std::string_view champ;
        std::string_view unite;
        int decimales;
        std::int64_t bas;
        std::int64_t haut;
        std::int16_t MarineHeure::*membre;
        bool direction;
    };
    // clang-format off
    constexpr Variable VARIABLES[] = {
        {"wave_height", "hourly.wave_height", "m", 2, 0, HAUTEUR_VAGUES_MAX_CM, &MarineHeure::vagues_cm, false},
        {"wave_period", "hourly.wave_period", "s", 1, 0, PERIODE_MAX_DS, &MarineHeure::vagues_periode_ds, false},
        {"wave_direction", "hourly.wave_direction", "\xC2\xB0", 0, 0, 360, &MarineHeure::vagues_direction_deg, true},
        {"swell_wave_height", "hourly.swell_wave_height", "m", 2, 0, HAUTEUR_VAGUES_MAX_CM, &MarineHeure::houle_cm, false},
        {"swell_wave_period", "hourly.swell_wave_period", "s", 1, 0, PERIODE_MAX_DS, &MarineHeure::houle_periode_ds, false},
        {"swell_wave_direction", "hourly.swell_wave_direction", "\xC2\xB0", 0, 0, 360, &MarineHeure::houle_direction_deg, true},
        {"wind_wave_height", "hourly.wind_wave_height", "m", 2, 0, HAUTEUR_VAGUES_MAX_CM, &MarineHeure::mer_vent_cm, false},
        {"wind_wave_period", "hourly.wind_wave_period", "s", 1, 0, PERIODE_MAX_DS, &MarineHeure::mer_vent_periode_ds, false},
        {"wind_wave_direction", "hourly.wind_wave_direction", "\xC2\xB0", 0, 0, 360, &MarineHeure::mer_vent_direction_deg, true},
        {"sea_surface_temperature", "hourly.sea_surface_temperature", "\xC2\xB0" "C", 1, TEMPERATURE_MER_MIN_DIXIEMES,
         TEMPERATURE_MER_MAX_DIXIEMES, &MarineHeure::temperature_mer_dixiemes, false},
        {"sea_level_height_msl", "hourly.sea_level_height_msl", "m", 2, -NIVEAU_MER_MAX_CM, NIVEAU_MER_MAX_CM,
         &MarineHeure::maree_cm, false},
    };
    // clang-format on
    for (const Variable &v : VARIABLES) {
        bool presente = false;
        a.diag = v.direction
                     ? lire_serie(e, v.nom, v.champ, v.unite, v.decimales, v.bas, v.haut, presente, dir(v.membre))
                     : lire_serie(e, v.nom, v.champ, v.unite, v.decimales, v.bas, v.haut, presente, h(v.membre));
        if (!a.diag.ok())
            return a;
        if (v.membre == &MarineHeure::vagues_cm)
            m.a_vagues = presente;
        else if (v.membre == &MarineHeure::houle_cm)
            m.a_houle = presente;
        else if (v.membre == &MarineHeure::maree_cm)
            m.a_maree = presente;
        else if (v.membre == &MarineHeure::temperature_mer_dixiemes)
            m.a_temperature = presente;
    }
    return a;
}

// Lit la réponse de l'API de prévision (vent à 10 m). Vitesse, rafale et direction : `null` = pas de donnée.
constexpr AnalyseVent analyser_vent(std::string_view texte) noexcept {
    AnalyseVent a{};
    detail::Entete e{};
    a.diag = detail::lire_entete(texte, e);
    if (!a.diag.ok())
        return a;
    Vent &v = a.vent;
    v.decalage_utc_s = e.decalage;
    v.nb = static_cast<std::uint8_t>(e.nb);
    a.diag = detail::lire_temps(e, [&v](std::size_t i, std::int64_t t) { v.heures[i].debut_s = t; });
    if (!a.diag.ok())
        return a;
    bool presente = false;
    a.diag =
        detail::lire_serie(e, "wind_speed_10m", "hourly.wind_speed_10m", "km/h", 1, 0, VITESSE_VENT_MAX_DIXIEMES_KMH,
                           presente, [&v](std::size_t i, std::int16_t x) { v.heures[i].vitesse_dixiemes_kmh = x; });
    if (!a.diag.ok())
        return a;
    a.diag =
        detail::lire_serie(e, "wind_gusts_10m", "hourly.wind_gusts_10m", "km/h", 1, 0, VITESSE_VENT_MAX_DIXIEMES_KMH,
                           presente, [&v](std::size_t i, std::int16_t x) { v.heures[i].rafale_dixiemes_kmh = x; });
    if (!a.diag.ok())
        return a;
    a.diag = detail::lire_serie(
        e, "wind_direction_10m", "hourly.wind_direction_10m", "\xC2\xB0", 0, 0, 360, presente,
        [&v](std::size_t i, std::int16_t x) { v.heures[i].direction_deg = detail::direction_vers_0_359(x); });
    return a;
}

} // namespace ecran::surf
