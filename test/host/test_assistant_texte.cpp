// Tests du noyau pur : outils de texte de l'assistant (UTF-8 strict, tampon de sortie) — assistant_texte.hpp.
#include "ecran/assistant_texte.hpp"
#include "mini.hpp"

using namespace ecran::assistant;

namespace {

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

constexpr int longueur_attendue_d_apres_le_premier_octet() {
    VERIFIER(utf8::longueur_attendue('a') == 1);
    VERIFIER(utf8::longueur_attendue('\x7f') == 1);
    VERIFIER(utf8::longueur_attendue('\xC2') == 2);
    VERIFIER(utf8::longueur_attendue('\xDF') == 2);
    VERIFIER(utf8::longueur_attendue('\xE0') == 3);
    VERIFIER(utf8::longueur_attendue('\xEF') == 3);
    VERIFIER(utf8::longueur_attendue('\xF0') == 4);
    VERIFIER(utf8::longueur_attendue('\xF4') == 4);
    VERIFIER(utf8::longueur_attendue('\x80') == 0);  // continuation isolée
    VERIFIER(utf8::longueur_attendue('\xBF') == 0);
    VERIFIER(utf8::longueur_attendue('\xC0') == 0);  // forme sur-longue
    VERIFIER(utf8::longueur_attendue('\xC1') == 0);
    VERIFIER(utf8::longueur_attendue('\xF5') == 0);  // au-delà de U+10FFFF
    VERIFIER(utf8::longueur_attendue('\xFF') == 0);
    return 0;
}

constexpr int sequences_valides_et_invalides() {
    VERIFIER(utf8::sequence_valide("a", 0) == 1);
    VERIFIER(utf8::sequence_valide("é", 0) == 2);
    VERIFIER(utf8::sequence_valide("€", 0) == 3);
    VERIFIER(utf8::sequence_valide("😀", 0) == 4);
    VERIFIER(utf8::sequence_valide("aé", 1) == 2);
    VERIFIER(utf8::sequence_valide("", 0) == 0);          // rien
    VERIFIER(utf8::sequence_valide("a", 1) == 0);         // hors du texte
    VERIFIER(utf8::sequence_valide("\xC3", 0) == 0);      // incomplète
    VERIFIER(utf8::sequence_valide("\xE2\x82", 0) == 0);
    VERIFIER(utf8::sequence_valide("\xF0\x9F\x98", 0) == 0);
    VERIFIER(utf8::sequence_valide("\xC3(", 0) == 0);     // octet de suite invalide
    VERIFIER(utf8::sequence_valide("\xE2\x28\xA1", 0) == 0);
    VERIFIER(utf8::sequence_valide("\xC0\x80", 0) == 0);  // sur-longue 2 octets
    VERIFIER(utf8::sequence_valide("\xE0\x80\x80", 0) == 0);  // sur-longue 3 octets
    VERIFIER(utf8::sequence_valide("\xE0\xA0\x80", 0) == 3);  // U+0800, la plus petite valide sur 3
    VERIFIER(utf8::sequence_valide("\xF0\x80\x80\x80", 0) == 0);  // sur-longue 4 octets
    VERIFIER(utf8::sequence_valide("\xF0\x90\x80\x80", 0) == 4);  // U+10000
    VERIFIER(utf8::sequence_valide("\xED\x9F\xBF", 0) == 3);      // U+D7FF, juste avant les surrogates
    VERIFIER(utf8::sequence_valide("\xED\xA0\x80", 0) == 0);      // U+D800
    VERIFIER(utf8::sequence_valide("\xED\xBF\xBF", 0) == 0);      // U+DFFF
    VERIFIER(utf8::sequence_valide("\xEE\x80\x80", 0) == 3);      // U+E000, juste après
    VERIFIER(utf8::sequence_valide("\xF4\x8F\xBF\xBF", 0) == 4);  // U+10FFFF
    VERIFIER(utf8::sequence_valide("\xF4\x90\x80\x80", 0) == 0);  // U+110000
    VERIFIER(utf8::sequence_valide("\xF5\x80\x80\x80", 0) == 0);
    return 0;
}

constexpr int validation_d_un_texte_entier() {
    VERIFIER(utf8::valide(""));
    VERIFIER(utf8::valide("Ça va ? « oui » 5 € 😀"));
    VERIFIER(!utf8::valide("a\xFF"));
    VERIFIER(!utf8::valide("é\xC3"));
    VERIFIER(!utf8::valide("\x80"));
    VERIFIER(!utf8::valide("\xED\xA0\x80"));
    return 0;
}

constexpr int points_de_code() {
    VERIFIER(utf8::point_de_code("a", 0, 1) == 0x61U);
    VERIFIER(utf8::point_de_code("é", 0, 2) == 0xE9U);
    VERIFIER(utf8::point_de_code("€", 0, 3) == 0x20ACU);
    VERIFIER(utf8::point_de_code("😀", 0, 4) == 0x1F600U);
    VERIFIER(utf8::point_de_code("xé", 1, 2) == 0xE9U);
    return 0;
}

constexpr int encodage_aller_retour_sur_les_bornes() {
    constexpr std::uint32_t BORNES[] = {0x0, 0x41, 0x7F, 0x80, 0xE9, 0x7FF, 0x800, 0x20AC, 0xD7FF, 0xE000, 0xFFFD, 0xFFFF,
                                        0x10000, 0x1F600, 0x10FFFF};
    for (std::uint32_t cp : BORNES) {
        char b[4] = {};
        const std::size_t n = utf8::encoder(cp, b);
        VERIFIER(n == (cp < 0x80U ? 1U : cp < 0x800U ? 2U : cp < 0x10000U ? 3U : 4U));
        const std::string_view s(b, n);
        VERIFIER(utf8::sequence_valide(s, 0) == n);
        VERIFIER(utf8::point_de_code(s, 0, n) == cp);
    }
    char b[4] = {};
    VERIFIER(utf8::encoder(0xD800U, b) == 0);   // surrogate : pas un scalaire
    VERIFIER(utf8::encoder(0xDFFFU, b) == 0);
    VERIFIER(utf8::encoder(0x110000U, b) == 0);
    VERIFIER(utf8::encoder(0xFFFFFFFFU, b) == 0);
    return 0;
}

constexpr int coupe_sans_casser_un_caractere() {
    constexpr std::string_view S = "aé€😀";  // octets : a | C3 A9 | E2 82 AC | F0 9F 98 80 (10 octets)
    VERIFIER(utf8::coupe_sans_casser(S, 0) == 0);
    VERIFIER(utf8::coupe_sans_casser(S, 1) == 1);
    VERIFIER(utf8::coupe_sans_casser(S, 2) == 1);   // « é » coupé après son premier octet
    VERIFIER(utf8::coupe_sans_casser(S, 3) == 3);
    VERIFIER(utf8::coupe_sans_casser(S, 4) == 3);   // « € » coupé après 1 octet
    VERIFIER(utf8::coupe_sans_casser(S, 5) == 3);   // après 2 octets
    VERIFIER(utf8::coupe_sans_casser(S, 6) == 6);
    VERIFIER(utf8::coupe_sans_casser(S, 7) == 6);   // « 😀 » coupé après 1, 2, 3 octets
    VERIFIER(utf8::coupe_sans_casser(S, 8) == 6);
    VERIFIER(utf8::coupe_sans_casser(S, 9) == 6);
    VERIFIER(utf8::coupe_sans_casser(S, 10) == 10);
    VERIFIER(utf8::coupe_sans_casser(S, 99) == 10);  // au-delà du texte : le texte entier
    VERIFIER(utf8::coupe_sans_casser("\xFF\xFF", 2) == 2);  // octets invalides : pas un caractère coupé
    VERIFIER(utf8::coupe_sans_casser("\x80\x80\x80\x80\x80", 5) == 5);
    return 0;
}

constexpr int tampon_ecrit_sans_deborder() {
    char d[8] = {};
    Tampon t(d, sizeof(d));
    VERIFIER(t.ok() && t.taille() == 0 && t.capacite() == 8 && t.vue().empty());
    VERIFIER(t.ecrire("abc") && t.ecrire('d') && t.ecrire(std::string_view("ef", 2)));
    VERIFIER(egal(t.vue(), "abcdef") && t.taille() == 6);
    VERIFIER(t.ecrire("gh"));                       // pile la capacité
    VERIFIER(t.ok() && egal(t.vue(), "abcdefgh"));
    VERIFIER(!t.ecrire('i'));                       // un de trop
    VERIFIER(t.deborde() && !t.ok());
    VERIFIER(egal(t.vue(), "abcdefgh"));
    VERIFIER(!t.ecrire(""));                        // après un débordement, plus rien n'est écrit
    t.vider();
    VERIFIER(t.ok() && t.taille() == 0);
    return 0;
}

constexpr int une_ecriture_trop_grosse_n_ecrit_rien_du_tout() {
    char d[8] = {};
    Tampon t(d, sizeof(d));
    VERIFIER(t.ecrire("abcd"));
    VERIFIER(!t.ecrire("efghi"));  // 5 octets pour 4 places : rien ne part, pas même « efgh »
    VERIFIER(egal(t.vue(), "abcd") && t.deborde());
    Tampon u(d, sizeof(d));
    VERIFIER(!u.ecrire("123456789"));
    VERIFIER(u.taille() == 0 && u.deborde());
    return 0;
}

constexpr int tampon_ecrit_des_nombres() {
    char d[40] = {};
    Tampon t(d, sizeof(d));
    VERIFIER(t.ecrire_decimal(0) && t.ecrire('|') && t.ecrire_decimal(7) && t.ecrire('|') && t.ecrire_decimal(1234567890));
    VERIFIER(t.ecrire('|') && t.ecrire_decimal(18446744073709551615ULL));
    VERIFIER(egal(t.vue(), "0|7|1234567890|18446744073709551615"));
    char p[3] = {};
    Tampon petit(p, sizeof(p));
    VERIFIER(!petit.ecrire_decimal(1000));
    VERIFIER(petit.taille() == 0 && petit.deborde());
    return 0;
}

}  // namespace

#define LISTE(X)                                                  \
    X(longueur_attendue_d_apres_le_premier_octet)                 \
    X(sequences_valides_et_invalides)                             \
    X(validation_d_un_texte_entier)                               \
    X(points_de_code)                                             \
    X(encodage_aller_retour_sur_les_bornes)                       \
    X(coupe_sans_casser_un_caractere)                             \
    X(tampon_ecrit_sans_deborder)                                 \
    X(une_ecriture_trop_grosse_n_ecrit_rien_du_tout)              \
    X(tampon_ecrit_des_nombres)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
