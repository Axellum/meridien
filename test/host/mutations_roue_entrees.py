"""Mutations du noyau « roue, entrées du premier anneau » (components/core/include/ecran/roue_entrees.hpp), lues par lancer.py --mutations.

Même format que MUTATIONS_ROUE_COMMANDE : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou
« détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle : « Réglages » premier, « Détails » second, « Alertes » seulement quand la page existe, « Mer » seulement
quand un spot est choisi et en dernier. Mutations choisies à la main sur les règles de ce fichier, pas un balayage automatique.
"""

ENT = "test_roue_entrees.cpp"

H = "roue_entrees.hpp"

MUTATIONS_ROUE_ENTREES = [
    # --- les deux entrées qui sont toujours là ---
    (H, "liste.entree[liste.n++] = Entree::Reglages;", "liste.entree[liste.n++] = Entree::Details;",
     ENT, "reglages_est_toujours_le_premier_et_details_le_second", "« Réglages » n'est plus le premier bouton : les réglages ne sont plus atteignables d'un geste connu"),
    (H, "liste.entree[liste.n++] = Entree::Details;\n    if (p.alertes) {", "liste.entree[liste.n++] = Entree::Reglages;\n    if (p.alertes) {",
     ENT, "reglages_est_toujours_le_premier_et_details_le_second", "« Détails » disparaît du second rang (les réglages sont écrits deux fois)"),
    # --- les entrées conditionnelles ---
    (H, "if (p.alertes) {", "if (!p.alertes) {",
     ENT, "avec_les_alertes_seules_elles_viennent_apres_details", "« Alertes » apparaît quand la page des alertes n'existe pas, et disparaît quand elle existe"),
    (H, "if (p.alertes) {", "if (true) {",
     ENT, "avec_la_mer_seule_elle_vient_apres_details", "« Alertes » est toujours là, même sans page des alertes"),
    (H, "if (p.mer) {", "if (!p.mer) {",
     ENT, "avec_la_mer_seule_elle_vient_apres_details", "« Mer » apparaît sans spot choisi, et disparaît quand un spot est choisi"),
    (H, "if (p.mer) {", "if (true) {",
     ENT, "sans_alerte_ni_mer_la_roue_a_reglages_et_details", "« Mer » est toujours là, même sans spot choisi : un bouton vers une page vide"),
    (H, "if (p.mer) {", "if (p.mer && !p.alertes) {",
     ENT, "avec_les_deux_la_mer_est_la_derniere", "« Mer » disparaît dès que la page des alertes existe"),
    # --- l'ordre : la mer vient en dernier, sans déplacer personne ---
    (H, "liste.entree[liste.n++] = Entree::Mer;", "liste.entree[liste.n++] = Entree::Alertes;",
     ENT, "aucune_entree_n_est_en_double_et_le_premier_anneau_les_porte_toutes", "« Mer » ouvre en réalité les alertes (deux boutons identiques)"),
    (H, "liste.entree[liste.n++] = Entree::Alertes;", "liste.entree[liste.n++] = Entree::Mer;",
     ENT, "avec_les_alertes_seules_elles_viennent_apres_details", "« Alertes » ouvre en réalité la mer"),
    (H, "inline constexpr int ENTREES_MAX = 4;", "inline constexpr int ENTREES_MAX = 3;",
     ENT, "le_nombre_d_entrees_est_celui_des_presences", "le nombre maximal d'entrées annoncé ne compte plus la mer"),
]
