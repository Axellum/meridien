// PROVISOIRE (premier commit du contrat) : aucune météo n'est connue, `valide` est toujours faux. Le client Open-Meteo, le cache
// NVS et la péremption arrivent dans les commits suivants de cette branche, sans changer meteo_etat.h.
#include "meteo_etat.h"

#include <cstring>

bool meteo_etat_lire(MeteoEtat *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    std::memset(sortie, 0, sizeof(*sortie));
    return false;
}
