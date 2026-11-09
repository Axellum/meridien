// Tests du noyau pur : date longue et jours courts de l'écran principal, dans les sept langues.
#include <string_view>

#include "ecran/visuel_dates.hpp"
#include "mini.hpp"

using ecran::heure::HeureLocale;
using ecran::i18n::Langue;
using namespace ecran::visuel;

namespace {

constexpr bool egal(const char *a, std::string_view b) { return std::string_view(a) == b; }

// Vendredi 6 novembre 2026.
constexpr HeureLocale VENDREDI = {2026, 11, 6, 10, 40, 0, 5, 3600, false};
// Dimanche 1er novembre 2026.
constexpr HeureLocale PREMIER = {2026, 11, 1, 8, 0, 0, 0, 3600, false};
// Samedi 15 août 2026 : un mois dont l'accent est dans le mot (« août »), son initiale reste une lettre ASCII.
constexpr HeureLocale AOUT = {2026, 8, 15, 10, 0, 0, 6, 7200, true};
// Mercredi 30 septembre 2026 : la date la plus longue en espagnol.
constexpr HeureLocale LONGUE = {2026, 9, 30, 20, 8, 0, 3, 7200, true};

constexpr int la_date_suit_la_tournure_de_chaque_langue() {
    char t[64] = {};
    VERIFIER(formater_date(Langue::Fr, VENDREDI, t, sizeof t) > 0 && egal(t, "vendredi 6 novembre 2026"));
    VERIFIER(formater_date(Langue::En, VENDREDI, t, sizeof t) > 0 && egal(t, "Friday, 6 November 2026"));
    VERIFIER(formater_date(Langue::De, VENDREDI, t, sizeof t) > 0 && egal(t, "Freitag, 6. November 2026"));
    VERIFIER(formater_date(Langue::Nl, VENDREDI, t, sizeof t) > 0 && egal(t, "vrijdag 6 november 2026"));
    VERIFIER(formater_date(Langue::Es, VENDREDI, t, sizeof t) > 0 && egal(t, "viernes, 6 de noviembre de 2026"));
    VERIFIER(formater_date(Langue::It, VENDREDI, t, sizeof t) > 0 && egal(t, "venerdì 6 novembre 2026"));
    VERIFIER(formater_date(Langue::Tr, VENDREDI, t, sizeof t) > 0 && egal(t, "6 Kasım 2026 Cuma"));
    return 0;
}

constexpr int sans_annee_la_date_est_plus_courte() {
    char t[64] = {};
    VERIFIER(formater_date(Langue::Fr, VENDREDI, t, sizeof t, false) > 0 && egal(t, "vendredi 6 novembre"));
    VERIFIER(formater_date(Langue::Es, VENDREDI, t, sizeof t, false) > 0 && egal(t, "viernes, 6 de noviembre"));
    VERIFIER(formater_date(Langue::Tr, VENDREDI, t, sizeof t, false) > 0 && egal(t, "6 Kasım Cuma"));
    VERIFIER(formater_date(Langue::De, VENDREDI, t, sizeof t, false) > 0 && egal(t, "Freitag, 6. November"));
    return 0;
}

constexpr int avec_les_initiales_le_jour_et_le_mois_commencent_par_une_majuscule() {
    char t[64] = {};
    // La date de l'accueil : sans l'année, « Vendredi 6 Novembre ».
    VERIFIER(formater_date(Langue::Fr, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "Vendredi 6 Novembre"));
    VERIFIER(formater_date(Langue::Fr, VENDREDI, t, sizeof t, true, true) > 0 && egal(t, "Vendredi 6 Novembre 2026"));
    VERIFIER(formater_date(Langue::Fr, PREMIER, t, sizeof t, false, true) > 0 && egal(t, "Dimanche 1er Novembre"));
    VERIFIER(formater_date(Langue::Fr, AOUT, t, sizeof t, false, true) > 0 && egal(t, "Samedi 15 Août"));
    // Les langues dont les noms commencent déjà par une majuscule ne changent pas ; le « de » espagnol reste en minuscule.
    VERIFIER(formater_date(Langue::En, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "Friday, 6 November"));
    VERIFIER(formater_date(Langue::De, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "Freitag, 6. November"));
    VERIFIER(formater_date(Langue::Tr, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "6 Kasım Cuma"));
    VERIFIER(formater_date(Langue::Nl, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "Vrijdag 6 November"));
    VERIFIER(formater_date(Langue::Es, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "Viernes, 6 de Noviembre"));
    VERIFIER(formater_date(Langue::It, VENDREDI, t, sizeof t, false, true) > 0 && egal(t, "Venerdì 6 Novembre"));
    // Sans l'option (le défaut), rien ne change : la page des alertes écrit « samedi 8 novembre » en minuscules.
    VERIFIER(formater_date(Langue::Fr, VENDREDI, t, sizeof t, false) > 0 && egal(t, "vendredi 6 novembre"));
    return 0;
}

constexpr int le_premier_du_mois_est_ecrit_1er_en_francais_seulement() {
    char t[64] = {};
    VERIFIER(formater_date(Langue::Fr, PREMIER, t, sizeof t) > 0 && egal(t, "dimanche 1er novembre 2026"));
    VERIFIER(formater_date(Langue::En, PREMIER, t, sizeof t) > 0 && egal(t, "Sunday, 1 November 2026"));
    VERIFIER(formater_date(Langue::Tr, PREMIER, t, sizeof t) > 0 && egal(t, "1 Kasım 2026 Pazar"));
    return 0;
}

constexpr int une_date_invalide_ou_un_tampon_trop_petit_donne_un_texte_vide() {
    char t[64] = {'x'};
    HeureLocale h = VENDREDI;
    h.mois = 13;
    VERIFIER(formater_date(Langue::Fr, h, t, sizeof t) == 0 && t[0] == '\0');
    h = VENDREDI;
    h.jour_semaine = 7;
    t[0] = 'x';
    VERIFIER(formater_date(Langue::Fr, h, t, sizeof t) == 0 && t[0] == '\0');
    h = VENDREDI;
    h.jour = 0;
    t[0] = 'x';
    VERIFIER(formater_date(Langue::Fr, h, t, sizeof t) == 0 && t[0] == '\0');
    char petit[8] = {'x'};
    VERIFIER(formater_date(Langue::Fr, VENDREDI, petit, sizeof petit) == 0 && petit[0] == '\0');
    VERIFIER(formater_date(Langue::Fr, VENDREDI, nullptr, 64) == 0);
    VERIFIER(formater_date(Langue::Fr, VENDREDI, t, 0) == 0);
    return 0;
}

constexpr int la_date_la_plus_longue_tient_dans_quarante_octets() {
    // L'écran réserve 48 octets (TexteHorloge::date) ; on exige 40 pour garder de la marge dans la langue la plus bavarde.
    char t[64] = {};
    for (std::size_t l = 0; l < ecran::i18n::NOMBRE_LANGUES; ++l) {
        for (int mois = 1; mois <= 12; ++mois) {
            for (int jour_sem = 0; jour_sem <= 6; ++jour_sem) {
                HeureLocale h = LONGUE;
                h.mois = mois;
                h.jour_semaine = jour_sem;
                const std::size_t n = formater_date(static_cast<Langue>(l), h, t, sizeof t);
                VERIFIER(n > 0 && n <= 40);
            }
        }
    }
    return 0;
}

constexpr int le_jour_court_est_celui_que_la_langue_ecrit() {
    char t[16] = {};
    VERIFIER(jour_court(Langue::Fr, 5, t, sizeof t) == 3 && egal(t, "ven"));
    VERIFIER(jour_court(Langue::En, 5, t, sizeof t) == 3 && egal(t, "Fri"));
    VERIFIER(jour_court(Langue::De, 5, t, sizeof t) == 2 && egal(t, "Fr")); // pas « Fre » : personne ne l'écrit
    VERIFIER(jour_court(Langue::De, 3, t, sizeof t) == 2 && egal(t, "Mi"));
    VERIFIER(jour_court(Langue::Tr, 5, t, sizeof t) == 3 && egal(t, "Cum"));
    VERIFIER(jour_court(Langue::Tr, 1, t, sizeof t) == 3 && egal(t, "Pzt"));
    VERIFIER(jour_court(Langue::Tr, 3, t, sizeof t) > 0 && egal(t, "\xC3\x87"
                                                                   "ar")); // « Çar »
    VERIFIER(jour_court(Langue::Es, 6, t, sizeof t) > 0 && egal(t, "s\xC3\xA1"
                                                                   "b")); // « sáb »
    VERIFIER(jour_court(Langue::It, 5, t, sizeof t) == 3 && egal(t, "ven"));
    VERIFIER(jour_court(Langue::Fr, 7, t, sizeof t) == 0 && t[0] == '\0');
    VERIFIER(jour_court(Langue::Fr, -1, t, sizeof t) == 0);
    char petit[3] = {'x'};
    VERIFIER(jour_court(Langue::Fr, 5, petit, sizeof petit) == 0 && petit[0] == '\0');
    return 0;
}

constexpr int les_sept_jours_courts_de_chaque_langue_sont_distincts_et_courts() {
    char a[16] = {}, b[16] = {};
    for (std::size_t l = 0; l < ecran::i18n::NOMBRE_LANGUES; ++l) {
        for (int j = 0; j <= 6; ++j) {
            const std::size_t n = jour_court(static_cast<Langue>(l), j, a, sizeof a);
            VERIFIER(n >= 2 && n <= 4); // deux à trois lettres, un accent en prend deux octets
            for (int k = j + 1; k <= 6; ++k) {
                jour_court(static_cast<Langue>(l), k, b, sizeof b);
                VERIFIER(!egal(a, b)); // « Paz » pour dimanche et lundi était le défaut de la troncature
            }
        }
    }
    return 0;
}

constexpr int chaque_jour_et_chaque_mois_est_traduit_dans_les_sept_langues() {
    for (std::size_t l = 1; l < ecran::i18n::NOMBRE_LANGUES; ++l) {
        for (int j = 0; j <= 6; ++j) {
            VERIFIER(!ecran::i18n::propre(ecran::heure::nom_jour(j), static_cast<Langue>(l)).empty());
        }
        for (int m = 1; m <= 12; ++m) {
            VERIFIER(!ecran::i18n::propre(ecran::heure::nom_mois(m), static_cast<Langue>(l)).empty());
        }
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(la_date_suit_la_tournure_de_chaque_langue)                                                                       \
    X(sans_annee_la_date_est_plus_courte)                                                                              \
    X(avec_les_initiales_le_jour_et_le_mois_commencent_par_une_majuscule)                                              \
    X(le_premier_du_mois_est_ecrit_1er_en_francais_seulement)                                                          \
    X(une_date_invalide_ou_un_tampon_trop_petit_donne_un_texte_vide)                                                   \
    X(la_date_la_plus_longue_tient_dans_quarante_octets)                                                               \
    X(le_jour_court_est_celui_que_la_langue_ecrit)                                                                     \
    X(les_sept_jours_courts_de_chaque_langue_sont_distincts_et_courts)                                                 \
    X(chaque_jour_et_chaque_mois_est_traduit_dans_les_sept_langues)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
