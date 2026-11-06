// Tests du noyau pur : table météo WMO -> libellé français + icône jour/nuit.
#include "ecran/meteo_wmo.hpp"
#include "mini.hpp"

using ecran::meteo::Condition;
using ecran::meteo::decrire;
using ecran::meteo::Icone;
using ecran::meteo::nom_icone;

namespace {

// Les 29 codes du tableau « WMO Weather interpretation codes (WW) » de https://open-meteo.com/en/docs,
// lu le 06/11/2026 (copie indépendante de la table du noyau, pour que le test puisse la contredire).
constexpr int CODES_OPEN_METEO[] = {0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66,
                                    67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 97, 99};
constexpr std::size_t NOMBRE_CODES = sizeof(CODES_OPEN_METEO) / sizeof(CODES_OPEN_METEO[0]);

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

// Nombre de caractères (et non d'octets) d'un texte UTF-8.
constexpr std::size_t caracteres(std::string_view s) {
    std::size_t n = 0;
    for (char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++n;
    }
    return n;
}

constexpr int le_tableau_a_29_codes() {
    VERIFIER(NOMBRE_CODES == 29);
    return 0;
}

constexpr int tous_les_codes_sont_connus_jour_et_nuit() {
    for (int code : CODES_OPEN_METEO) {
        for (bool jour : {true, false}) {
            const Condition c = decrire(code, jour);
            VERIFIER(c.connu);
            VERIFIER(!c.libelle.empty());
            VERIFIER(c.icone != Icone::Inconnue);
        }
    }
    return 0;
}

constexpr int exactement_ces_29_codes_sont_connus() {
    // Parcourt tout l'intervalle plausible : un code de plus ou de moins dans la table est vu.
    int connus = 0;
    for (int code = -10; code <= 200; ++code) {
        if (decrire(code, true).connu) {
            bool dans_la_source = false;
            for (int c : CODES_OPEN_METEO) dans_la_source = dans_la_source || (c == code);
            VERIFIER(dans_la_source);
            ++connus;
        }
    }
    VERIFIER(connus == 29);
    return 0;
}

constexpr int codes_inconnus_donnent_meteo_inconnue() {
    for (int code : {-1, 4, 10, 44, 46, 50, 58, 60, 98, 100, 255, 2147483647, -2147483647}) {
        const Condition c = decrire(code, true);
        VERIFIER(!c.connu);
        VERIFIER(egal(c.libelle, "Météo inconnue"));
        VERIFIER(c.icone == Icone::Inconnue);
    }
    return 0;
}

constexpr int libelles_francais_exacts_avec_accents() {
    struct Attendu {
        int code;
        std::string_view libelle;
    };
    constexpr Attendu ATTENDUS[] = {
        {0, "Ciel dégagé"},
        {1, "Plutôt dégagé"},
        {2, "Partiellement nuageux"},
        {3, "Couvert"},
        {45, "Brouillard"},
        {48, "Brouillard givrant"},
        {51, "Bruine légère"},
        {53, "Bruine modérée"},
        {55, "Bruine dense"},
        {56, "Bruine verglaçante légère"},
        {57, "Bruine verglaçante dense"},
        {61, "Pluie faible"},
        {63, "Pluie modérée"},
        {65, "Pluie forte"},
        {66, "Pluie verglaçante légère"},
        {67, "Pluie verglaçante forte"},
        {71, "Neige faible"},
        {73, "Neige modérée"},
        {75, "Neige forte"},
        {77, "Grains de neige"},
        {80, "Averses faibles"},
        {81, "Averses modérées"},
        {82, "Averses violentes"},
        {85, "Averses de neige faibles"},
        {86, "Averses de neige fortes"},
        {95, "Orage"},
        {96, "Orage avec petite grêle"},
        {97, "Orage violent"},
        {99, "Orage avec forte grêle"},
    };
    VERIFIER(sizeof(ATTENDUS) / sizeof(ATTENDUS[0]) == NOMBRE_CODES);
    for (const Attendu& a : ATTENDUS) {
        VERIFIER(egal(decrire(a.code, true).libelle, a.libelle));
        VERIFIER(egal(decrire(a.code, false).libelle, a.libelle));  // le libellé ne dépend pas du jour
    }
    return 0;
}

constexpr int libelles_courts_distincts_et_sans_chiffre() {
    for (std::size_t i = 0; i < NOMBRE_CODES; ++i) {
        const std::string_view l = decrire(CODES_OPEN_METEO[i], true).libelle;
        VERIFIER(caracteres(l) <= 26);                  // tient sur une ligne d'écran
        VERIFIER(l.front() >= 'A' && l.front() <= 'Z');  // commence par une majuscule
        for (char c : l) VERIFIER(!(c >= '0' && c <= '9'));  // jamais un code à l'écran
        for (std::size_t j = i + 1; j < NOMBRE_CODES; ++j) {
            VERIFIER(!egal(l, decrire(CODES_OPEN_METEO[j], true).libelle));
        }
    }
    return 0;
}

constexpr int seuls_les_codes_0_a_2_changent_entre_jour_et_nuit() {
    VERIFIER(decrire(0, true).icone == Icone::Soleil);
    VERIFIER(decrire(0, false).icone == Icone::Lune);
    VERIFIER(decrire(1, true).icone == Icone::PeuNuageuxJour);
    VERIFIER(decrire(1, false).icone == Icone::PeuNuageuxNuit);
    VERIFIER(decrire(2, true).icone == Icone::NuageuxJour);
    VERIFIER(decrire(2, false).icone == Icone::NuageuxNuit);
    for (int code : CODES_OPEN_METEO) {
        if (code > 2) VERIFIER(decrire(code, true).icone == decrire(code, false).icone);
    }
    return 0;
}

constexpr int regroupements_d_icones() {
    struct Groupe {
        Icone icone;
        int codes[4];
        int n;
    };
    constexpr Groupe GROUPES[] = {
        {Icone::Couvert, {3}, 1},
        {Icone::Brouillard, {45, 48}, 2},
        {Icone::Bruine, {51, 53, 55}, 3},
        {Icone::PluieVerglacante, {56, 57, 66, 67}, 4},
        {Icone::Pluie, {61, 63}, 2},
        {Icone::PluieForte, {65}, 1},
        {Icone::Neige, {71, 73}, 2},
        {Icone::NeigeForte, {75}, 1},
        {Icone::GrainsDeNeige, {77}, 1},
        {Icone::Averses, {80, 81}, 2},
        {Icone::AversesViolentes, {82}, 1},
        {Icone::AversesDeNeige, {85, 86}, 2},
        {Icone::Orage, {95, 97}, 2},
        {Icone::OrageGrele, {96, 99}, 2},
    };
    for (const Groupe& g : GROUPES) {
        for (int i = 0; i < g.n; ++i) VERIFIER(decrire(g.codes[i], true).icone == g.icone);
    }
    return 0;
}

constexpr int chaque_icone_a_un_identifiant_ascii_unique() {
    constexpr int N = static_cast<int>(Icone::Nombre);
    VERIFIER(N == 21);
    for (int i = 0; i < N; ++i) {
        const std::string_view a = nom_icone(static_cast<Icone>(i));
        VERIFIER(!a.empty());
        for (char c : a) VERIFIER((c >= 'a' && c <= 'z') || c == '_');
        for (int j = i + 1; j < N; ++j) VERIFIER(!egal(a, nom_icone(static_cast<Icone>(j))));
    }
    return 0;
}

constexpr int toute_icone_rendue_a_un_code_source() {
    // Aucune icône du jeu n'est inutilisée (hors « Inconnue », réservée aux codes absents).
    for (int i = 1; i < static_cast<int>(Icone::Nombre); ++i) {
        bool utilisee = false;
        for (int code : CODES_OPEN_METEO) {
            for (bool jour : {true, false}) utilisee = utilisee || (decrire(code, jour).icone == static_cast<Icone>(i));
        }
        VERIFIER(utilisee);
    }
    return 0;
}

}  // namespace

#define LISTE(X)                                       \
    X(le_tableau_a_29_codes)                           \
    X(tous_les_codes_sont_connus_jour_et_nuit)         \
    X(exactement_ces_29_codes_sont_connus)             \
    X(codes_inconnus_donnent_meteo_inconnue)           \
    X(libelles_francais_exacts_avec_accents)           \
    X(libelles_courts_distincts_et_sans_chiffre)       \
    X(seuls_les_codes_0_a_2_changent_entre_jour_et_nuit) \
    X(regroupements_d_icones)                          \
    X(chaque_icone_a_un_identifiant_ascii_unique)      \
    X(toute_icone_rendue_a_un_code_source)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
