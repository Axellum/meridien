// Noyau pur : la liste des spots de Vigie lue PAR L'APPAREIL, et la recherche dedans (ADR-0026). C++17 standard, aucun include ESP-IDF,
// évaluable à la compilation.
//
// Pourquoi l'appareil la lit lui-même : Axel veut régler la mer sur l'écran, sans téléphone (08/11/2026, « il faut mettre le réglage sur
// l'écran en direct »). La page de réglages (ADR-0025) lisait déjà cette liste dans le navigateur ; l'écran n'a pas de navigateur, donc il
// la télécharge (`GET <adresse>/v1/surf/spots`, 51 spots, 20 405 octets bruts et 4 695 gzippés le 08/11/2026) avec le client HTTPS commun.
//
// Ce que ce fichier fait : lire la réponse (`analyser`), en garder ce qu'il faut pour choisir (identifiant, nom, lieu) et chercher
// dedans (`chercher`) sans tenir compte des majuscules ni des accents. Les règles de chaque champ sont celles de la page
// (web/app/app.js, `lireSpotsVigie`) : identifiant = un « slug » de Vigie, jamais deux fois le même ; nom = le premier de `name`,
// `osm_name`, `place` qui est un texte net, sinon l'identifiant ; recherche = le texte tapé se trouve dans le nom, le lieu ou
// l'identifiant. Rien n'est supposé de plus que ces champs : les autres (position, orientation, preuves) sont ignorés.
//
// Une réponse de serveur n'est jamais crue : un élément illisible est écarté (les autres sont gardés), une réponse qui n'est pas un objet
// avec une liste `spots` est refusée, et au-delà de SPOTS_MAX spots les suivants sont ignorés (la liste dit qu'elle est tronquée).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/mer_api.hpp"
#include "ecran/mer_spots.hpp"
#include "ecran/surf_spot.hpp"
#include "ecran/vigie_requete.hpp"

namespace ecran::mer::liste {

inline constexpr std::size_t SPOTS_MAX = 64; // Vigie en a 51 (08/11/2026) ; le reste est ignoré
inline constexpr std::size_t LIEU_OCTETS =
    65; // un lieu fait 32 caractères et 64 octets au plus (frontière de caractère), plus le zéro
// Le plus long texte à replier pour la recherche : le nom (129 octets avec le zéro final) est le plus grand des trois champs.
inline constexpr std::size_t REPLI_OCTETS = NOM_OCTETS;
// Le plus long texte tapé pris en compte : au-delà, rien ne correspond (le clavier de l'écran en laisse taper davantage).
inline constexpr std::size_t RECHERCHE_OCTETS = 96;

struct SpotVigie {
    char id[ID_OCTETS] = {};     // l'identifiant dans la liste de Vigie (« slug »)
    char nom[NOM_OCTETS] = {};   // le nom à garder : 32 caractères au plus, ce que le registre acceptera tel quel
    char lieu[LIEU_OCTETS] = {}; // la commune (`place`), vide si Vigie n'en donne pas
};

struct Liste {
    SpotVigie spots[SPOTS_MAX] = {};
    std::uint8_t nombre = 0;
    bool tronquee = false; // la réponse avait plus de SPOTS_MAX spots utilisables
};

enum class Issue : std::uint8_t {
    Ok,
    Illisible, // pas du JSON, pas un objet, ou pas de liste `spots`
    Vide,      // une liste, mais aucun spot utilisable
};

namespace detail {

// Table de repli des lettres accentuées U+00C0 à U+017F vers leurs lettres de base en minuscules (« é » → « e », « ß » → « ss »,
// « ş » → « s »), établie le 08/11/2026 par Python (unicodedata, NFD) puis relue. Un signe sans lettre (« × », « ÷ ») vaut le vide.
// clang-format off
inline constexpr const char *REPLI[192] = {
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i", "d", "n", "o", "o", "o", "o",
    "o", "", "o", "u", "u", "u", "u", "y", "th", "ss", "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e",
    "i", "i", "i", "i", "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u", "u", "y", "th", "y", "a", "a",
    "a", "a", "a", "a", "c", "c", "c", "c", "c", "c", "c", "c", "d", "d", "d", "d", "e", "e", "e", "e", "e", "e",
    "e", "e", "e", "e", "g", "g", "g", "g", "g", "g", "g", "g", "h", "h", "h", "h", "i", "i", "i", "i", "i", "i",
    "i", "i", "i", "i", "ij", "ij", "j", "j", "k", "k", "k", "l", "l", "l", "l", "l", "l", "", "", "l", "l", "n",
    "n", "n", "n", "n", "n", "n", "n", "n", "o", "o", "o", "o", "o", "o", "oe", "oe", "r", "r", "r", "r", "r", "r",
    "s", "s", "s", "s", "s", "s", "s", "s", "t", "t", "t", "t", "t", "t", "u", "u", "u", "u", "u", "u", "u", "u",
    "u", "u", "u", "u", "w", "w", "y", "y", "y", "z", "z", "z", "z", "z", "z", "s",
};
// clang-format on

constexpr bool est_espace(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// Combien d'octets fait le caractère UTF-8 qui commence par `c` (1 pour un octet invalide : il est traité tel quel).
constexpr std::size_t taille_caractere(unsigned char c) noexcept {
    if (c >= 0xF0 && c <= 0xF4)
        return 4;
    if (c >= 0xE0)
        return c <= 0xEF ? 3 : 1;
    if (c >= 0xC2)
        return 2;
    return 1;
}

// Le texte sans ses espaces de début et de fin.
constexpr std::string_view rogner(std::string_view t) noexcept {
    while (!t.empty() && est_espace(t.front()))
        t.remove_prefix(1);
    while (!t.empty() && est_espace(t.back()))
        t.remove_suffix(1);
    return t;
}

// Les `max_caracteres` premiers caractères de `t`, et pas plus de `max_octets` octets, coupés sur une frontière de caractère.
constexpr std::string_view couper(std::string_view t, std::size_t max_caracteres, std::size_t max_octets) noexcept {
    std::size_t octets = 0;
    std::size_t caracteres = 0;
    while (octets < t.size() && caracteres < max_caracteres) {
        const std::size_t n = taille_caractere(static_cast<unsigned char>(t[octets]));
        if (octets + n > t.size() || octets + n > max_octets)
            break;
        octets += n;
        ++caracteres;
    }
    return rogner(t.substr(0, octets));
}

// Un texte JSON lu dans `tampon` (zéro final compris), coupé si besoin : faux si ce n'est pas une chaîne (un `null` aussi).
constexpr bool lire_chaine(const json::Valeur &v, char *tampon, std::size_t capacite,
                           std::string_view &texte) noexcept {
    if (v.genre() != json::Genre::Chaine)
        return false;
    const std::size_t n = v.copier_texte(tampon, capacite, nullptr);
    texte = rogner(std::string_view(tampon, n));
    return true;
}

} // namespace detail

// Écrit dans `sortie` le texte replié : minuscules ASCII, lettres accentuées ramenées à leur lettre de base. Les autres caractères
// (chiffres, tirets, lettres hors du latin) passent tels quels. Le résultat n'est jamais plus long que le texte (au plus deux octets
// pour deux octets) ; rend faux, sortie inutilisable, si `capacite` ne suffit pas (zéro final compris).
constexpr bool replier(std::string_view texte, char *sortie, std::size_t capacite, std::size_t &longueur) noexcept {
    std::size_t n = 0;
    std::size_t i = 0;
    while (i < texte.size()) {
        const unsigned char c = static_cast<unsigned char>(texte[i]);
        const char *remplacement = nullptr;
        std::size_t lu = 1;
        char ascii[2] = {static_cast<char>(c), '\0'};
        if (c >= 'A' && c <= 'Z') {
            ascii[0] = static_cast<char>(c - 'A' + 'a');
            remplacement = ascii;
        } else if (c == 0xC3 && i + 1 < texte.size()) { // U+00C0 à U+00FF
            const unsigned char d = static_cast<unsigned char>(texte[i + 1]);
            if (d >= 0x80 && d <= 0xBF) {
                remplacement = detail::REPLI[d - 0x80];
                lu = 2;
            }
        } else if ((c == 0xC4 || c == 0xC5) && i + 1 < texte.size()) { // U+0100 à U+017F
            const unsigned char d = static_cast<unsigned char>(texte[i + 1]);
            if (d >= 0x80 && d <= 0xBF) {
                remplacement = detail::REPLI[(c == 0xC4 ? 0x40 : 0x80) + (d - 0x80)];
                lu = 2;
            }
        }
        if (remplacement != nullptr) {
            for (std::size_t k = 0; remplacement[k] != '\0'; ++k) {
                if (n + 1 >= capacite)
                    return false;
                sortie[n++] = remplacement[k];
            }
        } else {
            for (std::size_t k = 0; k < lu; ++k) {
                if (n + 1 >= capacite)
                    return false;
                sortie[n++] = texte[i + k];
            }
        }
        i += lu;
    }
    if (n >= capacite)
        return false;
    sortie[n] = '\0';
    longueur = n;
    return true;
}

// Vrai si `texte` (replié à l'instant) contient `aiguille`, DÉJÀ repliée. Une aiguille vide est dans tout texte.
constexpr bool contient_replie(std::string_view texte, std::string_view aiguille) noexcept {
    if (aiguille.empty())
        return true;
    char tampon[REPLI_OCTETS] = {};
    std::size_t n = 0;
    if (!replier(texte, tampon, sizeof tampon, n))
        return false; // un texte trop long pour le tampon ne peut pas être celui d'un spot lu par `analyser`
    return std::string_view(tampon, n).find(aiguille) != std::string_view::npos;
}

// ---- lire la réponse de Vigie --------------------------------------------------------------------------------------------------------

constexpr std::string_view id_du_spot(const SpotVigie &s) noexcept {
    return std::string_view(s.id, mer::detail::longueur_bornee(s.id, sizeof s.id));
}
constexpr std::string_view nom_du_spot(const SpotVigie &s) noexcept {
    return std::string_view(s.nom, mer::detail::longueur_bornee(s.nom, sizeof s.nom));
}
constexpr std::string_view lieu_du_spot(const SpotVigie &s) noexcept {
    return std::string_view(s.lieu, mer::detail::longueur_bornee(s.lieu, sizeof s.lieu));
}

namespace detail {

constexpr bool deja_vu(const Liste &l, std::string_view id) noexcept {
    for (std::size_t i = 0; i < l.nombre; ++i) {
        if (id_du_spot(l.spots[i]) == id)
            return true;
    }
    return false;
}

// Remplit `s` depuis un élément de la réponse ; faux si l'élément n'est pas utilisable (il est alors écarté, sans effet sur les autres).
constexpr bool lire_spot(const json::Valeur &element, const Liste &deja, SpotVigie &s) noexcept {
    if (element.genre() != json::Genre::Objet)
        return false;
    char id[ID_OCTETS + 1] = {};
    std::string_view texte_id;
    if (!lire_chaine(element.champ("id"), id, sizeof id, texte_id))
        return false;
    if (texte_id.size() > SPOT_ID_MAX || !spot_valide(texte_id) || deja_vu(deja, texte_id))
        return false;
    // Le lieu : `place`, coupé à 32 caractères et 64 octets (un lieu illisible vaut « pas de lieu », il n'écarte pas le spot).
    char brut_lieu[4 * LIEU_OCTETS] = {};
    std::string_view lieu;
    if (!lire_chaine(element.champ("place"), brut_lieu, sizeof brut_lieu, lieu))
        lieu = std::string_view();
    lieu = couper(lieu, surf::SPOT_NOM_MAX_CARACTERES, LIEU_OCTETS - 1);
    // Le nom : le premier de `name`, `osm_name`, `place` qui est un texte net de 32 caractères au plus après la coupe, sinon l'identifiant.
    const char *champs[3] = {"name", "osm_name", "place"};
    char brut_nom[4 * NOM_OCTETS] = {};
    std::string_view nom;
    for (const char *champ : champs) {
        std::string_view candidat;
        if (!lire_chaine(element.champ(champ), brut_nom, sizeof brut_nom, candidat))
            continue;
        candidat = couper(candidat, surf::SPOT_NOM_MAX_CARACTERES, NOM_OCTETS - 1);
        char essai[NOM_OCTETS] = {};
        if (candidat.empty() || !mer::detail::copier(candidat, essai, sizeof essai) ||
            !surf::nom_de_spot_valide(essai, sizeof essai))
            continue;
        nom = candidat;
        break;
    }
    if (nom.empty())
        nom = couper(texte_id, surf::SPOT_NOM_MAX_CARACTERES, NOM_OCTETS - 1);
    s = SpotVigie{};
    if (!mer::detail::copier(texte_id, s.id, sizeof s.id) || !mer::detail::copier(nom, s.nom, sizeof s.nom) ||
        !surf::nom_de_spot_valide(s.nom, sizeof s.nom))
        return false;
    // Un lieu qui n'est pas du texte net est laissé vide plutôt que d'écarter le spot.
    if (!mer::detail::copier(lieu, s.lieu, sizeof s.lieu) || !surf::nom_de_spot_valide(s.lieu, sizeof s.lieu))
        s.lieu[0] = '\0';
    return true;
}

} // namespace detail

// Lit la réponse de `GET /v1/surf/spots`. `sortie` est remplie (et n'a de sens que si le résultat est `Ok`).
constexpr Issue analyser(std::string_view texte, Liste &sortie) noexcept {
    sortie.nombre = 0;
    sortie.tronquee = false;
    const json::Resultat lu = json::lire(texte);
    if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet)
        return Issue::Illisible;
    const json::Valeur spots = lu.valeur.champ("spots");
    if (spots.genre() != json::Genre::Tableau)
        return Issue::Illisible;
    const std::size_t n = spots.taille();
    for (std::size_t i = 0; i < n; ++i) {
        SpotVigie s;
        if (!detail::lire_spot(spots.element(i), sortie, s))
            continue;
        if (sortie.nombre >= SPOTS_MAX) {
            sortie.tronquee = true;
            break;
        }
        sortie.spots[sortie.nombre] = s;
        ++sortie.nombre;
    }
    return sortie.nombre == 0 ? Issue::Vide : Issue::Ok;
}

// ---- la requête et le rythme ------------------------------------------------------------------------------------------------------
//
// Route : GET <adresse>/v1/surf/spots (contrat : vigie_api, route « surf/spots », relue le 08/11/2026 : 51 spots, 20 405 octets bruts et
// 4 695 gzippés). Sans paramètre : ni position de l'appareil ni rien de personnel ne part vers Vigie. L'adresse de base est celle que
// vigie::normaliser_adresse a rendue (ou l'instance publique), la même que pour la trame de la mer.

inline constexpr std::string_view REQUETE_LISTE = "/v1/surf/spots";
inline constexpr std::size_t TAILLE_REQUETE_LISTE_MAX = vigie::ADRESSE_MAX + REQUETE_LISTE.size() + 1;

// Écrit l'adresse complète dans `sortie` (zéro final compris) et rend sa longueur sans le zéro. Rend 0, sans rien garantir sur `sortie`,
// si l'adresse est vide ou trop longue, ou si `capacite` est trop petite.
constexpr std::size_t construire_requete_liste(std::string_view adresse, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0 || adresse.empty() || adresse.size() > vigie::ADRESSE_MAX)
        return 0;
    vigie::detail::Sortie s{sortie, capacite};
    s.texte(adresse);
    s.texte(REQUETE_LISTE);
    if (s.trop)
        return 0;
    sortie[s.n] = '\0';
    return s.n;
}

// Politesse envers l'instance publique (limitée par adresse IP) : la liste ne change presque jamais (Vigie la reconstruit avec ses
// données), donc une liste lue il y a moins de 10 minutes est gardée telle quelle quand la personne rouvre « Ajouter un spot » ; et
// après un échec on attend quelques secondes avant de réessayer (un bouton « Réessayer » tapé en rafale ne martèle pas le serveur).
inline constexpr std::int64_t LISTE_VALABLE_S = 10 * 60;
inline constexpr std::int64_t PAUSE_APRES_ECHEC_S = 5;

// Faut-il (re)télécharger la liste ? `age_liste_s` : depuis combien de temps la liste gardée a été lue (sans objet si `liste_gardee` est
// faux) ; `dernier_echec` : le dernier essai a échoué ; `age_essai_s` : depuis combien de temps il a fini.
constexpr bool relire_la_liste(bool liste_gardee, std::int64_t age_liste_s, bool dernier_echec,
                               std::int64_t age_essai_s) noexcept {
    if (liste_gardee && age_liste_s >= 0 && age_liste_s < LISTE_VALABLE_S)
        return false;
    if (dernier_echec && age_essai_s >= 0 && age_essai_s < PAUSE_APRES_ECHEC_S)
        return false;
    return true;
}

// ---- chercher ------------------------------------------------------------------------------------------------------------------------

// Écrit dans `indices` les rangs (dans la liste, dans son ordre) des spots dont le nom, le lieu ou l'identifiant contient `requete`,
// sans tenir compte des majuscules ni des accents. Une requête vide donne tous les spots. Au plus `max` rangs ; rend leur nombre. Une
// requête dont le texte replié dépasse RECHERCHE_OCTETS octets ne correspond à rien (aucun nom, lieu ou identifiant n'est si long).
constexpr std::size_t chercher(const Liste &l, std::string_view requete, std::uint8_t *indices,
                               std::size_t max) noexcept {
    char aiguille[RECHERCHE_OCTETS + 1] = {};
    std::size_t taille_aiguille = 0;
    requete = detail::rogner(requete);
    if (!replier(requete, aiguille, sizeof aiguille, taille_aiguille))
        return 0;
    const std::string_view cherche(aiguille, taille_aiguille);
    std::size_t trouves = 0;
    const std::size_t n = l.nombre < SPOTS_MAX ? l.nombre : SPOTS_MAX;
    for (std::size_t i = 0; i < n && trouves < max; ++i) {
        const SpotVigie &s = l.spots[i];
        if (contient_replie(nom_du_spot(s), cherche) || contient_replie(lieu_du_spot(s), cherche) ||
            contient_replie(id_du_spot(s), cherche))
            indices[trouves++] = static_cast<std::uint8_t>(i);
    }
    return trouves;
}

// ---- ce que fait un toucher sur un résultat -------------------------------------------------------------------------------------------

// Ce spot est-il déjà dans le registre ?
constexpr bool dans_le_registre(const Registre &r, const SpotVigie &s) noexcept {
    return indice_du_spot(r, id_du_spot(s)) < r.nombre;
}

// Ajoute le spot au registre avec le nom qu'il a dans la liste ; le premier spot ajouté devient le spot choisi (mer::ajouter).
constexpr ErreurRegistre ajouter_de_la_liste(Registre &r, const SpotVigie &s) noexcept {
    return ajouter(r, id_du_spot(s), nom_du_spot(s));
}

} // namespace ecran::mer::liste
