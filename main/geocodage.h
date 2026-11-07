// Recherche de lieu par son nom, côté appareil (géocodage d'Open-Meteo, ADR-0003 : l'appareil interroge lui-même le service).
// Deux clients, UN SEUL téléchargement à la fois, jamais dans la tâche LVGL :
//   - l'écran (réglages, installation guidée) : `geocodage_chercher` rend la main tout de suite ; il lit ensuite l'état et les
//     résultats à chaque rafraîchissement. Les noms suivent l'interface des réglages de l'écran (hote_reglages.h) :
//       lieu_chercher(texte) -> geocodage_chercher      lieu_recherche() -> geocodage_etat      lieu_resultats() -> geocodage_resultats
//   - la page web (main/serveur_web.cpp, GET /api/geocode) : `geocodage_telecharger` attend la réponse, mais le calcul TLS se fait
//     dans la tâche du module (priorité d'arrière-plan), pas dans celle du serveur HTTP.
// Le téléchargement prend le verrou TLS commun (verrou_tls.h) : une recherche de ville et une requête météo ne se déroulent jamais
// en même temps. Toute la logique de lecture et d'état est dans le noyau pur (ecran/geocodage.hpp, testé sur le PC).
//
// Sécurité : seule l'adresse https://geocoding-api.open-meteo.com/ est joignable d'ici (aucune adresse venue de la page ou de
// l'écran n'est suivie telle quelle), certificats vérifiés, réponse bornée à 16 Ko. Le texte cherché n'est pas écrit au journal.
#pragma once

#include <cstddef>
#include <string_view>

#include "ecran/api_reglages.hpp"
#include "ecran/geocodage.hpp"

namespace ecran {

// ---- pour l'écran ---------------------------------------------------------------------------------------------------------------
// Lance la recherche de `texte` (3 à 80 caractères ; sinon la recherche est « terminée » sans résultat). Ne bloque pas. Une recherche
// déjà en cours est remplacée : la dernière demande gagne.
void geocodage_chercher(std::string_view texte);

// Inactive (rien demandé) / EnCours / Terminee (résultats lisibles, éventuellement zéro) / SansConnexion (pas d'adresse IP, service
// injoignable, verrou TLS occupé trop longtemps, réponse illisible : la cause exacte est au journal).
geocodage::Etat geocodage_etat();

// Copie au plus `max` lieux de la recherche terminée dans `sortie` ; rend leur nombre (zéro si elle n'est pas terminée).
std::size_t geocodage_resultats(geocodage::Lieu *sortie, std::size_t max);

// ---- pour la page web -----------------------------------------------------------------------------------------------------------
// Télécharge `adresse` (doit commencer par https://geocoding-api.open-meteo.com/) dans `tampon` (`capacite` octets) et attend la fin,
// 30 s au plus. `taille` reçoit le nombre d'octets lus. Une seule demande de page à la fois (le serveur n'a qu'un fil).
api::Telechargement geocodage_telecharger(std::string_view adresse, char *tampon, std::size_t capacite,
                                          std::size_t &taille);

} // namespace ecran
