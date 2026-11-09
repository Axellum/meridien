// Client météo Open-Meteo de l'appareil : une tâche qui, une fois le Wi-Fi et l'heure disponibles, interroge Open-Meteo en HTTPS
// toutes les 15 min (reprise espacée après un échec), garde le dernier relevé en NVS et le publie par meteo_etat_lire()
// (main/meteo_etat.h). La requête et la lecture de la réponse sont dans le noyau pur (components/core : meteo_requete,
// meteo_openmeteo, meteo_planning, meteo_cache) ; ce fichier-ci n'est que le branchement sur ESP-IDF.
#pragma once

namespace ecran {

// Relit le cache de la NVS et le publie par meteo_etat_lire() : la météo est lisible tout de suite, avant même le Wi-Fi. À
// appeler AVANT la création de l'écran principal pour que sa première image porte déjà la météo (sinon elle n'arrive qu'au
// passage suivant du minuteur, ≤ 0,5 s plus tard). Ne demande ni le Wi-Fi ni l'heure ; le NVS doit être initialisé
// (boot_info_load()) et les réglages relus (reglages_demarrer(), pour le lieu). Sans effet au deuxième appel. Depuis la tâche
// principale seulement (c'est elle qui appelle aussi meteo_client_demarrer()).
void meteo_client_relire_cache();

// Lance la tâche (relit d'abord le cache s'il ne l'a pas été par meteo_client_relire_cache()). À appeler une fois, après
// reseau_demarrer(). Rend la main tout de suite.
void meteo_client_demarrer();

} // namespace ecran
