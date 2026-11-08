"""Mutations du noyau « boutons de la plage de vent de la page La mer de l'écran » (components/core/include/ecran/mer_reglage.hpp,
ADR-0026), lues par lancer.py --mutations.

Même format que MUTATIONS_MER_API : (fichier du noyau, texte exact à remplacer — il doit apparaître UNE fois —, remplacement,
test qui doit échouer, nom de la fonction de test qui doit être citée, ce que la mutation simule). Une mutation « non détectée » ou
« détectée par autre chose » fait échouer le lanceur.

Chaque mutation casse UNE règle d'un bouton : il décale la bonne borne, d'un nœud, par les règles de l'API (jamais un réglage que
l'appareil refuserait), et le bouton grisé dit vrai exactement quand le décalage passe.
"""

R = "mer_reglage.hpp"
T = "test_mer_reglage.cpp"

MUTATIONS_MER_REGLAGE = [
    (R, "if (borne == Borne::Min)\n        mini += pas;", "if (borne == Borne::Max)\n        mini += pas;",
     T, "chaque_bouton_decale_une_seule_borne_d_un_noeud", "le bouton du minimum décale le maximum, et inversement"),
    (R, "inline constexpr int PAS_VENT_NOEUDS = 1;", "inline constexpr int PAS_VENT_NOEUDS = 2;",
     T, "chaque_bouton_decale_une_seule_borne_d_un_noeud", "un toucher décale la borne de deux nœuds au lieu d'un"),
    (R, "    return plage_de_vent_valide(mini, maxi);\n}", "    return true;\n}",
     T, "vent_decalable_dit_vrai_exactement_quand_le_decalage_passe", "un bouton reste actif alors que son décalage serait refusé"),
    (R, "    return regler_vent(r, mini, maxi);\n}",
     "    r.vent_min_kn = static_cast<std::int16_t>(mini);\n    r.vent_max_kn = static_cast<std::int16_t>(maxi);\n    return ErreurRegistre::Aucune;\n}",
     T, "le_minimum_ne_peut_ni_toucher_le_maximum_ni_tomber_a_zero", "un décalage refusé par l'API est quand même écrit dans le registre"),
]
