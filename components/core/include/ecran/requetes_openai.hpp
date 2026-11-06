// Noyau pur : construction des requêtes vers un service compatible OpenAI (ADR-0002) : corps JSON de
// `chat/completions` en flux, corps JSON de `audio/speech`, en-tête et cadres multipart de `audio/transcriptions`
// (sans l'audio), assemblage « adresse de base + route ». C++17 standard, aucun include ESP-IDF, aucune
// allocation : tout s'écrit dans un `Tampon` posé sur un tableau de l'appelant (débordement = `deborde()`, jamais de
// texte tronqué). Évaluable à la compilation (les tests de test/host/ s'en servent).
//
// Échappement JSON : « " », « \ », \b \f \n \r \t, tout autre caractère de contrôle (< 0x20) en \u00xx ; les
// caractères non ASCII valides restent en UTF-8 tels quels (accents, €, émojis) ; tout octet qui ne forme pas une
// séquence UTF-8 valide (texte coupé en plein caractère, octet isolé) est remplacé par U+FFFD, un par octet, pour
// que le corps soit TOUJOURS un JSON UTF-8 valide (sinon le service répond 400).
//
// Rien n'est inventé : champ vide = champ omis (`model`, `voice`, `language`), sauf ce que l'API exige (`input`).
// Le corps de chat n'envoie ni `max_tokens` ni `temperature` : le nom de la limite a changé selon les modèles
// d'OpenAI (non vérifié lequel passe où) ; la brièveté se demande dans le prompt système.
//
// Les textes sont en UTF-8 : sous MSVC, compiler avec /utf-8.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/assistant_texte.hpp"

namespace ecran::assistant {

// ---------------------------------------------------------------------------------------------------------------
// Routes (relatives à l'adresse de base du service, voir docs/api-reglages.md §7)
// ---------------------------------------------------------------------------------------------------------------
inline constexpr std::string_view ROUTE_CHAT = "chat/completions";
inline constexpr std::string_view ROUTE_TTS = "audio/speech";
inline constexpr std::string_view ROUTE_STT = "audio/transcriptions";
inline constexpr std::string_view ROUTE_MODELES = "models";

namespace detail {

constexpr bool est_chiffre(char c) noexcept { return c >= '0' && c <= '9'; }

// Segment de version d'API : « v1 », « v2 », « v1beta », « v1alpha2 » (lettres minuscules, chiffres).
constexpr bool segment_de_version(std::string_view s) noexcept {
    if (s.size() < 2 || s[0] != 'v' || !est_chiffre(s[1])) return false;
    std::size_t i = 1;
    while (i < s.size() && est_chiffre(s[i])) ++i;
    if (i == s.size()) return true;
    std::size_t j = i;
    while (j < s.size() && s[j] >= 'a' && s[j] <= 'z') ++j;
    if (j == i) return false;  // pas de lettres : ce qui suit les chiffres n'est ni lettre ni fin
    while (j < s.size() && est_chiffre(s[j])) ++j;
    return j == s.size();
}

constexpr bool commence_par(std::string_view s, std::string_view prefixe) noexcept {
    return s.size() >= prefixe.size() && s.substr(0, prefixe.size()) == prefixe;
}

constexpr bool caractere_de_controle(char c) noexcept {
    const unsigned u = static_cast<unsigned char>(c);
    return u < 0x20U || u == 0x7FU;
}

}  // namespace detail

// ---------------------------------------------------------------------------------------------------------------
// Adresse
// ---------------------------------------------------------------------------------------------------------------

// Vrai si le chemin de l'adresse de base contient un segment de version (« /v1 », « /v1beta »…). Faux = le
// service répondra sans doute 404 : c'est le cas du « il manque peut-être /v1 » (docs/api-reglages.md §8).
// Ne corrige RIEN toute seule : l'adresse de Google (« …/v1beta/openai ») n'a pas de « /v1 ».
constexpr bool base_a_une_version(std::string_view base) noexcept {
    std::size_t debut = base.find("://");
    if (debut == std::string_view::npos) return false;
    debut += 3;
    const std::size_t chemin = base.find('/', debut);
    if (chemin == std::string_view::npos) return false;
    for (std::size_t i = chemin + 1; i <= base.size();) {
        std::size_t fin = base.find('/', i);
        if (fin == std::string_view::npos) fin = base.size();
        if (detail::segment_de_version(base.substr(i, fin - i))) return true;
        i = fin + 1;
    }
    return false;
}

// Écrit « base / route ». `base` : « http:// » ou « https:// », sans espace ni caractère de contrôle, sans
// « ? » ni « # » (une adresse à paramètres, façon Azure, n'est pas gérée). Les « / » en trop à la jonction sont
// supprimés. Cas du « /v1 doublé » : si la base finit déjà par « /v1 » et que la route commence par « v1/ »,
// ce « v1/ » est retiré (…/v1 + /v1/chat/completions = …/v1/chat/completions). Faux si la base est invalide
// ou si le tampon déborde.
constexpr bool assembler_url(Tampon& sortie, std::string_view base, std::string_view route) noexcept {
    if (!detail::commence_par(base, "http://") && !detail::commence_par(base, "https://")) return false;
    for (char c : base) {
        if (detail::caractere_de_controle(c) || c == ' ' || c == '?' || c == '#') return false;
    }
    for (char c : route) {
        if (detail::caractere_de_controle(c) || c == ' ') return false;
    }
    while (!base.empty() && base.back() == '/') base.remove_suffix(1);
    while (!route.empty() && route.front() == '/') route.remove_prefix(1);
    const std::size_t dernier = base.rfind('/');
    const std::string_view dernier_segment = dernier == std::string_view::npos ? base : base.substr(dernier + 1);
    if (dernier_segment == "v1" && (route == "v1" || detail::commence_par(route, "v1/"))) {
        route.remove_prefix(route.size() == 2 ? 2 : 3);
    }
    sortie.ecrire(base);
    if (!route.empty()) {
        sortie.ecrire('/');
        sortie.ecrire(route);
    }
    return sortie.ok();
}

// ---------------------------------------------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------------------------------------------

// Écrit `s` comme chaîne JSON, guillemets compris.
constexpr bool json_chaine(Tampon& t, std::string_view s) noexcept {
    constexpr std::string_view HEX = "0123456789abcdef";
    t.ecrire('"');
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x80U) {
            const std::size_t n = utf8::sequence_valide(s, i);
            if (n == 0) {
                t.ecrire("\xEF\xBF\xBD");  // U+FFFD, un par octet invalide
                ++i;
            } else {
                for (std::size_t k = 0; k < n; ++k) t.ecrire(s[i + k]);
                i += n;
            }
            continue;
        }
        switch (c) {
            case '"': t.ecrire("\\\""); break;
            case '\\': t.ecrire("\\\\"); break;
            case '\b': t.ecrire("\\b"); break;
            case '\f': t.ecrire("\\f"); break;
            case '\n': t.ecrire("\\n"); break;
            case '\r': t.ecrire("\\r"); break;
            case '\t': t.ecrire("\\t"); break;
            default:
                if (c < 0x20U) {
                    t.ecrire("\\u00");
                    t.ecrire(HEX[c >> 4]);
                    t.ecrire(HEX[c & 15U]);
                } else {
                    t.ecrire(static_cast<char>(c));
                }
        }
        ++i;
    }
    t.ecrire('"');
    return t.ok();
}

namespace detail {
// Écrit `"nom":"valeur"` ; sépare par une virgule si `virgule`. Rend vrai si le champ a été écrit.
constexpr void champ_texte(Tampon& t, bool& premier, std::string_view nom, std::string_view valeur) noexcept {
    if (!premier) t.ecrire(',');
    premier = false;
    json_chaine(t, nom);
    t.ecrire(':');
    json_chaine(t, valeur);
}
}  // namespace detail

// ---------------------------------------------------------------------------------------------------------------
// Chat (LLM)
// ---------------------------------------------------------------------------------------------------------------
enum class Role : std::uint8_t { Systeme, Utilisateur, Assistant };

constexpr std::string_view nom_role(Role role) noexcept {
    switch (role) {
        case Role::Systeme: return "system";
        case Role::Utilisateur: return "user";
        case Role::Assistant: return "assistant";
    }
    return "user";
}

struct Message {
    Role role;
    std::string_view contenu;
};

// Fenêtre d'historique bornée en OCTETS de contenu (avant échappement JSON, hors prompt système) : rend l'indice
// du premier message retenu parmi `messages[0, n)`, du plus ancien au plus récent. On remplit depuis le plus
// récent tant que le total reste ≤ `budget_octets` ; le DERNIER message (la question de l'utilisateur) est
// toujours retenu, même s'il dépasse seul le budget ; le premier message retenu est toujours celui d'un
// utilisateur (une réponse orpheline, dont la question est sortie de la fenêtre, est écartée).
constexpr std::size_t premier_message_retenu(const Message* messages, std::size_t n, std::size_t budget_octets) noexcept {
    if (messages == nullptr || n == 0) return 0;
    std::size_t total = 0;
    std::size_t premier = n;
    while (premier > 0) {
        const std::size_t taille = messages[premier - 1].contenu.size();
        if (premier < n && taille > budget_octets - (total < budget_octets ? total : budget_octets)) break;
        total += taille;
        --premier;
    }
    while (premier + 1 < n && messages[premier].role != Role::Utilisateur) ++premier;
    return premier;
}

struct OptionsChat {
    std::string_view modele;          // vide = champ omis (le serveur prend son modèle chargé)
    std::string_view prompt_systeme;  // vide = pas de message système
    const Message* historique = nullptr;
    std::size_t nombre = 0;           // messages de `historique` ; le dernier est la question en cours
    std::size_t budget_octets = 4096;
};

// {"model":"…","stream":true,"messages":[{"role":"system","content":"…"},{"role":"user","content":"…"},…]}
// Faux si le tampon déborde ou s'il n'y a aucun message à envoyer.
constexpr bool corps_chat(Tampon& t, const OptionsChat& o) noexcept {
    if (o.nombre == 0 || o.historique == nullptr) return false;
    bool premier = true;
    t.ecrire('{');
    if (!o.modele.empty()) detail::champ_texte(t, premier, "model", o.modele);
    if (!premier) t.ecrire(',');
    t.ecrire("\"stream\":true,\"messages\":[");
    bool premier_message = true;
    if (!o.prompt_systeme.empty()) {
        t.ecrire("{\"role\":\"system\",\"content\":");
        json_chaine(t, o.prompt_systeme);
        t.ecrire('}');
        premier_message = false;
    }
    for (std::size_t i = premier_message_retenu(o.historique, o.nombre, o.budget_octets); i < o.nombre; ++i) {
        if (!premier_message) t.ecrire(',');
        premier_message = false;
        t.ecrire("{\"role\":");
        json_chaine(t, nom_role(o.historique[i].role));
        t.ecrire(",\"content\":");
        json_chaine(t, o.historique[i].contenu);
        t.ecrire('}');
    }
    t.ecrire("]}");
    return t.ok();
}

// ---------------------------------------------------------------------------------------------------------------
// Synthèse vocale (TTS)
// ---------------------------------------------------------------------------------------------------------------
enum class FormatAudio : std::uint8_t { Pcm, Wav, Mp3, Opus };

constexpr std::string_view nom_format_audio(FormatAudio f) noexcept {
    switch (f) {
        case FormatAudio::Pcm: return "pcm";
        case FormatAudio::Wav: return "wav";
        case FormatAudio::Mp3: return "mp3";
        case FormatAudio::Opus: return "opus";
    }
    return "pcm";
}

struct OptionsTts {
    std::string_view modele;  // vide = omis
    std::string_view voix;    // vide = omis
    std::string_view texte;   // obligatoire
    FormatAudio format = FormatAudio::Pcm;
};

// {"model":"…","input":"…","voice":"…","response_format":"pcm"} ; faux si `texte` est vide ou le tampon déborde.
// Le format rendu par chaque serveur (cadence du PCM, etc.) n'est PAS vérifié ici : à constater sur le service.
constexpr bool corps_tts(Tampon& t, const OptionsTts& o) noexcept {
    if (o.texte.empty()) return false;
    bool premier = true;
    t.ecrire('{');
    if (!o.modele.empty()) detail::champ_texte(t, premier, "model", o.modele);
    detail::champ_texte(t, premier, "input", o.texte);
    if (!o.voix.empty()) detail::champ_texte(t, premier, "voice", o.voix);
    detail::champ_texte(t, premier, "response_format", nom_format_audio(o.format));
    t.ecrire('}');
    return t.ok();
}

// ---------------------------------------------------------------------------------------------------------------
// Transcription (STT) : multipart/form-data, sans l'audio
// ---------------------------------------------------------------------------------------------------------------

// Frontière admissible : 1 à 70 caractères parmi lettres, chiffres, « - » et « _ » (sous-ensemble sûr de la RFC 2046).
constexpr bool frontiere_valide(std::string_view f) noexcept {
    if (f.empty() || f.size() > 70) return false;
    for (char c : f) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || detail::est_chiffre(c) || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

// Écrit une frontière « ecran-» + 16 chiffres hexadécimaux tirés de `graine` (l'appelant fournit l'aléa, par ex.
// esp_random() sur 64 bits : le noyau reste pur).
constexpr bool ecrire_frontiere(Tampon& t, std::uint64_t graine) noexcept {
    constexpr std::string_view HEX = "0123456789abcdef";
    t.ecrire("ecran-");
    for (int i = 15; i >= 0; --i) t.ecrire(HEX[static_cast<std::size_t>((graine >> (4 * i)) & 0xFU)]);
    return t.ok();
}

// Valeur de l'en-tête « Content-Type: » de la requête.
constexpr bool type_contenu_multipart(Tampon& t, std::string_view frontiere) noexcept {
    if (!frontiere_valide(frontiere)) return false;
    t.ecrire("multipart/form-data; boundary=");
    t.ecrire(frontiere);
    return t.ok();
}

struct OptionsStt {
    std::string_view modele;                      // vide = omis
    std::string_view langue;                      // indice de langue (« fr »), vide = omis
    std::string_view nom_fichier = "audio.wav";   // [A-Za-z0-9._-]+ ; l'extension aide certains serveurs à choisir le format
    std::string_view type_mime = "audio/wav";     // [a-z0-9/+.-]+
};

namespace detail {

constexpr bool valeur_de_champ_sure(std::string_view v, std::string_view frontiere) noexcept {
    for (char c : v) {
        if (caractere_de_controle(c) || c == '"') return false;
    }
    return v.find(frontiere) == std::string_view::npos;  // la valeur ne doit pas pouvoir imiter la frontière
}

constexpr bool nom_de_fichier_sur(std::string_view v) noexcept {
    if (v.empty() || v.size() > 80) return false;
    for (char c : v) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || est_chiffre(c) || c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

constexpr bool type_mime_sur(std::string_view v) noexcept {
    if (v.empty() || v.size() > 60) return false;
    for (char c : v) {
        const bool ok = (c >= 'a' && c <= 'z') || est_chiffre(c) || c == '/' || c == '+' || c == '.' || c == '-';
        if (!ok) return false;
    }
    return true;
}

constexpr void champ_multipart(Tampon& t, std::string_view frontiere, std::string_view nom, std::string_view valeur) noexcept {
    t.ecrire("--");
    t.ecrire(frontiere);
    t.ecrire("\r\nContent-Disposition: form-data; name=\"");
    t.ecrire(nom);
    t.ecrire("\"\r\n\r\n");
    t.ecrire(valeur);
    t.ecrire("\r\n");
}

}  // namespace detail

// Tout ce qui précède les octets audio : champs `model`, `language`, `response_format=json`, puis l'en-tête de la
// partie `file` (qui se termine par la ligne vide). L'appelant envoie ensuite l'audio, puis `fin_multipart`.
// Faux si la frontière, une valeur, le nom de fichier ou le type sont inadmissibles, ou si le tampon déborde.
constexpr bool debut_multipart(Tampon& t, std::string_view frontiere, const OptionsStt& o) noexcept {
    if (!frontiere_valide(frontiere)) return false;
    if (!detail::valeur_de_champ_sure(o.modele, frontiere) || !detail::valeur_de_champ_sure(o.langue, frontiere)) return false;
    if (!detail::nom_de_fichier_sur(o.nom_fichier) || !detail::type_mime_sur(o.type_mime)) return false;
    if (!o.modele.empty()) detail::champ_multipart(t, frontiere, "model", o.modele);
    if (!o.langue.empty()) detail::champ_multipart(t, frontiere, "language", o.langue);
    detail::champ_multipart(t, frontiere, "response_format", "json");
    t.ecrire("--");
    t.ecrire(frontiere);
    t.ecrire("\r\nContent-Disposition: form-data; name=\"file\"; filename=\"");
    t.ecrire(o.nom_fichier);
    t.ecrire("\"\r\nContent-Type: ");
    t.ecrire(o.type_mime);
    t.ecrire("\r\n\r\n");
    return t.ok();
}

// Ce qui suit les octets audio.
constexpr bool fin_multipart(Tampon& t, std::string_view frontiere) noexcept {
    if (!frontiere_valide(frontiere)) return false;
    t.ecrire("\r\n--");
    t.ecrire(frontiere);
    t.ecrire("--\r\n");
    return t.ok();
}

}  // namespace ecran::assistant
