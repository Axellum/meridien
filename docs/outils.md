# Development tools

## English · [Français](#version-française)

---

Installed on 06/11/2026 at Axel's request ("install all the tools you need to maximize speed and quality"), **outside the repository**, without changing the system PATH.

### What is installed

| Tool | Version | Where | Used for |
|---|---|---|---|
| llvm-mingw: `clang`, `clang++`, `lld`, `llvm-symbolizer` | clang 23.1.5 (release 20261104, ucrt x86-64) | `H:\AuxFilsDesIdees\_outils\llvm-mingw\` (`VERSION.txt`: source and verified SHA-256) | compiling **and running** the core tests on the PC; memory-error detector (AddressSanitizer) and undefined-behaviour detector (UBSan) |
| `clang-format` | 23.1.5 | same, and pip package (`--user`) | one single C/C++ formatting for every session |
| `clang-tidy` | 23.1.4 (pip) / 23.1.5 | same | static analysis of the pure core |
| `gitleaks` | 8.30.1 | `H:\AuxFilsDesIdees\_outils\gitleaks\` (`VERSION.txt`: source and verified SHA-256) | secret scan of the index (before a commit) and of the history |
| `pre-commit` (pip package) | installed by pip (`--user`) | pip `Scripts` folder | **not used**: the repository hook is a small home-made script (`.githooks/pre-commit`), with no download of third-party hooks |
| `ruff`, `pytest`, `playwright`, `esptool`, `pyserial`, Node | already present before | — | Python tests, settings-page run-through, flashing, fake-device server |
| Pillow (pip, `--user`) | 12.3.0 | pip | drawing the weather icons (`tools/icones/generer_icones.py`) and assembling the PNG sheets (`rendre.ps1 -Planche`); a development tool, not shipped in the firmware |

llvm-mingw download: <https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104>, 191,037,046-byte archive, SHA-256 `552191b5…1b40f` identical to the one published by GitHub; once extracted, the archive is kept in `_outils\_telechargements\`.

### Using the tools

```powershell
. H:\AuxFilsDesIdees\_outils\env_hote.ps1 -Sanitizers   # PATH + CXX for this PowerShell session only
python test/host/lancer.py                              # must print: mode "complet (compilé + exécuté)"
python test/host/lancer.py --mutations                  # every mutation of the core must be detected
clang-format --dry-run -Werror components/core/include/ecran/*.hpp
```

Without `-Sanitizers`, `CXX` points to plain `clang++` (faster, no detectors): useful for the mutations. `clang++-san.cmd` adds `-fsanitize=address,undefined`.

### Automatic checks

| What | How | Blocks? |
|---|---|---|
| **Before every commit** | hook `.githooks/pre-commit` → `tools/verifier.ps1 -Rapide` (only what is in the git index) | yes: secrets, core tests, ruff and C/C++ formatting |
| **On demand, whole repository** | `.\tools\verifier.ps1` (add `-Web` for the Playwright run-through of the page, which needs the network) | same, plus the mutations |
| **On GitHub** | `.github/workflows/ci.yml` (next section) | the jobs listed below once `main` is protected |

- The hook is enabled in the repository by `git config core.hooksPath .githooks` (done on 06/11/2026). Every worktree sees it, but the `.githooks` folder only exists in a worktree after `main` has been merged into it: until then, run `.\tools\verifier.ps1` by hand.
- **Never** bypass it with `--no-verify`: fix the cause. The hook also refuses a commit if the test runner is not in "complet" mode (nothing executed).
- **Formatting is blocking.** `.clang-format` (LLVM, 4 spaces, 120 columns, comments not reflowed) is the rule; `.clang-format-ignore` leaves the generated files alone (`fuseaux_donnees.inc`, `test/host/donnees/*`). Fix with `clang-format -i --style=file <file>`. A table aligned on purpose is protected with `// clang-format off` … `// clang-format on`. The version is pinned (23.1.5, the same on the PC and in the CI) because another version can format differently.
- `ruff.toml` belongs to the project: without it ruff inherited a stricter configuration found higher up in `H:\AuxFilsDesIdees` (security rules of the Tab5 repository).
- Proof of 06/11/2026 (before the formatting became blocking): a fake Slack token in the index → commit refused (gitleaks, code 1); a broken assertion (`NOMBRE_CODES == 30`) → commit refused (code 1, compiler error on the line); in both cases no commit was created and the tree came back clean.
- Proof of 06/11/2026 after the one-shot reformatting (clang-format 23.1.5): complete `.\tools\verifier.ps1` (mutations included) on commit `034a3d7`, 28 min 59 s, exit code 0: secrets OK, 252 tests executed with AddressSanitizer + UBSan, 206 mutations out of 206 detected (0 problem), ruff OK on 9 files, formatting OK on 36 files (the 10 generated files are excluded). Run again, with the same result, on commit `29a548e` (27 min 20 s). Seen on: the PC (clang++ 23.1.5, Windows), no board involved.

### Continuous integration (GitHub Actions)

`.github/workflows/ci.yml`, triggered by a push to `main`, a pull request, or by hand (`workflow_dispatch`). The repository is private, so minutes are billable: the long jobs only run where they are needed. Read-only token (`contents: read`). Every action is pinned to the commit SHA of its tag (tag read in the action's own repository, kept as a comment in the file); pip tools and the downloaded `gitleaks` use the exact version of the PC.

| Job (stable name) | What it does | Runs on | Make it required? |
|---|---|---|---|
| `tests-noyau` | `python test/host/lancer.py` with clang++-18 (`-fsanitize=address,undefined`), then with g++-14 (AddressSanitizer only: g++ 14.2 cannot evaluate the tests' `static_assert` once `-fsanitize=undefined` is on); fails if the runner is not in "complet" mode | pull request, push, manual | yes |
| `ruff` | `ruff check` (ruff 0.16.10, `ruff.toml`) on every tracked `.py` | pull request, push, manual | yes |
| `clang-format` | `clang-format --dry-run -Werror` (23.1.5) on every tracked C/C++ file | pull request, push, manual | yes |
| `gitleaks` | `gitleaks git --redact` over the whole history; CLI 8.30.1 whose SHA-256 is checked (the `gitleaks-action` is free for a personal account but needs a licence key for an organisation) | pull request, push, manual | yes |
| `firmware-tab5` | official `espressif/idf` image, ESP-IDF 5.5.5 fixed by digest, through `espressif/esp-idf-ci-action`; `idf.py set-target esp32p4 && idf.py build && idf.py size`, then checks `build/meridien.bin`, the project name/target, the managed components and an unchanged `dependencies.lock` | pull request, push, manual | yes |
| `mutations-noyau` | `python test/host/lancer.py --mutations` with clang++-18 (compilation only) | push to `main`, manual | no (does not run on pull requests) |
| `parcours-web` | Playwright run-through of the settings page (Chromium, Node, real Open-Meteo calls) | push to `main`, manual | no (depends on an external service) |

What the CI proves and does not prove: it compiles the Tab5 firmware and runs the core tests with a Linux compiler. It proves **nothing** about the screen, the touch, the memory or the timing on the board: nothing is flashed. The Waveshare 7B has no board layer yet, so it has no job; it will be added to `firmware-tab5` when it exists.

Measured on 06/11/2026 (commit `29a548e`, runner `ubuntu-24.04` with clang 18.1.3 and g++ 13/14, private repository, no board involved): all seven jobs green in the last manual run. Durations: `tests-noyau` 3 to 5.5 min, `firmware-tab5` about 6 min (fresh `sdkconfig`, managed components downloaded; `build/meridien.bin` is 811 200 bytes, the same size as the PC build, its SHA-256 was not compared byte for byte with the PC's), `ruff`, `clang-format` and `gitleaks` under 11 s each, `parcours-web` about 1 min (49 checks out of 49, Node 22), `mutations-noyau` 35 to 48 min depending on the runner (206 mutations out of 206 detected). The first full run failed `parcours-web` for a real reason: the reformatting had split the weather table that the fake device (`web/faux-serveur/serveur.mjs`) reads with a regular expression, so it could not start; that table is now protected by `// clang-format off` … `// clang-format on` in `meteo_wmo.hpp`. Not covered: the memory leak detector is off (as on the PC); g++ is run without UBSan (see above); branch protection is not set up by the workflow itself.

### Rendering the screen on the PC (no board)

The board is a bottleneck (one unit, one flash at a time), so the interface can be drawn on the PC and written as a PNG, and the screens of a sequence (the guided set-up, for example) can be gathered in one **sheet**. No board, no serial port, no lock, no ESP-IDF build.

```powershell
.\tools\rendre.ps1                                  # every scene, its default languages and sizes -> docs\captures\ecran\
.\tools\rendre.ps1 -Lister                          # scenes, variants, languages, sizes, sequences
.\tools\rendre.ps1 -Scene bonjour -Taille 720x1280  # one scene (or one sequence), one size
.\tools\rendre.ps1 -Scene temoin -Langue fr,de,tr -Planche   # + one sheet per language
.\tools\rendre.ps1 -Taille 1280x720,1024x600        # the size is a parameter: any WxH, several separated by commas
.\tools\rendre.ps1 -Variante attente                # one variant
.\tools\rendre.ps1 -Instant 2026-11-06T14:05:00Z    # another fixed instant (UTC)
.\tools\rendre.ps1 -Nettoyer                        # forget the build cache (LVGL included)
```

Output, in `docs/captures/ecran/`: `<scene>-<variant>[-<language>]-<width>x<height>.png` (no suffix for French), for example [`bonjour-connecte-720x1280.png`](captures/ecran/bonjour-connecte-720x1280.png) (the screen the board runs today), and for a sheet `planche-<sequence>[-<language>]-<width>x<height>.png`. Two renders of the same code give the same bytes. The script refuses to start if `ninja` or `esphome` is running (one heavy build at a time on this PC; `-Forcer` overrides) and fails, rather than report success, when nothing was rendered.

**A scene is one file**, `tools/rendu/scenes/scene_<name>.cpp`, found by itself: adding one touches neither the bench nor the script. It holds directives in comments, read by `rendre.py`, and the table `kScenes[]`: for each variant its name (the PNG file name), its French caption, and the function that builds the screen with its sample data. `scene_temoin.cpp` is the shortest model; `scene_bonjour.cpp` and `scene_principal.cpp` compile the real firmware screens.

| Directive | Meaning |
|---|---|
| `// rendu-sources: main/ui_x.cpp ...` | files of the repository compiled with the scene. A scene whose file does not exist yet is skipped with a notice (this is how `ui_principal` will be picked up when it exists) |
| `// rendu-langues: fr de tr` | languages the scene can show (`fr en de nl es it tr`, or `toutes`); default `fr`. `-Langue` asks for others; one the scene cannot show is skipped with a notice |
| `// rendu-langues-defaut: fr de tr` | those rendered without `-Langue` (default: all it can show) |
| `// rendu-tailles: 1280x720` | default sizes (default `720x1280 1280x720`); `-Taille` overrides with any size |
| `// rendu-sequence: installation 3` | sequence and rank, for sheets (default: the scene name, rank 0). Order in a sheet: rank, scene file name, then order in `kScenes[]` |

**The main screen.** `scene_principal.cpp` (sequence `principal`, three sizes by default: 1280x720, 1024x600, 720x1280; languages `fr de tr`) compiles `main/ui_principal.cpp` and `main/ui_meteo_icones.cpp` unchanged, in thirteen variants: sun, rain, snow, storm, fog, cold night, heat wave with an extreme UV index, the data the device receives today (no hourly strip, no pressure trend, no rain amount), stale weather, the « Exemple » mention (shown when the weather comes from the stand-in of the weather contract), no weather received, time not yet received from the network, and the worst cases of width (longest city and label, wide digits, negative temperatures). The sample data are in `scene_principal_donnees.hpp` (invented, not measurements); the weather labels come from the real pure core and the time and date from the real Europe/Paris zone. The screen is drawn for 1280x720 (or 720x1280) and scaled to the other sizes; the 1024x600 render only simulates the size of the 7B: the board itself has not been tried.

**The themes.** `scene_principal_themes.cpp` (sequence `principal-themes`) shows the four themes of the pure core (`ciel`, `tableau`, `horloge`, `clair`) on eight weathers each (`<theme>-soleil|pluie|neige|orage|brouillard|nuit|chaleur|pires`), to compare the directions of [design/ecran-principal.md](design/ecran-principal.md); one sheet per theme: `-Variante ciel-soleil,ciel-pluie,... -Planche -Colonnes 4`. Five more variants (`bascule-*`) build the screen with one theme, draw it, then switch it live with `ui_principal_changer_theme`; `python tools\rendu\verifier_bascule.py <folder>` checks that each result is identical, pixel for pixel, to the screen built directly with the new theme (the switch towards `horloge` is refused and must leave the screen unchanged).

**Timing.** With `BANC_CHRONO=1` the bench prints, for each render, the construction time of the screen and of the first full render, in microseconds of the PC (relative figures between screens or versions of the drawing; they say nothing of the speed of the board).

**The icons.** `python tools\icones\generer_icones.py` (needs Pillow) draws the weather icons, writes `main/assets/icones_meteo.bin` (A8 images, 825,032 bytes), their table for the pure core (`visuel_icones_table.inc`) and the one of the screen (`main/ui_meteo_icones_table.inc`); `--verifier` checks that the files in the tree match the drawings. The bench embeds the same binary as `main/CMakeLists.txt` does.

**The settings over the device host.** `scene_hote_appareil.cpp` runs the real settings and install screens over the real `HoteAppareil` (pure core) with a fake memory and a fake device (`test/host/hote_appareil_faux.hpp`): five journeys (single-source display, a service key that never comes back, a Wi-Fi kept only after a success, an install that resumes, a full reset). `.\tools\rendre.ps1 -Scene hote_appareil` exits 1 when a check fails. **The same screens on the board** are exercised by `main/autotest_reglages.cpp`, built only with `.\tools\idf.ps1 -DMERIDIEN_AUTOTEST_REGLAGES=ON build` (never in the normal binary): a virtual pointer replays the gestures through LVGL's own object search in three phases separated by real restarts, and writes one `AUTOTEST OK|ECHEC` line per check on the serial log. It does not use the touch panel (so it proves nothing about a real finger) and it must feed **raw** panel coordinates, because LVGL rotates the point it reads ([depannage.md](depannage.md)). It is flashed under the board lock, app only; flash the normal binary back afterwards.

**Language.** `-Langue` sets the current language of the pure core (`ecran::i18n::definir_langue`) before the scene is built: interface code that calls `tr("texte français")` shows the text of the table (`i18n/*.yaml`, ADR-0006) with the same fallbacks as on the board (language, then English, then the French key). The scene also receives it in `banc::Contexte`, with the size and the display. `bonjour` is French only (it does not use `tr()` yet).

**Size.** Any `WxH`. The bench declares a display of that size; `board_get_info()` answers Tab5 ST7121, or, for 1024×600, Waveshare ESP32-P4 7B, EK79007, GT911 (strings of [compatibilite.md](compatibilite.md), a fixture: nothing of the 7B is run).

**Sheets.** `-Planche` gathers, for each sequence, language and size, the rendered screens in one PNG: 3 columns of 560 px in landscape (4 of 440 px from seven screens, 4 of 330 px in portrait; `-Colonnes` forces), the caption of each variant under its screen, a title with the language and the size. The caption is shown as written: number the steps in it (« Étape 3 : Lieu »). Sheets need Pillow (already on this PC; otherwise `python -m pip install --user pillow`), nothing else does.

**What is the same as the firmware.** LVGL 9.5.0 (checked against `dependencies.lock`), compiled with the firmware's own configuration (`tools/rendu/sdkconfig_lvgl.h`, a copy of the 106 `CONFIG_LV_*` lines of the firmware's `sdkconfig`; the script compares it with the `sdkconfig` of the repository when there is one, and warns of any difference); RGB565 display with partial 50-line buffers like `bsp_tab5`; LVGL's default theme; TinyTTF with the same `main/assets/roboto_700.ttf`. The interface files (`main/ui_*.cpp`) and `main/horloge.cpp` are compiled **unchanged**; the date and time text comes from the real pure core.

**What is replaced.**

| Firmware | On the PC | Where |
|---|---|---|
| `esp_log.h`, `freertos/*.h`, `esp_err.h`, `esp_rom_sys.h` | minimal stand-ins | `tools/rendu/bouchons/` |
| the `board_*` functions of `board.h` (the real header is used) | fixed answers: the identity of the board for the size asked | `tools/rendu/banc.cpp` |
| `reseau_etat()` (Wi-Fi, SNTP), weather state | state chosen by the variant | `tools/rendu/scenes/scene_*.cpp` |
| `std::time()` | the fixed instant (`_time64` is overridden) | `tools/rendu/banc.cpp` |
| display, DSI, touch | an in-memory buffer, no touch | `tools/rendu/banc.cpp` |

LVGL sources are found in `managed_components/lvgl__lvgl` of the repository (after an `idf.py reconfigure`), then in the main checkout, then in its `.cache/composants/`; `MERIDIEN_LVGL` forces a folder. Build cache: `.cache/rendu/` (ignored by git). A `main/ui_*.cpp` file must include no ESP-IDF header other than those listed above (rule 3 of the interface contract between sessions): otherwise the missing stand-in is added to the bench, never patched into the interface.

**What this render does NOT cover** (it is LVGL's software drawing on x86-64, not the panel):

- **Colours as the panel shows them**: the bench widens the 5-6-5 bits to 8 bits by repeating the high bits; the real panel's gamma, white point and internal processing are not simulated, and not compared with a photo of the board (not verified).
- **Tearing, DSI timings, DMA2D copy, PPA, refresh rate, speed and memory use**: nothing is measured here (x86-64, not the RISC-V).
- **Touch, backlight, brightness.**
- **Invented data**: boot number 42, 612 ms, Wi-Fi states, temperatures and ranges of the main screen: fixtures, not measurements.
- **1280x720**: a simulated size of the display. The board native size is 720x1280 and the landscape orientation is being developed on another branch; the current « Bonjour » screen was drawn for 720x1280 and overflows at the bottom in 1280x720 (seen in `bonjour-*-1280x720.png`).
- **A pixel-by-pixel comparison with the board**: not done. The same font file, the same LVGL version and configuration make the same layout expected, not proved.
- **Translations**: the sheets show what `tr()` returns; the German and Turkish texts were written by an AI and not reviewed (see [`i18n/README.md`](../i18n/README.md)). Whether a long German text fits a button on the real panel is shown by the render, not proved on the board.

**What the witness scene `temoin` showed** (`temoin-langues-*.png`, 1280x720 on the PC render): `-Langue de` and `-Langue tr` reach `tr()` (« Helligkeit », « Yeniden başlat », « Parlaklık »), and the test line of special characters (French `àâäçéèêëîïôöùûüÿœæ`, German `äöüß`, Dutch `ëï`, Spanish `áéíóúñ¿¡`, Italian `àèéìòù`, Turkish `çğıöşü ÇĞİÖŞÜ`) is drawn with no missing glyph by Roboto Bold through TinyTTF (seen by eye on the PC render; on the board: not tried).

Measured on 06/11/2026 (`-Nettoyer` then every scene, 7 PNG): 11.1 s from scratch (463 LVGL files compiled with 4 jobs); 1.7 s on the next run. Seen on: the PC (Windows, clang 23.1.5 llvm-mingw, LVGL 9.5.0), no board, commits `a1b67be`, `2345297` and `e00f791`. Main screen, measured on 07/11/2026 (branch `claude/ecran-visuel-ba722a`, commit `049de21` and the next ones): the 126 PNG and 9 sheets of the scene `principal` in 17.8 s with the LVGL library in cache (4 jobs); 12 PNG of `principal-themes` (one size) in 12 s on the first run, which compiled LVGL again. The nine sheets weigh about 1.1 to 1.4 MB each because of the dithered sky, so only five sheets and four screens are kept in `docs/captures/ecran/`; the rest is rebuilt in seconds.

### Not installed (on purpose)

- **Interactive LVGL simulator on the PC** (SDL2, with touch) and automatic comparison with reference screenshots in the CI: phase 5 of the [plan](plan.md). The PNG render above is the first step.
- **cppcheck**: `clang-tidy` covers the need for now.
- No tool replaces a test on the board: a test that passes on the PC (x86-64) says nothing about memory, alignment or timing on the RISC-V.

---

## Version Française

---

# Outils de développement

Installés le 06/11/2026 sur demande d'Axel (« installe tous les outils qu'il te faut pour maximiser rapidité et qualité »), **hors du dépôt**, sans modifier le PATH du système.

### Ce qui est installé

| Outil | Version | Où | Sert à |
|---|---|---|---|
| llvm-mingw : `clang`, `clang++`, `lld`, `llvm-symbolizer` | clang 23.1.5 (publication 20261104, ucrt x86-64) | `H:\AuxFilsDesIdees\_outils\llvm-mingw\` (`VERSION.txt` : source et SHA-256 vérifié) | compiler **et exécuter** les tests du noyau sur le PC ; détecteurs d'erreurs mémoire (AddressSanitizer) et de comportements indéfinis (UBSan) |
| `clang-format` | 23.1.5 | idem, et paquet pip (`--user`) | une seule mise en forme du C/C++ pour toutes les sessions |
| `clang-tidy` | 23.1.4 (pip) / 23.1.5 | idem | analyse statique du noyau pur |
| `gitleaks` | 8.30.1 | `H:\AuxFilsDesIdees\_outils\gitleaks\` (`VERSION.txt` : source et SHA-256 vérifié) | recherche de secrets dans l'index (avant commit) et dans l'historique |
| `pre-commit` (paquet pip) | installé par pip (`--user`) | dossier `Scripts` de pip | **non utilisé** : le crochet du dépôt est un petit script maison (`.githooks/pre-commit`), sans téléchargement de crochets tiers |
| `ruff`, `pytest`, `playwright`, `esptool`, `pyserial`, Node | déjà présents avant | — | tests Python, parcours de la page, flash, serveur du faux appareil |
| Pillow (pip, `--user`) | 12.3.0 | pip | dessiner les icônes météo (`tools/icones/generer_icones.py`) et assembler les planches PNG (`rendre.ps1 -Planche`) ; outil de développement, pas livré dans le firmware |

Téléchargement d'llvm-mingw : <https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104>, archive de 191 037 046 octets, SHA-256 `552191b5…1b40f` identique à celui publié par GitHub ; extraite, l'archive est gardée dans `_outils\_telechargements\`.

### Se servir des outils

```powershell
. H:\AuxFilsDesIdees\_outils\env_hote.ps1 -Sanitizers   # PATH + CXX pour cette session PowerShell seulement
python test/host/lancer.py                              # doit afficher : mode « complet (compilé + exécuté) »
python test/host/lancer.py --mutations                  # chaque mutation du noyau doit être détectée
clang-format --dry-run -Werror components/core/include/ecran/*.hpp
```

Sans `-Sanitizers`, `CXX` pointe sur `clang++` simple (plus rapide, sans détecteurs) : utile pour les mutations. `clang++-san.cmd` ajoute `-fsanitize=address,undefined`.

### Contrôles automatiques

| Quoi | Comment | Bloque ? |
|---|---|---|
| **Avant chaque commit** | crochet `.githooks/pre-commit` → `tools/verifier.ps1 -Rapide` (ce qui est dans l'index seulement) | oui : secrets, tests du noyau, ruff et mise en forme C/C++ |
| **À la demande, tout le dépôt** | `.\tools\verifier.ps1` (ajouter `-Web` pour le parcours Playwright de la page, qui demande le réseau) | idem, plus les mutations |
| **Sur GitHub** | `.github/workflows/ci.yml` (section suivante) | les jobs listés ci-dessous, une fois `main` protégée |

- Le crochet est activé dans le dépôt par `git config core.hooksPath .githooks` (fait le 06/11/2026). Il est vu par tous les worktrees, mais le dossier `.githooks` n'existe dans un worktree qu'après y avoir fusionné `main` : avant cela, lancer `.\tools\verifier.ps1` à la main.
- **Ne jamais** contourner avec `--no-verify` : corriger la cause. Le crochet refuse aussi un commit si le lanceur de tests n'est pas en mode « complet » (rien d'exécuté).
- **La mise en forme est bloquante.** `.clang-format` (LLVM, 4 espaces, 120 colonnes, commentaires non recoupés) fait foi ; `.clang-format-ignore` laisse les fichiers générés tranquilles (`fuseaux_donnees.inc`, `test/host/donnees/*`). Corriger avec `clang-format -i --style=file <fichier>`. Un tableau aligné exprès se protège avec `// clang-format off` … `// clang-format on`. La version est épinglée (23.1.5, la même sur le PC et dans la CI) parce qu'une autre version peut formater autrement.
- `ruff.toml` est propre au projet : sans lui ruff héritait d'une configuration plus stricte (règles de sécurité du dépôt Tab5) trouvée plus haut dans `H:\AuxFilsDesIdees`.
- Preuve du 06/11/2026 (avant que la mise en forme devienne bloquante) : un faux jeton Slack dans l'index → commit refusé (gitleaks, code 1) ; une assertion cassée (`NOMBRE_CODES == 30`) → commit refusé (code 1, erreur du compilateur sur la ligne) ; dans les deux cas aucun commit créé et l'arbre est revenu propre.
- Preuve du 06/11/2026 après le reformatage d'un seul coup (clang-format 23.1.5) : `.\tools\verifier.ps1` complet (mutations comprises) sur le commit `034a3d7`, 28 min 59 s, code de sortie 0 : secrets OK, 252 tests exécutés avec AddressSanitizer + UBSan, 206 mutations sur 206 détectées (0 problème), ruff OK sur 9 fichiers, mise en forme OK sur 36 fichiers (les 10 fichiers générés sont exclus). Relancé avec le même résultat sur le commit `29a548e` (27 min 20 s). Vu sur : le PC (clang++ 23.1.5, Windows), aucune carte concernée.

### Intégration continue (GitHub Actions)

`.github/workflows/ci.yml`, déclenché par un push sur `main`, une pull request ou à la main (`workflow_dispatch`). Le dépôt est privé : les minutes sont facturables, donc les jobs longs ne tournent que là où il le faut. Jeton en lecture seule (`contents: read`). Chaque action est épinglée par le SHA du commit de son tag (tag lu dans le dépôt de l'action, gardé en commentaire dans le fichier) ; les outils pip et le `gitleaks` téléchargé ont la version exacte du PC.

| Job (nom stable) | Ce qu'il fait | Quand | À rendre obligatoire ? |
|---|---|---|---|
| `tests-noyau` | `python test/host/lancer.py` avec clang++-18 (`-fsanitize=address,undefined`), puis avec g++-14 (AddressSanitizer seul : g++ 14.2 ne sait plus évaluer les `static_assert` des tests dès que `-fsanitize=undefined` est actif) ; échoue si le lanceur n'est pas en mode « complet » | pull request, push, manuel | oui |
| `ruff` | `ruff check` (ruff 0.16.10, `ruff.toml`) sur tous les `.py` suivis | pull request, push, manuel | oui |
| `clang-format` | `clang-format --dry-run -Werror` (23.1.5) sur tous les fichiers C/C++ suivis | pull request, push, manuel | oui |
| `gitleaks` | `gitleaks git --redact` sur tout l'historique ; CLI 8.30.1 dont le SHA-256 est vérifié (la `gitleaks-action` est gratuite pour un compte personnel mais demande une clé de licence pour une organisation) | pull request, push, manuel | oui |
| `firmware-tab5` | image officielle `espressif/idf`, ESP-IDF 5.5.5 figé par son empreinte, via `espressif/esp-idf-ci-action` ; `idf.py set-target esp32p4 && idf.py build && idf.py size`, puis vérifie `build/meridien.bin`, le nom et la cible du projet, les composants gérés et un `dependencies.lock` inchangé | pull request, push, manuel | oui |
| `mutations-noyau` | `python test/host/lancer.py --mutations` avec clang++-18 (compilation seule) | push sur `main`, manuel | non (ne tourne pas sur les pull requests) |
| `parcours-web` | parcours Playwright de la page de réglages (Chromium, Node, vrais appels à Open-Meteo) | push sur `main`, manuel | non (dépend d'un service extérieur) |

Ce que la CI prouve et ne prouve pas : elle compile le firmware du Tab5 et exécute les tests du noyau avec un compilateur Linux. Elle ne prouve **rien** sur l'écran, le tactile, la mémoire ou le temps sur la carte : rien n'est flashé. La Waveshare 7B n'a pas encore de couche carte, donc pas de job ; il sera ajouté à `firmware-tab5` quand elle existera.

Mesuré le 06/11/2026 (commit `29a548e`, runner `ubuntu-24.04` avec clang 18.1.3 et g++ 13/14, dépôt privé, aucune carte en jeu) : les sept jobs sont verts dans le dernier lancement manuel. Durées : `tests-noyau` 3 à 5,5 min, `firmware-tab5` environ 6 min (`sdkconfig` neuf, composants gérés téléchargés ; `build/meridien.bin` fait 811 200 octets, la taille du build du PC, dont le SHA-256 n'a pas été comparé octet par octet), `ruff`, `clang-format` et `gitleaks` moins de 11 s chacun, `parcours-web` environ 1 min (49 vérifications sur 49, Node 22), `mutations-noyau` de 35 à 48 min selon le runner (206 mutations détectées sur 206). Le premier run complet a fait échouer `parcours-web` pour une vraie raison : le reformatage avait coupé le tableau météo que le faux appareil (`web/faux-serveur/serveur.mjs`) lit par expression régulière, qui ne démarrait donc plus ; ce tableau est maintenant protégé par `// clang-format off` … `// clang-format on` dans `meteo_wmo.hpp`. Non couvert : le détecteur de fuites mémoire est coupé (comme sur le PC) ; g++ tourne sans UBSan (voir plus haut) ; la protection de la branche `main` n'est pas posée par le workflow lui-même.

### Rendu de l'écran sur le PC (sans carte)

La carte est un goulot (un exemplaire, un flash à la fois) : l'interface peut donc être dessinée sur le PC et écrite en PNG, et les écrans d'une séquence (l'installation guidée, par exemple) peuvent être réunis dans une seule **planche**. Ni carte, ni port série, ni verrou, ni compilation ESP-IDF.

```powershell
.\tools\rendre.ps1                                  # toutes les scènes, leurs langues et tailles par défaut -> docs\captures\ecran\
.\tools\rendre.ps1 -Lister                          # scènes, variantes, langues, tailles, séquences
.\tools\rendre.ps1 -Scene bonjour -Taille 720x1280  # une scène (ou une séquence), une taille
.\tools\rendre.ps1 -Scene temoin -Langue fr,de,tr -Planche   # + une planche par langue
.\tools\rendre.ps1 -Taille 1280x720,1024x600        # la taille est un paramètre : toute LxH, plusieurs séparées par des virgules
.\tools\rendre.ps1 -Variante attente                # une variante
.\tools\rendre.ps1 -Instant 2026-11-06T14:05:00Z    # un autre instant fixé (UTC)
.\tools\rendre.ps1 -Nettoyer                        # oublie le cache de compilation (LVGL compris)
```

Sortie, dans `docs/captures/ecran/` : `<scène>-<variante>[-<langue>]-<largeur>x<hauteur>.png` (sans suffixe pour le français), par exemple [`bonjour-connecte-720x1280.png`](captures/ecran/bonjour-connecte-720x1280.png) (l'écran que la carte fait tourner aujourd'hui), et pour une planche `planche-<séquence>[-<langue>]-<largeur>x<hauteur>.png`. Deux rendus du même code donnent les mêmes octets. Le script refuse de démarrer si `ninja` ou `esphome` tourne (une seule compilation lourde à la fois sur ce PC ; `-Forcer` passe outre) et échoue, au lieu d'annoncer un succès, quand rien n'a été rendu.

**Une scène est un seul fichier**, `tools/rendu/scenes/scene_<nom>.cpp`, découvert tout seul : en ajouter une ne touche ni au banc ni au script. Il contient des directives en commentaire, lues par `rendre.py`, et le tableau `kScenes[]` : pour chaque variante son nom (celui du fichier PNG), sa légende française et la fonction qui construit l'écran avec ses données d'exemple. `scene_temoin.cpp` est le modèle le plus court ; `scene_bonjour.cpp` et `scene_principal.cpp` compilent les vrais écrans du firmware.

| Directive | Sens |
|---|---|
| `// rendu-sources: main/ui_x.cpp ...` | fichiers du dépôt compilés avec la scène. Une scène dont un fichier n'existe pas encore est ignorée avec un avis (c'est ainsi que `ui_principal` sera pris en compte quand il existera) |
| `// rendu-langues: fr de tr` | langues que la scène sait afficher (`fr en de nl es it tr`, ou `toutes`) ; défaut `fr`. `-Langue` en demande d'autres ; une langue que la scène ne sait pas afficher est ignorée avec un avis |
| `// rendu-langues-defaut: fr de tr` | celles rendues sans `-Langue` (défaut : toutes celles qu'elle sait afficher) |
| `// rendu-tailles: 1280x720` | tailles par défaut (défaut `720x1280 1280x720`) ; `-Taille` impose n'importe quelle taille |
| `// rendu-sequence: installation 3` | séquence et rang, pour les planches (défaut : le nom de la scène, rang 0). Ordre dans une planche : rang, nom du fichier de scène, puis ordre dans `kScenes[]` |

**L'écran principal.** `scene_principal.cpp` (séquence `principal`, trois tailles par défaut : 1280x720, 1024x600, 720x1280 ; langues `fr de tr`) compile `main/ui_principal.cpp` et `main/ui_meteo_icones.cpp` sans modification, en treize variantes : soleil, pluie, neige, orage, brouillard, nuit froide, canicule avec indice UV extrême, les données que l'appareil reçoit aujourd'hui (ni heure par heure, ni tendance de pression, ni cumul de pluie), météo périmée, la mention « Exemple » (affichée quand la météo vient du bouchon du contrat météo), aucune météo reçue, heure du réseau pas encore reçue, et les pires cas de largeur (ville et libellé les plus longs, chiffres larges, températures négatives). Les données d'exemple sont dans `scene_principal_donnees.hpp` (inventées, pas des mesures) ; les libellés météo viennent du vrai noyau pur, l'heure et la date du vrai fuseau Europe/Paris. L'écran est dessiné pour 1280x720 (ou 720x1280) et mis à l'échelle sur les autres tailles ; le rendu 1024x600 ne fait que simuler la taille de la 7B : la carte elle-même n'a pas été essayée.

**Les thèmes.** `scene_principal_themes.cpp` (séquence `principal-themes`) montre les quatre thèmes du noyau pur (`ciel`, `tableau`, `horloge`, `clair`) sur huit temps chacun (`<thème>-soleil|pluie|neige|orage|brouillard|nuit|chaleur|pires`), pour comparer les pistes de [design/ecran-principal.md](design/ecran-principal.md) ; une planche par thème : `-Variante ciel-soleil,ciel-pluie,... -Planche -Colonnes 4`. Cinq variantes de plus (`bascule-*`) construisent l'écran avec un thème, le dessinent, puis le font basculer à chaud avec `ui_principal_changer_theme` ; `python tools\rendu\verifier_bascule.py <dossier>` vérifie que chaque résultat est identique, pixel pour pixel, à l'écran construit directement avec le nouveau thème (la bascule vers `horloge` est refusée et doit laisser l'écran inchangé).

**Chronométrage.** Avec `BANC_CHRONO=1` le banc écrit, pour chaque rendu, le temps de construction de l'écran et du premier rendu complet, en microsecondes du PC (des chiffres relatifs entre écrans ou entre versions du dessin ; ils ne disent rien de la vitesse de la carte).

**Les icônes.** `python tools\icones\generer_icones.py` (demande Pillow) dessine les icônes météo, écrit `main/assets/icones_meteo.bin` (images A8, 825 032 octets), leur table pour le noyau pur (`visuel_icones_table.inc`) et celle de l'écran (`main/ui_meteo_icones_table.inc`) ; `--verifier` contrôle que les fichiers de l'arbre correspondent aux dessins. Le banc embarque le même binaire que `main/CMakeLists.txt`.

**Les réglages au-dessus de l'hôte de l'appareil.** `scene_hote_appareil.cpp` fait tourner les vrais écrans de réglages et d'installation au-dessus du vrai `HoteAppareil` (noyau pur), avec une mémoire et un appareil factices (`test/host/hote_appareil_faux.hpp`) : cinq parcours (affichage à source unique, une clé de service qui ne ressort jamais, un Wi-Fi retenu seulement après un succès, une installation qui reprend, une réinitialisation complète). `.\tools\rendre.ps1 -Scene hote_appareil` sort en code 1 quand une vérification échoue. **Les mêmes écrans sur la carte** sont exercés par `main/autotest_reglages.cpp`, compilé seulement avec `.\tools\idf.ps1 -DMERIDIEN_AUTOTEST_REGLAGES=ON build` (jamais dans le binaire normal) : un pointeur virtuel rejoue les gestes par la recherche d'objet de LVGL en trois phases séparées par de vrais redémarrages, et écrit une ligne `AUTOTEST OK|ECHEC` par vérification au journal série. Il n'utilise pas le panneau tactile (il ne prouve donc rien sur un doigt réel) et doit donner des coordonnées **brutes** de la dalle, parce que LVGL tourne le point qu'il lit ([depannage.md](depannage.md)). On le flashe sous le verrou de la carte, app seule ; remettre ensuite le binaire normal.

**Langue.** `-Langue` pose la langue courante du noyau pur (`ecran::i18n::definir_langue`) avant la construction de la scène : le code d'interface qui appelle `tr("texte français")` affiche le texte de la table (`i18n/*.yaml`, ADR-0006), avec les mêmes replis que sur la carte (la langue, puis l'anglais, puis la clé française). La scène la reçoit aussi dans `banc::Contexte`, avec la taille et l'affichage. `bonjour` n'existe qu'en français (il n'utilise pas encore `tr()`).

**Taille.** Toute `LxH`. Le banc déclare un affichage de cette taille ; `board_get_info()` répond Tab5 ST7121 ou, pour 1024×600, Waveshare ESP32-P4 7B, EK79007, GT911 (chaînes de [compatibilite.md](compatibilite.md), une fixture : rien de la 7B n'est exécuté).

**Planches.** `-Planche` réunit, pour chaque séquence, langue et taille, les écrans rendus dans un seul PNG : 3 colonnes de 560 px en paysage (4 de 440 px à partir de sept écrans, 4 de 330 px en portrait ; `-Colonnes` impose), la légende de chaque variante sous son écran, un titre avec la langue et la taille. La légende est affichée telle qu'écrite : y numéroter les étapes (« Étape 3 : Lieu »). Les planches demandent Pillow (déjà présent sur ce PC ; sinon `python -m pip install --user pillow`), rien d'autre n'en a besoin.

**Ce qui est identique au firmware.** LVGL 9.5.0 (vérifié contre `dependencies.lock`), compilé avec la configuration du firmware (`tools/rendu/sdkconfig_lvgl.h`, copie des 106 lignes `CONFIG_LV_*` du `sdkconfig` du firmware ; le script le compare au `sdkconfig` du dépôt s'il existe et prévient de tout écart) ; affichage RGB565 en tampons partiels de 50 lignes comme `bsp_tab5` ; thème par défaut de LVGL ; TinyTTF avec le même `main/assets/roboto_700.ttf`. Les fichiers d'interface (`main/ui_*.cpp`) et `main/horloge.cpp` sont compilés **sans modification** ; le texte de l'heure et de la date vient du vrai noyau pur.

**Ce qui est remplacé.**

| Firmware | Sur le PC | Où |
|---|---|---|
| `esp_log.h`, `freertos/*.h`, `esp_err.h`, `esp_rom_sys.h` | remplaçants minimaux | `tools/rendu/bouchons/` |
| les fonctions `board_*` de `board.h` (l'en-tête réel est utilisé) | réponses fixes : l'identité de la carte pour la taille demandée | `tools/rendu/banc.cpp` |
| `reseau_etat()` (Wi-Fi, SNTP), état météo | état choisi par la variante | `tools/rendu/scenes/scene_*.cpp` |
| `std::time()` | l'instant fixé (`_time64` est redéfini) | `tools/rendu/banc.cpp` |
| affichage, DSI, tactile | un tampon en mémoire, pas de tactile | `tools/rendu/banc.cpp` |

Les sources de LVGL sont cherchées dans `managed_components/lvgl__lvgl` du dépôt (après un `idf.py reconfigure`), puis dans le dépôt principal, puis dans son `.cache/composants/` ; `MERIDIEN_LVGL` impose un dossier. Cache de compilation : `.cache/rendu/` (ignoré par git). Un fichier `main/ui_*.cpp` ne doit inclure aucun en-tête ESP-IDF hors ceux listés ci-dessus (règle 3 du contrat d'interface entre sessions) : sinon on ajoute le remplaçant manquant au banc, on ne modifie jamais l'interface pour lui.

**Ce que ce rendu ne couvre PAS** (c'est le dessin logiciel de LVGL sur x86-64, pas la dalle) :

- **Les couleurs telles que la dalle les montre** : le banc élargit les 5-6-5 bits à 8 bits en répétant les bits de poids fort ; le gamma, le point blanc et le traitement interne de la vraie dalle ne sont pas simulés, ni comparés à une photo de la carte (non vérifié).
- **La déchirure, les temps du DSI, la copie DMA2D, le PPA, la cadence, la vitesse et la mémoire** : rien n'est mesuré ici (x86-64, pas le RISC-V).
- **Le tactile, le rétroéclairage, la luminosité.**
- **Les données inventées** : démarrage n° 42, 612 ms, états du Wi-Fi, températures et plages de l'écran principal : des fixtures, pas des mesures.
- **1280x720** : une taille simulée de l'affichage. La taille native de la carte est 720x1280 et l'orientation paysage se développe sur une autre branche ; l'écran « Bonjour » actuel a été dessiné pour 720x1280 et déborde en bas en 1280x720 (visible dans `bonjour-*-1280x720.png`).
- **Une comparaison pixel à pixel avec la carte** : non faite. Même fichier de police, même version et même configuration de LVGL : la même mise en page est attendue, pas prouvée.
- **Les traductions** : les planches montrent ce que `tr()` rend ; les textes allemands et turcs ont été écrits par une IA et ne sont pas relus (voir [`i18n/README.md`](../i18n/README.md)). Qu'un long texte allemand tienne dans un bouton de la vraie dalle, le rendu le montre, il ne le prouve pas sur la carte.

**Ce qu'a montré la scène témoin `temoin`** (`temoin-langues-*.png`, 1280x720, rendu PC) : `-Langue de` et `-Langue tr` arrivent bien dans `tr()` (« Helligkeit », « Yeniden başlat », « Parlaklık »), et la ligne de caractères spéciaux (français `àâäçéèêëîïôöùûüÿœæ`, allemand `äöüß`, néerlandais `ëï`, espagnol `áéíóúñ¿¡`, italien `àèéìòù`, turc `çğıöşü ÇĞİÖŞÜ`) est dessinée sans glyphe manquant par Roboto Bold via TinyTTF (vu à l'œil sur le rendu PC ; sur la carte : non essayé).

Mesuré le 06/11/2026 (`-Nettoyer` puis toutes les scènes, 7 PNG) : 11,1 s depuis zéro (463 fichiers de LVGL compilés à 4 en parallèle) ; 1,7 s au lancement suivant. Vu sur : le PC (Windows, clang 23.1.5 llvm-mingw, LVGL 9.5.0), aucune carte, commits `a1b67be`, `2345297` et `e00f791`. Écran principal, mesuré le 07/11/2026 (branche `claude/ecran-visuel-ba722a`, commit `049de21` et suivants) : les 126 PNG et 9 planches de la scène `principal` en 17,8 s avec la bibliothèque LVGL en cache (4 jobs) ; 12 PNG de `principal-themes` (une taille) en 12 s au premier lancement, qui a recompilé LVGL. Les neuf planches pèsent environ 1,1 à 1,4 Mo chacune à cause du ciel tramé : seules cinq planches et quatre écrans sont gardés dans `docs/captures/ecran/` ; le reste se refait en quelques secondes.

### Ce qui n'est pas installé (volontairement)

- **Simulateur LVGL interactif sur PC** (SDL2, avec tactile) et comparaison automatique à des captures de référence dans la CI : phase 5 du [plan](plan.md). Le rendu PNG ci-dessus en est la première étape.
- **cppcheck** : `clang-tidy` couvre le besoin pour l'instant.
- Aucun outil ne remplace l'essai sur la carte : un test qui passe sur le PC (x86-64) ne dit rien de la mémoire, de l'alignement ni du temps sur le RISC-V.
