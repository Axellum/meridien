// Tests du noyau pur : la requête de la mer d'un spot et le rythme des requêtes (mer_requete.hpp). Aucune trame : ce fichier ne lit rien
// du réseau, il vérifie l'adresse qui part, ce qui la fait refuser, et les attentes entre deux requêtes.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_requete.hpp"
#include "mini.hpp"

using namespace ecran::mer;
namespace vigie = ecran::vigie;

namespace {

// Un hôte d'exemple : l'adresse de l'instance publique n'est jamais recopiée dans les tests.
constexpr std::string_view HOTE = "https://vigie.exemple.fr";
constexpr std::string_view SPOT = "soorts-hossegor-plage-sud-point-d-or";

struct Texte {
    char t[TAILLE_REQUETE_MAX + 8] = {};
    std::size_t n = 0;
    constexpr std::string_view vue() const { return std::string_view(t, n); }
};

constexpr Texte requete(std::string_view adresse, std::string_view spot, int vmin, int vmax) {
    Texte r;
    r.n = construire_requete(adresse, spot, vmin, vmax, r.t, sizeof r.t);
    return r;
}

constexpr bool contient(std::string_view t, std::string_view aiguille) {
    return t.find(aiguille) != std::string_view::npos;
}

// ---- la requête ----------------------------------------------------------------------------------------------------------------

constexpr int la_requete_par_defaut_n_a_aucun_parametre_de_vent() {
    const Texte r = requete(HOTE, SPOT, 12, 25);
    VERIFIER(
        r.vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=soorts-hossegor-plage-sud-point-d-or");
    VERIFIER(r.t[r.n] == '\0');
    VERIFIER(!contient(r.vue(), "wind"));
    return 0;
}

constexpr int la_position_de_l_appareil_ne_part_pas() {
    const Texte r = requete(HOTE, SPOT, 12, 25);
    VERIFIER(!contient(r.vue(), "lat") && !contient(r.vue(), "lon"));
    const Texte v = requete(HOTE, SPOT, 8, 30);
    VERIFIER(!contient(v.vue(), "lat=") && !contient(v.vue(), "lon="));
    return 0;
}

constexpr int une_autre_plage_de_vent_part_en_noeuds_entiers() {
    VERIFIER(
        requete(HOTE, "la-torche", 8, 30).vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=la-torche&wind_min_kn=8&wind_max_kn=30");
    VERIFIER(
        requete(HOTE, "la-torche", 9, 9 + 1).vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=la-torche&wind_min_kn=9&wind_max_kn=10");
    VERIFIER(
        requete(HOTE, "la-torche", 1, 60).vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=la-torche&wind_min_kn=1&wind_max_kn=60");
    VERIFIER(
        requete(HOTE, "la-torche", 59, 60).vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=la-torche&wind_min_kn=59&wind_max_kn=60");
    return 0;
}

constexpr int une_seule_borne_differente_envoie_quand_meme_les_deux() {
    VERIFIER(
        requete(HOTE, "la-torche", 12, 26).vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=la-torche&wind_min_kn=12&wind_max_kn=26");
    VERIFIER(
        requete(HOTE, "la-torche", 13, 25).vue() ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=la-torche&wind_min_kn=13&wind_max_kn=25");
    return 0;
}

constexpr int les_valeurs_par_defaut_sont_celles_de_vigie() {
    VERIFIER(VENT_MIN_DEFAUT_NOEUDS == 12 && VENT_MAX_DEFAUT_NOEUDS == 25 && VENT_PLAFOND_NOEUDS == 60);
    VERIFIER(plage_de_vent_valide(VENT_MIN_DEFAUT_NOEUDS, VENT_MAX_DEFAUT_NOEUDS));
    return 0;
}

constexpr int la_plage_de_vent_suit_les_bornes_de_l_api() {
    VERIFIER(plage_de_vent_valide(1, 2));
    VERIFIER(plage_de_vent_valide(59, 60));
    VERIFIER(plage_de_vent_valide(1, 60));
    VERIFIER(!plage_de_vent_valide(0, 25)); // 0 < min
    VERIFIER(!plage_de_vent_valide(-5, 25));
    VERIFIER(!plage_de_vent_valide(12, 12)); // min < max, strictement
    VERIFIER(!plage_de_vent_valide(25, 12));
    VERIFIER(!plage_de_vent_valide(12, 61)); // max <= 60
    VERIFIER(!plage_de_vent_valide(61, 62));
    VERIFIER(!plage_de_vent_valide(60, 60));
    return 0;
}

constexpr int un_identifiant_de_spot_refuse_donne_zero() {
    const std::string_view mauvais[] = {"",       "Hossegor", "hossegor sud", "hossegor/sud", "a?b=c",
                                        "a&b",    "a.b",      "hossegor\n",   "ho\xC3\xA9",   "a%20b",
                                        "../etc", "a\\b",     "a#b"};
    for (const std::string_view s : mauvais)
        VERIFIER(requete(HOTE, s, 12, 25).n == 0);
    // 64 octets passent, 65 non.
    char long64[65] = {};
    for (std::size_t i = 0; i < 64; ++i)
        long64[i] = 'a';
    VERIFIER(requete(HOTE, std::string_view(long64, 64), 12, 25).n != 0);
    char long65[66] = {};
    for (std::size_t i = 0; i < 65; ++i)
        long65[i] = 'a';
    VERIFIER(requete(HOTE, std::string_view(long65, 65), 12, 25).n == 0);
    return 0;
}

constexpr int une_plage_de_vent_refusee_donne_zero() {
    VERIFIER(requete(HOTE, SPOT, 0, 25).n == 0);
    VERIFIER(requete(HOTE, SPOT, 12, 12).n == 0);
    VERIFIER(requete(HOTE, SPOT, 30, 12).n == 0);
    VERIFIER(requete(HOTE, SPOT, 12, 61).n == 0);
    VERIFIER(requete(HOTE, SPOT, -1, 25).n == 0);
    return 0;
}

constexpr int une_adresse_vide_ou_trop_longue_donne_zero() {
    VERIFIER(requete("", SPOT, 12, 25).n == 0);
    char adresse81[82] = {};
    for (std::size_t i = 0; i < 81; ++i)
        adresse81[i] = 'a';
    VERIFIER(requete(std::string_view(adresse81, 81), SPOT, 12, 25).n == 0);
    VERIFIER(requete(std::string_view(adresse81, 80), SPOT, 12, 25).n != 0);
    return 0;
}

constexpr int un_tampon_absent_ou_trop_petit_donne_zero() {
    char t[TAILLE_REQUETE_MAX] = {};
    VERIFIER(construire_requete(HOTE, SPOT, 12, 25, nullptr, sizeof t) == 0);
    VERIFIER(construire_requete(HOTE, SPOT, 12, 25, t, 0) == 0);
    const std::size_t longueur = requete(HOTE, SPOT, 12, 25).n;
    VERIFIER(longueur != 0);
    VERIFIER(construire_requete(HOTE, SPOT, 12, 25, t, longueur) == 0);            // sans la place du zéro final
    VERIFIER(construire_requete(HOTE, SPOT, 12, 25, t, longueur + 1) == longueur); // juste la place
    VERIFIER(construire_requete(HOTE, SPOT, 12, 25, t, 5) == 0);
    return 0;
}

constexpr int le_pire_cas_tient_exactement_dans_le_tampon_maximal() {
    char adresse[vigie::ADRESSE_MAX + 1] = {};
    for (std::size_t i = 0; i < vigie::ADRESSE_MAX; ++i)
        adresse[i] = 'a';
    // 64 octets : le plus long identifiant que spot_valide accepte (et pas un de plus), écrit ici en chiffres et non avec SPOT_ID_MAX
    // pour que le test voie une constante qui bouge.
    VERIFIER(SPOT_ID_MAX == 64);
    char spot[65] = {};
    for (std::size_t i = 0; i < 64; ++i)
        spot[i] = 'z';
    VERIFIER(spot_valide(std::string_view(spot, 64)));
    char t[TAILLE_REQUETE_MAX] = {};
    const std::size_t n = construire_requete(std::string_view(adresse, vigie::ADRESSE_MAX), std::string_view(spot, 64),
                                             59, 60, t, sizeof t);
    VERIFIER(n == TAILLE_REQUETE_MAX - 1);
    VERIFIER(t[n] == '\0');
    char juste_trop_petit[TAILLE_REQUETE_MAX - 1] = {};
    VERIFIER(construire_requete(std::string_view(adresse, vigie::ADRESSE_MAX), std::string_view(spot, 64), 59, 60,
                                juste_trop_petit, sizeof juste_trop_petit) == 0);
    return 0;
}

// ---- le rythme -----------------------------------------------------------------------------------------------------------------

constexpr int la_periode_est_de_dix_minutes_et_respecte_la_politesse_des_alertes() {
    VERIFIER(PERIODE_S == 600);
    VERIFIER(PERIODE_S >= vigie::periode_s(vigie::PERIODE_MIN_MIN));
    return 0;
}

constexpr int le_premier_essai_part_entre_120_et_180_secondes() {
    VERIFIER(delai_premier_essai_s(0) == 120);
    VERIFIER(delai_premier_essai_s(255) == 180);
    VERIFIER(delai_premier_essai_s(128) == 120 + 60 * 128 / 255);
    // Toujours après le premier essai des alertes (60 à 120 s) : on ne tape pas Vigie en même temps.
    VERIFIER(delai_premier_essai_s(0) >= vigie::delai_premier_essai_s(255));
    for (int a = 0; a <= 255; ++a) {
        const std::int64_t d = delai_premier_essai_s(static_cast<std::uint8_t>(a));
        VERIFIER(d >= 120 && d <= 180);
    }
    return 0;
}

constexpr int apres_une_reponse_la_prochaine_requete_est_dans_dix_minutes() {
    VERIFIER(delai_avant_essai_s(0, false, 0) == 600);
    VERIFIER(delai_avant_essai_s(0, false, 255) == 600 + 60); // un dixième de décalage au plus
    return 0;
}

constexpr int apres_un_echec_l_attente_double_jusqu_a_la_periode() {
    VERIFIER(delai_avant_essai_s(1, false, 0) == 60);
    VERIFIER(delai_avant_essai_s(2, false, 0) == 120);
    VERIFIER(delai_avant_essai_s(3, false, 0) == 240);
    VERIFIER(delai_avant_essai_s(4, false, 0) == 480);
    VERIFIER(delai_avant_essai_s(5, false, 0) == 600); // plafonnée à la période
    VERIFIER(delai_avant_essai_s(50, false, 0) == 600);
    return 0;
}

constexpr int apres_un_429_ou_un_503_jamais_moins_de_trente_minutes() {
    VERIFIER(delai_avant_essai_s(1, true, 0) == 30 * 60);
    VERIFIER(delai_avant_essai_s(0, true, 0) == 30 * 60);
    VERIFIER(delai_avant_essai_s(9, true, 255) == 30 * 60 + 3 * 60);
    return 0;
}

constexpr int un_changement_de_spot_attend_un_court_delai_et_une_minute_apres_le_dernier_essai() {
    // Dernier essai récent : on attend soixante secondes après lui.
    VERIFIER(prochain_essai_apres_changement_s(1000, 1010) == 1060);
    // Dernier essai ancien : seulement l'anti-rebond (3 s).
    VERIFIER(prochain_essai_apres_changement_s(0, 1000) == 1003);
    // Juste à la limite des deux : la plus tardive des deux gagne.
    VERIFIER(prochain_essai_apres_changement_s(1000, 1057) == 1060);
    VERIFIER(prochain_essai_apres_changement_s(1000, 1058) == 1061);
    VERIFIER(ANTI_REBOND_S == 3);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(la_requete_par_defaut_n_a_aucun_parametre_de_vent)                                                               \
    X(la_position_de_l_appareil_ne_part_pas)                                                                           \
    X(une_autre_plage_de_vent_part_en_noeuds_entiers)                                                                  \
    X(une_seule_borne_differente_envoie_quand_meme_les_deux)                                                           \
    X(les_valeurs_par_defaut_sont_celles_de_vigie)                                                                     \
    X(la_plage_de_vent_suit_les_bornes_de_l_api)                                                                       \
    X(un_identifiant_de_spot_refuse_donne_zero)                                                                        \
    X(une_plage_de_vent_refusee_donne_zero)                                                                            \
    X(une_adresse_vide_ou_trop_longue_donne_zero)                                                                      \
    X(un_tampon_absent_ou_trop_petit_donne_zero)                                                                       \
    X(le_pire_cas_tient_exactement_dans_le_tampon_maximal)                                                             \
    X(la_periode_est_de_dix_minutes_et_respecte_la_politesse_des_alertes)                                              \
    X(le_premier_essai_part_entre_120_et_180_secondes)                                                                 \
    X(apres_une_reponse_la_prochaine_requete_est_dans_dix_minutes)                                                     \
    X(apres_un_echec_l_attente_double_jusqu_a_la_periode)                                                              \
    X(apres_un_429_ou_un_503_jamais_moins_de_trente_minutes)                                                           \
    X(un_changement_de_spot_attend_un_court_delai_et_une_minute_apres_le_dernier_essai)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
