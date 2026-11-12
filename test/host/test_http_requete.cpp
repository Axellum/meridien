// Tests du noyau pur : lecture d'une requête HTTP (http_requete.hpp) — adresse, jeton « Bearer », ligne d'état, étiquette de cache.
#include <string_view>

#include "ecran/http_requete.hpp"
#include "mini.hpp"

namespace http = ecran::http;
using sv = std::string_view;

namespace {

constexpr int uri_coupee_en_chemin_et_requete() {
    http::Uri u = http::separer_uri("/api/geocode?q=Lyon&x=1");
    VERIFIER(u.chemin == "/api/geocode" && u.requete == "q=Lyon&x=1");
    u = http::separer_uri("/api/system");
    VERIFIER(u.chemin == "/api/system" && u.requete.empty());
    u = http::separer_uri("/api/geocode?");
    VERIFIER(u.chemin == "/api/geocode" && u.requete.empty());
    u = http::separer_uri("/?a=b?c");
    VERIFIER(u.chemin == "/" && u.requete == "a=b?c"); // seul le premier « ? » coupe
    u = http::separer_uri("/app.js#haut");
    VERIFIER(u.chemin == "/app.js" && u.requete.empty());
    u = http::separer_uri("/a?q=1#x?y");
    VERIFIER(u.chemin == "/a" && u.requete == "q=1");
    u = http::separer_uri("");
    VERIFIER(u.chemin.empty() && u.requete.empty());
    return 0;
}

constexpr int jeton_bearer_bien_forme() {
    VERIFIER(http::jeton_bearer("Bearer abc123") == "abc123");
    VERIFIER(http::jeton_bearer("bearer abc123") == "abc123"); // la casse du mot ne compte pas
    VERIFIER(http::jeton_bearer("BEARER abc123") == "abc123");
    VERIFIER(http::jeton_bearer("Bearer    abc123") == "abc123"); // plusieurs espaces
    VERIFIER(http::jeton_bearer("Bearer 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") ==
             "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    return 0;
}

constexpr int jeton_bearer_refuse_le_reste() {
    VERIFIER(http::jeton_bearer("").empty());
    VERIFIER(http::jeton_bearer("Bearer").empty());
    VERIFIER(http::jeton_bearer("Bearer ").empty());
    VERIFIER(http::jeton_bearer("Bearer    ").empty());
    VERIFIER(http::jeton_bearer("Basic abc123").empty());
    VERIFIER(http::jeton_bearer("abc123").empty());
    VERIFIER(http::jeton_bearer("Bearerabc123").empty());   // pas d'espace
    VERIFIER(http::jeton_bearer("Bearer\tabc123").empty()); // tabulation au lieu d'une espace
    VERIFIER(http::jeton_bearer("Bearer abc 123").empty()); // espace dans le jeton
    VERIFIER(http::jeton_bearer("Bearer abc123 ").empty()); // espace à la fin
    VERIFIER(http::jeton_bearer("Bearer abc\x01"
                                "23")
                 .empty()); // octet de contrôle
    VERIFIER(http::jeton_bearer("Bearer abc\x7f"
                                "23")
                 .empty());
    VERIFIER(http::jeton_bearer("Bearer abc\xC3\xA9").empty()); // pas d'octet au-delà de 127
    VERIFIER(http::jeton_bearer(" Bearer abc123").empty());     // espace devant
    return 0;
}

constexpr int jeton_bearer_longueur_limitee() {
    char entete[140] = {'B', 'e', 'a', 'r', 'e', 'r', ' '};
    for (int i = 0; i < 128; ++i)
        entete[7 + i] = 'a';
    VERIFIER(http::jeton_bearer(sv(entete)).size() == 128);
    entete[7 + 128] = 'a'; // 129 caractères
    VERIFIER(http::jeton_bearer(sv(entete)).empty());
    return 0;
}

constexpr int lignes_d_etat() {
    VERIFIER(sv(http::ligne_statut(200)) == "200 OK");
    VERIFIER(sv(http::ligne_statut(204)) == "204 No Content");
    VERIFIER(sv(http::ligne_statut(304)) == "304 Not Modified");
    VERIFIER(sv(http::ligne_statut(401)) == "401 Unauthorized");
    VERIFIER(sv(http::ligne_statut(413)) == "413 Payload Too Large");
    VERIFIER(sv(http::ligne_statut(422)) == "422 Unprocessable Entity");
    VERIFIER(sv(http::ligne_statut(429)) == "429 Too Many Requests");
    VERIFIER(sv(http::ligne_statut(501)) == "501 Not Implemented");
    VERIFIER(sv(http::ligne_statut(503)) == "503 Service Unavailable");
    VERIFIER(sv(http::ligne_statut(418)) == "500 Internal Server Error"); // jamais un code imprévu
    VERIFIER(sv(http::ligne_statut(0)) == "500 Internal Server Error");
    // chaque ligne commence par son code
    for (int code : {200, 204, 304, 400, 401, 403, 404, 405, 413, 422, 429, 500, 501, 503}) {
        const sv ligne = http::ligne_statut(code);
        VERIFIER(ligne.size() > 4 && ligne[3] == ' ');
        int lu = 0;
        for (int i = 0; i < 3; ++i)
            lu = lu * 10 + (ligne[static_cast<std::size_t>(i)] - '0');
        VERIFIER(lu == code);
    }
    return 0;
}

constexpr int etiquette_de_cache() {
    VERIFIER(http::etag_correspond("\"abc\"", "\"abc\""));
    VERIFIER(!http::etag_correspond("\"abd\"", "\"abc\""));
    VERIFIER(!http::etag_correspond("\"ab\"", "\"abc\""));
    VERIFIER(!http::etag_correspond("", "\"abc\""));
    VERIFIER(!http::etag_correspond("\"abc\"", ""));
    VERIFIER(http::etag_correspond("*", "\"abc\""));
    VERIFIER(http::etag_correspond("W/\"abc\"", "\"abc\""));             // comparaison faible
    VERIFIER(http::etag_correspond("\"x\", \"abc\" ,\"y\"", "\"abc\"")); // liste
    VERIFIER(http::etag_correspond("\"x\",\"y\",\"abc\"", "\"abc\""));
    VERIFIER(!http::etag_correspond("\"x\", \"y\"", "\"abc\""));
    VERIFIER(!http::etag_correspond(",,", "\"abc\""));
    VERIFIER(!http::etag_correspond("W/", "\"abc\""));
    return 0;
}

// L'adresse de l'appareil, son nom mDNS et l'adresse du point d'accès d'installation : acceptés, avec ou sans « :80 ».
constexpr int hote_de_l_appareil_accepte() {
    constexpr sv ip = "192.168.1.52", nom = "ecran-1a2b", ap = "192.168.4.1";
    VERIFIER(http::hote_autorise("192.168.1.52", ip, nom, ap));
    VERIFIER(http::hote_autorise("192.168.1.52:80", ip, nom, ap));
    VERIFIER(http::hote_autorise("ecran-1a2b.local", ip, nom, ap));
    VERIFIER(http::hote_autorise("ecran-1a2b.local:80", ip, nom, ap));
    VERIFIER(http::hote_autorise("ECRAN-1A2B.Local", ip, nom, ap));  // un nom ne tient pas compte de la casse
    VERIFIER(http::hote_autorise("ecran-1a2b.local.", ip, nom, ap)); // point final : nom absolu
    VERIFIER(http::hote_autorise("192.168.4.1", ip, nom, ap));
    VERIFIER(http::hote_autorise("192.168.4.1:80", ip, nom, ap));
    VERIFIER(http::hote_autorise("192.168.4.1", {}, {}, ap)); // installation : ni adresse sur la maison, ni mDNS
    VERIFIER(http::hote_autorise("ecran-1a2b.local", {}, nom));
    return 0;
}

// Rebinding DNS et le reste : un autre nom, un autre port, un en-tête vide ou mal formé sont refusés.
constexpr int hote_etranger_refuse() {
    constexpr sv ip = "192.168.1.52", nom = "ecran-1a2b", ap = "192.168.4.1";
    VERIFIER(!http::hote_autorise("", ip, nom, ap));
    VERIFIER(!http::hote_autorise("attaquant.example", ip, nom, ap));
    VERIFIER(!http::hote_autorise("192.168.1.52.attaquant.example", ip, nom, ap));
    VERIFIER(!http::hote_autorise("ecran-1a2b.local.attaquant.example", ip, nom, ap));
    VERIFIER(!http::hote_autorise("ecran-1a2b", ip, nom, ap));      // sans « .local »
    VERIFIER(!http::hote_autorise("ecran", ip, nom, ap));           // plus court que le nom
    VERIFIER(!http::hote_autorise("ecran-1a2.local", ip, nom, ap)); // un autre appareil
    VERIFIER(!http::hote_autorise("xecran-1a2b.local", ip, nom, ap));
    VERIFIER(!http::hote_autorise("ecran-1a2b.locals", ip, nom, ap));
    VERIFIER(!http::hote_autorise("ecran-1a2b.lan.fr", ip, nom, ap)); // même longueur, autre domaine
    VERIFIER(!http::hote_autorise("ecran-1a2bxlocal", ip, nom, ap));
    VERIFIER(!http::hote_autorise("192.168.1.5", ip, nom, ap)); // préfixe de l'adresse
    VERIFIER(!http::hote_autorise("192.168.1.520", ip, nom, ap));
    VERIFIER(!http::hote_autorise("192.168.1.52:8080", ip, nom, ap)); // autre port
    VERIFIER(!http::hote_autorise("192.168.1.52:", ip, nom, ap));
    VERIFIER(!http::hote_autorise("192.168.1.52:80:80", ip, nom, ap));
    VERIFIER(!http::hote_autorise(":80", ip, nom, ap));
    VERIFIER(!http::hote_autorise(".", ip, nom, ap));
    VERIFIER(!http::hote_autorise(" 192.168.1.52", ip, nom, ap));
    VERIFIER(!http::hote_autorise("[::1]", ip, nom, ap));
    VERIFIER(!http::hote_autorise("localhost", ip, nom, ap));
    VERIFIER(!http::hote_autorise("127.0.0.1", ip, nom, ap));
    VERIFIER(!http::hote_autorise(".local", ip, {}, ap));        // nom vide : aucun nom n'est accepté
    VERIFIER(!http::hote_autorise("192.168.4.1", ip, nom, {}));  // pas de point d'accès : son adresse ne passe pas
    VERIFIER(!http::hote_autorise("192.168.1.52", {}, nom, {})); // pas encore d'adresse : elle ne passe pas
    // un nom trop long n'est jamais comparé (et ne peut pas l'être : au-delà de HOTE_MAX)
    VERIFIER(
        !http::hote_autorise("ecran-1a2b.local                                                        ", ip, nom, ap));
    return 0;
}

// La politique de contenu interdit tout code et tout style écrits dans le HTML, et tout cadre ; elle garde ce dont la page a besoin.
constexpr int politique_de_contenu_stricte() {
    constexpr sv p = http::POLITIQUE_CONTENU;
    VERIFIER(p.find("unsafe-inline") == sv::npos);
    VERIFIER(p.find("unsafe-eval") == sv::npos);
    VERIFIER(p.find("default-src 'none'") != sv::npos);
    VERIFIER(p.find("script-src 'self';") != sv::npos);
    VERIFIER(p.find("style-src 'self';") != sv::npos);
    VERIFIER(p.find("frame-ancestors 'none'") != sv::npos);
    VERIFIER(p.find("connect-src 'self' https:;") !=
             sv::npos); // la liste des spots est lue sur l'adresse Vigie (https)
    VERIFIER(p.find("base-uri 'none'") != sv::npos);
    VERIFIER(p.find("form-action 'none'") != sv::npos);
    VERIFIER(p.find("object-src 'none'") != sv::npos);
    VERIFIER(p.find('"') == sv::npos &&
             p.find('\n') == sv::npos); // un en-tête HTTP, recopié tel quel par le faux serveur
    VERIFIER(http::POLITIQUE_REFERENT == "no-referrer");
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(uri_coupee_en_chemin_et_requete)                                                                                 \
    X(jeton_bearer_bien_forme)                                                                                         \
    X(jeton_bearer_refuse_le_reste)                                                                                    \
    X(jeton_bearer_longueur_limitee)                                                                                   \
    X(lignes_d_etat)                                                                                                   \
    X(etiquette_de_cache)                                                                                              \
    X(hote_de_l_appareil_accepte)                                                                                      \
    X(hote_etranger_refuse)                                                                                            \
    X(politique_de_contenu_stricte)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
