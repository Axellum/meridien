// BOUCHON — voir meteo_etat.h de ce dossier. Valeurs d'EXEMPLE : ce n'est pas la météo de quelque part.
#include "meteo_etat.h"

#include <cstring>

bool meteo_etat_lire(MeteoEtat *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    *sortie = MeteoEtat{};
    sortie->valide = true;
    sortie->perime = false;
    sortie->maj_epoch_s = 0;
    sortie->temperature_c = 12.4F;
    sortie->code_wmo = 2;
    sortie->jour = true;
    sortie->min_c = 8.1F;
    sortie->max_c = 14.6F;
    sortie->minmax_connus = true;
    std::strncpy(sortie->libelle, "Partiellement nuageux", sizeof(sortie->libelle) - 1);
    return true;
}
