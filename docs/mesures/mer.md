# Mer : ce que les trames de Vigie donnent au noyau, et ce qui reste à prouver — PC seulement, aucune carte

> **Summary (EN)** — The pure core of the future "sea" page (`mer_trame.hpp`, `mer_vue.hpp`) was run on **six real frames** of the Vigie
> embedded-frame API (French Atlantic and Channel spots, captured 2026-11-08 between 13:15:38 and 13:15:53 UTC, kept in
> `test/host/donnees/`), on the PC only.
> - **Measured**: the CRC-32 of all six frames is right and equals zlib's; every value the core reads (name, instants, six tides, three
>   coefficients, four activities with verdict, reason, slot, two 24-hour strips and their criteria) is identical to an independent
>   Python reader's; the integer tide curve stays within 6.07 mm of a floating-point calculation at 46 instants per frame (rounding to
>   the centimetre included); 72 host tests pass under AddressSanitizer and UBSan; the two compile-time test files also build with
>   ESP-IDF's GCC 14.2; 150 targeted mutations are all detected.
> - **Not verified**: anything on a board; a real frame with a coastal warning or a "good" surf (none existed that day: the coastal
>   warning and the "good" surf are tested only on hand-edited, re-signed frames); the freshness thresholds in use; how the
>   verdicts match what is seen at the spot.

**Vu sur** : PC seulement (Windows 11, clang++ 23.1.5 avec AddressSanitizer + UBSan via `env_hote.ps1 -Sanitizers`, g++ 14.2 croisé RISC-V d'ESP-IDF en `-fsyntax-only`, Python 3), aucune carte ; trames réelles capturées le 08/11/2026 de 13:15:38 à 13:15:53 UTC (UNIX 1 794 143 738 à 1 794 143 753) depuis le PC d'Axel ; branche `claude/mer-noyau`, base `4dd7b2c`. Aucune MAC ni adresse IP ici.

Règle de lecture : « mesuré » = relu sur une sortie d'outil de cette session ; tout le reste est une lecture de code ou **non vérifié**. Le détail des règles est dans [l'ADR-0018](../decisions/0018-mer-trame-vigie-et-vues.md).

## 1. Question

Les trames embarquées de Vigie pour un spot (lignes `SEA`, `ACT`, `CRIT`), lues par le noyau pur de la mer, donnent-elles ce que la page « Décision en 2 secondes » doit montrer, et que valent les règles de lecture (CRC, valeurs inconnues, âge, courbe de marée) sur ces données ?

## 2. Mesuré : six spots, une trame chacun

« Maintenant » = l'instant de génération de la trame. Valeurs relues par le lecteur Python indépendant (`controle_python_mer.py`) ; le noyau C++ donne les mêmes (tests `test_mer_trame.cpp` et `test_mer_vue.cpp`). Hauteur à l'instant de la trame : calcul à virgule du lecteur Python (le noyau, en entiers, s'en écarte de moins de 8 mm, § 3).

| Spot | Octets | CRC-32 (ligne `END`) | Écart des 6 marées | Surf | Kite | Bateau | Pêche à pied | Hauteur maintenant |
|---|---|---|---|---|---|---|---|---|
| Soorts-Hossegor, plage Sud | 4 992 | `CB507BDC` | 343 cm | déconseillé (houle trop petite, 4/8) | déconseillé (vent trop faible) | bon | bon | 120,8 cm |
| Beuzec, plage de la Torche | 5 019 | `BBDDF1E3` | 396 cm | possible (vent fort, 4/8) | possible (rafales) | bon | bon | 128,4 cm |
| Saint-Malo, plage de Rochebonne | 5 017 | `D5D9EB74` | 965 cm | déconseillé (houle trop petite, 4/8) | possible (vent un peu faible) | bon | bon | −344,3 cm |
| Biarritz, plage du Port Vieux | 5 019 | `6B848522` | 343 cm | déconseillé (houle trop petite, 2/8) | possible (vent un peu faible) | bon | bon | 120,9 cm |
| Le Pouliguen, plage de la Govelle | 5 022 | `AB419566` | 413 cm | déconseillé (houle trop petite, 5/8) | possible (vent un peu faible) | bon | bon | 110,1 cm |
| Hendaye, Ondarraitz | 5 001 | `77EBB53D` | 342 cm | déconseillé (houle trop petite, 3/8) | possible (rafales) | bon | bon | 120,3 cm |

« Écart des 6 marées » = la plus haute des six hauteurs de marée listées moins la plus basse (hauteurs rapportées au niveau moyen, pas au zéro des cartes).

Lecture :
- **Une trame réelle garde le même gabarit**: une ligne `SEA` de 18 champs, quatre lignes `ACT` de 12 champs, quinze `CRIT` de 9, six marées, trois coefficients (83, 84, 83) ; les lignes `HEAD`, `ZONE`, `RAIN5M`, `DAY`, `HAZ` sont sautées sans perdre le lien entre une activité et ses critères.
- **La meilleure activité est le bateau sur les six trames** (premier « bon » dans l'ordre surf, kite, bateau, pêche à pied) ; aucune ne donne un surf « bon ».
- **Les vigilances côtières sont « lues, aucun avertissement »** (état `ok`, niveau vide) sur les six : aucune trame réelle ne montre un avertissement côtier. Ce cas est testé sur des trames réécrites à la main et resignées.
- **Saint-Malo sort de l'ordinaire** : 965 cm entre la plus haute et la plus basse des six marées, et une hauteur négative à l'instant de la trame (−3,44 m) ; les hauteurs négatives et les grandes amplitudes sont arrondies loin de zéro (test dédié).

## 3. Mesuré : la courbe de marée en entiers

Écart entre la hauteur entière du noyau (centimètres) et le calcul à virgule du lecteur Python (millièmes de centimètre), sur 46 instants par trame — début et fin de chaque paire de marées (à une seconde près), quarts de la durée, et trois angles qui ne sont **pas** des degrés entiers (51,4° ; 69,2° ; 124,6°) :

| Spot | Écart maximal |
|---|---|
| Hossegor | 5,01 mm |
| La Torche | 5,00 mm |
| Rochebonne | 6,07 mm |
| Port Vieux | 5,01 mm |
| Govelle | 5,00 mm |
| Ondarraitz | 5,01 mm |

Le plus gros de l'écart vient de l'**arrondi au centimètre** de la sortie (au plus 5 mm). La valeur la plus haute, 6,07 mm à Saint-Malo (plus de 9 m entre les marées), laisse penser que le cosinus au millième du noyau du surf y ajoute un peu ; la part de chacun n'a pas été décomposée. Le test exige moins de 8 mm.

Ce défaut aurait pu rester invisible : mes premiers instants d'essai (les quarts de la durée entre deux marées) tombent sur des angles entiers (45°, 90°, 135°) où l'interpolation du cosinus entre deux degrés ne sert à rien ; une mutation qui la supprimait n'était pas vue. Les trois angles fractionnaires ont été ajoutés pour cette raison (§ 5).

## 4. Mesuré : ce que les tests exécutent

| Contrôle | Résultat | Outil |
|---|---|---|
| Tests du noyau « mer » | 72 tests (23 + 30 + 19), 0 échec, sous AddressSanitizer et UBSan | `python test/host/lancer.py --seulement test_mer` avec `env_hote.ps1 -Sanitizers` |
| Compilation avec GCC | les trois fichiers de test compilent avec g++ 14.2 (croisé RISC-V d'ESP-IDF, `-fsyntax-only`) ; les deux fichiers évalués à la compilation y passent leurs `static_assert` ; le troisième (`test_mer_variantes.cpp`) n'y est **pas exécuté** | `lancer.py --cxx <g++ d'ESP-IDF>` |
| Lecteur Python indépendant | 6 trames relues, tous les `.inc` à jour | `controle_python_mer.py --verifier` (appelé par le lanceur) |
| Mise en forme et Python | `clang-format` sans écart sur les six fichiers C++, `ruff` sans remarque | `clang-format --dry-run`, `ruff check` |

Ce que le lecteur Python **ne prouve pas** : il a été écrit par la même personne, d'après le même texte du contrat (`docs/embedded-frame.md` de `vigie-api`). Il est indépendant pour le CRC (zlib), la découpe des lignes, des marées et des bandes ; il ne l'est pas pour la lecture du contrat lui-même.

## 5. Mesuré : les mutations

150 mutations ciblées (`test/host/mutations_mer.py`, enregistrées dans `lancer.py`) : une règle cassée à la fois — le polynôme et les constantes du CRC, la ligne `END`, une borne incluse ou exclue, l'indice d'un champ de `SEA`/`ACT`/`CRIT`, une table de noms, la façon de traiter une valeur absente ou une vigilance illisible, les seuils d'âge, le sens de la courbe de marée. **Résultat sur l'état final des fichiers : 150 mutations, toutes détectées, chacune par le test visé.**

Première série (avant les corrections) : 3 mutations sur 150 n'étaient **pas** détectées — des trous de test, comblés :
1. *un niveau de vigilance lu même quand l'état dit « on n'a pas pu lire »* : le test regardait `vigilances_lues` et `vigilance_presente`, pas le niveau gardé ; il regarde maintenant le niveau, la fin et la source ;
2. *la lettre `z` d'un identifiant de spot* : aucun cas ne contenait de `z` ; les quatre bornes (`a`, `z`, `0`, `9`) et leurs voisins (`` ` ``, `{`, `/`, `:`, `@`, `Z`) sont maintenant essayées ;
3. *le cosinus non interpolé entre deux degrés* : voir § 3.

Limite : ces mutations sont **choisies à la main** sur les règles de ce noyau, pas un balayage automatique. Elles ne disent pas que tout est testé, seulement que ces 150 règles-là le sont.

## 6. Ce qui reste à prouver

- La page sur une carte, avec une trame reçue par le réseau (le client est le lot B).
- Une trame **réelle** avec un avertissement côtier orange ou rouge, et une autre avec un surf « bon ».
- Les seuils de fraîcheur (10 minutes, 6 heures) à l'usage, sur une carte posée dans une pièce.
- Que les verdicts de Vigie décrivent bien ce qu'on voit sur place : ce sont des règles de Vigie, pas des règles officielles, et **pas pour la sécurité**.

## 7. Lot B : le client réseau et le registre des spots — PC et compilation, aucune carte

> **Summary (EN)** — Lot B (network client + spot registry, [ADR-0019](../decisions/0019-mer-client-et-registre-de-spots.md)). **Measured**:
> 55 host tests (17 for the request and the pace, 38 for the registry) pass under AddressSanitizer and UBSan and also build with ESP-IDF's
> GCC 14.2; 68 targeted mutations are all detected (a first series found 8 gaps, all closed); the firmware builds without any compiler
> warning for the Tab5 (2 978 768 bytes) and the Waveshare 7B (2 959 248 bytes), 29 % of the 4 MiB app partition free on both.
> **Not verified**: everything that needs a board — a real request to the public instance, the free internal RAM with the new task,
> a 404 on an unknown spot, the NVS round trip, the interplay of the TLS lock with the alerts and the weather. Nothing was flashed.

**Vu sur** : PC (même poste qu'au § 2, clang++ avec AddressSanitizer + UBSan, g++ 14.2 croisé d'ESP-IDF en `-fsyntax-only`), puis compilation ESP-IDF 5.5.5 pour les deux cartes (`tools/idf.ps1`, `CMAKE_BUILD_PARALLEL_LEVEL=4`) ; branche `claude/mer-client`, base `f15bc05` (la branche du lot A). Aucune carte, aucun flash, aucune requête réseau réelle.

**Mesuré** :
- **55 tests** de l'hôte : 17 pour la requête et le rythme (`test_mer_requete.cpp`), 38 pour le registre (`test_mer_spots.cpp`) ; sous clang++ ASan + UBSan, et en `static_assert` avec le g++ d'ESP-IDF.
- **68 mutations ciblées** (`test/host/mutations_mer_client.py`, enregistrées dans `lancer.py`) : une règle cassée à la fois — les trois constantes de l'adresse, les bornes et les défauts de la plage de vent, les chiffres écrits, le plafond de 12 spots, le refus d'un doublon ou d'un identifiant qui n'est pas un slug, la façon dont le choix suit son spot quand on en retire un, la réparation d'un registre relu en NVS, ce que `registre_valide` accepte, le rythme (période, premier essai, anti-rebond). **Résultat sur l'état final : 68 mutations, toutes détectées.** Première série (67 mutations) : **8 problèmes**, tous des trous de test ou de code, comblés : (1) le test du pire cas utilisait `SPOT_ID_MAX` des deux côtés (une constante qui bouge passait) ; (2) le premier spot ajouté à un registre vide mais abîmé (choix resté à 3) n'était pas essayé ; (3) retirer un spot avant le choix : avec trois spots, le choix retombait sur le bon spot par hasard, il faut quatre spots ; (4) un choix juste après le dernier spot (`choisi == nombre`) n'était pas essayé ; (5) une mutation **équivalente** : la condition `nombre > 0` de `spot_choisi` était redondante avec `choisi < nombre`, elle a été retirée du code ; (6) deux registres aux noms différents n'étaient pas comparés ; (7) deux textes sans zéro final n'étaient pas essayés ; (8) le test du verdict d'un registre à corriger n'essayait pas de plage de vent refusée (un autre test la voyait). Deuxième série : 2 problèmes (la longueur et l'identifiant dans l'égalité des registres, que le nom masquait), comblés. Limite : mutations **choisies à la main** sur ces deux fichiers, pas un balayage ; la tâche réseau (`main/mer_client.cpp`) et la NVS (`main/reglages_mer.cpp`) ne sont **pas** dans le noyau et ne sont pas mutées.
- **Firmware compilé pour les deux cartes** : Tab5 `meridien.bin` 2 978 768 octets (SHA-256 `6A36C1F8…7538BE`), Waveshare 7B 2 959 248 octets (SHA-256 `288D4C0B…8761D`), 29 % de la partition d'application de 4 Mio libres sur chacune ; **aucun avertissement** dans les journaux des deux compilations. Ces SHA-256 ne servent qu'à reconnaître ces binaires : ils n'ont pas été flashés.

**Non vérifié** (voir « À prouver » de l'ADR-0019) : une requête réelle d'une carte à l'instance publique ; la RAM interne libre avant et après la création de la tâche (seuils de [memoire-interne.md](memoire-interne.md)) ; le comportement devant un 404 et devant une trame sans bloc mer ; le registre écrit puis relu en NVS sur une carte ; le verrou TLS partagé avec les alertes et la météo. Tant que rien n'écrit le registre (lot C), un appareil garde un registre vide : pas de tâche, pas de requête.
