"""Mutations du noyau « mots et phrases de la page mer » (components/core/include/ecran/mer_textes.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_MER : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, test qui doit
échouer, nom de la fonction de test qui doit être citée (None : n'importe laquelle, y compris une erreur de compilation voulue), ce que la
mutation simule). Une mutation « non détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle écrite dans le fichier : l'heure du lieu, le créneau, l'âge d'une donnée, l'arrondi et le signe d'un nombre,
le séparateur décimal de la langue, les unités, les points d'un critère, la raison d'une carte, une marée, le coefficient, la vigilance côtière
(jamais un « tout va bien » quand elle est illisible), l'âge de la mer, le choix du bandeau. Ce sont des mutations choisies à la main sur les
règles de ce fichier, pas un balayage automatique : elles ne disent pas que « tout » est testé, seulement que ces règles-là le sont. Les
traductions elles-mêmes (le contenu des tables) ne sont pas mutées : elles sont écrites par une IA, non relues par des locuteurs natifs, et
le test ne contrôle que leur forme (six colonnes, marqueurs gardés, caractères dans la police), pas leur justesse.
"""

TT = "test_mer_textes.cpp"
TX = "mer_textes.hpp"

HEURE = "une_heure_s_ecrit_hh_mm_a_l_heure_du_lieu"
SANS_INSTANT = "sans_instant_il_n_y_a_pas_d_heure"
CRENEAU = "un_creneau_s_ecrit_avec_un_tiret_demi_cadratin"
AGE = "un_age_s_ecrit_dans_la_langue_avec_la_bonne_unite"
AGE_SANS = "un_age_sans_horloge_ou_sans_evenement_ne_se_dit_pas"
VALEUR = "une_valeur_s_ecrit_avec_son_unite_et_le_separateur_de_la_langue"
ARRONDI = "une_valeur_entiere_s_arrondit_au_plus_proche_moitie_loin_de_zero"
NEGATIF = "une_valeur_negative_garde_son_signe"
INCONNUE = "une_valeur_ou_une_unite_inconnue_ne_s_ecrit_pas"
POINTS = "les_points_d_un_critere_s_ecrivent_n_sur_max_ou_se_taisent"
NOTE = "la_note_s_ecrit_dans_la_langue_ou_se_tait"
RAISON = "la_raison_dit_le_critere_et_ce_que_la_regle_en_a_fait"
RAISON_INCONNUE = "une_raison_inconnue_ou_hors_vocabulaire_ne_s_ecrit_pas"
MAREE = "une_maree_s_ecrit_avec_son_heure_et_sa_hauteur"
HAUTEUR = "la_hauteur_d_une_maree_s_arrondit_au_dixieme_de_metre"
MAREE_SANS = "une_maree_sans_hauteur_ou_sans_heure"
COEFFICIENT = "le_coefficient_dit_qu_il_est_estime"
VIGILANCE = "la_vigilance_dit_son_niveau_et_sa_fin"
ILLISIBLES = "des_vigilances_illisibles_ne_sont_jamais_un_tout_va_bien"
RIEN_A_DIRE = "pas_de_vigilance_ou_verte_ou_mer_absente_rien_a_dire"
ETAT = "l_etat_de_la_mer_dit_l_absence_la_peremption_ou_l_age"
BANDEAU = "le_bandeau_choisit_le_plus_important"
PAIRE = "le_meme_etat_n_a_pas_le_meme_mot_selon_le_critere"

MUTATIONS_MER_TEXTES = [
    # ---- les heures et les âges ------------------------------------------------------------------------------------------------------
    (
        TX,
        "const int m = static_cast<int>(s / 60 % 60);",
        "const int m = static_cast<int>(s / 60 % 59);",
        TT,
        HEURE,
        "les minutes se calculent modulo 59 : « 14:59 » s'écrit « 14:00 », un créneau qui finit à l'heure pile se décale",
    ),
    (
        TX,
        "if (instant_s <= 0) {",
        "if (instant_s < 0) {",
        TT,
        SANS_INSTANT,
        "l'instant 0 (horloge pas réglée) s'écrit « 00:00 » au lieu de ne rien dire : on affiche minuit au spot",
    ),
    (
        TX,
        r'p.texte("\xE2\x80\x93");',
        'p.texte("-");',
        TT,
        CRENEAU,
        "le créneau s'écrit avec un trait d'union et non le tiret demi-cadratin : la police et la typographie changent",
    ),
    (
        TX,
        "fin_s <= debut_s",
        "fin_s < debut_s",
        TT,
        CRENEAU,
        "un créneau qui finit à l'instant où il commence s'écrit « 08:00–08:00 » au lieu de ne rien dire",
    ),
    (
        TX,
        "if (delta < 60) {",
        "if (delta < 120) {",
        TT,
        AGE,
        "une donnée vieille de deux minutes est « à l'instant » : l'âge ment par défaut",
    ),
    (
        TX,
        "if (delta >= 86400) {",
        "if (delta >= 172800) {",
        TT,
        AGE,
        "un jour entier s'écrit « il y a 24 h » au lieu de « il y a 1 j »",
    ),
    (
        TX,
        "} else if (delta >= 3600) {",
        "} else if (delta >= 7200) {",
        TT,
        AGE,
        "une heure entière s'écrit « il y a 60 min » au lieu de « il y a 1 h »",
    ),
    (
        TX,
        "if (maintenant_s <= 0 || evenement_s <= 0) {",
        "if (maintenant_s <= 0 || evenement_s < 0) {",
        TT,
        AGE_SANS,
        "un événement sans date (0) donne un âge de 56 ans : « il y a 20000 j »",
    ),
    # ---- les nombres et les unités ----------------------------------------------------------------------------------------------------
    (
        TX,
        "return d >= 0 ? (d + 5) / 10 : -((-d + 5) / 10);",
        "return d >= 0 ? d / 10 : -((-d + 5) / 10);",
        TT,
        ARRONDI,
        "un nombre positif se tronque au lieu de s'arrondir : 2,6 s'écrit 2",
    ),
    (
        TX,
        "return d >= 0 ? (d + 5) / 10 : -((-d + 5) / 10);",
        "return d >= 0 ? (d + 5) / 10 : (d + 5) / 10;",
        TT,
        ARRONDI,
        "un nombre négatif s'arrondit vers zéro au lieu de s'éloigner de zéro : -1,5 s'écrit -1",
    ),
    (
        TX,
        "const bool negatif = v < 0;",
        "const bool negatif = false;",
        TT,
        NEGATIF,
        "un entier négatif perd son signe : −3 s'écrit 3",
    ),
    (
        TX,
        "r.c[r.n++] = langue == Langue::En ? '.' : ',';",
        "r.c[r.n++] = ',';",
        TT,
        VALEUR,
        "le séparateur décimal ne suit plus la langue : l'anglais écrit « 2,6 m »",
    ),
    (
        TX,
        'p.texte(" kn");',
        'p.texte(" kt");',
        TT,
        VALEUR,
        "l'unité du vent change de sigle : « 11,9 kt » au lieu de « 11,9 kn »",
    ),
    (
        TX,
        'p.texte(" min");',
        'p.texte(" mn");',
        TT,
        VALEUR,
        "l'unité des minutes change de sigle",
    ),
    (
        TX,
        "if (c.valeur_d == INCONNU) {",
        "if (c.valeur_d == 0) {",
        TT,
        INCONNUE,
        "une valeur inconnue s'écrit comme un nombre et un vrai zéro se tait : « Houle 0 m » devient « Houle  »",
    ),
    (
        TX,
        "if (c.points < 0 || c.points_max <= 0 || c.points > c.points_max) {",
        "if (c.points < 0 || c.points_max <= 0 || c.points >= c.points_max) {",
        TT,
        POINTS,
        "les points au maximum (2/2) ne s'écrivent plus : le meilleur score se tait",
    ),
    (
        TX,
        "if (points < 0 || sur <= 0 || points > sur) {",
        "if (points < 0 || sur <= 0 || points >= sur) {",
        TT,
        NOTE,
        "la note maximale (8 sur 8) ne s'écrit plus",
    ),
    # ---- les raisons et les marées ------------------------------------------------------------------------------------------------------
    (
        TX,
        "if (etat.empty() || critere.empty()) {",
        "if (etat.empty() && critere.empty()) {",
        TT,
        RAISON_INCONNUE,
        "une raison à moitié connue s'écrit avec un trou : « Houle :  » ou « : trop petite »",
    ),
    (
        TX,
        "if (t.critere == critere && t.etat == etat)",
        "if (t.etat == etat)",
        TT,
        PAIRE,
        "le mot d'un état ne dépend plus du critère : « Houle : trop faible » au lieu de « Houle : trop petite »",
    ),
    (
        TX,
        "if (modele[i] == '{') {",
        "if (false) {",
        TT,
        RAISON,
        "les marqueurs {critere} et {etat} ne sont plus remplacés : l'écran montre les accolades",
    ),
    (
        TX,
        "const std::int64_t d = cm >= 0 ? (cm + 5) / 10 : -((-cm + 5) / 10);",
        "const std::int64_t d = cm >= 0 ? cm / 10 : -((-cm + 5) / 10);",
        TT,
        HAUTEUR,
        "la hauteur d'une marée se tronque au lieu de s'arrondir : 1,86 m s'écrit 1,8 m",
    ),
    (
        TX,
        "p.texte(texte(m.haute ? Texte::PleineMer : Texte::BasseMer, langue));",
        "p.texte(texte(m.haute ? Texte::BasseMer : Texte::PleineMer, langue));",
        TT,
        MAREE,
        "pleine mer et basse mer sont échangées : un pêcheur à pied arrive à marée haute",
    ),
    (
        TX,
        "if (m.hauteur_cm != INCONNU) {",
        "if (true) {",
        TT,
        MAREE_SANS,
        "une marée sans hauteur écrit quand même une hauteur : un nombre qui n'existe pas",
    ),
    (
        TX,
        "if (valeur == 0) {",
        "if (valeur == 1) {",
        TT,
        COEFFICIENT,
        "un coefficient absent (0) s'écrit « Coefficient 0 (estimé) »",
    ),
    # ---- la vigilance côtière, l'état de la mer et le bandeau -------------------------------------------------------------------------------
    (
        TX,
        "if (!mer.vigilances_lues) {",
        "if (false) {",
        TT,
        ILLISIBLES,
        "des vigilances qu'on n'a pas pu lire ne disent plus rien : un « tout va bien » par défaut",
    ),
    (
        TX,
        "niveau = Texte::NiveauRouge;",
        "niveau = Texte::NiveauOrange;",
        TT,
        VIGILANCE,
        "une vigilance rouge s'écrit orange : on minimise le danger",
    ),
    (
        TX,
        "if (!mer.vigilance_presente || niveau == Texte::Nombre) {",
        "if (!mer.vigilance_presente) {",
        TT,
        RIEN_A_DIRE,
        "une vigilance verte s'écrit comme une vigilance : « Vigilance côtière {niveau} »",
    ),
    (
        TX,
        "if (formater_heure(mer.vigilance_fin_s, decalage_s, h, sizeof h) != 0) {",
        "if (formater_heure(mer.vigilance_fin_s, decalage_s, h, sizeof h) == 0) {",
        TT,
        VIGILANCE,
        "la fin de la vigilance n'est plus écrite quand on la connaît (et un « 00:00 » inventé quand on ne la connaît pas)",
    ),
    (
        TX,
        "if (mer.donnees_gardees) {",
        "if (false) {",
        TT,
        ETAT,
        "des données qu'un fournisseur a gardées s'écrivent comme de simples données un peu vieilles",
    ),
    (
        TX,
        "return mer.vigilances_lues ? Bandeau::Vigilance : Bandeau::VigilanceIllisible;",
        "return Bandeau::Vigilance;",
        TT,
        BANDEAU,
        "des vigilances illisibles sont peintes comme une vigilance lue : la page choisit une couleur de niveau qu'on n'a pas",
    ),
    (
        TX,
        "if (f == Fraicheur::Rien || f == Fraicheur::Expiree)",
        "if (f == Fraicheur::Rien)",
        TT,
        BANDEAU,
        "une vigilance de la veille passe devant « données trop anciennes » : le bandeau rassure ou alarme sur du périmé",
    ),
]
