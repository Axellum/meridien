// Noyau pur : REGISTRE UNIQUE des réglages (ADR-0007). Pour chaque réglage : identifiant, type (choix, nombre, texte, secret,
// lieu), plage, défaut sûr, libellé et aide (clés de ecran/i18n.hpp), section, étape de l'installation guidée, et son chemin
// dans l'API (docs/api-reglages.md). L'écran et l'API en sont deux vues du même registre : un test du PC échoue si un réglage
// existe d'un côté seulement (test/host/test_registre_reglages.cpp, test/host/controle_reglages.py).
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation.
//
// Ce que ce fichier ne fait PAS : lire ni écrire en NVS, appeler l'API, dessiner. Les validations de la page web
// (reglages_valeurs.hpp, session reglages-appareil) restent le moteur côté appareil ; ici seulement les bornes que l'écran
// connaît avant d'envoyer une valeur (longueur d'un texte, plage d'un nombre).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/i18n.hpp"

namespace ecran::registre {

enum class Type : std::uint8_t {
    Choix,  // une option parmi une liste (langue, thème, préréglage de service)
    Nombre, // entier dans [min, max] (luminosité)
    Texte,  // texte libre au clavier (nom de réseau, adresse, modèle, voix)
    Secret, // texte jamais relu ni affiché en clair par défaut (mot de passe Wi-Fi, clé d'accès)
    Lieu,   // une ville choisie dans une recherche (nom, coordonnées, fuseau)
};

// Les six sections de la liste des réglages (ADR-0007, point 3), dans l'ordre d'affichage.
enum class Section : std::uint8_t { Reseau, LieuMeteo, Langue, Affichage, AssistantVocal, Appareil };
inline constexpr std::size_t NOMBRE_SECTIONS = 6;

// Étapes de l'installation guidée (ADR-0007, point 5), dans l'ordre : chacune dépend de la précédente.
// `Aucune` = le réglage ne fait pas partie de l'installation.
enum class Etape : std::uint8_t {
    Aucune = 0,
    Langue = 1,
    Wifi = 2,
    Lieu = 3,
    Affichage = 4,
    Assistant = 5,
    Telephone = 6,
    Recapitulatif = 7
};
inline constexpr int NOMBRE_ETAPES = 7;

// Une option d'un choix : `code` est la valeur de l'API (« clair », « openai », « de »), `libelle` le texte affiché.
// `traduire` : le libellé est une clé de tr() (faux pour un nom propre : « OpenAI », « Deutsch »).
struct Option {
    std::string_view code;
    std::string_view libelle;
    bool traduire;
};

// Unité des bornes d'un texte : l'API compte des caractères (adresse, modèle), le Wi-Fi des octets (SSID 32, mot de passe 63).
enum class Longueur : std::uint8_t { Caracteres, Octets };

struct Reglage {
    std::string_view id; // « affichage.luminosite » : stable, jamais traduit, jamais affiché
    Type type;
    Section section;
    Etape etape;              // installation guidée
    std::string_view libelle; // clé de tr()
    std::string_view aide;    // clé de tr(), ou vide
    // Contraintes. Nombre : valeur dans [min, max]. Texte et Secret : `max` = longueur maximale, `min` = longueur minimale
    // d'une valeur non vide. Choix : indice dans [0, nb_options).
    std::int32_t min;
    std::int32_t max;
    std::int32_t
        defaut; // Choix : indice de l'option (-1 = pas encore choisi, seulement si `facultatif`) ; Nombre : valeur
    std::string_view defaut_texte; // Texte et Secret : valeur par défaut (vide partout aujourd'hui)
    const Option *options;
    std::size_t nb_options;
    std::string_view unite; // « % », ou vide
    Longueur longueur;
    bool facultatif; // peut rester vide (Texte, Secret) ou non choisi (Choix)
    // Chemin dans l'API (docs/api-reglages.md) : méthode, route, champ du corps JSON.
    std::string_view route;
    std::string_view champ;
};

// ---- options ------------------------------------------------------------------------------------------------------------

// Les sept langues : codes et noms dans leur propre langue, ceux de ecran/i18n.hpp (un test les compare).
inline constexpr Option OPTIONS_LANGUES[i18n::NOMBRE_LANGUES] = {
    {"fr", "Français", false}, {"en", "English", false},  {"de", "Deutsch", false}, {"nl", "Nederlands", false},
    {"es", "Español", false},  {"it", "Italiano", false}, {"tr", "Türkçe", false},
};

inline constexpr Option OPTIONS_THEMES[3] = {
    {"auto", "Auto", true}, {"clair", "Clair", true}, {"sombre", "Sombre", true}};

// Le profil de l'accueil (ADR-0023) : seulement ce que l'appareil sait dessiner. Même ordre que reglages::ProfilAccueil (un test le compare).
inline constexpr Option OPTIONS_PROFILS[2] = {{"auto", "Auto", true}, {"meteo", "Météo", true}};

// Préréglages par service (docs/api-reglages.md §7 : un préréglage n'existe que pour les services que son fournisseur
// documente). Le catalogue complet (adresse, aide, clé requise) est dans ecran/presets_services.hpp.
inline constexpr Option OPTIONS_PRESETS_STT[3] = {
    {"openai", "OpenAI", false}, {"ollama", "Ollama (mon PC)", true}, {"personnalise", "Personnalisé", true}};
inline constexpr Option OPTIONS_PRESETS_LLM[5] = {{"openai", "OpenAI", false},
                                                  {"google", "Google", false},
                                                  {"lmstudio", "LM Studio (mon PC)", true},
                                                  {"ollama", "Ollama (mon PC)", true},
                                                  {"personnalise", "Personnalisé", true}};
inline constexpr Option OPTIONS_PRESETS_TTS[4] = {{"openai", "OpenAI", false},
                                                  {"ollama", "Ollama (mon PC)", true},
                                                  {"kokoro", "Kokoro (mon PC)", true},
                                                  {"personnalise", "Personnalisé", true}};

// Alertes officielles de Vigie (ADR-0009, docs/api-reglages.md §14). « Oui » vient en premier : le défaut est l'indice 0, et
// l'indice du niveau minimal est celui de vigie::NiveauMin (un test le compare, ce fichier n'inclut pas les en-têtes de Vigie).
inline constexpr Option OPTIONS_OUI_NON[2] = {{"true", "Oui", true}, {"false", "Non", true}};
inline constexpr Option OPTIONS_NIVEAUX_ALERTE[3] = {
    {"yellow", "Jaune", true}, {"orange", "Orange", true}, {"red", "Rouge", true}};

// ---- bornes (docs/api-reglages.md) --------------------------------------------------------------------------------------

inline constexpr std::int32_t LUMINOSITE_MIN = 5; // §6 : un plancher évite un écran rendu illisible
inline constexpr std::int32_t LUMINOSITE_MAX = 100;
inline constexpr std::int32_t LUMINOSITE_DEFAUT = 75; // comme le rétroéclairage du premier démarrage (main.cpp)
inline constexpr std::int32_t SSID_OCTETS_MAX = 32;
inline constexpr std::int32_t MOT_DE_PASSE_WIFI_MIN = 8; // WPA2 : 8 à 63 caractères ; vide = réseau ouvert
inline constexpr std::int32_t MOT_DE_PASSE_WIFI_MAX = 63;
inline constexpr std::int32_t URL_MAX = 200;        // §7
inline constexpr std::int32_t TEXTE_COURT_MAX = 80; // §7 : modèle, voix
inline constexpr std::int32_t CLE_MAX =
    256; // clés d'accès connues : de 40 à ~170 caractères ; la limite du corps JSON est 4 Ko
inline constexpr std::int32_t RECHERCHE_LIEU_MIN = 3; // §5
inline constexpr std::int32_t RECHERCHE_LIEU_MAX = 80;
// §14. Les mêmes nombres que vigie::PERIODE_*_MIN et vigie::ADRESSE_MAX (test/host/test_registre_reglages.cpp les compare).
inline constexpr std::int32_t ALERTES_PERIODE_MIN = 10; // minutes entre deux requêtes : l'instance publique est limitée
inline constexpr std::int32_t ALERTES_PERIODE_MAX = 180;
inline constexpr std::int32_t ALERTES_PERIODE_DEFAUT = 15;
inline constexpr std::int32_t ALERTES_ADRESSE_MAX = 80; // octets

namespace detail {

// Les sources de la météo (ecran/fournisseurs_meteo.hpp) : une option par entrée de la table, dans le même ordre (la position est
// l'indice du choix). `code` est l'identifiant stable (« auto », « knmi »), servi par l'API et gardé en mémoire permanente ;
// seul « Automatique » est traduit, un nom de service est un nom propre. La liste PROPOSÉE pour une ville est plus courte que cette
// liste : elle dépend du pays (fournisseurs::pour_pays), l'écran et l'API la filtrent.
struct OptionsSourcesMeteo {
    Option v[meteo::fournisseurs::NOMBRE] = {};
};

constexpr OptionsSourcesMeteo options_sources_meteo() noexcept {
    OptionsSourcesMeteo o;
    for (std::size_t i = 0; i < meteo::fournisseurs::NOMBRE; ++i) {
        const meteo::fournisseurs::Fournisseur &f = meteo::fournisseurs::TABLE[i];
        o.v[i] = {f.id, f.libelle, f.libelle_traduit};
    }
    return o;
}

inline constexpr OptionsSourcesMeteo OPTIONS_SOURCES_METEO = options_sources_meteo();

// Les trois services (STT, LLM, TTS) ont la même forme de réglages : un préréglage, une adresse, un modèle, une clé.
constexpr Reglage service_preset(std::string_view id, const Option *options, std::size_t nb) noexcept {
    return {id,
            Type::Choix,
            Section::AssistantVocal,
            Etape::Assistant,
            "Préréglage",
            "",
            0,
            static_cast<std::int32_t>(nb) - 1,
            -1,
            "",
            options,
            nb,
            "",
            Longueur::Caracteres,
            true,
            "/api/services/{id}",
            "preset"};
}

constexpr Reglage service_texte(std::string_view id, std::string_view libelle, std::int32_t max,
                                std::string_view champ) noexcept {
    return {id,
            Type::Texte,
            Section::AssistantVocal,
            Etape::Assistant,
            libelle,
            "",
            0,
            max,
            0,
            "",
            nullptr,
            0,
            "",
            Longueur::Caracteres,
            true,
            "/api/services/{id}",
            champ};
}

constexpr Reglage service_cle(std::string_view id) noexcept {
    return {id,
            Type::Secret,
            Section::AssistantVocal,
            Etape::Assistant,
            "Clé d’accès",
            "Une fois enregistrée, la clé ne s’affiche plus nulle part.",
            0,
            CLE_MAX,
            0,
            "",
            nullptr,
            0,
            "",
            Longueur::Caracteres,
            true,
            "/api/services/{id}",
            "cle"};
}

} // namespace detail

// ---- le registre --------------------------------------------------------------------------------------------------------

inline constexpr Reglage REGISTRE[] = {
    // --- Réseau ---
    {"wifi.ssid", Type::Texte, Section::Reseau, Etape::Wifi, "Réseau Wi-Fi", "", 0, SSID_OCTETS_MAX, 0, "", nullptr, 0,
     "", Longueur::Octets, false, "/api/wifi", "ssid"},
    {"wifi.mot_de_passe", Type::Secret, Section::Reseau, Etape::Wifi, "Mot de passe du Wi-Fi", "",
     MOT_DE_PASSE_WIFI_MIN, MOT_DE_PASSE_WIFI_MAX, 0, "", nullptr, 0, "", Longueur::Octets, true, "/api/wifi",
     "mot_de_passe"},
    // --- Lieu et météo ---
    {"lieu.ville", Type::Lieu, Section::LieuMeteo, Etape::Lieu, "Ville", "L’écran en déduit la météo et l’heure.",
     RECHERCHE_LIEU_MIN, RECHERCHE_LIEU_MAX, 0, "", nullptr, 0, "", Longueur::Caracteres, false, "/api/location",
     "nom"},
    // La source de la météo de cette ville : « Automatique » ou le service national du pays (ADR-0010). Liste filtrée par pays.
    {"meteo.source", Type::Choix, Section::LieuMeteo, Etape::Aucune, "Source météo",
     "Le service qui fournit la météo de la ville choisie.", 0,
     static_cast<std::int32_t>(meteo::fournisseurs::NOMBRE) - 1, 0, "", detail::OPTIONS_SOURCES_METEO.v,
     meteo::fournisseurs::NOMBRE, "", Longueur::Caracteres, false, "/api/location", "source_meteo"},
    // --- Lieu et météo : alertes officielles (hors installation guidée : les défauts conviennent) ---
    {"alertes.actives", Type::Choix, Section::LieuMeteo, Etape::Aucune, "Alertes officielles",
     "Affiche sur l’écran les alertes météo officielles relayées par Vigie, tant qu’une est en vigueur.", 0, 1, 0, "",
     OPTIONS_OUI_NON, 2, "", Longueur::Caracteres, false, "/api/alerts", "actives"},
    {"alertes.niveau", Type::Choix, Section::LieuMeteo, Etape::Aucune, "Niveau minimal",
     "Les alertes moins graves ne s’affichent pas.", 0, 2, 0, "", OPTIONS_NIVEAUX_ALERTE, 3, "", Longueur::Caracteres,
     false, "/api/alerts", "niveau_min"},
    {"alertes.periode", Type::Nombre, Section::LieuMeteo, Etape::Aucune, "Mise à jour des alertes",
     "L’écran demande les alertes à Vigie à ce rythme : 15 minutes par défaut, 10 au minimum.", ALERTES_PERIODE_MIN,
     ALERTES_PERIODE_MAX, ALERTES_PERIODE_DEFAUT, "", nullptr, 0, "min", Longueur::Caracteres, false, "/api/alerts",
     "periode_min"},
    {"alertes.adresse", Type::Texte, Section::LieuMeteo, Etape::Aucune, "Adresse de Vigie",
     "Laissez vide pour utiliser le service public de Vigie. Une adresse commence par https://.", 0,
     ALERTES_ADRESSE_MAX, 0, "", nullptr, 0, "", Longueur::Octets, true, "/api/alerts", "adresse"},
    // --- Langue ---
    {"langue.ecran", Type::Choix, Section::Langue, Etape::Langue, "Langue de l’écran", "", 0, 6, 0, "", OPTIONS_LANGUES,
     i18n::NOMBRE_LANGUES, "", Longueur::Caracteres, false, "/api/display", "langue_ecran"},
    {"langue.assistant", Type::Choix, Section::Langue, Etape::Assistant, "Langue de l’assistant",
     "Langue que l’assistant comprend et parle.", 0, 6, 0, "", OPTIONS_LANGUES, i18n::NOMBRE_LANGUES, "",
     Longueur::Caracteres, false, "/api/display", "langue_assistant"},
    // --- Affichage ---
    {"affichage.theme", Type::Choix, Section::Affichage, Etape::Affichage, "Thème", "", 0, 2, 0, "", OPTIONS_THEMES, 3,
     "", Longueur::Caracteres, false, "/api/display", "theme"},
    {"affichage.luminosite", Type::Nombre, Section::Affichage, Etape::Affichage, "Luminosité", "", LUMINOSITE_MIN,
     LUMINOSITE_MAX, LUMINOSITE_DEFAUT, "", nullptr, 0, "%", Longueur::Caracteres, false, "/api/display", "luminosite"},
    // Le profil de l'accueil (ADR-0023) : hors installation guidée, le défaut « Auto » convient.
    {"affichage.profil", Type::Choix, Section::Affichage, Etape::Aucune, "Profil",
     "Auto : l’écran choisit seul, selon la pluie et les alertes. Météo : l’écran reste sur la météo (une alerte "
     "officielle s’affiche quand même).",
     0, 1, 0, "", OPTIONS_PROFILS, 2, "", Longueur::Caracteres, false, "/api/display", "profil"},
    // --- Assistant vocal : trois services indépendants (ADR-0002) ---
    detail::service_preset("stt.preset", OPTIONS_PRESETS_STT, 3),
    detail::service_texte("stt.url", "Adresse du service", URL_MAX, "url"),
    detail::service_texte("stt.modele", "Modèle", TEXTE_COURT_MAX, "modele"),
    detail::service_cle("stt.cle"),
    detail::service_preset("llm.preset", OPTIONS_PRESETS_LLM, 5),
    detail::service_texte("llm.url", "Adresse du service", URL_MAX, "url"),
    detail::service_texte("llm.modele", "Modèle", TEXTE_COURT_MAX, "modele"),
    detail::service_cle("llm.cle"),
    detail::service_preset("tts.preset", OPTIONS_PRESETS_TTS, 4),
    detail::service_texte("tts.url", "Adresse du service", URL_MAX, "url"),
    detail::service_texte("tts.modele", "Modèle", TEXTE_COURT_MAX, "modele"),
    detail::service_texte("tts.voix", "Voix", TEXTE_COURT_MAX, "voix"),
    detail::service_cle("tts.cle"),
};

inline constexpr std::size_t NOMBRE_REGLAGES = sizeof(REGISTRE) / sizeof(REGISTRE[0]);

// ---- recherche ----------------------------------------------------------------------------------------------------------

constexpr const Reglage *chercher(std::string_view id) noexcept {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        if (REGISTRE[i].id == id)
            return &REGISTRE[i];
    }
    return nullptr;
}

// Premier réglage dont l'identifiant commence par `prefixe + "."` (« stt » → stt.preset), ou rien.
constexpr const Reglage *premier_du_groupe(std::string_view prefixe) noexcept {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        const std::string_view id = REGISTRE[i].id;
        if (id.size() > prefixe.size() && id.substr(0, prefixe.size()) == prefixe && id[prefixe.size()] == '.')
            return &REGISTRE[i];
    }
    return nullptr;
}

constexpr std::size_t compter_section(Section s) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i)
        n += REGISTRE[i].section == s ? 1 : 0;
    return n;
}

constexpr std::size_t compter_etape(Etape e) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i)
        n += REGISTRE[i].etape == e ? 1 : 0;
    return n;
}

// ---- validations que l'écran connaît ----------------------------------------------------------------------------------

constexpr bool nombre_valide(const Reglage &r, long long v) noexcept {
    return r.type == Type::Nombre && v >= r.min && v <= r.max;
}

// Valeur lue en mémoire permanente → valeur sûre : hors plage, on retombe sur le défaut (un NVS abîmé ne doit pas éteindre l'écran).
constexpr std::int32_t nombre_depuis_nvs(const Reglage &r, long long v) noexcept {
    return nombre_valide(r, v) ? static_cast<std::int32_t>(v) : r.defaut;
}

// Indice de choix lu en mémoire permanente → indice sûr (hors liste : le défaut du réglage).
constexpr std::int32_t choix_depuis_nvs(const Reglage &r, long long v) noexcept {
    const bool dans_la_liste = r.type == Type::Choix && v >= 0 && v < static_cast<long long>(r.nb_options);
    return dans_la_liste ? static_cast<std::int32_t>(v) : r.defaut;
}

constexpr int indice_option(const Reglage &r, std::string_view code) noexcept {
    for (std::size_t i = 0; i < r.nb_options; ++i) {
        if (r.options[i].code == code)
            return static_cast<int>(i);
    }
    return -1;
}

// Nombre de caractères d'un texte UTF-8 (les octets de continuation ne comptent pas).
constexpr std::size_t nombre_de_caracteres(std::string_view s) noexcept {
    std::size_t n = 0;
    for (char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u)
            ++n;
    }
    return n;
}

enum class ErreurTexte : std::uint8_t { Aucune, Vide, TropCourt, TropLong, CaractereInterdit };

// Texte saisi au clavier pour un réglage Texte ou Secret : vide permis seulement si `facultatif` ; sinon entre `min` (si > 0)
// et `max`, en caractères ou en octets selon le réglage ; pas d'octet de contrôle.
constexpr ErreurTexte valider_texte(const Reglage &r, std::string_view s) noexcept {
    if (r.type != Type::Texte && r.type != Type::Secret)
        return ErreurTexte::CaractereInterdit;
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c == 0x7F)
            return ErreurTexte::CaractereInterdit;
    }
    if (s.empty())
        return r.facultatif ? ErreurTexte::Aucune : ErreurTexte::Vide;
    const std::size_t n = r.longueur == Longueur::Octets ? s.size() : nombre_de_caracteres(s);
    if (n > static_cast<std::size_t>(r.max))
        return ErreurTexte::TropLong;
    if (r.min > 0 && n < static_cast<std::size_t>(r.min))
        return ErreurTexte::TropCourt;
    return ErreurTexte::Aucune;
}

} // namespace ecran::registre
