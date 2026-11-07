// La dernière réponse lue de Vigie (alertes officielles), partagée entre la tâche réseau (vigie_client.cpp, qui l'écrit) et
// l'interface (qui la lit : bandeau de l'écran principal, page des alertes). Aucun en-tête ESP-IDF ici : seulement les types du
// noyau (ecran/vigie_alertes.hpp).
//
// Pourquoi deux tampons : `vigie::Alertes` pèse ≈ 70 Ko (12 alertes de trois textes de l'émetteur chacune) : trop pour la pile d'une
// tâche, trop pour la RAM interne. Les deux tampons vivent en PSRAM. La tâche réseau lit la réponse dans le tampon de TRAVAIL ;
// `vigie_etat_publier` l'échange avec le tampon PUBLIÉ en un instant (échange de deux pointeurs sous le verrou), sans copier 70 Ko
// pendant que l'interface lit. L'interface prend un `VigieLecture` : tant qu'il existe, le tampon publié ne change pas.
#pragma once

#include <cstdint>

#include "ecran/vigie_alertes.hpp"

namespace ecran {

// Tampon de travail de la tâche réseau (PSRAM, alloué au premier appel, jamais rendu), ou nullptr si la PSRAM manque. Il n'a aucun
// lecteur : la tâche réseau y lit la réponse, puis le publie.
vigie::Alertes *vigie_etat_travail();

// Publie le tampon de travail : il devient la réponse courante, reçue à `recu_s` (secondes UNIX, horloge de l'appareil ; l'âge de
// la réponse en dépend : voir ecran/vigie_vue.hpp). L'ancienne réponse devient le tampon de travail. Attend la fin d'une lecture
// en cours (quelques dizaines de millisecondes au plus : dessiner la page).
void vigie_etat_publier(std::int64_t recu_s);

// Oublie la réponse courante (lieu, adresse ou langue changés ; alertes désactivées) : l'interface ne montre plus rien tant
// qu'une nouvelle réponse n'est pas là.
void vigie_etat_oublier();

// Lecture protégée de la réponse courante. Courte et sur la tâche de l'interface : ne pas la garder pendant une attente.
class VigieLecture {
  public:
    VigieLecture();
    ~VigieLecture();
    VigieLecture(const VigieLecture &) = delete;
    VigieLecture &operator=(const VigieLecture &) = delete;

    bool valide() const { return alertes_ != nullptr; }         // une réponse lisible est en mémoire
    const vigie::Alertes &alertes() const { return *alertes_; } // seulement si valide()
    std::int64_t recu_s() const { return recu_s_; }

  private:
    const vigie::Alertes *alertes_ = nullptr;
    std::int64_t recu_s_ = 0;
};

} // namespace ecran
