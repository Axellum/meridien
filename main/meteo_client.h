// Client météo Open-Meteo de l'appareil : une tâche qui, une fois le Wi-Fi et l'heure disponibles, interroge Open-Meteo en HTTPS
// toutes les 15 min (reprise espacée après un échec), garde le dernier relevé en NVS et le publie par meteo_etat_lire()
// (main/meteo_etat.h). La requête et la lecture de la réponse sont dans le noyau pur (components/core : meteo_requete,
// meteo_openmeteo, meteo_planning, meteo_cache) ; ce fichier-ci n'est que le branchement sur ESP-IDF.
#pragma once

namespace ecran {

// Relit le cache de la NVS (la météo est lisible tout de suite, avant même le Wi-Fi) puis lance la tâche. À appeler une fois,
// après reseau_demarrer() (le NVS est déjà initialisé par boot_info_load()). Rend la main tout de suite.
void meteo_client_demarrer();

} // namespace ecran
