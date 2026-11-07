// Heure et date locales à partir d'un instant UTC, DANS LE FUSEAU DU LIEU CHOISI (ADR-0012) : celui de la zone enregistrée
// (meteo_zone.h, champ `fuseau`), relu à chaque appel. Choisir une autre ville, sur l'écran ou sur la page web, change donc l'heure,
// la date et le changement de jour tout de suite ; la zone d'usine (Lyon) donne Europe/Paris. Tout le calcul est dans le noyau pur
// (heure_fr.hpp, fuseaux.hpp) : ce fichier ne fait que le brancher. Si le nom du fuseau n'est pas dans la table (NVS ancienne), toutes
// les fonctions disent « inconnu » (faux, 0) : jamais l'heure d'un autre fuseau.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/heure_fr.hpp"
#include "ecran/i18n.hpp"

namespace ecran {

struct TexteHorloge {
    char heure[24]; // « 11 h 40 »
    char date[48];  // « jeudi 6 novembre 2026 »
};

// Les règles du fuseau du lieu (décalage et changements d'heure), pour convertir n'importe quel instant, pas seulement le présent :
// l'écran principal s'en sert pour l'étiquette de chaque colonne « heure par heure », le lever et le coucher du soleil, les noms
// de jours (ecran/heure_lieu.hpp). Rend faux, sans rien écrire, si le fuseau n'a pas pu être lu.
bool horloge_fuseau(ecran::heure::Fuseau &sortie);

// Rend faux si le fuseau n'a pas pu être analysé ou si un texte ne tient pas dans son tampon.
bool horloge_formater(int64_t instant_utc_s, TexteHorloge &sortie);

// Heure locale en chiffres pour la grande horloge de l'écran principal : « 11:40 », « 09:05 » (24 h, deux chiffres partout, pas
// de secondes). Même fuseau que horloge_formater. Rend faux si le fuseau n'a pas pu être analysé ou si le tampon est trop
// petit (6 octets au moins, zéro final compris) ; le tampon est alors vide.
bool horloge_hhmm(int64_t instant_utc_s, char *sortie, std::size_t capacite);

// Date longue dans la langue de l'écran (« Freitag, 6. November 2026 », « 6 Kasım 2026 Cuma »), même fuseau que horloge_formater :
// la tournure de chaque langue est dans le noyau pur (ecran/visuel_dates.hpp). Rend faux si le fuseau n'a pas pu être analysé ou si
// le tampon est trop petit ; il est alors vide. (Ajout de la session ecran-visuel : horloge_formater ne parle que français.)
bool horloge_date_localisee(int64_t instant_utc_s, ecran::i18n::Langue langue, char *sortie, std::size_t capacite);

// Date et heure locales d'un instant (même fuseau que les autres), en chiffres : la page des alertes en fait « samedi 8 novembre · 05:59 »
// dans la langue de l'écran (ecran/vigie_textes.hpp). Rend faux, sans rien écrire, si le fuseau n'a pas pu être analysé.
bool horloge_heure_locale(int64_t instant_utc_s, ecran::heure::HeureLocale &sortie);

// Décalage à l'UTC en vigueur à cet instant, en secondes (3600 l'hiver, 7200 l'été à Paris) ; 0 si le fuseau n'a pas pu être analysé.
// Valable pour CET instant seulement : pour une fenêtre de prévision, utiliser horloge_fuseau() et convertir chaque instant.
int32_t horloge_decalage_s(int64_t instant_utc_s);

} // namespace ecran
