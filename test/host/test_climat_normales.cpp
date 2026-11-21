// Tests du noyau pur « est-ce normal ? » (climat_normales.hpp, ADR-0029) : lecture de la réponse de Vigie (trois réponses RÉELLES du
// 21/11/2026, test/host/donnees/climat_*.inc, plus des cas limites écrits à la main) et jugement du maximum prévu contre la normale.
#include <cstddef>
#include <optional>
#include <string_view>

#include "ecran/climat_normales.hpp"
#include "mini.hpp"

using namespace ecran::climat;
using sv = std::string_view;

namespace {

constexpr sv LORIENT =
#include "donnees/climat_lorient.inc"
    ;
constexpr sv REUNION =
#include "donnees/climat_reunion.inc"
    ;
constexpr sv LONDRES =
#include "donnees/climat_londres.inc"
    ;

constexpr bool texte_egal(const char *a, sv b) {
    std::size_t i = 0;
    for (; i < b.size(); ++i)
        if (a[i] != b[i])
            return false;
    return a[i] == '\0';
}

// ---- lecture --------------------------------------------------------------------------------------------------------------

constexpr int reponse_reelle_lorient() {
    Normales n;
    VERIFIER(analyser(LORIENT, n) == Statut::Ok);
    VERIFIER(n.mois == 11);
    VERIFIER(n.tmax_normale_dix && *n.tmax_normale_dix == 130);
    VERIFIER(n.tmax_record_dix && *n.tmax_record_dix == 196);
    VERIFIER(n.tmin_record_dix && *n.tmin_record_dix == -50);
    VERIFIER(n.station_active);
    VERIFIER(texte_egal(n.edition, "2026-11-06"));
    return 0;
}

constexpr int reponse_reelle_outre_mer() {
    Normales n;
    VERIFIER(analyser(REUNION, n) == Statut::Ok);
    VERIFIER(*n.tmax_normale_dix == 239 && *n.tmax_record_dix == 298 && *n.tmin_record_dix == 35);
    return 0;
}

constexpr int reponse_reelle_hors_couverture() {
    Normales n;
    VERIFIER(analyser(LONDRES, n) == Statut::HorsCouverture);
    VERIFIER(n.mois == 11);
    VERIFIER(!n.tmax_normale_dix && !n.tmax_record_dix && !n.tmin_record_dix && !n.station_active);
    return 0;
}

constexpr int illisible_incoherent() {
    Normales n;
    VERIFIER(analyser("", n) == Statut::Illisible);
    VERIFIER(analyser("{\"status\":\"available\",\"month\":11", n) == Statut::Illisible); // texte coupé
    VERIFIER(analyser("[]", n) == Statut::Incoherent);
    VERIFIER(analyser("{\"month\":11}", n) == Statut::Incoherent);             // pas de status
    VERIFIER(analyser("{\"status\":\"available\"}", n) == Statut::Incoherent); // pas de mois
    VERIFIER(analyser("{\"status\":\"available\",\"month\":0}", n) == Statut::Incoherent);
    VERIFIER(analyser("{\"status\":\"available\",\"month\":13}", n) == Statut::Incoherent);
    VERIFIER(analyser("{\"status\":\"autre\",\"month\":11}", n) == Statut::Incoherent);
    VERIFIER(n.mois == 0); // rien n'est gardé d'une réponse qu'on ne comprend pas
    return 0;
}

constexpr int valeur_absente_ou_aberrante_est_inconnue() {
    Normales n;
    VERIFIER(analyser("{\"status\":\"available\",\"month\":3,\"normals\":{\"tmax_mean_c\":{\"value\":null}},"
                      "\"records\":{\"tmax_c\":{\"value\":61.0},\"tmin_c\":{\"value\":-60.1}}}",
                      n) == Statut::Ok);
    VERIFIER(n.mois == 3);
    VERIFIER(!n.tmax_normale_dix); // null : inconnue, jamais 0
    VERIFIER(!n.tmax_record_dix);  // 61 °C : hors de -60..60
    VERIFIER(!n.tmin_record_dix);
    VERIFIER(!n.station_active && n.edition[0] == '\0');
    VERIFIER(analyser("{\"status\":\"available\",\"month\":3,\"normals\":{\"tmax_mean_c\":{\"value\":60.0}},"
                      "\"records\":{\"tmin_c\":{\"value\":-60.0}}}",
                      n) == Statut::Ok);
    VERIFIER(*n.tmax_normale_dix == 600 && *n.tmin_record_dix == -600); // les bornes sont comprises
    return 0;
}

constexpr int arrondi_au_dixieme() {
    Normales n;
    VERIFIER(analyser("{\"status\":\"available\",\"month\":1,\"normals\":{\"tmax_mean_c\":{\"value\":12.34}}}", n) ==
             Statut::Ok);
    VERIFIER(*n.tmax_normale_dix == 123);
    VERIFIER(analyser("{\"status\":\"available\",\"month\":1,\"normals\":{\"tmax_mean_c\":{\"value\":-0.04}}}", n) ==
             Statut::Ok);
    VERIFIER(*n.tmax_normale_dix == 0);
    return 0;
}

constexpr int edition_mal_formee_est_ignoree() {
    Normales n;
    VERIFIER(
        analyser("{\"status\":\"available\",\"month\":1,\"station\":{\"sheet_edited\":\"6 novembre\",\"active\":true}}",
                 n) == Statut::Ok);
    VERIFIER(n.edition[0] == '\0' && n.station_active);
    VERIFIER(analyser("{\"status\":\"available\",\"month\":1,\"station\":{\"sheet_edited\":\"2026-11-061\"}}", n) ==
             Statut::Ok);
    VERIFIER(n.edition[0] == '\0');
    VERIFIER(analyser("{\"status\":\"available\",\"month\":1,\"station\":{\"sheet_edited\":\"2026/11/06\"}}", n) ==
             Statut::Ok);
    VERIFIER(n.edition[0] == '\0');
    return 0;
}

constexpr int station_arretee_n_est_pas_active() {
    Normales n;
    VERIFIER(analyser("{\"status\":\"available\",\"month\":1,\"station\":{\"active\":false}}", n) == Statut::Ok);
    VERIFIER(!n.station_active);
    VERIFIER(analyser("{\"status\":\"available\",\"month\":1,\"station\":{\"active\":\"oui\"}}", n) == Statut::Ok);
    VERIFIER(!n.station_active); // pas un booléen : pas actif
    return 0;
}

// ---- jugement -------------------------------------------------------------------------------------------------------------

// Lorient en novembre : normale du maximum 13,0 ; records 19,6 et -5,0 ; station active ; mois 11.
constexpr Normales lorient() {
    Normales n;
    n.mois = 11;
    n.tmax_normale_dix = 130;
    n.tmax_record_dix = 196;
    n.tmin_record_dix = -50;
    n.station_active = true;
    return n;
}

// La même, sans la normale du maximum ou sans le record de chaleur.
constexpr Normales lorient_sans(bool sans_normale, bool sans_record) {
    Normales n;
    n.mois = 11;
    if (!sans_normale)
        n.tmax_normale_dix = 130;
    if (!sans_record)
        n.tmax_record_dix = 196;
    n.tmin_record_dix = -50;
    n.station_active = true;
    return n;
}

constexpr int au_dessus_en_dessous_normal() {
    const Normales n = lorient();
    Verdict v = juger(n, 140, 80, 11, 0);
    VERIFIER(v.etat == Etat::AuDessus && v.ecart_dix == 10); // 1,0° : le seuil est atteint, ce n'est plus « normal »
    v = juger(n, 139, 80, 11, 0);
    VERIFIER(v.etat == Etat::Normal && v.ecart_dix == 9);
    v = juger(n, 121, 80, 11, 0);
    VERIFIER(v.etat == Etat::Normal && v.ecart_dix == -9);
    v = juger(n, 120, 80, 11, 0);
    VERIFIER(v.etat == Etat::EnDessous && v.ecart_dix == -10);
    v = juger(n, 130, 80, 11, 0);
    VERIFIER(v.etat == Etat::Normal && v.ecart_dix == 0);
    v = juger(n, 88, 20, 11, 0);
    VERIFIER(v.etat == Etat::EnDessous && v.ecart_dix == -42);
    return 0;
}

constexpr int records() {
    Normales n = lorient();
    VERIFIER(juger(n, 197, 80, 11, 0).etat == Etat::RecordChaud);
    VERIFIER(juger(n, 196, 80, 11, 0).etat == Etat::AuDessus); // égaler n'est pas battre
    VERIFIER(juger(n, 100, -51, 11, 0).etat == Etat::RecordFroid);
    VERIFIER(juger(n, 100, -50, 11, 0).etat == Etat::EnDessous);
    VERIFIER(juger(n, 200, -60, 11, 0).etat == Etat::RecordChaud); // chaleur d'abord
    n.station_active = false;                                      // station arrêtée : ses records ne bougent plus
    VERIFIER(juger(n, 197, 80, 11, 0).etat == Etat::AuDessus);
    VERIFIER(juger(n, 100, -51, 11, 0).etat == Etat::EnDessous);
    return 0;
}

constexpr int valeurs_inconnues() {
    const Normales n = lorient();
    VERIFIER(juger(n, std::nullopt, 80, 11, 0).etat == Etat::Aucun);
    VERIFIER(juger(n, std::nullopt, -60, 11, 0).etat == Etat::RecordFroid); // le minimum seul peut battre un record
    const Normales sans_normale = lorient_sans(true, false);
    VERIFIER(juger(sans_normale, 140, 80, 11, 0).etat == Etat::Aucun);
    VERIFIER(juger(sans_normale, 197, 80, 11, 0).etat == Etat::RecordChaud); // le record n'a pas besoin de la normale
    const Normales sans_record = lorient_sans(false, true);
    VERIFIER(juger(sans_record, 300, 80, 11, 0).etat == Etat::AuDessus);
    return 0;
}

constexpr int autre_mois_ou_trop_vieux() {
    const Normales n = lorient();
    VERIFIER(juger(n, 140, 80, 12, 0).etat == Etat::Aucun); // le jour de la bascule : la réponse décrit le mois d'avant
    VERIFIER(juger(n, 140, 80, 0, 0).etat == Etat::Aucun);  // mois local inconnu
    VERIFIER(juger(n, 140, 80, 11, 25 * 3600).etat == Etat::AuDessus); // une requête ratée hier : la pastille reste
    VERIFIER(juger(n, 140, 80, 11, 47 * 3600).etat == Etat::AuDessus);
    VERIFIER(juger(n, 140, 80, 11, 49 * 3600).etat == Etat::Aucun);
    VERIFIER(juger(n, 140, 80, 11, VALIDITE_S).etat == Etat::AuDessus);
    VERIFIER(juger(n, 140, 80, 11, VALIDITE_S + 1).etat == Etat::Aucun);
    VERIFIER(juger(n, 140, 80, 11, -1).etat == Etat::Aucun); // l'horloge a reculé
    const Normales vide;
    VERIFIER(juger(vide, 140, 80, 0, 0).etat == Etat::Aucun); // rien de lu : jamais de verdict
    VERIFIER(pour_ce_mois(n, 11) && !pour_ce_mois(n, 10) && !pour_ce_mois(vide, 0));
    return 0;
}

// ---- écriture de l'écart --------------------------------------------------------------------------------------------------

constexpr int ecart_ecrit(int dix, char sep, sv attendu) {
    char s[16] = {};
    const std::size_t n = ecrire_ecart(dix, sep, s, sizeof s);
    return n == attendu.size() && texte_egal(s, attendu) ? 0 : 1;
}

constexpr int ecriture_des_ecarts() {
    VERIFIER(ecart_ecrit(10, ',', "+1,0") == 0);
    VERIFIER(ecart_ecrit(42, '.', "+4.2") == 0);
    VERIFIER(ecart_ecrit(-42, ',',
                         "\xE2\x88\x92"
                         "4,2") == 0); // le vrai signe moins U+2212
    VERIFIER(ecart_ecrit(0, ',', "+0,0") == 0);
    VERIFIER(ecart_ecrit(-5, ',',
                         "\xE2\x88\x92"
                         "0,5") == 0);
    VERIFIER(ecart_ecrit(123, ',', "+12,3") == 0);
    VERIFIER(ecart_ecrit(5000, ',', "+99,9") == 0); // plafonné : jamais de dépassement
    return 0;
}

constexpr int ecriture_tampon_trop_petit() {
    char s[16] = {'x'};
    VERIFIER(ecrire_ecart(10, ',', s, 0) == 0);
    VERIFIER(ecrire_ecart(10, ',', s, 4) == 0 && s[0] == '\0'); // « +1,0 » demande 5 octets
    VERIFIER(ecrire_ecart(10, ',', s, 5) == 4);
    VERIFIER(ecrire_ecart(-10, ',', s, 6) == 0 && s[0] == '\0'); // le moins fait 3 octets : 3 + 3 + 1
    VERIFIER(ecrire_ecart(-10, ',', s, 7) == 6);
    VERIFIER(ecrire_ecart(10, ',', nullptr, 16) == 0);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(reponse_reelle_lorient)                                                                                          \
    X(reponse_reelle_outre_mer)                                                                                        \
    X(reponse_reelle_hors_couverture)                                                                                  \
    X(illisible_incoherent)                                                                                            \
    X(valeur_absente_ou_aberrante_est_inconnue)                                                                        \
    X(arrondi_au_dixieme)                                                                                              \
    X(edition_mal_formee_est_ignoree)                                                                                  \
    X(station_arretee_n_est_pas_active)                                                                                \
    X(au_dessus_en_dessous_normal)                                                                                     \
    X(records)                                                                                                         \
    X(valeurs_inconnues)                                                                                               \
    X(autre_mois_ou_trop_vieux)                                                                                        \
    X(ecriture_des_ecarts)                                                                                             \
    X(ecriture_tampon_trop_petit)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
