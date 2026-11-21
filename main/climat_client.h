// Client des normales climatiques de Vigie (ADR-0029) : `GET <adresse de Vigie>/v1/climate/normals?lat=&lon=` UNE fois par jour, lu avec
// le noyau pur (ecran/climat_normales.hpp) et publié dans climat_etat.h pour la pastille « sur la normale » de l'accueil. Passe par la
// tâche réseau des données (tache_donnees.h), le client HTTPS commun (http_commun.h) et le verrou TLS.
//
// Mêmes réglages que les alertes (ADR-0009) : l'adresse de Vigie et l'interrupteur « Alertes officielles » ; désactivées, aucune requête
// ne part et la réponse en mémoire est oubliée. Rythme et politesse envers l'instance publique : ecran/climat_planning.hpp (24 h, premier
// essai 3 à 5 minutes après que le réseau et l'heure sont prêts, attente doublée après un échec, 1 h au moins après un 429 ou un 503).
#pragma once

namespace ecran {

// Inscrit le pas auprès de la tâche réseau des données. À appeler une fois, après tache_donnees_demarrer() et vigie_client_demarrer()
// (la NVS des alertes est déjà lue).
void climat_client_demarrer();

// Un réglage des alertes ou le lieu ont changé : la réponse en mémoire n'a plus de sens, une nouvelle requête part, au plus une toutes
// les cinq minutes. Appelable depuis n'importe quelle tâche.
void climat_client_a_change();

} // namespace ecran
