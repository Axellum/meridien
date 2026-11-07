// Tests du noyau pur : horloge matérielle RX8130 — trame, plausibilité et politique d'usage (horloge_materielle.hpp).
// Les trames de référence viennent de Python (datetime, hors du code testé) : instants 2026-01-01, 2026-11-06, 2028-02-29 (bissextile),
// 2026-11-07, 2099-12-31 et 2100-01-01.
#include "ecran/horloge_materielle.hpp"
#include "mini.hpp"

using namespace ecran::rtc;

namespace {

struct Reference {
    std::int64_t instant;
    std::uint8_t octet[7];
};

constexpr Reference REFERENCES[] = {
    {1767225600, {0x00, 0x00, 0x00, 0x10, 0x01, 0x01, 0x26}}, // jeudi
    {1793924400, {0x00, 0x20, 0x00, 0x20, 0x06, 0x11, 0x26}}, // vendredi
    {1835440496, {0x56, 0x34, 0x12, 0x04, 0x29, 0x02, 0x28}}, // mardi 29 fevrier 2028
    {1794017109, {0x09, 0x05, 0x02, 0x40, 0x07, 0x11, 0x26}}, // samedi
    {4102444799, {0x59, 0x59, 0x23, 0x10, 0x31, 0x12, 0x99}}, // jeudi, derniere seconde representable
};

constexpr Trame trame_de(const std::uint8_t (&o)[7]) {
    Trame t;
    for (int i = 0; i < 7; ++i) {
        t.octet[i] = o[i];
    }
    return t;
}

// ---- BCD ---------------------------------------------------------------------------------------------------------------

constexpr int bcd_aller_retour() {
    for (unsigned v = 0; v < 100; ++v) {
        VERIFIER(bcd_valide(dec_vers_bcd(v)));
        VERIFIER(bcd_vers_dec(dec_vers_bcd(v)) == v);
    }
    VERIFIER(dec_vers_bcd(59) == 0x59);
    VERIFIER(dec_vers_bcd(7) == 0x07);
    VERIFIER(bcd_vers_dec(0x42) == 42);
    return 0;
}

constexpr int bcd_invalide_refuse() {
    VERIFIER(!bcd_valide(0x0A));
    VERIFIER(!bcd_valide(0xA0));
    VERIFIER(!bcd_valide(0x1F));
    VERIFIER(!bcd_valide(0xFF));
    VERIFIER(bcd_valide(0x99));
    VERIFIER(bcd_valide(0x00));
    return 0;
}

// ---- trame -------------------------------------------------------------------------------------------------------------

constexpr int encodage_egal_aux_trames_de_python() {
    for (const Reference &r : REFERENCES) {
        Trame t;
        VERIFIER(encoder(r.instant, t) == Erreur::Aucune);
        for (int i = 0; i < 7; ++i) {
            VERIFIER(t.octet[i] == r.octet[i]);
        }
    }
    return 0;
}

constexpr int decodage_egal_aux_instants_de_python() {
    for (const Reference &r : REFERENCES) {
        std::int64_t s = 0;
        VERIFIER(decoder(trame_de(r.octet), s) == Erreur::Aucune);
        VERIFIER(s == r.instant);
    }
    return 0;
}

// Chaque seconde d'un jour de bascule (31 decembre -> 1er janvier d'une annee bissextile, et le 29 fevrier) fait l'aller-retour.
constexpr int aller_retour_sur_des_instants_sensibles() {
    constexpr std::int64_t debuts[] = {1835395200 /* 2028-02-29 00:00 */, 1830211200 /* 2027-12-31 00:00 */,
                                       1767225600 /* 2026-01-01 */};
    for (std::int64_t debut : debuts) {
        for (std::int64_t s = debut; s < debut + 86400;
             s += 997) { // pas premier : tombe sur des heures, minutes, secondes variees
            Trame t;
            std::int64_t retour = 0;
            VERIFIER(encoder(s, t) == Erreur::Aucune);
            VERIFIER(decoder(t, retour) == Erreur::Aucune);
            VERIFIER(retour == s);
        }
    }
    return 0;
}

constexpr int instant_hors_plage_non_encode() {
    Trame t;
    VERIFIER(encoder(kInstantPlausibleMin - 1, t) == Erreur::Instant);
    VERIFIER(encoder(0, t) == Erreur::Instant);
    VERIFIER(encoder(-1, t) == Erreur::Instant);
    VERIFIER(encoder(kInstantPlausibleMax + 1, t) == Erreur::Instant); // 2100-01-01 : la trame n'a pas de siecle
    VERIFIER(encoder(kInstantPlausibleMin, t) == Erreur::Aucune);
    VERIFIER(encoder(kInstantPlausibleMax, t) == Erreur::Aucune);
    return 0;
}

constexpr int trame_incoherente_refusee() {
    std::int64_t s = 0;
    Trame bon = trame_de(REFERENCES[1].octet);
    VERIFIER(decoder(bon, s) == Erreur::Aucune);

    Trame t = bon;
    t.octet[1] = 0x6A; // BCD invalide
    VERIFIER(decoder(t, s) == Erreur::Bcd);
    t = bon;
    t.octet[0] = 0x60; // 60 secondes
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[1] = 0x60; // 60 minutes
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[2] = 0x24; // 24 h
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[5] = 0x13; // mois 13
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[5] = 0x00; // mois 0
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[4] = 0x00; // jour 0
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[4] = 0x31; // 31 novembre
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = trame_de(REFERENCES[2].octet); // 29 fevrier 2028 : valide
    VERIFIER(decoder(t, s) == Erreur::Aucune);
    t.octet[6] = 0x27; // 29 fevrier 2027 : n'existe pas
    VERIFIER(decoder(t, s) == Erreur::Plage);
    t = bon;
    t.octet[3] = 0x40; // le 6 novembre 2026 est un vendredi, pas un samedi
    VERIFIER(decoder(t, s) == Erreur::JourSemaine);
    t.octet[3] = 0x30; // deux bits
    VERIFIER(decoder(t, s) == Erreur::JourSemaine);
    t.octet[3] = 0x00; // aucun bit
    VERIFIER(decoder(t, s) == Erreur::JourSemaine);
    t = bon;
    t.octet[6] = 0x00; // 2000-11-06 : avant la borne
    t.octet[3] = 0x04; // (lundi : le jour de la semaine serait juste, c'est l'annee qui est invraisemblable)
    VERIFIER(decoder(t, s) != Erreur::Aucune);
    return 0;
}

// Les bits de commande dans les octets de la puce (ex. bit 7 de la seconde) ne comptent pas dans la valeur.
constexpr int bits_de_commande_masques() {
    std::int64_t s = 0;
    Trame t = trame_de(REFERENCES[3].octet);
    t.octet[0] |= 0x80;
    t.octet[1] |= 0x80;
    t.octet[2] |= 0xC0;
    t.octet[4] |= 0xC0;
    t.octet[5] |= 0xE0;
    VERIFIER(decoder(t, s) == Erreur::Aucune);
    VERIFIER(s == REFERENCES[3].instant);
    return 0;
}

constexpr int plausibilite() {
    VERIFIER(!instant_plausible(0));
    VERIFIER(!instant_plausible(1767225599)); // 2025-12-31T23:59:59
    VERIFIER(instant_plausible(1767225600));
    VERIFIER(instant_plausible(1793924400));
    VERIFIER(instant_plausible(4102444799));
    VERIFIER(!instant_plausible(4102444800));
    return 0;
}

constexpr int heure_affichable_avant_sntp_si_le_systeme_est_plausible() {
    VERIFIER(heure_affichable(false, 1793924400));  // pile reprise ou heure gardée : on l'affiche sans attendre SNTP
    VERIFIER(!heure_affichable(false, 0));          // ni réseau ni heure du système : « --:-- » comme avant
    VERIFIER(!heure_affichable(false, 1767225599)); // avant 2026 : pas une heure vraie
    VERIFIER(heure_affichable(true, 0));            // le réseau a répondu : toujours affichable
    VERIFIER(heure_affichable(true, 1793924400));
    return 0;
}

// ---- politique au démarrage ---------------------------------------------------------------------------------------------

constexpr int demarrage_reprend_l_horloge_quand_le_systeme_est_vide() {
    EntreeDemarrage e;
    e.rtc_lue = true;
    e.rtc_s = 1793924400;
    e.systeme_s = 0; // le systeme repart de 1970
    VERIFIER(au_demarrage(e) == ActionDemarrage::ReprendreRtc);
    e.systeme_s = 5; // quelques secondes depuis le demarrage : toujours pas une heure
    VERIFIER(au_demarrage(e) == ActionDemarrage::ReprendreRtc);
    return 0;
}

// Le coeur de la consigne : une heure deja plausible (venue du reseau) n'est jamais remplacee par celle de la pile.
constexpr int demarrage_n_ecrase_jamais_une_heure_plausible() {
    EntreeDemarrage e;
    e.rtc_lue = true;
    e.rtc_s = 1793924400;
    e.systeme_s = 1793924400 + 3600; // le systeme est plus recent
    VERIFIER(au_demarrage(e) == ActionDemarrage::Rien);
    e.systeme_s = 1793924400 - 3600; // meme plus ancien que la pile, s'il est plausible on le garde
    VERIFIER(au_demarrage(e) == ActionDemarrage::Rien);
    return 0;
}

constexpr int demarrage_ignore_une_horloge_douteuse() {
    EntreeDemarrage e;
    e.rtc_lue = true;
    e.rtc_s = 1793924400;
    e.systeme_s = 0;
    e.rtc_perdue = true; // perte d'alimentation signalee par la puce
    VERIFIER(au_demarrage(e) == ActionDemarrage::Rien);
    e.rtc_perdue = false;
    e.rtc_lue = false; // illisible
    VERIFIER(au_demarrage(e) == ActionDemarrage::Rien);
    e.rtc_lue = true;
    e.rtc_s = 946684800; // 2000-01-01 : la puce n'a jamais ete reglee
    VERIFIER(au_demarrage(e) == ActionDemarrage::Rien);
    e.rtc_s = 4102444800; // 2100
    VERIFIER(au_demarrage(e) == ActionDemarrage::Rien);
    return 0;
}

// ---- qui a réglé l'horloge du système ------------------------------------------------------------------------------------

constexpr int source_reseau_quand_le_systeme_devient_plausible_sans_nous() {
    Observation o;
    o.source_avant = SourceHeure::Aucune;
    o.systeme_plausible_avant = false;
    o.systeme_plausible_maintenant = true;
    VERIFIER(source_apres(o) == SourceHeure::Reseau);
    return 0;
}

constexpr int source_reseau_apres_un_saut_meme_si_nous_avions_repris_la_pile() {
    Observation o;
    o.source_avant = SourceHeure::Rtc;
    o.systeme_plausible_avant = true;
    o.systeme_plausible_maintenant = true;
    o.saut_detecte = true;
    VERIFIER(source_apres(o) == SourceHeure::Reseau);
    return 0;
}

constexpr int source_inchangee_sans_evenement() {
    Observation o;
    o.systeme_plausible_avant = true;
    o.systeme_plausible_maintenant = true;
    o.source_avant = SourceHeure::Rtc;
    VERIFIER(source_apres(o) == SourceHeure::Rtc);
    o.source_avant = SourceHeure::Reseau;
    VERIFIER(source_apres(o) == SourceHeure::Reseau);
    o.source_avant = SourceHeure::Aucune; // heure plausible d'origine inconnue
    VERIFIER(source_apres(o) == SourceHeure::Aucune);
    return 0;
}

constexpr int source_aucune_si_le_systeme_n_est_plus_plausible() {
    Observation o;
    o.source_avant = SourceHeure::Reseau;
    o.systeme_plausible_avant = true;
    o.systeme_plausible_maintenant = false;
    VERIFIER(source_apres(o) == SourceHeure::Aucune);
    return 0;
}

// ---- politique en marche -------------------------------------------------------------------------------------------------

constexpr EntreePeriodique en_marche(SourceHeure source, std::int64_t depuis, std::int64_t ecart = 0) {
    EntreePeriodique e;
    e.source = source;
    e.systeme_s = 1793924400;
    e.rtc_lue = true;
    e.rtc_s = e.systeme_s + ecart;
    e.depuis_derniere_ecriture_s = depuis;
    return e;
}

constexpr int on_n_ecrit_que_depuis_le_reseau() {
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, -1)) == ActionPeriodique::EcrireRtc);
    VERIFIER(periodique(en_marche(SourceHeure::Rtc, -1)) == ActionPeriodique::Rien);    // jamais depuis elle-meme
    VERIFIER(periodique(en_marche(SourceHeure::Aucune, -1)) == ActionPeriodique::Rien); // ni depuis l'inconnu
    VERIFIER(periodique(en_marche(SourceHeure::Rtc, 100000, 500)) == ActionPeriodique::Rien);
    EntreePeriodique douteuse = en_marche(SourceHeure::Reseau, -1);
    douteuse.systeme_s = 100; // heure du reseau « impossible » : on n'ecrit pas
    VERIFIER(periodique(douteuse) == ActionPeriodique::Rien);
    return 0;
}

constexpr int ecriture_au_plus_toutes_les_six_heures() {
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 0)) == ActionPeriodique::Rien);
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 6 * 3600 - 1)) == ActionPeriodique::Rien);
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 6 * 3600)) == ActionPeriodique::EcrireRtc);
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 100000)) == ActionPeriodique::EcrireRtc);
    return 0;
}

constexpr int un_ecart_de_plus_de_deux_secondes_recale_tout_de_suite() {
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 60, 2)) == ActionPeriodique::Rien);
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 60, -2)) == ActionPeriodique::Rien);
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 60, 3)) == ActionPeriodique::EcrireRtc);
    VERIFIER(periodique(en_marche(SourceHeure::Reseau, 60, -3)) == ActionPeriodique::EcrireRtc);
    EntreePeriodique illisible = en_marche(SourceHeure::Reseau, 60, 500);
    illisible.rtc_lue = false; // on ne juge pas d'un ecart sur une lecture ratee
    VERIFIER(periodique(illisible) == ActionPeriodique::Rien);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(bcd_aller_retour)                                                                                                \
    X(bcd_invalide_refuse)                                                                                             \
    X(encodage_egal_aux_trames_de_python)                                                                              \
    X(decodage_egal_aux_instants_de_python)                                                                            \
    X(aller_retour_sur_des_instants_sensibles)                                                                         \
    X(instant_hors_plage_non_encode)                                                                                   \
    X(trame_incoherente_refusee)                                                                                       \
    X(bits_de_commande_masques)                                                                                        \
    X(plausibilite)                                                                                                    \
    X(heure_affichable_avant_sntp_si_le_systeme_est_plausible)                                                         \
    X(demarrage_reprend_l_horloge_quand_le_systeme_est_vide)                                                           \
    X(demarrage_n_ecrase_jamais_une_heure_plausible)                                                                   \
    X(demarrage_ignore_une_horloge_douteuse)                                                                           \
    X(source_reseau_quand_le_systeme_devient_plausible_sans_nous)                                                      \
    X(source_reseau_apres_un_saut_meme_si_nous_avions_repris_la_pile)                                                  \
    X(source_inchangee_sans_evenement)                                                                                 \
    X(source_aucune_si_le_systeme_n_est_plus_plausible)                                                                \
    X(on_n_ecrit_que_depuis_le_reseau)                                                                                 \
    X(ecriture_au_plus_toutes_les_six_heures)                                                                          \
    X(un_ecart_de_plus_de_deux_secondes_recale_tout_de_suite)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
