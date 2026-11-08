// Client de la mer d'un spot (trame de Vigie, ADR-0018 et ADR-0019). Une tâche en arrière-plan interroge
// `GET /v1/embedded/frame?device_type=generic&spot=<id>` UNE fois par période (10 minutes) pour le seul spot choisi dans le registre
// (reglages_mer.h), lit la trame avec le noyau pur (ecran/mer_trame.hpp) et la publie dans mer_etat.h pour l'interface.
//
// Registre vide (c'est l'état d'un appareil neuf) : aucune requête, aucune connexion, et même aucune tâche : sa pile de 12 Ko en RAM
// interne n'est prise qu'au premier choix d'un spot (au démarrage si le registre en NVS en a un, ou plus tard quand reglages_mer.h
// enregistre le premier). Une fois créée la tâche reste. L'adresse de Vigie est celle des réglages des alertes (reglages_alertes.h),
// instance publique par défaut : un seul serveur pour les deux.
//
// Politesse envers l'instance publique : les règles des alertes (ecran/mer_requete.hpp reprend vigie_planning.hpp) : jamais de rafale
// au démarrage (premier essai 120 à 180 s après que le réseau et l'heure soient prêts), attente doublée après chaque échec, 30 minutes
// après un 429 ou un 503, 60 secondes au moins entre deux requêtes.
#pragma once

namespace ecran {

// Lit le registre en NVS (reglages_mer_demarrer) puis lance la tâche. À appeler une fois, après le Wi-Fi.
void mer_client_demarrer();

// Le registre, la plage de vent ou l'adresse de Vigie ont changé : l'ancienne mer n'a plus de sens, une nouvelle requête part, au plus
// une par minute. Appelable depuis n'importe quelle tâche.
void mer_client_a_change();

} // namespace ecran
