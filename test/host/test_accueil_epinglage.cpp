// Tests du réglage « Profil » de l'accueil : accueil_epinglage.hpp, reglages_valeurs.hpp (ProfilAccueil) et son entrée du registre
// (ADR-0023, PR B).
//
// Aucune donnée réelle : les valeurs sont écrites à la main, le résultat attendu est ce que dit l'ADR (pas ce que rend le code). Chaque
// test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/accueil_epinglage.hpp"
#include "ecran/accueil_pilote.hpp"
#include "ecran/registre_reglages.hpp"
#include "ecran/reglages_valeurs.hpp"
#include "mini.hpp"

using namespace ecran::accueil;
namespace rg = ecran::reglages;
namespace rr = ecran::registre;

namespace {

constexpr rg::ProfilAccueil hors_table(int i) { return static_cast<rg::ProfilAccueil>(i); }

// ---- le réglage : codes, indices, valeur d'une NVS abîmée ----------------------------------------------------------------------

constexpr int les_deux_profils_ont_leur_code_et_leur_indice() {
    VERIFIER(rg::NOMBRE_PROFILS_ACCUEIL == 2);
    VERIFIER(static_cast<int>(rg::ProfilAccueil::Auto) == 0 && static_cast<int>(rg::ProfilAccueil::Meteo) == 1);
    VERIFIER(rg::code(rg::ProfilAccueil::Auto) == "auto" && rg::code(rg::ProfilAccueil::Meteo) == "meteo");
    VERIFIER(rg::profil_accueil_depuis_code("auto") == rg::ProfilAccueil::Auto);
    VERIFIER(rg::profil_accueil_depuis_code("meteo") == rg::ProfilAccueil::Meteo);
    return 0;
}

constexpr int un_code_inconnu_n_est_pas_un_profil() {
    VERIFIER(!rg::profil_accueil_depuis_code(""));
    VERIFIER(!rg::profil_accueil_depuis_code("Meteo")); // la casse compte, comme pour le thème
    VERIFIER(!rg::profil_accueil_depuis_code("surf"));  // pas encore dessiné : pas offert
    VERIFIER(!rg::profil_accueil_depuis_code("vent_bateau"));
    VERIFIER(!rg::profil_accueil_depuis_code("cote"));
    VERIFIER(!rg::profil_accueil_depuis_code("auto "));
    return 0;
}

constexpr int une_nvs_abimee_ne_donne_jamais_un_profil_epingle() {
    VERIFIER(rg::profil_accueil_depuis_indice(0) == rg::ProfilAccueil::Auto);
    VERIFIER(rg::profil_accueil_depuis_indice(1) == rg::ProfilAccueil::Meteo);
    VERIFIER(rg::profil_accueil_depuis_indice(2) == rg::ProfilAccueil::Auto);
    VERIFIER(rg::profil_accueil_depuis_indice(3) == rg::ProfilAccueil::Auto);
    VERIFIER(rg::profil_accueil_depuis_indice(255) == rg::ProfilAccueil::Auto);
    VERIFIER(rg::profil_accueil_depuis_indice(-1) == rg::ProfilAccueil::Auto);
    VERIFIER(rg::profil_accueil_depuis_indice(2147483647) == rg::ProfilAccueil::Auto);
    // Le défaut passé par l'appelant n'est utilisé que hors table.
    VERIFIER(rg::profil_accueil_depuis_indice(7, rg::ProfilAccueil::Meteo) == rg::ProfilAccueil::Meteo);
    VERIFIER(rg::profil_accueil_depuis_indice(0, rg::ProfilAccueil::Meteo) == rg::ProfilAccueil::Auto);
    return 0;
}

// ---- du réglage au pilote ---------------------------------------------------------------------------------------------------------

constexpr int auto_n_epingle_rien() {
    const Epinglage e = epinglage_de(rg::ProfilAccueil::Auto);
    VERIFIER(!e.epingle);
    return 0;
}

constexpr int meteo_epingle_le_profil_meteo() {
    const Epinglage e = epinglage_de(rg::ProfilAccueil::Meteo);
    VERIFIER(e.epingle && e.profil == Profil::Meteo);
    return 0;
}

constexpr int un_choix_hors_table_n_epingle_rien() {
    for (int i : {2, 3, 7, 9, 127, 255}) {
        VERIFIER(!epinglage_de(hors_table(i)).epingle);
        VERIFIER(!profil_offert(hors_table(i)));
    }
    return 0;
}

constexpr int deux_epinglages_sont_egaux_si_aucun_n_epingle() {
    VERIFIER((Epinglage{false, Profil::Meteo}) == (Epinglage{false, Profil::Surf}));
    VERIFIER(!((Epinglage{true, Profil::Meteo}) == (Epinglage{false, Profil::Meteo})));
    VERIFIER(!((Epinglage{true, Profil::Meteo}) == (Epinglage{true, Profil::Surf})));
    return 0;
}

// ---- ce que le réglage offre ------------------------------------------------------------------------------------------------------

constexpr int le_reglage_n_offre_que_ce_que_l_appareil_dessine() {
    VERIFIER(profil_offert(rg::ProfilAccueil::Auto));
    VERIFIER(profil_offert(rg::ProfilAccueil::Meteo));
    // Chaque profil épinglable fait partie de ceux que le pilote dessine.
    for (std::size_t i = 0; i < rg::NOMBRE_PROFILS_ACCUEIL; ++i) {
        const rg::ProfilAccueil choix = hors_table(static_cast<int>(i));
        const Epinglage e = epinglage_de(choix);
        VERIFIER(!e.epingle || profil_disponible(e.profil));
        VERIFIER(profil_offert(choix));
    }
    return 0;
}

constexpr int le_registre_et_l_api_offrent_les_memes_profils_dans_le_meme_ordre() {
    const rr::Reglage *r = rr::chercher("affichage.profil");
    VERIFIER(r != nullptr);
    VERIFIER(r->type == rr::Type::Choix && r->section == rr::Section::Affichage && r->etape == rr::Etape::Aucune);
    VERIFIER(r->nb_options == rg::NOMBRE_PROFILS_ACCUEIL && r->min == 0 && r->max == 1 && r->defaut == 0 &&
             !r->facultatif);
    VERIFIER(r->route == "/api/display" && r->champ == "profil");
    for (std::size_t i = 0; i < rg::NOMBRE_PROFILS_ACCUEIL; ++i) {
        VERIFIER(r->options[i].code == rg::PROFILS_ACCUEIL[i]);
        VERIFIER(r->options[i].traduire);
        VERIFIER(profil_offert(hors_table(static_cast<int>(i))));
        VERIFIER(rr::indice_option(*r, rg::PROFILS_ACCUEIL[i]) == static_cast<int>(i));
    }
    // Le défaut du registre est « Auto » : pas d'épinglage tant que la personne n'a rien choisi.
    VERIFIER(r->options[r->defaut].code == "auto");
    VERIFIER(rr::choix_depuis_nvs(*r, 2) == 0 && rr::choix_depuis_nvs(*r, -1) == 0 && rr::choix_depuis_nvs(*r, 1) == 1);
    return 0;
}

// ---- l'affichage et sa validation ------------------------------------------------------------------------------------------------

constexpr int l_affichage_vierge_n_epingle_rien() {
    const rg::Affichage a{};
    VERIFIER(a.profil == rg::ProfilAccueil::Auto);
    VERIFIER(!epinglage_de(a.profil).epingle);
    VERIFIER(a == rg::Affichage{});
    rg::Affichage b{};
    b.profil = rg::ProfilAccueil::Meteo;
    VERIFIER(!(a == b)); // le profil compte dans l'égalité : un changement de profil seul est enregistré
    return 0;
}

constexpr rg::ResultatAffichage valider(std::string_view corps, const rg::Affichage &avant = {}) {
    const ecran::json::Resultat r = ecran::json::lire(corps);
    return rg::valider_affichage(r.valeur, avant);
}

constexpr int la_validation_accepte_les_deux_codes_et_refuse_le_reste() {
    rg::ResultatAffichage r = valider(R"({"profil":"meteo"})");
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune && r.valeur.profil == rg::ProfilAccueil::Meteo);
    r = valider(R"({"profil":"auto"})", [] {
        rg::Affichage a{};
        a.profil = rg::ProfilAccueil::Meteo;
        return a;
    }());
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune && r.valeur.profil == rg::ProfilAccueil::Auto);
    // Absent : inchangé.
    rg::Affichage epingle{};
    epingle.profil = rg::ProfilAccueil::Meteo;
    r = valider(R"({"luminosite":50})", epingle);
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune && r.valeur.profil == rg::ProfilAccueil::Meteo &&
             r.valeur.luminosite == 50);
    // Refusé : tout ou rien, l'état actuel reste.
    for (std::string_view corps :
         {R"({"profil":"surf"})", R"({"profil":"Meteo"})", R"({"profil":""})", R"({"profil":2})", R"({"profil":null})",
          R"({"profil":true})", R"({"profil":"meteo_et_plus_de_seize_octets"})"}) {
        r = valider(corps, epingle);
        VERIFIER(r.erreur == rg::ErreurAffichage::Profil);
        VERIFIER(r.valeur == epingle);
    }
    // Une autre erreur n'écrase pas l'état : le profil valide n'est pas appliqué à moitié.
    r = valider(R"({"profil":"auto","luminosite":3})", epingle);
    VERIFIER(r.erreur == rg::ErreurAffichage::Luminosite && r.valeur.profil == rg::ProfilAccueil::Meteo);
    return 0;
}

// ---- de bout en bout, jusqu'au pilote ---------------------------------------------------------------------------------------------

constexpr Mesures mesures_avec_pluie_proche(const Epinglage &e) {
    Mesures m;
    m.maintenant_s = 1'794'092'400 + 12 * 3600;
    m.fuseau = ecran::heure::analyser_tz("CET-1CEST,M3.5.0,M10.5.0/3").fuseau;
    m.fuseau_connu = true;
    m.position_connue = true;
    m.latitude_deg = 48.8566;
    m.longitude_deg = 2.3522;
    m.lieu = cle_du_lieu("Paris", 48.8566, 2.3522);
    m.meteo_connue = true;
    m.pluie_dans_min = 12;
    m.epingle = e.epingle;
    m.profil_epingle = e.profil;
    return m;
}

constexpr int le_reglage_pilote_la_presentation_du_pilote() {
    Pilote auto_;
    const Presentation a = auto_.avancer(mesures_avec_pluie_proche(epinglage_de(rg::ProfilAccueil::Auto)));
    VERIFIER(!a.epingle && a.carte_pluie && a.raison == Raison::PluieProche);
    Pilote epingle;
    const Presentation p = epingle.avancer(mesures_avec_pluie_proche(epinglage_de(rg::ProfilAccueil::Meteo)));
    VERIFIER(p.epingle && !p.carte_pluie && p.raison == Raison::Epingle && p.profil == Profil::Meteo);
    // Un choix hors table se comporte comme Auto : la carte pluie reste.
    Pilote abime;
    const Presentation x = abime.avancer(mesures_avec_pluie_proche(epinglage_de(hors_table(9))));
    VERIFIER(!x.epingle && x.carte_pluie);
    return 0;
}

constexpr int se_desepingler_rend_la_main_aux_regles_tout_de_suite() {
    Pilote p;
    Presentation r = p.avancer(mesures_avec_pluie_proche(epinglage_de(rg::ProfilAccueil::Meteo)));
    VERIFIER(r.epingle && !r.carte_pluie);
    r = p.avancer(mesures_avec_pluie_proche(epinglage_de(rg::ProfilAccueil::Auto)));
    VERIFIER(!r.epingle && r.carte_pluie && r.raison == Raison::PluieProche);
    r = p.avancer(mesures_avec_pluie_proche(epinglage_de(rg::ProfilAccueil::Meteo)));
    VERIFIER(r.epingle && !r.carte_pluie);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_deux_profils_ont_leur_code_et_leur_indice)                                                                   \
    X(un_code_inconnu_n_est_pas_un_profil)                                                                             \
    X(une_nvs_abimee_ne_donne_jamais_un_profil_epingle)                                                                \
    X(auto_n_epingle_rien)                                                                                             \
    X(meteo_epingle_le_profil_meteo)                                                                                   \
    X(un_choix_hors_table_n_epingle_rien)                                                                              \
    X(deux_epinglages_sont_egaux_si_aucun_n_epingle)                                                                   \
    X(le_reglage_n_offre_que_ce_que_l_appareil_dessine)                                                                \
    X(le_registre_et_l_api_offrent_les_memes_profils_dans_le_meme_ordre)                                               \
    X(l_affichage_vierge_n_epingle_rien)                                                                               \
    X(la_validation_accepte_les_deux_codes_et_refuse_le_reste)                                                         \
    X(le_reglage_pilote_la_presentation_du_pilote)                                                                     \
    X(se_desepingler_rend_la_main_aux_regles_tout_de_suite)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
