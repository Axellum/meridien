// Noyau pur : appairage de la page de réglages (docs/api-reglages.md §2, ADR-0003). Un code à 6 chiffres affiché sur l'écran de
// l'appareil s'échange contre un jeton de session de 32 octets (64 caractères hexadécimaux), gardé en RAM.
//
// Règles :
//   - 5 essais ratés de suite -> verrou, pendant lequel même le bon code est refusé ; le verrou levé, le compte repart à 0 ;
//   - le verrou dure 60 s, puis double à chaque série ratée qui suit sans succès entre deux (120 s, 240 s… plafond 1 h) : au long
//     cours, un client du réseau local ne tente plus que 5 codes par heure au lieu de 5 par minute ;
//   - à partir de la 3e série ratée de suite, chaque verrou tire aussi un nouveau code (les sessions ouvertes restent) : un code ne
//     reçoit jamais plus de 15 essais faux, et le code qui change à l'écran rend l'attaque visible. Les deux premières séries
//     laissent le code en place : quelqu'un qui se trompe en recopiant garde le code qu'il lit ;
//   - un succès remet la durée du verrou à 60 s ; `nouveau_code` (démarrage, bouton « Nouveau code » de l'écran) lève en plus le
//     verrou en cours : la personne devant l'écran n'est jamais enfermée dehors par un tiers (le verrou est global, pas par client) ;
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
inline constexpr std::int64_t VERROU_MAX_MS = 3600000; // plafond du verrou qui double : une heure
inline constexpr int VERROUS_AVANT_NOUVEAU_CODE = 3;   // la 3e série ratée de suite (15 essais faux) change le code
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

// Le texte à 6 chiffres d'un entier aléatoire sur 32 bits (zéros de tête conservés : « 004217 »), terminé par un zéro. Le léger
// biais du modulo (4,3 milliards % 1 million) est de l'ordre de 0,0001 % : sans effet. Le même calcul sert au gestionnaire et à
// l'écran (main/appairage.cpp) : le code affiché est celui que la page acceptera.
constexpr void ecrire_code(std::uint32_t aleatoire, char (&sortie)[CODE_CHIFFRES + 1]) noexcept {
    std::uint32_t v = aleatoire % 1000000u;
    for (std::size_t i = CODE_CHIFFRES; i > 0; --i) {
        sortie[i - 1] = static_cast<char>('0' + v % 10u);
        v /= 10u;
    }
    sortie[CODE_CHIFFRES] = '\0';
}

// Durée du verrou de la `serie`-ième série ratée de suite (1 = la première) : 60 s, doublée à chaque série, plafonnée à 1 h.
constexpr std::int64_t duree_verrou(int serie) noexcept {
    std::int64_t d = VERROU_MS;
    for (int i = 1; i < serie && d < VERROU_MAX_MS; ++i)
        d *= 2;
    return d < VERROU_MAX_MS ? d : VERROU_MAX_MS;
}

struct Resultat {
    Issue issue = Issue::Refuse;
    int reessayer_dans = 0;
    bool code_change = false; // ce verrou a tiré un nouveau code (à montrer sur l'écran)
    char jeton[JETON_CARACTERES + 1]{};
};

class Gestionnaire {
  public:
    // Fixe le code à 6 chiffres depuis un entier aléatoire sur 32 bits (voir `ecrire_code`), ferme les sessions ouvertes et lève le
    // verrou : c'est le geste de la personne devant l'écran (bouton « Nouveau code ») ou un démarrage.
    constexpr void nouveau_code(std::uint32_t aleatoire) noexcept {
        tirer_code(aleatoire);
        for (Session &s : sessions_)
            s = Session{}; // un nouveau code ferme les sessions ouvertes avec l'ancien
        echecs_ = 0;
        series_ = 0;
        verrou_jusqua_ms_ = 0;
    }

    constexpr std::string_view code() const noexcept { return std::string_view(code_, CODE_CHIFFRES); }

    // Change à chaque nouveau code (démarrage, bouton, série d'essais ratés) : l'appelant sait qu'il doit remettre l'écran à jour.
    constexpr std::uint32_t generation() const noexcept { return generation_; }

    // `alea(uint8_t *sortie, std::size_t n)` remplit `n` octets aléatoires ; appelé quand le code est bon (le jeton) et quand une
    // série ratée tire un nouveau code (4 octets).
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
                if (series_ < SERIES_COMPTEES)
                    ++series_;
                const std::int64_t duree = duree_verrou(series_);
                verrou_jusqua_ms_ = maintenant_ms + duree;
                if (series_ >= VERROUS_AVANT_NOUVEAU_CODE) {
                    std::uint8_t o[4] = {};
                    alea(o, 4);
                    tirer_code(static_cast<std::uint32_t>(o[0]) | static_cast<std::uint32_t>(o[1]) << 8 |
                               static_cast<std::uint32_t>(o[2]) << 16 | static_cast<std::uint32_t>(o[3]) << 24);
                    r.code_change = true;
                }
                r.issue = Issue::Verrouille;
                r.reessayer_dans = static_cast<int>(duree / 1000);
            }
            return r;
        }
        series_ = 0;
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
    // Au-delà, le verrou est de toute façon au plafond : le compte s'arrête là (pas de débordement).
    static constexpr int SERIES_COMPTEES = 64;

    constexpr void tirer_code(std::uint32_t aleatoire) noexcept {
        ecrire_code(aleatoire, code_);
        ++generation_;
    }

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
    int series_ = 0; // séries de ESSAIS_MAX essais ratés de suite, sans succès entre deux
    std::int64_t verrou_jusqua_ms_ = 0;
    std::uint32_t generation_ = 0;
    std::uint32_t compteur_ = 0;
};

} // namespace ecran::appairage
