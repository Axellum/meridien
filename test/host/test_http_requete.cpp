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

} // namespace

#define LISTE(X)                                                                                                       \
    X(uri_coupee_en_chemin_et_requete)                                                                                 \
    X(jeton_bearer_bien_forme)                                                                                         \
    X(jeton_bearer_refuse_le_reste)                                                                                    \
    X(jeton_bearer_longueur_limitee)                                                                                   \
    X(lignes_d_etat)                                                                                                   \
    X(etiquette_de_cache)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
