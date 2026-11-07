// Noyau pur : les quatre réglages des alertes officielles (ADR-0009) et leur validation. Ce fichier est à part de
// reglages_valeurs.hpp (réglages d'affichage, ville, services) : il ne change aucune ligne de celui-ci.
//
//   - actives      : oui (défaut) ou non. Non : aucune requête, aucun bandeau, la réponse en mémoire est oubliée ;
//   - adresse      : l'adresse de base de Vigie. Vide = l'instance publique (vigie::ADRESSE_PUBLIQUE). Sinon « https:// » obligatoire
//                    (vigie::normaliser_adresse : « http:// » est refusé, pas d'identifiants ni de paramètres) ;
//   - periode_min  : minutes entre deux requêtes, de 10 à 180 (défaut 15). L'instance publique est limitée par adresse : pas plus vite ;
//   - niveau_min   : le niveau le plus bas montré à l'écran : jaune (défaut), orange ou rouge.
//
// Ce que ce fichier refuse (tout ou rien, comme valider_affichage) : un champ de mauvais type, une période hors de 10..180 ou non
// entière (« 15.5 » est refusé, pas arrondi), un niveau inconnu, une adresse que vigie::normaliser_adresse refuse. Une valeur lue en
// NVS qui n'a plus de sens retombe sur le défaut (un NVS abîmé ne doit pas couper les alertes ni les faire partir toutes les secondes).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/vigie_planning.hpp"
#include "ecran/vigie_requete.hpp"
#include "ecran/vigie_vue.hpp"

namespace ecran::reglages {

// Codes de l'API pour `niveau_min`, dans l'ordre de vigie::NiveauMin (l'indice est celui qui part en NVS : ne jamais renuméroter).
inline constexpr std::string_view CODES_NIVEAU_MIN[vigie::NOMBRE_NIVEAUX_MIN] = {"yellow", "orange", "red"};

constexpr std::string_view code(vigie::NiveauMin n) noexcept {
    const std::size_t i = static_cast<std::size_t>(n);
    return i < vigie::NOMBRE_NIVEAUX_MIN ? CODES_NIVEAU_MIN[i] : CODES_NIVEAU_MIN[0];
}

constexpr std::optional<vigie::NiveauMin> niveau_min_depuis_code(std::string_view c) noexcept {
    for (std::size_t i = 0; i < vigie::NOMBRE_NIVEAUX_MIN; ++i) {
        if (CODES_NIVEAU_MIN[i] == c)
            return static_cast<vigie::NiveauMin>(i);
    }
    return std::nullopt;
}

constexpr bool periode_alertes_valide(long long minutes) noexcept {
    return minutes >= vigie::PERIODE_MIN_MIN && minutes <= vigie::PERIODE_MAX_MIN;
}

struct Alertes {
    bool actives = true;
    char adresse[vigie::ADRESSE_MAX + 1] = {}; // normalisée ; vide = instance publique
    std::int32_t periode_min = vigie::PERIODE_DEFAUT_MIN;
    vigie::NiveauMin niveau_min = vigie::NiveauMin::Jaune;
};

namespace detail {
constexpr bool memes_octets(const char *a, const char *b, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] != b[i])
            return false;
        if (a[i] == '\0')
            return true; // les octets après le zéro ne comptent pas
    }
    return true;
}
} // namespace detail

constexpr bool operator==(const Alertes &a, const Alertes &b) noexcept {
    return a.actives == b.actives && a.periode_min == b.periode_min && a.niveau_min == b.niveau_min &&
           detail::memes_octets(a.adresse, b.adresse, sizeof a.adresse);
}

// L'adresse que le client utilise : celle des réglages, ou l'instance publique si elle est vide. Rend la longueur écrite (sans le
// zéro) ; 0 seulement si `capacite` est trop petite.
constexpr std::size_t adresse_utilisee(const Alertes &r, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    const std::size_t n = vigie::normaliser_adresse(std::string_view(r.adresse), sortie, capacite);
    if (n != 0)
        return n;
    const std::string_view publique = vigie::ADRESSE_PUBLIQUE;
    if (publique.size() + 1 > capacite)
        return 0;
    for (std::size_t i = 0; i < publique.size(); ++i)
        sortie[i] = publique[i];
    sortie[publique.size()] = '\0';
    return publique.size();
}

// Ce que le noyau relit de la NVS : actives (0/1), période, indice de niveau, adresse. Tout ce qui n'a plus de sens retombe sur le défaut.
constexpr Alertes alertes_depuis_nvs(int actives, long long periode_min, int niveau_min,
                                     std::string_view adresse) noexcept {
    Alertes a;
    a.actives = actives != 0;
    a.periode_min =
        periode_alertes_valide(periode_min) ? static_cast<std::int32_t>(periode_min) : vigie::PERIODE_DEFAUT_MIN;
    a.niveau_min = vigie::niveau_min_depuis_indice(niveau_min);
    char propre[vigie::ADRESSE_MAX + 1] = {};
    const std::size_t n = vigie::normaliser_adresse(adresse, propre, sizeof propre);
    for (std::size_t i = 0; i < n; ++i)
        a.adresse[i] = propre[i];
    a.adresse[n] = '\0';
    return a;
}

enum class ErreurAlertes : std::uint8_t { Aucune, Actives, Adresse, Periode, NiveauMin };

struct ResultatAlertes {
    ErreurAlertes erreur = ErreurAlertes::Aucune;
    Alertes valeur{};
};

// Applique à `actuel` les champs présents dans l'objet JSON `corps` (`actives`, `adresse`, `periode_min`, `niveau_min` ; absent =
// inchangé). Tout ou rien : à la première valeur refusée, `erreur` le dit et `valeur` reste l'état actuel. Les champs inconnus sont
// ignorés. `adresse` vide (« "" ») remet l'instance publique ; sinon elle est normalisée et c'est sa forme normalisée qui est gardée.
constexpr ResultatAlertes valider_alertes(const json::Valeur &corps, const Alertes &actuel) noexcept {
    ResultatAlertes r;
    r.valeur = actuel;
    Alertes n = actuel;
    const json::Valeur ac = corps.champ("actives");
    if (ac.existe()) {
        bool b = false;
        if (!ac.en_booleen(b)) {
            r.erreur = ErreurAlertes::Actives;
            return r;
        }
        n.actives = b;
    }
    const json::Valeur ad = corps.champ("adresse");
    if (ad.existe()) {
        char saisie[vigie::ADRESSE_MAX + 2] =
            {}; // un octet de plus que le maximum : « trop long » se voit, il n'est pas coupé
        bool tronque = false;
        if (ad.genre() != json::Genre::Chaine) {
            r.erreur = ErreurAlertes::Adresse;
            return r;
        }
        ad.copier_texte(saisie, sizeof saisie, &tronque);
        const std::string_view texte(saisie);
        // Une saisie vide, ou faite seulement d'espaces et de tabulations, remet l'instance publique ; les espaces de bord d'une vraie
        // adresse sont retirés par normaliser_adresse.
        bool vide = true;
        for (const char c : texte) {
            if (c != ' ' && c != '\t') {
                vide = false;
                break;
            }
        }
        if (tronque) {
            r.erreur = ErreurAlertes::Adresse;
            return r;
        }
        if (vide) {
            n.adresse[0] = '\0';
        } else {
            char propre[vigie::ADRESSE_MAX + 1] = {};
            const std::size_t longueur = vigie::normaliser_adresse(texte, propre, sizeof propre);
            if (longueur == 0) {
                r.erreur = ErreurAlertes::Adresse;
                return r;
            }
            for (std::size_t i = 0; i <= longueur; ++i)
                n.adresse[i] = propre[i];
        }
    }
    const json::Valeur pe = corps.champ("periode_min");
    if (pe.existe()) {
        std::int64_t v = 0;
        if (!pe.en_entier(v) || !periode_alertes_valide(v)) {
            r.erreur = ErreurAlertes::Periode;
            return r;
        }
        n.periode_min = static_cast<std::int32_t>(v);
    }
    const json::Valeur ni = corps.champ("niveau_min");
    if (ni.existe()) {
        char t[16] = {};
        bool tronque = false;
        ni.copier_texte(t, sizeof t, &tronque);
        const auto v = ni.genre() == json::Genre::Chaine && !tronque ? niveau_min_depuis_code(t) : std::nullopt;
        if (!v) {
            r.erreur = ErreurAlertes::NiveauMin;
            return r;
        }
        n.niveau_min = *v;
    }
    r.valeur = n;
    return r;
}

// ---- l'adresse saisie sur l'écran de l'appareil ----------------------------------------------------------------------------

// Rien, ou seulement des espaces et des tabulations : sur l'écran, c'est « remettre l'instance publique ».
constexpr bool saisie_blanche(std::string_view saisie) noexcept {
    for (const char c : saisie) {
        if (c != ' ' && c != '\t')
            return false;
    }
    return true;
}

// `actuel` avec l'adresse tapée au clavier de l'appareil : les mêmes règles que `valider_alertes` pour le champ « adresse »
// (l'écran ne passe pas par l'API). Une saisie blanche remet l'instance publique ; sinon c'est la forme normalisée qui est gardée,
// ou `erreur == Adresse` et `valeur` reste l'état actuel (https:// obligatoire, rien d'autre n'est accepté).
constexpr ResultatAlertes appliquer_adresse(const Alertes &actuel, std::string_view saisie) noexcept {
    ResultatAlertes r;
    r.valeur = actuel;
    Alertes n = actuel;
    if (saisie_blanche(saisie)) {
        n.adresse[0] = '\0';
    } else {
        char propre[vigie::ADRESSE_MAX + 1] = {};
        const std::size_t n_octets = vigie::normaliser_adresse(saisie, propre, sizeof propre);
        if (n_octets == 0) {
            r.erreur = ErreurAlertes::Adresse;
            return r;
        }
        for (std::size_t i = 0; i < n_octets; ++i)
            n.adresse[i] = propre[i];
        n.adresse[n_octets] = '\0';
    }
    r.valeur = n;
    return r;
}

} // namespace ecran::reglages
