// Heure et date locales en français, à partir d'un instant UTC. Le fuseau est codé en dur (Europe/Paris) en phase 1b ; le
// réglage du fuseau par l'utilisateur vient avec la page de réglages. Tout le calcul est dans le noyau pur (heure_fr.hpp,
// fuseaux.hpp) : ce fichier ne fait que le brancher.
#pragma once

#include <cstddef>
#include <cstdint>

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

} // namespace ecran
