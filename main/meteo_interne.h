// Liaison entre les trois fichiers météo de l'appareil : l'état (meteo_etat.cpp), la zone (meteo_zone.cpp) et le client
// Open-Meteo (meteo_client.cpp). PAS pour l'interface ni la page de réglages : elles ne voient que meteo_etat.h et
// meteo_zone.h (contrats), qui ne changent pas.
#pragma once

#include <cstdint>

#include "ecran/meteo_openmeteo.hpp"
#include "meteo_zone.h"

namespace ecran {

// ---- état (meteo_etat.cpp), écrit par le client --------------------------------------------------------------------

// Une réponse reçue, ou relue du cache : devient la météo courante et remet le compteur d'échecs à zéro.
void meteo_etat_publier(const meteo::Releve &releve);
// Un essai raté de plus (réseau, HTTP, réponse illisible). Sans effet sur la météo déjà connue, qui reste lisible.
void meteo_etat_compter_echec();
// La zone a changé : la météo de l'ancien lieu ne s'affiche plus (`valide` repasse à faux) jusqu'à la prochaine réponse.
void meteo_etat_oublier();
// Nombre d'essais ratés de suite depuis la dernière réponse.
int meteo_etat_echecs();

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

} // namespace ecran
