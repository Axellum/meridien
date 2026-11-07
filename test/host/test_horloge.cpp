// Test de main/horloge.cpp (l'horloge de l'appareil) compilé tel quel sur le PC, avec une zone fausse à la place de celle de la NVS
// (meteo_zone.h, zone_lire). Vérifie ADR-0012 : l'heure, la date et le fuseau sont ceux du LIEU choisi, relus à chaque appel.
// Les valeurs attendues viennent de Python zoneinfo (donnees/attendu_heure_lieu.inc, section HORLOGE, générée par
// controle_python_fuseaux.py) : huit lieux, treize instants de part et d'autre d'un minuit local, d'un changement d'heure, du nouvel an
// et d'un 29 février.
//
// Ce que ce test attrape et que les tests du noyau ne voient pas : un fuseau écrit en dur dans horloge.cpp (Paris), un fuseau gardé en
// mémoire après un changement de zone, un nom inconnu qui retombe sur Paris, une zone illisible qui donne quand même une heure.
#include <cstddef>
#include <cstring>
#include <string_view>

#include "../../main/horloge.cpp"
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
    sv hhmm, texte_heure, date;
    int decalage;
};
constexpr Ligne LIGNES[] = {
#define ATTENDU_LIEU_HORLOGE
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_HORLOGE
};
constexpr std::size_t NOMBRE_LIGNES = sizeof(LIGNES) / sizeof(LIGNES[0]);

// La zone « enregistrée » que lit l'horloge : posée par chaque test, comme la page de réglages ou l'écran la change sur l'appareil.
ZoneMeteo g_zone = {};
bool g_zone_lisible = true;
int g_lectures = 0;

void poser_zone(sv fuseau_iana) {
    g_zone = {};
    std::strcpy(g_zone.nom, "Essai");
    g_zone.latitude = 10.0F;
    g_zone.longitude = 20.0F;
    const std::size_t n = fuseau_iana.size() < sizeof g_zone.fuseau - 1 ? fuseau_iana.size() : sizeof g_zone.fuseau - 1;
    std::memcpy(g_zone.fuseau, fuseau_iana.data(), n);
    g_zone_lisible = true;
}

sv texte(const char *s) { return sv(s); }

} // namespace

// meteo_zone.h : sur l'appareil, la zone vient de la NVS ; ici, celle que le test a posée.
bool zone_lire(ZoneMeteo *sortie) {
    ++g_lectures;
    if (sortie == nullptr) {
        return false;
    }
    // Une lecture qui échoue peut laisser des octets dans `*sortie` (une copie à moitié faite) : on les laisse ici, pour qu'une horloge
    // qui ignore le retour de zone_lire donne une heure et soit prise.
    *sortie = g_zone;
    return g_zone_lisible;
}

namespace {

int le_tableau_attendu_est_la() {
    VERIFIER(NOMBRE_LIGNES == 104);
    VERIFIER(sizeof(ZONES) / sizeof(ZONES[0]) == 14);
    return 0;
}

int heure_date_et_decalage_sont_ceux_du_lieu_selon_zoneinfo() {
    // Boucle d'instant en dehors, de lieu en dedans : chaque appel change de zone par rapport au précédent. Un fuseau gardé de l'appel
    // d'avant (static) donnerait l'heure du lieu précédent.
    for (std::size_t i = 0; i < NOMBRE_LIGNES; ++i) {
        const Ligne &l = LIGNES[i];
        poser_zone(ZONES[static_cast<std::size_t>(l.zone)]);

        char hhmm[8] = {};
        VERIFIER(horloge_hhmm(l.instant, hhmm, sizeof hhmm));
        VERIFIER(texte(hhmm) == l.hhmm);

        TexteHorloge t = {};
        VERIFIER(horloge_formater(l.instant, t));
        VERIFIER(texte(t.heure) == l.texte_heure);
        VERIFIER(texte(t.date) == l.date);

        char date[48] = {};
        VERIFIER(horloge_date_localisee(l.instant, i18n::Langue::Fr, date, sizeof date));
        VERIFIER(texte(date) == l.date);

        VERIFIER(horloge_decalage_s(l.instant) == l.decalage);

        heure::HeureLocale h;
        VERIFIER(horloge_heure_locale(l.instant, h));
        VERIFIER(h.decalage_s == l.decalage);
        char reconstruit[8];
        reconstruit[0] = static_cast<char>('0' + h.heure / 10);
        reconstruit[1] = static_cast<char>('0' + h.heure % 10);
        reconstruit[2] = ':';
        reconstruit[3] = static_cast<char>('0' + h.minute / 10);
        reconstruit[4] = static_cast<char>('0' + h.minute % 10);
        reconstruit[5] = '\0';
        VERIFIER(texte(reconstruit) == l.hhmm);
    }
    return 0;
}

int le_fuseau_rendu_convertit_chaque_instant_comme_zoneinfo() {
    // horloge_fuseau : les règles du lieu, pas un décalage : elles donnent le bon décalage à des instants que le fuseau n'a jamais vus
    // (avant et après un changement d'heure).
    for (std::size_t i = 0; i < NOMBRE_LIGNES; ++i) {
        const Ligne &l = LIGNES[i];
        poser_zone(ZONES[static_cast<std::size_t>(l.zone)]);
        heure::Fuseau f;
        VERIFIER(horloge_fuseau(f));
        const heure::HeureLocale h = heure::en_heure_locale(l.instant, f);
        VERIFIER(h.decalage_s == l.decalage);
        VERIFIER(l.hhmm[0] == '0' + h.heure / 10 && l.hhmm[1] == '0' + h.heure % 10);
        VERIFIER(l.hhmm[3] == '0' + h.minute / 10 && l.hhmm[4] == '0' + h.minute % 10);
    }
    return 0;
}

int un_changement_de_zone_change_l_heure_et_le_jour_a_l_appel_suivant() {
    // Le même instant, deux lieux, sans rien redémarrer : Paris 21:30 le vendredi 6, Tokyo 05:30 le samedi 7.
    constexpr long long instant = 1793997000LL; // 2026-11-06 20:30 UTC (ligne de HORLOGE)
    char date[48];
    char hhmm[8];
    poser_zone("Europe/Paris");
    VERIFIER(horloge_hhmm(instant, hhmm, sizeof hhmm) && texte(hhmm) == "21:30");
    VERIFIER(horloge_date_localisee(instant, i18n::Langue::Fr, date, sizeof date) &&
             texte(date) == "vendredi 6 novembre 2026");
    poser_zone("Asia/Tokyo");
    VERIFIER(horloge_hhmm(instant, hhmm, sizeof hhmm) && texte(hhmm) == "05:30");
    VERIFIER(horloge_date_localisee(instant, i18n::Langue::Fr, date, sizeof date) &&
             texte(date) == "samedi 7 novembre 2026");
    poser_zone("Europe/Paris");
    VERIFIER(horloge_hhmm(instant, hhmm, sizeof hhmm) && texte(hhmm) == "21:30");
    return 0;
}

int la_zone_est_relue_a_chaque_appel() {
    // Pas de cache d'horloge : un appel = une lecture de la zone (ce que fait zone_lire sur l'appareil : une copie de la RAM).
    poser_zone("Europe/Paris");
    char hhmm[8];
    const int avant = g_lectures;
    VERIFIER(horloge_hhmm(1793961600LL, hhmm, sizeof hhmm));
    VERIFIER(g_lectures == avant + 1);
    VERIFIER(horloge_hhmm(1793961600LL, hhmm, sizeof hhmm));
    VERIFIER(g_lectures == avant + 2);
    return 0;
}

int un_fuseau_inconnu_ne_retombe_pas_sur_paris() {
    // Ni heure ni date, et pas de décalage inventé : l'appelant écrit « heure inconnue ».
    const sv inconnus[] = {"Mars/Olympus", "", "europe/paris", "Europe/Paris ", "Europe", "UTC+1"};
    for (const sv nom : inconnus) {
        poser_zone(nom);
        char hhmm[8] = {'x'};
        VERIFIER(!horloge_hhmm(1793961600LL, hhmm, sizeof hhmm));
        VERIFIER(hhmm[0] == '\0');
        TexteHorloge t = {};
        std::strcpy(t.heure, "x");
        std::strcpy(t.date, "x");
        VERIFIER(!horloge_formater(1793961600LL, t));
        VERIFIER(t.heure[0] == '\0' && t.date[0] == '\0');
        char date[48] = {'x'};
        VERIFIER(!horloge_date_localisee(1793961600LL, i18n::Langue::Fr, date, sizeof date));
        VERIFIER(date[0] == '\0');
        heure::HeureLocale h;
        VERIFIER(!horloge_heure_locale(1793961600LL, h));
        heure::Fuseau f;
        VERIFIER(!horloge_fuseau(f));
        VERIFIER(horloge_decalage_s(1793961600LL) == 0);
    }
    return 0;
}

int un_nom_de_fuseau_sans_zero_final_ne_deborde_pas() {
    // Les 40 octets du champ sont des « A » : aucun zéro. La lecture s'arrête au bout du champ (ASan le verrait sinon).
    g_zone = {};
    std::memset(g_zone.fuseau, 'A', sizeof g_zone.fuseau);
    g_zone_lisible = true;
    heure::Fuseau f;
    VERIFIER(!horloge_fuseau(f));
    VERIFIER(horloge_decalage_s(1793961600LL) == 0);
    return 0;
}

int une_zone_illisible_ne_donne_aucune_heure() {
    poser_zone("Europe/Paris");
    g_zone_lisible = false;
    char hhmm[8] = {'x'};
    VERIFIER(!horloge_hhmm(1793961600LL, hhmm, sizeof hhmm));
    VERIFIER(hhmm[0] == '\0');
    TexteHorloge t = {};
    VERIFIER(!horloge_formater(1793961600LL, t));
    heure::Fuseau f;
    VERIFIER(!horloge_fuseau(f));
    heure::HeureLocale h;
    VERIFIER(!horloge_heure_locale(1793961600LL, h));
    VERIFIER(horloge_decalage_s(1793961600LL) == 0);
    g_zone_lisible = true;
    return 0;
}

int les_tampons_trop_petits_et_nuls_sont_refuses() {
    poser_zone("Europe/Paris");
    char petit[5] = {'x'};
    VERIFIER(!horloge_hhmm(1793961600LL, petit, sizeof petit)); // « HH:MM » veut 6 octets
    VERIFIER(petit[0] == '\0');
    VERIFIER(!horloge_hhmm(1793961600LL, nullptr, 8));
    char un[1] = {'x'};
    VERIFIER(!horloge_hhmm(1793961600LL, un, 0));
    VERIFIER(!horloge_date_localisee(1793961600LL, i18n::Langue::Fr, nullptr, 48));
    char court[8] = {'x'};
    VERIFIER(!horloge_date_localisee(1793961600LL, i18n::Langue::Fr, court, sizeof court)); // la date ne tient pas
    VERIFIER(court[0] == '\0');
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(le_tableau_attendu_est_la)                                                                                       \
    X(heure_date_et_decalage_sont_ceux_du_lieu_selon_zoneinfo)                                                         \
    X(le_fuseau_rendu_convertit_chaque_instant_comme_zoneinfo)                                                         \
    X(un_changement_de_zone_change_l_heure_et_le_jour_a_l_appel_suivant)                                               \
    X(la_zone_est_relue_a_chaque_appel)                                                                                \
    X(un_fuseau_inconnu_ne_retombe_pas_sur_paris)                                                                      \
    X(un_nom_de_fuseau_sans_zero_final_ne_deborde_pas)                                                                 \
    X(une_zone_illisible_ne_donne_aucune_heure)                                                                        \
    X(les_tampons_trop_petits_et_nuls_sont_refuses)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
