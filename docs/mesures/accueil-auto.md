# Accueil, mode automatique : ce que les tests du noyau prouvent, et ce qui reste à prouver — PC seulement, aucune carte

> **Summary (EN)** — The pure core of the home screen's automatic mode (`accueil_auto.hpp`, ADR-0015) was run on the PC only.
> - **Measured**: 48 host tests pass, compiled **and executed** with AddressSanitizer + UBSan (clang++ 23.1.5) and also evaluated at
>   compile time by g++ 14.2 (ESP-IDF's cross compiler, syntax mode: nothing executed). 98 targeted mutations (one rule, threshold,
>   priority or guard broken at a time) are **all detected**, each by the test that is named for it. Two tests re-read the rules
>   another way: a table of all 4 608 combinations of the inputs that matter, and a whole day simulated minute by minute with
>   random inputs (at most one change per 30 min, none during a touch).
> - **Not verified**: everything on a screen or a board (nothing calls this core yet), the taste of the thresholds (60 min, 3 h,
>   30 min, 10:00 / 16:00), the real data feeding the inputs, the words of the pastille and the panel (not written yet).

**Vu sur** : PC seulement (Windows 11, clang++ 23.1.5 avec AddressSanitizer + UBSan via `env_hote.ps1`, Python 3 ; g++ 14.2.0 croisé
d'ESP-IDF en mode « syntaxe »), aucune carte, `99a9f14` + ce changement, 08/11/2026.

Règle de lecture : « mesuré » = relu sur une sortie d'outil de cette session ; tout le reste est une lecture de code ou **non vérifié**.
Les règles sont dans [l'ADR-0015](../decisions/0015-accueil-mode-automatique.md) ; **aucun seuil ne vient d'une source officielle**.

## 1. Question

Les règles du mode automatique (alerte, pluie proche, Surf, Vent·Bateau, Côte, Météo) et ses garde-fous (un changement par 30 min,
jamais pendant un toucher, donnée périmée, épinglage, maison sans spot, entrées inconnues) sont-ils écrits de façon que le test
**échoue** quand on en casse une ?

## 2. Mesuré

| Quoi | Résultat | Comment |
|---|---|---|
| Tests du noyau, compilés **et exécutés** | 48 sur 48 | `python test/host/lancer.py --seulement test_accueil_auto`, mode « complet », clang++ 23.1.5, ASan + UBSan |
| Mêmes tests, évalués à la compilation (`static_assert`) | 48 sur 48 | même commande avec `--cxx` = g++ 14.2.0 d'ESP-IDF, mode « syntaxe » (rien d'exécuté) |
| Mutations ciblées détectées | 98 sur 98 avec clang++ ; 98 sur 98 avec g++ 14.2 (mode « syntaxe », 38 s) | `python test/host/lancer.py --mutations --seulement test_accueil_auto` : chaque mutation change **un** texte de l'en-tête, la copie saine compile, la mutée doit échouer **et** citer le test prévu |
| Combinaisons d'entrées relues autrement | 4 608 | une table (`toutes_les_combinaisons_d_entrees_suivent_la_table_de_l_adr`) : 9 booléens, 2 fraîcheurs à 3 valeurs, valeurs de part et d'autre de chaque seuil (59/60, 180/181, 250/251) |
| Journée simulée à la minute | 1 440 pas | `sur_une_journee_au_plus_un_changement_par_30_minutes_et_aucun_pendant_un_toucher` : entrées au hasard (suite fixe), jamais deux changements à moins de 30 min, aucun pendant un toucher ; le test exige au moins 5 changements et 5 « différés » pour ne pas être vide |

Les 98 mutations, par règle :

| Règle ou garde-fou | Mutations | Exemples de ce qui est cassé (le test rougit à chaque fois) |
|---|---|---|
| 1. alerte orange / rouge | 9 | `>=` devient `>` (l'orange n'agrandit plus) ; le niveau minimal devient jaune ou rouge ; la pastille dit orange pour du rouge ; le bandeau dépend de l'épinglage, du toucher ou du profil montré |
| 2. pluie proche | 10 | `<` devient `<=` (60 min compte) ; le seuil devient 59 ou 61 ; la pluie déjà en cours (0 min) perd sa carte ; « aucune pluie » (-1) en a une ; un profil épinglé en a une |
| 3. Surf et créneau | 27 | 10 h pile dans le créneau ; 16 h pile hors du créneau ; le coucher inclus ; le matin sans le lever ; un lever d'hier ou un coucher de demain accepté ; décalage du lieu ignoré ou soustrait ; reste négatif d'une division non ramené dans le jour ; lever, coucher ou heure à 0 acceptés ; Surf sans spot, sans activité, sans créneau, avec une donnée inconnue ou périmée |
| 4. Vent·Bateau | 6 | `>` devient `>=` ; seuil non réglé qui laisse passer tout vent ; sans l'activité ; avec un spot exigé à tort ; avec des prévisions inconnues |
| 5. Côte | 7 | 180 min exclues ; seuil à 120 ou 240 ; « maintenant » (0) exclu ; -1 pris pour proche ; sans spot de pêche à pied ; avec des données inconnues |
| Ordre 3 > 4 > 5 | 3 | Vent·Bateau avant Surf ; Côte avant Vent·Bateau ; Côte avant les deux |
| Donnée périmée | 9 | une donnée périmée non notée (houle, vent, marée) ; « inconnue » prise pour périmée ; retour à Météo sans la raison, ou avec la raison à l'envers ; le délai de 30 min la retient ; elle passe sous le doigt ; elle ne laisse pas la place à la règle suivante |
| Un changement par 30 min | 14 | 30 min pile refusées ; seuil à 29 ou 31 min ; changement à la même seconde permis ; horloge revenue en arrière qui bloque ; premier choix retenu ; heure du changement non renouvelée ou renouvelée à tort ; tout retour à Météo passe devant le délai ; un profil gardé dit la raison du profil refusé |
| Jamais pendant un toucher, jamais sans heure | 2 | le profil change sous le doigt ; le profil change sans heure |
| Épinglage | 11 | épinglage hors table suivi ; épinglage sans le drapeau ; l'alerte efface « épinglé » de la pastille ; l'épinglage compte comme un changement automatique ; état hors table repris tel quel ; constantes (nombre de profils, de raisons, valeur « inconnu », durée d'un jour) |

Mutations **non écrites**, parce qu'elles ne changent aucun comportement : les contrôles redondants (par exemple « le vent est positif »
quand le seuil l'est déjà) ont été **retirés du code** plutôt que protégés par un test.

## 3. Ce que ce n'est pas

- Rien n'appelle ce noyau : `main/` n'est pas touché, aucune compilation ESP-IDF n'a été lancée pour ce changement, rien n'a été
  flashé ni affiché. Le coût sur le P4 n'est pas mesuré.
- Les tests sont écrits par la même personne que le code, à partir du même texte (la proposition d'Axel et ses esquisses). Ils prouvent
  que le code fait ce que l'ADR écrit, **pas** que l'ADR décrit un bon accueil : c'est à Axel de le juger sur l'écran réel.
- Les instants des tests (8 novembre 2026, Paris, lever à 8 h, coucher à 17 h 30) sont des **valeurs d'exemple**, pas un lever de
  soleil constaté.
- `lancer.py --mutations` sur **toute** la liste (1 530 mutations selon [outils.md](../outils.md)) n'a **pas** été relancé en entier :
  seule la partie de ce noyau l'a été (`--seulement test_accueil_auto`). Le job `mutations-noyau` de la CI, lancé à la main, contient
  désormais les 98.
