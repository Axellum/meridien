// Tests du noyau pur « surf » : le spot et ses règles de validité (surf_spot.hpp) ; les angles et arrondis de surf_base.hpp.
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "ecran/surf_base.hpp"
#include "ecran/surf_spot.hpp"
#include "mini.hpp"
#include "surf_outils.hpp"

using namespace ecran::surf;
using namespace surf_test;

namespace {

// Copie un texte dans le nom d'un spot (sans contrôle : le test fournit parfois un texte invalide exprès).
constexpr Spot avec_nom(const char *nom) noexcept {
    Spot s = spot_ouest();
    for (auto &c : s.nom)
        c = '\0';
    for (std::size_t i = 0; nom[i] != '\0' && i + 1 < sizeof(s.nom); ++i)
        s.nom[i] = nom[i];
    return s;
}

// Un nom fait de `n` fois le même texte (1 à 4 octets par caractère).
struct Nom {
    char d[SPOT_NOM_OCTETS + 8] = {};
};
constexpr Nom repete(const char *motif, std::size_t octets_par_motif, std::size_t fois) noexcept {
    Nom r;
    std::size_t n = 0;
    for (std::size_t k = 0; k < fois && n + octets_par_motif < sizeof(r.d); ++k) {
        for (std::size_t j = 0; j < octets_par_motif; ++j)
            r.d[n++] = motif[j];
    }
    return r;
}

constexpr int le_spot_de_test_est_valide() {
    VERIFIER(spot_valide(spot_ouest()));
    VERIFIER(verifier_spot(spot_ouest()) == ChampSpot::Aucun);
    VERIFIER(message_court(ChampSpot::Aucun)[0] == '\0');
    return 0;
}

constexpr int les_valeurs_par_defaut_sont_valides_et_documentees() {
    VERIFIER(HAUTEUR_MIN_DEFAUT_CM == 100 && HAUTEUR_MAX_DEFAUT_CM == 250);
    VERIFIER(fourchette_valide(HAUTEUR_MIN_DEFAUT_CM, HAUTEUR_MAX_DEFAUT_CM));
    Spot s{};
    VERIFIER(s.hauteur_min_cm == 100 && s.hauteur_max_cm == 250); // le spot neuf porte les défauts
    VERIFIER(SPOTS_MAX == 12);
    VERIFIER(SPOT_NOM_OCTETS == 129);
    return 0;
}

constexpr int un_spot_neuf_n_est_pas_valide_et_le_premier_champ_fautif_est_dit() {
    Spot s{};
    VERIFIER(verifier_spot(s) == ChampSpot::Nom); // pas de nom
    for (std::size_t i = 0; i < 4; ++i)
        s.nom[i] = "Test"[i];
    VERIFIER(verifier_spot(s) == ChampSpot::PositionNulle); // 0 ; 0 : jamais saisi
    s.latitude_e5 = 4366760;
    s.longitude_e5 = -144500;
    VERIFIER(spot_valide(s));
    return 0;
}

// ---- nom ---------------------------------------------------------------------------------------------------------------

constexpr int noms_ordinaires_et_accentues_acceptes() {
    VERIFIER(spot_valide(avec_nom("Hossegor")));
    VERIFIER(spot_valide(avec_nom("La Gravière")));
    VERIFIER(spot_valide(avec_nom("Plage d'Hossegor – nord")));        // apostrophe, tiret long
    VERIFIER(spot_valide(avec_nom("Muizenberg \xE2\x80\x93 Le Cap"))); // tiret demi-cadratin en UTF-8
    VERIFIER(spot_valide(avec_nom("\xE5\x8D\x83\xE8\x91\x89")));       // « 千葉 », 3 octets par lettre
    VERIFIER(spot_valide(avec_nom("\xF0\x9F\x8C\x8A vagues")));        // un caractère de 4 octets (vague)
    return 0;
}

constexpr int le_nom_est_limite_en_caracteres_pas_en_octets() {
    // 32 lettres de 2 octets = 64 octets : accepté, parce que ce sont 32 caractères.
    const Nom n32 = repete("\xC3\xA9", 2, 32);
    VERIFIER(nombre_de_caracteres(n32.d) == 32);
    VERIFIER(spot_valide(avec_nom(n32.d)));
    // 33 caractères : refusé.
    const Nom n33 = repete("\xC3\xA9", 2, 33);
    VERIFIER(nombre_de_caracteres(n33.d) == 33);
    VERIFIER(verifier_spot(avec_nom(n33.d)) == ChampSpot::Nom);
    // 32 caractères de 4 octets = 128 octets + le zéro = exactement la place du tampon : accepté.
    const Nom n4 = repete("\xF0\x9F\x8C\x8A", 4, 32);
    VERIFIER(spot_valide(avec_nom(n4.d)));
    // 32 ASCII et 33 ASCII.
    VERIFIER(spot_valide(avec_nom("12345678901234567890123456789012")));
    VERIFIER(verifier_spot(avec_nom("123456789012345678901234567890123")) == ChampSpot::Nom);
    return 0;
}

constexpr int noms_invalides_refuses() {
    VERIFIER(verifier_spot(avec_nom("")) == ChampSpot::Nom);
    VERIFIER(verifier_spot(avec_nom("     ")) == ChampSpot::Nom);               // seulement des espaces
    VERIFIER(verifier_spot(avec_nom("Hos\x01segor")) == ChampSpot::Nom);        // caractère de contrôle
    VERIFIER(verifier_spot(avec_nom("Hos\x7F")) == ChampSpot::Nom);             // DEL
    VERIFIER(verifier_spot(avec_nom("Hos\x80segor")) == ChampSpot::Nom);        // octet de continuation isolé
    VERIFIER(verifier_spot(avec_nom("Hos\xC0\xAF")) == ChampSpot::Nom);         // forme trop longue (C0 AF)
    VERIFIER(verifier_spot(avec_nom("Hos\xED\xA0\x80")) == ChampSpot::Nom);     // substitut UTF-16
    VERIFIER(verifier_spot(avec_nom("Hos\xF4\x90\x80\x80")) == ChampSpot::Nom); // au-delà de U+10FFFF
    VERIFIER(verifier_spot(avec_nom("Hos\xE5\x8D")) == ChampSpot::Nom);         // séquence coupée
    // Un nom sans zéro final dans la place donnée : refusé par la fonction nue.
    constexpr char plein[4] = {'a', 'b', 'c', 'd'};
    VERIFIER(!nom_de_spot_valide(plein, sizeof(plein)));
    VERIFIER(!nom_de_spot_valide(nullptr, 10));
    return 0;
}

// ---- position ----------------------------------------------------------------------------------------------------------

constexpr int latitude_et_longitude_dans_leurs_bornes() {
    Spot s = spot_ouest();
    s.latitude_e5 = 9000000;
    VERIFIER(spot_valide(s));
    s.latitude_e5 = -9000000;
    VERIFIER(spot_valide(s));
    s.latitude_e5 = 9000001;
    VERIFIER(verifier_spot(s) == ChampSpot::Latitude);
    s.latitude_e5 = -9000001;
    VERIFIER(verifier_spot(s) == ChampSpot::Latitude);
    s = spot_ouest();
    s.longitude_e5 = 18000000;
    VERIFIER(spot_valide(s));
    s.longitude_e5 = -18000000;
    VERIFIER(spot_valide(s));
    s.longitude_e5 = 18000001;
    VERIFIER(verifier_spot(s) == ChampSpot::Longitude);
    s.longitude_e5 = -18000001;
    VERIFIER(verifier_spot(s) == ChampSpot::Longitude);
    return 0;
}

constexpr int la_position_zero_zero_est_une_position_jamais_saisie_mais_zero_ailleurs_est_permis() {
    Spot s = spot_ouest();
    s.latitude_e5 = 0;
    s.longitude_e5 = 0;
    VERIFIER(verifier_spot(s) == ChampSpot::PositionNulle);
    s.longitude_e5 = 9930000; // l'équateur à Mentawai : latitude ~ -2, mais une latitude 0 est possible
    VERIFIER(spot_valide(s));
    s.longitude_e5 = 0;
    s.latitude_e5 = 5000000; // le méridien de Greenwich
    VERIFIER(spot_valide(s));
    return 0;
}

constexpr int les_champs_sont_verifies_dans_l_ordre_nom_latitude_longitude_orientation_fourchette() {
    Spot s = avec_nom("");
    s.latitude_e5 = 9999999;
    s.longitude_e5 = 99999999;
    s.orientation_deg = 400;
    s.hauteur_min_cm = 1;
    VERIFIER(verifier_spot(s) == ChampSpot::Nom);
    s = spot_ouest();
    s.latitude_e5 = 9999999;
    s.longitude_e5 = 99999999;
    s.orientation_deg = 400;
    s.hauteur_min_cm = 1;
    VERIFIER(verifier_spot(s) == ChampSpot::Latitude);
    s.latitude_e5 = 100;
    VERIFIER(verifier_spot(s) == ChampSpot::Longitude);
    s.longitude_e5 = 100;
    VERIFIER(verifier_spot(s) == ChampSpot::Orientation);
    s.orientation_deg = 10;
    VERIFIER(verifier_spot(s) == ChampSpot::Fourchette);
    s.hauteur_min_cm = 100;
    VERIFIER(spot_valide(s));
    return 0;
}

// ---- orientation et fourchette -----------------------------------------------------------------------------------------

constexpr int orientation_de_0_a_359() {
    VERIFIER(orientation_valide(0));
    VERIFIER(orientation_valide(359));
    VERIFIER(orientation_valide(180));
    VERIFIER(!orientation_valide(360)); // une seule écriture du nord : 0
    VERIFIER(!orientation_valide(-1));
    VERIFIER(!orientation_valide(100000));
    Spot s = spot_ouest();
    s.orientation_deg = 360;
    VERIFIER(verifier_spot(s) == ChampSpot::Orientation);
    s.orientation_deg = -1;
    VERIFIER(verifier_spot(s) == ChampSpot::Orientation);
    s.orientation_deg = 359;
    VERIFIER(spot_valide(s));
    return 0;
}

constexpr int fourchette_de_hauteur_bornes_et_ecart() {
    VERIFIER(fourchette_valide(10, 30)); // écart 20 : le minimum
    VERIFIER(!fourchette_valide(10, 29));
    VERIFIER(!fourchette_valide(9, 100)); // minimum sous 10 cm
    VERIFIER(fourchette_valide(100, 2500));
    VERIFIER(!fourchette_valide(100, 2501));
    VERIFIER(!fourchette_valide(250, 100)); // à l'envers
    VERIFIER(!fourchette_valide(100, 100));
    VERIFIER(!fourchette_valide(-50, 100));
    Spot s = spot_ouest();
    s.hauteur_min_cm = 200;
    s.hauteur_max_cm = 150;
    VERIFIER(verifier_spot(s) == ChampSpot::Fourchette);
    return 0;
}

constexpr int chaque_erreur_a_une_phrase_courte() {
    for (const ChampSpot c : {ChampSpot::Nom, ChampSpot::Latitude, ChampSpot::Longitude, ChampSpot::PositionNulle,
                              ChampSpot::Orientation, ChampSpot::Fourchette}) {
        VERIFIER(message_court(c)[0] != '\0');
    }
    return 0;
}

// ---- surf_base.hpp -----------------------------------------------------------------------------------------------------

constexpr int normaliser_degres_et_ecart_angulaire_aux_bords() {
    VERIFIER(normaliser_degres(0) == 0 && normaliser_degres(359) == 359 && normaliser_degres(360) == 0);
    VERIFIER(normaliser_degres(-1) == 359 && normaliser_degres(-360) == 0 && normaliser_degres(-361) == 359);
    VERIFIER(normaliser_degres(725) == 5 && normaliser_degres(-725) == 355);
    VERIFIER(ecart_angulaire(359, 0) == 1 && ecart_angulaire(0, 359) == 1); // le nord n'est pas à 359° de lui-même
    VERIFIER(ecart_angulaire(1, 359) == 2 && ecart_angulaire(359, 1) == 2);
    VERIFIER(ecart_angulaire(0, 180) == 180 && ecart_angulaire(270, 90) == 180);
    VERIFIER(ecart_angulaire(10, -10) == 20 && ecart_angulaire(90, 90) == 0);
    VERIFIER(ecart_angulaire(350, 10) == 20);
    return 0;
}

constexpr int arrondis_entiers() {
    VERIFIER(division_arrondie(5, 2) == 3 && division_arrondie(4, 2) == 2 && division_arrondie(3, 2) == 2);
    VERIFIER(division_arrondie(-5, 2) == -3 && division_arrondie(-3, 2) == -2 && division_arrondie(-1, 3) == 0);
    VERIFIER(division_arrondie(7, 3) == 2 && division_arrondie(-7, 3) == -2 && division_arrondie(0, 5) == 0);
    VERIFIER(division_plancher(7, 2) == 3 && division_plancher(-7, 2) == -4 && division_plancher(-8, 2) == -4);
    VERIFIER(division_plancher(0, 86400) == 0 && division_plancher(-1, 86400) == -1 &&
             division_plancher(86399, 86400) == 0);
    VERIFIER(division_plancher(86400, 86400) == 1);
    VERIFIER(limiter(5, 0, 10) == 5 && limiter(-5, 0, 10) == 0 && limiter(15, 0, 10) == 10);
    VERIFIER(present(0) && present(-1) && !present(ABSENT));
    return 0;
}

constexpr int rampe_lineaire_plate_aux_bouts_et_arrondie_au_plus_proche() {
    // montante : (50 -> 0) .. (130 -> 25)
    VERIFIER(rampe(0, 50, 0, 130, 25) == 0 && rampe(50, 50, 0, 130, 25) == 0);
    VERIFIER(rampe(130, 50, 0, 130, 25) == 25 && rampe(999, 50, 0, 130, 25) == 25);
    VERIFIER(rampe(90, 50, 0, 130, 25) == 13);  // 12,5 -> 13 : la moitié monte
    VERIFIER(rampe(100, 50, 0, 130, 25) == 16); // 15,625
    VERIFIER(rampe(70, 50, 0, 130, 25) == 6);   // 6,25
    // descendante : (250 -> 35) .. (700 -> 0) ; la valeur TOTALE est arrondie, pas l'écart
    VERIFIER(rampe(250, 250, 35, 700, 0) == 35 && rampe(700, 250, 35, 700, 0) == 0 &&
             rampe(1000, 250, 35, 700, 0) == 0);
    VERIFIER(rampe(0, 250, 35, 700, 0) == 35);
    VERIFIER(rampe(475, 250, 35, 700, 0) == 18); // 17,5 -> 18
    // bornes données à l'envers
    VERIFIER(rampe(90, 130, 25, 50, 0) == 13);
    // marche
    VERIFIER(rampe(5, 10, 7, 10, 3) == 7);
    return 0;
}

// ---- liste ---------------------------------------------------------------------------------------------------------------

#define LISTE(X)                                                                                                       \
    X(le_spot_de_test_est_valide)                                                                                      \
    X(les_valeurs_par_defaut_sont_valides_et_documentees)                                                              \
    X(un_spot_neuf_n_est_pas_valide_et_le_premier_champ_fautif_est_dit)                                                \
    X(noms_ordinaires_et_accentues_acceptes)                                                                           \
    X(le_nom_est_limite_en_caracteres_pas_en_octets)                                                                   \
    X(noms_invalides_refuses)                                                                                          \
    X(latitude_et_longitude_dans_leurs_bornes)                                                                         \
    X(la_position_zero_zero_est_une_position_jamais_saisie_mais_zero_ailleurs_est_permis)                              \
    X(les_champs_sont_verifies_dans_l_ordre_nom_latitude_longitude_orientation_fourchette)                             \
    X(orientation_de_0_a_359)                                                                                          \
    X(fourchette_de_hauteur_bornes_et_ecart)                                                                           \
    X(chaque_erreur_a_une_phrase_courte)                                                                               \
    X(normaliser_degres_et_ecart_angulaire_aux_bords)                                                                  \
    X(arrondis_entiers)                                                                                                \
    X(rampe_lineaire_plate_aux_bouts_et_arrondie_au_plus_proche)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
