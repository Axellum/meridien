// Tests des cartes du mode automatique de l'accueil : accueil_cartes.hpp (ADR-0023) — la carte pluie, le bandeau d'alerte agrandi, et les
// rectangles de chaque composition de l'écran.
//
// Aucune donnée réelle : les cumuls horaires sont écrits à la main, les rectangles attendus sont calculés ici à partir des tailles de
// l'écran de référence (1280 x 720 du Tab5, 1280 x 750 de la 7B de 1024 x 600, 720 x 1280 en portrait), pas relus dans le code.
// Les instants sont ceux du dimanche 8 novembre 2026 à Paris (valeurs d'exemple). Chaque test est évalué à la compilation ET à l'exécution.
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "ecran/accueil_cartes.hpp"
#include "mini.hpp"

using namespace ecran::accueil;
using ecran::visuel::HeurePrevue;

namespace {

constexpr std::int64_t MINUIT = 1'794'092'400; // 2026-11-08 00:00 à Paris
constexpr std::int64_t MIDI = MINUIT + 12 * 3600;
constexpr int INCONNU_H = ecran::visuel::INCONNU_DIX;

constexpr bool egal(const Rect &a, const Rect &b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
constexpr bool vide(const Rect &r) { return r.w <= 0 || r.h <= 0; }
constexpr bool dedans(const Rect &r, int w, int h) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= w && r.y + r.h <= h; }
constexpr bool se_chevauchent(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

struct Ecran {
    int w;
    int h;
    bool portrait;
};
constexpr Ecran ECRANS[] = {{1280, 720, false}, {1280, 750, false}, {720, 1280, true}};

// Une série d'heures à partir de midi : la pluie de chaque heure en dixièmes de mm.
template <std::size_t N> struct Serie {
    HeurePrevue h[N];
    int n = static_cast<int>(N);
};
template <std::size_t N> constexpr Serie<N> serie(const int (&pluie)[N]) {
    Serie<N> s;
    for (std::size_t i = 0; i < N; ++i) {
        s.h[i].instant_s = MIDI + static_cast<std::int64_t>(i) * 3600;
        s.h[i].pluie_dix_mm = pluie[i];
    }
    return s;
}

// ---- les constantes ---------------------------------------------------------------------------------------------------------------

constexpr int les_constantes_des_cartes() {
    VERIFIER(ECART_CARTES == 16);
    VERIFIER(BANDE_JOURS_H == 92);
    VERIFIER(TUILE_PLUIE_W == 360);
    VERIFIER(TUILE_PLUIE_H == 88);
    VERIFIER(ALERTE_H_MIN == 150);
    VERIFIER(BARRES_MAX == 12);
    VERIFIER(SEUIL_MODEREE_DIX == 10 && SEUIL_FORTE_DIX == 30);
    return 0;
}

// ---- la composition -----------------------------------------------------------------------------------------------------------------

constexpr int l_alerte_passe_avant_la_pluie_dans_la_composition() {
    Presentation p;
    VERIFIER(composition_de(p) == Composition::Normale);
    p.carte_pluie = true;
    VERIFIER(composition_de(p) == Composition::CartePluie);
    p.bandeau_agrandi = true; // les deux : l'alerte garde sa place, la pluie est dite par la tuile
    VERIFIER(composition_de(p) == Composition::AlerteAgrandie);
    p.carte_pluie = false;
    VERIFIER(composition_de(p) == Composition::AlerteAgrandie);
    // Épinglé : pas de carte pluie, l'alerte agrandie reste (ADR-0015).
    Presentation e;
    e.epingle = true;
    VERIFIER(composition_de(e) == Composition::Normale);
    e.bandeau_agrandi = true;
    VERIFIER(composition_de(e) == Composition::AlerteAgrandie);
    return 0;
}

// ---- les rectangles ------------------------------------------------------------------------------------------------------------------

constexpr int la_composition_normale_est_l_accueil_d_avant_pixel_pour_pixel() {
    for (const Ecran &e : ECRANS) {
        const Zones z = disposer(e.w, e.h, e.portrait);
        const ZonesAuto a = disposer_auto(e.w, e.h, e.portrait, Composition::Normale, true, true);
        VERIFIER(egal(a.bandeau, z.bandeau) && egal(a.bas, z.jours));
        VERIFIER(vide(a.alerte) && vide(a.tuile));
        VERIFIER(!a.bas_en_bande && !a.bas_en_carte_pluie);
    }
    return 0;
}

constexpr int la_carte_pluie_sans_alerte_monte_prendre_la_place_du_bandeau() {
    for (const Ecran &e : ECRANS) {
        const Zones z = disposer(e.w, e.h, e.portrait);
        const ZonesAuto a = disposer_auto(e.w, e.h, e.portrait, Composition::CartePluie, false, false);
        VERIFIER(a.bas_en_carte_pluie && !a.bas_en_bande);
        VERIFIER(a.bas.x == z.bandeau.x && a.bas.y == z.bandeau.y && a.bas.w == z.bandeau.w);
        VERIFIER(a.bas.y + a.bas.h ==
                 z.jours.y + z.jours.h); // elle va jusqu'en bas de l'écran, comme la carte des jours
        VERIFIER(vide(a.bandeau) && vide(a.alerte) && vide(a.tuile));
        VERIFIER(dedans(a.bas, e.w, e.h));
    }
    return 0;
}

constexpr int la_carte_pluie_avec_alerte_garde_le_bandeau_et_prend_la_place_des_jours() {
    for (const Ecran &e : ECRANS) {
        const Zones z = disposer(e.w, e.h, e.portrait);
        const ZonesAuto a = disposer_auto(e.w, e.h, e.portrait, Composition::CartePluie, true, false);
        VERIFIER(egal(a.bandeau, z.bandeau) && egal(a.alerte, z.bandeau));
        VERIFIER(egal(a.bas, z.jours));
        VERIFIER(a.bas_en_carte_pluie && !a.bas_en_bande);
    }
    return 0;
}

constexpr int la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5() {
    // 1280 x 720 : les jours finissent à 688 (720 - 32). La bande : 596 à 688. La carte d'alerte et la tuile : de 346 à 580.
    const ZonesAuto a = disposer_auto(1280, 720, false, Composition::AlerteAgrandie, true, true);
    VERIFIER(egal(a.bas, Rect{32, 596, 1216, 92}));
    VERIFIER(egal(a.bandeau, Rect{32, 346, 1216, 234}));
    VERIFIER(egal(a.alerte, Rect{32, 346, 840, 234}));
    VERIFIER(egal(a.tuile, Rect{888, 346, 360, 234}));
    VERIFIER(a.bas_en_bande && !a.bas_en_carte_pluie);
    return 0;
}

constexpr int la_composition_d_alerte_donne_les_30_pixels_de_la_7b_a_la_carte_d_alerte() {
    // 1280 x 750 (la 7B) : les jours finissent à 718 ; la bande reste de 92 px, la carte d'alerte gagne les 30 px.
    const ZonesAuto a = disposer_auto(1280, 750, false, Composition::AlerteAgrandie, true, true);
    VERIFIER(egal(a.bas, Rect{32, 626, 1216, 92}));
    VERIFIER(egal(a.alerte, Rect{32, 346, 840, 264}));
    VERIFIER(egal(a.tuile, Rect{888, 346, 360, 264}));
    return 0;
}

constexpr int la_composition_d_alerte_en_portrait_met_la_tuile_sous_la_carte() {
    // 720 x 1280 : les jours finissent à 1252 ; la bande : 1160 à 1252 ; la zone : 664 à 1144 (480 px) ; la tuile en prend 88 + 16.
    const ZonesAuto a = disposer_auto(720, 1280, true, Composition::AlerteAgrandie, true, true);
    VERIFIER(egal(a.bas, Rect{28, 1160, 664, 92}));
    VERIFIER(egal(a.alerte, Rect{28, 664, 664, 376}));
    VERIFIER(egal(a.tuile, Rect{28, 1056, 664, 88}));
    VERIFIER(a.bas_en_bande);
    return 0;
}

constexpr int sans_rien_a_dire_de_la_pluie_la_carte_d_alerte_prend_toute_la_largeur() {
    const ZonesAuto h = disposer_auto(1280, 720, false, Composition::AlerteAgrandie, true, false);
    VERIFIER(egal(h.alerte, Rect{32, 346, 1216, 234}) && vide(h.tuile));
    const ZonesAuto v = disposer_auto(720, 1280, true, Composition::AlerteAgrandie, true, false);
    VERIFIER(egal(v.alerte, Rect{28, 664, 664, 480}) && vide(v.tuile));
    return 0;
}

constexpr int les_rectangles_d_alerte_ne_se_chevauchent_pas_et_tiennent_dans_l_ecran() {
    for (const Ecran &e : ECRANS) {
        const Zones z = disposer(e.w, e.h, e.portrait);
        for (const bool tuile : {false, true}) {
            const ZonesAuto a = disposer_auto(e.w, e.h, e.portrait, Composition::AlerteAgrandie, true, tuile);
            VERIFIER(dedans(a.alerte, e.w, e.h) && dedans(a.bas, e.w, e.h));
            VERIFIER(a.alerte.h >= ALERTE_H_MIN);
            VERIFIER(a.alerte.y ==
                     z.bandeau.y); // la carte d'alerte part du haut de la zone du bandeau : rien au-dessus ne bouge
            VERIFIER(!se_chevauchent(a.alerte, a.bas));
            VERIFIER(a.bas.h == BANDE_JOURS_H);
            VERIFIER(a.bas.y + a.bas.h == z.jours.y + z.jours.h);
            if (tuile) {
                VERIFIER(dedans(a.tuile, e.w, e.h));
                VERIFIER(!se_chevauchent(a.alerte, a.tuile) && !se_chevauchent(a.tuile, a.bas));
                // L'écart de 16 px entre la carte, la tuile et la bande, sur l'axe où elles se suivent.
                if (!e.portrait) {
                    VERIFIER(a.tuile.x - (a.alerte.x + a.alerte.w) == ECART_CARTES);
                    VERIFIER(a.tuile.x + a.tuile.w ==
                             z.bandeau.x + z.bandeau.w); // la tuile finit au bord droit de la zone
                    VERIFIER(a.tuile.w == TUILE_PLUIE_W && a.tuile.h == a.alerte.h);
                } else {
                    VERIFIER(a.tuile.y - (a.alerte.y + a.alerte.h) == ECART_CARTES);
                    VERIFIER(a.tuile.h == TUILE_PLUIE_H && a.tuile.w == a.alerte.w);
                    VERIFIER(a.bas.y - (a.tuile.y + a.tuile.h) == ECART_CARTES);
                }
            } else {
                VERIFIER(vide(a.tuile));
                VERIFIER(a.bas.y - (a.alerte.y + a.alerte.h) == ECART_CARTES);
            }
        }
    }
    return 0;
}

// ---- la carte pluie : ce qu'elle dit -------------------------------------------------------------------------------------------------

constexpr EpisodePluie episode(const HeurePrevue *h, int n, std::int64_t maintenant_s, int pluie_dans_min) {
    return dans_l_horizon(episode_de_pluie(h, n, maintenant_s, pluie_dans_min), maintenant_s);
}

constexpr int pas_de_carte_sans_pluie_proche() {
    const auto s = serie({0, 5, 12, 35, 8, 0, 0, 0});
    const std::int64_t maintenant = MIDI + 20 * 60;
    // Genres qui ne font pas une carte : rien, inconnu, pluie « plus tard » (60 min et plus).
    EpisodePluie e;
    e.genre = GenrePluie::AUCUNE;
    VERIFIER(!composer_carte_pluie(e, s.h, s.n, maintenant, 9).presente);
    e.genre = GenrePluie::INCONNU;
    VERIFIER(!composer_carte_pluie(e, s.h, s.n, maintenant, 9).presente);
    const EpisodePluie plus_tard = episode(s.h, s.n, maintenant, 90);
    VERIFIER(plus_tard.genre == GenrePluie::PLUS_TARD);
    VERIFIER(!composer_carte_pluie(plus_tard, s.h, s.n, maintenant, 9).presente);
    // Les deux genres qui en font une.
    VERIFIER(composer_carte_pluie(episode(s.h, s.n, maintenant, 25), s.h, s.n, maintenant, 9).presente);
    VERIFIER(composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9).presente);
    return 0;
}

constexpr int la_carte_dit_le_compte_a_rebours_la_force_le_pic_la_duree_et_le_cumul() {
    // 12 h 20. Pluie : 13 h 0,4 mm, 14 h 1,2 mm, 15 h 3,5 mm, 16 h 0,8 mm, puis sec. Annoncée dans 25 min (12 h 45).
    const auto s = serie({0, 4, 12, 35, 8, 0, 0, 0});
    const std::int64_t maintenant = MIDI + 20 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 25), s.h, s.n, maintenant, 9);
    VERIFIER(c.presente && c.genre == GenrePluie::BIENTOT && c.dans_min == 25);
    VERIFIER(c.force_connue && c.force == NiveauPluie::FAIBLE);     // la première heure de l'épisode : 0,4 mm
    VERIFIER(c.plus_forte_plus_tard && c.pic_s == MIDI + 3 * 3600); // 15 h : 3,5 mm, « forte »
    VERIFIER(c.duree_h == 4);                                       // 12 h 45 à 17 h : 4 h 15
    VERIFIER(c.cumul_dix_mm == 59);                                 // 0,4 + 1,2 + 3,5 + 0,8
    VERIFIER(c.echelle_dix == 35);                                  // la plus forte barre dépasse le plancher de 3 mm
    return 0;
}

constexpr int une_pluie_en_cours_est_dite_en_cours() {
    const auto s = serie({15, 15, 0, 0, 0, 0});
    const std::int64_t maintenant = MIDI + 20 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9);
    VERIFIER(c.presente && c.genre == GenrePluie::EN_COURS && c.dans_min == 0);
    VERIFIER(c.force_connue && c.force == NiveauPluie::MOYEN); // 1,5 mm
    VERIFIER(!c.plus_forte_plus_tard);                         // 13 h : 1,5 mm, même niveau
    VERIFIER(c.duree_h == 2);                                  // de 12 h 20 à 14 h : 1 h 40, arrondi à 2
    VERIFIER(c.cumul_dix_mm == 30);
    return 0;
}

constexpr int la_duree_n_est_dite_que_si_la_pluie_finit_dans_les_donnees() {
    // La pluie touche la dernière heure connue : elle dure peut-être bien plus, on ne dit pas « environ 3 h ».
    const auto s = serie({0, 10, 10, 10});
    const std::int64_t maintenant = MIDI + 20 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 30), s.h, s.n, maintenant, 9);
    VERIFIER(c.presente && c.duree_h == 0);
    VERIFIER(c.cumul_dix_mm == 30); // le cumul, lui, est celui des heures connues
    // La même pluie suivie d'une heure sèche connue : elle finit, la durée est dite.
    const auto t = serie({0, 10, 10, 10, 0});
    const CartePluie d = composer_carte_pluie(episode(t.h, t.n, maintenant, 30), t.h, t.n, maintenant, 9);
    VERIFIER(d.duree_h == 3); // de 12 h 50 à 16 h : 3 h 10
    return 0;
}

constexpr int la_duree_ne_tombe_jamais_sous_une_heure() {
    // En cours depuis maintenant, une seule heure pluvieuse qui finit dans 10 min : 10 min, arrondies à 1 h (jamais « 0 h »).
    const auto s = serie({10, 0, 0});
    const std::int64_t maintenant = MIDI + 50 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9);
    VERIFIER(c.presente && c.duree_h == 1);
    return 0;
}

constexpr int la_pluie_plus_forte_plus_tard_ne_se_dit_que_si_le_niveau_monte() {
    const std::int64_t maintenant = MIDI + 5 * 60;
    // Même niveau (faible) toute l'heure : rien à signaler.
    const auto a = serie({3, 5, 8, 0, 0});
    VERIFIER(!composer_carte_pluie(episode(a.h, a.n, maintenant, 0), a.h, a.n, maintenant, 9).plus_forte_plus_tard);
    // Le niveau monte (faible puis moyen) : signalé, avec l'heure du pic.
    const auto b = serie({3, 5, 15, 0, 0});
    const CartePluie cb = composer_carte_pluie(episode(b.h, b.n, maintenant, 0), b.h, b.n, maintenant, 9);
    VERIFIER(cb.plus_forte_plus_tard && cb.pic_s == MIDI + 2 * 3600);
    // Fort au départ, fort ensuite : rien à signaler, même si l'heure suivante est plus forte en millimètres.
    const auto c = serie({32, 50, 0, 0});
    VERIFIER(!composer_carte_pluie(episode(c.h, c.n, maintenant, 0), c.h, c.n, maintenant, 9).plus_forte_plus_tard);
    // Moins fort ensuite : rien à signaler non plus.
    const auto d = serie({35, 12, 0, 0});
    VERIFIER(!composer_carte_pluie(episode(d.h, d.n, maintenant, 0), d.h, d.n, maintenant, 9).plus_forte_plus_tard);
    // Le pic est celui de l'épisode, pas d'une pluie qui reprend après une heure sèche.
    const auto e = serie({3, 0, 40, 0});
    VERIFIER(!composer_carte_pluie(episode(e.h, e.n, maintenant, 0), e.h, e.n, maintenant, 9).plus_forte_plus_tard);
    return 0;
}

constexpr int le_pic_est_la_premiere_heure_la_plus_forte() {
    const std::int64_t maintenant = MIDI + 5 * 60;
    const auto s = serie({3, 25, 25, 12, 0});
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9);
    VERIFIER(c.plus_forte_plus_tard && c.pic_s == MIDI + 1 * 3600); // deux heures à 2,5 mm : la première
    return 0;
}

constexpr int les_barres_commencent_a_l_heure_en_cours_et_se_bornent_aux_colonnes() {
    const auto s = serie({0, 4, 12, 35, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const std::int64_t maintenant = MIDI + 20 * 60; // 12 h 20 : l'heure en cours est celle de 12 h
    const EpisodePluie e = episode(s.h, s.n, maintenant, 25);
    CartePluie c = composer_carte_pluie(e, s.h, s.n, maintenant, 9);
    VERIFIER(c.n == 9 && c.barre[0].instant_s == MIDI && c.barre[1].instant_s == MIDI + 3600);
    VERIFIER(c.barre[1].dix_mm == 4 && c.barre[3].dix_mm == 35 && c.barre[3].niveau == NiveauPluie::FORT);
    VERIFIER(c.barre[2].niveau == NiveauPluie::MOYEN && c.barre[1].niveau == NiveauPluie::FAIBLE);
    c = composer_carte_pluie(e, s.h, s.n, maintenant, 8);
    VERIFIER(c.n == 8);
    c = composer_carte_pluie(e, s.h, s.n, maintenant, 40); // jamais plus que BARRES_MAX
    VERIFIER(c.n == BARRES_MAX);
    // Une heure déjà finie n'est pas une barre : à 13 h 05 la première barre est celle de 13 h.
    const std::int64_t plus_tard = MIDI + 65 * 60;
    c = composer_carte_pluie(episode(s.h, s.n, plus_tard, 0), s.h, s.n, plus_tard, 9);
    VERIFIER(c.n == 9 && c.barre[0].instant_s == MIDI + 3600);
    // À 13 h pile l'heure de 12 h est finie (sa fin n'est pas « après maintenant ») : la première barre est celle de 13 h.
    const std::int64_t pile = MIDI + 3600;
    c = composer_carte_pluie(episode(s.h, s.n, pile, 0), s.h, s.n, pile, 9);
    VERIFIER(c.barre[0].instant_s == MIDI + 3600);
    // Moins d'heures connues que de colonnes : autant de barres que d'heures.
    const auto courte = serie({10, 10, 0});
    c = composer_carte_pluie(episode(courte.h, courte.n, MIDI + 5 * 60, 0), courte.h, courte.n, MIDI + 5 * 60, 9);
    VERIFIER(c.n == 3);
    return 0;
}

constexpr int une_heure_sans_valeur_est_inconnue_jamais_zero() {
    const auto s = serie({10, INCONNU_H, 20, 0});
    const std::int64_t maintenant = MIDI + 5 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9);
    VERIFIER(c.n == 4);
    VERIFIER(c.barre[0].dix_mm == 10);
    VERIFIER(c.barre[1].dix_mm == INCONNU_H); // une barre inconnue ne se dessine pas comme une barre sèche
    VERIFIER(c.barre[2].dix_mm == 20 && c.barre[3].dix_mm == 0);
    VERIFIER(
        c.duree_h ==
        1); // l'épisode s'arrête devant l'heure inconnue : la durée est celle de la première heure (55 min, arrondie)
    VERIFIER(c.echelle_dix == 30);
    return 0;
}

constexpr int l_echelle_a_un_plancher_de_trois_millimetres_et_suit_la_plus_forte_barre() {
    const std::int64_t maintenant = MIDI + 5 * 60;
    const auto faible = serie({5, 8, 0, 0});
    VERIFIER(composer_carte_pluie(episode(faible.h, faible.n, maintenant, 0), faible.h, faible.n, maintenant, 9)
                 .echelle_dix == 30);
    const auto pile = serie({30, 0, 0});
    VERIFIER(composer_carte_pluie(episode(pile.h, pile.n, maintenant, 0), pile.h, pile.n, maintenant, 9).echelle_dix ==
             30);
    const auto forte = serie({12, 47, 0, 0});
    VERIFIER(
        composer_carte_pluie(episode(forte.h, forte.n, maintenant, 0), forte.h, forte.n, maintenant, 9).echelle_dix ==
        47);
    // Une barre au-delà des colonnes montrées ne fixe pas l'échelle.
    const auto loin = serie({12, 10, 10, 90, 0});
    VERIFIER(composer_carte_pluie(episode(loin.h, loin.n, maintenant, 0), loin.h, loin.n, maintenant, 3).echelle_dix ==
             30);
    return 0;
}

constexpr int la_force_est_celle_de_la_premiere_heure_pluvieuse_pas_d_une_heure_seche() {
    // 12 h 20, pluie dans 25 min : l'heure de 12 h est sèche, celle de 13 h (1,5 mm, « moyenne ») est la première qui compte.
    const auto s = serie({0, 15, 15, 0});
    const std::int64_t maintenant = MIDI + 20 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 25), s.h, s.n, maintenant, 9);
    VERIFIER(c.force_connue && c.force == NiveauPluie::MOYEN);
    return 0;
}

constexpr int une_heure_pluvieuse_trop_loin_du_debut_annonce_ne_donne_ni_force_ni_cumul() {
    // Annoncée dans 20 min par les quarts d'heure, mais la seule heure pluvieuse des cumuls commence 3 h plus tard : elle ne couvre
    // pas le début (episode_de_pluie ne lui donne pas de cumul) ; la carte ne lui prête pas non plus de force ni de durée.
    const auto s = serie({0, 0, 0, 15, 0});
    const std::int64_t maintenant = MIDI + 5 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 20), s.h, s.n, maintenant, 9);
    VERIFIER(c.presente && !c.force_connue && c.duree_h == 0 && c.cumul_dix_mm == INCONNU_H && !c.plus_forte_plus_tard);
    return 0;
}

constexpr int un_trou_dans_les_heures_coupe_l_episode() {
    // 12 h : 1 mm, 13 h : 1 mm, (14 h manque), 15 h : 4 mm. Le trou finit l'épisode : la pluie de 15 h n'en est pas le pic.
    Serie<3> s;
    s.h[0] = {MIDI, INCONNU_H, 0, true, -1, 10};
    s.h[1] = {MIDI + 3600, INCONNU_H, 0, true, -1, 10};
    s.h[2] = {MIDI + 3 * 3600, INCONNU_H, 0, true, -1, 40};
    const std::int64_t maintenant = MIDI + 5 * 60;
    const CartePluie c = composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9);
    VERIFIER(c.presente && !c.plus_forte_plus_tard);
    VERIFIER(c.duree_h == 2); // 12 h 05 à 14 h : l'épisode finit avec la dernière heure contiguë
    VERIFIER(c.cumul_dix_mm == 20);
    return 0;
}

constexpr int le_cumul_est_celui_de_l_episode_et_se_tait_s_il_est_inconnu_ou_nul() {
    const std::int64_t maintenant = MIDI + 5 * 60;
    const auto s = serie({15, 20, 0, 0});
    VERIFIER(composer_carte_pluie(episode(s.h, s.n, maintenant, 0), s.h, s.n, maintenant, 9).cumul_dix_mm == 35);
    // Pluie proche annoncée au quart d'heure mais aucune heure pluvieuse ne la couvre : pas de cumul (inconnu), pas de force.
    const auto sec = serie({0, 0, 0, 0});
    const CartePluie c = composer_carte_pluie(episode(sec.h, sec.n, maintenant, 20), sec.h, sec.n, maintenant, 9);
    VERIFIER(c.presente && c.cumul_dix_mm == INCONNU_H && !c.force_connue && c.duree_h == 0 && !c.plus_forte_plus_tard);
    return 0;
}

constexpr int sans_heures_ou_sans_heure_la_carte_dit_ce_qu_elle_sait_sans_barres() {
    EpisodePluie e;
    e.genre = GenrePluie::BIENTOT;
    e.dans_min = 12;
    e.debut_s = MIDI + 12 * 60;
    CartePluie c = composer_carte_pluie(e, nullptr, 0, MIDI, 9);
    VERIFIER(c.presente && c.dans_min == 12 && c.n == 0 && !c.force_connue && c.cumul_dix_mm == INCONNU_H);
    const auto s = serie({10, 10, 0});
    c = composer_carte_pluie(e, s.h, s.n, 0, 9); // l'heure du moment est inconnue
    VERIFIER(c.presente && c.dans_min == 12 && c.n == 0);
    c = composer_carte_pluie(e, s.h, 0, MIDI, 9);
    VERIFIER(c.presente && c.n == 0);
    return 0;
}

// ---- le dessin des barres ------------------------------------------------------------------------------------------------------------

constexpr int la_hauteur_d_une_barre_est_proportionnelle_avec_un_plancher_et_un_plafond() {
    VERIFIER(hauteur_de_barre(0, 30, 100, 4) == 0);         // sec : rien
    VERIFIER(hauteur_de_barre(INCONNU_H, 30, 100, 4) == 0); // inconnu : rien
    VERIFIER(hauteur_de_barre(-3, 30, 100, 4) == 0);
    VERIFIER(hauteur_de_barre(15, 0, 100, 4) == 0 && hauteur_de_barre(15, 30, 0, 4) == 0);
    VERIFIER(hauteur_de_barre(15, 30, 100, 4) == 50); // la moitié de l'échelle
    VERIFIER(hauteur_de_barre(30, 30, 100, 4) == 100);
    VERIFIER(hauteur_de_barre(45, 30, 100, 4) == 100); // au-delà de l'échelle : plafonnée
    VERIFIER(hauteur_de_barre(1, 30, 100, 4) == 4);    // une goutte se voit : plancher
    VERIFIER(hauteur_de_barre(1, 30, 3, 5) == 3);      // le plafond passe avant le plancher
    VERIFIER(hauteur_de_barre(10, 30, 99, 4) == 33);
    return 0;
}

constexpr int les_reperes_moyen_et_fort_sont_sur_la_meme_echelle_que_les_barres() {
    VERIFIER(hauteur_du_repere(SEUIL_MODEREE_DIX, 30, 90) == 30);
    VERIFIER(hauteur_du_repere(SEUIL_FORTE_DIX, 30, 90) == 90);
    VERIFIER(hauteur_du_repere(SEUIL_FORTE_DIX, 60, 90) == 45);
    VERIFIER(hauteur_du_repere(SEUIL_FORTE_DIX, 0, 90) == 0 && hauteur_du_repere(SEUIL_FORTE_DIX, 30, 0) == 0);
    VERIFIER(hauteur_du_repere(100, 30, 90) == 90); // plafonné
    // Un repère n'est tracé que s'il est dans le graphique.
    VERIFIER(repere_visible(SEUIL_MODEREE_DIX, 30) && repere_visible(SEUIL_FORTE_DIX, 30));
    VERIFIER(!repere_visible(SEUIL_FORTE_DIX, 29));
    VERIFIER(repere_visible(SEUIL_FORTE_DIX, 30));
    VERIFIER(!repere_visible(SEUIL_MODEREE_DIX, 0));
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(les_constantes_des_cartes)                                                                                       \
    X(l_alerte_passe_avant_la_pluie_dans_la_composition)                                                               \
    X(la_composition_normale_est_l_accueil_d_avant_pixel_pour_pixel)                                                   \
    X(la_carte_pluie_sans_alerte_monte_prendre_la_place_du_bandeau)                                                    \
    X(la_carte_pluie_avec_alerte_garde_le_bandeau_et_prend_la_place_des_jours)                                         \
    X(la_composition_d_alerte_tient_la_bande_des_jours_au_bas_de_l_ecran_tab5)                                         \
    X(la_composition_d_alerte_donne_les_30_pixels_de_la_7b_a_la_carte_d_alerte)                                        \
    X(la_composition_d_alerte_en_portrait_met_la_tuile_sous_la_carte)                                                  \
    X(sans_rien_a_dire_de_la_pluie_la_carte_d_alerte_prend_toute_la_largeur)                                           \
    X(les_rectangles_d_alerte_ne_se_chevauchent_pas_et_tiennent_dans_l_ecran)                                          \
    X(pas_de_carte_sans_pluie_proche)                                                                                  \
    X(la_carte_dit_le_compte_a_rebours_la_force_le_pic_la_duree_et_le_cumul)                                           \
    X(une_pluie_en_cours_est_dite_en_cours)                                                                            \
    X(la_duree_n_est_dite_que_si_la_pluie_finit_dans_les_donnees)                                                      \
    X(la_duree_ne_tombe_jamais_sous_une_heure)                                                                         \
    X(la_pluie_plus_forte_plus_tard_ne_se_dit_que_si_le_niveau_monte)                                                  \
    X(le_pic_est_la_premiere_heure_la_plus_forte)                                                                      \
    X(les_barres_commencent_a_l_heure_en_cours_et_se_bornent_aux_colonnes)                                             \
    X(une_heure_sans_valeur_est_inconnue_jamais_zero)                                                                  \
    X(l_echelle_a_un_plancher_de_trois_millimetres_et_suit_la_plus_forte_barre)                                        \
    X(la_force_est_celle_de_la_premiere_heure_pluvieuse_pas_d_une_heure_seche)                                         \
    X(une_heure_pluvieuse_trop_loin_du_debut_annonce_ne_donne_ni_force_ni_cumul)                                       \
    X(un_trou_dans_les_heures_coupe_l_episode)                                                                         \
    X(le_cumul_est_celui_de_l_episode_et_se_tait_s_il_est_inconnu_ou_nul)                                              \
    X(sans_heures_ou_sans_heure_la_carte_dit_ce_qu_elle_sait_sans_barres)                                              \
    X(la_hauteur_d_une_barre_est_proportionnelle_avec_un_plancher_et_un_plafond)                                       \
    X(les_reperes_moyen_et_fort_sont_sur_la_meme_echelle_que_les_barres)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
