// Tests du noyau pur : choix de l'icône (code WMO + jour/nuit -> pièces teintées), choix de la taille d'image, et couverture
// des 29 codes par une icône ET un libellé dans les sept langues de l'écran.
#include <initializer_list>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/visuel_icones.hpp"
#include "mini.hpp"

using namespace ecran::visuel;
using ecran::i18n::Langue;
using ecran::meteo::decrire;

namespace {

// Les 29 codes d'Open-Meteo (copie indépendante de la table du noyau, comme dans test_meteo_wmo.cpp).
constexpr int CODES[] = {0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66,
                         67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 97, 99};

constexpr int chaque_code_a_une_icone_composee_jour_et_nuit() {
    for (int code : CODES) {
        for (bool jour : {true, false}) {
            const IdIcone id = icone_pour_code(code, jour);
            VERIFIER(id != IdIcone::inconnue);
            VERIFIER(id != IdIcone::NOMBRE);
            const Composition &c = composition(id);
            VERIFIER(c.nb >= 1 && c.nb <= 3);
            for (int i = 0; i < c.nb; ++i) {
                VERIFIER(c.couches[i].piece != Piece::AUCUNE);
                VERIFIER(c.couches[i].role != Role::AUCUN);
                // une icône météo n'emploie que des pièces de la famille météo (jamais une icône d'information)
                VERIFIER(famille(c.couches[i].piece) == Famille::METEO);
            }
            // les couches inutilisées sont vides
            for (int i = c.nb; i < 3; ++i) {
                VERIFIER(c.couches[i].piece == Piece::AUCUNE);
                VERIFIER(c.couches[i].role == Role::AUCUN);
            }
        }
    }
    return 0;
}

constexpr int un_code_inconnu_donne_le_nuage_seul() {
    for (int code : {-1, 4, 10, 100, 1000}) {
        VERIFIER(icone_pour_code(code, true) == IdIcone::inconnue);
        VERIFIER(composition(IdIcone::inconnue).nb == 1);
    }
    return 0;
}

constexpr int le_jour_et_la_nuit_different_pour_le_ciel_degage_et_les_averses() {
    // Codes 0 à 2 : soleil / lune ; averses (80, 81) et averses de neige (85, 86) : soleil ou lune derrière le nuage.
    for (int code : {0, 1, 2, 80, 81, 85, 86}) {
        VERIFIER(icone_pour_code(code, true) != icone_pour_code(code, false));
    }
    // Tous les autres codes : la même icône de jour et de nuit.
    for (int code : CODES) {
        if (code <= 2 || code == 80 || code == 81 || code == 85 || code == 86)
            continue;
        VERIFIER(icone_pour_code(code, true) == icone_pour_code(code, false));
    }
    // Cohérence : la nuit, l'astre est une lune ; le jour, un soleil.
    VERIFIER(composition(icone_pour_code(0, true)).couches[0].role == Role::soleil);
    VERIFIER(composition(icone_pour_code(0, false)).couches[0].role == Role::lune);
    VERIFIER(composition(icone_pour_code(80, true)).couches[0].role == Role::soleil);
    VERIFIER(composition(icone_pour_code(80, false)).couches[0].role == Role::lune);
    return 0;
}

constexpr int aucune_icone_n_est_orpheline() {
    // Chaque icône de la table sert à au moins un code WMO (jour ou nuit), sauf l'icône « inconnue » (codes absents).
    for (int i = 0; i < static_cast<int>(IdIcone::NOMBRE); ++i) {
        const IdIcone id = static_cast<IdIcone>(i);
        if (id == IdIcone::inconnue)
            continue;
        bool utilisee = false;
        for (int code : CODES) {
            for (bool jour : {true, false})
                utilisee = utilisee || (icone_pour_code(code, jour) == id);
        }
        VERIFIER(utilisee);
    }
    return 0;
}

constexpr int la_pluie_est_bleue_la_neige_blanche_l_eclair_jaune() {
    // Rôles par famille de temps : la lecture d'un coup d'oeil repose dessus.
    const auto a_role = [](int code, Role r) {
        const Composition &c = composition(icone_pour_code(code, true));
        for (int i = 0; i < c.nb; ++i) {
            if (c.couches[i].role == r)
                return true;
        }
        return false;
    };
    for (int code : {51, 53, 55, 61, 63, 65, 80, 81, 82})
        VERIFIER(a_role(code, Role::pluie));
    for (int code : {71, 73, 75, 77, 85, 86})
        VERIFIER(a_role(code, Role::neige));
    for (int code : {95, 96, 97, 99})
        VERIFIER(a_role(code, Role::eclair));
    VERIFIER(a_role(0, Role::soleil));
    VERIFIER(a_role(45, Role::brume));
    VERIFIER(!a_role(0, Role::pluie) && !a_role(3, Role::pluie));
    return 0;
}

constexpr int les_phases_de_lune_sont_huit_pieces_consecutives() {
    VERIFIER(NB_PHASES_LUNE == 8);
    for (int i = 0; i < NB_PHASES_LUNE; ++i) {
        VERIFIER(famille(piece_phase_lune(i)) == Famille::LUNE);
        VERIFIER(static_cast<int>(piece_phase_lune(i)) == static_cast<int>(piece_phase_lune(0)) + i);
    }
    VERIFIER(piece_phase_lune(8) == piece_phase_lune(0));  // le cycle boucle
    VERIFIER(piece_phase_lune(-1) == piece_phase_lune(7)); // jamais hors tableau
    return 0;
}

constexpr int la_taille_choisie_ne_depasse_jamais_la_place() {
    constexpr int T[] = {44, 60, 84, 120, 168, 232};
    constexpr std::size_t N = sizeof(T) / sizeof(T[0]);
    VERIFIER(choisir_taille(T, N, 232) == 232);
    VERIFIER(choisir_taille(T, N, 1000) == 232);
    VERIFIER(choisir_taille(T, N, 231) == 168);
    VERIFIER(choisir_taille(T, N, 84) == 84);
    VERIFIER(choisir_taille(T, N, 60) == 60);
    VERIFIER(choisir_taille(T, N, 43) == 44); // rien ne tient : la plus petite
    VERIFIER(choisir_taille(T, N, 0) == 44);
    VERIFIER(choisir_taille(T, 0, 100) == 0);
    VERIFIER(choisir_taille(nullptr, 3, 100) == 0);
    for (int cible = 44; cible <= 400; ++cible) {
        VERIFIER(choisir_taille(T, N, cible) <= cible);
    }
    return 0;
}

constexpr int les_tailles_generees_sont_croissantes() {
    for (std::size_t i = 1; i < sizeof(TAILLES_METEO) / sizeof(TAILLES_METEO[0]); ++i)
        VERIFIER(TAILLES_METEO[i] > TAILLES_METEO[i - 1]);
    for (std::size_t i = 1; i < sizeof(TAILLES_INFO) / sizeof(TAILLES_INFO[0]); ++i)
        VERIFIER(TAILLES_INFO[i] > TAILLES_INFO[i - 1]);
    for (std::size_t i = 1; i < sizeof(TAILLES_LUNE) / sizeof(TAILLES_LUNE[0]); ++i)
        VERIFIER(TAILLES_LUNE[i] > TAILLES_LUNE[i - 1]);
    for (std::size_t i = 1; i < sizeof(EMBARQUEES_METEO) / sizeof(EMBARQUEES_METEO[0]); ++i)
        VERIFIER(EMBARQUEES_METEO[i] > EMBARQUEES_METEO[i - 1]);
    for (int t : EMBARQUEES_METEO)
        VERIFIER(contient(TAILLES_METEO, sizeof(TAILLES_METEO) / sizeof(TAILLES_METEO[0]), t));
    for (int t : EMBARQUEES_INFO)
        VERIFIER(contient(TAILLES_INFO, sizeof(TAILLES_INFO) / sizeof(TAILLES_INFO[0]), t));
    for (int t : EMBARQUEES_LUNE)
        VERIFIER(contient(TAILLES_LUNE, sizeof(TAILLES_LUNE) / sizeof(TAILLES_LUNE[0]), t));
    return 0;
}

// Le binaire n'embarque que les tailles que l'écran choisit (relevé au banc de rendu, tools/icones/generer_icones.py) : pour toute
// place, la taille servie est celle de l'échelle (aucun pixel ne change), ou 0 si l'échelle choisit une taille absente du binaire
// (l'erreur est alors visible : ICONE_ABSENTE). Les places relevées au banc le 11/11/2026 sont servies telles quelles.
constexpr int une_taille_absente_du_binaire_est_une_erreur_et_jamais_une_autre_taille() {
    constexpr std::size_t NM = sizeof(TAILLES_METEO) / sizeof(TAILLES_METEO[0]);
    constexpr std::size_t NI = sizeof(TAILLES_INFO) / sizeof(TAILLES_INFO[0]);
    constexpr std::size_t NL = sizeof(TAILLES_LUNE) / sizeof(TAILLES_LUNE[0]);
    for (int cible = 0; cible <= 400; ++cible) {
        const int m = taille_meteo(cible);
        VERIFIER(m == 0 || m == choisir_taille(TAILLES_METEO, NM, cible));
        const int i = taille_info(cible);
        VERIFIER(i == 0 || i == choisir_taille(TAILLES_INFO, NI, cible));
        const int l = taille_lune(cible);
        VERIFIER(l == 0 || l == choisir_taille(TAILLES_LUNE, NL, cible));
    }
    // Places relevées au banc (météo 30 à 206 px, info 22 à 56, lune 38 et 48) : servies, à la taille de l'échelle.
    VERIFIER(taille_meteo(30) == 44 && taille_meteo(56) == 44);
    VERIFIER(taille_meteo(137) == 120 && taille_meteo(164) == 120);
    VERIFIER(taille_meteo(172) == 168 && taille_meteo(206) == 168);
    VERIFIER(taille_info(22) == 28 && taille_info(38) == 28 && taille_info(48) == 40 && taille_info(56) == 56);
    VERIFIER(taille_lune(38) == 64 && taille_lune(48) == 64);
    // Tailles retirées du binaire : une place qui les choisirait obtient 0, jamais une image plus petite en silence.
    VERIFIER(taille_meteo(60) == 0 && taille_meteo(84) == 0 && taille_meteo(232) == 0 && taille_meteo(10000) == 0);
    VERIFIER(taille_info(80) == 0 && taille_lune(112) == 0);
    VERIFIER(taille_servie(TAILLES_METEO, NM, nullptr, 0, 100) == 0);
    return 0;
}

// Les 29 libellés de l'écran existent dans les sept langues : pas de repli silencieux sur le français ou l'anglais, un écran
// allemand ou turc qui montre du français est un défaut (ADR-0006). Ce que le test ne dit PAS : que la traduction soit juste
// (aucune n'a été relue par un locuteur, voir i18n/README.md).
constexpr int chaque_code_a_un_libelle_dans_les_sept_langues() {
    for (int code : CODES) {
        const auto c = decrire(code, true);
        VERIFIER(c.connu && !c.libelle.empty());
        for (std::size_t l = 1; l < ecran::i18n::NOMBRE_LANGUES; ++l) {
            const std::string_view propre = ecran::i18n::propre(c.libelle, static_cast<Langue>(l));
            VERIFIER(!propre.empty());
        }
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_code_a_une_icone_composee_jour_et_nuit)                                                                   \
    X(un_code_inconnu_donne_le_nuage_seul)                                                                             \
    X(le_jour_et_la_nuit_different_pour_le_ciel_degage_et_les_averses)                                                 \
    X(aucune_icone_n_est_orpheline)                                                                                    \
    X(la_pluie_est_bleue_la_neige_blanche_l_eclair_jaune)                                                              \
    X(les_phases_de_lune_sont_huit_pieces_consecutives)                                                                \
    X(la_taille_choisie_ne_depasse_jamais_la_place)                                                                    \
    X(les_tailles_generees_sont_croissantes)                                                                           \
    X(une_taille_absente_du_binaire_est_une_erreur_et_jamais_une_autre_taille)                                         \
    X(chaque_code_a_un_libelle_dans_les_sept_langues)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
