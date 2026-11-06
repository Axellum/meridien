// Tests du noyau pur : installation guidée, changement de Wi-Fi avec retour arrière, remplacement des marqueurs {nom}
// (ecran/installation.hpp, ecran/changement_wifi.hpp, ecran/texte_modele.hpp). Chaque test est évalué à la compilation
// (static_assert) ET à l'exécution (voir mini.hpp).
//
// Ce que ces tests prouvent : l'octet gardé en mémoire permanente se relit à l'identique et un octet abîmé ne passe jamais pour
// « installation terminée » ; seules la langue et le Wi-Fi bloquent la suite ; « refaire » et « réinitialiser » sont deux
// transitions différentes ; la réinitialisation exige deux gestes dans un délai ; un essai de Wi-Fi qui ne répond pas dans le délai
// devient un échec. Ce qu'ils NE prouvent PAS : que l'appareil reprend vraiment l'ancien réseau (c'est main/reseau.*, à constater
// sur la carte), ni que 30 secondes soit le bon délai (valeur de départ, à régler à la mesure).
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ecran/changement_wifi.hpp"
#include "ecran/i18n.hpp"
#include "ecran/installation.hpp"
#include "ecran/texte_modele.hpp"
#include "mini.hpp"

using namespace ecran::installation;
using ecran::i18n::Langue;
namespace wifi = ecran::wifi;
namespace texte = ecran::texte;

namespace {

constexpr Etape TOUTES_LES_ETAPES[NOMBRE_ETAPES] = {Etape::Langue,       Etape::Wifi,      Etape::Lieu,
                                                    Etape::Affichage,    Etape::Assistant, Etape::Telephone,
                                                    Etape::Recapitulatif};

// ---- installation guidée ------------------------------------------------------------------------------------------------

constexpr int l_octet_garde_se_relit_a_l_identique() {
    for (Etape e : TOUTES_LES_ETAPES) {
        for (Mode m : {Mode::Premiere, Mode::Refaire}) {
            const Progression p{e, false, m};
            VERIFIER(decoder(encoder(p)) == p);
        }
    }
    for (Mode m : {Mode::Premiere, Mode::Refaire}) {
        const Progression finie{Etape::Langue, true, m};
        VERIFIER(decoder(encoder(finie)) == finie);
        VERIFIER(decoder(encoder(finie)).terminee);
    }
    VERIFIER(encoder(Progression{Etape::Langue, false, Mode::Premiere}) == 1);
    VERIFIER(encoder(Progression{Etape::Recapitulatif, false, Mode::Premiere}) == 7);
    VERIFIER(encoder(Progression{Etape::Langue, true, Mode::Premiere}) == 8);
    VERIFIER(encoder(Progression{Etape::Wifi, false, Mode::Refaire}) == 0x12);
    return 0;
}

constexpr int un_octet_abime_ne_passe_jamais_pour_une_installation_terminee() {
    // 0 (rien d'écrit), 9 à 15 (inconnus), au-delà de 0x1F, négatif : première installation, étape 1
    for (long long v : {0LL, 9LL, 10LL, 15LL, 0x19LL, 0x1FLL, 0x20LL, 255LL, -1LL, 100000LL}) {
        const Progression p = decoder(v);
        VERIFIER(!p.terminee);
        VERIFIER(p.etape == Etape::Langue);
        VERIFIER(p.mode == Mode::Premiere);
        VERIFIER(installation_a_montrer(p));
    }
    // TOUT ce qui dépasse 0x1F (un octet écrit par une version plus récente, un stockage abîmé) retombe sur la valeur sûre
    for (long long v = 0x20; v <= 0x1FF; ++v)
        VERIFIER(decoder(v) == Progression{});
    return 0;
}

constexpr int le_drapeau_termine_decide_de_l_ecran_au_demarrage() {
    VERIFIER(installation_a_montrer(Progression{}));
    VERIFIER(installation_a_montrer(decoder(3)));
    VERIFIER(!installation_a_montrer(decoder(8)));
    VERIFIER(!installation_a_montrer(decoder(0x18))); // terminée, venue d'un « refaire »
    return 0;
}

constexpr int les_sept_etapes_se_parcourent_dans_l_ordre() {
    VERIFIER(NOMBRE_ETAPES == 7);
    Progression p = Progression{};
    for (int attendu = 1; attendu <= NOMBRE_ETAPES; ++attendu) {
        VERIFIER(numero(p.etape) == attendu);
        VERIFIER(!p.terminee);
        p = avancer(p);
    }
    VERIFIER(p.terminee); // après le récapitulatif, l'installation est terminée
    VERIFIER(suivante(Etape::Recapitulatif) == Etape::Recapitulatif); // pas de huitième étape
    VERIFIER(precedente(Etape::Langue) == Etape::Langue);             // pas d'étape zéro
    VERIFIER(suivante(Etape::Aucune) == Etape::Aucune && precedente(Etape::Aucune) == Etape::Aucune);
    for (int i = 0; i + 1 < NOMBRE_ETAPES; ++i) {
        VERIFIER(suivante(TOUTES_LES_ETAPES[i]) == TOUTES_LES_ETAPES[i + 1]);
        VERIFIER(precedente(TOUTES_LES_ETAPES[i + 1]) == TOUTES_LES_ETAPES[i]);
    }
    return 0;
}

constexpr int reculer_est_possible_partout_sauf_a_la_premiere_etape() {
    VERIFIER(!retour_possible(Etape::Langue));
    for (int i = 1; i < NOMBRE_ETAPES; ++i)
        VERIFIER(retour_possible(TOUTES_LES_ETAPES[i]));
    VERIFIER(reculer(Progression{Etape::Lieu, false, Mode::Premiere}).etape == Etape::Wifi);
    VERIFIER(reculer(Progression{Etape::Langue, false, Mode::Premiere}).etape == Etape::Langue);
    VERIFIER(reculer(Progression{Etape::Wifi, false, Mode::Refaire}).mode == Mode::Refaire); // le mode se garde
    VERIFIER(avancer(Progression{Etape::Wifi, false, Mode::Refaire}).mode == Mode::Refaire);
    return 0;
}

constexpr int seules_la_langue_et_le_wifi_bloquent_la_suite() {
    Contexte rien;
    for (Etape e : TOUTES_LES_ETAPES) {
        const bool bloque = e == Etape::Langue || e == Etape::Wifi;
        VERIFIER(peut_continuer(e, rien, Mode::Premiere) == !bloque);
    }
    // la langue : débloquée dès qu'une langue est touchée (même celle déjà en place)
    Contexte langue;
    langue.langue_choisie = true;
    VERIFIER(peut_continuer(Etape::Langue, langue, Mode::Premiere));
    // le Wi-Fi : connecté, ou abandon explicite ; un abandon n'ouvre pas la langue
    Contexte connecte;
    connecte.wifi_connecte = true;
    Contexte abandon;
    abandon.wifi_abandonne = true;
    VERIFIER(peut_continuer(Etape::Wifi, connecte, Mode::Premiere));
    VERIFIER(peut_continuer(Etape::Wifi, abandon, Mode::Premiere));
    VERIFIER(!peut_continuer(Etape::Langue, abandon, Mode::Premiere));
    VERIFIER(!peut_continuer(Etape::Wifi, langue, Mode::Premiere));
    return 0;
}

constexpr int en_mode_refaire_la_langue_est_deja_en_place() {
    Contexte rien;
    VERIFIER(peut_continuer(Etape::Langue, rien, Mode::Refaire));
    VERIFIER(!peut_continuer(Etape::Wifi, rien, Mode::Refaire)); // le Wi-Fi se confirme : connecté ou abandonné
    return 0;
}

constexpr int deux_etapes_sont_facultatives() {
    int n = 0;
    for (Etape e : TOUTES_LES_ETAPES)
        n += facultative(e) ? 1 : 0;
    VERIFIER(n == 2 && facultative(Etape::Assistant) && facultative(Etape::Telephone));
    VERIFIER(!facultative(Etape::Langue) && !facultative(Etape::Wifi) && !facultative(Etape::Recapitulatif));
    return 0;
}

constexpr int l_assistant_suit_l_ecran_seulement_a_la_premiere_installation_et_s_il_etait_pareil() {
    // première installation, tout en français, on passe à l'allemand : l'assistant suit
    VERIFIER(langue_assistant_apres_choix(Mode::Premiere, Langue::Fr, Langue::Fr, Langue::De) == Langue::De);
    // l'assistant avait déjà été réglé autrement : on n'y touche pas
    VERIFIER(langue_assistant_apres_choix(Mode::Premiere, Langue::Fr, Langue::En, Langue::De) == Langue::En);
    // refaire : jamais
    VERIFIER(langue_assistant_apres_choix(Mode::Refaire, Langue::Fr, Langue::Fr, Langue::De) == Langue::Fr);
    // on revient en arrière sur le choix : l'assistant suit encore (il valait l'ancienne langue de l'écran)
    VERIFIER(langue_assistant_apres_choix(Mode::Premiere, Langue::De, Langue::De, Langue::Tr) == Langue::Tr);
    return 0;
}

constexpr int refaire_et_reinitialiser_sont_deux_transitions_distinctes() {
    const Progression terminee{Etape::Langue, true, Mode::Premiere};
    const Progression r = refaire(terminee);
    VERIFIER(r.etape == Etape::Langue && !r.terminee && r.mode == Mode::Refaire);
    const Progression i = apres_reinitialisation();
    VERIFIER(i.etape == Etape::Langue && !i.terminee && i.mode == Mode::Premiere);
    VERIFIER(!(r == i)); // jamais l'une pour l'autre
    VERIFIER(sortie_libre(Mode::Refaire) && !sortie_libre(Mode::Premiere));
    // quitter un « refaire » : la progression redevient terminée, en gardant le mode pour la trace
    VERIFIER(terminer(r).terminee);
    // refaire depuis n'importe quel état de la progression repart de la première étape
    VERIFIER(refaire(Progression{Etape::Assistant, false, Mode::Premiere}).etape == Etape::Langue);
    return 0;
}

constexpr int un_redemarrage_au_milieu_reprend_a_la_meme_etape() {
    for (Etape e : TOUTES_LES_ETAPES) {
        const Progression avant{e, false, Mode::Refaire};
        const Progression apres = decoder(encoder(avant)); // ce qu'on relit après le redémarrage
        VERIFIER(apres.etape == e && !apres.terminee && apres.mode == Mode::Refaire);
        VERIFIER(installation_a_montrer(apres));
    }
    return 0;
}

constexpr int la_reinitialisation_demande_deux_gestes() {
    EtatConfirmation e;
    ResultatConfirmation r = confirmer(e, Geste::Toucher, 1000);
    VERIFIER(!r.executer && r.etat.etat == Confirmation::Armee); // premier geste : on arme, on n'efface rien
    r = confirmer(r.etat, Geste::Toucher, 2500);
    VERIFIER(r.executer &&
             r.etat.etat == Confirmation::Repos); // second geste dans le délai : on exécute, une seule fois
    r = confirmer(r.etat, Geste::Toucher, 2600);
    VERIFIER(!r.executer && r.etat.etat == Confirmation::Armee); // un troisième geste ré-arme sans exécuter
    return 0;
}

constexpr int la_confirmation_retombe_seule_ou_sur_annuler() {
    // le délai est court (quelques secondes) : assez pour un second geste voulu, trop peu pour qu'une poche le fasse par hasard
    VERIFIER(DELAI_CONFIRMATION_MS >= 3000 && DELAI_CONFIRMATION_MS <= 10000);
    EtatConfirmation armee = confirmer({}, Geste::Toucher, 1000).etat;
    // le temps passe : avant le délai rien ne change, au délai l'attente retombe
    VERIFIER(confirmer(armee, Geste::Temps, 1000 + DELAI_CONFIRMATION_MS - 1).etat.etat == Confirmation::Armee);
    VERIFIER(confirmer(armee, Geste::Temps, 1000 + DELAI_CONFIRMATION_MS).etat.etat == Confirmation::Repos);
    // un second toucher APRÈS le délai ne confirme pas : il ré-arme
    ResultatConfirmation tard = confirmer(armee, Geste::Toucher, 1000 + DELAI_CONFIRMATION_MS);
    VERIFIER(!tard.executer && tard.etat.etat == Confirmation::Armee &&
             tard.etat.depuis_ms == 1000 + DELAI_CONFIRMATION_MS);
    // juste avant la fin du délai : il confirme
    VERIFIER(confirmer(armee, Geste::Toucher, 1000 + DELAI_CONFIRMATION_MS - 1).executer);
    // annuler désarme, y compris au repos
    VERIFIER(confirmer(armee, Geste::Annuler, 1500).etat.etat == Confirmation::Repos &&
             !confirmer(armee, Geste::Annuler, 1500).executer);
    VERIFIER(confirmer({}, Geste::Annuler, 1500).etat.etat == Confirmation::Repos);
    // le temps qui passe au repos ne fait rien
    VERIFIER(confirmer({}, Geste::Temps, 99999).etat.etat == Confirmation::Repos &&
             !confirmer({}, Geste::Temps, 99999).executer);
    // une horloge qui recule ne confirme pas
    VERIFIER(!confirmer(armee, Geste::Toucher, 500).executer);
    return 0;
}

// ---- changement de Wi-Fi avec retour arrière ----------------------------------------------------------------------------

constexpr int un_essai_qui_aboutit_est_reussi() {
    wifi::Changement c = wifi::demarrer(1000, true);
    VERIFIER(c.etat == wifi::Etat::Connexion && c.avait_un_reseau && !wifi::termine(c));
    c = wifi::sur_resultat(c, wifi::Resultat::Connecte);
    VERIFIER(c.etat == wifi::Etat::Reussi && c.cause == wifi::Cause::Aucune && wifi::termine(c));
    return 0;
}

constexpr int chaque_resultat_d_echec_donne_sa_cause() {
    VERIFIER(wifi::sur_resultat(wifi::demarrer(0, true), wifi::Resultat::MotDePasseRefuse).cause ==
             wifi::Cause::MotDePasse);
    VERIFIER(wifi::sur_resultat(wifi::demarrer(0, true), wifi::Resultat::ReseauIntrouvable).cause ==
             wifi::Cause::Introuvable);
    VERIFIER(wifi::sur_resultat(wifi::demarrer(0, true), wifi::Resultat::Autre).cause == wifi::Cause::Autre);
    for (wifi::Resultat r :
         {wifi::Resultat::MotDePasseRefuse, wifi::Resultat::ReseauIntrouvable, wifi::Resultat::Autre}) {
        const wifi::Changement c = wifi::sur_resultat(wifi::demarrer(0, false), r);
        VERIFIER(c.etat == wifi::Etat::Echec && !c.avait_un_reseau && wifi::termine(c));
    }
    return 0;
}

constexpr int sans_reponse_dans_le_delai_c_est_un_echec_et_non_un_blocage() {
    const wifi::Changement c = wifi::demarrer(10000, true);
    VERIFIER(wifi::sur_temps(c, 10000 + wifi::DELAI_CONNEXION_MS - 1).etat == wifi::Etat::Connexion);
    const wifi::Changement f = wifi::sur_temps(c, 10000 + wifi::DELAI_CONNEXION_MS);
    VERIFIER(f.etat == wifi::Etat::Echec && f.cause == wifi::Cause::Delai && f.avait_un_reseau);
    // une horloge qui recule est lue comme un délai écoulé
    VERIFIER(wifi::sur_temps(c, 5000).etat == wifi::Etat::Echec);
    VERIFIER(wifi::DELAI_CONNEXION_MS == 30000); // valeur de départ de l'ADR-0007 (à régler à la mesure)
    return 0;
}

constexpr int un_resultat_tardif_ne_change_rien() {
    wifi::Changement f = wifi::sur_temps(wifi::demarrer(0, true), wifi::DELAI_CONNEXION_MS);
    VERIFIER(f.etat == wifi::Etat::Echec);
    f = wifi::sur_resultat(f, wifi::Resultat::Connecte); // arrive après l'abandon
    VERIFIER(f.etat == wifi::Etat::Echec && f.cause == wifi::Cause::Delai);
    // et deux résultats d'affilée : le premier gagne
    wifi::Changement c = wifi::sur_resultat(wifi::demarrer(0, true), wifi::Resultat::Connecte);
    c = wifi::sur_resultat(c, wifi::Resultat::MotDePasseRefuse);
    VERIFIER(c.etat == wifi::Etat::Reussi);
    // le temps qui passe après la fin ne change rien
    VERIFIER(wifi::sur_temps(c, 999999).etat == wifi::Etat::Reussi);
    VERIFIER(wifi::sur_temps(wifi::Changement{}, 999999).etat == wifi::Etat::Inactif);
    return 0;
}

constexpr int le_compte_a_rebours_descend_de_30_a_0() {
    const wifi::Changement c = wifi::demarrer(1000, false);
    VERIFIER(wifi::secondes_restantes(c, 1000) == 30);
    VERIFIER(wifi::secondes_restantes(c, 1001) == 30);
    VERIFIER(wifi::secondes_restantes(c, 2000) == 29);
    VERIFIER(wifi::secondes_restantes(c, 30999) == 1);
    VERIFIER(wifi::secondes_restantes(c, 31000) == 0);
    VERIFIER(wifi::secondes_restantes(c, 999999) == 0);
    VERIFIER(wifi::secondes_restantes(c, 5) == 0); // horloge reculée
    VERIFIER(wifi::secondes_restantes(wifi::sur_resultat(c, wifi::Resultat::Connecte), 1000) ==
             0); // plus d'essai en cours
    return 0;
}

// ---- marqueurs nommés ---------------------------------------------------------------------------------------------------

constexpr int les_marqueurs_nommes_sont_remplaces_dans_n_importe_quel_ordre() {
    char sortie[96] = {};
    const texte::Parametre p[] = {{"ssid", "Maison"}, {"n", "30"}};
    const std::size_t n = texte::remplir("Connecté à « {ssid} » ({n} s).", p, 2, sortie, sizeof sortie);
    VERIFIER(std::string_view(sortie, n) == "Connecté à « Maison » (30 s).");
    VERIFIER(sortie[n] == '\0');
    // les traductions peuvent permuter les marqueurs
    const std::size_t m = texte::remplir("{n} s pour « {ssid} »", p, 2, sortie, sizeof sortie);
    VERIFIER(std::string_view(sortie, m) == "30 s pour « Maison »");
    // un marqueur inconnu reste visible, un marqueur répété est remplacé deux fois
    const std::size_t k = texte::remplir("{inconnu} {n}{n} {", p, 2, sortie, sizeof sortie);
    VERIFIER(std::string_view(sortie, k) == "{inconnu} 3030 {");
    VERIFIER(texte::remplir("", p, 2, sortie, sizeof sortie) == 0 && sortie[0] == '\0');
    // deux noms qui commencent par la même lettre ne se confondent pas
    const texte::Parametre q[] = {{"ssid", "Maison"}, {"signal", "4"}};
    const std::size_t r = texte::remplir("{signal}/{ssid}", q, 2, sortie, sizeof sortie);
    VERIFIER(std::string_view(sortie, r) == "4/Maison");
    return 0;
}

constexpr int le_remplacement_ne_deborde_jamais_et_ne_coupe_pas_un_caractere() {
    char petit[8] = {};
    const texte::Parametre p[] = {{"v", "ééééé"}};
    const std::size_t n = texte::remplir("{v}", p, 1, petit, sizeof petit);
    // 7 octets utiles au plus : 3 « é » entiers (6 octets), jamais un demi-caractère
    VERIFIER(n == 6 && petit[6] == '\0');
    VERIFIER(std::string_view(petit, n) == "ééé");
    char un[1] = {'x'};
    VERIFIER(texte::remplir("abc", p, 1, un, 1) == 0 && un[0] == '\0');
    VERIFIER(texte::remplir("abc", p, 1, nullptr, 0) == 0);
    char trois[3] = {};
    VERIFIER(texte::remplir("abcdef", p, 1, trois, 3) == 2 && std::string_view(trois, 2) == "ab");
    // un modèle qui contient lui-même un caractère multi-octet à la coupure
    char quatre[4] = {};
    VERIFIER(texte::remplir("aaéé", p, 1, quatre, 4) == 2 &&
             std::string_view(quatre, 2) == "aa"); // « é » (2 octets) ne tient pas en entier
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(l_octet_garde_se_relit_a_l_identique)                                                                            \
    X(un_octet_abime_ne_passe_jamais_pour_une_installation_terminee)                                                   \
    X(le_drapeau_termine_decide_de_l_ecran_au_demarrage)                                                               \
    X(les_sept_etapes_se_parcourent_dans_l_ordre)                                                                      \
    X(reculer_est_possible_partout_sauf_a_la_premiere_etape)                                                           \
    X(seules_la_langue_et_le_wifi_bloquent_la_suite)                                                                   \
    X(en_mode_refaire_la_langue_est_deja_en_place)                                                                     \
    X(deux_etapes_sont_facultatives)                                                                                   \
    X(l_assistant_suit_l_ecran_seulement_a_la_premiere_installation_et_s_il_etait_pareil)                              \
    X(refaire_et_reinitialiser_sont_deux_transitions_distinctes)                                                       \
    X(un_redemarrage_au_milieu_reprend_a_la_meme_etape)                                                                \
    X(la_reinitialisation_demande_deux_gestes)                                                                         \
    X(la_confirmation_retombe_seule_ou_sur_annuler)                                                                    \
    X(un_essai_qui_aboutit_est_reussi)                                                                                 \
    X(chaque_resultat_d_echec_donne_sa_cause)                                                                          \
    X(sans_reponse_dans_le_delai_c_est_un_echec_et_non_un_blocage)                                                     \
    X(un_resultat_tardif_ne_change_rien)                                                                               \
    X(le_compte_a_rebours_descend_de_30_a_0)                                                                           \
    X(les_marqueurs_nommes_sont_remplaces_dans_n_importe_quel_ordre)                                                   \
    X(le_remplacement_ne_deborde_jamais_et_ne_coupe_pas_un_caractere)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
