// Noyau pur : les SOURCES de météo que l'écran sait demander, et la liste proposée pour le pays de la ville choisie. C++17
// standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation. Décision et mesures : ADR-0010, docs/mesures/meteo-modeles.md.
//
// Une entrée = un service au nom connu (« Météo-France », « DWD »…), la valeur du paramètre `models=` d'Open-Meteo qui le demande,
// les pays où il est la source nationale, et ses LIMITES : ce que la capture réelle du 07/11/2026 (la requête COMPLÈTE de
// l'écran, `test/host/capturer_modeles_meteo.py`) a montré d'absent dans sa réponse. « Automatique » (identifiant `auto`) ne
// demande aucun modèle : l'adresse reste, à l'octet près, celle d'avant ce réglage.
//
// QUI DÉCIDE QU'UNE SOURCE EST PROPOSÉE : le LECTEUR du firmware (`analyser_riche`, meteo_riche.hpp), pas ce fichier. Il refuse une
// réponse entière quand `daily.uv_index_max`, `current.wind_gusts_10m`, `daily.precipitation_probability_max` ou le code de ciel
// d'un des sept jours manque. La plupart des modèles nationaux n'ont pas d'UV chez Open-Meteo : ils sont dans la table, avec
// leur limite, mais ne sont PAS proposés tant que le lecteur ne les accepte pas (`LECTEUR_TOLERE`). Si le lecteur est un jour
// assoupli pour une limite, on ajoute son bit à `LECTEUR_TOLERE` : la source apparaît partout (écran, API, page) sans autre
// changement. Un test (test_fournisseurs_meteo.cpp) rejoue de VRAIES réponses dans le lecteur : il échoue si la table dit
// « proposée » pour une réponse que le lecteur refuse, ou l'inverse (si le lecteur change, la table doit suivre).
//
// L'ORDRE DE LA TABLE NE CHANGE JAMAIS : la position sert d'indice d'option dans le registre des réglages. On ajoute à la fin.
// Les identifiants (`id`) ne changent jamais non plus : ils sont gardés en mémoire permanente et servis par l'API.
//
// Trois « familles » : Open-Meteo (un seul analyseur, tous les modèles nationaux), et deux services lus DIRECTEMENT, sans clé, par
// leur propre analyseur : MET Norway (api.met.no) et le NWS américain (api.weather.gov), ADR-0014. Le client choisit l'adresse et
// l'analyseur selon `famille`. Une source directe n'a pas de `models=` (vide) ; si elle échoue, le client se replie sur
// « Automatique » d'Open-Meteo et le dit (l'écran et la page montrent la source qui a VRAIMENT donné la météo, `attribution_de`).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_requete.hpp"

namespace ecran::meteo::fournisseurs {

// Quel analyseur lit la réponse (et quelle adresse la demande).
enum class Famille : std::uint8_t {
    OpenMeteo = 0, // adresse de meteo_requete.hpp + `models=`, réponse lue par analyser_riche() (meteo_riche.hpp)
    MetNo = 1,     // api.met.no : une requête, réponse lue par analyser_metno() (meteo_metno.hpp)
    Nws = 2,       // api.weather.gov : deux requêtes (/points puis la grille), lues par analyser_nws() (meteo_nws.hpp)
};

// Limites mesurées (bits). Vu sur : PC, réponses réelles d'Open-Meteo du 07/11/2026, une ville par modèle ; voir
// docs/mesures/meteo-modeles.md pour la ville, la taille et la section de chaque variable absente.
namespace limite {
inline constexpr std::uint8_t SANS_UV = 1u
                                        << 0; // `uv_index` (horaire) et `uv_index_max` (jour) vides : lecteur : REFUSÉE
inline constexpr std::uint8_t SANS_RAFALES = 1u << 1; // `current.wind_gusts_10m` vide : lecteur : REFUSÉE
inline constexpr std::uint8_t SANS_PROBA = 1u << 2;   // `daily.precipitation_probability_max` vide : lecteur : REFUSÉE
inline constexpr std::uint8_t JOURS_COURTS = 1u
                                             << 3; // moins de sept jours de prévision (codes de ciel vides) : REFUSÉE
// `daily.sunshine_duration` vide : le lecteur ne refuse pas, il masque seulement le détail des jours (meteo_riche.hpp).
inline constexpr std::uint8_t SANS_ENSOLEILLEMENT = 1u << 4;
} // namespace limite

// Les limites que le lecteur du firmware accepte aujourd'hui sans refuser la réponse (vérifié par test_fournisseurs_meteo.cpp
// sur les réponses réelles de test/host/donnees/openmeteo_modele_*.json). Pour proposer les modèles « sans UV », y ajouter
// `limite::SANS_UV` ET assouplir le lecteur (meteo_openmeteo.hpp, meteo_riche.hpp) dans le même changement.
inline constexpr std::uint8_t LECTEUR_TOLERE = limite::SANS_ENSOLEILLEMENT;

struct Fournisseur {
    std::string_view id; // « meteofrance » : stable, jamais traduit, gardé en NVS et servi par l'API
    std::string_view
        libelle; // nom du service en clair : un nom propre, sans mot d'une langue, donc jamais traduit (« Automatique » l'est)
    bool libelle_traduit; // vrai : `libelle` est une phrase de l'écran (clé de tr())
    Famille famille;
    std::string_view modeles; // valeur de `models=` d'Open-Meteo ; vide : ne rien envoyer (Automatique)
    std::string_view pays;    // codes ISO 3166-1 alpha-2 en majuscules, séparés par une espace ; vide : tous les pays
    std::uint8_t limites;     // bits de `limite::`
};

// clang-format off
inline constexpr Fournisseur TABLE[] = {
    // id             libellé                   traduit  famille            models=                      pays                    limites
    {"auto",        "Automatique",              true,  Famille::OpenMeteo, "",                          "",                     0},
    // Modèles nationaux ou régionaux d'Open-Meteo. Pays : ceux où la capture a été faite et rendait des valeurs (NON vérifié ailleurs).
    {"meteofrance", "Météo-France",             false, Famille::OpenMeteo, "meteofrance_seamless",      "FR RE",                limite::SANS_UV | limite::SANS_PROBA | limite::JOURS_COURTS},
    {"dwd",         "DWD",                      false, Famille::OpenMeteo, "icon_seamless",             "DE",                   limite::SANS_UV},
    {"metoffice",   "Met Office",               false, Famille::OpenMeteo, "ukmo_seamless",             "GB",                   limite::SANS_UV | limite::SANS_ENSOLEILLEMENT},
    {"metno",       "MET Norway",               false, Famille::OpenMeteo, "metno_seamless",            "NO SE FI DK IS EE",    0},
    {"dmi",         "DMI",                      false, Famille::OpenMeteo, "dmi_seamless",              "DK",                   limite::SANS_UV},
    {"knmi",        "KNMI",                     false, Famille::OpenMeteo, "knmi_seamless",             "NL BE LU",             0},
    {"meteosuisse", "MeteoSwiss",               false, Famille::OpenMeteo, "meteoswiss_icon_seamless",  "CH",                   limite::SANS_UV | limite::JOURS_COURTS},
    {"jma",         "JMA",                      false, Famille::OpenMeteo, "jma_seamless",              "JP",                   limite::SANS_UV | limite::SANS_RAFALES | limite::SANS_PROBA | limite::SANS_ENSOLEILLEMENT},
    {"gem",         "ECCC (GEM)",               false, Famille::OpenMeteo, "gem_seamless",              "CA",                   limite::SANS_UV},
    {"geosphere",   "GeoSphere Austria",        false, Famille::OpenMeteo, "geosphere_seamless",        "AT",                   limite::SANS_UV},
    {"arpae",       "ItaliaMeteo (ARPAE)",      false, Famille::OpenMeteo, "italia_meteo_arpae_icon_2i","IT",                   limite::SANS_UV | limite::SANS_PROBA | limite::JOURS_COURTS},
    {"chmi",        "CHMI",                     false, Famille::OpenMeteo, "chmi_aladin_seamless",      "CZ",                   limite::SANS_UV},
    // Modèles mondiaux : proposés dans tous les pays.
    {"noaa",        "NOAA GFS",                 false, Famille::OpenMeteo, "gfs_seamless",              "",                     0},
    {"ecmwf",       "ECMWF IFS",                false, Famille::OpenMeteo, "ecmwf_ifs025",              "",                     limite::SANS_UV},
    // Services lus directement (ADR-0014), ajoutés APRÈS les précédents : l'ordre ne change jamais. Pas de `models=`. Leurs limites
    // (pas d'UV, pas de rafales hors de la zone nordique…) ne refusent pas la réponse : le lecteur laisse la valeur « inconnue »
    // (docs/mesures/fournisseurs-meteo.md), d'où `limites` à 0. Pays : seulement ceux dont une réponse RÉELLE du 07/11/2026 est en
    // fixture (test_meteo_metno.cpp, test_meteo_nws.cpp les comparent à cette liste) ; Suède, Danemark, Estonie, îles Vierges, Guam…
    // viendront avec leur capture.
    {"metnorway",   "MET Norway (api.met.no)",  false, Famille::MetNo,     "",                          "NO FI IS SJ",          0},
    {"nws",         "NWS (weather.gov)",        false, Famille::Nws,       "",                          "US PR",                0},
};
// clang-format on

inline constexpr std::size_t NOMBRE = sizeof(TABLE) / sizeof(TABLE[0]);
inline constexpr std::size_t INDICE_AUTO = 0; // l'entrée « Automatique » : le défaut, toujours première

// ---- recherche ----------------------------------------------------------------------------------------------------------

// L'entrée d'identifiant `id`, ou nullptr.
constexpr const Fournisseur *trouver(std::string_view id) noexcept {
    for (std::size_t i = 0; i < NOMBRE; ++i) {
        if (TABLE[i].id == id)
            return &TABLE[i];
    }
    return nullptr;
}

// Position de l'entrée d'identifiant `id`, ou NOMBRE si elle n'existe pas.
constexpr std::size_t indice_de(std::string_view id) noexcept {
    for (std::size_t i = 0; i < NOMBRE; ++i) {
        if (TABLE[i].id == id)
            return i;
    }
    return NOMBRE;
}

// La famille de la source d'identifiant `id` : celle d'« Automatique » pour un identifiant inconnu (on ne tente jamais un service
// qu'on ne connaît pas).
constexpr Famille famille_de(std::string_view id) noexcept {
    const Fournisseur *f = trouver(id);
    return f != nullptr ? f->famille : Famille::OpenMeteo;
}

// Qui doit être nommé à côté de la météo affichée (l'obligation de la licence, ou la politesse). `nom` et `lien` sont des noms
// propres : jamais traduits. La phrase qui les entoure (licence) est dans la table de traduction, une par famille.
struct Attribution {
    std::string_view id; // identifiant stable servi par l'API (« open-meteo », « met-norway », « nws »)
    std::string_view nom;
    std::string_view lien; // adresse du site du service
};

constexpr Attribution attribution_de(Famille f) noexcept {
    switch (f) {
    case Famille::MetNo:
        return {"met-norway", "MET Norway", "https://api.met.no/"};
    case Famille::Nws:
        return {"nws", "NWS", "https://www.weather.gov/"};
    case Famille::OpenMeteo:
        break;
    }
    return {"open-meteo", "Open-Meteo", "https://open-meteo.com/"};
}

// ---- ce qui est proposé -------------------------------------------------------------------------------------------------

// Vrai si le lecteur du firmware accepte la réponse de cette source (toutes ses limites sont tolérées). « Automatique » l'est
// toujours.
// `tolere` : les limites que le lecteur accepte (par défaut celles d'aujourd'hui ; un test le fait varier pour montrer ce qui
// apparaîtrait le jour où le lecteur serait assoupli).
constexpr bool proposable(const Fournisseur &f, std::uint8_t tolere = LECTEUR_TOLERE) noexcept {
    return (f.limites & static_cast<std::uint8_t>(~tolere)) == 0;
}

namespace detail {

constexpr char majuscule(char c) noexcept { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

// `code` (deux lettres, casse indifférente) figure dans la liste de codes `liste` séparés par une espace. Un code qui n'est pas
// fait de deux lettres (vide, trois lettres, chiffres) ne figure jamais nulle part.
constexpr bool code_dans(std::string_view liste, std::string_view code) noexcept {
    if (code.size() != 2)
        return false;
    const char a = majuscule(code[0]);
    const char b = majuscule(code[1]);
    if (a < 'A' || a > 'Z' || b < 'A' || b > 'Z')
        return false;
    for (std::size_t i = 0; i + 1 < liste.size(); ++i) {
        const bool debut = i == 0 || liste[i - 1] == ' ';
        const bool fin = i + 2 == liste.size() || liste[i + 2] == ' ';
        if (debut && fin && liste[i] == a && liste[i + 1] == b)
            return true;
    }
    return false;
}

} // namespace detail

// Cette source est-elle une source de ce pays ? Les sources mondiales (`pays` vide) le sont de tous les pays ; « Automatique »
// aussi. Aucune notion de « proposable » ici : voir `propose_pour`.
constexpr bool concerne_le_pays(const Fournisseur &f, std::string_view code_pays) noexcept {
    return f.pays.empty() || detail::code_dans(f.pays, code_pays);
}

// La liste proposée pour un pays : positions dans TABLE, « Automatique » en tête, puis les sources nationales de ce pays, puis
// les mondiales, dans l'ordre de la table. Seules les sources proposables y sont (voir plus haut).
struct Liste {
    std::size_t indices[NOMBRE] = {};
    std::size_t n = 0;
};

constexpr Liste pour_pays(std::string_view code_pays, std::uint8_t tolere = LECTEUR_TOLERE) noexcept {
    Liste l;
    l.indices[l.n++] = INDICE_AUTO;
    for (int passe = 0; passe < 2; ++passe) { // 0 : nationales, 1 : mondiales
        for (std::size_t i = 1; i < NOMBRE; ++i) {
            const Fournisseur &f = TABLE[i];
            if (f.pays.empty() != (passe == 1) || !proposable(f, tolere) || !concerne_le_pays(f, code_pays))
                continue;
            l.indices[l.n++] = i;
        }
    }
    return l;
}

// L'identifiant `id` est-il valable (connu, proposable) pour ce pays ? Sert à revenir à « Automatique » quand la ville change.
constexpr bool propose_pour(std::string_view id, std::string_view code_pays,
                            std::uint8_t tolere = LECTEUR_TOLERE) noexcept {
    const std::size_t i = indice_de(id);
    if (i >= NOMBRE)
        return false;
    const Liste l = pour_pays(code_pays, tolere);
    for (std::size_t k = 0; k < l.n; ++k) {
        if (l.indices[k] == i)
            return true;
    }
    return false;
}

// La source à garder quand la ville (donc le pays) change : la même si elle convient encore, sinon « Automatique ». Sert partout
// où la ville change (écran, page web) et à la relecture de la mémoire permanente : un identifiant inconnu ou qui ne convient
// pas au pays redonne « auto », jamais une erreur muette ni une requête avec un modèle qui ne couvre pas la ville.
constexpr std::string_view source_valable_pour(std::string_view id, std::string_view code_pays,
                                               std::uint8_t tolere = LECTEUR_TOLERE) noexcept {
    return propose_pour(id, code_pays, tolere) ? TABLE[indice_de(id)].id : TABLE[INDICE_AUTO].id;
}

// Valeur de `models=` pour cet identifiant : vide pour « Automatique » ET pour un identifiant inconnu (on ne demande jamais
// un modèle qu'on ne connaît pas : retour au comportement d'avant).
constexpr std::string_view modeles_de(std::string_view id) noexcept {
    const Fournisseur *f = trouver(id);
    return f != nullptr ? f->modeles : std::string_view{};
}

// ---- cohérence (évaluée à la compilation) -------------------------------------------------------------------------------

namespace detail {

// `pays` est vide (tous les pays) ou une suite de codes « XX » en majuscules séparés par UNE espace, sans espace aux bouts.
constexpr bool pays_bien_ecrit(std::string_view pays) noexcept {
    if (pays.empty())
        return true;
    if (pays.size() % 3 != 2)
        return false;
    for (std::size_t i = 0; i < pays.size(); ++i) {
        const bool separateur = i % 3 == 2;
        const char c = pays[i];
        if (separateur ? c != ' ' : (c < 'A' || c > 'Z'))
            return false;
    }
    return true;
}

constexpr bool table_coherente() noexcept {
    if (NOMBRE == 0 || TABLE[0].id != "auto" || !TABLE[0].modeles.empty() || TABLE[0].limites != 0)
        return false;
    for (std::size_t i = 0; i < NOMBRE; ++i) {
        const Fournisseur &f = TABLE[i];
        if (f.id.empty() || f.libelle.empty() || f.id.size() > 15) // 15 : longueur d'une clé NVS
            return false;
        if (i > 0 && (f.modeles.empty() != (f.famille != Famille::OpenMeteo)))
            return false; // un modèle pour chaque source d'Open-Meteo (sauf « Automatique »), aucun pour un service lu directement
        if (f.famille != Famille::OpenMeteo && f.pays.empty())
            return false; // un service lu directement couvre des pays précis : il n'est jamais « mondial »
        if (f.modeles.size() > MODELES_MAX || !modeles_valide(f.modeles) || !pays_bien_ecrit(f.pays))
            return false;
        for (std::size_t k = 0; k < i; ++k) {
            if (TABLE[k].id == f.id || (!f.modeles.empty() && TABLE[k].modeles == f.modeles))
                return false; // un identifiant, un modèle : une seule entrée
        }
    }
    return true;
}

} // namespace detail

static_assert(detail::table_coherente(),
              "TABLE : « auto » en tête sans modèle, identifiants et modèles uniques et "
              "valables, un modèle seulement pour Open-Meteo, pays bien écrits, tailles bornées");

} // namespace ecran::meteo::fournisseurs
