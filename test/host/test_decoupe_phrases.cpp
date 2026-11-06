// Tests du noyau pur : découpe de la réponse du LLM en phrases pour le TTS (decoupe_phrases.hpp).
#include "ecran/assistant_texte.hpp"
#include "ecran/decoupe_phrases.hpp"
#include "mini.hpp"

using namespace ecran::assistant;

namespace {

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

// Enregistre les phrases rendues, chacune suivie de « | ».
struct Rec {
    char t[512] = {};
    std::size_t n = 0;
    std::size_t nb = 0;
    bool utf8_valide = true;
    bool trop_long = false;
    std::size_t plus_longue = 0;
    constexpr void operator()(std::string_view p) {
        if (n + p.size() + 1 > sizeof(t)) {
            trop_long = true;
            return;
        }
        for (char c : p)
            t[n++] = c;
        t[n++] = '|';
        ++nb;
        utf8_valide = utf8_valide && utf8::valide(p);
        if (p.size() > plus_longue)
            plus_longue = p.size();
    }
    constexpr std::string_view vue() const { return std::string_view(t, n); }
};

template <std::size_t L = 200> constexpr bool decoupe_donne(std::string_view texte, std::string_view attendu) {
    DecoupePhrases<L> d;
    Rec r;
    d.ajouter(texte, r);
    d.terminer(r);
    return !r.trop_long && r.utf8_valide && egal(r.vue(), attendu);
}

constexpr int phrases_simples() {
    VERIFIER(decoupe_donne("Bonjour. Comment ça va ? Très bien !", "Bonjour.|Comment ça va ?|Très bien !|"));
    VERIFIER(
        decoupe_donne("Bonjour tout le monde", "Bonjour tout le monde|")); // sans ponctuation finale : émis à la fin
    VERIFIER(decoupe_donne("", ""));
    VERIFIER(decoupe_donne("   ", ""));
    VERIFIER(decoupe_donne("Oui.", "Oui.|"));
    return 0;
}

constexpr int blancs_de_bord_retires() {
    VERIFIER(decoupe_donne("  Bonjour.  \r\n  Salut  ", "Bonjour.|Salut|"));
    return 0;
}

constexpr int retours_a_la_ligne_coupent() {
    VERIFIER(decoupe_donne("Ligne un\nLigne deux.\n\nLigne trois", "Ligne un|Ligne deux.|Ligne trois|"));
    VERIFIER(decoupe_donne("a\r\nb\r\nc", "a|b|c|"));
    return 0;
}

constexpr int abreviations_courantes_ne_coupent_pas() {
    VERIFIER(decoupe_donne("M. Dupont est là. Mme Martin aussi.", "M. Dupont est là.|Mme Martin aussi.|"));
    VERIFIER(decoupe_donne("Mme. Martin vient. Dr. Leroy aussi.", "Mme. Martin vient.|Dr. Leroy aussi.|"));
    VERIFIER(decoupe_donne("Mlle. Rose et Pr. Chêne sont là. Fin.", "Mlle. Rose et Pr. Chêne sont là.|Fin.|"));
    VERIFIER(decoupe_donne("Il a des pommes, des poires, etc. Puis il part.",
                           "Il a des pommes, des poires, etc. Puis il part.|"));
    VERIFIER(
        decoupe_donne("Il y en a plusieurs, p. ex. celui-ci. Fin.", "Il y en a plusieurs, p. ex. celui-ci.|Fin.|"));
    VERIFIER(decoupe_donne("Voir cf. la note. Fin.", "Voir cf. la note.|Fin.|"));
    VERIFIER(decoupe_donne("Rue St. Jean, av. J.-C. Fin.", "Rue St. Jean, av. J.-C. Fin.|"));
    VERIFIER(decoupe_donne("Tél. mercredi. Fin.", "Tél. mercredi.|Fin.|"));
    return 0;
}

constexpr int abreviations_sans_tenir_compte_de_la_casse() {
    VERIFIER(decoupe_donne("MME. Durand arrive. dr. Martin aussi.", "MME. Durand arrive.|dr. Martin aussi.|"));
    VERIFIER(decoupe_donne("ETC. oui. Fin.", "ETC. oui.|Fin.|"));
    return 0;
}

constexpr int initiales_et_sigles_ne_coupent_pas() {
    VERIFIER(decoupe_donne("J. K. Rowling écrit. Fin.", "J. K. Rowling écrit.|Fin.|"));
    VERIFIER(decoupe_donne("Aux U.S.A. cela marche. Ok.", "Aux U.S.A. cela marche.|Ok.|"));
    VERIFIER(decoupe_donne("C'est-à-dire i.e. voilà. Ok.", "C'est-à-dire i.e. voilà.|Ok.|"));
    // mais un nom de domaine ou un mot ordinaire coupe
    VERIFIER(decoupe_donne("Allez sur www.exemple.fr. Ok.", "Allez sur www.exemple.fr.|Ok.|"));
    VERIFIER(decoupe_donne("Il a vu un chat. Moi aussi.", "Il a vu un chat.|Moi aussi.|"));
    return 0;
}

constexpr int nombres_decimaux_et_adresses_ne_coupent_pas() {
    VERIFIER(decoupe_donne("Il fait 3.5 degrés. Voir exemple.fr pour plus. Fin.",
                           "Il fait 3.5 degrés.|Voir exemple.fr pour plus.|Fin.|"));
    VERIFIER(decoupe_donne("La v1.2.3 est sortie. Ok.", "La v1.2.3 est sortie.|Ok.|"));
    VERIFIER(decoupe_donne("Il est 10.30 pile. Ok.", "Il est 10.30 pile.|Ok.|"));
    VERIFIER(decoupe_donne("Il a payé 1.000,50 euros. Ok.", "Il a payé 1.000,50 euros.|Ok.|"));
    return 0;
}

constexpr int listes_numerotees_ne_sont_pas_coupees() {
    VERIFIER(decoupe_donne("Voici :\n1. Pommes\n2. Poires\n3. Kiwis", "Voici :|1. Pommes|2. Poires|3. Kiwis|"));
    VERIFIER(decoupe_donne("Étapes : 1. Ouvrir. 2. Fermer.", "Étapes : 1. Ouvrir.|2. Fermer.|"));
    VERIFIER(decoupe_donne("Le 12. Marché ouvert. Fin.", "Le 12. Marché ouvert.|Fin.|"));
    // limite documentée : une phrase qui finit par un petit nombre n'est pas coupée
    VERIFIER(decoupe_donne("Voir la page 3. Puis la suite.", "Voir la page 3. Puis la suite.|"));
    // trois chiffres : ce n'est plus un numéro de liste
    VERIFIER(decoupe_donne("Il y en a 120. Puis la suite.", "Il y en a 120.|Puis la suite.|"));
    return 0;
}

constexpr int points_de_suspension_et_ponctuation_multiple() {
    VERIFIER(decoupe_donne("Hmm… d'accord ! Vraiment ?! Oui...", "Hmm…|d'accord !|Vraiment ?!|Oui...|"));
    VERIFIER(decoupe_donne("Euh... bon. Ok.", "Euh...|bon.|Ok.|"));
    VERIFIER(decoupe_donne("Quoi ? Oui ! Non.", "Quoi ?|Oui !|Non.|"));
    return 0;
}

constexpr int guillemets_et_parentheses_fermants_restent_avec_la_phrase() {
    VERIFIER(decoupe_donne("Il a dit : \"Viens.\" Puis il part.", "Il a dit : \"Viens.\"|Puis il part.|"));
    VERIFIER(decoupe_donne("Il part (vite !) Puis revient.", "Il part (vite !)|Puis revient.|"));
    // guillemets français : l'espace avant « » les sépare du point, le fermant passe à la phrase suivante
    VERIFIER(decoupe_donne("Il a dit : « Viens. » Puis il part.", "Il a dit : « Viens.|» Puis il part.|"));
    return 0;
}

constexpr int phrases_sans_contenu_ne_sont_pas_emises() {
    VERIFIER(decoupe_donne("… Bonjour. - Ok.", "Bonjour.|- Ok.|"));
    VERIFIER(decoupe_donne("*** \n---\nBonjour.", "Bonjour.|"));
    VERIFIER(decoupe_donne("...\n!!!\n???", ""));
    VERIFIER(decoupe_donne("« » … – —", ""));
    return 0;
}

constexpr int phrase_a_cheval_sur_deux_morceaux_attend_la_confirmation() {
    DecoupePhrases<> d;
    Rec r;
    d.ajouter("Bonjour.", r);
    VERIFIER(r.nb == 0); // le blanc qui confirme la fin n'est pas arrivé
    d.ajouter(" Ça va", r);
    VERIFIER(egal(r.vue(), "Bonjour.|"));
    d.ajouter(" ?", r);
    VERIFIER(r.nb == 1); // « ? » est au bout : on attend encore
    d.ajouter(" Oui", r);
    VERIFIER(egal(r.vue(), "Bonjour.|Ça va ?|"));
    d.terminer(r);
    VERIFIER(egal(r.vue(), "Bonjour.|Ça va ?|Oui|"));
    VERIFIER(d.en_attente() == 0);
    return 0;
}

constexpr int decimal_coupe_entre_deux_morceaux() {
    DecoupePhrases<> d;
    Rec r;
    d.ajouter("Il fait 3.", r);
    d.ajouter("5 degrés. Ok.", r);
    d.terminer(r);
    VERIFIER(egal(r.vue(), "Il fait 3.5 degrés.|Ok.|"));
    DecoupePhrases<> e;
    Rec s;
    e.ajouter("M.", s);
    e.ajouter(" Dupont arrive. Fin.", s);
    e.terminer(s);
    VERIFIER(egal(s.vue(), "M. Dupont arrive.|Fin.|"));
    return 0;
}

constexpr int une_reponse_apres_l_autre() {
    DecoupePhrases<> d;
    Rec r;
    d.ajouter("Premier.", r);
    d.terminer(r);
    VERIFIER(egal(r.vue(), "Premier.|") && d.en_attente() == 0);
    d.ajouter("Second", r);
    VERIFIER(d.en_attente() == 6);
    d.remettre_a_zero();
    VERIFIER(d.en_attente() == 0);
    d.ajouter("Troisième.", r);
    d.terminer(r);
    VERIFIER(egal(r.vue(), "Premier.|Troisième.|"));
    return 0;
}

// --- borne de longueur --------------------------------------------------------------------------------------

constexpr int coupe_forcee_a_la_derniere_espace() {
    VERIFIER((decoupe_donne<40>("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll",
                                "aaaa bbbb cccc dddd eeee ffff gggg hhhh|iiii jjjj kkkk llll|")));
    return 0;
}

constexpr int coupe_forcee_prefere_la_virgule() {
    VERIFIER((decoupe_donne<40>("aaaa, bbbb cccc dddd eeee ffff, gggg hhhh iiii jjjj",
                                "aaaa, bbbb cccc dddd eeee ffff,|gggg hhhh iiii jjjj|")));
    return 0;
}

constexpr int coupe_forcee_ignore_les_separateurs_de_la_premiere_moitie() {
    // « aaa, » + 50 « b » : la virgule et l'espace sont dans la première moitié (indices 3 et 4 < 20) : on coupe
    // donc au 40e octet, pas à 4 (une phrase de 4 caractères suivie d'un bloc de 50 ne vaut rien)
    char x[55] = {};
    x[0] = 'a';
    x[1] = 'a';
    x[2] = 'a';
    x[3] = ',';
    x[4] = ' ';
    for (int i = 5; i < 55; ++i)
        x[i] = 'b';
    DecoupePhrases<40> d;
    Rec r;
    d.ajouter(std::string_view(x, 55), r);
    d.terminer(r);
    VERIFIER(r.nb == 2 && r.n == 57);
    VERIFIER(r.t[40] == '|' && r.t[56] == '|');
    return 0;
}

constexpr int coupe_forcee_sans_espace_au_dernier_caractere_entier() {
    // 100 « x » sans espace ni ponctuation : tranches de 40, 40 puis 20 octets
    char x[100] = {};
    for (char &c : x)
        c = 'x';
    DecoupePhrases<40> d;
    Rec r;
    d.ajouter(std::string_view(x, 100), r);
    d.terminer(r);
    VERIFIER(r.nb == 3 && r.n == 103);
    VERIFIER(r.t[40] == '|' && r.t[81] == '|' && r.t[102] == '|');
    // « a » + 30 « é » (61 octets) : la coupe à 40 octets tomberait au milieu d'un « é » -> 39 octets
    char e[61] = {};
    e[0] = 'a';
    for (int i = 0; i < 30; ++i) {
        e[1 + 2 * i] = static_cast<char>(0xC3);
        e[2 + 2 * i] = static_cast<char>(0xA9);
    }
    DecoupePhrases<40> f;
    Rec s;
    f.ajouter(std::string_view(e, 61), s);
    f.terminer(s);
    VERIFIER(s.utf8_valide && s.nb == 2 && s.n == 63);
    VERIFIER(s.t[39] == '|' && s.t[62] == '|'); // 1 + 19 « é » = 39 octets, puis 11 « é » = 22 octets
    return 0;
}

constexpr int jamais_plus_de_limite_octets_en_attente() {
    constexpr std::string_view TEXTE =
        "un texte long sans aucune ponctuation qui continue encore et encore pour verifier que le tampon ne retient "
        "jamais "
        "plus que la limite fixee a la construction de l objet meme si le flux est tres long ";
    DecoupePhrases<64> d;
    Rec r;
    for (int tour = 0; tour < 8; ++tour) {
        for (char c : TEXTE) {
            d.ajouter(std::string_view(&c, 1), r);
            VERIFIER(d.en_attente() < 64);
        }
    }
    d.terminer(r);
    VERIFIER(r.utf8_valide && r.plus_longue <= 64 && r.nb > 8);
    return 0;
}

// --- indépendance vis-à-vis du découpage en morceaux ---------------------------------------------------------

constexpr std::string_view COMPOSITE =
    "M. Dupont a dit : « Bonjour ! » Il fait 3.5 degrés… Voyons voir. Étapes :\n1. Ouvrir\n2. Fermer, puis attendre, "
    "etc. Ensuite, on part ?! Oui. www.exemple.fr est un site.\nFin sans point  é€😀 aussi, peut-être ; et voilà : "
    "c'est fini";

template <std::size_t L> constexpr int coupures_en_deux_morceaux(std::size_t premiere, std::size_t derniere) {
    DecoupePhrases<L> ref;
    Rec attendu;
    ref.ajouter(COMPOSITE, attendu);
    ref.terminer(attendu);
    VERIFIER(attendu.nb >= 6 && attendu.utf8_valide);
    for (std::size_t coupe = premiere; coupe <= derniere; ++coupe) { // une coupure à chaque position
        DecoupePhrases<L> d;
        Rec r;
        d.ajouter(COMPOSITE.substr(0, coupe), r);
        d.ajouter(COMPOSITE.substr(coupe), r);
        d.terminer(r);
        VERIFIER(r.utf8_valide && egal(r.vue(), attendu.vue()));
    }
    return 0;
}

template <std::size_t L> constexpr int morceaux_de_tailles_variees() {
    DecoupePhrases<L> ref;
    Rec attendu;
    ref.ajouter(COMPOSITE, attendu);
    ref.terminer(attendu);
    for (std::size_t taille : {1, 2, 3, 5, 7, 11, 29, 64}) { // 1 = octet par octet
        DecoupePhrases<L> d;
        Rec r;
        for (std::size_t i = 0; i < COMPOSITE.size(); i += taille)
            d.ajouter(COMPOSITE.substr(i, taille), r);
        d.terminer(r);
        VERIFIER(r.utf8_valide && egal(r.vue(), attendu.vue()));
    }
    return 0;
}

// Les coupures sont réparties en tranches de 31 positions : le compilateur plafonne le nombre de pas d'une
// évaluation constexpr (clang : 1 048 576). 8 tranches couvrent les positions 0 à COMPOSITE.size() (vérifié
// ci-dessous).
constexpr std::size_t LARGEUR_TRANCHE = 31;
constexpr std::size_t NB_TRANCHES = 8;
static_assert(NB_TRANCHES * LARGEUR_TRANCHE > COMPOSITE.size(),
              "les tranches doivent couvrir toutes les positions de coupure");

template <std::size_t L, std::size_t K> constexpr int coupures_en_deux_tranche() {
    constexpr std::size_t fin = (K + 1) * LARGEUR_TRANCHE - 1;
    return coupures_en_deux_morceaux<L>(K * LARGEUR_TRANCHE, fin < COMPOSITE.size() ? fin : COMPOSITE.size());
}
template <std::size_t K> constexpr int coupures_en_deux_limite_200() { return coupures_en_deux_tranche<200, K>(); }
template <std::size_t K> constexpr int coupures_en_deux_limite_40() { return coupures_en_deux_tranche<40, K>(); }
constexpr int morceaux_varies_avec_la_limite_par_defaut() { return morceaux_de_tailles_variees<200>(); }
constexpr int morceaux_varies_avec_une_petite_limite() { return morceaux_de_tailles_variees<40>(); }

constexpr int le_texte_composite_donne_les_phrases_attendues() {
    VERIFIER(decoupe_donne(COMPOSITE,
                           "M. Dupont a dit : « Bonjour !|» Il fait 3.5 degrés…|Voyons voir.|Étapes :|1. Ouvrir|"
                           "2. Fermer, puis attendre, etc. Ensuite, on part ?!|Oui.|www.exemple.fr est un site.|"
                           "Fin sans point  é€😀 aussi, peut-être ; et voilà : c'est fini|"));
    return 0;
}

constexpr int a_du_contenu_ponctuation_et_lettres() {
    VERIFIER(detail::a_du_contenu("a"));
    VERIFIER(detail::a_du_contenu("7"));
    VERIFIER(detail::a_du_contenu("… é"));
    VERIFIER(detail::a_du_contenu("😀"));
    VERIFIER(detail::a_du_contenu("日本"));
    VERIFIER(!detail::a_du_contenu(""));
    VERIFIER(!detail::a_du_contenu("... !? - ** « » … – —"));
    VERIFIER(!detail::a_du_contenu("\xFF\xFE")); // octets invalides seuls : rien à dire
    return 0;
}

} // namespace

// clang-format off
#define LISTE(X) \
    X(phrases_simples)                                             \
    X(blancs_de_bord_retires)                                      \
    X(retours_a_la_ligne_coupent)                                  \
    X(abreviations_courantes_ne_coupent_pas)                       \
    X(abreviations_sans_tenir_compte_de_la_casse)                  \
    X(initiales_et_sigles_ne_coupent_pas)                          \
    X(nombres_decimaux_et_adresses_ne_coupent_pas)                 \
    X(listes_numerotees_ne_sont_pas_coupees)                       \
    X(points_de_suspension_et_ponctuation_multiple)                \
    X(guillemets_et_parentheses_fermants_restent_avec_la_phrase)   \
    X(phrases_sans_contenu_ne_sont_pas_emises)                     \
    X(phrase_a_cheval_sur_deux_morceaux_attend_la_confirmation)    \
    X(decimal_coupe_entre_deux_morceaux)                           \
    X(une_reponse_apres_l_autre)                                   \
    X(coupe_forcee_a_la_derniere_espace)                           \
    X(coupe_forcee_prefere_la_virgule)                             \
    X(coupe_forcee_ignore_les_separateurs_de_la_premiere_moitie)   \
    X(coupe_forcee_sans_espace_au_dernier_caractere_entier)        \
    X(jamais_plus_de_limite_octets_en_attente)                     \
    X(coupures_en_deux_limite_200<0>)                              \
    X(coupures_en_deux_limite_200<1>)                              \
    X(coupures_en_deux_limite_200<2>)                              \
    X(coupures_en_deux_limite_200<3>)                              \
    X(coupures_en_deux_limite_200<4>)                              \
    X(coupures_en_deux_limite_200<5>)                              \
    X(coupures_en_deux_limite_200<6>)                              \
    X(coupures_en_deux_limite_200<7>)                              \
    X(morceaux_varies_avec_la_limite_par_defaut)                   \
    X(coupures_en_deux_limite_40<0>)                               \
    X(coupures_en_deux_limite_40<1>)                               \
    X(coupures_en_deux_limite_40<2>)                               \
    X(coupures_en_deux_limite_40<3>)                               \
    X(coupures_en_deux_limite_40<4>)                               \
    X(coupures_en_deux_limite_40<5>)                               \
    X(coupures_en_deux_limite_40<6>)                               \
    X(coupures_en_deux_limite_40<7>)                               \
    X(morceaux_varies_avec_une_petite_limite)                      \
    X(le_texte_composite_donne_les_phrases_attendues)              \
    X(a_du_contenu_ponctuation_et_lettres)
// clang-format on

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
