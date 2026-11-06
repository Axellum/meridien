// Noyau pur : appairage de la page de réglages (docs/api-reglages.md §2, ADR-0003). Un code à 6 chiffres affiché sur l'écran de
// l'appareil s'échange contre un jeton de session de 32 octets (64 caractères hexadécimaux), gardé en RAM.
//
// Règles :
//   - 5 essais ratés de suite -> verrou de 60 s, pendant lequel même le bon code est refusé ; le verrou levé, le compte repart à 0 ;
//   - 4 sessions au plus : à la cinquième, la plus ancienne est fermée ;
//   - comparaison à temps constant (le code et le jeton ne se devinent pas à la durée de la réponse) ;
//   - aucune horloge ni source aléatoire ici : l'appelant injecte l'instant (millisecondes) et un générateur d'octets, pour que le
//     tout se teste sur le PC. Rien n'est écrit en NVS : un redémarrage ferme les sessions et change le code (§2).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation (les tests de test/host/ s'en servent).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::appairage {

inline constexpr int ESSAIS_MAX = 5;
inline constexpr std::int64_t VERROU_MS = 60000;
inline constexpr std::size_t SESSIONS_MAX = 4;
inline constexpr std::size_t CODE_CHIFFRES = 6;
inline constexpr std::size_t JETON_OCTETS = 32;
inline constexpr std::size_t JETON_CARACTERES = 2 * JETON_OCTETS;

// Égalité à temps constant de deux textes : la durée ne dépend que des longueurs, pas de l'endroit de la première différence.
constexpr bool egal_temps_constant(std::string_view a, std::string_view b) noexcept {
    unsigned diff = a.size() == b.size() ? 0u : 1u;
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i)
        diff |= static_cast<unsigned>(static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]));
    return diff == 0;
}

enum class Issue : std::uint8_t {
    Accepte,    // bon code : `jeton` est rempli
    Refuse,     // mauvais code (403)
    Verrouille, // trop d'essais (429) : `reessayer_dans` donne les secondes restantes
};

struct Resultat {
    Issue issue = Issue::Refuse;
    int reessayer_dans = 0;
    char jeton[JETON_CARACTERES + 1]{};
};

class Gestionnaire {
  public:
    // Fixe le code à 6 chiffres depuis un entier aléatoire sur 32 bits (zéros de tête conservés : « 004217 »). Le léger biais du
    // modulo (4,3 milliards % 1 million) est de l'ordre de 0,0001 % : sans effet sur un code qui change à chaque démarrage.
    constexpr void nouveau_code(std::uint32_t aleatoire) noexcept {
        std::uint32_t v = aleatoire % 1000000u;
        for (std::size_t i = CODE_CHIFFRES; i > 0; --i) {
            code_[i - 1] = static_cast<char>('0' + v % 10u);
            v /= 10u;
        }
        code_[CODE_CHIFFRES] = '\0';
        for (Session &s : sessions_)
            s = Session{}; // un nouveau code ferme les sessions ouvertes avec l'ancien
    }

    constexpr std::string_view code() const noexcept { return std::string_view(code_, CODE_CHIFFRES); }

    // `alea(uint8_t *sortie, std::size_t n)` remplit `n` octets aléatoires ; appelé seulement si le code est bon.
    template <class Alea>
    constexpr Resultat essayer(std::string_view propose, std::int64_t maintenant_ms, Alea &&alea) noexcept {
        Resultat r;
        if (maintenant_ms < verrou_jusqua_ms_) {
            r.issue = Issue::Verrouille;
            r.reessayer_dans = static_cast<int>((verrou_jusqua_ms_ - maintenant_ms + 999) / 1000);
            return r;
        }
        if (verrou_jusqua_ms_ != 0) { // le verrou est levé : on repart de zéro
            verrou_jusqua_ms_ = 0;
            echecs_ = 0;
        }
        if (!egal_temps_constant(propose, code())) {
            if (++echecs_ >= ESSAIS_MAX) {
                echecs_ = 0;
                verrou_jusqua_ms_ = maintenant_ms + VERROU_MS;
                r.issue = Issue::Verrouille;
                r.reessayer_dans = static_cast<int>(VERROU_MS / 1000);
            }
            return r;
        }
        echecs_ = 0;
        std::uint8_t octets[JETON_OCTETS] = {};
        alea(octets, JETON_OCTETS);
        constexpr char HEX[] = "0123456789abcdef";
        for (std::size_t i = 0; i < JETON_OCTETS; ++i) {
            r.jeton[2 * i] = HEX[octets[i] >> 4];
            r.jeton[2 * i + 1] = HEX[octets[i] & 15u];
        }
        r.jeton[JETON_CARACTERES] = '\0';
        ouvrir(std::string_view(r.jeton, JETON_CARACTERES));
        r.issue = Issue::Accepte;
        return r;
    }

    // Le jeton est-il celui d'une session ouverte ? Parcourt toutes les sessions sans s'arrêter au premier succès.
    constexpr bool jeton_valide(std::string_view jeton) const noexcept {
        bool trouve = false;
        for (const Session &s : sessions_) {
            if (s.ouverte && egal_temps_constant(jeton, std::string_view(s.jeton, JETON_CARACTERES)))
                trouve = true;
        }
        return trouve;
    }

    constexpr void fermer(std::string_view jeton) noexcept {
        for (Session &s : sessions_) {
            if (s.ouverte && egal_temps_constant(jeton, std::string_view(s.jeton, JETON_CARACTERES)))
                s = Session{};
        }
    }

    constexpr std::size_t sessions_ouvertes() const noexcept {
        std::size_t n = 0;
        for (const Session &s : sessions_)
            n += s.ouverte ? 1u : 0u;
        return n;
    }

  private:
    struct Session {
        bool ouverte = false;
        std::uint32_t rang = 0; // ordre d'ouverture : la plus petite valeur est la plus ancienne
        char jeton[JETON_CARACTERES + 1]{};
    };

    constexpr void ouvrir(std::string_view jeton) noexcept {
        Session *cible = nullptr;
        for (Session &s : sessions_) {
            if (!s.ouverte) {
                cible = &s;
                break;
            }
        }
        if (cible == nullptr) { // pleine : on ferme la plus ancienne
            cible = &sessions_[0];
            for (Session &s : sessions_) {
                if (s.rang < cible->rang)
                    cible = &s;
            }
        }
        cible->ouverte = true;
        cible->rang = ++compteur_;
        for (std::size_t i = 0; i < JETON_CARACTERES; ++i)
            cible->jeton[i] = jeton[i];
        cible->jeton[JETON_CARACTERES] = '\0';
    }

    char code_[CODE_CHIFFRES + 1]{};
    Session sessions_[SESSIONS_MAX]{};
    int echecs_ = 0;
    std::int64_t verrou_jusqua_ms_ = 0;
    std::uint32_t compteur_ = 0;
};

} // namespace ecran::appairage
