// Tests du noyau pur « mer » : les cas limites de la lecture de la trame (mer_trame.hpp), obtenus en CASSANT UNE SEULE CHOSE dans une vraie
// trame de Vigie (celle de Hossegor, donnees/mer_hossegor.txt) puis en la re-signant (le CRC32 du contrat, recalculé ici sur les octets
// avant la ligne END). Le calcul du CRC est lui-même prouvé sur les six trames réelles dans test_mer_trame.cpp, contre zlib.
//
// Ce fichier travaille sur des `std::string` : il n'est évalué qu'à l'exécution (pas de static_assert).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>

#include "ecran/mer_trame.hpp"
#include "ecran/mer_vue.hpp"
#include "mer_donnees.hpp"
#include "mini.hpp"

using namespace mer_test;
using std::string;

namespace {

string sans_end(sv t) { return string(t.substr(0, position_end(t))); }

string signer(const string &corps) {
    char fin[16];
    std::snprintf(fin, sizeof fin, "END|%08X\n", static_cast<unsigned>(detail::crc32(corps)));
    return corps + fin;
}

// Remplace UNE occurrence exacte de `ancien` ; faux si le texte n'existe pas exactement une fois (le test ne se trompe pas de cible).
bool remplacer(string &t, sv ancien, sv nouveau) {
    const std::size_t p = t.find(ancien);
    if (p == string::npos || t.find(ancien, p + 1) != string::npos)
        return false;
    t.replace(p, ancien.size(), nouveau);
    return true;
}

// La trame de Hossegor avec une chose changée, re-signée.
bool variante(string &sortie, sv ancien, sv nouveau) {
    string corps = sans_end(HOSSEGOR);
    if (!remplacer(corps, ancien, nouveau))
        return false;
    sortie = signer(corps);
    return true;
}

bool lire_variante(sv ancien, sv nouveau, Mer &m) {
    string t;
    return variante(t, ancien, nouveau) && decoder(t, m);
}

// Retire toutes les lignes qui commencent par un des débuts donnés.
string sans_lignes(const string &corps, std::initializer_list<sv> debuts) {
    string sortie;
    std::size_t pos = 0;
    while (pos < corps.size()) {
        const std::size_t nl = corps.find('\n', pos);
        const std::size_t fin = nl == string::npos ? corps.size() : nl + 1;
        const sv ligne(corps.data() + pos, fin - pos);
        bool retiree = false;
        for (const sv d : debuts)
            retiree = retiree || ligne.substr(0, d.size()) == d;
        if (!retiree)
            sortie.append(ligne);
        pos = fin;
    }
    return sortie;
}

constexpr sv FIN_SEA_PAR_DEFAUT = "|ok|||\n";
constexpr sv MAREES_HOSSEGOR =
    "|L:1794126780:-1.66,H:1794149460:1.71,L:1794171060:-1.67,H:1794193680:1.72,L:1794215400:-1.71,H:1794238020:1.68|";
constexpr sv COEFFICIENTS_HOSSEGOR = "|1794150900:83,1794195120:84,1794239400:83|";
constexpr sv ACT_SURF_FIN = "|1794142800|4|8\n";

Mer hossegor() {
    Mer m{};
    decoder(HOSSEGOR, m);
    return m;
}

// ---- la trame entière : CRC, fin, bloc absent -----------------------------------------------------------------------------------
int une_trame_dont_un_octet_a_change_est_refusee_et_la_mer_d_avant_est_gardee() {
    string t(HOSSEGOR);
    VERIFIER(remplacer(t, "Soorts-Hossegor", "Soorts-Hossegoz")); // un octet change, le CRC annoncé reste celui d'avant
    Mer neuve{};
    VERIFIER(!decoder(t, neuve) && !neuve.valide);
    Mer mer = hossegor();
    VERIFIER(mettre_a_jour(t, mer) == MiseAJour::Refusee);
    VERIFIER(sv(mer.lieu) == "Soorts-Hossegor"); // l'ancienne mer est intacte
    return 0;
}

int une_trame_coupee_ou_vide_est_refusee() {
    Mer mer = hossegor();
    VERIFIER(mettre_a_jour("", mer) == MiseAJour::Refusee);
    VERIFIER(mettre_a_jour(HOSSEGOR.substr(0, position_end(HOSSEGOR)), mer) == MiseAJour::Refusee);
    VERIFIER(mettre_a_jour(HOSSEGOR.substr(0, HOSSEGOR.size() - 3), mer) == MiseAJour::Refusee);
    VERIFIER(mettre_a_jour(HOSSEGOR.substr(0, HOSSEGOR.size() / 2), mer) == MiseAJour::Refusee);
    VERIFIER(sv(mer.nom) == "Plage Sud (Point d'Or)");
    return 0;
}

int un_crc_de_neuf_chiffres_est_refuse_et_les_minuscules_sont_acceptees() {
    string corps = sans_end(HOSSEGOR);
    char fin[24];
    std::snprintf(fin, sizeof fin, "END|%08X0\n", static_cast<unsigned>(detail::crc32(corps)));
    Mer m{};
    VERIFIER(!decoder(corps + fin, m)); // 9 chiffres : ce n'est pas un CRC
    std::snprintf(fin, sizeof fin, "END|%08x\n", static_cast<unsigned>(detail::crc32(corps)));
    VERIFIER(decoder(corps + fin, m)); // le décodeur de l'écran de Vigie accepte aussi les minuscules : même choix
    return 0;
}

int une_trame_sans_le_bloc_mer_garde_l_ancienne_mer() {
    string corps = sans_lignes(sans_end(HOSSEGOR), {"SEA|", "ACT|", "CRIT|"});
    VERIFIER(corps.size() < HOSSEGOR.size() && corps.find("HEAD|") == 0);
    const string t = signer(corps);
    Mer mer = hossegor();
    VERIFIER(mettre_a_jour(t, mer) == MiseAJour::SansBloc);
    VERIFIER(mer.valide && sv(mer.nom) == "Plage Sud (Point d'Or)" &&
             mer.nb_marees == 6); // pas de « rien à craindre » : la mer d'avant reste
    Mer vide{};
    VERIFIER(mettre_a_jour(t, vide) == MiseAJour::SansBloc && !vide.valide);
    VERIFIER(!decoder(t, vide)); // décoder une trame sans bloc ne donne pas une mer vide « valide »
    string abimee = t;
    VERIFIER(remplacer(abimee, "HEAD|", "HEAD|9")); // sans bloc ET sans CRC valide : refusée, pas « sans bloc »
    VERIFIER(mettre_a_jour(abimee, mer) == MiseAJour::Refusee);
    return 0;
}

int une_ligne_dont_la_balise_ressemble_a_sea_n_est_pas_le_bloc() {
    string corps = sans_lignes(sans_end(HOSSEGOR), {"SEA|", "ACT|", "CRIT|"});
    corps += "SEAX|a|b|c\nSEA2|x\n";
    Mer mer = hossegor();
    VERIFIER(mettre_a_jour(signer(corps), mer) == MiseAJour::SansBloc);
    return 0;
}

int une_nouvelle_trame_remplace_toute_la_mer() {
    Mer mer = hossegor();
    VERIFIER(mer.activites[0].nb_criteres == 5);
    VERIFIER(mettre_a_jour(LA_TORCHE, mer) == MiseAJour::Remplacee);
    VERIFIER(sv(mer.nom) == "Plage de la Torche" && mer.marees[0].instant_s == 1794127560 &&
             mer.marees[0].hauteur_cm == -195);
    return 0;
}

// ---- les lignes -----------------------------------------------------------------------------------------------------------------
int une_balise_inconnue_est_sautee_et_ne_coupe_pas_le_lien_entre_une_activite_et_ses_criteres() {
    string corps = sans_end(HOSSEGOR);
    VERIFIER(remplacer(corps, "SEA|", "FOO|1|2|3\nSEA|"));
    VERIFIER(remplacer(corps, "CRIT|surf|wind|", "BAR|x\nCRIT|surf|wind|"));
    VERIFIER(remplacer(corps, "ACT|kite|", "BAZ\nACT|kite|")); // une ligne sans « | » du tout
    Mer m{};
    VERIFIER(decoder(signer(corps), m) && m.valide);
    VERIFIER(m.activites[0].nb_criteres == 5 && m.activites[1].nb_criteres == 3 && m.activites[3].nb_criteres == 4);
    VERIFIER(sv(m.nom) == "Plage Sud (Point d'Or)");
    return 0;
}

int une_ligne_act_avant_la_ligne_sea_est_ignoree() {
    string corps = sans_lignes(sans_end(HOSSEGOR), {"ACT|kite|", "CRIT|kite|"});
    VERIFIER(remplacer(corps, "SEA|",
                       "ACT|kite|1|good||||GGGGGGGGGGGGGGGGGGGGGGGG|111111111111111111111111|1794142800||\nSEA|"));
    Mer m{};
    VERIFIER(decoder(signer(corps), m) && m.valide);
    VERIFIER(!m.activites[1].presente &&
             m.activites[1].verdict == Verdict::Inconnu); // elle n'a de sens qu'après sa ligne SEA
    VERIFIER(m.activites[0].presente && m.activites[2].presente);
    return 0;
}

int avec_deux_lignes_sea_la_premiere_gagne() {
    string corps = sans_end(HOSSEGOR);
    const std::size_t p = corps.find("\nACT|surf|");
    VERIFIER(p != string::npos);
    corps.insert(p + 1, "SEA|Autre|Ailleurs|1794143738|1794142800|0|12.0|25.0|||||||ok|||\n");
    Mer m{};
    VERIFIER(decoder(signer(corps), m));
    VERIFIER(sv(m.nom) == "Plage Sud (Point d'Or)" && m.nb_marees == 6);
    return 0;
}

int une_ligne_sea_inutilisable_refuse_la_trame_et_garde_l_ancienne_mer() {
    Mer mer = hossegor();
    // moins de 18 champs
    string corps = sans_end(HOSSEGOR);
    const std::size_t debut = corps.find("SEA|");
    const std::size_t fin = corps.find('\n', debut);
    VERIFIER(debut != string::npos && fin != string::npos);
    string court = corps;
    court.replace(debut, fin - debut, "SEA|Plage|Lieu|1794143738|1794142800|0|12.0|25.0|1|2|3|4|||ok|");
    VERIFIER(mettre_a_jour(signer(court), mer) == MiseAJour::Refusee && sv(mer.nom) == "Plage Sud (Point d'Or)");
    VERIFIER(!lire_variante("|Soorts-Hossegor|1794143738|", "|Soorts-Hossegor||", mer)); // « généré le » inconnu
    VERIFIER(!lire_variante("SEA|Plage Sud (Point d'Or)|", "SEA||", mer));               // pas de nom
    VERIFIER(sv(mer.nom) == "Plage Sud (Point d'Or)");
    return 0;
}

int une_ligne_crit_d_une_autre_activite_que_celle_du_dessus_est_ignoree() {
    Mer m{};
    VERIFIER(lire_variante("CRIT|kite|wind|7.8|", "CRIT|boat|wind|7.8|", m));
    VERIFIER(m.activites[1].nb_criteres == 2 && m.activites[2].nb_criteres == 3);
    VERIFIER(m.activites[1].criteres[0].id == Critere::EcartRafales);
    return 0;
}

int une_ligne_crit_avant_toute_ligne_act_est_ignoree() {
    string corps = sans_end(HOSSEGOR);
    VERIFIER(remplacer(corps, "ACT|surf|", "CRIT|surf|wind|9.9|km/h|offshore|good|2|2\nACT|surf|"));
    Mer m{};
    VERIFIER(decoder(signer(corps), m));
    VERIFIER(m.activites[0].nb_criteres == 5 && m.activites[0].criteres[3].valeur_d == 145);
    return 0;
}

int un_critere_au_nom_inconnu_n_est_pas_compte_mais_un_etat_ou_une_unite_inconnus_gardent_la_valeur() {
    Mer m{};
    VERIFIER(lire_variante("CRIT|surf|swell_height|", "CRIT|surf|swell_hauteur|", m));
    VERIFIER(m.activites[0].nb_criteres == 4 && m.activites[0].criteres[0].id == Critere::PeriodeHoule);
    VERIFIER(lire_variante("|0.4|m|too_small|poor|0|2", "|0.4|m|too_enorme|poor|0|2", m));
    VERIFIER(m.activites[0].nb_criteres == 5);
    VERIFIER(m.activites[0].criteres[0].etat == Etat::Inconnu &&
             m.activites[0].criteres[0].verdict == Verdict::Deconseille);
    VERIFIER(m.activites[0].criteres[0].valeur_d == 4);
    VERIFIER(lire_variante("|7.5|s|short|", "|7.5|parsec|short|", m));
    VERIFIER(m.activites[0].criteres[1].unite == Unite::Inconnue && m.activites[0].criteres[1].valeur_d == 75);
    return 0;
}

int au_dela_de_six_criteres_les_suivants_sont_laisses() {
    string corps = sans_end(HOSSEGOR);
    VERIFIER(
        remplacer(corps, "ACT|kite|",
                  "CRIT|surf|wind|10.0|km/h|offshore|good|2|2\nCRIT|surf|wind|11.0|km/h|offshore|good|2|2\nACT|kite|"));
    Mer m{};
    VERIFIER(decoder(signer(corps), m));
    VERIFIER(m.activites[0].nb_criteres == CRITERES_MAX);
    VERIFIER(m.activites[0].criteres[5].valeur_d == 100); // le sixième est le premier ajouté ; le septième est laissé
    return 0;
}

int un_champ_vide_reste_inconnu_jamais_zero() {
    Mer m{};
    VERIFIER(lire_variante("|12.0|25.0|", "|||", m));
    VERIFIER(m.vent_min_d == INCONNU && m.vent_max_d == INCONNU);
    VERIFIER(lire_variante("|12.0|25.0|", "|abc|25.0|", m));
    VERIFIER(m.vent_min_d == INCONNU && m.vent_max_d == 250);
    VERIFIER(lire_variante("|1794143738|1794142800|0|", "|1794143738||0|", m));
    VERIFIER(m.premiere_heure_s == 0 && m.genere_s == 1794143738);
    VERIFIER(lire_variante("|1794120636|1794156478|1794207116|1794242808|", "|1794120636||1794207116||", m));
    VERIFIER(m.lever_s == 1794120636 && m.coucher_s == 0 && m.lever2_s == 1794207116 && m.coucher2_s == 0);
    VERIFIER(lire_variante("|1794142800|4|8\n", "|1794142800|101|abc\n",
                           m)); // points hors de 0 à 100 ou illisibles : inconnus
    VERIFIER(m.activites[0].points == -1 && m.activites[0].sur == -1);
    VERIFIER(lire_variante("|1794142800|0|12.0|", "|1794142800|1|12.0|", m));
    VERIFIER(m.donnees_gardees); // l'API sert sa dernière réponse
    return 0;
}

// ---- les vigilances côtières ----------------------------------------------------------------------------------------------------
int un_avertissement_orange_est_lu_avec_sa_fin_et_sa_source() {
    Mer m{};
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|ok|ORANGE|1794160800|Meteo-France\n", m));
    VERIFIER(m.vigilances_lues && m.vigilance_presente && m.vigilance_niveau == ecran::vigie::Niveau::Orange);
    VERIFIER(m.vigilance_fin_s == 1794160800 && sv(m.vigilance_source) == "Meteo-France");
    return 0;
}

int les_quatre_niveaux_sont_lus() {
    Mer m{};
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|ok|RED|1794160800|MF\n", m) &&
             m.vigilance_niveau == ecran::vigie::Niveau::Rouge);
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|ok|YELLOW||\n", m) &&
             m.vigilance_niveau == ecran::vigie::Niveau::Jaune);
    VERIFIER(m.vigilance_presente && m.vigilance_fin_s == 0 && sv(m.vigilance_source).empty()); // fin inconnue
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|ok|GREEN||\n", m) &&
             m.vigilance_niveau == ecran::vigie::Niveau::Vert && m.vigilance_presente);
    return 0;
}

int sans_avertissement_lu_est_different_de_avertissements_illisibles() {
    Mer m{};
    VERIFIER(decoder(HOSSEGOR, m) && m.vigilances_lues && !m.vigilance_presente); // lu, et rien ne tient
    // un niveau inconnu de ce firmware, ou un état autre que « ok » : on n'a PAS pu savoir, ce n'est jamais « rien à craindre »
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|ok|PURPLE||\n", m) && !m.vigilances_lues && !m.vigilance_presente);
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|ok|orange|1794160800|MF\n", m) && !m.vigilances_lues &&
             !m.vigilance_presente);
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|unknown|ORANGE|1794160800|MF\n", m) && !m.vigilances_lues &&
             !m.vigilance_presente);
    // et le niveau qui l'accompagne n'est pas gardé « au cas où » : un niveau lu d'un état qu'on n'a pas pu lire serait un niveau inventé
    VERIFIER(m.vigilance_niveau == ecran::vigie::Niveau::Inconnu && m.vigilance_fin_s == 0 &&
             sv(m.vigilance_source).empty());
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "|unavailable|||\n", m) && !m.vigilances_lues && !m.vigilance_presente);
    VERIFIER(lire_variante(FIN_SEA_PAR_DEFAUT, "||||\n", m) && !m.vigilances_lues && !m.vigilance_presente);
    VERIFIER(m.vigilance_niveau == ecran::vigie::Niveau::Inconnu && m.vigilance_fin_s == 0);
    return 0;
}

// ---- les bandes, le créneau, l'activité indisponible ---------------------------------------------------------------------------
int une_bande_de_la_mauvaise_longueur_ou_avec_un_tiret_laisse_l_inconnu() {
    const string debut = "|swell_height.too_small|||";
    const string x24(24, 'X');
    Mer m{};
    VERIFIER(lire_variante(debut + x24 + "|", debut + string(23, 'X') + "|", m));
    for (int h = 0; h < HEURES; ++h)
        VERIFIER(m.activites[0].heures[h] == Verdict::Inconnu); // 23 lettres : aucune n'est devinée
    VERIFIER(lire_variante(debut + x24 + "|", debut + string(25, 'X') + "|", m));
    VERIFIER(m.activites[0].heures[0] == Verdict::Inconnu); // 25 lettres : pareil
    VERIFIER(lire_variante(debut + x24 + "|", debut + "G-P-" + string(20, 'X') + "|", m));
    VERIFIER(m.activites[0].heures[0] == Verdict::Bon && m.activites[0].heures[1] == Verdict::Inconnu);
    VERIFIER(m.activites[0].heures[2] == Verdict::Possible && m.activites[0].heures[3] == Verdict::Inconnu);
    VERIFIER(m.activites[0].heures[4] == Verdict::Deconseille && m.activites[0].heures[23] == Verdict::Deconseille);
    return 0;
}

int les_lumieres_de_la_bande_sont_jour_nuit_ou_inconnues() {
    const string lumiere24 = "1111" + string(14, '0') + "111111"; // celle de la trame réelle
    const string suite = "|1794142800|4|8";
    Mer m{};
    VERIFIER(lire_variante("|" + lumiere24 + suite, "|1-01" + string(13, '0') + "111111" + suite, m)); // 23 caractères
    for (int h = 0; h < HEURES; ++h)
        VERIFIER(m.activites[0].lumiere[h] == Lumiere::Inconnue); // rien n'est deviné
    VERIFIER(lire_variante("|" + lumiere24 + suite, "|1-01" + string(14, '0') + "111111" + suite, m)); // 24 caractères
    VERIFIER(m.activites[0].lumiere[0] == Lumiere::Jour && m.activites[0].lumiere[1] == Lumiere::Inconnue);
    VERIFIER(m.activites[0].lumiere[2] == Lumiere::Nuit && m.activites[0].lumiere[3] == Lumiere::Jour);
    VERIFIER(m.activites[0].lumiere[4] == Lumiere::Nuit && m.activites[0].lumiere[23] == Lumiere::Jour);
    return 0;
}

int un_creneau_qui_finit_avant_de_commencer_n_est_pas_un_creneau() {
    Mer m{};
    VERIFIER(lire_variante("|1794207600|1794229200|", "|1794229200|1794207600|", m));
    VERIFIER(m.activites[2].creneau_debut_s == 0 && m.activites[2].creneau_fin_s == 0);
    VERIFIER(lire_variante("|1794207600|1794229200|", "|1794207600|1794207600|", m));
    VERIFIER(m.activites[2].creneau_debut_s == 0 && m.activites[2].creneau_fin_s == 0); // vide
    VERIFIER(lire_variante("|1794207600|1794229200|", "||1794229200|", m));
    VERIFIER(m.activites[2].creneau_debut_s == 0 && m.activites[2].creneau_fin_s == 0); // début inconnu
    return 0;
}

int une_activite_indisponible_n_a_ni_verdict_ni_bande_ni_critere() {
    // 1. indisponible alors que ses champs 3 à 11 ne sont pas vides (le contrat les veut vides : on ne s'y fie pas)
    // 2. indisponible avec ses champs vides, comme l'API les rend
    for (const bool champs_vides : {false, true}) {
        string corps = sans_lignes(sans_end(HOSSEGOR), {"CRIT|shore_fishing|"});
        if (champs_vides) {
            const std::size_t debut = corps.find("ACT|shore_fishing|");
            const std::size_t fin = corps.find('\n', debut);
            VERIFIER(debut != string::npos && fin != string::npos);
            corps.replace(debut, fin - debut, "ACT|shore_fishing|0|||||||||");
        } else {
            VERIFIER(remplacer(corps, "ACT|shore_fishing|1|good||", "ACT|shore_fishing|0|good||"));
        }
        Mer m{};
        VERIFIER(decoder(signer(corps), m));
        const Prevision &p = m.activites[3];
        VERIFIER(p.presente && !p.disponible && p.verdict == Verdict::Inconnu && p.nb_criteres == 0);
        VERIFIER(p.creneau_debut_s == 0 && p.heures[6] == Verdict::Inconnu && p.points == -1);
        VERIFIER(meilleure_activite(m) == 2); // le bateau : la pêche à pied indisponible ne compte pas
    }
    return 0;
}

int une_ligne_act_ou_crit_trop_courte_est_ignoree() {
    // un ACT du kite à 5 champs : ignoré, et les CRIT du kite qui le suivent n'ont plus d'ACT auquel s'attacher
    string corps = sans_lignes(sans_end(HOSSEGOR), {"ACT|kite|"});
    VERIFIER(remplacer(corps, "CRIT|kite|wind|", "ACT|kite|1|good|wind\nCRIT|kite|wind|"));
    Mer m{};
    VERIFIER(decoder(signer(corps), m));
    VERIFIER(!m.activites[1].presente && m.activites[1].nb_criteres == 0 && m.activites[2].nb_criteres == 3);
    // un CRIT du surf à 5 champs : ignoré
    VERIFIER(lire_variante("CRIT|surf|wind|14.5|km/h|offshore|good|2|2\n", "CRIT|surf|wind|14.5|km/h\n", m));
    VERIFIER(m.activites[0].nb_criteres == 4 && m.activites[0].criteres[3].id == Critere::Maree);
    return 0;
}

// ---- les marées et les coefficients ---------------------------------------------------------------------------------------------
int plus_de_huit_marees_les_suivantes_sont_laissees() {
    string dix;
    for (int i = 0; i < 10; ++i)
        dix += string(i % 2 == 0 ? "L" : "H") + ":" + std::to_string(1794126780 + i * 22000) + ":" +
               (i % 2 == 0 ? "-1.5" : "1.5") + (i < 9 ? "," : "");
    Mer m{};
    VERIFIER(lire_variante(MAREES_HOSSEGOR, "|" + dix + "|", m));
    VERIFIER(m.nb_marees == MAREES_MAX && m.marees[7].instant_s == 1794126780 + 7 * 22000 && m.marees[7].haute);
    return 0;
}

int un_element_de_maree_mal_forme_est_saute_et_une_hauteur_vide_reste_inconnue() {
    Mer m{};
    VERIFIER(
        lire_variante(MAREES_HOSSEGOR, "|X:1:2,H:abc:1.0,H:1794149460:1.71,L:1794171060:,H:1794193680,L:0:1.0|", m));
    VERIFIER(m.nb_marees == 2);
    VERIFIER(m.marees[0].haute && m.marees[0].hauteur_cm == 171);
    VERIFIER(!m.marees[1].haute && m.marees[1].instant_s == 1794171060 && m.marees[1].hauteur_cm == INCONNU);
    VERIFIER(lire_variante(MAREES_HOSSEGOR, "||", m) && m.nb_marees == 0); // pas de modèle de marée près du spot
    VERIFIER(lire_variante(MAREES_HOSSEGOR, "|H:1794149460:1.7|", m) && m.nb_marees == 1 &&
             m.marees[0].hauteur_cm == 170);
    VERIFIER(lire_variante(MAREES_HOSSEGOR, "|H:1794149460:1.714|", m) &&
             m.marees[0].hauteur_cm == 171); // plus de décimales que prévu : arrondi
    VERIFIER(lire_variante(MAREES_HOSSEGOR, "|H:1794149460:1.715|", m) && m.marees[0].hauteur_cm == 172);
    VERIFIER(lire_variante(MAREES_HOSSEGOR, "|L:1794149460:-1.715|", m) &&
             m.marees[0].hauteur_cm == -172); // loin de zéro aussi en négatif
    return 0;
}

int un_coefficient_hors_de_1_a_200_est_saute_et_il_en_reste_quatre_au_plus() {
    Mer m{};
    VERIFIER(lire_variante(COEFFICIENTS_HOSSEGOR, "|1794150900:0,1794195120:201,1794239400:83,1:5,2:6,3:7,x:9,4:|", m));
    VERIFIER(m.nb_coefficients == 4);
    VERIFIER(m.coefficients[0].valeur == 83 && m.coefficients[0].pleine_mer_s == 1794239400);
    VERIFIER(m.coefficients[1].valeur == 5 && m.coefficients[3].valeur == 7);
    VERIFIER(lire_variante(COEFFICIENTS_HOSSEGOR, "|1794150900:200,1794195120:1|", m));
    VERIFIER(m.nb_coefficients == 2 && m.coefficients[0].valeur == 200 && m.coefficients[1].valeur == 1);
    VERIFIER(lire_variante(COEFFICIENTS_HOSSEGOR, "||", m) && m.nb_coefficients == 0);
    return 0;
}

// ---- le texte, la forme ----------------------------------------------------------------------------------------------------------
int un_nom_trop_long_est_coupe_sans_casser_un_caractere() {
    string e;
    for (int i = 0; i < 60; ++i)
        e += "\xC3\xA9"; // 60 « é » : 120 octets
    Mer m{};
    VERIFIER(lire_variante("|Soorts-Hossegor|", "|" + e + "|", m));
    VERIFIER(sv(m.lieu).size() == 98); // 49 caractères entiers ; le 100e octet est celui du zéro final
    VERIFIER(sv(m.lieu).substr(96) == "\xC3\xA9" && sv(m.nom) == "Plage Sud (Point d'Or)");
    return 0;
}

int les_fins_de_ligne_windows_sont_acceptees_et_le_crc_les_couvre() {
    string corps = sans_end(HOSSEGOR);
    string crlf;
    for (const char c : corps) {
        if (c == '\n')
            crlf += '\r';
        crlf += c;
    }
    Mer m{};
    VERIFIER(decoder(signer(crlf), m));
    VERIFIER(sv(m.nom) == "Plage Sud (Point d'Or)" && m.nb_marees == 6 && m.activites[0].nb_criteres == 5);
    VERIFIER(m.vigilances_lues && !m.vigilance_presente && m.activites[3].nb_criteres == 4);
    VERIFIER(m.activites[0].sur == 8 &&
             m.activites[0].criteres[0].points_max == 2); // le dernier champ d'une ligne ne garde pas le \r
    return 0;
}

int des_lignes_vides_et_des_lignes_tres_longues_ne_changent_rien() {
    string corps = sans_end(HOSSEGOR);
    VERIFIER(remplacer(corps, "ACT|surf|", "\n\nACT|surf|"));
    VERIFIER(remplacer(corps, ACT_SURF_FIN,
                       "|1794142800|4|8" + string(40, '|') + "x\n")); // plus de 32 champs : les premiers suffisent
    Mer m{};
    VERIFIER(decoder(signer(corps), m));
    VERIFIER(m.activites[0].points == 4 && m.activites[0].sur == 8 && m.activites[0].nb_criteres == 5);
    return 0;
}

int une_trame_apres_la_ligne_end_est_ignoree() {
    const string t =
        string(HOSSEGOR) +
        "ACT|surf|1|good||1794207600|1794214800|GGGGGGGGGGGGGGGGGGGGGGGG|111100000000000000111111|1794207600|9|9\n";
    Mer m{};
    VERIFIER(decoder(t, m) && sv(m.nom) == "Plage Sud (Point d'Or)");
    VERIFIER(m.activites[0].verdict == Verdict::Deconseille &&
             m.activites[0].points == 4); // ce qui suit END n'est pas lu
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(une_trame_dont_un_octet_a_change_est_refusee_et_la_mer_d_avant_est_gardee)                                       \
    X(une_trame_coupee_ou_vide_est_refusee)                                                                            \
    X(un_crc_de_neuf_chiffres_est_refuse_et_les_minuscules_sont_acceptees)                                             \
    X(une_trame_sans_le_bloc_mer_garde_l_ancienne_mer)                                                                 \
    X(une_ligne_dont_la_balise_ressemble_a_sea_n_est_pas_le_bloc)                                                      \
    X(une_nouvelle_trame_remplace_toute_la_mer)                                                                        \
    X(une_balise_inconnue_est_sautee_et_ne_coupe_pas_le_lien_entre_une_activite_et_ses_criteres)                       \
    X(une_ligne_act_avant_la_ligne_sea_est_ignoree)                                                                    \
    X(avec_deux_lignes_sea_la_premiere_gagne)                                                                          \
    X(une_ligne_sea_inutilisable_refuse_la_trame_et_garde_l_ancienne_mer)                                              \
    X(une_ligne_crit_d_une_autre_activite_que_celle_du_dessus_est_ignoree)                                             \
    X(une_ligne_crit_avant_toute_ligne_act_est_ignoree)                                                                \
    X(un_critere_au_nom_inconnu_n_est_pas_compte_mais_un_etat_ou_une_unite_inconnus_gardent_la_valeur)                 \
    X(au_dela_de_six_criteres_les_suivants_sont_laisses)                                                               \
    X(un_champ_vide_reste_inconnu_jamais_zero)                                                                         \
    X(un_avertissement_orange_est_lu_avec_sa_fin_et_sa_source)                                                         \
    X(les_quatre_niveaux_sont_lus)                                                                                     \
    X(sans_avertissement_lu_est_different_de_avertissements_illisibles)                                                \
    X(une_bande_de_la_mauvaise_longueur_ou_avec_un_tiret_laisse_l_inconnu)                                             \
    X(les_lumieres_de_la_bande_sont_jour_nuit_ou_inconnues)                                                            \
    X(un_creneau_qui_finit_avant_de_commencer_n_est_pas_un_creneau)                                                    \
    X(une_activite_indisponible_n_a_ni_verdict_ni_bande_ni_critere)                                                    \
    X(une_ligne_act_ou_crit_trop_courte_est_ignoree)                                                                   \
    X(plus_de_huit_marees_les_suivantes_sont_laissees)                                                                 \
    X(un_element_de_maree_mal_forme_est_saute_et_une_hauteur_vide_reste_inconnue)                                      \
    X(un_coefficient_hors_de_1_a_200_est_saute_et_il_en_reste_quatre_au_plus)                                          \
    X(un_nom_trop_long_est_coupe_sans_casser_un_caractere)                                                             \
    X(les_fins_de_ligne_windows_sont_acceptees_et_le_crc_les_couvre)                                                   \
    X(des_lignes_vides_et_des_lignes_tres_longues_ne_changent_rien)                                                    \
    X(une_trame_apres_la_ligne_end_est_ignoree)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
