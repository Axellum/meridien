// Tests du noyau pur « normales climatiques, côté requête » : adresse (climat_requete.hpp) et cadence (climat_planning.hpp), ADR-0029.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/climat_planning.hpp"
#include "ecran/climat_requete.hpp"
#include "mini.hpp"

using namespace ecran::climat;
using sv = std::string_view;

namespace {

constexpr bool egal(const char *a, sv b) {
    std::size_t i = 0;
    for (; i < b.size(); ++i)
        if (a[i] != b[i])
            return false;
    return a[i] == '\0';
}

constexpr int requete_lorient() {
    char s[TAILLE_REQUETE_MAX] = {};
    const std::size_t n = construire_requete("https://vigie.auxfilsdesidees.fr", 4774800, -336600, s, sizeof s);
    VERIFIER(n > 0);
    VERIFIER(egal(s, "https://vigie.auxfilsdesidees.fr/v1/climate/normals?lat=47.748&lon=-3.366"));
    VERIFIER(n == sv("https://vigie.auxfilsdesidees.fr/v1/climate/normals?lat=47.748&lon=-3.366").size());
    return 0;
}

constexpr int requete_refusee() {
    char s[TAILLE_REQUETE_MAX] = {};
    VERIFIER(construire_requete("", 100, 100, s, sizeof s) == 0);                 // pas d'adresse
    VERIFIER(construire_requete("https://v.fr", 9000001, 0, s, sizeof s) == 0);   // latitude > 90
    VERIFIER(construire_requete("https://v.fr", 0, -18000001, s, sizeof s) == 0); // longitude < -180
    VERIFIER(construire_requete("https://v.fr", 0, 0, s, 10) == 0);               // tampon trop petit
    VERIFIER(construire_requete("https://v.fr", 0, 0, nullptr, 100) == 0);
    VERIFIER(construire_requete("https://v.fr", 0, 0, s, 0) == 0);
    VERIFIER(construire_requete("https://v.fr", 9000000, 18000000, s, sizeof s) > 0); // bornes comprises
    return 0;
}

constexpr int la_requete_ne_porte_aucune_valeur_du_jour() {
    // Ni `tmax`, ni `tmin`, ni `rain_mm` : la valeur du jour ne quitte pas l'appareil (ADR-0029).
    char s[TAILLE_REQUETE_MAX] = {};
    VERIFIER(construire_requete("https://v.fr", 1, 2, s, sizeof s) > 0);
    const sv u = s;
    VERIFIER(u.find("tmax") == sv::npos && u.find("tmin") == sv::npos && u.find("rain") == sv::npos);
    return 0;
}

constexpr int premier_essai() {
    VERIFIER(delai_premier_essai_s(0) == 180);
    VERIFIER(delai_premier_essai_s(255) == 300);
    VERIFIER(delai_premier_essai_s(128) > 180 && delai_premier_essai_s(128) < 300);
    return 0;
}

constexpr int une_requete_par_jour() {
    VERIFIER(delai_avant_essai_s(0, false, 0) == 24 * 3600);
    VERIFIER(delai_avant_essai_s(0, false, 255) == 24 * 3600 + 30 * 60);
    return 0;
}

constexpr int reessais_doubles_puis_plafonnes() {
    VERIFIER(delai_avant_essai_s(1, false, 0) == 300);
    VERIFIER(delai_avant_essai_s(2, false, 0) == 600);
    VERIFIER(delai_avant_essai_s(3, false, 0) == 1200);
    VERIFIER(delai_avant_essai_s(4, false, 0) == 2400);
    VERIFIER(delai_avant_essai_s(5, false, 0) == 3600);
    VERIFIER(delai_avant_essai_s(1000000, false, 0) == 3600);
    VERIFIER(delai_avant_essai_s(2, false, 255) == 600 + 60); // décalage : 10 % après un échec
    return 0;
}

constexpr int trop_de_requetes_attend_une_heure_au_moins() {
    VERIFIER(delai_avant_essai_s(1, true, 0) == 3600);
    VERIFIER(delai_avant_essai_s(3, true, 0) == 3600);
    VERIFIER(delai_avant_essai_s(5, true, 0) == 3600);
    VERIFIER(delai_avant_essai_s(1, true, 255) ==
             3600 + 3600 / 10); // le décalage suit la base relevée : 10 % de 3600 s
    return 0;
}

constexpr int changement_de_mois() {
    // Dernier essai à 1000 s, prochain prévu dans un jour : on réessaie dans une heure au plus.
    VERIFIER(prochain_apres_changement_de_mois_s(2000, 1000, 1000 + 86400) == 1000 + 3600);
    // Le dernier essai date d'il y a plus d'une heure : tout de suite.
    VERIFIER(prochain_apres_changement_de_mois_s(10000, 1000, 1000 + 86400) == 10000);
    // Si le prochain essai prévu est déjà plus proche, on le garde.
    VERIFIER(prochain_apres_changement_de_mois_s(2000, 1000, 1500) == 1500);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(requete_lorient)                                                                                                 \
    X(requete_refusee)                                                                                                 \
    X(la_requete_ne_porte_aucune_valeur_du_jour)                                                                       \
    X(premier_essai)                                                                                                   \
    X(une_requete_par_jour)                                                                                            \
    X(reessais_doubles_puis_plafonnes)                                                                                 \
    X(trop_de_requetes_attend_une_heure_au_moins)                                                                      \
    X(changement_de_mois)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
