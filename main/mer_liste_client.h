// Lecture, par l'appareil, de la liste des spots de Vigie (ADR-0026) : `GET <adresse>/v1/surf/spots`, ≈ 20 Ko bruts et ≈ 4,7 Ko gzippés
// (51 spots, relevé du 08/11/2026). C'est ce que l'écran « La mer » des réglages montre quand on touche « Ajouter un spot » : la personne
// choisit dans la liste, sans téléphone ni clavier de saisie d'identifiant.
//
// Les noms suivent l'interface des réglages de l'écran (hote_reglages.hpp) :
//   mer_liste_demander() -> mer_liste_lecture_lancer     mer_liste_etat() -> mer_liste_lecture_etat    mer_liste_copier() -> mer_liste_lecture_copier
//
// La lecture est un pas de LA tâche réseau des données (tache_donnees.h : pas de pile à elle, la RAM interne manque,
// docs/mesures/memoire-interne.md), qui la fait passer avant la météo, les alertes et la mer dès la fin du pas en cours. L'aller-retour passe par le client commun
// (http_commun.h : gzip, tampons en PSRAM, verrou TLS), l'adresse est celle des réglages des alertes (reglages_alertes.h, instance publique
// par défaut) : un seul serveur pour les alertes, la mer et cette liste. La lecture et les règles sont dans le noyau pur
// (ecran/mer_liste.hpp, testé sur le PC) ; ici : la tâche, le réseau, le verrou.
//
// Politesse envers l'instance publique : une liste lue il y a moins de 10 minutes est gardée telle quelle (rouvrir « Ajouter un spot » ne
// redemande rien), et après un échec on attend 5 secondes avant de réessayer (ecran/mer_liste.hpp : relire_la_liste). L'adresse n'est
// jamais écrite au journal.
#pragma once

#include "ecran/hote_reglages.hpp"
#include "ecran/mer_liste.hpp"

namespace ecran {

// Lance la lecture. Ne bloque pas. Sans effet si une lecture est en cours, si la liste gardée est encore bonne, ou pendant la pause qui suit un
// échec. Sans réseau : l'état passe tout de suite à `SansConnexion`.
void mer_liste_lecture_lancer();

// Inactive (rien demandé) / EnCours / Terminee (liste prête) / Vide / SansConnexion / Echec.
ListeMer mer_liste_lecture_etat();

// Copie la liste lue dans `sortie` (≈ 16,6 Ko : en PSRAM chez l'appelant). Faux, `sortie` inchangée, tant que l'état n'est pas `Terminee`.
bool mer_liste_lecture_copier(mer::liste::Liste &sortie);

} // namespace ecran
