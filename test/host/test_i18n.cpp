// Tests du noyau pur : traduction en sept langues (ecran/i18n.hpp et la table générée i18n_donnees.inc).
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
//
// Ce que ces tests prouvent sur le fichier tel qu'il est compilé : toute clé existe dans chaque langue (ou retombe sur
// le repli), les marqueurs %d %s %u et {noms} sont les mêmes et dans le même ordre, l'indice de chaque langue ne bouge
// pas, la table est triée (la dichotomie en dépend), le texte est de l'UTF-8 valide. Ce qu'ils NE prouvent PAS : que
// les traductions soient justes (écrites par une IA, relues par personne) ni qu'elles tiennent à l'écran (non vérifié).
#include <climits>
#include <cstdio>

#include "ecran/heure_fr.hpp"
#include "ecran/i18n.hpp"
#include "ecran/meteo_wmo.hpp"
#include "mini.hpp"

using ecran::i18n::Entree;
using ecran::i18n::est_une_cle;
using ecran::i18n::indice;
using ecran::i18n::Langue;
using ecran::i18n::langue_depuis_code;
using ecran::i18n::langue_depuis_indice;
using ecran::i18n::NOMBRE_CLES;
using ecran::i18n::NOMBRE_LANGUES;
using ecran::i18n::propre;
using ecran::i18n::propre_dans;
using ecran::i18n::TABLE;
using ecran::i18n::texte_francais;
using ecran::i18n::tr;
using ecran::i18n::tr_dans;

namespace {

constexpr Langue TOUTES[NOMBRE_LANGUES] = {Langue::Fr, Langue::En, Langue::De, Langue::Nl,
                                           Langue::Es, Langue::It, Langue::Tr};

// ---- outils des tests (écrits ici, indépendants du noyau)
// ----------------------------------------------------------------

// Prochain marqueur de `s` à partir de `pos` : %d %s %u %02d %lu… ou {nom}. « %% » et « % » seul n'en sont pas.
struct Marqueur {
    std::size_t debut;
    std::size_t longueur; // 0 = plus aucun marqueur
};

constexpr Marqueur marqueur_suivant(std::string_view s, std::size_t pos) {
    for (std::size_t i = pos; i < s.size(); ++i) {
        if (s[i] == '%') {
            if (i + 1 < s.size() && s[i + 1] == '%') {
                ++i;
                continue;
            }
            std::size_t j = i + 1;
            while (j < s.size() &&
                   (s[j] == '-' || s[j] == '+' || s[j] == '#' || s[j] == '.' || (s[j] >= '0' && s[j] <= '9')))
                ++j;
            while (j < s.size() && (s[j] == 'h' || s[j] == 'l' || s[j] == 'z'))
                ++j;
            if (j < s.size() && (s[j] == 'd' || s[j] == 'i' || s[j] == 'u' || s[j] == 'x' || s[j] == 'X' ||
                                 s[j] == 's' || s[j] == 'c')) {
                return {i, j + 1 - i};
            }
        } else if (s[i] == '{') {
            std::size_t j = i + 1;
            while (j < s.size() && ((s[j] >= 'a' && s[j] <= 'z') || s[j] == '_' || (s[j] >= '0' && s[j] <= '9')))
                ++j;
            if (j > i + 1 && j < s.size() && s[j] == '}' && !(s[i + 1] >= '0' && s[i + 1] <= '9'))
                return {i, j + 1 - i};
        }
    }
    return {0, 0};
}

// Nombre de fois où le marqueur `m` (« {nom} ») apparaît dans `s`.
constexpr std::size_t occurrences(std::string_view s, std::string_view m) {
    std::size_t n = 0;
    std::size_t pos = 0;
    for (;;) {
        const Marqueur x = marqueur_suivant(s, pos);
        if (x.longueur == 0)
            return n;
        if (s.substr(x.debut, x.longueur) == m)
            ++n;
        pos = x.debut + x.longueur;
    }
}

// Les « %d %s… » (lus par printf dans l'ordre) : même liste, même ordre. Les {noms} (remplacés par leur nom) : les mêmes, dans
// l'ordre que la langue veut (« {sur} üzerinden {points} puan » en turc).
constexpr bool memes_marqueurs(std::string_view a, std::string_view b) {
    std::size_t pa = 0;
    std::size_t pb = 0;
    for (;;) {
        Marqueur ma = marqueur_suivant(a, pa);
        while (ma.longueur != 0 && a[ma.debut] == '{')
            ma = marqueur_suivant(a, ma.debut + ma.longueur);
        Marqueur mb = marqueur_suivant(b, pb);
        while (mb.longueur != 0 && b[mb.debut] == '{')
            mb = marqueur_suivant(b, mb.debut + mb.longueur);
        if (ma.longueur == 0 || mb.longueur == 0) {
            if (ma.longueur != mb.longueur)
                return false;
            break;
        }
        if (a.substr(ma.debut, ma.longueur) != b.substr(mb.debut, mb.longueur))
            return false;
        pa = ma.debut + ma.longueur;
        pb = mb.debut + mb.longueur;
    }
    for (const std::string_view s : {a, b}) {
        std::size_t pos = 0;
        for (Marqueur m = marqueur_suivant(s, pos); m.longueur != 0; m = marqueur_suivant(s, m.debut + m.longueur)) {
            const std::string_view nom = s.substr(m.debut, m.longueur);
            if (nom[0] == '{' && occurrences(a, nom) != occurrences(b, nom))
                return false;
        }
    }
    return true;
}

constexpr std::size_t nombre_de_marqueurs(std::string_view s) {
    std::size_t n = 0;
    std::size_t pos = 0;
    for (;;) {
        const Marqueur m = marqueur_suivant(s, pos);
        if (m.longueur == 0)
            return n;
        ++n;
        pos = m.debut + m.longueur;
    }
}

// UTF-8 bien formé (pas d'octet isolé, pas de forme trop longue, pas de substitut, pas au-delà de U+10FFFF), sans
// caractère de contrôle ASCII.
constexpr bool utf8_valide(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned c = static_cast<unsigned char>(s[i]);
        if (c < 0x20 || c == 0x7F)
            return false;
        std::size_t suite = 0;
        unsigned cp = 0;
        if (c < 0x80) {
            ++i;
            continue;
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
            return false;
        }
        if (i + suite >= s.size() + 0 && i + suite > s.size() - 1)
            return false;
        for (std::size_t k = 1; k <= suite; ++k) {
            const unsigned d = static_cast<unsigned char>(s[i + k]);
            if ((d & 0xC0U) != 0x80U)
                return false;
            cp = (cp << 6) | (d & 0x3FU);
        }
        if (suite == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)))
            return false;
        if (suite == 3 && (cp < 0x10000 || cp > 0x10FFFF))
            return false;
        i += suite + 1;
    }
    return true;
}

// Nombre de caractères (et non d'octets) d'un texte UTF-8.
constexpr std::size_t caracteres(std::string_view s) {
    std::size_t n = 0;
    for (char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U)
            ++n;
    }
    return n;
}

// ---- l'indice de langue ne bouge pas
// --------------------------------------------------------------------------------------

constexpr int ordre_et_indices_des_langues_figes() {
    // Même ordre que le sélecteur « Langue » du Tab5 (Tab5/lang/*.yaml, _index), enregistré en NVS : ne JAMAIS changer.
    VERIFIER(NOMBRE_LANGUES == 7);
    VERIFIER(indice(Langue::Fr) == 0);
    VERIFIER(indice(Langue::En) == 1);
    VERIFIER(indice(Langue::De) == 2);
    VERIFIER(indice(Langue::Nl) == 3);
    VERIFIER(indice(Langue::Es) == 4);
    VERIFIER(indice(Langue::It) == 5);
    VERIFIER(indice(Langue::Tr) == 6);
    VERIFIER(ecran::i18n::code(Langue::Fr) == "fr");
    VERIFIER(ecran::i18n::code(Langue::En) == "en");
    VERIFIER(ecran::i18n::code(Langue::De) == "de");
    VERIFIER(ecran::i18n::code(Langue::Nl) == "nl");
    VERIFIER(ecran::i18n::code(Langue::Es) == "es");
    VERIFIER(ecran::i18n::code(Langue::It) == "it");
    VERIFIER(ecran::i18n::code(Langue::Tr) == "tr");
    for (std::size_t i = 0; i < NOMBRE_LANGUES; ++i) {
        VERIFIER(indice(TOUTES[i]) == static_cast<int>(i));
        VERIFIER(ecran::i18n::CODES[i] == ecran::i18n::code(TOUTES[i]));
    }
    return 0;
}

constexpr int noms_des_langues_dans_leur_propre_langue() {
    VERIFIER(ecran::i18n::nom(Langue::Fr) == "Français");
    VERIFIER(ecran::i18n::nom(Langue::En) == "English");
    VERIFIER(ecran::i18n::nom(Langue::De) == "Deutsch");
    VERIFIER(ecran::i18n::nom(Langue::Nl) == "Nederlands");
    VERIFIER(ecran::i18n::nom(Langue::Es) == "Español");
    VERIFIER(ecran::i18n::nom(Langue::It) == "Italiano");
    VERIFIER(ecran::i18n::nom(Langue::Tr) == "Türkçe");
    return 0;
}

constexpr int indice_lu_en_nvs_hors_limites_donne_le_defaut() {
    for (int i = 0; i < 7; ++i)
        VERIFIER(indice(langue_depuis_indice(i)) == i);
    for (int hors : {-1, 7, 8, 255, 256, -2147483647, INT_MAX, INT_MIN}) {
        VERIFIER(langue_depuis_indice(hors) == Langue::Fr);
        VERIFIER(langue_depuis_indice(hors, Langue::En) == Langue::En);
    }
    return 0;
}

constexpr int codes_de_langue_reconnus_ou_refuses() {
    for (Langue l : TOUTES) {
        const auto lue = langue_depuis_code(ecran::i18n::code(l));
        VERIFIER(lue.has_value() && *lue == l);
    }
    for (std::string_view faux : {"", "DE", "De", "deu", "d", "fr ", " fr", "xx", "en-GB"}) {
        VERIFIER(!langue_depuis_code(faux).has_value());
    }
    VERIFIER(!langue_depuis_code(std::string_view("tr\0", 3)).has_value()); // un zéro après le code n'est pas le code
    return 0;
}

// ---- la table
// -----------------------------------------------------------------------------------------------------------

constexpr int table_triee_par_octets_sans_doublon() {
    VERIFIER(NOMBRE_CLES >= 280);
    for (std::size_t i = 1; i < NOMBRE_CLES; ++i)
        VERIFIER(TABLE[i - 1].cle < TABLE[i].cle);
    return 0;
}

constexpr int chaque_cle_se_retrouve_par_la_dichotomie() {
    for (std::size_t i = 0; i < NOMBRE_CLES; ++i) {
        VERIFIER(est_une_cle(TABLE[i].cle));
        const Entree *e = ecran::i18n::detail::chercher(TABLE, NOMBRE_CLES, TABLE[i].cle);
        VERIFIER(e == &TABLE[i]);
    }
    return 0;
}

constexpr int cles_et_traductions_en_utf8_valide() {
    for (std::size_t i = 0; i < NOMBRE_CLES; ++i) {
        VERIFIER(!TABLE[i].cle.empty());
        VERIFIER(utf8_valide(TABLE[i].cle));
        for (const std::string_view t : TABLE[i].t)
            VERIFIER(utf8_valide(t));
    }
    return 0;
}

constexpr int toute_cle_existe_dans_chaque_langue() {
    for (std::size_t i = 0; i < NOMBRE_CLES; ++i) {
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l) {
            VERIFIER(!propre(TABLE[i].cle, TOUTES[l]).empty()); // traduction propre, pas un repli
            VERIFIER(tr(TABLE[i].cle, TOUTES[l]) == propre(TABLE[i].cle, TOUTES[l]));
        }
        VERIFIER(propre(TABLE[i].cle, Langue::Fr).empty()); // le français est la clé, il n'a pas de case
        VERIFIER(tr(TABLE[i].cle, Langue::Fr) == texte_francais(TABLE[i].cle)); // sans son contexte
    }
    return 0;
}

constexpr int marqueurs_identiques_et_dans_le_meme_ordre() {
    for (std::size_t i = 0; i < NOMBRE_CLES; ++i) {
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l) {
            VERIFIER(memes_marqueurs(TABLE[i].cle, propre(TABLE[i].cle, TOUTES[l])));
        }
    }
    return 0;
}

constexpr int le_comparateur_de_marqueurs_voit_les_differences() {
    VERIFIER(memes_marqueurs("Carte : %s, %d × %d", "Board: %s, %d × %d"));
    VERIFIER(!memes_marqueurs("Carte : %s, %d × %d", "Board: %d, %s × %d")); // ordre
    VERIFIER(!memes_marqueurs("Carte : %s, %d × %d", "Board: %s, %d"));      // un marqueur de moins
    VERIFIER(!memes_marqueurs("Carte : %s", "Board: %s, %d"));               // un marqueur de plus
    VERIFIER(!memes_marqueurs("Démarrage n° %u", "Boot no. %d"));            // autre conversion
    VERIFIER(memes_marqueurs("%02d", "%02d"));
    VERIFIER(!memes_marqueurs("%02d", "%d"));
    VERIFIER(memes_marqueurs("{nom} · {ssid}", "{nom} - {ssid}"));
    VERIFIER(memes_marqueurs("{nom} · {ssid}", "{ssid} · {nom}"));   // les noms changent d'ordre : permis
    VERIFIER(!memes_marqueurs("{nom} · {ssid}", "{ssid} · {ssid}")); // un nom remplacé par un autre déjà là
    VERIFIER(!memes_marqueurs("{nom} · {nom}", "{nom}"));            // un nom de moins
    VERIFIER(!memes_marqueurs("%s {nom} %d", "%d {nom} %s"));        // les « % » gardent leur ordre
    VERIFIER(!memes_marqueurs("{nom}", "{ville}"));                  // autre nom
    VERIFIER(!memes_marqueurs("{nom}", "nom"));                      // accolades perdues
    VERIFIER(memes_marqueurs("100 %", "100%"));                      // « % » seul n'est pas un marqueur
    VERIFIER(memes_marqueurs("100 %% sûr", "100 %% safe"));          // « %% » non plus
    VERIFIER(memes_marqueurs("Pas de marqueur", "Kein Marker"));
    VERIFIER(nombre_de_marqueurs("Carte : %s, %d × %d") == 3);
    VERIFIER(nombre_de_marqueurs("{h}:{m}") == 2);
    VERIFIER(nombre_de_marqueurs("5 et 100 %.") == 0);
    return 0;
}

// ---- tr() : traduction et repli
// -------------------------------------------------------------------------------------------

constexpr int vocabulaire_du_tab5_repris_tel_quel() {
    // Noms des jours et des mois et « Redémarrer » viennent de Tab5/lang/*.yaml (lu le 06/11/2026) : mêmes mots. Les formes
    // courtes du Tab5 (« Dim », « Janv »…) ont été retirées le 11/11/2026 : l'écran ne s'en servait pas.
    struct Attendu {
        std::string_view cle;
        std::string_view t[6]; // en, de, nl, es, it, tr
    };
    constexpr Attendu ATTENDUS[] = {
        {"dimanche", {"Sunday", "Sonntag", "zondag", "domingo", "domenica", "Pazar"}},
        {"août", {"August", "August", "augustus", "agosto", "agosto", "Ağustos"}},
        {"Redémarrer", {"Restart", "Neu starten", "Herstarten", "Reiniciar", "Riavvia", "Yeniden başlat"}},
        {"Orage", {"Thunderstorm", "Gewitter", "Onweer", "Tormenta", "Temporale", "Fırtına"}},
    };
    for (const Attendu &a : ATTENDUS) {
        VERIFIER(tr(a.cle, Langue::Fr) == a.cle);
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l)
            VERIFIER(tr(a.cle, TOUTES[l]) == a.t[l - 1]);
    }
    return 0;
}

constexpr int repli_anglais_puis_francais_puis_la_cle() {
    // Table fabriquée pour le test (triée par octets) : les vraies langues sont complètes, le repli ne s'y voit donc
    // pas.
    constexpr Entree FAUSSE[] = {
        {"a", {"A-en", "A-de", "", "", "", ""}},
        {"b", {"", "B-de", "", "", "", ""}},
        {"c", {"", "", "", "", "", ""}},
        {"d", {"D-en", "", "", "", "", "D-tr"}},
    };
    constexpr std::size_t N = 4;
    VERIFIER(tr_dans(FAUSSE, N, "a", Langue::De) == "A-de"); // traduction propre
    VERIFIER(tr_dans(FAUSSE, N, "a", Langue::Nl) == "A-en"); // absente -> anglais
    VERIFIER(tr_dans(FAUSSE, N, "a", Langue::En) == "A-en");
    VERIFIER(tr_dans(FAUSSE, N, "b", Langue::De) == "B-de");
    VERIFIER(tr_dans(FAUSSE, N, "b", Langue::Es) == "b"); // ni la langue ni l'anglais -> le français (la clé)
    VERIFIER(tr_dans(FAUSSE, N, "b", Langue::En) == "b");
    VERIFIER(tr_dans(FAUSSE, N, "c", Langue::Tr) == "c");
    VERIFIER(tr_dans(FAUSSE, N, "d", Langue::De) == "D-en");
    VERIFIER(tr_dans(FAUSSE, N, "d", Langue::Tr) == "D-tr");
    VERIFIER(tr_dans(FAUSSE, N, "a", Langue::Fr) == "a");     // le français est la clé
    VERIFIER(tr_dans(FAUSSE, N, "zzz", Langue::De) == "zzz"); // clé inconnue : rendue telle quelle
    VERIFIER(tr_dans(FAUSSE, N, "", Langue::De).empty());
    VERIFIER(tr_dans(FAUSSE, 0, "a", Langue::De) == "a"); // table vide
    VERIFIER(tr_dans(FAUSSE, 1, "d", Langue::Tr) == "d"); // la dichotomie ne sort pas des n premières lignes
    // sans repli : seule la traduction propre
    VERIFIER(propre_dans(FAUSSE, N, "a", Langue::Nl).empty());
    VERIFIER(propre_dans(FAUSSE, N, "a", Langue::De) == "A-de");
    VERIFIER(propre_dans(FAUSSE, N, "a", Langue::Fr).empty());
    VERIFIER(propre_dans(FAUSSE, N, "zzz", Langue::De).empty());
    // chaque ligne de 1 à 4 se retrouve (premier, dernier, milieux)
    for (std::size_t n = 1; n <= N; ++n) {
        for (std::size_t i = 0; i < n; ++i)
            VERIFIER(ecran::i18n::detail::chercher(FAUSSE, n, FAUSSE[i].cle) == &FAUSSE[i]);
    }
    return 0;
}

constexpr int cle_inconnue_rendue_telle_quelle() {
    for (Langue l : TOUTES) {
        VERIFIER(tr("Ce texte n'est dans aucune table", l) == "Ce texte n'est dans aucune table");
        VERIFIER(tr("", l).empty());
        VERIFIER(tr("Dimanche", l) == "Dimanche");   // la casse compte : « dimanche » est une clé, pas « Dimanche »
        VERIFIER(tr("dimanche ", l) == "dimanche "); // ni une espace de plus
    }
    VERIFIER(est_une_cle("dimanche"));
    VERIFIER(!est_une_cle("Dimanche"));
    VERIFIER(!est_une_cle("dimanche "));
    VERIFIER(!est_une_cle(""));
    return 0;
}

// ---- le vocabulaire dont le produit a besoin aujourd'hui
// ------------------------------------------------------------------

constexpr int libelles_meteo_traduits_dans_chaque_langue() {
    constexpr int CODES_OPEN_METEO[] = {0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66,
                                        67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 97, 99};
    for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l) {
        for (std::size_t i = 0; i <= 29; ++i) {
            const int code = i < 29 ? CODES_OPEN_METEO[i] : -1; // -1 : « Météo inconnue »
            const std::string_view fr = ecran::meteo::decrire(code, true).libelle;
            VERIFIER(est_une_cle(fr));
            const std::string_view t = propre(fr, TOUTES[l]);
            VERIFIER(!t.empty());
            VERIFIER(caracteres(t) <= 34); // budget de conception ; la place réelle à l'écran n'est PAS vérifiée
            for (char c : t)
                VERIFIER(!(c >= '0' && c <= '9'));
            for (std::size_t j = 0; j < i; ++j) { // deux conditions différentes ne se lisent pas pareil
                const int autre = CODES_OPEN_METEO[j];
                VERIFIER(t != propre(ecran::meteo::decrire(autre, true).libelle, TOUTES[l]));
            }
        }
    }
    return 0;
}

constexpr int jours_et_mois_de_heure_fr_traduits() {
    for (int j = 0; j <= 6; ++j) {
        VERIFIER(est_une_cle(ecran::heure::nom_jour(j)));
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l)
            VERIFIER(!propre(ecran::heure::nom_jour(j), TOUTES[l]).empty());
    }
    for (int m = 1; m <= 12; ++m) {
        VERIFIER(est_une_cle(ecran::heure::nom_mois(m)));
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l)
            VERIFIER(!propre(ecran::heure::nom_mois(m), TOUTES[l]).empty());
    }
    // Les formes courtes du Tab5 (« Dim »… « Déc ») ne sont plus des clés : aucun écran ne les lisait, et « Mer » (mercredi)
    // prenait la place de la page « Mer ».
    constexpr std::string_view MORTES[] = {"Dim", "Lun",  "Jeu",  "Ven",  "Sam",  "Janv", "Févr", "Avr",
                                           "Mai", "Juin", "Juil", "Août", "Sept", "Oct",  "Nov",  "Déc"};
    for (std::string_view c : MORTES)
        VERIFIER(!est_une_cle(c));
    return 0;
}

constexpr int une_cle_a_contexte_garde_un_seul_sens() {
    // Le même texte français, deux sens : la page « Mer » (la mer) et « mer » (mercredi, bande de la semaine).
    VERIFIER(tr("Mer", Langue::En) == "Sea");
    VERIFIER(tr("jour court|mer", Langue::En) == "Wed");
    VERIFIER(tr("jour court|mer", Langue::Tr) == "Çar");
    VERIFIER(!est_une_cle("mer")); // sans contexte, « mer » n'a pas de sens
    // En français, le contexte ne s'affiche jamais.
    VERIFIER(tr("jour court|mer", Langue::Fr) == "mer");
    VERIFIER(tr("vent|faible", Langue::Fr) == "faible");
    // Trois sens de « faible » sur la page mer : un vent léger, une mer basse, un petit coefficient.
    VERIFIER(tr("vent|faible", Langue::En) == "light");
    VERIFIER(tr("mer|faible", Langue::En) == "low");
    VERIFIER(tr("coefficient|faible", Langue::En) == "small");
    // Une clé à contexte inconnue rend son texte, pas le contexte.
    VERIFIER(tr("inconnu|texte", Langue::De) == "texte");
    VERIFIER(ecran::i18n::texte_francais("a|b|c") == "b|c");
    VERIFIER(ecran::i18n::texte_francais("sans contexte") == "sans contexte");
    // Chaque clé de la table a un texte français non vide, qui finit là où finit la clé (donc terminé par un zéro).
    for (std::size_t i = 0; i < NOMBRE_CLES; ++i) {
        const std::string_view f = ecran::i18n::texte_francais(TABLE[i].cle);
        VERIFIER(!f.empty());
        VERIFIER(f.data() + f.size() == TABLE[i].cle.data() + TABLE[i].cle.size());
    }
    return 0;
}

constexpr int textes_de_l_ecran_bonjour_traduits() {
    // Copie des textes de main/ui_bonjour.cpp (espace de noms `texte`, l'écran de diagnostic) et de la table des causes de
    // redémarrage de main/boot_info.cpp, au 11/11/2026. test/host/controle_i18n.py vérifie que ces fichiers n'ont pas changé sans la
    // table.
    constexpr std::string_view BONJOUR[] = {
        "Bonjour",
        "Écran autonome",
        "Carte : %s, %d × %d",
        "Contrôleur d'écran : %s",
        "Tactile : %s",
        "Démarrage n° %u",
        "Démarrage n° %u (compteur non enregistré)",
        "Dernier redémarrage : %s",
        "Carte prête après %d ms",
        "Toucher : aucun",
        "Toucher : x = %d, y = %d",
        "Redémarrer",
        "-- h --",
        "Heure du réseau en attente",
        "Wi-Fi : réglages absents du firmware",
        "Wi-Fi : coprocesseur absent",
        "Wi-Fi : coprocesseur en démarrage",
        "Wi-Fi : connexion en cours",
        "Wi-Fi : connecté, adresse en cours",
        "Wi-Fi : connecté",
        "Heure : en attente du réseau",
        "Heure : reçue du réseau",
        "mise sous tension",
        "redémarrage logiciel",
        "plantage (exception)",
        "chien de garde",
        "chute de tension",
        "sortie de veille profonde",
        "réinitialisation par l'USB",
        "broche de réinitialisation",
        "cause inconnue",
    };
    for (std::string_view fr : BONJOUR) {
        VERIFIER(est_une_cle(fr));
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l)
            VERIFIER(!propre(fr, TOUTES[l]).empty());
    }
    VERIFIER(nombre_de_marqueurs("Carte : %s, %d × %d") == 3);
    VERIFIER(nombre_de_marqueurs("Toucher : x = %d, y = %d") == 2);
    return 0;
}

// Les textes de la page et de l'API suivent la même règle : un échantillon sur lequel le contrat s'appuie.
constexpr int messages_de_l_api_traduits_avec_leurs_marqueurs() {
    constexpr std::string_view MESSAGES[] = {
        "Ce code n’est pas le bon. Regardez l’écran de l’appareil.",
        "Trop d’essais. Réessayez dans {n} secondes.",
        "Mot de passe refusé par « {ssid} ». L’ancien réseau est conservé.",
        "Le service répond en {s} s.",
        "Retour à la version {version} réussi.",
        "{h} h {m}",
        "Données météo : {lien} (licence CC BY 4.0)",
    };
    for (std::string_view fr : MESSAGES) {
        VERIFIER(est_une_cle(fr));
        for (std::size_t l = 1; l < NOMBRE_LANGUES; ++l) {
            VERIFIER(!propre(fr, TOUTES[l]).empty());
            VERIFIER(memes_marqueurs(fr, propre(fr, TOUTES[l])));
        }
    }
    VERIFIER(propre("Données météo : {lien} (licence CC BY 4.0)", Langue::En).find("CC BY 4.0") !=
             std::string_view::npos);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(ordre_et_indices_des_langues_figes)                                                                              \
    X(noms_des_langues_dans_leur_propre_langue)                                                                        \
    X(indice_lu_en_nvs_hors_limites_donne_le_defaut)                                                                   \
    X(codes_de_langue_reconnus_ou_refuses)                                                                             \
    X(table_triee_par_octets_sans_doublon)                                                                             \
    X(chaque_cle_se_retrouve_par_la_dichotomie)                                                                        \
    X(cles_et_traductions_en_utf8_valide)                                                                              \
    X(toute_cle_existe_dans_chaque_langue)                                                                             \
    X(marqueurs_identiques_et_dans_le_meme_ordre)                                                                      \
    X(le_comparateur_de_marqueurs_voit_les_differences)                                                                \
    X(vocabulaire_du_tab5_repris_tel_quel)                                                                             \
    X(repli_anglais_puis_francais_puis_la_cle)                                                                         \
    X(cle_inconnue_rendue_telle_quelle)                                                                                \
    X(libelles_meteo_traduits_dans_chaque_langue)                                                                      \
    X(jours_et_mois_de_heure_fr_traduits)                                                                              \
    X(une_cle_a_contexte_garde_un_seul_sens)                                                                           \
    X(textes_de_l_ecran_bonjour_traduits)                                                                              \
    X(messages_de_l_api_traduits_avec_leurs_marqueurs)

TESTS_A_LA_COMPILATION(LISTE)

// La langue courante est un std::atomic : pas évaluable à la compilation, donc vérifiée ici, à l'exécution seulement.
static int langue_courante_a_l_execution() {
    using namespace ecran::i18n;
    if (langue_courante() != Langue::Fr)
        return __LINE__; // français tant que rien n'est réglé
    if (tr("dimanche") != "dimanche")
        return __LINE__;
    definir_langue(Langue::De);
    if (langue_courante() != Langue::De)
        return __LINE__;
    if (tr("dimanche") != "Sonntag")
        return __LINE__;
    if (std::string_view(tr_c("dimanche")) != "Sonntag")
        return __LINE__; // terminé par un zéro
    definir_langue(Langue::Tr);
    if (tr("dimanche") != "Pazar")
        return __LINE__;
    if (std::string_view(tr_c("texte sans traduction")) != "texte sans traduction")
        return __LINE__;
    definir_langue(Langue::Fr);
    if (tr("dimanche") != "dimanche")
        return __LINE__;
    return 0;
}

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    int echecs = lancer_tous(cas);
    const int ligne = langue_courante_a_l_execution();
    if (ligne == 0) {
        std::printf("  ok      langue_courante_a_l_execution\n");
    } else {
        std::printf("  ECHEC   langue_courante_a_l_execution (verification a la ligne %d)\n", ligne);
        echecs = 1;
    }
    return echecs;
}
