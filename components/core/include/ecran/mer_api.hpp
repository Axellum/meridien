// Noyau pur : l'écriture du registre des spots de la page mer par l'API de la page de réglages (docs/api-reglages.md §15, ADR-0025).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
//
// `valider_mer` applique à un registre les champs présents d'un objet JSON (`spots`, `choisi`, `vent_min_kn`, `vent_max_kn` ; absent =
// inchangé). Les règles de chaque valeur sont celles du registre (mer_spots.hpp : `ajouter`, `regler_vent`) : ce fichier ne fait que
// lire le JSON, dire LAQUELLE des sept erreurs de l'API correspond à ce que le registre refuse, et tenir le tout-ou-rien (à la première
// valeur refusée, `neuf` redevient `actuel` : rien n'est à enregistrer).
//
//   spots        : la liste ENTIÈRE, dans l'ordre à garder, 0 à 12 objets {"id", "nom"} (les autres champs d'un objet sont ignorés) ;
//   choisi       : l'identifiant du spot dont l'écran montre la mer, dans la liste telle qu'elle sera après le changement ; "" seulement
//                  pour une liste vide. Absent : le même spot s'il est toujours dans la liste, sinon le premier ;
//   vent_min_kn, vent_max_kn : nœuds ENTIERS (« 12.5 » est refusé, pas arrondi), 0 < min < max <= 60 ; si un seul est envoyé, le couple
//                  (celui envoyé, celui du registre) est jugé.
//
// Le registre pèse ≈ 2,3 Ko : l'appelant fournit les deux (`actuel`, `neuf`) dans des tampons en PSRAM, jamais sur la pile d'une tâche.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/mer_spots.hpp"

namespace ecran::mer {

// Les sept refus de l'API (les identifiants stables sont dans api_reglages.hpp).
enum class ErreurApi : std::uint8_t {
    Aucune,
    Spots,       // `spots` n'est pas une liste d'objets
    Identifiant, // un `id` absent, pas du texte, ou pas un « slug » de Vigie
    Nom,         // un `nom` absent, pas du texte, vide, trop long ou pas du texte net
    Doublon,     // deux objets de même `id`
    Plein,       // plus de 12 objets
    Choix,       // `choisi` n'est pas dans la liste
    Vent,        // plage de vent refusée, ou pas en nœuds entiers
};

// Le texte d'un identifiant ou d'un nom du registre, sans le zéro final (un tableau abîmé sans zéro rend son contenu entier).
constexpr std::string_view id_du_spot(const SpotMer &s) noexcept {
    return std::string_view(s.id, detail::longueur_bornee(s.id, sizeof s.id));
}
constexpr std::string_view nom_du_spot(const SpotMer &s) noexcept {
    return std::string_view(s.nom, detail::longueur_bornee(s.nom, sizeof s.nom));
}

// Indice du spot d'identifiant `id` dans la liste, ou `r.nombre` s'il n'y est pas.
constexpr std::size_t indice_du_spot(const Registre &r, std::string_view id) noexcept {
    const std::size_t n = r.nombre < REGISTRE_MAX ? r.nombre : REGISTRE_MAX;
    for (std::size_t i = 0; i < n; ++i) {
        if (id_du_spot(r.spots[i]) == id)
            return i;
    }
    return r.nombre;
}

namespace detail {

// Un texte JSON lu dans `tampon` (zéro final compris). Faux si ce n'est pas une chaîne ou si elle ne tient pas.
constexpr bool lire_texte(const json::Valeur &v, char *tampon, std::size_t capacite, std::string_view &texte) noexcept {
    if (v.genre() != json::Genre::Chaine)
        return false;
    bool tronque = false;
    const std::size_t n = v.copier_texte(tampon, capacite, &tronque);
    if (tronque)
        return false;
    // Sur la longueur écrite (le décodeur JSON ne produit jamais d'octet nul : « \u0000 » devient U+FFFD).
    texte = std::string_view(tampon, n);
    return true;
}

constexpr ErreurApi appliquer(const json::Valeur &corps, const Registre &actuel, Registre &neuf) noexcept {
    // --- spots : la liste entière ---
    const json::Valeur spots = corps.champ("spots");
    if (spots.existe()) {
        if (spots.genre() != json::Genre::Tableau)
            return ErreurApi::Spots;
        const std::size_t n = spots.taille();
        if (n > REGISTRE_MAX)
            return ErreurApi::Plein;
        for (std::size_t i = 0; i < REGISTRE_MAX; ++i)
            neuf.spots[i] = SpotMer{};
        neuf.nombre = 0;
        neuf.choisi = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const json::Valeur element = spots.element(i);
            if (element.genre() != json::Genre::Objet)
                return ErreurApi::Spots;
            // Un octet de plus que le registre : un texte trop long pour lui ne tient pas dans le tampon sans être coupé, et c'est
            // `ajouter` qui le refuse ; au-delà, la lecture le dit tronqué.
            char id[ID_OCTETS + 1] = {};
            char nom[NOM_OCTETS + 1] = {};
            std::string_view texte_id;
            std::string_view texte_nom;
            if (!lire_texte(element.champ("id"), id, sizeof id, texte_id))
                return ErreurApi::Identifiant;
            if (!lire_texte(element.champ("nom"), nom, sizeof nom, texte_nom))
                return ErreurApi::Nom;
            switch (ajouter(neuf, texte_id, texte_nom)) {
            case ErreurRegistre::Aucune:
                break;
            case ErreurRegistre::Plein:
                return ErreurApi::Plein;
            case ErreurRegistre::Doublon:
                return ErreurApi::Doublon;
            case ErreurRegistre::Nom:
                return ErreurApi::Nom;
            case ErreurRegistre::Identifiant:
            case ErreurRegistre::Indice:
            case ErreurRegistre::Vent:
                return ErreurApi::Identifiant;
            }
        }
        // Sans `choisi` dans l'envoi, l'appareil reste sur le même spot s'il est toujours là (`ajouter` a mis le choix sur le premier).
        if (actuel.nombre > 0 && actuel.nombre <= REGISTRE_MAX && actuel.choisi < actuel.nombre) {
            const std::size_t p = indice_du_spot(neuf, id_du_spot(actuel.spots[actuel.choisi]));
            if (p < neuf.nombre)
                neuf.choisi = static_cast<std::uint8_t>(p);
        }
    }

    // --- choisi ---
    const json::Valeur choisi = corps.champ("choisi");
    if (choisi.existe()) {
        char id[ID_OCTETS + 1] = {};
        std::string_view texte;
        if (!lire_texte(choisi, id, sizeof id, texte))
            return ErreurApi::Choix;
        if (neuf.nombre == 0) {
            if (!texte.empty())
                return ErreurApi::Choix;
            neuf.choisi = 0;
        } else {
            const std::size_t p = indice_du_spot(neuf, texte);
            if (p >= neuf.nombre)
                return ErreurApi::Choix;
            neuf.choisi = static_cast<std::uint8_t>(p);
        }
    }

    // --- plage de vent ---
    const json::Valeur vent_min = corps.champ("vent_min_kn");
    const json::Valeur vent_max = corps.champ("vent_max_kn");
    if (vent_min.existe() || vent_max.existe()) {
        std::int64_t mini = neuf.vent_min_kn;
        std::int64_t maxi = neuf.vent_max_kn;
        if (vent_min.existe() && !vent_min.en_entier(mini))
            return ErreurApi::Vent;
        if (vent_max.existe() && !vent_max.en_entier(maxi))
            return ErreurApi::Vent;
        if (regler_vent(neuf, mini, maxi) != ErreurRegistre::Aucune)
            return ErreurApi::Vent;
    }
    return ErreurApi::Aucune;
}

} // namespace detail

// Applique `corps` (un objet JSON) à `actuel` et écrit le résultat dans `neuf`. Tout ou rien : si une valeur est refusée, `neuf` est
// `actuel` et l'erreur dit laquelle. `neuf` et `actuel` sont deux registres distincts.
constexpr ErreurApi valider_mer(const json::Valeur &corps, const Registre &actuel, Registre &neuf) noexcept {
    neuf = actuel;
    const ErreurApi e = detail::appliquer(corps, actuel, neuf);
    if (e != ErreurApi::Aucune)
        neuf = actuel;
    return e;
}

} // namespace ecran::mer
