// Noyau pur : courbe de luminosité PERÇUE -> rapport cyclique (PWM) du rétroéclairage.
//
// Pourquoi une courbe : l'œil juge la luminosité à peu près comme la racine cinquième de la lumière émise (loi de Stevens, exposant
// ~0,4), alors qu'un rétroéclairage émet une lumière proportionnelle au rapport cyclique. Un curseur « luminosité » branché en
// direct sur le rapport cyclique est donc inutilisable : tout le travail se passe entre 0 et 20 %, et 50 % paraît presque plein.
// Ici le niveau 1..100 (ce que montre le curseur, ce que règle la page) est élevé à la puissance 2,5 pour retrouver le rapport
// cyclique : niveau 50 -> 19 % de lumière émise, qui paraît « à moitié ».
//
// Trois bornes, propres à chaque carte (une `Courbe`) :
//   - niveau 0 = ÉTEINT, rapport cyclique 0, toujours (jamais le plancher) ;
//   - niveau 1 = le plus bas encore UTILISABLE : un rétroéclairage sous quelques pour mille clignote ou ne s'allume pas ;
//   - niveau 100 = le plafond de la carte (pas forcément 100 % : un Tab5 sur un port USB faible a déjà redémarré à pleine lumière,
//     « brownout » du 08/10/2026 dans les notes du dépôt Tab5).
// Les valeurs de la courbe du Tab5 sont dans components/bsp_tab5 ; ce fichier ne connaît aucune carte.
//
// C++17 standard, aucun include ESP-IDF, calcul entier seulement (pas de libm), évaluable à la compilation : les tests de
// test/host/ s'en servent.
#pragma once

#include <cstdint>

namespace ecran::luminosite {

// Rapports cycliques en POUR MILLE (0..1000), indépendants de la résolution du PWM de la carte.
struct Courbe {
    std::int32_t duty_min_pm; // au niveau 1
    std::int32_t duty_max_pm; // au niveau 100
};

// Valeurs par défaut d'une carte dont on n'a rien mesuré : plancher 2 %, plafond 100 %.
constexpr Courbe kCourbeParDefaut{20, 1000};

constexpr int kNiveauMin = 0;
constexpr int kNiveauMax = 100;

constexpr int borner_niveau(int niveau) noexcept {
    return niveau < kNiveauMin ? kNiveauMin : (niveau > kNiveauMax ? kNiveauMax : niveau);
}

// Racine carrée entière par arrondi vers le bas (Newton). Sert à x^2,5 = x * x * racine(x).
constexpr std::uint64_t racine_entiere(std::uint64_t n) noexcept {
    if (n < 2) {
        return n;
    }
    std::uint64_t x = n;
    std::uint64_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

// (niveau / 100) ^ 2,5 en virgule fixe Q16 (0..65536), pour un niveau déjà borné à 0..100.
constexpr std::uint32_t fraction_q16(int niveau) noexcept {
    const int n = borner_niveau(niveau);
    if (n <= 0) {
        return 0;
    }
    if (n >= 100) {
        return 65536;
    }
    const std::uint64_t x = (static_cast<std::uint64_t>(n) << 16) / 100; // Q16
    const std::uint64_t x2 = (x * x) >> 16;                              // x^2, Q16
    const std::uint64_t rac = racine_entiere(x << 16);                   // racine(x), Q16
    return static_cast<std::uint32_t>((x2 * rac) >> 16);                 // x^2,5, Q16
}

// Rapport cyclique en pour mille pour un niveau perçu. 0 -> 0 (éteint) ; 1..100 -> de duty_min_pm à duty_max_pm.
constexpr std::int32_t duty_pour_mille(int niveau, const Courbe &c = kCourbeParDefaut) noexcept {
    const int n = borner_niveau(niveau);
    if (n == 0) {
        return 0;
    }
    const std::int64_t etendue = static_cast<std::int64_t>(c.duty_max_pm) - c.duty_min_pm;
    const std::int64_t ajout = (etendue * fraction_q16(n) + 32768) >> 16;
    return static_cast<std::int32_t>(c.duty_min_pm + ajout);
}

// Valeur à écrire dans le registre du PWM (résolution `bits`, 1..16) : arrondie, jamais au-delà du maximum du registre.
constexpr std::uint32_t duty_pour_bits(int niveau, int bits, const Courbe &c = kCourbeParDefaut) noexcept {
    const std::uint32_t maximum = (bits >= 1 && bits <= 16) ? ((1u << bits) - 1u) : 1023u;
    const std::uint64_t pm = static_cast<std::uint64_t>(duty_pour_mille(niveau, c));
    const std::uint64_t valeur = (pm * maximum + 500) / 1000;
    return static_cast<std::uint32_t>(valeur > maximum ? maximum : valeur);
}

// Niveau le plus bas des deux : sert à l'atténuation (écran assombri après un temps sans toucher) et au plafond choisi.
constexpr int niveau_plafonne(int niveau, int plafond) noexcept {
    const int n = borner_niveau(niveau);
    const int p = borner_niveau(plafond);
    return n < p ? n : p;
}

} // namespace ecran::luminosite
