// Tests du noyau pur : ce que la roue d'appui long décide sans dessiner (roue_commande.hpp, ADR-0019) : l'écran de la roue d'après la
// taille de l'écran, la demande faite à la géométrie, le dépliement d'une famille (la roue reposée quand son second anneau ne tient
// pas) et la décision d'un toucher. La géométrie elle-même est testée dans test_roue_geometrie.cpp et test_roue_balayage.cpp ; ici
// elle sert de règle : les invariants vérifiés (dans l'écran, sans recouvrement) sont réécrits à part, ils ne passent pas par
// `anneau_complet`.
#include "ecran/roue_commande.hpp"
#include "mini.hpp"

#include <initializer_list>

using namespace ecran::roue;

namespace {

constexpr Mesures M5 = mesures(ECRAN_TAB5);
constexpr Mesures M7 = mesures(ECRAN_7B);

constexpr int carre(int v) { return v * v; }

// Tous les centres d'un anneau sont dans l'écran à `marge` près, et deux boutons ne se recouvrent pas.
constexpr bool anneau_dans_l_ecran_sans_recouvrement(const Mesures &m, const Anneau &a, int diametre) {
    for (int i = 0; i < a.n; i++) {
        const int r = diametre / 2;
        if (a.bouton[i].x - r < m.marge || a.bouton[i].x + r > m.largeur - m.marge || a.bouton[i].y - r < m.marge ||
            a.bouton[i].y + r > m.hauteur - m.marge) {
            return false;
        }
        for (int j = i + 1; j < a.n; j++) {
            if (carre(a.bouton[i].x - a.bouton[j].x) + carre(a.bouton[i].y - a.bouton[j].y) < carre(diametre)) {
                return false;
            }
        }
    }
    return true;
}

// ---- 1. L'écran de la roue ------------------------------------------------------------------------------------------------------

constexpr int l_ecran_du_tab5_est_a_l_echelle_1000() {
    constexpr Ecran e = ecran_de(1280, 720);
    VERIFIER(e.largeur == 1280 && e.hauteur == 720 && e.echelle_pm == 1000);
    VERIFIER(e.echelle_pm == ECRAN_TAB5.echelle_pm);
    return 0;
}

constexpr int l_ecran_de_la_7b_est_a_l_echelle_0_8() {
    constexpr Ecran e = ecran_de(1024, 600);
    VERIFIER(e.largeur == 1024 && e.hauteur == 600 && e.echelle_pm == 800);
    VERIFIER(e.echelle_pm == ECRAN_7B.echelle_pm); // 1024 / 1280 = 0,8 et 600 / 720 = 0,833 : le plus petit rapport
    return 0;
}

constexpr int le_portrait_prend_le_plus_petit_rapport_lui_aussi() {
    VERIFIER(ecran_de(720, 1280).echelle_pm == 1000);
    VERIFIER(ecran_de(600, 1024).echelle_pm == 800); // 600 / 720 = 0,833 et 1024 / 1280 = 0,8
    VERIFIER(ecran_de(600, 1024).largeur == 600 && ecran_de(600, 1024).hauteur == 1024);
    return 0;
}

constexpr int un_ecran_plus_grand_agrandit_la_roue() {
    VERIFIER(ecran_de(1920, 1080).echelle_pm == 1500);
    // le rapport serré gagne : large mais peu haut
    VERIFIER(ecran_de(2560, 720).echelle_pm == 1000);
    return 0;
}

constexpr int un_ecran_sans_taille_garde_les_mesures_du_tab5() {
    VERIFIER(ecran_de(0, 0).echelle_pm == 1000 && ecran_de(0, 0).largeur == TAB5_LARGEUR);
    VERIFIER(ecran_de(-5, 720).echelle_pm == 1000 && ecran_de(1280, 0).hauteur == TAB5_HAUTEUR);
    return 0;
}

constexpr int le_seuil_de_glissement_suit_l_echelle() {
    VERIFIER(glissement(ECRAN_TAB5) == 24);
    VERIFIER(glissement(ECRAN_7B) == 19); // 24 x 0,8 = 19,2
    VERIFIER(glissement(Ecran{1920, 1080, 1500}) == 36);
    return 0;
}

constexpr int le_glissement_se_mesure_a_vol_d_oiseau() {
    VERIFIER(!a_glisse(Point{100, 100}, Point{100, 100}, 24));
    VERIFIER(!a_glisse(Point{100, 100}, Point{124, 100}, 24)); // exactement le seuil : pas un glissement
    VERIFIER(a_glisse(Point{100, 100}, Point{125, 100}, 24));
    VERIFIER(a_glisse(Point{100, 100}, Point{100, 75}, 24));
    VERIFIER(!a_glisse(Point{100, 100}, Point{116, 117}, 24)); // 16² + 17² = 545 ≤ 576
    VERIFIER(a_glisse(Point{100, 100}, Point{117, 117}, 24));  // 17² + 17² = 578 > 576
    VERIFIER(a_glisse(Point{100, 100}, Point{76, 123}, 24) == (carre(24) + carre(23) > carre(24)));
    return 0;
}

// ---- 2. La demande ----------------------------------------------------------------------------------------------------------------

constexpr int la_demande_sans_exiger_les_seconds_anneaux() {
    constexpr Forme f[3] = {{80, 0}, {70, 5}, {96, 8}};
    constexpr Demande d = demande(f, 3, 110, Exigence::PremierAnneauSeul);
    VERIFIER(d.n == 3 && d.mot_choix == 110);
    VERIFIER(d.mot[0] == 80 && d.mot[1] == 70 && d.mot[2] == 96);
    VERIFIER(d.choix[0] == 0 && d.choix[1] == 0 && d.choix[2] == 0); // aucun second anneau n'est exigé à l'ouverture
    return 0;
}

constexpr int la_demande_exigeant_tous_les_anneaux() {
    constexpr Forme f[3] = {{80, 0}, {70, 5}, {96, 12}};
    constexpr Demande d = demande(f, 3, 110, Exigence::TousLesAnneaux);
    VERIFIER(d.n == 3 && d.mot_choix == 110);
    VERIFIER(d.choix[0] == 0 && d.choix[1] == 5);
    VERIFIER(d.choix[2] == CHOIX_MAX); // 12 choix demandés : bornés aux 8 du second anneau
    return 0;
}

constexpr int la_demande_borne_le_nombre_de_boutons_et_accepte_rien() {
    constexpr Forme f[8] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}};
    VERIFIER(demande(f, 8, 0, Exigence::PremierAnneauSeul).n == BOUTONS_MAX);
    VERIFIER(demande(f, -3, 0, Exigence::PremierAnneauSeul).n == 0);
    constexpr Demande vide = demande(nullptr, 4, 0, Exigence::TousLesAnneaux);
    VERIFIER(vide.n == 4 && vide.mot[0] == 0 && vide.choix[3] == 0);
    VERIFIER(demande(f, 6, 0, Exigence::PremierAnneauSeul).mot[5] == 6);
    return 0;
}

constexpr int le_reglage_retenu_n_exige_que_le_premier_anneau() {
    // ADR-0019 : la valeur retenue pour l'essai sur la carte. Si Axel tranche autrement, ce test et la constante changent ensemble.
    VERIFIER(EXIGENCE_A_L_OUVERTURE == Exigence::PremierAnneauSeul);
    VERIFIER(GLISSEMENT_PX == 24 && INACTIVITE_MS == 15000);
    return 0;
}

// ---- le dépliement -----------------------------------------------------------------------------------------------------------------

// Trois boutons dont le deuxième (rang 1) est une famille de six choix, des mots de 90 px, des mots de choix de 100 px.
constexpr Forme FORMES[3] = {{90, 0}, {90, 6}, {90, 0}};
constexpr int LARGEURS_CHOIX[8] = {100, 100, 100, 100, 100, 100, 100, 100};

constexpr int la_roue_du_centre_deplie_sans_bouger() {
    constexpr Demande d = demande(FORMES, 3, 100, Exigence::PremierAnneauSeul);
    constexpr Roue r = ouvrir(M5, Point{640, 400}, d);
    VERIFIER(r.tient && !r.moyeu_deplace);
    constexpr Depliage p = deplier_famille(M5, r, d, 1, 6, LARGEURS_CHOIX);
    VERIFIER(p.tient && !p.reposee);
    VERIFIER(p.roue.moyeu == r.moyeu && p.roue.appui == r.appui);
    VERIFIER(p.second.n == 6);
    VERIFIER(anneau_dans_l_ecran_sans_recouvrement(M5, p.second, M5.diametre_2));
    return 0;
}

constexpr int dans_un_coin_la_roue_se_repose_pour_que_le_second_anneau_tienne() {
    constexpr Demande d = demande(FORMES, 3, 100, Exigence::PremierAnneauSeul);
    constexpr Point coin{60, 60};
    constexpr Roue r = ouvrir(M5, coin, d);
    VERIFIER(r.tient); // le premier anneau seul tient (avec un moyeu peu déplacé)
    constexpr Depliage p = deplier_famille(M5, r, d, 1, 6, LARGEURS_CHOIX);
    VERIFIER(p.reposee && p.tient);
    VERIFIER(!(p.roue.moyeu == r.moyeu));
    VERIFIER(p.roue.appui == coin);        // le point d'appui d'origine est gardé
    VERIFIER(p.roue.moyeu.x >= r.moyeu.x); // le moyeu avance vers le centre de l'écran, jamais en s'en éloignant
    VERIFIER(p.roue.moyeu.y >= r.moyeu.y);
    VERIFIER(anneau_dans_l_ecran_sans_recouvrement(M5, p.second, M5.diametre_2));
    VERIFIER(anneau_dans_l_ecran_sans_recouvrement(M5, p.roue.premier, M5.diametre_1)); // et le premier tient toujours
    VERIFIER(p.roue.tient);
    return 0;
}

constexpr int exiger_tous_les_anneaux_a_l_ouverture_evite_la_repose() {
    constexpr Demande d = demande(FORMES, 3, 100, Exigence::TousLesAnneaux);
    constexpr Roue r = ouvrir(M5, Point{60, 60}, d);
    VERIFIER(r.tient);
    constexpr Depliage p = deplier_famille(M5, r, d, 1, 6, LARGEURS_CHOIX);
    VERIFIER(p.tient && !p.reposee && p.roue.moyeu == r.moyeu);
    return 0;
}

constexpr int la_roue_reposee_garde_les_autres_exigences() {
    // une autre famille déjà exigée à l'ouverture reste exigée après la repose
    constexpr Forme formes[3] = {{90, 6}, {90, 6}, {90, 0}};
    constexpr Demande d = demande(formes, 3, 100, Exigence::PremierAnneauSeul);
    constexpr Roue r = ouvrir(M5, Point{60, 60}, d);
    constexpr Depliage p = deplier_famille(M5, r, d, 0, 6, LARGEURS_CHOIX);
    VERIFIER(p.tient);
    VERIFIER(anneau_dans_l_ecran_sans_recouvrement(M5, p.second, M5.diametre_2));
    return 0;
}

constexpr int une_famille_inexistante_ne_deplie_rien_et_ne_repose_pas() {
    constexpr Demande d = demande(FORMES, 3, 100, Exigence::PremierAnneauSeul);
    constexpr Roue r = ouvrir(M5, Point{60, 60}, d);
    constexpr Depliage a = deplier_famille(M5, r, d, 5, 6, LARGEURS_CHOIX);
    VERIFIER(a.second.n == 0 && !a.reposee && a.roue.moyeu == r.moyeu);
    constexpr Depliage b = deplier_famille(M5, r, d, -1, 6, LARGEURS_CHOIX);
    VERIFIER(b.second.n == 0 && !b.reposee && b.roue.moyeu == r.moyeu);
    return 0;
}

constexpr int la_7b_deplie_aussi_dans_un_coin() {
    constexpr Demande d = demande(FORMES, 3, 80, Exigence::PremierAnneauSeul);
    constexpr int mots[8] = {80, 80, 80, 80, 80, 80, 80, 80};
    constexpr Roue r = ouvrir(M7, Point{50, 50}, d);
    constexpr Depliage p = deplier_famille(M7, r, d, 1, 6, mots);
    VERIFIER(p.tient && p.reposee);
    VERIFIER(anneau_dans_l_ecran_sans_recouvrement(M7, p.second, M7.diametre_2));
    return 0;
}

constexpr int si_rien_ne_tient_la_roue_ne_bouge_pas_et_le_dit() {
    // Un écran synthétique, plus étroit que toute carte (480 x 520, mesures du Tab5 à l'échelle 1000 ; relevé par essai : le premier
    // anneau y tient, aucun second anneau de 8 choix ne tient). Le doigt est au centre : la roue y est déjà, aucune autre position
    // ne vaut mieux, elle reste où elle est.
    constexpr Ecran trop_petit{480, 520, 1000};
    constexpr Mesures m = mesures(trop_petit);
    constexpr Forme formes[3] = {{90, 0}, {90, 8}, {90, 0}};
    constexpr Demande d = demande(formes, 3, 100, Exigence::PremierAnneauSeul);
    constexpr Roue r = ouvrir(m, Point{240, 260}, d);
    VERIFIER(r.tient);
    constexpr Depliage p = deplier_famille(m, r, d, 1, 8, LARGEURS_CHOIX);
    VERIFIER(!p.tient); // il le dit
    VERIFIER(!p.reposee && p.roue.moyeu == r.moyeu);
    VERIFIER(p.second.n == 8); // et montre quand même ce qu'il a : à l'appelant de décider
    return 0;
}

// ---- 3. Ce que fait un toucher -----------------------------------------------------------------------------------------------------

constexpr Demande D3 = demande(FORMES, 3, 100, Exigence::PremierAnneauSeul);
constexpr Roue R3 = ouvrir(M5, Point{640, 400}, D3);
constexpr Depliage P3 = deplier_famille(M5, R3, D3, 1, 6, LARGEURS_CHOIX);
constexpr bool FAMILLES3[3] = {false, true, false};
constexpr Anneau SANS_SECOND{};
constexpr int SEUIL = 24;

constexpr Point centre_premier(int i) { return Point{R3.premier.bouton[i].x, R3.premier.bouton[i].y}; }
constexpr Point centre_choix(int j) { return Point{P3.second.bouton[j].x, P3.second.bouton[j].y}; }

constexpr Decision toucher(Point appui, Point relache, int famille = -1, bool annule = false) {
    return decider(M5, R3, famille, famille >= 0 ? P3.second : SANS_SECOND, FAMILLES3, appui, relache, SEUIL, annule);
}

constexpr int toucher_un_bouton_le_choisit() {
    for (int i : {0, 2}) {
        const Decision d = toucher(centre_premier(i), centre_premier(i));
        VERIFIER(d.geste == Geste::Choisir && d.rang == i);
    }
    return 0;
}

constexpr int toucher_une_famille_la_deplie() {
    const Decision d = toucher(centre_premier(1), centre_premier(1));
    VERIFIER(d.geste == Geste::Deplier && d.rang == 1);
    return 0;
}

constexpr int toucher_la_famille_depliee_la_replie() {
    const Decision d = toucher(centre_premier(1), centre_premier(1), 1);
    VERIFIER(d.geste == Geste::Replier);
    return 0;
}

constexpr int toucher_un_autre_bouton_quand_une_famille_est_depliee() {
    // une commande part (la roue se ferme)
    const Decision a = toucher(centre_premier(2), centre_premier(2), 1);
    VERIFIER(a.geste == Geste::Choisir && a.rang == 2);
    // une autre famille remplacerait la première : ici le seul bouton famille est le 1, vérifié avec une table à deux familles
    constexpr bool deux[3] = {true, true, false};
    const Decision b = decider(M5, R3, 1, P3.second, deux, centre_premier(0), centre_premier(0), SEUIL, false);
    VERIFIER(b.geste == Geste::Deplier && b.rang == 0);
    return 0;
}

constexpr int toucher_un_choix_le_choisit() {
    for (int j = 0; j < P3.second.n; j++) {
        const Decision d = toucher(centre_choix(j), centre_choix(j), 1);
        VERIFIER(d.geste == Geste::ChoisirChoix && d.rang == j && d.famille == 1);
    }
    return 0;
}

constexpr int un_choix_n_existe_que_quand_la_famille_est_depliee() {
    // le même point, second anneau replié : ce n'est plus un bouton, on ferme
    const Decision d = toucher(centre_choix(2), centre_choix(2), -1);
    VERIFIER(d.geste == Geste::Fermer);
    return 0;
}

constexpr int toucher_hors_des_boutons_ferme_ou_replie() {
    const Point vide{40, 40}; // loin de tout bouton
    VERIFIER(bouton_sous(M5, R3.premier, M5.diametre_1, vide) < 0);
    VERIFIER(toucher(vide, vide).geste == Geste::Fermer);
    VERIFIER(toucher(vide, vide, 1).geste == Geste::Replier);
    // le moyeu est « hors des boutons » aussi
    VERIFIER(toucher(R3.moyeu, R3.moyeu).geste == Geste::Fermer);
    VERIFIER(toucher(R3.moyeu, R3.moyeu, 1).geste == Geste::Replier);
    return 0;
}

constexpr int un_glissement_ne_fait_rien() {
    const Point vide{40, 40};
    const Point loin{40 + SEUIL + 1, 40};
    VERIFIER(toucher(vide, loin).geste == Geste::Rien);
    VERIFIER(toucher(vide, loin, 1).geste == Geste::Rien);
    // un glissement qui démarre sur un bouton et finit sur lui, mais trop long : rien non plus
    const Point sur_bouton = centre_premier(0);
    const Point bouge{sur_bouton.x - 20,
                      sur_bouton.y + 15}; // 25 px : au-delà du seuil de 24, encore dans la zone de toucher du bouton
    VERIFIER(a_glisse(sur_bouton, bouge, SEUIL));
    VERIFIER(toucher(sur_bouton, bouge).geste == Geste::Rien);
    // exactement le seuil : un toucher
    const Point juste{sur_bouton.x, sur_bouton.y + SEUIL};
    VERIFIER(!a_glisse(sur_bouton, juste, SEUIL));
    return 0;
}

constexpr int un_appui_long_sur_la_roue_ne_fait_rien() {
    VERIFIER(toucher(centre_premier(0), centre_premier(0), -1, true).geste == Geste::Rien);
    VERIFIER(toucher(Point{40, 40}, Point{40, 40}, -1, true).geste == Geste::Rien);
    VERIFIER(toucher(centre_choix(0), centre_choix(0), 1, true).geste == Geste::Rien);
    return 0;
}

constexpr int un_doigt_pose_sur_un_bouton_et_leve_sur_un_autre_ne_fait_rien() {
    // Deux boutons sont toujours à plus que le seuil de glissement l'un de l'autre : ces cas-là s'arrêtent au glissement.
    VERIFIER(toucher(centre_premier(0), centre_premier(2)).geste == Geste::Rien);
    VERIFIER(toucher(centre_choix(0), centre_choix(1), 1).geste == Geste::Rien);
    // Seul un seuil démesuré laisse la règle « posé ET levé sur le même bouton » en jeu (3000 px : plus que la diagonale d'un écran).
    constexpr int ENORME = 3000;
    VERIFIER(!a_glisse(centre_premier(0), centre_premier(2), ENORME));
    VERIFIER(decider(M5, R3, -1, SANS_SECOND, FAMILLES3, centre_premier(0), centre_premier(2), ENORME, false).geste ==
             Geste::Rien);
    VERIFIER(decider(M5, R3, 1, P3.second, FAMILLES3, centre_choix(0), centre_choix(1), ENORME, false).geste ==
             Geste::Rien);
    return 0;
}

constexpr int un_doigt_pose_sur_le_bord_d_un_bouton_et_leve_juste_dehors_ne_fait_rien() {
    // Le cas qui arrive vraiment : 1 px de déplacement (pas un glissement), mais le doigt n'est pas levé sur le bouton.
    const int rt1 = rayon_tactile(M5, M5.diametre_1);
    const Point c = centre_premier(0);
    const Point bord{c.x, c.y - rt1};
    const Point hors{c.x, c.y - rt1 - 1};
    VERIFIER(bouton_sous(M5, R3.premier, M5.diametre_1, bord) == 0);
    VERIFIER(bouton_sous(M5, R3.premier, M5.diametre_1, hors) < 0);
    VERIFIER(toucher(bord, hors).geste == Geste::Rien);
    VERIFIER(toucher(hors, bord).geste == Geste::Rien);
    // et au second anneau
    const int rt2 = rayon_tactile(M5, M5.diametre_2);
    const Point d = centre_choix(0);
    const Point bord2{d.x, d.y - rt2};
    const Point hors2{d.x, d.y - rt2 - 1};
    VERIFIER(bouton_sous(M5, P3.second, M5.diametre_2, bord2) == 0);
    VERIFIER(bouton_sous(M5, P3.second, M5.diametre_2, hors2) < 0);
    VERIFIER(toucher(bord2, hors2, 1).geste == Geste::Rien);
    VERIFIER(toucher(hors2, bord2, 1).geste == Geste::Rien);
    return 0;
}

constexpr int la_zone_de_toucher_du_second_anneau_est_celle_de_la_geometrie() {
    // Les deux anneaux ont aujourd'hui le même diamètre (72) : ce test ne distingue pas l'un de l'autre, il vérifie que la zone du
    // second anneau est bien le disque du bouton élargi de la marge, au pixel.
    const int rt2 = rayon_tactile(M5, M5.diametre_2);
    const Point d = centre_choix(0);
    const Point dedans{d.x, d.y - rt2};
    const Point dehors{d.x, d.y - rt2 - 1};
    VERIFIER(toucher(dedans, dedans, 1).geste == Geste::ChoisirChoix);
    VERIFIER(toucher(dedans, dedans, 1).rang == 0);
    VERIFIER(toucher(dehors, dehors, 1).geste == Geste::Replier);
    return 0;
}

constexpr int la_zone_de_toucher_est_celle_de_la_geometrie() {
    // un pixel juste dedans (distance = rayon de toucher), un pixel juste dehors : la décision suit bouton_sous
    const Point c = centre_premier(0);
    const int rt = rayon_tactile(M5, M5.diametre_1);
    VERIFIER(rt == 44);
    const Point dedans{c.x, c.y - rt};
    const Point dehors{c.x, c.y - rt - 1};
    VERIFIER(bouton_sous(M5, R3.premier, M5.diametre_1, dedans) == 0);
    VERIFIER(toucher(dedans, dedans).geste == Geste::Choisir);
    VERIFIER(bouton_sous(M5, R3.premier, M5.diametre_1, dehors) < 0);
    VERIFIER(toucher(dehors, dehors).geste == Geste::Fermer);
    return 0;
}

constexpr int la_roue_sans_table_de_familles_n_a_que_des_commandes() {
    const Decision d = decider(M5, R3, -1, SANS_SECOND, nullptr, centre_premier(1), centre_premier(1), SEUIL, false);
    VERIFIER(d.geste == Geste::Choisir && d.rang == 1);
    return 0;
}

constexpr int une_famille_hors_du_premier_anneau_n_ouvre_pas_de_second_anneau() {
    // `famille` hors de la roue (un rappel de l'interface qui se trompe) : traitée comme replié
    const Decision d = decider(M5, R3, 7, P3.second, FAMILLES3, centre_choix(0), centre_choix(0), SEUIL, false);
    VERIFIER(d.geste == Geste::Fermer);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(l_ecran_du_tab5_est_a_l_echelle_1000)                                                                            \
    X(l_ecran_de_la_7b_est_a_l_echelle_0_8)                                                                            \
    X(le_portrait_prend_le_plus_petit_rapport_lui_aussi)                                                               \
    X(un_ecran_plus_grand_agrandit_la_roue)                                                                            \
    X(un_ecran_sans_taille_garde_les_mesures_du_tab5)                                                                  \
    X(le_seuil_de_glissement_suit_l_echelle)                                                                           \
    X(le_glissement_se_mesure_a_vol_d_oiseau)                                                                          \
    X(la_demande_sans_exiger_les_seconds_anneaux)                                                                      \
    X(la_demande_exigeant_tous_les_anneaux)                                                                            \
    X(la_demande_borne_le_nombre_de_boutons_et_accepte_rien)                                                           \
    X(le_reglage_retenu_n_exige_que_le_premier_anneau)                                                                 \
    X(la_roue_du_centre_deplie_sans_bouger)                                                                            \
    X(dans_un_coin_la_roue_se_repose_pour_que_le_second_anneau_tienne)                                                 \
    X(exiger_tous_les_anneaux_a_l_ouverture_evite_la_repose)                                                           \
    X(la_roue_reposee_garde_les_autres_exigences)                                                                      \
    X(une_famille_inexistante_ne_deplie_rien_et_ne_repose_pas)                                                         \
    X(la_7b_deplie_aussi_dans_un_coin)                                                                                 \
    X(si_rien_ne_tient_la_roue_ne_bouge_pas_et_le_dit)                                                                 \
    X(toucher_un_bouton_le_choisit)                                                                                    \
    X(toucher_une_famille_la_deplie)                                                                                   \
    X(toucher_la_famille_depliee_la_replie)                                                                            \
    X(toucher_un_autre_bouton_quand_une_famille_est_depliee)                                                           \
    X(toucher_un_choix_le_choisit)                                                                                     \
    X(un_choix_n_existe_que_quand_la_famille_est_depliee)                                                              \
    X(toucher_hors_des_boutons_ferme_ou_replie)                                                                        \
    X(un_glissement_ne_fait_rien)                                                                                      \
    X(un_appui_long_sur_la_roue_ne_fait_rien)                                                                          \
    X(un_doigt_pose_sur_un_bouton_et_leve_sur_un_autre_ne_fait_rien)                                                   \
    X(un_doigt_pose_sur_le_bord_d_un_bouton_et_leve_juste_dehors_ne_fait_rien)                                         \
    X(la_zone_de_toucher_est_celle_de_la_geometrie)                                                                    \
    X(la_zone_de_toucher_du_second_anneau_est_celle_de_la_geometrie)                                                   \
    X(la_roue_sans_table_de_familles_n_a_que_des_commandes)                                                            \
    X(une_famille_hors_du_premier_anneau_n_ouvre_pas_de_second_anneau)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
