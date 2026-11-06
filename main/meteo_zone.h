// Contrat « zone météo » : l'endroit dont on affiche la météo. Propriétaire : le client Open-Meteo (cette session) ; la page
// de réglages l'écrit, l'interface peut en lire le nom. Ce fichier est PUR (aucun en-tête ESP-IDF). On peut AJOUTER des champs,
// jamais en retirer.
//
// Stockée en NVS. Défaut : Lyon. Un changement de zone (`zone_ecrire` qui réussit avec une valeur différente) déclenche tout de
// suite une nouvelle requête.
#pragma once

struct ZoneMeteo {
    char nom[48];    // « Lyon » (affichage), UTF-8, terminé par un zéro
    float latitude;  // degrés décimaux, -90 à 90
    float longitude; // degrés décimaux, -180 à 180
    char fuseau[40]; // nom IANA (« Europe/Paris »), terminé par un zéro
};

// Lit la zone enregistrée, ou la zone par défaut (Lyon 45,74906 / 4,84789 / Europe/Paris) si rien n'est enregistré ou si
// l'enregistrement est illisible. Rend vrai si `*sortie` a été remplie (même avec le défaut) ; faux seulement si `sortie` est nul.
bool zone_lire(ZoneMeteo *sortie);

// Enregistre la zone. Refuse (rend faux, rien n'est écrit) un nom vide, une latitude hors de -90..90, une longitude hors de
// -180..180, un nombre non fini ou un fuseau vide ou aux caractères inattendus.
bool zone_ecrire(const ZoneMeteo &zone);
