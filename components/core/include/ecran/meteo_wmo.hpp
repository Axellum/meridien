// Noyau pur : code météo WMO (celui d'Open-Meteo) -> libellé français court + identifiant d'icône jour/nuit.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation (les tests de test/host/ s'en servent).
//
// Source des codes : tableau « WMO Weather interpretation codes (WW) » de https://open-meteo.com/en/docs,
// lu le 06/11/2026 : 29 codes (0 1 2 3 45 48 51 53 55 56 57 61 63 65 66 67 71 73 75 77 80 81 82 85 86 95 96 97 99).
// La page précise que 96 et 99 ne sont rapportés que par les modèles qui prévoient la grêle (DWD ICON, UKMO) ;
// les autres modèles rendent 95 et 97. Les libellés français sont notre traduction (la source est en anglais).
//
// Les textes sont en UTF-8 : sous MSVC, compiler avec /utf-8.
#pragma once

#include <cstdint>
#include <string_view>

namespace ecran::meteo {

// Identifiants d'icône : jeu fermé. L'association à un glyphe (police d'icônes) se fait dans l'interface, avec
// un `switch` sans `default` pour que le compilateur signale toute icône oubliée (jamais un rectangle vide).
// Jour/nuit : seuls les codes 0 à 2 changent d'icône (soleil ou lune).
enum class Icone : std::uint8_t {
    Inconnue = 0,
    Soleil,           // 0, jour
    Lune,             // 0, nuit
    PeuNuageuxJour,   // 1, jour
    PeuNuageuxNuit,   // 1, nuit
    NuageuxJour,      // 2, jour
    NuageuxNuit,      // 2, nuit
    Couvert,          // 3
    Brouillard,       // 45, 48
    Bruine,           // 51, 53, 55
    PluieVerglacante, // 56, 57, 66, 67
    Pluie,            // 61, 63
    PluieForte,       // 65
    Neige,            // 71, 73
    NeigeForte,       // 75
    GrainsDeNeige,    // 77
    Averses,          // 80, 81
    AversesViolentes, // 82
    AversesDeNeige,   // 85, 86
    Orage,            // 95, 97
    OrageGrele,       // 96, 99
    Nombre            // nombre d'icônes (sentinelle, pas une icône)
};

struct Condition {
    std::string_view libelle; // phrase courte en français, jamais un code
    Icone icone;
    bool connu; // faux : code absent du tableau d'Open-Meteo (libellé « Météo inconnue »)
};

// Identifiant stable en ASCII (journaux, tests, éventuelle table d'icônes de l'interface).
constexpr std::string_view nom_icone(Icone icone) noexcept {
    switch (icone) {
    case Icone::Inconnue:
        return "inconnue";
    case Icone::Soleil:
        return "soleil";
    case Icone::Lune:
        return "lune";
    case Icone::PeuNuageuxJour:
        return "peu_nuageux_jour";
    case Icone::PeuNuageuxNuit:
        return "peu_nuageux_nuit";
    case Icone::NuageuxJour:
        return "nuageux_jour";
    case Icone::NuageuxNuit:
        return "nuageux_nuit";
    case Icone::Couvert:
        return "couvert";
    case Icone::Brouillard:
        return "brouillard";
    case Icone::Bruine:
        return "bruine";
    case Icone::PluieVerglacante:
        return "pluie_verglacante";
    case Icone::Pluie:
        return "pluie";
    case Icone::PluieForte:
        return "pluie_forte";
    case Icone::Neige:
        return "neige";
    case Icone::NeigeForte:
        return "neige_forte";
    case Icone::GrainsDeNeige:
        return "grains_de_neige";
    case Icone::Averses:
        return "averses";
    case Icone::AversesViolentes:
        return "averses_violentes";
    case Icone::AversesDeNeige:
        return "averses_de_neige";
    case Icone::Orage:
        return "orage";
    case Icone::OrageGrele:
        return "orage_grele";
    case Icone::Nombre:
        break;
    }
    return "inconnue";
}

// `jour` = champ `is_day` d'Open-Meteo (1 = jour). Les libellés de 0 à 2 sont neutres (valables de nuit).
constexpr Condition decrire(int code_wmo, bool jour) noexcept {
    switch (code_wmo) {
        // clang-format off
        // Une ligne par code : web/faux-serveur/serveur.mjs et web/tests/parcours.py lisent ce tableau par expression
        // régulière (case N: return {"libellé"). clang-format ne doit pas couper ces lignes.
        case 0: return {"Ciel dégagé", jour ? Icone::Soleil : Icone::Lune, true};
        case 1: return {"Plutôt dégagé", jour ? Icone::PeuNuageuxJour : Icone::PeuNuageuxNuit, true};
        case 2: return {"Partiellement nuageux", jour ? Icone::NuageuxJour : Icone::NuageuxNuit, true};
        case 3: return {"Couvert", Icone::Couvert, true};
        case 45: return {"Brouillard", Icone::Brouillard, true};
        case 48: return {"Brouillard givrant", Icone::Brouillard, true};
        case 51: return {"Bruine légère", Icone::Bruine, true};
        case 53: return {"Bruine modérée", Icone::Bruine, true};
        case 55: return {"Bruine dense", Icone::Bruine, true};
        case 56: return {"Bruine verglaçante légère", Icone::PluieVerglacante, true};
        case 57: return {"Bruine verglaçante dense", Icone::PluieVerglacante, true};
        case 61: return {"Pluie faible", Icone::Pluie, true};
        case 63: return {"Pluie modérée", Icone::Pluie, true};
        case 65: return {"Pluie forte", Icone::PluieForte, true};
        case 66: return {"Pluie verglaçante légère", Icone::PluieVerglacante, true};
        case 67: return {"Pluie verglaçante forte", Icone::PluieVerglacante, true};
        case 71: return {"Neige faible", Icone::Neige, true};
        case 73: return {"Neige modérée", Icone::Neige, true};
        case 75: return {"Neige forte", Icone::NeigeForte, true};
        case 77: return {"Grains de neige", Icone::GrainsDeNeige, true};
        case 80: return {"Averses faibles", Icone::Averses, true};
        case 81: return {"Averses modérées", Icone::Averses, true};
        case 82: return {"Averses violentes", Icone::AversesViolentes, true};
        case 85: return {"Averses de neige faibles", Icone::AversesDeNeige, true};
        case 86: return {"Averses de neige fortes", Icone::AversesDeNeige, true};
        case 95: return {"Orage", Icone::Orage, true};
        case 96: return {"Orage avec petite grêle", Icone::OrageGrele, true};
        case 97: return {"Orage violent", Icone::Orage, true};
        case 99: return {"Orage avec forte grêle", Icone::OrageGrele, true};
        default: return {"Météo inconnue", Icone::Inconnue, false};
        // clang-format on
    }
}

} // namespace ecran::meteo
