// Code d'appairage de la page de réglages (docs/api-reglages.md §2, ADR-0003). L'appareil tire un code à 6 chiffres à chaque
// démarrage ; la personne qui ouvre la page le lit sur l'écran de l'appareil et le tape pour recevoir un jeton de session. Rien
// n'est gardé en NVS : un redémarrage ferme les sessions et change le code.
//
// Le code change aussi quand on touche « Nouveau code » sur l'écran (sessions fermées, verrou levé) et quand le gestionnaire en tire
// un après une série d'essais ratés (ecran/appairage.hpp). Deux tâches s'en servent :
//   - l'interface (tâche LVGL) lit le code affiché et demande un nouveau code ;
//   - le serveur de la page (fil d'esp_http_server, main/serveur_web.cpp) possède le gestionnaire ; il applique la demande et
//     publie le code par `appairage_synchroniser`, au début et à la fin de chaque requête de l'API.
// Le code demandé par le bouton s'affiche tout de suite ; le serveur l'applique avant de traiter la requête suivante : aucune
// requête n'est jugée avec l'ancien code une fois le nouveau affiché (sauf celle déjà en cours au moment de l'appui).
//
// Contrat avec l'interface : texte terminé par un zéro, de 6 chiffres, vide tant que le serveur de la page n'a pas démarré (pas
// d'adresse IP). Le code ne va ni au journal, ni dans un fichier, ni dans une réponse de l'API. Ce fichier est PUR (aucun en-tête
// ESP-IDF) : main/appairage.cpp se teste sur le PC (test/host/test_appairage_ecran.cpp).
#pragma once

#include <cstdint>

#include "ecran/appairage.hpp"

// Le code d'appairage courant, ou « » s'il n'existe pas encore. Le texte rendu vit dans un tampon de ce module, réécrit à chaque
// appel : à lire depuis la seule tâche de l'interface.
const char *appairage_jeton();

namespace ecran {

// Bouton « Nouveau code » (tâche de l'interface) : `aleatoire` donne le nouveau code (ecran::appairage::ecrire_code), affiché tout
// de suite. Faux, et rien ne change, tant que le serveur n'a pas publié de premier code (pas de page, donc rien à renouveler).
bool appairage_demander_nouveau_code(std::uint32_t aleatoire);

// La même demande avec un nombre tiré par le générateur matériel (esp_random). Définie dans main/serveur_web.cpp, pour garder ce
// module pur et testable sur le PC.
bool appairage_demander_nouveau_code();

// Fil du serveur seulement : applique une demande en attente (nouveau code, sessions fermées, verrou levé), puis publie le code
// du gestionnaire s'il a changé depuis la dernière publication (démarrage, bouton, série d'essais ratés).
void appairage_synchroniser(appairage::Gestionnaire &g);

} // namespace ecran
