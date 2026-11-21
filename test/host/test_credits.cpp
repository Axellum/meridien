// Tests du noyau pur de la page « Sources » (credits.hpp, ADR-0029) : quelles mentions de source l'écran montre, dans quel ordre, avec quelle
// phrase. Ce que ces tests prouvent : chaque phrase est une clé de la table de traduction (sinon l'écran afficherait du français dans les
// six autres langues sans rien dire), chaque traduction garde les marqueurs `{lien}` / `{date}`, la météo est créditée au service qui l'a
// RÉELLEMENT donnée, et rien n'est cité sans être montré.
#include <cstddef>
#include <string_view>

#include "ecran/credits.hpp"
#include "ecran/i18n.hpp"
#include "mini.hpp"

using namespace ecran::credits;
using ecran::i18n::Langue;
using ecran::meteo::fournisseurs::Famille;
using sv = std::string_view;

namespace {

constexpr bool contient(sv texte, sv aiguille) { return texte.find(aiguille) != sv::npos; }

constexpr int rang_de(const Liste &l, Rubrique r) {
    for (std::size_t i = 0; i < l.n; ++i)
        if (l.ligne[i].rubrique == r)
            return static_cast<int>(i);
    return -1;
}

constexpr bool phrase_de(const Liste &l, Rubrique r, const char *cle) {
    const int i = rang_de(l, r);
    return i >= 0 && l.ligne[i].cle != nullptr && sv(l.ligne[i].cle) == sv(cle);
}

// ---- la table de traduction ----------------------------------------------------------------------------------------------

constexpr int chaque_phrase_est_une_cle_de_la_table() {
    for (const char *cle : CLES)
        VERIFIER(ecran::i18n::est_une_cle(cle));
    return 0;
}

constexpr int chaque_traduction_garde_ses_marqueurs() {
    constexpr Langue langues[] = {Langue::En, Langue::De, Langue::Nl, Langue::Es, Langue::It, Langue::Tr};
    for (const char *cle : CLES) {
        const bool lien = contient(cle, "{lien}");
        const bool date = contient(cle, "{date}");
        for (const Langue langue : langues) {
            const sv t = ecran::i18n::tr(cle, langue);
            VERIFIER(!t.empty());
            VERIFIER(contient(t, "{lien}") == lien);
            VERIFIER(contient(t, "{date}") == date);
        }
    }
    return 0;
}

constexpr int les_six_langues_traduisent_les_nouvelles_phrases() {
    // « Sources » (titre) est volontairement identique en anglais ; les phrases longues, elles, doivent différer du français.
    constexpr const char *longues[] = {ALERTES, NORMALES, NORMALES_SANS_DATE, POLICES};
    constexpr Langue langues[] = {Langue::En, Langue::De, Langue::Nl, Langue::Es, Langue::It, Langue::Tr};
    for (const char *cle : longues)
        for (const Langue langue : langues)
            VERIFIER(ecran::i18n::tr(cle, langue) != sv(cle));
    return 0;
}

// ---- ce qui est montré ---------------------------------------------------------------------------------------------------

constexpr int toujours_les_lieux_et_les_polices() {
    const Liste l = composer({});
    VERIFIER(l.n == 2);
    VERIFIER(phrase_de(l, Rubrique::Lieux, LIEUX) && sv(l.ligne[0].lien) == "GeoNames");
    VERIFIER(phrase_de(l, Rubrique::Polices, POLICES));
    VERIFIER(rang_de(l, Rubrique::Meteo) == -1 && rang_de(l, Rubrique::Alertes) == -1 &&
             rang_de(l, Rubrique::Normales) == -1);
    VERIFIER(rang_de(l, Rubrique::Mer) == -1 && rang_de(l, Rubrique::Spots) == -1);
    return 0;
}

constexpr int la_meteo_est_creditee_au_service_qui_l_a_donnee() {
    Presences p;
    p.meteo = true;
    Liste l = composer(p);
    VERIFIER(phrase_de(l, Rubrique::Meteo, METEO_OPEN_METEO) &&
             sv(l.ligne[rang_de(l, Rubrique::Meteo)].lien) == "Open-Meteo");
    p.origine = Famille::MetNo;
    l = composer(p);
    VERIFIER(phrase_de(l, Rubrique::Meteo, METEO_MET_NO) &&
             sv(l.ligne[rang_de(l, Rubrique::Meteo)].lien) == "MET Norway");
    VERIFIER(contient(METEO_MET_NO, "NLOD 2.0") && contient(METEO_MET_NO, "adaptées"));
    VERIFIER(rang_de(l, Rubrique::MeteoNws) == -1);
    p.origine = Famille::Nws;
    l = composer(p);
    VERIFIER(phrase_de(l, Rubrique::Meteo, METEO_NWS) && sv(l.ligne[rang_de(l, Rubrique::Meteo)].lien) == "NWS");
    VERIFIER(phrase_de(l, Rubrique::MeteoNws,
                       NWS_SANS_APPROBATION)); // le NWS demande de ne pas laisser croire à son approbation
    VERIFIER(rang_de(l, Rubrique::MeteoNws) == rang_de(l, Rubrique::Meteo) + 1);
    return 0;
}

constexpr int pas_de_meteo_pas_de_credit_meteo() {
    Presences p;
    p.meteo = false;
    p.origine = Famille::Nws; // l'origine mémorisée ne suffit pas : sans météo montrée, rien n'est cité
    const Liste l = composer(p);
    VERIFIER(rang_de(l, Rubrique::Meteo) == -1 && rang_de(l, Rubrique::MeteoNws) == -1);
    return 0;
}

constexpr int vigie_n_est_citee_que_si_les_alertes_sont_actives() {
    Presences p;
    VERIFIER(rang_de(composer(p), Rubrique::Alertes) == -1);
    p.alertes = true;
    const Liste l = composer(p);
    VERIFIER(phrase_de(l, Rubrique::Alertes, ALERTES) && sv(l.ligne[rang_de(l, Rubrique::Alertes)].lien) == "Vigie");
    VERIFIER(contient(ALERTES, "licence de son émetteur")); // chaque alerte garde la licence de son émetteur
    return 0;
}

constexpr int les_normales_nomment_meteo_france_la_licence_et_la_date_d_edition() {
    Presences p;
    VERIFIER(rang_de(composer(p), Rubrique::Normales) == -1);
    p.normales = true;
    p.edition = "2026-11-06";
    Liste l = composer(p);
    const int i = rang_de(l, Rubrique::Normales);
    VERIFIER(i >= 0 && phrase_de(l, Rubrique::Normales, NORMALES));
    VERIFIER(sv(l.ligne[i].lien) == "Météo-France" && sv(l.ligne[i].date) == "2026-11-06");
    VERIFIER(contient(NORMALES, "Licence Ouverte 2.0") && contient(NORMALES, "{date}"));
    p.edition = ""; // la fiche n'a pas donné sa date : la phrase n'en invente pas
    l = composer(p);
    VERIFIER(phrase_de(l, Rubrique::Normales, NORMALES_SANS_DATE) &&
             l.ligne[rang_de(l, Rubrique::Normales)].date == nullptr);
    VERIFIER(!contient(NORMALES_SANS_DATE, "{date}") && contient(NORMALES_SANS_DATE, "Licence Ouverte 2.0"));
    p.edition = nullptr;
    VERIFIER(phrase_de(composer(p), Rubrique::Normales, NORMALES_SANS_DATE));
    return 0;
}

constexpr int la_mer_et_les_spots_vont_ensemble() {
    Presences p;
    VERIFIER(rang_de(composer(p), Rubrique::Mer) == -1 && rang_de(composer(p), Rubrique::Spots) == -1);
    p.mer = true;
    const Liste l = composer(p);
    VERIFIER(rang_de(l, Rubrique::Mer) >= 0 &&
             l.ligne[rang_de(l, Rubrique::Mer)].cle == nullptr); // le crédit de mer_textes.hpp
    VERIFIER(phrase_de(l, Rubrique::Spots, SPOTS));
    VERIFIER(rang_de(l, Rubrique::Spots) == rang_de(l, Rubrique::Mer) + 1);
    return 0;
}

constexpr int l_ordre_est_celui_de_l_ecran() {
    Presences p;
    p.meteo = true;
    p.origine = Famille::Nws;
    p.alertes = true;
    p.normales = true;
    p.edition = "2026-11-06";
    p.mer = true;
    const Liste l = composer(p);
    VERIFIER(l.n == 8 && LIGNES_MAX == 8); // une ligne par rubrique
    constexpr Rubrique attendu[] = {Rubrique::Meteo,    Rubrique::MeteoNws, Rubrique::Lieux, Rubrique::Alertes,
                                    Rubrique::Normales, Rubrique::Mer,      Rubrique::Spots, Rubrique::Polices};
    for (std::size_t i = 0; i < 8; ++i)
        VERIFIER(l.ligne[i].rubrique == attendu[i]);
    return 0;
}

constexpr int la_liste_ne_depasse_jamais_sa_capacite() {
    for (int m = 0; m < 2; ++m)
        for (int o = 0; o < 3; ++o)
            for (int a = 0; a < 2; ++a)
                for (int n = 0; n < 2; ++n)
                    for (int s = 0; s < 2; ++s) {
                        Presences p;
                        p.meteo = m != 0;
                        p.origine = static_cast<Famille>(o);
                        p.alertes = a != 0;
                        p.normales = n != 0;
                        p.mer = s != 0;
                        const Liste l = composer(p);
                        VERIFIER(l.n >= 2 && l.n <= LIGNES_MAX);
                        for (std::size_t i = 0; i < l.n; ++i)
                            for (std::size_t j = i + 1; j < l.n; ++j)
                                VERIFIER(l.ligne[i].rubrique !=
                                         l.ligne[j].rubrique); // jamais deux fois la même mention
                    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_phrase_est_une_cle_de_la_table)                                                                           \
    X(chaque_traduction_garde_ses_marqueurs)                                                                           \
    X(les_six_langues_traduisent_les_nouvelles_phrases)                                                                \
    X(toujours_les_lieux_et_les_polices)                                                                               \
    X(la_meteo_est_creditee_au_service_qui_l_a_donnee)                                                                 \
    X(pas_de_meteo_pas_de_credit_meteo)                                                                                \
    X(vigie_n_est_citee_que_si_les_alertes_sont_actives)                                                               \
    X(les_normales_nomment_meteo_france_la_licence_et_la_date_d_edition)                                               \
    X(la_mer_et_les_spots_vont_ensemble)                                                                               \
    X(l_ordre_est_celui_de_l_ecran)                                                                                    \
    X(la_liste_ne_depasse_jamais_sa_capacite)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
