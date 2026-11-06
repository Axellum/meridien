// Noyau pur : du `Releve` d'Open-Meteo à ce que l'appareil montre (température, min/max du jour, libellé français, « périmé »).
// C++17 standard, aucun include ESP-IDF, pas de flottant. `main/meteo_etat.cpp` n'ajoute que le passage des dixièmes aux
// `float` du contrat MeteoEtat et la copie dans la structure.
//
// Règle « périmé » de l'appareil (choix de cette session, à valider par Axel) :
//   - la réponse a plus de 2 h (DELAI_PERIME_APPAREIL_S) : périmée, même si les essais ne sont pas encore en échec ;
//   - 3 essais ratés de suite (ECHECS_POUR_PERIME) ET une réponse de plus de 30 min (AGE_PERIME_PAR_ECHECS_S, deux
//     rafraîchissements manqués) : périmée. L'âge évite d'alerter pour une box redémarrée il y a 4 minutes ;
//   - l'heure de l'appareil n'est pas réglée, ou a reculé de plus de 5 min depuis la réponse : on ne sait pas juger,
//     donc périmée (jamais « à jour » sur une supposition). Au démarrage, la météo du cache s'affiche donc marquée
//     périmée jusqu'à la première réponse SNTP.
// `valide` ne dépend pas de la fraîcheur : une météo périmée reste lisible, marquée `perime`.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_openmeteo.hpp"
#include "ecran/meteo_wmo.hpp"

namespace ecran::meteo {

inline constexpr std::int64_t DELAI_PERIME_APPAREIL_S = 2 * 3600;
inline constexpr int ECHECS_POUR_PERIME = 3;
inline constexpr std::int64_t AGE_PERIME_PAR_ECHECS_S = 30 * 60;

constexpr bool est_perime(const Releve &r, std::int64_t maintenant_s, int echecs_consecutifs) noexcept {
    if (fraicheur(r, maintenant_s, DELAI_PERIME_APPAREIL_S) != Fraicheur::Frais)
        return true; // périmée, très périmée, ou horloge incertaine
    const std::int64_t age = maintenant_s > r.recu_a_s ? maintenant_s - r.recu_a_s : 0;
    return echecs_consecutifs >= ECHECS_POUR_PERIME && age > AGE_PERIME_PAR_ECHECS_S;
}

struct Synthese {
    bool perime = false;
    std::int64_t maj_s = 0; // quand la réponse a été reçue
    std::int16_t temperature_dixiemes = 0;
    std::int16_t code_wmo = 0;
    bool jour = false;
    bool minmax_connus =
        false; // faux : pas de prévision pour ce jour ; min et max valent alors la température du moment
    std::int16_t min_dixiemes = 0;
    std::int16_t max_dixiemes = 0;
    std::string_view libelle{}; // texte statique (UTF-8), jamais un code
};

// `maintenant_s` : l'heure de l'appareil (UNIX, UTC). Le jour des min/max est celui qui contient `maintenant_s` ; à défaut
// (cache d'il y a plus de sept jours), celui de la mesure.
constexpr Synthese synthetiser(const Releve &r, std::int64_t maintenant_s, int echecs_consecutifs) noexcept {
    Synthese s;
    s.perime = est_perime(r, maintenant_s, echecs_consecutifs);
    s.maj_s = r.recu_a_s;
    s.temperature_dixiemes = r.temperature_dixiemes;
    s.code_wmo = r.code_wmo;
    s.jour = r.jour;
    s.libelle = decrire(r.code_wmo, r.jour).libelle;
    int i = index_du_jour(r, maintenant_s);
    if (i < 0)
        i = index_du_jour(r, r.instant_s);
    if (i >= 0) {
        s.minmax_connus = true;
        s.min_dixiemes = r.jours[i].min_dixiemes;
        s.max_dixiemes = r.jours[i].max_dixiemes;
    } else {
        s.min_dixiemes = r.temperature_dixiemes;
        s.max_dixiemes = r.temperature_dixiemes;
    }
    return s;
}

// Dixièmes -> texte français « -0,5 », « 11,6 » (virgule décimale, signe moins ASCII). Rend la longueur sans le zéro final, ou
// 0 si `capacite` est trop petite (rien n'est alors garanti dans `sortie`). Sert aux journaux : pas de printf("%f").
constexpr std::size_t ecrire_dixiemes(std::int32_t dixiemes, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr)
        return 0;
    std::int64_t a = dixiemes;
    const bool negatif = a < 0;
    if (negatif)
        a = -a;
    char chiffres[12] = {};
    int nb = 0;
    std::int64_t entier = a / 10;
    do {
        chiffres[nb++] = static_cast<char>('0' + entier % 10);
        entier /= 10;
    } while (entier > 0);
    const std::size_t besoin =
        (negatif ? 1U : 0U) + static_cast<std::size_t>(nb) + 2U + 1U; // signe, entier, « ,d », zéro
    if (capacite < besoin)
        return 0;
    std::size_t n = 0;
    if (negatif)
        sortie[n++] = '-';
    while (nb > 0)
        sortie[n++] = chiffres[--nb];
    sortie[n++] = ',';
    sortie[n++] = static_cast<char>('0' + a % 10);
    sortie[n] = '\0';
    return n;
}

} // namespace ecran::meteo
