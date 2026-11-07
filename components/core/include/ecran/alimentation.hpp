// Noyau pur : alimentation de la carte — conversion des registres d'un moniteur de courant INA226 et lecture de l'état (batterie ou
// non, en charge ou non). Aucun accès au bus : la couche carte lit les registres, ce fichier les traduit.
//
// Sources (lues le 06/11/2026) : composant `ina226` d'ESPHome 2026.9.0 (registres, coefficients, formule de l'étalonnage), config du
// Tab5 du dépôt voisin (résistance de shunt 5 mΩ, courant maximal 8,192 A, adresse I2C 0x41), notes d'Axel sur la batterie du Tab5.
//   - tension du bus : registre 0x02, 1,25 mV par pas, non signée ;
//   - tension du shunt : registre 0x01, 2,5 µV par pas, signée ;
//   - courant : registre 0x04 = (shunt * étalonnage) / 2048, signé, pas = « LSB du courant » = ceil(Imax / 32768) ;
//   - étalonnage (registre 0x05) = 0,00512 / (LSB[A] * R[ohm]) : 4096 pour Imax = 8,192 A et R = 5 mΩ ;
//   - sens du courant sur le Tab5 : NÉGATIF = la batterie se charge, POSITIF = elle se décharge (M5Unified, vérifié par un tiers
//     chez qui la batterie est montée) ;
//   - identité de la puce : registre 0xFE = 0x5449 (« TI »), registre 0xFF = 0x2260 (fiche TI INA226) — NON vérifié ici tant
//     que la puce n'a pas répondu sur la carte (docs/mesures/tab5-materiel.md dit ce qui a été lu).
// ATTENTION, propre au Tab5 : la tension lue est celle du connecteur de BATTERIE. Sans batterie (l'exemplaire d'Axel) elle ne dit
// rien de la batterie : ESPHome y a lu 1,83 à 1,94 V chargeur coupé, 4,2 V, 5,7 V ou une oscillation 4,2 <-> 8,4 V chargeur
// allumé. Seul « une batterie 2S est présente » (> 5,5 V chargeur coupé) est une conclusion permise ici.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstdint>

namespace ecran::alimentation {

constexpr std::uint8_t kRegConfig = 0x00;
constexpr std::uint8_t kRegShunt = 0x01;
constexpr std::uint8_t kRegTension = 0x02;
constexpr std::uint8_t kRegPuissance = 0x03;
constexpr std::uint8_t kRegCourant = 0x04;
constexpr std::uint8_t kRegEtalonnage = 0x05;
constexpr std::uint8_t kRegFabricant = 0xFE;
constexpr std::uint8_t kRegPuce = 0xFF;
constexpr std::uint16_t kFabricantTi = 0x5449;
constexpr std::uint16_t kPuceIna226 = 0x2260;

// LSB du courant en microampères pour un courant maximal en milliampères : ceil(Imax / 32768).
constexpr std::int32_t lsb_courant_ua(std::int32_t courant_max_ma) noexcept {
    return (courant_max_ma * 1000 + 32767) / 32768;
}

// Valeur du registre d'étalonnage (0x05) : 0,00512 / (LSB[A] * R[ohm]) = 5 120 000 / (LSB[µA] * R[mΩ]).
constexpr std::int32_t etalonnage(std::int32_t lsb_ua, std::int32_t shunt_mohm) noexcept {
    return (lsb_ua <= 0 || shunt_mohm <= 0) ? 0 : 5120000 / (lsb_ua * shunt_mohm);
}

constexpr std::int32_t tension_mv(std::uint16_t brut) noexcept {
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(brut) * 5u + 2u) / 4u); // 1,25 mV par pas, arrondi
}

constexpr std::int32_t shunt_uv(std::uint16_t brut) noexcept {
    const std::int32_t v = static_cast<std::int16_t>(brut);
    return (v * 5) / 2; // 2,5 µV par pas
}

constexpr std::int32_t courant_ma(std::uint16_t brut, std::int32_t lsb_ua) noexcept {
    const std::int32_t v = static_cast<std::int16_t>(brut);
    const std::int64_t ua = static_cast<std::int64_t>(v) * lsb_ua;
    return static_cast<std::int32_t>(ua >= 0 ? (ua + 500) / 1000 : -((-ua + 500) / 1000));
}

// ---- état ------------------------------------------------------------------------------------------------------------------

// Batterie 2S : pleine 8,4 V, coupée par sa protection vers 6 V. Sous ce seuil, chargeur COUPÉ, il n'y en a pas (valeur de la
// décision du firmware ESPHome du Tab5 : 5,5 V ; 1,9 V mesuré sans batterie).
constexpr std::int32_t kSeuilBatterieMv = 5500;
constexpr std::int32_t kSeuilRepos_ma = 20; // en dessous de ±20 mA : ni charge ni décharge nettes

constexpr bool batterie_presente(std::int32_t tension_mv_valeur, bool chargeur_coupe) noexcept {
    // Chargeur allumé, la tension lue est celle du chargeur : elle ne prouve rien.
    return chargeur_coupe && tension_mv_valeur >= kSeuilBatterieMv;
}

enum class Etat : std::uint8_t { Inconnu, SansBatterie, Repos, EnCharge, EnDecharge };

constexpr Etat classer(bool puce_repond, std::int32_t tension_mv_valeur, std::int32_t courant_ma_valeur,
                       bool chargeur_coupe) noexcept {
    if (!puce_repond) {
        return Etat::Inconnu;
    }
    if (!batterie_presente(tension_mv_valeur, chargeur_coupe)) {
        // Chargeur coupé et tension basse : pas de batterie. Chargeur allumé : on ne sait pas dire (on le dit « sans »).
        return chargeur_coupe ? Etat::SansBatterie : Etat::Inconnu;
    }
    if (courant_ma_valeur <= -kSeuilRepos_ma) {
        return Etat::EnCharge;
    }
    if (courant_ma_valeur >= kSeuilRepos_ma) {
        return Etat::EnDecharge;
    }
    return Etat::Repos;
}

} // namespace ecran::alimentation
