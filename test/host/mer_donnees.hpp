// Données communes aux tests de la mer (test_mer_trame.cpp, test_mer_vue.cpp, test_mer_variantes.cpp) : les six trames réelles de Vigie
// (donnees/mer_<spot>.inc, capturées par capturer_mer.py) et ce que Python en a lu (donnees/attendu_mer_<spot>.inc, écrits par
// controle_python_mer.py, qui décrit aussi l'ordre des champs de `Att`). Rien de ce fichier n'appelle le noyau, sauf pour nommer ses
// constantes (INCONNU, MAREES_MAX…).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_trame.hpp"

namespace mer_test {

using namespace ecran::mer;
using sv = std::string_view;

inline constexpr sv HOSSEGOR =
#include "donnees/mer_hossegor.inc"
    ;
inline constexpr sv LA_TORCHE =
#include "donnees/mer_la_torche.inc"
    ;
inline constexpr sv ROCHEBONNE =
#include "donnees/mer_rochebonne.inc"
    ;
inline constexpr sv PORT_VIEUX =
#include "donnees/mer_port_vieux.inc"
    ;
inline constexpr sv GOVELLE =
#include "donnees/mer_govelle.inc"
    ;
inline constexpr sv ONDARRAITZ =
#include "donnees/mer_ondarraitz.inc"
    ;

struct AttCrit {
    sv id;
    long long valeur_d;
    sv unite, etat, verdict;
    int points, points_max;
};
struct AttAct {
    sv nom;
    int disponible;
    sv verdict, raison_critere, raison_etat;
    long long debut, fin;
    sv heures, lumiere;
    long long detail;
    int points, sur, nb_criteres;
    AttCrit criteres[CRITERES_MAX];
};
struct AttMaree {
    int haute;
    long long instant, cm;
};
struct AttCoefficient {
    long long pleine_mer;
    int valeur;
};
struct AttPoint {
    long long maintenant;
    int heure, fraicheur, meilleure, prochaine_maree, coefficient;
};
struct AttHauteur {
    long long instant, milli_cm;
};
struct Att {
    long long crc;
    sv nom, lieu;
    long long genere, premiere;
    int gardees;
    long long vent_min_d, vent_max_d;
    long long lever, coucher, lever2, coucher2;
    int nb_marees;
    AttMaree marees[MAREES_MAX];
    int nb_coefficients;
    AttCoefficient coefficients[COEFFICIENTS_MAX];
    int vigilances_lues, vigilance_presente;
    sv niveau;
    long long vigilance_fin;
    sv source;
    AttAct activites[NB_ACTIVITES];
    int nb_points;
    AttPoint points[48];
    int nb_hauteurs;
    AttHauteur hauteurs[64];
    int nb_hors;
    long long hors[8];
};

inline constexpr Att ATT_HOSSEGOR =
#include "donnees/attendu_mer_hossegor.inc"
    ;
inline constexpr Att ATT_LA_TORCHE =
#include "donnees/attendu_mer_la_torche.inc"
    ;
inline constexpr Att ATT_ROCHEBONNE =
#include "donnees/attendu_mer_rochebonne.inc"
    ;
inline constexpr Att ATT_PORT_VIEUX =
#include "donnees/attendu_mer_port_vieux.inc"
    ;
inline constexpr Att ATT_GOVELLE =
#include "donnees/attendu_mer_govelle.inc"
    ;
inline constexpr Att ATT_ONDARRAITZ =
#include "donnees/attendu_mer_ondarraitz.inc"
    ;

struct Spot {
    sv nom;
    sv trame;
    const Att &attendu;
};
inline constexpr Spot SPOTS[] = {
    {"hossegor", HOSSEGOR, ATT_HOSSEGOR},       {"la_torche", LA_TORCHE, ATT_LA_TORCHE},
    {"rochebonne", ROCHEBONNE, ATT_ROCHEBONNE}, {"port_vieux", PORT_VIEUX, ATT_PORT_VIEUX},
    {"govelle", GOVELLE, ATT_GOVELLE},          {"ondarraitz", ONDARRAITZ, ATT_ONDARRAITZ},
};
inline constexpr int NB_SPOTS = 6;

// ---- ce que les tests lisent à part du noyau (tables écrites à la main) -----------------------------------------------------------
constexpr Verdict verdict_bande(char c) noexcept {
    return c == 'G' ? Verdict::Bon : c == 'P' ? Verdict::Possible : c == 'X' ? Verdict::Deconseille : Verdict::Inconnu;
}
constexpr Lumiere lumiere_bande(char c) noexcept {
    return c == '1' ? Lumiere::Jour : c == '0' ? Lumiere::Nuit : Lumiere::Inconnue;
}
constexpr ecran::vigie::Niveau niveau_de(sv s) noexcept {
    using N = ecran::vigie::Niveau;
    return s == "GREEN"    ? N::Vert
           : s == "YELLOW" ? N::Jaune
           : s == "ORANGE" ? N::Orange
           : s == "RED"    ? N::Rouge
                           : N::Inconnu;
}

// Position de la ligne END d'une trame (sv::npos s'il n'y en a pas).
constexpr std::size_t position_end(sv t) noexcept { return t.rfind("END|"); }

} // namespace mer_test
