// Acquisition rapide de l'heure par SNTP direct (sockets), jalon 1b.
//
// Pourquoi : le SNTP de lwIP interroge UN serveur à la fois et n'attend la réponse que par paliers de 15 s (SNTP_RECV_TIMEOUT, imposé
// par la spécification). Sur le Tab5, sa première requête restait sans réponse dans environ un démarrage sur trois, alors que la même
// requête envoyée par socket était répondue en ~20 ms (docs/mesures/1b.md) : l'heure arrivait 15, 30 ou 45 s trop tard, voire pas du tout
// dans les 60 s. Ici : requêtes vers plusieurs serveurs du pool à la fois, relance toutes les 1,5 s, horloge réglée à la première
// réponse vérifiée. Le SNTP de lwIP reste en place pour la remise à l'heure périodique.
#pragma once

namespace ecran {

// Lance, une seule fois, la tâche d'acquisition. À appeler dès l'obtention de l'adresse IP. `quand_regle` est appelée depuis cette
// tâche, une fois l'horloge du système réglée (peut être nul).
void sntp_rapide_demarrer(void (*quand_regle)());

} // namespace ecran
