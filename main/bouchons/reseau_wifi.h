// Ce que la page de réglages et l'installation guidée (ADR-0007) demandent au réseau de l'application : chercher les réseaux Wi-Fi,
// en essayer un nouveau, et reprendre l'ancien si le nouveau ne répond pas. À FOURNIR par main/reseau.* (jalon-1b, propriétaire de
// ces fichiers) : cette interface est une PROPOSITION de reglages-ecran, pas du code de reseau.cpp.
//
// BOUCHON : tant que main/reseau_wifi.h n'existe pas, main/bouchons/reseau_wifi_bouchon.cpp répond « rien trouvé » et « impossible ».
// Quand jalon-1b livre le vrai, il déplace ces déclarations dans main/reseau_wifi.h (ou reseau.h) et supprime le dossier bouchons/.
//
// Règles :
//  - Rien n'est bloquant : l'écran ne doit jamais attendre le Wi-Fi. Les fonctions rendent la main tout de suite.
//  - Aucun identifiant n'est écrit au journal, ni en clair ni en clair partiel (SSID et mot de passe : jamais).
//  - Le mot de passe n'est lu que le temps de l'essai ; la mémoire permanente est tenue par l'hôte des réglages (main/reglages.*),
//    pas par le réseau : sur « Connecte », l'hôte enregistre le nouveau réseau ; sur tout autre résultat, rien n'est écrit.
//  - Le retour arrière est fait ICI, pas par l'écran : un essai raté ou abandonné ramène l'ancien réseau (s'il y en avait un).
// C++17, aucun include ESP-IDF dans cet en-tête.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/changement_wifi.hpp"

namespace ecran {

// Un réseau vu à la recherche.
struct PointAcces {
    char ssid[33];   // octets, terminé par un zéro (jamais vide : les réseaux cachés ne sont pas listés)
    int8_t rssi_dbm; // par exemple -52 ; l'écran en tire 0 à 4 barres
    bool securise;   // faux : réseau ouvert (pas de mot de passe à demander)
};

// ---- recherche -----------------------------------------------------------------------------------------------------------
// Lance une recherche (une à la fois : sans effet s'il y en a déjà une). Pendant la recherche le lien du réseau actuel peut
// se couper quelques secondes : à la charge de reseau.* de le reprendre.
void reseau_scanner_lancer();
bool reseau_scanner_en_cours();
// Copie les réseaux trouvés par la dernière recherche terminée : du plus fort au plus faible, sans doublon de SSID, au plus `max`.
// Rend le nombre copié (zéro tant qu'aucune recherche n'a abouti).
std::size_t reseau_scanner_resultats(PointAcces *sortie, std::size_t max);

// ---- connexion à un nouveau réseau ------------------------------------------------------------------------------------------
// `contexte` est rendu tel quel. Appelé UNE fois par essai (jamais après reseau_abandonner), depuis la tâche réseau : le
// rappel ne fait que noter le résultat (pas de LVGL, pas d'attente).
using SurResultatConnexion = void (*)(wifi::Resultat resultat, void *contexte);

// Essaie ce réseau ; `mot_de_passe` nul ou vide pour un réseau ouvert. Rend faux si l'essai n'a pas pu démarrer (un essai est déjà
// en cours, lien avec le coprocesseur Wi-Fi absent) : le rappel n'est alors pas appelé.
// Résultats : Connecte (adresse IP obtenue), MotDePasseRefuse (échec d'authentification), ReseauIntrouvable (aucune réponse
// du point d'accès), Autre (tout le reste). Sur un échec, l'ancien réseau est repris avant le rappel.
// NON VÉRIFIÉ : que le C6 (esp_hosted) transmette bien au P4 la raison de l'échec ; sinon « Autre » partout et l'écran dira
// seulement « Connexion impossible » (le texte existe).
bool reseau_connecter(const char *ssid, const char *mot_de_passe, SurResultatConnexion sur_resultat, void *contexte);

// L'écran n'a pas eu de réponse dans son délai (ecran::wifi::DELAI_CONNEXION_MS, 30 s, à régler par mesure sur la carte) ou la
// personne a touché « Retour » / « Annuler » : abandonner l'essai et reprendre l'ancien réseau. Sans effet si aucun essai n'est
// en cours. Le rappel de l'essai abandonné n'est plus appelé.
void reseau_abandonner();

// Le réseau auquel l'appareil est associé (pour « Connecté à « Maison_5G » » sur la page Réseau) ; chaîne vide s'il n'y en a
// pas. Ce n'est pas un secret : c'est le seul identifiant que l'écran affiche. (reseau.h écrivait « jamais l'SSID à l'écran » : la
// page de réglages change cette règle, décision ADR-0007 à confirmer par jalon-1b.)
std::size_t reseau_ssid_courant(char *sortie, std::size_t capacite);

} // namespace ecran
