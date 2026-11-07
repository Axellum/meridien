// Tests du noyau pur : le rythme des requêtes à MET Norway et au NWS (meteo_rythme_natif.hpp).
//
// Les valeurs de `Expires` viennent des réponses réelles du 07/11/2026 (donnees/fournisseurs_capture.json) : MET Norway rend un
// `Expires` 310 à 1 828 s après la requête (30 minutes après le `Last-Modified`, la réponse pouvant avoir déjà 25 minutes dans son
// cache), la grille du NWS une heure au plus. Chaque propriété est vérifiée pour les 256 valeurs de l'octet aléatoire.
#include <cstdint>

#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/meteo_rythme_natif.hpp"
#include "mini.hpp"

using namespace ecran::meteo::fournisseurs;

namespace {

constexpr std::int64_t T0 = 1794052800;  // samedi 7 novembre 2026, 12:00:00 UTC : une heure ronde
constexpr std::int64_t T = T0 + 10 * 60; // 12:10:00 : 1 800 s plus tard, on est à 12:40 (loin de l'heure ronde)
constexpr std::int64_t DEUX_HEURES = 2 * 3600;

constexpr std::int64_t reste_dans_l_heure(std::int64_t instant) { return ((instant % 3600) + 3600) % 3600; }

constexpr int les_planchers_sont_ceux_de_chaque_service() {
    VERIFIER(plancher_s(Famille::MetNo) == 30 * 60);
    VERIFIER(plancher_s(Famille::Nws) == 60 * 60);
    VERIFIER(plancher_s(Famille::OpenMeteo) == 15 * 60); // le rythme d'avant ne change pas
    VERIFIER(PLAFOND_S == 90 * 60 && PLAFOND_S < DEUX_HEURES);
    VERIFIER(DUREE_MAILLE_NWS_S == 24 * 3600);
    return 0;
}

constexpr int sans_expires_c_est_le_plancher_plus_au_plus_un_dixieme() {
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, 0, 0) == 1800);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, 0, 255) == 1800 + 180); // un dixième exactement
    VERIFIER(delai_apres_succes_s(Famille::Nws, T, 0, 0) == 3600);
    VERIFIER(delai_apres_succes_s(Famille::Nws, T, 0, 255) == 3600 + 360);
    VERIFIER(delai_apres_succes_s(Famille::OpenMeteo, T, 0, 0) == 900);
    // plus l'octet est grand, plus le délai l'est (jamais l'inverse) : le décalage n'est pas à l'envers
    std::int64_t avant = 0;
    for (int a = 0; a < 256; ++a) {
        const std::int64_t d = delai_apres_succes_s(Famille::MetNo, T, 0, static_cast<std::uint8_t>(a));
        VERIFIER(d >= avant && d >= 1800 && d <= 1980);
        avant = d;
    }
    return 0;
}

constexpr int le_delai_suit_expires_quand_il_est_plus_loin_que_le_plancher() {
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 1800, 0) == 1800); // pile le plancher
    // Les cinq `Expires` de MET Norway mesurés le 07/11/2026, en secondes après la requête : 310 (Tromsø), 747 (Reykjavik) et 1 521
    // (Sydney) sont sous le plancher, qui s'applique (plus strict que le service) ; 1 816 (Oslo) et 1 828 (Longyearbyen) le dépassent.
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 310, 0) == 1800);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 747, 0) == 1800);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 1521, 0) == 1800);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 1816, 0) == 1816);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 1828, 0) == 1828);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 2400, 0) == 2400);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 2400, 255) == 2400 + 240);
    VERIFIER(delai_apres_succes_s(Famille::Nws, T, T + 3600, 0) == 3600);
    VERIFIER(delai_apres_succes_s(Famille::Nws, T, T + 4200, 0) == 4200);
    return 0;
}

constexpr int un_expires_absent_passe_ou_trop_court_ne_raccourcit_jamais_le_plancher() {
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, 0, 0) == 1800);       // absent ou illisible
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, -5, 0) == 1800);      //
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T - 100, 0) == 1800); // déjà passé
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T, 0) == 1800);       // maintenant
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 60, 0) == 1800);  // dans une minute
    VERIFIER(delai_apres_succes_s(Famille::Nws, T, T + 108, 0) == 3600); // la grille de Miami avait un max-age de 108 s
    return 0;
}

constexpr int un_expires_lointain_est_plafonne_a_quatre_vingt_dix_minutes() {
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, T + 3 * 3600, 0) == 5400);
    VERIFIER(delai_apres_succes_s(Famille::Nws, T, T + 86400, 0) == 5400);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T, 400000000000LL, 0) == 5400); // l'an 14 000
    return 0;
}

constexpr int l_heure_ronde_est_evitee_avec_un_etalement() {
    constexpr std::int64_t T30 = T0 + 30 * 60; // 12:30:00 : 1 800 s plus tard, 13:00:00 pile
    // 13:00:00 -> sorti de la zone à 13:02:00 (120 s de plus), puis alea & 63 s
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T30, 0, 0) == 1800 + 120);
    // un octet aléatoire qui retombe à 13:00:44 : sorti à 13:02:00 (+76 s), puis +63 s d'étalement
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T30, 0, 63) == 1844 + 76 + 63);
    // 12:59:00 est dans la minute qui précède l'heure ronde : sorti à 13:02:00 aussi
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T0 + 29 * 60, 0, 0) == 1800 + 180);
    // les bornes exactes : 12:58:59 est libre, 12:59:00 ne l'est plus, 13:01:59 ne l'est plus, 13:02:00 l'est
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T0 + 28 * 60 + 59, 0, 0) == 1800);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T0 + 29 * 60, 0, 0) == 1980);
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T0 + 31 * 60 + 59, 0, 0) ==
             1800 + 1); // 12:31:59 + 1 800 s = 13:01:59 : dans la zone, sortie à 13:02:00
    VERIFIER(delai_apres_succes_s(Famille::MetNo, T0 + 32 * 60, 0, 0) == 1800); // 12:32:00 + 1 800 s = 13:02:00 : libre
    return 0;
}

constexpr int jamais_dans_la_zone_de_l_heure_ronde_ni_sous_le_plancher_ni_au_dessus_de_deux_heures() {
    constexpr Famille FAMILLES[] = {Famille::MetNo, Famille::Nws, Famille::OpenMeteo};
    constexpr std::int64_t EXPIRES_SUR[] = {0, 1, 600, 2400, 5400, 100000};
    for (const Famille f : FAMILLES) {
        for (std::int64_t decalage = 0; decalage < 3 * 3600;
             decalage += 97) { // 97 s, premier avec 3 600 : tombe sur toutes les secondes de l'heure
            const std::int64_t maintenant = T0 + decalage;
            for (const std::int64_t e : EXPIRES_SUR) {
                const std::int64_t expires = e == 0 ? 0 : maintenant + e;
                for (int a = 0; a < 256; ++a) {
                    const std::int64_t d = delai_apres_succes_s(f, maintenant, expires, static_cast<std::uint8_t>(a));
                    const std::int64_t reste = reste_dans_l_heure(maintenant + d);
                    VERIFIER(reste >= 120 && reste < 3540); // loin de l'heure ronde
                    VERIFIER(d >= plancher_s(f));           // jamais sous le plancher du service
                    VERIFIER(d < DEUX_HEURES);              // jamais assez long pour périmer la météo
                    const std::int64_t exige = e < PLAFOND_S ? e : PLAFOND_S;
                    VERIFIER(d >= exige); // jamais avant `Expires` (dans la limite du plafond)
                }
            }
        }
    }
    return 0;
}

constexpr int une_heure_incertaine_ne_donne_pas_de_delai_absurde() {
    // horloge non réglée (proche de zéro), négative, ou très grande : le résultat reste un délai positif et borné
    for (const std::int64_t maintenant : {std::int64_t(0), std::int64_t(1), std::int64_t(-7200), std::int64_t(-1),
                                          std::int64_t(4102444800LL), std::int64_t(250000000000LL)}) {
        for (const Famille f : {Famille::MetNo, Famille::Nws, Famille::OpenMeteo}) {
            for (int a = 0; a < 256; a += 5) {
                const std::int64_t d = delai_apres_succes_s(f, maintenant, 0, static_cast<std::uint8_t>(a));
                VERIFIER(d >= plancher_s(f) && d < DEUX_HEURES);
                const std::int64_t reste = reste_dans_l_heure(maintenant + d);
                VERIFIER(reste >= 120 && reste < 3540);
            }
        }
    }
    return 0;
}

constexpr int la_maille_du_nws_vaut_vingt_quatre_heures() {
    constexpr std::int64_t OBTENUE = T0;
    VERIFIER(maille_nws_valable(OBTENUE, OBTENUE));                           // tout de suite
    VERIFIER(maille_nws_valable(OBTENUE, OBTENUE + 3600));                    //
    VERIFIER(maille_nws_valable(OBTENUE, OBTENUE + DUREE_MAILLE_NWS_S - 1));  // 23 h 59 min 59 s
    VERIFIER(!maille_nws_valable(OBTENUE, OBTENUE + DUREE_MAILLE_NWS_S));     // 24 h : on redemande
    VERIFIER(!maille_nws_valable(OBTENUE, OBTENUE + 3 * DUREE_MAILLE_NWS_S)); //
    VERIFIER(!maille_nws_valable(OBTENUE, OBTENUE - 1)); // l'horloge a reculé : on ne se fie pas à un âge négatif
    VERIFIER(!maille_nws_valable(0, OBTENUE) && !maille_nws_valable(-5, OBTENUE)); // jamais obtenue
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_planchers_sont_ceux_de_chaque_service)                                                                       \
    X(sans_expires_c_est_le_plancher_plus_au_plus_un_dixieme)                                                          \
    X(le_delai_suit_expires_quand_il_est_plus_loin_que_le_plancher)                                                    \
    X(un_expires_absent_passe_ou_trop_court_ne_raccourcit_jamais_le_plancher)                                          \
    X(un_expires_lointain_est_plafonne_a_quatre_vingt_dix_minutes)                                                     \
    X(l_heure_ronde_est_evitee_avec_un_etalement)                                                                      \
    X(jamais_dans_la_zone_de_l_heure_ronde_ni_sous_le_plancher_ni_au_dessus_de_deux_heures)                            \
    X(une_heure_incertaine_ne_donne_pas_de_delai_absurde)                                                              \
    X(la_maille_du_nws_vaut_vingt_quatre_heures)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
