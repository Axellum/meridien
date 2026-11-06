# Troubleshooting — incidents already diagnosed

## English · [Français](#version-française)

---

Kept up to date over the sessions. **Read it before re-diagnosing a symptom that looks like one of these**; several cost hours when searched for again.

Format of each entry: **Symptom → Cause → Solution**, then three lines:
- **Seen on**: board, display revision, commit, date (see [compatibility](compatibilite.md#firmware-and-board-version-how-it-is-noted)). No MAC address is published.
- **Evidence**: how it was observed (log, screen, measurement, re-reading).
- **Status**: *resolved*, *worked around*, *open* or *known elsewhere, not yet seen here*.

An entry “known elsewhere” comes from the ESPHome firmware of the Tab5 ([its troubleshooting](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/docs/troubleshooting.md)): the hardware cause is the same, but it has not yet been looked at again with this native firmware.

### Contents

- [Open](#open)
- [Tools and build](#tools-and-build)
- [Chip, display and touch](#chip-display-and-touch)
- [Flash, serial port and backup](#flash-serial-port-and-backup)
- [Settings page and core](#settings-page-and-core)

---

### Open

#### After a power cut, the image only appears after a second gesture

**Symptom.** Power cut on the Tab5, then power back: the first start-up shows no image; the “Bonjour” image and the test pattern appear after a second gesture by the author (clean start-up no. 9, ST7121 detected). The first start-up “without a screen” **is not in the serial log**: the start counter goes from 8 to 9.

**Cause.** **Not found** as of 06/11/2026. The log holds only one start-up after the cut (no. 9), complete and clean: ST7121 detected at the 1st probe, first image, backlight at 75% at 1598 ms. So either the first start-up did not run this firmware, or it was not captured (tablet on battery, without serial port). Hypotheses, **not verified**: the Tab5's USB power perhaps does not turn the tablet on without a press on the button; the display reset through the expander (which needs one second, by analogy with ESPHome) does not seem to be the cause, since no. 9 is clean. The reset cause read at power-up is “watchdog” (`HP_SYS_HP_WDT_RESET`) at **both** observed power-ups: perhaps the normal cause of a power-up of this board, not verified; no crash (`Guru`, `abort`, `Backtrace`) seen.

**Solution.** None applied. For the user: after a complete cut, a short press on the button turns the test pattern on. The criterion of 1a (“image after a restart **and** after a power cut”) is therefore observed **with a reservation**; the step-by-step trial (plug the USB back in without touching any button, wait 30 s, note what is displayed, serial port open) is to be redone at milestone 1b before attributing the cause to the firmware or to the board. The author chose not to redo it during 1a.

**Seen on** Tab5 ST7121, native firmware commit `e7a4803`, 06/11/2026. **Evidence**: log of start-up no. 9, the author's observation; details in [mesures/1a.md](mesures/1a.md). **Status**: open. Milestone 1b (06/11/2026): the step-by-step trial was **postponed by the author** (“Not now”, 20:59), neither done nor failed; see [mesures/1b.md](mesures/1b.md).

#### Return to ESPHome never tried

The restore recipe (three writes: start of the flash, application, NVS) is in the `LISEZMOI.md` file of the author's local backup, which is outside this repository. It has **never been played**: as long as it has not been, the return is “not verified”. **Status**: open.

---

### Tools and build

#### ESP-IDF's `export.ps1` fails, then “python_env doesn't exist”

**Symptom.** The `export.ps1` of the ESP-IDF installed by ESPHome stops (it looks for `python_env\idf5.5_py3.13_env`); then `idf.py set-target` answers “python_env doesn't exist”.

**Cause.** That ESP-IDF is installed and owned by ESPHome, with a different organisation of the Python environment (`penvs\5.5.5`) from the one Espressif's scripts expect.

**Solution.** `tools/idf.ps1` does not use `export.ps1`: it calls `tools\idf.py` with the Python of `penvs\5.5.5` of that ESP-IDF, sets the `PATH` of the tools (cmake, ninja, ccache, RISC-V GCC) and the variable `IDF_PYTHON_ENV_PATH`. Change nothing in the folder of that ESP-IDF (it is ESPHome's).

**Seen on** the author's PC (Windows 11), ESP-IDF 5.5.5, 06/11/2026. **Evidence**: skeleton built in 38 s, 204,048 bytes (not flashed). **Status**: resolved.

#### Warnings and dead paths without effect

- “Error while generating esp_rom gdbinit” at the start of a build: harmless, the build succeeds. *Status: observed on the skeleton.*
- The `idf-env.json` of the ESPHome-owned ESP-IDF still cites a path on another drive (dead path, ESPHome's file not corrected): no effect on our builds so far. *Status: not verified beyond that.*

#### The PC crashes during a build

**Cause.** Too many parallel tasks (the PC has already crashed at 6) or two builds at once.

**Solution.** `CMAKE_BUILD_PARALLEL_LEVEL=4` (set by `tools/idf.ps1`), **only one ESP-IDF build at a time**, from PowerShell (never Git Bash). After a crash: `git fsck`. The sessions that do not need to build (web page, core, research) do not build.

**Seen on** the author's PC: crashed at 6 parallel tasks (the author's note, ESPHome builds), never yet with this project. **Status**: worked around by the limit of 4.

#### A change in `sdkconfig.defaults*` has no effect

**Cause.** The defaults are only read again when `sdkconfig` does not exist.

**Solution.** Delete `sdkconfig`, run `.\tools\idf.ps1 set-target esp32p4` again, then check the value in the generated `sdkconfig` (not in the defaults file). **Seen on** Tab5 ST7121, 06/11/2026. **Status**: resolved.

#### A PowerShell script is refused: a deletion aimed at the ESP-IDF folder

The safeguard of the author's AI session tool refuses any command containing a `Remove-Item` aimed at the ESP-IDF folder owned by ESPHome, even inside a script written elsewhere. Do not delete in scripts; move or ignore. **Status**: worked around.

#### `idf.py` stops on “macro expanded to blank string” as soon as `esp_hosted` is a dependency

**Symptom.** After adding `espressif/esp_hosted` 2.12.12, the configuration (`kconfgen`) stops on the Kconfig of that component with “macro expanded to blank string”.

**Cause.** That Kconfig reads `$(ESP_IDF_VERSION)` (e.g. “5.5”), a variable that ESP-IDF does not set (it sets `IDF_VERSION`).

**Solution.** `tools/idf.ps1` sets `ESP_IDF_VERSION` to “major.minor” (here 5.5) before calling `idf.py`, as ESPHome does for its own builds.

**Seen on** the author's PC, ESP-IDF 5.5.5, `esp_hosted` 2.12.12, 06/11/2026, commit `f86b066`. **Evidence**: configuration and full build pass afterwards (binary of 1,240,272 bytes). **Status**: resolved.

#### `ninja`: “reglages_locaux.cmake is defined as an output multiple times”

**Cause.** A `set_property(DIRECTORY … CMAKE_CONFIGURE_DEPENDS …)` on a file that CMake already tracks because `main/CMakeLists.txt` reads it with `include()`.

**Solution.** Remove the `set_property`: CMake re-runs the configuration by itself when an included file changes. **Seen on** the author's PC, 06/11/2026. **Status**: resolved.

#### A new script is missing from `git status`: the secrets safeguard ignored it

**Cause.** The pattern `reglages_locaux*` of `.gitignore` (meant for the generated `reglages_locaux.cmake`, which holds the Wi-Fi credentials) also matched `tools/reglages_locaux.ps1`. Nothing warns: the file just never appears (`git check-ignore -v <file>` shows the pattern).

**Solution.** The script was renamed `tools/preparer_wifi.ps1`; the pattern is kept as it is (it is the safeguard). **Seen on** the author's PC, 06/11/2026, commit `761d135`. **Status**: resolved.

#### The translation guard does not see the texts added in 1b

`test/host/controle_i18n.py` reads only the `kXxx = "…";` constants of `main/ui_bonjour.cpp` and `main/boot_info.cpp`, checked against the “Bonjour screen” section of the translation table. The new texts of 1b (clock, network state) therefore live in `main/textes_1b.h`, **not covered** by the table and not through `tr()`; they will go through `tr()` with the settings page. **Status**: worked around, to be redone in phase 2.

#### `idf.py set-target` fails while downloading components: “No such file or directory” on a very long path

**Symptom.** In a new git worktree, `.\tools\idf.ps1 set-target esp32p4` stops during the component download with a Python `FileNotFoundError: [Errno 2] No such file or directory: '…\.cache\composants\service_…\espressif__esp_hosted_2.12.12_…\examples\host_network_split__power_save\…\host_wakeup_demo_using_udp_packet.c'`. A half-written `build/` is then left behind, and the next `set-target` stops on “doesn't seem to be a CMake build directory”.

**Cause.** The 260-character Windows path limit (`LongPathsEnabled` is 0 on this PC) meets the component cache that `tools/idf.ps1` puts inside the project (`<worktree>\.cache\composants`). The longest file of `esp_hosted` 2.12.12 lies 184 characters below the worktree root; with the folder `reglages-appareil-b3aad6` (root of 76 characters) its path is exactly 260 characters. The other worktrees, whose names are 2 to 5 characters shorter, come out at 251–258 and pass: **a worktree folder name of 24 characters or more fails** here.

**Solution.** Worked around, not fixed in the repository: delete the half-written `build/` and `sdkconfig*`, then run the build with a private copy of `tools/idf.ps1` (not committed) in which `IDF_COMPONENT_CACHE_PATH` points to a short folder outside the repository (here `C:\Users\axell\.tab5\cache-composants`); everything else is identical. Durable options **not done**: have `tools/idf.ps1` keep an `IDF_COMPONENT_CACHE_PATH` that is already set (one line), or give worktrees shorter names. Turning on Windows long paths is a system setting and was not touched.

**Seen on** the build PC (Windows 11, ESP-IDF 5.5.5), branch `claude/reglages-appareil-b3aad6`, 06/11/2026; no board involved. **Evidence**: Python traceback in the build log; path length computed (260) and the same computation for the other worktrees; with the short cache the configuration, the download of the components (including `espressif/mdns` 1.13.1) and the build (exit code 0) went through. **Status**: worked around.

---

### Chip, display and touch

#### ESP-IDF refuses to start on the Tab5's chip

**Cause.** The Tab5's chip is an ESP32-P4 **revision 1.3**; ESP-IDF 5.5.5 requires by default a revision ≥ 3.1.

**Solution.** In `boards/tab5/sdkconfig.defaults.board`: `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y` (checked in the generated `sdkconfig`: `CONFIG_ESP32P4_REV_MIN_FULL=100`). Chip settings belong to each board: the 7B (revision 3.x) has its own.

**Seen on** Tab5 ST7121, 06/11/2026 (reported by the hardware track). **Status**: resolved.

#### Which DSI line rate for the ST7121? Four sources contradict each other

| Source | Value |
|---|---|
| esp-bsp (`m5stack_tab5`) | 965 Mbit/s |
| ESPHome, model `M5STACK-TAB5-ST7121` (the one that runs this tablet) | 965 Mbit/s |
| espp | 900 Mbit/s |
| macro of the `esp_lcd_st7121` component | 1300 Mbit/s |

**Solution.** 965: it is what already makes the tablet work under ESPHome. 900 then 1300 only if the screen stays black. DPI margins that agree across four sources: clock 70 MHz, horizontal 40/2/40, vertical 24/20/200, 720 × 1280 in RGB565. **Seen on** Tab5 ST7121. **Status**: resolved — 965 Mbit/s is the final value, the image is displayed with it (observed on 06/11/2026, commit `e7a4803`); the trials at 900 and 1300 were not made, they were unnecessary.

#### The `esp_lcd_st7123` 1.0.2 component does not suit the Tab5 as it is

Its macros describe a 720 × 1560 panel, not the Tab5's. The init table of the Tab5 ST7123 is in esp-bsp (`disp_init_data_1.h`), to be ported when the unit is back from repair. The ST7123 and the ILI9881C are **detected** (log + `ESP_ERR_NOT_SUPPORTED`), not initialised. **Status**: known, not addressed.

#### Accents appear as squares or are missing

**Cause.** LVGL's built-in fonts stop at ASCII.

**Solution.** Embedded TinyTTF font (Roboto Bold, OFL licence, `main/assets/`). Every icon or glyph displayed must exist in the loaded font: an MDI icon is never an empty rectangle. **Status**: resolved (accents); icons: to be checked when they arrive.

#### The touch point is shifted to the left

**Symptom.** First trial on the Tab5: the point follows the finger but shifted to the left; a swipe could scroll the screen.

**Cause.** The point was placed on the screen, whose top padding (90 px) shifts floating objects: LVGL places them relative to the content area of the parent.

**Solution.** Point on the top layer (no padding), non-scrollable screen, and a test pattern (“mire”: a frame on the edges + five targets at the corners and the centre) to measure the remaining offset between touch and image from the presses in the serial log.

**Seen on** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Evidence**: the author's observation (the point is under the finger) and five presses on known targets: mean error (−6, −4) px, from −12 to +10 px on four targets and −26 px in x on the fifth (top right), with no calibration applied (details in [mesures/1a.md](mesures/1a.md)). **Status**: resolved; accuracy of the order of a finger.

#### No log over USB, or a build error on a `|` between two LVGL flags

- **No log**: it needs `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (the default UART console is not wired to the Tab5's USB). Set in `boards/tab5/sdkconfig.defaults.board`.
- **`A | B` on two LVGL flags**: in C++ the result is an `int` that LVGL refuses; one call per flag. Likewise for Espressif's configuration macros (designated initialisers in an order that C++ refuses): the board layer (`components/bsp_tab5/`) is therefore written in **C**, the application in C++.

**Seen on** Tab5 ST7121, 06/11/2026. **Status**: resolved.

#### Log `swap_xy is not supported by this panel` at start-up

Emitted by `esp_lvgl_port` at initialisation; no observed effect on the image (fixed native portrait orientation, no software rotation). Cause not explored. **Seen on** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Status**: known, no observed effect.

#### Low bootloader margin and PSRAM test

The bootloader takes 0x5a30 bytes of its 24 KiB: only **6% margin**, to watch if options are added. Start-up takes about 2.9 s from the reset to the first image, of which ~0.95 s of PSRAM test (`CONFIG_SPIRAM_MEMTEST=y`) and 1 s of waiting for the expander: two possible gains, **to be measured before sacrificing anything**, one change at a time. **Seen on** Tab5 ST7121, 06/11/2026. **Status**: known, not addressed.

#### Black screen after a software restart

*Known elsewhere; here, only one software restart tried and it succeeded (no. 2 of 06/11/2026, `SW_SYS_RESET`, image came back, Tab5 ST7121), no series of 20.* On the ESPHome firmware, the display's reset pin goes through the PI4IOE5V6408 expander, which needs about a second to be reliable: reset too early = black screen. The native firmware takes over the 1 s wait before the reset (specific to the board: the 7B has none, its reset is a P4 pin). On the P4, `esp_restart()` only resets the cores: finish with `esp_rom_software_reset_system()`. To be verified at milestone 1b with 20 restarts.

---

### Flash, serial port and backup

#### `read-flash` fails: “Packet content transfer stopped”

**Cause.** Reading the whole flash with esptool's stub on a zone that does not answer; here also, an offset error (the NVS is not at the start of the flash).

**Solution.** Read the useful partitions at the offsets read from the **board's partition table** (here ESPHome's NVS is at `0xf90000`), twice, and compare the SHA-256; fall back to `--no-stub` if the stub fails; `verify-flash` (MD5 sum computed by the chip) to prove that a stored file is identical to the flash. **Seen on** Tab5 ST7121, esptool 5.3.1, 06/11/2026. **Status**: resolved (partial backup made, wanted by the author: the code is in git, the NVS is not).

#### The COM port is not the one you think

**Cause.** The number changes with the USB socket; on 06/11/2026 the Tab5 was on COM11, and COM14 was an **ESP32-S3 from another project**.

**Solution.** Always find the port by the **MAC** (`serial.tools.list_ports`, USB serial number) and read it again before writing. Take the device and port lock (the author's own lock script, kept outside this repository). **Status**: worked around.

#### Opening the serial port restarts the board

Opening or closing the USB port can reset the chip. Side effect observed on 06/11: the USB restart brought the volume of the ESPHome firmware back from 85 to 100 (the ESPHome firmware apparently does not keep it; not fixed). Open the port with `dtr=False, rts=False` set before `open()` when you want to listen without resetting.

#### Serial capture on Windows: nothing is displayed, or the start counter goes up by itself

- An RTS pulse is only transmitted if DTR is also updated (esptool does it); a read started without delay after opening blocks.
- `esptool flash-id` and `read-mac` **reset the chip at every call**: the start counter of the log goes up (no. 3 of 06/11 is probably that one). Do not run them during a restart trial.
- A USB-C cable not fully pushed in leaves the tablet on **battery, without serial port** (Windows: `Present = False` for the MAC): no log, so no readable cause for what happens during that time.

**Seen on** Tab5 ST7121, esptool 5.3.1, 06/11/2026. **Status**: worked around.

---

### Settings page and core

#### No host compiler on the PC: the core tests do not run

**Cause.** No `gcc`, `clang` or `cl` installed (nor Visual Studio) on the author's PC.

**Provisional solution (until 06/11/2026).** The tests are `constexpr` functions evaluated **at compile time** (`static_assert`) by ESP-IDF's RISC-V cross g++ in `-fsyntax-only`.

**Definitive solution.** llvm-mingw (clang 23.1.5) installed on 06/11/2026, see [outils.md](outils.md): after setting up the host environment as described there (`env_hote.ps1 -Sanitizers`), `python test/host/lancer.py` compiles, links and **runs** the tests, with AddressSanitizer and UBSan.

**Seen on** the author's PC (Windows 11), `main` on 06/11/2026. **Evidence**: a deliberately faulty program (a read outside an array) is detected by AddressSanitizer with the line and the allocation site; the 23 core tests pass when run (with and without detectors); 18 mutations out of 18 detected. After the merge of the tracks, the same check runs 252 tests (ASan + UBSan) and detects 206 mutations out of 206. **Limit**: the PC is x86-64; a test that passes on the PC does not prove the behaviour of the board's RISC-V (sizes, alignment, available memory): the observation on the board is still needed. **Status**: resolved.

#### A 404 when trying an LLM service: error or not?

A 404/405 on `GET /models` is **not** an error (valid local service: not all of them expose this route). Only a 404 on the route of the real trial gives the sentence “il manque peut-être /v1” (“/v1 may be missing”; contract, [api-reglages.md](api-reglages.md)). **Status**: resolved in the contract.

#### The fake service copies the received key into its error

Intended: the fake service answers 401 with a body that copies the key, to **prove** that the device never retransmits the text of a third-party service. Check of the check: a mutation trial makes two checks fail.

#### The web run needs the network

`python web/tests/parcours.py` really calls Open-Meteo (place search, weather): without Internet it fails. Replayed on 06/11/2026 by the coordination session: 49/49 checks, exit code 0. The stylesheet uses `:has()`: on an old phone, only the red outline of an invalid field would be missing (Safari iOS and Firefox: not verified).

---

## Version Française

---

Tenu au fil des sessions. **À lire avant de re-diagnostiquer un symptôme qui ressemble à l'un de ceux-ci** ; plusieurs coûtent des heures quand on les cherche à nouveau.

Format de chaque entrée : **Symptôme → Cause → Solution**, puis trois lignes :
- **Vu sur** : carte, révision d'écran, commit, date (voir [compatibilité](compatibilite.md#version-du-firmware-et-de-la-carte--comment-on-la-note)). Aucune adresse MAC n'est publiée.
- **Preuve** : comment on l'a constaté (journal, écran, mesure, relecture).
- **Statut** : *résolu*, *contourné*, *ouvert* ou *connu ailleurs, pas encore vu ici*.

Une entrée « connue ailleurs » vient du firmware ESPHome du Tab5 ([son dépannage](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/docs/troubleshooting.md)) : la cause matérielle est la même, mais elle n'a pas encore été revue avec ce firmware natif.

### Sommaire

- [Ouverts](#ouverts)
- [Outils et compilation](#outils-et-compilation)
- [Puce, écran et tactile](#puce-écran-et-tactile)
- [Flash, port série et sauvegarde](#flash-port-série-et-sauvegarde)
- [Page de réglages et noyau](#page-de-réglages-et-noyau)

---

### Ouverts

#### Après une coupure de courant, l'image n'apparaît qu'après un second geste

**Symptôme.** Coupure de courant du Tab5, puis remise : le premier démarrage ne montre pas d'image ; l'image « Bonjour » et la mire s'affichent après un second geste de l'auteur (démarrage n° 9 propre, ST7121 détecté). Le premier démarrage « sans écran » **n'est pas dans le journal série** : le compteur de démarrages passe de 8 à 9.

**Cause.** **Non trouvée** au 06/11/2026. Le journal ne contient qu'un démarrage après la coupure (n° 9), complet et propre : ST7121 détecté au 1er sondage, première image, rétroéclairage à 75 % à 1598 ms. Donc soit le premier démarrage n'a pas lancé ce firmware, soit il n'a pas été capturé (tablette sur batterie, sans port série). Hypothèses **non vérifiées** : l'alimentation USB du Tab5 n'allume peut-être pas la tablette sans appui sur le bouton ; le reset de l'écran par l'expandeur (qui a besoin d'une seconde, par analogie avec ESPHome) ne semble pas en cause puisque le n° 9 est propre. La cause lue du rallumage est « chien de garde » (`HP_SYS_HP_WDT_RESET`) aux **deux** remises sous tension observées : peut-être la cause normale d'un rallumage de cette carte, non vérifié, aucun plantage (`Guru`, `abort`, `Backtrace`) vu.

**Solution.** Aucune appliquée. Pour l'utilisateur : après une coupure complète, un appui court sur le bouton allume la mire. Le critère de 1a (« image après redémarrage **et** après coupure ») est donc constaté **avec réserve** ; l'essai pas à pas (rebrancher l'USB sans toucher aucun bouton, attendre 30 s, noter ce qui s'affiche, port série ouvert) est à refaire au jalon 1b avant d'attribuer la cause au firmware ou à la carte. L'auteur a choisi de ne pas le refaire pendant 1a.

**Vu sur** Tab5 ST7121, firmware natif commit `e7a4803`, 06/11/2026. **Preuve** : journal du démarrage n° 9, constat de l'auteur ; détail dans [mesures/1a.md](mesures/1a.md). **Statut** : ouvert. Jalon 1b (06/11/2026) : l'essai pas à pas a été **reporté par l'auteur** (« Pas maintenant », 20 h 59), ni fait ni raté ; voir [mesures/1b.md](mesures/1b.md).

#### Retour à ESPHome jamais essayé

La recette de remise en état (trois écritures : début de flash, application, NVS) est dans le `LISEZMOI.md` de la sauvegarde locale de l'auteur, hors de ce dépôt. Elle n'a **jamais été jouée** : tant qu'elle ne l'est pas, le retour est « non vérifié ». **Statut** : ouvert.

---

### Outils et compilation

#### `export.ps1` d'ESP-IDF échoue, puis « python_env doesn't exist »

**Symptôme.** L'`export.ps1` de l'ESP-IDF installé par ESPHome s'arrête (il cherche `python_env\idf5.5_py3.13_env`) ; ensuite `idf.py set-target` répond « python_env doesn't exist ».

**Cause.** Cet ESP-IDF est installé et possédé par ESPHome, avec une autre organisation de l'environnement Python (`penvs\5.5.5`) que celle attendue par les scripts d'Espressif.

**Solution.** `tools/idf.ps1` n'utilise pas `export.ps1` : il appelle `tools\idf.py` avec le Python de `penvs\5.5.5` de cet ESP-IDF, pose le `PATH` des outils (cmake, ninja, ccache, GCC RISC-V) et la variable `IDF_PYTHON_ENV_PATH`. Ne rien modifier dans le dossier de cet ESP-IDF (c'est celui d'ESPHome).

**Vu sur** PC de l'auteur (Windows 11), ESP-IDF 5.5.5, 06/11/2026. **Preuve** : squelette compilé en 38 s, 204 048 octets (non flashé). **Statut** : résolu.

#### Avertissements et chemins morts sans effet

- « Error while generating esp_rom gdbinit » au début d'une compilation : inoffensif, la compilation réussit. *Statut : constaté sur le squelette.*
- L'`idf-env.json` de l'ESP-IDF d'ESPHome cite encore un chemin sur un autre disque (chemin mort, fichier d'ESPHome non corrigé) : sans effet sur nos compilations jusqu'ici. *Statut : non vérifié au-delà.*

#### Le PC plante pendant une compilation

**Cause.** Trop de tâches en parallèle (le PC a déjà planté à 6) ou deux compilations à la fois.

**Solution.** `CMAKE_BUILD_PARALLEL_LEVEL=4` (posé par `tools/idf.ps1`), **une seule compilation ESP-IDF à la fois**, depuis PowerShell (jamais Git Bash). Après un plantage : `git fsck`. Les sessions qui n'ont pas besoin de compiler (page web, noyau, recherche) ne compilent pas.

**Vu sur** PC de l'auteur : planté à 6 tâches en parallèle (consigne de l'auteur, compilations ESPHome), jamais encore avec ce projet. **Statut** : contourné par la limite à 4.

#### Une modification de `sdkconfig.defaults*` n'a aucun effet

**Cause.** Les valeurs par défaut ne sont relues que lorsque `sdkconfig` n'existe pas.

**Solution.** Supprimer `sdkconfig`, relancer `.\tools\idf.ps1 set-target esp32p4`, puis vérifier la valeur dans le `sdkconfig` généré (pas dans le fichier de défauts). **Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : résolu.

#### Un script PowerShell est refusé : suppression visant le dossier d'ESP-IDF

Le garde-fou de l'outil de session IA de l'auteur refuse toute commande contenant un `Remove-Item` visant le dossier de l'ESP-IDF d'ESPHome, même dans un script écrit ailleurs. Ne pas supprimer dans les scripts ; déplacer ou ignorer. **Statut** : contourné.

#### `idf.py` s'arrête sur « macro expanded to blank string » dès qu'`esp_hosted` est une dépendance

**Symptôme.** Après l'ajout d'`espressif/esp_hosted` 2.12.12, la configuration (`kconfgen`) s'arrête sur le Kconfig de ce composant avec « macro expanded to blank string ».

**Cause.** Ce Kconfig lit `$(ESP_IDF_VERSION)` (par exemple « 5.5 »), une variable qu'ESP-IDF ne pose pas (il pose `IDF_VERSION`).

**Solution.** `tools/idf.ps1` pose `ESP_IDF_VERSION` en « majeur.mineur » (ici 5.5) avant d'appeler `idf.py`, comme le fait ESPHome pour ses compilations.

**Vu sur** PC de l'auteur, ESP-IDF 5.5.5, `esp_hosted` 2.12.12, 06/11/2026, commit `f86b066`. **Preuve** : la configuration et la compilation complète passent ensuite (binaire de 1 240 272 octets). **Statut** : résolu.

#### `ninja` : « reglages_locaux.cmake is defined as an output multiple times »

**Cause.** Un `set_property(DIRECTORY … CMAKE_CONFIGURE_DEPENDS …)` sur un fichier que CMake suit déjà parce que `main/CMakeLists.txt` le lit avec `include()`.

**Solution.** Retirer le `set_property` : CMake relance la configuration tout seul quand un fichier inclus change. **Vu sur** PC de l'auteur, 06/11/2026. **Statut** : résolu.

#### Un script neuf n'apparaît pas dans `git status` : le garde-fou des secrets l'ignorait

**Cause.** Le motif `reglages_locaux*` du `.gitignore` (prévu pour `reglages_locaux.cmake`, généré, qui contient les identifiants Wi-Fi) correspondait aussi à `tools/reglages_locaux.ps1`. Rien ne prévient : le fichier n'apparaît simplement jamais (`git check-ignore -v <fichier>` montre le motif).

**Solution.** Le script a été renommé `tools/preparer_wifi.ps1` ; le motif reste tel quel (c'est le garde-fou). **Vu sur** PC de l'auteur, 06/11/2026, commit `761d135`. **Statut** : résolu.

#### Le garde de traduction ne voit pas les textes ajoutés en 1b

`test/host/controle_i18n.py` ne lit que les constantes `kXxx = "…";` de `main/ui_bonjour.cpp` et `main/boot_info.cpp`, confrontées à la section « Écran Bonjour » de la table de traduction. Les textes nouveaux de 1b (horloge, état du réseau) vivent donc dans `main/textes_1b.h`, **hors table** et sans passer par `tr()` ; ils passeront par `tr()` avec la page de réglages. **Statut** : contourné, à refaire en phase 2.

#### `idf.py set-target` échoue au téléchargement des composants : « No such file or directory » sur un chemin très long

**Symptôme.** Dans un nouveau worktree git, `.\tools\idf.ps1 set-target esp32p4` s'arrête pendant le téléchargement des composants sur une `FileNotFoundError: [Errno 2] No such file or directory: '…\.cache\composants\service_…\espressif__esp_hosted_2.12.12_…\examples\host_network_split__power_save\…\host_wakeup_demo_using_udp_packet.c'` de Python. Un `build/` à moitié écrit reste, et le `set-target` suivant s'arrête sur « doesn't seem to be a CMake build directory ».

**Cause.** La limite de 260 caractères des chemins Windows (`LongPathsEnabled` vaut 0 sur ce PC) rencontre le cache de composants que `tools/idf.ps1` place dans le projet (`<worktree>\.cache\composants`). Le plus long fichier d'`esp_hosted` 2.12.12 est à 184 caractères sous la racine du worktree ; avec le dossier `reglages-appareil-b3aad6` (racine de 76 caractères) son chemin fait exactement 260 caractères. Les autres worktrees, aux noms plus courts de 2 à 5 caractères, tombent à 251–258 et passent : **un nom de dossier de worktree de 24 caractères ou plus échoue** ici.

**Solution.** Contournée, pas corrigée dans le dépôt : supprimer le `build/` à moitié écrit et `sdkconfig*`, puis lancer la compilation avec une copie privée de `tools/idf.ps1` (non commitée) où `IDF_COMPONENT_CACHE_PATH` désigne un dossier court hors du dépôt (ici `C:\Users\axell\.tab5\cache-composants`) ; tout le reste est identique. Pistes durables **non faites** : faire respecter par `tools/idf.ps1` un `IDF_COMPONENT_CACHE_PATH` déjà défini (une ligne), ou donner des noms plus courts aux worktrees. Activer les chemins longs de Windows est un réglage système : non touché.

**Vu sur** le PC de compilation (Windows 11, ESP-IDF 5.5.5), branche `claude/reglages-appareil-b3aad6`, 06/11/2026 ; aucune carte en jeu. **Preuve** : trace Python dans le journal de compilation ; longueur du chemin calculée (260) et même calcul pour les autres worktrees ; avec le cache court, la configuration, le téléchargement des composants (dont `espressif/mdns` 1.13.1) et la compilation (code de sortie 0) sont passés. **Statut** : contourné.

---

### Puce, écran et tactile

#### ESP-IDF refuse de démarrer sur la puce du Tab5

**Cause.** La puce du Tab5 est un ESP32-P4 **révision 1.3** ; ESP-IDF 5.5.5 exige par défaut une révision ≥ 3.1.

**Solution.** Dans `boards/tab5/sdkconfig.defaults.board` : `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` et `CONFIG_ESP32P4_REV_MIN_100=y` (vérifié dans le `sdkconfig` généré : `CONFIG_ESP32P4_REV_MIN_FULL=100`). Les réglages de puce sont propres à chaque carte : la 7B (révision 3.x) a les siens.

**Vu sur** Tab5 ST7121, 06/11/2026 (relevé par la piste matériel). **Statut** : résolu.

#### Quelle vitesse de ligne DSI pour le ST7121 ? Quatre sources se contredisent

| Source | Valeur |
|---|---|
| esp-bsp (`m5stack_tab5`) | 965 Mbit/s |
| ESPHome, modèle `M5STACK-TAB5-ST7121` (celui qui fait tourner cette tablette) | 965 Mbit/s |
| espp | 900 Mbit/s |
| macro du composant `esp_lcd_st7121` | 1300 Mbit/s |

**Solution.** 965 : c'est ce qui fait déjà fonctionner la tablette sous ESPHome. 900 puis 1300 seulement si l'écran reste noir. Marges DPI concordantes sur quatre sources : horloge 70 MHz, horizontales 40/2/40, verticales 24/20/200, 720 × 1280 en RGB565. **Vu sur** Tab5 ST7121. **Statut** : résolu — 965 Mbit/s est la valeur finale, l'image s'affiche avec elle (constaté le 06/11/2026, commit `e7a4803`) ; les essais à 900 et 1300 n'ont pas été faits, inutiles.

#### Le composant `esp_lcd_st7123` 1.0.2 ne convient pas au Tab5 tel quel

Ses macros décrivent une dalle 720 × 1560, pas celle du Tab5. La table d'initialisation du Tab5 ST7123 est dans l'esp-bsp (`disp_init_data_1.h`), à porter quand l'exemplaire sera revenu du SAV. Le ST7123 et l'ILI9881C sont **détectés** (journal + `ESP_ERR_NOT_SUPPORTED`), pas initialisés. **Statut** : connu, non traité.

#### Les accents s'affichent en carrés ou manquent

**Cause.** Les polices intégrées de LVGL s'arrêtent à l'ASCII.

**Solution.** Police TinyTTF embarquée (Roboto Bold, licence OFL, `main/assets/`). Toute icône ou glyphe affiché doit exister dans la police chargée : une icône MDI n'est jamais un rectangle vide. **Statut** : résolu (accents) ; icônes : à vérifier quand elles arrivent.

#### Le point tactile est décalé vers la gauche

**Symptôme.** Premier essai sur le Tab5 : le point suit le doigt mais décalé vers la gauche ; un glissement pouvait faire défiler l'écran.

**Cause.** Le point était posé sur l'écran dont le remplissage haut (90 px) décale les objets flottants : LVGL les place par rapport à la zone de contenu du parent.

**Solution.** Point sur la couche supérieure (sans remplissage), écran non défilable, et une **mire** (cadre sur les bords + cinq cibles aux coins et au centre) pour mesurer le décalage restant entre le tactile et l'image à partir des appuis du journal série.

**Vu sur** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Preuve** : constat de l'auteur (le point est sous son doigt) et cinq appuis sur des cibles connues : écart moyen (−6, −4) px, de −12 à +10 px sur quatre cibles et −26 px en x sur la cinquième (haut droit), sans étalonnage appliqué (détail dans [mesures/1a.md](mesures/1a.md)). **Statut** : résolu ; précision de l'ordre d'un doigt.

#### Aucun journal par l'USB, ou une erreur de compilation sur un `|` entre deux drapeaux LVGL

- **Pas de journal** : il faut `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (la console UART par défaut n'est pas câblée à l'USB du Tab5). Posé dans `boards/tab5/sdkconfig.defaults.board`.
- **`A | B` sur deux drapeaux LVGL** : en C++ le résultat est un `int` que LVGL refuse ; un appel par drapeau. De même, les macros de configuration d'Espressif (initialiseurs désignés dans un ordre que C++ refuse) : la couche carte (`components/bsp_tab5/`) est donc écrite en **C**, l'application en C++.

**Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : résolu.

#### Journal `swap_xy is not supported by this panel` au démarrage

Émis par `esp_lvgl_port` à l'initialisation ; sans effet constaté sur l'image (orientation portrait native fixe, aucune rotation logicielle). Cause non explorée. **Vu sur** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Statut** : connu, sans effet constaté.

#### Marge faible du bootloader et test de la PSRAM

Le bootloader occupe 0x5a30 octets dans ses 24 Kio : **6 % de marge** seulement, à surveiller si on ajoute des options. Le démarrage prend environ 2,9 s de la remise à zéro à la première image, dont ~0,95 s de test de la PSRAM (`CONFIG_SPIRAM_MEMTEST=y`) et 1 s d'attente de l'expandeur : deux gains possibles, **à mesurer avant de sacrifier quoi que ce soit**, un changement à la fois. **Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : connu, pas traité.

#### Écran noir après un redémarrage logiciel

*Connu ailleurs ; ici, un seul redémarrage logiciel essayé et réussi (n° 2 du 06/11/2026, `SW_SYS_RESET`, image revenue, Tab5 ST7121), pas de série de 20.* Sur le firmware ESPHome, la broche de reset de l'écran passe par l'expandeur PI4IOE5V6408, qui a besoin d'environ une seconde pour être fiable : reset trop tôt = écran noir. Le firmware natif reprend l'attente de 1 s avant le reset (propre à la carte : la 7B n'en a pas, son reset est une broche du P4). Sur P4, `esp_restart()` ne remet à zéro que les cœurs : terminer par `esp_rom_software_reset_system()`. À vérifier au jalon 1b par 20 redémarrages.

---

### Flash, port série et sauvegarde

#### `read-flash` échoue : « Packet content transfer stopped »

**Cause.** Lecture de toute la flash avec le stub d'esptool sur une zone qui ne répond pas ; ici aussi, une erreur d'offset (la NVS n'est pas au début de la flash).

**Solution.** Lire les partitions utiles aux offsets lus dans la **table de partitions de la carte** (ici la NVS d'ESPHome est en `0xf90000`), deux fois, comparer les SHA-256 ; repli `--no-stub` si le stub échoue ; `verify-flash` (somme MD5 calculée par la puce) pour prouver qu'un fichier rangé est identique à la flash. **Vu sur** Tab5 ST7121, esptool 5.3.1, 06/11/2026. **Statut** : résolu (sauvegarde partielle faite, voulue par l'auteur : le code est dans git, pas la NVS).

#### Le port COM n'est pas celui qu'on croit

**Cause.** Le numéro change avec la prise USB ; le 06/11/2026 le Tab5 était sur COM11, et COM14 était un **ESP32-S3 d'un autre projet**.

**Solution.** Toujours retrouver le port par la **MAC** (`serial.tools.list_ports`, numéro de série USB) et la relire avant d'écrire. Prendre le verrou de l'appareil et du port (le script de verrou propre à l'auteur, gardé hors de ce dépôt). **Statut** : contourné.

#### Ouvrir le port série redémarre la carte

Ouvrir ou fermer le port USB peut réinitialiser la puce. Effet de bord constaté le 06/11 : le redémarrage USB a ramené le volume du firmware ESPHome de 85 à 100 (le firmware ESPHome ne le conserve apparemment pas ; non corrigé). Ouvrir le port avec `dtr=False, rts=False` avant `open()` quand on veut écouter sans réinitialiser.

#### Capture série sous Windows : rien ne s'affiche, ou le compteur de démarrages monte seul

- Une impulsion RTS n'est transmise que si DTR est aussi mis à jour (esptool le fait) ; une lecture lancée sans délai après l'ouverture bloque.
- `esptool flash-id` et `read-mac` **remettent la puce à zéro à chaque appel** : le compteur de démarrages du journal monte (le n° 3 du 06/11 est probablement celui-là). Ne pas les lancer pendant un essai de redémarrage.
- Un USB-C mal enfoncé laisse la tablette allumée **sur batterie, sans port série** (Windows : `Present = False` pour la MAC) : aucun journal, donc aucune cause lisible pour ce qui se passe pendant ce temps.

**Vu sur** Tab5 ST7121, esptool 5.3.1, 06/11/2026. **Statut** : contourné.

---

### Page de réglages et noyau

#### Aucun compilateur hôte sur le PC : les tests du noyau ne s'exécutent pas

**Cause.** Pas de `gcc`, `clang` ni `cl` installé (ni Visual Studio) sur le PC de l'auteur.

**Solution provisoire (jusqu'au 06/11/2026).** Les tests sont des fonctions `constexpr` évaluées **à la compilation** (`static_assert`) par le g++ croisé RISC-V d'ESP-IDF en `-fsyntax-only`.

**Solution définitive.** llvm-mingw (clang 23.1.5) installé le 06/11/2026, voir [outils.md](outils.md) : une fois l'environnement hôte posé comme il y est décrit (`env_hote.ps1 -Sanitizers`), `python test/host/lancer.py` compile, lie et **exécute** les tests, avec AddressSanitizer et UBSan.

**Vu sur** PC de l'auteur (Windows 11), `main` au 06/11/2026. **Preuve** : un programme fautif volontaire (lecture hors d'un tableau) est détecté par AddressSanitizer avec la ligne et le lieu d'allocation ; les 23 tests du noyau passent exécutés (avec et sans détecteurs) ; 18 mutations sur 18 détectées. Après la fusion des pistes, le même contrôle exécute 252 tests (ASan + UBSan) et détecte 206 mutations sur 206. **Limite** : le PC est en x86-64 ; un test réussi sur le PC ne prouve pas le comportement du RISC-V de la carte (tailles, alignement, mémoire disponible) : le constat sur carte reste nécessaire. **Statut** : résolu.

#### Un 404 sur l'essai d'un service LLM : erreur ou pas ?

Un 404/405 sur `GET /models` n'est **pas** une erreur (service local valide : tous n'exposent pas cette route). Seul un 404 sur la route de l'essai réel donne la phrase « il manque peut-être /v1 » (contrat, [api-reglages.md](api-reglages.md)). **Statut** : résolu dans le contrat.

#### Le faux service recopie la clé reçue dans son erreur

Voulu : le faux service répond 401 avec un corps qui recopie la clé, pour **prouver** que l'appareil ne retransmet jamais le texte d'un service tiers. Contrôle du contrôle : un essai de mutation fait échouer deux vérifications.

#### Le parcours web demande le réseau

`python web/tests/parcours.py` appelle vraiment Open-Meteo (recherche de lieu, météo) : sans Internet il échoue. Rejoué le 06/11/2026 par la session de coordination : 49/49 vérifications, code de sortie 0. La feuille de style utilise `:has()` : sur un téléphone ancien, seul le contour rouge d'un champ invalide manquerait (Safari iOS et Firefox : non vérifié).
