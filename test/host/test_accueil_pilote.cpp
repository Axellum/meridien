// Tests du pilote du mode automatique de l'accueil : accueil_pilote.hpp (ADR-0023, qui branche ADR-0015 sur les mesures de l'appareil).
//
// Aucune donnée réelle : les mesures sont écrites à la main, le résultat attendu est ce que dit l'ADR (pas ce que rend le code). Un test
// relit la règle autrement (toutes les combinaisons d'alerte, de pluie, de fraîcheur, d'épinglage et de toucher) pour que l'oubli d'un
// croisement se voie. Les instants sont ceux du dimanche 8 novembre 2026 à Paris (CET, UTC+1) : valeurs d'exemple. Le lever et le coucher
// du soleil sont ceux que calcule soleil.hpp (le test vérifie qu'ils sont plausibles pour Paris en novembre, pas qu'ils sont exacts).
// Chaque test est évalué à la compilation (static_assert) ET à l'exécution (voir mini.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ecran/accueil_pilote.hpp"
#include "ecran/visuel_textes.hpp"
#include "mini.hpp"

using namespace ecran::accueil;
using ecran::heure::Fuseau;
using ecran::vigie::Niveau;

namespace {

// 2026-11-08 00:00 à Paris = 2026-11-07 23:00 UTC.
constexpr std::int64_t MINUIT = 1'794'092'400;
constexpr std::int64_t T0 = MINUIT + 12 * 3600; // midi
constexpr std::int64_t JOUR = 86'400;

constexpr Fuseau paris() { return ecran::heure::analyser_tz("CET-1CEST,M3.5.0,M10.5.0/3").fuseau; }

constexpr Mesures base() {
    Mesures m;
    m.maintenant_s = T0;
    m.fuseau = paris();
    m.fuseau_connu = true;
    m.position_connue = true;
    m.latitude_deg = 48.8566;
    m.longitude_deg = 2.3522;
    m.lieu = cle_du_lieu("Paris", 48.8566, 2.3522);
    m.meteo_connue = true;
    m.meteo_perimee = false;
    m.pluie_dans_min = -1; // rien dans 2 h
    return m;
}

constexpr bool sans_ajout(const Presentation &p) {
    return p.profil == Profil::Meteo && p.raison == Raison::Defaut && p.raison_profil == Raison::Defaut && !p.epingle &&
           !p.bandeau_agrandi && !p.carte_pluie;
}

constexpr bool meme_chaine(const char *a, std::string_view b) { return std::string_view{a} == b; }

// ---- les constantes ---------------------------------------------------------------------------------------------------------------

constexpr int les_constantes_du_pilote() {
    VERIFIER(PROFILS_DISPONIBLES == 1);
    VERIFIER(GEL_TOUCHER_MAX_S == 10);
    VERIFIER(profil_disponible(Profil::Meteo));
    VERIFIER(!profil_disponible(Profil::Surf));
    VERIFIER(!profil_disponible(Profil::VentBateau));
    VERIFIER(!profil_disponible(Profil::Cote));
    VERIFIER(!profil_disponible(static_cast<Profil>(9)));
    VERIFIER(!profil_disponible(static_cast<Profil>(255)));
    return 0;
}

// ---- la clé du lieu -----------------------------------------------------------------------------------------------------------------

constexpr int la_cle_du_lieu_suit_le_nom_et_la_position() {
    const std::uint32_t a = cle_du_lieu("Paris", 48.8566, 2.3522);
    VERIFIER(a != 0);
    VERIFIER(a == cle_du_lieu("Paris", 48.8566, 2.3522));
    VERIFIER(a != cle_du_lieu("Lyon", 48.8566, 2.3522));    // le nom
    VERIFIER(a != cle_du_lieu("Paris", 48.8567, 2.3522));   // 1e-4 degré de latitude
    VERIFIER(a != cle_du_lieu("Paris", 48.8566, 2.3523));   // 1e-4 degré de longitude
    VERIFIER(a != cle_du_lieu("Paris", -48.8566, 2.3522));  // le signe de la latitude
    VERIFIER(a != cle_du_lieu("Paris", 48.8566, -2.3522));  // le signe de la longitude
    VERIFIER(a == cle_du_lieu("Paris", 48.85662, 2.35221)); // moins d'un demi 1e-4 : la même position
    VERIFIER(a != cle_du_lieu("Pari", 48.8566, 2.3522));    // un nom plus court
    VERIFIER(a != cle_du_lieu("Paris ", 48.8566, 2.3522));  // un octet de plus
    // Les latitudes négatives s'arrondissent comme les positives (l'hémisphère sud n'est pas un cas à part).
    VERIFIER(cle_du_lieu("Sydney", -33.8688, 151.2093) == cle_du_lieu("Sydney", -33.86882, 151.20932));
    VERIFIER(cle_du_lieu("Sydney", -33.8688, 151.2093) != cle_du_lieu("Sydney", -33.8689, 151.2093));
    VERIFIER(cle_du_lieu(nullptr, 0, 0) != 0);
    VERIFIER(cle_du_lieu("", 0, 0) == cle_du_lieu(nullptr, 0, 0)); // pas de nom : rien de mêlé
    return 0;
}

constexpr int la_cle_du_lieu_n_est_jamais_nulle() {
    VERIFIER(detail::cle_non_nulle(0) == 1);
    VERIFIER(detail::cle_non_nulle(1) == 1);
    VERIFIER(detail::cle_non_nulle(7) == 7);
    VERIFIER(detail::cle_non_nulle(0xFFFFFFFFU) == 0xFFFFFFFFU);
    return 0;
}

// ---- les phrases de la pastille ----------------------------------------------------------------------------------------------------

constexpr int une_phrase_par_raison() {
    VERIFIER(meme_chaine(phrase_pastille(Raison::Defaut, Profil::Meteo), "Auto · Météo"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::AlerteOrange, Profil::Meteo), "Auto · alerte orange"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::AlerteRouge, Profil::Meteo), "Auto · alerte rouge"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::PluieProche, Profil::Meteo), "Auto · pluie proche"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::Surf, Profil::Surf), "Auto · Surf"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::VentBateau, Profil::VentBateau), "Auto · Vent·Bateau"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::Cote, Profil::Cote), "Auto · Côte"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::DonneesPerimees, Profil::Meteo), "Auto · données périmées"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::Epingle, Profil::Meteo), "Météo · épinglé"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::ChangementDiffere, Profil::Meteo), "Auto · changement différé"));
    VERIFIER(meme_chaine(PHRASE_AUTO_EN_PAUSE, "Auto en pause"));
    return 0;
}

constexpr int la_phrase_d_un_epinglage_ne_dit_que_le_profil() {
    // Un profil sans phrase propre (il n'est pas offert aujourd'hui) se lit « Épinglé » : jamais « Météo · épinglé » à tort.
    VERIFIER(meme_chaine(phrase_pastille(Raison::Epingle, Profil::Surf), "Épinglé"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::Epingle, Profil::Cote), "Épinglé"));
    VERIFIER(meme_chaine(phrase_pastille(Raison::Epingle, Profil::Meteo), "Météo · épinglé"));
    return 0;
}

constexpr int toutes_les_phrases_sont_differentes_et_une_raison_inconnue_se_lit_par_defaut() {
    // Dix raisons, dix phrases (le profil Météo pour l'épinglage) : deux raisons ne se confondent pas à l'écran.
    for (std::size_t i = 0; i < NOMBRE_RAISONS; ++i) {
        const auto ri = static_cast<Raison>(i);
        VERIFIER(phrase_pastille(ri, Profil::Meteo)[0] != '\0');
        for (std::size_t j = i + 1; j < NOMBRE_RAISONS; ++j)
            VERIFIER(!meme_chaine(phrase_pastille(ri, Profil::Meteo),
                                  phrase_pastille(static_cast<Raison>(j), Profil::Meteo)));
    }
    VERIFIER(meme_chaine(phrase_pastille(static_cast<Raison>(99), Profil::Meteo), "Auto · Météo"));
    return 0;
}

constexpr bool a_ses_six_traductions(std::string_view cle) {
    if (!ecran::i18n::est_une_cle(cle))
        return false;
    for (int l = 1; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
        if (ecran::i18n::propre(cle, ecran::i18n::langue_depuis_indice(l)).empty())
            return false;
    }
    return true;
}

constexpr int chaque_phrase_de_la_pastille_a_ses_six_traductions() {
    // La table commune des sept langues (i18n/*.yaml) porte une entrée pour chacune des phrases que le pilote peut demander.
    for (std::size_t i = 0; i < NOMBRE_RAISONS; ++i)
        VERIFIER(a_ses_six_traductions(phrase_pastille(static_cast<Raison>(i), Profil::Meteo)));
    VERIFIER(a_ses_six_traductions(phrase_pastille(Raison::Epingle, Profil::Surf)));
    VERIFIER(a_ses_six_traductions(PHRASE_AUTO_EN_PAUSE));
    return 0;
}

constexpr int le_ton_de_la_pastille() {
    VERIFIER(ton_de(Raison::AlerteOrange) == Ton::Alerte);
    VERIFIER(ton_de(Raison::AlerteRouge) == Ton::Alerte);
    VERIFIER(ton_de(Raison::PluieProche) == Ton::Pluie);
    VERIFIER(ton_de(Raison::Epingle) == Ton::Epingle);
    VERIFIER(ton_de(Raison::Defaut) == Ton::Neutre);
    VERIFIER(ton_de(Raison::DonneesPerimees) == Ton::Neutre);
    VERIFIER(ton_de(Raison::ChangementDiffere) == Ton::Neutre);
    VERIFIER(ton_de(Raison::Surf) == Ton::Neutre);
    return 0;
}

// ---- la présentation ----------------------------------------------------------------------------------------------------------------

constexpr int l_egalite_de_deux_presentations_compare_chaque_champ() {
    const Presentation a;
    VERIFIER(a == Presentation{});
    VERIFIER(!(a != Presentation{}));
    Presentation b = a;
    b.profil = Profil::Surf;
    VERIFIER(a != b);
    b = a;
    b.raison = Raison::PluieProche;
    VERIFIER(a != b);
    b = a;
    b.raison_profil = Raison::Surf;
    VERIFIER(a != b);
    b = a;
    b.epingle = true;
    VERIFIER(a != b);
    b = a;
    b.bandeau_agrandi = true;
    VERIFIER(a != b);
    b = a;
    b.carte_pluie = true;
    VERIFIER(a != b);
    return 0;
}

// ---- le pilote : ce qui entre dans la décision ---------------------------------------------------------------------------------------

constexpr int sans_rien_de_recu_l_ecran_est_meteo_sans_ajout() {
    Pilote p;
    const Presentation r = p.avancer(Mesures{});
    VERIFIER(sans_ajout(r));
    VERIFIER(!p.gelee());
    return 0;
}

constexpr int les_mesures_vont_dans_les_entrees_de_la_decision() {
    Pilote p;
    Mesures m = base();
    m.niveau_alerte = Niveau::Orange;
    m.pluie_dans_min = 25;
    p.avancer(m);
    const Entrees &e = p.dernieres_entrees();
    VERIFIER(e.maintenant_s == T0);
    VERIFIER(e.decalage_local_s == 3600); // CET le 8 novembre
    VERIFIER(e.niveau_alerte == Niveau::Orange);
    VERIFIER(e.pluie_dans_min == 25);
    VERIFIER(e.fraicheur_meteo == Fraicheur::Fraiche);
    VERIFIER(!e.epingle && !e.toucher_en_cours);
    // Les mêmes mesures en été : l'heure d'été (CEST, UTC+2) est celle du lieu.
    Pilote ete;
    Mesures m2 = base();
    m2.maintenant_s = 1'784'109'600; // 2026-07-15 12:00 à Paris
    ete.avancer(m2);
    VERIFIER(ete.dernieres_entrees().decalage_local_s == 7200);
    return 0;
}

constexpr int la_fraicheur_de_la_meteo_est_inconnue_fraiche_ou_perimee() {
    Pilote p;
    Mesures m = base();
    m.meteo_connue = false;
    m.meteo_perimee = false;
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().fraicheur_meteo == Fraicheur::Inconnue);
    m.meteo_perimee = true; // pas de météo : « périmée » ne veut rien dire
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().fraicheur_meteo == Fraicheur::Inconnue);
    m.meteo_connue = true;
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().fraicheur_meteo == Fraicheur::Perimee);
    m.meteo_perimee = false;
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().fraicheur_meteo == Fraicheur::Fraiche);
    return 0;
}

constexpr int la_pluie_inconnue_ou_absente_reste_inconnue_pour_la_decision() {
    Pilote p;
    Mesures m = base();
    m.pluie_dans_min = -1; // rien dans deux heures
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().pluie_dans_min == VALEUR_INCONNUE);
    m.pluie_dans_min = -2; // inconnue
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().pluie_dans_min == VALEUR_INCONNUE);
    m.pluie_dans_min = 0; // il pleut : 0 est une VRAIE valeur, pas « inconnu »
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().pluie_dans_min == 0);
    m.pluie_dans_min = 1;
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().pluie_dans_min == 1);
    return 0;
}

constexpr int ce_que_l_appareil_ne_sait_pas_encore_reste_inconnu() {
    // Ni activité, ni spot, ni seuil de vent, ni données de mer : la décision ne peut jamais en tirer une bascule (ADR-0015).
    Pilote p;
    Mesures m = base();
    m.niveau_alerte = Niveau::Rouge;
    m.pluie_dans_min = 10;
    p.avancer(m);
    const Entrees &e = p.dernieres_entrees();
    VERIFIER(!e.activite_surf && !e.activite_vent_bateau && !e.spot_surf && !e.spot_peche_a_pied);
    VERIFIER(e.seuil_vent_dixiemes_kmh == 0);
    VERIFIER(e.vent_dixiemes_kmh == VALEUR_INCONNUE);
    VERIFIER(e.fraicheur_mer == Fraicheur::Inconnue);
    VERIFIER(e.basse_mer_dans_min == VALEUR_INCONNUE);
    return 0;
}

// ---- le lever et le coucher du soleil -------------------------------------------------------------------------------------------------

constexpr int le_lever_et_le_coucher_sont_ceux_du_lieu_et_du_jour() {
    Pilote p;
    p.avancer(base());
    const Entrees &e = p.dernieres_entrees();
    // Paris, début novembre : lever vers 7 h 15, coucher vers 17 h 10 (heure locale). Des bornes larges : le test vérifie le lieu et le
    // jour, pas la précision de soleil.hpp (testée ailleurs).
    VERIFIER(e.lever_s > MINUIT + 6 * 3600 + 30 * 60 && e.lever_s < MINUIT + 8 * 3600);
    VERIFIER(e.coucher_s > MINUIT + 16 * 3600 + 30 * 60 && e.coucher_s < MINUIT + 17 * 3600 + 45 * 60);
    VERIFIER(e.coucher_s > e.lever_s);
    return 0;
}

constexpr int sans_position_ou_sans_fuseau_ni_lever_ni_coucher() {
    Pilote p;
    Mesures m = base();
    m.position_connue = false;
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().lever_s == 0 && p.dernieres_entrees().coucher_s == 0);
    VERIFIER(p.dernieres_entrees().decalage_local_s == 3600); // le fuseau, lui, est connu
    Pilote q;
    m = base();
    m.fuseau_connu = false;
    q.avancer(m);
    VERIFIER(q.dernieres_entrees().lever_s == 0 && q.dernieres_entrees().coucher_s == 0);
    VERIFIER(q.dernieres_entrees().decalage_local_s == 0); // pas de fuseau : le décalage n'est pas deviné
    Pilote r;
    m = base();
    m.maintenant_s = 0; // heure inconnue
    r.avancer(m);
    VERIFIER(r.dernieres_entrees().lever_s == 0 && r.dernieres_entrees().coucher_s == 0);
    VERIFIER(r.dernieres_entrees().decalage_local_s == 0 && r.dernieres_entrees().maintenant_s == 0);
    return 0;
}

constexpr int un_jour_ou_une_nuit_polaire_n_a_ni_lever_ni_coucher() {
    Pilote p;
    Mesures m = base();
    m.maintenant_s = 1'782'036'000; // 2026-06-21 12:00 (Longyearbyen, UTC+2 l'été) : soleil de minuit
    m.latitude_deg = 78.22;
    m.longitude_deg = 15.65;
    p.avancer(m);
    VERIFIER(p.dernieres_entrees().lever_s == 0 && p.dernieres_entrees().coucher_s == 0);
    return 0;
}

// Le lever est recalculé quand UNE des données du calcul change (jour, mois, année, latitude, longitude) : un pilote neuf par essai, pour
// qu'un changement ne cache pas l'oubli d'un autre.
constexpr int le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu() {
    // Le lendemain : un autre lever (24 h de plus, à quelques dizaines de secondes près).
    Pilote jour;
    Mesures m = base();
    jour.avancer(m);
    const std::int64_t lever_j0 = jour.dernieres_entrees().lever_s;
    m.maintenant_s = T0 + JOUR;
    jour.avancer(m);
    const std::int64_t lever_j1 = jour.dernieres_entrees().lever_s;
    VERIFIER(lever_j1 > lever_j0 + JOUR - 300 && lever_j1 < lever_j0 + JOUR + 300);
    // Un mois plus tard (8 décembre) : même quantième, autre mois.
    Pilote mois;
    m = base();
    mois.avancer(m);
    m.maintenant_s = T0 + 30 * JOUR;
    mois.avancer(m);
    VERIFIER(mois.dernieres_entrees().lever_s > MINUIT + 30 * JOUR &&
             mois.dernieres_entrees().lever_s < MINUIT + 31 * JOUR);
    // Un an plus tard (8 novembre 2027) : même jour et même mois, autre année.
    Pilote an;
    m = base();
    an.avancer(m);
    m.maintenant_s = T0 + 365 * JOUR;
    an.avancer(m);
    VERIFIER(an.dernieres_entrees().lever_s > MINUIT + 365 * JOUR &&
             an.dernieres_entrees().lever_s < MINUIT + 366 * JOUR);
    // Une autre latitude, le même jour (Reykjavik).
    Pilote lat;
    m = base();
    lat.avancer(m);
    const std::int64_t lever_paris = lat.dernieres_entrees().lever_s;
    m.latitude_deg = 64.1466;
    lat.avancer(m);
    VERIFIER(lat.dernieres_entrees().lever_s != lever_paris);
    // Une autre longitude, la même latitude : le soleil se lève plus tôt à l'est.
    Pilote lon;
    m = base();
    lon.avancer(m);
    const std::int64_t lever_a = lon.dernieres_entrees().lever_s;
    m.longitude_deg = 12.0;
    lon.avancer(m);
    VERIFIER(lon.dernieres_entrees().lever_s < lever_a - 1000);
    return 0;
}

// ---- ce que l'écran montre ------------------------------------------------------------------------------------------------------------

constexpr int une_alerte_orange_ou_rouge_agrandit_le_bandeau_et_donne_sa_pastille() {
    Pilote p;
    Mesures m = base();
    m.niveau_alerte = Niveau::Orange;
    Presentation r = p.avancer(m);
    VERIFIER(r.bandeau_agrandi && !r.carte_pluie && !r.epingle);
    VERIFIER(r.raison == Raison::AlerteOrange && r.profil == Profil::Meteo && r.raison_profil == Raison::Defaut);
    m.niveau_alerte = Niveau::Rouge;
    r = p.avancer(m);
    VERIFIER(r.bandeau_agrandi && r.raison == Raison::AlerteRouge);
    m.niveau_alerte = Niveau::Jaune; // sous l'orange : rien ne change à l'écran
    r = p.avancer(m);
    VERIFIER(sans_ajout(r));
    m.niveau_alerte = Niveau::Inconnu;
    r = p.avancer(m);
    VERIFIER(sans_ajout(r));
    return 0;
}

constexpr int la_pluie_proche_donne_la_carte_a_59_minutes_pas_a_60() {
    Pilote p;
    Mesures m = base();
    m.pluie_dans_min = 59;
    Presentation r = p.avancer(m);
    VERIFIER(r.carte_pluie && !r.bandeau_agrandi && r.raison == Raison::PluieProche &&
             r.raison_profil == Raison::Defaut);
    m.pluie_dans_min = 60;
    VERIFIER(sans_ajout(p.avancer(m)));
    m.pluie_dans_min = 0; // il pleut
    r = p.avancer(m);
    VERIFIER(r.carte_pluie && r.raison == Raison::PluieProche);
    m.pluie_dans_min = -1;
    VERIFIER(sans_ajout(p.avancer(m)));
    return 0;
}

constexpr int la_carte_pluie_veut_des_previsions_fraiches() {
    Pilote p;
    Mesures m = base();
    m.pluie_dans_min = 20;
    m.meteo_perimee = true;
    VERIFIER(!p.avancer(m).carte_pluie);
    m.meteo_perimee = false;
    m.meteo_connue = false;
    VERIFIER(!p.avancer(m).carte_pluie);
    m.meteo_connue = true;
    VERIFIER(p.avancer(m).carte_pluie);
    return 0;
}

constexpr int l_alerte_passe_avant_la_pluie_dans_la_pastille_mais_les_deux_sont_montrees() {
    Pilote p;
    Mesures m = base();
    m.niveau_alerte = Niveau::Rouge;
    m.pluie_dans_min = 12;
    const Presentation r = p.avancer(m);
    VERIFIER(r.bandeau_agrandi && r.carte_pluie);
    VERIFIER(r.raison == Raison::AlerteRouge);
    return 0;
}

constexpr int un_profil_epingle_coupe_la_carte_pluie_mais_pas_le_bandeau_agrandi() {
    Pilote p;
    Mesures m = base();
    m.pluie_dans_min = 10;
    m.niveau_alerte = Niveau::Orange;
    m.epingle = true;
    m.profil_epingle = Profil::Meteo;
    const Presentation r = p.avancer(m);
    VERIFIER(r.epingle && r.profil == Profil::Meteo);
    VERIFIER(r.raison == Raison::Epingle && r.raison_profil == Raison::Epingle);
    VERIFIER(!r.carte_pluie);
    VERIFIER(r.bandeau_agrandi);
    VERIFIER(p.etat().profil == Profil::Meteo);
    return 0;
}

constexpr int le_desepinglage_reprend_les_regles_tout_de_suite() {
    Pilote p;
    Mesures m = base();
    m.pluie_dans_min = 10;
    m.epingle = true;
    Presentation r = p.avancer(m);
    VERIFIER(r.epingle && !r.carte_pluie && r.raison == Raison::Epingle);
    m.epingle = false;
    r = p.avancer(m);
    VERIFIER(!r.epingle && r.carte_pluie && r.raison == Raison::PluieProche);
    return 0;
}

constexpr int un_epinglage_que_l_appareil_ne_sait_pas_montrer_vaut_pas_d_epinglage() {
    Mesures m = base();
    m.pluie_dans_min = 10;
    m.epingle = true;
    for (const Profil hors :
         {Profil::Surf, Profil::VentBateau, Profil::Cote, static_cast<Profil>(9), static_cast<Profil>(255)}) {
        Pilote p;
        m.profil_epingle = hors;
        const Presentation r = p.avancer(m);
        VERIFIER(!r.epingle);
        VERIFIER(r.profil == Profil::Meteo);
        VERIFIER(r.carte_pluie && r.raison == Raison::PluieProche); // les règles reprennent la main
        VERIFIER(!p.dernieres_entrees().epingle);
    }
    return 0;
}

constexpr int un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu() {
    // Un état gardé d'un autre temps (Surf, 100 s plus tôt) : les règles ne le veulent plus mais le délai de 30 min le garderait. L'écran
    // ne sait dessiner que Météo : il montre Météo, et la raison du profil n'est plus « changement différé ».
    const EtatAuto garde{Profil::Surf, true, T0 - 100};
    Pilote p(garde);
    Presentation r = p.avancer(base());
    VERIFIER(r.profil == Profil::Meteo);
    VERIFIER(r.raison_profil == Raison::Defaut && r.raison == Raison::Defaut);
    VERIFIER(p.etat().profil == Profil::Meteo);
    VERIFIER(p.etat().changement_connu && p.etat().change_a_s == T0 - 100); // le reste de l'état est gardé
    // Avec une alerte : la pastille garde l'alerte, c'est le profil qui revient à Météo.
    Pilote q(garde);
    Mesures m = base();
    m.niveau_alerte = Niveau::Orange;
    r = q.avancer(m);
    VERIFIER(r.profil == Profil::Meteo && r.raison == Raison::AlerteOrange && r.raison_profil == Raison::Defaut);
    VERIFIER(r.bandeau_agrandi);
    return 0;
}

constexpr int changer_de_lieu_remet_l_etat_de_la_decision_a_zero() {
    const EtatAuto garde{Profil::Surf, true, T0 - 100};
    Pilote p(garde);
    Mesures m = base();
    p.avancer(m);
    p.avancer(m);
    // Même lieu, deux fois de suite : l'état gardé reste (son dernier changement compte toujours).
    VERIFIER(p.etat().changement_connu && p.etat().change_a_s == T0 - 100);
    // Un autre lieu : la décision précédente parlait d'ailleurs.
    m.lieu = cle_du_lieu("Brest", 48.39, -4.49);
    p.avancer(m);
    VERIFIER(!p.etat().changement_connu && p.etat().change_a_s == 0 && p.etat().profil == Profil::Meteo);
    return 0;
}

constexpr int le_premier_lieu_vu_ne_remet_rien_a_zero() {
    // Au premier appel il n'y a pas de « lieu d'avant » : l'état donné au départ est gardé.
    const EtatAuto garde{Profil::Surf, true, T0 - 100};
    Pilote p(garde);
    p.avancer(base());
    VERIFIER(p.etat().changement_connu && p.etat().change_a_s == T0 - 100);
    return 0;
}

// ---- pas de redessin sous le doigt ---------------------------------------------------------------------------------------------------------

constexpr int sous_le_doigt_la_presentation_ne_change_pas_pendant_dix_secondes_au_plus() {
    Pilote p;
    Mesures m = base();
    VERIFIER(sans_ajout(p.avancer(m)));
    // Le doigt se pose, une alerte orange arrive : l'écran ne bouge pas.
    m.toucher = true;
    m.niveau_alerte = Niveau::Orange;
    m.maintenant_s = T0 + 1;
    Presentation r = p.avancer(m);
    VERIFIER(sans_ajout(r) && p.gelee());
    m.maintenant_s = T0 + 10; // 9 s de toucher
    r = p.avancer(m);
    VERIFIER(sans_ajout(r) && p.gelee());
    m.maintenant_s = T0 + 11; // 10 s de toucher : la limite est atteinte, l'écran se met à jour
    r = p.avancer(m);
    VERIFIER(r.bandeau_agrandi && !p.gelee());
    m.maintenant_s = T0 + 12; // le doigt reste : on ne gèle pas deux fois
    r = p.avancer(m);
    VERIFIER(r.bandeau_agrandi && !p.gelee());
    return 0;
}

constexpr int au_relachement_l_ecran_se_met_a_jour_tout_de_suite() {
    Pilote p;
    Mesures m = base();
    p.avancer(m);
    m.toucher = true;
    m.pluie_dans_min = 8;
    m.maintenant_s = T0 + 1;
    VERIFIER(sans_ajout(p.avancer(m)) && p.gelee());
    VERIFIER(p.dernieres_entrees()
                 .toucher_en_cours); // la décision sait qu'un doigt est posé (elle ne change alors pas de profil)
    m.toucher = false;
    m.maintenant_s = T0 + 3;
    const Presentation r = p.avancer(m);
    VERIFIER(r.carte_pluie && !p.gelee());
    VERIFIER(!p.dernieres_entrees().toucher_en_cours);
    return 0;
}

constexpr int un_nouveau_toucher_recommence_le_compte_a_dix_secondes() {
    Pilote p;
    Mesures m = base();
    p.avancer(m);
    m.toucher = true;
    m.maintenant_s = T0 + 1;
    p.avancer(m);
    m.toucher = false;
    m.maintenant_s = T0 + 2;
    p.avancer(m);
    // Second toucher, 30 s plus tard : le compte repart de zéro (et non de T0 + 1).
    m.toucher = true;
    m.niveau_alerte = Niveau::Rouge;
    m.maintenant_s = T0 + 30;
    VERIFIER(!p.avancer(m).bandeau_agrandi && p.gelee());
    m.maintenant_s = T0 + 39;
    VERIFIER(!p.avancer(m).bandeau_agrandi && p.gelee());
    m.maintenant_s = T0 + 40;
    VERIFIER(p.avancer(m).bandeau_agrandi && !p.gelee());
    return 0;
}

constexpr int le_premier_ecran_n_est_jamais_gele() {
    // Rien n'a encore été montré : il n'y a pas d'écran précédent à garder.
    Pilote p;
    Mesures m = base();
    m.toucher = true;
    m.niveau_alerte = Niveau::Orange;
    const Presentation r = p.avancer(m);
    VERIFIER(r.bandeau_agrandi && !p.gelee());
    return 0;
}

constexpr int sans_heure_ou_avec_une_horloge_a_l_envers_rien_n_est_gele() {
    Pilote p;
    Mesures m = base();
    p.avancer(m);
    // Heure inconnue : on ne sait pas depuis quand le doigt est posé.
    m.toucher = true;
    m.maintenant_s = 0;
    m.niveau_alerte = Niveau::Orange;
    VERIFIER(p.avancer(m).bandeau_agrandi && !p.gelee());
    // Horloge revenue en arrière pendant le toucher : le compte ne veut plus rien dire.
    Pilote q;
    m = base();
    q.avancer(m);
    m.toucher = true;
    m.maintenant_s = T0 + 100;
    q.avancer(m);
    m.maintenant_s = T0 + 95;
    m.niveau_alerte = Niveau::Orange;
    VERIFIER(q.avancer(m).bandeau_agrandi && !q.gelee());
    return 0;
}

constexpr int epingler_ou_desepingler_sous_le_doigt_se_voit_tout_de_suite() {
    Pilote p;
    Mesures m = base();
    m.pluie_dans_min = 10;
    VERIFIER(p.avancer(m).carte_pluie);
    // La personne épingle : c'est son geste, il se voit même si le doigt est encore posé.
    m.toucher = true;
    m.epingle = true;
    m.maintenant_s = T0 + 1;
    Presentation r = p.avancer(m);
    VERIFIER(r.epingle && !r.carte_pluie && !p.gelee());
    // Le doigt reste, rien ne change : gelé sur l'état épinglé.
    m.maintenant_s = T0 + 2;
    VERIFIER(p.avancer(m).epingle && p.gelee());
    // Elle désépingle sous le doigt : se voit aussi.
    m.epingle = false;
    m.maintenant_s = T0 + 3;
    r = p.avancer(m);
    VERIFIER(!r.epingle && r.carte_pluie && !p.gelee());
    return 0;
}

constexpr int changer_de_lieu_sous_le_doigt_se_voit_tout_de_suite() {
    Pilote p;
    Mesures m = base();
    p.avancer(m);
    m.toucher = true;
    m.niveau_alerte = Niveau::Orange;
    m.maintenant_s = T0 + 1;
    VERIFIER(!p.avancer(m).bandeau_agrandi && p.gelee()); // gelé : même lieu
    m.lieu = cle_du_lieu("Brest", 48.39, -4.49);
    m.maintenant_s = T0 + 2;
    VERIFIER(p.avancer(m).bandeau_agrandi && !p.gelee()); // le lieu a changé : l'ancien écran parlait d'ailleurs
    return 0;
}

// ---- toutes les combinaisons ------------------------------------------------------------------------------------------------------------

constexpr int toutes_les_combinaisons_suivent_la_table_de_l_adr() {
    constexpr Niveau niveaux[] = {Niveau::Inconnu, Niveau::Vert, Niveau::Jaune, Niveau::Orange, Niveau::Rouge};
    constexpr int pluies[] = {-2, -1, 0, 1, 30, 59, 60, 61, 119};
    for (const Niveau niveau : niveaux) {
        for (const int pluie : pluies) {
            for (int fraicheur = 0; fraicheur < 3; ++fraicheur) { // 0 : météo inconnue, 1 : fraîche, 2 : périmée
                for (int epingle = 0; epingle < 2; ++epingle) {
                    Pilote p;
                    Mesures m = base();
                    m.position_connue =
                        false; // les combinaisons ne dépendent pas du soleil : on évite de le calculer 270 fois
                    m.niveau_alerte = niveau;
                    m.pluie_dans_min = pluie;
                    m.meteo_connue = fraicheur != 0;
                    m.meteo_perimee = fraicheur == 2;
                    m.epingle = epingle != 0;
                    const Presentation r = p.avancer(m);

                    // Attendu, écrit autrement que le code : à partir des seuils de l'ADR.
                    const bool alerte = niveau == Niveau::Orange || niveau == Niveau::Rouge;
                    const bool pluie_proche = fraicheur == 1 && pluie >= 0 && pluie <= 59;
                    VERIFIER(r.bandeau_agrandi == alerte);
                    VERIFIER(r.carte_pluie == (pluie_proche && epingle == 0));
                    VERIFIER(r.epingle == (epingle != 0));
                    VERIFIER(r.profil == Profil::Meteo);
                    Raison attendue = Raison::Defaut;
                    if (epingle != 0)
                        attendue = Raison::
                            Epingle; // ADR-0015 : épinglé, la pastille dit « épinglé » même sous alerte (le bandeau reste agrandi)
                    else if (alerte)
                        attendue = niveau == Niveau::Rouge ? Raison::AlerteRouge : Raison::AlerteOrange;
                    else if (pluie_proche)
                        attendue = Raison::PluieProche;
                    VERIFIER(r.raison == attendue);
                }
            }
        }
    }
    return 0;
}

constexpr int sur_une_journee_a_la_minute_rien_ne_clignote_sous_le_doigt_ni_ne_depasse_la_table() {
    Pilote p;
    Mesures m = base();
    m.position_connue = false;
    Presentation avant;
    bool premier = true;
    int gelees = 0;
    for (int minute = 0; minute < 24 * 60; ++minute) {
        m.maintenant_s = MINUIT + minute * 60;
        // Une pluie qui vient à 10 h 00 pour une heure, une alerte orange de 14 h à 15 h, un doigt posé de 10 h 30 à 10 h 31.
        m.pluie_dans_min = (minute >= 9 * 60 + 5 && minute < 10 * 60)
                               ? 10 * 60 - minute
                               : (minute >= 10 * 60 && minute < 11 * 60 ? 0 : -1);
        m.niveau_alerte = (minute >= 14 * 60 && minute < 15 * 60) ? Niveau::Orange : Niveau::Inconnu;
        m.toucher = minute == 10 * 60 + 30;
        const Presentation r = p.avancer(m);
        VERIFIER(r.profil == Profil::Meteo);
        VERIFIER(r.bandeau_agrandi == (minute >= 14 * 60 && minute < 15 * 60) || p.gelee());
        if (p.gelee()) {
            ++gelees;
            VERIFIER(!premier && r == avant); // gelé : exactement l'écran d'avant
        }
        avant = r;
        premier = false;
    }
    VERIFIER(gelees == 0 || gelees == 1); // un doigt posé 1 minute à la minute : au plus un pas gelé
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_constantes_du_pilote)                                                                                        \
    X(la_cle_du_lieu_suit_le_nom_et_la_position)                                                                       \
    X(la_cle_du_lieu_n_est_jamais_nulle)                                                                               \
    X(une_phrase_par_raison)                                                                                           \
    X(la_phrase_d_un_epinglage_ne_dit_que_le_profil)                                                                   \
    X(toutes_les_phrases_sont_differentes_et_une_raison_inconnue_se_lit_par_defaut)                                    \
    X(chaque_phrase_de_la_pastille_a_ses_six_traductions)                                                              \
    X(le_ton_de_la_pastille)                                                                                           \
    X(l_egalite_de_deux_presentations_compare_chaque_champ)                                                            \
    X(sans_rien_de_recu_l_ecran_est_meteo_sans_ajout)                                                                  \
    X(les_mesures_vont_dans_les_entrees_de_la_decision)                                                                \
    X(la_fraicheur_de_la_meteo_est_inconnue_fraiche_ou_perimee)                                                        \
    X(la_pluie_inconnue_ou_absente_reste_inconnue_pour_la_decision)                                                    \
    X(ce_que_l_appareil_ne_sait_pas_encore_reste_inconnu)                                                              \
    X(le_lever_et_le_coucher_sont_ceux_du_lieu_et_du_jour)                                                             \
    X(sans_position_ou_sans_fuseau_ni_lever_ni_coucher)                                                                \
    X(un_jour_ou_une_nuit_polaire_n_a_ni_lever_ni_coucher)                                                             \
    X(le_lever_suit_le_jour_le_mois_l_annee_et_le_lieu)                                                                \
    X(une_alerte_orange_ou_rouge_agrandit_le_bandeau_et_donne_sa_pastille)                                             \
    X(la_pluie_proche_donne_la_carte_a_59_minutes_pas_a_60)                                                            \
    X(la_carte_pluie_veut_des_previsions_fraiches)                                                                     \
    X(l_alerte_passe_avant_la_pluie_dans_la_pastille_mais_les_deux_sont_montrees)                                      \
    X(un_profil_epingle_coupe_la_carte_pluie_mais_pas_le_bandeau_agrandi)                                              \
    X(le_desepinglage_reprend_les_regles_tout_de_suite)                                                                \
    X(un_epinglage_que_l_appareil_ne_sait_pas_montrer_vaut_pas_d_epinglage)                                            \
    X(un_profil_que_l_appareil_ne_dessine_pas_n_est_jamais_rendu)                                                      \
    X(changer_de_lieu_remet_l_etat_de_la_decision_a_zero)                                                              \
    X(le_premier_lieu_vu_ne_remet_rien_a_zero)                                                                         \
    X(sous_le_doigt_la_presentation_ne_change_pas_pendant_dix_secondes_au_plus)                                        \
    X(au_relachement_l_ecran_se_met_a_jour_tout_de_suite)                                                              \
    X(un_nouveau_toucher_recommence_le_compte_a_dix_secondes)                                                          \
    X(le_premier_ecran_n_est_jamais_gele)                                                                              \
    X(sans_heure_ou_avec_une_horloge_a_l_envers_rien_n_est_gele)                                                       \
    X(epingler_ou_desepingler_sous_le_doigt_se_voit_tout_de_suite)                                                     \
    X(changer_de_lieu_sous_le_doigt_se_voit_tout_de_suite)                                                             \
    X(toutes_les_combinaisons_suivent_la_table_de_l_adr)                                                               \
    X(sur_une_journee_a_la_minute_rien_ne_clignote_sous_le_doigt_ni_ne_depasse_la_table)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
