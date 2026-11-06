// Code d'appairage de la page de réglages (docs/api-reglages.md §2, ADR-0003). L'appareil tire un code à 6 chiffres à chaque
// démarrage ; la personne qui ouvre la page le lit sur l'écran de l'appareil et le tape pour recevoir un jeton de session. Rien
// n'est gardé en NVS : un redémarrage ferme les sessions et change le code.
//
// Contrat avec `ecran-principal` (qui l'affichera à l'écran principal) : texte terminé par un zéro, de 6 chiffres, vide tant
// que le serveur de la page n'a pas démarré (pas d'adresse IP). Valable jusqu'au prochain redémarrage. Le code ne va ni au
// journal, ni dans un fichier, ni dans une réponse de l'API. Ce fichier est PUR (aucun en-tête ESP-IDF).
#pragma once

// Le code d'appairage courant, ou « » s'il n'existe pas encore. Le pointeur reste valable pendant toute la vie du programme.
const char *appairage_jeton();

namespace ecran {

// Appelé une seule fois par le serveur de la page (main/serveur_web.cpp) quand il a tiré le code. `six_chiffres` : 6 chiffres.
void appairage_publier_code(const char *six_chiffres);

} // namespace ecran
