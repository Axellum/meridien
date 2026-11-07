// Tests du noyau pur « alertes officielles de Vigie » : lecture de la réponse (vigie_alertes.hpp) et choix de ce qu'il faut montrer
// maintenant (vigie_vue.hpp).
//
// Les valeurs viennent de VRAIES réponses de Vigie reçues le 07/11/2026 avec l'adresse exacte du firmware (donnees/vigie_<lieu>.inc, capturées
// par test/host/capturer_vigie.py) et de deux réponses « toutes langues » captées à la main le même matin. Les valeurs attendues sont
// calculées par Python seul, sans reprendre une ligne du noyau (donnees/attendu_vigie_<lieu>.inc, test/host/controle_python_vigie.py) : pour
// chaque réponse, ce que `analyser()` doit lire et, pour des instants pris autour de chaque début et de chaque fin, ce que `composer()` doit
// montrer à chaque niveau minimal. Les cas limites sont écrits à la main, ou obtenus en cassant UNE chose dans une vraie réponse
// (`remplacer`). Aucune réponse n'est inventée de toutes pièces sauf les petites réponses des tests de capacité et de doublons, qui le disent.
//
// Ces tests ne sont évalués qu'à l'exécution (les structures pèsent dizaines de Ko : pas de static_assert).
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_textes.hpp"
#include "ecran/vigie_vue.hpp"
#include "mini.hpp"

using namespace ecran::vigie;
using sv = std::string_view;
using std::string;

namespace {

constexpr sv ANCONA =
#include "donnees/vigie_ancona.inc"
    ;
constexpr sv BODO =
#include "donnees/vigie_bodo.inc"
    ;
constexpr sv EBERSWALDE =
#include "donnees/vigie_eberswalde.inc"
    ;
constexpr sv EBERSWALDE_TL =
#include "donnees/vigie_eberswalde_toutes_langues.inc"
    ;
constexpr sv FUNCHAL =
#include "donnees/vigie_funchal.inc"
    ;
constexpr sv HAMBOURG =
#include "donnees/vigie_hambourg.inc"
    ;
constexpr sv LA_PALMA =
#include "donnees/vigie_la_palma.inc"
    ;
constexpr sv LA_PALMA_TL =
#include "donnees/vigie_la_palma_toutes_langues.inc"
    ;
constexpr sv LONDRES =
#include "donnees/vigie_londres.inc"
    ;
constexpr sv OSLO =
#include "donnees/vigie_oslo.inc"
    ;
constexpr sv TOKYO =
#include "donnees/vigie_tokyo.inc"
    ;
constexpr sv VALENCE =
#include "donnees/vigie_valence.inc"
    ;

struct AttTexte {
    sv langue, titre, description, consigne;
    int tronque;
};
struct AttAlerte {
    int niveau, phenomene, etat;
    long long envoi, effectif, debut, expire, fin;
    int tronque, attribution_tronquee;
    sv pays, niveau_original, phenomene_original, evenement, emetteur, zone, attribution;
    int nb_textes;
    AttTexte textes[TEXTES_MAX];
};
struct AttPoint {
    long long maintenant;
    int minimum;
    int nombre;
    int indices[ALERTES_MAX];
};
constexpr std::size_t POINTS_MAX = 96;
struct Att {
    long long serveur;
    int nombre, lues, doublons, omises, ignorees, couverture;
    sv attribution, avertissement;
    AttAlerte alertes[ALERTES_MAX];
    AttPoint points[POINTS_MAX];
};

const Att ATT_ANCONA =
#include "donnees/attendu_vigie_ancona.inc"
    ;
const Att ATT_BODO =
#include "donnees/attendu_vigie_bodo.inc"
    ;
const Att ATT_EBERSWALDE =
#include "donnees/attendu_vigie_eberswalde.inc"
    ;
const Att ATT_EBERSWALDE_TL =
#include "donnees/attendu_vigie_eberswalde_toutes_langues.inc"
    ;
const Att ATT_FUNCHAL =
#include "donnees/attendu_vigie_funchal.inc"
    ;
const Att ATT_HAMBOURG =
#include "donnees/attendu_vigie_hambourg.inc"
    ;
const Att ATT_LA_PALMA =
#include "donnees/attendu_vigie_la_palma.inc"
    ;
const Att ATT_LA_PALMA_TL =
#include "donnees/attendu_vigie_la_palma_toutes_langues.inc"
    ;
const Att ATT_LONDRES =
#include "donnees/attendu_vigie_londres.inc"
    ;
const Att ATT_OSLO =
#include "donnees/attendu_vigie_oslo.inc"
    ;
const Att ATT_TOKYO =
#include "donnees/attendu_vigie_tokyo.inc"
    ;
const Att ATT_VALENCE =
#include "donnees/attendu_vigie_valence.inc"
    ;

// Deux espaces de travail : une réponse pèse des dizaines de Ko, on ne la pose pas sur la pile.
Alertes g_a;
Alertes g_b;

int bits_couverture(const Couverture &c) {
    return (c.source_perimee ? 1 : 0) | (c.source_absente ? 2 : 0) | (c.non_rattachables ? 4 : 0) |
           (c.pays_sans_source ? 8 : 0);
}

// Compare ce que le noyau a lu avec ce que Python a lu : un retour non nul est le numéro de LIGNE de la première différence.
int comparer_lecture(const Alertes &a, const Att &att) {
    VERIFIER(a.statut == Statut::Ok);
    VERIFIER(a.serveur_s.value_or(-1) == att.serveur);
    VERIFIER(a.nombre == att.nombre);
    VERIFIER(a.lues == att.lues);
    VERIFIER(a.doublons == att.doublons);
    VERIFIER(a.omises == att.omises);
    VERIFIER(a.ignorees == att.ignorees);
    VERIFIER(bits_couverture(a.couverture) == att.couverture);
    VERIFIER(texte_de(a.attribution) == att.attribution);
    VERIFIER(texte_de(a.avertissement) == att.avertissement);
    for (int i = 0; i < att.nombre; ++i) {
        const Alerte &x = a.alertes[i];
        const AttAlerte &y = att.alertes[i];
        VERIFIER(static_cast<int>(x.niveau) == y.niveau);
        VERIFIER(static_cast<int>(x.phenomene) == y.phenomene);
        VERIFIER(static_cast<int>(x.etat) == y.etat);
        VERIFIER(x.envoi.value_or(-1) == y.envoi);
        VERIFIER(x.effectif.value_or(-1) == y.effectif);
        VERIFIER(x.debut.value_or(-1) == y.debut);
        VERIFIER(x.expire.value_or(-1) == y.expire);
        VERIFIER(x.fin.value_or(-1) == y.fin);
        VERIFIER(static_cast<int>(x.tronque) == y.tronque);
        VERIFIER(static_cast<int>(x.attribution_tronquee) == y.attribution_tronquee);
        VERIFIER(texte_de(x.pays) == y.pays);
        VERIFIER(texte_de(x.niveau_original) == y.niveau_original);
        VERIFIER(texte_de(x.phenomene_original) == y.phenomene_original);
        VERIFIER(texte_de(x.evenement) == y.evenement);
        VERIFIER(texte_de(x.emetteur) == y.emetteur);
        VERIFIER(texte_de(x.zone) == y.zone);
        VERIFIER(texte_de(x.attribution) == y.attribution);
        VERIFIER(x.nb_textes == y.nb_textes);
        for (int j = 0; j < y.nb_textes; ++j) {
            VERIFIER(texte_de(x.textes[j].langue) == y.textes[j].langue);
            VERIFIER(texte_de(x.textes[j].titre) == y.textes[j].titre);
            VERIFIER(texte_de(x.textes[j].description) == y.textes[j].description);
            VERIFIER(texte_de(x.textes[j].consigne) == y.textes[j].consigne);
            VERIFIER(static_cast<int>(x.textes[j].tronque) == y.textes[j].tronque);
        }
    }
    return 0;
}

int comparer_vue(const Alertes &a, const Att &att) {
    int points = 0;
    for (const AttPoint &p : att.points) {
        if (p.maintenant == 0)
            break;
        ++points;
        const Vue v = composer(a, p.maintenant, p.maintenant - 60, niveau_min_depuis_indice(p.minimum));
        VERIFIER(v.nombre == p.nombre);
        for (int j = 0; j < p.nombre; ++j)
            VERIFIER(v.indices[j] == p.indices[j]);
        VERIFIER(v.pire == (p.nombre > 0 ? a.alertes[p.indices[0]].niveau : Niveau::Inconnu));
    }
    VERIFIER(points >= 1);
    VERIFIER(points <
             static_cast<int>(POINTS_MAX)); // le tableau n'est pas plein : aucun instant de contrôle n'a été perdu
    return 0;
}

#define ECHANTILLON(NOM, JSON, ATT)                                                                                    \
    int NOM##_est_lu_comme_python() {                                                                                  \
        VERIFIER(analyser(JSON, "fr", g_a) == Statut::Ok);                                                             \
        return comparer_lecture(g_a, ATT);                                                                             \
    }                                                                                                                  \
    int NOM##_est_montre_comme_python() {                                                                              \
        VERIFIER(analyser(JSON, "fr", g_a) == Statut::Ok);                                                             \
        return comparer_vue(g_a, ATT);                                                                                 \
    }
ECHANTILLON(ancona, ANCONA, ATT_ANCONA)
ECHANTILLON(bodo, BODO, ATT_BODO)
ECHANTILLON(eberswalde, EBERSWALDE, ATT_EBERSWALDE)
ECHANTILLON(eberswalde_toutes_langues, EBERSWALDE_TL, ATT_EBERSWALDE_TL)
ECHANTILLON(funchal, FUNCHAL, ATT_FUNCHAL)
ECHANTILLON(hambourg, HAMBOURG, ATT_HAMBOURG)
ECHANTILLON(la_palma, LA_PALMA, ATT_LA_PALMA)
ECHANTILLON(la_palma_toutes_langues, LA_PALMA_TL, ATT_LA_PALMA_TL)
ECHANTILLON(londres, LONDRES, ATT_LONDRES)
ECHANTILLON(oslo, OSLO, ATT_OSLO)
ECHANTILLON(tokyo, TOKYO, ATT_TOKYO)
ECHANTILLON(valence, VALENCE, ATT_VALENCE)

// ---- outils des cas limites ------------------------------------------------------------------------------------------

// `base` avec UNE occurrence de `ancien` remplacée ; chaîne vide si `ancien` n'y est pas exactement une fois (le test le voit).
string remplacer(sv base, sv ancien, sv nouveau) {
    const size_t p = base.find(ancien);
    if (p == sv::npos || base.find(ancien, p + 1) != sv::npos)
        return {};
    string s(base);
    s.replace(p, ancien.size(), nouveau);
    return s;
}

// Une alerte minimale ÉCRITE À LA MAIN (pas une vraie réponse) : tout ce que les tests de capacité et de tri ont besoin de faire varier.
string alerte_json(sv niveau, sv phenomene, long long debut, long long expire, sv titre = "Titre", sv zone = "Zone") {
    return string("{\"level\":\"") + string(niveau) + "\",\"phenomenon\":\"" + string(phenomene) +
           "\",\"status\":\"new\",\"onset_utc\":" + std::to_string(debut) +
           ",\"expires_utc\":" + std::to_string(expire) + ",\"area\":{\"description\":\"" + string(zone) +
           "\"},\"texts\":[{\"language\":\"fr\",\"headline\":\"" + string(titre) +
           "\",\"description\":\"D\",\"instruction\":\"I\"}]}";
}

string reponse_json(const std::initializer_list<string> &alertes, sv reste = "") {
    string s = "{\"version\":\"1.0\",\"server_utc\":1794041555,\"vigilances\":[";
    bool premier = true;
    for (const string &a : alertes) {
        if (!premier)
            s += ",";
        s += a;
        premier = false;
    }
    s += "]";
    s += reste;
    s += "}";
    return s;
}

constexpr long long T0 = 1794041555; // l'instant où Eberswalde a été captée
constexpr long long H = 3600;

// ---- réponses inutilisables --------------------------------------------------------------------------------------------

int une_reponse_illisible_ne_donne_aucune_alerte() {
    VERIFIER(analyser("", "fr", g_a) == Statut::Illisible && g_a.nombre == 0);
    VERIFIER(analyser("   ", "fr", g_a) == Statut::Illisible);
    VERIFIER(analyser("{", "fr", g_a) == Statut::Illisible);
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[", "fr", g_a) == Statut::Illisible);
    VERIFIER(analyser("nul", "fr", g_a) == Statut::Illisible);
    // Une vraie réponse coupée en deux (une connexion qui tombe) : rien, et surtout pas les alertes lues avant la coupure.
    const sv moitie = LA_PALMA.substr(0, LA_PALMA.size() / 2);
    VERIFIER(analyser(moitie, "fr", g_a) == Statut::Illisible && g_a.nombre == 0 && g_a.lues == 0);
    VERIFIER(composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    return 0;
}

int une_reponse_qui_n_est_pas_celle_attendue_est_refusee() {
    VERIFIER(analyser("[]", "fr", g_a) == Statut::Incoherent);
    VERIFIER(analyser("42", "fr", g_a) == Statut::Incoherent);
    VERIFIER(analyser("\"x\"", "fr", g_a) == Statut::Incoherent);
    VERIFIER(analyser("{\"version\":\"1.0\"}", "fr", g_a) == Statut::Incoherent);
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":{}}", "fr", g_a) == Statut::Incoherent);
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":null}", "fr", g_a) == Statut::Incoherent);
    // Le corps d'une erreur de Vigie (HTTP 422, par exemple) : du JSON, mais pas une réponse.
    VERIFIER(analyser("{\"detail\":\"`lat` and `lon` go together\"}", "fr", g_a) == Statut::VersionInconnue);
    VERIFIER(g_a.nombre == 0);
    return 0;
}

int une_version_inconnue_est_refusee_sans_rien_deviner() {
    VERIFIER(analyser("{\"vigilances\":[]}", "fr", g_a) == Statut::VersionInconnue);
    VERIFIER(analyser("{\"version\":\"2.0\",\"vigilances\":[]}", "fr", g_a) == Statut::VersionInconnue);
    VERIFIER(analyser("{\"version\":1,\"vigilances\":[]}", "fr", g_a) == Statut::VersionInconnue);
    VERIFIER(analyser("{\"version\":null,\"vigilances\":[]}", "fr", g_a) == Statut::VersionInconnue);
    VERIFIER(analyser("{\"version\":\"1\",\"vigilances\":[]}", "fr", g_a) == Statut::VersionInconnue);
    VERIFIER(analyser("{\"version\":\"1.7\",\"vigilances\":[]}", "fr", g_a) == Statut::Ok); // 1.x : compatible
    const string v2 = remplacer(EBERSWALDE, "\"version\":\"1.0\"", "\"version\":\"2.0\"");
    VERIFIER(!v2.empty() && analyser(v2, "fr", g_a) == Statut::VersionInconnue && g_a.nombre == 0);
    return 0;
}

int une_reponse_sans_alerte_est_valide_et_vide() {
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[]}", "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.nombre == 0 && g_a.lues == 0 && !g_a.serveur_s.has_value());
    VERIFIER(composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    // Une analyse réutilisée sur la même case est vidée d'abord : rien ne reste de la réponse précédente.
    VERIFIER(analyser(LA_PALMA, "fr", g_a) == Statut::Ok && g_a.nombre == 9);
    VERIFIER(analyser(HAMBOURG, "fr", g_a) == Statut::Ok && g_a.nombre == 0 && g_a.lues == 0);
    VERIFIER(texte_de(g_a.alertes[0].zone).empty() && g_a.alertes[0].nb_textes == 0);
    return 0;
}

// ---- ce qui est lu, ce qui reste inconnu ------------------------------------------------------------------------------------

int un_champ_absent_ou_nul_reste_inconnu_jamais_vert_ni_zero() {
    string s = remplacer(EBERSWALDE, "\"level\":\"YELLOW\"", "\"level\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.alertes[0].niveau == Niveau::Inconnu);
    s = remplacer(EBERSWALDE, "\"level\":\"YELLOW\",", "");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].niveau == Niveau::Inconnu);
    s = remplacer(EBERSWALDE, "\"phenomenon\":\"cold\"", "\"phenomenon\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].phenomene == Phenomene::Inconnu);
    s = remplacer(EBERSWALDE, "\"status\":\"new\"", "\"status\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].etat == Etat::Inconnu);
    s = remplacer(EBERSWALDE, "\"onset_utc\":1793995200", "\"onset_utc\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && !g_a.alertes[0].debut.has_value());
    VERIFIER(g_a.alertes[0].effectif.value_or(-1) == 1793964420 && g_a.alertes[0].expire.value_or(-1) == 1794042000);
    s = remplacer(EBERSWALDE, "\"issuer\":\"Deutscher Wetterdienst\"", "\"issuer\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(texte_de(g_a.alertes[0].emetteur) ==
             "Deutscher Wetterdienst"); // repli : le nom donné par le premier texte
    return 0;
}

int un_niveau_ou_un_phenomene_hors_de_la_table_de_vigie_est_inconnu() {
    string s = remplacer(EBERSWALDE, "\"level\":\"YELLOW\"", "\"level\":\"PURPLE\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].niveau == Niveau::Inconnu);
    s = remplacer(EBERSWALDE, "\"level\":\"YELLOW\"", "\"level\":\"yellow\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok &&
             g_a.alertes[0].niveau == Niveau::Inconnu); // la casse compte
    s = remplacer(EBERSWALDE, "\"level\":\"YELLOW\"", "\"level\":2");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].niveau == Niveau::Inconnu);
    s = remplacer(EBERSWALDE, "\"phenomenon\":\"cold\"", "\"phenomenon\":\"tornado\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].phenomene == Phenomene::Inconnu);
    // Les huit niveaux et phénomènes de la table sont reconnus.
    const sv niveaux[] = {"GREEN", "YELLOW", "ORANGE", "RED"};
    const Niveau attendus[] = {Niveau::Vert, Niveau::Jaune, Niveau::Orange, Niveau::Rouge};
    for (int i = 0; i < 4; ++i) {
        const string r = reponse_json({alerte_json(niveaux[i], "wind", T0 - H, T0 + H)});
        VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && g_a.alertes[0].niveau == attendus[i]);
    }
    const sv phenomenes[] = {"wind",  "rain",      "thunderstorm", "snow-ice", "heat", "cold",
                             "flood", "avalanche", "coastal",      "fire",     "fog",  "other"};
    for (std::size_t i = 0; i < 12; ++i) {
        const string r = reponse_json({alerte_json("YELLOW", phenomenes[i], T0 - H, T0 + H)});
        VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && static_cast<std::size_t>(g_a.alertes[0].phenomene) == i + 1);
    }
    return 0;
}

int l_etat_vient_de_status_ou_du_type_de_message_cap() {
    const string base = reponse_json({alerte_json("YELLOW", "wind", T0 - H, T0 + H)});
    string s = remplacer(base, "\"status\":\"new\"", "\"status\":\"update\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].etat == Etat::MiseAJour);
    s = remplacer(base, "\"status\":\"new\"", "\"status\":\"cancel\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].etat == Etat::Annule);
    s = remplacer(base, "\"status\":\"new\"", "\"message_type\":\"Cancel\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok &&
             g_a.alertes[0].etat == Etat::Annule); // `status` manque : le type CAP le dit
    s = remplacer(base, "\"status\":\"new\"", "\"status\":\"bizarre\",\"message_type\":\"Alert\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].etat == Etat::Inconnu);
    return 0;
}

int un_instant_qui_n_est_pas_un_entier_de_2000_a_2100_est_inconnu() {
    const sv valeurs[] = {"0",          "-5",           "946684799",     "4102444801",
                          "5000000000", "1793995200.5", "1.793995200e9", "\"1793995200\"",
                          "true",       "null"};
    for (const sv v : valeurs) {
        const string s = remplacer(EBERSWALDE, "\"onset_utc\":1793995200", string("\"onset_utc\":") + string(v));
        VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
        VERIFIER(!g_a.alertes[0].debut.has_value());
    }
    const sv bons[] = {"946684800", "4102444800", "1793995200"};
    for (const sv v : bons) {
        const string s = remplacer(EBERSWALDE, "\"onset_utc\":1793995200", string("\"onset_utc\":") + string(v));
        VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.alertes[0].debut.has_value());
    }
    return 0;
}

int les_mots_de_l_emetteur_sont_copies_tels_quels() {
    VERIFIER(analyser(EBERSWALDE, "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    const Alerte &a = g_a.alertes[0];
    VERIFIER(texte_de(a.niveau_original) == "Minor"); // pas « Jaune » : le niveau tel que l'émetteur l'écrit
    VERIFIER(texte_de(a.phenomene_original) == "FROST");
    VERIFIER(texte_de(a.evenement) == "FROST");
    VERIFIER(texte_de(a.emetteur) == "Deutscher Wetterdienst");
    VERIFIER(texte_de(a.zone) == "Kreis Barnim");
    VERIFIER(a.niveau == Niveau::Jaune && a.phenomene == Phenomene::Froid && a.etat == Etat::Nouveau);
    VERIFIER(a.nb_textes == 2);
    const Texte *fr = choisir_texte(a, "fr");
    VERIFIER(fr != nullptr && texte_de(fr->langue) == "fr");
    VERIFIER(texte_de(fr->titre) == "ALERTE officielle INTEMPÉRIES pour GEL");
    // Accents, retours à la ligne, espace INSÉCABLE avant « : » (U+00A0, octets C2 A0 : c'est ce que l'émetteur a envoyé, pas une espace
    // ordinaire) et espace en fin de ligne : tels que l'émetteur les a écrits.
    VERIFIER(texte_de(fr->description) == "Il y a un risque de gel (Niveau 1 sur 2).\nTempérature minimale: 0 - -4 °C");
    VERIFIER(texte_de(fr->consigne) ==
             "Attention\xC2\xA0: dégats éventuels causés par le gel. \nRecommandations d'action\xC2\xA0: \nLe cas "
             "échéant, prenez des mesures de protection contre le gel");
    const Texte *en = choisir_texte(a, "en");
    VERIFIER(en != nullptr && texte_de(en->titre) == "Official WARNING of FROST");
    VERIFIER(texte_de(a.attribution) == "Source: Deutscher Wetterdienst | © GeoBasis-DE / BKG 2021 (Daten modifiziert) "
                                        "| Issued by Deutscher Wetterdienst");
    VERIFIER(texte_de(g_a.attribution) == "Source: Deutscher Wetterdienst");
    return 0;
}

int l_avertissement_des_sources_est_garde_pour_la_page() {
    VERIFIER(analyser(BODO, "fr", g_a) == Statut::Ok);
    VERIFIER(texte_de(g_a.avertissement).substr(0, 30) == "Time delays between this websi");
    VERIFIER(texte_de(g_a.attribution).substr(0, 30) ==
             "Source: EUMETNET – MeteoAlar"); // 30 octets : le tiret demi-cadratin en fait 3
    VERIFIER(analyser(EBERSWALDE, "fr", g_a) == Statut::Ok &&
             texte_de(g_a.avertissement).empty()); // `disclaimer` nul : rien
    return 0;
}

// ---- textes : ce que la police sait dessiner -------------------------------------------------------------------------------

int les_caracteres_de_la_police_sont_dessinables_les_autres_non() {
    VERIFIER(texte_affichable(""));
    VERIFIER(texte_affichable("Gale, yellow level"));
    VERIFIER(texte_affichable("Température minimale : −4 °C")); // « − » (U+2212) est dans la police
    VERIFIER(texte_affichable("naranja — «avisos» “x” ‘y’ … €"));
    VERIFIER(texte_affichable("Привет, мир")); // cyrillique
    VERIFIER(texte_affichable("Ωμέγα Άλφα"));  // grec
    VERIFIER(texte_affichable("Ệ ơ ư Ơ Ư"));   // vietnamien (dont les lettres à crochet, latin étendu B)
    VERIFIER(texte_affichable("Ș ș Ț ț"));     // roumain (virgule souscrite, U+0218 à U+021B)
    VERIFIER(texte_affichable("Łódź Žilina Ankara İstanbul")); // latin étendu A
    VERIFIER(!texte_affichable("ƀ")); // latin étendu B hors des lettres ci-dessus : la police ne l'a pas
    VERIFIER(texte_affichable("ligne 1\nligne 2"));
    VERIFIER(!texte_affichable("صقيع"));        // arabe : pas dans la police
    VERIFIER(!texte_affichable("霜"));          // chinois
    VERIFIER(!texte_affichable("ボウ"));        // japonais
    VERIFIER(!texte_affichable("⚠ warning"));   // symbole d'avertissement U+26A0
    VERIFIER(!texte_affichable("😀"));          // hors du plan de base
    VERIFIER(!texte_affichable("a\tb"));        // tabulation
    VERIFIER(!texte_affichable("a\rb"));        // retour chariot
    VERIFIER(!texte_affichable(sv("a\0b", 3))); // octet nul
    return 0;
}

int un_utf8_invalide_n_est_pas_dessinable() {
    VERIFIER(!texte_affichable("\xC3"));     // coupé
    VERIFIER(!texte_affichable("\xC3("));    // pas une suite
    VERIFIER(!texte_affichable("\xC0\x80")); // forme trop longue de U+0000
    VERIFIER(!texte_affichable("\xC1\xBF")); // forme trop longue de U+007F
    VERIFIER(!texte_affichable(
        "\xC0\xA0")); // forme trop longue d'une espace (U+0020, dessinable : seul le décodage strict la refuse)
    VERIFIER(!texte_affichable("\xE0\x80\xA0"));     // la même sur trois octets
    VERIFIER(!texte_affichable("\xF0\x80\x80\xA0")); // la même sur quatre octets
    VERIFIER(!texte_affichable("\xE0\x80\x80"));     // forme trop longue
    VERIFIER(!texte_affichable("\xED\xA0\x80"));     // moitié de paire de substitution U+D800
    VERIFIER(!texte_affichable("\xF4\x90\x80\x80")); // au-delà de U+10FFFF
    VERIFIER(!texte_affichable("\xF8\x88\x80\x80\x80"));
    VERIFIER(!texte_affichable("\xFF"));
    VERIFIER(!texte_affichable("\x80"));
    VERIFIER(texte_affichable("\xC3\xA9")); // « é » valide
    return 0;
}

int la_langue_se_compare_par_sa_partie_principale() {
    VERIFIER(langue_correspond("fr", "fr"));
    VERIFIER(langue_correspond("fr-FR", "fr"));
    VERIFIER(langue_correspond("fr_CA", "fr"));
    VERIFIER(langue_correspond("FR", "fr"));
    VERIFIER(langue_correspond("en-GB", "en"));
    VERIFIER(langue_correspond("de-DE", "de"));
    VERIFIER(!langue_correspond("fra", "fr"));
    VERIFIER(!langue_correspond("fr", "en"));
    VERIFIER(!langue_correspond("", "fr"));
    VERIFIER(!langue_correspond("-FR", "fr"));
    VERIFIER(!langue_correspond("french", "fr"));
    VERIFIER(!langue_correspond("fr", ""));
    VERIFIER(langue_correspond("fil-PH", "fil")); // un code à trois lettres
    VERIFIER(!langue_correspond("fili", "fil"));  // quatre lettres : pas une étiquette de langue
    VERIFIER(!langue_correspond("french", "fre"));
    return 0;
}

int un_texte_avec_un_caractere_que_la_police_ne_dessine_pas_est_ecarte_en_entier() {
    // Le titre anglais reçoit un symbole que la police n'a pas : le texte anglais entier disparaît, le français reste, tel quel.
    string s = remplacer(EBERSWALDE, "Official WARNING of FROST", "Official \xE2\x9A\xA0 WARNING of FROST");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    VERIFIER(g_a.alertes[0].nb_textes == 1 && texte_de(g_a.alertes[0].textes[0].langue) == "fr");
    const Texte *t = choisir_texte(g_a.alertes[0], "en");
    VERIFIER(t != nullptr && texte_de(t->langue) == "fr"); // « en » demandé, seul le français est gardé : on le montre
    // Même chose si c'est la CONSIGNE du texte qui est fautive : on ne montre pas la moitié d'un avis.
    s = remplacer(EBERSWALDE, "If necessary, take precautions against frost.",
                  "If necessary, take \xE9\x9C\x9C precautions.");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.alertes[0].nb_textes == 1 && texte_de(g_a.alertes[0].textes[0].langue) == "fr");
    // Tous les textes écartés : l'alerte reste (niveau, phénomène, `event`), sans texte : l'écran composera avec les mots de Vigie.
    s = remplacer(s, "ALERTE officielle INTEMPÉRIES pour GEL", "ALERTE \xE2\x9A\xA0 GEL");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.nombre == 1 && g_a.alertes[0].nb_textes == 0);
    VERIFIER(g_a.alertes[0].niveau == Niveau::Jaune && choisir_texte(g_a.alertes[0], "fr") == nullptr);
    VERIFIER(titre_de(g_a.alertes[0], "fr").origine == OrigineTitre::Evenement &&
             titre_de(g_a.alertes[0], "fr").texte == "FROST");
    return 0;
}

int un_octet_invalide_dans_un_titre_ecarte_le_texte() {
    string s = remplacer(EBERSWALDE, "Official WARNING of FROST", "Official WARNING \xFF of FROST");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.alertes[0].nb_textes == 1 && texte_de(g_a.alertes[0].textes[0].langue) == "fr");
    return 0;
}

int un_champ_court_non_dessinable_est_vide_sans_perdre_l_alerte() {
    string s =
        remplacer(EBERSWALDE, "\"issuer\":\"Deutscher Wetterdienst\"", "\"issuer\":\"Wetterdienst \xE9\x9C\x9C\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    VERIFIER(texte_de(g_a.alertes[0].emetteur) == "Deutscher Wetterdienst"); // repli sur le nom du premier texte
    s = remplacer(EBERSWALDE, "\"description\":\"Kreis Barnim\"", "\"description\":\"\xE9\x9C\x9C\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.nombre == 1 &&
             texte_de(g_a.alertes[0].zone).empty());
    return 0;
}

// ---- tailles ---------------------------------------------------------------------------------------------------------------

int un_champ_trop_long_est_coupe_sur_une_frontiere_de_caractere_et_signale() {
    string zone;
    for (int i = 0; i < 100; ++i)
        zone += "\xC3\xA9"; // 100 « é » = 200 octets pour 95 de place
    string r = reponse_json({alerte_json("YELLOW", "wind", T0 - H, T0 + H, "T", zone)});
    VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    const Alerte &a = g_a.alertes[0];
    const sv z = texte_de(a.zone);
    VERIFIER(z.size() == 94 && z.size() % 2 == 0); // 47 « é » entiers : jamais un octet coupé en deux
    VERIFIER(texte_affichable(z));
    VERIFIER(a.tronque);
    // Le même titre à la limite exacte n'est pas signalé comme coupé.
    string titre(TITRE_CAP - 1, 'a');
    r = reponse_json({alerte_json("YELLOW", "wind", T0 - H, T0 + H, titre)});
    VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && texte_de(g_a.alertes[0].textes[0].titre).size() == TITRE_CAP - 1 &&
             !g_a.alertes[0].tronque);
    titre.push_back('a');
    r = reponse_json({alerte_json("YELLOW", "wind", T0 - H, T0 + H, titre)});
    VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && texte_de(g_a.alertes[0].textes[0].titre).size() == TITRE_CAP - 1);
    VERIFIER(g_a.alertes[0].tronque && g_a.alertes[0].textes[0].tronque);
    return 0;
}

int une_attribution_trop_longue_est_signalee_a_part() {
    const string longue(ATTRIBUTION_CAP + 50, 'x');
    const string r =
        reponse_json({alerte_json("YELLOW", "wind", T0 - H, T0 + H)}, string(",\"attribution\":\"") + longue + "\"");
    VERIFIER(analyser(r, "fr", g_a) == Statut::Ok);
    VERIFIER(texte_de(g_a.attribution).size() == ATTRIBUTION_CAP - 1 && g_a.attribution_tronquee);
    VERIFIER(!g_a.alertes[0].attribution_tronquee);
    return 0;
}

int au_dela_de_trois_textes_on_garde_la_langue_de_l_ecran_puis_l_anglais() {
    // Eberswalde « toutes langues » : de-DE, en, fr, es, ar (non dessinable), ru, tr, pl.
    VERIFIER(analyser(EBERSWALDE_TL, "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    const Alerte *a = &g_a.alertes[0];
    VERIFIER(a->nb_textes == 3);
    VERIFIER(texte_de(a->textes[0].langue) == "de-DE" && texte_de(a->textes[1].langue) == "en" &&
             texte_de(a->textes[2].langue) == "fr");
    VERIFIER(analyser(EBERSWALDE_TL, "tr", g_a) == Statut::Ok);
    a = &g_a.alertes[0];
    VERIFIER(a->nb_textes == 3 && texte_de(a->textes[0].langue) == "de-DE" && texte_de(a->textes[1].langue) == "en" &&
             texte_de(a->textes[2].langue) == "tr");
    VERIFIER(analyser(EBERSWALDE_TL, "ru", g_a) == Statut::Ok);
    a = &g_a.alertes[0];
    VERIFIER(a->nb_textes == 3 && texte_de(a->textes[2].langue) == "ru");
    const Texte *t = choisir_texte(*a, "ru");
    VERIFIER(t != nullptr && texte_de(t->titre) == "Официальное метеорологическое предупреждение: Заморозки");
    // « ar » : le texte arabe n'est pas dessinable, on retombe sur l'anglais.
    VERIFIER(analyser(EBERSWALDE_TL, "ar", g_a) == Statut::Ok);
    t = choisir_texte(g_a.alertes[0], "ar");
    VERIFIER(t != nullptr && texte_de(t->langue) == "en");
    return 0;
}

int au_dela_de_douze_alertes_on_garde_les_plus_graves_puis_les_plus_anciennes() {
    // Réponse ÉCRITE À LA MAIN : huit jaunes (début T0-8h… plus récent à chaque fois) puis sept oranges.
    string corps = "{\"version\":\"1.0\",\"server_utc\":1794041555,\"vigilances\":[";
    for (int i = 0; i < 8; ++i)
        corps += alerte_json("YELLOW", "wind", T0 - (8 - i) * H, T0 + H, "J" + std::to_string(i)) + ",";
    for (int i = 0; i < 7; ++i)
        corps += alerte_json("ORANGE", "rain", T0 - (7 - i) * H, T0 + H, "O" + std::to_string(i)) + (i < 6 ? "," : "");
    corps += "]}";
    VERIFIER(analyser(corps, "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.lues == 15 && g_a.nombre == ALERTES_MAX && g_a.omises == 3);
    int oranges = 0, jaunes = 0;
    for (std::size_t i = 0; i < g_a.nombre; ++i) {
        oranges += g_a.alertes[i].niveau == Niveau::Orange;
        jaunes += g_a.alertes[i].niveau == Niveau::Jaune;
    }
    VERIFIER(oranges == 7 && jaunes == 5); // les sept oranges sont tous là
    // Les jaunes gardés sont les cinq plus anciens (J0 à J4 : débuts les plus lointains).
    for (int k = 0; k < 5; ++k) {
        bool trouve = false;
        for (std::size_t i = 0; i < g_a.nombre; ++i)
            trouve = trouve || texte_de(g_a.alertes[i].textes[0].titre) == "J" + std::to_string(k);
        VERIFIER(trouve);
    }
    // La vue montre les oranges d'abord, du plus ancien au plus récent.
    const Vue v = composer(g_a, T0, T0 - 60, NiveauMin::Jaune);
    VERIFIER(v.nombre == ALERTES_MAX && v.pire == Niveau::Orange);
    VERIFIER(texte_de(g_a.alertes[v.indices[0]].textes[0].titre) == "O0" &&
             texte_de(g_a.alertes[v.indices[6]].textes[0].titre) == "O6");
    return 0;
}

int une_alerte_plus_grave_remplace_la_moins_importante_quand_c_est_plein() {
    // Douze jaunes puis un rouge : le rouge entre, le jaune le plus récent sort.
    string corps = "{\"version\":\"1.0\",\"vigilances\":[";
    for (int i = 0; i < 12; ++i)
        corps += alerte_json("YELLOW", "wind", T0 - (12 - i) * H, T0 + H, "J" + std::to_string(i)) + ",";
    corps += alerte_json("RED", "rain", T0 - H, T0 + H, "R") + "]}";
    VERIFIER(analyser(corps, "fr", g_a) == Statut::Ok && g_a.nombre == ALERTES_MAX && g_a.omises == 1 &&
             g_a.lues == 13);
    bool rouge = false, j11 = false, j0 = false;
    for (std::size_t i = 0; i < g_a.nombre; ++i) {
        const sv titre = texte_de(g_a.alertes[i].textes[0].titre);
        rouge = rouge || titre == "R";
        j11 = j11 || titre == "J11";
        j0 = j0 || titre == "J0";
    }
    VERIFIER(rouge && j0 && !j11);
    return 0;
}

int deux_alertes_identiques_en_tout_n_en_font_qu_une() {
    const string a = alerte_json("ORANGE", "rain", T0 - H, T0 + H);
    const string b = alerte_json("ORANGE", "rain", T0 - H, T0 + H, "Autre titre");
    VERIFIER(analyser(reponse_json({a, a}), "fr", g_a) == Statut::Ok && g_a.nombre == 1 && g_a.lues == 2 &&
             g_a.doublons == 1);
    VERIFIER(analyser(reponse_json({a, b}), "fr", g_a) == Statut::Ok && g_a.nombre == 2 &&
             g_a.doublons == 0); // un mot de différence : deux alertes
    VERIFIER(analyser(reponse_json({a, alerte_json("ORANGE", "rain", T0 - H, T0 + H + 1)}), "fr", g_a) == Statut::Ok &&
             g_a.nombre == 2);
    VERIFIER(analyser(reponse_json({a, alerte_json("RED", "rain", T0 - H, T0 + H)}), "fr", g_a) == Statut::Ok &&
             g_a.nombre == 2);
    VERIFIER(analyser(reponse_json({a, alerte_json("ORANGE", "wind", T0 - H, T0 + H)}), "fr", g_a) == Statut::Ok &&
             g_a.nombre == 2);
    VERIFIER(analyser(reponse_json({a, alerte_json("ORANGE", "rain", T0 - H, T0 + H, "Titre", "Ailleurs")}), "fr",
                      g_a) == Statut::Ok &&
             g_a.nombre == 2);
    // Dans la vraie réponse de La Palma, les paires d'avertissements de l'AEMET diffèrent par leur description (30 mm en 1 h / 100 mm en 12 h) :
    // aucune n'est fusionnée.
    VERIFIER(analyser(LA_PALMA, "fr", g_a) == Statut::Ok && g_a.nombre == 9 && g_a.doublons == 0);
    return 0;
}

int une_alerte_sans_niveau_ni_phenomene_ni_mot_est_ignoree() {
    const string vide = "{\"onset_utc\":1793995200,\"expires_utc\":1794042000,\"texts\":[]}";
    VERIFIER(analyser(reponse_json({vide}), "fr", g_a) == Statut::Ok && g_a.nombre == 0 && g_a.ignorees == 1 &&
             g_a.lues == 0);
    // Un niveau seul suffit pour la garder ; un `event` seul aussi.
    VERIFIER(analyser(reponse_json({"{\"level\":\"RED\"}"}), "fr", g_a) == Statut::Ok && g_a.nombre == 1 &&
             g_a.ignorees == 0);
    VERIFIER(analyser(reponse_json({"{\"event\":\"Tornado\"}"}), "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    VERIFIER(analyser(reponse_json({"{\"phenomenon\":\"fog\"}"}), "fr", g_a) == Statut::Ok && g_a.nombre == 1);
    VERIFIER(analyser(reponse_json({"42", "\"x\"", "null", "[]"}), "fr", g_a) == Statut::Ok && g_a.nombre == 0 &&
             g_a.lues == 0); // pas des objets
    return 0;
}

int la_couverture_dit_ce_que_vigie_n_a_pas_pu_faire() {
    VERIFIER(analyser(HAMBOURG, "fr", g_a) == Statut::Ok && g_a.couverture.complete());
    VERIFIER(analyser(FUNCHAL, "fr", g_a) == Statut::Ok && g_a.nombre == 0 && g_a.couverture.non_rattachables &&
             !g_a.couverture.complete());
    VERIFIER(analyser(TOKYO, "fr", g_a) == Statut::Ok && g_a.nombre == 0 && g_a.couverture.pays_sans_source &&
             !g_a.couverture.complete());
    VERIFIER(analyser(ANCONA, "fr", g_a) == Statut::Ok && g_a.couverture.non_rattachables); // 4 sur 4
    string s = remplacer(HAMBOURG, "\"status\":\"ok\"", "\"status\":\"stale\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.couverture.source_perimee &&
             !g_a.couverture.source_absente);
    s = remplacer(HAMBOURG, "\"status\":\"ok\"", "\"status\":\"unavailable\"");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok && g_a.couverture.source_absente &&
             !g_a.couverture.source_perimee);
    return 0;
}

int l_emetteur_vient_du_premier_texte_quand_issuer_manque() {
    const string r = "{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"texts\":[{\"language\":\"fr\",\"sender_"
                     "name\":\"Météo-France\","
                     "\"headline\":\"Vent\"}]}]}";
    VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && texte_de(g_a.alertes[0].emetteur) == "Météo-France");
    return 0;
}

// ---- « en vigueur » -------------------------------------------------------------------------------------------------

constexpr long long DEBUT_E = 1793995200; // Eberswalde : onset
constexpr long long FIN_E = 1794042000;   // Eberswalde : expires

int une_alerte_n_est_en_vigueur_qu_entre_son_debut_et_sa_fin() {
    VERIFIER(analyser(EBERSWALDE, "fr", g_a) == Statut::Ok);
    const Alerte &a = g_a.alertes[0];
    VERIFIER(!en_vigueur(a, DEBUT_E - 1, 60));
    VERIFIER(en_vigueur(a, DEBUT_E, 60));
    VERIFIER(en_vigueur(a, FIN_E - 1, 60));
    VERIFIER(!en_vigueur(a, FIN_E, 60)); // la fin est exclue : à l'instant « expires », c'est fini
    VERIFIER(!en_vigueur(a, FIN_E + 1, 60));
    VERIFIER(en_vigueur(a, T0, 60)); // l'instant de la capture : le gel était en vigueur
    const Vue v = composer(g_a, T0, T0 - 5, NiveauMin::Jaune);
    VERIFIER(v.nombre == 1 && v.indices[0] == 0 && v.pire == Niveau::Jaune);
    return 0;
}

int une_alerte_qui_commence_dans_le_futur_n_est_pas_montree() {
    // La Palma à l'instant de la capture : neuf avertissements, tous à venir (le premier commence à 12 h UTC) : l'écran se tait.
    VERIFIER(analyser(LA_PALMA, "fr", g_a) == Statut::Ok && g_a.nombre == 9);
    VERIFIER(composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    VERIFIER(composer(g_a, T0 + 3 * H, T0 + 3 * H - 60, NiveauMin::Jaune).vide());
    // À 12 h UTC (1794052800), le jaune « vent » commence : un seul est en vigueur.
    const Vue v = composer(g_a, 1794052800, 1794052740, NiveauMin::Jaune);
    VERIFIER(v.nombre == 1 && g_a.alertes[v.indices[0]].niveau == Niveau::Jaune &&
             g_a.alertes[v.indices[0]].phenomene == Phenomene::Vent);
    return 0;
}

int la_palma_montre_plusieurs_niveaux_a_la_fois_du_plus_grave_au_moins_grave() {
    VERIFIER(analyser(LA_PALMA, "fr", g_a) == Statut::Ok);
    // 21 h UTC (1794085200) : trois orangés (pluie, pluie, vent) ET un jaune (orage) commencent ; le jaune « vent » vient de finir (1794085199).
    Vue v = composer(g_a, 1794085200, 1794085140, NiveauMin::Jaune);
    VERIFIER(v.nombre == 4 && v.pire == Niveau::Orange);
    VERIFIER(g_a.alertes[v.indices[0]].niveau == Niveau::Orange && g_a.alertes[v.indices[2]].niveau == Niveau::Orange &&
             g_a.alertes[v.indices[3]].niveau == Niveau::Jaune);
    v = composer(g_a, 1794085200, 1794085140, NiveauMin::Orange);
    VERIFIER(v.nombre == 3);
    v = composer(g_a, 1794085200, 1794085140, NiveauMin::Rouge);
    VERIFIER(v.vide() && v.pire == Niveau::Inconnu);
    v = composer(g_a, 1794085198, 1794085138, NiveauMin::Jaune); // deux secondes avant : seul le jaune « vent » est là
    VERIFIER(v.nombre == 1 && g_a.alertes[v.indices[0]].phenomene == Phenomene::Vent);
    // À 1794085199 (= hh:59:59, son `expires_utc`) il est encore là : les émetteurs ne s'accordent pas sur la dernière seconde (l'AEMET finit
    // à hh:59:59, le DWD commence à hh:00:00), et cette seconde-là fait partie de l'alerte (`fin_exclue_s`, ADR-0009) : sans cela l'écran se
    // tairait UNE seconde entre le jaune « vent » et les orangés. À ...200 il est fini, les orangés sont là.
    v = composer(g_a, 1794085199, 1794085139, NiveauMin::Jaune);
    VERIFIER(v.nombre == 1 && g_a.alertes[v.indices[0]].niveau == Niveau::Jaune &&
             g_a.alertes[v.indices[0]].phenomene == Phenomene::Vent);
    return 0;
}

int une_fin_a_hh_59_59_est_la_derniere_seconde_de_l_heure_et_toute_autre_fin_reste_exclue() {
    // 1794002400 = hh:00:00 ; 1794002399 = hh:59:59 ; 1794001199 = hh:39:59 (même heure, 20 minutes plus tôt)
    string s = remplacer(EBERSWALDE, "\"ends_utc\":null", "\"ends_utc\":1794002399");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(en_vigueur(g_a.alertes[0], 1794002398, 60));
    VERIFIER(
        en_vigueur(g_a.alertes[0], 1794002399, 60)); // la dernière seconde de l'heure fait encore partie de l'alerte
    VERIFIER(!en_vigueur(g_a.alertes[0], 1794002400, 60));
    s = remplacer(EBERSWALDE, "\"ends_utc\":null", "\"ends_utc\":1794001199"); // une fin ordinaire reste exclue
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(en_vigueur(g_a.alertes[0], 1794001198, 60));
    VERIFIER(!en_vigueur(g_a.alertes[0], 1794001199, 60));
    s = remplacer(EBERSWALDE, "\"ends_utc\":null", "\"ends_utc\":1794002400"); // hh:00:00 (le DWD) : exclue aussi
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(en_vigueur(g_a.alertes[0], 1794002399, 60));
    VERIFIER(!en_vigueur(g_a.alertes[0], 1794002400, 60));
    s = remplacer(EBERSWALDE, "\"expires_utc\":1794042000",
                  "\"expires_utc\":1794002399"); // sans fin attendue, l'expiration suit la même règle
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(en_vigueur(g_a.alertes[0], 1794002399, 60));
    VERIFIER(!en_vigueur(g_a.alertes[0], 1794002400, 60));
    return 0;
}

int la_fin_attendue_prime_sur_l_expiration_du_message() {
    string s = remplacer(EBERSWALDE, "\"ends_utc\":null", "\"ends_utc\":1794000000");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(en_vigueur(g_a.alertes[0], 1793999999, 60));
    VERIFIER(!en_vigueur(g_a.alertes[0], 1794000000, 60)); // `ends` passé, alors que `expires` est loin
    return 0;
}

int un_debut_inconnu_compte_comme_deja_commence_puis_l_effectif_sert_de_repli() {
    string s =
        remplacer(EBERSWALDE, "\"onset_utc\":1793995200", "\"onset_utc\":null"); // reste « effective » 1793964420
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(!en_vigueur(g_a.alertes[0], 1793964419, 60)); // avant l'effectif
    VERIFIER(en_vigueur(g_a.alertes[0], 1793964420, 60));
    s = remplacer(s, "\"effective_utc\":1793964420", "\"effective_utc\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    VERIFIER(en_vigueur(g_a.alertes[0], 1700000000, 60)); // aucun début connu : déjà commencée
    VERIFIER(!en_vigueur(g_a.alertes[0], FIN_E, 60));
    return 0;
}

int sans_fin_connue_l_alerte_tient_tant_que_la_reponse_est_fraiche() {
    const string s = remplacer(EBERSWALDE, "\"expires_utc\":1794042000", "\"expires_utc\":null");
    VERIFIER(!s.empty() && analyser(s, "fr", g_a) == Statut::Ok);
    const Alerte &a = g_a.alertes[0];
    VERIFIER(!a.expire.has_value() && !a.fin.has_value());
    VERIFIER(en_vigueur(a, T0 + 5 * H, 5 * H)); // 5 h après la réponse : encore
    VERIFIER(en_vigueur(a, T0 + AGE_MAX_S, AGE_MAX_S));
    VERIFIER(!en_vigueur(a, T0 + AGE_MAX_S + 1, AGE_MAX_S + 1)); // 6 h et 1 s : l'écran se tait
    return 0;
}

int une_reponse_de_plus_de_six_heures_ne_montre_rien_meme_avec_une_fin_connue() {
    VERIFIER(analyser(EBERSWALDE, "fr", g_a) == Statut::Ok);
    VERIFIER(composer(g_a, T0, T0 - AGE_MAX_S, NiveauMin::Jaune).nombre == 1);
    VERIFIER(composer(g_a, T0, T0 - AGE_MAX_S - 1, NiveauMin::Jaune).vide()); // la réponse a 6 h et 1 s
    return 0;
}

int une_horloge_inconnue_ou_qui_recule_ne_montre_rien() {
    VERIFIER(analyser(EBERSWALDE, "fr", g_a) == Statut::Ok);
    VERIFIER(composer(g_a, 0, T0, NiveauMin::Jaune).vide()); // l'heure du réseau n'est pas là
    VERIFIER(composer(g_a, -1, T0, NiveauMin::Jaune).vide());
    VERIFIER(composer(g_a, T0, 0, NiveauMin::Jaune).vide());      // la réponse n'a pas d'heure de réception
    VERIFIER(composer(g_a, T0, T0 + 1, NiveauMin::Jaune).vide()); // la réponse serait « reçue dans le futur »
    VERIFIER(composer(g_a, T0, T0, NiveauMin::Jaune).nombre == 1);
    // Une analyse qui a échoué ne montre rien, même si des alertes y traînaient.
    VERIFIER(analyser("{", "fr", g_a) == Statut::Illisible && composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    return 0;
}

int une_alerte_annulee_ou_verte_n_est_jamais_montree() {
    const string base = reponse_json({alerte_json("ORANGE", "rain", T0 - H, T0 + H)});
    VERIFIER(analyser(base, "fr", g_a) == Statut::Ok && composer(g_a, T0, T0, NiveauMin::Jaune).nombre == 1);
    string s = remplacer(base, "\"status\":\"new\"", "\"status\":\"cancel\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok && composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    s = remplacer(base, "\"status\":\"new\"", "\"message_type\":\"Cancel\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok && composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    s = remplacer(base, "\"level\":\"ORANGE\"", "\"level\":\"GREEN\"");
    VERIFIER(analyser(s, "fr", g_a) == Statut::Ok && composer(g_a, T0, T0, NiveauMin::Jaune).vide());
    // Sans passer par le niveau minimal (qui écarte déjà le vert) : `en_vigueur` seule dit non au vert et à l'annulée, oui à l'orange.
    VERIFIER(g_a.alertes[0].niveau == Niveau::Vert && !en_vigueur(g_a.alertes[0], T0, 0));
    VERIFIER(analyser(base, "fr", g_a) == Statut::Ok && en_vigueur(g_a.alertes[0], T0, 0));
    return 0;
}

int le_niveau_minimal_filtre_et_un_niveau_inconnu_reste_au_plus_bas() {
    const string jaune = reponse_json({alerte_json("YELLOW", "wind", T0 - H, T0 + H)});
    const string orange = reponse_json({alerte_json("ORANGE", "wind", T0 - H, T0 + H)});
    const string rouge = reponse_json({alerte_json("RED", "wind", T0 - H, T0 + H)});
    const string inconnu = reponse_json({"{\"phenomenon\":\"wind\",\"onset_utc\":" + std::to_string(T0 - H) +
                                         ",\"expires_utc\":" + std::to_string(T0 + H) + ",\"event\":\"Gale\"}"});
    const string *reponses[] = {&jaune, &orange, &rouge, &inconnu};
    // lignes : jaune, orange, rouge, inconnu ; colonnes : minimum Jaune, Orange, Rouge
    const int attendu[4][3] = {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 0}};
    for (int i = 0; i < 4; ++i) {
        VERIFIER(analyser(*reponses[i], "fr", g_a) == Statut::Ok);
        for (int m = 0; m < 3; ++m)
            VERIFIER(composer(g_a, T0, T0, niveau_min_depuis_indice(m)).nombre == attendu[i][m]);
    }
    VERIFIER(analyser(inconnu, "fr", g_a) == Statut::Ok && g_a.alertes[0].niveau == Niveau::Inconnu);
    VERIFIER(composer(g_a, T0, T0, NiveauMin::Jaune).pire == Niveau::Inconnu);
    VERIFIER(niveau_min_depuis_indice(0) == NiveauMin::Jaune && niveau_min_depuis_indice(2) == NiveauMin::Rouge);
    VERIFIER(niveau_min_depuis_indice(3) == NiveauMin::Jaune &&
             niveau_min_depuis_indice(-1) == NiveauMin::Jaune); // valeur NVS abîmée : le défaut
    return 0;
}

int les_alertes_montrees_sont_triees_par_gravite_puis_anciennete() {
    // Réponse ÉCRITE À LA MAIN, dans le désordre : jaune récent, rouge récent, orange ancien, rouge ancien, jaune ancien.
    const string r = reponse_json(
        {alerte_json("YELLOW", "wind", T0 - H, T0 + H, "Jr"), alerte_json("RED", "rain", T0 - H, T0 + H, "Rr"),
         alerte_json("ORANGE", "fog", T0 - 5 * H, T0 + H, "Oa"), alerte_json("RED", "rain", T0 - 4 * H, T0 + H, "Ra"),
         alerte_json("YELLOW", "wind", T0 - 3 * H, T0 + H, "Ja")});
    VERIFIER(analyser(r, "fr", g_a) == Statut::Ok && g_a.nombre == 5);
    const Vue v = composer(g_a, T0, T0, NiveauMin::Jaune);
    VERIFIER(v.nombre == 5 && v.pire == Niveau::Rouge);
    const char *ordre[] = {"Ra", "Rr", "Oa", "Ja", "Jr"};
    for (int i = 0; i < 5; ++i)
        VERIFIER(texte_de(g_a.alertes[v.indices[i]].textes[0].titre) == ordre[i]);
    return 0;
}

// ---- les mots à montrer ---------------------------------------------------------------------------------------------------

// Langue / titre du texte choisi, « (aucun) » s'il n'y en a pas : un texte qui manque fait échouer le test, il ne le fait pas planter.
string langue_choisie(const Alerte &a, sv langue) {
    const Texte *t = choisir_texte(a, langue);
    return t == nullptr ? "(aucun)" : string(texte_de(t->langue));
}

string titre_choisi(const Alerte &a, sv langue) {
    const Texte *t = choisir_texte(a, langue);
    return t == nullptr ? "(aucun)" : string(texte_de(t->titre));
}

int le_texte_choisi_est_celui_de_la_langue_puis_l_anglais_puis_le_premier() {
    VERIFIER(analyser(EBERSWALDE_TL, "fr", g_a) == Statut::Ok);
    const Alerte &a = g_a.alertes[0]; // gardés : de-DE, en, fr
    VERIFIER(langue_choisie(a, "fr") == "fr");
    VERIFIER(langue_choisie(a, "de") == "de-DE");
    VERIFIER(langue_choisie(a, "en") == "en");
    VERIFIER(langue_choisie(a, "tr") == "en"); // pas de turc gardé : l'anglais
    VERIFIER(langue_choisie(a, "xx") == "en");
    VERIFIER(analyser(OSLO, "fr", g_b) ==
             Statut::Ok); // norvégien + anglais demandé « fr,en » : seul l'anglais est arrivé
    VERIFIER(g_b.alertes[0].nb_textes == 1 && langue_choisie(g_b.alertes[0], "fr") == "en-GB");
    // Sans anglais : le premier.
    const string s = "{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"texts\":[{\"language\":\"nb\","
                     "\"headline\":\"Kuling\"},"
                     "{\"language\":\"se\",\"headline\":\"Storm\"}]}]}";
    VERIFIER(analyser(s, "fr", g_b) == Statut::Ok && titre_choisi(g_b.alertes[0], "fr") == "Kuling");
    VERIFIER(choisir_texte(Alerte{}, "fr") == nullptr);
    return 0;
}

int le_titre_vient_de_l_emetteur_puis_de_l_evenement_puis_du_phenomene_d_origine() {
    VERIFIER(analyser(BODO, "fr", g_a) == Statut::Ok);
    Titre t = titre_de(g_a.alertes[0], "fr");
    VERIFIER(t.origine == OrigineTitre::TitreEmetteur && t.texte.substr(0, 4) == "Gale");
    const string base = "{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",%s}]}";
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"event\":\"Tornado\",\"phenomenon_"
                      "original\":\"Wind\"}]}",
                      "fr", g_a) == Statut::Ok);
    t = titre_de(g_a.alertes[0], "fr");
    VERIFIER(t.origine == OrigineTitre::Evenement && t.texte == "Tornado");
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"phenomenon_original\":\"Wind\"}]}",
                      "fr", g_a) == Statut::Ok);
    t = titre_de(g_a.alertes[0], "fr");
    VERIFIER(t.origine == OrigineTitre::PhenomeneOriginal && t.texte == "Wind");
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\"}]}", "fr", g_a) == Statut::Ok);
    t = titre_de(g_a.alertes[0], "fr");
    VERIFIER(t.origine == OrigineTitre::Aucune && t.texte.empty());
    // Un texte sans titre (description seule) : retombe sur `event`, jamais un titre inventé.
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"event\":\"Gale\",\"texts\":[{"
                      "\"language\":\"fr\",\"description\":\"D\"}]}]}",
                      "fr", g_a) == Statut::Ok);
    VERIFIER(titre_de(g_a.alertes[0], "fr").origine == OrigineTitre::Evenement);
    (void)base;
    return 0;
}

int l_empreinte_change_avec_ce_qui_se_voit_et_pas_sans() {
    VERIFIER(analyser(EBERSWALDE, "fr", g_a) == Statut::Ok);
    const Vue v = composer(g_a, T0, T0, NiveauMin::Jaune);
    const std::uint32_t e = empreinte(g_a, v, "fr");
    VERIFIER(e == empreinte(g_a, v, "fr")); // déterministe
    VERIFIER(e != empreinte(g_a, v, "en")); // la langue change le titre
    VERIFIER(e != empreinte(g_a, composer(g_a, FIN_E + 1, T0, NiveauMin::Jaune),
                            "fr")); // l'alerte a fini : la bande disparaît
    VERIFIER(analyser(EBERSWALDE, "fr", g_b) == Statut::Ok &&
             e == empreinte(g_b, composer(g_b, T0, T0, NiveauMin::Jaune), "fr")); // même réponse : même empreinte
    VERIFIER(analyser(OSLO, "fr", g_b) == Statut::Ok);
    VERIFIER(e != empreinte(g_b, composer(g_b, T0, T0, NiveauMin::Jaune), "fr"));
    const string s = remplacer(EBERSWALDE, "\"level\":\"YELLOW\"", "\"level\":\"ORANGE\"");
    VERIFIER(analyser(s, "fr", g_b) == Statut::Ok &&
             e != empreinte(g_b, composer(g_b, T0, T0, NiveauMin::Jaune), "fr"));
    return 0;
}

int l_anglais_est_garde_avant_les_langues_etrangeres() {
    // Réponse ÉCRITE À LA MAIN : quatre textes, la langue de l'écran arrive en dernier ; sur trois places l'anglais doit rester.
    const string s =
        "{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"texts\":[{\"language\":\"de\",\"headline\":\"D\"},"
        "{\"language\":\"es\",\"headline\":\"E\"},{\"language\":\"en\",\"headline\":\"N\"},{\"language\":\"tr\","
        "\"headline\":\"T\"}]}]}";
    VERIFIER(analyser(s, "tr", g_a) == Statut::Ok && g_a.nombre == 1 && g_a.alertes[0].nb_textes == 3);
    bool en = false, tr = false;
    for (std::size_t i = 0; i < 3; ++i) {
        en = en || texte_de(g_a.alertes[0].textes[i].langue) == "en";
        tr = tr || texte_de(g_a.alertes[0].textes[i].langue) == "tr";
    }
    VERIFIER(en && tr);
    return 0;
}

int un_texte_vide_n_est_pas_garde_mais_un_texte_partiel_l_est() {
    // Réponses ÉCRITES À LA MAIN.
    const string avant = "{\"version\":\"1.0\",\"vigilances\":[{\"level\":\"RED\",\"texts\":[{\"language\":\"fr\",";
    VERIFIER(analyser(avant + "\"headline\":\"\",\"description\":null}]}]}", "fr", g_a) == Statut::Ok &&
             g_a.alertes[0].nb_textes == 0);
    VERIFIER(analyser(avant + "\"headline\":\"Seul le titre\"}]}]}", "fr", g_a) == Statut::Ok &&
             g_a.alertes[0].nb_textes == 1);
    VERIFIER(texte_de(g_a.alertes[0].textes[0].titre) == "Seul le titre" &&
             texte_de(g_a.alertes[0].textes[0].consigne).empty());
    VERIFIER(analyser(avant + "\"description\":\"Seule la description\"}]}]}", "fr", g_a) == Statut::Ok &&
             g_a.alertes[0].nb_textes == 1);
    VERIFIER(analyser(avant + "\"instruction\":\"Seule la consigne\"}]}]}", "fr", g_a) == Statut::Ok &&
             g_a.alertes[0].nb_textes == 1);
    VERIFIER(texte_de(g_a.alertes[0].textes[0].consigne) == "Seule la consigne");
    return 0;
}

int une_alerte_moins_importante_n_entre_pas_quand_c_est_plein() {
    // Réponse ÉCRITE À LA MAIN : douze oranges (début T0-12h … T0-1h), puis un orange PLUS RÉCENT, puis un orange PLUS ANCIEN, puis, EN DERNIER, un
    // jaune très ancien (arrivé le dernier, il ne doit pas chasser un orange : une mutation qui le laisse entrer ne se verrait pas s'il venait avant).
    string corps = "{\"version\":\"1.0\",\"vigilances\":[";
    for (int i = 0; i < 12; ++i)
        corps += alerte_json("ORANGE", "wind", T0 - (12 - i) * H, T0 + H, "O" + std::to_string(i)) + ",";
    corps += alerte_json("ORANGE", "rain", T0 - H / 2, T0 + H, "Recent") + ",";
    corps += alerte_json("ORANGE", "fog", T0 - 20 * H, T0 + H, "Ancien") + ",";
    corps += alerte_json("YELLOW", "rain", T0 - 30 * H, T0 + H, "Jaune") + "]}";
    VERIFIER(analyser(corps, "fr", g_a) == Statut::Ok && g_a.nombre == ALERTES_MAX && g_a.lues == 15 &&
             g_a.omises == 3);
    bool jaune = false, recent = false, ancien = false, o11 = false, o0 = false;
    for (std::size_t i = 0; i < g_a.nombre; ++i) {
        const sv titre = texte_de(g_a.alertes[i].textes[0].titre);
        jaune = jaune || titre == "Jaune";
        recent = recent || titre == "Recent";
        ancien = ancien || titre == "Ancien";
        o11 = o11 || titre == "O11";
        o0 = o0 || titre == "O0";
    }
    // Le jaune et le plus récent des oranges sont refusés ; l'orange plus ancien entre et fait sortir le plus récent des gardés (O11).
    VERIFIER(!jaune && !recent && ancien && !o11 && o0);
    return 0;
}

int une_analyse_repart_de_zero_sur_la_meme_case() {
    // Réponse ÉCRITE À LA MAIN : tout ce qu'une analyse compte ou retient (compteurs, couverture, mentions, drapeaux de coupe, instants).
    const string longue(ATTRIBUTION_CAP + 20, 'x');
    string corps =
        "{\"version\":\"1.0\",\"server_utc\":1794041555,\"attribution\":\"" + longue + "\",\"disclaimer\":\"" +
        string(AVERTISSEMENT_CAP + 20, 'y') +
        "\",\"coverage\":[{\"status\":\"stale\",\"warnings_not_matchable\":2}],\"uncovered\":[\"XX\"],\"vigilances\":[";
    // Alerte 1 : tout est renseigné, la zone est trop longue ; alerte 2 : presque rien (rien ne doit lui rester de la première).
    corps +=
        "{\"level\":\"ORANGE\",\"phenomenon\":\"rain\",\"status\":\"update\",\"sent_utc\":1794000001,\"effective_utc\":"
        "1794000002,"
        "\"onset_utc\":1794000003,\"expires_utc\":1794090004,\"ends_utc\":1794090005,\"country\":\"FR\",\"level_"
        "original\":\"Orange\","
        "\"phenomenon_original\":\"Pluie\",\"event\":\"Pluie\",\"issuer\":\"Emetteur\",\"attribution\":\"" +
        longue + "\",\"area\":{\"description\":\"" + string(ZONE_CAP + 10, 'z') +
        "\"},\"texts\":[{\"language\":\"fr\",\"headline\":\"T1\",\"description\":\"D1\",\"instruction\":\"I1\"}]},";
    corps += "{\"level\":\"RED\"},";
    // Dix alertes de plus, distinctes, pour dépasser la capacité ; une jumelle de la dernière ; une vide.
    for (int i = 0; i < 11; ++i)
        corps += alerte_json("YELLOW", "wind", T0 - (i + 1) * H, T0 + H, "J" + std::to_string(i)) + ",";
    corps += alerte_json("YELLOW", "wind", T0 - 11 * H, T0 + H, "J10") + ",";
    corps += "{\"onset_utc\":1794000003}]}";
    VERIFIER(analyser(corps, "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.nombre == ALERTES_MAX && g_a.omises == 1 && g_a.doublons == 1 && g_a.ignorees == 1 && g_a.lues == 14);
    VERIFIER(g_a.serveur_s.has_value() && g_a.attribution_tronquee && g_a.avertissement_tronque &&
             !g_a.couverture.complete());
    VERIFIER(g_a.alertes[0].tronque && g_a.alertes[0].attribution_tronquee && g_a.alertes[0].debut.has_value());
    // La deuxième alerte ne porte que son niveau : tout le reste est inconnu ou vide, rien ne vient de la première.
    const Alerte &b = g_a.alertes[1];
    VERIFIER(b.niveau == Niveau::Rouge && b.phenomene == Phenomene::Inconnu && b.etat == Etat::Inconnu);
    VERIFIER(!b.envoi && !b.effectif && !b.debut && !b.expire && !b.fin);
    VERIFIER(texte_de(b.pays).empty() && texte_de(b.niveau_original).empty() &&
             texte_de(b.phenomene_original).empty() && texte_de(b.evenement).empty() && texte_de(b.emetteur).empty() &&
             texte_de(b.zone).empty() && texte_de(b.attribution).empty());
    VERIFIER(!b.tronque && !b.attribution_tronquee && b.nb_textes == 0);
    // Une réponse vide sur la même case : plus rien de ce qui précède.
    VERIFIER(analyser("{\"version\":\"1.0\",\"vigilances\":[]}", "fr", g_a) == Statut::Ok);
    VERIFIER(g_a.nombre == 0 && g_a.lues == 0 && g_a.omises == 0 && g_a.doublons == 0 && g_a.ignorees == 0);
    VERIFIER(!g_a.serveur_s.has_value() && g_a.couverture.complete());
    VERIFIER(texte_de(g_a.attribution).empty() && texte_de(g_a.avertissement).empty());
    VERIFIER(!g_a.attribution_tronquee && !g_a.avertissement_tronque);
    VERIFIER(texte_de(g_a.alertes[0].zone).empty() && !g_a.alertes[0].tronque && g_a.alertes[0].nb_textes == 0);
    return 0;
}

// Une case d'alerte est-elle vide : rien de ce qu'une analyse précédente y a écrit ne doit y rester (hors tampons de texte au-delà du premier octet).
bool alerte_vide(const Alerte &a) {
    return a.niveau == Niveau::Inconnu && a.phenomene == Phenomene::Inconnu && a.etat == Etat::Inconnu && !a.envoi &&
           !a.effectif && !a.debut && !a.expire && !a.fin && texte_de(a.pays).empty() &&
           texte_de(a.niveau_original).empty() && texte_de(a.phenomene_original).empty() &&
           texte_de(a.evenement).empty() && texte_de(a.emetteur).empty() && texte_de(a.zone).empty() &&
           texte_de(a.attribution).empty() && !a.attribution_tronquee && !a.tronque && a.nb_textes == 0 &&
           texte_de(a.textes[0].langue).empty() && texte_de(a.textes[0].titre).empty() &&
           texte_de(a.textes[0].description).empty() && texte_de(a.textes[0].consigne).empty() && !a.textes[0].tronque;
}

int une_reponse_refusee_ne_laisse_rien_de_la_precedente() {
    // Réponse ÉCRITE À LA MAIN : tout est renseigné (première alerte, mentions, instant du serveur, couverture), puis des réponses que l'on refuse :
    // l'écran ne doit rien garder de la bonne réponse d'avant (il montrerait une alerte d'une réponse que Vigie vient de désavouer).
    const string longue(ATTRIBUTION_CAP + 20, 'x');
    const string riche =
        "{\"version\":\"1.0\",\"server_utc\":1794041555,\"attribution\":\"Sources\",\"disclaimer\":\"Avertissement\","
        "\"coverage\":[{\"status\":\"stale\",\"warnings_not_matchable\":2}],\"uncovered\":[\"XX\"],\"vigilances\":["
        "{\"level\":\"ORANGE\",\"phenomenon\":\"rain\",\"status\":\"update\",\"sent_utc\":1794000001,\"effective_utc\":"
        "1794000002,"
        "\"onset_utc\":1794000003,\"expires_utc\":1794090004,\"ends_utc\":1794090005,\"country\":\"FR\",\"level_"
        "original\":\"Orange\","
        "\"phenomenon_original\":\"Pluie\",\"event\":\"Pluie\",\"issuer\":\"Emetteur\",\"attribution\":\"" +
        longue + "\",\"area\":{\"description\":\"Zone\"},\"texts\":[{\"language\":\"fr\",\"headline\":\"" +
        string(TITRE_CAP + 10, 't') +
        "\",\"description\":\"D1\",\"instruction\":\"I1\"}]}]}"; // titre trop long : le texte porte le drapeau « coupé »
    const string refusees[] = {
        "{\"version\":\"1.0\",\"vigilances\":[",   // coupée : illisible
        "{\"version\":\"2.0\",\"vigilances\":[]}", // version inconnue
        "{\"version\":\"1.0\",\"vigilances\":{}}", // pas un tableau : incohérente
        "[]",                                      // pas un objet
    };
    for (const string &mauvaise : refusees) {
        VERIFIER(analyser(riche, "fr", g_a) == Statut::Ok && g_a.nombre == 1 && g_a.serveur_s &&
                 !alerte_vide(g_a.alertes[0]));
        VERIFIER(g_a.alertes[0].textes[0].tronque && g_a.brouillon.tronque && !texte_de(g_a.brouillon.langue).empty());
        VERIFIER(analyser(mauvaise, "fr", g_a) != Statut::Ok && g_a.nombre == 0);
        VERIFIER(!g_a.serveur_s && g_a.couverture.complete() && g_a.lues == 0 && g_a.doublons == 0 && g_a.omises == 0 &&
                 g_a.ignorees == 0);
        VERIFIER(texte_de(g_a.attribution).empty() && texte_de(g_a.avertissement).empty() &&
                 !g_a.attribution_tronquee && !g_a.avertissement_tronque);
        VERIFIER(alerte_vide(g_a.alertes[0]) && alerte_vide(g_a.alertes[ALERTES_MAX]));
        VERIFIER(texte_de(g_a.brouillon.langue).empty() && texte_de(g_a.brouillon.titre).empty() &&
                 texte_de(g_a.brouillon.description).empty() && texte_de(g_a.brouillon.consigne).empty() &&
                 !g_a.brouillon.tronque);
    }
    return 0;
}

int l_empreinte_voit_chaque_champ_montre() {
    // Réponse ÉCRITE À LA MAIN : une alerte en vigueur à T0 dont chaque mot ou instant montré change tour à tour.
    const string base =
        R"JSON({"version":"1.0","server_utc":1794041555,"attribution":"Src","disclaimer":"Disc","vigilances":[{"level":"ORANGE","phenomenon":"rain","status":"new","onset_utc":1794000000,"expires_utc":1794090000,"ends_utc":1794080000,"issuer":"Em","attribution":"AttrA","event":"Ev","area":{"description":"Zone"},"texts":[{"language":"fr","headline":"Titre","description":"Desc","instruction":"Cons"}]}]})JSON";
    struct Variante {
        sv ancien, nouveau;
    };
    const Variante variantes[] = {
        {"\"level\":\"ORANGE\"", "\"level\":\"RED\""},
        {"\"phenomenon\":\"rain\"", "\"phenomenon\":\"wind\""},
        {"\"onset_utc\":1794000000", "\"onset_utc\":1794000001"},
        {"\"ends_utc\":1794080000", "\"ends_utc\":1794080001"},
        {"\"expires_utc\":1794090000", "\"expires_utc\":1794090001"},
        {"\"headline\":\"Titre\"", "\"headline\":\"Titre2\""},
        {"\"description\":\"Desc\"", "\"description\":\"Desc2\""},
        {"\"instruction\":\"Cons\"", "\"instruction\":\"Cons2\""},
        {"\"description\":\"Zone\"", "\"description\":\"Zone2\""},
        {"\"issuer\":\"Em\"", "\"issuer\":\"Em2\""},
        {"\"attribution\":\"AttrA\"", "\"attribution\":\"AttrB\""},
        {"\"attribution\":\"Src\"", "\"attribution\":\"Src2\""},
        {"\"disclaimer\":\"Disc\"", "\"disclaimer\":\"Disc2\""},
    };
    VERIFIER(analyser(base, "fr", g_a) == Statut::Ok);
    const Vue v0 = composer(g_a, T0, T0, NiveauMin::Jaune);
    VERIFIER(v0.nombre == 1);
    const std::uint32_t e0 = empreinte(g_a, v0, "fr");
    for (const Variante &x : variantes) {
        const string s = remplacer(base, x.ancien, x.nouveau);
        VERIFIER(!s.empty() && analyser(s, "fr", g_b) == Statut::Ok);
        const Vue v = composer(g_b, T0, T0, NiveauMin::Jaune);
        VERIFIER(v.nombre == 1);
        VERIFIER(empreinte(g_b, v, "fr") != e0);
    }
    // Le séparateur entre deux champs voisins compte : description « AB » + consigne « C » n'est pas « A » + « BC ».
    string a = remplacer(base, "\"description\":\"Desc\"", "\"description\":\"AB\"");
    a = remplacer(a, "\"instruction\":\"Cons\"", "\"instruction\":\"C\"");
    string b = remplacer(base, "\"description\":\"Desc\"", "\"description\":\"A\"");
    b = remplacer(b, "\"instruction\":\"Cons\"", "\"instruction\":\"BC\"");
    VERIFIER(!a.empty() && !b.empty() && analyser(a, "fr", g_a) == Statut::Ok && analyser(b, "fr", g_b) == Statut::Ok);
    VERIFIER(empreinte(g_a, composer(g_a, T0, T0, NiveauMin::Jaune), "fr") !=
             empreinte(g_b, composer(g_b, T0, T0, NiveauMin::Jaune), "fr"));
    // Aucune alerte montrée : l'empreinte ne dépend plus que des mentions de la réponse.
    VERIFIER(analyser(base, "fr", g_a) == Statut::Ok &&
             empreinte(g_a, composer(g_a, FIN_E + 10 * 86400, T0, NiveauMin::Jaune), "fr") != e0);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(ancona_est_lu_comme_python)                                                                                      \
    X(ancona_est_montre_comme_python)                                                                                  \
    X(bodo_est_lu_comme_python)                                                                                        \
    X(bodo_est_montre_comme_python)                                                                                    \
    X(eberswalde_est_lu_comme_python)                                                                                  \
    X(eberswalde_est_montre_comme_python)                                                                              \
    X(eberswalde_toutes_langues_est_lu_comme_python)                                                                   \
    X(eberswalde_toutes_langues_est_montre_comme_python)                                                               \
    X(funchal_est_lu_comme_python)                                                                                     \
    X(funchal_est_montre_comme_python)                                                                                 \
    X(hambourg_est_lu_comme_python)                                                                                    \
    X(hambourg_est_montre_comme_python)                                                                                \
    X(la_palma_est_lu_comme_python)                                                                                    \
    X(la_palma_est_montre_comme_python)                                                                                \
    X(la_palma_toutes_langues_est_lu_comme_python)                                                                     \
    X(la_palma_toutes_langues_est_montre_comme_python)                                                                 \
    X(londres_est_lu_comme_python)                                                                                     \
    X(londres_est_montre_comme_python)                                                                                 \
    X(oslo_est_lu_comme_python)                                                                                        \
    X(oslo_est_montre_comme_python)                                                                                    \
    X(tokyo_est_lu_comme_python)                                                                                       \
    X(tokyo_est_montre_comme_python)                                                                                   \
    X(valence_est_lu_comme_python)                                                                                     \
    X(valence_est_montre_comme_python)                                                                                 \
    X(une_reponse_illisible_ne_donne_aucune_alerte)                                                                    \
    X(une_reponse_qui_n_est_pas_celle_attendue_est_refusee)                                                            \
    X(une_version_inconnue_est_refusee_sans_rien_deviner)                                                              \
    X(une_reponse_sans_alerte_est_valide_et_vide)                                                                      \
    X(un_champ_absent_ou_nul_reste_inconnu_jamais_vert_ni_zero)                                                        \
    X(un_niveau_ou_un_phenomene_hors_de_la_table_de_vigie_est_inconnu)                                                 \
    X(l_etat_vient_de_status_ou_du_type_de_message_cap)                                                                \
    X(un_instant_qui_n_est_pas_un_entier_de_2000_a_2100_est_inconnu)                                                   \
    X(les_mots_de_l_emetteur_sont_copies_tels_quels)                                                                   \
    X(l_avertissement_des_sources_est_garde_pour_la_page)                                                              \
    X(les_caracteres_de_la_police_sont_dessinables_les_autres_non)                                                     \
    X(un_utf8_invalide_n_est_pas_dessinable)                                                                           \
    X(la_langue_se_compare_par_sa_partie_principale)                                                                   \
    X(un_texte_avec_un_caractere_que_la_police_ne_dessine_pas_est_ecarte_en_entier)                                    \
    X(un_octet_invalide_dans_un_titre_ecarte_le_texte)                                                                 \
    X(un_champ_court_non_dessinable_est_vide_sans_perdre_l_alerte)                                                     \
    X(un_champ_trop_long_est_coupe_sur_une_frontiere_de_caractere_et_signale)                                          \
    X(une_attribution_trop_longue_est_signalee_a_part)                                                                 \
    X(au_dela_de_trois_textes_on_garde_la_langue_de_l_ecran_puis_l_anglais)                                            \
    X(au_dela_de_douze_alertes_on_garde_les_plus_graves_puis_les_plus_anciennes)                                       \
    X(une_alerte_plus_grave_remplace_la_moins_importante_quand_c_est_plein)                                            \
    X(deux_alertes_identiques_en_tout_n_en_font_qu_une)                                                                \
    X(une_alerte_sans_niveau_ni_phenomene_ni_mot_est_ignoree)                                                          \
    X(la_couverture_dit_ce_que_vigie_n_a_pas_pu_faire)                                                                 \
    X(l_emetteur_vient_du_premier_texte_quand_issuer_manque)                                                           \
    X(une_alerte_n_est_en_vigueur_qu_entre_son_debut_et_sa_fin)                                                        \
    X(une_alerte_qui_commence_dans_le_futur_n_est_pas_montree)                                                         \
    X(la_palma_montre_plusieurs_niveaux_a_la_fois_du_plus_grave_au_moins_grave)                                        \
    X(la_fin_attendue_prime_sur_l_expiration_du_message)                                                               \
    X(une_fin_a_hh_59_59_est_la_derniere_seconde_de_l_heure_et_toute_autre_fin_reste_exclue)                           \
    X(un_debut_inconnu_compte_comme_deja_commence_puis_l_effectif_sert_de_repli)                                       \
    X(sans_fin_connue_l_alerte_tient_tant_que_la_reponse_est_fraiche)                                                  \
    X(une_reponse_de_plus_de_six_heures_ne_montre_rien_meme_avec_une_fin_connue)                                       \
    X(une_horloge_inconnue_ou_qui_recule_ne_montre_rien)                                                               \
    X(une_alerte_annulee_ou_verte_n_est_jamais_montree)                                                                \
    X(le_niveau_minimal_filtre_et_un_niveau_inconnu_reste_au_plus_bas)                                                 \
    X(les_alertes_montrees_sont_triees_par_gravite_puis_anciennete)                                                    \
    X(le_texte_choisi_est_celui_de_la_langue_puis_l_anglais_puis_le_premier)                                           \
    X(le_titre_vient_de_l_emetteur_puis_de_l_evenement_puis_du_phenomene_d_origine)                                    \
    X(l_anglais_est_garde_avant_les_langues_etrangeres)                                                                \
    X(un_texte_vide_n_est_pas_garde_mais_un_texte_partiel_l_est)                                                       \
    X(une_alerte_moins_importante_n_entre_pas_quand_c_est_plein)                                                       \
    X(une_analyse_repart_de_zero_sur_la_meme_case)                                                                     \
    X(une_reponse_refusee_ne_laisse_rien_de_la_precedente)                                                             \
    X(l_empreinte_voit_chaque_champ_montre)                                                                            \
    X(l_empreinte_change_avec_ce_qui_se_voit_et_pas_sans)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
