// Noyau pur : validation des valeurs que la page de réglages envoie à l'appareil (docs/api-reglages.md §5 et §6) et petits
// outils de texte (UTF-8, adresse web). Rien de ce qui entre par le réseau n'est cru : tailles, plages, caractères.
//
// Ce que ce fichier refuse (et pourquoi) :
//   - luminosité hors de 5..100 ou non entière (un écran noir depuis un téléphone n'est pas récupérable) ;
//   - thème ou langue absents de la liste de l'appareil (les listes viennent de l'appareil, pas de la page) ;
//   - ville : nom vide, trop long, UTF-8 invalide ou avec un caractère de contrôle ; coordonnées hors plage ou non finies ;
//     fuseau absent de la table embarquée (sans fuseau connu l'appareil ne saurait pas régler l'heure) ;
//   - recherche de lieu : moins de 3 caractères (et non octets : « Zü » en fait 2), plus de 80, UTF-8 invalide, contrôles.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation (les tests de test/host/ s'en servent).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ecran/api_json.hpp"
#include "ecran/fuseaux.hpp"
#include "ecran/i18n.hpp"
#include "ecran/json_mini.hpp"

namespace ecran::reglages {

// ---- texte ---------------------------------------------------------------------------------------------------------

// UTF-8 bien formé : pas de forme trop longue, pas de substitut (U+D800..DFFF), rien au-delà de U+10FFFF, pas d'octet 0.
constexpr bool utf8_valide(std::string_view s) noexcept {
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t suite = 0;
        std::uint32_t cp = 0;
        std::uint32_t minimum = 0;
        if (c == 0) {
            return false;
        } else if (c < 0x80) {
            ++i;
            continue;
        } else if (c >= 0xC2 && c <= 0xDF) {
            suite = 1;
            cp = c & 0x1Fu;
            minimum = 0x80;
        } else if (c >= 0xE0 && c <= 0xEF) {
            suite = 2;
            cp = c & 0x0Fu;
            minimum = 0x800;
        } else if (c >= 0xF0 && c <= 0xF4) {
            suite = 3;
            cp = c & 0x07u;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (i + suite >= s.size()) // il manque des octets de continuation
            return false;
        for (std::size_t k = 1; k <= suite; ++k) {
            const unsigned char d = static_cast<unsigned char>(s[i + k]);
            if ((d & 0xC0u) != 0x80u)
                return false;
            cp = (cp << 6) | (d & 0x3Fu);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += suite + 1;
    }
    return true;
}

// Nombre de caractères (points de code) d'un texte UTF-8 DÉJÀ valide.
constexpr std::size_t nombre_de_caracteres(std::string_view s) noexcept {
    std::size_t n = 0;
    for (char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u)
            ++n;
    }
    return n;
}

// Aucun octet de contrôle (0x00..0x1F, 0x7F).
constexpr bool sans_controle(std::string_view s) noexcept {
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c == 0x7F)
            return false;
    }
    return true;
}

// Retire les blancs ASCII (espace, tabulation, CR, LF) au début et à la fin.
constexpr std::string_view rogner(std::string_view s) noexcept {
    auto blanc = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && blanc(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && blanc(s.back()))
        s.remove_suffix(1);
    return s;
}

// Plus long début de `s` d'au plus `max_octets` octets qui ne coupe pas un caractère UTF-8 en deux.
constexpr std::string_view couper_utf8(std::string_view s, std::size_t max_octets) noexcept {
    if (s.size() <= max_octets)
        return s;
    std::size_t n = max_octets;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0u) == 0x80u)
        --n;
    return s.substr(0, n);
}

// Copie `s` dans `sortie` (taille `capacite`, zéro final compris) en coupant sur une frontière de caractère. Rend l'octets écrits.
constexpr std::size_t copier_texte(std::string_view s, char *sortie, std::size_t capacite) noexcept {
    if (capacite == 0)
        return 0;
    const std::string_view r = couper_utf8(s, capacite - 1);
    for (std::size_t i = 0; i < r.size(); ++i)
        sortie[i] = r[i];
    sortie[r.size()] = '\0';
    return r.size();
}

constexpr int chiffre_hexa(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Valeur du paramètre `nom` dans une chaîne de requête « a=1&q=Lyon%20nord », DÉCODÉE (%XX et « + »), écrite dans `sortie`
// (zéro final). `trouve` dit si le paramètre existe ; `tronque` dit si le tampon était trop petit ; `invalide` si un « % » n'est
// pas suivi de deux chiffres hexadécimaux. La valeur n'est PAS encore vérifiée en UTF-8.
struct Parametre {
    bool trouve = false;
    bool tronque = false;
    bool invalide = false;
    std::size_t taille = 0;
};
constexpr Parametre parametre_decode(std::string_view requete, std::string_view nom, char *sortie,
                                     std::size_t capacite) noexcept {
    Parametre r;
    if (capacite == 0)
        return r;
    sortie[0] = '\0';
    std::size_t debut = 0;
    while (debut <= requete.size()) {
        std::size_t fin = requete.find('&', debut);
        if (fin == std::string_view::npos)
            fin = requete.size();
        const std::string_view paire = requete.substr(debut, fin - debut);
        const std::size_t egal = paire.find('=');
        const std::string_view cle = egal == std::string_view::npos ? paire : paire.substr(0, egal);
        if (cle == nom) {
            r.trouve = true;
            if (egal != std::string_view::npos) {
                const std::string_view v = paire.substr(egal + 1);
                std::size_t i = 0;
                while (i < v.size()) {
                    char c = v[i];
                    if (c == '+') {
                        c = ' ';
                        ++i;
                    } else if (c == '%') {
                        if (i + 2 >= v.size()) { // « % » sans deux caractères derrière
                            r.invalide = true;
                            break;
                        }
                        const int hi = chiffre_hexa(v[i + 1]);
                        const int lo = chiffre_hexa(v[i + 2]);
                        if (hi < 0 || lo < 0) {
                            r.invalide = true;
                            break;
                        }
                        c = static_cast<char>(hi * 16 + lo);
                        i += 3;
                    } else {
                        ++i;
                    }
                    if (r.taille + 1 >= capacite) {
                        r.tronque = true;
                        break;
                    }
                    sortie[r.taille++] = c;
                }
                sortie[r.taille] = '\0';
            }
            return r;
        }
        debut = fin + 1;
    }
    return r;
}

// Écrit `s` encodé pour une adresse web (RFC 3986 : lettres, chiffres et « -._~ » tels quels, le reste en %XX majuscules).
constexpr void encoder_url(std::string_view s, api::Ecrivain &e) noexcept {
    constexpr char HEX[] = "0123456789ABCDEF";
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        const bool libre = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                           c == '_' || c == '.' || c == '~';
        if (libre) {
            e.octet(ch);
        } else {
            e.octet('%');
            e.octet(HEX[c >> 4]);
            e.octet(HEX[c & 15]);
        }
    }
}

// ---- affichage : langues, thème, luminosité (api-reglages.md §6) ------------------------------------------------------

enum class Theme : std::uint8_t { Auto = 0, Clair = 1, Sombre = 2 };
inline constexpr std::size_t NOMBRE_THEMES = 3;
inline constexpr std::string_view THEMES[NOMBRE_THEMES] = {"auto", "clair", "sombre"};

constexpr std::string_view code(Theme t) noexcept { return THEMES[static_cast<std::size_t>(t)]; }
constexpr std::optional<Theme> theme_depuis_code(std::string_view c) noexcept {
    for (std::size_t i = 0; i < NOMBRE_THEMES; ++i) {
        if (THEMES[i] == c)
            return static_cast<Theme>(i);
    }
    return std::nullopt;
}
// Indice lu en NVS -> thème ; hors de 0..2 : `defaut`.
constexpr Theme theme_depuis_indice(int i, Theme defaut = Theme::Auto) noexcept {
    return (i >= 0 && i < static_cast<int>(NOMBRE_THEMES)) ? static_cast<Theme>(i) : defaut;
}

inline constexpr int LUMINOSITE_MIN = 5;
inline constexpr int LUMINOSITE_MAX = 100;
// 75 % comme le rétroéclairage du premier démarrage (main.cpp) : moins de courant et de chaleur que 100 %.
inline constexpr int LUMINOSITE_DEFAUT = 75;

constexpr bool luminosite_valide(long long v) noexcept { return v >= LUMINOSITE_MIN && v <= LUMINOSITE_MAX; }
// Valeur lue en NVS : hors plage -> défaut (un NVS abîmé ne doit pas éteindre l'écran).
constexpr int luminosite_depuis_nvs(long long v) noexcept {
    return luminosite_valide(v) ? static_cast<int>(v) : LUMINOSITE_DEFAUT;
}

struct Affichage {
    i18n::Langue langue_ecran = i18n::Langue::Fr;
    i18n::Langue langue_assistant = i18n::Langue::Fr;
    Theme theme = Theme::Auto;
    int luminosite = LUMINOSITE_DEFAUT;
};

constexpr bool operator==(const Affichage &a, const Affichage &b) noexcept {
    return a.langue_ecran == b.langue_ecran && a.langue_assistant == b.langue_assistant && a.theme == b.theme &&
           a.luminosite == b.luminosite;
}

enum class ErreurAffichage : std::uint8_t { Aucune, LangueEcran, LangueAssistant, Theme, Luminosite };

struct ResultatAffichage {
    ErreurAffichage erreur = ErreurAffichage::Aucune;
    Affichage valeur{};
};

// Applique à `actuel` les champs présents dans l'objet JSON `corps` (absent = inchangé). Tout ou rien : à la première valeur
// refusée, `erreur` le dit et `valeur` reste l'état actuel. Les champs inconnus sont ignorés.
constexpr ResultatAffichage valider_affichage(const json::Valeur &corps, const Affichage &actuel) noexcept {
    ResultatAffichage r;
    r.valeur = actuel;
    Affichage n = actuel;
    const json::Valeur le = corps.champ("langue_ecran");
    if (le.existe()) {
        char t[8] = {};
        bool tronque = false;
        le.copier_texte(t, sizeof t, &tronque);
        const auto l = le.genre() == json::Genre::Chaine && !tronque ? i18n::langue_depuis_code(t) : std::nullopt;
        if (!l) {
            r.erreur = ErreurAffichage::LangueEcran;
            return r;
        }
        n.langue_ecran = *l;
    }
    const json::Valeur la = corps.champ("langue_assistant");
    if (la.existe()) {
        char t[8] = {};
        bool tronque = false;
        la.copier_texte(t, sizeof t, &tronque);
        const auto l = la.genre() == json::Genre::Chaine && !tronque ? i18n::langue_depuis_code(t) : std::nullopt;
        if (!l) {
            r.erreur = ErreurAffichage::LangueAssistant;
            return r;
        }
        n.langue_assistant = *l;
    }
    const json::Valeur th = corps.champ("theme");
    if (th.existe()) {
        char t[16] = {};
        bool tronque = false;
        th.copier_texte(t, sizeof t, &tronque);
        const auto v = th.genre() == json::Genre::Chaine && !tronque ? theme_depuis_code(t) : std::nullopt;
        if (!v) {
            r.erreur = ErreurAffichage::Theme;
            return r;
        }
        n.theme = *v;
    }
    const json::Valeur lu = corps.champ("luminosite");
    if (lu.existe()) {
        std::int64_t v = 0;
        if (!lu.en_entier(v) || !luminosite_valide(v)) {
            r.erreur = ErreurAffichage::Luminosite;
            return r;
        }
        n.luminosite = static_cast<int>(v);
    }
    r.valeur = n;
    return r;
}

// ---- recherche de lieu (api-reglages.md §5) -----------------------------------------------------------------------------

inline constexpr std::size_t RECHERCHE_MIN = 3;                        // caractères, après rognage
inline constexpr std::size_t RECHERCHE_MAX = 80;                       // caractères
inline constexpr std::size_t RECHERCHE_OCTETS_MAX = 4 * RECHERCHE_MAX; // une adresse décodée tient dans ce tampon

enum class ErreurRecherche : std::uint8_t { Aucune, Illisible, TropCourte, TropLongue };

struct ResultatRecherche {
    ErreurRecherche erreur = ErreurRecherche::Aucune;
    std::string_view q{}; // rognée, dans le texte reçu
};

constexpr ResultatRecherche valider_recherche(std::string_view brut) noexcept {
    ResultatRecherche r;
    const std::string_view q = rogner(brut);
    if (!utf8_valide(q) || !sans_controle(q)) {
        r.erreur = ErreurRecherche::Illisible;
        return r;
    }
    const std::size_t n = nombre_de_caracteres(q);
    if (n < RECHERCHE_MIN) {
        r.erreur = ErreurRecherche::TropCourte;
    } else if (n > RECHERCHE_MAX) {
        r.erreur = ErreurRecherche::TropLongue;
    } else {
        r.q = q;
    }
    return r;
}

// Adresse du géocodage d'Open-Meteo pour la recherche `q` dans la langue `langue` (api-reglages.md §5 : count=8, format=json).
// Écrit dans `e` ; l'appelant vérifie `e.ok()`.
constexpr void adresse_geocodage(std::string_view q, i18n::Langue langue, api::Ecrivain &e) noexcept {
    e.brut("https://geocoding-api.open-meteo.com/v1/search?name=");
    encoder_url(q, e);
    e.brut("&count=8&language=");
    e.brut(i18n::code(langue));
    e.brut("&format=json");
}

// ---- ville enregistrée (api-reglages.md §5) -----------------------------------------------------------------------------

inline constexpr std::size_t NOM_ZONE_OCTETS =
    48; // zéro final compris : identique à ZoneMeteo::nom (main/meteo_zone.h)
inline constexpr std::size_t TEXTE_ZONE_OCTETS = 64;
inline constexpr std::size_t NOM_MAX_CARACTERES = 80;

struct Zone {
    char nom[NOM_ZONE_OCTETS]{};
    char region[TEXTE_ZONE_OCTETS]{};
    char pays[TEXTE_ZONE_OCTETS]{};
    char code_pays[4]{};
    double latitude = 0;
    double longitude = 0;
    char fuseau[40]{};
};

enum class ErreurZone : std::uint8_t { Aucune, Nom, Coordonnees, Fuseau };

struct ResultatZone {
    ErreurZone erreur = ErreurZone::Aucune;
    Zone zone{};
};

namespace detail {

// Champ texte facultatif : absent ou nul -> vide ; sinon UTF-8 valide sans contrôle, coupé à la taille du tampon.
// Rend faux si le champ est présent mais d'un autre genre qu'une chaîne, ou invalide.
template <std::size_t N> constexpr bool texte_facultatif(const json::Valeur &v, char (&sortie)[N]) noexcept {
    sortie[0] = '\0';
    if (!v.existe() || v.est_nul())
        return true;
    if (v.genre() != json::Genre::Chaine)
        return false;
    char tmp[N + 64] = {};
    v.copier_texte(tmp, sizeof tmp);
    const std::string_view t = rogner(tmp);
    if (!utf8_valide(t) || !sans_controle(t))
        return false;
    copier_texte(t, sortie, N);
    return true;
}

} // namespace detail

// Valide un résultat de recherche tel que la page le renvoie (champs `nom`, `region`, `pays`, `code_pays`, `latitude`,
// `longitude`, `fuseau` ; les autres, dont `altitude`, sont ignorés). Ordre des vérifications : nom, coordonnées, fuseau.
constexpr ResultatZone valider_zone(const json::Valeur &corps) noexcept {
    ResultatZone r;
    // nom : obligatoire, 1 à 80 caractères, UTF-8 sans contrôle ; gardé jusqu'à NOM_ZONE_OCTETS - 1 octets
    const json::Valeur nom = corps.champ("nom");
    char brut[4 * NOM_MAX_CARACTERES + 8] = {};
    bool tronque = false;
    if (nom.genre() != json::Genre::Chaine) {
        r.erreur = ErreurZone::Nom;
        return r;
    }
    nom.copier_texte(brut, sizeof brut, &tronque);
    const std::string_view n = rogner(brut);
    if (tronque || n.empty() || !utf8_valide(n) || !sans_controle(n) || nombre_de_caracteres(n) > NOM_MAX_CARACTERES) {
        r.erreur = ErreurZone::Nom;
        return r;
    }
    copier_texte(n, r.zone.nom, NOM_ZONE_OCTETS);
    // coordonnées : deux nombres finis dans la plage
    double lat = 0;
    double lon = 0;
    if (!corps.champ("latitude").en_double(lat) || !corps.champ("longitude").en_double(lon) ||
        !(lat >= -90.0 && lat <= 90.0) || !(lon >= -180.0 && lon <= 180.0)) {
        r.erreur = ErreurZone::Coordonnees;
        return r;
    }
    r.zone.latitude = lat;
    r.zone.longitude = lon;
    // fuseau : un nom IANA de la table embarquée
    char f[40] = {};
    bool f_tronque = false;
    const json::Valeur fv = corps.champ("fuseau");
    if (fv.genre() != json::Genre::Chaine) {
        r.erreur = ErreurZone::Fuseau;
        return r;
    }
    fv.copier_texte(f, sizeof f, &f_tronque);
    if (f_tronque || fuseaux::trouver(f) == nullptr) {
        r.erreur = ErreurZone::Fuseau;
        return r;
    }
    copier_texte(f, r.zone.fuseau, sizeof r.zone.fuseau);
    // champs d'affichage facultatifs : invalides -> vides (ils ne servent qu'à la page)
    if (!detail::texte_facultatif(corps.champ("region"), r.zone.region))
        r.zone.region[0] = '\0';
    if (!detail::texte_facultatif(corps.champ("pays"), r.zone.pays))
        r.zone.pays[0] = '\0';
    char cp[16] = {};
    if (detail::texte_facultatif(corps.champ("code_pays"), cp)) {
        std::size_t k = 0;
        bool lettres = std::string_view(cp).size() <= 3;
        for (; cp[k] != '\0' && k < 3; ++k) {
            const char c = cp[k];
            lettres = lettres && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
        }
        if (lettres) {
            for (std::size_t i = 0; i < k; ++i)
                r.zone.code_pays[i] = cp[i];
            r.zone.code_pays[k] = '\0';
        }
    }
    return r;
}

} // namespace ecran::reglages
