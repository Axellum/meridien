// Tests du noyau pur : les entrées du premier anneau de la roue d'appui long (roue_entrees.hpp, ADR-0024) : lesquelles, dans quel ordre,
// et surtout ce qui ne bouge jamais (« Réglages » premier, « Détails » second, rien ne change de rang quand « Mer » ou « Profil » arrive).
#include "ecran/roue_entrees.hpp"
#include "mini.hpp"

using namespace ecran::roue;

namespace {

constexpr bool est(const Liste &l, int rang, Entree e) { return rang >= 0 && rang < l.n && l.entree[rang] == e; }

constexpr int rang_de(const Liste &l, Entree e) {
    for (int i = 0; i < l.n; i++) {
        if (l.entree[i] == e) {
            return i;
        }
    }
    return -1;
}

constexpr Liste sans(Presences p = {}) { return entrees_du_premier_anneau(p); }

constexpr int sans_alerte_ni_mer_la_roue_a_reglages_et_details() {
    constexpr Liste l = sans();
    VERIFIER(l.n == 2);
    VERIFIER(est(l, 0, Entree::Reglages));
    VERIFIER(est(l, 1, Entree::Details));
    return 0;
}

constexpr int avec_les_alertes_seules_elles_viennent_apres_details() {
    constexpr Liste l = sans({true, false});
    VERIFIER(l.n == 3);
    VERIFIER(est(l, 0, Entree::Reglages) && est(l, 1, Entree::Details) && est(l, 2, Entree::Alertes));
    VERIFIER(rang_de(l, Entree::Mer) == -1);
    return 0;
}

constexpr int avec_la_mer_seule_elle_vient_apres_details() {
    constexpr Liste l = sans({false, true});
    VERIFIER(l.n == 3);
    VERIFIER(est(l, 0, Entree::Reglages) && est(l, 1, Entree::Details) && est(l, 2, Entree::Mer));
    VERIFIER(rang_de(l, Entree::Alertes) == -1);
    return 0;
}

constexpr int avec_les_deux_la_mer_est_la_derniere() {
    constexpr Liste l = sans({true, true});
    VERIFIER(l.n == 4);
    VERIFIER(est(l, 0, Entree::Reglages) && est(l, 1, Entree::Details) && est(l, 2, Entree::Alertes) &&
             est(l, 3, Entree::Mer));
    return 0;
}

constexpr int avec_le_profil_seul_il_vient_apres_details() {
    constexpr Liste l = sans({false, false, true});
    VERIFIER(l.n == 3);
    VERIFIER(est(l, 0, Entree::Reglages) && est(l, 1, Entree::Details) && est(l, 2, Entree::Profil));
    VERIFIER(rang_de(l, Entree::Alertes) == -1 && rang_de(l, Entree::Mer) == -1);
    return 0;
}

constexpr int avec_les_trois_le_profil_est_le_dernier_et_la_mer_le_precede() {
    constexpr Liste l = sans({true, true, true});
    VERIFIER(l.n == 5);
    VERIFIER(est(l, 0, Entree::Reglages) && est(l, 1, Entree::Details) && est(l, 2, Entree::Alertes) &&
             est(l, 3, Entree::Mer) && est(l, 4, Entree::Profil));
    return 0;
}

constexpr int reglages_est_toujours_le_premier_et_details_le_second() {
    for (int a = 0; a < 2; a++) {
        for (int m = 0; m < 2; m++) {
            for (int p = 0; p < 2; p++) {
                const Liste l = sans({a != 0, m != 0, p != 0});
                VERIFIER(est(l, 0, Entree::Reglages));
                VERIFIER(est(l, 1, Entree::Details));
            }
        }
    }
    return 0;
}

constexpr int l_arrivee_de_la_mer_ne_change_le_rang_d_aucune_autre_entree() {
    for (int a = 0; a < 2; a++) {
        const Liste sans_mer = sans({a != 0, false});
        const Liste avec_mer = sans({a != 0, true});
        VERIFIER(avec_mer.n == sans_mer.n + 1);
        for (int i = 0; i < sans_mer.n; i++) {
            VERIFIER(avec_mer.entree[i] == sans_mer.entree[i]);
        }
    }
    return 0;
}

constexpr int l_arrivee_du_profil_ne_change_le_rang_d_aucune_autre_entree() {
    for (int a = 0; a < 2; a++) {
        for (int m = 0; m < 2; m++) {
            const Liste sans_profil = sans({a != 0, m != 0, false});
            const Liste avec_profil = sans({a != 0, m != 0, true});
            VERIFIER(avec_profil.n == sans_profil.n + 1);
            for (int i = 0; i < sans_profil.n; i++) {
                VERIFIER(avec_profil.entree[i] == sans_profil.entree[i]);
            }
            VERIFIER(est(avec_profil, avec_profil.n - 1, Entree::Profil));
        }
    }
    return 0;
}

constexpr int aucune_entree_n_est_en_double_et_le_premier_anneau_les_porte_toutes() {
    for (int a = 0; a < 2; a++) {
        for (int m = 0; m < 2; m++) {
            for (int p = 0; p < 2; p++) {
                const Liste l = sans({a != 0, m != 0, p != 0});
                VERIFIER(l.n >= 2 && l.n <= ENTREES_MAX && l.n <= BOUTONS_MAX);
                for (int i = 0; i < l.n; i++) {
                    for (int j = i + 1; j < l.n; j++) {
                        VERIFIER(l.entree[i] != l.entree[j]);
                    }
                }
            }
        }
    }
    return 0;
}

constexpr int le_nombre_d_entrees_est_celui_des_presences() {
    VERIFIER(sans({false, false}).n == 2);
    VERIFIER(sans({true, false}).n == 3);
    VERIFIER(sans({false, true}).n == 3);
    VERIFIER(sans({true, true}).n == 4);
    VERIFIER(sans({false, false, true}).n == 3);
    VERIFIER(sans({true, true, true}).n == 5);
    VERIFIER(ENTREES_MAX == 5);
    return 0;
}

constexpr int la_famille_profil_n_est_proposee_qu_a_partir_de_deux_choix() {
    VERIFIER(!profil_a_proposer(-1));
    VERIFIER(!profil_a_proposer(0));
    VERIFIER(!profil_a_proposer(1));
    VERIFIER(profil_a_proposer(2));
    VERIFIER(profil_a_proposer(3));
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(sans_alerte_ni_mer_la_roue_a_reglages_et_details)                                                                \
    X(avec_les_alertes_seules_elles_viennent_apres_details)                                                            \
    X(avec_la_mer_seule_elle_vient_apres_details)                                                                      \
    X(avec_les_deux_la_mer_est_la_derniere)                                                                            \
    X(avec_le_profil_seul_il_vient_apres_details)                                                                      \
    X(avec_les_trois_le_profil_est_le_dernier_et_la_mer_le_precede)                                                    \
    X(reglages_est_toujours_le_premier_et_details_le_second)                                                           \
    X(l_arrivee_de_la_mer_ne_change_le_rang_d_aucune_autre_entree)                                                     \
    X(l_arrivee_du_profil_ne_change_le_rang_d_aucune_autre_entree)                                                     \
    X(aucune_entree_n_est_en_double_et_le_premier_anneau_les_porte_toutes)                                             \
    X(le_nombre_d_entrees_est_celui_des_presences)                                                                     \
    X(la_famille_profil_n_est_proposee_qu_a_partir_de_deux_choix)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
