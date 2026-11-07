// Ce que la page de réglages et l'installation guidée (ADR-0007) demandent au réseau de l'application : chercher les réseaux Wi-Fi,
// en essayer un nouveau, et reprendre l'ancien si le nouveau ne répond pas. Défini dans main/reseau.cpp (jalon 1b). Les noms et
// les types sont ceux proposés par la session reglages-ecran (son bouchon main/bouchons/reseau_wifi.h) : l'écran n'a rien à changer.
//
// Règles :
//  - Rien n'est bloquant : l'écran ne doit jamais attendre le Wi-Fi. Les fonctions rendent la main tout de suite (elles ne font
//    que poster un message à la boucle d'événements du système, où tout le travail se fait).
//  - Aucun identifiant n'est écrit au journal, ni en clair ni en clair partiel (SSID et mot de passe : jamais).
//  - Le mot de passe n'est gardé que le temps de l'essai (puis il devient celui du réseau courant, en mémoire vive seulement) ; la
//    mémoire permanente est tenue par l'hôte des réglages (main/reglages.*, main/wifi_stocke.*), pas par le réseau : sur
//    « Connecte », l'hôte enregistre le nouveau réseau ; sur tout autre résultat, rien n'est écrit.
//  - Le retour arrière est fait ICI, pas par l'écran : un essai raté ou abandonné ramène l'ancien réseau (s'il y en avait un).
// C++17, aucun include ESP-IDF dans cet en-tête.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/changement_wifi.hpp"
#include "ecran/points_acces.hpp" // ecran::PointAcces { ssid[33], rssi_dbm, securise }

namespace ecran {

// ---- recherche -----------------------------------------------------------------------------------------------------------
// Lance une recherche (une à la fois : sans effet s'il y en a déjà une, si un essai de connexion est en cours, ou si le Wi-Fi n'est
// pas démarré). Elle dure de 1 à 3 s. Pendant la recherche le lien du réseau actuel peut ralentir ou se couper un instant : la
// couche réseau le reprend toute seule (réessai toutes les 2 s). Une recherche sans réponse de la radio est abandonnée au bout de 15 s.
void reseau_scanner_lancer();
bool reseau_scanner_en_cours();
// Copie les réseaux trouvés par la dernière recherche terminée : du plus fort au plus faible, sans doublon de SSID, sans réseau
// caché, au plus `max` (la couche réseau en garde 24 au plus). Rend le nombre copié (zéro tant qu'aucune recherche n'a abouti).
std::size_t reseau_scanner_resultats(PointAcces *sortie, std::size_t max);

// ---- connexion à un nouveau réseau ------------------------------------------------------------------------------------------
// `contexte` est rendu tel quel. Appelé UNE fois par essai (jamais après reseau_abandonner), depuis la tâche d'événements du système
// (« sys_evt », pas la tâche d'interface) : le rappel ne fait que noter le résultat (pas de LVGL, pas d'attente, pile courte).
using SurResultatConnexion = void (*)(wifi::Resultat resultat, void *contexte);

// Essaie ce réseau ; `mot_de_passe` nul ou vide pour un réseau ouvert (8 à 63 caractères sinon). Rend faux si l'essai n'a pas pu
// démarrer (un essai est déjà en cours — le retour arrière compris, jusqu'au rappel —, Wi-Fi pas démarré, SSID vide ou de plus de 32
// octets, mot de passe de 1 à 7 ou de plus de 63 caractères) : le rappel n'est alors pas appelé.
// Résultats : Connecte (adresse IP obtenue), MotDePasseRefuse (la radio a refusé l'authentification ou la poignée de main : même
// cause sûre vue deux fois, ou dernière cause sûre vue au délai), ReseauIntrouvable (aucun point d'accès de ce nom, idem), Autre
// (tout le reste, y compris le délai de ecran::wifi::DELAI_CONNEXION_MS écoulé sans aucune cause sûre : sur le point d'accès essayé,
// un mot de passe faux ne donne pas toujours la raison 202 avant le délai, voir docs/mesures/wifi-retour-arriere.md). Sur un échec,
// l'ancien réseau est repris et le rappel n'est appelé qu'après son retour (adresse IP : ≈ 6 s mesurées) ou, s'il tarde, au bout de
// ecran::wifi::DELAI_RETOUR_MS (le Wi-Fi continue de le chercher). Un échec que seul le délai a détecté arrive donc à ≈ 36 s, après
// le compte à rebours de 30 s de l'écran. Sans ancien réseau (appareil neuf), le rappel est appelé tout de suite.
bool reseau_connecter(const char *ssid, const char *mot_de_passe, SurResultatConnexion sur_resultat, void *contexte);

// L'écran n'a pas eu de réponse dans son délai (ecran::wifi::DELAI_CONNEXION_MS, 30 s) ou la personne a touché « Retour » /
// « Annuler » : abandonner l'essai et reprendre l'ancien réseau. Sans effet si aucun essai n'est en cours. Le rappel de l'essai
// abandonné n'est plus appelé. L'essai se termine de toute façon tout seul au bout de DELAI_CONNEXION_MS.
void reseau_abandonner();

// Le réseau auquel l'appareil est associé (pour « Connecté à « Maison_5G » » sur la page Réseau) ; chaîne vide si l'appareil n'est
// associé à aucun réseau, ou pendant un essai de connexion. Ce n'est pas un secret : c'est le seul identifiant que l'écran affiche
// (ADR-0007). Rend le nombre d'octets de l'SSID. Le mot de passe, lui, ne sort jamais.
std::size_t reseau_ssid_courant(char *sortie, std::size_t capacite);

} // namespace ecran
