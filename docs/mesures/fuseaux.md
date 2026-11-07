# L'heure du lieu choisi — fuseaux, changements d'heure, lever et coucher du soleil, ce qui reste à voir sur la carte

> **Summary (EN)** — Written on 2026-11-07, **checked on the PC only**. Everything the screen writes as a time now follows the
> time zone of the saved zone ([ADR-0012](../decisions/0012-l-heure-affichee-est-celle-du-lieu-choisi.md)): the clock, the date,
> each « hour by hour » column (converted with the rules in force at its own instant, so a window across a clock change is right),
> sunrise and sunset, day names, « today » and the hours of an official alert. The sky without weather uses sunrise and sunset
> computed for the place (NOAA method, pure core): 8 s at most from Open-Meteo's real values over 63 days. Expected values come from
> Python `zoneinfo` (tzdata 2026c), never typed by hand; 38 + 14 targeted mutations are all detected; 7 PC renders at 1024x600
> were looked at; the Tab5 and the 7B build with 0 compiler warning. **Not seen on a board** (nothing flashed): the panel, the web
> page changing the zone, the cost of the sunrise computation on the ESP32-P4.

Branche `claude/fuseaux-horaires-42f27b` (tirée de `main` au commit `c8097a4`). Carte visée : Tab5 ; la Waveshare 7B n'est que compilée. Règle de lecture : tout ce qui n'est pas écrit « constaté » est une décision ou une lecture de source ; ce qui n'a pas été observé est écrit « non vérifié ».

## 1. Ce que l'écran fait maintenant (décision : ADR-0012)

| Ce qui s'écrit | Avant | Maintenant |
|---|---|---|
| Grande horloge, date, changement de jour | Europe/Paris, écrit en dur dans `horloge.cpp` | fuseau de la zone enregistrée, relu à chaque appel (`zone_lire`, copie de la RAM) |
| Étiquette de chaque colonne « heure par heure » | un seul décalage pour toute la fenêtre | règles du fuseau **à l'instant de la colonne** ; « 7h » / « 7:00 » à l'heure pile, « 05:30 » sinon |
| Lever, coucher, jour de la semaine, « aujourd'hui » | décalage de la réponse météo | fuseau du lieu ; jour de la semaine lu au **midi local** ; « aujourd'hui » par numéro de jour civil local |
| Ciel sans météo (démarrage) | « jour de 8 h à 18 h » en heure de Paris | lever et coucher **calculés** pour le lieu et le jour local (`soleil.hpp`) ; 8 h à 18 h locales seulement sans position |
| Heures d'une alerte de Vigie | heure de Paris | `horloge_heure_locale` → fuseau du lieu (chemin vérifié : `ecran_principal.cpp:116`, `ui_alertes.cpp:113`) |
| Fuseau inconnu de la table (ancien enregistrement) | heure de Paris | aucune heure (« -- : -- » et la phrase d'attente), jamais Paris |

## 2. Tests sur le PC (constaté)

Compilés et lancés avec AddressSanitizer + UBSan (mode « complet (compilé + exécuté) » du lanceur) :

| Fichier | Tests | Ce qu'il vérifie |
|---|---|---|
| `test_heure_fr.cpp` | 32 | 13 zones, 1 574 instants de `zoneinfo` (dont Katmandou, Caracas, Auckland, Honolulu) ; Casablanca gardée comme limite connue (85 lignes) |
| `test_heure_lieu.cpp` | 15 | 408 colonnes d'une fenêtre, 73 jours, 98 levers et couchers réels en `hh:mm` local, New York (1er novembre 2026) et Sydney (4 octobre 2026), étiquettes, jours de 23 et 25 heures |
| `test_soleil.cpp` | 11 | NOAA contre une réécriture Python indépendante (120 lignes) et contre 63 jours réels d'Open-Meteo ; jour et nuit polaires ; coordonnées invraisemblables |
| `test_horloge.cpp` | 9 | le **vrai** `main/horloge.cpp` avec une zone fausse : 8 lieux × 13 instants, changement de zone entre deux appels, zone illisible, nom sans zéro final |
| `test_vigie_heure_lieu.cpp` | 3 | `vigie::formater_instant` pour 8 lieux × 13 instants, en français et en anglais |

**Valeurs attendues générées par Python** (`test/host/controle_python_fuseaux.py` et `controle_python.py`, `--verifier` : « tout est à jour »), jamais écrites à la main : 14 zones, 408 colonnes, 73 jours, 98 levers/couchers réels, 104 lignes d'horloge ; 120 lignes de soleil recalculées et 63 jours d'Open-Meteo.

**Précision du lever et du coucher** : plus grand écart relevé entre l'algorithme et les valeurs **réelles** d'Open-Meteo (neuf réponses capturées, 63 jours) : **8 s** (`controle_python_fuseaux.py` l'affiche). Le test tolère volontairement 2 minutes : Open-Meteo peut changer de modèle.

**Open-Meteo aligne ses colonnes sur l'heure locale pile** (mesuré sur les neuf réponses gardées dans `test/host/donnees/`) : à :30 UTC à Kolkata, :15 UTC à Katmandou. Le format « 05:30 » d'une colonne ne se voit donc, aujourd'hui, qu'avec une source qui donnerait des instants ronds en UTC.

## 3. Mutations (constaté)

| Cible | Mutations | Détectées | Commande |
|---|---|---|---|
| `heure_lieu.hpp`, `soleil.hpp` | 38 | 38 (0 problème) | `python test/host/lancer.py --mutations --seulement test_heure_lieu,test_soleil` |
| `main/horloge.cpp` (hors du noyau) | 14 | 14 (0 problème) | `python test/host/mutations_horloge.py` |

Trois mutations ont d'abord échappé et ont fait corriger le **test** : le jour de la semaine lu à minuit local (le test n'essayait que des minuits exacts : ajout de décalages de ±1 h et ±30 min), un drapeau impliqué par les règles du hachage (mutant équivalent, remplacé), et l'oubli du retour de `zone_lire` (le faux renvoyait une zone vide, donc `false` aussi : le faux remplit maintenant `*sortie` avant de répondre « faux »). Les mutations qui ne compilaient pas (`-Werror`) sont comptées comme un problème, pas comme une détection.

## 4. Rendus sur le PC (constaté, regardés)

`.\tools\rendre.ps1 -Scene fuseaux`, 1024x600, français et anglais, sept variantes : Tokyo, Kolkata, Kolkata aux colonnes UTC, Katmandou, Katmandou aux colonnes UTC, New York à travers le changement d'heure, Sydney à travers le changement d'heure. **Vu** : New York écrit « 1h » **deux fois** et Sydney **saute** « 2h » ; les colonnes UTC de Kolkata et de Katmandou s'écrivent « 13:30 » (Kolkata) et « 13:45 » (Katmandou) sans déborder de leur colonne, la première colonne étant celle de l'heure courante ; le lever et le coucher de chaque lieu sont ceux d'un calcul Python indépendant (New York 06:26 / 16:52, Sydney 06:28 / 18:59, Tokyo 06:07 / 16:41, Kolkata 05:43 / 16:56, Katmandou 06:17 / 17:16). C'est une simulation logicielle de LVGL sur le PC (docs/outils.md), pas la dalle.

Les captures versionnées `docs/captures/ecran/bonjour-*.png` ne correspondent plus à un rendu frais de la page « Bonjour » (sa mise en page a changé avant ce lot) : **non vérifié** si ce lot y contribue en quoi que ce soit ; les captures n'ont pas été régénérées ici.

## 5. Compilation (constaté)

`.\tools\idf.ps1` (ESP-IDF 5.5.5, `CMAKE_BUILD_PARALLEL_LEVEL=4`, verrou `build` tenu puis rendu), arbre de travail au-dessus de `c8097a4`, le 07/11/2026 vers 14 h 20 (heure de Paris), dans un dossier de build propre à la branche :

| Carte | Code de sortie | Avertissements du compilateur | Binaire | Place libre dans l'emplacement d'application |
|---|---|---|---|---|
| Tab5 (`BOARD=tab5`) | 0 | 0 | 2 912 272 octets, SHA-256 `85DD05F8…` | 31 % |
| Waveshare 7B (`BOARD=waveshare-7b`) | 0 | 0 | 2 892 896 octets, SHA-256 `730003F2…` | 31 % |

Seul avertissement relevé : « Error while generating esp_rom gdbinit », un message CMake connu et inoffensif ([dépannage](../depannage.md)). `dependencies.lock` et le fichier de verrou de la 7B sont restés inchangés. Une compilation qui réussit ne prouve pas que ça marche : voir §6.

**Vu sur** : PC d'Axel (Windows 11, ESP-IDF 5.5.5, llvm-mingw clang++ 23.1.5 avec AddressSanitizer + UBSan pour les tests, Python 3.13 `zoneinfo` / tzdata 2026c), branche `claude/fuseaux-horaires-42f27b` au-dessus de `c8097a4`, 07/11/2026 ; **carte : aucune**, rien n'a été flashé et aucun port série n'a été ouvert.

## 6. Non vérifié (rien de tout cela n'a été vu sur une dalle)

- L'heure, la date, la colonne « heure par heure » et le ciel avec une **autre zone** sur le Tab5 et sur la 7B.
- Le **changement de zone depuis la page web** : seul le chemin `zone_lire` → horloge est testé (la page écrit la même zone que l'écran, ADR-0003).
- Le **temps de calcul** du lever et du coucher sur l'ESP32-P4 (double précision, résultat gardé par jour) : non mesuré.
- La table de fuseaux n'a été comparée à `zoneinfo` que pour 13 noms (1 574 instants) ; les **455 autres** noms ne l'ont pas été un par un.
- Les heures d'une **alerte réelle** de Vigie dans une ville d'un autre fuseau : le chemin (`horloge_heure_locale` → `vigie::formater_instant`) est testé sur 8 lieux, aucune alerte réelle n'a été reçue.
- Le moment qui suit un changement de zone : la requête météo repart, mais pendant quelques secondes l'écran montre l'heure du **nouveau** lieu avec la météo de l'**ancien** (le lever et le coucher de l'ancienne ville sont écrits en heure du nouveau lieu). Comportement connu, non corrigé ici (ADR-0012, conséquences).

## 7. Décisions qui sont à Axel

1. **Format d'une colonne qui n'est pas à l'heure pile** : « 05:30 » (retenu, comme le lever du soleil et la grande horloge) ou « 5h30 » ; une seule fonction à changer, `formater_etiquette_heure`.
2. **`Africa/Casablanca`** : l'heure est fausse d'une heure pendant le ramadan et à partir du 20/09/2026 (tzdata 2026c : `+00`, la table : `+01` fixe) ; la règle ne se code pas en POSIX. Laisser, ou retirer ce nom de la liste proposée.
3. Le nom de la ville est relu **à chaque passage de l'écran** (500 ms) au lieu de toutes les 30 s, pour que l'heure et la ville changent ensemble.
