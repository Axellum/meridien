// Tests du noyau pur : les lieux que la recherche de ville de l'écran propose (ecran/lieux_geocodage.hpp). Chaque test est évalué
// à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
//
// Ce que ces tests prouvent : la réponse du géocodage d'Open-Meteo donne la même liste que celle de la page web (mêmes lieux écartés) ;
// le libellé « Ville, Région, Pays » saute les parties vides et ne coupe jamais un caractère ; au plus huit lieux ; une réponse
// illisible ne donne aucun lieu et ne plante pas. Ce qu'ils NE prouvent PAS : que le vrai serveur répond encore ainsi (la réponse
// d'exemple est celle du test de la page web, écrite d'après la documentation d'Open-Meteo ; à constater sur l'appareil).
#include <string_view>

#include "ecran/lieux_geocodage.hpp"
#include "mini.hpp"

using sv = std::string_view;
namespace lieux = ecran::lieux;
namespace reglages = ecran::reglages;

namespace {

constexpr sv GEOCODAGE_LYON = R"({"results":[
  {"id":2996944,"name":"Lyon","latitude":45.74846,"longitude":4.84671,"elevation":173.0,"feature_code":"PPLA",
   "country_code":"FR","timezone":"Europe/Paris","population":522250,"country":"France","admin1":"Auvergne-Rhône-Alpes"},
  {"id":4262045,"name":"Lyon","latitude":37.69,"longitude":-85.1,"elevation":150.0,"country_code":"US",
   "timezone":"America/Chicago","country":"United States","admin1":"Kentucky"},
  {"id":1,"name":"Sans fuseau","latitude":1.0,"longitude":2.0,"country_code":"XX"},
  {"id":2,"name":"Hors plage","latitude":95.0,"longitude":2.0,"timezone":"Europe/Paris"},
  {"id":3,"name":"Hors plage est","latitude":10.0,"longitude":181.0,"timezone":"Europe/Paris"},
  {"id":4,"latitude":10.0,"longitude":20.0,"timezone":"Europe/Paris"}
 ],"generationtime_ms":0.6})";

constexpr bool egal(const char *a, sv b) { return sv(a) == b; }

constexpr int liste_de_la_page_web_memes_lieux_ecartes() {
    lieux::Candidat c[lieux::CANDIDATS_MAX] = {};
    const std::size_t n = lieux::lire(GEOCODAGE_LYON, c, lieux::CANDIDATS_MAX);
    VERIFIER(n == 2); // sans fuseau, latitude 95, longitude 181, sans nom : écartés
    VERIFIER(egal(c[0].zone.nom, "Lyon") && egal(c[0].zone.region, "Auvergne-Rhône-Alpes") &&
             egal(c[0].zone.pays, "France"));
    VERIFIER(egal(c[0].zone.code_pays, "FR") && egal(c[0].zone.fuseau, "Europe/Paris"));
    VERIFIER(c[0].zone.latitude > 45.748 && c[0].zone.latitude < 45.749 && c[0].zone.longitude > 4.846 &&
             c[0].zone.longitude < 4.847);
    VERIFIER(egal(c[0].libelle, "Lyon, Auvergne-Rhône-Alpes, France"));
    VERIFIER(egal(c[1].libelle, "Lyon, Kentucky, United States") && egal(c[1].zone.fuseau, "America/Chicago"));
    VERIFIER(c[1].zone.longitude < -85.0 && c[1].zone.longitude > -85.2); // l'hémisphère ouest garde son signe
    return 0;
}

constexpr int au_plus_le_nombre_demande_et_huit_au_total() {
    lieux::Candidat c[lieux::CANDIDATS_MAX] = {};
    VERIFIER(lieux::lire(GEOCODAGE_LYON, c, 1) == 1);
    VERIFIER(lieux::lire(GEOCODAGE_LYON, c, 0) == 0);
    VERIFIER(lieux::lire(GEOCODAGE_LYON, nullptr, 0) == 0);
    // Dix lieux valables : huit gardés (la taille du tableau de l'écran), pas un de plus.
    constexpr sv dix = R"({"results":[
      {"name":"A1","latitude":1,"longitude":1,"timezone":"Europe/Paris"},{"name":"A2","latitude":1,"longitude":1,"timezone":"Europe/Paris"},
      {"name":"A3","latitude":1,"longitude":1,"timezone":"Europe/Paris"},{"name":"A4","latitude":1,"longitude":1,"timezone":"Europe/Paris"},
      {"name":"A5","latitude":1,"longitude":1,"timezone":"Europe/Paris"},{"name":"A6","latitude":1,"longitude":1,"timezone":"Europe/Paris"},
      {"name":"A7","latitude":1,"longitude":1,"timezone":"Europe/Paris"},{"name":"A8","latitude":1,"longitude":1,"timezone":"Europe/Paris"},
      {"name":"A9","latitude":1,"longitude":1,"timezone":"Europe/Paris"},{"name":"A10","latitude":1,"longitude":1,"timezone":"Europe/Paris"}]})";
    VERIFIER(lieux::lire(dix, c, 100) == lieux::CANDIDATS_MAX);
    VERIFIER(egal(c[7].zone.nom, "A8"));
    return 0;
}

constexpr int reponse_illisible_ou_vide_ne_donne_aucun_lieu() {
    lieux::Candidat c[lieux::CANDIDATS_MAX] = {};
    VERIFIER(lieux::lire("", c, 8) == 0);
    VERIFIER(lieux::lire("<html>502 Bad Gateway</html>", c, 8) == 0);
    VERIFIER(lieux::lire("[1,2,3]", c, 8) == 0);
    VERIFIER(lieux::lire(R"({"results":[]})", c, 8) == 0);
    VERIFIER(lieux::lire("{}", c, 8) == 0); // « aucun lieu de ce nom » : Open-Meteo n'écrit alors pas « results »
    VERIFIER(lieux::lire(R"({"results":"non"})", c, 8) == 0);
    VERIFIER(lieux::lire(R"({"results":[1,"a",null]})", c, 8) == 0);
    VERIFIER(
        lieux::lire(R"({"results":[{"name":"X","latitude":"1","longitude":2,"timezone":"Europe/Paris"}]})", c, 8) == 0);
    return 0;
}

constexpr int libelle_saute_les_parties_vides() {
    reglages::Zone z;
    char sortie[lieux::LIBELLE_OCTETS] = {};
    reglages::copier_texte("Lyon", z.nom, sizeof z.nom);
    VERIFIER(lieux::libelle(z, sortie, sizeof sortie) == 4 && egal(sortie, "Lyon"));
    reglages::copier_texte("France", z.pays, sizeof z.pays);
    VERIFIER(lieux::libelle(z, sortie, sizeof sortie) == 12 &&
             egal(sortie, "Lyon, France")); // sans région : une seule virgule
    reglages::copier_texte("Rhône", z.region, sizeof z.region);
    VERIFIER(egal(sortie, "Lyon, France") && lieux::libelle(z, sortie, sizeof sortie) > 0 &&
             egal(sortie, "Lyon, Rhône, France"));
    z = reglages::Zone{};
    VERIFIER(lieux::libelle(z, sortie, sizeof sortie) == 0 && sortie[0] == '\0');
    VERIFIER(lieux::libelle(z, sortie, 0) == 0);
    return 0;
}

constexpr int libelle_trop_long_coupe_sur_un_caractere_entier() {
    reglages::Zone z;
    reglages::copier_texte("Château-d’Œx", z.nom, sizeof z.nom); // « ’ » et « Œ » : plusieurs octets chacun
    char sortie[10] = {};
    const std::size_t n = lieux::libelle(z, sortie, sizeof sortie);
    VERIFIER(n <= 9 && sortie[n] == '\0');
    VERIFIER(reglages::utf8_valide(sv(sortie, n))); // aucun caractère coupé en deux
    char large[lieux::LIBELLE_OCTETS] = {};
    VERIFIER(lieux::libelle(z, large, sizeof large) == sv("Château-d’Œx").size());
    return 0;
}

constexpr int region_ou_pays_invalides_sont_ignores_sans_perdre_le_lieu() {
    // Un octet 0xFF n'est pas de l'UTF-8 : la région est perdue, le lieu reste.
    constexpr char corps[] =
        "{\"results\":[{\"name\":\"Lyon\",\"latitude\":45.7,\"longitude\":4.8,\"timezone\":\"Europe/Paris\","
        "\"admin1\":\"Rh\xFF"
        "ne\",\"country\":\"France\"}]}";
    lieux::Candidat c[lieux::CANDIDATS_MAX] = {};
    VERIFIER(lieux::lire(sv(corps, sizeof corps - 1), c, 8) == 1);
    VERIFIER(egal(c[0].zone.region, "") && egal(c[0].zone.pays, "France") && egal(c[0].libelle, "Lyon, France"));
    // Un nom qui n'est pas de l'UTF-8 écarte le lieu : on ne le montre pas à moitié.
    constexpr char mauvais[] = "{\"results\":[{\"name\":\"Ly\xFF"
                               "on\",\"latitude\":45.7,\"longitude\":4.8,\"timezone\":\"Europe/Paris\"}]}";
    VERIFIER(lieux::lire(sv(mauvais, sizeof mauvais - 1), c, 8) == 0);
    return 0;
}

constexpr int un_candidat_precedent_est_remis_a_zero() {
    // Les tableaux de l'écran sont réutilisés d'une recherche à l'autre : rien de l'ancien lieu ne doit rester dans le nouveau.
    lieux::Candidat c[lieux::CANDIDATS_MAX] = {};
    VERIFIER(lieux::lire(GEOCODAGE_LYON, c, 8) == 2);
    VERIFIER(egal(c[0].zone.region, "Auvergne-Rhône-Alpes"));
    constexpr sv sans_region =
        R"({"results":[{"name":"Paris","latitude":48.85,"longitude":2.35,"timezone":"Europe/Paris"}]})";
    VERIFIER(lieux::lire(sans_region, c, 8) == 1);
    VERIFIER(egal(c[0].zone.nom, "Paris") && egal(c[0].zone.region, "") && egal(c[0].zone.pays, "") &&
             egal(c[0].zone.code_pays, "") && egal(c[0].libelle, "Paris"));
    // Même avec une région illisible (la branche qui ne copie rien), l'ancienne région ne reste pas.
    VERIFIER(lieux::lire(GEOCODAGE_LYON, c, 8) == 2 && egal(c[0].zone.region, "Auvergne-Rhône-Alpes"));
    constexpr char region_illisible[] = "{\"results\":[{\"name\":\"Paris\",\"latitude\":48.85,\"longitude\":2.35,"
                                        "\"timezone\":\"Europe/Paris\",\"admin1\":\"\xFF\"}]}";
    VERIFIER(lieux::lire(sv(region_illisible, sizeof region_illisible - 1), c, 8) == 1);
    VERIFIER(egal(c[0].zone.nom, "Paris") && egal(c[0].zone.region, ""));
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(liste_de_la_page_web_memes_lieux_ecartes)                                                                        \
    X(au_plus_le_nombre_demande_et_huit_au_total)                                                                      \
    X(reponse_illisible_ou_vide_ne_donne_aucun_lieu)                                                                   \
    X(libelle_saute_les_parties_vides)                                                                                 \
    X(libelle_trop_long_coupe_sur_un_caractere_entier)                                                                 \
    X(region_ou_pays_invalides_sont_ignores_sans_perdre_le_lieu)                                                       \
    X(un_candidat_precedent_est_remis_a_zero)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
