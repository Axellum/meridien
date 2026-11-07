// Liaison entre les trois fichiers météo de l'appareil : l'état (meteo_etat.cpp), la zone (meteo_zone.cpp) et le client
// Open-Meteo (meteo_client.cpp). PAS pour l'interface ni la page de réglages : elles ne voient que meteo_etat.h et
// meteo_zone.h (contrats), qui ne changent pas.
#pragma once

#include <cstdint>

#include "ecran/meteo_riche.hpp"
#include "meteo_zone.h"

namespace ecran {

// ---- état (meteo_etat.cpp), écrit par le client --------------------------------------------------------------------

// Une réponse reçue, ou relue du cache : devient la météo courante, remet le compteur d'échecs à zéro et change le numéro de
// version (`MeteoDetail::version`). Rend faux, sans rien publier, si l'état n'a pas pu être alloué (ni PSRAM ni RAM interne).
bool meteo_etat_publier(const meteo::Meteo &meteo);
// Un essai raté de plus (réseau, HTTP, réponse illisible). Sans effet sur la météo déjà connue, qui reste lisible.
void meteo_etat_compter_echec();
// La zone a changé : la météo de l'ancien lieu ne s'affiche plus (`valide` repasse à faux, `meteo_detail_version()` à 0) jusqu'à
// la prochaine réponse.
void meteo_etat_oublier();
// Nombre d'essais ratés de suite depuis la dernière réponse.
int meteo_etat_echecs();
// Copie du relevé complet (prévisions des jours, lever et coucher du soleil, vent, pression...) pour l'écran principal, qui
// montre plus que MeteoEtat. Rend faux, sans rien écrire, s'il n'y a pas de relevé. `echecs` (peut être nul) : essais ratés de suite.
// Ajout de la session ecran-visuel (le contrat meteo_etat.h, lui, ne change pas).
bool meteo_etat_lire_releve(meteo::Releve *sortie, int *echecs);

// ---- zone (meteo_zone.cpp), lue par le client ---------------------------------------------------------------------

struct PositionE5 {
    int32_t latitude_e5;
    int32_t longitude_e5;
    bool operator==(const PositionE5 &autre) const {
        return latitude_e5 == autre.latitude_e5 && longitude_e5 == autre.longitude_e5;
    }
};

// Zone courante et sa position exacte en e5 (centièmes de millième de degré), celle qui part dans la requête.
void zone_lire_e5(ZoneMeteo *zone, PositionE5 *position);

// ---- client (meteo_client.cpp), appelé quand la zone enregistrée change de position --------------------------------

// Efface la météo de l'ancien lieu et réveille la tâche pour une requête immédiate.
void meteo_client_zone_changee();

// Même chose quand la SOURCE change (main/reglages.cpp) : efface en plus le relevé gardé en NVS (il est indexé par la position
// seulement : il montrerait la météo de l'ancienne source jusqu'à la prochaine réponse) et réveille la tâche.
void meteo_client_source_changee();

} // namespace ecran
