// Tests du noyau pur : l'heure LOCALE DU LIEU, instant par instant (heure_lieu.hpp, ADR-0012).
// Les valeurs attendues viennent de Python zoneinfo (donnees/attendu_heure_lieu.inc, généré par controle_python_fuseaux.py) : des
// fenêtres de 24 colonnes à cheval sur un changement d'heure (New York le 1er novembre, Sydney le 4 octobre, Lord Howe et sa demi-heure)
// ou sur le nouvel an (Auckland, Honolulu), des fuseaux aux minutes non nulles (Kolkata, Katmandou, Chatham), des jours de prévision
// de 23 et 25 heures, et les levers et couchers RÉELS d'Open-Meteo écrits en « hh:mm » local.
#include <string_view>

#include "ecran/fuseaux.hpp"
#include "ecran/heure_lieu.hpp"
#include "mini.hpp"

using namespace ecran::heure;
namespace fuseaux = ecran::fuseaux;
using sv = std::string_view;

namespace {

constexpr sv ZONES[] = {
#define ATTENDU_LIEU_ZONES
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_ZONES
};
constexpr std::size_t NOMBRE_ZONES = sizeof(ZONES) / sizeof(ZONES[0]);

struct Colonne {
    int zone;
    long long instant;
    sv francais, autres;
    int heure, minute, decalage, jour_semaine;
};
constexpr Colonne COLONNES[] = {
#define ATTENDU_LIEU_FENETRES
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_FENETRES
};
constexpr std::size_t NOMBRE_COLONNES = sizeof(COLONNES) / sizeof(COLONNES[0]);

struct Jour {
    int zone;
    long long minuit;
    int jour_semaine;
    int ordinal; // date.toordinal() de Python : le 0001-01-01 vaut 1
};
constexpr Jour JOURS[] = {
#define ATTENDU_LIEU_JOURS
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_JOURS
};
constexpr std::size_t NOMBRE_JOURS = sizeof(JOURS) / sizeof(JOURS[0]);
constexpr long long ORDINAL_1970_01_01 = 719163;

struct Lever {
    int zone;
    long long instant;
    sv hhmm;
};
constexpr Lever LEVERS[] = {
#define ATTENDU_LIEU_LEVERS
#include "donnees/attendu_heure_lieu.inc"
#undef ATTENDU_LIEU_LEVERS
};
constexpr std::size_t NOMBRE_LEVERS = sizeof(LEVERS) / sizeof(LEVERS[0]);

constexpr sv texte(const char *tampon, std::size_t n) { return sv(tampon, n); }

constexpr AnalyseTz fuseau_de(int zone) { return analyser_tz(fuseaux::posix_pour(ZONES[zone])); }

// Indice d'une zone dans la liste de Python (-1 si elle n'y est pas).
constexpr int indice_zone(sv nom) {
    for (std::size_t z = 0; z < NOMBRE_ZONES; ++z) {
        if (ZONES[z] == nom) {
            return static_cast<int>(z);
        }
    }
    return -1;
}

// ---- 1. les valeurs de Python sont là ---------------------------------------------------------------------------------------

constexpr int les_lignes_python_sont_la() {
    VERIFIER(NOMBRE_ZONES == 14);
    VERIFIER(NOMBRE_COLONNES == 17 * 24); // 17 fenêtres de 24 colonnes
    VERIFIER(NOMBRE_JOURS == 73);
    VERIFIER(NOMBRE_LEVERS == 98); // levers et couchers réels d'Open-Meteo (nuit polaire exclue)
    for (std::size_t z = 0; z < NOMBRE_ZONES; ++z) {
        VERIFIER(fuseau_de(static_cast<int>(z)).ok()); // chaque zone de Python est dans la table et se lit
    }
    return 0;
}

// ---- 2. les colonnes « heure par heure » ------------------------------------------------------------------------------------

constexpr int les_colonnes_sont_celles_de_zoneinfo() {
    // Toutes les colonnes des 17 fenêtres : heure et minute locales, décalage en vigueur à CET instant, jour de la semaine, puis les
    // deux étiquettes (français, autres langues).
    for (const Colonne &c : COLONNES) {
        const AnalyseTz a = fuseau_de(c.zone);
        VERIFIER(a.ok());
        const HeureLocale h = en_heure_locale(c.instant, a.fuseau);
        VERIFIER(h.heure == c.heure);
        VERIFIER(h.minute == c.minute);
        VERIFIER(h.decalage_s == c.decalage);
        VERIFIER(h.jour_semaine == c.jour_semaine);
        char t[16] = {};
        VERIFIER(texte(t, formater_etiquette_heure(h, true, t, sizeof t)) == c.francais);
        VERIFIER(texte(t, formater_etiquette_heure(h, false, t, sizeof t)) == c.autres);
    }
    return 0;
}

// Le décalage d'une fenêtre à cheval sur un changement d'heure CHANGE dans la fenêtre : un décalage unique, pris au début (comme
// faisait l'écran), donnerait des colonnes fausses. On le montre sur New York le 1er novembre et sur Sydney le 4 octobre.
constexpr int un_seul_decalage_donnerait_des_colonnes_fausses(int zone, long long debut) {
    const AnalyseTz a = fuseau_de(zone);
    VERIFIER(a.ok());
    const std::int32_t au_debut = en_heure_locale(debut, a.fuseau).decalage_s;
    const Fuseau fixe = fuseau_fixe(au_debut);
    int justes_en_fixe = 0;
    int justes_par_instant = 0;
    int vues = 0;
    for (const Colonne &c : COLONNES) {
        if (c.zone != zone || c.instant < debut || c.instant >= debut + 24 * 3600) {
            continue;
        }
        ++vues;
        char t[16] = {};
        if (texte(t, formater_etiquette_heure(en_heure_locale(c.instant, fixe), true, t, sizeof t)) == c.francais) {
            ++justes_en_fixe;
        }
        if (texte(t, formater_etiquette_heure(en_heure_locale(c.instant, a.fuseau), true, t, sizeof t)) == c.francais) {
            ++justes_par_instant;
        }
    }
    VERIFIER(vues == 24);
    VERIFIER(justes_par_instant == 24);
    VERIFIER(justes_en_fixe < 24); // le défaut d'avant : au moins une colonne fausse après le changement d'heure
    VERIFIER(justes_en_fixe >= 1); // et celles d'avant le changement étaient justes
    return 0;
}
constexpr int new_york_le_1er_novembre_2026() {
    return un_seul_decalage_donnerait_des_colonnes_fausses(indice_zone("America/New_York"), 1793502000LL);
}
constexpr int sydney_le_4_octobre_2026() {
    return un_seul_decalage_donnerait_des_colonnes_fausses(indice_zone("Australia/Sydney"), 1791036000LL);
}

constexpr int new_york_repete_une_heure_et_sydney_en_saute_une() {
    // 2026-11-01 03:00 UTC = 23:00 EDT du 31 octobre : …, 23 h, 0 h, 1 h, 1 h (deuxième fois, EST), 2 h, …
    const AnalyseTz ny = fuseau_de(indice_zone("America/New_York"));
    VERIFIER(ny.ok());
    char t[16] = {};
    const char *attendu_ny[] = {"23h", "0h", "1h", "1h", "2h", "3h"};
    for (int i = 0; i < 6; ++i) {
        const HeureLocale h = en_heure_locale(1793502000LL + 3600LL * i, ny.fuseau);
        VERIFIER(texte(t, formater_etiquette_heure(h, true, t, sizeof t)) == attendu_ny[i]);
    }
    // 2026-10-03 14:00 UTC = 00:00 AEST du 4 octobre : 0 h, 1 h, 3 h (la 2 h n'existe pas), 4 h
    const AnalyseTz sy = fuseau_de(indice_zone("Australia/Sydney"));
    VERIFIER(sy.ok());
    const char *attendu_sy[] = {"0h", "1h", "3h", "4h"};
    for (int i = 0; i < 4; ++i) {
        const HeureLocale h = en_heure_locale(1791036000LL + 3600LL * i, sy.fuseau);
        VERIFIER(texte(t, formater_etiquette_heure(h, true, t, sizeof t)) == attendu_sy[i]);
    }
    return 0;
}

constexpr int les_fuseaux_a_la_demi_heure_et_au_quart_d_heure_donnent_hh_mm() {
    char t[16] = {};
    // Kolkata, 2026-11-06 10:00 UTC = 15:30 IST ; Katmandou = 15:45 ; Chatham = 23:45
    const AnalyseTz kolkata = analyser_tz(fuseaux::posix_pour("Asia/Kolkata"));
    const AnalyseTz katmandou = analyser_tz(fuseaux::posix_pour("Asia/Kathmandu"));
    const AnalyseTz chatham = analyser_tz(fuseaux::posix_pour("Pacific/Chatham"));
    VERIFIER(kolkata.ok() && katmandou.ok() && chatham.ok());
    constexpr long long T = 1793959200LL; // 2026-11-06 10:00:00 UTC
    VERIFIER(texte(t, formater_etiquette_heure(en_heure_locale(T, kolkata.fuseau), true, t, sizeof t)) == "15:30");
    VERIFIER(texte(t, formater_etiquette_heure(en_heure_locale(T, kolkata.fuseau), false, t, sizeof t)) == "15:30");
    VERIFIER(texte(t, formater_etiquette_heure(en_heure_locale(T, katmandou.fuseau), true, t, sizeof t)) == "15:45");
    VERIFIER(texte(t, formater_etiquette_heure(en_heure_locale(T, chatham.fuseau), true, t, sizeof t)) == "23:45");
    // Une colonne à l'heure pleine LOCALE (ce que renvoie Open-Meteo) reste « 16h » à Kolkata.
    VERIFIER(texte(t, formater_etiquette_heure(en_heure_locale(T + 1800, kolkata.fuseau), true, t, sizeof t)) == "16h");
    return 0;
}

// ---- 3. formats : cas écrits à la main --------------------------------------------------------------------------------------

constexpr HeureLocale a(int heure, int minute) {
    HeureLocale h;
    h.heure = heure;
    h.minute = minute;
    return h;
}

constexpr int etiquette_cas_limites() {
    char t[16] = {};
    VERIFIER(texte(t, formater_etiquette_heure(a(0, 0), true, t, sizeof t)) == "0h");
    VERIFIER(texte(t, formater_etiquette_heure(a(0, 0), false, t, sizeof t)) == "0:00");
    VERIFIER(texte(t, formater_etiquette_heure(a(7, 0), true, t, sizeof t)) == "7h");
    VERIFIER(texte(t, formater_etiquette_heure(a(7, 0), false, t, sizeof t)) == "7:00");
    VERIFIER(texte(t, formater_etiquette_heure(a(23, 0), true, t, sizeof t)) == "23h");
    VERIFIER(texte(t, formater_etiquette_heure(a(23, 0), false, t, sizeof t)) == "23:00");
    VERIFIER(texte(t, formater_etiquette_heure(a(5, 30), true, t, sizeof t)) == "05:30");
    VERIFIER(texte(t, formater_etiquette_heure(a(5, 30), false, t, sizeof t)) == "05:30");
    VERIFIER(texte(t, formater_etiquette_heure(a(0, 15), true, t, sizeof t)) == "00:15");
    VERIFIER(texte(t, formater_etiquette_heure(a(23, 59), false, t, sizeof t)) == "23:59");
    VERIFIER(texte(t, formater_etiquette_heure(a(12, 1), true, t, sizeof t)) == "12:01");
    return 0;
}

constexpr int etiquette_capacite_et_valeurs_hors_limites() {
    char t[16] = {};
    // « 7h » tient dans 3 octets, pas dans 2 ; « 05:30 » dans 6, pas dans 5. En cas d'échec : 0 et chaîne vide.
    VERIFIER(formater_etiquette_heure(a(7, 0), true, t, 3) == 2);
    VERIFIER(texte(t, 2) == "7h");
    t[0] = 'x';
    VERIFIER(formater_etiquette_heure(a(7, 0), true, t, 2) == 0 && t[0] == '\0');
    VERIFIER(formater_etiquette_heure(a(5, 30), true, t, 6) == 5);
    t[0] = 'x';
    VERIFIER(formater_etiquette_heure(a(5, 30), true, t, 5) == 0 && t[0] == '\0');
    VERIFIER(formater_etiquette_heure(a(7, 0), false, t, 5) == 4 && texte(t, 4) == "7:00");
    t[0] = 'x';
    VERIFIER(formater_etiquette_heure(a(7, 0), false, t, 4) == 0 && t[0] == '\0');
    VERIFIER(formater_etiquette_heure(a(24, 0), true, t, sizeof t) == 0);
    VERIFIER(formater_etiquette_heure(a(-1, 0), true, t, sizeof t) == 0);
    VERIFIER(formater_etiquette_heure(a(5, 60), true, t, sizeof t) == 0);
    VERIFIER(formater_etiquette_heure(a(5, -1), true, t, sizeof t) == 0);
    VERIFIER(formater_etiquette_heure(a(5, 30), true, nullptr, 16) == 0);
    VERIFIER(formater_etiquette_heure(a(5, 30), true, t, 0) == 0);
    return 0;
}

constexpr int hh_mm_cas_limites() {
    char t[16] = {};
    VERIFIER(texte(t, formater_hhmm(a(0, 0), t, sizeof t)) == "00:00");
    VERIFIER(texte(t, formater_hhmm(a(7, 12), t, sizeof t)) == "07:12");
    VERIFIER(texte(t, formater_hhmm(a(23, 59), t, sizeof t)) == "23:59");
    VERIFIER(formater_hhmm(a(7, 12), t, 6) == 5);
    t[0] = 'x';
    VERIFIER(formater_hhmm(a(7, 12), t, 5) == 0 && t[0] == '\0');
    VERIFIER(formater_hhmm(a(24, 0), t, sizeof t) == 0);
    VERIFIER(formater_hhmm(a(-1, 0), t, sizeof t) == 0);
    VERIFIER(formater_hhmm(a(1, 60), t, sizeof t) == 0);
    VERIFIER(formater_hhmm(a(1, -1), t, sizeof t) == 0);
    VERIFIER(formater_hhmm(a(1, 1), nullptr, 16) == 0);
    return 0;
}

constexpr int les_levers_et_couchers_reels_en_hh_mm_local() {
    // 98 instants réels d'Open-Meteo (Paris, Lyon, Seattle, Singapour, Sydney, Katmandou, Terre-Neuve…), lus dans le fuseau du lieu.
    for (const Lever &l : LEVERS) {
        const AnalyseTz z = fuseau_de(l.zone);
        VERIFIER(z.ok());
        char t[16] = {};
        VERIFIER(texte(t, formater_hhmm(en_heure_locale(l.instant, z.fuseau), t, sizeof t)) == l.hhmm);
    }
    return 0;
}

// ---- 4. jours de prévision --------------------------------------------------------------------------------------------------

constexpr int le_jour_de_la_semaine_vient_du_midi_local() {
    for (const Jour &j : JOURS) {
        const AnalyseTz z = fuseau_de(j.zone);
        VERIFIER(z.ok());
        VERIFIER(jour_semaine_du_jour(j.minuit, z.fuseau) == j.jour_semaine);
        // le numéro du jour local est celui de la date de Python
        VERIFIER(numero_jour_local(j.minuit, z.fuseau) == j.ordinal - ORDINAL_1970_01_01);
        // La date d'un jour de prévision est « minuit local » calculé par la météo avec UN décalage (celui de la réponse) : après un
        // changement d'heure elle est fausse d'une heure (ou d'une demi-heure à Lord Howe), parfois la veille à 23 h. Le jour de la
        // semaine doit rester celui du jour, parce qu'il est lu au midi local et non au « minuit ».
        for (const int erreur_s : {-3600, -1800, 1800, 3600}) {
            VERIFIER(jour_semaine_du_jour(j.minuit + erreur_s, z.fuseau) == j.jour_semaine);
        }
    }
    return 0;
}

constexpr int le_numero_du_jour_local_survit_aux_journees_de_23_et_25_heures() {
    // Pour deux jours consécutifs d'une même zone : la dernière seconde du jour est au même numéro que son minuit, le minuit
    // suivant est au numéro suivant. À New York, le 1er novembre 2026 dure 25 h et le 8 mars 2026 en dure 23.
    int jours_speciaux = 0;
    for (std::size_t i = 0; i + 1 < NOMBRE_JOURS; ++i) {
        const Jour &j = JOURS[i];
        const Jour &suivant = JOURS[i + 1];
        if (suivant.zone != j.zone || suivant.ordinal != j.ordinal + 1) {
            continue;
        }
        const AnalyseTz z = fuseau_de(j.zone);
        VERIFIER(z.ok());
        VERIFIER(numero_jour_local(suivant.minuit - 1, z.fuseau) == numero_jour_local(j.minuit, z.fuseau));
        VERIFIER(numero_jour_local(suivant.minuit, z.fuseau) == numero_jour_local(j.minuit, z.fuseau) + 1);
        // 86400 s après le minuit : c'est ce que supposait « aujourd'hui » avant. Faux les jours de 23, 23,5, 24,5 et 25 heures :
        // on tombe encore dans le même jour si et seulement si le jour dure plus de 86 400 s.
        const long long longueur = suivant.minuit - j.minuit;
        VERIFIER(longueur >= 82800 && longueur <= 90000);
        const bool meme_jour = numero_jour_local(j.minuit + 86400, z.fuseau) == numero_jour_local(j.minuit, z.fuseau);
        VERIFIER(meme_jour == (longueur > 86400));
        if (longueur != 86400) {
            ++jours_speciaux;
        }
    }
    VERIFIER(jours_speciaux >= 4); // New York ×2, Sydney (le 4 octobre dure 23 h), Lord Howe (23,5 h)
    return 0;
}

// ---- 5. fuseau fixe et clé ----------------------------------------------------------------------------------------------------

constexpr int le_fuseau_fixe_ne_change_jamais_d_heure() {
    const Fuseau f = fuseau_fixe(19800); // +5:30
    VERIFIER(!f.a_heure_ete && f.decalage_std_s == 19800 && f.decalage_ete_s == 19800);
    const HeureLocale h = en_heure_locale(1793959200LL, f); // 2026-11-06 10:00:00 UTC
    VERIFIER(h.heure == 15 && h.minute == 30 && h.decalage_s == 19800 && !h.heure_ete);
    const HeureLocale ete = en_heure_locale(1782820800LL, f); // 2026-06-30 12:00 UTC
    VERIFIER(ete.heure == 17 && ete.minute == 30 && ete.decalage_s == 19800);
    const Fuseau ouest = fuseau_fixe(-12600); // Terre-Neuve l'hiver
    VERIFIER(en_heure_locale(1793959200LL, ouest).heure == 6 && en_heure_locale(1793959200LL, ouest).minute == 30);
    VERIFIER(fuseau_fixe(0).decalage_std_s == 0);
    return 0;
}

constexpr bool meme(const Fuseau &x, const Fuseau &y) {
    return x.decalage_std_s == y.decalage_std_s && x.decalage_ete_s == y.decalage_ete_s &&
           x.a_heure_ete == y.a_heure_ete &&
           (!x.a_heure_ete ||
            (x.debut.mois == y.debut.mois && x.debut.semaine == y.debut.semaine && x.debut.jour == y.debut.jour &&
             x.debut.heure_s == y.debut.heure_s && x.fin.mois == y.fin.mois && x.fin.semaine == y.fin.semaine &&
             x.fin.jour == y.fin.jour && x.fin.heure_s == y.fin.heure_s));
}

constexpr int la_cle_suit_chaque_champ_du_fuseau() {
    const AnalyseTz paris = analyser_tz(fuseaux::posix_pour("Europe/Paris"));
    VERIFIER(paris.ok());
    const std::uint32_t cle = cle_du_fuseau(paris.fuseau);
    VERIFIER(cle == cle_du_fuseau(paris.fuseau)); // déterministe
    Fuseau f = paris.fuseau;
    f.decalage_std_s += 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.decalage_ete_s += 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.a_heure_ete = false;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.debut.mois += 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.debut.semaine -= 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.debut.jour += 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.debut.heure_s += 3600;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.fin.mois -= 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.fin.semaine -= 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.fin.jour += 1;
    VERIFIER(cle_du_fuseau(f) != cle);
    f = paris.fuseau;
    f.fin.heure_s += 3600;
    VERIFIER(cle_du_fuseau(f) != cle);
    // Sans heure d'été, les règles ne comptent pas (elles ne sont pas lues) ; le décalage compte.
    Fuseau sans = fuseau_fixe(3600);
    Fuseau sans_avec_regles = sans;
    sans_avec_regles.debut.mois = 3;
    VERIFIER(cle_du_fuseau(sans) == cle_du_fuseau(sans_avec_regles));
    VERIFIER(cle_du_fuseau(fuseau_fixe(3600)) != cle_du_fuseau(fuseau_fixe(7200)));
    return 0;
}

constexpr int deux_fuseaux_de_la_table_ont_la_meme_cle_seulement_s_ils_sont_identiques() {
    constexpr sv noms[] = {"Europe/Paris",     "Europe/Berlin",    "America/New_York", "Asia/Kolkata",
                           "Asia/Kathmandu",   "Asia/Tokyo",       "Australia/Sydney", "Australia/Lord_Howe",
                           "Pacific/Chatham",  "Pacific/Auckland", "Europe/Dublin",    "America/Caracas",
                           "Pacific/Honolulu", "Africa/Casablanca"};
    for (sv x : noms) {
        for (sv y : noms) {
            const AnalyseTz ax = analyser_tz(fuseaux::posix_pour(x));
            const AnalyseTz ay = analyser_tz(fuseaux::posix_pour(y));
            VERIFIER(ax.ok() && ay.ok());
            VERIFIER((cle_du_fuseau(ax.fuseau) == cle_du_fuseau(ay.fuseau)) == meme(ax.fuseau, ay.fuseau));
        }
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_lignes_python_sont_la)                                                                                       \
    X(les_colonnes_sont_celles_de_zoneinfo)                                                                            \
    X(new_york_le_1er_novembre_2026)                                                                                   \
    X(sydney_le_4_octobre_2026)                                                                                        \
    X(new_york_repete_une_heure_et_sydney_en_saute_une)                                                                \
    X(les_fuseaux_a_la_demi_heure_et_au_quart_d_heure_donnent_hh_mm)                                                   \
    X(etiquette_cas_limites)                                                                                           \
    X(etiquette_capacite_et_valeurs_hors_limites)                                                                      \
    X(hh_mm_cas_limites)                                                                                               \
    X(les_levers_et_couchers_reels_en_hh_mm_local)                                                                     \
    X(le_jour_de_la_semaine_vient_du_midi_local)                                                                       \
    X(le_numero_du_jour_local_survit_aux_journees_de_23_et_25_heures)                                                  \
    X(le_fuseau_fixe_ne_change_jamais_d_heure)                                                                         \
    X(la_cle_suit_chaque_champ_du_fuseau)                                                                              \
    X(deux_fuseaux_de_la_table_ont_la_meme_cle_seulement_s_ils_sont_identiques)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
