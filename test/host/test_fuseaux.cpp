// Tests du noyau pur : table des fuseaux IANA -> chaîne POSIX TZ, et recherche.
#include <initializer_list>

#include "ecran/fuseaux.hpp"
#include "mini.hpp"

namespace fx = ecran::fuseaux;

namespace {

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

constexpr int la_table_est_complete() {
    // 461 zones de posix_tz_db + 7 compléments tirés de la base tz (generer_fuseaux.py).
    VERIFIER(fx::NOMBRE == 468);
    return 0;
}

constexpr int la_table_est_strictement_triee_donc_sans_doublon() {
    for (std::size_t i = 1; i < fx::NOMBRE; ++i) {
        VERIFIER(fx::TABLE[i - 1].iana.compare(fx::TABLE[i].iana) < 0);
    }
    return 0;
}

constexpr int chaque_entree_se_retrouve_par_la_dichotomie() {
    for (std::size_t i = 0; i < fx::NOMBRE; ++i) {
        const fx::Entree *e = fx::trouver(fx::TABLE[i].iana);
        VERIFIER(e == &fx::TABLE[i]);
    }
    return 0;
}

constexpr int chaque_chaine_posix_est_plausible() {
    for (std::size_t i = 0; i < fx::NOMBRE; ++i) {
        const std::string_view p = fx::TABLE[i].posix;
        VERIFIER(!p.empty());
        // Un nom de fuseau commence par une lettre ou « < » (forme <-03>3) ; jamais d'espace.
        VERIFIER((p.front() >= 'A' && p.front() <= 'Z') || p.front() == '<');
        for (char c : p)
            VERIFIER(c != ' ');
        // Le nom est suivi d'un décalage : au moins un chiffre dans la chaîne.
        bool chiffre = false;
        for (char c : p)
            chiffre = chiffre || (c >= '0' && c <= '9');
        VERIFIER(chiffre);
        // « < » et « > » vont par paires.
        int ouverts = 0;
        for (char c : p) {
            if (c == '<')
                ++ouverts;
            if (c == '>')
                --ouverts;
            VERIFIER(ouverts == 0 || ouverts == 1);
        }
        VERIFIER(ouverts == 0);
    }
    return 0;
}

constexpr int valeurs_connues_lues_dans_posix_tz_db() {
    struct Attendu {
        std::string_view iana;
        std::string_view posix;
    };
    constexpr Attendu ATTENDUS[] = {
        {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
        {"Asia/Tokyo", "JST-9"},
        {"Asia/Kolkata", "IST-5:30"},
        {"America/Sao_Paulo", "<-03>3"},
        {"Etc/UTC", "UTC0"},
        {"Australia/Lord_Howe", "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0"},
    };
    for (const Attendu &a : ATTENDUS)
        VERIFIER(egal(fx::posix_pour(a.iana), a.posix));
    return 0;
}

constexpr int complements_de_la_base_tz() {
    // Noms actuels que le géocodage d'Open-Meteo renvoie (Europe/Kyiv : constaté le 06/11/2026) et que
    // posix_tz_db ne contient pas.
    VERIFIER(egal(fx::posix_pour("Europe/Kyiv"), fx::posix_pour("Europe/Kiev")));
    VERIFIER(egal(fx::posix_pour("Europe/Kyiv"), "EET-2EEST,M3.5.0/3,M10.5.0/4"));
    VERIFIER(egal(fx::posix_pour("Pacific/Kanton"), "<+13>-13"));
    VERIFIER(egal(fx::posix_pour("America/Ciudad_Juarez"), fx::posix_pour("America/Denver")));
    VERIFIER(egal(fx::posix_pour("America/Coyhaique"), "<-03>3"));
    VERIFIER(egal(fx::posix_pour("Asia/Qostanay"), "<+05>-5"));
    VERIFIER(egal(fx::posix_pour("UTC"), "UTC0"));
    VERIFIER(egal(fx::posix_pour("GMT"), "GMT0"));
    return 0;
}

constexpr int noms_inconnus_ou_mal_ecrits_ne_sont_pas_trouves() {
    for (std::string_view nom :
         {std::string_view{""}, std::string_view{"Europe/Paris "}, std::string_view{"europe/paris"},
          std::string_view{"Europe/Pari"}, std::string_view{"Europe/Parisx"}, std::string_view{"Paris"},
          std::string_view{"Mars/Olympus_Mons"}}) {
        VERIFIER(fx::trouver(nom) == nullptr);
        VERIFIER(fx::posix_pour(nom).empty());
    }
    return 0;
}

constexpr int recherche_par_ville_sans_casse() {
    const fx::Entree *r[8] = {};
    std::size_t n = fx::chercher("paris", r, 8);
    VERIFIER(n >= 1);
    VERIFIER(egal(r[0]->iana, "Europe/Paris"));
    n = fx::chercher("PARIS", r, 8);
    VERIFIER(n >= 1 && egal(r[0]->iana, "Europe/Paris"));
    n = fx::chercher("Tok", r, 8);
    VERIFIER(n >= 1 && egal(r[0]->iana, "Asia/Tokyo"));
    return 0;
}

constexpr int recherche_espace_vaut_souligne() {
    const fx::Entree *r[8] = {};
    const std::size_t n = fx::chercher("new york", r, 8);
    VERIFIER(n >= 1);
    VERIFIER(egal(r[0]->iana, "America/New_York"));
    return 0;
}

constexpr int recherche_par_debut_du_nom_complet() {
    const fx::Entree *r[8] = {};
    const std::size_t n = fx::chercher("Europe/Par", r, 8);
    VERIFIER(n == 1);
    VERIFIER(egal(r[0]->iana, "Europe/Paris"));
    return 0;
}

constexpr int recherche_les_debuts_passent_avant_les_sous_chaines() {
    // « ang » : la ville commence par « ang » (America/Anguilla...) avant celles qui le contiennent seulement.
    const fx::Entree *r[64] = {};
    const std::size_t n = fx::chercher("ang", r, 64);
    VERIFIER(n >= 2);
    bool vu_sous_chaine = false;
    for (std::size_t i = 0; i < n; ++i) {
        const bool debut = fx::detail::commence_par(r[i]->iana, "ang");
        if (!debut)
            vu_sous_chaine = true;
        VERIFIER(!(debut && vu_sous_chaine)); // plus aucun « début » après la première sous-chaîne
        VERIFIER(fx::detail::contient(r[i]->iana, "ang"));
    }
    VERIFIER(vu_sous_chaine); // la liste contient aussi des noms qui ne la contiennent qu'au milieu (Asia/Bangkok)
    return 0;
}

constexpr int recherche_vide_ou_sans_resultat_ou_sans_place() {
    const fx::Entree *r[4] = {};
    VERIFIER(fx::chercher("", r, 4) == 0);
    VERIFIER(fx::chercher("zzzzzzzz", r, 4) == 0);
    VERIFIER(fx::chercher("paris", r, 0) == 0);
    VERIFIER(fx::chercher("paris", nullptr, 4) == 0);
    return 0;
}

constexpr int recherche_respecte_le_maximum_sans_doublon() {
    const fx::Entree *r[5] = {};
    const std::size_t n = fx::chercher("a", r, 5);
    VERIFIER(n == 5);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j)
            VERIFIER(r[i] != r[j]);
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(la_table_est_complete)                                                                                           \
    X(la_table_est_strictement_triee_donc_sans_doublon)                                                                \
    X(chaque_entree_se_retrouve_par_la_dichotomie)                                                                     \
    X(chaque_chaine_posix_est_plausible)                                                                               \
    X(valeurs_connues_lues_dans_posix_tz_db)                                                                           \
    X(complements_de_la_base_tz)                                                                                       \
    X(noms_inconnus_ou_mal_ecrits_ne_sont_pas_trouves)                                                                 \
    X(recherche_par_ville_sans_casse)                                                                                  \
    X(recherche_espace_vaut_souligne)                                                                                  \
    X(recherche_par_debut_du_nom_complet)                                                                              \
    X(recherche_les_debuts_passent_avant_les_sous_chaines)                                                             \
    X(recherche_vide_ou_sans_resultat_ou_sans_place)                                                                   \
    X(recherche_respecte_le_maximum_sans_doublon)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
