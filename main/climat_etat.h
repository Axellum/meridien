// La dernière réponse lue de Vigie pour les normales climatiques (ADR-0029), partagée entre la tâche réseau (climat_client.cpp, qui
// l'écrit) et la tâche de l'interface (ecran_principal.cpp, qui la lit pour la pastille « sur la normale » de l'accueil et pour la page
// Sources). Une structure d'une quarantaine d'octets copiée sous un verrou : pas de tampon en PSRAM ni de lecture qui bloque l'écriture.
// Aucun en-tête ESP-IDF ici : seulement les types du noyau (ecran/climat_normales.hpp).
#pragma once

#include <cstdint>

#include "ecran/climat_normales.hpp"

namespace ecran {

// Publie une réponse lue (statut `Ok`), reçue à `recu_s` (secondes UNIX, horloge de l'appareil : l'âge de la réponse en dépend).
void climat_etat_publier(const climat::Normales &normales, std::int64_t recu_s);

// Oublie la réponse (lieu ou adresse changés, alertes désactivées) : l'interface ne dit plus rien tant qu'une nouvelle n'est pas là.
void climat_etat_oublier();

// Copie la réponse courante dans `sortie` et son instant de réception dans `recu_s`. Faux si aucune réponse utilisable n'est en mémoire.
bool climat_etat_lire(climat::Normales &sortie, std::int64_t &recu_s);

} // namespace ecran
