// La dernière mer lue de Vigie (trame d'un spot), partagée entre la tâche réseau (mer_client.cpp, qui l'écrit) et l'interface (qui la
// lira : page mer, lots suivants). Aucun en-tête ESP-IDF ici : seulement les types du noyau (ecran/mer_trame.hpp, mer_spots.hpp).
//
// Même schéma que vigie_etat.h : deux tampons en PSRAM, la tâche réseau décode dans celui de TRAVAIL, `mer_etat_publier` l'échange avec
// le tampon PUBLIÉ en un instant (échange de deux pointeurs sous le verrou). L'interface prend un `MerLecture` : tant qu'il existe, le
// tampon publié ne change pas.
//
// La mer publiée porte l'identifiant du spot pour lequel elle a été demandée : `mer::Mer` ne le contient pas (la ligne SEA donne le nom
// et la commune, pas l'identifiant). Une page qui montre la mer d'un spot vérifie que cet identifiant est celui du spot choisi.
#pragma once

#include <cstdint>

#include "ecran/mer_spots.hpp"
#include "ecran/mer_trame.hpp"

namespace ecran {

struct MerPubliee {
    mer::Mer mer;
    char spot[mer::ID_OCTETS] = {}; // l'identifiant du spot demandé (mer::spot_valide)
};

// Tampon de travail de la tâche réseau (PSRAM, alloué au premier appel, jamais rendu), ou nullptr si la PSRAM manque. Il n'a aucun
// lecteur : la tâche réseau y décode la trame, puis le publie.
MerPubliee *mer_etat_travail();

// Publie le tampon de travail : il devient la mer courante, reçue à `recu_s` (secondes UNIX, horloge de l'appareil). L'ancienne mer
// devient le tampon de travail. Attend la fin d'une lecture en cours (quelques dizaines de millisecondes au plus : dessiner la page).
void mer_etat_publier(std::int64_t recu_s);

// Oublie la mer courante (spot, plage de vent ou adresse changés ; plus de spot choisi) : l'interface ne montre plus rien tant qu'une
// nouvelle mer n'est pas là.
void mer_etat_oublier();

// Lecture protégée de la mer courante. Courte et sur la tâche de l'interface : ne pas la garder pendant une attente.
class MerLecture {
  public:
    MerLecture();
    ~MerLecture();
    MerLecture(const MerLecture &) = delete;
    MerLecture &operator=(const MerLecture &) = delete;

    bool valide() const { return publiee_ != nullptr; }   // une mer lisible est en mémoire
    const mer::Mer &mer() const { return publiee_->mer; } // seulement si valide()
    const char *spot() const { return publiee_->spot; }   // seulement si valide()
    std::int64_t recu_s() const { return recu_s_; }

  private:
    const MerPubliee *publiee_ = nullptr;
    std::int64_t recu_s_ = 0;
};

} // namespace ecran
