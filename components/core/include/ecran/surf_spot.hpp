// Noyau pur « surf » : le spot (un lieu de surf saisi par la personne) et les règles de validité de ses champs. C++17 standard,
// aucun include ESP-IDF. L'appareil les applique à tout ce qui vient de la page du téléphone ou de la NVS : rien d'invraisemblable
// ne part vers Open-Meteo et aucune note n'est calculée sur un spot incohérent. Même rôle que `meteo_zone_regles.hpp` pour la zone
// météo, dont ce fichier reprend le contrôle du nom (UTF-8 bien formé, ni contrôle ni nom d'espaces).
//
// Pas de base mondiale de spots (décision d'Axel du 07/11/2026) : aucun jeu ouvert vérifié ne donne l'orientation de la plage, le
// côté de la vague ni le fond (voir docs/decisions/0013-surf-donnees-note-et-limites.md). La personne saisit donc, pour chaque
// spot, son nom, sa position et l'ORIENTATION DE LA PLAGE ; la fourchette de hauteur qu'elle aime est facultative (valeurs par
// défaut ci-dessous, à ajuster : ce sont nos choix, pas une norme).
//
// Orientation = la direction VERS LAQUELLE la plage fait face, vers le large, en degrés depuis le nord vrai, dans le sens des
// aiguilles d'une montre (nord 0, est 90, sud 180, ouest 270). Une plage de la côte landaise (océan à l'ouest) fait face à 270°.
// C'est la convention du vent : une direction est toujours « d'où vient » le vent (comme Open-Meteo), donc un vent dont la
// direction égale l'orientation de la plage souffle de la mer vers la terre (voir surf_vent.hpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_zone_regles.hpp"
#include "ecran/surf_base.hpp"

namespace ecran::surf {

// Nom affiché dans la liste « Mes spots » : 1 à 32 caractères (pas d'octets : « Plage d'Hossegor » et « 千葉 » comptent leurs
// lettres), au pire 4 octets chacun ; le tampon garde la place du zéro final.
inline constexpr std::size_t SPOT_NOM_MAX_CARACTERES = 32;
inline constexpr std::size_t SPOT_NOM_OCTETS = 4 * SPOT_NOM_MAX_CARACTERES + 1;

// Nombre de spots gardés. Chaque spot coûte DEUX requêtes par rafraîchissement (mer et vent) : douze spots font 24 appels, très
// loin des 10 000 par jour de l'API gratuite même toutes les 15 minutes (24 x 96 = 2 304), et assez pour une liste lisible.
inline constexpr std::size_t SPOTS_MAX = 12;

// Fourchette de hauteur de houle préférée, en centimètres. Défauts : de 1,00 m à 2,50 m, une surface où la plupart des surfeurs
// se plaisent sans être ni dans la mousse ni dans le gros temps (choix de l'auteur, modifiable par spot).
inline constexpr std::int16_t HAUTEUR_MIN_DEFAUT_CM = 100;
inline constexpr std::int16_t HAUTEUR_MAX_DEFAUT_CM = 250;
// Bornes de ce qu'on accepte de saisir : de 10 cm (mer d'huile) à 25 m (Nazaré a connu plus de 20 m), et un écart d'au moins 20 cm
// pour que « dans la fourchette » veuille dire quelque chose.
inline constexpr std::int16_t HAUTEUR_SAISIE_MIN_CM = 10;
inline constexpr std::int16_t HAUTEUR_SAISIE_MAX_CM = 2500;
inline constexpr std::int16_t HAUTEUR_ECART_MIN_CM = 20;

struct Spot {
    char nom[SPOT_NOM_OCTETS] = {};
    std::int32_t latitude_e5 = 0;  // centaines de millièmes de degré (e5), comme la zone météo
    std::int32_t longitude_e5 = 0; // idem
    std::int16_t orientation_deg = 0;
    std::int16_t hauteur_min_cm = HAUTEUR_MIN_DEFAUT_CM;
    std::int16_t hauteur_max_cm = HAUTEUR_MAX_DEFAUT_CM;
};

// Le premier champ en défaut, pour que la page ou l'écran dise LEQUEL corriger (jamais un « invalide » muet).
enum class ChampSpot : std::uint8_t {
    Aucun, // le spot est valide
    Nom,
    Latitude,
    Longitude,
    PositionNulle, // 0 ; 0 exactement : une position jamais saisie, pas un spot
    Orientation,
    Fourchette,
};

// Nombre de caractères d'un texte UTF-8 DÉJÀ reconnu bien formé (les octets de continuation 10xxxxxx ne comptent pas).
constexpr std::size_t nombre_de_caracteres(const char *nom) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; nom[i] != '\0'; ++i) {
        if ((static_cast<unsigned char>(nom[i]) & 0xC0) != 0x80)
            ++n;
    }
    return n;
}

// Nom d'un spot : mêmes règles que le nom d'une zone (UTF-8 bien formé, pas de caractère de contrôle, pas seulement des espaces,
// zéro final dans `capacite`) et 32 caractères au plus.
constexpr bool nom_de_spot_valide(const char *nom, std::size_t capacite) noexcept {
    if (!meteo::nom_de_zone_valide(nom, capacite))
        return false;
    return nombre_de_caracteres(nom) <= SPOT_NOM_MAX_CARACTERES;
}

// Orientation : 0 à 359. 360 est refusé (la saisie le ramène à 0 avec `normaliser_degres`) pour qu'un même spot n'ait pas deux
// écritures.
constexpr bool orientation_valide(std::int64_t deg) noexcept { return deg >= 0 && deg <= 359; }

// Fourchette de hauteur : chaque borne dans la plage de saisie et la haute au moins 20 cm au-dessus de la basse.
constexpr bool fourchette_valide(std::int64_t min_cm, std::int64_t max_cm) noexcept {
    return min_cm >= HAUTEUR_SAISIE_MIN_CM && max_cm <= HAUTEUR_SAISIE_MAX_CM &&
           max_cm - min_cm >= HAUTEUR_ECART_MIN_CM;
}

constexpr ChampSpot verifier_spot(const Spot &s) noexcept {
    if (!nom_de_spot_valide(s.nom, sizeof(s.nom)))
        return ChampSpot::Nom;
    if (s.latitude_e5 < -meteo::LATITUDE_E5_MAX || s.latitude_e5 > meteo::LATITUDE_E5_MAX)
        return ChampSpot::Latitude;
    if (s.longitude_e5 < -meteo::LONGITUDE_E5_MAX || s.longitude_e5 > meteo::LONGITUDE_E5_MAX)
        return ChampSpot::Longitude;
    if (s.latitude_e5 == 0 && s.longitude_e5 == 0)
        return ChampSpot::PositionNulle;
    if (!orientation_valide(s.orientation_deg))
        return ChampSpot::Orientation;
    if (!fourchette_valide(s.hauteur_min_cm, s.hauteur_max_cm))
        return ChampSpot::Fourchette;
    return ChampSpot::Aucun;
}

constexpr bool spot_valide(const Spot &s) noexcept { return verifier_spot(s) == ChampSpot::Aucun; }

// Phrase courte pour la page ou l'écran (jamais un code ni un état brut) ; vide si le spot est valide. Les textes affichés
// passeront par la table de traduction quand l'écran existera ; ces phrases sont la clé française.
constexpr const char *message_court(ChampSpot c) noexcept {
    switch (c) {
    case ChampSpot::Aucun:
        return "";
    case ChampSpot::Nom:
        return "Nom du spot invalide";
    case ChampSpot::Latitude:
        return "Latitude invalide";
    case ChampSpot::Longitude:
        return "Longitude invalide";
    case ChampSpot::PositionNulle:
        return "Position du spot non saisie";
    case ChampSpot::Orientation:
        return "Orientation de la plage invalide";
    case ChampSpot::Fourchette:
        return "Hauteur préférée invalide";
    }
    return "Spot invalide";
}

} // namespace ecran::surf
