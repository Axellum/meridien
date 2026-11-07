// Réglages des alertes officielles de Vigie (ADR-0009) : activées ou non, adresse de Vigie, période entre deux requêtes, niveau
// minimal montré. NVS (espace « alertes », clé « reglage », un blob versionné écrit d'un seul coup). Toutes les règles (bornes,
// adresse en https://, valeurs par défaut quand la NVS est abîmée) sont dans le noyau pur ecran/reglages_alertes.hpp, testé sur le
// PC ; ce fichier ne fait que lire, écrire et prévenir le client réseau. À part de reglages.h (affichage, ville) : un autre module,
// pour ne pas toucher ce que la session météo modifie.
// Ce fichier est PUR (aucun en-tête ESP-IDF).
#pragma once

#include "ecran/reglages_alertes.hpp"

namespace ecran {

// Lit la NVS. À appeler une fois, après nvs_flash_init() ; vigie_client_demarrer() le fait. Sans NVS lisible : valeurs par défaut
// (alertes activées, instance publique, 15 minutes, à partir du jaune).
void reglages_alertes_demarrer();

// Copie des valeurs courantes (sûre depuis n'importe quelle tâche).
reglages::Alertes reglages_alertes();

// Enregistre en NVS puis applique, et réveille le client réseau (une seule requête, au plus une par minute, si la requête a changé).
// Rend faux, sans rien changer, si la valeur n'est pas valide (période hors de 10..180, adresse refusée) ou si l'écriture échoue.
bool reglages_alertes_enregistrer(const reglages::Alertes &nouveau);

} // namespace ecran
