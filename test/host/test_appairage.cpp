// Tests du noyau pur : appairage de la page de réglages (appairage.hpp). Code à 6 chiffres, verrou après 5 essais ratés,
// jetons de session (4 au plus), comparaison à temps constant.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/appairage.hpp"
#include "mini.hpp"

namespace ap = ecran::appairage;
using sv = std::string_view;

namespace {

// Générateur d'octets déterministe : 0, 1, 2… puis, au jeton suivant, on repart 7 plus loin.
struct Alea {
    std::uint8_t graine = 0;
    int appels = 0;
    constexpr void operator()(std::uint8_t *sortie, std::size_t n) {
        ++appels;
        for (std::size_t i = 0; i < n; ++i)
            sortie[i] = static_cast<std::uint8_t>(graine + i);
        graine = static_cast<std::uint8_t>(graine + 7);
    }
};

constexpr int code_a_six_chiffres_avec_zeros_de_tete() {
    ap::Gestionnaire g;
    g.nouveau_code(4217);
    VERIFIER(g.code() == "004217");
    g.nouveau_code(1234567);
    VERIFIER(g.code() == "234567");
    g.nouveau_code(0);
    VERIFIER(g.code() == "000000");
    g.nouveau_code(4294967295u);
    VERIFIER(g.code() == "967295");
    VERIFIER(g.code().size() == ap::CODE_CHIFFRES);
    return 0;
}

constexpr int bon_code_donne_un_jeton_hexadecimal() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    const ap::Resultat r = g.essayer("123456", 1000, alea);
    VERIFIER(r.issue == ap::Issue::Accepte);
    VERIFIER(sv(r.jeton) == "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    VERIFIER(sv(r.jeton).size() == ap::JETON_CARACTERES);
    VERIFIER(g.jeton_valide(r.jeton));
    VERIFIER(g.sessions_ouvertes() == 1);
    VERIFIER(alea.appels == 1);
    return 0;
}

constexpr int mauvais_code_est_refuse_sans_tirer_d_octets() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    VERIFIER(g.essayer("123457", 0, alea).issue == ap::Issue::Refuse);
    VERIFIER(g.essayer("12345", 0, alea).issue == ap::Issue::Refuse);   // trop court
    VERIFIER(g.essayer("1234567", 0, alea).issue == ap::Issue::Refuse); // trop long
    VERIFIER(g.essayer("", 0, alea).issue == ap::Issue::Refuse);
    VERIFIER(alea.appels == 0);
    VERIFIER(g.sessions_ouvertes() == 0);
    return 0;
}

constexpr int cinq_essais_rates_verrouillent_soixante_secondes() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    for (int i = 0; i < 4; ++i)
        VERIFIER(g.essayer("000000", 10000, alea).issue == ap::Issue::Refuse);
    const ap::Resultat cinquieme = g.essayer("000000", 10000, alea);
    VERIFIER(cinquieme.issue == ap::Issue::Verrouille && cinquieme.reessayer_dans == 60);
    // pendant le verrou, même le bon code est refusé, et le compte à rebours avance
    ap::Resultat r = g.essayer("123456", 10000 + 10000, alea);
    VERIFIER(r.issue == ap::Issue::Verrouille && r.reessayer_dans == 50);
    r = g.essayer("123456", 10000 + 59500, alea);
    VERIFIER(r.issue == ap::Issue::Verrouille && r.reessayer_dans == 1); // une demi-seconde restante : « 1 seconde »
    VERIFIER(alea.appels == 0);
    // le verrou levé (60 s exactement), le bon code passe
    r = g.essayer("123456", 10000 + 60000, alea);
    VERIFIER(r.issue == ap::Issue::Accepte);
    return 0;
}

constexpr int le_compte_repart_a_zero_apres_le_verrou() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    for (int i = 0; i < 5; ++i)
        g.essayer("000000", 0, alea);
    // 4 mauvais essais après le verrou : pas de nouveau verrou, le bon code passe encore
    for (int i = 0; i < 4; ++i)
        VERIFIER(g.essayer("000000", 70000, alea).issue == ap::Issue::Refuse);
    VERIFIER(g.essayer("123456", 70000, alea).issue == ap::Issue::Accepte);
    return 0;
}

constexpr int un_succes_remet_le_compte_a_zero() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    for (int i = 0; i < 4; ++i)
        g.essayer("000000", 0, alea);
    VERIFIER(g.essayer("123456", 0, alea).issue == ap::Issue::Accepte);
    for (int i = 0; i < 4; ++i)
        VERIFIER(g.essayer("000000", 0, alea).issue == ap::Issue::Refuse); // 4 de plus : toujours pas verrouillé
    VERIFIER(g.essayer("000000", 0, alea).issue == ap::Issue::Verrouille);
    return 0;
}

constexpr int quatre_sessions_au_plus_la_plus_ancienne_est_fermee() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    char jetons[5][ap::JETON_CARACTERES + 1] = {};
    for (int i = 0; i < 4; ++i) {
        const ap::Resultat r = g.essayer("123456", 0, alea);
        VERIFIER(r.issue == ap::Issue::Accepte);
        for (std::size_t k = 0; k <= ap::JETON_CARACTERES; ++k)
            jetons[i][k] = r.jeton[k];
    }
    VERIFIER(g.sessions_ouvertes() == 4);
    for (int i = 0; i < 4; ++i)
        VERIFIER(g.jeton_valide(jetons[i]));
    const ap::Resultat cinquieme = g.essayer("123456", 0, alea);
    for (std::size_t k = 0; k <= ap::JETON_CARACTERES; ++k)
        jetons[4][k] = cinquieme.jeton[k];
    VERIFIER(g.sessions_ouvertes() == 4);
    VERIFIER(!g.jeton_valide(jetons[0])); // la plus ancienne
    for (int i = 1; i < 5; ++i)
        VERIFIER(g.jeton_valide(jetons[i]));
    // la sixième ferme la deuxième, pas la troisième
    const ap::Resultat sixieme = g.essayer("123456", 0, alea);
    VERIFIER(!g.jeton_valide(jetons[1]) && g.jeton_valide(jetons[2]) && g.jeton_valide(sixieme.jeton));
    return 0;
}

constexpr int fermer_une_session_ne_touche_pas_les_autres() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    const ap::Resultat a = g.essayer("123456", 0, alea);
    const ap::Resultat b = g.essayer("123456", 0, alea);
    g.fermer(a.jeton);
    VERIFIER(!g.jeton_valide(a.jeton));
    VERIFIER(g.jeton_valide(b.jeton));
    VERIFIER(g.sessions_ouvertes() == 1);
    g.fermer("n'importe quoi"); // sans effet
    g.fermer("");
    VERIFIER(g.sessions_ouvertes() == 1);
    // la place libérée sert avant d'en fermer une autre
    for (int i = 0; i < 3; ++i)
        g.essayer("123456", 0, alea);
    VERIFIER(g.sessions_ouvertes() == 4 && g.jeton_valide(b.jeton));
    return 0;
}

constexpr int jeton_inconnu_vide_ou_modifie_est_refuse() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    VERIFIER(!g.jeton_valide("")); // aucune session : le texte vide ne vaut pas un jeton
    VERIFIER(!g.jeton_valide("0000000000000000000000000000000000000000000000000000000000000000"));
    char zeros[ap::JETON_CARACTERES] =
        {}; // 64 octets nuls : c'est le contenu d'une session fermée, ça ne doit jamais ouvrir l'accès
    VERIFIER(!g.jeton_valide(sv(zeros, ap::JETON_CARACTERES)));
    Alea alea;
    const ap::Resultat r = g.essayer("123456", 0, alea);
    char copie[ap::JETON_CARACTERES + 1] = {};
    for (std::size_t k = 0; k < ap::JETON_CARACTERES; ++k)
        copie[k] = r.jeton[k];
    VERIFIER(g.jeton_valide(copie));
    copie[ap::JETON_CARACTERES - 1] = copie[ap::JETON_CARACTERES - 1] == '0' ? '1' : '0'; // dernier caractère changé
    VERIFIER(!g.jeton_valide(copie));
    copie[ap::JETON_CARACTERES - 1] = r.jeton[ap::JETON_CARACTERES - 1];
    copie[0] = copie[0] == '0' ? '1' : '0'; // premier caractère changé
    VERIFIER(!g.jeton_valide(copie));
    VERIFIER(!g.jeton_valide(sv(r.jeton, ap::JETON_CARACTERES - 1))); // trop court
    return 0;
}

constexpr int un_nouveau_code_ferme_les_sessions() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    const ap::Resultat r = g.essayer("123456", 0, alea);
    g.nouveau_code(654321);
    VERIFIER(!g.jeton_valide(r.jeton));
    VERIFIER(g.sessions_ouvertes() == 0);
    VERIFIER(g.essayer("123456", 0, alea).issue == ap::Issue::Refuse);
    VERIFIER(g.essayer("654321", 0, alea).issue == ap::Issue::Accepte);
    return 0;
}

// Cinq mauvais codes à l'instant `t` : rend le dernier résultat (celui du verrou).
template <class A> constexpr ap::Resultat serie_ratee(ap::Gestionnaire &g, std::int64_t t, A &alea) {
    ap::Resultat r;
    for (int i = 0; i < ap::ESSAIS_MAX; ++i)
        r = g.essayer("999999", t, alea);
    return r;
}

constexpr int le_verrou_double_a_chaque_serie_jusqu_a_une_heure() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    constexpr int attendu[] = {60, 120, 240, 480, 960, 1920, 3600, 3600, 3600};
    std::int64_t t = 0;
    for (int k = 0; k < 9; ++k) {
        const ap::Resultat r = serie_ratee(g, t, alea);
        VERIFIER(r.issue == ap::Issue::Verrouille && r.reessayer_dans == attendu[k]);
        // toujours verrouillé une seconde avant la fin, levé à la fin
        VERIFIER(g.essayer("999999", t + attendu[k] * 1000 - 1000, alea).issue == ap::Issue::Verrouille);
        t += attendu[k] * 1000;
    }
    VERIFIER(ap::duree_verrou(1) == 60000 && ap::duree_verrou(7) == 3600000 && ap::duree_verrou(1000) == 3600000);
    return 0;
}

constexpr int la_troisieme_serie_ratee_change_le_code_sans_fermer_les_sessions() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    const ap::Resultat ouverte = g.essayer("123456", 0, alea);
    VERIFIER(ouverte.issue == ap::Issue::Accepte);
    const std::uint32_t gen = g.generation();
    // deux séries : le code ne bouge pas (quelqu'un qui recopie mal garde le code qu'il lit)
    ap::Resultat r = serie_ratee(g, 0, alea);
    VERIFIER(!r.code_change && g.code() == "123456");
    r = serie_ratee(g, 60000, alea);
    VERIFIER(!r.code_change && g.code() == "123456" && g.generation() == gen && alea.appels == 1);
    // la troisième : nouveau code, tiré des 4 octets suivants du générateur (7, 8, 9, 10 -> 0x0A090807 = 168364039)
    r = serie_ratee(g, 180000, alea);
    VERIFIER(r.code_change && r.reessayer_dans == 240 && alea.appels == 2);
    VERIFIER(g.code() == "364039" && g.generation() == gen + 1);
    VERIFIER(g.jeton_valide(ouverte.jeton) && g.sessions_ouvertes() == 1); // la session ouverte reste
    // le verrou levé : l'ancien code est refusé, le nouveau accepté
    VERIFIER(g.essayer("123456", 420000, alea).issue == ap::Issue::Refuse);
    VERIFIER(g.essayer("364039", 420000, alea).issue == ap::Issue::Accepte);
    // chaque série suivante change encore le code (après un succès, il faut de nouveau trois séries)
    serie_ratee(g, 420000, alea);
    serie_ratee(g, 480000, alea);
    VERIFIER(g.code() == "364039");
    r = serie_ratee(g, 600000, alea);
    VERIFIER(r.code_change && g.code() != "364039");
    const std::uint32_t apres = g.generation();
    r = serie_ratee(g, 840000, alea);
    VERIFIER(r.code_change && r.reessayer_dans == 480 && g.generation() == apres + 1);
    return 0;
}

constexpr int un_succes_ramene_le_verrou_a_une_minute() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    serie_ratee(g, 0, alea);
    VERIFIER(serie_ratee(g, 60000, alea).reessayer_dans == 120);
    VERIFIER(g.essayer("123456", 180000, alea).issue == ap::Issue::Accepte);
    VERIFIER(serie_ratee(g, 180000, alea).reessayer_dans == 60);
    return 0;
}

constexpr int un_nouveau_code_leve_le_verrou_et_sa_duree() {
    ap::Gestionnaire g;
    g.nouveau_code(123456);
    Alea alea;
    std::int64_t t = 0;
    for (int k = 0; k < 4; ++k) {
        t += ap::duree_verrou(k) * (k > 0 ? 1 : 0);
        serie_ratee(g, t, alea);
    }
    VERIFIER(g.essayer("123456", t + 1000, alea).issue == ap::Issue::Verrouille);
    const std::uint32_t gen = g.generation();
    g.nouveau_code(654321); // le bouton de l'écran, pendant le verrou de 480 s
    VERIFIER(g.generation() == gen + 1);
    // plus de verrou, et la série ratée suivante ne verrouille qu'une minute (sans succès entre deux)
    VERIFIER(serie_ratee(g, t + 1000, alea).reessayer_dans == 60);
    g.nouveau_code(111111); // pendant ce verrou d'une minute : le bon code passe tout de suite
    VERIFIER(g.essayer("111111", t + 2000, alea).issue == ap::Issue::Accepte);
    return 0;
}

constexpr int le_code_ecrit_est_celui_du_gestionnaire() {
    constexpr std::uint32_t valeurs[] = {0u, 4217u, 999999u, 1000000u, 1234567u, 4294967295u};
    for (std::uint32_t v : valeurs) {
        ap::Gestionnaire g;
        g.nouveau_code(v);
        char texte[ap::CODE_CHIFFRES + 1] = {};
        ap::ecrire_code(v, texte);
        VERIFIER(sv(texte) == g.code() && texte[ap::CODE_CHIFFRES] == '\0');
    }
    ap::Gestionnaire g;
    VERIFIER(g.generation() == 0);
    g.nouveau_code(1);
    g.nouveau_code(1); // même code : c'est quand même un nouveau code (sessions fermées), l'écran doit le savoir
    VERIFIER(g.generation() == 2);
    return 0;
}

constexpr int egalite_a_temps_constant() {
    VERIFIER(ap::egal_temps_constant("abc", "abc"));
    VERIFIER(ap::egal_temps_constant("", ""));
    VERIFIER(!ap::egal_temps_constant("abc", "abd"));
    VERIFIER(!ap::egal_temps_constant("abc", "xbc"));
    VERIFIER(!ap::egal_temps_constant("abc", "ab")); // préfixe
    VERIFIER(!ap::egal_temps_constant("ab", "abc"));
    VERIFIER(!ap::egal_temps_constant("", "a"));
    VERIFIER(!ap::egal_temps_constant("\xFF", "\x7F")); // octets au-delà de 127
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(code_a_six_chiffres_avec_zeros_de_tete)                                                                          \
    X(bon_code_donne_un_jeton_hexadecimal)                                                                             \
    X(mauvais_code_est_refuse_sans_tirer_d_octets)                                                                     \
    X(cinq_essais_rates_verrouillent_soixante_secondes)                                                                \
    X(le_compte_repart_a_zero_apres_le_verrou)                                                                         \
    X(un_succes_remet_le_compte_a_zero)                                                                                \
    X(quatre_sessions_au_plus_la_plus_ancienne_est_fermee)                                                             \
    X(fermer_une_session_ne_touche_pas_les_autres)                                                                     \
    X(jeton_inconnu_vide_ou_modifie_est_refuse)                                                                        \
    X(un_nouveau_code_ferme_les_sessions)                                                                              \
    X(le_verrou_double_a_chaque_serie_jusqu_a_une_heure)                                                               \
    X(la_troisieme_serie_ratee_change_le_code_sans_fermer_les_sessions)                                                \
    X(un_succes_ramene_le_verrou_a_une_minute)                                                                         \
    X(un_nouveau_code_leve_le_verrou_et_sa_duree)                                                                      \
    X(le_code_ecrit_est_celui_du_gestionnaire)                                                                         \
    X(egalite_a_temps_constant)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
