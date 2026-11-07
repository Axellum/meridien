// Noyau pur : lecture de la réponse de Vigie (GET /v1/vigilances) — les avertissements OFFICIELS d'un lieu. C++17 standard, aucun include
// ESP-IDF ni LVGL, aucune allocation, aucune exception, évaluable à la compilation.
//
// Ce que ce fichier fait : transformer le JSON en une structure à tailles fixes (`Alertes`), SANS rien réécrire des mots de l'émetteur
// (titre, description, consigne, nom, zone, niveau et phénomène d'origine, dates) : ils sont copiés tels quels, coupés seulement
// sur une frontière de caractère quand ils dépassent la place prévue (et le drapeau `tronque` le dit). Un champ absent ou `null`
// reste INCONNU (`std::optional` vide, chaîne vide, `Niveau::Inconnu`) : jamais 0, jamais « vert » par défaut.
// Ce qu'il ne fait pas : décider si une alerte est « en vigueur » à tel instant (vigie_vue.hpp) ni la dessiner.
//
// Ce que Vigie ajoute à la parole de l'émetteur, et qui est marqué `derived` dans la réponse : `status` (new/update/cancel), `level`
// (GREEN/YELLOW/ORANGE/RED) et `phenomenon` (wind, rain…). Les autres champs sont ceux de l'émetteur. (vigie_api/app/models/vigilance.py.)
//
// Règles de lecture (ADR-0009) :
//   - la racine doit être un objet dont `version` commence par « 1. » et qui porte un tableau `vigilances` ; sinon un statut d'erreur
//     (`Illisible`, `VersionInconnue`, `Incoherent`) et AUCUNE alerte : on ne devine rien d'une réponse qu'on ne comprend pas ;
//   - une chaîne qui contient un caractère que la police embarquée ne sait pas dessiner (arabe, chinois, symbole rare, UTF-8 invalide)
//     n'est PAS gardée : un texte entier est gardé ou écarté, jamais réécrit. Il reste alors les autres langues, puis les mots
//     de Vigie (vigie_textes.hpp) ;
//   - un instant doit être un entier entre 2000 et 2100, sinon il est inconnu ;
//   - plus de TEXTES_MAX textes pour une alerte : on garde la langue de l'écran (`langue`), puis l'anglais, puis les premiers ;
//   - plus de ALERTES_MAX alertes : on garde les plus graves, puis les plus anciennes ; le reste est compté dans `omises` ;
//   - deux avertissements identiques en tout (niveau, phénomène, dates, zone, textes… ; l'AEMET en publie ainsi par paires) ne
//     font qu'une alerte, `doublons` les compte.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ecran/json_mini.hpp"

namespace ecran::vigie {

inline constexpr std::size_t ALERTES_MAX = 12;
inline constexpr std::size_t TEXTES_MAX = 3;
inline constexpr std::size_t LANGUE_CAP = 12;
inline constexpr std::size_t TITRE_CAP = 192;
inline constexpr std::size_t DESCRIPTION_CAP = 768;
inline constexpr std::size_t CONSIGNE_CAP = 512;
inline constexpr std::size_t EMETTEUR_CAP = 64;
inline constexpr std::size_t ZONE_CAP = 96;
inline constexpr std::size_t ORIGINAL_CAP = 64;
inline constexpr std::size_t ATTRIBUTION_CAP = 512;
inline constexpr std::size_t AVERTISSEMENT_CAP = 384;

// Niveau dérivé par Vigie. L'ordre numérique est celui de la gravité ; `Inconnu` (niveau absent ou hors de sa table) est le plus bas.
enum class Niveau : std::uint8_t { Inconnu = 0, Vert = 1, Jaune = 2, Orange = 3, Rouge = 4 };

enum class Phenomene : std::uint8_t {
    Inconnu = 0,
    Vent,
    Pluie,
    Orage,
    NeigeVerglas,
    Chaleur,
    Froid,
    Crue,
    Avalanche,
    Cotier,
    Incendie,
    Brouillard,
    Autre,
};
inline constexpr std::size_t NOMBRE_PHENOMENES = 13;

enum class Etat : std::uint8_t { Inconnu = 0, Nouveau, MiseAJour, Annule };

enum class Statut : std::uint8_t {
    Illisible = 0,   // pas du JSON, ou texte coupé
    VersionInconnue, // `version` absente ou pas « 1.x »
    Incoherent,      // du JSON, mais pas la réponse attendue (pas d'objet, pas de tableau `vigilances`)
    Ok,
};

namespace detail {

struct Plage {
    char32_t a;
    char32_t b;
};

// Caractères DESSINABLES par la police embarquée (main/assets/roboto_700.ttf, table cmap lue avec fontTools le 07/11/2026) : latin
// (ASCII, Latin-1, latin étendu A, et du latin étendu B seulement le roumain « Ș ș Ț ț » et les lettres vietnamiennes à crochet « Ơ ơ Ư
// ư »), grec, cyrillique, vietnamien, ponctuation courante, euro, moins. Une plage ne donne que ce que la
// police contient : test_vigie_alertes.cpp la compare à la police (controle_python_vigie.py).
inline constexpr Plage PLAGES_POLICE[] = {
    {0x0020, 0x007E}, {0x00A0, 0x017F}, {0x01A0, 0x01A1}, {0x01AF, 0x01B0}, {0x0218, 0x021B}, {0x0384, 0x038A},
    {0x038C, 0x038C}, {0x038E, 0x03A1}, {0x03A3, 0x03CE}, {0x0400, 0x0486}, {0x0488, 0x0513}, {0x1EA0, 0x1EF9},
    {0x2010, 0x2011}, {0x2013, 0x2015}, {0x2017, 0x201E}, {0x2020, 0x2022}, {0x2025, 0x2027}, {0x2030, 0x2030},
    {0x2032, 0x2033}, {0x2039, 0x203A}, {0x20AC, 0x20AC}, {0x2212, 0x2212},
};

constexpr bool dessinable(char32_t c) noexcept {
    if (c == U'\n')
        return true; // retour à la ligne : les descriptions en contiennent
    for (const Plage &p : PLAGES_POLICE) {
        if (c >= p.a && c <= p.b)
            return true;
    }
    return false;
}

// Décode UN caractère UTF-8 STRICT (pas de forme trop longue, pas de moitié de paire, pas au-delà de U+10FFFF) en `t[i]` ; rend sa
// longueur en octets (1 à 4), ou 0 si la séquence est invalide ou coupée.
constexpr int decoder_utf8(std::string_view t, std::size_t i, char32_t &cp) noexcept {
    if (i >= t.size())
        return 0;
    const unsigned char c = static_cast<unsigned char>(t[i]);
    auto suite = [&](std::size_t k) -> int { return i + k < t.size() ? static_cast<unsigned char>(t[i + k]) : -1; };
    auto est_suite = [](int o) { return o >= 0x80 && o <= 0xBF; };
    if (c < 0x80) {
        cp = c;
        return 1;
    }
    if (c >= 0xC2 && c <= 0xDF) {
        const int b1 = suite(1);
        if (!est_suite(b1))
            return 0;
        cp = static_cast<char32_t>(((c & 0x1F) << 6) | (b1 & 0x3F));
        return 2;
    }
    if (c >= 0xE0 && c <= 0xEF) {
        const int b1 = suite(1);
        const int b2 = suite(2);
        if (!est_suite(b1) || !est_suite(b2))
            return 0;
        if (c == 0xE0 && b1 < 0xA0)
            return 0; // forme trop longue
        if (c == 0xED && b1 > 0x9F)
            return 0; // moitié de paire de substitution
        cp = static_cast<char32_t>(((c & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
        return 3;
    }
    if (c >= 0xF0 && c <= 0xF4) {
        const int b1 = suite(1);
        const int b2 = suite(2);
        const int b3 = suite(3);
        if (!est_suite(b1) || !est_suite(b2) || !est_suite(b3))
            return 0;
        if (c == 0xF0 && b1 < 0x90)
            return 0;
        if (c == 0xF4 && b1 > 0x8F)
            return 0;
        cp = static_cast<char32_t>(((c & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));
        return 4;
    }
    return 0;
}

template <std::size_t N> constexpr std::string_view vue_de(const char (&t)[N]) noexcept {
    std::size_t n = 0;
    while (n < N && t[n] != '\0')
        ++n;
    return std::string_view(t, n);
}

constexpr char minuscule(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

// « fr-FR », « fr_CA », « FR » -> « fr » (les lettres avant le premier « - » ou « _ », en minuscules, au plus 3).
constexpr std::string_view primaire(std::string_view etiquette, char (&sortie)[4]) noexcept {
    std::size_t n = 0;
    while (n < etiquette.size() && n < 3 && etiquette[n] != '-' && etiquette[n] != '_') {
        sortie[n] = minuscule(etiquette[n]);
        ++n;
    }
    if (n < etiquette.size() && etiquette[n] != '-' && etiquette[n] != '_')
        n = 0; // plus de trois lettres : pas une étiquette de langue que l'on sait comparer
    sortie[n] = '\0';
    return std::string_view(sortie, n);
}

} // namespace detail

// Toute la chaîne est-elle dessinable (UTF-8 valide, uniquement des caractères de la police embarquée, retours à la ligne permis) ?
constexpr bool texte_affichable(std::string_view t) noexcept {
    std::size_t i = 0;
    while (i < t.size()) {
        char32_t cp = 0;
        const int n = detail::decoder_utf8(t, i, cp);
        if (n == 0 || !detail::dessinable(cp))
            return false;
        i += static_cast<std::size_t>(n);
    }
    return true;
}

// « fr-FR » correspond-il à « fr » ? (langue principale, casse ignorée)
constexpr bool langue_correspond(std::string_view etiquette, std::string_view code) noexcept {
    char tampon[4] = {};
    const std::string_view p = detail::primaire(etiquette, tampon);
    return !p.empty() && p == code;
}

struct Texte {
    char langue[LANGUE_CAP] = {};
    char titre[TITRE_CAP] = {};
    char description[DESCRIPTION_CAP] = {};
    char consigne[CONSIGNE_CAP] = {};
    bool tronque = false; // un des trois textes a été coupé faute de place
};

struct Couverture {
    bool source_perimee = false; // une source n'a pas répondu à Vigie : sa dernière réponse est servie (`stale`)
    bool source_absente = false; // une source n'a rien (`unavailable`)
    bool non_rattachables =
        false; // des avertissements du pays n'ont pas de forme sur cette instance : impossible de les rattacher à un point
    bool pays_sans_source = false; // le lieu est dans un pays dont cette instance ne lit aucune source (`uncovered`)
    constexpr bool complete() const noexcept {
        return !source_perimee && !source_absente && !non_rattachables && !pays_sans_source;
    }
};

struct Alerte {
    Niveau niveau = Niveau::Inconnu;
    Phenomene phenomene = Phenomene::Inconnu;
    Etat etat = Etat::Inconnu;
    // Instants UTC en secondes ; vide = inconnu (`null` dans la réponse). `debut` = onset (début attendu de l'événement),
    // `effectif` = effective, `expire` = expires (fin de validité du message), `fin` = ends (fin attendue de l'événement, quand l'émetteur la donne).
    std::optional<std::int64_t> envoi;
    std::optional<std::int64_t> effectif;
    std::optional<std::int64_t> debut;
    std::optional<std::int64_t> expire;
    std::optional<std::int64_t> fin;
    char pays[4] = {};
    char niveau_original[ORIGINAL_CAP] = {}; // « naranja », « Minor », « Yellow » : le niveau comme l'émetteur l'écrit
    char phenomene_original[ORIGINAL_CAP] = {}; // le phénomène comme l'émetteur l'écrit
    char evenement[ORIGINAL_CAP] = {};          // `event` de l'émetteur
    char emetteur[EMETTEUR_CAP] = {};
    char zone[ZONE_CAP] = {};               // `area.description`
    char attribution[ATTRIBUTION_CAP] = {}; // la mention que la source demande (à afficher telle quelle)
    bool attribution_tronquee = false;
    bool tronque = false; // un des champs ci-dessus a été coupé faute de place
    std::uint8_t nb_textes = 0;
    Texte textes[TEXTES_MAX];
};

struct Alertes {
    Statut statut = Statut::Illisible;
    std::optional<std::int64_t> serveur_s; // `server_utc`
    std::uint8_t nombre = 0;               // alertes gardées (dans `alertes[0..nombre)`)
    std::uint16_t lues = 0;                // alertes lisibles dans la réponse (gardées, doublons et omises comprises)
    std::uint16_t doublons = 0;
    std::uint16_t omises = 0;
    std::uint16_t ignorees = 0; // sans niveau, ni phénomène, ni mot de l'émetteur : rien à montrer
    Couverture couverture;
    char attribution[ATTRIBUTION_CAP] = {}; // `attribution` de la réponse (les sources utilisées)
    char avertissement[AVERTISSEMENT_CAP] =
        {}; // `disclaimer` : la mention que les sources demandent de publier avec leurs avertissements
    bool attribution_tronquee = false;
    bool avertissement_tronque = false;
    Alerte alertes[ALERTES_MAX + 1]; // la dernière case sert de brouillon pendant la lecture
    Texte brouillon;
};

// Une chaîne des structures ci-dessus (zéro final) vue comme `string_view`.
template <std::size_t N> constexpr std::string_view texte_de(const char (&t)[N]) noexcept { return detail::vue_de(t); }

constexpr std::int64_t INSTANT_MIN_S = 946684800;  // 01/01/2000
constexpr std::int64_t INSTANT_MAX_S = 4102444800; // 01/01/2100

namespace detail {

constexpr Niveau niveau_de(const json::Valeur &v) noexcept {
    if (v.texte_egal("GREEN"))
        return Niveau::Vert;
    if (v.texte_egal("YELLOW"))
        return Niveau::Jaune;
    if (v.texte_egal("ORANGE"))
        return Niveau::Orange;
    if (v.texte_egal("RED"))
        return Niveau::Rouge;
    return Niveau::Inconnu;
}

struct NomPhenomene {
    std::string_view nom;
    Phenomene valeur;
};
inline constexpr NomPhenomene PHENOMENES[] = {
    {"wind", Phenomene::Vent},          {"rain", Phenomene::Pluie},
    {"thunderstorm", Phenomene::Orage}, {"snow-ice", Phenomene::NeigeVerglas},
    {"heat", Phenomene::Chaleur},       {"cold", Phenomene::Froid},
    {"flood", Phenomene::Crue},         {"avalanche", Phenomene::Avalanche},
    {"coastal", Phenomene::Cotier},     {"fire", Phenomene::Incendie},
    {"fog", Phenomene::Brouillard},     {"other", Phenomene::Autre},
};

constexpr Phenomene phenomene_de(const json::Valeur &v) noexcept {
    for (const NomPhenomene &p : PHENOMENES) {
        if (v.texte_egal(p.nom))
            return p.valeur;
    }
    return Phenomene::Inconnu;
}

constexpr Etat etat_de(const json::Valeur &statut, const json::Valeur &type_message) noexcept {
    if (statut.texte_egal("new"))
        return Etat::Nouveau;
    if (statut.texte_egal("update"))
        return Etat::MiseAJour;
    if (statut.texte_egal("cancel"))
        return Etat::Annule;
    // `status` est dérivé de `message_type` : s'il manque, on lit le type du message CAP (« Cancel » = annulée).
    if (type_message.texte_egal("Cancel"))
        return Etat::Annule;
    return Etat::Inconnu;
}

// Instant en secondes UTC : un entier entre 2000 et 2100, sinon inconnu.
constexpr std::optional<std::int64_t> instant_de(const json::Valeur &v) noexcept {
    std::int64_t s = 0;
    if (!v.en_entier(s) || s < INSTANT_MIN_S || s > INSTANT_MAX_S)
        return std::nullopt;
    return s;
}

enum class Copie : std::uint8_t { Rien, Gardee, Refusee };

// Copie une chaîne JSON dans `sortie`. `Rien` : absente, nulle ou vide ; `Refusee` : elle contient un caractère que la police ne sait pas
// dessiner (`sortie` est alors vide : on ne montre pas un texte réécrit) ; `Gardee` sinon. `tronque` passe à vrai si du texte a été
// perdu faute de place.
template <std::size_t N> constexpr Copie copier(const json::Valeur &v, char (&sortie)[N], bool &tronque) noexcept {
    bool coupe = false;
    v.copier_texte(sortie, N, &coupe);
    if (sortie[0] == '\0')
        return Copie::Rien;
    if (!texte_affichable(vue_de(sortie))) {
        sortie[0] = '\0';
        return Copie::Refusee;
    }
    tronque = tronque || coupe;
    return Copie::Gardee;
}

constexpr int rang_langue(std::string_view etiquette, std::string_view preferee) noexcept {
    if (langue_correspond(etiquette, preferee))
        return 0;
    if (langue_correspond(etiquette, "en"))
        return 1;
    return 2;
}

constexpr bool memes_textes(const Texte &a, const Texte &b) noexcept {
    return vue_de(a.langue) == vue_de(b.langue) && vue_de(a.titre) == vue_de(b.titre) &&
           vue_de(a.description) == vue_de(b.description) && vue_de(a.consigne) == vue_de(b.consigne);
}

// Deux alertes identiques en tout ce que l'écran peut montrer ? (l'identifiant n'est pas gardé : c'est lui qui diffère entre deux jumelles.)
constexpr bool identiques(const Alerte &a, const Alerte &b) noexcept {
    if (a.niveau != b.niveau || a.phenomene != b.phenomene || a.etat != b.etat || a.envoi != b.envoi ||
        a.effectif != b.effectif || a.debut != b.debut || a.expire != b.expire || a.fin != b.fin ||
        a.nb_textes != b.nb_textes) {
        return false;
    }
    if (vue_de(a.pays) != vue_de(b.pays) || vue_de(a.niveau_original) != vue_de(b.niveau_original) ||
        vue_de(a.phenomene_original) != vue_de(b.phenomene_original) || vue_de(a.evenement) != vue_de(b.evenement) ||
        vue_de(a.emetteur) != vue_de(b.emetteur) || vue_de(a.zone) != vue_de(b.zone) ||
        vue_de(a.attribution) != vue_de(b.attribution)) {
        return false;
    }
    for (std::size_t i = 0; i < a.nb_textes; ++i) {
        if (!memes_textes(a.textes[i], b.textes[i]))
            return false;
    }
    return true;
}

// `a` est-elle à garder avant `b` quand il faut choisir ? Plus grave d'abord, puis plus ancienne (début inconnu = le plus ancien).
constexpr bool plus_importante(const Alerte &a, const Alerte &b) noexcept {
    if (a.niveau != b.niveau)
        return a.niveau > b.niveau;
    return a.debut.value_or(0) < b.debut.value_or(0);
}

constexpr void vider_alerte(Alerte &a) noexcept {
    a.niveau = Niveau::Inconnu;
    a.phenomene = Phenomene::Inconnu;
    a.etat = Etat::Inconnu;
    a.envoi.reset();
    a.effectif.reset();
    a.debut.reset();
    a.expire.reset();
    a.fin.reset();
    a.pays[0] = '\0';
    a.niveau_original[0] = '\0';
    a.phenomene_original[0] = '\0';
    a.evenement[0] = '\0';
    a.emetteur[0] = '\0';
    a.zone[0] = '\0';
    a.attribution[0] = '\0';
    a.attribution_tronquee = false;
    a.tronque = false;
    a.nb_textes = 0;
    for (Texte &t : a.textes) {
        t.langue[0] = '\0';
        t.titre[0] = '\0';
        t.description[0] = '\0';
        t.consigne[0] = '\0';
        t.tronque = false;
    }
}

// Lit les textes de `v` (tableau `texts`) dans `a` : au plus TEXTES_MAX, en gardant d'abord la langue de l'écran, puis l'anglais.
constexpr void lire_textes(const json::Valeur &liste, std::string_view preferee, Alerte &a, Texte &brouillon) noexcept {
    int rangs[TEXTES_MAX] = {};
    const std::size_t total = liste.taille();
    for (std::size_t j = 0; j < total; ++j) {
        const json::Valeur t = liste.element(j);
        if (t.genre() != json::Genre::Objet)
            continue;
        bool coupe = false;
        char langue_brute[LANGUE_CAP] = {};
        t.champ("language").copier_texte(langue_brute, LANGUE_CAP);
        const Copie titre = copier(t.champ("headline"), brouillon.titre, coupe);
        const Copie description = copier(t.champ("description"), brouillon.description, coupe);
        const Copie consigne = copier(t.champ("instruction"), brouillon.consigne, coupe);
        // Un texte dont UNE des trois parties n'est pas dessinable est écarté en entier : on ne montre pas la moitié d'un avis.
        if (titre == Copie::Refusee || description == Copie::Refusee || consigne == Copie::Refusee)
            continue;
        if (titre == Copie::Rien && description == Copie::Rien && consigne == Copie::Rien)
            continue;
        bool langue_coupee = false;
        copier(t.champ("language"), brouillon.langue, langue_coupee);
        brouillon.tronque = coupe;
        const int rang = rang_langue(vue_de(langue_brute), preferee);
        std::size_t place = a.nb_textes;
        if (a.nb_textes >= TEXTES_MAX) {
            // Plein : remplacer le moins bon (à égalité, le dernier) seulement par un texte STRICTEMENT meilleur.
            std::size_t pire = 0;
            for (std::size_t k = 1; k < TEXTES_MAX; ++k) {
                if (rangs[k] >= rangs[pire])
                    pire = k;
            }
            if (rang >= rangs[pire])
                continue;
            place = pire;
        } else {
            ++a.nb_textes;
        }
        a.textes[place] = brouillon;
        rangs[place] = rang;
    }
}

} // namespace detail

constexpr void vider(Alertes &s) noexcept {
    s.statut = Statut::Illisible;
    s.serveur_s.reset();
    s.nombre = 0;
    s.lues = 0;
    s.doublons = 0;
    s.omises = 0;
    s.ignorees = 0;
    s.couverture = Couverture{};
    s.attribution[0] = '\0';
    s.avertissement[0] = '\0';
    s.attribution_tronquee = false;
    s.avertissement_tronque = false;
    for (Alerte &a : s.alertes)
        detail::vider_alerte(a);
    s.brouillon.langue[0] = '\0';
    s.brouillon.titre[0] = '\0';
    s.brouillon.description[0] = '\0';
    s.brouillon.consigne[0] = '\0';
    s.brouillon.tronque = false;
}

// Lit la réponse de Vigie dans `sortie` (qui est vidée d'abord) ; `langue` = code de la langue de l'écran (« fr »). Rend le statut,
// aussi écrit dans `sortie.statut` ; tant qu'il n'est pas `Ok`, `sortie.nombre` vaut 0. Ne garde aucun pointeur sur `json`.
constexpr Statut analyser(std::string_view json, std::string_view langue, Alertes &sortie) noexcept {
    vider(sortie);
    const json::Resultat lu = json::lire(json);
    if (!lu.ok())
        return sortie.statut = Statut::Illisible;
    const json::Valeur racine = lu.valeur;
    if (racine.genre() != json::Genre::Objet)
        return sortie.statut = Statut::Incoherent;
    const json::Valeur version = racine.champ("version");
    {
        char v[8] = {};
        version.copier_texte(v, sizeof v);
        if (version.genre() != json::Genre::Chaine || v[0] != '1' || v[1] != '.')
            return sortie.statut = Statut::VersionInconnue;
    }
    const json::Valeur liste = racine.champ("vigilances");
    if (liste.genre() != json::Genre::Tableau)
        return sortie.statut = Statut::Incoherent;

    sortie.serveur_s = detail::instant_de(racine.champ("server_utc"));
    detail::copier(racine.champ("attribution"), sortie.attribution, sortie.attribution_tronquee);
    detail::copier(racine.champ("disclaimer"), sortie.avertissement, sortie.avertissement_tronque);

    // Couverture : ce que Vigie dit ne pas avoir pu faire, pour qu'une réponse vide ne se lise pas « rien à craindre ».
    const json::Valeur couvertures = racine.champ("coverage");
    for (std::size_t i = 0; i < couvertures.taille(); ++i) {
        const json::Valeur c = couvertures.element(i);
        const json::Valeur statut = c.champ("status");
        if (statut.texte_egal("stale"))
            sortie.couverture.source_perimee = true;
        else if (statut.texte_egal("unavailable"))
            sortie.couverture.source_absente = true;
        std::int64_t non = 0;
        if (c.champ("warnings_not_matchable").en_entier(non) && non > 0)
            sortie.couverture.non_rattachables = true;
    }
    if (racine.champ("uncovered").taille() > 0)
        sortie.couverture.pays_sans_source = true;

    Alerte &essai = sortie.alertes[ALERTES_MAX];
    const std::size_t total = liste.taille();
    for (std::size_t i = 0; i < total; ++i) {
        const json::Valeur v = liste.element(i);
        if (v.genre() != json::Genre::Objet)
            continue;
        detail::vider_alerte(essai);
        essai.niveau = detail::niveau_de(v.champ("level"));
        essai.phenomene = detail::phenomene_de(v.champ("phenomenon"));
        essai.etat = detail::etat_de(v.champ("status"), v.champ("message_type"));
        essai.envoi = detail::instant_de(v.champ("sent_utc"));
        essai.effectif = detail::instant_de(v.champ("effective_utc"));
        essai.debut = detail::instant_de(v.champ("onset_utc"));
        essai.expire = detail::instant_de(v.champ("expires_utc"));
        essai.fin = detail::instant_de(v.champ("ends_utc"));
        detail::copier(v.champ("country"), essai.pays, essai.tronque);
        detail::copier(v.champ("level_original"), essai.niveau_original, essai.tronque);
        detail::copier(v.champ("phenomenon_original"), essai.phenomene_original, essai.tronque);
        detail::copier(v.champ("event"), essai.evenement, essai.tronque);
        detail::copier(v.champ("issuer"), essai.emetteur, essai.tronque);
        detail::copier(v.champ("area").champ("description"), essai.zone, essai.tronque);
        detail::copier(v.champ("attribution"), essai.attribution, essai.attribution_tronquee);
        detail::lire_textes(v.champ("texts"), langue, essai, sortie.brouillon);
        if (essai.emetteur[0] == '\0' && essai.nb_textes > 0) {
            // Sans `issuer` (le document CAP n'a pas été lu), le nom de l'émetteur est celui du premier texte, quand il en a un.
            bool inutile = false;
            detail::copier(v.champ("texts").element(0).champ("sender_name"), essai.emetteur, inutile);
        }
        for (const Texte &t : essai.textes)
            essai.tronque = essai.tronque || t.tronque;

        const bool a_des_mots =
            essai.nb_textes > 0 || essai.evenement[0] != '\0' || essai.phenomene_original[0] != '\0';
        if (essai.niveau == Niveau::Inconnu && essai.phenomene == Phenomene::Inconnu && !a_des_mots) {
            ++sortie.ignorees;
            continue;
        }
        ++sortie.lues;
        bool double_ = false;
        for (std::size_t k = 0; k < sortie.nombre; ++k) {
            if (detail::identiques(sortie.alertes[k], essai)) {
                double_ = true;
                break;
            }
        }
        if (double_) {
            ++sortie.doublons;
            continue;
        }
        if (sortie.nombre < ALERTES_MAX) {
            sortie.alertes[sortie.nombre++] = essai;
            continue;
        }
        // Plein : la moins importante des alertes gardées cède sa place à une plus importante, sinon la nouvelle est omise.
        std::size_t pire = 0;
        for (std::size_t k = 1; k < ALERTES_MAX; ++k) {
            if (detail::plus_importante(sortie.alertes[pire], sortie.alertes[k]))
                pire = k;
        }
        if (detail::plus_importante(essai, sortie.alertes[pire]))
            sortie.alertes[pire] = essai;
        ++sortie.omises;
    }
    return sortie.statut = Statut::Ok;
}

} // namespace ecran::vigie
