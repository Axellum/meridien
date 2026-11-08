// Tests du noyau pur : balayage de tout l'écran pour la roue d'appui long (roue_geometrie.hpp). Ce fichier n'est ÉVALUÉ qu'à
// l'exécution (programme lancé, avec AddressSanitizer et UBSan quand le PC les a) : des milliers d'appuis sur une grille, trop pour
// que le compilateur les évalue un par un. Les invariants sont écrits ici, indépendamment du noyau (rien n'est emprunté à anneau_tient).
// Les valeurs de référence du Tab5 et les cas isolés sont dans test_roue_geometrie.cpp.
//
// Demandes balayées (nombres de boutons, choix et largeurs de mots d'essai, pas de la police) :
//   - « courante » : 6 boutons, mots de 80 px, deux familles de 5 et 8 choix, mots de choix de 110 px ;
//   - « lourde » : 6 boutons, mots de 96 px, six familles de 8 choix, mots de choix de 110 px (le pire cas de l'ADR-0042).
#include "ecran/roue_geometrie.hpp"
#include "mini.hpp"

#include <cmath>
#include <initializer_list>

using namespace ecran::roue;

namespace {

int carre(int v) { return v * v; }

Demande demande_courante(int n) {
    Demande d;
    d.n = n;
    for (int i = 0; i < n; i++) {
        d.mot[i] = 80;
        d.choix[i] = i % 3 == 1 ? 5 : (i % 3 == 2 ? 8 : 0);
    }
    d.mot_choix = 110;
    return d;
}

Demande demande_lourde() {
    Demande d;
    d.n = 6;
    for (int i = 0; i < 6; i++) {
        d.mot[i] = 96;
        d.choix[i] = 8;
    }
    d.mot_choix = 110;
    return d;
}

// Les invariants d'un anneau, vus d'ici seulement.
bool anneau_correct(const Mesures &m, const Anneau &a, Point moyeu, int rayon, int diametre, int pas) {
    const int rb = diametre / 2;
    for (int i = 0; i < a.n; i++) {
        const Bouton &b = a.bouton[i];
        if (b.x - rb < m.marge || b.x + rb > m.largeur - m.marge || b.y - rb < m.marge ||
            b.y + rb > m.hauteur - m.marge) {
            return false;
        }
        const int d2 = carre(b.x - moyeu.x) + carre(b.y - moyeu.y);
        if (d2 < carre(rayon - 1) || d2 > carre(rayon + 1)) {
            return false;
        }
        if (i + 1 < a.n && b.angle - a.bouton[i + 1].angle != pas) {
            return false;
        }
        for (int j = i + 1; j < a.n; j++) {
            if (carre(b.x - a.bouton[j].x) + carre(b.y - a.bouton[j].y) < carre(diametre)) {
                return false;
            }
        }
        if (b.mot.largeur == 0) {
            continue;
        }
        const int gauche = b.mot.x - b.mot.largeur / 2;
        const int haut = b.mot.y - m.legende_h / 2;
        if (gauche < m.marge || gauche + b.mot.largeur > m.largeur - m.marge || haut < m.marge ||
            haut + m.legende_h > m.hauteur - m.marge) {
            return false;
        }
        for (int j = 0; j < a.n; j++) {
            const int px = a.bouton[j].x < gauche
                               ? gauche
                               : (a.bouton[j].x > gauche + b.mot.largeur ? gauche + b.mot.largeur : a.bouton[j].x);
            const int py =
                a.bouton[j].y < haut ? haut : (a.bouton[j].y > haut + m.legende_h ? haut + m.legende_h : a.bouton[j].y);
            if (carre(a.bouton[j].x - px) + carre(a.bouton[j].y - py) < carre(rb)) {
                return false;
            }
            const Mot &o = a.bouton[j].mot;
            if (j > i && o.largeur != 0) {
                const int g2 = o.x - o.largeur / 2;
                const int h2 = o.y - m.legende_h / 2;
                if (gauche < g2 + o.largeur && g2 < gauche + b.mot.largeur && haut < h2 + m.legende_h &&
                    h2 < haut + m.legende_h) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool moyeu_dans_l_ecran(const Mesures &m, Point p) {
    return p.x - m.jauge / 2 >= m.marge && p.x + m.jauge / 2 <= m.largeur - m.marge && p.y - m.jauge / 2 >= m.marge &&
           p.y + m.jauge / 2 <= m.hauteur - m.marge;
}

// Toute la roue d'un appui : moyeu, premier anneau, second anneau de chaque famille à toutes les tailles de la demande.
// Rend 0 si tout est correct, sinon un code de l'invariant violé.
int roue_correcte(const Mesures &m, Point appui, const Demande &d, bool exige_qu_elle_tienne) {
    const Roue r = ouvrir(m, appui, d);
    if (exige_qu_elle_tienne && !r.tient) {
        return 1;
    }
    if (!r.tient) {
        return 0; // rien n'a convenu : seule l'honnêteté du drapeau est exigée
    }
    if (!moyeu_dans_l_ecran(m, r.moyeu) || !(r.appui == appui) || r.moyeu_deplace != !(r.moyeu == appui)) {
        return 2;
    }
    if (r.dessous != passe_dessous(m, r.moyeu.y) || r.premier.rogne || r.premier.n != d.n) {
        return 3;
    }
    if (!anneau_correct(m, r.premier, r.moyeu, m.rayon_1, m.diametre_1, m.pas_1)) {
        return 4;
    }
    int largeur[CHOIX_MAX];
    for (int j = 0; j < CHOIX_MAX; j++) {
        largeur[j] = d.mot_choix;
    }
    for (int f = 0; f < d.n; f++) {
        if (d.choix[f] == 0) {
            continue;
        }
        // la garantie porte sur le nombre réel de choix de la famille (un éventail plus court pivote autrement : non garanti)
        const Anneau a = deplier(m, r, f, d.choix[f], largeur);
        if (a.rogne || a.n != d.choix[f] || !anneau_correct(m, a, r.moyeu, m.rayon_2, m.diametre_2, m.pas_2)) {
            return 5;
        }
    }
    // chaque centre se touche lui-même, et jamais un autre
    for (int i = 0; i < r.premier.n; i++) {
        if (bouton_sous(m, r.premier, m.diametre_1, Point{r.premier.bouton[i].x, r.premier.bouton[i].y}) != i) {
            return 6;
        }
    }
    return 0;
}

template <typename F> int sur_la_grille(const Mesures &m, int pas, F f) {
    for (int y = 0; y < m.hauteur; y += pas) {
        for (int x = 0; x < m.largeur; x += pas) {
            const int r = f(Point{x, y});
            if (r != 0) {
                std::printf("    appui (%d, %d) : invariant %d\n", x, y, r);
                return r;
            }
        }
    }
    return 0;
}

int toute_la_roue_courante_tient_partout_sur_le_tab5() {
    const Mesures m = mesures(ECRAN_TAB5);
    for (int n = 1; n <= 6; n++) {
        const Demande d = demande_courante(n);
        const int r = sur_la_grille(m, 16, [&](Point p) { return roue_correcte(m, p, d, true); });
        if (r != 0) {
            return r;
        }
    }
    return 0;
}

int toute_la_roue_courante_tient_partout_sur_la_7b() {
    const Mesures m = mesures(ECRAN_7B);
    for (int n = 1; n <= 6; n++) {
        const Demande d = demande_courante(n);
        const int r = sur_la_grille(m, 13, [&](Point p) { return roue_correcte(m, p, d, true); });
        if (r != 0) {
            return r;
        }
    }
    return 0;
}

int le_tab5_sur_1024_x_600_garde_ses_pixels_et_tient_partout() {
    // 1024 x 600 aux pixels du Tab5 (échelle 1,0) : le second anneau (rayon 290) y est plus haut que l'écran, donc seul le premier
    // anneau avec ses mots est exigé partout ; avec familles, la roue peut ne pas tenir, mais alors elle le dit (invariants vérifiés
    // pour toute roue qui se déclare tenir).
    const Mesures m = mesures(Ecran{1024, 600, 1000});
    Demande sans_famille = demande_courante(6);
    for (int i = 0; i < 6; i++) {
        sans_famille.choix[i] = 0;
    }
    const int r = sur_la_grille(m, 16, [&](Point p) { return roue_correcte(m, p, sans_famille, true); });
    if (r != 0) {
        return r;
    }
    const Demande avec_familles = demande_courante(6);
    return sur_la_grille(m, 16, [&](Point p) { return roue_correcte(m, p, avec_familles, false); });
}

int la_demande_lourde_tient_ou_le_dit() {
    // Six familles de huit choix aux mots de 110 px : peu de positions conviennent, mais ouvrir en trouve toujours une, et le
    // drapeau `tient` ne ment jamais (roue_correcte vérifie tout ce qu'il promet).
    for (const Mesures &m : {mesures(ECRAN_TAB5), mesures(ECRAN_7B)}) {
        const Demande d = demande_lourde();
        const int r = sur_la_grille(m, 29, [&](Point p) { return roue_correcte(m, p, d, true); });
        if (r != 0) {
            return r;
        }
    }
    return 0;
}

// Le moyeu choisi est sur le chemin du doigt (limité à la zone du moyeu) vers le centre, décalé verticalement d'un multiple de
// 4 px, et aucune position plus proche du doigt sur le même chemin ne convient.
int le_moyeu_est_la_premiere_position_du_chemin_qui_tient() {
    const Mesures m = mesures(ECRAN_TAB5);
    const Demande d = demande_courante(6);
    const int bord = m.marge + m.jauge / 2;
    for (int y = 0; y < m.hauteur; y += 40) {
        for (int x = 0; x < m.largeur; x += 40) {
            const Roue r = ouvrir(m, Point{x, y}, d);
            if (!r.moyeu_deplace) {
                continue;
            }
            const int dx0 = (x < bord ? bord : (x > m.largeur - bord ? m.largeur - bord : x));
            const int dy0 = (y < bord ? bord : (y > m.hauteur - bord ? m.hauteur - bord : y));
            const Point centre{m.largeur / 2, m.hauteur / 2};
            // sur le chemin sans décalage vertical : trouver k, vérifier que tout k' < k ne convient pas
            bool sur_le_chemin = false;
            for (int k = 0; k <= ETAPES_MOYEU; k++) {
                const Point p{dx0 + (centre.x - dx0) * k / ETAPES_MOYEU, dy0 + (centre.y - dy0) * k / ETAPES_MOYEU};
                if (p == r.moyeu) {
                    sur_le_chemin = true;
                    // aucune position d'avant sur ce chemin ne tient (pas de raccourci manqué)
                    for (int k2 = 0; k2 < k; k2++) {
                        const Point q{dx0 + (centre.x - dx0) * k2 / ETAPES_MOYEU,
                                      dy0 + (centre.y - dy0) * k2 / ETAPES_MOYEU};
                        if (detail::composer(m, Point{x, y}, q, d).tient) {
                            std::printf("    appui (%d, %d) : la position %d du chemin tenait avant la position %d\n",
                                        x, y, k2, k);
                            return 1;
                        }
                    }
                    break;
                }
            }
            if (!sur_le_chemin) {
                // décalé verticalement : le chemin direct, lui, n'avait aucune position qui tienne
                for (int k = 0; k <= ETAPES_MOYEU; k++) {
                    const Point q{dx0 + (centre.x - dx0) * k / ETAPES_MOYEU, dy0 + (centre.y - dy0) * k / ETAPES_MOYEU};
                    if (detail::composer(m, Point{x, y}, q, d).tient) {
                        std::printf("    appui (%d, %d) : décalé alors que la position %d du chemin direct tenait\n", x,
                                    y, k);
                        return 2;
                    }
                }
            }
        }
    }
    return 0;
}

int le_moyeu_reste_sous_le_doigt_loin_des_bords() {
    // Au milieu de la zone « au-dessus » (y >= 400) et loin des côtés (x de 320 à 960 : le mot de 80 px du bouton à 15° dépasse
    // la marge de 12 px en dessous de 305 et au-dessus de 975), six boutons et mots tiennent : le moyeu est le point d'appui, au
    // pixel près, et la roue est celle de disposer.
    const Mesures m = mesures(ECRAN_TAB5);
    Demande d;
    d.n = 6;
    for (int i = 0; i < 6; i++) {
        d.mot[i] = 80;
    }
    for (int y = 400; y < 500; y += 7) {
        for (int x = 320; x <= 960; x += 7) {
            const Roue r = ouvrir(m, Point{x, y}, d);
            if (r.moyeu_deplace || !(r.moyeu == Point{x, y}) || !r.tient) {
                std::printf("    appui (%d, %d) : moyeu (%d, %d)\n", x, y, r.moyeu.x, r.moyeu.y);
                return 1;
            }
            const Anneau tab5 = disposer(m, Point{x, y}, 6, m.rayon_1, m.pas_1, 90, m.diametre_1 / 2, false);
            for (int i = 0; i < 6; i++) {
                if (tab5.bouton[i].x != r.premier.bouton[i].x || tab5.bouton[i].y != r.premier.bouton[i].y) {
                    return 2;
                }
            }
        }
    }
    return 0;
}

// L'éloignement du moyeu par rapport au doigt, mesuré (ADR-0017) avec le premier anneau seul : 76 px en moyenne et 368 au pire au
// Tab5, 78 et 312 à la 7B (pas de 4 px). Garde-fou : la moyenne reste sous 90 px et le pire sous 400 ; une règle qui écarterait
// plus le moyeu (par exemple en exigeant plus de place) fait échouer ce test, il faut alors le dire dans l'ADR.
int le_premier_anneau_seul_ecarte_peu_le_moyeu_du_doigt() {
    for (const Ecran &e : {ECRAN_TAB5, ECRAN_7B}) {
        const Mesures m = mesures(e);
        Demande d = demande_courante(6);
        for (int i = 0; i < 6; i++) {
            d.choix[i] = 0;
        }
        d.mot_choix = 0;
        double somme = 0;
        int pire = 0;
        int nb = 0;
        for (int y = 0; y < m.hauteur; y += 8) {
            for (int x = 0; x < m.largeur; x += 8) {
                const Roue r = ouvrir(m, Point{x, y}, d);
                if (!r.tient) {
                    std::printf("    appui (%d, %d) : le premier anneau seul ne tient pas\n", x, y);
                    return 1;
                }
                const int ecart =
                    static_cast<int>(std::sqrt(static_cast<double>(carre(r.moyeu.x - x) + carre(r.moyeu.y - y))));
                somme += ecart;
                pire = ecart > pire ? ecart : pire;
                nb++;
            }
        }
        const int moyenne = static_cast<int>(somme / nb);
        std::printf("    %d x %d : écart moyen %d px, pire %d px\n", m.largeur, m.hauteur, moyenne, pire);
        if (moyenne > 90 || pire > 400) {
            return 2;
        }
    }
    return 0;
}

int les_boutons_du_tab5_pivotent_par_pas_de_5_et_dans_la_marge_ou_sont_ramenes() {
    // Pour tout moyeu de l'écran, disposer rend soit un éventail régulier (centres à la même distance, même écart), soit déclare
    // qu'un centre a été ramené dans l'écran ; jamais un éventail irrégulier sans le dire.
    const Mesures m = mesures(ECRAN_TAB5);
    for (int y = 0; y < m.hauteur; y += 9) {
        for (int x = 0; x < m.largeur; x += 9) {
            for (int n : {1, 2, 3, 4, 5, 6}) {
                const Anneau a =
                    disposer(m, Point{x, y}, n, m.rayon_1, m.pas_1, 90, m.diametre_1 / 2, passe_dessous(m, y));
                if (a.pivot % PIVOT != 0) {
                    return 1;
                }
                if (!a.rogne && !anneau_correct(m, a, Point{x, y}, m.rayon_1, m.diametre_1, m.pas_1)) {
                    // sans rognage, l'éventail doit être complet : dans l'écran, régulier, sans chevauchement
                    std::printf("    moyeu (%d, %d) n=%d : éventail non régulier et non déclaré rogné\n", x, y, n);
                    return 2;
                }
            }
        }
    }
    return 0;
}

int le_toucher_ne_confond_jamais_deux_boutons_d_une_roue() {
    const Mesures m = mesures(ECRAN_TAB5);
    const Demande d = demande_courante(6);
    for (int y = 0; y < m.hauteur; y += 40) {
        for (int x = 0; x < m.largeur; x += 40) {
            const Roue r = ouvrir(m, Point{x, y}, d);
            for (int i = 0; i < r.premier.n; i++) {
                const int rb = m.diametre_1 / 2;
                // le centre, les quatre points du cercle du bouton : toujours ce bouton
                const Point pts[] = {{r.premier.bouton[i].x, r.premier.bouton[i].y},
                                     {r.premier.bouton[i].x + rb, r.premier.bouton[i].y},
                                     {r.premier.bouton[i].x - rb, r.premier.bouton[i].y},
                                     {r.premier.bouton[i].x, r.premier.bouton[i].y + rb},
                                     {r.premier.bouton[i].x, r.premier.bouton[i].y - rb}};
                for (const Point p : pts) {
                    if (bouton_sous(m, r.premier, m.diametre_1, p) != i) {
                        std::printf("    appui (%d, %d), bouton %d, pixel (%d, %d)\n", x, y, i, p.x, p.y);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(toute_la_roue_courante_tient_partout_sur_le_tab5)                                                                \
    X(toute_la_roue_courante_tient_partout_sur_la_7b)                                                                  \
    X(le_tab5_sur_1024_x_600_garde_ses_pixels_et_tient_partout)                                                        \
    X(la_demande_lourde_tient_ou_le_dit)                                                                               \
    X(le_moyeu_est_la_premiere_position_du_chemin_qui_tient)                                                           \
    X(le_moyeu_reste_sous_le_doigt_loin_des_bords)                                                                     \
    X(le_premier_anneau_seul_ecarte_peu_le_moyeu_du_doigt)                                                             \
    X(les_boutons_du_tab5_pivotent_par_pas_de_5_et_dans_la_marge_ou_sont_ramenes)                                      \
    X(le_toucher_ne_confond_jamais_deux_boutons_d_une_roue)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
