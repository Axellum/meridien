// Tests du noyau pur : le début et la fin d'une alerte officielle de Vigie s'écrivent à l'heure LOCALE DU LIEU, pas à celle de Paris
// (ADR-0012). test_vigie_textes.cpp ne vérifie que le format et un seul cas, à Paris : ici, les mêmes textes pour huit lieux et treize
// instants (de part et d'autre d'un minuit local, d'un changement d'heure, du nouvel an), attendus par Python zoneinfo
// (donnees/attendu_heure_lieu.inc, section VIGIE).
//
// Le chemin est celui de l'appareil : l'instant UTC de l'alerte -> les règles du fuseau de la zone enregistrée (analyser_tz, comme
// main/horloge.cpp : horloge_heure_locale) -> vigie::formater_instant. Le branchement sur la zone (main/ui_alertes.cpp ne connaît que
// horloge_heure_locale) est vérifié par test_horloge.cpp.
#include <cstddef>
#include <string_view>

#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/i18n.hpp"
#include "ecran/vigie_textes.hpp"
#include "mini.hpp"

using namespace ecran;
using sv = std::string_view;

namespace {

constexpr sv ZONES[] = {
#define ATTENDU_LIEU_ZONES
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_ZONES
};

struct Ligne {
    int zone;
    long long instant;
    sv francais, anglais;
};
constexpr Ligne LIGNES[] = {
#define ATTENDU_LIEU_VIGIE
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_VIGIE
};
constexpr std::size_t NOMBRE_LIGNES = sizeof(LIGNES) / sizeof(LIGNES[0]);

constexpr bool egal(const char *a, sv b) { return sv(a) == b; }

constexpr int les_lignes_python_sont_la() {
    VERIFIER(NOMBRE_LIGNES == 104);
    return 0;
}

constexpr int le_debut_d_une_alerte_s_ecrit_a_l_heure_du_lieu_selon_zoneinfo() {
    for (const Ligne &l : LIGNES) {
        const heure::AnalyseTz a = heure::analyser_tz(fuseaux::posix_pour(ZONES[static_cast<std::size_t>(l.zone)]));
        VERIFIER(a.ok());
        const heure::HeureLocale h = heure::en_heure_locale(l.instant, a.fuseau);
        char t[64] = {};
        VERIFIER(vigie::formater_instant(i18n::Langue::Fr, h, t, sizeof t) > 0);
        VERIFIER(egal(t, l.francais));
        VERIFIER(vigie::formater_instant(i18n::Langue::En, h, t, sizeof t) > 0);
        VERIFIER(egal(t, l.anglais));
    }
    return 0;
}

constexpr int le_meme_instant_s_ecrit_autrement_selon_le_lieu() {
    // Le même instant UTC : le vendredi soir à Paris, le samedi matin à Tokyo. Ce que l'écran ne doit jamais faire : écrire l'heure et
    // le jour de Paris pour une alerte d'un autre lieu.
    sv paris, tokyo;
    long long instant = 0;
    for (const Ligne &l : LIGNES) {
        if (ZONES[static_cast<std::size_t>(l.zone)] == "Europe/Paris" && l.instant == 1793997000LL) {
            paris = l.francais;
            instant = l.instant;
        }
        if (ZONES[static_cast<std::size_t>(l.zone)] == "Asia/Tokyo" && l.instant == 1793997000LL) {
            tokyo = l.francais;
        }
    }
    VERIFIER(instant != 0 && !paris.empty() && !tokyo.empty());
    VERIFIER(paris != tokyo); // les deux textes viennent de Python : l'heure ET le jour diffèrent entre les deux lieux
    VERIFIER(paris.substr(0, 5) != tokyo.substr(0, 5));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_lignes_python_sont_la)                                                                                       \
    X(le_debut_d_une_alerte_s_ecrit_a_l_heure_du_lieu_selon_zoneinfo)                                                  \
    X(le_meme_instant_s_ecrit_autrement_selon_le_lieu)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
