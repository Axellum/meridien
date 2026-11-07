// Heure et date locales en français, à partir d'un instant UTC. Le fuseau est codé en dur (Europe/Paris) en phase 1b ; le
// réglage du fuseau par l'utilisateur vient avec la page de réglages. Tout le calcul est dans le noyau pur (heure_fr.hpp,
// fuseaux.hpp) : ce fichier ne fait que le brancher.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/i18n.hpp"

namespace ecran {

struct TexteHorloge {
    char heure[24]; // « 11 h 40 »
    char date[48];  // « jeudi 6 novembre 2026 »
};

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

// Décalage à l'UTC en vigueur à cet instant, en secondes (3600 l'hiver, 7200 l'été à Paris) ; 0 si le fuseau n'a pas pu être analysé.
// Sert à rendre l'heure locale des levers et couchers de soleil et des colonnes « heure par heure ».
int32_t horloge_decalage_s(int64_t instant_utc_s);

} // namespace ecran
