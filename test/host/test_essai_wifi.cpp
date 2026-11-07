// Tests du noyau pur : essai d'un nouveau Wi-Fi avec retour arrière (essai_wifi.hpp, ADR-0007 point 7).
// Ce que la couche réseau (main/reseau.cpp) décide à chaque événement : réussite, mot de passe refusé, réseau introuvable, délai,
// abandon, absence d'ancien réseau. L'exécution sur la carte (esp_wifi, minuteurs) n'est PAS couverte ici : voir
// docs/mesures/wifi-retour-arriere.md.
#include <cstdint>

#include "ecran/essai_wifi.hpp"
#include "mini.hpp"

namespace w = ecran::wifi;
using w::Phase;
using w::Resultat;

namespace {

constexpr int code_de_chaque_cause() {
    VERIFIER(w::resultat_pour_raison(w::raison::POIGNEE_DE_MAIN_DELAI) == Resultat::MotDePasseRefuse); // 15
    VERIFIER(w::resultat_pour_raison(w::raison::AUTHENTIFICATION) == Resultat::MotDePasseRefuse);      // 202
    VERIFIER(w::resultat_pour_raison(w::raison::POIGNEE_TEMPS) == Resultat::MotDePasseRefuse);         // 204
    VERIFIER(w::resultat_pour_raison(w::raison::MIC_ECHEC) == Resultat::MotDePasseRefuse);             // 14
    VERIFIER(w::resultat_pour_raison(w::raison::AUCUN_POINT_D_ACCES) == Resultat::ReseauIntrouvable);  // 201
    VERIFIER(w::resultat_pour_raison(w::raison::SECURITE_INCOMPATIBLE) == Resultat::ReseauIntrouvable);
    VERIFIER(w::resultat_pour_raison(w::raison::MODE_AUTH_SEUIL) == Resultat::ReseauIntrouvable);
    VERIFIER(w::resultat_pour_raison(w::raison::RSSI_SEUIL) == Resultat::ReseauIntrouvable);
    VERIFIER(w::resultat_pour_raison(200) == Resultat::Autre); // balise perdue : on ne sait pas
    VERIFIER(w::resultat_pour_raison(2) == Resultat::Autre);   // authentification expirée
    VERIFIER(w::resultat_pour_raison(0) == Resultat::Autre);
    VERIFIER(w::resultat_pour_raison(-1) == Resultat::Autre);
    // Les valeurs viennent des sources d'ESP-IDF 5.5.5 : on les fige pour qu'un changement de numérotation se voie.
    VERIFIER(w::raison::FIN_D_ASSOCIATION == 8 && w::raison::MIC_ECHEC == 14 && w::raison::POIGNEE_DE_MAIN_DELAI == 15);
    VERIFIER(w::raison::AUCUN_POINT_D_ACCES == 201 && w::raison::AUTHENTIFICATION == 202 &&
             w::raison::POIGNEE_TEMPS == 204);
    VERIFIER(w::raison::SECURITE_INCOMPATIBLE == 210 && w::raison::MODE_AUTH_SEUIL == 211 &&
             w::raison::RSSI_SEUIL == 212);
    return 0;
}

constexpr int delais_alignes_sur_ceux_de_l_ecran() {
    VERIFIER(w::DELAI_ESSAI_MS == w::DELAI_CONNEXION_MS);
    VERIFIER(w::DELAI_ESSAI_MS == 30000);
    VERIFIER(w::DELAI_RETOUR_MS > 0 && w::DELAI_RETOUR_MS < w::DELAI_ESSAI_MS);
    VERIFIER(w::SEUIL_PREUVES == 2);
    return 0;
}

constexpr int essai_reussi() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    VERIFIER(s.etat.phase == Phase::Essai && s.appliquer_nouveau);
    VERIFIER(!s.appliquer_ancien && !s.rappeler && !s.adopter_nouveau);
    VERIFIER(w::occupe(s.etat));
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos && !w::occupe(s.etat));
    VERIFIER(s.rappeler && s.resultat == Resultat::Connecte);
    VERIFIER(s.adopter_nouveau); // le nouveau réseau devient le réseau courant
    VERIFIER(!s.appliquer_ancien && !s.appliquer_nouveau);
    return 0;
}

constexpr int mot_de_passe_faux_deux_fois_retour_arriere() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI); // 1re preuve : pas encore définitif
    VERIFIER(s.etat.phase == Phase::Essai && !s.appliquer_ancien && !s.rappeler);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI); // 2e preuve : définitif
    VERIFIER(s.etat.phase == Phase::Retour);
    VERIFIER(s.appliquer_ancien);
    VERIFIER(!s.rappeler); // le rappel attend le retour de l'ancien réseau
    VERIFIER(!s.adopter_nouveau);
    s = w::sur_adresse_ip(s.etat); // l'ancien réseau est revenu
    VERIFIER(s.etat.phase == Phase::Repos);
    VERIFIER(s.rappeler && s.resultat == Resultat::MotDePasseRefuse);
    VERIFIER(!s.adopter_nouveau); // jamais : l'ancien reste le réseau courant
    return 0;
}

constexpr int deux_codes_differents_de_la_meme_cause_comptent_ensemble() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI); // 15
    s = w::sur_deconnexion(s.etat, w::raison::AUTHENTIFICATION);      // 202 : même cause « mot de passe »
    VERIFIER(s.etat.phase == Phase::Retour && s.appliquer_ancien);
    return 0;
}

constexpr int causes_differentes_ne_s_additionnent_pas() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI); // mot de passe
    s = w::sur_deconnexion(s.etat, w::raison::AUCUN_POINT_D_ACCES);   // introuvable : on repart à une preuve
    VERIFIER(s.etat.phase == Phase::Essai && !s.appliquer_ancien);
    VERIFIER(s.etat.preuves == 1 && s.etat.derniere_cause == Resultat::ReseauIntrouvable);
    s = w::sur_deconnexion(s.etat, w::raison::AUCUN_POINT_D_ACCES); // deuxième « introuvable » de suite
    VERIFIER(s.etat.phase == Phase::Retour && s.appliquer_ancien);
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.rappeler && s.resultat == Resultat::ReseauIntrouvable);
    return 0;
}

constexpr int cause_autre_n_arrete_jamais_l_essai() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    for (int i = 0; i < 400; ++i) { // balises perdues à la chaîne : seul le délai tranche
        s = w::sur_deconnexion(s.etat, 200);
        VERIFIER(s.etat.phase == Phase::Essai && !s.appliquer_ancien && !s.rappeler);
    }
    s = w::sur_delai_essai(s.etat);
    VERIFIER(s.etat.phase == Phase::Retour && s.appliquer_ancien);
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.rappeler && s.resultat == Resultat::Autre);
    return 0;
}

constexpr int la_deconnexion_provoquee_par_nous_est_ignoree() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    for (int i = 0; i < 5;
         ++i) { // quitter l'ancien réseau envoie « fin d'association » : ce n'est pas un échec de l'essai
        s = w::sur_deconnexion(s.etat, w::raison::FIN_D_ASSOCIATION);
        VERIFIER(s.etat.phase == Phase::Essai && !s.etat.cause_connue && s.etat.preuves == 0);
    }
    // Elle n'interrompt pas non plus une série de preuves : 15, 8, 15 = deux preuves.
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI);
    s = w::sur_deconnexion(s.etat, w::raison::FIN_D_ASSOCIATION);
    VERIFIER(s.etat.phase == Phase::Essai && s.etat.preuves == 1);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI);
    VERIFIER(s.etat.phase == Phase::Retour);
    return 0;
}

constexpr int delai_sans_aucune_cause_rend_autre() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_delai_essai(s.etat);
    VERIFIER(s.etat.phase == Phase::Retour && s.appliquer_ancien && !s.rappeler);
    VERIFIER(s.etat.attendu == Resultat::Autre);
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.rappeler && s.resultat == Resultat::Autre);
    return 0;
}

constexpr int delai_garde_la_derniere_cause_vue() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::AUCUN_POINT_D_ACCES); // une seule fois : pas définitif
    VERIFIER(s.etat.phase == Phase::Essai);
    s = w::sur_delai_essai(s.etat);
    VERIFIER(s.etat.attendu == Resultat::ReseauIntrouvable);
    return 0;
}

constexpr int sans_ancien_reseau_l_echec_est_rendu_tout_de_suite() {
    w::Sortie s = w::demarrer(w::Essai{}, false);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI);
    s = w::sur_deconnexion(s.etat, w::raison::POIGNEE_DE_MAIN_DELAI);
    VERIFIER(s.etat.phase == Phase::Repos);
    VERIFIER(!s.appliquer_ancien); // rien à reprendre
    VERIFIER(s.rappeler && s.resultat == Resultat::MotDePasseRefuse);
    // Même chose au délai.
    s = w::demarrer(w::Essai{}, false);
    s = w::sur_delai_essai(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos && !s.appliquer_ancien && s.rappeler && s.resultat == Resultat::Autre);
    return 0;
}

constexpr int abandon_pendant_l_essai_reprend_l_ancien_sans_rappel() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::abandonner(s.etat);
    VERIFIER(s.etat.phase == Phase::Retour && s.etat.muet);
    VERIFIER(s.appliquer_ancien && !s.rappeler);
    // L'ancien réseau revient : toujours aucun rappel.
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos && !s.rappeler && !s.adopter_nouveau);
    return 0;
}

constexpr int abandon_sans_ancien_reseau() {
    w::Sortie s = w::demarrer(w::Essai{}, false);
    s = w::abandonner(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos && !s.appliquer_ancien && !s.rappeler);
    return 0;
}

constexpr int abandon_pendant_le_retour_supprime_le_rappel() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::AUCUN_POINT_D_ACCES);
    s = w::sur_deconnexion(s.etat, w::raison::AUCUN_POINT_D_ACCES); // retour arrière lancé
    VERIFIER(s.etat.phase == Phase::Retour && !s.etat.muet);
    s = w::abandonner(s.etat); // la personne quitte l'écran pendant le retour
    VERIFIER(s.etat.phase == Phase::Retour && s.etat.muet && !s.appliquer_ancien); // pas de seconde reprise
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos && !s.rappeler);
    return 0;
}

constexpr int delai_du_retour_rend_la_main_quand_meme() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_delai_essai(s.etat);
    VERIFIER(s.etat.phase == Phase::Retour);
    s = w::sur_delai_retour(s.etat); // l'ancien réseau n'est pas revenu à temps
    VERIFIER(s.etat.phase == Phase::Repos && s.rappeler && s.resultat == Resultat::Autre);
    VERIFIER(!s.appliquer_ancien && !s.adopter_nouveau);
    // Muet : pas de rappel.
    s = w::demarrer(w::Essai{}, true);
    s = w::abandonner(s.etat);
    s = w::sur_delai_retour(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos && !s.rappeler);
    return 0;
}

constexpr int un_seul_essai_a_la_fois() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    const w::Essai avant = s.etat;
    s = w::demarrer(s.etat, false); // refusé : rien ne change, rien à faire
    VERIFIER(s.etat.phase == avant.phase && s.etat.avait_ancien == true);
    VERIFIER(!s.appliquer_nouveau && !s.appliquer_ancien && !s.rappeler);
    // Aussi pendant le retour.
    s = w::sur_delai_essai(s.etat);
    s = w::demarrer(s.etat, true);
    VERIFIER(s.etat.phase == Phase::Retour && !s.appliquer_nouveau);
    return 0;
}

constexpr int evenements_hors_phase_sans_effet() {
    w::Essai repos{};
    w::Sortie s = w::sur_adresse_ip(repos); // une adresse IP au repos (reconnexion normale) : rien à faire
    VERIFIER(s.etat.phase == Phase::Repos && !s.rappeler && !s.adopter_nouveau);
    s = w::sur_deconnexion(repos, w::raison::AUTHENTIFICATION);
    VERIFIER(s.etat.phase == Phase::Repos && !s.appliquer_ancien && !s.rappeler);
    s = w::sur_delai_essai(repos);
    VERIFIER(s.etat.phase == Phase::Repos && !s.appliquer_ancien && !s.rappeler);
    s = w::sur_delai_retour(repos);
    VERIFIER(!s.rappeler);
    s = w::abandonner(repos);
    VERIFIER(s.etat.phase == Phase::Repos && !s.appliquer_ancien && !s.rappeler);
    // Une déconnexion pendant le retour ne relance pas d'échec ni de rappel.
    w::Sortie r = w::demarrer(w::Essai{}, true);
    r = w::sur_delai_essai(r.etat);
    r = w::sur_deconnexion(r.etat, w::raison::AUTHENTIFICATION);
    VERIFIER(r.etat.phase == Phase::Retour && !r.rappeler && !r.appliquer_ancien);
    return 0;
}

constexpr int un_resultat_tardif_ne_rappelle_pas_deux_fois() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::AUTHENTIFICATION);
    s = w::sur_deconnexion(s.etat, w::raison::AUTHENTIFICATION);
    s = w::sur_adresse_ip(s.etat); // rappel de l'échec
    VERIFIER(s.rappeler);
    const w::Sortie encore = w::sur_adresse_ip(s.etat); // une seconde adresse IP (renouvellement du bail)
    VERIFIER(!encore.rappeler && encore.etat.phase == Phase::Repos);
    const w::Sortie delai = w::sur_delai_essai(s.etat); // le minuteur arrive après la fin
    VERIFIER(!delai.rappeler && !delai.appliquer_ancien);
    return 0;
}

constexpr int un_nouvel_essai_repart_de_zero() {
    w::Sortie s = w::demarrer(w::Essai{}, true);
    s = w::sur_deconnexion(s.etat, w::raison::AUTHENTIFICATION);
    s = w::sur_deconnexion(s.etat, w::raison::AUTHENTIFICATION);
    s = w::sur_adresse_ip(s.etat);
    VERIFIER(s.etat.phase == Phase::Repos);
    // L'état gardé après l'échec ne doit pas contaminer l'essai suivant.
    s = w::demarrer(s.etat, true);
    VERIFIER(s.etat.phase == Phase::Essai && !s.etat.muet && !s.etat.cause_connue && s.etat.preuves == 0);
    s = w::sur_deconnexion(s.etat, w::raison::AUTHENTIFICATION); // une seule preuve : pas définitif
    VERIFIER(s.etat.phase == Phase::Essai);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(code_de_chaque_cause)                                                                                            \
    X(delais_alignes_sur_ceux_de_l_ecran)                                                                              \
    X(essai_reussi)                                                                                                    \
    X(mot_de_passe_faux_deux_fois_retour_arriere)                                                                      \
    X(deux_codes_differents_de_la_meme_cause_comptent_ensemble)                                                        \
    X(causes_differentes_ne_s_additionnent_pas)                                                                        \
    X(cause_autre_n_arrete_jamais_l_essai)                                                                             \
    X(la_deconnexion_provoquee_par_nous_est_ignoree)                                                                   \
    X(delai_sans_aucune_cause_rend_autre)                                                                              \
    X(delai_garde_la_derniere_cause_vue)                                                                               \
    X(sans_ancien_reseau_l_echec_est_rendu_tout_de_suite)                                                              \
    X(abandon_pendant_l_essai_reprend_l_ancien_sans_rappel)                                                            \
    X(abandon_sans_ancien_reseau)                                                                                      \
    X(abandon_pendant_le_retour_supprime_le_rappel)                                                                    \
    X(delai_du_retour_rend_la_main_quand_meme)                                                                         \
    X(un_seul_essai_a_la_fois)                                                                                         \
    X(evenements_hors_phase_sans_effet)                                                                                \
    X(un_resultat_tardif_ne_rappelle_pas_deux_fois)                                                                    \
    X(un_nouvel_essai_repart_de_zero)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
