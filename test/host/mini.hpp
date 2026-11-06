// Mini cadre de test sans dépendance (C++17 standard). Un test est une fonction `constexpr int` qui rend 0 si
// tout va bien, sinon le numéro de la ligne de la première vérification ratée.
//
// Chaque test est évalué DEUX fois :
//   - à la compilation (`static_assert`) : une régression ne COMPILE plus, même si le programme n'est jamais
//     lancé (c'est ce qui permet de tester avec un simple `-fsyntax-only`, voir lancer.py) ;
//   - à l'exécution (`main`) : le programme dit quel test et quelle ligne.
// Utilisation : définir les tests, une liste X-macro `LISTE(X)`, puis `TESTS_A_LA_COMPILATION(LISTE)` et
// `int main() { return lancer_tous(LISTE(CAS)); }` (voir test_meteo_wmo.cpp).
#pragma once

#include <cstddef>
#include <cstdio>

#define VERIFIER(cond)                \
    do {                              \
        if (!(cond)) return __LINE__; \
    } while (0)

struct Cas {
    const char* nom;
    int (*fonction)();
};

#define TEST_A_LA_COMPILATION(nom) \
    static_assert(nom() == 0, #nom " : une verification a echoue (le numero de ligne est dans le message du compilateur)");
#define TESTS_A_LA_COMPILATION(LISTE) LISTE(TEST_A_LA_COMPILATION)
#define CAS(nom) {#nom, nom},

template <std::size_t N>
inline int lancer_tous(const Cas (&cas)[N]) {
    int echecs = 0;
    for (const Cas& c : cas) {
        const int ligne = c.fonction();
        if (ligne == 0) {
            std::printf("  ok      %s\n", c.nom);
        } else {
            std::printf("  ECHEC   %s (verification a la ligne %d)\n", c.nom, ligne);
            ++echecs;
        }
    }
    std::printf("%d test(s), %d echec(s)\n", static_cast<int>(N), echecs);
    return echecs == 0 ? 0 : 1;
}
