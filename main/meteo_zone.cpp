// PROVISOIRE (premier commit du contrat) : la zone par défaut (Lyon) est rendue telle quelle, `zone_ecrire` n'enregistre rien.
// La NVS arrive dans un commit suivant de cette branche, sans changer meteo_zone.h.
#include "meteo_zone.h"

#include <cstring>

bool zone_lire(ZoneMeteo *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    std::memset(sortie, 0, sizeof(*sortie));
    std::strcpy(sortie->nom, "Lyon");
    sortie->latitude = 45.74906F;
    sortie->longitude = 4.84789F;
    std::strcpy(sortie->fuseau, "Europe/Paris");
    return true;
}

bool zone_ecrire(const ZoneMeteo &) {
    return false; // provisoire : rien n'est enregistré
}
