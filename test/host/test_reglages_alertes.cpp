// Tests du noyau pur : les quatre réglages des alertes officielles (reglages_alertes.hpp) : ce que l'API accepte, refuse, et ce que
// la NVS relue devient quand elle est abîmée.
#include <cstddef>
#include <initializer_list>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/reglages_alertes.hpp"
#include "mini.hpp"

using namespace ecran::reglages;
namespace json = ecran::json;
namespace vigie = ecran::vigie;

namespace {

constexpr bool egal(const char *a, std::string_view b) { return std::string_view(a) == b; }

// Un corps JSON validé contre `actuel`. Le texte doit rester vivant tant que le résultat sert.
constexpr ResultatAlertes valider(std::string_view corps, const Alertes &actuel = Alertes{}) {
    const json::Resultat lu = json::lire(corps);
    if (!lu.ok()) {
        ResultatAlertes r;
        r.erreur = ErreurAlertes::Actives; // le corps n'est pas du JSON : les tests n'en envoient pas
        return r;
    }
    return valider_alertes(lu.valeur, actuel);
}

constexpr Alertes autre_etat() {
    Alertes a;
    a.actives = false;
    a.periode_min = 60;
    a.niveau_min = vigie::NiveauMin::Rouge;
    const std::string_view adr = "https://vigie.exemple.fr";
    for (std::size_t i = 0; i < adr.size(); ++i)
        a.adresse[i] = adr[i];
    a.adresse[adr.size()] = '\0';
    return a;
}

constexpr int les_valeurs_par_defaut() {
    const Alertes a;
    VERIFIER(a.actives);
    VERIFIER(a.adresse[0] == '\0');
    VERIFIER(a.periode_min == 15);
    VERIFIER(a.niveau_min == vigie::NiveauMin::Jaune);
    return 0;
}

constexpr int un_corps_vide_ne_change_rien() {
    const Alertes avant = autre_etat();
    const ResultatAlertes r = valider("{}", avant);
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(r.valeur == avant);
    return 0;
}

constexpr int chaque_champ_valide_change_seulement_son_champ() {
    const Alertes base;
    ResultatAlertes r = valider(R"({"actives":false})", base);
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(!r.valeur.actives && r.valeur.periode_min == 15 && r.valeur.niveau_min == vigie::NiveauMin::Jaune &&
             r.valeur.adresse[0] == '\0');

    r = valider(R"({"periode_min":45})", base);
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(r.valeur.actives && r.valeur.periode_min == 45 && r.valeur.niveau_min == vigie::NiveauMin::Jaune);

    r = valider(R"({"niveau_min":"orange"})", base);
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(r.valeur.niveau_min == vigie::NiveauMin::Orange && r.valeur.periode_min == 15 && r.valeur.actives);

    r = valider(R"({"adresse":"https://vigie.exemple.fr"})", base);
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(egal(r.valeur.adresse, "https://vigie.exemple.fr") && r.valeur.actives && r.valeur.periode_min == 15);
    return 0;
}

constexpr int les_quatre_champs_ensemble() {
    const ResultatAlertes r =
        valider(R"({"actives":false,"adresse":"https://a.b:8443/vigie","periode_min":180,"niveau_min":"red"})");
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(!r.valeur.actives);
    VERIFIER(egal(r.valeur.adresse, "https://a.b:8443/vigie"));
    VERIFIER(r.valeur.periode_min == 180);
    VERIFIER(r.valeur.niveau_min == vigie::NiveauMin::Rouge);
    return 0;
}

constexpr int actives_doit_etre_un_booleen() {
    for (std::string_view corps :
         {R"({"actives":"true"})", R"({"actives":1})", R"({"actives":null})", R"({"actives":"oui"})"}) {
        const ResultatAlertes r = valider(corps, autre_etat());
        VERIFIER(r.erreur == ErreurAlertes::Actives);
        VERIFIER(r.valeur == autre_etat());
    }
    return 0;
}

constexpr int la_periode_est_un_entier_de_10_a_180() {
    for (std::string_view bon : {R"({"periode_min":10})", R"({"periode_min":15})", R"({"periode_min":180})"}) {
        VERIFIER(valider(bon).erreur == ErreurAlertes::Aucune);
    }
    for (std::string_view mauvais :
         {R"({"periode_min":9})", R"({"periode_min":181})", R"({"periode_min":0})", R"({"periode_min":-15})",
          R"({"periode_min":15.5})", R"({"periode_min":1e1})", R"({"periode_min":"15"})", R"({"periode_min":null})",
          R"({"periode_min":true})", R"({"periode_min":99999999999999999999})"}) {
        const ResultatAlertes r = valider(mauvais, autre_etat());
        VERIFIER(r.erreur == ErreurAlertes::Periode);
        VERIFIER(r.valeur == autre_etat());
    }
    return 0;
}

constexpr int le_niveau_est_un_des_trois_codes_exacts() {
    VERIFIER(valider(R"({"niveau_min":"yellow"})").valeur.niveau_min == vigie::NiveauMin::Jaune);
    VERIFIER(valider(R"({"niveau_min":"orange"})").valeur.niveau_min == vigie::NiveauMin::Orange);
    VERIFIER(valider(R"({"niveau_min":"red"})").valeur.niveau_min == vigie::NiveauMin::Rouge);
    for (std::string_view mauvais : {R"({"niveau_min":"Orange"})", R"({"niveau_min":"rouge"})",
                                     R"({"niveau_min":"green"})", R"({"niveau_min":""})", R"({"niveau_min":2})",
                                     R"({"niveau_min":null})", R"({"niveau_min":"yellowyellowyellowyellow"})"}) {
        const ResultatAlertes r = valider(mauvais, autre_etat());
        VERIFIER(r.erreur == ErreurAlertes::NiveauMin);
        VERIFIER(r.valeur == autre_etat());
    }
    return 0;
}

constexpr int l_adresse_vide_remet_l_instance_publique() {
    for (std::string_view corps : {R"({"adresse":""})", R"({"adresse":"   "})", R"({"adresse":"\t "})"}) {
        const ResultatAlertes r = valider(corps, autre_etat());
        VERIFIER(r.erreur == ErreurAlertes::Aucune);
        VERIFIER(r.valeur.adresse[0] == '\0');
        VERIFIER(r.valeur.periode_min == 60); // le reste est resté
    }
    return 0;
}

constexpr int l_adresse_est_normalisee_avant_d_etre_gardee() {
    ResultatAlertes r = valider(R"({"adresse":"  HTTPS://Vigie.Exemple.fr/  "})");
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(
        egal(r.valeur.adresse, "https://Vigie.Exemple.fr")); // schéma en minuscules, « / » final et espaces retirés
    r = valider(R"({"adresse":"https://h.fr:8443/vigie/"})");
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(egal(r.valeur.adresse, "https://h.fr:8443/vigie"));
    // Une adresse plus courte remplace entièrement une plus longue : rien de l'ancienne ne reste après le zéro.
    r = valider(R"({"adresse":"https://a.fr"})", autre_etat()); // l'ancienne : « https://vigie.exemple.fr »
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(egal(r.valeur.adresse, "https://a.fr"));
    VERIFIER(r.valeur.adresse[12] == '\0');
    return 0;
}

constexpr int une_adresse_que_vigie_refuse_est_refusee_par_l_api() {
    for (std::string_view corps :
         {R"({"adresse":"http://vigie.exemple.fr"})", R"({"adresse":"vigie.exemple.fr"})", R"({"adresse":"https://"})",
          R"({"adresse":"https://u:p@vigie.exemple.fr"})", R"({"adresse":"https://vigie.exemple.fr?x=1"})",
          R"({"adresse":"https://vigie.exemple.fr#a"})", R"({"adresse":"https://vigie exemple.fr"})",
          R"({"adresse":"https://vigie.exemple.fr\\a"})", R"({"adresse":"ftp://vigie.exemple.fr"})",
          R"({"adresse":"https://-vigie.exemple.fr"})", R"({"adresse":"https://vigié.fr"})", R"({"adresse":42})",
          R"({"adresse":null})", R"({"adresse":true})"}) {
        const ResultatAlertes r = valider(corps, autre_etat());
        VERIFIER(r.erreur == ErreurAlertes::Adresse);
        VERIFIER(r.valeur == autre_etat());
    }
    return 0;
}

constexpr int une_adresse_trop_longue_est_refusee_et_non_coupee() {
    // 80 octets : accepté ; 81 : refusé ; très longue : refusée aussi (la copie en tampon ne la coupe pas en silence).
    const std::string_view debut = "https://";
    char corps[160] = {};
    const auto fabriquer = [&](std::size_t n) {
        std::size_t p = 0;
        for (char c : std::string_view(R"({"adresse":")"))
            corps[p++] = c;
        for (std::size_t i = 0; i < n; ++i)
            corps[p++] = i < debut.size() ? debut[i] : 'a';
        corps[p++] = '"';
        corps[p++] = '}';
        corps[p] = '\0';
    };
    fabriquer(80);
    VERIFIER(valider(corps).erreur == ErreurAlertes::Aucune);
    fabriquer(81);
    VERIFIER(valider(corps).erreur == ErreurAlertes::Adresse);
    fabriquer(120);
    VERIFIER(valider(corps).erreur == ErreurAlertes::Adresse);
    // Une adresse courte suivie de beaucoup d'espaces dépasse le tampon de lecture : refusée, et non coupée juste après l'adresse (sinon
    // la saisie « https://a.fr » + 100 espaces serait acceptée alors que la même saisie de 81 octets utiles est refusée).
    std::size_t p = 0;
    for (char c : std::string_view(R"({"adresse":"https://a.fr)"))
        corps[p++] = c;
    for (int i = 0; i < 100; ++i)
        corps[p++] = ' ';
    corps[p++] = '"';
    corps[p++] = '}';
    corps[p] = '\0';
    VERIFIER(valider(corps).erreur == ErreurAlertes::Adresse);
    return 0;
}

constexpr int tout_ou_rien() {
    // La première valeur refusée annule tout, y compris les champs valides placés avant ou après.
    const Alertes avant = autre_etat();
    ResultatAlertes r = valider(R"({"actives":true,"periode_min":5})", avant);
    VERIFIER(r.erreur == ErreurAlertes::Periode);
    VERIFIER(r.valeur == avant && !r.valeur.actives);
    r = valider(R"({"niveau_min":"orange","adresse":"http://x.fr"})", avant);
    VERIFIER(r.erreur == ErreurAlertes::Adresse);
    VERIFIER(r.valeur == avant && r.valeur.niveau_min == vigie::NiveauMin::Rouge);
    return 0;
}

constexpr int les_champs_inconnus_sont_ignores() {
    const ResultatAlertes r = valider(R"({"couleur":"rouge","periode_min":30,"x":{"y":1}})");
    VERIFIER(r.erreur == ErreurAlertes::Aucune);
    VERIFIER(r.valeur.periode_min == 30);
    return 0;
}

constexpr int la_nvs_abimee_retombe_sur_les_valeurs_par_defaut() {
    Alertes a = alertes_depuis_nvs(1, 45, 2, "https://vigie.exemple.fr");
    VERIFIER(a.actives && a.periode_min == 45 && a.niveau_min == vigie::NiveauMin::Rouge);
    VERIFIER(egal(a.adresse, "https://vigie.exemple.fr"));

    a = alertes_depuis_nvs(0, 15, 0, "");
    VERIFIER(!a.actives);
    a = alertes_depuis_nvs(2, 15, 0, ""); // tout ce qui n'est pas 0 est « oui »
    VERIFIER(a.actives);

    for (long long mauvaise : {0LL, 9LL, 181LL, -15LL, 4000000000LL}) {
        VERIFIER(alertes_depuis_nvs(1, mauvaise, 0, "").periode_min == 15);
    }
    VERIFIER(alertes_depuis_nvs(1, 10, 0, "").periode_min == 10);
    VERIFIER(alertes_depuis_nvs(1, 180, 0, "").periode_min == 180);

    VERIFIER(alertes_depuis_nvs(1, 15, 3, "").niveau_min == vigie::NiveauMin::Jaune);
    VERIFIER(alertes_depuis_nvs(1, 15, -1, "").niveau_min == vigie::NiveauMin::Jaune);
    VERIFIER(alertes_depuis_nvs(1, 15, 255, "").niveau_min == vigie::NiveauMin::Jaune);

    // Une adresse que le noyau refuse n'est pas gardée : on retombe sur l'instance publique (adresse vide), jamais sur « http:// ».
    VERIFIER(alertes_depuis_nvs(1, 15, 0, "http://vigie.exemple.fr").adresse[0] == '\0');
    VERIFIER(alertes_depuis_nvs(1, 15, 0, "n'importe quoi").adresse[0] == '\0');
    VERIFIER(egal(alertes_depuis_nvs(1, 15, 0, "https://a.fr/").adresse, "https://a.fr"));
    return 0;
}

constexpr int l_adresse_utilisee_est_celle_des_reglages_ou_l_instance_publique() {
    char t[vigie::ADRESSE_MAX + 1] = {};
    Alertes a;
    std::size_t n = adresse_utilisee(a, t, sizeof t);
    VERIFIER(n == vigie::ADRESSE_PUBLIQUE.size());
    VERIFIER(egal(t, vigie::ADRESSE_PUBLIQUE));

    a = autre_etat();
    n = adresse_utilisee(a, t, sizeof t);
    VERIFIER(n == 24 && egal(t, "https://vigie.exemple.fr"));

    // Une adresse abîmée en mémoire ne part jamais telle quelle : l'instance publique la remplace.
    a = Alertes{};
    const std::string_view mauvaise = "http://non.fr";
    for (std::size_t i = 0; i < mauvaise.size(); ++i)
        a.adresse[i] = mauvaise[i];
    n = adresse_utilisee(a, t, sizeof t);
    VERIFIER(egal(t, vigie::ADRESSE_PUBLIQUE));

    // Tampon trop petit : rien d'écrit en trop, 0 rendu.
    char petit[8] = {};
    VERIFIER(adresse_utilisee(Alertes{}, petit, sizeof petit) == 0);
    VERIFIER(adresse_utilisee(Alertes{}, nullptr, 10) == 0);
    VERIFIER(adresse_utilisee(Alertes{}, t, 0) == 0);
    return 0;
}

constexpr int les_codes_de_niveau_vont_et_viennent() {
    for (std::size_t i = 0; i < vigie::NOMBRE_NIVEAUX_MIN; ++i) {
        const auto n = static_cast<vigie::NiveauMin>(i);
        const auto retour = niveau_min_depuis_code(code(n));
        VERIFIER(retour.has_value() && *retour == n);
    }
    VERIFIER(code(vigie::NiveauMin::Jaune) == "yellow");
    VERIFIER(code(vigie::NiveauMin::Orange) == "orange");
    VERIFIER(code(vigie::NiveauMin::Rouge) == "red");
    VERIFIER(!niveau_min_depuis_code("").has_value());
    VERIFIER(!niveau_min_depuis_code("jaune").has_value());
    // Un niveau hors de la table (valeur abîmée en mémoire) ne lit pas hors du tableau : il se dit comme le niveau par défaut.
    VERIFIER(code(static_cast<vigie::NiveauMin>(vigie::NOMBRE_NIVEAUX_MIN)) == "yellow");
    VERIFIER(code(static_cast<vigie::NiveauMin>(200)) == "yellow");
    return 0;
}

constexpr int l_egalite_regarde_chaque_champ_et_ignore_ce_qui_suit_le_zero() {
    Alertes a, b;
    VERIFIER(a == b);
    b.actives = false;
    VERIFIER(!(a == b));
    b = a;
    b.periode_min = 16;
    VERIFIER(!(a == b));
    b = a;
    b.niveau_min = vigie::NiveauMin::Orange;
    VERIFIER(!(a == b));
    b = a;
    b.adresse[0] = 'h';
    VERIFIER(!(a == b));
    // Deux adresses vides égales même si l'une garde des octets après le zéro.
    b = a;
    b.adresse[5] = 'x';
    VERIFIER(a == b);
    return 0;
}

// L'écran de l'appareil et l'API rendent le même verdict pour une même adresse (même état de départ, même résultat).
constexpr bool meme_verdict_que_l_api(std::string_view saisie) {
    char corps[200] = {};
    std::size_t n = 0;
    for (const char c : std::string_view(R"({"adresse":")"))
        corps[n++] = c;
    for (const char c : saisie)
        corps[n++] = c;
    for (const char c : std::string_view(R"("})"))
        corps[n++] = c;
    const ResultatAlertes api = valider(std::string_view(corps, n), autre_etat());
    const ResultatAlertes ecran = appliquer_adresse(autre_etat(), saisie);
    return api.erreur == ecran.erreur && api.valeur == ecran.valeur;
}

constexpr int l_adresse_saisie_sur_l_ecran_suit_les_regles_de_l_api() {
    // Une saisie blanche remet l'instance publique et ne touche à rien d'autre.
    for (const std::string_view blanche : {"", " ", "   ", "\t", " \t "}) {
        VERIFIER(saisie_blanche(blanche));
        const ResultatAlertes r = appliquer_adresse(autre_etat(), blanche);
        VERIFIER(r.erreur == ErreurAlertes::Aucune && r.valeur.adresse[0] == '\0');
        VERIFIER(!r.valeur.actives && r.valeur.periode_min == 60 && r.valeur.niveau_min == vigie::NiveauMin::Rouge);
    }
    VERIFIER(!saisie_blanche("a") && !saisie_blanche(" a ") && !saisie_blanche("\n"));
    // Une adresse valide est gardée sous sa forme normalisée.
    const ResultatAlertes bonne = appliquer_adresse(Alertes{}, "  HTTPS://vigie.exemple.fr/  ");
    VERIFIER(bonne.erreur == ErreurAlertes::Aucune && egal(bonne.valeur.adresse, "https://vigie.exemple.fr"));
    VERIFIER(bonne.valeur.actives && bonne.valeur.periode_min == 15 &&
             bonne.valeur.niveau_min == vigie::NiveauMin::Jaune);
    // Ce que Vigie refuse est refusé, et l'état reste ce qu'il était (tout ou rien).
    for (const std::string_view refusee :
         {"http://vigie.exemple.fr", "vigie.exemple.fr", "https://", "https://u:p@vigie.exemple.fr",
          "https://vigie.exemple.fr/?a=1", "https://vigie.exemple.fr/#a", "https://vigie exemple.fr"}) {
        const ResultatAlertes r = appliquer_adresse(autre_etat(), refusee);
        VERIFIER(r.erreur == ErreurAlertes::Adresse && r.valeur == autre_etat());
    }
    // Le maximum : 81 octets refusés, jamais coupés ; 80 octets : le verdict de l'API.
    char longue[96] = {};
    const std::string_view schema = "https://";
    for (std::size_t i = 0; i < schema.size(); ++i)
        longue[i] = schema[i];
    for (std::size_t i = schema.size(); i < 81; ++i)
        longue[i] = 'a';
    const ResultatAlertes trop = appliquer_adresse(autre_etat(), std::string_view(longue, 81));
    VERIFIER(trop.erreur == ErreurAlertes::Adresse && trop.valeur == autre_etat());
    VERIFIER(meme_verdict_que_l_api(std::string_view(longue, 80)));
    // Même verdict que l'API sur tout un échantillon, valides ou non.
    for (const std::string_view s :
         {"", "   ", "https://vigie.exemple.fr", "HTTPS://A.fr/", " https://x.fr ", "https://x.fr:8443/vigie",
          "http://x.fr", "vigie.fr", "https://", "https://u:p@x.fr", "https://x.fr/?a", "https://x.fr/#a", "ftp://x.fr",
          "https://x.fr//", "https://x y.fr"})
        VERIFIER(meme_verdict_que_l_api(s));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_valeurs_par_defaut)                                                                                          \
    X(un_corps_vide_ne_change_rien)                                                                                    \
    X(chaque_champ_valide_change_seulement_son_champ)                                                                  \
    X(les_quatre_champs_ensemble)                                                                                      \
    X(actives_doit_etre_un_booleen)                                                                                    \
    X(la_periode_est_un_entier_de_10_a_180)                                                                            \
    X(le_niveau_est_un_des_trois_codes_exacts)                                                                         \
    X(l_adresse_vide_remet_l_instance_publique)                                                                        \
    X(l_adresse_est_normalisee_avant_d_etre_gardee)                                                                    \
    X(une_adresse_que_vigie_refuse_est_refusee_par_l_api)                                                              \
    X(une_adresse_trop_longue_est_refusee_et_non_coupee)                                                               \
    X(tout_ou_rien)                                                                                                    \
    X(les_champs_inconnus_sont_ignores)                                                                                \
    X(la_nvs_abimee_retombe_sur_les_valeurs_par_defaut)                                                                \
    X(l_adresse_utilisee_est_celle_des_reglages_ou_l_instance_publique)                                                \
    X(les_codes_de_niveau_vont_et_viennent)                                                                            \
    X(l_egalite_regarde_chaque_champ_et_ignore_ce_qui_suit_le_zero)                                                    \
    X(l_adresse_saisie_sur_l_ecran_suit_les_regles_de_l_api)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
