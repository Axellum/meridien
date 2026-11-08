// Registre des spots de la page mer (ADR-0019) : jusqu'à 12 spots de la liste de Vigie, celui dont on demande la mer, la plage de vent
// du kite. NVS (espace « mer », clé « registre », un blob versionné écrit d'un seul coup). Toutes les règles (identifiants, noms,
// doublons, plage de vent, ce que devient une NVS abîmée) sont dans le noyau pur ecran/mer_spots.hpp, testé sur le PC ; ce fichier ne
// fait que lire, écrire et prévenir le client réseau.
// Ce fichier est PUR (aucun en-tête ESP-IDF). Aucune route web, aucune page ni aucun écran ne l'écrit encore (lots suivants) : tant que
// rien ne l'écrit, le registre est vide et l'appareil ne demande rien à Vigie.
#pragma once

#include <cstdint>

#include "ecran/mer_spots.hpp"

namespace ecran {

// Lit la NVS. À appeler une fois, après nvs_flash_init() ; mer_client_demarrer() le fait. Sans NVS lisible : registre vide.
void reglages_mer_demarrer();

// Copie le registre courant dans `sortie` (≈ 2,3 Ko : à la charge de l'appelant, pas sur une pile de 12 Ko). Sûre depuis n'importe quelle
// tâche. Rend faux, `sortie` inchangée, si le registre n'a pas pu être gardé en mémoire (PSRAM pleine au démarrage).
bool reglages_mer(mer::Registre &sortie);

// Ce que le client réseau demande (mer::DemandeMer : le spot choisi et la plage de vent, ≈ 70 octets, se copie sur une pile). `presente`
// faux : registre vide, rien à demander.
mer::DemandeMer reglages_mer_demande();

// Enregistre en NVS puis applique, et réveille le client réseau (une seule requête, au plus une par minute, si la requête a changé).
// Rend faux, sans rien changer, si le registre n'est pas valide (mer::registre_valide : entrée à corriger, doublon, choix hors de la
// liste, plage de vent refusée) ou si l'écriture échoue.
bool reglages_mer_enregistrer(const mer::Registre &nouveau);

} // namespace ecran
