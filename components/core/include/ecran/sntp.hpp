// Noyau pur : demande d'heure SNTP directe — lecture du paquet de réponse et planification des demandes. Aucun socket ici :
// main/sntp_rapide.cpp envoie et reçoit les paquets, ce fichier dit si une réponse est bonne et QUAND redemander.
//
// Paquet SNTP (RFC 4330, 48 octets, entiers gros-boutistes) :
//     octet 0      indicateur d'alerte (2 bits de poids fort), version (3 bits), mode (3 bits de poids faible : 3 client, 4 serveur)
//     octet 1      strate (0 = « Kiss-of-Death », 1..15 = serveur synchronisé, 16 = non synchronisé)
//     octets 24-31 horodatage d'origine : le serveur y recopie l'horodatage d'émission du client (notre nonce)
//     octets 40-47 horodatage d'émission du serveur : secondes depuis 1900 (32 bits) puis fraction de seconde (32 bits)
//
// Politique de demande (chaque règle est un test de test/host/test_sntp.cpp) :
//   - chaque nouvelle adresse IP lance une demande, sauf si une demande est déjà en cours (jamais deux à la fois) ;
//   - une demande réussie ne programme rien : la suivante viendra d'une nouvelle IP (le SNTP de lwIP garde la remise à l'heure
//     périodique) ;
//   - une demande échouée est relancée après un délai qui double à chaque échec, de 10 s à 10 min au plus ;
//   - une nouvelle IP remet le compte des échecs à zéro (nouveau réseau, nouvelle chance).
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstdint>

namespace ecran::sntp {

constexpr int kTaillePaquet = 48;
constexpr int kTailleNonce = 8;
constexpr int kPositionOrigine = 24;  // horodatage d'origine de la réponse : notre nonce recopié
constexpr int kPositionEmission = 40; // horodatage d'émission (du client dans la requête, du serveur dans la réponse)
constexpr std::uint8_t kOctetRequete = 0x23; // pas d'alerte, version 4, mode client
constexpr std::uint64_t kSecondesEntre1900Et1970 = 2208988800ULL;
constexpr std::int64_t kUnixMinimum = 1767225600; // 1er janvier 2026 : en dessous, la réponse est absurde

enum class Refus : std::uint8_t {
    Aucun,          // réponse acceptée
    TropCourt,      // moins de 48 octets
    PasUnServeur,   // mode différent de 4
    NonSynchronise, // indicateur d'alerte 3 : horloge du serveur non synchronisée
    Strate,         // strate 0 (« Kiss-of-Death ») ou au-delà de 15
    NonceDifferent, // pas la réponse à NOTRE dernière requête (paquet falsifié ou en retard d'une manche)
    HorodatageNul,  // secondes à zéro : le serveur n'a pas d'heure
    Absurde,        // avant 2026
};

struct Instant {
    std::int64_t secondes = 0;       // depuis 1970, UTC
    std::uint32_t microsecondes = 0; // 0..999 999
};

constexpr std::uint32_t lire_u32(const std::uint8_t *o) noexcept {
    return (static_cast<std::uint32_t>(o[0]) << 24) | (static_cast<std::uint32_t>(o[1]) << 16) |
           (static_cast<std::uint32_t>(o[2]) << 8) | static_cast<std::uint32_t>(o[3]);
}

// Requête client : tout à zéro sauf l'octet 0 et le nonce, placé dans l'horodatage d'émission (le serveur le recopiera).
constexpr void preparer_requete(std::uint8_t (&requete)[kTaillePaquet],
                                const std::uint8_t (&nonce)[kTailleNonce]) noexcept {
    for (std::uint8_t &o : requete) {
        o = 0;
    }
    requete[0] = kOctetRequete;
    for (int i = 0; i < kTailleNonce; ++i) {
        requete[kPositionEmission + i] = nonce[i];
    }
}

// Secondes NTP -> secondes Unix. Les secondes NTP repartent de zéro le 7 février 2036 : une valeur sous 1970 (bit de poids fort à 0)
// appartient à l'ère suivante.
constexpr std::int64_t unix_depuis_ntp(std::uint32_t secondes) noexcept {
    const std::uint64_t depuis_1900 =
        (secondes >= kSecondesEntre1900Et1970) ? secondes : (static_cast<std::uint64_t>(secondes) + (1ULL << 32));
    return static_cast<std::int64_t>(depuis_1900 - kSecondesEntre1900Et1970);
}

// Fraction NTP (1/2^32 s) -> microsecondes, arrondi par défaut.
constexpr std::uint32_t microsecondes_depuis_fraction(std::uint32_t fraction) noexcept {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(fraction) * 1000000ULL) >> 32);
}

// Lit `reponse` (`taille` octets reçus) comme la réponse à la requête qui portait `nonce`. Remplit `instant` seulement si la réponse
// est acceptée.
constexpr Refus lire_reponse(const std::uint8_t *reponse, int taille, const std::uint8_t (&nonce)[kTailleNonce],
                             Instant &instant) noexcept {
    if (reponse == nullptr || taille < kTaillePaquet) {
        return Refus::TropCourt;
    }
    if ((reponse[0] & 7) != 4) {
        return Refus::PasUnServeur;
    }
    if ((reponse[0] >> 6) == 3) {
        return Refus::NonSynchronise;
    }
    const int strate = reponse[1];
    if (strate < 1 || strate > 15) {
        return Refus::Strate;
    }
    for (int i = 0; i < kTailleNonce; ++i) {
        if (reponse[kPositionOrigine + i] != nonce[i]) {
            return Refus::NonceDifferent;
        }
    }
    const std::uint32_t secondes = lire_u32(reponse + kPositionEmission);
    if (secondes == 0) {
        return Refus::HorodatageNul;
    }
    const std::int64_t unix_s = unix_depuis_ntp(secondes);
    if (unix_s < kUnixMinimum) {
        return Refus::Absurde;
    }
    instant.secondes = unix_s;
    instant.microsecondes = microsecondes_depuis_fraction(lire_u32(reponse + kPositionEmission + 4));
    return Refus::Aucun;
}

// ---- planification des demandes ------------------------------------------------------------------------------------------

constexpr std::uint32_t kRelanceMinMs = 10000;  // après le premier échec
constexpr std::uint32_t kRelanceMaxMs = 600000; // plafond : 10 min
constexpr std::uint8_t kEchecsMax = 30;         // compteur borné (le délai est au plafond bien avant)

// Délai avant de redemander après `echecs` échecs consécutifs (1 = premier échec) : 10 s, 20 s, 40 s… jusqu'à 10 min.
constexpr std::uint32_t delai_relance_ms(std::uint8_t echecs) noexcept {
    if (echecs == 0) {
        return 0;
    }
    std::uint32_t delai = kRelanceMinMs;
    for (std::uint8_t i = 1; i < echecs && delai < kRelanceMaxMs; ++i) {
        delai *= 2;
    }
    return delai < kRelanceMaxMs ? delai : kRelanceMaxMs;
}

struct Planificateur {
    bool en_cours = false;       // une demande (tâche) tourne
    bool relance_prevue = false; // un minuteur de relance est armé
    std::uint8_t echecs = 0;     // échecs consécutifs depuis la dernière IP ou le dernier succès
};

struct Decision {
    bool lancer = false;               // créer la tâche de demande maintenant
    bool annuler_relance = false;      // arrêter le minuteur de relance armé
    std::uint32_t relance_dans_ms = 0; // > 0 : armer le minuteur de relance
};

// Nouvelle adresse IP.
constexpr Decision sur_nouvelle_ip(Planificateur &p) noexcept {
    Decision d;
    p.echecs = 0;
    if (p.en_cours) {
        return d; // la demande en cours sert ; si elle échoue, sa relance partira du compte remis à zéro
    }
    if (p.relance_prevue) {
        p.relance_prevue = false;
        d.annuler_relance = true;
    }
    p.en_cours = true;
    d.lancer = true;
    return d;
}

// Fin de la demande en cours (`reussi` : horloge réglée).
constexpr Decision sur_fin(Planificateur &p, bool reussi) noexcept {
    Decision d;
    if (!p.en_cours) {
        return d; // fin sans demande connue : rien à décider
    }
    p.en_cours = false;
    if (reussi) {
        p.echecs = 0;
        return d;
    }
    if (p.echecs < kEchecsMax) {
        ++p.echecs;
    }
    p.relance_prevue = true;
    d.relance_dans_ms = delai_relance_ms(p.echecs);
    return d;
}

// Le minuteur de relance a sonné.
constexpr Decision sur_minuteur(Planificateur &p) noexcept {
    Decision d;
    if (!p.relance_prevue) {
        return d; // minuteur annulé entre-temps
    }
    p.relance_prevue = false;
    if (p.en_cours) {
        return d; // une IP a déjà relancé une demande
    }
    p.en_cours = true;
    d.lancer = true;
    return d;
}

} // namespace ecran::sntp
