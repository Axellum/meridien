// Noyau pur : petits outils pour lire une requête HTTP et écrire sa réponse, sans réseau ni ESP-IDF. Le serveur du firmware
// (main/serveur_web.cpp) ne fait que passer les textes reçus d'ESP-IDF à ces fonctions : ce qui décide (qui est le porteur du
// jeton, si la page en cache est encore la bonne, quel texte de statut) se teste sur le PC.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation (les tests de test/host/ s'en servent).
#pragma once

#include <cstddef>
#include <string_view>

namespace ecran::http {

struct Uri {
    std::string_view chemin{};  // « /api/location », sans la requête
    std::string_view requete{}; // ce qui suit « ? » (non décodé) ; vide sans « ? »
};

// Coupe « /api/geocode?q=Lyon » en chemin et requête. Un fragment (« #… ») n'est jamais envoyé par un navigateur ; s'il est là,
// il est ignoré.
constexpr Uri separer_uri(std::string_view uri) noexcept {
    const std::size_t diese = uri.find('#');
    if (diese != std::string_view::npos)
        uri = uri.substr(0, diese);
    const std::size_t point = uri.find('?');
    if (point == std::string_view::npos)
        return {uri, {}};
    return {uri.substr(0, point), uri.substr(point + 1)};
}

inline constexpr std::size_t JETON_MAX = 128; // caractères ; un jeton de l'appareil en fait 64

namespace detail {
constexpr char minuscule(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
} // namespace detail

// En-tête « Authorization: Bearer <jeton> » -> le jeton. Le mot « Bearer » ne tient pas compte de la casse (RFC 7235) ; une ou
// plusieurs espaces le séparent du jeton, qui fait de 1 à JETON_MAX caractères visibles, sans espace. Tout autre cas (autre
// schéma, jeton vide, espace dans le jeton, octet de contrôle) -> texte vide, c'est-à-dire « pas de jeton ».
constexpr std::string_view jeton_bearer(std::string_view entete) noexcept {
    constexpr std::string_view schema = "bearer";
    if (entete.size() <= schema.size())
        return {};
    for (std::size_t i = 0; i < schema.size(); ++i) {
        if (detail::minuscule(entete[i]) != schema[i])
            return {};
    }
    std::size_t p = schema.size();
    if (entete[p] != ' ')
        return {};
    while (p < entete.size() && entete[p] == ' ')
        ++p;
    const std::string_view jeton = entete.substr(p);
    if (jeton.empty() || jeton.size() > JETON_MAX)
        return {};
    for (char ch : jeton) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c <= 0x20 || c >= 0x7F)
            return {};
    }
    return jeton;
}

// Ligne d'état HTTP pour un code de réponse de l'API (« 429 Too Many Requests »). Un code inconnu devient une erreur 500 : on
// n'envoie jamais un code que ce firmware n'a pas prévu.
constexpr const char *ligne_statut(int code) noexcept {
    switch (code) {
    case 200:
        return "200 OK";
    case 204:
        return "204 No Content";
    case 304:
        return "304 Not Modified";
    case 400:
        return "400 Bad Request";
    case 401:
        return "401 Unauthorized";
    case 403:
        return "403 Forbidden";
    case 404:
        return "404 Not Found";
    case 405:
        return "405 Method Not Allowed";
    case 413:
        return "413 Payload Too Large";
    case 422:
        return "422 Unprocessable Entity";
    case 429:
        return "429 Too Many Requests";
    case 500:
        return "500 Internal Server Error";
    case 501:
        return "501 Not Implemented";
    case 503:
        return "503 Service Unavailable";
    default:
        return "500 Internal Server Error";
    }
}

// L'en-tête « If-None-Match » du navigateur désigne-t-il l'étiquette `etag` (guillemets compris, « "a1b2" ») ? Accepte « * », une
// liste séparée par des virgules et le préfixe « W/ » de comparaison faible.
constexpr bool etag_correspond(std::string_view si_none_match, std::string_view etag) noexcept {
    if (etag.empty())
        return false;
    std::size_t debut = 0;
    while (debut <= si_none_match.size()) {
        std::size_t fin = si_none_match.find(',', debut);
        if (fin == std::string_view::npos)
            fin = si_none_match.size();
        std::string_view element = si_none_match.substr(debut, fin - debut);
        while (!element.empty() && element.front() == ' ')
            element.remove_prefix(1);
        while (!element.empty() && element.back() == ' ')
            element.remove_suffix(1);
        if (element == "*")
            return true;
        if (element.size() > 2 && element[0] == 'W' && element[1] == '/')
            element.remove_prefix(2);
        if (!element.empty() && element == etag)
            return true;
        debut = fin + 1;
    }
    return false;
}

// En-têtes de sécurité de la page (docs/api-reglages.md §12). La politique de contenu suit ce que la page fait vraiment :
//   - scripts et feuilles de style : seulement les fichiers servis par l'appareil (app.js, style.css), aucun code ni style écrit
//     dans le HTML (pas d'`unsafe-inline` : index.html n'a ni <script> en ligne, ni attribut `style`, ni `onclick`) ;
//   - requêtes : l'appareil, plus `https:` parce que la page lit elle-même la liste des spots de la mer sur l'adresse Vigie que
//     l'utilisateur a réglée (https obligatoire, contrôlé par l'appareil) ;
//   - images : l'appareil (icône d'onglet) ; les icônes de la page sont des <symbol> du HTML, non concernés ;
//   - ni cadre (`frame-ancestors 'none'` : la page ne s'affiche dans aucune autre), ni <base>, ni envoi de formulaire, ni objet.
// Le faux serveur de web/ lit cette chaîne ici et l'envoie à l'identique : le parcours Playwright joue la page sous cette règle.
inline constexpr std::string_view POLITIQUE_CONTENU =
    "default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self' https:; "
    "base-uri 'none'; form-action 'none'; frame-ancestors 'none'; object-src 'none'";
inline constexpr std::string_view POLITIQUE_REFERENT = "no-referrer";

namespace detail {
// « a » et « b » sont-ils le même texte, sans tenir compte de la casse des lettres ASCII ?
constexpr bool egal_sans_casse(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (minuscule(a[i]) != minuscule(b[i]))
            return false;
    }
    return true;
}
} // namespace detail

inline constexpr std::size_t HOTE_MAX = 64; // un nom « ecran-xxxx.local » ou une adresse IPv4 en font moins de 25

// L'en-tête « Host » d'une requête désigne-t-il bien cet appareil ? Protection contre le rebinding DNS : une page d'Internet dont
// le nom pointe soudain vers l'adresse locale de l'appareil enverrait ses requêtes avec SON nom dans « Host » ; elles sont refusées.
// Acceptés, sans tenir compte de la casse, avec ou sans « :80 » (le port du serveur) :
//   - `ip` : l'adresse IPv4 de l'appareil sur le Wi-Fi de la maison (« 192.168.1.52 ») ;
//   - `nom` + « .local » (un point final toléré) : son nom mDNS (`nom` = « ecran-1a2b ») ;
//   - `ip_installation` : l'adresse de l'appareil sur son propre point d'accès d'installation, quand il en ouvre un.
// Un paramètre vide n'accepte rien. Tout le reste est refusé : en-tête absent ou vide, autre nom, autre port, adresse IPv6, espace.
constexpr bool hote_autorise(std::string_view hote, std::string_view ip, std::string_view nom,
                             std::string_view ip_installation = {}) noexcept {
    if (hote.size() > HOTE_MAX)
        return false;
    const std::size_t deux_points = hote.find(':');
    if (deux_points != std::string_view::npos) {
        if (hote.substr(deux_points) != ":80")
            return false;
        hote = hote.substr(0, deux_points);
    }
    if (hote.empty()) // en-tête absent, vide ou réduit à « :80 »
        return false;
    if (!ip.empty() && hote == ip)
        return true;
    if (!ip_installation.empty() && hote == ip_installation)
        return true;
    if (nom.empty())
        return false;
    if (hote.back() == '.')
        hote.remove_suffix(1);
    constexpr std::string_view suffixe = ".local";
    if (hote.size() != nom.size() + suffixe.size()) // aussi la garde de substr() ci-dessous
        return false;
    return detail::egal_sans_casse(hote.substr(0, nom.size()), nom) &&
           detail::egal_sans_casse(hote.substr(nom.size()), suffixe);
}

} // namespace ecran::http
