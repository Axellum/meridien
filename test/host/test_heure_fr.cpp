// Tests du noyau pur : heure locale depuis une chaîne POSIX TZ + formats français (heure_fr.hpp).
// Les heures attendues viennent de Python zoneinfo (donnees/attendu_heure.inc, généré par controle_python.py) pour
// Europe/Paris, America/New_York, Asia/Kolkata, Australia/Sydney, Pacific/Chatham, Europe/Dublin, America/Godthab
// et Australia/Lord_Howe, juste avant / pendant / juste après chaque changement d'heure de 2026 à 2028.
#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"
#include "mini.hpp"

using namespace ecran::heure;
namespace fuseaux = ecran::fuseaux;
using sv = std::string_view;

namespace {

constexpr sv ZONES[] = {
#define ATTENDU_HEURE_ZONES
#include "donnees/attendu_heure.inc"
#undef ATTENDU_HEURE_ZONES
};

struct Ligne {
    int zone;
    long long instant;
    int an, mois, jour, heure, minute, seconde, jour_semaine, decalage;
    bool ete;
};
constexpr Ligne LIGNES[] = {
#define ATTENDU_HEURE_LIGNES
#include "donnees/attendu_heure.inc"
#undef ATTENDU_HEURE_LIGNES
};
constexpr std::size_t NOMBRE_LIGNES = sizeof(LIGNES) / sizeof(LIGNES[0]);

constexpr sv texte(const char* tampon, std::size_t n) { return sv(tampon, n); }

// ---- 1. les valeurs de Python ----------------------------------------------------------------------------------------

constexpr int les_lignes_python_sont_la() {
    VERIFIER(sizeof(ZONES) / sizeof(ZONES[0]) == 8);
    VERIFIER(NOMBRE_LIGNES == 1093);
    return 0;
}

constexpr int meme_ligne(const Ligne& l, const Fuseau& f) {
    const HeureLocale h = en_heure_locale(l.instant, f);
    VERIFIER(h.an == l.an);
    VERIFIER(h.mois == l.mois);
    VERIFIER(h.jour == l.jour);
    VERIFIER(h.heure == l.heure);
    VERIFIER(h.minute == l.minute);
    VERIFIER(h.seconde == l.seconde);
    VERIFIER(h.jour_semaine == l.jour_semaine);
    VERIFIER(h.decalage_s == l.decalage);
    VERIFIER(h.heure_ete == l.ete);
    return 0;
}

// Les 1 094 instants, zone par zone. On analyse la chaîne POSIX de fuseaux.hpp une fois par zone.
constexpr int heures_locales_identiques_a_zoneinfo(int zone) {
    const sv posix = fuseaux::posix_pour(ZONES[zone]);
    VERIFIER(!posix.empty());
    const AnalyseTz a = analyser_tz(posix);
    VERIFIER(a.ok());
    int vues = 0;
    for (const Ligne& l : LIGNES) {
        if (l.zone != zone) continue;
        const int r = meme_ligne(l, a.fuseau);
        if (r != 0) return r;
        ++vues;
    }
    VERIFIER(vues > 50);    // la zone a bien ses lignes (grille + changements d'heure)
    return 0;
}
constexpr int heure_paris() { return heures_locales_identiques_a_zoneinfo(0); }
constexpr int heure_new_york() { return heures_locales_identiques_a_zoneinfo(1); }
constexpr int heure_kolkata() { return heures_locales_identiques_a_zoneinfo(2); }
constexpr int heure_sydney() { return heures_locales_identiques_a_zoneinfo(3); }
constexpr int heure_chatham() { return heures_locales_identiques_a_zoneinfo(4); }
constexpr int heure_dublin() { return heures_locales_identiques_a_zoneinfo(5); }
constexpr int heure_godthab() { return heures_locales_identiques_a_zoneinfo(6); }
constexpr int heure_lord_howe() { return heures_locales_identiques_a_zoneinfo(7); }

constexpr int convertir_direct_depuis_la_chaine() {
    // Même chose par le raccourci `convertir` (chaîne -> résultat), sur toutes les lignes.
    for (const Ligne& l : LIGNES) {
        const Local r = convertir(l.instant, fuseaux::posix_pour(ZONES[l.zone]));
        VERIFIER(r.ok());
        VERIFIER(r.heure.heure == l.heure && r.heure.minute == l.minute && r.heure.decalage_s == l.decalage);
    }
    return 0;
}

constexpr int le_cas_de_la_consigne_paris_11_h_40() {
    // 2026-11-06 10:40:00 UTC = 11 h 40 à Paris (CET, +1 h), un vendredi.
    const Local l = convertir(1793961600, "CET-1CEST,M3.5.0,M10.5.0/3");
    VERIFIER(l.ok());
    VERIFIER(l.heure.an == 2026 && l.heure.mois == 11 && l.heure.jour == 6 && l.heure.heure == 11 && l.heure.minute == 40);
    VERIFIER(l.heure.jour_semaine == 5 && !l.heure.heure_ete && l.heure.decalage_s == 3600);
    char t[32] = {};
    VERIFIER(texte(t, formater_heure(l.heure, t, sizeof t)) == "11 h 40");
    VERIFIER(texte(t, formater_date_longue(l.heure, t, sizeof t)) == "vendredi 6 novembre");
    VERIFIER(texte(t, formater_date_longue(l.heure, t, sizeof t, true)) == "vendredi 6 novembre 2026");
    return 0;
}

// ---- 2. changements d'heure, valeurs écrites à la main (vérifiées sur la règle, pas sur le code) ----------------

constexpr int paris_autour_du_changement_de_mars_2026() {
    // Passage à l'heure d'été : dimanche 29 mars 2026 à 02:00 CET = 01:00 UTC (1774746000).
    constexpr long long T = 1774746000;
    const AnalyseTz a = analyser_tz("CET-1CEST,M3.5.0,M10.5.0/3");
    VERIFIER(a.ok());
    const HeureLocale avant = en_heure_locale(T - 1, a.fuseau);
    VERIFIER(avant.heure == 1 && avant.minute == 59 && avant.seconde == 59 && !avant.heure_ete && avant.decalage_s == 3600);
    VERIFIER(avant.jour == 29 && avant.mois == 3 && avant.jour_semaine == 0);
    const HeureLocale pile = en_heure_locale(T, a.fuseau);
    VERIFIER(pile.heure == 3 && pile.minute == 0 && pile.seconde == 0 && pile.heure_ete && pile.decalage_s == 7200);
    const HeureLocale apres = en_heure_locale(T + 1, a.fuseau);
    VERIFIER(apres.heure == 3 && apres.seconde == 1 && apres.heure_ete);
    return 0;
}

constexpr int paris_autour_du_changement_d_octobre_2026() {
    // Retour à l'heure d'hiver : dimanche 25 octobre 2026 à 03:00 CEST = 01:00 UTC (1792890000).
    constexpr long long T = 1792890000;
    const AnalyseTz a = analyser_tz("CET-1CEST,M3.5.0,M10.5.0/3");
    VERIFIER(a.ok());
    const HeureLocale avant = en_heure_locale(T - 1, a.fuseau);
    VERIFIER(avant.heure == 2 && avant.minute == 59 && avant.seconde == 59 && avant.heure_ete && avant.decalage_s == 7200);
    const HeureLocale pile = en_heure_locale(T, a.fuseau);
    VERIFIER(pile.heure == 2 && pile.minute == 0 && !pile.heure_ete && pile.decalage_s == 3600);   // 02:00 deux fois
    VERIFIER(pile.jour == 25 && pile.mois == 10 && pile.jour_semaine == 0);
    return 0;
}

constexpr int hemisphere_sud_sydney() {
    const AnalyseTz a = analyser_tz("AEST-10AEDT,M10.1.0,M4.1.0/3");
    VERIFIER(a.ok());
    VERIFIER(a.fuseau.decalage_std_s == 36000 && a.fuseau.decalage_ete_s == 39600);
    // Janvier (été austral) : heure d'été ; juillet : standard.
    VERIFIER(en_heure_locale(1768478400, a.fuseau).heure_ete);    // 2026-01-15 12:00 UTC
    VERIFIER(!en_heure_locale(1784116800, a.fuseau).heure_ete);   // 2026-07-15 12:00 UTC
    VERIFIER(en_heure_locale(1768478400, a.fuseau).decalage_s == 39600);
    return 0;
}

// ---- 3. analyse de la chaîne POSIX ------------------------------------------------------------------------------------

constexpr int analyse_des_formes_connues() {
    {
        const AnalyseTz a = analyser_tz("CET-1CEST,M3.5.0,M10.5.0/3");
        VERIFIER(a.ok() && a.fuseau.nom_std == "CET" && a.fuseau.nom_ete == "CEST");
        VERIFIER(a.fuseau.decalage_std_s == 3600 && a.fuseau.decalage_ete_s == 7200 && a.fuseau.a_heure_ete);
        VERIFIER(a.fuseau.debut.mois == 3 && a.fuseau.debut.semaine == 5 && a.fuseau.debut.jour == 0);
        VERIFIER(a.fuseau.debut.heure_s == 7200);    // 2 h par défaut
        VERIFIER(a.fuseau.fin.mois == 10 && a.fuseau.fin.heure_s == 10800);
    }
    {   // pas d'heure d'été
        const AnalyseTz a = analyser_tz("IST-5:30");
        VERIFIER(a.ok() && !a.fuseau.a_heure_ete && a.fuseau.decalage_std_s == 19800 && a.fuseau.decalage_ete_s == 19800);
        VERIFIER(a.fuseau.nom_std == "IST");
    }
    {   // nom entre <> et décalage à l'ouest, sans heure d'été
        const AnalyseTz a = analyser_tz("<-03>3");
        VERIFIER(a.ok() && a.fuseau.nom_std == "-03" && a.fuseau.decalage_std_s == -10800 && !a.fuseau.a_heure_ete);
    }
    {   // décalage d'été par défaut = +1 h ; New York
        const AnalyseTz a = analyser_tz("EST5EDT,M3.2.0,M11.1.0");
        VERIFIER(a.ok() && a.fuseau.decalage_std_s == -18000 && a.fuseau.decalage_ete_s == -14400);
        VERIFIER(a.fuseau.debut.semaine == 2 && a.fuseau.fin.mois == 11 && a.fuseau.fin.semaine == 1);
    }
    {   // Terre-Neuve : demi-heure
        const AnalyseTz a = analyser_tz("NST3:30NDT,M3.2.0,M11.1.0");
        VERIFIER(a.ok() && a.fuseau.decalage_std_s == -12600 && a.fuseau.decalage_ete_s == -9000);
    }
    {   // Groenland : heures de changement négative et nulle
        const AnalyseTz a = analyser_tz("<-02>2<-01>,M3.5.0/-1,M10.5.0/0");
        VERIFIER(a.ok() && a.fuseau.debut.heure_s == -3600 && a.fuseau.fin.heure_s == 0);
        VERIFIER(a.fuseau.decalage_std_s == -7200 && a.fuseau.decalage_ete_s == -3600);
    }
    {   // Chatham : 45 minutes, heures de changement avec minutes
        const AnalyseTz a = analyser_tz("<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45");
        VERIFIER(a.ok() && a.fuseau.decalage_std_s == 45900 && a.fuseau.decalage_ete_s == 49500);
        VERIFIER(a.fuseau.debut.heure_s == 2 * 3600 + 45 * 60 && a.fuseau.fin.heure_s == 3 * 3600 + 45 * 60);
    }
    {   // Dublin : « heure d'été » plus petite que la standard (IST toute l'année, GMT l'hiver)
        const AnalyseTz a = analyser_tz("IST-1GMT0,M10.5.0,M3.5.0/1");
        VERIFIER(a.ok() && a.fuseau.decalage_std_s == 3600 && a.fuseau.decalage_ete_s == 0);
    }
    {   // décalage d'été explicite et secondes
        const AnalyseTz a = analyser_tz("AAA-1:00:30BBB-3:15:10,M1.1.1/0:00:05,M12.5.6/23:59:59");
        VERIFIER(a.ok() && a.fuseau.decalage_std_s == 3630 && a.fuseau.decalage_ete_s == 11710);
        VERIFIER(a.fuseau.debut.heure_s == 5 && a.fuseau.fin.heure_s == 86399);
    }
    return 0;
}

constexpr int chaines_posix_invalides_refusees() {
    struct Cas {
        sv chaine;
        ErreurTz erreur;
    };
    constexpr Cas CAS[] = {
        {"", ErreurTz::Vide},
        {"CET", ErreurTz::DecalageInvalide},
        {"CET-", ErreurTz::DecalageInvalide},
        {"C-1", ErreurTz::NomInvalide},
        {"-1", ErreurTz::NomInvalide},
        {"<>-1", ErreurTz::NomInvalide},
        {"<a b>1", ErreurTz::NomInvalide},
        {"<+01", ErreurTz::NomInvalide},
        {"CET-168", ErreurTz::DecalageInvalide},
        {"CET-1:60", ErreurTz::DecalageInvalide},
        {"CET-1:5", ErreurTz::DecalageInvalide},
        {"CET-1:30:60", ErreurTz::DecalageInvalide},
        {"CET-1CEST", ErreurTz::RegleInvalide},                       // heure d'été sans règle : non gérée
        {"CET-1CEST,M3.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,M3.5.0,", ErreurTz::RegleInvalide},
        {"CET-1CEST,M13.5.0,M10.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,M0.5.0,M10.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,M3.6.0,M10.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,M3.0.0,M10.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,M3.5.7,M10.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,J60,J300", ErreurTz::RegleInvalide},              // formes Jn et n : non gérées
        {"CET-1CEST,60,300", ErreurTz::RegleInvalide},
        {"CET-1CEST,M3.5.0/,M10.5.0", ErreurTz::RegleInvalide},
        {"CET-1CEST,M3.5.0,M10.5.0,M1.1.1", ErreurTz::TexteEnTrop},
        {"CET-1CEST,M3.5.0,M10.5.0 ", ErreurTz::TexteEnTrop},
        {"IST-5:30x", ErreurTz::NomInvalide},                          // « x » : nom d'été de 1 lettre
    };
    for (const Cas& c : CAS) {
        const AnalyseTz a = analyser_tz(c.chaine);
        VERIFIER(!a.ok());
        VERIFIER(a.erreur == c.erreur);
    }
    VERIFIER(message(ErreurTz::RegleInvalide) == "règle d'heure d'été invalide ou non gérée");
    VERIFIER(!convertir(0, "").ok() && convertir(0, "").erreur == ErreurTz::Vide);
    return 0;
}

constexpr int toutes_les_zones_de_la_table_sont_comprises() {
    int avec_ete = 0;
    for (std::size_t i = 0; i < fuseaux::NOMBRE; ++i) {
        const AnalyseTz a = analyser_tz(fuseaux::TABLE[i].posix);
        VERIFIER(a.ok());
        const Fuseau& f = a.fuseau;
        VERIFIER(f.decalage_std_s >= -12 * 3600 && f.decalage_std_s <= 14 * 3600);
        const int ecart = f.decalage_ete_s - f.decalage_std_s;
        if (f.a_heure_ete) {
            ++avec_ete;
            VERIFIER(ecart == 1800 || ecart == 3600 || ecart == 7200 || ecart == -3600);   // Lord Howe, le reste, Troll, Dublin
        } else {
            VERIFIER(ecart == 0);
        }
        // l'instant de chaque changement existe pour chaque année 2026..2028 : aucune dérive du calcul
        if (f.a_heure_ete) {
            for (int an = 2026; an <= 2028; ++an) {
                const int j1 = detail::jour_de_la_regle(an, f.debut);
                const int j2 = detail::jour_de_la_regle(an, f.fin);
                VERIFIER(j1 >= 1 && j1 <= 31 && j2 >= 1 && j2 <= 31);
                VERIFIER(detail::jour_semaine_de(detail::jours_depuis_epoch(an, f.debut.mois, j1)) == f.debut.jour);
                VERIFIER(detail::jour_semaine_de(detail::jours_depuis_epoch(an, f.fin.mois, j2)) == f.fin.jour);
            }
        }
    }
    VERIFIER(avec_ete > 100 && avec_ete < 200);   // 468 zones, dont un peu moins de la moitié avec heure d'été
    return 0;
}

// ---- 4. calendrier --------------------------------------------------------------------------------------------------------------

constexpr int calendrier_dates_connues() {
    VERIFIER(detail::jours_depuis_epoch(1970, 1, 1) == 0);
    VERIFIER(detail::jours_depuis_epoch(1970, 1, 2) == 1);
    VERIFIER(detail::jours_depuis_epoch(1969, 12, 31) == -1);
    VERIFIER(detail::jours_depuis_epoch(2000, 3, 1) == 11017);
    VERIFIER(detail::jours_depuis_epoch(2026, 11, 6) == 20763);   // (date(2026,11,6) - date(1970,1,1)).days en Python
    VERIFIER(detail::jours_depuis_epoch(2100, 3, 1) - detail::jours_depuis_epoch(2100, 2, 28) == 1);  // 2100 n'est pas bissextile
    VERIFIER(detail::jours_depuis_epoch(2000, 3, 1) - detail::jours_depuis_epoch(2000, 2, 28) == 2);  // 2000 l'est
    VERIFIER(detail::bissextile(2000) && !detail::bissextile(2100) && detail::bissextile(2028) && !detail::bissextile(2026));
    VERIFIER(detail::jours_du_mois(2028, 2) == 29 && detail::jours_du_mois(2026, 2) == 28 && detail::jours_du_mois(2026, 11) == 30);
    VERIFIER(detail::jour_semaine_de(0) == 4);       // jeudi
    VERIFIER(detail::jour_semaine_de(-1) == 3);      // mercredi
    VERIFIER(detail::jour_semaine_de(-7) == 4);
    return 0;
}

constexpr int calendrier_aller_retour() {
    // de 1900 à 2150, une date par semaine : chaque jour redonne sa date
    std::int64_t jours = detail::jours_depuis_epoch(1900, 1, 1);
    const std::int64_t fin = detail::jours_depuis_epoch(2150, 1, 1);
    std::int64_t an = 0;
    int mois = 0, jour = 0;
    int precedent_mois = 0, precedent_jour = 0;
    std::int64_t precedent_an = 0;
    for (; jours < fin; jours += 7) {   // pas d'une semaine : 13 000 dates couvrent tous les mois et jours de semaine
        detail::date_depuis_jours(jours, an, mois, jour);
        VERIFIER(detail::jours_depuis_epoch(an, mois, jour) == jours);
        VERIFIER(mois >= 1 && mois <= 12 && jour >= 1 && jour <= detail::jours_du_mois(an, mois));
        if (precedent_mois != 0) VERIFIER(an > precedent_an || mois != precedent_mois || jour > precedent_jour);
        precedent_an = an;
        precedent_mois = mois;
        precedent_jour = jour;
    }
    // jour par jour sur quatre ans autour de 2028 : le lendemain est « jour+1 » ou le 1er du mois suivant
    for (std::int64_t j = detail::jours_depuis_epoch(2026, 1, 1); j < detail::jours_depuis_epoch(2030, 1, 1); ++j) {
        std::int64_t a1 = 0, a2 = 0;
        int m1 = 0, d1 = 0, m2 = 0, d2 = 0;
        detail::date_depuis_jours(j, a1, m1, d1);
        detail::date_depuis_jours(j + 1, a2, m2, d2);
        if (d1 < detail::jours_du_mois(a1, m1)) {
            VERIFIER(a2 == a1 && m2 == m1 && d2 == d1 + 1);
        } else {
            VERIFIER(d2 == 1 && ((m1 == 12 && m2 == 1 && a2 == a1 + 1) || (m1 < 12 && m2 == m1 + 1 && a2 == a1)));
        }
        VERIFIER(detail::jour_semaine_de(j + 1) == (detail::jour_semaine_de(j) + 1) % 7);
    }
    // les jours de février d'une année bissextile, un par un
    for (int j = 27; j <= 29; ++j) {
        detail::date_depuis_jours(detail::jours_depuis_epoch(2028, 2, j), an, mois, jour);
        VERIFIER(an == 2028 && mois == 2 && jour == j);
    }
    detail::date_depuis_jours(detail::jours_depuis_epoch(2028, 2, 29) + 1, an, mois, jour);
    VERIFIER(an == 2028 && mois == 3 && jour == 1);
    detail::date_depuis_jours(detail::jours_depuis_epoch(2026, 12, 31) + 1, an, mois, jour);
    VERIFIER(an == 2027 && mois == 1 && jour == 1);
    return 0;
}

constexpr int instants_negatifs_et_lointains() {
    const AnalyseTz utc = analyser_tz("UTC0");
    VERIFIER(utc.ok());
    const HeureLocale h0 = en_heure_locale(0, utc.fuseau);
    VERIFIER(h0.an == 1970 && h0.mois == 1 && h0.jour == 1 && h0.heure == 0 && h0.jour_semaine == 4);
    const HeureLocale m1 = en_heure_locale(-1, utc.fuseau);
    VERIFIER(m1.an == 1969 && m1.mois == 12 && m1.jour == 31 && m1.heure == 23 && m1.minute == 59 && m1.seconde == 59);
    const HeureLocale m86400 = en_heure_locale(-86400, utc.fuseau);
    VERIFIER(m86400.an == 1969 && m86400.jour == 31 && m86400.heure == 0);
    const HeureLocale max32 = en_heure_locale(2147483647, utc.fuseau);   // 2038-01-19 03:14:07 UTC
    VERIFIER(max32.an == 2038 && max32.mois == 1 && max32.jour == 19 && max32.heure == 3 && max32.minute == 14 && max32.seconde == 7);
    const HeureLocale apres = en_heure_locale(2147483648LL, utc.fuseau);   // pas de dépassement 2038
    VERIFIER(apres.an == 2038 && apres.seconde == 8);
    return 0;
}

// ---- 5. formats français --------------------------------------------------------------------------------------------------------

constexpr HeureLocale h(int an, int mois, int jour, int jsem, int heure = 0, int minute = 0) {
    HeureLocale x;
    x.an = an;
    x.mois = mois;
    x.jour = jour;
    x.jour_semaine = jsem;
    x.heure = heure;
    x.minute = minute;
    return x;
}

constexpr int noms_des_jours_et_des_mois() {
    constexpr sv JOURS[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
    constexpr sv MOIS[] = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"};
    for (int i = 0; i < 7; ++i) VERIFIER(nom_jour(i) == JOURS[i]);
    for (int i = 1; i <= 12; ++i) VERIFIER(nom_mois(i) == MOIS[i - 1]);
    VERIFIER(nom_jour(-1).empty() && nom_jour(7).empty() && nom_mois(0).empty() && nom_mois(13).empty());
    return 0;
}

constexpr int date_longue_en_toutes_lettres() {
    char t[48] = {};
    VERIFIER(texte(t, formater_date_longue(h(2025, 11, 6, 4), t, sizeof t)) == "jeudi 6 novembre");
    VERIFIER(texte(t, formater_date_longue(h(2026, 11, 1, 0), t, sizeof t)) == "dimanche 1er novembre");
    VERIFIER(texte(t, formater_date_longue(h(2026, 11, 1, 0), t, sizeof t, true)) == "dimanche 1er novembre 2026");
    VERIFIER(texte(t, formater_date_longue(h(2026, 2, 28, 6), t, sizeof t)) == "samedi 28 février");
    VERIFIER(texte(t, formater_date_longue(h(2028, 2, 29, 2), t, sizeof t)) == "mardi 29 février");    // bissextile
    VERIFIER(texte(t, formater_date_longue(h(2026, 8, 15, 6), t, sizeof t)) == "samedi 15 août");
    VERIFIER(texte(t, formater_date_longue(h(2026, 12, 31, 4), t, sizeof t, true)) == "jeudi 31 décembre 2026");
    VERIFIER(texte(t, formater_date_longue(h(2026, 5, 2, 6), t, sizeof t)) == "samedi 2 mai");
    VERIFIER(texte(t, formater_date_longue(h(2026, 5, 11, 1), t, sizeof t)) == "lundi 11 mai");        // pas de « 1er » pour 11
    // jour de la semaine et mois accentués : taille en octets (é, û, è = 2 octets)
    VERIFIER(formater_date_longue(h(2026, 12, 21, 1), t, sizeof t) == sv("lundi 21 décembre").size());
    // refus : date impossible ou hors limites
    VERIFIER(formater_date_longue(h(2026, 0, 6, 4), t, sizeof t) == 0 && t[0] == '\0');
    VERIFIER(formater_date_longue(h(2026, 13, 6, 4), t, sizeof t) == 0);
    VERIFIER(formater_date_longue(h(2026, 2, 29, 4), t, sizeof t) == 0);      // 2026 n'est pas bissextile
    VERIFIER(formater_date_longue(h(2026, 4, 31, 4), t, sizeof t) == 0);
    VERIFIER(formater_date_longue(h(2026, 11, 0, 4), t, sizeof t) == 0);
    VERIFIER(formater_date_longue(h(2026, 11, 6, 7), t, sizeof t) == 0);
    VERIFIER(formater_date_longue(h(2026, 11, 6, -1), t, sizeof t) == 0);
    return 0;
}

constexpr int date_longue_capacite_exacte() {
    constexpr sv ATTENDU = "jeudi 6 novembre";   // 16 octets
    char exact[17] = {};
    VERIFIER(formater_date_longue(h(2025, 11, 6, 4), exact, sizeof exact) == 16 && sv(exact) == ATTENDU);
    char trop_court[16] = {'z'};
    VERIFIER(formater_date_longue(h(2025, 11, 6, 4), trop_court, sizeof trop_court) == 0 && trop_court[0] == '\0');
    char un[1] = {'z'};
    VERIFIER(formater_date_longue(h(2025, 11, 6, 4), un, 1) == 0 && un[0] == '\0');
    VERIFIER(formater_date_longue(h(2025, 11, 6, 4), nullptr, 0) == 0);
    VERIFIER(formater_date_longue(h(2025, 11, 6, 4), nullptr, 20) == 0);
    char tout_petit[2] = {'z', 'z'};
    VERIFIER(formater_date_longue(h(2025, 11, 6, 4), tout_petit, 0) == 0 && tout_petit[0] == 'z');   // rien écrit
    return 0;
}

constexpr int heure_24_h_avec_h_et_minutes_sur_deux_chiffres() {
    char t[16] = {};
    VERIFIER(texte(t, formater_heure(h(2026, 11, 6, 5, 11, 40), t, sizeof t)) == "11 h 40");
    VERIFIER(texte(t, formater_heure(h(2026, 11, 6, 5, 9, 5), t, sizeof t)) == "9 h 05");
    VERIFIER(texte(t, formater_heure(h(2026, 11, 6, 5, 0, 0), t, sizeof t)) == "0 h 00");
    VERIFIER(texte(t, formater_heure(h(2026, 11, 6, 5, 12, 0), t, sizeof t)) == "12 h 00");
    VERIFIER(texte(t, formater_heure(h(2026, 11, 6, 5, 23, 59), t, sizeof t)) == "23 h 59");
    VERIFIER(texte(t, formater_heure(h(2026, 11, 6, 5, 13, 7), t, sizeof t)) == "13 h 07");
    VERIFIER(formater_heure(h(2026, 11, 6, 5, 24, 0), t, sizeof t) == 0 && t[0] == '\0');
    VERIFIER(formater_heure(h(2026, 11, 6, 5, -1, 0), t, sizeof t) == 0);
    VERIFIER(formater_heure(h(2026, 11, 6, 5, 10, 60), t, sizeof t) == 0);
    char exact[8] = {};   // « 11 h 40 » = 7 octets + zéro
    VERIFIER(formater_heure(h(2026, 11, 6, 5, 11, 40), exact, sizeof exact) == 7);
    char court[7] = {};
    VERIFIER(formater_heure(h(2026, 11, 6, 5, 11, 40), court, sizeof court) == 0);
    return 0;
}

constexpr int temperature_arrondie_avec_le_signe_degre() {
    struct Cas {
        int dixiemes;
        sv entier;
        sv decimale;
        sv degre;
    };
    constexpr Cas CAS[] = {
        {116, "12 °C", "11,6 °C", "12°"},    {115, "12 °C", "11,5 °C", "12°"},   {114, "11 °C", "11,4 °C", "11°"},
        {0, "0 °C", "0,0 °C", "0°"},         {4, "0 °C", "0,4 °C", "0°"},        {5, "1 °C", "0,5 °C", "1°"},
        {-4, "0 °C", "-0,4 °C", "0°"},       {-5, "-1 °C", "-0,5 °C", "-1°"},    {-115, "-12 °C", "-11,5 °C", "-12°"},
        {-114, "-11 °C", "-11,4 °C", "-11°"}, {-31, "-3 °C", "-3,1 °C", "-3°"},   {97, "10 °C", "9,7 °C", "10°"},
        {1000, "100 °C", "100,0 °C", "100°"}, {-900, "-90 °C", "-90,0 °C", "-90°"}, {32767, "3277 °C", "3276,7 °C", "3277°"},
        {-32768, "-3277 °C", "-3276,8 °C", "-3277°"}, {2147483647, "214748365 °C", "214748364,7 °C", "214748365°"},
        {-2147483647 - 1, "-214748365 °C", "-214748364,8 °C", "-214748365°"},
    };
    char t[32] = {};
    for (const Cas& c : CAS) {
        VERIFIER(texte(t, formater_temperature(c.dixiemes, t, sizeof t)) == c.entier);
        VERIFIER(texte(t, formater_temperature(c.dixiemes, t, sizeof t, true)) == c.decimale);
        VERIFIER(texte(t, formater_degre(c.dixiemes, t, sizeof t)) == c.degre);
    }
    // le « ° » est bien U+00B0 (octets C2 B0) et le « C » suit
    formater_temperature(116, t, sizeof t);
    VERIFIER(static_cast<unsigned char>(t[3]) == 0xC2 && static_cast<unsigned char>(t[4]) == 0xB0 && t[5] == 'C');
    formater_degre(116, t, sizeof t);
    VERIFIER(static_cast<unsigned char>(t[2]) == 0xC2 && static_cast<unsigned char>(t[3]) == 0xB0 && t[4] == '\0');
    // places
    char exact[7] = {};   // « 12 °C » = 6 octets + zéro
    VERIFIER(formater_temperature(116, exact, sizeof exact) == 6);
    char court[6] = {'z'};
    VERIFIER(formater_temperature(116, court, sizeof court) == 0 && court[0] == '\0');
    VERIFIER(formater_degre(116, nullptr, 0) == 0);
    return 0;
}

}  // namespace

#define LISTE(X)                                           \
    X(les_lignes_python_sont_la)                           \
    X(heure_paris)                                         \
    X(heure_new_york)                                      \
    X(heure_kolkata)                                       \
    X(heure_sydney)                                        \
    X(heure_chatham)                                       \
    X(heure_dublin)                                        \
    X(heure_godthab)                                       \
    X(heure_lord_howe)                                     \
    X(convertir_direct_depuis_la_chaine)                   \
    X(le_cas_de_la_consigne_paris_11_h_40)                 \
    X(paris_autour_du_changement_de_mars_2026)             \
    X(paris_autour_du_changement_d_octobre_2026)           \
    X(hemisphere_sud_sydney)                               \
    X(analyse_des_formes_connues)                          \
    X(chaines_posix_invalides_refusees)                    \
    X(toutes_les_zones_de_la_table_sont_comprises)         \
    X(calendrier_dates_connues)                            \
    X(calendrier_aller_retour)               \
    X(instants_negatifs_et_lointains)                      \
    X(noms_des_jours_et_des_mois)                          \
    X(date_longue_en_toutes_lettres)                       \
    X(date_longue_capacite_exacte)                         \
    X(heure_24_h_avec_h_et_minutes_sur_deux_chiffres)      \
    X(temperature_arrondie_avec_le_signe_degre)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
