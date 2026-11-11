// Noyau pur : la recherche d'un lieu par son nom (géocodage d'Open-Meteo), sans réseau ni ESP-IDF.
//
// Trois morceaux, tous testés sur le PC avec de VRAIES réponses du service (test/host/donnees/geocodage_*.json) :
//   1. `analyser`  : réponse JSON d'Open-Meteo -> au plus 8 `Lieu` (une structure, pas du JSON). Les mêmes lieux servent à la page
//                    web (`ecrire_resultats` -> {"resultats":[...]} du contrat docs/api-reglages.md §5) et à l'écran de l'appareil.
//   2. `vers_zone` : un `Lieu` choisi -> la ville à enregistrer (`reglages::Zone`), avec les MÊMES règles que la page
//                    (`reglages::valider_zone`) : un lieu que l'écran accepte est un lieu que la page aurait accepté, et inversement.
//   3. `Recherche` : l'état d'une recherche demandée par l'écran (en cours / terminée / sans connexion), « la dernière demande
//                    gagne ». Pas de verrou ici (noyau pur). Plus appelé par le firmware depuis le 11/11/2026 : l'écran passe par
//                    HoteAppareil (ecran/hote_appareil.hpp, lieux_geocodage.hpp) et le téléchargement brut de main/geocodage.cpp.
//
// Règles de lecture (inchangées depuis la première version de la page, jalon « réglages ») : un lieu sans nom, sans fuseau, au nom
// qui n'est pas de l'UTF-8 valide ou contient un octet de contrôle, ou aux coordonnées hors plage est écarté ; une région ou un pays
// invalide est laissé vide ; au plus 8 lieux. AJOUT de ce jalon, vu sur une réponse réelle (« Istanbul Old Town », 06/11/2026) :
// Open-Meteo écrit -9999 quand l'altitude est inconnue ; une altitude hors de [-500 ; 9000] m n'est plus transmise.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/api_json.hpp"
#include "ecran/fuseaux.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/reglages_valeurs.hpp"

namespace ecran::geocodage {

inline constexpr std::size_t LIEUX_MAX = 8; // le service est interrogé avec count=8 (reglages::adresse_geocodage)

// Tailles des textes d'un lieu, zéro final compris.
inline constexpr std::size_t NOM_OCTETS =
    81; // 80 octets : assez pour la plupart des noms, coupés sur un caractère entier sinon
inline constexpr std::size_t REGION_OCTETS = 64;
inline constexpr std::size_t PAYS_OCTETS = 64;
inline constexpr std::size_t CODE_PAYS_OCTETS = 4;
inline constexpr std::size_t FUSEAU_OCTETS = 40;
inline constexpr std::size_t LIBELLE_OCTETS = 256; // « nom, région, pays » : au plus 80 + 63 + 63 + 4 octets

inline constexpr double ALTITUDE_MIN = -500.0; // m ; la mer Morte est à -430 m
inline constexpr double ALTITUDE_MAX = 9000.0; // m

constexpr bool altitude_plausible(double metres) noexcept { return metres >= ALTITUDE_MIN && metres <= ALTITUDE_MAX; }

struct Lieu {
    std::int64_t id = 0; // identifiant GeoNames
    bool id_connu = false;
    char nom[NOM_OCTETS]{};
    char region[REGION_OCTETS]{}; // « Auvergne-Rhône-Alpes » ; vide si le service n'en donne pas
    char pays[PAYS_OCTETS]{};
    char code_pays[CODE_PAYS_OCTETS]{}; // « FR »
    char fuseau[FUSEAU_OCTETS]{};       // « Europe/Paris »
    double latitude = 0;                // degrés, -90 à 90
    double longitude = 0;               // degrés, -180 à 180
    double altitude = 0;                // mètres
    bool altitude_connue = false;
    std::int64_t population = 0;
    bool population_connue = false;
    char libelle[LIBELLE_OCTETS]{}; // « Lyon, Auvergne-Rhône-Alpes, France » : ce que l'écran et la page affichent
};

// Écrit « a, b, c » : les parties non vides, séparées par « , ».
constexpr void ecrire_libelle(api::Ecrivain &e, std::string_view a, std::string_view b, std::string_view c) noexcept {
    bool premier = true;
    for (std::string_view p : {a, b, c}) {
        if (p.empty())
            continue;
        if (!premier)
            e.brut(", ");
        e.brut(p);
        premier = false;
    }
}

namespace detail {

// Un lieu de la réponse. Rend faux (le lieu est écarté) si un champ obligatoire manque ou est invalide.
constexpr bool lire_lieu(const json::Valeur &v, Lieu &l) noexcept {
    v.champ("name").copier_texte(l.nom, sizeof l.nom);
    v.champ("timezone").copier_texte(l.fuseau, sizeof l.fuseau);
    double lat = 0;
    double lon = 0;
    if (l.nom[0] == '\0' || l.fuseau[0] == '\0' || !reglages::utf8_valide(l.nom) || !reglages::sans_controle(l.nom) ||
        !v.champ("latitude").en_double(lat) || !v.champ("longitude").en_double(lon) || !(lat >= -90.0 && lat <= 90.0) ||
        !(lon >= -180.0 && lon <= 180.0)) {
        return false;
    }
    l.latitude = lat;
    l.longitude = lon;
    v.champ("admin1").copier_texte(l.region, sizeof l.region);
    v.champ("country").copier_texte(l.pays, sizeof l.pays);
    v.champ("country_code").copier_texte(l.code_pays, sizeof l.code_pays);
    if (!reglages::utf8_valide(l.region) || !reglages::sans_controle(l.region))
        l.region[0] = '\0';
    if (!reglages::utf8_valide(l.pays) || !reglages::sans_controle(l.pays))
        l.pays[0] = '\0';
    l.id_connu = v.champ("id").en_entier(l.id);
    l.altitude_connue = v.champ("elevation").en_double(l.altitude) && altitude_plausible(l.altitude);
    std::int64_t habitants = 0;
    if (v.champ("population").en_entier(habitants) && habitants >= 0) {
        l.population = habitants;
        l.population_connue = true;
    }
    api::Ecrivain libelle(l.libelle, sizeof l.libelle);
    ecrire_libelle(libelle, l.nom, l.region, l.pays);
    if (libelle.ok())
        l.libelle[libelle.vue().size()] = '\0';
    return libelle.ok();
}

// Texte d'affichage facultatif : rogné, gardé s'il est de l'UTF-8 valide sans contrôle, sinon laissé vide (comme la page).
template <std::size_t N> constexpr void copier_facultatif(std::string_view t, char (&sortie)[N]) noexcept {
    t = reglages::rogner(t);
    if (reglages::utf8_valide(t) && reglages::sans_controle(t))
        reglages::copier_texte(t, sortie, N);
}

} // namespace detail

// Lit la réponse du géocodage. Rend faux si ce n'est pas un objet JSON (réponse d'erreur HTML, tableau, texte coupé) ; sinon vrai,
// et `n` vaut le nombre de lieux retenus (zéro si le service n'a rien trouvé : l'objet n'a alors pas de « results »).
constexpr bool analyser(std::string_view texte, Lieu *sortie, std::size_t max, std::size_t &n) noexcept {
    n = 0;
    const json::Resultat lu = json::lire(texte);
    if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet)
        return false;
    const json::Valeur liste = lu.valeur.champ("results");
    const std::size_t total = liste.taille();
    for (std::size_t i = 0; i < total && n < max && n < LIEUX_MAX; ++i) {
        sortie[n] = Lieu{};
        if (detail::lire_lieu(liste.element(i), sortie[n]))
            ++n;
    }
    return true;
}

// {"resultats":[...]} du contrat de la page (docs/api-reglages.md §5). Rend faux si `e` a débordé.
constexpr bool ecrire_resultats(const Lieu *lieux, std::size_t n, api::Ecrivain &e) noexcept {
    e.ouvrir_objet();
    e.cle("resultats");
    e.ouvrir_tableau();
    for (std::size_t i = 0; i < n; ++i) {
        const Lieu &l = lieux[i];
        char texte_id[24] = {};
        e.ouvrir_objet();
        e.champ("id", l.id_connu ? api::entier_en_texte(l.id, texte_id) : std::string_view{});
        e.champ("nom", l.nom);
        e.champ("region", l.region);
        e.champ("pays", l.pays);
        e.champ("code_pays", l.code_pays);
        e.champ_decimal("latitude", l.latitude, 5);
        e.champ_decimal("longitude", l.longitude, 5);
        if (l.altitude_connue)
            e.champ_decimal("altitude", l.altitude, 0);
        e.champ("fuseau", l.fuseau);
        if (l.population_connue)
            e.champ_entier("population", l.population);
        e.champ("libelle", l.libelle);
        e.fermer_objet();
    }
    e.fermer_tableau();
    e.fermer_objet();
    return e.ok();
}

// Réponse brute du service -> {"resultats":[...]} de la page, en une fois (c'est ce que fait GET /api/geocode).
constexpr bool convertir(std::string_view texte, api::Ecrivain &e) noexcept {
    Lieu lieux[LIEUX_MAX] = {};
    std::size_t n = 0;
    if (!analyser(texte, lieux, LIEUX_MAX, n))
        return false;
    return ecrire_resultats(lieux, n, e);
}

// ---- le lieu choisi devient la ville de l'appareil -----------------------------------------------------------------------------

// Mêmes vérifications, dans le même ordre, que `reglages::valider_zone` (nom, coordonnées, fuseau) : c'est la règle de la page
// (PUT /api/location). Une erreur dit laquelle a échoué ; la zone n'est valable que sans erreur.
constexpr reglages::ResultatZone vers_zone(const Lieu &l) noexcept {
    reglages::ResultatZone r;
    const std::string_view nom = reglages::rogner(l.nom);
    if (nom.empty() || !reglages::utf8_valide(nom) || !reglages::sans_controle(nom) ||
        reglages::nombre_de_caracteres(nom) > reglages::NOM_MAX_CARACTERES) {
        r.erreur = reglages::ErreurZone::Nom;
        return r;
    }
    reglages::copier_texte(nom, r.zone.nom, reglages::NOM_ZONE_OCTETS);
    if (!(l.latitude >= -90.0 && l.latitude <= 90.0) || !(l.longitude >= -180.0 && l.longitude <= 180.0)) {
        r.erreur = reglages::ErreurZone::Coordonnees;
        return r;
    }
    r.zone.latitude = l.latitude;
    r.zone.longitude = l.longitude;
    if (fuseaux::trouver(l.fuseau) == nullptr) {
        r.erreur = reglages::ErreurZone::Fuseau;
        return r;
    }
    reglages::copier_texte(l.fuseau, r.zone.fuseau, sizeof r.zone.fuseau);
    detail::copier_facultatif(l.region, r.zone.region);
    detail::copier_facultatif(l.pays, r.zone.pays);
    // code pays : deux ou trois lettres, sinon vide (comme la page)
    const std::string_view cp = l.code_pays;
    bool lettres = !cp.empty() && cp.size() <= 3;
    for (char c : cp)
        lettres = lettres && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
    if (lettres)
        reglages::copier_texte(cp, r.zone.code_pays, sizeof r.zone.code_pays);
    return r;
}

// ---- l'état d'une recherche demandée par l'écran -------------------------------------------------------------------------------

// Mêmes noms que `RechercheLieu` de l'interface des réglages de l'écran (main/hote_reglages.h).
enum class Etat : std::uint8_t { Inactive, EnCours, Terminee, SansConnexion };

// Comment s'est finie la recherche côté réseau. Toute panne (pas d'IP, TLS refusé, délai, HTTP autre que 200, réponse illisible)
// est « sans connexion » pour l'écran, qui n'a pas d'autre phrase ; la cause exacte va au journal de main/geocodage.cpp.
enum class Issue : std::uint8_t { Trouvee, SansConnexion };

// « La dernière demande gagne » : une demande faite pendant qu'une autre s'exécute la remplace ; le résultat de l'ancienne, quand il
// arrive, est jeté. Aucune valeur obsolète n'est jamais montrée. Sans verrou : l'appelant sérialise les accès.
class Recherche {
  public:
    // Texte demandé par l'utilisateur. Refusé (trop court : moins de 3 caractères, trop long : plus de 80, illisible) -> l'état
    // devient `Terminee` sans résultat, la demande en cours est abandonnée, et la raison est rendue. Accepté -> `EnCours`,
    // résultats effacés, et la demande attend qu'une tâche la prenne.
    constexpr reglages::ErreurRecherche demander(std::string_view texte) noexcept {
        const reglages::ResultatRecherche v = reglages::valider_recherche(texte);
        ++generation_;
        nombre_ = 0;
        a_traiter_ = false;
        if (v.erreur != reglages::ErreurRecherche::Aucune) {
            etat_ = Etat::Terminee;
            return v.erreur;
        }
        reglages::copier_texte(v.q, texte_, sizeof texte_);
        a_traiter_ = true;
        etat_ = Etat::EnCours;
        return reglages::ErreurRecherche::Aucune;
    }

    constexpr Etat etat() const noexcept { return etat_; }
    constexpr std::size_t nombre() const noexcept { return nombre_; }

    // Copie au plus `max` résultats de la recherche terminée (zéro tant qu'elle ne l'est pas : `nombre_` est remis à zéro à chaque
    // demande et ne devient positif qu'à la fin d'une recherche réussie).
    constexpr std::size_t resultats(Lieu *sortie, std::size_t max) const noexcept {
        const std::size_t n = nombre_ < max ? nombre_ : max;
        for (std::size_t i = 0; i < n; ++i)
            sortie[i] = lieux_[i];
        return n;
    }

    // ---- côté tâche ----
    // Y a-t-il une demande à exécuter ? Si oui, la recopie dans `texte` (zéro final) et rend son numéro ; la demande est « prise »
    // (une seconde tâche ne la prendrait pas). Le texte est déjà validé et rogné.
    constexpr bool prendre(char (&texte)[reglages::RECHERCHE_OCTETS_MAX + 1], std::uint32_t &numero) noexcept {
        if (!a_traiter_)
            return false;
        reglages::copier_texte(texte_, texte, sizeof texte);
        numero = generation_;
        a_traiter_ = false;
        return true;
    }

    // Fin de la demande `numero`. Rend faux, sans rien changer, si une demande plus récente est arrivée entre-temps, ou si celle-ci
    // est déjà finie (une demande se termine une seule fois).
    constexpr bool terminer(std::uint32_t numero, Issue issue, const Lieu *lieux, std::size_t n) noexcept {
        if (numero != generation_ || etat_ != Etat::EnCours)
            return false;
        if (issue == Issue::SansConnexion) {
            etat_ = Etat::SansConnexion;
            return true;
        }
        const std::size_t garde = n < LIEUX_MAX ? n : LIEUX_MAX;
        for (std::size_t i = 0; i < garde; ++i)
            lieux_[i] = lieux[i];
        nombre_ = garde;
        etat_ = Etat::Terminee;
        return true;
    }

  private:
    Etat etat_ = Etat::Inactive;
    bool a_traiter_ = false;
    std::uint32_t generation_ = 0;
    std::size_t nombre_ = 0;
    char texte_[reglages::RECHERCHE_OCTETS_MAX + 1]{};
    Lieu lieux_[LIEUX_MAX]{};
};

} // namespace ecran::geocodage
