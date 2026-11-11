// Noyau pur : l'ordonnanceur de LA tâche réseau des données (météo, alertes de Vigie, mer du spot, liste des spots). Aucune tâche,
// aucun minuteur ici : main/tache_donnees.cpp attend, réveille et appelle ; ce fichier dit QUI passe et QUAND.
//
// Pourquoi une seule tâche : les quatre clients ne font jamais deux requêtes à la fois (verrou TLS commun, main/verrou_tls.h), mais
// chacun gardait sa propre pile de 12 Ko en RAM interne. Chaque client garde son propre rythme : il ne fait qu'un « pas » à la fois
// (un essai, ou un simple contrôle des réglages) et rend le délai avant son pas suivant. L'ordonnanceur ne fait que les enchaîner.
//
// Règles (chaque règle est un test de test/host/test_ordonnanceur_reseau.cpp) :
//   - une source endormie (« jamais ») ne passe pas ; un réveil la rend due après son anti-rebond (0 ms : tout de suite) ;
//   - un réveil n'éloigne jamais un pas déjà prévu plus tôt, et une rafale de réveils ne repousse pas l'échéance (la première compte) ;
//   - parmi les sources dues, une source PRIORITAIRE (une personne attend devant l'écran : la liste des spots) passe d'abord ;
//     sinon la plus en retard (échéance la plus ancienne), puis l'ordre de la table : aucune source ne peut en affamer une autre ;
//   - un pas qui rend « plus rien » endort la source ; un délai négatif vaut 0 ;
//   - l'attente de la tâche va jusqu'à la prochaine échéance, bornée par un plafond (jamais une attente sans fin).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstdint>

namespace ecran::ordonnanceur {

// Les sources, dans l'ordre de la table (départage des égalités).
enum class Source : std::uint8_t {
    Meteo = 0,
    Vigie = 1,
    Mer = 2,
    ListeSpots = 3,
};
constexpr int kNombreSources = 4;

constexpr std::int64_t kJamais = INT64_MAX; // source endormie : seul un réveil la fait passer
constexpr std::int64_t kPlusRien = -1;      // rendu par un pas : « rien à faire tant qu'on ne me réveille pas »
constexpr int kAucune = -1;                 // `choisir` : aucune source n'est due

constexpr int indice(Source s) { return static_cast<int>(s); }

// Ce que chaque source attend d'un réveil.
struct Regle {
    std::int64_t
        anti_rebond_ms; // un réveil rend la source due au plus tard après ce délai (plusieurs réveils = un seul pas)
    bool prioritaire;   // une personne attend la réponse devant l'écran
};

// Météo : 1,5 s d'anti-rebond après un changement de zone ou de source (meteo_client.cpp, avant : kAntiRebondZoneMs).
// Vigie et mer : relisent leurs réglages tout de suite ; leur propre anti-rebond est dans leurs règles (vigie_planning.hpp,
// mer_requete.hpp). Liste des spots : demandée à l'écran, une personne attend.
constexpr Regle kRegles[kNombreSources] = {
    {1500, false}, // Meteo
    {0, false},    // Vigie
    {0, false},    // Mer
    {0, true},     // ListeSpots
};

struct Ordonnanceur {
    std::int64_t echeance_ms[kNombreSources] = {kJamais, kJamais, kJamais, kJamais};
    bool reveillee[kNombreSources] = {}; // un réveil est arrivé depuis le dernier pas de la source

    // Un réveil de `s` à l'instant `maintenant_ms` (changement de réglage, demande de l'écran, démarrage).
    constexpr void reveiller(Source s, std::int64_t maintenant_ms) {
        const int i = indice(s);
        const std::int64_t au_plus_tard = maintenant_ms + kRegles[i].anti_rebond_ms;
        if (au_plus_tard < echeance_ms[i]) {
            echeance_ms[i] = au_plus_tard;
        }
        reveillee[i] = true;
    }

    // La source qui doit passer maintenant, ou kAucune.
    constexpr int choisir(std::int64_t maintenant_ms) const {
        int choisie = kAucune;
        for (int i = 0; i < kNombreSources; ++i) {
            if (echeance_ms[i] == kJamais || echeance_ms[i] > maintenant_ms) {
                continue;
            }
            if (choisie == kAucune) {
                choisie = i;
                continue;
            }
            const bool p = kRegles[i].prioritaire;
            const bool pc = kRegles[choisie].prioritaire;
            if (p != pc) {
                if (p) {
                    choisie = i;
                }
                continue;
            }
            if (echeance_ms[i] < echeance_ms[choisie]) {
                choisie = i;
            }
        }
        return choisie;
    }

    // À appeler AVANT le pas de `s` : rend vrai si un réveil est arrivé depuis son pas précédent (et l'efface).
    constexpr bool prendre_reveil(Source s) {
        const int i = indice(s);
        const bool r = reveillee[i];
        reveillee[i] = false;
        return r;
    }

    // À appeler APRÈS le pas de `s`, avec ce qu'il a rendu : un délai en ms, ou kPlusRien. Un réveil arrivé PENDANT le pas reste
    // compté : il sera appliqué par `reveiller` quand la tâche lira ses notifications.
    constexpr void apres_pas(Source s, std::int64_t maintenant_ms, std::int64_t delai_ms) {
        const int i = indice(s);
        if (delai_ms == kPlusRien) {
            echeance_ms[i] = kJamais;
            return;
        }
        echeance_ms[i] = maintenant_ms + (delai_ms < 0 ? 0 : delai_ms);
    }

    // Combien de temps la tâche peut dormir (ms) : jusqu'à la prochaine échéance, entre 0 et `plafond_ms`.
    constexpr std::int64_t attente_ms(std::int64_t maintenant_ms, std::int64_t plafond_ms) const {
        std::int64_t attente = plafond_ms;
        for (int i = 0; i < kNombreSources; ++i) {
            if (echeance_ms[i] == kJamais) {
                continue;
            }
            const std::int64_t reste = echeance_ms[i] - maintenant_ms;
            if (reste < attente) {
                attente = reste;
            }
        }
        return attente < 0 ? 0 : attente;
    }
};

} // namespace ecran::ordonnanceur
