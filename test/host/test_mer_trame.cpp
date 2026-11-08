// Tests du noyau pur « mer » : lecture des lignes SEA, ACT et CRIT de la trame de Vigie (mer_trame.hpp).
//
// Les valeurs viennent de SIX VRAIES trames reçues de l'instance publique de Vigie le 08/11/2026 vers 13 h 15 UTC, pour six spots de la
// côte atlantique et de la Manche (donnees/mer_<spot>.txt, capturées par test/host/capturer_mer.py ; leur CRC a été recalculé à part avec
// zlib). Les valeurs attendues sont lues PAR PYTHON SEUL, d'après le contrat et sans une ligne du noyau (donnees/attendu_mer_<spot>.inc,
// test/host/controle_python_mer.py) : le CRC, la ligne SEA, les quatre lignes ACT et leurs CRIT. Les noms du contrat (critères, états,
// unités, activités) sont écrits ici A LA MAIN, depuis le contrat, pour que l'erreur d'une table ne passe pas inaperçue d'un côté comme de
// l'autre. Les trames cassées exprès (CRC faux, ligne tronquée, balise inconnue…) sont dans test_mer_variantes.cpp.
//
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_trame.hpp"
#include "mer_donnees.hpp"
#include "mini.hpp"

using namespace mer_test;

namespace {

// Compare ce que le noyau lit d'une trame avec ce que Python en a lu. Rend 0, ou la ligne de la première différence.
constexpr int comparer(sv trame, const Att &a) {
    Mer m{};
    VERIFIER(decoder(trame, m));
    VERIFIER(m.valide);
    VERIFIER(sv(m.nom) == a.nom && sv(m.lieu) == a.lieu);
    VERIFIER(m.genere_s == a.genere && m.premiere_heure_s == a.premiere);
    VERIFIER(m.donnees_gardees == (a.gardees == 1));
    VERIFIER(m.vent_min_d == a.vent_min_d && m.vent_max_d == a.vent_max_d);
    VERIFIER(m.lever_s == a.lever && m.coucher_s == a.coucher && m.lever2_s == a.lever2 && m.coucher2_s == a.coucher2);
    VERIFIER(static_cast<int>(m.nb_marees) == a.nb_marees);
    for (int i = 0; i < a.nb_marees; ++i) {
        VERIFIER(m.marees[i].haute == (a.marees[i].haute == 1));
        VERIFIER(m.marees[i].instant_s == a.marees[i].instant);
        VERIFIER(m.marees[i].hauteur_cm == a.marees[i].cm);
    }
    VERIFIER(static_cast<int>(m.nb_coefficients) == a.nb_coefficients);
    for (int i = 0; i < a.nb_coefficients; ++i) {
        VERIFIER(m.coefficients[i].pleine_mer_s == a.coefficients[i].pleine_mer);
        VERIFIER(m.coefficients[i].valeur == a.coefficients[i].valeur);
    }
    VERIFIER(m.vigilances_lues == (a.vigilances_lues == 1));
    VERIFIER(m.vigilance_presente == (a.vigilance_presente == 1));
    VERIFIER(m.vigilance_niveau == niveau_de(a.niveau));
    VERIFIER(m.vigilance_fin_s == a.vigilance_fin);
    VERIFIER(sv(m.vigilance_source) == a.source);
    for (int i = 0; i < NB_ACTIVITES; ++i) {
        const AttAct &x = a.activites[i];
        const Prevision &p = m.activites[i];
        VERIFIER(activite_de_nom(x.nom) == static_cast<Activite>(i));
        VERIFIER(p.presente);
        VERIFIER(p.disponible == (x.disponible == 1));
        VERIFIER(p.verdict == verdict_de_nom(x.verdict));
        VERIFIER(p.raison_critere == critere_de_nom(x.raison_critere));
        VERIFIER(p.raison_etat == etat_de_nom(x.raison_etat));
        VERIFIER((p.raison_critere != Critere::Inconnu) == !x.raison_critere.empty());
        VERIFIER(p.creneau_debut_s == x.debut && p.creneau_fin_s == x.fin);
        VERIFIER(x.heures.size() == static_cast<std::size_t>(HEURES) &&
                 x.lumiere.size() == static_cast<std::size_t>(HEURES));
        for (int h = 0; h < HEURES; ++h) {
            VERIFIER(p.heures[h] == verdict_bande(x.heures[static_cast<std::size_t>(h)]));
            VERIFIER(p.lumiere[h] == lumiere_bande(x.lumiere[static_cast<std::size_t>(h)]));
        }
        VERIFIER(p.detail_s == x.detail);
        VERIFIER(p.points == x.points && p.sur == x.sur);
        VERIFIER(static_cast<int>(p.nb_criteres) == x.nb_criteres);
        for (int k = 0; k < x.nb_criteres; ++k) {
            const AttCrit &c = x.criteres[k];
            const Critere_ligne &l = p.criteres[k];
            VERIFIER(l.id == critere_de_nom(c.id) && l.id != Critere::Inconnu);
            VERIFIER(l.valeur_d == c.valeur_d);
            VERIFIER(l.unite == unite_de_nom(c.unite) && l.unite != Unite::Inconnue);
            VERIFIER(l.etat == etat_de_nom(c.etat));
            VERIFIER(l.verdict == verdict_de_nom(c.verdict));
            VERIFIER(l.points == c.points && l.points_max == c.points_max);
        }
    }
    return 0;
}

// ---- le CRC -----------------------------------------------------------------------------------------------------------------------
constexpr int le_crc32_est_celui_de_zlib() {
    VERIFIER(detail::crc32("123456789") == 0xCBF43926u); // la valeur de contrôle du CRC32 IEEE 802.3
    VERIFIER(detail::crc32("") == 0u);
    VERIFIER(detail::crc32("a") == 0xE8B7BE43u);
    return 0;
}

constexpr int le_crc_de_chaque_trame_reelle_est_celui_que_python_a_calcule() {
    const sv trames[] = {HOSSEGOR, LA_TORCHE, ROCHEBONNE, PORT_VIEUX, GOVELLE, ONDARRAITZ};
    const long long crc[] = {ATT_HOSSEGOR.crc,   ATT_LA_TORCHE.crc, ATT_ROCHEBONNE.crc,
                             ATT_PORT_VIEUX.crc, ATT_GOVELLE.crc,   ATT_ONDARRAITZ.crc};
    for (int i = 0; i < 6; ++i) {
        const sv t = trames[i];
        const std::size_t fin = position_end(t);
        VERIFIER(fin != sv::npos);
        VERIFIER(static_cast<long long>(detail::crc32(t.substr(0, fin))) == crc[i]);
        VERIFIER(detail::crc_valide(t));
    }
    return 0;
}

constexpr int un_octet_change_avant_la_ligne_end_invalide_le_crc() {
    // Une trame réelle de 5 Ko, dont on change UNE lettre du nom du spot (pas la ligne END) : plus un CRC valide. Fabriquée à la
    // compilation sans chaîne dynamique : on décode avec un décodeur de CRC qui lit la trame d'origine, puis on compare au CRC d'une
    // copie dont un octet diffère (construite à la main dans un tableau).
    char copie[6000] = {};
    const sv t = HOSSEGOR;
    VERIFIER(t.size() < sizeof(copie));
    for (std::size_t i = 0; i < t.size(); ++i)
        copie[i] = t[i];
    VERIFIER(detail::crc_valide(sv(copie, t.size())));
    const std::size_t sea = t.find("SEA|");
    VERIFIER(sea != sv::npos);
    copie[sea + 4] = copie[sea + 4] == 'P' ? 'Q' : 'P';
    VERIFIER(!detail::crc_valide(sv(copie, t.size())));
    return 0;
}

constexpr int une_trame_sans_ligne_end_ou_coupee_est_refusee() {
    const sv t = HOSSEGOR;
    const std::size_t fin = position_end(t);
    VERIFIER(!detail::crc_valide(t.substr(0, fin)));           // sans la ligne END
    VERIFIER(!detail::crc_valide(t.substr(0, fin + 7)));       // « END|CB507 » : 5 chiffres sur 8
    VERIFIER(!detail::crc_valide(t.substr(0, t.size() - 20))); // coupée en route
    VERIFIER(!detail::crc_valide(""));
    VERIFIER(!detail::crc_valide("END|"));
    return 0;
}

// ---- les six trames réelles ----------------------------------------------------------------------------------------------------
constexpr int hossegor_est_lue_comme_python_la_lit() { return comparer(HOSSEGOR, ATT_HOSSEGOR); }
constexpr int la_torche_est_lue_comme_python_la_lit() { return comparer(LA_TORCHE, ATT_LA_TORCHE); }
constexpr int rochebonne_est_lue_comme_python_la_lit() { return comparer(ROCHEBONNE, ATT_ROCHEBONNE); }
constexpr int port_vieux_est_lu_comme_python_le_lit() { return comparer(PORT_VIEUX, ATT_PORT_VIEUX); }
constexpr int govelle_est_lue_comme_python_la_lit() { return comparer(GOVELLE, ATT_GOVELLE); }
constexpr int ondarraitz_est_lue_comme_python_la_lit() { return comparer(ONDARRAITZ, ATT_ONDARRAITZ); }

constexpr int les_valeurs_de_hossegor_lues_a_la_main_dans_la_trame() {
    Mer m{};
    VERIFIER(decoder(HOSSEGOR, m));
    VERIFIER(sv(m.nom) == "Plage Sud (Point d'Or)" && sv(m.lieu) == "Soorts-Hossegor");
    VERIFIER(m.genere_s == 1794143738 && m.premiere_heure_s == 1794142800 && !m.donnees_gardees);
    VERIFIER(m.vent_min_d == 120 && m.vent_max_d == 250); // 12,0 et 25,0 nœuds en dixièmes
    VERIFIER(m.nb_marees == 6 && !m.marees[0].haute && m.marees[0].instant_s == 1794126780 &&
             m.marees[0].hauteur_cm == -166);
    VERIFIER(m.marees[1].haute && m.marees[1].instant_s == 1794149460 && m.marees[1].hauteur_cm == 171);
    VERIFIER(m.nb_coefficients == 3 && m.coefficients[0].pleine_mer_s == 1794150900 && m.coefficients[0].valeur == 83);
    VERIFIER(m.vigilances_lues && !m.vigilance_presente && m.vigilance_niveau == ecran::vigie::Niveau::Inconnu);
    const Prevision &surf = m.activites[static_cast<int>(Activite::Surf)];
    VERIFIER(surf.disponible && surf.verdict == Verdict::Deconseille);
    VERIFIER(surf.raison_critere == Critere::HauteurHoule && surf.raison_etat == Etat::TropPetite);
    VERIFIER(surf.creneau_debut_s == 0 && surf.creneau_fin_s == 0); // déconseillé : pas de créneau
    VERIFIER(surf.points == 4 && surf.sur == 8 && surf.nb_criteres == 5);
    VERIFIER(surf.criteres[0].id == Critere::HauteurHoule && surf.criteres[0].valeur_d == 4 &&
             surf.criteres[0].unite == Unite::Metres);
    VERIFIER(surf.criteres[0].etat == Etat::TropPetite && surf.criteres[0].verdict == Verdict::Deconseille);
    VERIFIER(surf.criteres[0].points == 0 && surf.criteres[0].points_max == 2);
    const Critere_ligne &maree =
        surf.criteres[4]; // `CRIT|surf|tide|1.0|m||||2` : une valeur, ni état, ni verdict, ni points
    VERIFIER(maree.id == Critere::Maree && maree.valeur_d == 10 && maree.unite == Unite::Metres);
    VERIFIER(maree.etat == Etat::Inconnu && maree.verdict == Verdict::Inconnu && maree.points == -1 &&
             maree.points_max == 2);
    const Prevision &kite = m.activites[static_cast<int>(Activite::Kite)];
    VERIFIER(kite.verdict == Verdict::Deconseille && kite.raison_critere == Critere::Vent &&
             kite.raison_etat == Etat::TropFaible);
    VERIFIER(kite.points == -1 && kite.sur == -1 && kite.nb_criteres == 3);
    VERIFIER(kite.criteres[0].unite == Unite::Noeuds && kite.criteres[0].valeur_d == 78);
    VERIFIER(kite.criteres[2].id == Critere::DirectionVent && kite.criteres[2].valeur_d == 1250 &&
             kite.criteres[2].unite == Unite::Degres);
    const Prevision &bateau = m.activites[static_cast<int>(Activite::Bateau)];
    VERIFIER(bateau.verdict == Verdict::Bon && bateau.raison_critere == Critere::Inconnu &&
             bateau.raison_etat == Etat::Inconnu);
    VERIFIER(bateau.creneau_debut_s == 1794207600 && bateau.creneau_fin_s == 1794229200);
    VERIFIER(bateau.criteres[0].unite == Unite::Beaufort && bateau.criteres[0].etat == Etat::JusquaModere);
    const Prevision &pied = m.activites[static_cast<int>(Activite::PecheAPied)];
    VERIFIER(pied.verdict == Verdict::Bon && pied.creneau_debut_s == 1794207600 && pied.creneau_fin_s == 1794214800);
    VERIFIER(pied.heures[6] == Verdict::Bon && pied.heures[7] == Verdict::Bon &&
             pied.heures[5] == Verdict::Deconseille);
    VERIFIER(pied.criteres[0].id == Critere::BasseMer && pied.criteres[0].unite == Unite::Minutes &&
             pied.criteres[0].valeur_d == 1000);
    VERIFIER(pied.criteres[1].id == Critere::Coefficient && pied.criteres[1].unite == Unite::Aucune &&
             pied.criteres[1].valeur_d == 840);
    VERIFIER(pied.criteres[1].etat == Etat::Grand);
    return 0;
}

constexpr int le_bloc_mer_est_ignore_par_les_autres_lignes_de_la_trame() {
    // La trame réelle porte aussi HEAD, ZONE, RAIN5M, DAY et HAZ : le décodeur de la mer les saute et ne les confond avec rien.
    Mer m{};
    VERIFIER(HOSSEGOR.find("HEAD|") == 0 && HOSSEGOR.find("\nZONE|") != sv::npos &&
             HOSSEGOR.find("\nDAY|") != sv::npos);
    VERIFIER(decoder(HOSSEGOR, m) && m.valide);
    return 0;
}

// ---- les noms du contrat, écrits à la main ---------------------------------------------------------------------------------------
constexpr int les_activites_les_verdicts_et_les_unites_ont_les_noms_du_contrat() {
    VERIFIER(activite_de_nom("surf") == Activite::Surf && activite_de_nom("kite") == Activite::Kite);
    VERIFIER(activite_de_nom("boat") == Activite::Bateau && activite_de_nom("shore_fishing") == Activite::PecheAPied);
    VERIFIER(activite_de_nom("") == Activite::Nombre && activite_de_nom("Surf") == Activite::Nombre &&
             activite_de_nom("wing") == Activite::Nombre);
    VERIFIER(verdict_de_nom("good") == Verdict::Bon && verdict_de_nom("possible") == Verdict::Possible);
    VERIFIER(verdict_de_nom("poor") == Verdict::Deconseille);
    VERIFIER(verdict_de_nom("") == Verdict::Inconnu && verdict_de_nom("Good") == Verdict::Inconnu &&
             verdict_de_nom("bad") == Verdict::Inconnu);
    VERIFIER(unite_de_nom("") == Unite::Aucune && unite_de_nom("m") == Unite::Metres &&
             unite_de_nom("s") == Unite::Secondes);
    VERIFIER(unite_de_nom("deg") == Unite::Degres && unite_de_nom("km/h") == Unite::KmH &&
             unite_de_nom("kn") == Unite::Noeuds);
    VERIFIER(unite_de_nom("Beaufort") == Unite::Beaufort && unite_de_nom("min") == Unite::Minutes);
    VERIFIER(unite_de_nom("furlong") == Unite::Inconnue && unite_de_nom("beaufort") == Unite::Inconnue);
    return 0;
}

constexpr int les_criteres_ont_les_noms_du_contrat() {
    VERIFIER(critere_de_nom("swell_height") == Critere::HauteurHoule &&
             critere_de_nom("swell_period") == Critere::PeriodeHoule);
    VERIFIER(critere_de_nom("swell_direction") == Critere::DirectionHoule && critere_de_nom("wind") == Critere::Vent);
    VERIFIER(critere_de_nom("tide") == Critere::Maree && critere_de_nom("gust_gap") == Critere::EcartRafales);
    VERIFIER(critere_de_nom("wind_direction") == Critere::DirectionVent &&
             critere_de_nom("wind_force") == Critere::ForceVent);
    VERIFIER(critere_de_nom("gust_force") == Critere::ForceRafales &&
             critere_de_nom("sea_height") == Critere::HauteurMer);
    VERIFIER(critere_de_nom("low_water") == Critere::BasseMer && critere_de_nom("coefficient") == Critere::Coefficient);
    VERIFIER(critere_de_nom("vigilance") == Critere::Vigilance);
    VERIFIER(critere_de_nom("") == Critere::Inconnu && critere_de_nom("swell") == Critere::Inconnu &&
             critere_de_nom("Wind") == Critere::Inconnu);
    return 0;
}

constexpr int les_etats_ont_les_noms_du_contrat() {
    VERIFIER(etat_de_nom("too_small") == Etat::TropPetite && etat_de_nom("small") == Etat::Petite &&
             etat_de_nom("right") == Etat::Bonne);
    VERIFIER(etat_de_nom("big") == Etat::Grosse && etat_de_nom("too_big") == Etat::TropGrosse);
    VERIFIER(etat_de_nom("short") == Etat::Courte && etat_de_nom("medium") == Etat::Moyenne &&
             etat_de_nom("long") == Etat::Longue);
    VERIFIER(etat_de_nom("facing") == Etat::DeFace && etat_de_nom("oblique") == Etat::Oblique &&
             etat_de_nom("off_axis") == Etat::HorsAxe);
    VERIFIER(etat_de_nom("light") == Etat::Faible && etat_de_nom("offshore") == Etat::Offshore &&
             etat_de_nom("cross_shore") == Etat::Travers);
    VERIFIER(etat_de_nom("onshore") == Etat::Onshore && etat_de_nom("strong") == Etat::Forte);
    VERIFIER(etat_de_nom("right_stage") == Etat::BonMoment && etat_de_nom("wrong_stage") == Etat::MauvaisMoment);
    VERIFIER(etat_de_nom("in_range") == Etat::DansLaPlage && etat_de_nom("a_bit_light") == Etat::UnPeuFaible);
    VERIFIER(etat_de_nom("a_bit_strong") == Etat::UnPeuForte && etat_de_nom("too_light") == Etat::TropFaible);
    VERIFIER(etat_de_nom("too_strong") == Etat::TropFort);
    VERIFIER(etat_de_nom("steady") == Etat::Regulier && etat_de_nom("gusty") == Etat::Rafaleux &&
             etat_de_nom("very_gusty") == Etat::TresRafaleux);
    VERIFIER(etat_de_nom("up_to_moderate") == Etat::JusquaModere && etat_de_nom("fresh") == Etat::Frais);
    VERIFIER(etat_de_nom("strong_or_more") == Etat::FortOuPlus && etat_de_nom("up_to_fresh") == Etat::JusquaFrais);
    VERIFIER(etat_de_nom("gale_or_more") == Etat::CoupDeVentOuPlus &&
             etat_de_nom("near_gale_or_more") == Etat::QuasiCoupDeVentOuPlus);
    VERIFIER(etat_de_nom("low") == Etat::Basse && etat_de_nom("moderate") == Etat::Modere &&
             etat_de_nom("high") == Etat::Haute);
    VERIFIER(etat_de_nom("in_window") == Etat::DansLaFenetre && etat_de_nom("outside_window") == Etat::HorsFenetre);
    VERIFIER(etat_de_nom("large") == Etat::Grand && etat_de_nom("very_large") == Etat::TresGrand);
    VERIFIER(etat_de_nom("orange") == Etat::Orange && etat_de_nom("red") == Etat::Rouge);
    VERIFIER(etat_de_nom("") == Etat::Inconnu && etat_de_nom("Orange") == Etat::Inconnu &&
             etat_de_nom("green") == Etat::Inconnu);
    return 0;
}

constexpr int la_raison_est_un_critere_un_point_et_un_etat() {
    Critere c = Critere::HauteurHoule;
    Etat e = Etat::Bonne;
    detail::lire_raison("swell_height.too_small", c, e);
    VERIFIER(c == Critere::HauteurHoule && e == Etat::TropPetite);
    detail::lire_raison("vigilance.orange", c, e);
    VERIFIER(c == Critere::Vigilance && e == Etat::Orange);
    // un nom inconnu d'un côté ou de l'autre, pas de point, ou vide : rien (jamais un critère sans son état)
    detail::lire_raison("swell_height.enorme", c, e);
    VERIFIER(c == Critere::Inconnu && e == Etat::Inconnu);
    detail::lire_raison("houle.too_small", c, e);
    VERIFIER(c == Critere::Inconnu && e == Etat::Inconnu);
    detail::lire_raison("swell_height", c, e);
    VERIFIER(c == Critere::Inconnu && e == Etat::Inconnu);
    detail::lire_raison("", c, e);
    VERIFIER(c == Critere::Inconnu && e == Etat::Inconnu);
    return 0;
}

// ---- lecture des champs ------------------------------------------------------------------------------------------------------------
constexpr int les_decimales_sont_lues_sur_le_texte_et_arrondies_loin_de_zero() {
    std::int32_t v = 99;
    VERIFIER(detail::lire_fixe("1.71", 2, v) && v == 171);
    VERIFIER(detail::lire_fixe("-1.66", 2, v) && v == -166);
    VERIFIER(detail::lire_fixe("0.4", 1, v) && v == 4);
    VERIFIER(detail::lire_fixe("12", 1, v) && v == 120); // un entier : complété
    VERIFIER(detail::lire_fixe("12.0", 1, v) && v == 120);
    VERIFIER(detail::lire_fixe("1.6", 2, v) && v == 160); // moins de décimales que demandé : complété
    VERIFIER(detail::lire_fixe("-0", 1, v) && v == 0);
    VERIFIER(detail::lire_fixe("0.04", 1, v) && v == 0); // 0,04 -> 0,0
    VERIFIER(detail::lire_fixe("0.05", 1, v) && v == 1); // la moitié s'éloigne de zéro
    VERIFIER(detail::lire_fixe("-0.05", 1, v) && v == -1);
    VERIFIER(detail::lire_fixe("0.049", 1, v) &&
             v == 0); // le premier chiffre perdu décide : 4 -> on arrondit vers le bas
    VERIFIER(detail::lire_fixe("1.25", 1, v) && v == 13);
    VERIFIER(detail::lire_fixe("2.349", 2, v) && v == 235);
    VERIFIER(detail::lire_fixe("999999", 1, v) && v == 9999990);
    VERIFIER(detail::lire_fixe("1.5", 0, v) && v == 2); // sans décimale gardée
    return 0;
}

constexpr int un_nombre_illisible_est_refuse_et_laisse_la_sortie_intacte() {
    std::int32_t v = 77;
    const sv mauvais[] = {"",    "-",  ".5", "5.",  "1,5",     "1e3",  "+1", "1.2.3",
                          "abc", "1 ", " 1", "--1", "1234567", "0x10", "1.a"};
    for (const sv t : mauvais) {
        VERIFIER(!detail::lire_fixe(t, 1, v));
        VERIFIER(v == 77);
    }
    return 0;
}

constexpr int un_instant_est_un_entier_sur_32_bits_sinon_zero() {
    VERIFIER(detail::lire_instant("1794143738") == 1794143738LL);
    VERIFIER(detail::lire_instant("4294967295") == 4294967295LL);
    VERIFIER(detail::lire_instant("4294967296") == 0); // au-delà de 32 bits : inconnu
    VERIFIER(detail::lire_instant("") == 0 && detail::lire_instant("12a") == 0 && detail::lire_instant("-5") == 0);
    VERIFIER(detail::lire_instant("1.5") == 0 && detail::lire_instant(" 5") == 0);
    VERIFIER(detail::lire_instant("0") == 0);
    return 0;
}

constexpr int les_points_vont_de_0_a_100_sinon_inconnus() {
    VERIFIER(detail::lire_points("0") == 0 && detail::lire_points("4") == 4 && detail::lire_points("100") == 100);
    VERIFIER(detail::lire_points("101") == -1 && detail::lire_points("") == -1 && detail::lire_points("abc") == -1);
    VERIFIER(detail::lire_points("00") == -1 && detail::lire_points("-1") == -1 && detail::lire_points("4.0") == -1);
    return 0;
}

constexpr int une_ligne_est_coupee_en_gardant_les_champs_vides() {
    detail::Champs c;
    detail::decouper("CRIT|surf|tide|1.0|m||||2", '|', c);
    VERIFIER(c.n == 9);
    VERIFIER(c.v[0] == "CRIT" && c.v[3] == "1.0" && c.v[5].empty() && c.v[6].empty() && c.v[7].empty() &&
             c.v[8] == "2");
    detail::decouper("", '|', c);
    VERIFIER(c.n == 1 && c.v[0].empty());
    detail::decouper("a|", '|', c);
    VERIFIER(c.n == 2 && c.v[0] == "a" && c.v[1].empty());
    detail::decouper("|", '|', c);
    VERIFIER(c.n == 2 && c.v[0].empty() && c.v[1].empty());
    // au-delà de CHAMPS_MAX, le reste est laissé
    char beaucoup[80] = {};
    for (int i = 0; i < 79; ++i)
        beaucoup[i] = i % 2 == 0 ? 'x' : '|'; // x|x|x|… : 40 champs
    detail::decouper(sv(beaucoup, 79), '|', c);
    VERIFIER(c.n == CHAMPS_MAX);
    return 0;
}

constexpr int un_texte_trop_long_est_coupe_sur_une_frontiere_de_caractere() {
    char d[4] = {};
    detail::copier_texte("abc", d); // 3 caractères tiennent avec le zéro final
    VERIFIER(sv(d) == "abc");
    detail::copier_texte("abcdef", d);
    VERIFIER(sv(d) == "abc");
    detail::copier_texte("ab\xC3\xA9", d); // « abé » : é (2 octets) ne tient pas en entier -> « ab »
    VERIFIER(sv(d) == "ab");
    detail::copier_texte("a\xC3\xA9", d); // « aé » tient pile (3 octets)
    VERIFIER(sv(d) == "a\xC3\xA9");
    detail::copier_texte("", d);
    VERIFIER(sv(d).empty());
    char e[6] = {};
    detail::copier_texte("ab\xE2\x82\xAC", e); // « ab€ » : 5 octets tiennent
    VERIFIER(sv(e) == "ab\xE2\x82\xAC");
    detail::copier_texte("abc\xE2\x82\xAC", e); // « abc€ » : 6 octets, le € coupé -> « abc »
    VERIFIER(sv(e) == "abc");
    return 0;
}

constexpr int un_identifiant_de_spot_est_un_slug() {
    VERIFIER(spot_valide("soorts-hossegor-plage-sud-point-d-or") && spot_valide("a") && spot_valide("a_b-9"));
    VERIFIER(!spot_valide("") && !spot_valide("A") && !spot_valide("a b") && !spot_valide("a/b") &&
             !spot_valide("a?b=1"));
    VERIFIER(!spot_valide("é") && !spot_valide("a.b"));
    // les quatre bornes des intervalles permises, et leurs voisins juste dehors (un `<=` devenu `<` se verrait ici)
    VERIFIER(spot_valide("az09-_") && spot_valide("z") && spot_valide("9") && spot_valide("a") && spot_valide("0"));
    VERIFIER(!spot_valide("`") && !spot_valide("{") && !spot_valide("/") && !spot_valide(":") && !spot_valide("@") &&
             !spot_valide("Z"));
    char longue[66] = {};
    for (int i = 0; i < 65; ++i)
        longue[i] = 'a';
    VERIFIER(spot_valide(sv(longue, 64)));  // 64 caractères : la limite de l'API
    VERIFIER(!spot_valide(sv(longue, 65))); // 65 : refusé
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(le_crc32_est_celui_de_zlib)                                                                                      \
    X(le_crc_de_chaque_trame_reelle_est_celui_que_python_a_calcule)                                                    \
    X(un_octet_change_avant_la_ligne_end_invalide_le_crc)                                                              \
    X(une_trame_sans_ligne_end_ou_coupee_est_refusee)                                                                  \
    X(hossegor_est_lue_comme_python_la_lit)                                                                            \
    X(la_torche_est_lue_comme_python_la_lit)                                                                           \
    X(rochebonne_est_lue_comme_python_la_lit)                                                                          \
    X(port_vieux_est_lu_comme_python_le_lit)                                                                           \
    X(govelle_est_lue_comme_python_la_lit)                                                                             \
    X(ondarraitz_est_lue_comme_python_la_lit)                                                                          \
    X(les_valeurs_de_hossegor_lues_a_la_main_dans_la_trame)                                                            \
    X(le_bloc_mer_est_ignore_par_les_autres_lignes_de_la_trame)                                                        \
    X(les_activites_les_verdicts_et_les_unites_ont_les_noms_du_contrat)                                                \
    X(les_criteres_ont_les_noms_du_contrat)                                                                            \
    X(les_etats_ont_les_noms_du_contrat)                                                                               \
    X(la_raison_est_un_critere_un_point_et_un_etat)                                                                    \
    X(les_decimales_sont_lues_sur_le_texte_et_arrondies_loin_de_zero)                                                  \
    X(un_nombre_illisible_est_refuse_et_laisse_la_sortie_intacte)                                                      \
    X(un_instant_est_un_entier_sur_32_bits_sinon_zero)                                                                 \
    X(les_points_vont_de_0_a_100_sinon_inconnus)                                                                       \
    X(une_ligne_est_coupee_en_gardant_les_champs_vides)                                                                \
    X(un_texte_trop_long_est_coupe_sur_une_frontiere_de_caractere)                                                     \
    X(un_identifiant_de_spot_est_un_slug)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
