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

## 8. Lot C : la page — PC, rendu PC et compilation, aucune carte

> **Summary (EN)** — Lot C (the page itself, [ADR-0022](../decisions/0022-mer-la-page-lot-c.md)). **Measured**: 85 host tests (34 for the
> texts, 30 for what to show, 21 for the layout) pass under AddressSanitizer and UBSan and are also evaluated with ESP-IDF's GCC 14.2;
> 92 targeted mutations are all detected (a first series found 9 gaps and a second one more, all closed); every character of the page
> exists in the embedded font (91 characters); the page was rendered on the PC (LVGL software render, 21 screens at 1280x720, 1024x600
> and 720x1280 in French, German and Turkish); the firmware builds **without any compiler warning** for the Tab5 and the Waveshare 7B.
> **Nothing calls the page yet**, so the linker drops it and the normal binaries are the same size as lot B's; a throw-away build that
> forces the link (`-Wl,-u` on the opening function, then removed) links on both boards: +45 584 bytes on the Tab5 (3 024 352) and
> +45 648 bytes on the 7B (3 004 896), 28 % of the 4 MiB app partition still free. **Not verified**: anything on a board — touch, drawing
> time, free internal RAM and PSRAM with the page open, closing and re-opening, a real coastal warning on the page, Spanish and Italian
> on screen, and the German, Dutch, Spanish, Italian and Turkish sentences (AI-written, read by no native speaker). Nothing was flashed.

**Vu sur** : PC (même poste qu'au § 2, clang++ avec AddressSanitizer + UBSan, g++ 14.2 croisé d'ESP-IDF en `-fsyntax-only`, rendu logiciel de LVGL par `tools/rendre.ps1`), puis compilation ESP-IDF 5.5.5 pour les deux cartes (`tools/idf.ps1`, `CMAKE_BUILD_PARALLEL_LEVEL=4`) ; branche `claude/mer-page`, base la branche du lot B (`claude/mer-client`). Aucune carte, aucun flash.

**Mesuré** :
- **85 tests** de l'hôte : 34 pour les textes (`test_mer_textes.cpp`), 30 pour ce qu'il faut montrer (`test_mer_page.cpp`), 21 pour la mise en page (`test_mer_geometrie.cpp`) ; sous clang++ ASan + UBSan, et en `static_assert` avec le g++ d'ESP-IDF (mode syntaxe : les assertions de compilation sont évaluées, `main` n'est pas exécuté).
- **92 mutations ciblées** (`mutations_mer_textes.py` 31, `mutations_mer_page.py` 34, `mutations_mer_geometrie.py` 27, enregistrées dans `lancer.py`) : une règle cassée à la fois. Textes : heures, minutes et créneaux, l'âge (« à l'instant », « il y a 1 h », « il y a 1 j »), arrondis et signes, séparateur décimal selon la langue, unités, valeurs inconnues, marqueurs nommés, pleine et basse mer, coefficient absent, vigilances (illisibles, niveau, fin, données gardées par un fournisseur). Composition : un verdict jamais montré pour une mer trop vieille ou une activité absente, « Indisponible ici », la nuit et la couleur neutre, les teintes des verdicts, le trait « maintenant », les rangées et les blocs de points, la courbe de marée (heure de départ, trou dans la trame, marnage presque nul, repère au-delà de 24 heures), la bannière. Mise en page : recouvrement et appui des doigts, échelle, hauteur minimale d'une carte et limite à partir de laquelle la page n'existe plus, marée et soleil, le panneau de détail (colonnes, croix, centrage). **Résultat sur l'état final : 92 mutations, toutes détectées.** Première série : **9 problèmes** (des trous de test, 2 mutations **équivalentes**, 1 texte de mutation inapplicable), comblés ; deuxième série : 1 problème (`t0 == 0` dans la courbe de marée, qui n'était pas essayé), comblé. Limite : mutations **choisies à la main** sur ces trois fichiers, pas un balayage ; le dessin (`main/ui_mer.cpp`) et la colle (`main/ui_mer_ouvrir.cpp`) ne sont **pas** dans le noyau et ne sont pas mutés.
- **Les caractères** : un contrôle Python (`controle_python_mer.py`) vérifie que chaque caractère des textes existe dans `roboto_700.ttf` : 91 caractères, tous présents.
- **Le rendu PC** : 21 écrans aux trois tailles, en français, allemand et turc (`tools/rendre.ps1 -Scene mer`), rangés en trois planches dans `docs/captures/ecran/`. Les premiers viennent des **six vraies trames** de Vigie du § 2 ; les autres sont le détail de trois activités et des **états réécrits à la main** (un surf « bon » ; une vigilance côtière orange, rouge ou illisible ; des données de 22 minutes, de 7 heures, la dernière réponse d'un fournisseur, aucune donnée, pas d'horloge ; un spot sans modèle de marée) : aucune capture du jour ne les porte, ils montrent ce que la page sait dessiner, pas ce que Vigie rendra. **Regardé** : des écrans en pleine taille aux trois tailles pendant la mise au point (dont l'icône du surf, redessinée deux fois) et les trois planches en vignettes. **Pas regardé** : l'espagnol et l'italien ; une vignette ne permet pas de juger un texte qui frôle son cadre.
- **Firmware compilé pour les deux cartes** : Tab5 `meridien.bin` 2 978 768 octets (SHA-256 `AD9F17F5…70C`), Waveshare 7B 2 959 248 octets (SHA-256 `8ED368B1…EC2`) ; **aucun avertissement** dans les journaux des deux compilations, et **exactement la taille du lot B** : `ui_mer*.cpp` est compilé mais **rien ne l'appelle**, l'éditeur de liens le jette. Ces binaires ne montrent donc rien de nouveau.
- **Le lien de la page** (compilation d'essai, `main/CMakeLists.txt` modifié le temps du test puis restauré, empreinte vérifiée identique avant et après) : un `-Wl,-u` sur `ui_mer_ouvrir` force l'éditeur de liens à garder la page. Résultat : **le lien réussit sur les deux cartes**, sans référence indéfinie ni avertissement nouveau ; `ui_mer_ouvrir` et `ui_mer_creer` sont présents dans l'ELF de la Tab5. Tab5 3 024 352 octets (SHA-256 `A72AD905…11A6C`, **+45 584**), 7B 3 004 896 octets (SHA-256 `15AB2F7E…5A7C`, **+45 648**) ; il reste 28 % de la partition d'application de 4 Mio sur chacune (0x11da20 octets sur la Tab5, 0x122620 sur la 7B). Cette taille est celle du code et des tables de la page, **pas** celle de la mémoire vive qu'elle prendra. Ces binaires d'essai n'ont pas été flashés et ne sont pas livrés. Une ligne `OSError: ESP_ROM_ELF_DIR environment variable is not defined` apparaît dans les journaux ; elle est aussi dans les journaux de compilations faites plus tôt sur ce poste (avant le lien forcé) et ne vient pas du compilateur ; son origine n'a pas été cherchée.

**Non vérifié** (voir « À prouver » de l'ADR-0022) : l'ouverture, le toucher des quatre cartes, le détail, la croix et « Fermer » sur une carte (le tactile n'a jamais été essayé) ; le temps entre l'appui et la première image ; la mémoire interne et la PSRAM avant et après l'ouverture, la fermeture et la réouverture répétées ; le rendu sur la carte contre le rendu PC (couleurs, icônes de 30 px, texte qui tient en allemand et en turc) ; l'espagnol et l'italien à l'écran ; la page avec une vraie vigilance côtière orange ou rouge de Vigie ; la justesse des phrases allemandes, néerlandaises, espagnoles, italiennes et turques, **écrites par une IA et relues par aucun locuteur natif** ; le passage à l'heure d'été ou d'hiver pendant les 24 heures de la page (non géré). Tant que rien n'appelle `ui_mer_ouvrir` (lot D, l'appui long) et que rien n'écrit le registre de spots, un appareil flashé avec ce lot ne montre rien de nouveau.

## 9. Lot D : l'entrée « Mer » de la roue — PC, rendu PC et compilation, aucune carte

> **Summary (EN)** — Lot D (the long-press wheel opens the page, [ADR-0024](../decisions/0024-mer-l-entree-de-la-roue-lot-d.md)). **Measured**:
> 8 host tests and 10 targeted mutations (all detected) for the pure-core file that decides which buttons the first ring has and in which
> order; the seven existing wheel renders are **byte-identical** to the ones before the change (the refactor is neutral); five new wheel
> variants and a touch run (long press, no "Sea" without a chosen spot, "Sea" opens the page exactly once, close, "Settings" still
> reachable) pass on the render bench; the fourth button moves the hub away from the finger near the top-left corner (**162 → 220 px** on
> the Tab5, **123 → 177 px** on the 7B); the firmware builds **without any compiler warning** for the Tab5 and the Waveshare 7B, and the page
> is **now linked** (+45 968 bytes on the Tab5, +45 952 on the 7B; 27 % and 28 % of the 4 MiB app partition still free). **Not verified**:
> anything on a board (touch, the glyph on the real screen, hub distance on the real screen, opening time, memory); the device glue
> (`ui_mer_ouvrir.cpp`) has been compiled and linked but never run; **the button will not show on a real device yet, because nothing writes
> the spot registry**. Nothing was flashed.

**Vu sur** : PC (même poste qu'au § 2, clang++ avec AddressSanitizer + UBSan, rendu logiciel de LVGL 9.5.0 par `tools/rendre.ps1`), puis compilation ESP-IDF 5.5.5 pour les deux cartes (`tools/idf.ps1`, `CMAKE_BUILD_PARALLEL_LEVEL=4`) ; branche `claude/mer-roue`, base la branche du lot C (`claude/mer-page`) rebasée sur `main` (qui contient la roue, ADR-0021, et la page Détails, ADR-0020). Aucune carte, aucun flash.

**Mesuré** :
- **8 tests** de l'hôte (`test_roue_entrees.cpp`) : sans alerte ni mer, « Réglages » et « Détails » seulement ; avec les alertes seules, « Alertes » vient après « Détails » ; avec la mer seule, « Mer » vient après « Détails » ; avec les deux, « Mer » est la dernière ; « Réglages » est toujours le premier et « Détails » le second ; **l'arrivée de la mer ne change le rang d'aucune autre entrée** ; aucune entrée n'est en double et le premier anneau les porte toutes ; le nombre maximal d'entrées est celui des présences. En plus, un `static_assert` du fichier garde que ce nombre tient dans les boutons que la roue sait poser (`BOUTONS_MAX`).
- **10 mutations ciblées** (`mutations_roue_entrees.py`, enregistrées dans `lancer.py`) : une règle cassée à la fois (premier rang, second rang, « Alertes » sans page ou toujours là, « Mer » sans spot ou toujours là, « Mer » qui disparaît avec les alertes, deux boutons identiques, deux entrées échangées, le nombre maximal annoncé). **Résultat : 10 mutations, toutes détectées par le test attendu.** Limite : mutations **choisies à la main** sur ce fichier, pas un balayage ; `composer_entrees` (`main/ecran_principal.cpp`) n'est pas dans le noyau et n'est pas muté.
- **Le glyphe** : `generer_police_roue.py --verifier` relit `mdi_roue.ttf` : 5 glyphes, **1 564 octets** (1 380 avant), SHA-256 `9f4f0f05…6e6a873` (empreinte entière dans `roue_glyphes.json` et `main/assets/README.md`).
- **Le refactor est neutre** : les **sept rendus existants** de la roue de `docs/captures/ecran/` (centre, coin haut gauche, bord droit aux deux tailles, famille de quatre au centre en 1280 × 720) sont **identiques octet pour octet** à ceux que produit le banc après la modification (SHA-256 comparés un à un). `composer_entrees` passe par le noyau et le banc aussi : l'ordre et le nombre de boutons d'avant sont conservés.
- **Le rendu PC** (banc `tools/rendre.ps1 -Scene roue`, 1280 × 720 et 1024 × 600) : cinq variantes nouvelles (au centre avec « Mer » ; au centre sans la page des alertes, trois boutons ; près du coin haut gauche, du coin bas droit et du bord droit avec « Mer »), une roue de quatre boutons dont « Mer » est le dernier, dans la vue de l'écran principal. **Regardé** : le centre et le coin haut gauche en 1280 × 720 (le glyphe « waves » se lit à la taille des boutons, son mot est à sa place, rien ne déborde de l'écran). **Pas regardé de près** : les autres variantes et la 1024 × 600, vérifiées par les mesures du journal du banc (distance du moyeu, objets) et non à l'œil.
- **Le parcours tactile simulé** (`-Scene roue -Variante parcours-mer`, qui sort en erreur à la moindre constatation fausse) : (1) sans spot choisi, la roue a trois boutons et pas de « Mer » ; (2) avec un spot, quatre boutons et « Mer » est le dernier, sans cinquième ; (3) le toucher de « Mer » ferme la roue et ouvre la page **exactement une fois** ; (4) « Fermer » rend l'écran principal ; (5) « Réglages » reste atteignable ensuite. **Résultat : tout passe aux deux tailles.** Le banc ouvre la page par **une copie** de ce que fait `ui_mer_ouvrir` (sur une vraie trame de Hossegor du § 2) : le fuseau, l'horloge et le registre de l'appareil ne sont pas essayés.
- **Éloignement du moyeu au doigt** (banc, premier anneau seul, avec « Mer » contre sans) : Tab5 1280 × 720 — centre 0 → 0 px, coin haut gauche **162 → 220**, coin bas droit 49 → 41, bord droit 62 → 54 ; 7B 1024 × 600 — centre 0 → 0, coin haut gauche **123 → 177**, coin bas droit 38 → 32, bord droit 49 → 43. Sans la page des alertes (trois boutons : « Réglages », « Détails », « Mer »), le moyeu reste à 0 px au centre. Le pire cas reste le coin haut gauche, comme à trois boutons (ADR-0021).
- **Firmware compilé pour les deux cartes** (références : le lot C rebasé sur `main`, compilé avec le même outil juste avant) : Tab5 `meridien.bin` **3 058 304 octets** (3 012 336 avant, **+45 968** ; SHA-256 `1B4CB13B…FB8`), Waveshare 7B **3 038 768 octets** (2 992 816 avant, **+45 952** ; SHA-256 `38DFA2AD…1CB`). **Aucun avertissement du compilateur** dans les quatre journaux (références comprises) ; il reste 27 % de la partition d'application de 4 Mio sur la Tab5 (0x115580 octets) et 28 % sur la 7B (0x11a1d0). **La page est maintenant liée sans artifice** : l'éditeur de liens garde `ui_mer_ouvrir` parce que `ecran_principal.cpp` l'appelle (lu dans la carte d'édition de liens `build/meridien.map` de la 7B ; pour la Tab5, la même croissance de ~46 Ko, comparable au +45 584 du lien forcé du § 8, mais la carte de la Tab5 n'a pas été relue). Cette taille est celle du code et des tables, **pas** celle de la mémoire vive que la page prendra. Ces binaires n'ont pas été flashés et ne sont pas livrés.

**Non vérifié** (voir « À prouver » de l'ADR-0024) : tout ce qui tourne sur une carte — l'appui long, la roue à quatre boutons, le toucher de « Mer », l'ouverture et la fermeture de la page (le tactile n'a jamais été essayé) ; le glyphe et son mot sur l'écran de la carte ; l'éloignement du moyeu sur l'écran contre le banc ; le temps entre le toucher et la première image ; la mémoire interne et la PSRAM après plusieurs ouvertures ; ce que fait le bouton quand `ui_mer_ouvrir` refuse d'ouvrir (silencieux) ; la colle de l'appareil (`ui_mer_ouvrir.cpp`), compilée et liée mais jamais exécutée. **Le bouton n'apparaît pas encore sur un vrai appareil** : rien n'écrit le registre des spots (`reglages_mer_enregistrer` n'a aucun appelant), donc `reglages_mer_demande().presente` reste faux.

## 10. Lot E : l'écriture du registre des spots (`/api/sea` et la carte « La mer » de la page) — PC et compilation, aucune carte

> **Summary (EN)** — Lot E (the settings page writes the spot registry, [ADR-0025](../decisions/0025-mer-ecriture-du-registre-des-spots-lot-e.md)).
> **Measured**: 30 new host tests (19 for the pure-core rules, 11 for the route) and 27 new targeted mutations, **58 mutations in the two test
> files, all detected**; the full host suite (84 test files) passes; the settings page played by Playwright against a fake device: **316 of 316
> checks** (213 before, so 103 for the new card), the page's language check and the translation check pass; the page weighs 24 107 bytes
> gzipped with French (budget 30 720); the firmware builds **without any compiler warning** for the Tab5 (3 078 544 bytes, +20 240) and the
> Waveshare 7B (3 059 008 bytes, +20 240), 27 % of the 4 MiB app partition still free on both. **Not verified**: anything on a board; the
> page against a real device; Vigie's real list of spots read by a browser (the page's search was played only against an invented list); the
> six AI translations. The commit hook also **executed** the whole host suite under AddressSanitizer and UBSan (1 879 tests, green, on the
> two commits that touch the core). **The page's "La mer" card and the route have never run on a board; nothing was flashed.**

**Vu sur** : PC (même poste qu'au § 2), compilation ESP-IDF 5.5.5 pour les deux cartes (`tools/idf.ps1`, `CMAKE_BUILD_PARALLEL_LEVEL=4`, verrou `build` pris puis rendu), Playwright (Chromium) contre le faux appareil de `web/faux-serveur/` ; branche `claude/mer-registre`, base la branche du lot D (`claude/mer-roue`, `4fc4ef7`). Aucune carte, aucun flash.

**Mesuré** :
- **30 tests de l'hôte** : 19 dans `test_mer_api.cpp` (objet vide sans effet, liste remplacée en entier, même spot gardé quand `choisi` manque, choix hors liste ou mal formé refusé, 12 spots passent et 13 sont trop, plage de vent en nœuds entiers et ordonnée, un refus n'applique rien même pour les champs valides, …) et 11 dans `test_api_reglages.cpp` (35 scénarios maintenant : registre vide avec ses bornes, liste écrite puis relue telle que gardée, douze spots aux noms les plus longs dans la réponse, un nom avec des guillemets reste du JSON valide, écriture qui échoue = 500, registre absent = 500 sans rien écraser, `/api/sea` protégé comme les autres routes). La suite complète de l'hôte, lancée avec `test/host/lancer.py` (**mode « syntaxe »** : les `static_assert` sont évalués par le g++ 14.2 d'ESP-IDF, `main()` n'est pas exécuté), compile ses **84 fichiers de test** et se termine par « tout passe » ; le crochet de commit a ensuite **exécuté** toute la suite avec AddressSanitizer + UBSan (**1 879 tests**, verts aux deux commits qui touchent le noyau, `e44f95b` et `2365b16`) ; les contrôles croisés (36 phrases de l'API identiques entre `api_reglages.hpp`, la table de traduction et `test_api_reglages.cpp` ; i18n : 395 clés, 182 identifiants de page, sources cohérentes et sorties à jour ; contrat des réglages) passent.
- **27 mutations nouvelles** (`mutations_mer_api.py`, 14 sur `mer_api.hpp`, 13 sur `api_reglages.hpp`), **58 en tout** avec les 31 qui existaient déjà sur ces deux fichiers de test : **toutes détectées par le test attendu** (chaque ligne du journal nomme le test), 0 survivante. Limite : mutations **choisies à la main** ; l'allocation des deux registres et `HoteAppareil` (`main/serveur_web.cpp`) ne sont pas dans le noyau et ne sont pas mutées.
- **La page** : `web/tests/parcours.py` (Chromium, rendu téléphone 390 × 844, faux appareil, la lecture de la liste de Vigie interceptée) : **316 vérifications sur 316**, code de sortie 0, lancé trois fois (par l'agent qui a écrit la page, puis deux fois par moi sur la même arborescence, la dernière après la correction des cinq alertes de `ruff` dans le parcours lui-même) ; **213 avant ce lot**, donc 103 pour la carte (le registre, le délai de 15 s de la lecture chez Vigie, l'allemand et le turc). Le contrôle des langues (`controle_langues.py`) passe : la carte compte 18 textes, mêmes clés et même ordre dans les six langues, glossaire respecté. Poids : **24 107 octets gzip** avec `fr.json` (budget 30 720 ; la carte ajoute 3 931 octets), 24 337 avec la langue la plus lourde (de). Sept captures de la carte dans `web/captures/` (`mer-1-vide.png` à `mer-7-turc.png`), dont le vide, la recherche, la liste, l'erreur de Vigie, le refus du vent, l'allemand et le turc ; les 24 captures déjà suivies par git que le parcours réécrit ont changé (la barre d'onglets a un onglet de plus). **Regardé** : la liste et l'erreur de Vigie (à l'œil, à 390 px).
- **Contre-épreuve de la page** (faite par l'agent, relevée dans son rapport, non refaite) : 8 altérations volontaires des textes, 8 repérées par le contrôle des langues ; 8 sabotages de `app.js` (copies hors dépôt), **7 font échouer le parcours** ; le huitième (`listeLue = () => false` seul) n'est pas repéré, parce qu'il ne change aucune requête.
- **Firmware compilé pour les deux cartes** (références : le lot D, `4fc4ef7`, compilé avec le même outil juste avant) : Tab5 `meridien.bin` **3 078 544 octets** (3 058 304 avant, **+20 240** ; SHA-256 `0CC13633…8CF6`), Waveshare 7B **3 059 008 octets** (3 038 768 avant, **+20 240** ; SHA-256 `3651B8C1…7DC9`). **Aucun avertissement du compilateur** dans les deux journaux ; il reste 27 % de la partition d'application de 4 Mio sur les deux cartes (0x110670 octets sur le Tab5, 0x1152c0 sur la 7B ; 28 % sur la 7B avant). **La route est liée** : `/api/sea`, les identifiants d'erreur (`trop_de_spots`, `spot_double`) et une phrase d'erreur en français se retrouvent dans le binaire du Tab5. La page web y est embarquée en gzip : ses textes ne se lisent pas dans le binaire, et **je n'ai pas lu la carte d'édition de liens ni le fichier généré** pour le prouver ; la reconstruction est partie d'un dossier de construction neuf, donc de l'état courant de `web/app`. Cette taille est celle du code et des tables, **pas** celle de la mémoire vive que les deux registres de travail prennent (2 × ≈ 2,3 Ko de PSRAM à l'ouverture du serveur, **non mesurés sur carte**). Ces binaires n'ont pas été flashés et ne sont pas livrés.
- Lu chez Vigie le 08/11/2026 (requête en lecture seule, sans garder le corps) : `GET /v1/surf/spots` répond avec `Access-Control-Allow-Origin` égal à l'origine reçue. **Une requête, pas une page dans un navigateur sur le réseau de l'appareil.**

**Non vérifié** (voir « À prouver » de l'ADR-0025) : tout ce qui tourne sur une carte — `PUT /api/sea` depuis un téléphone, la relecture après un redémarrage, le réveil du client réseau, l'apparition du bouton « Mer » de la roue, l'allocation des deux registres en PSRAM (mémoire avant et après), le temps d'un `PUT` ; la carte « La mer » contre un **vrai appareil** (le parcours n'a joué que contre le faux, qui réécrit en JavaScript les règles du noyau : aucun test ne compare les deux) ; la **vraie** liste de Vigie lue par la page dans un navigateur (CORS, accents, 51 spots, format réel au-delà de `id`, `name`, `osm_name`, `place`) ; les traductions des six langues (écrites par une IA, aucune relue par un natif) ; la page sur un vrai téléphone et dans un autre navigateur que Chromium ; l'export des réglages, qui ne contient pas le registre.
