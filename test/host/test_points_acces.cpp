// Tests du noyau pur : classement du résultat d'une recherche de réseaux Wi-Fi (points_acces.hpp).
// Ce que l'écran de réglages affiche : un réseau par SSID, du plus fort au plus faible, sans réseau caché, sans dépasser la liste.
// La recherche elle-même (esp_wifi_scan_*) n'est PAS couverte ici : voir docs/mesures/wifi-retour-arriere.md.
#include <cstddef>
#include <cstdint>

#include "ecran/points_acces.hpp"
#include "mini.hpp"

using ecran::PointAcces;
namespace w = ecran::wifi;

namespace {

// Comparaison à part, pour ne pas tester le noyau avec lui-même.
constexpr bool ssid_est(const PointAcces &p, const char *attendu) {
    std::size_t i = 0;
    for (; attendu[i] != '\0'; ++i) {
        if (p.ssid[i] != attendu[i]) {
            return false;
        }
    }
    return p.ssid[i] == '\0';
}

constexpr int liste_vide_ou_sans_place() {
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(nullptr, 0, sortie, 4) == 0);
    const PointAcces brut[] = {{"Maison", -50, true}};
    VERIFIER(w::classer_points_acces(brut, 1, sortie, 0) == 0);
    VERIFIER(w::classer_points_acces(brut, 0, sortie, 4) == 0);
    return 0;
}

constexpr int reseau_cache_n_est_pas_liste() {
    const PointAcces brut[] = {{"", -30, true}, {"Maison", -60, true}, {"", -40, false}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 3, sortie, 4) == 1);
    VERIFIER(ssid_est(sortie[0], "Maison"));
    return 0;
}

constexpr int du_plus_fort_au_plus_faible() {
    const PointAcces brut[] = {{"Faible", -85, true}, {"Fort", -40, true}, {"Moyen", -65, false}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 3, sortie, 4) == 3);
    VERIFIER(ssid_est(sortie[0], "Fort") && sortie[0].rssi_dbm == -40 && sortie[0].securise);
    VERIFIER(ssid_est(sortie[1], "Moyen") && sortie[1].rssi_dbm == -65 && !sortie[1].securise);
    VERIFIER(ssid_est(sortie[2], "Faible") && sortie[2].rssi_dbm == -85);
    return 0;
}

constexpr int force_egale_ordre_des_octets_du_ssid() {
    const PointAcces brut[] = {{"Zebre", -60, true}, {"Abeille", -60, true}, {"Mouton", -60, true}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 3, sortie, 4) == 3);
    VERIFIER(ssid_est(sortie[0], "Abeille") && ssid_est(sortie[1], "Mouton") && ssid_est(sortie[2], "Zebre"));
    // Même résultat quel que soit l'ordre d'arrivée (la recherche rend les points d'accès dans un ordre quelconque).
    const PointAcces autre[] = {{"Mouton", -60, true}, {"Zebre", -60, true}, {"Abeille", -60, true}};
    PointAcces sortie2[4] = {};
    VERIFIER(w::classer_points_acces(autre, 3, sortie2, 4) == 3);
    VERIFIER(ssid_est(sortie2[0], "Abeille") && ssid_est(sortie2[1], "Mouton") && ssid_est(sortie2[2], "Zebre"));
    return 0;
}

constexpr int un_ssid_vu_plusieurs_fois_garde_le_plus_fort() {
    // Un répéteur faible, la borne principale forte, un 5 GHz moyen : une seule ligne, celle de la borne principale.
    const PointAcces brut[] = {
        {"Maison", -80, false}, {"Maison", -45, true}, {"Maison", -60, false}, {"Voisin", -70, true}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 4, sortie, 4) == 2);
    VERIFIER(ssid_est(sortie[0], "Maison") && sortie[0].rssi_dbm == -45);
    VERIFIER(sortie[0].securise); // le drapeau suit le point d'accès retenu, pas le premier vu
    VERIFIER(ssid_est(sortie[1], "Voisin"));
    return 0;
}

constexpr int le_plus_fort_arrive_en_premier_et_les_autres_sont_ignores() {
    const PointAcces brut[] = {{"Maison", -45, true}, {"Maison", -80, false}, {"Maison", -45, false}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 3, sortie, 4) == 1);
    VERIFIER(sortie[0].rssi_dbm == -45 && sortie[0].securise); // à force égale, le premier vu reste
    return 0;
}

constexpr int un_doublon_plus_fort_remonte_dans_la_liste() {
    const PointAcces brut[] = {{"A", -40, true}, {"B", -50, true}, {"C", -60, true}, {"C", -30, true}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 4, sortie, 4) == 3);
    VERIFIER(ssid_est(sortie[0], "C") && sortie[0].rssi_dbm == -30);
    VERIFIER(ssid_est(sortie[1], "A") && ssid_est(sortie[2], "B"));
    return 0;
}

constexpr int un_doublon_du_milieu_plus_fort_garde_les_autres() {
    const PointAcces brut[] = {{"A", -40, true}, {"B", -50, true}, {"C", -60, true}, {"B", -45, true}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 4, sortie, 4) == 3);
    VERIFIER(ssid_est(sortie[0], "A") && sortie[0].rssi_dbm == -40);
    VERIFIER(ssid_est(sortie[1], "B") && sortie[1].rssi_dbm == -45);
    VERIFIER(ssid_est(sortie[2], "C") && sortie[2].rssi_dbm == -60);
    return 0;
}

constexpr int liste_pleine_garde_les_plus_forts() {
    const PointAcces brut[] = {
        {"R1", -90, true}, {"R2", -70, true}, {"R3", -50, true}, {"R4", -80, true}, {"R5", -60, true}};
    PointAcces sortie[3] = {};
    VERIFIER(w::classer_points_acces(brut, 5, sortie, 3) == 3);
    VERIFIER(ssid_est(sortie[0], "R3") && ssid_est(sortie[1], "R5") && ssid_est(sortie[2], "R2"));
    return 0;
}

constexpr int liste_pleine_un_reseau_plus_faible_que_le_dernier_est_ignore() {
    const PointAcces brut[] = {{"A", -40, true}, {"B", -50, true}, {"C", -60, true}, {"D", -95, true}};
    PointAcces sortie[3] = {};
    VERIFIER(w::classer_points_acces(brut, 4, sortie, 3) == 3);
    VERIFIER(ssid_est(sortie[0], "A") && ssid_est(sortie[1], "B") && ssid_est(sortie[2], "C"));
    return 0;
}

constexpr int liste_pleine_un_reseau_plus_fort_chasse_le_dernier() {
    const PointAcces brut[] = {{"A", -40, true}, {"B", -50, true}, {"C", -60, true}, {"D", -45, true}};
    PointAcces sortie[3] = {};
    VERIFIER(w::classer_points_acces(brut, 4, sortie, 3) == 3);
    VERIFIER(ssid_est(sortie[0], "A") && ssid_est(sortie[1], "D") && ssid_est(sortie[2], "B"));
    return 0;
}

constexpr int liste_pleine_un_doublon_plus_fort_ne_chasse_personne() {
    const PointAcces brut[] = {{"A", -40, true}, {"B", -50, true}, {"C", -60, true}, {"C", -30, true}};
    PointAcces sortie[3] = {};
    VERIFIER(w::classer_points_acces(brut, 4, sortie, 3) == 3);
    VERIFIER(ssid_est(sortie[0], "C") && ssid_est(sortie[1], "A") && ssid_est(sortie[2], "B"));
    return 0;
}

constexpr int une_seule_place() {
    const PointAcces brut[] = {{"A", -70, true}, {"B", -40, true}, {"C", -55, true}};
    PointAcces sortie[1] = {};
    VERIFIER(w::classer_points_acces(brut, 3, sortie, 1) == 1);
    VERIFIER(ssid_est(sortie[0], "B"));
    return 0;
}

constexpr int ssid_de_trente_deux_octets_complet() {
    // 32 octets, terminé par le zéro du tableau de 33 : copié en entier et distinct d'un SSID qui n'en diffère que par le dernier octet.
    const PointAcces brut[] = {{"0123456789012345678901234567890A", -50, true},
                               {"0123456789012345678901234567890B", -55, true}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 2, sortie, 4) == 2);
    VERIFIER(ssid_est(sortie[0], "0123456789012345678901234567890A"));
    VERIFIER(ssid_est(sortie[1], "0123456789012345678901234567890B"));
    return 0;
}

constexpr int ssid_sans_zero_final_est_borne() {
    // L'appelant a oublié le zéro : 33 octets non nuls. La copie ne doit pas sortir du tableau et doit rester terminée.
    PointAcces mauvais = {{}, -50, true};
    for (std::size_t i = 0; i < 33; ++i) {
        mauvais.ssid[i] = 'x';
    }
    PointAcces sortie[2] = {};
    const std::size_t n = w::classer_points_acces(&mauvais, 1, sortie, 2);
    VERIFIER(n == 1);
    VERIFIER(sortie[0].ssid[32] == '\0');
    VERIFIER(sortie[0].ssid[31] == 'x');
    return 0;
}

constexpr int octets_au_dela_de_127_comparent_sans_signe() {
    // Un SSID en UTF-8 (« É » = 0xC3 0x89) passe après un SSID ASCII de même force : comparaison en octets non signés.
    const PointAcces brut[] = {{"\xC3\x89t\xC3\xA9", -60, true}, {"Zebre", -60, true}};
    PointAcces sortie[2] = {};
    VERIFIER(w::classer_points_acces(brut, 2, sortie, 2) == 2);
    VERIFIER(ssid_est(sortie[0], "Zebre"));
    return 0;
}

constexpr int les_ssid_qui_different_par_la_casse_sont_distincts() {
    const PointAcces brut[] = {{"maison", -50, true}, {"Maison", -55, true}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 2, sortie, 4) == 2);
    return 0;
}

constexpr int l_entree_n_est_pas_modifiee() {
    PointAcces brut[] = {{"B", -70, true}, {"A", -40, false}};
    PointAcces sortie[4] = {};
    VERIFIER(w::classer_points_acces(brut, 2, sortie, 4) == 2);
    VERIFIER(ssid_est(brut[0], "B") && brut[0].rssi_dbm == -70 && ssid_est(brut[1], "A"));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(liste_vide_ou_sans_place)                                                                                        \
    X(reseau_cache_n_est_pas_liste)                                                                                    \
    X(du_plus_fort_au_plus_faible)                                                                                     \
    X(force_egale_ordre_des_octets_du_ssid)                                                                            \
    X(un_ssid_vu_plusieurs_fois_garde_le_plus_fort)                                                                    \
    X(le_plus_fort_arrive_en_premier_et_les_autres_sont_ignores)                                                       \
    X(un_doublon_plus_fort_remonte_dans_la_liste)                                                                      \
    X(un_doublon_du_milieu_plus_fort_garde_les_autres)                                                                 \
    X(liste_pleine_garde_les_plus_forts)                                                                               \
    X(liste_pleine_un_reseau_plus_faible_que_le_dernier_est_ignore)                                                    \
    X(liste_pleine_un_reseau_plus_fort_chasse_le_dernier)                                                              \
    X(liste_pleine_un_doublon_plus_fort_ne_chasse_personne)                                                            \
    X(une_seule_place)                                                                                                 \
    X(ssid_de_trente_deux_octets_complet)                                                                              \
    X(ssid_sans_zero_final_est_borne)                                                                                  \
    X(octets_au_dela_de_127_comparent_sans_signe)                                                                      \
    X(les_ssid_qui_different_par_la_casse_sont_distincts)                                                              \
    X(l_entree_n_est_pas_modifiee)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
