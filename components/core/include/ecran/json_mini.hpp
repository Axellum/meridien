// Noyau pur : lecteur JSON minimal, en lecture seule, sur std::string_view. Brique commune (météo, assistant).
//
// API EN TROIS LIGNES (courte et stable : n'y ajouter que des fonctions, ne rien renommer) :
//   1. const json::Resultat r = json::lire(texte);   // valide TOUT le texte une fois ; r.ok(), r.erreur, r.position
//   2. const json::Valeur v = r.valeur.trouver("daily.sunrise[2]");   // chemin « a.b[3].c » ; v.existe() == false si absent
//   3. v.en_entier(n) / v.en_fixe(1, dixiemes) / v.en_double(d) / v.en_booleen(b) / v.texte_egal("x") / v.copier_texte(buf, n)
//      rendent un bool (faux = mauvais type ou hors plage), jamais d'exception ; v.champ(cle), v.element(i), v.taille()
//      pour parcourir.
//
// Garanties : aucune allocation dynamique, aucune exception, aucun include ESP-IDF, évaluable à la compilation
// (les tests de test/host/ s'en servent). Une `Valeur` est une VUE sur le texte source : le texte doit survivre.
// Rien n'est copié ; la navigation rescanne le texte (O(n) par accès : prévu pour des réponses de quelques Ko).
//
// Ce que `lire` accepte : la grammaire JSON de la RFC 8259 (objets, tableaux, chaînes, nombres, true/false/null,
// blancs espace/tab/LF/CR). Profondeur limitée à PROFONDEUR_MAX (au-delà : Erreur::TropProfond). Choix assumés :
//   - clé en double dans un objet : la PREMIÈRE gagne ;
//   - les octets >= 0x80 des chaînes sont passés tels quels (UTF-8 non vérifié) ;
//   - \uD800..\uDFFF isolé ou \u0000 sont rendus U+FFFD (jamais un octet 0 dans une chaîne C) ;
//   - nombres : au-delà de 19 chiffres significatifs, les suivants sont ignorés (troncature) ;
//   - `en_double` est exact (identique à strtod) jusqu'à 15 chiffres significatifs et |exposant| <= 22, sinon
//     approché. `en_fixe` est exact sur le texte décimal (pas de passage par un flottant).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::json {

inline constexpr int PROFONDEUR_MAX = 24; // niveaux d'objets/tableaux imbriqués acceptés

enum class Genre : std::uint8_t { Absent, Objet, Tableau, Chaine, Nombre, Booleen, Nul };

enum class Erreur : std::uint8_t {
    Aucune,
    Vide,          // texte vide ou seulement des blancs
    FinInattendue, // texte coupé (objet, tableau ou chaîne non fermé)
    Syntaxe,       // caractère inattendu
    TropProfond,   // plus de PROFONDEUR_MAX niveaux
    TexteEnTrop,   // du texte après la valeur
};

constexpr std::string_view message(Erreur e) noexcept {
    switch (e) {
    case Erreur::Aucune:
        return "aucune erreur";
    case Erreur::Vide:
        return "texte vide";
    case Erreur::FinInattendue:
        return "texte coupé";
    case Erreur::Syntaxe:
        return "syntaxe invalide";
    case Erreur::TropProfond:
        return "trop d'imbrications";
    case Erreur::TexteEnTrop:
        return "texte après la valeur";
    }
    return "erreur inconnue";
}

namespace detail {

constexpr bool est_blanc(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
constexpr bool est_chiffre(char c) noexcept { return c >= '0' && c <= '9'; }
constexpr bool est_separateur(char c) noexcept { return c == ',' || c == ']' || c == '}' || est_blanc(c); }

constexpr std::size_t sauter_blancs(std::string_view t, std::size_t p) noexcept {
    while (p < t.size() && est_blanc(t[p]))
        ++p;
    return p;
}

// `p` pointe le guillemet ouvrant ; rend l'index qui suit le guillemet fermant (t.size() si absent).
constexpr std::size_t fin_chaine(std::string_view t, std::size_t p) noexcept {
    ++p;
    while (p < t.size()) {
        if (t[p] == '\\') {
            p += 2;
        } else if (t[p] == '"') {
            return p + 1;
        } else {
            ++p;
        }
    }
    return t.size();
}

// `p` pointe le premier caractère d'une valeur ; rend l'index qui suit la valeur.
constexpr std::size_t fin_valeur(std::string_view t, std::size_t p) noexcept {
    if (p >= t.size())
        return t.size();
    const char c = t[p];
    if (c == '"')
        return fin_chaine(t, p);
    if (c == '{' || c == '[') {
        int prof = 0;
        while (p < t.size()) {
            const char d = t[p];
            if (d == '"') {
                p = fin_chaine(t, p);
                continue;
            }
            if (d == '{' || d == '[') {
                ++prof;
            } else if (d == '}' || d == ']') {
                if (--prof == 0)
                    return p + 1;
            }
            ++p;
        }
        return t.size();
    }
    while (p < t.size() && !est_separateur(t[p]))
        ++p;
    return p;
}

constexpr Genre genre_de(char c) noexcept {
    switch (c) {
    case '{':
        return Genre::Objet;
    case '[':
        return Genre::Tableau;
    case '"':
        return Genre::Chaine;
    case 't':
    case 'f':
        return Genre::Booleen;
    case 'n':
        return Genre::Nul;
    default:
        return (c == '-' || est_chiffre(c)) ? Genre::Nombre : Genre::Absent;
    }
}

// ---- validation (grammaire RFC 8259) -------------------------------------------------------------------------

constexpr int valeur_hex(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// `p` pointe le guillemet ouvrant ; en cas de succès `p` suit le guillemet fermant.
constexpr Erreur valider_chaine(std::string_view t, std::size_t &p) noexcept {
    ++p;
    while (p < t.size()) {
        const unsigned char c = static_cast<unsigned char>(t[p]);
        if (c == '"') {
            ++p;
            return Erreur::Aucune;
        }
        if (c < 0x20)
            return Erreur::Syntaxe; // caractère de contrôle brut interdit
        if (c == '\\') {
            if (p + 1 >= t.size())
                return Erreur::FinInattendue;
            const char e = t[p + 1];
            if (e == 'u') {
                if (p + 5 >= t.size())
                    return Erreur::FinInattendue;
                for (std::size_t i = 2; i <= 5; ++i) {
                    if (valeur_hex(t[p + i]) < 0)
                        return Erreur::Syntaxe;
                }
                p += 6;
                continue;
            }
            if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f' && e != 'n' && e != 'r' && e != 't') {
                return Erreur::Syntaxe;
            }
            p += 2;
            continue;
        }
        ++p;
    }
    return Erreur::FinInattendue;
}

constexpr Erreur valider_nombre(std::string_view t, std::size_t &p) noexcept {
    if (p < t.size() && t[p] == '-')
        ++p;
    if (p >= t.size())
        return Erreur::FinInattendue;
    if (t[p] == '0') {
        ++p; // un zéro initial ne peut pas être suivi d'un chiffre
    } else if (t[p] >= '1' && t[p] <= '9') {
        while (p < t.size() && est_chiffre(t[p]))
            ++p;
    } else {
        return Erreur::Syntaxe;
    }
    if (p < t.size() && t[p] == '.') {
        ++p;
        if (p >= t.size())
            return Erreur::FinInattendue;
        if (!est_chiffre(t[p]))
            return Erreur::Syntaxe;
        while (p < t.size() && est_chiffre(t[p]))
            ++p;
    }
    if (p < t.size() && (t[p] == 'e' || t[p] == 'E')) {
        ++p;
        if (p < t.size() && (t[p] == '+' || t[p] == '-'))
            ++p;
        if (p >= t.size())
            return Erreur::FinInattendue;
        if (!est_chiffre(t[p]))
            return Erreur::Syntaxe;
        while (p < t.size() && est_chiffre(t[p]))
            ++p;
    }
    return Erreur::Aucune;
}

constexpr Erreur valider_mot(std::string_view t, std::size_t &p, std::string_view mot) noexcept {
    if (t.size() - p < mot.size()) {
        return t.substr(p) == mot.substr(0, t.size() - p) ? Erreur::FinInattendue : Erreur::Syntaxe;
    }
    if (t.substr(p, mot.size()) != mot)
        return Erreur::Syntaxe;
    p += mot.size();
    if (p < t.size() && !est_separateur(t[p]))
        return Erreur::Syntaxe; // « nullx »
    return Erreur::Aucune;
}

constexpr Erreur valider_valeur(std::string_view t, std::size_t &p, int prof) noexcept {
    p = sauter_blancs(t, p);
    if (p >= t.size())
        return Erreur::FinInattendue;
    const char c = t[p];
    if (c == '{' || c == '[') {
        if (prof >= PROFONDEUR_MAX)
            return Erreur::TropProfond;
        const bool objet = (c == '{');
        const char fermant = objet ? '}' : ']';
        ++p;
        p = sauter_blancs(t, p);
        if (p >= t.size())
            return Erreur::FinInattendue;
        if (t[p] == fermant) {
            ++p;
            return Erreur::Aucune;
        }
        for (;;) {
            p = sauter_blancs(t, p);
            if (p >= t.size())
                return Erreur::FinInattendue;
            if (objet) {
                if (t[p] != '"')
                    return Erreur::Syntaxe;
                Erreur e = valider_chaine(t, p);
                if (e != Erreur::Aucune)
                    return e;
                p = sauter_blancs(t, p);
                if (p >= t.size())
                    return Erreur::FinInattendue;
                if (t[p] != ':')
                    return Erreur::Syntaxe;
                ++p;
            }
            const Erreur e = valider_valeur(t, p, prof + 1);
            if (e != Erreur::Aucune)
                return e;
            p = sauter_blancs(t, p);
            if (p >= t.size())
                return Erreur::FinInattendue;
            if (t[p] == ',') {
                ++p;
                continue;
            }
            if (t[p] == fermant) {
                ++p;
                return Erreur::Aucune;
            }
            return Erreur::Syntaxe;
        }
    }
    if (c == '"')
        return valider_chaine(t, p);
    if (c == 't')
        return valider_mot(t, p, "true");
    if (c == 'f')
        return valider_mot(t, p, "false");
    if (c == 'n')
        return valider_mot(t, p, "null");
    if (c == '-' || est_chiffre(c))
        return valider_nombre(t, p);
    return Erreur::Syntaxe;
}

// ---- nombres ---------------------------------------------------------------------------------------------------

// Lit un nombre déjà validé : valeur = (négatif ? -1 : 1) * mantisse * 10^exposant ; `entier` = ni « . » ni « e ».
constexpr bool decomposer_nombre(std::string_view t, bool &negatif, std::uint64_t &mantisse, int &exposant,
                                 bool &entier) noexcept {
    constexpr std::uint64_t LIMITE = (UINT64_MAX - 9) / 10;
    std::size_t p = 0;
    negatif = false;
    mantisse = 0;
    exposant = 0;
    entier = true;
    if (p < t.size() && t[p] == '-') {
        negatif = true;
        ++p;
    }
    if (p >= t.size() || !est_chiffre(t[p]))
        return false;
    while (p < t.size() && est_chiffre(t[p])) {
        if (mantisse <= LIMITE) {
            mantisse = mantisse * 10 + static_cast<std::uint64_t>(t[p] - '0');
        } else {
            ++exposant; // chiffre ignoré, mais l'ordre de grandeur est gardé
        }
        ++p;
    }
    if (p < t.size() && t[p] == '.') {
        entier = false;
        ++p;
        while (p < t.size() && est_chiffre(t[p])) {
            if (mantisse <= LIMITE) {
                mantisse = mantisse * 10 + static_cast<std::uint64_t>(t[p] - '0');
                --exposant;
            }
            ++p;
        }
    }
    if (p < t.size() && (t[p] == 'e' || t[p] == 'E')) {
        entier = false;
        ++p;
        bool neg_exp = false;
        if (p < t.size() && (t[p] == '+' || t[p] == '-')) {
            neg_exp = (t[p] == '-');
            ++p;
        }
        int e = 0;
        while (p < t.size() && est_chiffre(t[p])) {
            if (e < 100000)
                e = e * 10 + (t[p] - '0');
            ++p;
        }
        exposant += neg_exp ? -e : e;
    }
    return p == t.size();
}

inline constexpr double PUISSANCES_10[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                           1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

// ---- chaînes : décodage des échappements en UTF-8 -----------------------------------------------------------------

constexpr int ecrire_utf8(char32_t cp, char *sortie) noexcept {
    if (cp < 0x80) {
        sortie[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        sortie[0] = static_cast<char>(0xC0 | (cp >> 6));
        sortie[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        sortie[0] = static_cast<char>(0xE0 | (cp >> 12));
        sortie[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        sortie[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    sortie[0] = static_cast<char>(0xF0 | (cp >> 18));
    sortie[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    sortie[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    sortie[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
}

constexpr char32_t lire_hex4(std::string_view c, std::size_t p) noexcept { // c[p..p+3] déjà validés
    char32_t v = 0;
    for (std::size_t i = 0; i < 4; ++i)
        v = (v << 4) | static_cast<char32_t>(valeur_hex(c[p + i]));
    return v;
}

// Parcourt le CONTENU d'une chaîne (sans les guillemets) et rend, un caractère à la fois, ses octets UTF-8.
struct Decodeur {
    std::string_view contenu;
    std::size_t pos = 0;

    constexpr bool suivant(char (&octets)[4], int &n) noexcept {
        if (pos >= contenu.size())
            return false;
        const unsigned char c = static_cast<unsigned char>(contenu[pos]);
        if (c != '\\') { // caractère brut : on copie la séquence UTF-8 entière
            int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (pos + static_cast<std::size_t>(len) > contenu.size())
                len = static_cast<int>(contenu.size() - pos);
            for (int i = 0; i < len; ++i)
                octets[i] = contenu[pos + static_cast<std::size_t>(i)];
            pos += static_cast<std::size_t>(len);
            n = len;
            return true;
        }
        const char e = pos + 1 < contenu.size() ? contenu[pos + 1] : '\\';
        pos += 2;
        char sortie = e;
        switch (e) {
        case 'b':
            sortie = '\b';
            break;
        case 'f':
            sortie = '\f';
            break;
        case 'n':
            sortie = '\n';
            break;
        case 'r':
            sortie = '\r';
            break;
        case 't':
            sortie = '\t';
            break;
        case 'u': {
            char32_t cp = pos + 4 <= contenu.size() ? lire_hex4(contenu, pos) : 0xFFFD;
            pos += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF) { // moitié haute : doit être suivie de \uDC00..\uDFFF
                if (pos + 6 <= contenu.size() && contenu[pos] == '\\' && contenu[pos + 1] == 'u') {
                    const char32_t bas = lire_hex4(contenu, pos + 2);
                    if (bas >= 0xDC00 && bas <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (bas - 0xDC00);
                        pos += 6;
                    } else {
                        cp = 0xFFFD;
                    }
                } else {
                    cp = 0xFFFD;
                }
            } else if ((cp >= 0xDC00 && cp <= 0xDFFF) || cp == 0) {
                cp = 0xFFFD;
            }
            n = ecrire_utf8(cp, octets);
            return true;
        }
        default:
            break; // " \ /
        }
        octets[0] = sortie;
        n = 1;
        return true;
    }
};

} // namespace detail

// Une valeur JSON = une tranche du texte source (du premier au dernier caractère de la valeur) et son genre.
// Construite par `lire` ou par la navigation ; ne pas la fabriquer à la main.
class Valeur {
  public:
    constexpr Valeur() noexcept = default;
    constexpr Valeur(std::string_view brut, Genre genre) noexcept : brut_(brut), genre_(genre) {}

    constexpr Genre genre() const noexcept { return genre_; }
    constexpr bool existe() const noexcept { return genre_ != Genre::Absent; }
    constexpr bool est_nul() const noexcept { return genre_ == Genre::Nul; }
    constexpr std::string_view brut() const noexcept { return brut_; }

    // Membre d'un objet (clé comparée APRÈS décodage des échappements). Absent si pas un objet ou clé inconnue.
    constexpr Valeur champ(std::string_view cle) const noexcept {
        if (genre_ != Genre::Objet)
            return {};
        std::size_t p = 1;
        for (;;) {
            p = detail::sauter_blancs(brut_, p);
            if (p >= brut_.size() || brut_[p] != '"')
                return {};
            const std::size_t fin_cle = detail::fin_chaine(brut_, p);
            if (fin_cle < p + 2 || fin_cle > brut_.size())
                return {};
            const std::string_view contenu_cle = brut_.substr(p + 1, fin_cle - p - 2);
            p = detail::sauter_blancs(brut_, fin_cle);
            if (p >= brut_.size() || brut_[p] != ':')
                return {};
            p = detail::sauter_blancs(brut_, p + 1);
            if (p >= brut_.size())
                return {};
            const std::size_t fin = detail::fin_valeur(brut_, p);
            if (egal_decode(contenu_cle, cle))
                return Valeur(brut_.substr(p, fin - p), detail::genre_de(brut_[p]));
            p = detail::sauter_blancs(brut_, fin);
            if (p < brut_.size() && brut_[p] == ',') {
                ++p;
            } else {
                return {};
            }
        }
    }

    // Élément n° `i` (depuis 0) d'un tableau. Absent si pas un tableau ou i hors limites.
    constexpr Valeur element(std::size_t i) const noexcept {
        if (genre_ != Genre::Tableau)
            return {};
        std::size_t p = 1;
        std::size_t idx = 0;
        for (;;) {
            p = detail::sauter_blancs(brut_, p);
            if (p >= brut_.size() || brut_[p] == ']')
                return {};
            const std::size_t fin = detail::fin_valeur(brut_, p);
            if (idx == i)
                return Valeur(brut_.substr(p, fin - p), detail::genre_de(brut_[p]));
            ++idx;
            p = detail::sauter_blancs(brut_, fin);
            if (p < brut_.size() && brut_[p] == ',') {
                ++p;
            } else {
                return {};
            }
        }
    }

    // Nombre d'éléments d'un tableau ou de membres d'un objet ; 0 pour tout le reste.
    constexpr std::size_t taille() const noexcept {
        if (genre_ != Genre::Tableau && genre_ != Genre::Objet)
            return 0;
        const char fermant = genre_ == Genre::Objet ? '}' : ']';
        std::size_t p = detail::sauter_blancs(brut_, 1);
        if (p >= brut_.size() || brut_[p] == fermant)
            return 0;
        std::size_t n = 0;
        for (;;) {
            p = detail::sauter_blancs(brut_, p);
            if (genre_ == Genre::Objet) { // clé, puis « : »
                if (p >= brut_.size() || brut_[p] != '"')
                    return n;
                p = detail::sauter_blancs(brut_, detail::fin_chaine(brut_, p));
                if (p >= brut_.size() || brut_[p] != ':')
                    return n;
                p = detail::sauter_blancs(brut_, p + 1);
            }
            if (p >= brut_.size())
                return n;
            p = detail::sauter_blancs(brut_, detail::fin_valeur(brut_, p));
            ++n;
            if (p < brut_.size() && brut_[p] == ',') {
                ++p;
            } else {
                return n;
            }
        }
    }

    // Chemin « a.b[3].c » : noms séparés par des points, indices entre crochets, « [2] » possible en tête.
    // Chemin vide = cette valeur. Les clés qui contiennent « . » ou « [ » ne s'atteignent qu'avec champ().
    constexpr Valeur trouver(std::string_view chemin) const noexcept {
        Valeur v = *this;
        std::size_t i = 0;
        bool apres_crochet = false;
        while (i < chemin.size()) {
            if (!v.existe())
                return {};
            if (chemin[i] == '[') {
                ++i;
                std::size_t n = 0;
                bool chiffre = false;
                while (i < chemin.size() && detail::est_chiffre(chemin[i])) {
                    if (n > 100000000)
                        return {};
                    n = n * 10 + static_cast<std::size_t>(chemin[i] - '0');
                    chiffre = true;
                    ++i;
                }
                if (!chiffre || i >= chemin.size() || chemin[i] != ']')
                    return {};
                ++i;
                v = v.element(n);
                apres_crochet = true;
                continue;
            }
            if (chemin[i] == '.') {
                if (i == 0)
                    return {};
                ++i;
                if (i >= chemin.size() || chemin[i] == '.' || chemin[i] == '[')
                    return {};
            } else if (apres_crochet) {
                return {}; // « a[0]b » : il faut un point
            }
            const std::size_t debut = i;
            while (i < chemin.size() && chemin[i] != '.' && chemin[i] != '[')
                ++i;
            v = v.champ(chemin.substr(debut, i - debut));
            apres_crochet = false;
        }
        return v;
    }

    constexpr bool en_booleen(bool &sortie) const noexcept {
        if (genre_ != Genre::Booleen)
            return false;
        sortie = (brut_ == "true");
        return true;
    }

    // Entier écrit sans « . » ni exposant (« 3 » oui, « 3.0 » et « 3e0 » non), dans l'intervalle de int64.
    constexpr bool en_entier(std::int64_t &sortie) const noexcept {
        bool neg = false, entier = false;
        std::uint64_t m = 0;
        int e = 0;
        if (genre_ != Genre::Nombre || !detail::decomposer_nombre(brut_, neg, m, e, entier) || !entier || e != 0) {
            return false;
        }
        return versvaleur(neg, m, sortie);
    }

    // Nombre décimal multiplié par 10^decimales, arrondi à l'entier le plus proche (moitié : loin de zéro), calculé
    // sur le texte (« 11.6 », 1 -> 116 ; « 1.35 », 1 -> 14 ; « -0.04 », 1 -> 0). false si hors de int64.
    constexpr bool en_fixe(int decimales, std::int64_t &sortie) const noexcept {
        bool neg = false, entier = false;
        std::uint64_t m = 0;
        int e = 0;
        if (genre_ != Genre::Nombre || decimales < 0 || decimales > 18 ||
            !detail::decomposer_nombre(brut_, neg, m, e, entier)) {
            return false;
        }
        const int s = e + decimales;
        if (m != 0 && s >= 0) {
            if (s > 19)
                return false;
            for (int i = 0; i < s; ++i) {
                if (m > UINT64_MAX / 10)
                    return false;
                m *= 10;
            }
        } else if (m != 0) {
            const int decalage = -s;
            if (decalage > 19) {
                m = 0;
            } else {
                std::uint64_t p = 1;
                for (int i = 0; i < decalage; ++i)
                    p *= 10;
                std::uint64_t q = m / p;
                const std::uint64_t r = m % p;
                if (r >= p - r)
                    ++q;
                m = q;
            }
        }
        return versvaleur(neg, m, sortie);
    }

    constexpr bool en_double(double &sortie) const noexcept {
        bool neg = false, entier = false;
        std::uint64_t m = 0;
        int e = 0;
        if (genre_ != Genre::Nombre || !detail::decomposer_nombre(brut_, neg, m, e, entier))
            return false;
        double v = static_cast<double>(m);
        if (m != 0) {
            if (e > 308 || e < -400)
                return false;
            if (e < 0) {
                int reste = -e;
                for (; reste > 22; reste -= 22)
                    v /= detail::PUISSANCES_10[22];
                v /= detail::PUISSANCES_10[reste];
            } else {
                int reste = e;
                for (; reste > 22; reste -= 22) {
                    if (v > 1e280)
                        return false;
                    v *= detail::PUISSANCES_10[22];
                }
                v *= detail::PUISSANCES_10[reste];
            }
        }
        sortie = neg ? -v : v;
        return true;
    }

    // Une chaîne égale (une fois décodée) à `attendu` ? Faux pour tout ce qui n'est pas une chaîne.
    constexpr bool texte_egal(std::string_view attendu) const noexcept {
        if (genre_ != Genre::Chaine || brut_.size() < 2)
            return false;
        return egal_decode(brut_.substr(1, brut_.size() - 2), attendu);
    }

    // Copie la chaîne décodée (UTF-8) dans `sortie`, toujours terminée par un octet 0 si capacite > 0. Une chaîne
    // trop longue est coupée sur une frontière de caractère. Rend le nombre d'octets écrits (sans le 0) ;
    // `*tronque` (facultatif) dit si du texte a été perdu. Rend 0 pour une valeur qui n'est pas une chaîne.
    constexpr std::size_t copier_texte(char *sortie, std::size_t capacite, bool *tronque = nullptr) const noexcept {
        if (tronque != nullptr)
            *tronque = false;
        if (capacite == 0 || sortie == nullptr) {
            if (tronque != nullptr)
                *tronque = true;
            return 0;
        }
        sortie[0] = '\0';
        if (genre_ != Genre::Chaine || brut_.size() < 2)
            return 0;
        detail::Decodeur d{brut_.substr(1, brut_.size() - 2)};
        char octets[4] = {};
        int n = 0;
        std::size_t ecrit = 0;
        while (d.suivant(octets, n)) {
            if (ecrit + static_cast<std::size_t>(n) + 1 > capacite) {
                if (tronque != nullptr)
                    *tronque = true;
                break;
            }
            for (int i = 0; i < n; ++i)
                sortie[ecrit + static_cast<std::size_t>(i)] = octets[i];
            ecrit += static_cast<std::size_t>(n);
        }
        sortie[ecrit] = '\0';
        return ecrit;
    }

  private:
    static constexpr bool versvaleur(bool neg, std::uint64_t m, std::int64_t &sortie) noexcept {
        constexpr std::uint64_t MAX = static_cast<std::uint64_t>(INT64_MAX);
        if (neg) {
            if (m > MAX + 1)
                return false;
            sortie = m == MAX + 1 ? INT64_MIN : -static_cast<std::int64_t>(m);
        } else {
            if (m > MAX)
                return false;
            sortie = static_cast<std::int64_t>(m);
        }
        return true;
    }

    // `contenu` (sans guillemets, échappements non décodés) vaut-il `attendu` une fois décodé ?
    static constexpr bool egal_decode(std::string_view contenu, std::string_view attendu) noexcept {
        detail::Decodeur d{contenu};
        char octets[4] = {};
        int n = 0;
        std::size_t i = 0;
        while (d.suivant(octets, n)) {
            for (int k = 0; k < n; ++k, ++i) {
                if (i >= attendu.size() || attendu[i] != octets[k])
                    return false;
            }
        }
        return i == attendu.size();
    }

    std::string_view brut_{};
    Genre genre_ = Genre::Absent;
};

struct Resultat {
    Valeur valeur{}; // la valeur racine ; Genre::Absent si erreur
    Erreur erreur = Erreur::Aucune;
    std::size_t position = 0; // index (en octets) où l'erreur a été vue
    constexpr bool ok() const noexcept { return erreur == Erreur::Aucune; }
};

// Valide tout `texte` (une seule valeur racine entourée de blancs) puis rend la valeur racine.
constexpr Resultat lire(std::string_view texte) noexcept {
    std::size_t p = detail::sauter_blancs(texte, 0);
    if (p >= texte.size())
        return Resultat{{}, Erreur::Vide, p};
    const std::size_t debut = p;
    const Erreur e = detail::valider_valeur(texte, p, 0);
    if (e != Erreur::Aucune)
        return Resultat{{}, e, p};
    const std::size_t fin = p;
    p = detail::sauter_blancs(texte, p);
    if (p < texte.size())
        return Resultat{{}, Erreur::TexteEnTrop, p};
    return Resultat{Valeur(texte.substr(debut, fin - debut), detail::genre_de(texte[debut])), Erreur::Aucune, 0};
}

} // namespace ecran::json
