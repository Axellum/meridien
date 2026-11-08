// Tests du noyau pur : les boutons « − » et « + » de la plage de vent de la page « La mer » de l'écran (mer_reglage.hpp, ADR-0026).
#include <cstdint>

#include "ecran/mer_reglage.hpp"
#include "mini.hpp"

using namespace ecran::mer;
using namespace ecran::mer::reglage;

namespace {

constexpr Registre avec_vent(int mini, int maxi) {
    Registre r;
    r.vent_min_kn = static_cast<std::int16_t>(mini);
    r.vent_max_kn = static_cast<std::int16_t>(maxi);
    return r;
}

constexpr int un_registre_neuf_a_la_plage_par_defaut_de_vigie() {
    const Registre r;
    VERIFIER(r.vent_min_kn == VENT_MIN_DEFAUT_NOEUDS && r.vent_max_kn == VENT_MAX_DEFAUT_NOEUDS);
    return 0;
}

constexpr int chaque_bouton_decale_une_seule_borne_d_un_noeud() {
    Registre r = avec_vent(12, 25);
    VERIFIER(decaler_vent(r, Borne::Min, +PAS_VENT_NOEUDS) == ErreurRegistre::Aucune && r.vent_min_kn == 13 &&
             r.vent_max_kn == 25);
    VERIFIER(decaler_vent(r, Borne::Min, -PAS_VENT_NOEUDS) == ErreurRegistre::Aucune && r.vent_min_kn == 12 &&
             r.vent_max_kn == 25);
    VERIFIER(decaler_vent(r, Borne::Max, +PAS_VENT_NOEUDS) == ErreurRegistre::Aucune && r.vent_min_kn == 12 &&
             r.vent_max_kn == 26);
    VERIFIER(decaler_vent(r, Borne::Max, -PAS_VENT_NOEUDS) == ErreurRegistre::Aucune && r.vent_min_kn == 12 &&
             r.vent_max_kn == 25);
    return 0;
}

constexpr int le_minimum_ne_peut_ni_toucher_le_maximum_ni_tomber_a_zero() {
    Registre r = avec_vent(24, 25);
    VERIFIER(!vent_decalable(r, Borne::Min, +1)); // 25 / 25 : le minimum doit rester sous le maximum
    VERIFIER(decaler_vent(r, Borne::Min, +1) == ErreurRegistre::Vent && r.vent_min_kn == 24 && r.vent_max_kn == 25);
    VERIFIER(vent_decalable(r, Borne::Min, -1));
    Registre s = avec_vent(1, 25);
    VERIFIER(!vent_decalable(s, Borne::Min, -1)); // 0 nœud : refusé par l'API
    VERIFIER(decaler_vent(s, Borne::Min, -1) == ErreurRegistre::Vent && s.vent_min_kn == 1);
    return 0;
}

constexpr int le_maximum_ne_peut_ni_toucher_le_minimum_ni_depasser_soixante() {
    Registre r = avec_vent(12, 13);
    VERIFIER(!vent_decalable(r, Borne::Max, -1)); // 12 / 12
    VERIFIER(decaler_vent(r, Borne::Max, -1) == ErreurRegistre::Vent && r.vent_max_kn == 13);
    VERIFIER(vent_decalable(r, Borne::Max, +1));
    Registre s = avec_vent(12, VENT_PLAFOND_NOEUDS);
    VERIFIER(!vent_decalable(s, Borne::Max, +1));
    VERIFIER(decaler_vent(s, Borne::Max, +1) == ErreurRegistre::Vent && s.vent_max_kn == VENT_PLAFOND_NOEUDS);
    VERIFIER(vent_decalable(s, Borne::Max, -1));
    return 0;
}

constexpr int vent_decalable_dit_vrai_exactement_quand_le_decalage_passe() {
    for (int mini = 1; mini <= 59; mini += 7) {
        for (int maxi = mini + 1; maxi <= VENT_PLAFOND_NOEUDS; maxi += 5) {
            for (const Borne b : {Borne::Min, Borne::Max}) {
                for (const int pas : {-1, +1}) {
                    Registre r = avec_vent(mini, maxi);
                    const bool dit = vent_decalable(r, b, pas);
                    const bool fait = decaler_vent(r, b, pas) == ErreurRegistre::Aucune;
                    VERIFIER(dit == fait);
                    if (!fait) {
                        VERIFIER(r.vent_min_kn == mini && r.vent_max_kn == maxi); // un refus ne change rien
                    }
                }
            }
        }
    }
    return 0;
}

constexpr int le_decalage_ne_touche_ni_les_spots_ni_le_choix() {
    Registre r;
    VERIFIER(ajouter(r, "spot-a", "Spot A") == ErreurRegistre::Aucune);
    VERIFIER(ajouter(r, "spot-b", "Spot B") == ErreurRegistre::Aucune);
    VERIFIER(choisir(r, 1) == ErreurRegistre::Aucune);
    VERIFIER(decaler_vent(r, Borne::Max, +PAS_VENT_NOEUDS) == ErreurRegistre::Aucune);
    VERIFIER(r.nombre == 2 && r.choisi == 1 && registre_valide(r));
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(un_registre_neuf_a_la_plage_par_defaut_de_vigie)                                                                 \
    X(chaque_bouton_decale_une_seule_borne_d_un_noeud)                                                                 \
    X(le_minimum_ne_peut_ni_toucher_le_maximum_ni_tomber_a_zero)                                                       \
    X(le_maximum_ne_peut_ni_toucher_le_minimum_ni_depasser_soixante)                                                   \
    X(vent_decalable_dit_vrai_exactement_quand_le_decalage_passe)                                                      \
    X(le_decalage_ne_touche_ni_les_spots_ni_le_choix)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
