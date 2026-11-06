// Noyau pur : mesures sur des échantillons audio 16 bits — niveau efficace (RMS) et crête en dBFS, écrêtage, détection d'une
// fréquence (Goertzel) et génération d'un sinus de test. Sert aux autotests de la carte (le micro entend-il le haut-parleur ?) et à
// toute jauge de niveau à l'écran. Aucun accès au matériel : des tableaux d'entiers en entrée.
//
// dBFS : 0 = pleine échelle d'un entier 16 bits (32768), un sinus pleine échelle a un niveau efficace de -3,0 dBFS. Les valeurs sont
// en DIXIÈMES de dB (`_x10`) pour rester entières ; le silence absolu vaut kSilence_x10 (-120,0 dBFS), jamais -l'infini.
//
// Le logarithme est calculé sans libm (log2 en virgule fixe Q16 par élévations au carré successives) : le tout s'évalue à la
// compilation, donc les tests de test/host/ s'exécutent aussi en `static_assert`. Le Goertzel et le sinus utilisent des `double` et
// un développement de Taylor écrit ici (pas de <cmath>, non constexpr en C++17).
// C++17 standard, aucun include ESP-IDF.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ecran::audio {

constexpr std::int32_t kSilence_x10 = -1200; // -120 dBFS

// ---- logarithme entier ---------------------------------------------------------------------------------------------------

// log2(v) en virgule fixe Q16 pour v >= 1 (0 si v == 0). Précision ~2^-16 : une erreur de 0,0005 dB.
constexpr std::int64_t log2_q16(std::uint64_t v) noexcept {
    if (v == 0) {
        return 0;
    }
    int n = 0; // partie entière : position du bit de poids fort
    for (std::uint64_t t = v; t > 1; t >>= 1) {
        ++n;
    }
    // Mantisse m dans [1,2) en Q31 : 2^31 <= m < 2^32.
    std::uint64_t m = n >= 31 ? (v >> (n - 31)) : (v << (31 - n));
    std::int64_t fraction = 0;
    for (int i = 0; i < 16; ++i) {
        m = (m * m) >> 31; // on élève au carré : le bit suivant de log2 est 1 quand le carré dépasse 2
        fraction <<= 1;
        if (m >= (1ull << 32)) {
            fraction |= 1;
            m >>= 1;
        }
    }
    return (static_cast<std::int64_t>(n) << 16) + fraction;
}

// Quotient num / den arrondi à l'entier le plus proche (demi-valeurs éloignées de zéro), den > 0.
constexpr std::int32_t arrondi_diviseur(std::int64_t num, std::int64_t den) noexcept {
    return static_cast<std::int32_t>(num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den));
}

// Niveau en dBFS (x10) d'une amplitude 0..32768 : 20 log10(amp / 32768).
constexpr std::int32_t amplitude_dbfs_x10(std::uint32_t amplitude) noexcept {
    if (amplitude == 0) {
        return kSilence_x10;
    }
    const std::uint32_t a = amplitude > 32768u ? 32768u : amplitude;
    // 20 log10(x) * 10 = 60,206 * log2(x) ; log2(a / 2^15) = log2(a) - 15.
    const std::int64_t l = log2_q16(a) - (15ll << 16);
    const std::int32_t v = arrondi_diviseur(l * 60206, 1000ll * 65536);
    return v < kSilence_x10 ? kSilence_x10 : v;
}

// Niveau en dBFS (x10) d'une énergie moyenne (somme des carrés / n) : 10 log10(moyenne / 2^30).
constexpr std::int32_t energie_dbfs_x10(std::uint64_t somme_carres, std::size_t n) noexcept {
    if (n == 0 || somme_carres == 0) {
        return kSilence_x10;
    }
    const std::uint64_t moyenne = somme_carres / n;
    if (moyenne == 0) {
        return kSilence_x10;
    }
    const std::int64_t l = log2_q16(moyenne) - (30ll << 16);
    const std::int32_t v = arrondi_diviseur(l * 30103, 1000ll * 65536);
    return v < kSilence_x10 ? kSilence_x10 : v;
}

// ---- niveau d'un bloc ------------------------------------------------------------------------------------------------------

struct Niveau {
    std::int32_t rms_dbfs_x10 = kSilence_x10;
    std::int32_t crete_dbfs_x10 = kSilence_x10;
    std::uint32_t crete = 0;   // amplitude absolue maximale, 0..32768
    std::uint32_t ecretes = 0; // nombre d'échantillons à pleine échelle (|x| >= 32767) : le signal a saturé
    std::int32_t continu = 0;  // moyenne arithmétique : un décalage du convertisseur apparaît ici
};

constexpr std::uint32_t valeur_absolue(std::int16_t v) noexcept {
    return v < 0 ? static_cast<std::uint32_t>(-static_cast<std::int32_t>(v)) : static_cast<std::uint32_t>(v);
}

// `donnees` : `n` échantillons ; `pas` : écart entre deux échantillons d'une même voie (2 pour une voie d'un flux stéréo
// entrelacé, en partant de `donnees` pour la gauche et de `donnees + 1` pour la droite).
constexpr Niveau mesurer(const std::int16_t *donnees, std::size_t n, std::size_t pas = 1) noexcept {
    Niveau r;
    if (donnees == nullptr || n == 0 || pas == 0) {
        return r;
    }
    std::uint64_t somme = 0;
    std::int64_t somme_signee = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::int16_t v = donnees[i * pas];
        const std::uint32_t a = valeur_absolue(v);
        somme += static_cast<std::uint64_t>(a) * a;
        somme_signee += v;
        if (a > r.crete) {
            r.crete = a;
        }
        if (a >= 32767u) {
            ++r.ecretes;
        }
    }
    r.rms_dbfs_x10 = energie_dbfs_x10(somme, n);
    r.crete_dbfs_x10 = amplitude_dbfs_x10(r.crete);
    r.continu = static_cast<std::int32_t>(somme_signee / static_cast<std::int64_t>(n));
    return r;
}

// ---- sinus et Goertzel (virgule flottante, sans libm) ---------------------------------------------------------------------

constexpr double kPi = 3.14159265358979323846;

// sin(x) pour x en radians, par réduction à [-pi/2, pi/2] puis Taylor jusqu'au terme de degré 17 (erreur < 1e-12).
constexpr double sinus(double x) noexcept {
    const double deux_pi = 2.0 * kPi;
    // Ramener x dans [-pi, pi] (les phases passées ici sont déjà < 2 pi : une seule soustraction suffit en pratique).
    while (x > kPi) {
        x -= deux_pi;
    }
    while (x < -kPi) {
        x += deux_pi;
    }
    // sin(pi - x) = sin(x) : ramener dans [-pi/2, pi/2].
    if (x > kPi / 2) {
        x = kPi - x;
    } else if (x < -kPi / 2) {
        x = -kPi - x;
    }
    const double x2 = x * x;
    double terme = x;
    double somme = x;
    for (int k = 1; k <= 8; ++k) {
        terme *= -x2 / static_cast<double>((2 * k) * (2 * k + 1));
        somme += terme;
    }
    return somme;
}

constexpr double cosinus(double x) noexcept { return sinus(x + kPi / 2); }

constexpr double racine(double v) noexcept {
    if (v <= 0.0) {
        return 0.0;
    }
    double x = v < 1.0 ? 1.0 : v;
    for (int i = 0; i < 64; ++i) {
        x = 0.5 * (x + v / x);
    }
    return x;
}

// Échantillon n d'un sinus de fréquence `hz` à la cadence `rate`, d'amplitude `amplitude` (0..32767). La phase est calculée en
// ENTIERS ((hz * n) mod rate) : pas de dérive, même après des millions d'échantillons.
constexpr std::int16_t echantillon_sinus(std::uint32_t hz, std::uint32_t rate, std::uint64_t n,
                                         std::int32_t amplitude) noexcept {
    if (rate == 0) {
        return 0;
    }
    const std::uint64_t reste = (static_cast<std::uint64_t>(hz) * n) % rate;
    const double phase = 2.0 * kPi * static_cast<double>(reste) / static_cast<double>(rate);
    const double v = sinus(phase) * static_cast<double>(amplitude);
    const std::int32_t arrondi = static_cast<std::int32_t>(v >= 0.0 ? v + 0.5 : v - 0.5);
    return static_cast<std::int16_t>(arrondi > 32767 ? 32767 : (arrondi < -32767 ? -32767 : arrondi));
}

// Amplitude de CRÊTE (en échantillons 16 bits) d'un sinus dont la crête est `dbfs_x10` sous la pleine échelle : -200 (= -20,0 dBFS)
// donne 3277. Le niveau efficace de ce sinus est 3,0 dB plus bas que sa crête. 10^(dB/20) est calculé sans libm par produit de
// facteurs : 10 dB = 0,316228 ; 1 dB = 0,891251 ; 0,1 dB = 0,988553.
constexpr std::int32_t amplitude_pour_dbfs_x10(std::int32_t dbfs_x10) noexcept {
    if (dbfs_x10 >= 0) {
        return 32767;
    }
    double facteur = 1.0;
    std::int32_t reste = -dbfs_x10; // dixièmes de dB à atténuer, positifs
    while (reste >= 100) {          // 10 dB : facteur 0,316227766
        facteur *= 0.31622776601683794;
        reste -= 100;
    }
    while (reste >= 10) { // 1 dB
        facteur *= 0.8912509381337456;
        reste -= 10;
    }
    while (reste > 0) { // 0,1 dB
        facteur *= 0.9885530946569389;
        --reste;
    }
    return static_cast<std::int32_t>(32767.0 * facteur + 0.5);
}

// Niveau (dBFS_x10) de la composante de fréquence `hz` dans `n` échantillons (voie extraite avec `pas`), au sens d'un SINUS pur de
// cette fréquence : un sinus d'amplitude A donne le même nombre que la crête mesurée par amplitude_dbfs_x10(A). Goertzel, fenêtre
// de Hann pour réduire la fuite spectrale.
constexpr std::int32_t goertzel_dbfs_x10(const std::int16_t *donnees, std::size_t n, std::uint32_t hz,
                                         std::uint32_t rate, std::size_t pas = 1) noexcept {
    if (donnees == nullptr || n < 8 || rate == 0 || hz == 0 || hz * 2 >= rate) {
        return kSilence_x10;
    }
    const double omega = 2.0 * kPi * static_cast<double>(hz) / static_cast<double>(rate);
    const double coefficient = 2.0 * cosinus(omega);
    double s1 = 0.0;
    double s2 = 0.0;
    double somme_fenetre = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        // Hann : 0,5 (1 - cos(2 pi i / (n - 1))). Phase calculée par réduction entière pour rester exacte.
        const double fenetre = 0.5 * (1.0 - cosinus(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n - 1)));
        const double x = static_cast<double>(donnees[i * pas]) * fenetre;
        const double s0 = x + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
        somme_fenetre += fenetre;
    }
    const double puissance = s1 * s1 + s2 * s2 - coefficient * s1 * s2; // |X(omega)|^2
    if (puissance <= 0.0 || somme_fenetre <= 0.0) {
        return kSilence_x10;
    }
    // Amplitude du sinus = 2 |X| / somme de la fenêtre.
    const double amplitude = 2.0 * racine(puissance) / somme_fenetre;
    const std::uint32_t a = amplitude >= 32768.0 ? 32768u : static_cast<std::uint32_t>(amplitude + 0.5);
    return amplitude_dbfs_x10(a);
}

} // namespace ecran::audio
