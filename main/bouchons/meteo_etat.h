// BOUCHON — ne remplace PAS le vrai fichier. Le propriétaire de `main/meteo_etat.h` est la session `meteo-appareil` (contrat
// d'interface n° 1) ; tant qu'elle ne l'a pas livré, ce fichier-ci donne à l'écran principal de quoi se compiler et s'afficher,
// avec des valeurs d'EXEMPLE clairement marquées « Exemple » à l'écran. main/CMakeLists.txt n'ajoute ce dossier au chemin
// d'inclusion que si `main/meteo_etat.h` n'existe pas : dès que le vrai fichier est là, ce dossier est ignoré (et à supprimer).
// On peut AJOUTER des champs au contrat, jamais en retirer : la forme ci-dessous est celle de l'énoncé du contrat.
#pragma once

#include <cstdint>

#define MERIDIEN_METEO_BOUCHON 1

struct MeteoEtat {
    bool valide;         // une météo a été reçue au moins une fois
    bool perime;         // la dernière météo reçue est trop ancienne
    int64_t maj_epoch_s; // instant UTC de la dernière mise à jour
    float temperature_c;
    int code_wmo;
    bool jour;
    float min_c;
    float max_c;
    char libelle[40]; // libellé court, déjà traduit
};

// Copie atomique, sûre entre tâches. Rend faux si aucune météo n'est disponible.
bool meteo_etat_lire(MeteoEtat *sortie);
