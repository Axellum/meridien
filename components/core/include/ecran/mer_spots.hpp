// Noyau pur : le registre des spots de la page mer. C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
//
// Le registre garde au plus 12 spots (le même plafond que les spots de surf saisis à la main, surf_spot.hpp) pris dans la liste de Vigie
// (`GET /v1/surf/spots` : 51 spots des côtes atlantique et de la Manche, relus dans vigie-api le 08/11/2026) ; chacun est un
// identifiant de Vigie et un nom à afficher. UN SEUL est « choisi » : c'est le seul dont l'appareil demande la mer (une requête par
// période, jamais douze). Les autres attendent dans la liste que la personne en change (page de réglages ou roue : lots suivants).
//
// Registre vide = aucune requête à Vigie (c'est l'état d'un appareil neuf : la page mer n'ajoute aucun trafic tant qu'on n'a rien
// choisi). La plage de vent du kite (nœuds) fait partie du registre : elle part dans la même requête (mer_requete.hpp).
//
// Les opérations (ajouter, retirer, choisir, regler_vent) modifient le registre EN PLACE et rendent l'erreur qui dit LAQUELLE (jamais un
// refus muet) ; à la première erreur le registre est resté tel qu'il était (tout ou rien, comme valider_alertes). En place parce qu'un
// registre pèse ≈ 2,3 Ko : en rendre un par valeur ferait deux ou trois copies sur la pile d'une tâche de 4 à 12 Ko. Ce que la NVS rend
// est repris par `registre_depuis_nvs` : une entrée abîmée est écartée, le reste est gardé (un NVS abîmé ne doit pas vider la liste
// entière) ; elle rend un registre par valeur, à n'appeler qu'au démarrage ou depuis un tampon en PSRAM.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_requete.hpp"
#include "ecran/surf_spot.hpp"

namespace ecran::mer {

inline constexpr std::size_t REGISTRE_MAX = 12;
inline constexpr std::size_t ID_OCTETS = SPOT_ID_MAX + 1;        // identifiant et zéro final
inline constexpr std::size_t NOM_OCTETS = surf::SPOT_NOM_OCTETS; // 32 caractères de 4 octets au plus et le zéro final

struct SpotMer {
    char id[ID_OCTETS] = {};   // l'identifiant dans la liste de Vigie (« slug »)
    char nom[NOM_OCTETS] = {}; // le nom affiché (UTF-8, 1 à 32 caractères)
};

struct Registre {
    SpotMer spots[REGISTRE_MAX] = {};
    std::uint8_t nombre = 0; // spots gardés
    std::uint8_t choisi = 0; // indice du spot dont on demande la mer ; sans objet si `nombre` vaut 0
    std::int16_t vent_min_kn = VENT_MIN_DEFAUT_NOEUDS;
    std::int16_t vent_max_kn = VENT_MAX_DEFAUT_NOEUDS;
};

enum class ErreurRegistre : std::uint8_t {
    Aucune,
    Plein,       // déjà 12 spots
    Doublon,     // cet identifiant est déjà dans la liste
    Identifiant, // pas un « slug » de Vigie (a-z, 0-9, « - », « _ », 64 octets au plus)
    Nom,         // nom vide, trop long, ou pas du texte UTF-8 net (surf::nom_de_spot_valide)
    Indice,      // pas de spot à cet indice
    Vent,        // plage de vent refusée (0 < min < max <= 60 nœuds)
};

namespace detail {

// Longueur d'un texte rangé dans un tableau de `capacite` octets, ou `capacite` s'il n'a pas de zéro final (tableau abîmé).
constexpr std::size_t longueur_bornee(const char *t, std::size_t capacite) noexcept {
    std::size_t n = 0;
    while (n < capacite && t[n] != '\0')
        ++n;
    return n;
}

// Même texte dans les deux tableaux (jusqu'au zéro) ; l'un d'eux sans zéro final n'est égal à rien.
constexpr bool memes_textes(const char *a, std::size_t cap_a, const char *b, std::size_t cap_b) noexcept {
    const std::size_t la = longueur_bornee(a, cap_a);
    const std::size_t lb = longueur_bornee(b, cap_b);
    if (la == cap_a || lb == cap_b || la != lb)
        return false;
    for (std::size_t i = 0; i < la; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

constexpr bool meme_id(const SpotMer &a, const SpotMer &b) noexcept {
    return memes_textes(a.id, sizeof a.id, b.id, sizeof b.id);
}

// Un spot lu dans un tableau déjà rempli (NVS) : l'identifiant et le nom sont-ils utilisables ? Un identifiant sans zéro final fait 65
// octets ou plus : `spot_valide` le refuse ; un nom sans zéro final est refusé par `nom_de_spot_valide` (zéro obligatoire dans le
// tableau).
constexpr bool spot_utilisable(const SpotMer &s) noexcept {
    return spot_valide(std::string_view(s.id, longueur_bornee(s.id, sizeof s.id))) &&
           surf::nom_de_spot_valide(s.nom, sizeof s.nom);
}

// Copie `texte` dans un tableau de `capacite` octets, zéro final compris et octets suivants à zéro. Faux, sans rien écrire, s'il ne tient
// pas ou s'il contient un octet nul.
constexpr bool copier(std::string_view texte, char *dest, std::size_t capacite) noexcept {
    if (texte.size() + 1 > capacite)
        return false;
    for (const char c : texte) {
        if (c == '\0')
            return false;
    }
    for (std::size_t i = 0; i < capacite; ++i)
        dest[i] = i < texte.size() ? texte[i] : '\0';
    return true;
}

} // namespace detail

// Le spot dont on demande la mer, ou nullptr si le registre est vide (ou `choisi` hors de la liste : un registre abîmé).
constexpr const SpotMer *spot_choisi(const Registre &r) noexcept {
    return (r.nombre <= REGISTRE_MAX && r.choisi < r.nombre) ? &r.spots[r.choisi] : nullptr;
}

// Deux registres disent-ils la même chose ? Les entrées au-delà de `nombre` ne comptent pas.
constexpr bool egaux(const Registre &a, const Registre &b) noexcept {
    if (a.nombre != b.nombre || a.choisi != b.choisi || a.vent_min_kn != b.vent_min_kn ||
        a.vent_max_kn != b.vent_max_kn)
        return false;
    if (a.nombre > REGISTRE_MAX)
        return false;
    for (std::size_t i = 0; i < a.nombre; ++i) {
        if (!detail::meme_id(a.spots[i], b.spots[i]) ||
            !detail::memes_textes(a.spots[i].nom, sizeof a.spots[i].nom, b.spots[i].nom, sizeof b.spots[i].nom))
            return false;
    }
    return true;
}

// ---- ce que le client demande --------------------------------------------------------------------------------------------------

// Ce que le client réseau demande à Vigie : le spot choisi (son identifiant seulement) et la plage de vent. Petite (≈ 70 octets) : elle se
// copie sur la pile d'une tâche, contrairement au registre entier.
struct DemandeMer {
    bool presente = false; // faux : registre vide, rien à demander
    char spot[ID_OCTETS] = {};
    int vent_min_kn = VENT_MIN_DEFAUT_NOEUDS;
    int vent_max_kn = VENT_MAX_DEFAUT_NOEUDS;
};

constexpr DemandeMer demande_du_registre(const Registre &r) noexcept {
    DemandeMer d;
    const SpotMer *s = spot_choisi(r);
    if (s == nullptr)
        return d;
    d.presente = true;
    for (std::size_t i = 0; i < ID_OCTETS; ++i)
        d.spot[i] = s->id[i];
    d.spot[ID_OCTETS - 1] = '\0';
    d.vent_min_kn = r.vent_min_kn;
    d.vent_max_kn = r.vent_max_kn;
    return d;
}

// La mer qu'on a déjà ne vaut-elle encore que pour la nouvelle demande ? Vrai si la même mer serait demandée : même spot (ou aucun des
// deux) et même plage de vent. Les autres spots de la liste, leur nom et l'ordre n'y sont pour rien (ils ne sont pas dans la demande).
// Sert à décider s'il faut oublier la mer en mémoire et redemander.
constexpr bool meme_demande(const DemandeMer &a, const DemandeMer &b) noexcept {
    if (a.presente != b.presente)
        return false;
    if (!a.presente)
        return true; // rien n'est demandé dans les deux cas, le vent n'y change rien
    return detail::memes_textes(a.spot, sizeof a.spot, b.spot, sizeof b.spot) && a.vent_min_kn == b.vent_min_kn &&
           a.vent_max_kn == b.vent_max_kn;
}

// Écrit dans `sortie` la requête de cette demande (zéro final compris) et rend sa longueur sans le zéro. Rend 0 si rien n'est à demander
// ou si la requête n'a pas pu être construite (voir mer::construire_requete).
constexpr std::size_t requete_de_la_demande(std::string_view adresse, const DemandeMer &d, char *sortie,
                                            std::size_t capacite) noexcept {
    if (!d.presente)
        return 0;
    return construire_requete(adresse, std::string_view(d.spot, detail::longueur_bornee(d.spot, sizeof d.spot)),
                              d.vent_min_kn, d.vent_max_kn, sortie, capacite);
}

// ---- les opérations ------------------------------------------------------------------------------------------------------------

// Ajoute un spot en fin de liste. Le premier spot ajouté devient le spot choisi ; ensuite le choix ne bouge pas.
constexpr ErreurRegistre ajouter(Registre &r, std::string_view id, std::string_view nom) noexcept {
    if (r.nombre >= REGISTRE_MAX)
        return ErreurRegistre::Plein;
    SpotMer neuf;
    if (!spot_valide(id) || !detail::copier(id, neuf.id, sizeof neuf.id))
        return ErreurRegistre::Identifiant;
    if (!detail::copier(nom, neuf.nom, sizeof neuf.nom) || !surf::nom_de_spot_valide(neuf.nom, sizeof neuf.nom))
        return ErreurRegistre::Nom;
    for (std::size_t i = 0; i < r.nombre; ++i) {
        if (detail::meme_id(r.spots[i], neuf))
            return ErreurRegistre::Doublon;
    }
    if (r.nombre == 0)
        r.choisi = 0;
    r.spots[r.nombre] = neuf;
    ++r.nombre;
    return ErreurRegistre::Aucune;
}

// Retire le spot d'indice `indice`. Si c'était le spot choisi, le choix passe au spot qui prend sa place dans la liste (le suivant, ou
// le dernier s'il était le dernier) ; si la liste devient vide, plus rien n'est demandé. Un spot avant le choix qui part décale le
// choix d'un cran pour qu'il reste sur le même spot.
constexpr ErreurRegistre retirer(Registre &r, std::size_t indice) noexcept {
    if (r.nombre > REGISTRE_MAX || indice >= r.nombre)
        return ErreurRegistre::Indice;
    for (std::size_t i = indice; i + 1 < r.nombre; ++i)
        r.spots[i] = r.spots[i + 1];
    // La case libérée est remise à zéro : deux registres égaux sont égaux octet pour octet.
    r.spots[r.nombre - 1] = SpotMer{};
    --r.nombre;
    if (r.nombre == 0) {
        r.choisi = 0;
    } else if (indice < r.choisi) {
        --r.choisi;
    } else if (r.choisi >= r.nombre) {
        r.choisi = static_cast<std::uint8_t>(r.nombre - 1);
    }
    return ErreurRegistre::Aucune;
}

// Choisit le spot d'indice `indice`.
constexpr ErreurRegistre choisir(Registre &r, std::size_t indice) noexcept {
    if (r.nombre > REGISTRE_MAX || indice >= r.nombre)
        return ErreurRegistre::Indice;
    r.choisi = static_cast<std::uint8_t>(indice);
    return ErreurRegistre::Aucune;
}

// Règle la plage de vent du kite, en nœuds.
constexpr ErreurRegistre regler_vent(Registre &r, std::int64_t min_noeuds, std::int64_t max_noeuds) noexcept {
    if (!plage_de_vent_valide(min_noeuds, max_noeuds))
        return ErreurRegistre::Vent;
    r.vent_min_kn = static_cast<std::int16_t>(min_noeuds);
    r.vent_max_kn = static_cast<std::int16_t>(max_noeuds);
    return ErreurRegistre::Aucune;
}

// ---- la NVS --------------------------------------------------------------------------------------------------------------------

// Ce que le noyau garde d'un registre relu en NVS (ou d'un registre construit à la main). Une entrée dont l'identifiant ou le nom n'est
// pas utilisable est écartée, ainsi qu'un doublon ; les autres gardent leur ordre. Le spot choisi reste le même spot s'il a survécu,
// sinon c'est le premier. Une plage de vent refusée retombe sur les valeurs par défaut de Vigie. Plus de 12 entrées annoncées : les 12
// premières seulement.
constexpr Registre registre_depuis_nvs(const Registre &brut) noexcept {
    Registre n;
    const std::size_t annonce = brut.nombre < REGISTRE_MAX ? brut.nombre : REGISTRE_MAX;
    for (std::size_t i = 0; i < annonce; ++i) {
        const SpotMer &s = brut.spots[i];
        if (!detail::spot_utilisable(s))
            continue;
        bool doublon = false;
        for (std::size_t j = 0; j < n.nombre; ++j) {
            if (detail::meme_id(n.spots[j], s)) {
                doublon = true;
                break;
            }
        }
        if (doublon)
            continue;
        // Copie propre : tout ce qui suit le zéro final est remis à zéro.
        const std::size_t li = detail::longueur_bornee(s.id, sizeof s.id);
        const std::size_t ln = detail::longueur_bornee(s.nom, sizeof s.nom);
        detail::copier(std::string_view(s.id, li), n.spots[n.nombre].id, sizeof n.spots[n.nombre].id);
        detail::copier(std::string_view(s.nom, ln), n.spots[n.nombre].nom, sizeof n.spots[n.nombre].nom);
        if (i == brut.choisi)
            n.choisi = n.nombre;
        ++n.nombre;
    }
    if (plage_de_vent_valide(brut.vent_min_kn, brut.vent_max_kn)) {
        n.vent_min_kn = brut.vent_min_kn;
        n.vent_max_kn = brut.vent_max_kn;
    }
    return n;
}

// Un registre est valide s'il est déjà tel que `registre_depuis_nvs` le rendrait : au plus 12 entrées toutes utilisables et sans
// doublon, un choix dans la liste (0 si elle est vide), une plage de vent acceptée. Sans copie : ≈ 2,3 Ko de moins sur la pile de l'appelant
// (le test test_mer_spots.cpp vérifie qu'elle donne le même verdict que `egaux(registre_depuis_nvs(r), r)`).
constexpr bool registre_valide(const Registre &r) noexcept {
    if (r.nombre > REGISTRE_MAX || r.choisi >= (r.nombre > 0 ? r.nombre : 1))
        return false;
    if (!plage_de_vent_valide(r.vent_min_kn, r.vent_max_kn))
        return false;
    for (std::size_t i = 0; i < r.nombre; ++i) {
        if (!detail::spot_utilisable(r.spots[i]))
            return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (detail::meme_id(r.spots[j], r.spots[i]))
                return false;
        }
    }
    return true;
}

} // namespace ecran::mer
