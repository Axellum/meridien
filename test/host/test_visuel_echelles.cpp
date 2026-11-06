// Tests du noyau pur : échelles, catégories, alertes, couleurs et formats de l'écran principal.
#include <cstring>
#include <string_view>

#include "ecran/visuel_echelles.hpp"
#include "mini.hpp"

using namespace ecran::visuel;
using ecran::meteo::decrire;

namespace {

constexpr int CODES[] = {0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66,
                         67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 97, 99};

constexpr bool texte_egal(const char *a, const char *b) {
    std::size_t i = 0;
    for (; a[i] != '\0' && b[i] != '\0'; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return a[i] == b[i];
}

constexpr int chaque_code_a_une_ambiance_et_les_familles_sont_les_bonnes() {
    for (int code : CODES) {
        for (bool jour : {true, false}) {
            const Ambiance a = ambiance_pour_code(code, jour);
            VERIFIER(a != Ambiance::NOMBRE);
        }
    }
    VERIFIER(ambiance_pour_code(0, true) == Ambiance::CLAIR);
    VERIFIER(ambiance_pour_code(1, false) == Ambiance::CLAIR);
    VERIFIER(ambiance_pour_code(2, true) == Ambiance::NUAGEUX);
    VERIFIER(ambiance_pour_code(3, true) == Ambiance::COUVERT);
    VERIFIER(ambiance_pour_code(45, true) == Ambiance::BRUME);
    VERIFIER(ambiance_pour_code(63, true) == Ambiance::PLUIE);
    VERIFIER(ambiance_pour_code(80, false) == Ambiance::PLUIE);
    VERIFIER(ambiance_pour_code(73, true) == Ambiance::NEIGE);
    VERIFIER(ambiance_pour_code(85, true) == Ambiance::NEIGE);
    VERIFIER(ambiance_pour_code(95, true) == Ambiance::ORAGE);
    VERIFIER(ambiance_pour_code(99, false) == Ambiance::ORAGE);
    VERIFIER(ambiance_pour_code(12345, true) == Ambiance::COUVERT); // code inconnu : ciel neutre
    return 0;
}

constexpr int l_indice_uv_suit_l_echelle_de_l_oms() {
    VERIFIER(categorie_uv(0) == CategorieUv::FAIBLE);
    VERIFIER(categorie_uv(29) == CategorieUv::FAIBLE);
    VERIFIER(categorie_uv(30) == CategorieUv::MODERE);
    VERIFIER(categorie_uv(59) == CategorieUv::MODERE);
    VERIFIER(categorie_uv(60) == CategorieUv::ELEVE);
    VERIFIER(categorie_uv(79) == CategorieUv::ELEVE);
    VERIFIER(categorie_uv(80) == CategorieUv::TRES_ELEVE);
    VERIFIER(categorie_uv(109) == CategorieUv::TRES_ELEVE);
    VERIFIER(categorie_uv(110) == CategorieUv::EXTREME);
    VERIFIER(categorie_uv(INCONNU_DIX) == CategorieUv::INCONNU);
    VERIFIER(categorie_uv(-5) == CategorieUv::INCONNU);
    return 0;
}

constexpr int le_vent_suit_l_echelle_de_beaufort() {
    // bornes basses en km/h : 1, 6, 12, 20, 29, 39, 50, 62, 75, 89, 103, 118
    VERIFIER(beaufort(0) == 0);
    VERIFIER(beaufort(9) == 0); // 0,9 km/h : calme
    VERIFIER(beaufort(10) == 1);
    VERIFIER(beaufort(59) == 1);
    VERIFIER(beaufort(60) == 2);
    VERIFIER(beaufort(119) == 2);
    VERIFIER(beaufort(120) == 3);
    VERIFIER(beaufort(200) == 4);
    VERIFIER(beaufort(289) == 4);
    VERIFIER(beaufort(290) == 5);
    VERIFIER(beaufort(390) == 6);
    VERIFIER(beaufort(500) == 7);
    VERIFIER(beaufort(620) == 8);
    VERIFIER(beaufort(750) == 9);
    VERIFIER(beaufort(890) == 10);
    VERIFIER(beaufort(1030) == 11);
    VERIFIER(beaufort(1180) == 12);
    VERIFIER(beaufort(5000) == 12);
    VERIFIER(beaufort(INCONNU_DIX) == INCONNU);
    return 0;
}

constexpr int la_tendance_de_pression_a_un_seuil_de_un_hectopascal() {
    VERIFIER(tendance_pression(10) == TendancePression::HAUSSE);
    VERIFIER(tendance_pression(9) == TendancePression::STABLE);
    VERIFIER(tendance_pression(0) == TendancePression::STABLE);
    VERIFIER(tendance_pression(-9) == TendancePression::STABLE);
    VERIFIER(tendance_pression(-10) == TendancePression::BAISSE);
    VERIFIER(tendance_pression(INCONNU_DIX) == TendancePression::INCONNUE);
    return 0;
}

constexpr int le_vent_se_range_en_secteurs_de_45_degres() {
    VERIFIER(secteur_vent(0) == 0 && secteur_vent(360) == 0);
    VERIFIER(secteur_vent(22) == 0);
    VERIFIER(secteur_vent(23) == 1);
    VERIFIER(secteur_vent(45) == 1);
    VERIFIER(secteur_vent(90) == 2);
    VERIFIER(secteur_vent(135) == 3);
    VERIFIER(secteur_vent(180) == 4);
    VERIFIER(secteur_vent(225) == 5);
    VERIFIER(secteur_vent(270) == 6);
    VERIFIER(secteur_vent(315) == 7);
    VERIFIER(secteur_vent(337) == 7);
    VERIFIER(secteur_vent(338) == 0);
    VERIFIER(secteur_vent(-1) == INCONNU);
    VERIFIER(secteur_vent(361) == INCONNU);
    for (int d = 0; d <= 360; ++d) {
        VERIFIER(secteur_vent(d) >= 0 && secteur_vent(d) <= 7);
    }
    // la flèche pointe là où le vent va : un vent du nord (0°) souffle vers le sud (180°)
    VERIFIER(angle_fleche_vent(0) == 180);
    VERIFIER(angle_fleche_vent(90) == 270);
    VERIFIER(angle_fleche_vent(270) == 90);
    VERIFIER(angle_fleche_vent(180) == 0);
    VERIFIER(angle_fleche_vent(-1) == INCONNU);
    return 0;
}

constexpr int les_alertes_ont_une_priorite_et_ne_s_inventent_pas() {
    EntreesAlerte e;
    VERIFIER(alerte_principale(e) == Alerte::AUCUNE); // tout inconnu : rien
    e.min_dix = 40;
    e.max_dix = 150;
    e.code_wmo = 3;
    VERIFIER(alerte_principale(e) == Alerte::AUCUNE);
    e.min_dix = 0;
    VERIFIER(alerte_principale(e) == Alerte::GEL);
    e.min_dix = 1;
    VERIFIER(alerte_principale(e) == Alerte::AUCUNE);
    e.min_dix = -30;
    e.max_dix = 319;
    VERIFIER(alerte_principale(e) == Alerte::GEL);
    e.max_dix = 320;
    VERIFIER(alerte_principale(e) == Alerte::CHALEUR); // plus grave que le gel
    e.rafales_dix_kmh = 599;
    VERIFIER(alerte_principale(e) == Alerte::CHALEUR);
    e.rafales_dix_kmh = 600;
    VERIFIER(alerte_principale(e) == Alerte::VENT_FORT);
    e.code_wmo = 65;
    VERIFIER(alerte_principale(e) == Alerte::PLUIE_FORTE);
    e.code_wmo = 95;
    VERIFIER(alerte_principale(e) == Alerte::ORAGE);
    for (int orage : {95, 96, 97, 99}) {
        EntreesAlerte o;
        o.code_wmo = orage;
        VERIFIER(alerte_principale(o) == Alerte::ORAGE);
    }
    // cumul de pluie du jour : 20 mm ET une probabilité d'au moins 60 %
    EntreesAlerte p;
    p.pluie_jour_dix_mm = 200;
    p.proba_pluie_pct = 59;
    VERIFIER(alerte_principale(p) == Alerte::AUCUNE);
    p.proba_pluie_pct = 60;
    VERIFIER(alerte_principale(p) == Alerte::PLUIE_FORTE);
    p.pluie_jour_dix_mm = 199;
    VERIFIER(alerte_principale(p) == Alerte::AUCUNE);
    return 0;
}

constexpr int la_pluie_proche_distingue_les_quatre_cas() {
    VERIFIER(pluie_proche(0) == PluieProche::EN_COURS);
    VERIFIER(pluie_proche(1) == PluieProche::BIENTOT);
    VERIFIER(pluie_proche(119) == PluieProche::BIENTOT);
    VERIFIER(pluie_proche(-1) == PluieProche::AUCUNE);
    VERIFIER(pluie_proche(-2) == PluieProche::INCONNUE);
    VERIFIER(pluie_proche(-99) == PluieProche::INCONNUE);
    return 0;
}

constexpr int l_echelle_de_la_semaine_arrondit_vers_l_exterieur_et_n_est_jamais_nulle() {
    const int mins[] = {82, 65, 30, -12, 54, 70, 91};
    const int maxs[] = {146, 98, 130, 70, 160, 151, 174};
    const Echelle e = echelle_semaine(mins, maxs, 7);
    VERIFIER(e.mini_dix == -20); // -1,2 -> -2 °C
    VERIFIER(e.maxi_dix == 180); // 17,4 -> 18 °C
    // semaine très plate : étendue minimale de 10 °C, centrée
    const int plat_min[] = {120, 121, 119};
    const int plat_max[] = {130, 131, 129};
    const Echelle p = echelle_semaine(plat_min, plat_max, 3);
    VERIFIER(p.maxi_dix - p.mini_dix >= ETENDUE_MIN_DIX);
    VERIFIER(p.mini_dix <= 119 && p.maxi_dix >= 131);
    // inconnues : une échelle quand même utilisable
    const int inconnus[] = {INCONNU_DIX, INCONNU_DIX};
    const Echelle i = echelle_semaine(inconnus, inconnus, 2);
    VERIFIER(i.maxi_dix > i.mini_dix);
    // un jour sans minimum connu ne fausse pas l'échelle
    const int m2[] = {INCONNU_DIX, 50};
    const int x2[] = {200, INCONNU_DIX};
    const Echelle d = echelle_semaine(m2, x2, 2);
    VERIFIER(d.mini_dix <= 50 && d.maxi_dix >= 200);
    // négatif entier
    const int fm[] = {-100};
    const int fx[] = {-30};
    const Echelle n = echelle_semaine(fm, fx, 1);
    VERIFIER(n.mini_dix <= -100 && n.maxi_dix >= -30);
    return 0;
}

constexpr int une_valeur_est_placee_sur_l_echelle_et_bornee() {
    const Echelle e{0, 200};
    VERIFIER(position_sur_echelle(0, e, 400) == 0);
    VERIFIER(position_sur_echelle(100, e, 400) == 200);
    VERIFIER(position_sur_echelle(200, e, 400) == 400);
    VERIFIER(position_sur_echelle(-50, e, 400) == 0);   // sous l'échelle
    VERIFIER(position_sur_echelle(999, e, 400) == 400); // au-dessus
    VERIFIER(position_sur_echelle(INCONNU_DIX, e, 400) == 0);
    VERIFIER(position_sur_echelle(10, Echelle{5, 5}, 400) == 0); // échelle dégénérée : pas de division par zéro
    return 0;
}

constexpr int les_barres_de_pluie_se_voient_des_5_pour_cent() {
    VERIFIER(hauteur_barre_pct(0, 60) == 0);
    VERIFIER(hauteur_barre_pct(-1, 60) == 0); // inconnue
    VERIFIER(hauteur_barre_pct(1, 60) == 2);  // minimum visible
    VERIFIER(hauteur_barre_pct(5, 60) == 3);
    VERIFIER(hauteur_barre_pct(50, 60) == 30);
    VERIFIER(hauteur_barre_pct(100, 60) == 60);
    VERIFIER(hauteur_barre_pct(250, 60) == 60); // jamais au-dessus du cadre
    return 0;
}

constexpr int la_couleur_d_une_temperature_suit_la_rampe() {
    const Rampe r{{0x0000FF, 0x00FFFF, 0x00FF00, 0xFFFF00, 0xFF0000}};
    VERIFIER(couleur_temperature(-100, r) == 0x0000FF);
    VERIFIER(couleur_temperature(-500, r) == 0x0000FF); // plus froid que le premier repère
    VERIFIER(couleur_temperature(0, r) == 0x00FFFF);
    VERIFIER(couleur_temperature(120, r) == 0x00FF00);
    VERIFIER(couleur_temperature(240, r) == 0xFFFF00);
    VERIFIER(couleur_temperature(360, r) == 0xFF0000);
    VERIFIER(couleur_temperature(900, r) == 0xFF0000);
    VERIFIER(couleur_temperature(INCONNU_DIX, r) == 0x00FF00); // neutre
    // à mi-chemin entre 0 °C (00FFFF) et 12 °C (00FF00) : 00FF80 (arrondi)
    const std::uint32_t milieu = couleur_temperature(60, r);
    VERIFIER((milieu >> 16) == 0x00 && ((milieu >> 8) & 0xFF) == 0xFF && (milieu & 0xFF) >= 0x7F &&
             (milieu & 0xFF) <= 0x80);
    // monotone : plus chaud ne revient jamais en arrière sur le canal rouge de 12 °C à 36 °C
    int rouge_precedent = 0;
    for (int t = 120; t <= 360; t += 5) {
        const int rouge = static_cast<int>(couleur_temperature(t, r) >> 16);
        VERIFIER(rouge >= rouge_precedent);
        rouge_precedent = rouge;
    }
    return 0;
}

constexpr int l_heure_et_le_jour_locaux_suivent_le_decalage() {
    constexpr std::int64_t MINUIT_UTC = 1793923200; // 2026-11-06 00:00:00 UTC, un vendredi
    VERIFIER(heure_locale(MINUIT_UTC + 10 * 3600 + 40 * 60, 0) == 10);
    VERIFIER(heure_locale(MINUIT_UTC + 10 * 3600 + 40 * 60, 3600) == 11); // Europe/Paris en hiver
    VERIFIER(heure_locale(MINUIT_UTC + 23 * 3600, 3600) == 0);            // passe minuit
    VERIFIER(heure_locale(MINUIT_UTC + 1 * 3600, -5 * 3600) == 20);       // la veille au soir
    VERIFIER(heure_locale(MINUIT_UTC, 0) == 0);
    VERIFIER(jour_semaine_locale(MINUIT_UTC, 0) == 5);                    // vendredi
    VERIFIER(jour_semaine_locale(MINUIT_UTC + 23 * 3600, 3600) == 6);     // samedi à Paris
    VERIFIER(jour_semaine_locale(MINUIT_UTC + 1 * 3600, -5 * 3600) == 4); // jeudi à New York
    VERIFIER(jour_semaine_locale(0, 0) == 4);                             // le 1er janvier 1970 : jeudi
    VERIFIER(heure_locale(-1, 0) == 23);                                  // instant négatif
    return 0;
}

constexpr int les_formats_de_nombres_utilisent_le_vrai_signe_moins() {
    char t[16] = {};
    VERIFIER(formater_temperature(124, t, sizeof t) == 2 && texte_egal(t, "12"));
    VERIFIER(formater_temperature(125, t, sizeof t) == 2 && texte_egal(t, "13")); // moitié loin de zéro
    VERIFIER(formater_temperature(0, t, sizeof t) == 1 && texte_egal(t, "0"));
    VERIFIER(formater_temperature(-26, t, sizeof t) == 4 && texte_egal(t, "\xE2\x88\x92"
                                                                          "3")); // −3
    VERIFIER(formater_temperature(-4, t, sizeof t) == 1 && texte_egal(t, "0"));  // −0,4 -> 0 (jamais « −0 »)
    VERIFIER(formater_temperature(-1234, t, sizeof t) == 6 && texte_egal(t, "\xE2\x88\x92"
                                                                            "123"));
    VERIFIER(formater_temperature(INCONNU_DIX, t, sizeof t) == 0 && t[0] == '\0');
    char court[3] = {};
    VERIFIER(formater_temperature(-1234, court, sizeof court) == 0 && court[0] == '\0'); // tampon trop petit
    VERIFIER(formater_entier_signe(7, t, sizeof t) == 1 && texte_egal(t, "7"));
    VERIFIER(formater_entier_signe(1013, t, sizeof t) == 4 && texte_egal(t, "1013"));
    VERIFIER(formater_entier_signe(0, nullptr, 8) == 0);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_code_a_une_ambiance_et_les_familles_sont_les_bonnes)                                                      \
    X(l_indice_uv_suit_l_echelle_de_l_oms)                                                                             \
    X(le_vent_suit_l_echelle_de_beaufort)                                                                              \
    X(la_tendance_de_pression_a_un_seuil_de_un_hectopascal)                                                            \
    X(le_vent_se_range_en_secteurs_de_45_degres)                                                                       \
    X(les_alertes_ont_une_priorite_et_ne_s_inventent_pas)                                                              \
    X(la_pluie_proche_distingue_les_quatre_cas)                                                                        \
    X(l_echelle_de_la_semaine_arrondit_vers_l_exterieur_et_n_est_jamais_nulle)                                         \
    X(une_valeur_est_placee_sur_l_echelle_et_bornee)                                                                   \
    X(les_barres_de_pluie_se_voient_des_5_pour_cent)                                                                   \
    X(la_couleur_d_une_temperature_suit_la_rampe)                                                                      \
    X(l_heure_et_le_jour_locaux_suivent_le_decalage)                                                                   \
    X(les_formats_de_nombres_utilisent_le_vrai_signe_moins)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
