// Tests du noyau pur « surf » : les adresses des deux requêtes (surf_requete.hpp). Chaque test est évalué à la compilation
// (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ecran/surf_donnees.hpp"
#include "ecran/surf_requete.hpp"
#include "mini.hpp"

using namespace ecran::surf;
using sv = std::string_view;

namespace {

// Le tampon est rempli de « x » avant l'appel : un zéro final oublié se voit.
struct Tampon {
    char d[600] = {};
    std::size_t n = 0;
    constexpr Tampon() noexcept {
        for (char &c : d)
            c = 'x';
    }
    constexpr sv vue() const noexcept { return sv(d, n); }
};

constexpr Tampon marine(std::int32_t lat, std::int32_t lon) noexcept {
    Tampon t;
    t.n = construire_requete_marine(lat, lon, t.d, sizeof(t.d));
    return t;
}
constexpr Tampon vent(std::int32_t lat, std::int32_t lon) noexcept {
    Tampon t;
    t.n = construire_requete_vent(lat, lon, t.d, sizeof(t.d));
    return t;
}
constexpr bool contient(sv texte, sv motif) noexcept { return texte.find(motif) != sv::npos; }

// ---- l'adresse exacte ----------------------------------------------------------------------------------------------------

constexpr int adresse_exacte_de_la_requete_mer() {
    constexpr sv ATTENDU =
        "https://marine-api.open-meteo.com/v1/marine?latitude=43.66760&longitude=-1.44500"
        "&hourly=wave_height,wave_period,wave_direction,swell_wave_height,swell_wave_period,swell_wave_direction,"
        "wind_wave_height,wind_wave_period,wind_wave_direction,sea_surface_temperature,sea_level_height_msl"
        "&length_unit=metric&cell_selection=sea&past_hours=6&forecast_hours=48&timezone=auto&timeformat=unixtime";
    const Tampon t = marine(4366760, -144500);
    VERIFIER(t.n == ATTENDU.size());
    VERIFIER(t.vue() == ATTENDU);
    VERIFIER(t.d[t.n] == '\0');
    return 0;
}

constexpr int adresse_exacte_de_la_requete_vent() {
    constexpr sv ATTENDU = "https://api.open-meteo.com/v1/forecast?latitude=43.66760&longitude=-1.44500"
                           "&hourly=wind_speed_10m,wind_direction_10m,wind_gusts_10m&wind_speed_unit=kmh"
                           "&past_hours=6&forecast_hours=48&timezone=auto&timeformat=unixtime";
    const Tampon t = vent(4366760, -144500);
    VERIFIER(t.n == ATTENDU.size());
    VERIFIER(t.vue() == ATTENDU);
    VERIFIER(t.d[t.n] == '\0');
    return 0;
}

// ---- les degrés ----------------------------------------------------------------------------------------------------------

constexpr int les_degres_ont_cinq_decimales_et_un_signe() {
    // Hémisphère sud et ouest, zéro, très petite valeur négative (-0,00005 ne devient pas « 0.00005 » ni « -0.0000 »).
    VERIFIER(contient(marine(-3386785, 15120930).vue(), "latitude=-33.86785&longitude=151.20930&"));
    VERIFIER(contient(marine(0, 1).vue(), "latitude=0.00000&longitude=0.00001&"));
    VERIFIER(contient(marine(-5, -5).vue(), "latitude=-0.00005&longitude=-0.00005&"));
    VERIFIER(contient(vent(-3386785, 15120930).vue(), "latitude=-33.86785&longitude=151.20930&"));
    VERIFIER(contient(vent(5, -5).vue(), "latitude=0.00005&longitude=-0.00005&"));
    return 0;
}

constexpr int les_bornes_exactes_sont_acceptees_et_tiennent_dans_la_taille_maximale() {
    const Tampon a = marine(-9000000, -18000000);
    VERIFIER(a.n > 0 && contient(a.vue(), "latitude=-90.00000&longitude=-180.00000&"));
    VERIFIER(a.n + 1 ==
             TAILLE_REQUETE_MARINE_MAX); // le pire cas remplit EXACTEMENT la taille annoncée, zéro final compris
    const Tampon b = vent(-9000000, -18000000);
    VERIFIER(b.n > 0 && b.n + 1 == TAILLE_REQUETE_VENT_MAX);
    const Tampon c = marine(9000000, 18000000);
    VERIFIER(c.n > 0 && contient(c.vue(), "latitude=90.00000&longitude=180.00000&"));
    VERIFIER(c.n + 1 <
             TAILLE_REQUETE_MARINE_MAX); // « 90.00000 » et « 180.00000 » : un caractère de moins que les négatifs
    return 0;
}

constexpr int une_coordonnee_hors_du_globe_est_refusee() {
    VERIFIER(marine(9000001, 0).n == 0 && marine(-9000001, 0).n == 0);
    VERIFIER(marine(0, 18000001).n == 0 && marine(0, -18000001).n == 0);
    VERIFIER(vent(9000001, 100).n == 0 && vent(-9000001, 100).n == 0);
    VERIFIER(vent(100, 18000001).n == 0 && vent(100, -18000001).n == 0);
    VERIFIER(marine(INT32_MAX, INT32_MIN).n == 0);
    return 0;
}

// ---- le tampon -----------------------------------------------------------------------------------------------------------

constexpr int un_tampon_trop_petit_est_refuse_sans_deborder() {
    const std::size_t pile = marine(4366760, -144500).n + 1; // la longueur + le zéro final
    char t[600] = {};
    VERIFIER(construire_requete_marine(4366760, -144500, t, pile) == pile - 1);
    VERIFIER(construire_requete_marine(4366760, -144500, t, pile - 1) == 0);
    VERIFIER(construire_requete_marine(4366760, -144500, t, 1) == 0);
    VERIFIER(construire_requete_marine(4366760, -144500, t, 0) == 0);
    VERIFIER(construire_requete_marine(4366760, -144500, nullptr, 600) == 0);
    const std::size_t pile_v = vent(4366760, -144500).n + 1;
    VERIFIER(construire_requete_vent(4366760, -144500, t, pile_v) == pile_v - 1);
    VERIFIER(construire_requete_vent(4366760, -144500, t, pile_v - 1) == 0);
    VERIFIER(construire_requete_vent(4366760, -144500, t, 0) == 0);
    VERIFIER(construire_requete_vent(4366760, -144500, nullptr, 600) == 0);
    return 0;
}

constexpr int le_tampon_maximal_annonce_suffit_et_reste_sous_la_ligne_de_requete_d_esp_http_client() {
    VERIFIER(TAILLE_REQUETE_MARINE_MAX < 512 && TAILLE_REQUETE_VENT_MAX < 512);
    VERIFIER(TAILLE_REQUETE_MARINE_MAX > TAILLE_REQUETE_VENT_MAX); // la mer demande plus de variables
    char t[TAILLE_REQUETE_MARINE_MAX] = {};
    VERIFIER(construire_requete_marine(-9000000, -18000000, t, TAILLE_REQUETE_MARINE_MAX) ==
             TAILLE_REQUETE_MARINE_MAX - 1);
    char v[TAILLE_REQUETE_VENT_MAX] = {};
    VERIFIER(construire_requete_vent(-9000000, -18000000, v, TAILLE_REQUETE_VENT_MAX) == TAILLE_REQUETE_VENT_MAX - 1);
    return 0;
}

// ---- le contrat avec l'analyseur -----------------------------------------------------------------------------------------

constexpr int les_heures_demandees_sont_celles_que_l_analyseur_attend() {
    VERIFIER(PASSE_HEURES == 6 && FUTUR_HEURES == 48);
    VERIFIER(HEURES_ATTENDUES == PASSE_HEURES + FUTUR_HEURES);
    VERIFIER(HEURES_MAX > static_cast<std::size_t>(HEURES_ATTENDUES)); // une marge, pas une troncature
    VERIFIER(HEURES_MAX == 60);
    for (const Tampon &t : {marine(100, 100), vent(100, 100)}) {
        VERIFIER(contient(t.vue(), "&past_hours=6&forecast_hours=48&")); // les deux nombres de PASSE_/FUTUR_HEURES
        VERIFIER(contient(t.vue(), "&timezone=auto&timeformat=unixtime"));
    }
    return 0;
}

constexpr int la_requete_mer_demande_les_onze_variables_que_l_analyseur_lit() {
    const Tampon t = marine(100, 100);
    for (const sv nom : {"wave_height", "wave_period", "wave_direction", "swell_wave_height", "swell_wave_period",
                         "swell_wave_direction", "wind_wave_height", "wind_wave_period", "wind_wave_direction",
                         "sea_surface_temperature", "sea_level_height_msl"}) {
        VERIFIER(contient(t.vue(), nom));
    }
    // Les unités sont fixées : l'analyseur refuse une autre unité, la requête ne doit donc jamais en demander une.
    VERIFIER(contient(t.vue(), "&length_unit=metric&"));
    VERIFIER(contient(t.vue(), "&cell_selection=sea&"));
    VERIFIER(!contient(t.vue(), "imperial") && !contient(t.vue(), "fahrenheit"));
    return 0;
}

constexpr int la_requete_vent_demande_les_trois_variables_en_kmh() {
    const Tampon t = vent(100, 100);
    for (const sv nom : {"wind_speed_10m", "wind_direction_10m", "wind_gusts_10m"})
        VERIFIER(contient(t.vue(), nom));
    VERIFIER(contient(t.vue(), "&wind_speed_unit=kmh&"));
    VERIFIER(!contient(t.vue(), "mph") && !contient(t.vue(), "&models="));
    return 0;
}

constexpr int les_deux_requetes_vont_vers_les_bons_serveurs_en_https() {
    VERIFIER(marine(100, 100).vue().substr(0, 40) == "https://marine-api.open-meteo.com/v1/mar");
    VERIFIER(vent(100, 100).vue().substr(0, 39) == "https://api.open-meteo.com/v1/forecast?");
    VERIFIER(!contient(marine(100, 100).vue(), "apikey") && !contient(vent(100, 100).vue(), "apikey")); // aucune clé
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(adresse_exacte_de_la_requete_mer)                                                                                \
    X(adresse_exacte_de_la_requete_vent)                                                                               \
    X(les_degres_ont_cinq_decimales_et_un_signe)                                                                       \
    X(les_bornes_exactes_sont_acceptees_et_tiennent_dans_la_taille_maximale)                                           \
    X(une_coordonnee_hors_du_globe_est_refusee)                                                                        \
    X(un_tampon_trop_petit_est_refuse_sans_deborder)                                                                   \
    X(le_tampon_maximal_annonce_suffit_et_reste_sous_la_ligne_de_requete_d_esp_http_client)                            \
    X(les_heures_demandees_sont_celles_que_l_analyseur_attend)                                                         \
    X(la_requete_mer_demande_les_onze_variables_que_l_analyseur_lit)                                                   \
    X(la_requete_vent_demande_les_trois_variables_en_kmh)                                                              \
    X(les_deux_requetes_vont_vers_les_bons_serveurs_en_https)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
