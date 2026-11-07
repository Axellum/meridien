// Noyau pur : les réponses du NWS (National Weather Service des États-Unis, api.weather.gov) -> la météo du noyau (`AnalyseRiche`,
// comme les lecteurs d'Open-Meteo et de MET Norway). C++17 standard, aucun include ESP-IDF, aucun flottant dans la lecture des
// données (entiers en dixièmes) ; seul le calcul du soleil (soleil.hpp) emploie des `double`. Décision et mesures :
// ADR-0014, docs/mesures/fournisseurs-meteo.md.
//
// DEUX RÉPONSES (meteo_requete_fournisseurs.hpp) :
//   1. `/points/{lat},{lon}` -> `properties.gridId` (bureau, 3 lettres), `gridX`, `gridY`, `timeZone` (nom IANA). HTTP 404 hors
//      de la couverture (Canada, Cuba, France : « Unable to provide data for requested point »), HTTP 200 avec `gridId` nul pour
//      les Samoa américaines (relevé le 07/11/2026) : dans les deux cas le NWS n'a pas de prévision et l'appelant retombe sur
//      Open-Meteo. `analyser_points`.
//   2. `/gridpoints/{bureau}/{x},{y}` -> `properties.<couche>.values[]`, une liste d'INTERVALLES `{validTime: "début/durée",
//      value}` par grandeur (température, vent…). Les intervalles d'une couche se suivent sans trou ni chevauchement et font de
//      1 heure à plusieurs jours (relevé le 07/11/2026 sur six mailles) ; la valeur vaut pour tout l'intervalle. `analyser_nws`.
//
// CE QUE LA SOURCE DONNE, ET CE QU'ELLE NE DONNE PAS (aucune valeur n'est inventée : « inconnu » = INCONNU_DIX / INCONNU_ENTIER) :
//   - température, température ressentie (`apparentTemperature`), humidité, couverture nuageuse (`skyCover`), vent, rafales,
//     direction, probabilité de précipitations : à chaque instant, par la valeur de l'intervalle qui le contient. Unités vérifiées
//     (`uom` : degC, percent, km_h-1, degree_(angle), mm) ; une autre unité fait refuser la réponse.
//   - PRESSION : la couche `pressure` existe mais est vide (relevé le 07/11/2026 sur six mailles) : inconnue.
//   - CUMUL de pluie (`quantitativePrecipitation`) et de neige (`snowfallAmount`) : sur des intervalles de 2 à 9 heures, jamais
//     par heure. Une heure n'a un cumul que si un intervalle d'EXACTEMENT cette heure existe ; un jour n'en a un que si des
//     intervalles entiers couvrent ses 24 heures sans déborder. Les intervalles du NWS (00, 06, 12, 18 h UTC) ne tombent presque
//     jamais sur le minuit local des États-Unis : le cumul d'un jour est donc le plus souvent INCONNU, et on ne répartit pas un
//     cumul sur des heures qu'il ne couvre pas à moitié (répartir = inventer).
//   - INDICE UV, durée d'ensoleillement, nombre d'heures de pluie, direction dominante du vent d'un jour, pluie par quarts
//     d'heure : non fournis.
//   - lever et coucher du soleil : non fournis, calculés ici (soleil.hpp).
//   - minimum et maximum du jour : sur les intervalles de température qui touchent le jour local ; la couche `maxTemperature` /
//     `minTemperature` du NWS porte sur des fenêtres de 9 à 15 heures qui ne sont pas des jours civils : non utilisée.
//   - la probabilité de précipitations d'une heure est celle de l'intervalle qui la contient, si cet intervalle dure 6 heures au
//     plus ; au-delà elle est inconnue (une probabilité sur 12 ou 16 heures n'est pas celle d'une heure).
//
// SYMBOLE (code WMO du projet) : le ciel d'après `skyCover` (moins de 12,5 % : 0 ; moins de 37,5 % : 1 ; moins de 87,5 % : 2 ;
// sinon 3 — les seuils des libellés du NWS « clear / mostly clear / partly cloudy-mostly cloudy / cloudy », À VÉRIFIER), puis
// le phénomène de la couche `weather` s'il en a un et s'il est autre chose qu'une simple possibilité (`slight_chance`,
// `isolated`, `few`, `brief` : ignorés ; la probabilité reste affichée à part). Le plus fort des deux l'emporte. Les phénomènes
// sans équivalent parmi nos 29 codes (brume sèche, fumée, poussière, sable, cendres, givre, cristaux de glace, trombes, embruns
// verglaçants, poudrerie) sont ignorés : le ciel garde sa place. Un nom absent de la liste du NWS fait refuser la réponse.
// Le code d'un JOUR est le plus fort des codes de ses heures (comme le `weather_code` quotidien d'Open-Meteo : « the most severe
// weather condition on a given day »).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/iso8601.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/meteo_natif_commun.hpp"
#include "ecran/meteo_requete_fournisseurs.hpp"
#include "ecran/meteo_riche.hpp"
#include "ecran/parcours_json.hpp"
#include "ecran/soleil.hpp"

namespace ecran::meteo::nws {

// ---- la maille (réponse /points) --------------------------------------------------------------------------------------

struct Maille {
    char bureau[4] = {}; // « SEW »
    std::int16_t x = 0;
    std::int16_t y = 0;
    char fuseau[FUSEAU_MAX] = {}; // nom IANA (« America/Los_Angeles »), connu de fuseaux.hpp
};

// Lit la réponse de `/points/{lat},{lon}`. Un `gridId` absent ou nul (les Samoa américaines, relevé le 07/11/2026) ou un fuseau
// inconnu est un refus : le NWS n'a pas de prévision pour cette position.
constexpr Diagnostic analyser_points(std::string_view texte, Maille &m) noexcept {
    m = Maille{};
    const json::Resultat lu = json::lire(texte);
    if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet)
        return Diagnostic{Erreur::JsonInvalide, "json"};
    const json::Valeur p = lu.valeur.champ("properties");
    if (p.genre() != json::Genre::Objet)
        return Diagnostic{p.existe() ? Erreur::MauvaisType : Erreur::ChampAbsent, "properties"};
    char bureau[8] = {};
    bool tronque = false;
    const json::Valeur gid = p.champ("gridId");
    if (gid.genre() != json::Genre::Chaine)
        return Diagnostic{gid.existe() && !gid.est_nul() ? Erreur::MauvaisType : Erreur::ChampAbsent, "gridId"};
    const std::size_t n = gid.copier_texte(bureau, sizeof bureau, &tronque);
    if (tronque || !fournisseurs::bureau_nws_valide(std::string_view(bureau, n)))
        return Diagnostic{Erreur::HorsPlage, "gridId"};
    std::int64_t x = 0, y = 0;
    const json::Valeur vx = p.champ("gridX");
    const json::Valeur vy = p.champ("gridY");
    if (!vx.existe() || !vy.existe())
        return Diagnostic{Erreur::ChampAbsent, "gridX"};
    if (!vx.en_entier(x) || !vy.en_entier(y))
        return Diagnostic{Erreur::MauvaisType, "gridX"};
    if (x < 0 || x > fournisseurs::MAILLE_NWS_MAX || y < 0 || y > fournisseurs::MAILLE_NWS_MAX)
        return Diagnostic{Erreur::HorsPlage, "gridX"};
    char iana[FUSEAU_MAX] = {};
    const json::Valeur tz = p.champ("timeZone");
    if (tz.genre() != json::Genre::Chaine)
        return Diagnostic{tz.existe() && !tz.est_nul() ? Erreur::MauvaisType : Erreur::ChampAbsent, "timeZone"};
    const std::size_t nz = tz.copier_texte(iana, sizeof iana, &tronque);
    if (tronque || nz == 0 || fuseaux::posix_pour(std::string_view(iana, nz)).empty())
        return Diagnostic{Erreur::HorsPlage, "timeZone"};
    for (std::size_t i = 0; i < n; ++i)
        m.bureau[i] = bureau[i];
    m.bureau[n] = '\0';
    m.x = static_cast<std::int16_t>(x);
    m.y = static_cast<std::int16_t>(y);
    for (std::size_t i = 0; i <= nz; ++i)
        m.fuseau[i] = iana[i];
    return Diagnostic{};
}

// ---- une couche lue intervalle par intervalle ---------------------------------------------------------------------------

// Le plus grand nombre d'intervalles lus par couche (la plus longue du 07/11/2026 en a 149) ; les suivants sont ignorés.
inline constexpr std::size_t INTERVALLES_MAX = 192;

struct Intervalle {
    std::int32_t deb = 0; // secondes depuis `Serie::base`
    std::int32_t fin = 0; // idem, exclue
    std::int16_t v =
        0; // valeur en entiers du noyau (voir `Mode`) ; l'inconnu est INCONNU_DIX ou INCONNU_ENTIER selon le mode
};

struct Couche {
    Intervalle e[INTERVALLES_MAX] = {};
    std::size_t n = 0;
};

// Espace de travail de `analyser_nws` (≈ 25 Ko) : à fournir par l'appelant (sur la carte, en PSRAM, pas sur la pile).
struct Serie {
    std::int64_t base = 0; // début de `validTimes` (UNIX)
    Couche temperature, ressentie, humidite, nuages, vent, rafales, direction, proba, pluie, neige, temps;
};

enum class Mode : std::uint8_t {
    Dixiemes,        // nombre à une décimale -> dixièmes ; inconnu = INCONNU_DIX
    Entier,          // nombre entier >= 0 -> entier ; inconnu = INCONNU_ENTIER
    Pourcent,        // entier 0..100 (au-delà : réponse refusée) ; inconnu = INCONNU_ENTIER
    DixiemesPositif, // idem Dixiemes, mais refuse un nombre négatif (cumul)
    NeigeMm,         // millimètres -> dixièmes de centimètre ; inconnu = INCONNU_DIX
    Temps,           // la couche `weather` -> code WMO du projet, ou -1 : aucun phénomène retenu
};

namespace detail {

constexpr bool texte_court(const json::Valeur &v, char *tampon, std::size_t capacite,
                           std::string_view &sortie) noexcept {
    sortie = std::string_view();
    if (v.genre() != json::Genre::Chaine)
        return false;
    bool tronque = false;
    const std::size_t n = v.copier_texte(tampon, capacite, &tronque);
    if (tronque)
        return false;
    sortie = std::string_view(tampon, n);
    return true;
}

enum class Force : std::uint8_t { Legere, Moderee, Forte };

// Code WMO du projet pour un phénomène du NWS, -1 s'il n'en a pas (ignoré), -2 si le nom n'est pas dans la liste du NWS.
constexpr int code_phenomene(std::string_view nom, Force force, bool petite_grele, bool grosse_grele) noexcept {
    const auto trois = [&](int legere, int moderee, int forte) {
        return force == Force::Legere ? legere : (force == Force::Moderee ? moderee : forte);
    };
    if (nom == "drizzle")
        return trois(51, 53, 55);
    if (nom == "freezing_drizzle")
        return force == Force::Legere ? 56 : 57;
    if (nom == "rain")
        return trois(61, 63, 65);
    if (nom == "rain_showers")
        return trois(80, 81, 82);
    if (nom == "freezing_rain")
        return force == Force::Legere ? 66 : 67;
    if (nom == "snow")
        return trois(71, 73, 75);
    if (nom == "snow_showers")
        return force == Force::Forte ? 86 : 85;
    if (nom == "sleet")
        return 77; // « ice pellets » : grains de glace, le plus proche de nos 29 codes
    if (nom == "hail")
        return grosse_grele ? 99 : 96;
    if (nom == "thunderstorms") {
        if (grosse_grele)
            return 99;
        if (petite_grele)
            return 96;
        return force == Force::Forte ? 97 : 95;
    }
    if (nom == "fog")
        return 45;
    if (nom == "freezing_fog" || nom == "ice_fog")
        return 48;
    for (const std::string_view ignore :
         {std::string_view("blowing_dust"), std::string_view("blowing_sand"), std::string_view("blowing_snow"),
          std::string_view("frost"), std::string_view("haze"), std::string_view("ice_crystals"),
          std::string_view("smoke"), std::string_view("volcanic_ash"), std::string_view("water_spouts"),
          std::string_view("freezing_spray")}) {
        if (nom == ignore)
            return -1;
    }
    return -2;
}

// Une simple possibilité n'est pas un phénomène prévu.
constexpr bool couverture_faible(std::string_view c) noexcept {
    return c == "slight_chance" || c == "isolated" || c == "few" || c == "brief";
}

// Le plus fort des phénomènes retenus d'un intervalle de `weather`, -1 s'il n'y en a aucun ; Invalide si un nom est inconnu.
struct Temps {
    int code = -1;
    bool ok = true;
};

constexpr Temps lire_temps(const json::Valeur &valeur) noexcept {
    Temps t;
    if (valeur.genre() != json::Genre::Tableau) {
        t.ok = false;
        return t;
    }
    json::Parcours parcours(valeur);
    json::Valeur el;
    while (parcours.suivant(el)) {
        if (el.genre() != json::Genre::Objet) {
            t.ok = false;
            return t;
        }
        char b1[40] = {}, b2[32] = {}, b3[32] = {};
        std::string_view nom, couverture, intensite;
        const json::Valeur vn = el.champ("weather");
        if (vn.est_nul() || !vn.existe())
            continue; // pas de phénomène dans cet élément
        if (!texte_court(vn, b1, sizeof b1, nom)) {
            t.ok = false;
            return t;
        }
        const json::Valeur vc = el.champ("coverage");
        if (!vc.est_nul() && vc.existe() && !texte_court(vc, b2, sizeof b2, couverture)) {
            t.ok = false;
            return t;
        }
        const json::Valeur vi = el.champ("intensity");
        if (!vi.est_nul() && vi.existe() && !texte_court(vi, b3, sizeof b3, intensite)) {
            t.ok = false;
            return t;
        }
        Force force = Force::Moderee; // sans intensité, le NWS écrit « Rain » : une pluie ordinaire
        if (intensite == "very_light" || intensite == "light") {
            force = Force::Legere;
        } else if (intensite == "heavy") {
            force = Force::Forte;
        } else if (!intensite.empty() && intensite != "moderate") {
            t.ok = false;
            return t;
        }
        bool petite = false, grosse = false;
        const json::Valeur attributs = el.champ("attributes");
        if (attributs.genre() == json::Genre::Tableau) {
            json::Parcours pa(attributs);
            json::Valeur a;
            while (pa.suivant(a)) {
                char b4[32] = {};
                std::string_view nom_a;
                if (texte_court(a, b4, sizeof b4, nom_a)) {
                    if (nom_a == "small_hail")
                        petite = true;
                    if (nom_a == "large_hail")
                        grosse = true;
                }
            }
        }
        const int code = code_phenomene(nom, force, petite, grosse);
        if (code == -2) {
            t.ok = false;
            return t;
        }
        if (code >= 0 && !couverture_faible(couverture) && code > t.code)
            t.code = code;
    }
    return t;
}

constexpr bool unite_attendue(const json::Valeur &couche, std::string_view attendue) noexcept {
    char tampon[40] = {};
    std::string_view u;
    return texte_court(couche.champ("uom"), tampon, sizeof tampon, u) && u == attendue;
}

// Lit la couche `nom` de `properties` dans `c`. Une couche absente ou sans intervalle reste vide (ses valeurs sont inconnues) ;
// une unité inattendue, un intervalle illisible, qui se chevauche ou à l'envers, ou une valeur invraisemblable, fait refuser la
// réponse (faux, avec le diagnostic).
constexpr bool lire_couche(const json::Valeur &proprietes, std::string_view nom, std::string_view unite, Mode mode,
                           Serie &s, Couche &c, Diagnostic &diag) noexcept {
    c.n = 0;
    const json::Valeur couche = proprietes.champ(nom);
    if (!couche.existe() || couche.est_nul())
        return true;
    if (couche.genre() != json::Genre::Objet) {
        natif::signaler(diag, Erreur::MauvaisType, nom);
        return false;
    }
    const json::Valeur valeurs = couche.champ("values");
    if (!valeurs.existe() || valeurs.est_nul())
        return true;
    if (valeurs.genre() != json::Genre::Tableau) {
        natif::signaler(diag, Erreur::MauvaisType, nom);
        return false;
    }
    if (valeurs.taille() == 0)
        return true;
    if (mode != Mode::Temps && !unite_attendue(couche, unite)) {
        natif::signaler(diag, Erreur::MauvaisType, nom); // l'unité n'est pas celle qu'on lit : on ne devine pas
        return false;
    }
    json::Parcours parcours(valeurs);
    json::Valeur el;
    std::int32_t fin_precedente = 0;
    while (c.n < INTERVALLES_MAX && parcours.suivant(el)) {
        if (el.genre() != json::Genre::Objet) {
            natif::signaler(diag, Erreur::MauvaisType, nom);
            return false;
        }
        char tampon[64] = {};
        std::string_view vt;
        std::int64_t debut = 0, duree = 0;
        if (!texte_court(el.champ("validTime"), tampon, sizeof tampon, vt) || !iso::intervalle(vt, debut, duree)) {
            natif::signaler(diag, Erreur::MauvaisType, nom);
            return false;
        }
        const std::int64_t rel = debut - s.base;
        constexpr std::int64_t HORIZON_MAX =
            20LL * 86400; // 20 jours : très au-delà des 8 jours du NWS, bien en dessous de int32
        if (rel < 0 || rel > HORIZON_MAX || duree > HORIZON_MAX || (c.n > 0 && rel < fin_precedente)) {
            natif::signaler(diag, Erreur::TableauxIncoherents, nom);
            return false;
        }
        Intervalle &it = c.e[c.n];
        it.deb = static_cast<std::int32_t>(rel);
        it.fin = static_cast<std::int32_t>(rel + duree);
        fin_precedente = it.fin;
        const json::Valeur v = el.champ("value");
        switch (mode) {
        case Mode::Dixiemes:
            it.v = natif::fixe_ou_inconnu(v, 1, nom, diag);
            break;
        case Mode::DixiemesPositif:
            it.v = natif::fixe_ou_inconnu(v, 1, nom, diag);
            if (it.v != INCONNU_DIX && it.v < 0) {
                natif::signaler(diag, Erreur::HorsPlage, nom);
                it.v = INCONNU_DIX;
            }
            break;
        case Mode::NeigeMm:
            it.v = natif::fixe_ou_inconnu(v, 0, nom, diag);
            if (it.v != INCONNU_DIX && it.v < 0) {
                natif::signaler(diag, Erreur::HorsPlage, nom);
                it.v = INCONNU_DIX;
            }
            break;
        case Mode::Entier:
            it.v = natif::entier_ou_inconnu(v, nom, diag);
            break;
        case Mode::Pourcent:
            it.v = natif::pourcentage_ou_inconnu(v, nom, diag);
            break;
        case Mode::Temps: {
            const Temps t = lire_temps(v);
            if (!t.ok)
                natif::signaler(diag, Erreur::MauvaisType, nom);
            it.v = static_cast<std::int16_t>(t.code);
            break;
        }
        }
        if (!diag.ok())
            return false;
        ++c.n;
    }
    return true;
}

// L'intervalle qui contient l'instant `rel` (secondes depuis la base), ou nullptr.
constexpr const Intervalle *contenant(const Couche &c, std::int64_t rel) noexcept {
    for (std::size_t i = 0; i < c.n; ++i) {
        if (rel >= c.e[i].deb && rel < c.e[i].fin)
            return &c.e[i];
        if (c.e[i].deb > rel)
            break;
    }
    return nullptr;
}

constexpr std::int16_t valeur_dix(const Couche &c, std::int64_t rel) noexcept {
    const Intervalle *i = contenant(c, rel);
    return i == nullptr ? INCONNU_DIX : i->v;
}

constexpr std::int16_t valeur_entier(const Couche &c, std::int64_t rel) noexcept {
    const Intervalle *i = contenant(c, rel);
    return i == nullptr ? INCONNU_ENTIER : i->v;
}

// Probabilité de précipitations de l'heure : celle de l'intervalle qui la contient, s'il dure 6 heures au plus.
constexpr std::int16_t proba_a(const Couche &c, std::int64_t rel) noexcept {
    const Intervalle *i = contenant(c, rel);
    if (i == nullptr || i->fin - i->deb > 6 * 3600)
        return INCONNU_ENTIER;
    return i->v;
}

// Ciel d'après `skyCover` en dixièmes de pour cent... en pourcents entiers : voir l'en-tête pour les seuils.
constexpr int code_ciel(int pourcent) noexcept {
    if (pourcent < 0)
        return -1;
    // 12,5 % -> 13 ; 37,5 % -> 38 ; 87,5 % -> 88 (valeurs entières : « moins de 12,5 % » = au plus 12 %)
    if (pourcent <= 12)
        return 0;
    if (pourcent <= 37)
        return 1;
    if (pourcent <= 87)
        return 2;
    return 3;
}

// Le code WMO de l'instant `rel`, ou -1 si le ciel ou la couche `weather` n'y est pas connu.
constexpr int code_a(const Serie &s, std::int64_t rel) noexcept {
    const Intervalle *ciel = contenant(s.nuages, rel);
    const Intervalle *temps = contenant(s.temps, rel);
    if (ciel == nullptr || ciel->v == INCONNU_ENTIER || temps == nullptr)
        return -1;
    const int c = code_ciel(ciel->v);
    return temps->v > c ? temps->v : c;
}

// Plus petit et plus grand d'une couche sur [rel0, rel1) : tous les intervalles qui touchent la fenêtre.
constexpr void extremes_sur(const Couche &c, std::int64_t rel0, std::int64_t rel1, natif::Extremes &x) noexcept {
    for (std::size_t i = 0; i < c.n; ++i) {
        if (c.e[i].deb < rel1 && c.e[i].fin > rel0)
            x.noter(c.e[i].v);
    }
}

constexpr int maximum_entier_sur(const Couche &c, std::int64_t rel0, std::int64_t rel1) noexcept {
    int m = INCONNU_ENTIER;
    for (std::size_t i = 0; i < c.n; ++i) {
        if (c.e[i].deb < rel1 && c.e[i].fin > rel0 && c.e[i].v > m)
            m = c.e[i].v;
    }
    return m;
}

// Le cumul exact de [rel0, rel1) : des intervalles ENTIERS qui se suivent de rel0 à rel1 sans déborder ni laisser de trou, tous
// connus. Faux sinon. Le total est en dixièmes (mm de pluie ou cm de neige selon la couche).
constexpr bool cumul_exact(const Couche &c, std::int64_t rel0, std::int64_t rel1, std::int64_t &total) noexcept {
    std::int64_t curseur = rel0;
    total = 0;
    while (curseur < rel1) {
        const Intervalle *trouve = nullptr;
        for (std::size_t i = 0; i < c.n && trouve == nullptr; ++i) {
            if (c.e[i].deb == curseur)
                trouve = &c.e[i];
        }
        if (trouve == nullptr || trouve->v == INCONNU_DIX || trouve->fin > rel1)
            return false;
        total += trouve->v;
        curseur = trouve->fin;
    }
    return true;
}

} // namespace detail

// Analyse la grille. `recu_a_s` : heure (UNIX) de réception. `latitude_e5`, `longitude_e5` : la position demandée (pour le soleil).
// `maille` : la réponse de `analyser_points` (le fuseau en vient). `travail` : voir `Serie`.
//
// Même contrat que `analyser_riche` et `analyser_metno` : `diag` dit si la météo de base est utilisable ; `diag_heures`,
// `diag_jours` et `diag_quarts` disent pourquoi une section est vide (jamais une valeur inventée à la place).
constexpr AnalyseRiche analyser_nws(std::string_view texte, const Maille &maille, std::int64_t recu_a_s,
                                    std::int32_t latitude_e5, std::int32_t longitude_e5, Serie &travail) noexcept {
    AnalyseRiche a;
    Releve &r = a.meteo.releve;
    Riche &q = a.meteo.riche;

    const json::Resultat lu = json::lire(texte);
    if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet) {
        a.diag = Diagnostic{Erreur::JsonInvalide, "json"};
        return a;
    }
    const std::string_view posix = fuseaux::posix_pour(maille.fuseau);
    const heure::AnalyseTz tz = heure::analyser_tz(posix);
    if (posix.empty() || !tz.ok() || !natif::copier_fuseau(r, maille.fuseau)) {
        a.diag = Diagnostic{Erreur::ChampAbsent, "fuseau"};
        return a;
    }
    const json::Valeur p = lu.valeur.champ("properties");
    if (p.genre() != json::Genre::Objet) {
        a.diag = Diagnostic{p.existe() ? Erreur::MauvaisType : Erreur::ChampAbsent, "properties"};
        return a;
    }
    // la grille doit être celle de la maille demandée (le NWS peut rediriger vers une autre)
    {
        char tampon[8] = {};
        std::string_view bureau;
        std::int64_t x = -1, y = -1;
        if (!detail::texte_court(p.champ("gridId"), tampon, sizeof tampon, bureau) || bureau != maille.bureau ||
            !p.champ("gridX").en_entier(x) || !p.champ("gridY").en_entier(y) || x != maille.x || y != maille.y) {
            a.diag = Diagnostic{Erreur::TableauxIncoherents, "gridId"};
            return a;
        }
    }
    // début de la période couverte : `validTimes` = « début/durée »
    {
        char tampon[64] = {};
        std::string_view vt;
        std::int64_t debut = 0, duree = 0;
        if (!detail::texte_court(p.champ("validTimes"), tampon, sizeof tampon, vt) ||
            !iso::intervalle(vt, debut, duree)) {
            a.diag = Diagnostic{Erreur::ChampAbsent, "validTimes"};
            return a;
        }
        travail.base = debut;
    }
    Serie &s = travail;
    Diagnostic lecture;
    const bool bon =
        detail::lire_couche(p, "temperature", "wmoUnit:degC", Mode::Dixiemes, s, s.temperature, lecture) &&
        detail::lire_couche(p, "apparentTemperature", "wmoUnit:degC", Mode::Dixiemes, s, s.ressentie, lecture) &&
        detail::lire_couche(p, "relativeHumidity", "wmoUnit:percent", Mode::Pourcent, s, s.humidite, lecture) &&
        detail::lire_couche(p, "skyCover", "wmoUnit:percent", Mode::Pourcent, s, s.nuages, lecture) &&
        detail::lire_couche(p, "windSpeed", "wmoUnit:km_h-1", Mode::Dixiemes, s, s.vent, lecture) &&
        detail::lire_couche(p, "windGust", "wmoUnit:km_h-1", Mode::Dixiemes, s, s.rafales, lecture) &&
        detail::lire_couche(p, "windDirection", "wmoUnit:degree_(angle)", Mode::Entier, s, s.direction, lecture) &&
        detail::lire_couche(p, "probabilityOfPrecipitation", "wmoUnit:percent", Mode::Pourcent, s, s.proba, lecture) &&
        detail::lire_couche(p, "quantitativePrecipitation", "wmoUnit:mm", Mode::DixiemesPositif, s, s.pluie, lecture) &&
        detail::lire_couche(p, "snowfallAmount", "wmoUnit:mm", Mode::NeigeMm, s, s.neige, lecture) &&
        detail::lire_couche(p, "weather", "", Mode::Temps, s, s.temps, lecture);
    if (!bon || !lecture.ok()) {
        a.diag = lecture.ok() ? Diagnostic{Erreur::MauvaisType, "couche"} : lecture;
        return a;
    }

    // ---- l'instant présent ----
    const std::int64_t maintenant = recu_a_s - s.base;
    const std::int16_t temperature = detail::valeur_dix(s.temperature, maintenant);
    const int code_courant = detail::code_a(s, maintenant);
    r.recu_a_s = recu_a_s;
    r.instant_s = recu_a_s;
    r.decalage_utc_s = heure::en_heure_locale(recu_a_s, tz.fuseau).decalage_s;
    if (temperature == INCONNU_DIX) {
        a.diag = Diagnostic{Erreur::HorsPlage, "temperature"}; // la grille ne couvre pas l'instant présent
        return a;
    }
    if (code_courant < 0) {
        a.diag = Diagnostic{Erreur::ChampAbsent, "skyCover"};
        return a;
    }
    r.temperature_dixiemes = temperature;
    r.ressentie_dixiemes = detail::valeur_dix(s.ressentie, maintenant);
    r.code_wmo = static_cast<std::int16_t>(code_courant);
    r.jour = soleil::est_leve_e5(recu_a_s, latitude_e5, longitude_e5);
    r.vent_dixiemes_kmh = detail::valeur_dix(s.vent, maintenant);
    r.rafales_dixiemes_kmh = detail::valeur_dix(s.rafales, maintenant);
    r.direction_vent_deg = detail::valeur_entier(s.direction, maintenant);
    r.pression_dixiemes_hpa = INCONNU_DIX; // la couche `pressure` du NWS est vide
    r.humidite_pct = static_cast<std::int8_t>(detail::valeur_entier(s.humidite, maintenant));
    r.proba_precip_pct = static_cast<std::int8_t>(detail::proba_a(s.proba, maintenant));
    r.nb_jours = 0;
    q.precip_courante_dixiemes = INCONNU_DIX; // « le dernier quart d'heure écoulé » : le NWS ne le donne pas
    q.nuages_pct = detail::valeur_entier(s.nuages, maintenant);
    a.diag = verifier(a.meteo.releve);
    if (!a.diag.ok())
        return a;
    a.diag = ecran::meteo::detail::verifier_courant_riche(q);
    if (!a.diag.ok())
        return a;

    // ---- 24 heures : de l'heure UTC en cours, chacune avec sa température et son code ----
    {
        const std::int64_t h0 = recu_a_s - ((recu_a_s % PAS_HEURE_S) + PAS_HEURE_S) % PAS_HEURE_S;
        bool bonne = true;
        for (int k = 0; bonne && k < HEURES_MAX; ++k) {
            const std::int64_t t = h0 + PAS_HEURE_S * k;
            const std::int64_t rel = t - s.base;
            const int code = detail::code_a(s, rel);
            const std::int16_t temp = detail::valeur_dix(s.temperature, rel);
            bonne = code >= 0 && temp != INCONNU_DIX;
            if (!bonne)
                break;
            HeureRiche &h = q.heures[k];
            h.debut_s = t;
            h.temperature_dixiemes = temp;
            h.ressentie_dixiemes = detail::valeur_dix(s.ressentie, rel);
            h.humidite_pct = detail::valeur_entier(s.humidite, rel);
            h.proba_pct = detail::proba_a(s.proba, rel);
            // un cumul n'existe que pour un intervalle d'exactement cette heure
            std::int64_t cumul = 0;
            h.precip_dixiemes_mm = detail::cumul_exact(s.pluie, rel, rel + PAS_HEURE_S, cumul) && cumul <= INT16_MAX
                                       ? static_cast<std::int16_t>(cumul)
                                       : INCONNU_DIX;
            h.code_wmo = static_cast<std::int16_t>(code);
            h.nuages_pct = detail::valeur_entier(s.nuages, rel);
            h.vent_dixiemes_kmh = detail::valeur_dix(s.vent, rel);
            h.rafales_dixiemes_kmh = detail::valeur_dix(s.rafales, rel);
            h.direction_deg = detail::valeur_entier(s.direction, rel);
            h.uv_dixiemes = INCONNU_DIX; // le NWS ne donne pas d'indice UV
            h.jour = soleil::est_leve_e5(t, latitude_e5, longitude_e5);
        }
        if (!bonne) {
            a.diag_heures = Diagnostic{Erreur::TableauxIncoherents, "hourly"};
        } else {
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
            const std::int64_t rel0 = d0 - s.base;
            const std::int64_t rel1 = d1 - s.base;
            natif::Extremes temp, ress, vent, raf;
            detail::extremes_sur(s.temperature, rel0, rel1, temp);
            detail::extremes_sur(s.ressentie, rel0, rel1, ress);
            detail::extremes_sur(s.vent, rel0, rel1, vent);
            detail::extremes_sur(s.rafales, rel0, rel1, raf);
            int code = -1;
            for (std::int64_t t = d0; t < d1; t += PAS_HEURE_S) {
                const int c = detail::code_a(s, t - s.base);
                if (c > code)
                    code = c;
            }
            if (temp.vide() || code < 0)
                break; // la grille ne va pas plus loin
            std::int64_t pluie = 0, neige = 0;
            const bool pluie_connue = detail::cumul_exact(s.pluie, rel0, rel1, pluie) && pluie <= INT16_MAX;
            const bool neige_connue = detail::cumul_exact(s.neige, rel0, rel1, neige) && neige <= INT16_MAX;

            Jour &j = r.jours[k];
            j.date_s = d0;
            j.code_wmo = static_cast<std::int16_t>(code);
            j.min_dixiemes = temp.min;
            j.max_dixiemes = temp.max;
            natif::remplir_soleil(j, d1, latitude_e5, longitude_e5);
            j.uv_max_dixiemes = INCONNU_DIX;
            j.proba_precip_max = static_cast<std::int8_t>(detail::maximum_entier_sur(s.proba, rel0, rel1));
            JourRiche &cj = q.jours[k];
            cj.ressentie_min_dixiemes = ress.min;
            cj.ressentie_max_dixiemes = ress.max;
            cj.ensoleillement_min = INCONNU_ENTIER;
            cj.precip_dixiemes_mm = pluie_connue ? static_cast<std::int16_t>(pluie) : INCONNU_DIX;
            cj.neige_dixiemes_cm = neige_connue ? static_cast<std::int16_t>(neige) : INCONNU_DIX;
            cj.heures_precip = INCONNU_ENTIER;
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

    // Le NWS n'a pas de pluie par quarts d'heure : la section reste vide, et le dit.
    a.diag_quarts = Diagnostic{Erreur::ChampAbsent, "minutely_15"};
    return a;
}

} // namespace ecran::meteo::nws
