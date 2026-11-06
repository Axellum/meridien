// Noyau pur : horloge matérielle (RTC) — conversions et politique d'usage. Aucun accès au bus : la couche carte lit ou écrit les
// 7 octets de la trame, ce fichier les convertit et décide QUAND s'en servir.
//
// Pourquoi une horloge matérielle : l'heure SNTP n'arrive que plusieurs secondes après l'adresse IP (docs/mesures/1b.md), et pas du
// tout sans réseau. Le Tab5 porte une RX8130CE (adresse I2C 0x32, supercondensateur de secours) : l'écran peut afficher une heure
// juste dès le démarrage, hors ligne comprise, et les plages de veille (veille.hpp) fonctionnent avant le Wi-Fi.
//
// Trame de la RX8130CE (registres 0x10..0x16, lue ou écrite d'un bloc ; source : composant `rx8130` d'ESPHome 2026.9.0, lu le
// 06/11/2026, fiche Epson RX8130CE) :
//     octet 0 seconde (BCD)   1 minute (BCD)   2 heure (BCD, 24 h)   3 jour de la semaine (UN bit, bit 0 = dimanche)
//     octet 4 jour du mois    5 mois           6 année sur deux chiffres (BCD, 2000 + valeur)
// L'horloge garde l'heure UTC (c'est ce que fait déjà le firmware ESPHome de la tablette) : le fuseau reste l'affaire de
// l'application.
//
// Politique (chaque règle est un test de test/host/test_horloge_materielle.cpp) :
//   - AU DÉMARRAGE : on reprend l'horloge matérielle seulement si elle est lisible, plausible, sans perte d'alimentation signalée
//     ET si l'heure du système n'est PAS déjà plausible. Une heure venue du réseau n'est jamais écrasée par une heure de pile.
//   - EN MARCHE : on écrit l'horloge matérielle seulement depuis une heure venue du RÉSEAU (jamais depuis elle-même : ce serait
//     lui faire croire qu'on l'a vérifiée), au premier réglage puis au plus une fois toutes les 6 h, ou tout de suite si elle
//     s'écarte de plus de 2 s.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstdint>

#include "ecran/heure_fr.hpp"

namespace ecran::rtc {

// Bornes de plausibilité d'un instant : avant 2026 (année de conception du projet) ou après 2099, ce n'est pas une heure vraie.
constexpr std::int64_t kInstantPlausibleMin = 1767225600; // 2026-01-01T00:00:00Z
// 2099-12-31T23:59:59Z : la trame n'a que deux chiffres d'année.
constexpr std::int64_t kInstantPlausibleMax = 4102444799;
constexpr std::int64_t kPeriodeEcritureS = 6 * 3600; // au plus une écriture toutes les 6 h
constexpr std::int64_t kEcartTolereS = 2;            // au-delà, l'horloge matérielle est recalée tout de suite

constexpr bool instant_plausible(std::int64_t s) noexcept {
    return s >= kInstantPlausibleMin && s <= kInstantPlausibleMax;
}

// ---- BCD -----------------------------------------------------------------------------------------------------------------

constexpr bool bcd_valide(std::uint8_t v) noexcept { return (v >> 4) <= 9 && (v & 0x0F) <= 9; }
constexpr std::uint8_t bcd_vers_dec(std::uint8_t v) noexcept {
    return static_cast<std::uint8_t>((v >> 4) * 10 + (v & 0x0F));
}
constexpr std::uint8_t dec_vers_bcd(unsigned v) noexcept {
    return static_cast<std::uint8_t>(((v / 10) << 4) | (v % 10));
}

// ---- trame -----------------------------------------------------------------------------------------------------------------

struct Trame {
    std::uint8_t octet[7] = {};
};

enum class Erreur : std::uint8_t { Aucune, Bcd, Plage, JourSemaine, Instant };

// Instant UTC -> trame. Refusé hors de 2000..2099 (la trame n'a pas de siècle) ou si l'instant n'est pas plausible.
constexpr Erreur encoder(std::int64_t instant_s, Trame &t) noexcept {
    if (!instant_plausible(instant_s)) {
        return Erreur::Instant;
    }
    const std::int64_t jours = heure::detail::division_plancher(instant_s, 86400);
    const std::int64_t reste = instant_s - jours * 86400;
    std::int64_t an = 0;
    int mois = 0;
    int jour = 0;
    heure::detail::date_depuis_jours(jours, an, mois, jour);
    const int semaine = heure::detail::jour_semaine_de(jours);
    t.octet[0] = dec_vers_bcd(static_cast<unsigned>(reste % 60));
    t.octet[1] = dec_vers_bcd(static_cast<unsigned>(reste % 3600 / 60));
    t.octet[2] = dec_vers_bcd(static_cast<unsigned>(reste / 3600));
    t.octet[3] = static_cast<std::uint8_t>(1u << semaine);
    t.octet[4] = dec_vers_bcd(static_cast<unsigned>(jour));
    t.octet[5] = dec_vers_bcd(static_cast<unsigned>(mois));
    t.octet[6] = dec_vers_bcd(static_cast<unsigned>(an % 100));
    return Erreur::Aucune;
}

// Trame -> instant UTC. Les bits de commande des octets (bit 7 des secondes et minutes, bits 6 et 7 de l'heure, du jour et du
// mois) sont masqués comme le fait ESPHome ; tout le reste doit être cohérent : BCD, plages, et jour de la semaine égal à celui
// que donne la date (une horloge dont la date et le jour se contredisent n'a pas été réglée par un code correct).
constexpr Erreur decoder(const Trame &t, std::int64_t &instant_s) noexcept {
    const std::uint8_t s = t.octet[0] & 0x7F;
    const std::uint8_t mn = t.octet[1] & 0x7F;
    const std::uint8_t h = t.octet[2] & 0x3F;
    const std::uint8_t j = t.octet[4] & 0x3F;
    const std::uint8_t mo = t.octet[5] & 0x1F;
    const std::uint8_t a = t.octet[6];
    if (!bcd_valide(s) || !bcd_valide(mn) || !bcd_valide(h) || !bcd_valide(j) || !bcd_valide(mo) || !bcd_valide(a)) {
        return Erreur::Bcd;
    }
    const int seconde = bcd_vers_dec(s);
    const int minute = bcd_vers_dec(mn);
    const int heure = bcd_vers_dec(h);
    const int jour = bcd_vers_dec(j);
    const int mois = bcd_vers_dec(mo);
    const std::int64_t an = 2000 + bcd_vers_dec(a);
    if (seconde > 59 || minute > 59 || heure > 23 || mois < 1 || mois > 12 || jour < 1 ||
        jour > heure::detail::jours_du_mois(an, mois)) {
        return Erreur::Plage;
    }
    const std::int64_t jours = heure::detail::jours_depuis_epoch(an, mois, jour);
    const int semaine = heure::detail::jour_semaine_de(jours);
    if ((t.octet[3] & 0x7F) != (1u << semaine)) {
        return Erreur::JourSemaine;
    }
    const std::int64_t valeur = jours * 86400 + heure * 3600 + minute * 60 + seconde;
    if (!instant_plausible(valeur)) {
        return Erreur::Instant;
    }
    instant_s = valeur;
    return Erreur::Aucune;
}

// ---- politique -------------------------------------------------------------------------------------------------------------

enum class SourceHeure : std::uint8_t { Aucune, Rtc, Reseau };

struct EntreeDemarrage {
    bool rtc_lue = false;       // la trame a été lue ET décodée sans erreur
    bool rtc_perdue = false;    // la puce signale une perte d'alimentation (bit VLF) : son contenu n'est pas fiable
    std::int64_t rtc_s = 0;     // instant décodé
    std::int64_t systeme_s = 0; // heure du système au moment de la décision
};

enum class ActionDemarrage : std::uint8_t { Rien, ReprendreRtc };

constexpr ActionDemarrage au_demarrage(const EntreeDemarrage &e) noexcept {
    if (e.rtc_lue && !e.rtc_perdue && instant_plausible(e.rtc_s) && !instant_plausible(e.systeme_s)) {
        return ActionDemarrage::ReprendreRtc;
    }
    return ActionDemarrage::Rien;
}

// Qui a réglé l'horloge du système ? Observé à chaque passage de la tâche de surveillance. Le système ne dit pas d'où vient son
// heure : on la déduit. Un saut (plus de kEcartTolereS entre l'heure du système et celle qu'elle devrait avoir en continuant sur
// sa lancée) ou un passage de « non plausible » à « plausible » que nous n'avons pas posé = quelqu'un d'autre l'a réglée, c'est-à-dire
// le réseau (SNTP). Notre propre reprise de l'horloge matérielle (au démarrage) fixe `source` à Rtc et `plausible_avant` à vrai AVANT
// la première observation, donc elle ne passe jamais pour le réseau. Une correction du réseau de moins de kEcartTolereS passe
// inaperçue : l'horloge matérielle n'est alors pas réécrite, et son écart reste borné par ce seuil (plus la dérive d'un jour).
struct Observation {
    SourceHeure source_avant = SourceHeure::Aucune;
    bool systeme_plausible_avant = false;
    bool systeme_plausible_maintenant = false;
    bool saut_detecte = false;
};

constexpr SourceHeure source_apres(const Observation &o) noexcept {
    if (!o.systeme_plausible_maintenant) {
        return SourceHeure::Aucune;
    }
    if (o.saut_detecte || !o.systeme_plausible_avant) {
        return SourceHeure::Reseau;
    }
    // Un système déjà plausible dont on ne connaît pas l'origine reste « inconnu » (source_avant) : on n'écrit pas.
    return o.source_avant;
}

struct EntreePeriodique {
    SourceHeure source = SourceHeure::Aucune;
    std::int64_t systeme_s = 0;
    bool rtc_lue = false;
    std::int64_t rtc_s = 0;
    std::int64_t depuis_derniere_ecriture_s = -1; // -1 = jamais écrite depuis le démarrage
};

enum class ActionPeriodique : std::uint8_t { Rien, EcrireRtc };

constexpr std::int64_t valeur_absolue(std::int64_t v) noexcept { return v < 0 ? -v : v; }

constexpr ActionPeriodique periodique(const EntreePeriodique &e) noexcept {
    if (e.source != SourceHeure::Reseau || !instant_plausible(e.systeme_s)) {
        return ActionPeriodique::Rien; // jamais depuis l'horloge elle-même, jamais depuis une heure douteuse
    }
    if (e.depuis_derniere_ecriture_s < 0) {
        return ActionPeriodique::EcrireRtc;
    }
    if (e.rtc_lue && valeur_absolue(e.rtc_s - e.systeme_s) > kEcartTolereS) {
        return ActionPeriodique::EcrireRtc;
    }
    if (e.depuis_derniere_ecriture_s >= kPeriodeEcritureS) {
        return ActionPeriodique::EcrireRtc;
    }
    return ActionPeriodique::Rien;
}

} // namespace ecran::rtc
