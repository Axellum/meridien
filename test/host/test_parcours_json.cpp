// Tests du noyau pur : le parcours d'un tableau JSON en un seul passage (parcours_json.hpp). Il doit rendre exactement les éléments
// que `Valeur::element(i)` (qui repart du début à chaque appel) rend, dans le même ordre, et rien d'autre.
#include <cstddef>
#include <string_view>

#include "aplatir_fournisseurs.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/parcours_json.hpp"
#include "mini.hpp"

using namespace ecran::json;
using fournisseurs_test::Texte;

namespace {

// Rend 0 si le parcours donne les mêmes éléments que element(i) et que taille(), sinon un numéro de cas.
constexpr int meme_suite(std::string_view texte, std::size_t attendu) {
    const Resultat lu = lire(texte);
    if (!lu.ok())
        return 1;
    const Valeur tableau = lu.valeur;
    if (tableau.genre() != Genre::Tableau || tableau.taille() != attendu)
        return 2;
    Parcours p(tableau);
    Valeur v;
    std::size_t i = 0;
    while (p.suivant(v)) {
        if (i >= attendu)
            return 3; // trop d'éléments
        const Valeur ref = tableau.element(i);
        if (v.brut() != ref.brut() || v.genre() != ref.genre())
            return 4;
        ++i;
    }
    if (i != attendu)
        return 5;
    // une fois fini, il reste fini
    if (p.suivant(v) || p.suivant(v))
        return 6;
    return 0;
}

constexpr int des_nombres() {
    VERIFIER(meme_suite("[1,2,3]", 3) == 0);
    VERIFIER(meme_suite("[ 1 , 2 ,  3 ]", 3) == 0);
    VERIFIER(meme_suite("[\n  1,\n  2\n]", 2) == 0);
    VERIFIER(meme_suite("[-1.5e3, 0, 7]", 3) == 0);
    return 0;
}

constexpr int un_tableau_vide_ou_d_un_element() {
    VERIFIER(meme_suite("[]", 0) == 0);
    VERIFIER(meme_suite("[ ]", 0) == 0);
    VERIFIER(meme_suite("[42]", 1) == 0);
    VERIFIER(meme_suite("[ null ]", 1) == 0);
    VERIFIER(meme_suite("[[]]", 1) == 0);
    return 0;
}

constexpr int des_chaines_qui_contiennent_des_signes_du_json() {
    VERIFIER(meme_suite(R"(["a,b","]","[","\"","x\\"])", 5) == 0);
    VERIFIER(meme_suite(R"(["é,à", "日本"])", 2) == 0);
    return 0;
}

constexpr int des_objets_et_des_tableaux_imbriques() {
    VERIFIER(meme_suite(R"([{"a":[1,2,{"b":"]"}],"c":null},{"d":1},[],[[1],[2,3]]])", 4) == 0);
    VERIFIER(
        meme_suite(R"([{"validTime":"2026-11-07T01:00:00+00:00/PT3H","value":13.3},{"validTime":"x","value":null}])",
                   2) == 0);
    return 0;
}

constexpr int le_parcours_rend_les_genres_attendus() {
    const Resultat lu = lire(R"([1,"a",true,null,{},[]])");
    VERIFIER(lu.ok());
    Parcours p(lu.valeur);
    Valeur v;
    constexpr Genre G[] = {Genre::Nombre, Genre::Chaine, Genre::Booleen, Genre::Nul, Genre::Objet, Genre::Tableau};
    for (const Genre g : G) {
        VERIFIER(p.suivant(v));
        VERIFIER(v.genre() == g);
    }
    VERIFIER(!p.suivant(v));
    return 0;
}

constexpr int autre_chose_qu_un_tableau_donne_un_parcours_vide() {
    Valeur v;
    {
        const Resultat lu = lire(R"({"a":[1,2]})");
        VERIFIER(lu.ok());
        Parcours p(lu.valeur);
        VERIFIER(!p.suivant(v));
    }
    {
        const Resultat lu = lire("12");
        VERIFIER(lu.ok());
        Parcours p(lu.valeur);
        VERIFIER(!p.suivant(v));
    }
    {
        const Resultat lu = lire(R"("texte")");
        VERIFIER(lu.ok());
        Parcours p(lu.valeur);
        VERIFIER(!p.suivant(v));
    }
    {
        Parcours p(Valeur{}); // valeur absente (champ qui n'existe pas)
        VERIFIER(!p.suivant(v));
    }
    {
        const Resultat lu = lire(R"({"a":1})");
        VERIFIER(lu.ok());
        Parcours p(lu.valeur.champ("manquant"));
        VERIFIER(!p.suivant(v));
    }
    return 0;
}

constexpr int un_long_tableau_est_lu_en_entier() {
    // 300 éléments : bien plus que les 128 ou 192 d'une réponse réelle
    Texte<4096> t;
    t += "[";
    for (int i = 0; i < 300; ++i) {
        if (i > 0)
            t += ",";
        t += i % 2 == 0 ? R"({"a":1})" : "7";
    }
    t += "]";
    VERIFIER(!t.trop);
    VERIFIER(meme_suite(t.vue(), 300) == 0);
    return 0;
}

constexpr int un_parcours_s_arrete_au_bout_du_tableau_lu_dans_un_objet() {
    // le tableau est un champ d'un objet plus grand : le parcours ne lit pas ce qui suit le crochet fermant
    const Resultat lu = lire(R"({"a":[1,2],"b":[3,4,5]})");
    VERIFIER(lu.ok());
    Parcours p(lu.valeur.champ("a"));
    Valeur v;
    std::size_t n = 0;
    while (p.suivant(v))
        ++n;
    VERIFIER(n == 2);
    Parcours q(lu.valeur.champ("b"));
    n = 0;
    while (q.suivant(v))
        ++n;
    VERIFIER(n == 3);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(des_nombres)                                                                                                     \
    X(un_tableau_vide_ou_d_un_element)                                                                                 \
    X(des_chaines_qui_contiennent_des_signes_du_json)                                                                  \
    X(des_objets_et_des_tableaux_imbriques)                                                                            \
    X(le_parcours_rend_les_genres_attendus)                                                                            \
    X(autre_chose_qu_un_tableau_donne_un_parcours_vide)                                                                \
    X(un_long_tableau_est_lu_en_entier)                                                                                \
    X(un_parcours_s_arrete_au_bout_du_tableau_lu_dans_un_objet)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
