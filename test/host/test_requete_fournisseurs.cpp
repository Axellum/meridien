// Tests du noyau pur : les adresses des requêtes MET Norway et NWS (meteo_requete_fournisseurs.hpp).
//
// Quatre décimales au plus, par TRONCATURE vers zéro (jamais d'arrondi) : MET Norway l'exige pour son cache, le NWS redirige sinon.
// Le bureau du NWS entre dans l'adresse : trois lettres capitales, rien d'autre.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_requete_fournisseurs.hpp"
#include "mini.hpp"

using namespace ecran::meteo::fournisseurs;
using sv = std::string_view;

namespace {

constexpr sv METNO = "https://api.met.no/weatherapi/locationforecast/2.0/complete?lat=";

// Rend vrai si l'adresse construite est exactement `attendu` (longueur rendue, texte, zéro final).
template <std::size_t N> constexpr bool metno_donne(std::int32_t lat, std::int32_t lon, sv attendu, char (&tampon)[N]) {
    for (char &c : tampon)
        c = 'x';
    const std::size_t n = construire_requete_metno(lat, lon, tampon, N);
    return n == attendu.size() && sv(tampon, n) == attendu && tampon[n] == '\0';
}

constexpr bool metno(std::int32_t lat, std::int32_t lon, sv suite) {
    char tampon[TAILLE_REQUETE_METNO_MAX] = {};
    char attendu[200] = {};
    std::size_t n = 0;
    for (const char c : METNO)
        attendu[n++] = c;
    for (const char c : suite)
        attendu[n++] = c;
    return metno_donne(lat, lon, sv(attendu, n), tampon);
}

constexpr int metno_quatre_decimales() {
    VERIFIER(metno(5991273, 1074609, "59.9127&lon=10.7460"));     // Oslo : 59,91273 ; 10,74609 -> pas 10,7461
    VERIFIER(metno(-3386780, 15120730, "-33.8678&lon=151.2073")); // Sydney
    VERIFIER(metno(0, 0, "0.0000&lon=0.0000"));
    VERIFIER(metno(9000000, 18000000, "90.0000&lon=180.0000"));
    VERIFIER(metno(-9000000, -18000000, "-90.0000&lon=-180.0000"));
    VERIFIER(metno(4574906, 484780, "45.7490&lon=4.8478")); // Lyon
    return 0;
}

constexpr int la_cinquieme_decimale_est_supprimee_vers_zero() {
    VERIFIER(metno(-3386789, 15120739, "-33.8678&lon=151.2073")); // négatif : -33,86789 -> -33,8678 (pas -33,8679)
    VERIFIER(metno(5999999, 9999, "59.9999&lon=0.0999"));
    VERIFIER(metno(-5999999, -9999, "-59.9999&lon=-0.0999"));
    VERIFIER(metno(10, 10, "0.0001&lon=0.0001"));
    return 0;
}

constexpr int jamais_de_signe_devant_un_zero() {
    VERIFIER(metno(-1, -9, "0.0000&lon=0.0000")); // -0,00001 et -0,00009 se tronquent à zéro
    VERIFIER(metno(-10, -10, "-0.0001&lon=-0.0001"));
    return 0;
}

constexpr int hors_du_globe_rien_n_est_construit() {
    char t[TAILLE_REQUETE_METNO_MAX] = {};
    VERIFIER(construire_requete_metno(9000001, 0, t, sizeof t) == 0);
    VERIFIER(construire_requete_metno(-9000001, 0, t, sizeof t) == 0);
    VERIFIER(construire_requete_metno(0, 18000001, t, sizeof t) == 0);
    VERIFIER(construire_requete_metno(0, -18000001, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_points(9000001, 0, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_points(0, -18000001, t, sizeof t) == 0);
    return 0;
}

constexpr int le_tampon_est_exactement_assez_grand_ou_rien_n_est_ecrit() {
    // le pire cas (-90 ; -180) tient exactement dans TAILLE_REQUETE_METNO_MAX, zéro final compris
    char juste[TAILLE_REQUETE_METNO_MAX] = {};
    const std::size_t n = construire_requete_metno(-9000000, -18000000, juste, sizeof juste);
    VERIFIER(n > 0 && n + 1 == TAILLE_REQUETE_METNO_MAX && juste[n] == '\0');
    char court[TAILLE_REQUETE_METNO_MAX - 1] = {};
    VERIFIER(construire_requete_metno(-9000000, -18000000, court, sizeof court) == 0);
    VERIFIER(construire_requete_metno(0, 0, court, 0) == 0);
    VERIFIER(construire_requete_metno(0, 0, nullptr, 100) == 0);
    char points[TAILLE_REQUETE_NWS_POINTS_MAX] = {};
    const std::size_t np = construire_requete_nws_points(-9000000, -18000000, points, sizeof points);
    VERIFIER(np > 0 && np + 1 == TAILLE_REQUETE_NWS_POINTS_MAX);
    char points_court[TAILLE_REQUETE_NWS_POINTS_MAX - 1] = {};
    VERIFIER(construire_requete_nws_points(-9000000, -18000000, points_court, sizeof points_court) == 0);
    char grille[TAILLE_REQUETE_NWS_GRILLE_MAX] = {};
    const std::size_t ng = construire_requete_nws_grille("HFO", 9999, 9999, grille, sizeof grille);
    VERIFIER(ng > 0 && ng + 1 == TAILLE_REQUETE_NWS_GRILLE_MAX);
    char grille_court[TAILLE_REQUETE_NWS_GRILLE_MAX - 1] = {};
    VERIFIER(construire_requete_nws_grille("HFO", 9999, 9999, grille_court, sizeof grille_court) == 0);
    static_assert(TAILLE_REQUETE_FOURNISSEURS_MAX >= TAILLE_REQUETE_METNO_MAX);
    static_assert(TAILLE_REQUETE_FOURNISSEURS_MAX >= TAILLE_REQUETE_NWS_POINTS_MAX);
    static_assert(TAILLE_REQUETE_FOURNISSEURS_MAX >= TAILLE_REQUETE_NWS_GRILLE_MAX);
    return 0;
}

constexpr bool points_donne(std::int32_t lat, std::int32_t lon, sv attendu) {
    char t[TAILLE_REQUETE_NWS_POINTS_MAX] = {};
    const std::size_t n = construire_requete_nws_points(lat, lon, t, sizeof t);
    return n == attendu.size() && sv(t, n) == attendu && t[n] == '\0';
}

constexpr int nws_points() {
    VERIFIER(points_donne(4760620, -12233210, "https://api.weather.gov/points/47.6062,-122.3321")); // Seattle
    VERIFIER(points_donne(2130690, -15785830, "https://api.weather.gov/points/21.3069,-157.8583")); // Honolulu
    VERIFIER(
        points_donne(4071289, -7400609, "https://api.weather.gov/points/40.7128,-74.0060")); // troncature vers zéro
    VERIFIER(points_donne(0, 0, "https://api.weather.gov/points/0.0000,0.0000"));
    return 0;
}

constexpr bool grille_donne(sv bureau, int x, int y, sv attendu) {
    char t[TAILLE_REQUETE_NWS_GRILLE_MAX] = {};
    const std::size_t n = construire_requete_nws_grille(bureau, x, y, t, sizeof t);
    return n == attendu.size() && sv(t, n) == attendu && t[n] == '\0';
}

constexpr int nws_grille() {
    VERIFIER(grille_donne("SEW", 125, 68, "https://api.weather.gov/gridpoints/SEW/125,68"));
    VERIFIER(grille_donne("OKX", 33, 42, "https://api.weather.gov/gridpoints/OKX/33,42"));
    VERIFIER(grille_donne("AER", 0, 0, "https://api.weather.gov/gridpoints/AER/0,0"));
    VERIFIER(grille_donne("HFO", 9999, 9999, "https://api.weather.gov/gridpoints/HFO/9999,9999"));
    return 0;
}

constexpr int la_grille_refuse_un_bureau_ou_une_maille_qui_ne_va_pas_dans_une_adresse() {
    char t[TAILLE_REQUETE_NWS_GRILLE_MAX] = {};
    for (const sv b : {sv(""), sv("sew"), sv("SE"), sv("SEWA"), sv("S3W"), sv("SE/"), sv("S W"), sv("../"), sv("SEW\n"),
                       sv("É"), sv("SEw")})
        VERIFIER(construire_requete_nws_grille(b, 1, 1, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_grille("SEW", -1, 1, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_grille("SEW", 1, -1, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_grille("SEW", 10000, 1, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_grille("SEW", 1, 10000, t, sizeof t) == 0);
    VERIFIER(construire_requete_nws_grille("SEW", 1, 1, nullptr, 100) == 0);
    VERIFIER(construire_requete_nws_grille("SEW", 1, 1, t, 0) == 0);
    return 0;
}

constexpr int le_bureau_est_trois_lettres_capitales() {
    VERIFIER(bureau_nws_valide("SEW") && bureau_nws_valide("AAA") && bureau_nws_valide("ZZZ"));
    VERIFIER(!bureau_nws_valide("") && !bureau_nws_valide("SE") && !bureau_nws_valide("SEWA") &&
             !bureau_nws_valide("sew"));
    VERIFIER(!bureau_nws_valide("SE1") && !bureau_nws_valide("S-W") && !bureau_nws_valide("@AB") &&
             !bureau_nws_valide("[AB"));
    return 0;
}

constexpr int l_identification_est_celle_de_l_application() {
    // les deux services exigent un User-Agent propre à l'application, avec un contact ; ASCII imprimable, une seule ligne
    VERIFIER(USER_AGENT.size() > 10 && USER_AGENT.size() < 120);
    VERIFIER(USER_AGENT.find("Meridien") != sv::npos);
    VERIFIER(USER_AGENT.find("github.com/Axellum/meridien") != sv::npos);
    VERIFIER(USER_AGENT.find('@') == sv::npos); // pas d'adresse électronique : le contact est le dépôt
    for (const char c : USER_AGENT)
        VERIFIER(c >= 0x20 && c < 0x7F);
    return 0;
}

constexpr int les_bornes_de_reponse_ont_de_la_marge_sur_les_reponses_reelles() {
    // Les plus grandes réponses RÉELLES du 07/11/2026 (octets décompressés, test/host/donnees/*.json)
    constexpr std::size_t PLUS_GRANDE_METNO = 90783;       // Tromsø
    constexpr std::size_t PLUS_GRANDE_NWS_GRILLE = 240402; // San Juan
    constexpr std::size_t PLUS_GRANDE_NWS_POINTS = 3937;   // Honolulu
    // au moins 1,5 fois, au plus 4 fois : assez pour une position plus chargée, pas assez pour qu'un tampon énorme passe inaperçu
    VERIFIER(TAILLE_REPONSE_METNO_MAX * 2 >= PLUS_GRANDE_METNO * 3 &&
             TAILLE_REPONSE_METNO_MAX <= PLUS_GRANDE_METNO * 4);
    VERIFIER(TAILLE_REPONSE_NWS_GRILLE_MAX * 2 >= PLUS_GRANDE_NWS_GRILLE * 3 &&
             TAILLE_REPONSE_NWS_GRILLE_MAX <= PLUS_GRANDE_NWS_GRILLE * 4);
    VERIFIER(TAILLE_REPONSE_NWS_POINTS_MAX >= PLUS_GRANDE_NWS_POINTS * 10);
    // des multiples de 1 024 octets, et sous la borne absolue du client commun (1 Mo)
    for (const std::size_t n :
         {TAILLE_REPONSE_METNO_MAX, TAILLE_REPONSE_NWS_POINTS_MAX, TAILLE_REPONSE_NWS_GRILLE_MAX}) {
        VERIFIER(n % 1024 == 0 && n <= 1024 * 1024);
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(metno_quatre_decimales)                                                                                          \
    X(la_cinquieme_decimale_est_supprimee_vers_zero)                                                                   \
    X(jamais_de_signe_devant_un_zero)                                                                                  \
    X(hors_du_globe_rien_n_est_construit)                                                                              \
    X(le_tampon_est_exactement_assez_grand_ou_rien_n_est_ecrit)                                                        \
    X(nws_points)                                                                                                      \
    X(nws_grille)                                                                                                      \
    X(la_grille_refuse_un_bureau_ou_une_maille_qui_ne_va_pas_dans_une_adresse)                                         \
    X(le_bureau_est_trois_lettres_capitales)                                                                           \
    X(l_identification_est_celle_de_l_application) X(les_bornes_de_reponse_ont_de_la_marge_sur_les_reponses_reelles)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
