// Client des alertes officielles de Vigie (https://vigie.auxfilsdesidees.fr, instance publique d'Axel ; adresse modifiable dans les
// réglages). Une tâche en arrière-plan interroge `GET /v1/vigilances?lat=&lon=&lang=` UNE fois par période (réglage, 15 min par
// défaut), lit la réponse avec le noyau pur (ecran/vigie_alertes.hpp) et la publie dans vigie_etat.h pour l'interface.
//
// Politesse envers l'instance publique (limitée par adresse, « pas faite pour bâtir un service dessus ») : une requête par période,
// jamais de rafale au démarrage (premier essai 60 à 120 s après que le réseau et l'heure soient prêts), attente doublée après chaque
// échec, 30 minutes après un 429 ou un 503 (ecran/vigie_planning.hpp).
#pragma once

namespace ecran {

// Lance la tâche. À appeler une fois, après reglages_demarrer() et le Wi-Fi. Sans effet si les alertes sont désactivées dans les
// réglages : la tâche tourne quand même (le réglage peut changer) mais n'ouvre aucune connexion.
void vigie_client_demarrer();

// Un réglage des alertes, le lieu ou la langue de l'écran ont changé : l'ancienne réponse n'a plus de sens, une nouvelle requête
// part, au plus une par minute (même si les changements se suivent). Appelable depuis n'importe quelle tâche.
void vigie_client_a_change();

} // namespace ecran
