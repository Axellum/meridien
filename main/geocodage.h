// Recherche de lieu par son nom, côté appareil (géocodage d'Open-Meteo, ADR-0003 : l'appareil interroge lui-même le service).
// LE SEUL chemin HTTPS vers le géocodage du firmware. Deux clients, UN SEUL téléchargement à la fois, jamais dans la tâche LVGL :
//   - l'écran (réglages, installation guidée ; ports `telecharger_*` de main/hote_appareil_esp.cpp) : `geocodage_ecran_lancer`
//     rend la main tout de suite ; l'écran lit ensuite l'état et la réponse brute à chaque rafraîchissement. La lecture de la
//     réponse et les textes affichés sont dans le noyau (ecran/hote_appareil.hpp, ecran/lieux_geocodage.hpp).
//   - la page web (main/serveur_web.cpp, GET /api/geocode) : `geocodage_telecharger` attend la réponse, mais le calcul TLS se fait
//     dans la tâche du module (priorité d'arrière-plan), pas dans celle du serveur HTTP.
// Le téléchargement prend le verrou TLS commun (verrou_tls.h) : une recherche de ville et une requête météo ne se déroulent jamais
// en même temps.
//
// Sécurité : seule l'adresse https://geocoding-api.open-meteo.com/ est joignable d'ici (aucune adresse venue de la page ou de
// l'écran n'est suivie telle quelle), certificats vérifiés, réponse bornée à 16 Ko. Le texte cherché n'est pas écrit au journal.
#pragma once

#include <cstddef>
#include <string_view>

#include "ecran/api_reglages.hpp"
#include "ecran/hote_appareil.hpp"

namespace ecran {

// ---- pour l'écran ---------------------------------------------------------------------------------------------------------------
// Lance le téléchargement de `adresse` (doit commencer par https://geocoding-api.open-meteo.com/). Ne bloque pas. Une demande
// encore en cours est remplacée : la dernière gagne, la réponse d'une demande remplacée est jetée.
void geocodage_ecran_lancer(const char *adresse);
// Inactif (rien demandé) / EnCours / Termine (réponse lisible) / SansInternet (pas d'adresse IP, service injoignable, verrou TLS
// occupé trop longtemps) / Echec (adresse refusée, réponse refusée ou trop grande, mémoire).
EtatTelechargement geocodage_ecran_etat();
// La réponse brute si l'état est Termine (vide sinon), valable jusqu'au prochain `geocodage_ecran_lancer` ou `_liberer`.
std::string_view geocodage_ecran_corps();
// Oublie la demande et sa réponse.
void geocodage_ecran_liberer();

// ---- pour la page web -----------------------------------------------------------------------------------------------------------
// Télécharge `adresse` (doit commencer par https://geocoding-api.open-meteo.com/) dans `tampon` (`capacite` octets) et attend la fin,
// 30 s au plus. `taille` reçoit le nombre d'octets lus. Une seule demande de page à la fois (le serveur n'a qu'un fil).
api::Telechargement geocodage_telecharger(std::string_view adresse, char *tampon, std::size_t capacite,
                                          std::size_t &taille);

} // namespace ecran
