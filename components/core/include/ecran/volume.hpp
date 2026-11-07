// Noyau pur : courbe de volume PERÇU -> atténuation en dixièmes de décibel -> pas du convertisseur.
//
// Pourquoi une courbe : l'oreille juge le volume en décibels (10 dB de plus = deux fois plus fort environ), pas en amplitude. Un
// curseur qui multiplie l'amplitude par le pourcentage donne un volume qui s'effondre entre 100 et 80 %, puis ne bouge plus. Ici le
// niveau 1..100 est une atténuation en dB, QUADRATIQUE par rapport à la distance au maximum : 100 -> 0 dB, 50 -> -12,7 dB, 25 -> -28,7 dB,
// 1 -> -50 dB. Les crans du haut sont fins (le volume où l'on vit), ceux du bas grossiers (on cherche seulement « presque rien »).
//
// Niveau 0 = MUET (pas une atténuation de 50 dB : le muet coupe le convertisseur). Le plafond de la carte (`plafond_db_x10`, <= 0)
// décale toute la courbe : un haut-parleur qui sature à 0 dB du convertisseur (la Waveshare 7B, ES8311) y perd de la marge de
// sécurité sans changer l'allure du curseur.
//
// C++17 standard, aucun include ESP-IDF, calcul entier seulement, évaluable à la compilation.
#pragma once

#include <cstdint>

namespace ecran::volume {

constexpr int kNiveauMin = 0;
constexpr int kNiveauMax = 100;
constexpr int kEtendueDbX10 = 500;   // du niveau 100 au niveau 1 : 50 dB
constexpr int kDbX10Plancher = -960; // -96 dB : le bas de course des convertisseurs courants (ES8388), jamais dépassé
constexpr int kPasMax = 192;         // ES8388 : pas de 0,5 dB, 0 = 0 dB, 192 = -96 dB

constexpr int borner_niveau(int niveau) noexcept {
    return niveau < kNiveauMin ? kNiveauMin : (niveau > kNiveauMax ? kNiveauMax : niveau);
}

constexpr bool est_muet(int niveau) noexcept { return niveau <= 0; }

// Atténuation en dixièmes de dB (négative ou nulle) pour un niveau 1..100 ; `plafond_db_x10` <= 0 décale la courbe entière.
// À ne pas appeler pour le muet : voir est_muet.
constexpr int db_x10(int niveau, int plafond_db_x10 = 0) noexcept {
    const int n = borner_niveau(niveau) < 1 ? 1 : borner_niveau(niveau);
    const int distance = kNiveauMax - n; // 0..99
    // -500 * distance^2 / 99^2, arrondi à l'entier le plus proche (par défaut : on arrondit vers le haut de la valeur négative).
    const int attenuation = -((kEtendueDbX10 * distance * distance + (99 * 99) / 2) / (99 * 99));
    const int plafond = plafond_db_x10 > 0 ? 0 : plafond_db_x10;
    const int total = plafond + attenuation;
    return total < kDbX10Plancher ? kDbX10Plancher : total;
}

// Pas du convertisseur à 0,5 dB (ES8388 : registre de volume du DAC, 0x00 = 0 dB ... 0xC0 = -96 dB) pour une atténuation donnée.
constexpr int pas_demi_db(int db_x10_valeur) noexcept {
    const int a = db_x10_valeur > 0 ? 0 : -db_x10_valeur; // dixièmes de dB, positifs
    const int pas = (a + 2) / 5;                          // 5 dixièmes = 0,5 dB ; arrondi au plus proche
    return pas > kPasMax ? kPasMax : pas;
}

// Raccourci : niveau -> pas du convertisseur à 0,5 dB. Le muet n'a pas de pas (rend kPasMax, le plus bas) : le pilote coupe aussi
// le convertisseur par son bit de sourdine.
constexpr int pas_pour_niveau(int niveau, int plafond_db_x10 = 0) noexcept {
    return est_muet(niveau) ? kPasMax : pas_demi_db(db_x10(niveau, plafond_db_x10));
}

} // namespace ecran::volume
