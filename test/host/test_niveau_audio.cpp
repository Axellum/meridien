// Tests du noyau pur : niveaux audio, Goertzel et sinus de test (niveau_audio.hpp). Références de dB : calcul flottant fait à part
// (Python math.log10), pas du code testé.
#include "ecran/niveau_audio.hpp"
#include "mini.hpp"

using namespace ecran::audio;

namespace {

constexpr int proche(std::int32_t a, double attendu, double tol) {
    const double d = static_cast<double>(a) - attendu;
    return (d < 0 ? -d : d) <= tol ? 1 : 0;
}

constexpr int log2_connu() {
    VERIFIER(log2_q16(1) == 0);
    VERIFIER(log2_q16(2) == (1 << 16));
    VERIFIER(log2_q16(1024) == (10 << 16));
    VERIFIER(log2_q16(1ull << 40) == (40ll << 16));
    VERIFIER(log2_q16(0) == 0);
    // log2(3) = 1,5849625 -> Q16 = 103872 (a quelques unites pres)
    VERIFIER(proche(static_cast<std::int32_t>(log2_q16(3)), 103872.0, 4.0));
    // log2(10) = 3,3219281 -> 217706
    VERIFIER(proche(static_cast<std::int32_t>(log2_q16(10)), 217706.0, 4.0));
    return 0;
}

constexpr int amplitude_en_dbfs_valeurs_de_reference() {
    VERIFIER(amplitude_dbfs_x10(32768) == 0);
    VERIFIER(amplitude_dbfs_x10(40000) == 0); // au-dela de la pleine echelle : borne
    VERIFIER(proche(amplitude_dbfs_x10(16384), -60.21, 1.0));
    VERIFIER(proche(amplitude_dbfs_x10(3277), -199.99, 1.0));
    VERIFIER(proche(amplitude_dbfs_x10(1000), -303.09, 1.0));
    VERIFIER(proche(amplitude_dbfs_x10(100), -503.09, 1.0));
    VERIFIER(proche(amplitude_dbfs_x10(1), -903.09, 1.0));
    VERIFIER(amplitude_dbfs_x10(0) == kSilence_x10);
    // Valeurs entières exactes là où l'arrondi au plus proche et la troncature diffèrent (-4,288 dB = -42,88 dixièmes, et -7,666
    // dixièmes de dB pour 30000 : calcul Python à part).
    VERIFIER(amplitude_dbfs_x10(20000) == -43);
    VERIFIER(amplitude_dbfs_x10(30000) == -8);
    return 0;
}

constexpr int energie_en_dbfs() {
    VERIFIER(energie_dbfs_x10(0, 100) == kSilence_x10);
    VERIFIER(energie_dbfs_x10(100, 0) == kSilence_x10);
    // n echantillons de valeur constante 16384 : moyenne des carres = 2^28 -> 10 log10(2^-2) = -6,02 dB
    VERIFIER(proche(energie_dbfs_x10(1ull << 28, 1), -60.2, 1.0));
    VERIFIER(energie_dbfs_x10(1ull << 30, 1) == 0);
    return 0;
}

constexpr int niveau_d_un_bloc_constant() {
    std::int16_t b[64] = {};
    for (std::size_t i = 0; i < 64; ++i) {
        b[i] = 16384;
    }
    const Niveau n = mesurer(b, 64);
    VERIFIER(proche(n.rms_dbfs_x10, -60.2, 1.0));
    VERIFIER(proche(n.crete_dbfs_x10, -60.2, 1.0));
    VERIFIER(n.crete == 16384);
    VERIFIER(n.ecretes == 0);
    VERIFIER(n.continu == 16384);
    return 0;
}

constexpr int silence_et_ecretage() {
    std::int16_t zero[16] = {};
    const Niveau s = mesurer(zero, 16);
    VERIFIER(s.rms_dbfs_x10 == kSilence_x10 && s.crete_dbfs_x10 == kSilence_x10 && s.crete == 0 && s.ecretes == 0);
    std::int16_t sature[8] = {32767, -32768, 32767, -32768, 100, 100, 100, 100};
    const Niveau c = mesurer(sature, 8);
    VERIFIER(c.ecretes == 4);
    VERIFIER(c.crete == 32768);
    VERIFIER(c.crete_dbfs_x10 == 0);
    VERIFIER(mesurer(nullptr, 8).crete == 0);
    VERIFIER(mesurer(zero, 0).crete == 0);
    return 0;
}

// Voie gauche d'un flux stereo entrelace : pas = 2.
constexpr int voies_d_un_flux_entrelace() {
    std::int16_t st[32] = {};
    for (std::size_t i = 0; i < 16; ++i) {
        st[2 * i] = 8192;    // gauche
        st[2 * i + 1] = 100; // droite
    }
    const Niveau g = mesurer(st, 16, 2);
    const Niveau d = mesurer(st + 1, 16, 2);
    VERIFIER(g.crete == 8192);
    VERIFIER(d.crete == 100);
    VERIFIER(g.rms_dbfs_x10 > d.rms_dbfs_x10);
    return 0;
}

constexpr int sinus_de_taylor() {
    VERIFIER(proche(static_cast<std::int32_t>(sinus(0.0) * 1e6), 0.0, 2.0));
    VERIFIER(proche(static_cast<std::int32_t>(sinus(kPi / 2) * 1e6), 1e6, 2.0));
    VERIFIER(proche(static_cast<std::int32_t>(sinus(kPi / 6) * 1e6), 5e5, 2.0));
    VERIFIER(proche(static_cast<std::int32_t>(sinus(-kPi / 2) * 1e6), -1e6, 2.0));
    VERIFIER(proche(static_cast<std::int32_t>(sinus(kPi) * 1e6), 0.0, 2.0));
    VERIFIER(proche(static_cast<std::int32_t>(cosinus(0.0) * 1e6), 1e6, 2.0));
    VERIFIER(proche(static_cast<std::int32_t>(cosinus(kPi) * 1e6), -1e6, 2.0));
    VERIFIER(
        proche(static_cast<std::int32_t>(sinus(7.0) * 1e6), 656987.0, 3.0)); // sin(7) = 0,6569866 (au-dela de 2 pi)
    return 0;
}

constexpr int racine_de_newton() {
    VERIFIER(proche(static_cast<std::int32_t>(racine(2.0) * 1e6), 1414214.0, 2.0));
    VERIFIER(racine(0.0) == 0.0);
    VERIFIER(proche(static_cast<std::int32_t>(racine(0.25) * 1e6), 500000.0, 2.0));
    return 0;
}

// 1 kHz a 16 kHz : 16 echantillons par periode.
constexpr int le_sinus_genere_a_la_bonne_amplitude_et_periode() {
    VERIFIER(echantillon_sinus(1000, 16000, 0, 10000) == 0);
    VERIFIER(echantillon_sinus(1000, 16000, 4, 10000) == 10000); // quart de periode : crete
    VERIFIER(echantillon_sinus(1000, 16000, 12, 10000) == -10000);
    VERIFIER(proche(echantillon_sinus(1000, 16000, 2, 10000), 7071.0, 2.0)); // 45 degres
    VERIFIER(echantillon_sinus(1000, 16000, 16, 10000) == echantillon_sinus(1000, 16000, 0, 10000));
    VERIFIER(echantillon_sinus(1000, 16000, 100000004, 10000) ==
             10000); // pas de derive apres cent millions d'echantillons
    VERIFIER(echantillon_sinus(1000, 0, 4, 10000) == 0);
    VERIFIER(echantillon_sinus(1000, 16000, 4, 50000) == 32767); // bornee
    return 0;
}

constexpr int amplitude_pour_un_niveau_de_crete() {
    VERIFIER(amplitude_pour_dbfs_x10(0) == 32767);
    VERIFIER(amplitude_pour_dbfs_x10(50) == 32767);
    VERIFIER(proche(amplitude_pour_dbfs_x10(-200), 3277.0, 3.0));
    VERIFIER(proche(amplitude_pour_dbfs_x10(-100), 10362.0, 4.0));
    VERIFIER(proche(amplitude_pour_dbfs_x10(-333), 709.0, 3.0));
    VERIFIER(proche(amplitude_pour_dbfs_x10(-600), 33.0, 2.0));
    return 0;
}

// Le Goertzel retrouve le niveau d'un sinus de 1 kHz et ne voit presque rien a 2 kHz.
constexpr int goertzel_mesure_un_sinus() {
    constexpr std::size_t N = 800; // 50 ms a 16 kHz, 50 periodes entieres
    std::int16_t b[N] = {};
    for (std::size_t i = 0; i < N; ++i) {
        b[i] = echantillon_sinus(1000, 16000, i, 10000);
    }
    const std::int32_t a1k = goertzel_dbfs_x10(b, N, 1000, 16000);
    VERIFIER(proche(a1k, -102.9, 5.0)); // 20 log10(10000/32768) = -10,31 dBFS -> -103,1 en dixiemes
    const std::int32_t a2k = goertzel_dbfs_x10(b, N, 2000, 16000);
    VERIFIER(a2k < a1k - 400); // au moins 40 dB plus bas hors de la frequence
    // Le niveau suit l'amplitude : moitie d'amplitude = 6 dB de moins.
    std::int16_t c[N] = {};
    for (std::size_t i = 0; i < N; ++i) {
        c[i] = echantillon_sinus(1000, 16000, i, 5000);
    }
    const std::int32_t c1k = goertzel_dbfs_x10(c, N, 1000, 16000);
    VERIFIER(proche(a1k - c1k, 60.2, 4.0));
    return 0;
}

constexpr int goertzel_entrees_invalides() {
    std::int16_t b[16] = {};
    VERIFIER(goertzel_dbfs_x10(nullptr, 16, 1000, 16000) == kSilence_x10);
    VERIFIER(goertzel_dbfs_x10(b, 4, 1000, 16000) == kSilence_x10);
    VERIFIER(goertzel_dbfs_x10(b, 16, 0, 16000) == kSilence_x10);
    VERIFIER(goertzel_dbfs_x10(b, 16, 9000, 16000) == kSilence_x10); // au-dela de Nyquist
    VERIFIER(goertzel_dbfs_x10(b, 16, 1000, 16000) == kSilence_x10); // silence
    return 0;
}

// Un sinus a 1 kHz, le bloc mesure par mesurer() : le niveau efficace est 3 dB sous la crete.
constexpr int rms_d_un_sinus_est_trois_db_sous_la_crete() {
    constexpr std::size_t N = 1600;
    std::int16_t b[N] = {};
    for (std::size_t i = 0; i < N; ++i) {
        b[i] = echantillon_sinus(1000, 16000, i, 10000);
    }
    const Niveau n = mesurer(b, N);
    VERIFIER(n.crete == 10000);
    VERIFIER(proche(n.crete_dbfs_x10 - n.rms_dbfs_x10, 30.1, 2.0));
    VERIFIER(proche(n.continu, 0.0, 2.0));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(log2_connu)                                                                                                      \
    X(amplitude_en_dbfs_valeurs_de_reference)                                                                          \
    X(energie_en_dbfs)                                                                                                 \
    X(niveau_d_un_bloc_constant)                                                                                       \
    X(silence_et_ecretage)                                                                                             \
    X(voies_d_un_flux_entrelace)                                                                                       \
    X(sinus_de_taylor)                                                                                                 \
    X(racine_de_newton)                                                                                                \
    X(le_sinus_genere_a_la_bonne_amplitude_et_periode)                                                                 \
    X(amplitude_pour_un_niveau_de_crete)                                                                               \
    X(goertzel_mesure_un_sinus)                                                                                        \
    X(goertzel_entrees_invalides)                                                                                      \
    X(rms_d_un_sinus_est_trois_db_sous_la_crete)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
