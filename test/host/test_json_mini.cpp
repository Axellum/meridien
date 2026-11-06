// Tests du noyau pur : lecteur JSON minimal (json_mini.hpp).
// La comparaison avec un json.loads de Python indépendant (réponse réelle d'Open-Meteo) est dans
// test_meteo_openmeteo.cpp, qui lit test/host/donnees/attendu_json_*.inc généré par controle_python.py.
#include "ecran/json_mini.hpp"
#include "mini.hpp"

using namespace ecran::json;
using sv = std::string_view;

namespace {

constexpr Erreur erreur_de(sv t) { return lire(t).erreur; }
constexpr bool valide(sv t) { return lire(t).ok(); }

constexpr int genres_des_valeurs_simples() {
    VERIFIER(lire("{}").valeur.genre() == Genre::Objet);
    VERIFIER(lire("[]").valeur.genre() == Genre::Tableau);
    VERIFIER(lire("\"a\"").valeur.genre() == Genre::Chaine);
    VERIFIER(lire("-12.5e3").valeur.genre() == Genre::Nombre);
    VERIFIER(lire("true").valeur.genre() == Genre::Booleen);
    VERIFIER(lire("false").valeur.genre() == Genre::Booleen);
    VERIFIER(lire("null").valeur.genre() == Genre::Nul);
    VERIFIER(lire("null").valeur.est_nul());
    VERIFIER(!lire("0").valeur.est_nul());
    VERIFIER(lire("  \t\r\n 7 \n").valeur.genre() == Genre::Nombre);  // blancs autour
    VERIFIER(lire("  \t\r\n 7 \n").valeur.brut() == "7");
    bool b = false;
    VERIFIER(lire("true").valeur.en_booleen(b) && b);
    VERIFIER(lire("false").valeur.en_booleen(b) && !b);
    VERIFIER(!lire("1").valeur.en_booleen(b));
    VERIFIER(!lire("\"true\"").valeur.en_booleen(b));
    return 0;
}

constexpr int nombres_valides_et_invalides() {
    constexpr sv BONS[] = {"0",      "-0",     "1",       "-1",       "10",    "0.5",   "-0.5",         "1e5",
                           "1E5",    "1e+5",   "1e-5",    "1.5e10",   "0e0",   "0.0",   "123456789012345678901234567890",
                           "-0.0e-0"};
    constexpr sv MAUVAIS[] = {"01", "-01", "00", "1.", ".5", "+1", "-", "1e", "1e+", "1.e5", "--1", "0x10", "1_000",
                              "NaN", "Infinity", "-Infinity", "1 2", "1,2", "- 1", "1e5.5", "0.5.5"};
    for (sv t : BONS) VERIFIER(valide(t));
    for (sv t : MAUVAIS) VERIFIER(!valide(t));
    return 0;
}

constexpr int litteraux_exacts() {
    VERIFIER(valide("true") && valide("false") && valide("null"));
    for (sv t : {"tru", "truee", "True", "TRUE", "nul", "nulle", "fals", "falsey", "nil", "undefined", "t", "n"}) {
        VERIFIER(!valide(t));
    }
    VERIFIER(valide("[true,false,null]"));
    VERIFIER(!valide("[truex]"));
    VERIFIER(!valide("[nullnull]"));
    return 0;
}

constexpr int blancs_et_structures_vides() {
    VERIFIER(valide(" { } ") && valide("[ ]") && valide("[[],{},[[]]]") && valide("{\"a\":{},\"b\":[]}"));
    VERIFIER(valide("{ \"a\" : 1 , \"b\" : [ 1 , 2 ] }"));
    VERIFIER(lire("[]").valeur.taille() == 0 && lire("{}").valeur.taille() == 0 && lire("[ ]").valeur.taille() == 0);
    VERIFIER(!valide("[1,]") && !valide("{\"a\":1,}") && !valide("[,1]") && !valide("[1 2]") && !valide("{\"a\" 1}"));
    VERIFIER(!valide("{a:1}") && !valide("{'a':1}") && !valide("{1:2}") && !valide("[1;2]") && !valide("{\"a\":}"));
    VERIFIER(!valide("{\"a\":1 \"b\":2}") && !valide("{,}") && !valide("[}") && !valide("{]"));
    return 0;
}

constexpr int erreurs_et_positions() {
    VERIFIER(erreur_de("") == Erreur::Vide);
    VERIFIER(erreur_de("   \n") == Erreur::Vide);
    VERIFIER(erreur_de("{") == Erreur::FinInattendue);
    VERIFIER(erreur_de("[1,2") == Erreur::FinInattendue);
    VERIFIER(erreur_de("{\"a\":") == Erreur::FinInattendue);
    VERIFIER(erreur_de("\"abc") == Erreur::FinInattendue);
    VERIFIER(erreur_de("\"abc\\") == Erreur::FinInattendue);
    VERIFIER(erreur_de("tru") == Erreur::FinInattendue);
    VERIFIER(erreur_de("1 2") == Erreur::TexteEnTrop);
    VERIFIER(erreur_de("{}x") == Erreur::TexteEnTrop);
    VERIFIER(erreur_de("[1]]") == Erreur::TexteEnTrop);
    VERIFIER(erreur_de("{}{}") == Erreur::TexteEnTrop);
    VERIFIER(erreur_de("@") == Erreur::Syntaxe);
    VERIFIER(erreur_de("[1,@]") == Erreur::Syntaxe);
    VERIFIER(lire("[1,@]").position == 3);   // où l'erreur a été vue
    VERIFIER(lire("1 2").position == 2);
    VERIFIER(lire("[1] x").position == 4);
    VERIFIER(lire("[1,@]").valeur.genre() == Genre::Absent);  // jamais de valeur partielle
    VERIFIER(lire("[1,@]").ok() == false && lire("[1]").ok());
    VERIFIER(message(Erreur::Syntaxe) == "syntaxe invalide");
    VERIFIER(message(Erreur::Aucune) == "aucune erreur" && message(Erreur::Vide) == "texte vide");
    VERIFIER(message(Erreur::TropProfond) == "trop d'imbrications");
    return 0;
}

// {"a":{"a":...{}...}} (objets) ou [[...[]...]] (tableaux) sur `niveaux` niveaux.
constexpr Erreur imbriques(int niveaux, bool objets) {
    char buf[160] = {};
    int n = 0;
    for (int i = 0; i < niveaux; ++i) {
        buf[n++] = objets ? '{' : '[';
        if (objets && i + 1 < niveaux) {
            for (char c : {'"', 'a', '"', ':'}) buf[n++] = c;
        }
    }
    for (int i = 0; i < niveaux; ++i) buf[n++] = objets ? '}' : ']';
    return lire(sv(buf, static_cast<std::size_t>(n))).erreur;
}

constexpr int profondeur_limitee() {
    VERIFIER(PROFONDEUR_MAX == 24);
    VERIFIER(imbriques(PROFONDEUR_MAX, false) == Erreur::Aucune);
    VERIFIER(imbriques(PROFONDEUR_MAX + 1, false) == Erreur::TropProfond);
    VERIFIER(imbriques(PROFONDEUR_MAX, true) == Erreur::Aucune);
    VERIFIER(imbriques(PROFONDEUR_MAX + 1, true) == Erreur::TropProfond);
    VERIFIER(imbriques(1, false) == Erreur::Aucune);
    return 0;
}

constexpr int chaines_avec_echappements() {
    VERIFIER(lire("\"\"").valeur.texte_egal(""));
    VERIFIER(lire("\"abc\"").valeur.texte_egal("abc"));
    VERIFIER(!lire("\"abc\"").valeur.texte_egal("abd"));
    VERIFIER(!lire("\"abc\"").valeur.texte_egal("ab"));    // préfixe
    VERIFIER(!lire("\"abc\"").valeur.texte_egal("abcd"));  // plus long
    VERIFIER(lire("\"a\\\"b\"").valeur.texte_egal("a\"b"));
    VERIFIER(lire("\"a\\\\b\"").valeur.texte_egal("a\\b"));
    VERIFIER(lire("\"a\\/b\"").valeur.texte_egal("a/b"));
    VERIFIER(lire("\"\\b\\f\\n\\r\\t\"").valeur.texte_egal("\b\f\n\r\t"));
    VERIFIER(lire("\"\\u0041\\u00e9\\u00E9\"").valeur.texte_egal("Aéé"));
    VERIFIER(lire("\"é à ° €\"").valeur.texte_egal("é à ° €"));   // UTF-8 brut passé tel quel
    VERIFIER(lire("\"\\u20ac\"").valeur.texte_egal("€"));        // 3 octets
    VERIFIER(lire("\"\\u00b0C\"").valeur.texte_egal("°C"));      // 2 octets
    VERIFIER(!lire("12").valeur.texte_egal("12"));                // un nombre n'est pas une chaîne
    VERIFIER(!lire("[\"a\"]").valeur.texte_egal("a"));
    // échappements invalides
    VERIFIER(!valide("\"\\x41\"") && !valide("\"\\u12\"") && !valide("\"\\u12G4\"") && !valide("\"\\\""));
    VERIFIER(!valide("\"a\nb\"") && !valide("\"a\tb\"") && !valide("\"a\x01\""));  // contrôle brut interdit
    VERIFIER(valide("\"\\uD83D\\uDE00\"") && valide("\"\\ud83d\"") && valide("\"\\udc00\""));
    return 0;
}

constexpr int unicode_paires_et_isoles() {
    // U+1F600 = F0 9F 98 80
    VERIFIER(lire("\"\\ud83d\\ude00\"").valeur.texte_egal("\xF0\x9F\x98\x80"));
    VERIFIER(lire("\"\\uD83D\\uDE00x\"").valeur.texte_egal("\xF0\x9F\x98\x80x"));
    // moitié haute seule, moitié basse seule, haute suivie d'un non-bas, \u0000 : U+FFFD (EF BF BD)
    VERIFIER(lire("\"\\ud83d\"").valeur.texte_egal("\xEF\xBF\xBD"));
    VERIFIER(lire("\"\\ude00\"").valeur.texte_egal("\xEF\xBF\xBD"));
    VERIFIER(lire("\"\\ud83d\\u0041\"").valeur.texte_egal("\xEF\xBF\xBD" "A"));
    VERIFIER(lire("\"a\\u0000b\"").valeur.texte_egal("a\xEF\xBF\xBD" "b"));
    return 0;
}

constexpr int copie_de_texte_et_troncature() {
    char buf[16] = {};
    bool tronque = true;
    VERIFIER(lire("\"Europe/Paris\"").valeur.copier_texte(buf, sizeof buf, &tronque) == 12);
    VERIFIER(!tronque && sv(buf) == "Europe/Paris");
    // exactement la place : 12 octets + le zéro dans 13
    char juste[13] = {};
    VERIFIER(lire("\"Europe/Paris\"").valeur.copier_texte(juste, sizeof juste, &tronque) == 12 && !tronque);
    char court[12] = {};
    VERIFIER(lire("\"Europe/Paris\"").valeur.copier_texte(court, sizeof court, &tronque) == 11);
    VERIFIER(tronque && sv(court) == "Europe/Pari");
    // jamais au milieu d'un caractère : « é » prend 2 octets
    char petit[3] = {};
    VERIFIER(lire("\"aé\"").valeur.copier_texte(petit, sizeof petit, &tronque) == 1);
    VERIFIER(sv(petit) == "a");  // « é » (2 octets) ne rentre pas dans les 2 octets restants - le zéro
    VERIFIER(tronque);
    char ok3[4] = {};
    VERIFIER(lire("\"aé\"").valeur.copier_texte(ok3, sizeof ok3, &tronque) == 3);
    VERIFIER(!tronque && sv(ok3) == "aé");
    // échappements décodés à la copie
    VERIFIER(lire("\"x\\u00b0\\n\"").valeur.copier_texte(buf, sizeof buf) == 4);
    VERIFIER(sv(buf) == "x°\n");
    // pas une chaîne : tampon vidé, 0 octet
    buf[0] = 'z';
    VERIFIER(lire("12").valeur.copier_texte(buf, sizeof buf) == 0 && buf[0] == '\0');
    // capacité nulle : rien d'écrit, signalé tronqué
    VERIFIER(lire("\"abc\"").valeur.copier_texte(nullptr, 0, &tronque) == 0 && tronque);
    // capacité 1 : seulement le zéro
    char un[1] = {'z'};
    VERIFIER(lire("\"abc\"").valeur.copier_texte(un, 1, &tronque) == 0 && un[0] == '\0' && tronque);
    return 0;
}

constexpr int acces_aux_champs_et_elements() {
    constexpr sv DOC = R"({"a": 1, "b": [10, 20, {"c": "x"}], "d": {"e": null, "f": true}, "g": "h"})";
    const Resultat r = lire(DOC);
    VERIFIER(r.ok());
    const Valeur v = r.valeur;
    VERIFIER(v.taille() == 4);
    VERIFIER(v.champ("a").genre() == Genre::Nombre && v.champ("a").brut() == "1");
    VERIFIER(v.champ("b").taille() == 3);
    VERIFIER(v.champ("b").element(0).brut() == "10" && v.champ("b").element(1).brut() == "20");
    VERIFIER(v.champ("b").element(2).champ("c").texte_egal("x"));
    VERIFIER(v.champ("b").element(3).genre() == Genre::Absent);         // hors limites
    VERIFIER(v.champ("b").element(1000000).genre() == Genre::Absent);
    VERIFIER(v.champ("d").champ("e").est_nul() && v.champ("d").champ("e").existe());
    VERIFIER(v.champ("d").champ("f").genre() == Genre::Booleen);
    VERIFIER(v.champ("d").taille() == 2);
    VERIFIER(v.champ("g").texte_egal("h"));
    VERIFIER(!v.champ("zzz").existe());
    VERIFIER(!v.champ("A").existe());          // la casse compte
    VERIFIER(!v.champ("").existe());
    VERIFIER(!v.element(0).existe());          // un objet n'a pas d'éléments
    VERIFIER(!v.champ("a").champ("x").existe());  // un nombre n'a pas de champs
    VERIFIER(v.champ("g").taille() == 0 && v.champ("a").taille() == 0);
    VERIFIER(v.champ("b").element(2).brut() == R"({"c": "x"})");   // tranche exacte, blancs internes conservés
    return 0;
}

constexpr int tailles_exactes() {
    VERIFIER(lire("[1]").valeur.taille() == 1);
    VERIFIER(lire("[1,2]").valeur.taille() == 2);
    VERIFIER(lire("[[1,2],[3]]").valeur.taille() == 2);
    VERIFIER(lire("[\"a,b\",\"]\"]").valeur.taille() == 2);   // virgules et crochets dans des chaînes
    VERIFIER(lire("{\"a\":1}").valeur.taille() == 1);
    VERIFIER(lire("{\"a\":[1,2,3],\"b\":{\"c\":1,\"d\":2}}").valeur.taille() == 2);
    VERIFIER(lire("[ 1 , 2 , 3 ]").valeur.taille() == 3);
    VERIFIER(lire("[null,null]").valeur.taille() == 2);
    return 0;
}

constexpr int cle_en_double_la_premiere_gagne() {
    const Valeur v = lire(R"({"a":1,"a":2})").valeur;
    std::int64_t n = 0;
    VERIFIER(v.champ("a").en_entier(n) && n == 1);
    VERIFIER(v.taille() == 2);
    return 0;
}

constexpr int cles_et_valeurs_avec_pieges() {
    // une clé qui contient des guillemets échappés, des accolades, des deux-points
    const Valeur v = lire(R"({"x\"y":1,"a}b":2,"c:d":3,"é":4,"\u00e9t\u00e9":5, "[0]":6})").valeur;
    std::int64_t n = 0;
    VERIFIER(v.champ("x\"y").en_entier(n) && n == 1);
    VERIFIER(v.champ("a}b").en_entier(n) && n == 2);
    VERIFIER(v.champ("c:d").en_entier(n) && n == 3);
    VERIFIER(v.champ("é").en_entier(n) && n == 4);
    VERIFIER(v.champ("été").en_entier(n) && n == 5);   // clé écrite avec \u, cherchée en UTF-8
    VERIFIER(v.champ("[0]").en_entier(n) && n == 6);
    VERIFIER(!v.trouver("[0]").existe());              // trouver() lit « [0] » comme un indice, pas comme une clé
    // une valeur chaîne qui contient des délimiteurs ne trompe pas le saut de valeur
    const Valeur w = lire(R"({"a":"}]\"{[","b":2})").valeur;
    VERIFIER(w.champ("b").en_entier(n) && n == 2);
    VERIFIER(w.champ("a").texte_egal("}]\"{["));
    return 0;
}

constexpr int chemins_trouver() {
    constexpr sv DOC = R"({"current":{"t":11.6,"jour":1},"daily":{"time":[10,20,30],"m":[[1,2],[3,4]]},"s":"ok"})";
    const Valeur v = lire(DOC).valeur;
    std::int64_t n = 0;
    VERIFIER(v.trouver("").brut() == v.brut());
    VERIFIER(v.trouver("current.jour").en_entier(n) && n == 1);
    VERIFIER(v.trouver("daily.time[2]").en_entier(n) && n == 30);
    VERIFIER(v.trouver("daily.time[0]").en_entier(n) && n == 10);
    VERIFIER(v.trouver("daily.m[1][0]").en_entier(n) && n == 3);
    VERIFIER(v.trouver("daily.m[0][1]").en_entier(n) && n == 2);
    VERIFIER(v.trouver("daily").trouver("time[1]").en_entier(n) && n == 20);  // relatif à une sous-valeur
    VERIFIER(v.trouver("s").texte_egal("ok"));
    VERIFIER(v.trouver("daily.time").taille() == 3);
    // absents
    VERIFIER(!v.trouver("daily.time[3]").existe());
    VERIFIER(!v.trouver("daily.zzz").existe());
    VERIFIER(!v.trouver("zzz.time").existe());
    VERIFIER(!v.trouver("current.jour.x").existe());
    VERIFIER(!v.trouver("s[0]").existe());
    // chemins mal formés : rien, jamais de crash
    for (sv c : {".current", "current.", "current..jour", "daily.time[", "daily.time[]", "daily.time[x]", "daily.time[1",
                 "daily.time[1]x", "daily.time.[1]", "[", "]", "daily[0]", "daily.time[99999999999999999999]"}) {
        VERIFIER(!v.trouver(c).existe());
    }
    // un chemin qui commence par un indice
    const Valeur t = lire("[[5,6],[7]]").valeur;
    VERIFIER(t.trouver("[1][0]").en_entier(n) && n == 7);
    VERIFIER(t.trouver("[0][1]").en_entier(n) && n == 6);
    return 0;
}

constexpr int valeur_absente_sans_danger() {
    const Valeur a;  // jamais initialisée par lire
    std::int64_t n = 99;
    double d = 99;
    bool b = true;
    char buf[4] = {'x', 0, 0, 0};
    VERIFIER(!a.existe() && a.genre() == Genre::Absent);
    VERIFIER(!a.champ("x").existe() && !a.element(0).existe() && !a.trouver("a.b[0]").existe());
    VERIFIER(a.taille() == 0);
    VERIFIER(!a.en_entier(n) && !a.en_fixe(1, n) && !a.en_double(d) && !a.en_booleen(b) && !a.texte_egal(""));
    VERIFIER(n == 99 && d == 99 && b);     // la sortie n'est pas touchée en cas d'échec
    VERIFIER(a.copier_texte(buf, sizeof buf) == 0 && buf[0] == '\0');
    return 0;
}

constexpr int entier_strict() {
    std::int64_t n = 0;
    VERIFIER(lire("0").valeur.en_entier(n) && n == 0);
    VERIFIER(lire("-0").valeur.en_entier(n) && n == 0);
    VERIFIER(lire("325").valeur.en_entier(n) && n == 325);
    VERIFIER(lire("-12").valeur.en_entier(n) && n == -12);
    VERIFIER(lire("1793966400").valeur.en_entier(n) && n == 1793966400);   // un instant UNIX de 2026
    VERIFIER(lire("9223372036854775807").valeur.en_entier(n) && n == INT64_MAX);
    VERIFIER(lire("-9223372036854775808").valeur.en_entier(n) && n == INT64_MIN);
    VERIFIER(!lire("9223372036854775808").valeur.en_entier(n));
    VERIFIER(!lire("-9223372036854775809").valeur.en_entier(n));
    VERIFIER(!lire("99999999999999999999999").valeur.en_entier(n));
    VERIFIER(!lire("3.0").valeur.en_entier(n));     // pas d'arrondi caché
    VERIFIER(!lire("3e0").valeur.en_entier(n));
    VERIFIER(!lire("3.5").valeur.en_entier(n));
    VERIFIER(!lire("\"3\"").valeur.en_entier(n));   // une chaîne n'est pas un nombre
    VERIFIER(!lire("true").valeur.en_entier(n));
    VERIFIER(!lire("null").valeur.en_entier(n));
    return 0;
}

constexpr int fixe_arrondi_sur_le_texte() {
    struct Cas1 {
        sv texte;
        int decimales;
        std::int64_t attendu;
    };
    constexpr Cas1 CAS_OK[] = {
        {"11.6", 1, 116},        {"11.65", 1, 117},      {"-11.65", 1, -117},    {"1.35", 1, 14},
        {"1.34", 1, 13},         {"-0.04", 1, 0},        {"-0.05", 1, -1},       {"0.04", 1, 0},
        {"0.05", 1, 1},          {"3", 1, 30},           {"3", 0, 3},            {"2.5", 0, 3},
        {"-2.5", 0, -3},         {"2.4999", 0, 2},       {"1e2", 0, 100},        {"1.5e1", 0, 15},
        {"1e-3", 2, 0},          {"5e-3", 2, 1},         {"0.0000", 1, 0},       {"0e100", 0, 0},
        {"123456789.987654321", 3, 123456789988},        {"9223372036854775807", 0, INT64_MAX},
        {"-9223372036854775808", 0, INT64_MIN},          {"7e-30", 1, 0},        {"1022.6", 1, 10226},
        {"-31.9", 1, -319},      {"4.4", 1, 44},         {"0.1", 2, 10},         {"12", 2, 1200},
    };
    for (const Cas1& c : CAS_OK) {
        std::int64_t n = -12345;
        VERIFIER(lire(c.texte).valeur.en_fixe(c.decimales, n));
        VERIFIER(n == c.attendu);
    }
    std::int64_t n = 0;
    VERIFIER(!lire("9223372036854775808").valeur.en_fixe(0, n));   // dépasse int64
    VERIFIER(!lire("1e19").valeur.en_fixe(0, n));
    VERIFIER(!lire("9223372036854775807").valeur.en_fixe(1, n));   // ×10 déborde
    VERIFIER(!lire("1").valeur.en_fixe(-1, n) && !lire("1").valeur.en_fixe(19, n));
    VERIFIER(!lire("\"1.5\"").valeur.en_fixe(1, n) && !lire("null").valeur.en_fixe(1, n));
    return 0;
}

constexpr int double_identique_a_strtod() {
    double d = 0;
    // valeurs qu'un double représente à l'arrondi correct : <= 15 chiffres, |exposant| <= 22
    VERIFIER(lire("0").valeur.en_double(d) && d == 0.0);
    VERIFIER(lire("-0").valeur.en_double(d) && d == 0.0);
    VERIFIER(lire("11.6").valeur.en_double(d) && d == 11.6);
    VERIFIER(lire("0.1").valeur.en_double(d) && d == 0.1);
    VERIFIER(lire("-31.9").valeur.en_double(d) && d == -31.9);
    VERIFIER(lire("1022.6").valeur.en_double(d) && d == 1022.6);
    VERIFIER(lire("48.853382").valeur.en_double(d) && d == 48.853382);
    VERIFIER(lire("2.355208").valeur.en_double(d) && d == 2.355208);
    VERIFIER(lire("0.6103515625").valeur.en_double(d) && d == 0.6103515625);
    VERIFIER(lire("123456789.123").valeur.en_double(d) && d == 123456789.123);
    VERIFIER(lire("1e22").valeur.en_double(d) && d == 1e22);
    VERIFIER(lire("1e-22").valeur.en_double(d) && d == 1e-22);
    VERIFIER(lire("-3.5e2").valeur.en_double(d) && d == -350.0);
    VERIFIER(lire("2.5E+3").valeur.en_double(d) && d == 2500.0);
    VERIFIER(lire("1793966400").valeur.en_double(d) && d == 1793966400.0);
    // au-delà de 19 chiffres : approché mais du bon ordre de grandeur
    VERIFIER(lire("12345678901234567890").valeur.en_double(d) && d > 1.23456789e19 && d < 1.23456790e19);
    VERIFIER(lire("1e308").valeur.en_double(d) && d > 9.9e307);
    VERIFIER(!lire("1e999").valeur.en_double(d));        // déborde : refusé, pas d'infini
    VERIFIER(!lire("1e-999").valeur.en_double(d));       // trop petit : refusé aussi
    VERIFIER(!lire("\"1\"").valeur.en_double(d) && !lire("[1]").valeur.en_double(d));
    return 0;
}

}  // namespace

#define LISTE(X)                                \
    X(genres_des_valeurs_simples)               \
    X(nombres_valides_et_invalides)             \
    X(litteraux_exacts)                         \
    X(blancs_et_structures_vides)               \
    X(erreurs_et_positions)                     \
    X(profondeur_limitee)                       \
    X(chaines_avec_echappements)                \
    X(unicode_paires_et_isoles)                 \
    X(copie_de_texte_et_troncature)             \
    X(acces_aux_champs_et_elements)             \
    X(tailles_exactes)                          \
    X(cle_en_double_la_premiere_gagne)          \
    X(cles_et_valeurs_avec_pieges)              \
    X(chemins_trouver)                          \
    X(valeur_absente_sans_danger)               \
    X(entier_strict)                            \
    X(fixe_arrondi_sur_le_texte)                \
    X(double_identique_a_strtod)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
