// Tests du noyau pur : dispositions du clavier à l'écran, sept langues (ecran/clavier.hpp).
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
//
// Ce que ces tests prouvent : chaque lettre de chaque alphabet utile est atteignable en minuscule et en majuscule, chaque touche
// est UN caractère (ou une commande de la liste fermée), minuscules et majuscules ont la même forme, le turc distingue ı/I et
// i/İ. Ce qu'ils NE prouvent PAS : que la police dessine ces caractères (test/host/controle_reglages.py le lit dans la police),
// que les touches soient assez grandes pour un doigt, ni que l'ordre des touches plaise à ceux qui tapent (à constater sur la carte).
#include <cstdio>
#include <initializer_list>

#include "ecran/clavier.hpp"
#include "ecran/i18n.hpp"
#include "mini.hpp"

using ecran::clavier::compter_touches;
using ecran::clavier::Controle;
using ecran::clavier::controle;
using ecran::clavier::disposition;
using ecran::clavier::Disposition;
using ecran::clavier::Mode;
using ecran::clavier::touche;
using ecran::i18n::Langue;
using ecran::i18n::NOMBRE_LANGUES;

namespace {

constexpr Langue TOUTES[NOMBRE_LANGUES] = {Langue::Fr, Langue::En, Langue::De, Langue::Nl,
                                           Langue::Es, Langue::It, Langue::Tr};

// ---- outils des tests (écrits ici, indépendants du noyau) ---------------------------------------------------------------

// Décode UN caractère UTF-8 à la position `i` et avance. Rend 0 si le texte est mal formé.
constexpr char32_t decoder(std::string_view s, std::size_t &i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    std::size_t suite = 0;
    char32_t cp = 0;
    if (c < 0x80) {
        ++i;
        return c;
    } else if (c >= 0xC2 && c <= 0xDF) {
        suite = 1;
        cp = c & 0x1FU;
    } else if (c >= 0xE0 && c <= 0xEF) {
        suite = 2;
        cp = c & 0x0FU;
    } else if (c >= 0xF0 && c <= 0xF4) {
        suite = 3;
        cp = c & 0x07U;
    } else {
        return 0;
    }
    if (i + suite >= s.size())
        return 0; // il manque des octets de continuation
    for (std::size_t k = 1; k <= suite; ++k) {
        const unsigned char d = static_cast<unsigned char>(s[i + k]);
        if ((d & 0xC0U) != 0x80U)
            return 0;
        cp = (cp << 6) | (d & 0x3FU);
    }
    i += suite + 1;
    return cp;
}

// Le caractère d'une touche qui n'est pas une commande, ou 0 si la touche n'est pas UN caractère.
constexpr char32_t caractere_de(std::string_view t) {
    std::size_t i = 0;
    const char32_t cp = decoder(t, i);
    return (cp != 0 && i == t.size()) ? cp : 0;
}

struct Paire {
    char32_t min;
    char32_t maj;
};

// Majuscule attendue d'une minuscule (hors turc) : les paires accentuées, puis ASCII a-z.
constexpr Paire PAIRES[] = {
    {U'é', U'É'}, {U'è', U'È'}, {U'ê', U'Ê'}, {U'ë', U'Ë'}, {U'à', U'À'}, {U'â', U'Â'}, {U'ù', U'Ù'},
    {U'û', U'Û'}, {U'ç', U'Ç'}, {U'ô', U'Ô'}, {U'î', U'Î'}, {U'ï', U'Ï'}, {U'œ', U'Œ'}, {U'ü', U'Ü'},
    {U'ö', U'Ö'}, {U'ä', U'Ä'}, {U'ß', U'ẞ'}, {U'ñ', U'Ñ'}, {U'á', U'Á'}, {U'í', U'Í'}, {U'ó', U'Ó'},
    {U'ú', U'Ú'}, {U'ì', U'Ì'}, {U'ò', U'Ò'}, {U'ğ', U'Ğ'}, {U'ş', U'Ş'},
};

constexpr char32_t majuscule_attendue(char32_t c, Langue langue) {
    if (langue == Langue::Tr && c == U'i')
        return U'İ'; // i avec point : capitale avec point
    if (langue == Langue::Tr && c == U'ı')
        return U'I'; // i sans point : capitale sans point
    for (const Paire &p : PAIRES) {
        if (p.min == c)
            return p.maj;
    }
    if (c >= U'a' && c <= U'z')
        return c - U'a' + U'A';
    return c; // chiffres, signes : identiques
}

// Lettres que chaque langue doit pouvoir taper (minuscules ; les majuscules en découlent), au-delà de a-z.
constexpr std::string_view LETTRES_PROPRES[NOMBRE_LANGUES] = {
    "éèêëàâùûçôîïœ", // fr
    "",              // en
    "äöüß",          // de
    "éèêëïöüä",      // nl
    "áéíóúüñ",       // es
    "àèéìòù",        // it
    "çğıöşü",        // tr
};

constexpr bool contient(const Disposition &d, char32_t c) {
    for (std::size_t l = 0; l < d.nb; ++l) {
        const std::size_t n = compter_touches(d.lignes[l]);
        for (std::size_t k = 0; k < n; ++k) {
            if (caractere_de(touche(d.lignes[l], k)) == c)
                return true;
        }
    }
    return false;
}

constexpr bool contient_texte(Langue langue, Mode mode, std::string_view lettres) {
    const Disposition d = disposition(langue, mode);
    std::size_t i = 0;
    while (i < lettres.size()) {
        const char32_t c = decoder(lettres, i);
        if (c == 0 || !contient(d, c))
            return false;
    }
    return true;
}

constexpr std::size_t compter_controle(const Disposition &d, Controle c) {
    std::size_t n = 0;
    for (std::size_t l = 0; l < d.nb; ++l) {
        const std::size_t nb = compter_touches(d.lignes[l]);
        for (std::size_t k = 0; k < nb; ++k)
            n += controle(touche(d.lignes[l], k)) == c ? 1 : 0;
    }
    return n;
}

// ---- les tests ----------------------------------------------------------------------------------------------------------

constexpr int lecture_des_lignes_et_des_touches() {
    VERIFIER(compter_touches("") == 0);
    VERIFIER(compter_touches("a") == 1);
    VERIFIER(compter_touches("a z  e") == 3); // espaces multiples
    VERIFIER(compter_touches(" a z ") == 2);  // espaces autour
    VERIFIER(touche("a z e", 0) == "a");
    VERIFIER(touche("a z e", 2) == "e");
    VERIFIER(touche("a z e", 3).empty());
    VERIFIER(touche("", 0).empty());
    VERIFIER(touche("#maj é #eff", 1) == "é"); // un caractère multi-octet reste entier
    return 0;
}

constexpr int commandes_reconnues_et_liste_fermee() {
    VERIFIER(controle("#maj") == Controle::Maj);
    VERIFIER(controle("#sym") == Controle::Symboles);
    VERIFIER(controle("#abc") == Controle::Lettres);
    VERIFIER(controle("#eff") == Controle::Efface);
    VERIFIER(controle("#ok") == Controle::Valider);
    VERIFIER(controle("#esp") == Controle::Espace);
    // la liste fermée a exactement les six commandes, et chacune est reconnue
    VERIFIER(sizeof(ecran::clavier::CONTROLES) / sizeof(ecran::clavier::CONTROLES[0]) == 6);
    for (std::string_view c : ecran::clavier::CONTROLES)
        VERIFIER(controle(c) != Controle::Aucun);
    VERIFIER(controle("#") == Controle::Aucun); // le signe dièse est un caractère, pas une commande
    VERIFIER(controle("#zzz") == Controle::Aucun);
    VERIFIER(controle("a") == Controle::Aucun);
    VERIFIER(controle("") == Controle::Aucun);
    return 0;
}

constexpr int chaque_touche_est_un_caractere_ou_une_commande() {
    for (Langue langue : TOUTES) {
        for (Mode mode : {Mode::Minuscules, Mode::Majuscules, Mode::Symboles}) {
            const Disposition d = disposition(langue, mode);
            VERIFIER(d.nb >= 4 && d.nb <= ecran::clavier::LIGNES_MAX);
            for (std::size_t l = 0; l < d.nb; ++l) {
                const std::size_t n = compter_touches(d.lignes[l]);
                VERIFIER(n >= 3); // une ligne n'est jamais vide
                for (std::size_t k = 0; k < n; ++k) {
                    const std::string_view t = touche(d.lignes[l], k);
                    VERIFIER(!t.empty());
                    if (t.front() == '#' && t.size() > 1) {
                        VERIFIER(controle(t) != Controle::Aucun); // « #quelquechose » doit être une commande connue
                        bool dans_la_liste = false;
                        for (std::string_view c : ecran::clavier::CONTROLES)
                            dans_la_liste = dans_la_liste || c == t;
                        VERIFIER(dans_la_liste); // et figurer dans la liste fermée que l'interface lit
                    } else {
                        VERIFIER(caractere_de(t) != 0); // exactement un caractère Unicode, UTF-8 valide
                        VERIFIER(ecran::clavier::nombre_de_caracteres(t) == 1);
                    }
                }
            }
        }
    }
    return 0;
}

constexpr int le_decompte_des_caracteres_ne_compte_pas_les_octets() {
    using ecran::clavier::nombre_de_caracteres;
    VERIFIER(nombre_de_caracteres("") == 0 && nombre_de_caracteres("a") == 1 && nombre_de_caracteres("é") == 1);
    VERIFIER(nombre_de_caracteres("İ") == 1 && nombre_de_caracteres("ẞ") == 1 && nombre_de_caracteres("œ") == 1);
    VERIFIER(nombre_de_caracteres("aéẞ") == 3 && nombre_de_caracteres("😀") == 1);
    return 0;
}

constexpr int minuscules_et_majuscules_ont_la_meme_forme() {
    for (Langue langue : TOUTES) {
        const Disposition mn = disposition(langue, Mode::Minuscules);
        const Disposition mj = disposition(langue, Mode::Majuscules);
        VERIFIER(mn.nb == mj.nb);
        for (std::size_t l = 0; l < mn.nb; ++l) {
            const std::size_t n = compter_touches(mn.lignes[l]);
            VERIFIER(n == compter_touches(mj.lignes[l]));
            for (std::size_t k = 0; k < n; ++k) {
                const std::string_view a = touche(mn.lignes[l], k);
                const std::string_view b = touche(mj.lignes[l], k);
                VERIFIER(controle(a) == controle(b)); // les commandes sont aux mêmes places
                if (controle(a) != Controle::Aucun)
                    continue;
                // la majuscule de chaque touche est celle que la langue attend (chiffres et signes : identiques)
                VERIFIER(caractere_de(b) == majuscule_attendue(caractere_de(a), langue));
            }
        }
    }
    return 0;
}

constexpr int le_turc_distingue_i_pointe_et_i_sans_point() {
    const Disposition mn = disposition(Langue::Tr, Mode::Minuscules);
    const Disposition mj = disposition(Langue::Tr, Mode::Majuscules);
    VERIFIER(contient(mn, U'i') && contient(mn, U'ı'));
    VERIFIER(contient(mj, U'İ') && contient(mj, U'I'));
    // i et ı sont deux touches différentes, I et İ aussi
    VERIFIER(!contient(mn, U'İ') && !contient(mn, U'I'));
    VERIFIER(!contient(mj, U'i') && !contient(mj, U'ı'));
    // le turc a bien w, q, x du clavier Q
    VERIFIER(contient(mn, U'q') && contient(mn, U'w') && contient(mn, U'x'));
    return 0;
}

constexpr int les_autres_langues_ont_i_et_I_simples() {
    for (Langue langue : TOUTES) {
        if (langue == Langue::Tr)
            continue;
        VERIFIER(contient(disposition(langue, Mode::Minuscules), U'i'));
        VERIFIER(contient(disposition(langue, Mode::Majuscules), U'I'));
        VERIFIER(!contient(disposition(langue, Mode::Majuscules), U'İ'));
        VERIFIER(!contient(disposition(langue, Mode::Minuscules), U'ı'));
    }
    return 0;
}

constexpr int toutes_les_lettres_de_chaque_langue_sont_atteignables() {
    for (Langue langue : TOUTES) {
        // a à z en minuscule et en majuscule
        for (char32_t c = U'a'; c <= U'z'; ++c) {
            VERIFIER(contient(disposition(langue, Mode::Minuscules), c));
            VERIFIER(contient(disposition(langue, Mode::Majuscules), c - U'a' + U'A'));
        }
        // les lettres propres à la langue, minuscules puis majuscules
        const std::string_view propres = LETTRES_PROPRES[static_cast<int>(langue)];
        VERIFIER(contient_texte(langue, Mode::Minuscules, propres));
        std::size_t i = 0;
        while (i < propres.size()) {
            const char32_t c = decoder(propres, i);
            VERIFIER(contient(disposition(langue, Mode::Majuscules), majuscule_attendue(c, langue)));
        }
    }
    // En turc, w q x sont sur le clavier ; ses majuscules aussi (le test ci-dessus les saute à cause de I/İ : les voici)
    for (char32_t c = U'a'; c <= U'z'; ++c) {
        const char32_t m = majuscule_attendue(c, Langue::Tr);
        VERIFIER(contient(disposition(Langue::Tr, Mode::Majuscules), m));
    }
    return 0;
}

constexpr int une_seule_validation_un_seul_effacement_une_seule_espace_par_carte() {
    for (Langue langue : TOUTES) {
        for (Mode mode : {Mode::Minuscules, Mode::Majuscules, Mode::Symboles}) {
            const Disposition d = disposition(langue, mode);
            VERIFIER(compter_controle(d, Controle::Valider) == 1);
            VERIFIER(compter_controle(d, Controle::Efface) == 1);
            VERIFIER(compter_controle(d, Controle::Espace) == 1);
        }
    }
    return 0;
}

constexpr int chaque_carte_permet_de_changer_de_carte() {
    for (Langue langue : TOUTES) {
        const Disposition mn = disposition(langue, Mode::Minuscules);
        const Disposition mj = disposition(langue, Mode::Majuscules);
        const Disposition sy = disposition(langue, Mode::Symboles);
        VERIFIER(compter_controle(mn, Controle::Maj) == 1 && compter_controle(mj, Controle::Maj) == 1);
        VERIFIER(compter_controle(mn, Controle::Symboles) == 1 && compter_controle(mj, Controle::Symboles) == 1);
        VERIFIER(compter_controle(sy, Controle::Lettres) == 1);
        VERIFIER(compter_controle(sy, Controle::Maj) == 0); // pas de majuscule sur la carte des signes
        VERIFIER(compter_controle(mn, Controle::Lettres) == 0);
    }
    return 0;
}

constexpr int les_chiffres_sont_sur_la_premiere_ligne_des_lettres() {
    for (Langue langue : TOUTES) {
        for (Mode mode : {Mode::Minuscules, Mode::Majuscules, Mode::Symboles}) {
            const Disposition d = disposition(langue, mode);
            VERIFIER(d.lignes[0] == "1 2 3 4 5 6 7 8 9 0");
        }
    }
    return 0;
}

constexpr int les_signes_d_une_adresse_et_d_un_mot_de_passe_existent() {
    // de quoi écrire http://192.168.1.20:1234/v1, un nom de réseau, un mot de passe courant
    constexpr std::string_view necessaires = ":/.-_@?&=%#~+!$*()[]{}<>,;'\"`^|\\";
    VERIFIER(contient_texte(Langue::Fr, Mode::Symboles, ":/-_@?&=%#~+!$*()[]{}<>;'\"`^|\\"));
    for (Langue langue : TOUTES) {
        // le point et la virgule sont aussi sur la ligne du bas des lettres
        VERIFIER(contient(disposition(langue, Mode::Minuscules), U'.') &&
                 contient(disposition(langue, Mode::Minuscules), U','));
        VERIFIER(contient_texte(langue, Mode::Symboles, necessaires));
    }
    return 0;
}

constexpr int la_carte_des_signes_est_la_meme_partout() {
    const Disposition ref = disposition(Langue::Fr, Mode::Symboles);
    for (Langue langue : TOUTES) {
        const Disposition d = disposition(langue, Mode::Symboles);
        VERIFIER(d.nb == ref.nb);
        for (std::size_t l = 0; l < d.nb; ++l)
            VERIFIER(d.lignes[l] == ref.lignes[l]);
    }
    return 0;
}

constexpr int largeur_maximale_d_une_ligne_raisonnable() {
    // Une ligne trop longue donne des touches trop étroites pour un doigt (à 720 px de large en portrait). 13 est le maximum voulu.
    for (Langue langue : TOUTES) {
        for (Mode mode : {Mode::Minuscules, Mode::Majuscules, Mode::Symboles}) {
            const Disposition d = disposition(langue, mode);
            for (std::size_t l = 0; l < d.nb; ++l)
                VERIFIER(compter_touches(d.lignes[l]) <= 13);
        }
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(lecture_des_lignes_et_des_touches)                                                                               \
    X(commandes_reconnues_et_liste_fermee)                                                                             \
    X(chaque_touche_est_un_caractere_ou_une_commande)                                                                  \
    X(le_decompte_des_caracteres_ne_compte_pas_les_octets)                                                             \
    X(minuscules_et_majuscules_ont_la_meme_forme)                                                                      \
    X(le_turc_distingue_i_pointe_et_i_sans_point)                                                                      \
    X(les_autres_langues_ont_i_et_I_simples)                                                                           \
    X(toutes_les_lettres_de_chaque_langue_sont_atteignables)                                                           \
    X(une_seule_validation_un_seul_effacement_une_seule_espace_par_carte)                                              \
    X(chaque_carte_permet_de_changer_de_carte)                                                                         \
    X(les_chiffres_sont_sur_la_premiere_ligne_des_lettres)                                                             \
    X(les_signes_d_une_adresse_et_d_un_mot_de_passe_existent)                                                          \
    X(la_carte_des_signes_est_la_meme_partout)                                                                         \
    X(largeur_maximale_d_une_ligne_raisonnable)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
