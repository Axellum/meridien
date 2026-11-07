// Tests du noyau pur : lecture de l'INA226 et état de l'alimentation (alimentation.hpp). Références : formules de la fiche TI et du
// composant `ina226` d'ESPHome 2026.9.0 (lsb = ceil(Imax / 32768), étalonnage = 0,00512 / (lsb * R)).
#include "ecran/alimentation.hpp"
#include "mini.hpp"

using namespace ecran::alimentation;

namespace {

constexpr int etalonnage_du_tab5() {
    // Config du Tab5 : shunt 5 mohm, courant maximal 8,192 A -> LSB 250 uA -> registre 4096 (valeur de la fiche TI : 0x1000).
    VERIFIER(lsb_courant_ua(8192) == 250);
    VERIFIER(etalonnage(250, 5) == 4096);
    VERIFIER(lsb_courant_ua(1000) == 31); // 1 A / 32768 = 30,5 uA -> arrondi vers le haut
    VERIFIER(etalonnage(31, 100) == 1651);
    VERIFIER(etalonnage(0, 5) == 0);
    VERIFIER(etalonnage(250, 0) == 0);
    return 0;
}

constexpr int tension_du_bus() {
    VERIFIER(tension_mv(0) == 0);
    VERIFIER(tension_mv(1) == 1);         // 1,25 mV arrondi
    VERIFIER(tension_mv(3) == 4);         // 3,75 mV arrondi au plus proche
    VERIFIER(tension_mv(4) == 5);         // 5,00 mV
    VERIFIER(tension_mv(6720) == 8400);   // batterie 2S pleine : 8,400 V
    VERIFIER(tension_mv(1464) == 1830);   // 1,83 V : valeur relevee sans batterie, chargeur coupe
    VERIFIER(tension_mv(65535) == 81919); // plein registre (non signe)
    return 0;
}

constexpr int tension_du_shunt_et_courant_signes() {
    VERIFIER(shunt_uv(0) == 0);
    VERIFIER(shunt_uv(400) == 1000);    // 400 * 2,5 uV
    VERIFIER(shunt_uv(0xFFFF) == -2);   // -1 * 2,5 uV tronque vers zero
    VERIFIER(shunt_uv(0xFF9C) == -250); // -100 * 2,5 uV
    // courant : pas de 250 uA
    VERIFIER(courant_ma(0, 250) == 0);
    VERIFIER(courant_ma(4000, 250) == 1000); // +1 A (decharge)
    VERIFIER(courant_ma(static_cast<std::uint16_t>(-1800), 250) ==
             -450);                    // -0,450 A : charge (valeur vue chez un tiers)
    VERIFIER(courant_ma(1, 250) == 0); // 0,25 mA -> 0 (arrondi)
    VERIFIER(courant_ma(2, 250) == 1); // 0,5 mA -> 1 (arrondi au plus proche)
    VERIFIER(courant_ma(static_cast<std::uint16_t>(-2), 250) == -1);
    return 0;
}

constexpr int presence_de_batterie() {
    VERIFIER(batterie_presente(8400, true));
    VERIFIER(batterie_presente(5500, true));
    VERIFIER(!batterie_presente(5499, true));
    VERIFIER(!batterie_presente(1830, true)); // sans batterie, chargeur coupe
    VERIFIER(
        !batterie_presente(8400, false)); // chargeur allume : la tension lue est celle du chargeur, rien n'est prouve
    VERIFIER(!batterie_presente(0, true));
    return 0;
}

constexpr int classement_de_l_etat() {
    VERIFIER(classer(false, 8000, 0, true) == Etat::Inconnu); // la puce ne repond pas
    VERIFIER(classer(true, 1830, 0, true) == Etat::SansBatterie);
    VERIFIER(classer(true, 1900, -300, true) == Etat::SansBatterie); // le courant n'y change rien
    VERIFIER(classer(true, 8000, 5, true) == Etat::Repos);
    VERIFIER(classer(true, 8000, -450, true) == Etat::EnCharge);
    VERIFIER(classer(true, 8000, 250, true) == Etat::EnDecharge);
    VERIFIER(classer(true, 8000, -19, true) == Etat::Repos); // sous le seuil de repos
    VERIFIER(classer(true, 8000, -20, true) == Etat::EnCharge);
    VERIFIER(classer(true, 8000, 20, true) == Etat::EnDecharge);
    VERIFIER(classer(true, 8000, 19, true) == Etat::Repos);
    VERIFIER(classer(true, 8400, -450, false) == Etat::Inconnu); // chargeur allume : on ne conclut pas
    return 0;
}

constexpr int identite_de_la_puce() {
    VERIFIER(kFabricantTi == 0x5449);
    VERIFIER(kPuceIna226 == 0x2260);
    VERIFIER(kRegTension == 0x02 && kRegCourant == 0x04 && kRegEtalonnage == 0x05 && kRegFabricant == 0xFE &&
             kRegPuce == 0xFF);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(etalonnage_du_tab5)                                                                                              \
    X(tension_du_bus)                                                                                                  \
    X(tension_du_shunt_et_courant_signes)                                                                              \
    X(presence_de_batterie)                                                                                            \
    X(classement_de_l_etat)                                                                                            \
    X(identite_de_la_puce)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
