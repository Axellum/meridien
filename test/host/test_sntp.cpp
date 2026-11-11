// Tests du noyau pur : demande d'heure SNTP directe — lecture du paquet et planification des demandes (sntp.hpp).
// Les deux paquets « réels » sont des réponses reçues le 11/11/2026 depuis un PC (0.fr.pool.ntp.org, strate 3, et 2.pool.ntp.org,
// strate 2) à une requête portant le nonce 01 23 45 67 89 AB CD EF ; l'identifiant de référence (octets 12-15, adresse d'un serveur
// amont) est mis à zéro, il n'entre pas dans la lecture. Les valeurs attendues viennent de Python (struct.unpack, hors du code testé).
#include "ecran/sntp.hpp"
#include "mini.hpp"

using namespace ecran::sntp;

namespace {

constexpr std::uint8_t NONCE[kTailleNonce] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

constexpr std::uint8_t REEL_STRATE_3[kTaillePaquet] = {
    0x24, 0x03, 0x00, 0xE3, 0x00, 0x00, 0x03, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xEE, 0x9F, 0x26, 0x65, 0x14, 0x87, 0xFF, 0xFF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEE, 0x9F, 0x41, 0x51, 0x5C, 0x42, 0x3F, 0xFF, 0xEE, 0x9F, 0x41, 0x51, 0x5C, 0x42, 0x5F, 0xFF};
constexpr std::uint8_t REEL_STRATE_2[kTaillePaquet] = {
    0x24, 0x02, 0x01, 0xEA, 0x00, 0x00, 0x01, 0x6B, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0xEE, 0x9F, 0x41, 0x36, 0x0B, 0x1D, 0x39, 0xB7, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEE, 0x9F, 0x41, 0x51, 0x69, 0x9C, 0x0E, 0x9B, 0xEE, 0x9F, 0x41, 0x51, 0x69, 0xB5, 0x5E, 0x0E};

struct Paquet {
    std::uint8_t o[64] = {};
};

constexpr Paquet copie(const std::uint8_t (&source)[kTaillePaquet]) {
    Paquet p;
    for (int i = 0; i < kTaillePaquet; ++i) {
        p.o[i] = source[i];
    }
    return p;
}

// Réponse minimale valide : serveur, strate 2, notre nonce, horodatage d'émission donné.
constexpr Paquet reponse(std::uint32_t secondes, std::uint32_t fraction) {
    Paquet p;
    p.o[0] = 0x24;
    p.o[1] = 2;
    for (int i = 0; i < kTailleNonce; ++i) {
        p.o[kPositionOrigine + i] = NONCE[i];
    }
    for (int i = 0; i < 4; ++i) {
        p.o[kPositionEmission + i] = static_cast<std::uint8_t>(secondes >> (24 - 8 * i));
        p.o[kPositionEmission + 4 + i] = static_cast<std::uint8_t>(fraction >> (24 - 8 * i));
    }
    return p;
}

constexpr Refus lire(const Paquet &p, int taille, Instant &i) { return lire_reponse(p.o, taille, NONCE, i); }

// ---- paquet ----------------------------------------------------------------------------------------------------------------

constexpr int paquets_reels_acceptes() {
    Instant i;
    VERIFIER(lire(copie(REEL_STRATE_3), kTaillePaquet, i) == Refus::Aucun);
    VERIFIER(i.secondes == 1794425553);
    VERIFIER(i.microsecondes == 360387);
    Instant j;
    VERIFIER(lire(copie(REEL_STRATE_2), kTaillePaquet, j) == Refus::Aucun);
    VERIFIER(j.secondes == 1794425553);
    VERIFIER(j.microsecondes == 412923);
    // Un paquet plus long (extension, code d'authentification) reste lisible.
    Instant k;
    VERIFIER(lire(copie(REEL_STRATE_2), 64, k) == Refus::Aucun);
    VERIFIER(k.secondes == 1794425553);
    return 0;
}

constexpr int requete_porte_le_nonce() {
    std::uint8_t requete[kTaillePaquet] = {};
    for (std::uint8_t &o : requete) {
        o = 0x5A; // reste d'une manche précédente : tout doit être remis à zéro
    }
    preparer_requete(requete, NONCE);
    VERIFIER(requete[0] == 0x23);
    VERIFIER((requete[0] & 7) == 3);        // mode client
    VERIFIER(((requete[0] >> 3) & 7) == 4); // version 4
    for (int i = 1; i < kPositionEmission; ++i) {
        VERIFIER(requete[i] == 0);
    }
    for (int i = 0; i < kTailleNonce; ++i) {
        VERIFIER(requete[kPositionEmission + i] == NONCE[i]);
    }
    return 0;
}

constexpr int paquet_trop_court_refuse() {
    Instant i;
    i.secondes = 42;
    VERIFIER(lire(copie(REEL_STRATE_3), kTaillePaquet - 1, i) == Refus::TropCourt);
    VERIFIER(lire(copie(REEL_STRATE_3), 0, i) == Refus::TropCourt);
    VERIFIER(lire(copie(REEL_STRATE_3), -1, i) == Refus::TropCourt);
    VERIFIER(lire_reponse(nullptr, kTaillePaquet, NONCE, i) == Refus::TropCourt);
    VERIFIER(i.secondes == 42); // rien écrit sur un refus
    return 0;
}

constexpr int mode_autre_que_serveur_refuse() {
    Instant i;
    for (int mode = 0; mode < 8; ++mode) {
        Paquet p = copie(REEL_STRATE_3);
        p.o[0] = static_cast<std::uint8_t>((p.o[0] & ~7) | mode);
        VERIFIER((lire(p, kTaillePaquet, i) == Refus::Aucun) == (mode == 4));
        if (mode != 4) {
            VERIFIER(lire(p, kTaillePaquet, i) == Refus::PasUnServeur);
        }
    }
    return 0;
}

constexpr int serveur_non_synchronise_refuse() {
    Instant i;
    Paquet p = copie(REEL_STRATE_3);
    p.o[0] = static_cast<std::uint8_t>(0xC0 | (p.o[0] & 0x3F)); // indicateur d'alerte 3
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::NonSynchronise);
    // Indicateurs 1 et 2 (seconde intercalaire annoncée) : la réponse reste bonne.
    p.o[0] = static_cast<std::uint8_t>(0x40 | (p.o[0] & 0x3F));
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Aucun);
    p.o[0] = static_cast<std::uint8_t>(0x80 | (p.o[0] & 0x3F));
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Aucun);
    return 0;
}

constexpr int strate_invalide_refusee() {
    Instant i;
    Paquet p = copie(REEL_STRATE_3);
    p.o[1] = 0; // « Kiss-of-Death » : le serveur demande de ralentir
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Strate);
    p.o[1] = 16; // non synchronisé
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Strate);
    p.o[1] = 255;
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Strate);
    p.o[1] = 1;
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Aucun);
    p.o[1] = 15;
    VERIFIER(lire(p, kTaillePaquet, i) == Refus::Aucun);
    return 0;
}

constexpr int nonce_different_refuse() {
    Instant i;
    for (int octet = 0; octet < kTailleNonce; ++octet) {
        Paquet p = copie(REEL_STRATE_3);
        p.o[kPositionOrigine + octet] ^= 0x01;
        VERIFIER(lire(p, kTaillePaquet, i) == Refus::NonceDifferent);
    }
    return 0;
}

constexpr int horodatage_nul_refuse() {
    Instant i;
    VERIFIER(lire(reponse(0, 0), kTaillePaquet, i) == Refus::HorodatageNul);
    VERIFIER(lire(reponse(0, 0x80000000u), kTaillePaquet, i) == Refus::HorodatageNul);
    return 0;
}

constexpr int instant_absurde_refuse() {
    Instant i;
    // 31/12/2025 23:59:59 UTC = 1767225599 (Unix) = 3976214399 (NTP).
    VERIFIER(lire(reponse(3976214399u, 0), kTaillePaquet, i) == Refus::Absurde);
    VERIFIER(lire(reponse(3976214400u, 0), kTaillePaquet, i) == Refus::Aucun);
    VERIFIER(i.secondes == 1767225600);
    VERIFIER(lire(reponse(kSecondesEntre1900Et1970, 0), kTaillePaquet, i) == Refus::Absurde); // 1970
    return 0;
}

constexpr int passage_de_2036() {
    // Dernière seconde de l'ère 0 : 07/02/2036 06:28:15 UTC.
    VERIFIER(unix_depuis_ntp(0xFFFFFFFFu) == 2085978495);
    // Ère 1 : les secondes repartent de zéro.
    VERIFIER(unix_depuis_ntp(1) == 2085978497);
    VERIFIER(unix_depuis_ntp(0x7FFFFFFFu) == 4233462143LL);
    VERIFIER(unix_depuis_ntp(static_cast<std::uint32_t>(kSecondesEntre1900Et1970 - 1)) == 4294967295LL);
    VERIFIER(unix_depuis_ntp(static_cast<std::uint32_t>(kSecondesEntre1900Et1970)) == 0);
    Instant i;
    VERIFIER(lire(reponse(1, 0), kTaillePaquet, i) == Refus::Aucun);
    VERIFIER(i.secondes == 2085978497);
    return 0;
}

constexpr int fraction_en_microsecondes() {
    VERIFIER(microsecondes_depuis_fraction(0) == 0);
    VERIFIER(microsecondes_depuis_fraction(0x80000000u) == 500000);
    VERIFIER(microsecondes_depuis_fraction(0x40000000u) == 250000);
    VERIFIER(microsecondes_depuis_fraction(0xFFFFFFFFu) == 999999);
    Instant i;
    VERIFIER(lire(reponse(3976214400u, 0xC0000000u), kTaillePaquet, i) == Refus::Aucun);
    VERIFIER(i.microsecondes == 750000);
    return 0;
}

// ---- planification -----------------------------------------------------------------------------------------------------------

constexpr int delai_double_puis_plafonne() {
    VERIFIER(delai_relance_ms(0) == 0);
    VERIFIER(delai_relance_ms(1) == 10000);
    VERIFIER(delai_relance_ms(2) == 20000);
    VERIFIER(delai_relance_ms(3) == 40000);
    VERIFIER(delai_relance_ms(4) == 80000);
    VERIFIER(delai_relance_ms(5) == 160000);
    VERIFIER(delai_relance_ms(6) == 320000);
    VERIFIER(delai_relance_ms(7) == 600000);
    VERIFIER(delai_relance_ms(kEchecsMax) == 600000);
    VERIFIER(delai_relance_ms(255) == 600000);
    for (unsigned n = 1; n < 256; ++n) {
        VERIFIER(delai_relance_ms(static_cast<std::uint8_t>(n)) >= delai_relance_ms(static_cast<std::uint8_t>(n - 1)));
    }
    return 0;
}

constexpr int premiere_ip_lance_une_demande() {
    Planificateur p;
    const Decision d = sur_nouvelle_ip(p);
    VERIFIER(d.lancer);
    VERIFIER(!d.annuler_relance);
    VERIFIER(d.relance_dans_ms == 0);
    VERIFIER(p.en_cours);
    return 0;
}

constexpr int jamais_deux_demandes_a_la_fois() {
    Planificateur p;
    VERIFIER(sur_nouvelle_ip(p).lancer);
    const Decision d = sur_nouvelle_ip(p); // nouvelle IP pendant la demande
    VERIFIER(!d.lancer);
    VERIFIER(d.relance_dans_ms == 0);
    VERIFIER(p.en_cours);
    return 0;
}

constexpr int succes_ne_programme_rien() {
    Planificateur p;
    sur_nouvelle_ip(p);
    const Decision d = sur_fin(p, true);
    VERIFIER(!d.lancer);
    VERIFIER(d.relance_dans_ms == 0);
    VERIFIER(!p.en_cours);
    VERIFIER(!p.relance_prevue);
    VERIFIER(p.echecs == 0);
    // Une nouvelle IP plus tard (box redémarrée) relance une demande.
    VERIFIER(sur_nouvelle_ip(p).lancer);
    return 0;
}

constexpr int echecs_relancent_avec_un_delai_croissant() {
    Planificateur p;
    sur_nouvelle_ip(p);
    Decision d = sur_fin(p, false);
    VERIFIER(!d.lancer);
    VERIFIER(d.relance_dans_ms == 10000);
    VERIFIER(!p.en_cours);
    VERIFIER(p.relance_prevue);
    d = sur_minuteur(p);
    VERIFIER(d.lancer);
    VERIFIER(p.en_cours);
    VERIFIER(!p.relance_prevue);
    d = sur_fin(p, false);
    VERIFIER(d.relance_dans_ms == 20000);
    sur_minuteur(p);
    d = sur_fin(p, false);
    VERIFIER(d.relance_dans_ms == 40000);
    // Un succès remet le compte à zéro.
    sur_minuteur(p);
    sur_fin(p, true);
    VERIFIER(p.echecs == 0);
    VERIFIER(!p.relance_prevue);
    return 0;
}

constexpr int echecs_sans_fin_restent_au_plafond() {
    Planificateur p;
    sur_nouvelle_ip(p);
    Decision d;
    for (int n = 0; n < 1000; ++n) {
        d = sur_fin(p, false);
        VERIFIER(d.relance_dans_ms > 0);
        VERIFIER(d.relance_dans_ms <= kRelanceMaxMs);
        VERIFIER(sur_minuteur(p).lancer);
    }
    VERIFIER(p.echecs == kEchecsMax);
    VERIFIER(sur_fin(p, false).relance_dans_ms == kRelanceMaxMs);
    return 0;
}

constexpr int nouvelle_ip_remplace_la_relance_prevue() {
    Planificateur p;
    sur_nouvelle_ip(p);
    sur_fin(p, false);
    sur_minuteur(p);
    sur_fin(p, false); // deux échecs : relance prévue dans 20 s
    VERIFIER(p.echecs == 2);
    const Decision d = sur_nouvelle_ip(p);
    VERIFIER(d.lancer);
    VERIFIER(d.annuler_relance);
    VERIFIER(!p.relance_prevue);
    VERIFIER(p.echecs == 0);
    // Le minuteur, s'il sonne quand même (annulation trop tardive), ne lance pas une seconde demande.
    const Decision tard = sur_minuteur(p);
    VERIFIER(!tard.lancer);
    VERIFIER(p.en_cours);
    // Et si la demande lancée par l'IP échoue, on repart du premier délai.
    VERIFIER(sur_fin(p, false).relance_dans_ms == 10000);
    return 0;
}

constexpr int ip_pendant_une_demande_remet_les_echecs_a_zero() {
    Planificateur p;
    sur_nouvelle_ip(p);
    for (int n = 0; n < 4; ++n) {
        sur_fin(p, false);
        sur_minuteur(p);
    }
    VERIFIER(p.echecs == 4);
    VERIFIER(p.en_cours);
    VERIFIER(!sur_nouvelle_ip(p).lancer);
    VERIFIER(sur_fin(p, false).relance_dans_ms == 10000);
    return 0;
}

constexpr int evenements_hors_propos_sans_effet() {
    Planificateur p;
    Decision d = sur_fin(p, false); // fin sans demande
    VERIFIER(!d.lancer);
    VERIFIER(d.relance_dans_ms == 0);
    VERIFIER(p.echecs == 0);
    VERIFIER(!p.relance_prevue);
    d = sur_minuteur(p); // minuteur sans relance prévue
    VERIFIER(!d.lancer);
    VERIFIER(!p.en_cours);
    // Minuteur qui sonne alors qu'une demande tourne : pas de doublon.
    sur_nouvelle_ip(p);
    p.relance_prevue = true;
    d = sur_minuteur(p);
    VERIFIER(!d.lancer);
    VERIFIER(!p.relance_prevue);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(paquets_reels_acceptes)                                                                                          \
    X(requete_porte_le_nonce)                                                                                          \
    X(paquet_trop_court_refuse)                                                                                        \
    X(mode_autre_que_serveur_refuse)                                                                                   \
    X(serveur_non_synchronise_refuse)                                                                                  \
    X(strate_invalide_refusee)                                                                                         \
    X(nonce_different_refuse)                                                                                          \
    X(horodatage_nul_refuse)                                                                                           \
    X(instant_absurde_refuse)                                                                                          \
    X(passage_de_2036)                                                                                                 \
    X(fraction_en_microsecondes)                                                                                       \
    X(delai_double_puis_plafonne)                                                                                      \
    X(premiere_ip_lance_une_demande)                                                                                   \
    X(jamais_deux_demandes_a_la_fois)                                                                                  \
    X(succes_ne_programme_rien)                                                                                        \
    X(echecs_relancent_avec_un_delai_croissant)                                                                        \
    X(echecs_sans_fin_restent_au_plafond)                                                                              \
    X(nouvelle_ip_remplace_la_relance_prevue)                                                                          \
    X(ip_pendant_une_demande_remet_les_echecs_a_zero)                                                                  \
    X(evenements_hors_propos_sans_effet)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
