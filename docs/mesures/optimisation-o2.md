# Compilation en `-O2` au lieu de `-Og` — mesure avant / après sur le Tab5 (ST7121) et sur la Waveshare 7B

> **Summary (EN)** — Before/after of building with `-O2` (`CONFIG_COMPILER_OPTIMIZATION_PERF=y`) instead of ESP-IDF's default `-Og`, measured on the Tab5 ST7121 development board on 2026-11-09 (6 soft restarts per binary, same method, Wi-Fi not configured in either build).
> - First image 2210 → 2116 ms (about −95 ms), main screen 2107 → 2012 ms; CPU rotation loop 649–651 → 303–306 µs; PPA path unchanged (361 → 355 µs); worst render time of an image 394 → 355 ms; free internal RAM +5.1 kB; largest free block unchanged; no crash.
> - **Cost to watch**: the minimum free stack of the `taskLVGL` task fell from 1768 to 716 bytes (stack of 7168 bytes, the `esp_lvgl_port` default). **Handled in a separate commit: the stack is now 8192 bytes on both boards** (see "The `taskLVGL` stack" below for the figures measured afterwards).
> - Waveshare 7B (the house display, same day, same method): first image 484 → 466 ms, main screen 382 → 364 ms, free internal RAM +5.5 kB, largest free block 212,992 → 221,184 B, no crash, Wi-Fi, NTP and weather requests (TLS) working in both builds; **the minimum free stack of `taskLVGL` fell from 1848 to 584 bytes** (stack of 7168 bytes). The 7B was restored to the firmware it was running (proved by the chip's MD5 and by the version read on the board).
> - **`taskLVGL` stack set to 8192 bytes on both boards** (separate commit), measured afterwards on the tree merged with `origin/main`: minimum free stack **1608 bytes at start-up on both boards**; on the Tab5, a walk through the settings screens (on-board settings self-test, 3 phases, 2 real reboots) went down to **1520 bytes** at worst. The self-test reported 1 failure out of 29 checks; its cause is a test-logic mismatch that predates this PR (see "The `taskLVGL` stack" section), not the stack.
> - Not measured: touch latency and I2C load of the 10 ms touch read (needs a finger), the stack margin on screens the settings self-test does not visit (wheel, alerts pages, weather), the self-test on the 7B (it writes the settings of the house display, so it was not run there), the average render time (not comparable: the two windows hold a different number of images).

**Vu sur** : M5Stack Tab5, écran ST7121 (carte de développement, identifiée par sa MAC lue sur le bus USB puis par `esptool read-mac`, jamais écrite dans le dépôt), alimentation USB ; binaire « avant » `c3fd97b` compilé en `-Og` (SHA-256 `00B75E12…`, 3 093 184 o, version lue sur la carte `jalon-1a-tab5-st7121-305-gc3fd9`), binaire « après » `dca5e93` compilé en `-O2` (SHA-256 `E063D3B9…`, 3 258 032 o, version lue sur la carte `jalon-1a-tab5-st7121-309-gdca5e`) ; 09/11/2026 ; 6 démarrages par binaire, tous par la commande série `redemarrer` (le premier aussi), **sans Wi-Fi configuré** (les identifiants ne sont pas compilés dans ces binaires). Aucune autre carte n'a été touchée. Le firmware d'origine (`c3fd97b`, compilé le 09/11/2026 à 08:21:27) a été remis sur la carte à la fin et sa version relue.

**Vu sur (7B)** : Waveshare ESP32-P4 7B, l'écran de la maison (port repéré par la MAC lue sur la puce et comparée à celle enregistrée pour cette carte, jamais écrite dans le dépôt), alimentation USB ; 09/11/2026, accord d'Axel. Firmware en service avant et après : celui de la branche `claude/demarrage-rapide` (version lue `jalon-1a-tab5-st7121-310-g93783`, compilée le 09/11/2026 à 09:17:00). Binaire « avant » `c3fd97b` en `-Og` (SHA-256 `78E3F1B5…`, 3 073 680 o, version lue `…-305-gc3fd9`, compilé à 08:38:28), binaire « après » `db823e0` en `-O2` (SHA-256 `A133D952…`, 3 235 808 o, version lue `…-310-gdb823`, compilé à 09:44:34) ; 6 démarrages par binaire, tous par la commande série `redemarrer`. **Cette fois le Wi-Fi est configuré** (dans la NVS de la carte) : les deux séries se sont connectées. Hors du Tab5 de développement, seule cette carte a été touchée.

**Vu sur (pile à 8192)** : mêmes deux cartes, 09/11/2026, accord d'Axel (« Oui, pile à 8192 »). Binaires compilés depuis `90c4af2` (arbre fusionné avec `origin/main` `332ccab`, `-O2`, lecture tactile à 10 ms, pile de `taskLVGL` à 8192 o) : Tab5 3 259 472 o (SHA-256 `E22D55ED…`, version lue sur la carte `jalon-1a-tab5-st7121-327-g90c4a`, compilé à 11:38:06), 7B 3 237 248 o (SHA-256 `7B5F370E…`). **Tab5 ST7121** : 6 démarrages par `redemarrer`, sans Wi-Fi configuré, puis un passage de l'autotest des réglages (binaire `-DMERIDIEN_AUTOTEST_REGLAGES=ON` du même arbre, avec une ligne de journal temporaire qui relève la marge de pile à chaque vérification, non commitée). **7B** (l'écran de la maison, MAC lue sur la puce et comparée, jamais écrite dans le dépôt) : 6 démarrages par `redemarrer`, Wi-Fi configuré (NVS de la carte), version lue `…-327-g90c4a` compilée à 11:38:26. Pas d'autotest sur la 7B. **Base de comparaison** : le firmware que chaque carte portait à l'arrivée, `main` `aea866d` en `-Og` (Tab5 compilé à 11:34:12, 7B à 11:21:21), capturé sur **un seul démarrage** chacun : c'est un échantillon unique, pas une moyenne. Les deux cartes ont ensuite été remises au firmware trouvé (`aea866d`) : écriture prouvée par le MD5 de la puce, version et heure de compilation relues sur la carte.

Règle de lecture : ce qui n'est pas marqué « mesuré » est une lecture de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## Méthode

- Deux binaires compilés avec le même ESP-IDF 5.5.5, dans des dossiers distincts : seule la ligne `CONFIG_COMPILER_OPTIMIZATION_PERF=y` (et les trois corrections d'avertissements, qui ne changent pas le texte produit) les sépare ; le binaire « après » contient aussi la lecture tactile à 10 ms, dont l'effet n'est pas mesuré ici.
- Flash de l'application seule (0x10000) après sauvegarde et vérification du hachage ; version et heure de compilation relues sur la carte avant chaque série.
- Chaque série : 6 démarrages de 50 s, journal série horodaté par le PC ; à 40 s de chaque cycle la commande `memoire`, au dernier cycle la commande `piles`. Les lignes lues sont celles que le firmware écrit déjà (`banc de rotation`, `rétroéclairage allumé`, `JALON_MEM`, `affichage sur … ms`, `MEMOIRE`, `PILE`).

## Résultats sur le Tab5 (mesuré, moyenne des 6 démarrages sauf mention)

| Mesure | `-Og` (avant) | `-O2` (après) | Écart |
| --- | --- | --- | --- |
| Première image affichée (rétroéclairage allumé) | 2236 ms (min 2209, max 2363) | 2116 ms (min 2115, max 2117) | −120 ms en moyenne, −94 ms sur le minimum |
| Écran principal affiché (`JALON_MEM`) | 2132 ms | 2012 ms | −120 ms |
| Boucle de rotation du processeur (banc au démarrage) | 649–651 µs | 303–306 µs | environ −53 % |
| Rotation par le PPA (banc au démarrage) | 361–362 µs | 355–356 µs | inchangé (−2 %) |
| Pire rendu d'une image (maximum de la fenêtre) | 394 ms | 355 ms | −10 % |
| Mémoire interne libre, après l'écran principal | 249 039 o | 254 123 o | +5 084 o |
| Mémoire interne libre, à 40 s | 204 663 o | 209 747 o | +5 084 o |
| Plus gros bloc interne libre | 86 016 o | 86 016 o | inchangé |
| Marge minimale de la pile de `taskLVGL` (pile de 7168 o) | 1768 o | **716 o** | **−1052 o** |
| Plantage, redémarrage inattendu | aucun | aucun | |

Notes :

- Le démarrage 4 de la série « avant » a une première image à 2363 ms : une erreur connue du ré-initialisation de l'expandeur (`pi4ioe5v6408: Chip reset failed`, défaut déjà ouvert, non causé par cette PR) qui ajoute un nouvel essai d'environ 150 ms. La série « après » ne l'a pas rencontrée ; l'écart de 120 ms en moyenne contient donc ce hasard, d'où le chiffre du minimum (−94 ms) à côté.
- **Le rendu moyen par image n'est pas comparable** : 127–155 ms « avant », 180–182 ms « après », mais les fenêtres de mesure ne contiennent pas le même nombre d'images (4 ou 5 « avant », 3 « après ») ni les mêmes (la première image, la plus lourde, pèse plus dans une fenêtre courte). On ne peut ni conclure à un gain ni à une perte sur la moyenne ; seul le maximum est repris.
- La rotation du PPA ne gagne pas ce que gagne la boucle du processeur : le Tab5 utilise le PPA, la boucle du processeur n'est que le repli. Le gain de rotation n'est donc pas un gain d'affichage en fonctionnement normal ; le gain visible vient du reste du code (première image, rendu).

## Résultats sur la Waveshare 7B (mesuré, moyenne des 6 démarrages)

| Mesure | `-Og` (avant) | `-O2` (après) | Écart |
| --- | --- | --- | --- |
| Première image affichée (rétroéclairage allumé) | 484 ms (483–485) | 466 ms (465–467) | −18 ms (−3,7 %) |
| Écran principal affiché (`JALON_MEM`) | 382 ms | 364 ms | −18 ms |
| Mémoire interne libre, après l'écran principal | 353 610 o | 359 066 o | +5 456 o |
| Mémoire interne libre, à 40 s | 277 798 o | 283 291 o | +5 493 o |
| Plus gros bloc interne libre | 212 992 o | 221 184 o | +8 192 o |
| Marge minimale de la pile de `taskLVGL` (pile de 7168 o) | 1848 o | **584 o** | **−1264 o** |
| Marge minimale de la pile de la tâche météo (12 288 o) | 6152 o | 6280 o | sans écart notable |
| Wi-Fi associé / IP obtenue / heure NTP, depuis le démarrage | 7,1 s / 10,2 s / 10,2 s | 7,3 s / 10,4 s / 10,4 s | sans écart (le premier démarrage « avant » est à 6,1 s, les cinq autres à 7,3 s) |
| Requêtes météo (TLS) | réussies, cache écrit en NVS | réussies, cache écrit en NVS | |
| Plantage, redémarrage inattendu | aucun | aucun | |

Notes :

- **La 7B n'a ni rotation ni les lignes « affichage sur … ms »** : le banc de rotation et le rendu par image ne sont pas mesurables sur cette carte avec ces journaux. Ce qui reste, c'est la première image, la mémoire, les piles et le réseau.
- Le gain de première image est modeste (3,7 %). Ce qui occupe les 466 ms restantes n'a pas été décomposé ici : **non vérifié**.
- Les erreurs du journal sont les mêmes dans les deux séries et déjà connues : `swap_xy is not supported by this panel` (le panneau ne sait pas permuter les axes) et `0 mac type is incorrect (not found)`. La série « après » compte une ligne `swap_xy` de plus : son premier cycle contient deux démarrages (celui qui suit l'écriture du binaire, puis le redémarrage logiciel demandé).
- **La marge de pile de `taskLVGL` est encore plus mince que sur le Tab5** : 584 octets sur 7168 (92 % de la pile atteinte) au démarrage et sur l'écran principal.

## La pile de `taskLVGL` : 7168 → 8192 octets (commit séparé)

**Le risque** : en `-O2`, la marge minimale de la pile de `taskLVGL` est tombée de 1768 à 716 octets sur le Tab5 et de 1848 à 584 octets sur la 7B, sur 7168 octets (valeur par défaut d'`esp_lvgl_port`), soit 90 % et 92 % de la pile atteinte, sur la seule séquence de démarrage et l'écran principal. Ce n'est pas un dépassement, mais les réglages, la saisie et les popups n'avaient pas été parcourus.

**La réponse** : `lvgl_port_cfg_t.task_stack = 8192` dans `lvgl_attach()` des deux cartes (`tab5_board.c`, `ws7b_board.c`). Coût : 1 ko de mémoire interne.

| Mesure (arbre fusionné, `-O2`, pile 8192) | Tab5 ST7121 | Waveshare 7B |
| --- | --- | --- |
| Marge minimale de `taskLVGL` en fin de démarrage (commande `piles`, dernier des 6 démarrages) | 1608 o | 1608 o |
| Rappel, pile 7168, avant la fusion (`-O2`) | 716 o | 584 o |
| Rappel, pile 7168, `-Og` | 1768 o | 1848 o |
| Marge minimale pendant le parcours des réglages (autotest) | **1520 o** | non exécuté |
| Mémoire interne libre après l'écran principal, avec cette pile | 252 827 o | 358 042 o |
| Même mesure sur `main` `aea866d` en `-Og` (1 démarrage) | 248 767 o | 353 611 o |
| Plantage, redémarrage inattendu (6 démarrages, `cause=3` partout) | aucun | aucun |

La mémoire interne gagnée par `-O2` (+5 084 o sur le Tab5, +5 456 o sur la 7B, mesurés plus haut) moins le ko de pile redonne les écarts observés (+4 060 o sur le Tab5, exactement ; +4 431 o sur la 7B, à un octet près) : le ko de pile est bien ce que l'on paie.

**Autotest des réglages, Tab5** (29 vérifications, 3 phases séparées par 2 vrais redémarrages) : la marge ne descend qu'à 1520 octets (relevé à la vérification « le curseur de luminosité est affiché », phase 1 ; aucune baisse ensuite dans les phases 2 et 3). Sur 7168 octets la même consommation laisserait environ 496 octets : **déduit** (1520 − 1024), pas mesuré. La pile de 8192 octets rend donc la marge confortable sur ce parcours ; elle ne prouve rien pour les écrans que l'autotest ne visite pas (roue d'appui long, pages des alertes, météo), qui restent **non vérifiés**.

**Un échec sur 29** : « l'installation reprend sur un écran à part » (phase 2, `autotest_reglages.cpp:445`). Cause **lue dans le code et dans le journal, non rejouée sur `c3fd97b`** : depuis le commit `cd362eb` (07/11/2026), `ecran_principal.cpp` appelle `ui_installation_ouvrir_si_necessaire()` au démarrage ; le journal montre `installation guidée ouverte à l'étape 3` à 2,5 s après le redémarrage, donc l'écran de l'installation est déjà l'écran actif quand l'autotest (qui démarre à 12 s) mémorise « l'écran principal » ; son appel ne rouvre rien (un second appel est sans effet, `ouvrir_ecran()` rend faux) et la comparaison `lv_screen_active() != principal` échoue. La vérification suivante, « l'écran affiche l'étape 3 sur 7 », passe : l'installation est bien reprise. Ni `-O2`, ni la pile, ni le tactile à 10 ms n'y interviennent d'après cette lecture ; le défaut est dans l'essai (mesure du 07/11 faite avant que l'écran principal reprenne l'installation : `docs/mesures/6-reglages-appareil.md`, ligne « non vérifié avec cette colle réelle »). **Non corrigé ici** (hors périmètre de la PR) ; à traiter dans la piste des réglages.

**Comparaison avec `main` (échantillon unique)** : le Tab5 affiche sa première image à 2405 ms sur `main` `aea866d` en `-Og` (un démarrage) et à 2316 ms sur l'arbre fusionné (moyenne de 6), le pire rendu d'une image passe de 463 à 421 ms et la boucle de rotation du processeur de 652 à 304 µs ; la 7B passe de 594 à 544 ms. Ces gains (−89 ms et −50 ms) sont du même ordre que ceux mesurés avant la fusion (−94 ms et −18 ms) ; la 7B gagne plus ici qu'avant la fusion, **sans cause isolée**. Les valeurs absolues d'avant la fusion (2116 ms sur le Tab5, 466 ms sur la 7B) ne sont pas comparables à celles d'après : `main` a bougé entre-temps (le même Tab5 en `-Og` est passé de 2236 ms sur `c3fd97b` à 2405 ms sur `aea866d`), ce que cette PR n'explique pas.

## Ce qui n'est pas vérifié

- Le délai appui → réaction à l'écran et la charge du bus I2C de la lecture tactile à 10 ms : il faut un doigt et un banc ; rien n'a été mesuré. Sur le Tab5, le rôle exact de l'interruption GPIO23 avec ce réglage n'est pas non plus vérifié.
- Le comportement avec le Wi-Fi, TLS et la météo en `-O2` **sur le Tab5** : les deux binaires mesurés n'avaient pas de Wi-Fi configuré. Sur la 7B, en revanche, Wi-Fi, NTP et requêtes météo ont fonctionné dans les deux séries (voir ci-dessus), sur 6 démarrages seulement.
- Une durée plus longue (fuite de mémoire de quelques minutes, 30 démarrages, plusieurs heures) : 6 démarrages seulement.
- Le tactile de la 7B comme celui du Tab5 : aucun appui n'a été fait (le GT911 de la 7B est interrogé, son INT n'est pas relié, d'après l'en-tête de `ws7b_board.c` : la lecture à 10 ms y multiplie par trois les interrogations du bus, dont le coût n'est pas mesuré).
- La marge de pile sur les écrans que l'autotest des réglages ne visite pas (roue d'appui long, pages des alertes, météo, Mer) et sur la 7B, où l'autotest n'a pas été lancé : il écrit les réglages de la carte (luminosité, texte du service, étape de l'installation) et l'écran de la maison n'est pas fait pour cela.
- L'origine exacte de l'échec de l'autotest (voir plus haut) : lue dans le code et le journal, pas rejouée sur `c3fd97b`.
- La base de comparaison de l'arbre fusionné (`main` `aea866d` en `-Og`) vient d'un seul démarrage par carte.
- Après la dernière restauration de la 7B, la NVS relue diffère de la copie d'avant en **un seul secteur de 4 Ko** (0x84b000, 971 octets) : le firmware remis avait déjà redémarré et écrit dans sa NVS (compteur de démarrages, cache) avant la relecture. Le contenu de ce secteur n'a pas été décodé. Sur le Tab5, la NVS a été remise à l'identique (MD5 de la puce) après l'autotest, qui l'avait modifiée.
- L'état de la NVS de la 7B : une seule clé lisible a changé entre la copie d'avant et l'état d'après la restauration, `ecran:boots` (le compteur de démarrages, monté par les redémarrages de la mesure) ; les 9 autres, Wi-Fi compris, sont identiques. Le cache météo (valeur binaire) n'a pas été comparé.
