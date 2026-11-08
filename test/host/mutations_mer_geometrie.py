"""Mutations du noyau « mise en page de la page mer » (components/core/include/ecran/mer_geometrie.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_MER : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui doit
échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle, y compris une erreur de compilation voulue), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle de la mise en page : les rectangles (dans, séparés, contient), l'échelle, l'ordre des tailles de police, le
sens portrait, la zone de « Fermer », le partage de la largeur entre quatre cartes, la hauteur minimale d'une carte et le rétrécissement de
la marée et du soleil en portrait, le contenu d'une carte, de la marée et du soleil, le détail (les colonnes, le centrage, la croix). Ce sont
des mutations choisies à la main sur les règles de ce fichier, pas un balayage automatique : elles ne disent pas que « tout » est testé,
seulement que ces règles-là le sont. Elles ne disent PAS qu'un texte tient dans sa zone : cela dépend de la police et de la langue, et on le
regarde sur le rendu PC (tools/rendu, planches de docs/captures/ecran/).
"""

TG = "test_mer_geometrie.cpp"
GE = "mer_geometrie.hpp"

RECTS = "les_rectangles_se_comparent_comme_on_s_y_attend"
ECHELLE = "l_echelle_suit_le_petit_cote_et_ne_descend_jamais_sous_un_pixel"
PETIT = "un_ecran_trop_petit_n_a_pas_de_page"
SENS = "le_sens_vient_de_la_forme_de_l_ecran"
POLICES = "les_polices_suivent_l_echelle_et_restent_lisibles"
ZONES = "chaque_zone_est_dans_l_ecran_et_aucune_n_en_chevauche_une_autre"
CARTES = "les_quatre_cartes_sont_egales_et_gardent_l_ordre_des_activites"
DOIGT = "les_zones_tactiles_se_touchent_au_doigt"
MINIMALE = "la_hauteur_minimale_d_une_carte_est_exactement_celle_qui_suffit"
HAUTEURS = "une_page_valide_garde_toujours_des_cartes_assez_hautes"
PORTRAIT_COURT = "un_ecran_portrait_trop_court_n_a_pas_de_page_et_un_assez_haut_en_a_une"
BARRES = "les_24_barres_tiennent_dans_la_carte_et_restent_assez_larges"
MAREE = "l_interieur_du_panneau_de_maree_tient_et_laisse_de_la_place_a_la_courbe"
SOLEIL = "l_interieur_du_panneau_du_soleil_tient"
DETAIL = "le_detail_tient_dans_l_ecran_et_ses_zones_ne_se_chevauchent_pas"
EPOUSE = "le_panneau_du_detail_epouse_le_nombre_de_criteres"
CENTRE = "le_detail_a_la_place_de_son_avis_et_de_ses_rangees"

MUTATIONS_MER_GEOMETRIE = [
    # ---- les outils de base -----------------------------------------------------------------------------------------------------------------
    (
        GE,
        "return a.w > 0 && a.h > 0 && a.x >= b.x",
        "return a.w >= 0 && a.h > 0 && a.x >= b.x",
        TG,
        RECTS,
        "une zone vide est « dans » son parent : un test de contenu qui ne voit plus une zone qui a disparu",
    ),
    (
        GE,
        "return a.droite() <= b.x ||",
        "return a.droite() < b.x ||",
        TG,
        RECTS,
        "deux zones qui se touchent par un bord sont dites se chevaucher : les cartes jointives deviennent une erreur",
    ),
    (
        GE,
        "return px >= r.x && px < r.droite() && py >= r.y && py < r.bas();",
        "return px >= r.x && px <= r.droite() && py >= r.y && py < r.bas();",
        TG,
        RECTS,
        "un doigt posé sur le bord droit d'une carte est dedans, et sur le bord gauche de sa voisine aussi : deux cartes touchées",
    ),
    (
        GE,
        "const int v = (px * petit_cote + 360) / 720;",
        "const int v = (px * petit_cote) / 720;",
        TG,
        ECHELLE,
        "l'échelle tronque au lieu d'arrondir : toutes les zones rétrécissent d'un pixel sur la 7B",
    ),
    (
        GE,
        "return v < 1 ? 1 : v;",
        "return v;",
        TG,
        ECHELLE,
        "une petite mesure tombe à zéro pixel sur un petit écran : un filet qui disparaît",
    ),
    # ---- la page ------------------------------------------------------------------------------------------------------------------------------
    (
        GE,
        "inline constexpr int PETIT_COTE_MIN = 320;",
        "inline constexpr int PETIT_COTE_MIN = 200;",
        TG,
        PETIT,
        "une page est proposée sur un écran où rien ne tient",
    ),
    (
        GE,
        "d.portrait = hauteur > largeur;",
        "d.portrait = hauteur >= largeur;",
        TG,
        SENS,
        "un écran carré passe en portrait : quatre cartes en deux rangées sur un écran qui n'a pas la hauteur",
    ),
    (
        GE,
        "d.polices = Polices{mis(44, m), mis(30, m), mis(22, m), mis(18, m)};",
        "d.polices = Polices{mis(44, m), mis(30, m), mis(22, m), mis(22, m)};",
        TG,
        POLICES,
        "la petite police prend la taille de la police du texte : plus de hiérarchie, des lignes qui ne tiennent plus",
    ),
    (
        GE,
        "const int w_fermer = mis(170, m);",
        "const int w_fermer = mis(60, m);",
        TG,
        DOIGT,
        "la zone tactile de « Fermer » rétrécit : un doigt ne la trouve plus du premier coup",
    ),
    (
        GE,
        "(largeur_utile - d.ecart) * 2 / 3, h_bas};",
        "(largeur_utile - d.ecart) * 3 / 3, h_bas};",
        TG,
        ZONES,
        "la marée prend toute la largeur : le soleil se retrouve avec une largeur négative",
    ),
    (
        GE,
        "d.cartes[i] = Rect{d.marge + i * (w_carte + d.ecart), y_cartes, w_carte, h_cartes};",
        "d.cartes[i] = Rect{d.marge + i * w_carte, y_cartes, w_carte, h_cartes};",
        TG,
        CARTES,
        "les cartes se touchent sans intervalle : deux doigts, une seule zone de verre",
    ),
    (
        GE,
        "const int w_carte = (largeur_utile - 3 * d.ecart) / 4;",
        "const int w_carte = (largeur_utile - 3 * d.ecart) / 3;",
        TG,
        ZONES,
        "chaque carte prend un tiers de la largeur : la quatrième sort de l'écran",
    ),
    (
        GE,
        "y_cartes + (i / 2) * (h_carte + d.ecart),",
        "y_cartes + (i / 2) * h_carte,",
        TG,
        CARTES,
        "les deux rangées de cartes se touchent en portrait : plus d'intervalle",
    ),
    (
        GE,
        "if (h_panneau > h_panneau_max)",
        "if (h_panneau > h_panneau_max + 40)",
        TG,
        PORTRAIT_COURT,
        "la marée et le soleil grossissent sur un grand écran au lieu de laisser la place aux cartes",
    ),
    (
        GE,
        "const int h_panneau_min = mis(150, m);",
        "const int h_panneau_min = mis(100, m);",
        TG,
        PORTRAIT_COURT,
        "la marée peut se rétrécir jusqu'à 100 : la courbe ne se lit plus (la limite de la page bouge)",
    ),
    (
        GE,
        "hauteur - d.marge - y_cartes - 2 * mesures_carte(m).hauteur_min - 4 * d.ecart - 2 * h_legende;",
        "hauteur - d.marge - y_cartes - 2 * mesures_carte(m).hauteur_min - 3 * d.ecart - 2 * h_legende;",
        TG,
        HAUTEURS,
        "un intervalle n'est pas compté : les cartes perdent quelques pixels sur les écrans les plus courts, leur contenu se recouvre",
    ),
    # ---- l'intérieur d'une carte --------------------------------------------------------------------------------------------------------------
    (
        GE,
        "2 * r.pad + r.h_nom + r.g + r.h_verdict + r.g + r.h_raison + r.h_milieu + RANGEES_CARTE * r.h_rangee;",
        "2 * r.pad + r.h_nom + r.g + r.h_verdict + r.g + r.h_raison + r.h_milieu;",
        TG,
        MINIMALE,
        "la hauteur minimale d'une carte oublie les rangées de critères : la page promet une place qu'elle n'a pas",
    ),
    (
        GE,
        "r.h_milieu = r.h_etiquette + r.h_creneau + r.g + r.h_bandes + r.sous_bandes + r.h_axe;",
        "r.h_milieu = r.h_etiquette + r.h_creneau + r.g + r.h_bandes + r.h_axe;",
        TG,
        MINIMALE,
        "le milieu d'une carte oublie l'écart sous les barres : l'axe touche les barres",
    ),
    (
        GE,
        "c.pas_barre = in.w / HEURES;",
        "c.pas_barre = (in.w + HEURES) / HEURES;",
        TG,
        BARRES,
        "le pas des barres arrondit vers le haut : les 24 barres sortent de la carte",
    ),
    (
        GE,
        "c.bandes = Rect{in.x + (in.w - largeur_bandes) / 2, ym, largeur_bandes, h_bandes};",
        "c.bandes = Rect{in.x, ym, largeur_bandes, h_bandes};",
        TG,
        BARRES,
        "les barres ne sont plus centrées : un vide à droite que le rendu montre de travers",
    ),
    # ---- la marée et le soleil -----------------------------------------------------------------------------------------------------------------
    (
        GE,
        "r.courbe = Rect{in.x, y3, in.w, in.bas() - y3};",
        "r.courbe = Rect{in.x, y3, in.w, in.bas() - y3 + 40};",
        TG,
        MAREE,
        "la courbe de marée dépasse du panneau",
    ),
    (
        GE,
        "r.avis = Rect{in.x, y2, in.w, in.bas() - y2};",
        "r.avis = Rect{in.x, y2, in.w, in.bas() - y2 + 40};",
        TG,
        SOLEIL,
        "l'avis du soleil dépasse du panneau",
    ),
    # ---- le détail -------------------------------------------------------------------------------------------------------------------------------
    (
        GE,
        "if (rangees < 1)",
        "if (rangees < 0)",
        TG,
        EPOUSE,
        "zéro rangée : le panneau du détail se calcule pour un critère qui n'existe pas",
    ),
    (
        GE,
        "const int cote_croix = mis(64, m);",
        "const int cote_croix = mis(30, m);",
        TG,
        DOIGT,
        "la croix du détail est trop petite pour un doigt",
    ),
    (
        GE,
        "const int parts[COLONNES_DETAIL] = {30, 22, 20, 28};",
        "const int parts[COLONNES_DETAIL] = {50, 32, 30, 28};",
        TG,
        DETAIL,
        "les trois premières colonnes du détail font 112 % de la largeur : la dernière (le reste) a une largeur négative",
    ),
    (
        GE,
        "t.avis = Rect{in.x, y, in.w, in.bas() - y};",
        "t.avis = Rect{in.x, y, in.w, in.bas() - y + 40};",
        TG,
        DETAIL,
        "l'avis du détail dépasse du panneau",
    ),
    (
        GE,
        "t.panneau = Rect{mx, (hauteur - h) / 2, largeur - 2 * mx, h};",
        "t.panneau = Rect{mx, (hauteur - h) / 3, largeur - 2 * mx, h};",
        TG,
        EPOUSE,
        "le panneau du détail n'est plus centré : collé en haut, un grand vide en bas",
    ),
]
