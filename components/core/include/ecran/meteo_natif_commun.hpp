// Noyau pur : ce que les lecteurs de MET Norway (meteo_metno.hpp) et du NWS (meteo_nws.hpp) ont en commun. C++17 standard, aucun
// include ESP-IDF, évaluable à la compilation.
//   - lecture d'un nombre FACULTATIF : absent ou nul = « inconnu » (INCONNU_DIX, INCONNU_ENTIER), jamais 0 ; présent mais pas un
//     nombre, ou hors de l'entier 16 bits = réponse refusée (le champ est nommé dans le diagnostic) ;
//   - les jours LOCAUX : minuit local de chacun des sept jours, changements d'heure compris (les deux services donnent leurs
//     instants en UTC, sans fuseau : c'est l'appareil qui découpe les jours) ;
//   - le lever et le coucher du soleil (soleil.hpp, le calcul de l'appareil, ADR-0012), que ni l'un ni l'autre ne donne ;
//   - de quoi garder le plus petit et le plus grand d'une série sans jamais compter une valeur inconnue.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/heure_fr.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/meteo_riche.hpp"
#include "ecran/soleil.hpp"

namespace ecran::meteo::natif {

// ---- nombres facultatifs ----------------------------------------------------------------------------------------------

enum class Lecture : std::uint8_t { Absente, Valeur, Invalide };

// `v` multiplié par 10^decimales, arrondi (moitié loin de zéro) ; Absente si le champ manque ou vaut null.
constexpr Lecture lire_fixe(const json::Valeur &v, int decimales, std::int64_t &sortie) noexcept {
    if (!v.existe() || v.est_nul())
        return Lecture::Absente;
    if (v.genre() != json::Genre::Nombre || !v.en_fixe(decimales, sortie))
        return Lecture::Invalide;
    return Lecture::Valeur;
}

constexpr void signaler(Diagnostic &diag, Erreur e, std::string_view nom) noexcept {
    if (diag.ok())
        diag = Diagnostic{e, nom};
}

// Nombre en `decimales` décimales qui doit tenir dans un int16 sans être la valeur « inconnu » (INCONNU_DIX).
constexpr std::int16_t fixe_ou_inconnu(const json::Valeur &v, int decimales, std::string_view nom, Diagnostic &diag,
                                       std::int16_t inconnu = INCONNU_DIX) noexcept {
    std::int64_t n = 0;
    switch (lire_fixe(v, decimales, n)) {
    case Lecture::Absente:
        return inconnu;
    case Lecture::Invalide:
        signaler(diag, Erreur::MauvaisType, nom);
        return inconnu;
    case Lecture::Valeur:
        break;
    }
    if (n <= INT16_MIN || n > INT16_MAX) {
        signaler(diag, Erreur::HorsPlage, nom);
        return inconnu;
    }
    return static_cast<std::int16_t>(n);
}

// Pourcentage ou degré : entier arrondi, jamais négatif (-1 est « inconnu »).
constexpr std::int16_t entier_ou_inconnu(const json::Valeur &v, std::string_view nom, Diagnostic &diag) noexcept {
    std::int64_t n = 0;
    switch (lire_fixe(v, 0, n)) {
    case Lecture::Absente:
        return INCONNU_ENTIER;
    case Lecture::Invalide:
        signaler(diag, Erreur::MauvaisType, nom);
        return INCONNU_ENTIER;
    case Lecture::Valeur:
        break;
    }
    // Un « -1 » écrit dans la réponse n'est pas un pourcentage : pas de confusion avec « inconnu ».
    if (n < 0 || n > INT16_MAX) {
        signaler(diag, Erreur::HorsPlage, nom);
        return INCONNU_ENTIER;
    }
    return static_cast<std::int16_t>(n);
}

// Pourcentage 0..100 (humidité, nébulosité, probabilité) ou inconnu. Au-delà de 100 la réponse est refusée (HorsPlage) : le noyau
// range ces valeurs dans des octets, où 306 deviendrait silencieusement 50.
constexpr std::int16_t pourcentage_ou_inconnu(const json::Valeur &v, std::string_view nom, Diagnostic &diag) noexcept {
    const std::int16_t n = entier_ou_inconnu(v, nom, diag);
    if (n > 100) {
        signaler(diag, Erreur::HorsPlage, nom);
        return INCONNU_ENTIER;
    }
    return n;
}

// Mètres par seconde (en dixièmes) -> km/h (en dixièmes) : × 3,6, arrondi à l'entier le plus proche. 0,9 m/s -> 32 (3,2 km/h).
constexpr std::int64_t kmh_dix_depuis_ms_dix(std::int64_t ms_dix) noexcept { return (ms_dix * 36 + 5) / 10; }

// ---- plus petit et plus grand d'une série -------------------------------------------------------------------------------

struct Extremes {
    std::int16_t min = INCONNU_DIX;
    std::int16_t max = INCONNU_DIX;
    constexpr void noter(std::int16_t v) noexcept {
        if (v == INCONNU_DIX)
            return;
        if (min == INCONNU_DIX || v < min)
            min = v;
        if (max == INCONNU_DIX || v > max)
            max = v;
    }
    constexpr bool vide() const noexcept { return min == INCONNU_DIX; }
};

// ---- jours locaux --------------------------------------------------------------------------------------------------------

// Instant UNIX du minuit local du jour `jours` (nombre de jours depuis le 1970-01-01 du calendrier LOCAL). Un jour de
// changement d'heure fait 23 ou 25 h : on essaie les deux décalages du fuseau et on garde celui où l'heure locale vaut bien
// 00:00:00 ce jour-là (le plus tôt si le minuit se produit deux fois). Si le minuit n'existe pas (saut d'heure à minuit), le jour
// commence à l'instant du saut.
constexpr std::int64_t minuit_local(const heure::Fuseau &f, std::int64_t jours) noexcept {
    const std::int64_t base = jours * 86400;
    const std::int64_t c1 = base - f.decalage_std_s;
    const std::int64_t c2 = base - f.decalage_ete_s;
    const auto bon = [&](std::int64_t c) {
        const heure::HeureLocale h = heure::en_heure_locale(c, f);
        return h.heure == 0 && h.minute == 0 && h.seconde == 0 &&
               heure::detail::jours_depuis_epoch(h.an, h.mois, h.jour) == jours;
    };
    const bool b1 = bon(c1);
    const bool b2 = bon(c2);
    if (b1 && b2)
        return c1 < c2 ? c1 : c2;
    if (b1)
        return c1;
    if (b2)
        return c2;
    return c1 > c2 ? c1 : c2;
}

// Les débuts des JOURS_MAX jours locaux à partir du jour de `recu_a_s`, puis la fin du dernier (= début du suivant).
struct Jours {
    std::int64_t debut[JOURS_MAX + 1] = {};
};

constexpr Jours jours_locaux(const heure::Fuseau &f, std::int64_t recu_a_s) noexcept {
    Jours j;
    const heure::HeureLocale h = heure::en_heure_locale(recu_a_s, f);
    const std::int64_t j0 = heure::detail::jours_depuis_epoch(h.an, h.mois, h.jour);
    for (int i = 0; i <= JOURS_MAX; ++i)
        j.debut[i] = minuit_local(f, j0 + i);
    return j;
}

// ---- soleil -----------------------------------------------------------------------------------------------------------

// Lever et coucher du jour `j` (qui finit à `fin_s`) pour la position donnée. Nuit polaire : lever == coucher == minuit local,
// comme Open-Meteo (`Jour::sans_soleil()`). Jour polaire : lever au minuit, coucher à la fin du jour.
constexpr void remplir_soleil(Jour &j, std::int64_t fin_s, std::int32_t latitude_e5,
                              std::int32_t longitude_e5) noexcept {
    const soleil::LeverCoucher e = soleil::lever_coucher_e5(j.date_s, latitude_e5, longitude_e5);
    switch (e.situation) {
    case soleil::Situation::Normal:
        j.lever_s = e.lever_s;
        j.coucher_s = e.coucher_s > e.lever_s ? e.coucher_s : e.lever_s;
        return;
    case soleil::Situation::NuitPolaire:
        j.lever_s = j.date_s;
        j.coucher_s = j.date_s;
        return;
    case soleil::Situation::JourPolaire:
        j.lever_s = j.date_s;
        j.coucher_s = fin_s;
        return;
    }
}

// ---- fuseau ------------------------------------------------------------------------------------------------------------

// Copie le nom IANA dans le relevé. Faux s'il ne tient pas (FUSEAU_MAX).
constexpr bool copier_fuseau(Releve &r, std::string_view iana) noexcept {
    if (iana.empty() || iana.size() >= FUSEAU_MAX)
        return false;
    for (std::size_t i = 0; i < iana.size(); ++i)
        r.fuseau[i] = iana[i];
    r.fuseau[iana.size()] = '\0';
    return true;
}

} // namespace ecran::meteo::natif
