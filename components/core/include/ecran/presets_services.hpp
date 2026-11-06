// Noyau pur : catalogue des préréglages des trois services de l'assistant vocal (docs/api-reglages.md §7). L'écran s'en sert pour
// préremplir l'adresse, dire si une clé est requise et afficher l'aide ; la page web et l'API (`GET /api/presets`) en sont l'autre
// vue : le catalogue vient de l'appareil, la page n'en code aucun.
//
// Sources de chaque ligne : le tableau du §7 (lu le 06/11/2026 dans la documentation de chaque fournisseur). Ce que le fournisseur
// ne documente pas (audio chez Google, LM Studio) n'a pas de préréglage pour ce service. Les ports sont ceux des exemples des
// documentations, « défaut officiel » non vérifié. `localhost` n'a aucun sens depuis l'écran : l'écran demande l'adresse du PC.
// C++17 standard, aucun include ESP-IDF ni LVGL.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::registre {

// Les services, comme dans l'API (`stt`, `llm`, `tts`) et dans les identifiants du registre.
enum class Service : std::uint8_t { Stt = 0, Llm = 1, Tts = 2 };
inline constexpr std::size_t NOMBRE_SERVICES = 3;
inline constexpr std::string_view CODES_SERVICES[NOMBRE_SERVICES] = {"stt", "llm", "tts"};

constexpr std::uint8_t masque(Service s) noexcept { return static_cast<std::uint8_t>(1u << static_cast<unsigned>(s)); }

struct Preset {
    std::string_view id;  // valeur de l'API
    std::string_view nom; // clé de tr() (ou nom propre si `nom_traduit` est faux)
    bool nom_traduit;
    std::string_view url; // adresse de base, sans « / » final
    bool local;           // tourne sur le PC de l'utilisateur : remplacer « localhost » par son adresse
    bool cle_requise;
    std::string_view aide; // clé de tr(), ou vide
    std::uint8_t services; // masque des services pour lesquels le fournisseur documente la route
};

inline constexpr std::uint8_t TOUS_LES_SERVICES = masque(Service::Stt) | masque(Service::Llm) | masque(Service::Tts);

inline constexpr Preset PRESETS[] = {
    {"openai", "OpenAI", false, "https://api.openai.com/v1", false, true, "Service sur Internet : il faut une clé.",
     TOUS_LES_SERVICES},
    {"google", "Google", false, "https://generativelanguage.googleapis.com/v1beta/openai", false, true,
     "Service sur Internet : il faut une clé.", masque(Service::Llm)},
    {"lmstudio", "LM Studio (mon PC)", true, "http://localhost:1234/v1", true, false,
     "Lancez le serveur local de LM Studio sur le PC.", masque(Service::Llm)},
    {"ollama", "Ollama (mon PC)", true, "http://localhost:11434/v1", true, false, "Ollama doit tourner sur votre PC.",
     TOUS_LES_SERVICES},
    {"kokoro", "Kokoro (mon PC)", true, "http://localhost:8880/v1", true, false,
     "Voix française locale, à lancer sur le PC.", masque(Service::Tts)},
    {"personnalise", "Personnalisé", true, "", false, false, "Tout service compatible OpenAI.", TOUS_LES_SERVICES},
};
inline constexpr std::size_t NOMBRE_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

constexpr const Preset *chercher_preset(std::string_view id) noexcept {
    for (std::size_t i = 0; i < NOMBRE_PRESETS; ++i) {
        if (PRESETS[i].id == id)
            return &PRESETS[i];
    }
    return nullptr;
}

constexpr bool preset_pour(const Preset &p, Service s) noexcept { return (p.services & masque(s)) != 0; }

constexpr std::size_t compter_presets(Service s) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < NOMBRE_PRESETS; ++i)
        n += preset_pour(PRESETS[i], s) ? 1 : 0;
    return n;
}

// L'adresse du préréglage contient-elle « localhost » ? L'écran demande alors l'adresse du PC (rien n'est bloqué : c'est un conseil).
constexpr bool contient_localhost(std::string_view url) noexcept {
    constexpr std::string_view cherche = "localhost";
    if (url.size() < cherche.size())
        return false;
    for (std::size_t i = 0; i + cherche.size() <= url.size(); ++i) {
        if (url.substr(i, cherche.size()) == cherche)
            return true;
    }
    return false;
}

} // namespace ecran::registre
