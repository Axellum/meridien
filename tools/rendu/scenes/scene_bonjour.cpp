// Scène « bonjour » : l'écran du jalon 1b (main/ui_bonjour.cpp), compilé tel quel. Données fixées à la place du temps réel.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_bonjour.cpp main/horloge.cpp
// rendu-langues: fr
// rendu-tailles: 720x1280 1280x720
// rendu-sequence: bonjour
//
// Fixtures (inventées, pas des mesures) : démarrage n° 42, cause « mise sous tension », carte prête après 612 ms. L'heure vient
// de l'instant fixé par le banc ; l'état du réseau est celui de la variante. Cet écran n'est pas traduit (français seulement).
#include "banc.h"
#include "boot_info.h"
#include "reseau.h"
#include "ui_bonjour.h"

namespace {

ecran::EtatReseau g_etat = {};

void construire() {
    const ecran::BootInfo boot = {42, true, "mise sous tension"};
    ecran::ui_bonjour_creer(boot);
}

// Wi-Fi associé, adresse obtenue, heure reçue par SNTP : l'écran de tous les jours.
void connecte(const banc::Contexte &) {
    g_etat = {};
    g_etat.identifiants_presents = true;
    g_etat.coprocesseur_pret = true;
    g_etat.wifi_connecte = true;
    g_etat.ip_obtenue = true;
    g_etat.heure_synchronisee = true;
    construire();
}

// Wi-Fi en cours de connexion, pas encore d'heure : ce que l'on voit les premières secondes après le démarrage.
void attente(const banc::Contexte &) {
    g_etat = {};
    g_etat.identifiants_presents = true;
    g_etat.coprocesseur_pret = true;
    construire();
}

} // namespace

namespace ecran {

// Réseau fictif : le vrai (main/reseau.cpp) lance le Wi-Fi par le C6 et SNTP, impossible sur le PC.
EtatReseau reseau_etat() { return g_etat; }

} // namespace ecran

namespace banc {

const Scene kScenes[] = {
    {"connecte", "Connecté : heure reçue du réseau", connecte},
    {"attente", "Au démarrage : heure en attente", attente},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
