// Serveur de la page de réglages (docs/api-reglages.md, ADR-0003) : sert web/app (embarquée en gzip) et l'API sur le port 80,
// en mode station seulement (aucun point d'accès, aucun portail captif : jalon « réglages »), et annonce `ecran-xxxx.local`
// par mDNS (xxxx = les 4 derniers caractères hexadécimaux de l'adresse MAC Wi-Fi ; elle n'est jamais écrite dans le dépôt).
//
// Le sous-ensemble de l'API est celui de ecran/api_reglages.hpp. Les réglages enregistrés par la page sont à lire dans
// reglages.h (affichage) et meteo_zone.h (ville). Le code d'appairage est dans appairage.h.
#pragma once

namespace ecran {

// Lance la tâche du serveur et rend la main tout de suite. Elle attend l'adresse IP (reseau_etat().ip_obtenue), puis démarre le
// serveur HTTP, mDNS, tire le code d'appairage (appairage_jeton()) et pose le bandeau « adresse + code » sur l'écran.
// À appeler une fois, après reseau_demarrer() et après la création de l'interface.
void serveur_web_demarrer();

} // namespace ecran
