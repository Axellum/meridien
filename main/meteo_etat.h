// Contrat « météo actuelle » entre le client Open-Meteo (cette session, propriétaire) et l'interface (ecran-principal) ou la
// page de réglages. Ce fichier est PUR : seul <cstdint>, aucun en-tête ESP-IDF, pour que le code d'interface se compile sur le
// PC (rendu-pc). On peut AJOUTER des champs ; on n'en retire pas et on ne change pas leur sens.
//
// Les noms sont au niveau global, comme dans le contrat convenu entre les sessions ; le code d'un `namespace ecran` les trouve
// sans qualification.
#pragma once

#include <cstdint>

struct MeteoEtat {
    bool valide; // une météo exploitable existe (reçue ou relue du cache) ; faux : ne rien afficher d'autre que « — »
    bool perime; // la dernière réponse date de plus de 2 h, ou les essais échouent depuis longtemps
    int64_t maj_epoch_s; // instant UNIX (UTC) de la dernière réponse reçue ; 0 si aucune
    float temperature_c; // °C, un chiffre après la virgule
    int code_wmo;        // code météo WMO d'Open-Meteo
    bool jour;           // is_day d'Open-Meteo (soleil ou lune pour les codes 0 à 2)
    float min_c;         // minimum du jour, °C ; seulement si valide
    float max_c;         // maximum du jour, °C ; seulement si valide
    char libelle[40];    // libellé français court (« Partiellement nuageux »), terminé par un zéro ; vide si !valide
};

// Copie atomique de l'état courant, sûre depuis n'importe quelle tâche. Rend `valide` (donc faux tant qu'aucune météo n'est
// connue, et alors `*sortie` est quand même remise à zéro). `sortie` nul : rend faux sans rien écrire.
bool meteo_etat_lire(MeteoEtat *sortie);
