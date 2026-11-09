# Accueil, la bande de pluie de la ligne centrale — ce qui est constaté sur le PC et à la compilation, ce qui reste à voir sur une carte

> **Summary (EN)** — The home screen's central line now shows the rain of the next two hours as **one bar per quarter of an hour**
> (ADR-0027), on the **left half** of the line, the right half staying for the official alert. It was **seen on PC renders and compiled
> for both boards only**: 20 new core tests (evaluated at compile time and at run time), 22 targeted mutations all detected (one was
> not at first, a boundary-case test was added), about twenty LVGL renders looked at (landscape 1280x720 and 1024x600, portrait 720x1280;
> French, German, Dutch, Spanish and Turkish), ESP-IDF 5.5.5 builds of the Tab5 and the Waveshare 7B (+7.0 KB and +7.1 KB of flash
> image against `origin/main`, +256 and +320 bytes of static RAM, all in `.data`). A render in portrait showed the half line too
> narrow (about 320 reference pixels): the strip takes the full width there when no alert needs the right half. **Not seen on a
> board**: the drawing on the real panels, legibility at one metre, the bar height on the 7B (the line is only 62 reference pixels
> tall), drawing time, free internal RAM while running, which weather providers fill the quarters, how the five AI-written languages
> read to a native speaker.

**Vu sur** : PC d'Axel (Windows 11) ; clang++ 23.1.5 avec AddressSanitizer + UBSan (tests), clang-format 23.1.5 ; LVGL 9.5.0 (rendu PC) ;
ESP-IDF 5.5.5 (compilation) ; branche `claude/bande-pluie`, commit `3c35d13` (au-dessus de `origin/main` = `5d5f3ad`, 09/11/2026 17 h 21),
le 09/11/2026 en fin de journée. **Aucune carte** (ni Tab5 ni 7B), **rien flashé**, aucun port série ouvert.

Règle de lecture : « mesuré » = relu sur une sortie d'outil de cette session ; tout le reste est une lecture de code ou **non vérifié**.

## 1. Question

Quand il pleut ou va pleuvoir, l'accueil ne montrait que la phrase (« Pluie dans 12 min »). Axel a demandé de voir la pluie **sur la ligne
centrale**, « avec les crans (horaire) qui vont avec l'API, en plus beau » : une barre par quart d'heure, comme sur l'écran du Tab5 sous
ESPHome. La bande tient-elle sur la ligne de 62 px de référence, à la moitié de sa largeur, sans cacher les cinq jours et sans mentir
sur ce que les données savent ?

## 2. Les données de la bande

Le pas est celui de l'API : Open-Meteo donne la pluie **par quart d'heure** (`minutely_15`) et date chaque valeur par la **fin** du quart. L'appareil
les garde déjà (`MeteoDetail.quarts_*`) ; la bande les lit sans rien inventer.

Cas réel du test (`test_accueil_bande_pluie.cpp`) : Open-Meteo pour la ville le **09/11/2026 à 12 h 18**, lu à la main dans la réponse d'une
requête faite pendant la session : 12 h 00 → 12 h 15 : 1,9 mm ; 12 h 15 → 12 h 30 : 0,7 mm ; 12 h 30 → 12 h 45 : 0,1 mm ; 12 h 45 → 13 h 00 :
0,1 mm ; puis 0,0 mm. Une seconde requête, refaite ensuite pour confirmer, a rendu des zéros (autre modèle ou autres coordonnées) : **ces
valeurs ne se rejouent pas**, elles sont écrites en dur dans le test avec leur provenance. Aucune coordonnée précise n'est écrite dans le dépôt.

## 3. Constaté

| Quoi | Résultat | Comment |
|---|---|---|
| Nouveaux tests du noyau | **20 tests** dans `test/host/test_accueil_bande_pluie.cpp`, évalués à la compilation (`constexpr`) **et** à l'exécution | `python test/host/lancer.py --seulement test_accueil_bande_pluie` |
| Tests du noyau, tous | **2 025 tests, 0 échec**, le lanceur répond « tout passe » (558 s), rejoués sur la branche rebasée sur `origin/main` | `python test/host/lancer.py` en mode complet (compilé + exécuté), clang++ 23.1.5, ASan + UBSan ; voir § 3 bis |
| Mutations ciblées | **22 sur 22 détectées**, chacune par le test prévu. **La 17e ne l'était pas** au premier passage (le repère « dans une heure » qui tombe pile au bord de la bande, `<` changé en `<=`) : aucun test n'avait une bande d'exactement une heure. Un cas limite (quatre quarts, « maintenant » au début : le repère sort ; cinq quarts : 800 ‰) a été ajouté au test visé | `python test/host/lancer.py --seulement test_accueil_bande_pluie --mutations` (22 mutations, 0 problème après l'ajout) |
| Rendus regardés | **une vingtaine de PNG** vus un par un (paysage 1280×720 et 1024×600, portrait 720×1280 ; français, allemand, néerlandais, espagnol, turc en 1024×600), scènes `bande-reelle`, `bande-plus-tard`, `bande-longue`, `pluie-proche`, `jaune-et-pluie`. `alerte-rouge-et-pluie` et `pluie-en-cours` sont rendues et gardées mais n'ont été regardées qu'avant la dernière correction de la fixture | `.\tools\rendre.ps1 -Scene auto -Instant 2026-11-08T07:45:00Z` (et `2026-11-09T11:18:00Z` pour la bande réelle) |
| Paysage inchangé par la règle du portrait | 4 images comparées (`pluie-proche` 1024×600 et 1280×720, `jaune-et-pluie` 1024×600, `pluie-en-cours` 1280×720) : **identiques à l'octet** avant et après | `Get-FileHash` |
| Compilation Tab5 | réussie : `meridien.bin` **3 306 560 octets** (21 % libres) ; `origin/main` (`5d5f3ad`) : 3 299 568 → **+6 992 octets** (+0,21 %) | `.\tools\idf.ps1 "-DBOARD=tab5" set-target esp32p4` puis `build` ; carte relue dans `BOARD:STRING=tab5` ; une seule alerte, le message CMake connu `gdbinit` |
| Compilation 7B | réussie : `meridien.bin` **3 284 768 octets** (22 % libres) ; avant : 3 277 712 → **+7 056 octets** (+0,22 %) | même commande avec `waveshare-7b` ; `BOARD:STRING=waveshare-7b` |
| RAM statique (DIRAM) | Tab5 : 145 842 → **146 098 octets** (+256) ; 7B : 145 624 → **145 944 octets** (+320) ; **`.bss` inchangé** (41 144 / 40 936), tout est dans `.data` (l'état `g.bande`) | `idf.ps1 size` |
| Le code est dans le binaire | la chaîne « fin dans %d min » est une fois dans chaque `meridien.bin` ; le symbole `texte_de_bande` est dans les deux ELF. `dessiner_bande_pluie` et `composer_bande_pluie` sont **intégrées** par le compilateur (pas de symbole) : leur présence n'est pas prouvée par `nm` | `riscv32-esp-elf-nm -C`, comptage de la chaîne dans le `.bin` |

Les chiffres « avant » viennent d'une compilation de `origin/main` (`5d5f3ad`) faite dans le même dossier de build juste avant, avec les mêmes commandes.

### 3 bis. Ce qui a été exécuté en local, et ce qui ne l'a pas été

- **Exécuté** : tous les tests du noyau (2 025, 0 échec) ; **347 mutations** des 13 fichiers de test `test_accueil*` et `test_visuel*` (dont les 22 de la bande et les 3 des textes : **0 problème**, 342 s) ;
  `clang-format` sur les fichiers C++ modifiés (le crochet de commit le rejoue sur les fichiers indexés) ; `gitleaks` à chaque commit ;
  `tools/controle_publication.py --historique` (0 trouvaille bloquante ; les valeurs « à juger » sont de l'historique ancien) ;
  `tools/controle_liens.py` (aucune des 10 erreurs ne vient des fichiers de ce travail : elles sont dans cinq autres pages, venues d'autres PR).
- **Non exécuté jusqu'au bout** : `tools\verifier.ps1` en entier. Sa passe de **toutes les mutations du dépôt** dure plusieurs heures sur ce PC partagé
  (un premier passage a relevé l'unique échec de ce travail, la mutation 17 de la bande, corrigée depuis) ; je l'ai arrêtée au second passage. Les mutations des **autres
  sujets** (hors `accueil` et `visuel`, le reste de la liste d'environ 1 530 d'après `ci.yml`) ne sont **pas rejouées** : leur code n'est **pas modifié** par ce travail. **La CI ne les
  rejoue pas non plus** (job `mutations-noyau` à lancement manuel seulement) ; la CI de la PR rejoue les tests du noyau (clang++-18 et g++-14, avec ASan), `ruff`,
  `clang-format`, `gitleaks` et la compilation du Tab5.

## 4. Ce que les rendus montrent

| État | Ce qui s'affiche | Capture |
|---|---|---|
| Pluie réelle de 12 h 18 | « Pluie modérée » à gauche, trois barres (moyenne, faible, faible) puis des traits secs, l'heure sous chaque demi-heure, repère « maintenant » en haut, repère pointillé « dans une heure » | `auto-bande-reelle-1280x720.png`, `-1024x600`, `-720x1280`, `-de-`, `-tr-` |
| Pluie dans 15 min (08 h 45) | « Pluie dans 15 min », un trait sec puis les barres qui montent et redescendent | `auto-pluie-proche-*` |
| Pluie plus tard | « Pluie dès 10 h 30 », des traits secs puis une barre | `auto-bande-plus-tard-1024x600.png` |
| Il pleut jusqu'au bout de la bande | pas de « fin dans N min » (la fin n'est pas dans les données) | `auto-bande-longue-1024x600.png` |
| Alerte jaune + pluie | la bande à gauche, l'alerte à droite, chacune à sa place | `auto-jaune-et-pluie-1024x600.png` |
| Alerte rouge ou orange agrandie | **inchangée** : la grande carte d'alerte, la pluie dite par sa petite tuile | `auto-alerte-rouge-et-pluie-*` |
| Portrait 720×1280 | sans alerte : la bande prend **toute la largeur** (la moitié, 320 px de référence, ne tenait ni la phrase — « Pluie fai… » — ni les barres) ; avec une alerte : la pastille d'avant | `auto-pluie-proche-720x1280.png` |

La phrase perd d'abord « · fin dans N min » avant d'être coupée par « … » : sur la moitié du bandeau c'est presque toujours le cas, et ce sont les
barres et les heures qui disent quand la pluie s'arrête. Le turc (« Orta şiddetli yağmur ») passe à la petite police et aux heures pleines seulement.

**Une erreur de ma fixture, corrigée** : la première version du banc mettait la pluie « dans 12 min » à l'intérieur du quart d'heure en cours ; le rendu
disait « Pluie faible » (il pleut) à côté d'une pastille « pluie proche ». L'appareil ne produit jamais ça : `meteo_riche.hpp` lit « il pleut » dès que le
**quart en cours** est pluvieux, et « dans N min » depuis le début du premier quart pluvieux, tous deux sur les mêmes quarts. La fixture suit
maintenant cette règle (la pluie commence au premier début de quart d'heure à `dans_min` minutes ou plus : 15 min à 08 h 45).

**Captures gardées** (`docs/captures/ecran/`) : celles que ce travail change sont refaites (`auto-pluie-proche-*`, `auto-pluie-en-cours-*`,
`auto-jaune-et-pluie-1024x600`, `auto-alerte-rouge-et-pluie-*`) et sept nouvelles s'ajoutent (`auto-bande-*`, environ 470 Ko). Les autres
`auto-*` n'ont **pas** été refaites : elles diffèrent déjà du rendu actuel de `main` (49 sur 49 au test d'identité, par exemple le ciel) sans
rapport avec ce travail. **`auto-planche-sept-langues-1024x600.png` montre encore l'ancienne grande carte pluie** (plus choisie) : à refaire ou à
retirer avec le code de cette carte.

## 5. Non constaté

- **Tout ce qui se joue sur une carte** : le dessin sur les panneaux (couleurs réelles, lisibilité à un mètre et à deux), la hauteur réelle des barres sur la
  7B (la ligne ne fait que 62 px de référence, environ 50 px sur cette carte), le temps de dessin, la mémoire interne libre en marche, le comportement sur
  une vraie journée de pluie, l'apparition et la disparition de la bande avec la vraie pluie.
- **Les fournisseurs météo** : seule la lecture riche d'Open-Meteo (`meteo_riche.hpp`) a été lue pour remplir les quarts. Si un autre fournisseur ne les
  remplit pas, `nb_quarts` vaut 0 et la bande ne s'affiche pas (la pastille d'avant reste) : **non vérifié**.
- Les **cinq langues écrites par l'IA** (allemand, néerlandais, espagnol, italien, turc) : jamais relues par un locuteur ; l'italien n'a pas été regardé au rendu
  (seulement le français, l'allemand, le néerlandais, l'espagnol et le turc).
- La **probabilité** de la pluie : la bande ne dit que les cumuls par quart d'heure du modèle.
- Le **portrait** n'a pas été vu par Axel ; la règle « toute la largeur sans alerte, pastille d'avant avec alerte » est un choix de l'auteur.
- La grande carte pluie n'est **pas retirée du code** (elle n'est plus atteinte) : elle part, avec ses tests, quand la bande est validée sur une carte.

## 6. Pour refaire

```powershell
python test/host/lancer.py --seulement test_accueil_bande_pluie --mutations
.\tools\rendre.ps1 -Scene auto -Variante bande-reelle -Instant 2026-11-09T11:18:00Z
.\tools\rendre.ps1 -Scene auto -Instant 2026-11-08T07:45:00Z
.\tools\idf.ps1 "-DBOARD=tab5" set-target esp32p4
.\tools\idf.ps1 build
```
