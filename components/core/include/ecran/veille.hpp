// Noyau pur : veille de l'écran. Décide, à partir du temps sans toucher, de l'heure locale et de plages horaires, si l'écran doit
// être ALLUMÉ, ATTÉNUÉ ou ÉTEINT. Ne touche à aucun matériel : la couche carte lit l'inactivité (LVGL) et l'heure, appelle `decider`,
// puis applique la décision (rétroéclairage, voile noir). C'est ce qui rend la logique testable sur le PC.
//
// Règles (chacune est un test de test/host/test_veille.cpp) :
//   1. `occupe` (l'assistant parle, un réveil sonne, une mise à jour tourne) : ALLUMÉ, rien d'autre ne compte.
//   2. Dans une plage horaire (heure connue seulement) : ÉTEINT une fois `rendormir_apres_s` écoulées sans toucher, ALLUMÉ avant.
//      Un appui dans la nuit allume l'écran, qui se rééteint tout seul quelques secondes après : on voit l'heure, on ne rallume
//      pas la chambre.
//   3. Hors plage : ÉTEINT après `eteindre_apres_s` sans toucher, sinon ATTÉNUÉ après `attenuer_apres_s`, sinon ALLUMÉ. 0 = jamais.
//   4. Heure inconnue (pas encore de SNTP ni d'horloge matérielle) : les plages sont ignorées, jamais d'extinction « par erreur ».
//
// Plage horaire : [debut_min, fin_min) en minutes depuis minuit (heure LOCALE), sur certains jours.
//   - debut < fin : la même journée ; debut > fin : passe minuit (22:00 -> 07:00) ; debut == fin : plage vide.
//   - `fin_min` peut valoir 1440 (« jusqu'à minuit ») ; `debut_min` va de 0 à 1439 ; 0:00 -> 24:00 = toute la journée.
//   - `jours` : bit 0 = dimanche ... bit 6 = samedi (même numérotation que `heure::HeureLocale::jour_semaine`). Une plage qui passe
//     minuit appartient au jour où elle COMMENCE : « vendredi 22:00 -> 07:00 » éteint aussi le samedi jusqu'à 07:00.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstdint>

namespace ecran::veille {

constexpr int kMinutesParJour = 1440;
constexpr int kPlagesMax = 4;
constexpr std::uint8_t kTousLesJours = 0x7F;

struct Plage {
    std::int16_t debut_min = 0;
    std::int16_t fin_min = 0;
    std::uint8_t jours = kTousLesJours;
};

constexpr bool plage_valide(const Plage &p) noexcept {
    return p.debut_min >= 0 && p.debut_min < kMinutesParJour && p.fin_min >= 0 && p.fin_min <= kMinutesParJour &&
           (p.jours & kTousLesJours) != 0;
}

constexpr bool jour_dans(std::uint8_t jours, int jour_semaine) noexcept {
    return jour_semaine >= 0 && jour_semaine <= 6 && ((jours >> jour_semaine) & 1u) != 0;
}

// `minute_du_jour` 0..1439, `jour_semaine` 0 (dimanche) .. 6 (samedi), tous deux en heure locale. Une plage invalide ne contient rien.
constexpr bool dans_la_plage(const Plage &p, int minute_du_jour, int jour_semaine) noexcept {
    if (!plage_valide(p) || minute_du_jour < 0 || minute_du_jour >= kMinutesParJour || jour_semaine < 0 ||
        jour_semaine > 6 || p.debut_min == p.fin_min) {
        return false;
    }
    if (p.debut_min < p.fin_min) {
        return jour_dans(p.jours, jour_semaine) && minute_du_jour >= p.debut_min && minute_du_jour < p.fin_min;
    }
    // Passe minuit : la partie du soir est au jour de départ, la partie du matin au lendemain de ce jour.
    const int veille = (jour_semaine + 6) % 7;
    return (jour_dans(p.jours, jour_semaine) && minute_du_jour >= p.debut_min) ||
           (jour_dans(p.jours, veille) && minute_du_jour < p.fin_min);
}

struct Reglage {
    std::uint32_t attenuer_apres_s = 0;   // 0 = jamais
    std::uint32_t eteindre_apres_s = 0;   // 0 = jamais
    std::uint32_t rendormir_apres_s = 15; // dans une plage : sans toucher pendant ce temps, l'écran se rééteint
    int niveau_attenue = 10;              // niveau perçu (1..100) d'un écran atténué
    Plage plages[kPlagesMax] = {};
    int nombre_plages = 0;
};

// Un réglage est utilisable si ses plages le sont et si l'atténuation vient AVANT l'extinction (sinon elle ne se verrait jamais).
constexpr bool reglage_valide(const Reglage &r) noexcept {
    if (r.nombre_plages < 0 || r.nombre_plages > kPlagesMax || r.niveau_attenue < 1 || r.niveau_attenue > 100) {
        return false;
    }
    if (r.attenuer_apres_s != 0 && r.eteindre_apres_s != 0 && r.attenuer_apres_s >= r.eteindre_apres_s) {
        return false;
    }
    for (int i = 0; i < r.nombre_plages; ++i) {
        if (!plage_valide(r.plages[i])) {
            return false;
        }
    }
    return true;
}

enum class Etat : std::uint8_t { Allume, Attenue, Eteint };
enum class Cause : std::uint8_t { Aucune, Occupe, Activite, Inactivite, PlageHoraire };

struct Entree {
    std::uint32_t inactif_s = 0; // secondes depuis le dernier toucher
    bool heure_connue = false;
    int minute_du_jour = 0; // heure locale, 0..1439
    int jour_semaine = 0;   // 0 = dimanche
    bool occupe = false;
};

struct Decision {
    Etat etat;
    Cause cause;
};

constexpr bool dans_une_plage(const Reglage &r, const Entree &e) noexcept {
    if (!e.heure_connue) {
        return false;
    }
    for (int i = 0; i < r.nombre_plages && i < kPlagesMax; ++i) {
        if (dans_la_plage(r.plages[i], e.minute_du_jour, e.jour_semaine)) {
            return true;
        }
    }
    return false;
}

constexpr Decision decider(const Reglage &r, const Entree &e) noexcept {
    if (e.occupe) {
        return {Etat::Allume, Cause::Occupe};
    }
    if (dans_une_plage(r, e)) {
        if (e.inactif_s >= r.rendormir_apres_s) {
            return {Etat::Eteint, Cause::PlageHoraire};
        }
        return {Etat::Allume, Cause::Activite};
    }
    if (r.eteindre_apres_s != 0 && e.inactif_s >= r.eteindre_apres_s) {
        return {Etat::Eteint, Cause::Inactivite};
    }
    if (r.attenuer_apres_s != 0 && e.inactif_s >= r.attenuer_apres_s) {
        return {Etat::Attenue, Cause::Inactivite};
    }
    return {Etat::Allume, Cause::Aucune};
}

} // namespace ecran::veille
