# Starting from Meridien for your own screen

## English · [Français](#version-française)

---

For someone who wants a touch screen built on an ESP32 with LVGL 9 in **native ESP-IDF**, on a board other than the two of this repository. This page says what can be kept, what has to be written, and what is only true on the ESP32-P4. It is a **reading of the code and of the documents** as of 19/11/2026 (`main` at `3f33a27`): **no third board has been tried**, and nothing below promises that the code runs on one.

The code is under the MIT licence ([LICENSE](../LICENSE)); the components it uses have their own licences ([THIRD_PARTY.md](../THIRD_PARTY.md)).

### What this repository is

Native ESP-IDF 5.5.5 + LVGL 9.5 firmware for the ESP32-P4: time and date, weather, official alerts, a settings page served by the device. Seen working on two boards, both ESP32-P4: the M5Stack Tab5 (ST7121 display revision) and the Waveshare 7B (since 07/11/2026). What is observed, board by board and function by function: [compatibility](compatibilite.md). Arduino and PlatformIO were set aside on purpose ([ADR-0001](decisions/0001-pile-esp-idf-natif.md)): if your project must stay on Arduino, this repository is not the right base.

### What you can keep as it is

| Part | Where | Why it travels | Limit |
|---|---|---|---|
| Pure core: weather readers (Open-Meteo, MET Norway, NWS), time zones, time requests, alerts, settings API, translations, screen geometry | `components/core/` | standard C++ with **no ESP-IDF include** (rule 1 of [AGENTS.md](../AGENTS.md)); tested on the PC with AddressSanitizer + UBSan and mutation testing (`python test/host/lancer.py`) | built only for the PC and for the ESP32-P4 so far |
| Board interface | `components/board_api/include/board.h` (pure C) | display (`lv_display_t`), lock, backlight, sleep, hardware clock, audio, power, Wi-Fi link: the application only knows these functions | the 7B was added **without changing it**; a third board is the next test of that claim |
| Application | `main/` | the screens are drawn from the real display size, not absolute coordinates ([ADR-0005](decisions/0005-couche-carte.md), point 3) | seen on panels at 1280 × 720 (Tab5) and 1024 × 600 (7B), rendered on the PC at those sizes and 720 × 1280; other sizes not tried. Many buffers are asked **in PSRAM** (`MALLOC_CAP_SPIRAM`): a board without PSRAM would need changes, how many is not verified |
| Settings page and its contract | `web/`, [api-reglages.md](api-reglages.md) | plain HTML/CSS/JS served by the device, tested against a fake device on the PC | — |
| Tools and checks | `tools/`, `test/host/`, `.github/workflows/ci.yml` | screen rendering on the PC at any width × height (`tools/rendre.ps1`), pre-commit checks, CI | written for Windows + PowerShell; the CI also builds with Espressif's own ESP-IDF 5.5.5 image under Linux |

### What you change: one folder per board

The rule is [ADR-0005](decisions/0005-couche-carte.md): one board = one folder, and no `if (my_board)` in the application. The 7B was added this way; its files are the example to copy.

1. **`boards/<board>/sdkconfig.defaults.board`**: chip revision, CPU frequency, flash size and mode, partition table, PSRAM mode and speed, console (USB-Serial/JTAG or UART), Wi-Fi transport. Example: [`boards/waveshare-7b/sdkconfig.defaults.board`](../boards/waveshare-7b/sdkconfig.defaults.board), every line commented with its reason.
2. **`boards/<board>/partitions.csv`**, and `boards/<board>/dependencies.lock` for any board other than the Tab5.
3. **`components/bsp_<board_id>/`** (the `-` of the board name becomes `_`): the code behind `board.h` — pins, display driver (an `esp_lcd_<controller>` component and its init table), touch, backlight, audio codec, Wi-Fi link. Keep the pure part (pins, timings, init table, arithmetic) in a header that also compiles on the PC, as [`ws7b_pur.h`](../components/bsp_waveshare_7b/include/ws7b_pur.h) does: 34 tests and 33 mutations run it without the board.
4. **Display size**: it comes from your display driver; the application reads it from LVGL.
5. **Build**: `.\tools\idf.ps1 -DBOARD=<board> set-target esp32p4`, then `build`. Changing board needs a fresh `sdkconfig` ([troubleshooting](depannage.md#tools-and-build), “Switching boards”). The board is chosen by the CMake variable `BOARD` in [`CMakeLists.txt`](../CMakeLists.txt), which refuses a board without its folder and its `bsp_` component.
6. **CI**: add a job like `firmware-waveshare-7b` ([outils.md](outils.md#continuous-integration-github-actions)).

The common [`sdkconfig.defaults`](../sdkconfig.defaults) sets `CONFIG_IDF_TARGET="esp32p4"` and the scripts call `set-target esp32p4`: another chip changes those too.

### What is specific to the ESP32-P4

These parts exist because both boards are ESP32-P4. On another chip they are replaced, not reused.

- **Display over MIPI-DSI.** The panel is driven by the P4's DSI host (DPI mode), the frame buffer lives in PSRAM, and the PSRAM runs in hex mode at 200 MHz (without it, about 30 “dsi underrun” per second on the 7B: [carte-7b.md](carte-7b.md)). The Tab5 has three panel revisions and detects the controller before starting the DSI ([carte-tab5.md](carte-tab5.md)). Rotation goes through the P4's PPA accelerator, the copy to the panel through DMA2D.
- **No radio in the P4.** Wi-Fi goes through an ESP32-C6 on SDIO, with `esp_hosted` / `esp_wifi_remote` pinned to the 2.12.x line; the C6 firmware is never updated (author's decision). Its transport shares the internal RAM: see [troubleshooting](depannage.md#memory-and-watchdog). Research: [recherche/05-wifi-c6-esp-hosted.md](recherche/05-wifi-c6-esp-hosted.md).
- **Memory.** Small allocations and mbedTLS go to PSRAM (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`, `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`, both boards). On the 7B the code also runs from PSRAM with a 256 KB L2 cache; these settings were set against a brief white flash at rest and do not fix it (seen again on 25/11/2026; cause not proven) ([ADR-0028](decisions/0028-7b-code-en-psram-et-cache-l2-256-ko.md)).
- **Chip revision.** The Tab5's P4 is a revision 1.3, which ESP-IDF 5.5.5 refuses without `CONFIG_ESP32P4_SELECTS_REV_LESS_V3`; the 7B's is a revision 3.2.
- **Restart.** On the P4 `esp_restart()` only resets the cores: every software restart ends with `esp_rom_software_reset_system()`.

### ESP32-S3 and other chips: not tested

- **Never built or run on anything but an ESP32-P4.** There is no board layer for an ESP32-S3 or any other chip, and none is planned here.
- **Display.** The ESP32-S3 has **no MIPI-DSI**. In ESP-IDF v5.5.5, the chip capability file of the P4 declares `SOC_MIPI_DSI_SUPPORTED` ([esp32p4 `soc_caps.h`](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32p4/include/soc/soc_caps.h)) and the S3's does not; the S3's declares the Intel 8080 (`SOC_LCD_I80_SUPPORTED`) and RGB (`SOC_LCD_RGB_SUPPORTED`) parallel LCD interfaces ([esp32s3 `soc_caps.h`](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32s3/include/soc/soc_caps.h); ESP-IDF guide for the S3: [I80 LCD](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/i80_lcd.html), [RGB LCD](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html), and SPI panels in the [LCD overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/index.html)). Read on 19/11/2026. So on an S3 the display part of the board layer is a different driver (RGB, I80 or SPI panel), and the P4's PPA and DMA2D, used here for rotation and the copy to the panel, do not exist there (`SOC_PPA_SUPPORTED`, `SOC_DMA2D_SUPPORTED`: P4 only in the same files). Whether LVGL's drawing of this application is fast enough on an S3 is **not verified**.
- **Wi-Fi.** The S3 has its own Wi-Fi (`SOC_WIFI_SUPPORTED` in its `soc_caps.h`, absent from the P4's). The ESP32-C6 link and `esp_hosted` would go away and `board_wifi_link_up()` would become the S3's own Wi-Fi start: **not written**.
- **Memory.** How much of the application fits in another chip's internal RAM and PSRAM is **not measured**. The two P4 boards needed the memory settings above, and several incidents of the troubleshooting page come from internal RAM running out.
- **Pure core.** It should build anywhere a C++ compiler builds standard C++, but that is checked only on the PC and for the P4.

### Tools, troubleshooting and method

- **Tools**: what is installed, where, and what each check does: [outils.md](outils.md). The build runs from PowerShell, never Git Bash.
- **Troubleshooting**: [depannage.md](depannage.md), symptom → cause → solution. Each of its 84 entries says who it concerns: *any ESP32 / ESP-IDF project* (34), *the ESP32-P4 only* (5), *this board only* (14) or *this project only* (31) (count of 19/11/2026). Start with the first group.
- **Decisions**: [decisions/](decisions/README.md), one ADR per non-obvious choice, with what was rejected and why. Read them before “fixing” something that looks wrong.
- **Measurements**: [mesures/](mesures/), one page per milestone. A result counts only with its “Seen on” line (board, display revision, commit, date). The reading rules — “observed”, “written, not tested on a board”, “not verified” — are in [docs/README.md](README.md#reading-rules).
- **Coding with an AI agent**: [AGENTS.md](../AGENTS.md) is the short brief any agent reads first (Claude Code reads it through `CLAUDE.md`): the rules of the code, the hardware limits, what “done” means. This repository was written that way; copy the file and adapt it to your board.

---

## Version française

---

Pour qui veut un écran tactile sur un ESP32 avec LVGL 9 en **ESP-IDF natif**, sur une autre carte que les deux de ce dépôt. Cette page dit ce qu'on peut garder, ce qu'il faut écrire, et ce qui n'est vrai que sur l'ESP32-P4. C'est une **lecture du code et des documents** au 19/11/2026 (`main` à `3f33a27`) : **aucune troisième carte n'a été essayée**, et rien ci-dessous ne promet que le code y tourne.

Le code est sous licence MIT ([LICENSE](../LICENSE)) ; les composants qu'il utilise ont leurs propres licences ([THIRD_PARTY.md](../THIRD_PARTY.md)).

### Ce qu'est ce dépôt

Firmware natif ESP-IDF 5.5.5 + LVGL 9.5 pour ESP32-P4 : heure et date, météo, alertes officielles, page de réglages servie par l'appareil. Vu fonctionner sur deux cartes, toutes deux ESP32-P4 : le M5Stack Tab5 (révision d'écran ST7121) et la Waveshare 7B (depuis le 07/11/2026). Ce qui est constaté, carte par carte et fonction par fonction : [compatibilité](compatibilite.md#version-française). Arduino et PlatformIO ont été écartés exprès ([ADR-0001](decisions/0001-pile-esp-idf-natif.md)) : si votre projet doit rester sous Arduino, ce dépôt n'est pas la bonne base.

### Ce qu'on garde tel quel

| Partie | Où | Pourquoi elle se transporte | Limite |
|---|---|---|---|
| Noyau pur : lecteurs météo (Open-Meteo, MET Norway, NWS), fuseaux horaires, demandes d'heure, alertes, API de réglages, traductions, géométrie de l'écran | `components/core/` | C++ standard **sans include ESP-IDF** (règle 1 d'[AGENTS.md](../AGENTS.md)) ; testé sur le PC avec AddressSanitizer + UBSan et des mutations (`python test/host/lancer.py`) | compilé jusqu'ici seulement pour le PC et pour l'ESP32-P4 |
| Interface carte | `components/board_api/include/board.h` (C pur) | écran (`lv_display_t`), verrou, rétroéclairage, veille, horloge matérielle, audio, alimentation, lien Wi-Fi : l'application ne connaît que ces fonctions | la 7B a été ajoutée **sans la modifier** ; une troisième carte serait la prochaine épreuve |
| Application | `main/` | les écrans sont dessinés d'après la taille réelle de l'écran, pas en coordonnées absolues ([ADR-0005](decisions/0005-couche-carte.md), point 3) | vu sur des dalles de 1280 × 720 (Tab5) et 1024 × 600 (7B), rendu sur le PC à ces tailles et en 720 × 1280 ; autres tailles non essayées. Beaucoup de tampons sont demandés **en PSRAM** (`MALLOC_CAP_SPIRAM`) : une carte sans PSRAM demanderait des changements, combien n'est pas vérifié |
| Page de réglages et son contrat | `web/`, [api-reglages.md](api-reglages.md) | HTML/CSS/JS simples servis par l'appareil, testés contre un faux appareil sur le PC | — |
| Outils et contrôles | `tools/`, `test/host/`, `.github/workflows/ci.yml` | rendu de l'écran sur le PC à toute largeur × hauteur (`tools/rendre.ps1`), contrôles avant commit, CI | écrits pour Windows + PowerShell ; la CI compile aussi avec l'image ESP-IDF 5.5.5 d'Espressif sous Linux |

### Ce qu'on change : un dossier par carte

La règle est l'[ADR-0005](decisions/0005-couche-carte.md) : une carte = un dossier, et aucun `if (ma_carte)` dans l'application. La 7B a été ajoutée ainsi ; ses fichiers sont l'exemple à copier.

1. **`boards/<carte>/sdkconfig.defaults.board`** : révision de puce, fréquence du processeur, taille et mode de la flash, table de partitions, mode et vitesse de la PSRAM, console (USB-Serial/JTAG ou UART), transport du Wi-Fi. Exemple : [`boards/waveshare-7b/sdkconfig.defaults.board`](../boards/waveshare-7b/sdkconfig.defaults.board), chaque ligne commentée avec sa raison.
2. **`boards/<carte>/partitions.csv`**, et `boards/<carte>/dependencies.lock` pour toute carte autre que le Tab5.
3. **`components/bsp_<carte_id>/`** (le `-` du nom de carte devient `_`) : le code derrière `board.h` — broches, pilote d'écran (un composant `esp_lcd_<contrôleur>` et sa table d'initialisation), tactile, rétroéclairage, codec audio, lien Wi-Fi. Garder la partie pure (broches, minutages, table d'initialisation, calculs) dans un en-tête qui compile aussi sur le PC, comme [`ws7b_pur.h`](../components/bsp_waveshare_7b/include/ws7b_pur.h) : 34 tests et 33 mutations l'exécutent sans la carte.
4. **Taille d'écran** : elle vient de votre pilote d'écran ; l'application la lit dans LVGL.
5. **Compiler** : `.\tools\idf.ps1 -DBOARD=<carte> set-target esp32p4`, puis `build`. Changer de carte demande un `sdkconfig` neuf ([dépannage](depannage.md#outils-et-compilation), « Changer de carte »). La carte est choisie par la variable CMake `BOARD` dans [`CMakeLists.txt`](../CMakeLists.txt), qui refuse une carte sans son dossier et son composant `bsp_`.
6. **CI** : ajouter un job comme `firmware-waveshare-7b` ([outils.md](outils.md#intégration-continue-github-actions)).

Le [`sdkconfig.defaults`](../sdkconfig.defaults) commun pose `CONFIG_IDF_TARGET="esp32p4"` et les scripts appellent `set-target esp32p4` : une autre puce change aussi cela.

### Ce qui est propre à l'ESP32-P4

Ces parties existent parce que les deux cartes sont des ESP32-P4. Sur une autre puce, on les remplace, on ne les réutilise pas.

- **Écran en MIPI-DSI.** La dalle est pilotée par l'hôte DSI du P4 (mode DPI), le tampon d'image est en PSRAM, et la PSRAM tourne en mode hexadécimal à 200 MHz (sans cela, environ 30 « dsi underrun » par seconde sur la 7B : [carte-7b.md](carte-7b.md#version-française)). Le Tab5 a trois révisions de dalle et détecte le contrôleur avant de démarrer le DSI ([carte-tab5.md](carte-tab5.md)). La rotation passe par l'accélérateur PPA du P4, la copie vers la dalle par le DMA2D.
- **Pas de radio dans le P4.** Le Wi-Fi passe par un ESP32-C6 en SDIO, avec `esp_hosted` / `esp_wifi_remote` épinglés sur la ligne 2.12.x ; le firmware du C6 n'est jamais mis à jour (décision de l'auteur). Son transport partage la RAM interne : voir le [dépannage](depannage.md#mémoire-et-chien-de-garde). Recherche : [recherche/05-wifi-c6-esp-hosted.md](recherche/05-wifi-c6-esp-hosted.md).
- **Mémoire.** Les petites allocations et mbedTLS vont en PSRAM (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`, `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`, les deux cartes). Sur la 7B, le code s'exécute aussi depuis la PSRAM avec un cache L2 de 256 Ko ; ces réglages, posés contre un bref flash blanc au repos, ne le corrigent pas (revu le 25/11/2026 ; cause non prouvée) ([ADR-0028](decisions/0028-7b-code-en-psram-et-cache-l2-256-ko.md)).
- **Révision de puce.** Le P4 du Tab5 est une révision 1.3, qu'ESP-IDF 5.5.5 refuse sans `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` ; celui de la 7B est une révision 3.2.
- **Redémarrage.** Sur le P4, `esp_restart()` ne remet à zéro que les cœurs : chaque redémarrage logiciel se termine par `esp_rom_software_reset_system()`.

### ESP32-S3 et autres puces : non testé

- **Jamais compilé ni exécuté sur autre chose qu'un ESP32-P4.** Il n'existe de couche carte ni pour un ESP32-S3 ni pour une autre puce, et aucune n'est prévue ici.
- **Écran.** L'ESP32-S3 n'a **pas de MIPI-DSI**. Dans ESP-IDF v5.5.5, le fichier des capacités de la puce du P4 déclare `SOC_MIPI_DSI_SUPPORTED` ([`soc_caps.h` de l'esp32p4](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32p4/include/soc/soc_caps.h)) et celui du S3 non ; celui du S3 déclare les interfaces d'écran parallèles Intel 8080 (`SOC_LCD_I80_SUPPORTED`) et RGB (`SOC_LCD_RGB_SUPPORTED`) ([`soc_caps.h` de l'esp32s3](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32s3/include/soc/soc_caps.h) ; guide ESP-IDF du S3 : [I80 LCD](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/i80_lcd.html), [RGB LCD](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html), et les dalles SPI dans la [vue d'ensemble LCD](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd/index.html)). Lu le 19/11/2026. Sur un S3, la partie écran de la couche carte est donc un autre pilote (dalle RGB, I80 ou SPI), et le PPA et le DMA2D du P4, utilisés ici pour la rotation et la copie vers la dalle, n'y existent pas (`SOC_PPA_SUPPORTED`, `SOC_DMA2D_SUPPORTED` : P4 seulement dans les mêmes fichiers). Que le dessin LVGL de cette application soit assez rapide sur un S3 **n'est pas vérifié**.
- **Wi-Fi.** Le S3 a son propre Wi-Fi (`SOC_WIFI_SUPPORTED` dans son `soc_caps.h`, absent de celui du P4). Le lien vers l'ESP32-C6 et `esp_hosted` disparaîtraient, et `board_wifi_link_up()` deviendrait le démarrage du Wi-Fi du S3 lui-même : **non écrit**.
- **Mémoire.** Ce qui tient de l'application dans la RAM interne et la PSRAM d'une autre puce **n'est pas mesuré**. Les deux cartes P4 ont eu besoin des réglages de mémoire ci-dessus, et plusieurs incidents du dépannage viennent d'une RAM interne épuisée.
- **Noyau pur.** Il devrait compiler partout où un compilateur C++ compile du C++ standard, mais ce n'est vérifié que sur le PC et pour le P4.

### Outils, dépannage et méthode

- **Outils** : ce qui est installé, où, et ce que fait chaque contrôle : [outils.md](outils.md). La compilation se lance depuis PowerShell, jamais Git Bash.
- **Dépannage** : [depannage.md](depannage.md#version-française), symptôme → cause → solution. Chacune de ses 84 entrées dit qui elle concerne : *tout projet ESP32 / ESP-IDF* (34), *l'ESP32-P4 seulement* (5), *cette carte seulement* (14) ou *ce projet seulement* (31) (compte du 19/11/2026). Commencer par le premier groupe.
- **Décisions** : [decisions/](decisions/README.md), une ADR par choix non évident, avec ce qui a été écarté et pourquoi. À lire avant de « corriger » ce qui semble faux.
- **Mesures** : [mesures/](mesures/), une page par jalon. Un résultat ne compte qu'avec sa ligne « Vu sur » (carte, révision d'écran, commit, date). Les règles de lecture — « constaté », « écrit, non testé sur carte », « non vérifié » — sont dans [docs/README.md](README.md#règles-de-lecture).
- **Coder avec un agent IA** : [AGENTS.md](../AGENTS.md) est la consigne courte que tout agent lit d'abord (Claude Code la lit par `CLAUDE.md`) : règles du code, limites du matériel, ce que veut dire « fait ». Ce dépôt a été écrit ainsi ; copiez le fichier et adaptez-le à votre carte.
