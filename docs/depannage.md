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
- [Memory and watchdog](#memory-and-watchdog)
- [Wi-Fi](#wi-fi)
- [Alerts (Vigie)](#alerts-vigie)
- [Audio, expanders and power](#audio-expanders-and-power)
- [Settings page and core](#settings-page-and-core)
- [Main screen](#main-screen)

---

### Open

#### After a power cut, the image only appears after a second gesture

**Symptom.** Power cut on the Tab5, then power back: the first start-up shows no image; the “Bonjour” image and the test pattern appear after a second gesture by the author (clean start-up no. 9, ST7121 detected). The first start-up “without a screen” **is not in the serial log**: the start counter goes from 8 to 9.

**Cause.** **Not found** as of 06/11/2026. The log holds only one start-up after the cut (no. 9), complete and clean: ST7121 detected at the 1st probe, first image, backlight at 75% at 1598 ms. So either the first start-up did not run this firmware, or it was not captured (tablet on battery, without serial port). Hypotheses, **not verified**: the Tab5's USB power perhaps does not turn the tablet on without a press on the button; the display reset through the expander (which needs one second, by analogy with ESPHome) does not seem to be the cause, since no. 9 is clean. The reset cause read at power-up is “watchdog” (`HP_SYS_HP_WDT_RESET`) at **both** observed power-ups: perhaps the normal cause of a power-up of this board, not verified; no crash (`Guru`, `abort`, `Backtrace`) seen.

**Solution.** None applied. For the user: after a complete cut, a short press on the button turns the test pattern on. The criterion of 1a (“image after a restart **and** after a power cut”) is therefore observed **with a reservation**; the step-by-step trial (plug the USB back in without touching any button, wait 30 s, note what is displayed, serial port open) is to be redone at milestone 1b before attributing the cause to the firmware or to the board. The author chose not to redo it during 1a.

**Seen on** Tab5 ST7121, native firmware commit `e7a4803`, 06/11/2026. **Evidence**: log of start-up no. 9, the author's observation; details in [mesures/1a.md](mesures/1a.md). **Status**: open. Milestone 1b (06/11/2026): the step-by-step trial was **postponed by the author** (“Not now”, 20:59), neither done nor failed; see [mesures/1b.md](mesures/1b.md).

**Update 06/11/2026, 22:27 (same family of failure, reproduced WITH a log).** *Seen on* Tab5 ST7121, binary of commit `c1fa7f6`, start-up no. 65, right after a flash (reset by esptool's RTS signal, **not** a power cut): `pi4ioe5v6408: reset(252): Chip reset failed` → `Reset failed` → `tab5_io_init: expandeur 0x43` → `board_init: E/S` → `carte non initialisée ; arrêt (l'écran reste éteint)`. The expander at 0x43 did not answer the very first I2C transaction; the firmware made **one** attempt and stopped, **black screen** (the author: “écran noir”); a new flash gave the image back. **Hypothesis, not verified**: this is also what happened at the first start-up “without a screen” after the power cut (no. 8 → 9, not captured). **Fix** (`tab5_io.c`): up to 5 attempts 150 ms apart with an I2C bus reset in between, one log line per attempt, then at most 3 recovery restarts (RTC counter) before a stop with a readable message. Tried on the board with **simulated** failures (test binaries): 2 failures then success at attempt 3/5; 5 failures repeated → 3 restarts (`SW_SYS_RESET`, counter kept) then stop. The **real** failure was not reproduced since: the fix is not proven against it. **Series of 20 resets** (06/11/2026, 22:49–23:00, Tab5 ST7121, esptool-style RTS reset with a fresh port each time; a start-up counts only if a new start-up number and the version line appear): **before** (`c1fa7f6`, single attempt) start-ups no. 99–118, **20/20** “Carte prête” + main screen loaded in the log, 0 “Chip reset failed”; **after** (`9f82623`, with recovery) no. 78–97, **20/20**, 0 “expandeur 0x43 : essai” line (the expander answers at the first attempt every time), 0 “Chip reset failed”; the start-ups right after each flash (no. 98 and no. 119) are fine too. So the no. 65 failure is **not reproduced** and the recovery is **not proven** by this test (0/21 failures before, 0/21 after); it only shows the recovery does not slow down a normal start-up (3.03–3.08 s to “Carte prête”). “Image back” means log lines: only the author saw the screen itself. **Do not confuse**: `ESP_ERR_TIMEOUT` on the expander = a **simulated** failure from a test binary (`TAB5_AUTOTEST_EXPANDEUR`, start-ups no. 67–72, version shown `g3b68f4` because built from a dirty tree); `Chip reset failed` / `ESP_ERR_INVALID_STATE` = the **real** failure (no. 65). **Status**: still open (cause of the real failure unknown; recovery added, not proven).

#### Return to ESPHome never tried

The restore recipe (three writes: start of the flash, application, NVS) is in the `LISEZMOI.md` file of the author's local backup, which is outside this repository. It has **never been played**: as long as it has not been, the return is “not verified”. **Status**: open.

#### The battery connector reads 8.2 V although the unit “has no battery”

**Symptom.** On 07/11/2026 the INA226 reads **8206 mV and +2 mA** at the battery connector, with the charger forced off, stable over the whole bench. The notes inherited from the ESPHome firmware say “the author's unit has no battery” (1.83–1.94 V charger off).
**Cause.** **Not found.** Compatible with a two-cell lithium pack nearly full and idle (a battery connected since), not with an empty connector. Nothing in the software can tell the two apart.
**Next step.** Look at the back of the unit (physical check). The firmware never switches the charger on, so nothing is charged by it in the meantime; the application must not infer a battery state from the reading.
**Seen on** Tab5 ST7121, commit `5f797e3`, 07/11/2026 (see [mesures/tab5-materiel.md](mesures/tab5-materiel.md)). **Evidence**: log lines `tab5_alim: INA226 tel que trouvé … (8206 mV)` at every start and `alim.lecture` of the bench. **Status**: open.

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

#### In a git worktree, CMake fails: `grabRef.cmake` “file failed to open head-ref”

**Symptom.** In a worktree created by `git worktree add` (one per session), `idf.py set-target` or the first build stops in `grabRef.cmake` (`file failed to open head-ref`), while the main checkout builds.

**Cause.** ESP-IDF reads the branch reference as a file `<git dir>/refs/heads/<branch>`; a branch that exists only in `packed-refs` (no loose file) has none. The version string (`git describe`) then cannot be computed.

**Solution.** Create the loose reference with the same commit: write the commit hash into the file `<git dir of the worktree's repository>/refs/heads/<branch>` (a `git update-ref` alone did not create it). Also: a binary built from a tree with modified tracked files carries `-dirty` in its version (`project_version` in `build/project_description.json`): commit first, then build the binary that is handed over.

**Seen on** the author's PC, worktree `claude/ecran-principal-f0f722`, 06/11/2026. **Evidence**: the build failed with that message, then succeeded after the file was created (version `jalon-1a-tab5-st7121-49-g6a0e6cc`, no `-dirty`). **Status**: worked around (cause read in the CMake script, not explored further).

#### The build stops on `-Werror=format-truncation` in the interface

**Cause.** The ESP-IDF build compiles with `-Werror=all`: an `snprintf` into a buffer the compiler cannot prove large enough (here 40 bytes for “min %s · max %s” with two 15-byte texts) is an error, not a warning.

**Solution.** A buffer sized for the worst case of the format (48 bytes there). **Seen on** ESP-IDF 5.5.5, 06/11/2026. **Status**: resolved.

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

#### `verrous.py` fails with “No module named 'filelock'” after a build

**Cause.** `tools/idf.ps1` puts the Python of the ESP-IDF environment first on the `PATH` for the rest of the PowerShell session (read in the script). The lock script that tells the sessions who builds or flashes (it lives outside the repository) needs `filelock`, which that Python does not have. Taking the lock before the build works; giving it back after the build, in the same session, fails and leaves the lock held.

**Solution.** Give the lock back from another PowerShell call (or with the PC's own Python, not the ESP-IDF one) and check with the `etat` command that it is free. **Seen on** the author's PC, 06/11/2026, commit `793bdfc`. **Evidence**: the traceback at the `rendre` call after `.\tools\idf.ps1 build`; the same call from a fresh call worked. Seen again on 07/11/2026 (branch `claude/source-meteo-client-reseau-29c0af`): a build script took the `build` lock, ran `tools\idf.ps1` twice, and its final `rendre` failed the same way, leaving the lock held after both builds had passed; checked in a clean call that `python` was the PC's before `idf.ps1` and `<ESP-IDF folder>\penvs\5.5.5\Scripts\python.exe` after, and released the lock with the PC's Python. **Status**: worked around.

#### The shared `clang++-san.cmd` wrapper fails: “'clang++' is not recognised as an internal or external command”

**Symptom.** After `. <the PC's tools folder>\env_hote.ps1 -Sanitizers`, every test of `python test/host/lancer.py` fails to build and the output says `'clang++' n’est pas reconnu en tant que commande interne ou externe, un programme exécutable ou un fichier de commandes` (French Windows), although `clang++.exe` is in `_outils\llvm-mingw\bin`.

**Cause.** `<the PC's tools folder>\clang++-san.cmd` (the `CXX` that `-Sanitizers` selects) is a batch file saved with **LF-only line endings (no CR at all)** and with accented letters in its `rem` line: `cmd.exe` cuts it at the wrong places and runs a fragment as a command. The file is not part of this repository.

**Solution.** Worked around, not fixed: a local copy of three lines, ASCII only, CRLF line endings (`@echo off`, an accent-free `rem`, then `"<the PC's tools folder>\llvm-mingw\bin\clang++.exe" -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer %*`), and `$env:CXX` pointed at it after `env_hote.ps1`. Fix for good, in the tools folder (owner's decision, not touched here): rewrite that file with CRLF endings and no accent.

**Seen on** the author's PC (Windows 11), branch `claude/source-meteo-client-reseau-29c0af`, 07/11/2026; no board involved. **Evidence**: the shared wrapper run by hand on an empty program prints the message above; the file has 265 bytes, 0 CR, 3 LF, 6 bytes above 127; with the local copy `python test/host/lancer.py` reports “complet (compilé + exécuté)”.

#### `idf.py set-target` fails while downloading components: “No such file or directory” on a very long path

**Symptom.** In a new git worktree, `.\tools\idf.ps1 set-target esp32p4` stops during the component download with a Python `FileNotFoundError: [Errno 2] No such file or directory: '…\.cache\composants\service_…\espressif__esp_hosted_2.12.12_…\examples\host_network_split__power_save\…\host_wakeup_demo_using_udp_packet.c'`. A half-written `build/` is then left behind, and the next `set-target` stops on “doesn't seem to be a CMake build directory”.

**Cause.** The 260-character Windows path limit (`LongPathsEnabled` is 0 on this PC) meets the component cache that `tools/idf.ps1` puts inside the project (`<worktree>\.cache\composants`). The longest file of `esp_hosted` 2.12.12 lies 184 characters below the worktree root; with the folder `reglages-appareil-b3aad6` (root of 76 characters) its path is exactly 260 characters. The other worktrees, whose names are 2 to 5 characters shorter, come out at 251–258 and pass: **a worktree folder name of 24 characters or more fails** here. *Correction (07/11/2026):* the longest file is in fact `…\host_wakeup_demo_using_udp_packet.sh`, one character longer than the `.c` file quoted above (185 characters below the root), so a folder name of **23 characters or more** fails (`agent-a4c19a135e3c33e73`: root of 75 characters, path of exactly 260).

**Solution.** Worked around, not fixed in the repository: delete the half-written `build/` and `sdkconfig*`, then run the build with a private copy of `tools/idf.ps1` (not committed) in which `IDF_COMPONENT_CACHE_PATH` points to a short folder outside the repository (here `%USERPROFILE%\.tab5\cache-composants`); everything else is identical. Durable options **not done**: have `tools/idf.ps1` keep an `IDF_COMPONENT_CACHE_PATH` that is already set (one line), or give worktrees shorter names. Turning on Windows long paths is a system setting and was not touched. A second way that needs no copy of the script (07/11/2026): a directory junction with a short name (`New-Item -ItemType Junction -Path H:\w -Target <worktree>`), then run `.\tools\idf.ps1` from `H:\w`, after copying a complete `.cache\composants` and `managed_components` from a sibling worktree **to the junction path**, not to the long one (same limit). CMake resolves the junction for the sources; only the component cache keeps the short path, which is the one that matters. Remove the junction when done (`cmd /c rmdir H:\w` removes the link, not the folder).

**Seen on** the build PC (Windows 11, ESP-IDF 5.5.5), branch `claude/reglages-appareil-b3aad6`, 06/11/2026; no board involved. **Evidence**: Python traceback in the build log; path length computed (260) and the same computation for the other worktrees; with the short cache the configuration, the download of the components (including `espressif/mdns` 1.13.1) and the build (exit code 0) went through. Seen again on 07/11/2026 in the worktree `agent-a4c19a135e3c33e73` (the `.sh` file, 260 characters): from the junction, the configuration and the builds of the Tab5 and of the Waveshare 7B went through (exit code 0). Seen again on 07/11/2026 in the worktree `source-meteo-client-reseau-29c0af` (root of 85 characters) and worked around **without a junction**, with the private copy of `tools/idf.ps1` whose component cache is `%USERPROFILE%\.tab5\cache-composants`: the configuration and the builds of the Tab5 and of the Waveshare 7B went through (exit code 0, about 3 minutes in all with the compiler cache warm). **Status**: worked around.


#### Switching boards: stale settings, or the dependency lock rewritten

**Symptom.** After changing `BOARD`, the build keeps the previous board's flash size, partition table or PSRAM mode; or a build of one board downloads the other board's drivers and rewrites `dependencies.lock`.

**Cause.** The `sdkconfig.defaults*` files are only read for a **new** `sdkconfig` (see “A change in `sdkconfig.defaults*` has no effect”); and the component manager looks at every `components/bsp_*` folder and merges all their dependencies into one lock file.

**Solution.** Delete `sdkconfig`, then run `set-target` again with `"-DBOARD=<board>"` ([commands](carte-7b.md#build)). The root CMake excludes the component of the board that is not selected (`EXCLUDE_COMPONENTS`, so the component manager does not see it) and each board other than the Tab5 has its own lock file (`boards/<board>/dependencies.lock`; the Tab5 keeps the one at the root).

**Seen on** the PC, no board, commit `52f6372`, 07/11/2026. **Evidence**: the 7B configures and builds (1,837,760 bytes); the Tab5 builds too (commit `093ac94`, separate build directory) and the root `dependencies.lock` was not modified by it. **Status**: resolved (build side only).

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

#### Landscape: the touch ring is “completely shifted from the finger”

**Symptom.** First landscape trial on the Tab5: the image is upright (“Yes, it's straight”), but the green ring does not appear under the finger (“not at the right place, completely shifted”).

**Cause.** The touch point was rotated **twice**: once by `esp_lcd_touch` (mirror Y + swap X/Y requested in `tab5_touch.c`), once by LVGL itself (`lv_indev.c`, `indev_pointer_proc` → `lv_display_rotate_point`, which for a 90° rotation gives `x = ver_res − Y − 1`, `y = X`). The original comment wrongly claimed that LVGL does not rotate the touch. A finger at logical `(x, y)` gave a ring at `(1279 − y, x)`, off-screen as soon as `x > 719`.

**Solution.** No mirror or swap in `tab5_touch.c`: the chip returns the native frame and LVGL rotates it alone. One log line per press (`appui : brut (X, Y) … -> logique (x, y) …`, `tab5_orientation.c`) gives the proof without relying on what is seen.

**Seen on** Tab5 ST7121, symptom on the binary of commit `102077b`, fix checked on `3b68f42` (test build with the touch self-test) and `9f82623`, 06/11/2026. **Evidence**: the self-test injects six presses in raw panel coordinates (four corners, centre, Settings button) and the log gives “AUTOTEST appui n/6 … OK” six times; 27 real presses are logged and the formula `x = 1279 − Y`, `y = X` holds on every line; the author: “the fingers were right”. **Status**: resolved (details in [mesures/orientation.md](mesures/orientation.md) § 5).

#### Every press on the screen restarted the board
**Symptom.** With the binary of commit `557eb44`, “as soon as I touch the screen it restarts” (the author, ~22:29). **Cause.** The per-press log callback called `lv_event_get_indev(e)`, but for the events of an input device LVGL passes the **device as the target** and the touched object as the parameter (`lv_indev_send_event`): the callback read an object as if it were a device. It had been compiled but never executed before the author was asked to press. **Solution.** `lv_event_get_target(e)` (commit `c1fa7f6`), then a self-test build injects six synthetic presses at start-up so that the event code runs on the board **before** anyone is asked to touch it. **Seen on** Tab5 ST7121, `557eb44` then `c1fa7f6`, 06/11/2026. **Evidence**: self-test 6/6 OK with no restart, then 27 real presses without a restart. **Status**: resolved.

#### No log over USB, or a build error on a `|` between two LVGL flags

- **No log**: it needs `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (the default UART console is not wired to the Tab5's USB). Set in `boards/tab5/sdkconfig.defaults.board`.
- **`A | B` on two LVGL flags**: in C++ the result is an `int` that LVGL refuses; one call per flag. Likewise for Espressif's configuration macros (designated initialisers in an order that C++ refuses): the board layer (`components/bsp_tab5/`) is therefore written in **C**, the application in C++.

**Seen on** Tab5 ST7121, 06/11/2026. **Status**: resolved.

#### Log `swap_xy is not supported by this panel` at start-up

Emitted by `esp_lvgl_port` at initialisation; no observed effect on the image (fixed native portrait orientation, no software rotation). Cause not explored. **Seen on** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Status**: known, no observed effect.

#### Low bootloader margin and PSRAM test

The bootloader takes 0x5a30 bytes of its 24 KiB: only **6% margin**, to watch if options are added. Start-up takes about 2.9 s from the reset to the first image, of which ~0.95 s of PSRAM test (`CONFIG_SPIRAM_MEMTEST=y`) and 1 s of waiting for the expander: two possible gains, **to be measured before sacrificing anything**, one change at a time. On the 7B build (commit `c48c418`, 07/11/2026, PC): bootloader 0x5d10 bytes, **0x2f0 bytes (3 %) free**, even tighter. **Seen on** Tab5 ST7121, 06/11/2026. **Status**: known, not addressed.

#### Black screen after a software restart

*Known elsewhere; here, only one software restart tried and it succeeded (no. 2 of 06/11/2026, `SW_SYS_RESET`, image came back, Tab5 ST7121), no series of 20.* On the ESPHome firmware, the display's reset pin goes through the PI4IOE5V6408 expander, which needs about a second to be reliable: reset too early = black screen. The native firmware takes over the 1 s wait before the reset (specific to the board: the 7B has none, its reset is a P4 pin). On the P4, `esp_restart()` only resets the cores: finish with `esp_rom_software_reset_system()`. To be verified at milestone 1b with 20 restarts.


#### Waveshare 7B: the backlight never goes fully black

*Known elsewhere (the author's ESPHome firmware on this board, 16–17/10/2026); the native fix is written and **not verified on the board**.* The backlight is analog (BL_CTRL → RC filter → AP3032): at 0 % the panel stays dimly lit, and the “display off” command (0x28) alone changed nothing visible. What worked under ESPHome: black veil, then 0x28, then “sleep in” 0x10, and the reverse on wake (0x11, 120 ms, 0x29). The board layer does the same in `board_set_backlight_percent(0)` (`ws7b_panel_plan()`, tested on the PC). **Seen on** Waveshare 7B under ESPHome, 17/10/2026 (the author's observation); native firmware: PC only, commit `52f6372`. **Status**: worked around under ESPHome; the native version is not tested on a board.

#### Waveshare 7B: the C6 does not answer the version request

*Known elsewhere (the author's measurements, 16–19/10/2026).* The factory ESP32-C6 firmware does not answer esp_hosted's “version” request, which can look like a link failure. The board layer does not ask (`WS7B_READ_C6_VERSION=0`) and never updates the C6 (decision of the author, 16/10/2026). **Seen on** Waveshare 7B under ESPHome. **Status**: worked around; not yet seen with this firmware.

#### Waveshare 7B: the speaker saturates above 0 dB

*Known elsewhere (the author's trial by ear, 17–18/10/2026, ESPHome).* ES8311 volume register 0x32: 0xBF = 0 dB (75 % of the register scale), above it the speaker saturates (+32 dB at 0xFF); below 40 % of the scale (0x66) “nothing is heard”. The layer caps at 0xBF and floors a non-zero volume at 0x66; the curve is tested on the PC. **Status**: worked around; not tested on a board.

#### Waveshare 7B: touch reset wiring, two sources disagree

**Symptom.** None seen yet. **Cause.** The schematic and the ESPHome firmware (`reset_pin: GPIO23`) put the GT911 reset on GPIO23; Waveshare's BSP declares it not connected / shared with the display reset. The layer pulses GPIO23 and gives the driver no reset pin. **Solution.** None applied. At the first trial, check in the log that the product ID “911” is read at 0x5D (or 0x14) after the pulse; if not, try without the pulse (display reset only). **Seen on** no board. **Status**: **open**, not verified.

---

### Flash, serial port and backup

#### Black screen after `write-flash --after no-reset`: the application never starts

**Symptom.** `esptool write-flash … --after no-reset` succeeds (“Hash of data verified”); a capture script that then “resets” the board with an RTS pulse only reads nothing for 35 s, and the screen stays black.

**Cause.** `--after no-reset` leaves the chip in the ROM download mode, and the RTS pulse of the capture script did not bring it out of it (the exact reason was not looked into).

**Solution.** Let esptool reset the chip (`python -m esptool --port COMn --after hard-reset chip-id`, or flash without `--after no-reset`), then start the capture at once: the ROM and bootloader lines of that start are missed, the application lines are not. The application started on the first try.

**Seen on** Tab5 ST7121, binary of the branch `fix/retours-axel-07-11` (SHA-256 `756A8333…`), 07/11/2026. **Evidence**: no byte on the port after the RTS pulse, black screen reported by the author; complete log after esptool's hard reset. **Status**: worked around.

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


#### Waveshare 7B: backing up the 32 MB flash

*Known elsewhere (the author's backups of this board on 20/10/2026, esptool 5.3.1).* On the ESP32-P4 the stub `read-flash` fails at fixed sectors: read in 2 MB pieces (about 21 s each), split by 8 on failure, 4 KB sectors with `--no-stub` as a last resort, use `--before default-reset` after a dropped stub, and prove the copy with `verify-flash` (MD5 computed in the chip). The real flash is **32 MB** (the first plan assumed 16 MB). The otadata (0x9000 and 0xa000) must be decoded before choosing what to keep: sequence number, CRC `crc32_le(0xFFFFFFFF, seq)`, slot `(seq − 1) % 2`. `tools/essai_7b.py` does all of this; its decoder was checked against the real otadata of the backup of 20/10/2026 (sequences 5 and 4, both CRC valid, slot 0 chosen). The part of the script that talks to the board has **not been run**. **Status**: worked around; script written, board part not run.

---

### Memory and watchdog

#### Tapping “Wi-Fi” in the settings restarts the board: `sem create failed`, then `assert … port_esp_hosted_host_os.c:589`

**Symptom.** Settings open, tap on “Wi-Fi” (network scan): the board restarts. Log: `sem create failed`, `assert failed: hosted_destroy_semaphore port_esp_hosted_host_os.c:589`, reboot. Twice on 07/11/2026. In the same state the city search failed to open its connection (`Failed to allocate memory`).

**Cause.** With the settings open only **9.7 kB** of internal RAM were free (largest block 4.6 kB). The Wi-Fi link with the ESP32-C6 (`esp_hosted`) and TLS live **only** in internal RAM. LVGL used the system `malloc`, and every block under 256 bytes (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`) is served from internal RAM; an LVGL object, style or label weighs 100 to 250 bytes: the main screen alone took 52 kB, each settings screen about 40 kB.

**Solution.** LVGL has its own allocator, PSRAM first and internal RAM only when PSRAM is full (`main/lvgl_memoire.cpp`, `CONFIG_LV_USE_CUSTOM_MALLOC=y`, commit `ce3ab9f`). The `-Wl,-u,<symbol>` lines of `main/CMakeLists.txt` are required: nothing in `main` calls these functions, and without them the linker leaves the file inside `libmain.a` (`undefined reference to lv_malloc_core`). After a change in `sdkconfig.defaults`, delete `sdkconfig` (the defaults are read again only for a new one). Safety net (commit `f572d9f`): `reseau.cpp` refuses a scan below 16 kB free or an 8 kB largest block, with a log warning; these two thresholds are first values, the real cost of a scan is not measured.

**Seen on** Tab5 ST7121, integrated build `953328d`, 07/11/2026. **Evidence**: before, free internal RAM 74 kB when the time arrives and 9.7 kB with the settings open (minimum 5.5 kB); after, 211 kB and 210 kB (minimum since boot 148,500 B), settings navigated and a scan started by hand by the author, no restart. Drawing speed was not compared with the old allocator. **Status**: resolved on this trial; to watch over a longer run.

#### The weather request fails: `esp-aes: Failed to allocate memory…` or `PK verify failed with error 0x4290`

**Symptom.** `esp-aes: Failed to allocate memory for the array of DMA descriptors` (or `… for start alignment buffer`) about 80 ms after an HTTPS request starts, or, on later attempts, `esp-x509-crt-bundle: PK verify failed with error 0x4290` after 14 s, then `esp-tls: Failed to open new connection`. The weather stays “unavailable”; free internal RAM is below 20 kB (700 B at the lowest).

**Cause.** The ESP32-P4 has about 455 kB of internal heap. By default (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384`) every `malloc()` up to 16 kB is served from it first, and mbedTLS takes its buffers from it too (16 kB in, 4 kB out, certificates). With the display buffers (216 kB), about 106 kB of task stacks and the objects of the screens, 111.6 kB were left before the first request and 74 kB once the web server was up. A handshake needs about 51 kB, and the hardware AES always needs a DMA-capable *internal* block for its descriptor array (`esp_aes_dma_core.c:458`). When one allocation fails the handshake aborts (0x4290 is the RSA public operation failing on an allocation).

**Solution.** In `boards/tab5/sdkconfig.defaults.board`: `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256` (objects of 256 B and more go to PSRAM: +65.9 kB free before the first request) and `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` (TLS buffers in PSRAM: about 29 kB less internal RAM during a handshake; the handshake lasts 2.8 s instead of 1.5 s). Task stacks cannot go to PSRAM here: `spi_flash/cache_utils.c:127` asserts that a task writing the flash (the NVS) has its stack in internal RAM.

**Seen on** Tab5 ST7121, integrated build `4e7d8af`, fix `4f6bd0a`, 06–07/11/2026. **Evidence**: per-stage log lines and the serial commands `memoire`, `blocs` and `piles`; before, 3 failed requests out of 3; after, 29 of 30 boots observed to the end with a successful request (27 in a row), 0 watchdog, 0 allocation failure; details in [mesures/memoire-interne.md](mesures/memoire-interne.md). **Status**: resolved on the Tab5 ST7121; other boards not tried.

#### The task watchdog fires: `IDLE0 (CPU 0)` … `CPU 0: meteo`

**Symptom.** `task_wdt: Task watchdog got triggered … IDLE0 (CPU 0) … CPU 0: meteo`, every 5 s while a TLS handshake runs.

**Cause.** A handshake computes for seconds without ever sleeping. The weather task was at priority 3, above the idle task (0): on the core it runs on, the idle task never gets the CPU and stops feeding the watchdog (`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y`, timeout 5 s).

**Solution.** The weather task runs at `tskIDLE_PRIORITY` (0) and shares the core with the idle task slice by slice (1 ms). The watchdog stays enabled and unchanged: do not disable it or lengthen it to hide this symptom.

**Seen on** Tab5 ST7121, `2a05811`, 07/11/2026. **Evidence**: the temporary serial commands `tourner3` and `tourner0` (a task pinned to core 0 computes for 8 s): the watchdog fires 5.0 s after the start at priority 3 and does not fire at priority 0. **Status**: resolved.

---

### Alerts (Vigie)

#### A small Vigie answer is not gzip: the decoder refuses it

**Symptom.** With `Accept-Encoding: gzip` sent, Hamburg's answer (426 bytes, no alert) has no `Content-Encoding` and starts with `{`, not with the gzip bytes `1f 8b`: a client that always inflates rejects it as “invalid header”.

**Cause.** The public instance compresses only bodies of about 500 bytes or more (the 9 other real answers are gzip, chunked, 365 to 1 976 bytes on the wire).

**Solution.** `main/vigie_client.cpp` decides from the first two bytes (`gzip_flux::est_gzip`), never from the length or from the header alone; a plain body is read as it is. A body that is neither JSON nor valid gzip is refused and counted as a failure (back-off), never half-read.

**Seen on** PC only (host tests with AddressSanitizer + UBSan, PC render), branch `claude/alertes-vigie-9b13fe`, 2026-11-07, real answers of the public Vigie instance captured 2026-11-07 08:52–08:53 UTC. **Not seen on a board.** **Evidence**: `test/host/donnees/vigie_hambourg.json` (426 bytes, read by the core tests) and `vigie_captures.json` (`content_encoding: null`). **Status**: handled on the PC; the board has not received any answer yet.

#### Without a `lang` filter, one warning weighs 26 kB

**Symptom.** La Palma without filter: 26 190 bytes of JSON for 9 warnings; Eberswalde: 7 623 bytes for one.

**Cause.** A German or Spanish service gives its text in up to eight languages; Vigie returns them all unless it is told which to keep.

**Solution.** The request always carries `lang=<screen language>,en` (`lang=en` for English): 19 238 and 2 797 bytes. The issuer's words are not touched, only filtered by language. The receive and text buffers are 64 kB each, in PSRAM, freed after each attempt; an answer that does not fit is refused, not cut.

**Seen on** PC only (host tests with AddressSanitizer + UBSan, PC render), branch `claude/alertes-vigie-9b13fe`, 2026-11-07, real answers of the public Vigie instance captured 2026-11-07 08:52–08:53 UTC. **Not seen on a board.** **Evidence**: `vigie_la_palma_toutes_langues.json` (26 190) against `vigie_la_palma.json` (19 238). **Status**: handled on the PC.

#### The screen goes quiet for one second at the change of the hour

**Symptom.** When two alerts follow each other, the first ending at `hh:59:59` (AEMET) and the second starting at `hh:00:00` (DWD) or the reverse, nothing is “in force” for one second: the pill can disappear and come back.

**Cause.** The end of an alert is exclusive and issuers do not agree on the last second.

**Solution.** An end written at `hh:59:59` now counts as the last second of the alert (`fin_exclue_s` in `vigie_vue.hpp`, 07/11/2026); any other end stays exclusive. The core tests pin both (`test_vigie_alertes.cpp`: instant 1794085199 of the La Palma answer, and a dedicated test around `hh:59:59`, `hh:00:00` and `hh:39:59`). A hysteresis (keeping the pill 2 s) was not chosen: it would keep every alert 2 s after its end. Limit: an issuer writing `hh:59:59` as an exclusive end would keep its alert one second too long (not seen).

**Seen on** PC only (host tests with AddressSanitizer + UBSan, PC render), branch `claude/alertes-vigie-9b13fe`, 2026-11-07, real answers of the public Vigie instance captured 2026-11-07 08:52–08:53 UTC. **Not seen on a board.** **Status**: fixed in the core (PC tests only), never seen on a board, before or after.

#### An issuer text disappears from the screen

**Symptom.** A warning is shown with Vigie's own words (“Orange level, rain”) instead of the issuer's title, or a description is missing.

**Cause.** The core never rewrites an issuer's text: a text containing a character the embedded font (`main/assets/roboto_700.ttf`) cannot draw is dropped as a whole, and the next language is tried (screen language, then English, then the first text). The table of drawable characters (790) is checked against the font by `test/host/controle_python_vigie.py`.

**Solution.** None needed: this is the intended behaviour (a square box in the middle of a warning is worse than Vigie's words). If a real issuer's text is dropped for a character that should be drawn, add the character to the font and to `PLAGES_POLICE` in `vigie_alertes.hpp`, then rerun the tests.

**Seen on** PC only (host tests with AddressSanitizer + UBSan, PC render), branch `claude/alertes-vigie-9b13fe`, 2026-11-07, real answers of the public Vigie instance captured 2026-11-07 08:52–08:53 UTC. **Not seen on a board.** **Status**: by design.

#### An alert time is cut in two lines (“00:” then “59”)

**Symptom.** On the alerts page, the end of an alert written as “dimanche 8 novembre · 00:59” wrapped after the colon: “00:” on one line, “59” on the next (PC render, 720×1280, La Palma, French).

**Cause.** The text engine of the screen (LVGL) treats a colon as a place where a line may break, like a space, so a time at the end of a long line can be cut inside the number.

**Solution.** The time now comes first: “00:59 · dimanche 8 novembre” (`vigie::formater_instant`, `components/core/include/ecran/vigie_textes.hpp`). At the start of a line a time cannot be cut. Tested by `test_vigie_textes.cpp` (exact text in the five languages, the truncated buffers) and its mutations.

**Seen on** PC only (host tests with AddressSanitizer + UBSan, PC render), branch `claude/alertes-vigie-9b13fe`, 2026-11-07. **Not seen on a board.** **Status**: fixed on the PC render; to be checked by eye on the panel.

#### Hooks and tools: traps met while writing the alerts

- **`robocopy … /E` is refused** by the guard of the `E:` disk (false positive on the `/E` option): use `Copy-Item -Recurse`.
- **A mutation that no longer compiles is not a detection.** `-Werror,-Wunused-but-set-variable` turned `if (false)` into a compile error counted as “detected by something else”: the mutation list must show 0 such lines, so the mutation was rewritten (`if (n == 0 && b.autres > 100)`).
- **A new header's formatting is checked at commit time**, not before: run `clang-format -i --style=file <file>` on new C/C++ files before `git commit`, then check that the mutation anchors still appear once (formatting can re-wrap a line). The pre-commit hook was refused once on 14 differences in two new files.

**Seen on** the PC, branch `claude/alertes-vigie-9b13fe`, 2026-11-07.

---

### Wi-Fi

#### A wrong Wi-Fi password is almost never recognised as such: the radio says 2 and 205

**Symptom.** Trying a new network with a wrong password: the disconnect events carry reason **2** (`AUTH_EXPIRE`, “authentication expired”) and **205** (`CONNECTION_FAIL`, “connection failed”), each repeated, and the reason **202** (`AUTH_FAIL`, the one that means “password refused”) came in **1 trial out of 9** (28.9 s after the start); the 8 other times nothing but 2 and 205 until the 30 s limit. A rule “the same sure cause twice” never fires, and the result given to the screen is “other”.

**Cause.** `reseau.cpp` retries the connection 2 s after every disconnect (`kDelaiReessaiWifiUs`) and each attempt ends with a reason code chosen by the radio; on this access point the authentication failure only comes after several attempts. Which codes appear for which fault depends on the access point: **not verified on another one**.

**Solution.** `essai_wifi.hpp`: only *sure* causes (14, 15, 202, 204 = password refused; 201, 210, 211, 212 = network not found) count; two of the same one end the trial early; any other reason, including our own disconnect (8), is neither proof nor denial; at the 30 s limit, the last sure cause seen is the one reported. Consequence for the screen: an answer found only at the 30 s limit reaches it about **36 s** after the request (30 s trial + about 6 s for the old network to come back), later than its own 30 s counter (`DELAI_CONNEXION_MS`), which has by then shown its own “no answer” failure and ignores a late result (`sur_resultat`, `changement_wifi.hpp`). So with this access point the screen shows its generic “no answer” message for a wrong password; to be decided on the screen's side (a “going back to the old network” state that waits for the answer, or a trial limit shorter than the screen's). Classing repeated 2 and 205 as “password refused” is one line in `resultat_pour_raison`, **not done**: measured on one access point only, and the documentation of ESP-IDF 5.5.5 does not tie these two codes to the password.

**Seen on** Tab5 ST7121, commit `fe54f8d`, 07/11/2026 (first series on `5f17506`). **Evidence**: log of 9 trials with a wrong password on the author's own network (7 with an old network: every time the old network back with its IP address 6.0 to 6.15 s after the decision; 2 without); the two sequences are replayed as tests (`serie_relevee_sur_la_carte_mot_de_passe_faux`, `…_sans_202_dit_autre`); details in [mesures/wifi-retour-arriere.md](mesures/wifi-retour-arriere.md). **Status**: worked around (roll-back reliable; specific cause unavailable on this access point).

#### With no known network, the Wi-Fi keeps retrying a connection every 2 s after a scan

**Symptom.** New device (nothing in the NVS, nothing compiled in): after the first network scan the log shows `Wi-Fi coupé (raison 201 …) ; nouvelle tentative dans 2 s` forever (11 times in 45 s), the radio is busy, and the next scan can fail.

**Cause.** `terminer_recherche()` restarted the connection retries whenever the Wi-Fi was not connected, with no network in service to find again (empty configuration → “no access point found”, reason 201).

**Solution.** `reconnexion_utile()` in `main/reseau.cpp`: retry only if there is a network in service or a trial in progress (`06b7afe`). The trial's own retries are unchanged.

**Seen on** Tab5 ST7121, local binary that ignores the compiled network (not committed), 07/11/2026. **Evidence**: before, 11 disconnects in 45 s after the scan; after, none between the scan and the next trial, a failed trial reported at +30.01 s with no retry afterwards, a good network adopted in 6.1 s. The normal path (with a known network) was replayed on the final binary `a244af0`. **Status**: resolved.
### Audio, expanders and power

#### Reading the microphones alone on the I2S port times out

**Symptom.** With only the receive channel of the shared I2S port enabled, every read ended on `ESP_ERR_TIMEOUT` (first bench pass of 07/11/2026): no microphone level, no recording.
**Cause.** **Not elucidated.** The same port read fine as soon as the transmit channel was also enabled. Hypothesis, **not verified**: the port's shared clocks do not run for a receive-only channel in this configuration.
**Solution.** `tab5_i2s_enable()` enables the transmit channel whenever the receive one is enabled; it then sends silence (the DAC is muted, the amplifier off). No audible effect. Do not “simplify” this away without re-running the bench.
**Seen on** Tab5 ST7121, 07/11/2026; fix in `993500f`. **Evidence**: bench lines `audio.micro.bruit` and `audio.enregistrement` OK at `5f797e3`. **Status**: worked around (cause open).

#### Expander: “can't set level in input mode”

**Symptom.** Driving `CHG_EN` or `nCHG_QC_EN` (expander 0x44) failed with `can't set level in input mode` after the pin had been read.
**Cause.** After a reset every PI4IOE5V6408 pin is an input, and reading a pin as input leaves its cached direction on “input”; the `esp_io_expander` driver then refuses to set a level.
**Solution.** In this order: direction (output), level, push-pull mode (`tab5_expander_output()`). The pin takes the output register's value (0 after a reset) for an instant: harmless for `SPEAKER_EN` and `CHG_EN`; `nCHG_QC_EN` is not driven when it is already high.
**Seen on** Tab5 ST7121, 07/11/2026; fix in `993500f`. **Evidence**: driver message in the first bench pass, then `tab5_alim` lines without error at every start. **Status**: resolved.

#### A pin driven as an output reads 0 in the expander's state register

**Symptom.** `SPEAKER_EN` read back as 0 while the speaker was playing (bench check `audio.ampli_allume` failed three times, once per DAC line, in the passes before `5f797e3`).
**Cause.** The state register (0x0F) does not show a pin driven as an output (seen on the board; **not verified on the chip's datasheet**). The registers told the truth: during the tone expander 0x43 read direction 0x6F, output 0x22, high impedance 0xDD, so bit 1 is an output, high, not in high impedance.
**Solution.** To check an output, read direction (0x03), output (0x05) and high impedance (0x07) (`tab5_expander_dump()`); the bench check does so since `5f797e3`.
**Seen on** Tab5 ST7121, 07/11/2026. **Evidence**: register dump during the tone, 3 lines, and the check passing 3 out of 3 at `5f797e3`. **Status**: resolved (the check was wrong, not the board).

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

#### LVGL 9.5 screens: three pitfalls found while building the settings screens

**Symptom → cause → solution.**
- A disabled button (the "Continuer" of step 1 before a language is touched) showed the **theme's grey** instead of the palette's disabled look → the default theme styles `LV_STATE_DISABLED` on top of the object's own style → the disabled look is drawn by hand (`bouton_activer(false)`, `main/ui_commun.cpp`: own colours, clickable flag removed).
- Testing "is it a button?" with `LV_OBJ_FLAG_CLICKABLE` also matched plain containers → `lv_obj_create()` objects are clickable by default in LVGL 9.5 → a button is recognised with `lv_obj_check_type(o, &lv_button_class)`.
- A screen deleted from one of its own event callbacks, or a keyboard/field replaced while its `DELETE` callback still points to it, left a stale pointer → objects are hidden then deleted with `lv_obj_delete_async()`, and the `DELETE` callbacks only clear the global if the deleted object is the one it holds.

**Seen on** PC rendering bench (software LVGL 9.5.0, `tools/rendre.ps1`), branch `claude/reglages-ecran-aea4e2`, 06/11/2026. **Evidence**: rendered boards (disabled button) and the scripted journeys of `tools/rendu/scenes/scene_parcours.cpp` (a disabled "Continuer" does nothing; the keyboard and fields survive a screen change). **Status**: resolved on the PC; not seen on a board.

#### LVGL 9.5 touch targets: slider zone, parents, scroll indicator

**Symptom → cause → solution.**
- Touch targets measured below the 72 px minimum (Retour 60, "Nouveau code" and "Tester" 60, preset pills 64, "Quitter l'installation" 56) and a brightness slider that could only be grabbed on a 22 px bar → buttons had their height overridden after `creer_bouton()`; for the slider, LVGL 9.5 accepts a touch only on the bar widened by `LV_DPX(8)` (`lv_slider.c:250` sets it, `lv_obj_pos.c:1195-1205` reads it), and the "knob only" test of `lv_slider.c:269-277` runs only when `LV_OBJ_FLAG_ADV_HITTEST` is set, which the slider does not do (only `lv_image.c:694` sets it) → the overrides are gone, `creer_bouton` sets a `min_height` that no caller can undercut, and the slider bar is widened with `lv_obj_set_ext_click_area()` up to 72 px high (`rangee_curseur`, `main/ui_reglages.cpp`).
- The widened slider zone did not answer near its top and bottom edges → LVGL searches a parent's children only if the point is inside the parent's own limits (`lv_indev.c:636`) → the slider sits in a row exactly as tall as its touch zone.
- A page cut off at its edge with no sign that there is more below → the scrollbar was `LV_SCROLLBAR_MODE_OFF` → `style_defilement()` (`main/ui_commun.cpp`) shows a bar (`LV_SCROLLBAR_MODE_AUTO`) as long as the content overflows.
- A render check that flagged an empty `flex_grow` text as an "overflowing container" → an empty label reports one line of content in a 0 px box → the check only looks at plain containers (`lv_obj_class`).
- **Still open**: keyboard keys are 59 px tall at 1280×720 (49 px at 1024×600) in the languages with an accent row (fr, nl, es, it) → six rows cannot exceed 55 % of the screen in landscape; with five rows (en, de, tr) they are exactly 72 px.

**Seen on** PC rendering bench (software LVGL 9.5.0, `tools/rendre.ps1`), branch `claude/reglages-ecran-aea4e2`, 06/11/2026. **Evidence**: scripted journeys `zones-tactiles-reglages`, `zones-tactiles-installation` and `curseur-au-doigt` (`tools/rendu/scenes/scene_parcours.cpp`; the last one uses a pointer input device and `lv_indev_search_obj`, so the touch goes through LVGL's own search) in 7 languages × 3 sizes; four of five deliberately introduced faults are detected (the fifth, a button forced back to 56 px, is neutralised by `min_height`). **Status**: resolved on the PC for buttons, slider and scroll indicator; the keyboard keys are an open decision; not seen on a board (real finger, physical size of the target: not verified).

#### On-board test: the virtual finger lands on the wrong object

**Symptom.** In the first two runs of the settings test on the board, a tap on the centre of "Affichage" opened the "Appareil" page, "Retour" did nothing, and the aimed button received no press, no release and no click (counts 0, 0, 0), although `lv_indev_search_obj()` found that button at that very point.

**Cause.** `lv_indev.c:743` calls `lv_display_rotate_point()` on every point a pointer reads, because touch drivers give **raw** panel coordinates (`components/bsp_tab5/tab5_touch.c` says so). The test's virtual pointer gave logical coordinates, so the point was rotated twice: (639, 424) became (855, 639), inside the "Appareil" row; (86, 43) became (1236, 86), on nothing. A `grep rotation` on `lv_indev.c` finds nothing: the function is called `rotate_point`.

**Solution.** `vers_brut()` in `main/autotest_reglages.cpp` gives the inverse rotation (90, 180, 270); after every press the test compares the point LVGL finally read with the aimed point and records a failure if they differ, so a test that misses cannot pass again. The screens were right; the test was wrong. **Seen on** Tab5 ST7121, commits `7b435eb` and `8f6c28d` (failing) and `176de0d` (fixed), 07/11/2026. **Evidence**: journal lines `GESTE … appui reçu 0, relâchement reçu 0, clic reçu 0` then `1, 1, 1`; 29 checks, 0 failures at `176de0d`. **Status**: resolved (in the test).

#### Internal RAM down to 20 bytes while the settings are open, and the weather failing

**Symptom.** With the settings open about 13 to 18 s after boot: `MEMOIRE … interne_libre=535 plus_gros_bloc=160 interne_min=20` and `mdns_send: Cannot allocate memory`. On the **normal** binary, settings never opened: `esp-aes: Failed to allocate memory for the array of DMA descriptors` then `meteo: essai raté (HTTP 0)`.

**Cause (a reading, partly measured).** The weather's TLS handshake (about 65 to 76 KB of internal RAM) starts about 13 s after boot, with 96.8 KB free just before. The settings list takes about 21.5 KB of internal RAM (8.8 KB of objects and styles, 12.6 KB at the first draw) and about 22 KB of PSRAM once the glyph bitmaps go there (`e686310`). Together they empty the internal heap. My first reading, "the list alone takes 28 KB", came from a measure taken while the TLS was already running.

**Solution.** Not done: TLS, `sdkconfig` and LVGL's allocation are not on the settings track. Proposed: mbedTLS in PSRAM (`4f6bd0a`, `jalon-1b` track), then measure again with the same `MEMOIRE` lines; if still too tight, put LVGL objects in PSRAM (custom allocation) or lower `SPIRAM_MALLOC_ALWAYSINTERNAL`. **Seen on** Tab5 ST7121, commits `8f6c28d` and `176de0d`, 07/11/2026. **Evidence**: `MEMOIRE` lines of the test journal and the `meteo` log lines ([mesures/6-reglages-appareil.md](mesures/6-reglages-appareil.md), section 4.3). **Status**: open (no crash or unexpected restart seen; zero margin).

#### A mutation keeps the compiler busy for minutes

**Cause.** The proof by mutation compiles the tests with one change in the core. A mutant that removes the exit condition of a `constexpr` loop (here `&& base < PERIODE_S` in `delai_avant_essai_s`) makes clang evaluate the loop up to its limit of 2 billion steps (`-fconstexpr-steps=2000000000`, set so that the tests which read a real response compile): 765 s of CPU measured before the compiler was stopped.

**Solution.** That mutant is left out of `test/host/mutations_meteo_appareil.py` (the reason is written at the top of the file); the real code leaves the loop after 7 turns, and `planning_normal_et_reessais` covers 1 000 000 and `INT_MAX` failures. When writing a mutation, check that it does not remove the end of a loop. **Seen on** the author's PC, 06/11/2026, clang 23.1.5. **Evidence**: process CPU time, mutant no. 32 of the list. **Status**: worked around.

#### Open-Meteo: the rain, probability and UV of an hour show up one hour (or one quarter of an hour) early

**Symptom.** Reading `hourly.precipitation[i]` next to `hourly.temperature_2m[i]` draws the rain of the hour `i` with the value that belongs to the hour before it; the same for `precipitation_probability`, `uv_index` and for `minutely_15.precipitation` (a quarter of an hour).

**Cause.** Open-Meteo labels a sum or an average by the END of its interval (“preceding hour”), unlike instantaneous values (temperature, wind, code), which are at the label. The data do not say it; the documentation does.

**Solution.** The core shifts by one label (`heures[i]`: instantaneous values at label `i`, sums at label `i + 1`; the quarters start at the first label minus 900 s) and the request asks for 25 hourly labels and 9 quarter-hour labels to keep 24 complete hours. Tests compare the result with values computed independently by Python on seven real responses.

**Seen on** the author's PC, real responses captured on 2026-11-07 at 00:12 (Paris time), branch `claude/meteo-appareil-f761d6`, no board. **Evidence**: Seattle (the only capture with rain): label +3,600 s = 1.0 mm = the four quarters labelled +900 to +3,600 s (0.3 + 0.3 + 0.2 + 0.2), the other convention would give 0.4; label +7,200 s = 0.2 mm = 0.1 + 0.0 + 0.1 + 0.0. **Status**: resolved in the core; not seen on a board.

#### Open-Meteo: in a time zone that is not on the hour (Newfoundland, Nepal) the series do not start at the current hour

**Symptom.** With `timezone=auto`, the hourly and quarter-hour series begin 45 minutes before `current.time` in Kathmandu (UTC+5:45) and 30 minutes after it in Newfoundland (UTC-3:30), but exactly at it in Lyon, Sydney or Singapore. A screen that looks for “the hour that contains now” finds nothing at the start in Newfoundland.

**Cause.** The series start at the current instant rounded to the UTC hour, minus the remainder of `utc_offset_seconds` modulo 3,600 (sign of C): +20,700 s gives -2,700 s, -12,600 s gives +1,800 s. Measured on two captures; the same rule is expected for +5:30, +3:30, +10:30 (not captured).

**Solution.** `heure_courante`, `quart_courant` and the “rain soon” block are recomputed against the device clock; when no hour contains it the index is -1 and `courant.proba_precip_pct` is `METEO_PROBA_INCONNUE`, never the probability of the neighbouring hour. The contract says so (`main/meteo_etat.h`) and tests cover both zones.

**Seen on** the author's PC, captures of 2026-11-07 00:12 (Paris time), no board. **Evidence**: `hourly.time[0] - current.time` = -2,700 s (Kathmandu), +1,800 s (Newfoundland), 0 (Lyon, Sydney, Singapore). **Status**: resolved in the core; the screen must accept `heure_courante == -1`.
### Main screen

#### No time and a near-black sky for the first 13 seconds after a start

**Symptom.** After a start the main screen shows no time and a night sky until the first SNTP answer (about 13 s). The author: “the blue background takes a while to appear”.

**Cause.** `ecran_principal.cpp` took the time only once SNTP had answered, although the system time was already right (the board layer resumes it from the RX8130 at 2.6 s after power-up, or it is kept across a software reset). In addition, as long as the weather was not read, the sky judged day and night in UTC (one or two hours of night too many).

**Solution.** The core rule `rtc::heure_affichable(reseau_synchronise, systeme_s)` (the network gave the time OR the system time is plausible; tested, 2 mutations) and `decalage_secours_s`, the clock's offset passed to the screen (commit `a740935`).

**Seen on** Tab5 ST7121, integrated build `953328d`, 07/11/2026. **Evidence**: log: “09:01” and the date 3.4 s after a restart instead of about 13 s; the sky itself was not looked at by me; a start after a power cut (hardware clock read) not replayed. **Status**: resolved in the log; to confirm by eye.

#### The sky gradient shows visible bands

**Symptom.** On a sky that goes from dark blue to a lighter blue, steps of colour show across the screen.

**Cause.** The panel takes RGB565 (32 or 64 levels per channel) and LVGL 9.5 has no dithering; its gradient (at most two stops here, `LV_GRADIENT_MAX_STOPS 2`) is drawn straight into that format, so each level becomes a band.

**Solution.** The sky is an RGB565 image built with an ordered 8 × 8 (Bayer) dither and set as the background image of the screen (`remplir_ciel_trame` and `appliquer_ciel` in `main/ui_principal.cpp`). It is rebuilt only when the moment of the day or the weather class changes, and its 1,843,200 bytes (1280 × 720) live in PSRAM. If the allocation fails, the screen falls back on LVGL's own vertical gradient (bands, but a working screen).
- **Seen on**: PC render (LVGL 9.5.0, clang), branch `claude/ecran-visuel-ba722a`, 2026-11-07; and a Tab5 (ST7121 screen), commit `cd362eb`, 2026-11-07 (serial log only, see the next entry).
- **Evidence**: renders before and after (the bands vanish). On the board: the screen is built (« 9 objets LVGL, 7 polices »), free PSRAM is 1.8 MB lower than with the old screen (compatible with the 1,843,200-byte sky, not isolated), no allocation error in the log; the first 10 s draw 3 images at 215 ms on average, then a minute change redraws one image of 10 zones in 82 ms (the old screen: about 24 ms). **Not verified**: the colours and the bands on the panel, by eye.
- **Status**: *resolved on the PC render*; *on the board: allocated and drawn without error, not looked at by eye*.

#### The board reboots about 14 s after start, at the first weather request

**Symptom.** With the new main screen, the Tab5 reboots in a loop: the screen is built (« écran principal 1280x720 chargé »), then, at about 13.7 s, the log prints `abort() was called at PC …` and « Rebooting… », and the next boot reports « plantage (exception) » (code 4). The old main screen, built on the same base commit, does not do it.

**Cause (probable, not isolated allocation by allocation).** Internal RAM runs out when the first weather request (TLS) and the web server start together. Free internal RAM just before that request: 109.2 KB with the old screen, 44.9 to 45.6 KB with the new one, on the same base (`4e7d8af`). ESP-IDF's default `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` keeps every allocation under 16 KB in internal RAM; the new screen draws much larger digits (the clock and the temperature) than the old one. The two `abort()` call sites, decoded with the ELF, are `lock_init_generic` (`newlib/locks.c:77`) and `esp_aes_intr_alloc` (`mbedtls/port/aes/dma/esp_aes_dma_core.c:117`): both abort when an internal allocation fails.

**Solution.** The memory settings of jalon 1b (commit `4f6bd0a`, branch `claude/jalon-1b-wifi-c2232d`, file `boards/tab5/sdkconfig.defaults.board`): `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256` and `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`. They are **not** in the branch of the screen (they are another session's file and arrive with the merge). Measured with those two lines added in a separate build directory, never committed: 154.4 KB of internal RAM free before the first request, 105.9 KB at 154 s (minimum since start 96.0 KB), no abort over 156 s.
- **Seen on**: Tab5, ST7121 screen (MAC read on the chip before writing, not copied here), ESP32-P4, commit `cd362eb` over `4e7d8af`, 2026-11-07 from 01:50 to 02:02; esptool 5.3.1, application only at `0x10000`; binaries `061A3E69…0A28` (without the settings) and `F317FCFD…FFCB` (with them).
- **Evidence**: serial logs, counted from the files. The new screen without the settings: 9 boots seen, 6 aborts at about 13.7 s, 1 boot that lived 58 s with 38 KB free and a weather request refused by mbedTLS for lack of memory, 2 boots cut by the end of the capture before 13 s. With the settings: 2 boots, 0 aborts (70 s and 156 s). The old screen on the same base: 2 boots, 0 aborts (30 s and 66 s).
- **Status**: *solved with the settings of jalon 1b*; *not committed on this branch*; to check again after the merge. The base itself is fragile: with the old screen, the minimum of free internal RAM since start was 1.7 to 1.8 KB on that base.

#### Hourly strip, pressure trend and rain amounts are absent on the device

**Symptom.** The main screen shows the weather now and the days, but neither the hourly strip, nor the pressure trend, nor the amount of rain, nor « rain in N min ».

**Cause.** The device's weather client (`meteo_client.cpp`, session `meteo-appareil`) does not request or keep those fields yet; the screen shows only what is received and never invents a value (the zone disappears and the others take its place).

**Solution.** A request listing the fields (hourly temperature, code and rain probability; pressure at three hours; rain amount; minutely precipitation) was written for the session that owns the client, in the shared notes folder (`ecran-visuel_besoins-meteo.md`). The owner published a contract (`MeteoDetail`, commit `f9537c5` of its branch) and, according to its message at 02:10 on 2026-11-07, committed the implementation and the pressure trend (commits `4489127` and `8f31fa0`; PC tests green and firmware compiled, never run on a board; not merged here). An adapter from that contract to the screen's view, tested on the PC against the real header, is in the shared notes folder (`ecran-visuel_branchement-meteo-v2.md`); it is not in this branch because it includes the owner's header.
- **Seen on**: a Tab5 (commit `cd362eb`, 2026-11-07): the screen shows the weather and the days, without those zones. The PC render shows the same with the variant `donnees-minimales`.
- **Status**: *open* (waiting for the client's implementation and for the adapter to be placed at the merge).

#### Turkish: Sunday and Monday had the same abbreviation

**Symptom.** In the week, both « Pazar » (Sunday) and « Pazartesi » (Monday) were written « Paz ».

**Cause.** The first three letters of the two names are the same.

**Solution.** The abbreviations are a table per language (`JOURS_COURTS` in `components/core/include/ecran/visuel_dates.hpp`), not a cut of the name; the test checks that the seven abbreviations of every language are distinct (« Paz » for Sunday, « Pzt » for Monday).
- **Seen on**: PC render, `fr de tr`, 2026-11-07. **Status**: *resolved*.

#### The render refuses to start: « a build is already running (ninja) »

`tools
endre.ps1` stops when `ninja` or `esphome` runs, so as not to put a second heavy compilation on the PC. Wait for the other session's build to end (the `build` lock of `verrous.py`), or, if you know what you are doing, `-Forcer`.

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
- [Mémoire et chien de garde](#mémoire-et-chien-de-garde)
- [Wi-Fi](#wi-fi)
- [Alertes (Vigie)](#alertes-vigie)
- [Audio, expandeurs et alimentation](#audio-expandeurs-et-alimentation)
- [Page de réglages et noyau](#page-de-réglages-et-noyau)
- [Écran principal](#écran-principal)

---

### Ouverts

#### Après une coupure de courant, l'image n'apparaît qu'après un second geste

**Symptôme.** Coupure de courant du Tab5, puis remise : le premier démarrage ne montre pas d'image ; l'image « Bonjour » et la mire s'affichent après un second geste de l'auteur (démarrage n° 9 propre, ST7121 détecté). Le premier démarrage « sans écran » **n'est pas dans le journal série** : le compteur de démarrages passe de 8 à 9.

**Cause.** **Non trouvée** au 06/11/2026. Le journal ne contient qu'un démarrage après la coupure (n° 9), complet et propre : ST7121 détecté au 1er sondage, première image, rétroéclairage à 75 % à 1598 ms. Donc soit le premier démarrage n'a pas lancé ce firmware, soit il n'a pas été capturé (tablette sur batterie, sans port série). Hypothèses **non vérifiées** : l'alimentation USB du Tab5 n'allume peut-être pas la tablette sans appui sur le bouton ; le reset de l'écran par l'expandeur (qui a besoin d'une seconde, par analogie avec ESPHome) ne semble pas en cause puisque le n° 9 est propre. La cause lue du rallumage est « chien de garde » (`HP_SYS_HP_WDT_RESET`) aux **deux** remises sous tension observées : peut-être la cause normale d'un rallumage de cette carte, non vérifié, aucun plantage (`Guru`, `abort`, `Backtrace`) vu.

**Solution.** Aucune appliquée. Pour l'utilisateur : après une coupure complète, un appui court sur le bouton allume la mire. Le critère de 1a (« image après redémarrage **et** après coupure ») est donc constaté **avec réserve** ; l'essai pas à pas (rebrancher l'USB sans toucher aucun bouton, attendre 30 s, noter ce qui s'affiche, port série ouvert) est à refaire au jalon 1b avant d'attribuer la cause au firmware ou à la carte. L'auteur a choisi de ne pas le refaire pendant 1a.

**Vu sur** Tab5 ST7121, firmware natif commit `e7a4803`, 06/11/2026. **Preuve** : journal du démarrage n° 9, constat de l'auteur ; détail dans [mesures/1a.md](mesures/1a.md). **Statut** : ouvert. Jalon 1b (06/11/2026) : l'essai pas à pas a été **reporté par l'auteur** (« Pas maintenant », 20 h 59), ni fait ni raté ; voir [mesures/1b.md](mesures/1b.md).

**Mise à jour du 06/11/2026, 22 h 27 (même famille de panne, reproduite AVEC journal).** *Vu sur* Tab5 ST7121, binaire du commit `c1fa7f6`, démarrage n° 65, juste après un flash (remise à zéro par le signal RTS d'esptool, **pas** une coupure de courant) : `pi4ioe5v6408: reset(252): Chip reset failed` → `Reset failed` → `tab5_io_init : expandeur 0x43` → `board_init : E/S` → `carte non initialisée ; arrêt (l'écran reste éteint)`. L'expandeur 0x43 n'a pas répondu à la toute première transaction I2C ; le firmware n'a fait **qu'un** essai et s'est arrêté, **écran noir** (Axel : « écran noir ») ; un nouveau flash a rendu l'image. **Hypothèse, non vérifiée** : c'est aussi ce qui s'est passé au premier démarrage « sans écran » après la coupure (n° 8 → 9, non capturé). **Correctif** (`tab5_io.c`) : jusqu'à 5 essais espacés de 150 ms avec remise à zéro du bus I2C entre deux, une ligne de journal par essai, puis au plus 3 redémarrages de reprise (compteur RTC) avant un arrêt avec un message lisible. Essayé sur la carte avec des échecs **simulés** (binaires d'essai) : 2 échecs puis succès à l'essai 3/5 ; 5 échecs répétés → 3 redémarrages (`SW_SYS_RESET`, compteur conservé) puis arrêt. La **vraie** panne n'a pas été reproduite depuis : le correctif n'est pas prouvé contre elle. **Séries de 20 remises à zéro** (06/11/2026, 22 h 49 à 23 h 00, Tab5 ST7121, remise à zéro par RTS comme esptool avec un port neuf à chaque tour ; un démarrage ne compte que si un nouveau numéro de démarrage et la ligne de version apparaissent) : **avant** (`c1fa7f6`, un seul essai) démarrages n° 99 à 118, **20/20** « Carte prête » + écran principal chargé dans le journal, 0 « Chip reset failed » ; **après** (`9f82623`, avec reprise) n° 78 à 97, **20/20**, 0 ligne « expandeur 0x43 : essai » (l'expandeur répond du premier essai à chaque fois), 0 « Chip reset failed » ; les démarrages qui suivent chaque flash (n° 98 et n° 119) sont corrects aussi. La panne du n° 65 n'est donc **pas reproduite** et la reprise n'est **pas prouvée** par ce test (0/21 échec avant, 0/21 après) ; il montre seulement qu'elle ne ralentit pas un démarrage normal (3,03 à 3,08 s jusqu'à « Carte prête »). « Image revenue » = lignes du journal : seul Axel a vu l'écran. **À ne pas confondre** : `ESP_ERR_TIMEOUT` sur l'expandeur = un échec **simulé** par un binaire d'essai (`TAB5_AUTOTEST_EXPANDEUR`, démarrages n° 67 à 72, version affichée `g3b68f4` car compilé sur un arbre sale) ; `Chip reset failed` / `ESP_ERR_INVALID_STATE` = la **vraie** panne (n° 65). **Statut** : toujours ouvert (cause de la vraie panne inconnue ; reprise ajoutée, non prouvée).

#### Retour à ESPHome jamais essayé

La recette de remise en état (trois écritures : début de flash, application, NVS) est dans le `LISEZMOI.md` de la sauvegarde locale de l'auteur, hors de ce dépôt. Elle n'a **jamais été jouée** : tant qu'elle ne l'est pas, le retour est « non vérifié ». **Statut** : ouvert.

#### Le connecteur de batterie lit 8,2 V alors que l'exemplaire « n'a pas de batterie »

**Symptôme.** Le 07/11/2026 l'INA226 lit **8206 mV et +2 mA** au connecteur de batterie, chargeur forcé à l'arrêt, de façon stable pendant tout le banc. Les notes héritées du firmware ESPHome disent « l'exemplaire d'Axel n'a pas de batterie » (1,83 à 1,94 V chargeur coupé).
**Cause.** **Non trouvée.** Compatible avec un bloc lithium à deux éléments presque plein au repos (une batterie branchée depuis), pas avec un connecteur vide. Rien dans le logiciel ne permet de trancher.
**Suite.** Regarder le dos de l'appareil (contrôle physique). Le firmware n'allume jamais le chargeur : rien n'est donc chargé par lui d'ici là ; l'application ne doit pas déduire un état de batterie de cette lecture.
**Vu sur** Tab5 ST7121, commit `5f797e3`, 07/11/2026 (voir [mesures/tab5-materiel.md](mesures/tab5-materiel.md)). **Preuve** : lignes de journal `tab5_alim: INA226 tel que trouvé … (8206 mV)` à chaque démarrage et `alim.lecture` du banc. **Statut** : ouvert.

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

#### Dans un worktree git, CMake échoue : `grabRef.cmake` « file failed to open head-ref »

**Symptôme.** Dans un worktree créé par `git worktree add` (un par session), `idf.py set-target` ou la première compilation s'arrête dans `grabRef.cmake` (`file failed to open head-ref`), alors que le dépôt principal se compile.

**Cause.** ESP-IDF lit la référence de la branche comme un fichier `<dossier git>/refs/heads/<branche>` ; une branche qui n'existe que dans `packed-refs` (pas de fichier) n'en a pas. La version (`git describe`) ne peut alors pas être calculée.

**Solution.** Créer la référence en fichier avec le même commit : écrire l'empreinte du commit dans le fichier `<dossier git du dépôt du worktree>/refs/heads/<branche>` (un `git update-ref` seul ne l'a pas créé). Autre point : un binaire compilé avec des fichiers suivis modifiés porte `-dirty` dans sa version (`project_version` de `build/project_description.json`) : commiter d'abord, puis compiler le binaire que l'on remet.

**Vu sur** PC de l'auteur, worktree `claude/ecran-principal-f0f722`, 06/11/2026. **Preuve** : la compilation échouait avec ce message, puis réussissait après la création du fichier (version `jalon-1a-tab5-st7121-49-g6a0e6cc`, sans `-dirty`). **Statut** : contourné (cause lue dans le script CMake, pas creusée davantage).

#### La compilation s'arrête sur `-Werror=format-truncation` dans l'interface

**Cause.** La compilation d'ESP-IDF utilise `-Werror=all` : un `snprintf` dans un tampon que le compilateur ne peut pas prouver assez grand (ici 40 octets pour « min %s · max %s » avec deux textes de 15 octets) est une erreur, pas un avertissement.

**Solution.** Un tampon dimensionné pour le pire cas du format (48 octets ici). **Vu sur** ESP-IDF 5.5.5, 06/11/2026. **Statut** : résolu.

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

#### `verrous.py` échoue avec « No module named 'filelock' » après une compilation

**Cause.** `tools/idf.ps1` met le Python de l'environnement ESP-IDF en tête du `PATH` pour la suite de la session PowerShell (lu dans le script). Le script de verrous qui dit aux sessions qui compile ou flashe (il vit hors du dépôt) a besoin de `filelock`, que ce Python n'a pas. Prendre le verrou avant la compilation marche ; le rendre après, dans la même session, échoue et laisse le verrou tenu.

**Solution.** Rendre le verrou depuis un autre appel PowerShell (ou avec le Python du PC, pas celui d'ESP-IDF) et vérifier avec la commande `etat` qu'il est libre. **Vu sur** PC de l'auteur, 06/11/2026, commit `793bdfc`. **Preuve** : la trace de l'appel `rendre` après `.\tools\idf.ps1 build` ; le même appel depuis un appel neuf a marché. Revu le 07/11/2026 (branche `claude/source-meteo-client-reseau-29c0af`) : un script de compilation a pris le verrou `build`, lancé `tools\idf.ps1` deux fois, et son `rendre` final a échoué de la même façon, laissant le verrou tenu alors que les deux compilations étaient passées ; vérifié dans un appel neuf que `python` est celui du PC avant `idf.ps1` et `<dossier ESP-IDF>\penvs\5.5.5\Scripts\python.exe` après, puis verrou rendu avec le Python du PC. **Statut** : contourné.

#### Le wrapper partagé `clang++-san.cmd` échoue : « 'clang++' n’est pas reconnu en tant que commande interne ou externe »

**Symptôme.** Après `. <dossier des outils du PC>\env_hote.ps1 -Sanitizers`, chaque test de `python test/host/lancer.py` échoue à la compilation et la sortie dit `'clang++' n’est pas reconnu en tant que commande interne ou externe, un programme exécutable ou un fichier de commandes`, alors que `clang++.exe` est bien dans `_outils\llvm-mingw\bin`.

**Cause.** `<dossier des outils du PC>\clang++-san.cmd` (le `CXX` que choisit `-Sanitizers`) est un fichier de commandes enregistré avec des fins de ligne **LF seules (aucun CR)** et des lettres accentuées dans sa ligne `rem` : `cmd.exe` le découpe aux mauvais endroits et exécute un morceau comme une commande. Ce fichier ne fait pas partie de ce dépôt.

**Solution.** Contournée, pas corrigée : une copie locale de trois lignes, en ASCII seulement, fins de ligne CRLF (`@echo off`, un `rem` sans accent, puis `"<dossier des outils du PC>\llvm-mingw\bin\clang++.exe" -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer %*`), et `$env:CXX` pointé dessus après `env_hote.ps1`. Correction durable, dans le dossier des outils (décision d'Axel, non touché ici) : réécrire ce fichier avec des fins de ligne CRLF et sans accent.

**Vu sur** le PC de l'auteur (Windows 11), branche `claude/source-meteo-client-reseau-29c0af`, 07/11/2026 ; aucune carte en jeu. **Preuve** : le wrapper partagé lancé à la main sur un programme vide affiche le message ci-dessus ; le fichier fait 265 octets, 0 CR, 3 LF, 6 octets au-dessus de 127 ; avec la copie locale, `python test/host/lancer.py` affiche « complet (compilé + exécuté) ».

#### `idf.py set-target` échoue au téléchargement des composants : « No such file or directory » sur un chemin très long

**Symptôme.** Dans un nouveau worktree git, `.\tools\idf.ps1 set-target esp32p4` s'arrête pendant le téléchargement des composants sur une `FileNotFoundError: [Errno 2] No such file or directory: '…\.cache\composants\service_…\espressif__esp_hosted_2.12.12_…\examples\host_network_split__power_save\…\host_wakeup_demo_using_udp_packet.c'` de Python. Un `build/` à moitié écrit reste, et le `set-target` suivant s'arrête sur « doesn't seem to be a CMake build directory ».

**Cause.** La limite de 260 caractères des chemins Windows (`LongPathsEnabled` vaut 0 sur ce PC) rencontre le cache de composants que `tools/idf.ps1` place dans le projet (`<worktree>\.cache\composants`). Le plus long fichier d'`esp_hosted` 2.12.12 est à 184 caractères sous la racine du worktree ; avec le dossier `reglages-appareil-b3aad6` (racine de 76 caractères) son chemin fait exactement 260 caractères. Les autres worktrees, aux noms plus courts de 2 à 5 caractères, tombent à 251–258 et passent : **un nom de dossier de worktree de 24 caractères ou plus échoue** ici. *Correction (07/11/2026) :* le plus long fichier est en fait `…\host_wakeup_demo_using_udp_packet.sh`, un caractère de plus que le `.c` cité plus haut (185 caractères sous la racine), donc un nom de dossier de **23 caractères ou plus** échoue (`agent-a4c19a135e3c33e73` : racine de 75 caractères, chemin de 260 exactement).

**Solution.** Contournée, pas corrigée dans le dépôt : supprimer le `build/` à moitié écrit et `sdkconfig*`, puis lancer la compilation avec une copie privée de `tools/idf.ps1` (non commitée) où `IDF_COMPONENT_CACHE_PATH` désigne un dossier court hors du dépôt (ici `%USERPROFILE%\.tab5\cache-composants`) ; tout le reste est identique. Pistes durables **non faites** : faire respecter par `tools/idf.ps1` un `IDF_COMPONENT_CACHE_PATH` déjà défini (une ligne), ou donner des noms plus courts aux worktrees. Activer les chemins longs de Windows est un réglage système : non touché. Une seconde façon, sans copie du script (07/11/2026) : une jonction de dossier à nom court (`New-Item -ItemType Junction -Path H:\w -Target <worktree>`), puis lancer `.\tools\idf.ps1` depuis `H:\w`, après avoir copié un `.cache\composants` et un `managed_components` complets d'un worktree voisin **vers le chemin de la jonction**, pas vers le chemin long (même limite). CMake résout la jonction pour les sources ; seul le cache de composants garde le chemin court, et c'est lui qui compte. Retirer la jonction ensuite (`cmd /c rmdir H:\w` retire le lien, pas le dossier).

**Vu sur** le PC de compilation (Windows 11, ESP-IDF 5.5.5), branche `claude/reglages-appareil-b3aad6`, 06/11/2026 ; aucune carte en jeu. **Preuve** : trace Python dans le journal de compilation ; longueur du chemin calculée (260) et même calcul pour les autres worktrees ; avec le cache court, la configuration, le téléchargement des composants (dont `espressif/mdns` 1.13.1) et la compilation (code de sortie 0) sont passés. Revu le 07/11/2026 dans le worktree `agent-a4c19a135e3c33e73` (le fichier `.sh`, 260 caractères) : depuis la jonction, la configuration et les compilations du Tab5 et de la Waveshare 7B sont passées (code de sortie 0). Revu le 07/11/2026 dans le worktree `source-meteo-client-reseau-29c0af` (racine de 85 caractères) et contourné **sans jonction**, avec la copie privée de `tools/idf.ps1` dont le cache de composants est `%USERPROFILE%\.tab5\cache-composants` : la configuration et les compilations du Tab5 et de la Waveshare 7B sont passées (code de sortie 0, environ 3 minutes en tout, cache du compilateur chaud). **Statut** : contourné.


#### Changer de carte : réglages périmés, ou verrou de dépendances réécrit

**Symptôme.** Après avoir changé `BOARD`, la compilation garde la taille de flash, la table de partitions ou le mode PSRAM de la carte précédente ; ou la compilation d'une carte télécharge les pilotes de l'autre et réécrit `dependencies.lock`.

**Cause.** Les `sdkconfig.defaults*` ne sont lus que pour un `sdkconfig` **neuf** (voir « Une modification de `sdkconfig.defaults*` n'a aucun effet ») ; et le gestionnaire de composants regarde tous les dossiers `components/bsp_*` et fusionne leurs dépendances dans un seul fichier de verrou.

**Solution.** Supprimer `sdkconfig`, puis relancer `set-target` avec `"-DBOARD=<carte>"` ([commandes](carte-7b.md#compiler)). Le CMake de la racine exclut le composant de la carte non choisie (`EXCLUDE_COMPONENTS`, donc le gestionnaire de composants ne le voit pas) et chaque carte autre que le Tab5 a son propre verrou (`boards/<carte>/dependencies.lock` ; le Tab5 garde celui de la racine).

**Vu sur** le PC, aucune carte, commit `52f6372`, 07/11/2026. **Preuve** : la 7B se configure et se compile (1 837 760 octets) ; le Tab5 se compile aussi (commit `093ac94`, dossier de compilation à part) et le `dependencies.lock` de la racine n'a pas été modifié. **Statut** : résolu (côté compilation seulement).

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

#### Paysage : l'anneau tactile est « totalement décalé par rapport au doigt »

**Symptôme.** Premier essai du paysage sur le Tab5 : l'image est droite (« Oui, c'est droit »), mais l'anneau vert n'apparaît pas sous le doigt (« pas au bon endroit, totalement décalé »).

**Cause.** Le point tactile était tourné **deux fois** : une fois par `esp_lcd_touch` (miroir Y + échange X/Y demandés dans `tab5_touch.c`), une fois par LVGL lui-même (`lv_indev.c`, `indev_pointer_proc` → `lv_display_rotate_point`, qui pour une rotation de 90° donne `x = ver_res − Y − 1`, `y = X`). Le commentaire d'origine affirmait à tort que LVGL ne tourne pas le tactile. Un doigt en `(x, y)` logique donnait un anneau en `(1279 − y, x)`, hors écran dès que `x > 719`.

**Solution.** Aucun miroir ni échange dans `tab5_touch.c` : la puce rend le repère natif et LVGL le tourne seul. Une ligne de journal par appui (`appui : brut (X, Y) … -> logique (x, y) …`, `tab5_orientation.c`) donne la preuve sans dépendre de ce que l'on voit.

**Vu sur** Tab5 ST7121, symptôme sur le binaire du commit `102077b`, correctif vérifié sur `3b68f42` (version d'essai avec l'autotest du tactile) et `9f82623`, 06/11/2026. **Preuve** : l'autotest injecte six appuis en coordonnées brutes de la dalle (quatre coins, centre, bouton Réglages) et le journal affiche six fois « AUTOTEST appui n/6 … OK » ; 27 appuis réels sont journalisés et la formule `x = 1279 − Y`, `y = X` se vérifie sur chaque ligne ; Axel : « les doigts étaient bons ». **Statut** : résolu (détail dans [mesures/orientation.md](mesures/orientation.md) § 5).

#### Chaque appui sur l'écran redémarrait la carte
**Symptôme.** Avec le binaire du commit `557eb44`, « dès que je touche l'écran il redémarre » (Axel, ~22:29). **Cause.** Le rappel du journal par appui appelait `lv_event_get_indev(e)`, or pour les événements d'un périphérique d'entrée LVGL passe le **périphérique comme cible** et l'objet touché comme paramètre (`lv_indev_send_event`) : le rappel lisait un objet comme si c'était un périphérique. Il avait été compilé mais jamais exécuté avant de demander à Axel d'appuyer. **Solution.** `lv_event_get_target(e)` (commit `c1fa7f6`), puis une version d'essai injecte six appuis simulés au démarrage pour que le code d'événement tourne sur la carte **avant** qu'on demande à quelqu'un de toucher. **Vu sur** Tab5 ST7121, `557eb44` puis `c1fa7f6`, 06/11/2026. **Preuve** : autotest 6/6 OK sans redémarrage, puis 27 appuis réels sans redémarrage. **Statut** : résolu.

#### Aucun journal par l'USB, ou une erreur de compilation sur un `|` entre deux drapeaux LVGL

- **Pas de journal** : il faut `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (la console UART par défaut n'est pas câblée à l'USB du Tab5). Posé dans `boards/tab5/sdkconfig.defaults.board`.
- **`A | B` sur deux drapeaux LVGL** : en C++ le résultat est un `int` que LVGL refuse ; un appel par drapeau. De même, les macros de configuration d'Espressif (initialiseurs désignés dans un ordre que C++ refuse) : la couche carte (`components/bsp_tab5/`) est donc écrite en **C**, l'application en C++.

**Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : résolu.

#### Journal `swap_xy is not supported by this panel` au démarrage

Émis par `esp_lvgl_port` à l'initialisation ; sans effet constaté sur l'image (orientation portrait native fixe, aucune rotation logicielle). Cause non explorée. **Vu sur** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Statut** : connu, sans effet constaté.

#### Marge faible du bootloader et test de la PSRAM

Le bootloader occupe 0x5a30 octets dans ses 24 Kio : **6 % de marge** seulement, à surveiller si on ajoute des options. Le démarrage prend environ 2,9 s de la remise à zéro à la première image, dont ~0,95 s de test de la PSRAM (`CONFIG_SPIRAM_MEMTEST=y`) et 1 s d'attente de l'expandeur : deux gains possibles, **à mesurer avant de sacrifier quoi que ce soit**, un changement à la fois. Sur la compilation de la 7B (commit `c48c418`, 07/11/2026, PC) : bootloader de 0x5d10 octets, **0x2f0 octets (3 %) de marge**, encore plus serré. **Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : connu, pas traité.

#### Écran noir après un redémarrage logiciel

*Connu ailleurs ; ici, un seul redémarrage logiciel essayé et réussi (n° 2 du 06/11/2026, `SW_SYS_RESET`, image revenue, Tab5 ST7121), pas de série de 20.* Sur le firmware ESPHome, la broche de reset de l'écran passe par l'expandeur PI4IOE5V6408, qui a besoin d'environ une seconde pour être fiable : reset trop tôt = écran noir. Le firmware natif reprend l'attente de 1 s avant le reset (propre à la carte : la 7B n'en a pas, son reset est une broche du P4). Sur P4, `esp_restart()` ne remet à zéro que les cœurs : terminer par `esp_rom_software_reset_system()`. À vérifier au jalon 1b par 20 redémarrages.


#### Waveshare 7B : le rétroéclairage ne devient jamais tout à fait noir

*Connu ailleurs (le firmware ESPHome de l'auteur sur cette carte, 16–17/10/2026) ; le correctif natif est écrit et **non vérifié sur la carte**.* Le rétroéclairage est analogique (BL_CTRL → filtre RC → AP3032) : à 0 % la dalle reste faiblement éclairée, et la commande « display off » (0x28) seule ne changeait rien de visible. Ce qui a marché sous ESPHome : voile noir, puis 0x28, puis « sleep in » 0x10, et l'inverse au réveil (0x11, 120 ms, 0x29). La couche carte fait pareil dans `board_set_backlight_percent(0)` (`ws7b_panel_plan()`, testé sur le PC). **Vu sur** Waveshare 7B sous ESPHome, 17/10/2026 (observation de l'auteur) ; firmware natif : PC seulement, commit `52f6372`. **Statut** : contourné sous ESPHome ; la version native n'est pas testée sur carte.

#### Waveshare 7B : le C6 ne répond pas à la requête de version

*Connu ailleurs (mesures de l'auteur, 16–19/10/2026).* Le micrologiciel d'origine de l'ESP32-C6 ne répond pas à la requête « version » d'esp_hosted, ce qui peut ressembler à une panne du lien. La couche carte ne la pose pas (`WS7B_READ_C6_VERSION=0`) et ne met jamais le C6 à jour (décision de l'auteur, 16/10/2026). **Vu sur** Waveshare 7B sous ESPHome. **Statut** : contourné ; pas encore vu avec ce firmware.

#### Waveshare 7B : le haut-parleur sature au-dessus de 0 dB

*Connu ailleurs (essai de l'auteur à l'oreille, 17–18/10/2026, ESPHome).* Registre de volume 0x32 de l'ES8311 : 0xBF = 0 dB (75 % de l'échelle du registre), au-dessus le haut-parleur sature (+32 dB à 0xFF) ; sous 40 % de l'échelle (0x66) « on n'entend rien ». La couche plafonne à 0xBF et fixe le plancher d'un volume non nul à 0x66 ; la courbe est testée sur le PC. **Statut** : contourné ; non testé sur carte.

#### Waveshare 7B : câblage du reset tactile, deux sources se contredisent

**Symptôme.** Aucun vu pour l'instant. **Cause.** Le schéma et le firmware ESPHome (`reset_pin: GPIO23`) mettent le reset du GT911 sur GPIO23 ; le BSP de Waveshare le déclare non relié / partagé avec le reset de l'écran. La couche envoie l'impulsion sur GPIO23 et ne donne aucune broche de reset au pilote. **Solution.** Aucune appliquée. Au premier essai, vérifier au journal que l'identifiant produit « 911 » est lu en 0x5D (ou 0x14) après l'impulsion ; sinon essayer sans l'impulsion (reset de l'écran seul). **Vu sur** aucune carte. **Statut** : **ouvert**, non vérifié.

---

### Flash, port série et sauvegarde

#### Écran noir après `write-flash --after no-reset` : l'application ne démarre pas

**Symptôme.** `esptool write-flash … --after no-reset` réussit (« Hash of data verified ») ; un script de capture qui « remet à zéro » ensuite la carte par une impulsion RTS ne lit rien pendant 35 s, et l'écran reste noir.

**Cause.** `--after no-reset` laisse la puce en mode téléchargement de la ROM, et l'impulsion RTS du script de capture ne l'en a pas sortie (la raison exacte n'a pas été cherchée).

**Solution.** Laisser esptool remettre la puce à zéro (`python -m esptool --port COMn --after hard-reset chip-id`, ou flasher sans `--after no-reset`), puis lancer la capture aussitôt : les lignes de la ROM et du chargeur de ce démarrage sont manquées, pas celles de l'application. L'application a démarré du premier coup.

**Vu sur** Tab5 ST7121, binaire de la branche `fix/retours-axel-07-11` (SHA-256 `756A8333…`), 07/11/2026. **Preuve** : aucun octet sur le port après l'impulsion RTS, écran noir signalé par l'auteur ; journal complet après la remise à zéro d'esptool. **Statut** : contourné.

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


#### Waveshare 7B : sauvegarder la flash de 32 Mo

*Connu ailleurs (les sauvegardes de l'auteur sur cette carte le 20/10/2026, esptool 5.3.1).* Sur l'ESP32-P4, `read-flash` avec le stub échoue à des secteurs fixes : lire par morceaux de 2 Mo (environ 21 s chacun), couper en 8 en cas d'échec, secteurs de 4 Ko en `--no-stub` en dernier recours, `--before default-reset` après un stub coupé, et prouver la copie par `verify-flash` (MD5 calculé dans la puce). La flash réelle fait **32 Mo** (le premier plan en supposait 16). L'otadata (0x9000 et 0xa000) se décode avant de choisir ce qu'on garde : numéro de séquence, CRC `crc32_le(0xFFFFFFFF, seq)`, emplacement `(seq − 1) % 2`. `tools/essai_7b.py` fait tout cela ; son décodeur a été confronté à l'otadata réelle de la sauvegarde du 20/10/2026 (séquences 5 et 4, CRC valides toutes les deux, emplacement 0 choisi). La partie du script qui parle à la carte **n'a pas été exécutée**. **Statut** : contourné ; script écrit, partie carte non exécutée.

---

### Mémoire et chien de garde

#### Toucher « Wi-Fi » dans les réglages redémarre la carte : `sem create failed`, puis `assert … port_esp_hosted_host_os.c:589`

**Symptôme.** Réglages ouverts, appui sur « Wi-Fi » (recherche de réseaux) : la carte redémarre. Journal : `sem create failed`, `assert failed: hosted_destroy_semaphore port_esp_hosted_host_os.c:589`, redémarrage. Deux fois le 07/11/2026. Dans le même état, la recherche de ville n'arrivait pas à ouvrir sa connexion (`Failed to allocate memory`).

**Cause.** Réglages ouverts, il ne restait que **9,7 Ko** de mémoire interne libres (plus gros bloc 4,6 Ko). La liaison Wi-Fi avec l'ESP32-C6 (`esp_hosted`) et le TLS ne vivent **qu'en** mémoire interne. LVGL utilisait le `malloc` du système, et tout bloc de moins de 256 octets (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`) va en mémoire interne ; un objet, un style ou une étiquette LVGL pèse 100 à 250 octets : l'écran principal seul en prenait 52 Ko, chaque écran de réglages une quarantaine.

**Solution.** LVGL a son propre allocateur, PSRAM d'abord et mémoire interne seulement quand la PSRAM est pleine (`main/lvgl_memoire.cpp`, `CONFIG_LV_USE_CUSTOM_MALLOC=y`, commit `ce3ab9f`). Les lignes `-Wl,-u,<symbole>` de `main/CMakeLists.txt` sont nécessaires : rien dans `main` n'appelle ces fonctions, et sans elles l'éditeur de liens laisse le fichier dans `libmain.a` (`undefined reference to lv_malloc_core`). Après un changement de `sdkconfig.defaults`, supprimer `sdkconfig` (les defaults ne sont relus que pour un neuf). Filet de sécurité (commit `f572d9f`) : `reseau.cpp` refuse une recherche sous 16 Ko libres ou un plus gros bloc de 8 Ko, avec un avertissement au journal ; ces deux seuils sont des valeurs de départ, le coût réel d'un scan n'est pas mesuré.

**Vu sur** Tab5 ST7121, build intégré `953328d`, 07/11/2026. **Preuve** : avant, mémoire interne libre 74 Ko à la réception de l'heure et 9,7 Ko réglages ouverts (minimum 5,5 Ko) ; après, 211 Ko et 210 Ko (minimum depuis le démarrage 148 500 o), réglages parcourus et recherche lancée à la main par l'auteur, aucun redémarrage. La vitesse de dessin n'a pas été comparée à l'ancien allocateur. **Statut** : résolu sur cet essai ; à surveiller sur une durée plus longue.

#### La requête météo échoue : `esp-aes: Failed to allocate memory…` ou `PK verify failed with error 0x4290`

**Symptôme.** `esp-aes: Failed to allocate memory for the array of DMA descriptors` (ou `… for start alignment buffer`) environ 80 ms après le début d'une requête HTTPS, ou, aux essais suivants, `esp-x509-crt-bundle: PK verify failed with error 0x4290` après 14 s, puis `esp-tls: Failed to open new connection`. La météo reste « indisponible » ; la RAM interne libre passe sous 20 Ko (700 o au plus bas).

**Cause.** L'ESP32-P4 a environ 455 Ko de tas interne. Par défaut (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384`) tout `malloc()` jusqu'à 16 Ko y est servi en premier, et mbedTLS y prend ses tampons (16 Ko en entrée, 4 Ko en sortie, certificats). Avec les tampons d'écran (216 Ko), environ 106 Ko de piles de tâches et les objets des écrans, il restait 111,6 Ko avant la première requête et 74 Ko une fois le serveur web lancé. Une prise de contact en demande environ 51 Ko, et l'AES matériel exige toujours un bloc *interne* compatible DMA pour son tableau de descripteurs (`esp_aes_dma_core.c:458`). Quand une allocation échoue, la prise de contact s'interrompt (0x4290 est l'opération publique RSA qui échoue sur une allocation).

**Solution.** Dans `boards/tab5/sdkconfig.defaults.board` : `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256` (les objets de 256 o et plus vont en PSRAM : +65,9 Ko libres avant la première requête) et `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` (tampons TLS en PSRAM : environ 29 Ko de moins en interne pendant une prise de contact ; elle dure 2,8 s au lieu de 1,5 s). Les piles de tâches ne peuvent pas aller en PSRAM ici : `spi_flash/cache_utils.c:127` vérifie qu'une tâche qui écrit en flash (la NVS) a sa pile en RAM interne.

**Vu sur** Tab5 ST7121, build intégré `4e7d8af`, correctif `4f6bd0a`, 06–07/11/2026. **Preuve** : lignes de journal par étape et commandes série `memoire`, `blocs` et `piles` ; avant, 3 requêtes sur 3 ratées ; après, 29 démarrages sur 30 suivis jusqu'au bout avec une requête réussie (27 de suite), 0 chien de garde, 0 échec d'allocation ; détail dans [mesures/memoire-interne.md](mesures/memoire-interne.md). **Statut** : résolu sur le Tab5 ST7121 ; autres cartes non essayées.

#### Le chien de garde des tâches se déclenche : `IDLE0 (CPU 0)` … `CPU 0: meteo`

**Symptôme.** `task_wdt: Task watchdog got triggered … IDLE0 (CPU 0) … CPU 0: meteo`, toutes les 5 s pendant une prise de contact TLS.

**Cause.** Une prise de contact calcule des secondes sans jamais s'endormir. La tâche météo était à la priorité 3, au-dessus de la tâche d'inactivité (0) : sur le cœur qu'elle occupe, la tâche d'inactivité n'obtient plus le processeur et cesse de nourrir le chien de garde (`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y`, délai de 5 s).

**Solution.** La tâche météo tourne à `tskIDLE_PRIORITY` (0) et partage le cœur avec la tâche d'inactivité tranche par tranche (1 ms). Le chien de garde reste actif et inchangé : ne pas le désactiver ni l'allonger pour masquer ce symptôme.

**Vu sur** Tab5 ST7121, `2a05811`, 07/11/2026. **Preuve** : les commandes série provisoires `tourner3` et `tourner0` (une tâche épinglée sur le cœur 0 calcule 8 s) : le chien de garde se déclenche 5,0 s après le début à la priorité 3 et ne se déclenche pas à la priorité 0. **Statut** : résolu.

---

### Alertes (Vigie)

#### Une petite réponse de Vigie n'est pas en gzip : le décodeur la refuse

**Symptôme.** Avec `Accept-Encoding: gzip` envoyé, la réponse de Hambourg (426 octets, aucune alerte) n'a pas de `Content-Encoding` et commence par `{`, pas par les octets gzip `1f 8b` : un client qui décompresse toujours la rejette comme « en-tête invalide ».

**Cause.** L'instance publique ne compresse que les corps d'environ 500 octets ou plus (les 9 autres réponses réelles sont en gzip, chunked, de 365 à 1 976 octets sur le fil).

**Solution.** `main/vigie_client.cpp` décide d'après les deux premiers octets (`gzip_flux::est_gzip`), jamais d'après la longueur ni l'en-tête seul ; un corps en clair est lu tel quel. Un corps qui n'est ni du JSON ni du gzip valide est refusé et compté comme un échec (éloignement), jamais lu à moitié.

**Vu sur** PC seulement (tests du noyau avec AddressSanitizer + UBSan, rendu PC), branche `claude/alertes-vigie-9b13fe`, 07/11/2026, réponses réelles de l'instance publique de Vigie captées le 07/11/2026 de 08:52 à 08:53 UTC. **Non vu sur une carte.** **Preuve** : `test/host/donnees/vigie_hambourg.json` (426 octets, lu par les tests du noyau) et `vigie_captures.json` (`content_encoding: null`). **Statut** : géré sur le PC ; la carte n'a pas encore reçu de réponse.

#### Sans filtre `lang`, un avertissement pèse 26 Ko

**Symptôme.** La Palma sans filtre : 26 190 octets de JSON pour 9 avertissements ; Eberswalde : 7 623 octets pour un seul.

**Cause.** Un service allemand ou espagnol donne son texte en jusqu'à huit langues ; Vigie les renvoie toutes si on ne lui dit pas lesquelles garder.

**Solution.** La requête porte toujours `lang=<langue de l'écran>,en` (`lang=en` pour l'anglais) : 19 238 et 2 797 octets. Les mots de l'émetteur ne sont pas touchés, seulement filtrés par langue. Les tampons de réception et de texte font 64 Ko chacun, en PSRAM, rendus après chaque essai ; une réponse qui ne tient pas est refusée, pas coupée.

**Vu sur** PC seulement (tests du noyau avec AddressSanitizer + UBSan, rendu PC), branche `claude/alertes-vigie-9b13fe`, 07/11/2026, réponses réelles de l'instance publique de Vigie captées le 07/11/2026 de 08:52 à 08:53 UTC. **Non vu sur une carte.** **Preuve** : `vigie_la_palma_toutes_langues.json` (26 190) contre `vigie_la_palma.json` (19 238). **Statut** : géré sur le PC.

#### L'écran se tait une seconde au changement d'heure

**Symptôme.** Quand deux alertes se suivent, la première finissant à `hh:59:59` (AEMET) et la seconde commençant à `hh:00:00` (DWD) ou l'inverse, rien n'est « en vigueur » pendant une seconde : la pastille peut disparaître puis revenir.

**Cause.** La fin d'une alerte est exclue et les émetteurs ne s'accordent pas sur la dernière seconde.

**Solution.** Une fin écrite à `hh:59:59` compte désormais comme la dernière seconde de l'alerte (`fin_exclue_s` dans `vigie_vue.hpp`, 07/11/2026) ; toute autre fin reste exclue. Les tests du noyau figent les deux (`test_vigie_alertes.cpp` : instant 1794085199 de la réponse de La Palma, et un test dédié autour de `hh:59:59`, `hh:00:00` et `hh:39:59`). Une hystérésis (garder la pastille 2 s) n'a pas été choisie : elle garderait toute alerte 2 s après sa fin. Limite : un émetteur qui écrirait `hh:59:59` comme fin exclue garderait son alerte une seconde de trop (non vu).

**Vu sur** PC seulement (tests du noyau avec AddressSanitizer + UBSan, rendu PC), branche `claude/alertes-vigie-9b13fe`, 07/11/2026, réponses réelles de l'instance publique de Vigie captées le 07/11/2026 de 08:52 à 08:53 UTC. **Non vu sur une carte.** **Statut** : corrigé dans le noyau (tests sur le PC seulement), jamais vu sur une carte, ni avant ni après.

#### Un texte d'émetteur disparaît de l'écran

**Symptôme.** Un avertissement est montré avec les mots de Vigie (« niveau orange, pluie ») au lieu du titre de l'émetteur, ou une description manque.

**Cause.** Le noyau ne réécrit jamais le texte d'un émetteur : un texte qui contient un caractère que la police embarquée (`main/assets/roboto_700.ttf`) ne dessine pas est écarté en entier, et la langue suivante est essayée (langue de l'écran, puis anglais, puis le premier texte). La table des caractères dessinables (790) est comparée à la police par `test/host/controle_python_vigie.py`.

**Solution.** Aucune : c'est voulu (un carré au milieu d'un avertissement est pire que les mots de Vigie). Si le texte d'un vrai émetteur est écarté pour un caractère qui devrait se dessiner, ajouter le caractère à la police et à `PLAGES_POLICE` dans `vigie_alertes.hpp`, puis rejouer les tests.

**Vu sur** PC seulement (tests du noyau avec AddressSanitizer + UBSan, rendu PC), branche `claude/alertes-vigie-9b13fe`, 07/11/2026, réponses réelles de l'instance publique de Vigie captées le 07/11/2026 de 08:52 à 08:53 UTC. **Non vu sur une carte.** **Statut** : voulu.

#### Une heure d'alerte est coupée en deux lignes (« 00: » puis « 59 »)

**Symptôme.** Sur la page des alertes, la fin d'une alerte écrite « dimanche 8 novembre · 00:59 » passait à la ligne après les deux-points : « 00: » sur une ligne, « 59 » sur la suivante (rendu PC, 720×1280, La Palma, français).

**Cause.** Le moteur de texte de l'écran (LVGL) traite les deux-points comme un endroit où une ligne peut se couper, comme une espace : une heure en bout de ligne longue peut donc être coupée au milieu du nombre.

**Solution.** L'heure passe en premier : « 00:59 · dimanche 8 novembre » (`vigie::formater_instant`, `components/core/include/ecran/vigie_textes.hpp`). Au début d'une ligne, une heure ne peut pas être coupée. Couvert par `test_vigie_textes.cpp` (texte exact dans les cinq langues, tampons tronqués) et ses mutations.

**Vu sur** PC seulement (tests du noyau avec AddressSanitizer + UBSan, rendu PC), branche `claude/alertes-vigie-9b13fe`, 07/11/2026. **Non vu sur une carte.** **Statut** : corrigé sur le rendu PC ; à vérifier à l'œil sur la dalle.

#### Crochets et outils : pièges rencontrés en écrivant les alertes

- **`robocopy … /E` est refusé** par la garde du disque `E:` (faux positif sur l'option `/E`) : utiliser `Copy-Item -Recurse`.
- **Une mutation qui ne compile plus n'est pas une détection.** `-Werror,-Wunused-but-set-variable` a transformé `if (false)` en erreur de compilation comptée « détectée par autre chose » : la liste des mutations doit en montrer 0, la mutation a donc été réécrite (`if (n == 0 && b.autres > 100)`).
- **La mise en forme d'un nouvel en-tête n'est contrôlée qu'au commit** : passer `clang-format -i --style=file <fichier>` sur les nouveaux fichiers C/C++ avant `git commit`, puis vérifier que les ancres des mutations apparaissent encore une seule fois (la mise en forme peut recouper une ligne). Le crochet a refusé un commit une fois pour 14 écarts dans deux fichiers neufs.

**Vu sur** le PC, branche `claude/alertes-vigie-9b13fe`, 07/11/2026.

---

### Wi-Fi

#### Un mauvais mot de passe Wi-Fi n'est presque jamais reconnu comme tel : la radio dit 2 et 205

**Symptôme.** Essai d'un nouveau réseau avec un mot de passe faux : les événements de déconnexion portent la raison **2** (`AUTH_EXPIRE`, « authentification expirée ») et **205** (`CONNECTION_FAIL`, « connexion échouée »), chacune répétée, et la raison **202** (`AUTH_FAIL`, celle qui veut dire « mot de passe refusé ») n'est venue que dans **1 essai sur 9** (à 28,9 s du début) ; les 8 autres fois, rien d'autre que 2 et 205 jusqu'au délai de 30 s. Une règle « la même cause sûre deux fois » ne se déclenche jamais, et le résultat rendu à l'écran est « autre ».

**Cause.** `reseau.cpp` réessaie la connexion 2 s après chaque coupure (`kDelaiReessaiWifiUs`) et chaque tentative se termine par un code de raison choisi par la radio ; sur ce point d'accès l'échec d'authentification n'arrive qu'après plusieurs tentatives. Quels codes apparaissent pour quelle faute dépend du point d'accès : **non vérifié sur un autre**.

**Solution.** `essai_wifi.hpp` : seules les causes *sûres* (14, 15, 202, 204 = mot de passe refusé ; 201, 210, 211, 212 = réseau introuvable) comptent ; deux fois la même arrête l'essai plus tôt ; toute autre raison, y compris notre propre déconnexion (8), n'est ni preuve ni démenti ; au délai de 30 s, c'est la dernière cause sûre vue qui est dite. Conséquence pour l'écran : une réponse trouvée seulement au délai de 30 s lui parvient environ **36 s** après la demande (30 s d'essai + environ 6 s pour que l'ancien réseau revienne), après son propre compte à rebours de 30 s (`DELAI_CONNEXION_MS`), qui a alors déjà affiché son échec « sans réponse » et ignore un résultat tardif (`sur_resultat`, `changement_wifi.hpp`). Avec ce point d'accès l'écran affiche donc son message générique « sans réponse » pour un mot de passe faux ; à trancher côté écran (un état « retour à l'ancien réseau » qui attend la réponse, ou un délai d'essai plus court que le sien). Classer 2 et 205 répétés comme « mot de passe refusé » tient en une ligne dans `resultat_pour_raison`, **non fait** : mesuré sur un seul point d'accès, et la documentation d'ESP-IDF 5.5.5 ne lie pas ces deux codes au mot de passe.

**Vu sur** Tab5 ST7121, commit `fe54f8d`, 07/11/2026 (première série sur `5f17506`). **Preuve** : journal de 9 essais avec un mot de passe faux sur le propre réseau d'Axel (7 avec un ancien réseau : à chaque fois l'ancien réseau revenu avec son adresse IP 6,0 à 6,15 s après la décision ; 2 sans) ; les deux suites sont rejouées en tests (`serie_relevee_sur_la_carte_mot_de_passe_faux`, `…_sans_202_dit_autre`) ; détail dans [mesures/wifi-retour-arriere.md](mesures/wifi-retour-arriere.md). **Statut** : contourné (retour arrière fiable ; cause précise indisponible sur ce point d'accès).

#### Sans réseau connu, le Wi-Fi réessaie une connexion toutes les 2 s après une recherche

**Symptôme.** Appareil neuf (rien en NVS, rien de compilé) : après la première recherche de réseaux, le journal montre sans fin `Wi-Fi coupé (raison 201 …) ; nouvelle tentative dans 2 s` (11 fois en 45 s), la radio est occupée et la recherche suivante peut échouer.

**Cause.** `terminer_recherche()` relançait les tentatives de connexion dès que le Wi-Fi n'était pas connecté, alors qu'aucun réseau en service n'était à retrouver (configuration vide → « aucun point d'accès », raison 201).

**Solution.** `reconnexion_utile()` dans `main/reseau.cpp` : réessayer seulement s'il y a un réseau en service ou un essai en cours (`06b7afe`). Les tentatives de l'essai lui-même ne changent pas.

**Vu sur** Tab5 ST7121, binaire local qui ignore le réseau de la compilation (non commité), 07/11/2026. **Preuve** : avant, 11 coupures en 45 s après la recherche ; après, aucune entre la recherche et l'essai suivant, un essai raté signalé à +30,01 s sans réessai ensuite, un bon réseau adopté en 6,1 s. Le chemin normal (avec un réseau connu) a été rejoué sur le binaire final `a244af0`. **Statut** : résolu.
### Audio, expandeurs et alimentation

#### La lecture des microphones seuls sur le port I2S expire

**Symptôme.** Avec seulement le canal de réception du port I2S partagé activé, chaque lecture finissait en `ESP_ERR_TIMEOUT` (premier passage du banc, 07/11/2026) : ni niveau de microphone, ni enregistrement.
**Cause.** **Non élucidée.** Le même port lisait correctement dès que le canal d'émission était aussi activé. Hypothèse, **non vérifiée** : les horloges partagées du port ne tournent pas pour un canal de réception seul dans cette configuration.
**Solution.** `tab5_i2s_enable()` active le canal d'émission dès que celui de réception l'est ; il envoie alors du silence (le DAC est en sourdine, l'amplificateur éteint). Aucun effet audible. Ne pas « simplifier » cela sans rejouer le banc.
**Vu sur** Tab5 ST7121, 07/11/2026 ; correctif dans `993500f`. **Preuve** : lignes `audio.micro.bruit` et `audio.enregistrement` du banc, OK à `5f797e3`. **Statut** : contourné (cause ouverte).

#### Expandeur : « can't set level in input mode »

**Symptôme.** Piloter `CHG_EN` ou `nCHG_QC_EN` (expandeur 0x44) échouait avec `can't set level in input mode` après que la broche avait été lue.
**Cause.** Après une remise à zéro toutes les broches du PI4IOE5V6408 sont des entrées, et lire une broche en entrée laisse sa direction mémorisée sur « entrée » ; le pilote `esp_io_expander` refuse alors d'écrire un niveau.
**Solution.** Dans cet ordre : direction (sortie), niveau, mode push-pull (`tab5_expander_output()`). La broche prend un instant la valeur du registre de sortie (0 après une remise à zéro) : sans effet pour `SPEAKER_EN` et `CHG_EN` ; `nCHG_QC_EN` n'est pas piloté quand elle est déjà haute.
**Vu sur** Tab5 ST7121, 07/11/2026 ; correctif dans `993500f`. **Preuve** : message du pilote au premier passage du banc, puis lignes `tab5_alim` sans erreur à chaque démarrage. **Statut** : résolu.

#### Une broche pilotée en sortie se lit à 0 dans le registre d'état de l'expandeur

**Symptôme.** `SPEAKER_EN` était relu à 0 alors que le haut-parleur jouait (le contrôle `audio.ampli_allume` du banc échouait trois fois, une par ligne du DAC, aux passages précédant `5f797e3`).
**Cause.** Le registre d'état (0x0F) ne montre pas une broche pilotée en sortie (vu sur la carte ; **non vérifié sur la fiche de la puce**). Les registres disaient vrai : pendant le son, l'expandeur 0x43 lisait direction 0x6F, sortie 0x22, haute impédance 0xDD, donc le bit 1 est en sortie, haut, hors haute impédance.
**Solution.** Pour vérifier une sortie, lire direction (0x03), sortie (0x05) et haute impédance (0x07) (`tab5_expander_dump()`) ; le contrôle du banc le fait depuis `5f797e3`.
**Vu sur** Tab5 ST7121, 07/11/2026. **Preuve** : relevé des registres pendant le son, 3 lignes, et le contrôle qui passe 3 fois sur 3 à `5f797e3`. **Statut** : résolu (le contrôle était faux, pas la carte).

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

#### Écrans LVGL 9.5 : trois pièges trouvés en construisant les écrans de réglages

**Symptôme → cause → solution.**
- Un bouton désactivé (le « Continuer » de l'étape 1 tant qu'aucune langue n'est touchée) montrait le **gris du thème** au lieu de l'aspect désactivé de la palette → le thème par défaut dessine `LV_STATE_DISABLED` par-dessus le style de l'objet → l'aspect désactivé est dessiné à la main (`bouton_activer(false)`, `main/ui_commun.cpp` : couleurs propres, drapeau cliquable retiré).
- Tester « est-ce un bouton ? » avec `LV_OBJ_FLAG_CLICKABLE` reconnaissait aussi de simples conteneurs → les objets de `lv_obj_create()` sont cliquables par défaut dans LVGL 9.5 → un bouton se reconnaît avec `lv_obj_check_type(o, &lv_button_class)`.
- Un écran supprimé depuis l'un de ses propres rappels d'événement, ou un clavier ou un champ remplacé alors que son rappel `DELETE` le désigne encore, laissait un pointeur périmé → les objets sont cachés puis supprimés par `lv_obj_delete_async()`, et les rappels `DELETE` ne vident la variable globale que si l'objet supprimé est celui qu'elle tient.

**Vu sur** banc de rendu PC (LVGL 9.5.0 logiciel, `tools/rendre.ps1`), branche `claude/reglages-ecran-aea4e2`, 06/11/2026. **Preuve** : planches rendues (bouton désactivé) et parcours de `tools/rendu/scenes/scene_parcours.cpp` (un « Continuer » désactivé ne fait rien ; le clavier et les champs survivent à un changement d'écran). **Statut** : résolu sur le PC ; non vu sur une carte.

#### Zones tactiles LVGL 9.5 : zone du curseur, parents, indicateur de défilement

**Symptôme → cause → solution.**
- Des zones tactiles sous le minimum de 72 px (Retour 60, « Nouveau code » et « Tester » 60, pastilles de préréglage 64, « Quitter l'installation » 56) et un curseur de luminosité qu'on ne saisissait que sur une barre de 22 px → les boutons avaient leur hauteur écrasée après `creer_bouton()` ; pour le curseur, LVGL 9.5 n'accepte le toucher que sur la barre élargie de `LV_DPX(8)` (`lv_slider.c:250` pose l'élargissement, `lv_obj_pos.c:1195-1205` le lit), et le test « rond seul » de `lv_slider.c:269-277` ne tourne que si `LV_OBJ_FLAG_ADV_HITTEST` est posé, ce que le curseur ne fait pas (seul `lv_image.c:694` le pose) → les écrasements sont retirés, `creer_bouton` pose un `min_height` qu'aucun appelant ne peut passer sous, et la barre du curseur est élargie par `lv_obj_set_ext_click_area()` jusqu'à 72 px de haut (`rangee_curseur`, `main/ui_reglages.cpp`).
- La zone élargie du curseur ne répondait pas près de son bord haut et de son bord bas → LVGL ne cherche dans les enfants d'un parent que si le point est dans les limites de ce parent (`lv_indev.c:636`) → le curseur est dans une rangée exactement aussi haute que sa zone tactile.
- Une page coupée net en bas, sans signe qu'il y a une suite → la barre de défilement était `LV_SCROLLBAR_MODE_OFF` → `style_defilement()` (`main/ui_commun.cpp`) montre une barre (`LV_SCROLLBAR_MODE_AUTO`) tant que le contenu dépasse.
- Un contrôle de rendu qui prenait un texte vide en `flex_grow` pour un « conteneur qui dépasse » → un texte vide annonce une ligne de contenu dans une boîte de 0 px → le contrôle ne regarde que les conteneurs ordinaires (`lv_obj_class`).
- **Reste ouvert** : les touches du clavier mesurent 59 px de haut à 1280×720 (49 px à 1024×600) dans les langues à rangée d'accents (fr, nl, es, it) → six rangées ne peuvent pas dépasser 55 % de l'écran en paysage ; à cinq rangées (en, de, tr) elles font exactement 72 px.

**Vu sur** banc de rendu PC (LVGL 9.5.0 logiciel, `tools/rendre.ps1`), branche `claude/reglages-ecran-aea4e2`, 06/11/2026. **Preuve** : parcours `zones-tactiles-reglages`, `zones-tactiles-installation` et `curseur-au-doigt` (`tools/rendu/scenes/scene_parcours.cpp` ; le dernier passe par un pointeur LVGL et `lv_indev_search_obj`, donc par la recherche d'objet de LVGL elle-même) dans 7 langues × 3 tailles ; quatre fautes sur cinq introduites exprès sont détectées (la cinquième, un bouton remis à 56 px, est neutralisée par `min_height`). **Statut** : résolu sur le PC pour les boutons, le curseur et l'indicateur de défilement ; touches du clavier : décision ouverte ; non vu sur une carte (doigt réel, taille physique de la cible : non vérifiés).

#### Essai sur la carte : le doigt virtuel tombe sur le mauvais objet

**Symptôme.** Aux deux premiers passages de l'essai des réglages sur la carte, un toucher au centre de « Affichage » ouvrait la page « Appareil », « Retour » ne faisait rien, et le bouton visé ne recevait ni appui, ni relâchement, ni clic (compteurs 0, 0, 0), alors que `lv_indev_search_obj()` trouvait ce bouton à ce point précis.

**Cause.** `lv_indev.c:743` appelle `lv_display_rotate_point()` sur chaque point lu par un pointeur, parce que les pilotes tactiles donnent des coordonnées **brutes** de la dalle (`components/bsp_tab5/tab5_touch.c` le dit). Le pointeur virtuel de l'essai donnait des coordonnées logiques : le point était tourné deux fois. (639, 424) devenait (855, 639), dans la rangée « Appareil » ; (86, 43) devenait (1236, 86), sur rien. Un `grep rotation` sur `lv_indev.c` ne trouve rien : la fonction s'appelle `rotate_point`.

**Solution.** `vers_brut()` dans `main/autotest_reglages.cpp` donne la rotation inverse (90, 180, 270) ; après chaque appui, l'essai compare le point que LVGL a fini par lire au point visé et note un échec s'ils diffèrent : un essai à côté ne peut plus passer. Les écrans étaient justes ; c'est l'essai qui avait tort. **Vu sur** Tab5 ST7121, commits `7b435eb` et `8f6c28d` (échec) et `176de0d` (corrigé), 07/11/2026. **Preuve** : lignes `GESTE … appui reçu 0, relâchement reçu 0, clic reçu 0` puis `1, 1, 1` ; 29 vérifications, 0 échec à `176de0d`. **Statut** : résolu (dans l'essai).

#### RAM interne tombée à 20 octets pendant que les réglages sont ouverts, et la météo qui échoue

**Symptôme.** Réglages ouverts 13 à 18 s après le démarrage : `MEMOIRE … interne_libre=535 plus_gros_bloc=160 interne_min=20` et `mdns_send: Cannot allocate memory`. Sur le binaire **normal**, réglages jamais ouverts : `esp-aes: Failed to allocate memory for the array of DMA descriptors` puis `meteo: essai raté (HTTP 0)`.

**Cause (une lecture, en partie mesurée).** La poignée de main TLS de la météo (≈ 65 à 76 Ko de RAM interne) démarre vers 13 s après le démarrage, avec 96,8 Ko libres juste avant. La liste des réglages prend ≈ 21,5 Ko de RAM interne (8,8 Ko d'objets et de styles, 12,6 Ko au premier dessin) et ≈ 22 Ko de PSRAM depuis que les bitmaps de glyphes y vont (`e686310`). Ensemble ils vident le tas interne. Ma première lecture, « la liste seule prend 28 Ko », venait d'une mesure prise alors que le TLS tournait déjà.

**Solution.** Non faite : le TLS, `sdkconfig` et l'allocation de LVGL ne sont pas sur la piste des réglages. Proposé : mbedTLS en PSRAM (`4f6bd0a`, piste `jalon-1b`), puis mesurer de nouveau avec les mêmes lignes `MEMOIRE` ; si c'est encore trop juste, mettre les objets LVGL en PSRAM (allocation personnalisée) ou baisser `SPIRAM_MALLOC_ALWAYSINTERNAL`. **Vu sur** Tab5 ST7121, commits `8f6c28d` et `176de0d`, 07/11/2026. **Preuve** : lignes `MEMOIRE` du journal de l'essai et lignes `meteo` ([mesures/6-reglages-appareil.md](mesures/6-reglages-appareil.md), section 4.3). **Statut** : ouvert (aucun plantage ni redémarrage inattendu vu ; marge nulle).

#### Une mutation occupe le compilateur pendant des minutes

**Cause.** La preuve par mutation compile les tests avec une modification du noyau. Un mutant qui retire la condition de sortie d'une boucle `constexpr` (ici `&& base < PERIODE_S` dans `delai_avant_essai_s`) fait évaluer la boucle à clang jusqu'à sa limite de 2 milliards de pas (`-fconstexpr-steps=2000000000`, réglée pour que les tests qui relisent une vraie réponse compilent) : 765 s de CPU mesurées avant d'arrêter le compilateur.

**Solution.** Ce mutant est écarté de `test/host/mutations_meteo_appareil.py` (la raison est écrite en tête du fichier) ; le vrai code sort de la boucle après 7 tours, et `planning_normal_et_reessais` couvre 1 000 000 et `INT_MAX` échecs. En écrivant une mutation, vérifier qu'elle ne supprime pas la fin d'une boucle. **Vu sur** PC de l'auteur, 06/11/2026, clang 23.1.5. **Preuve** : temps CPU du processus, mutant n° 32 de la liste. **Statut** : contourné.

#### Open-Meteo : la pluie, la probabilité et l'UV d'une heure arrivent une heure (ou un quart d'heure) trop tôt

**Symptôme.** Lire `hourly.precipitation[i]` à côté de `hourly.temperature_2m[i]` dessine la pluie de l'heure `i` avec la valeur de l'heure d'avant ; pareil pour `precipitation_probability`, `uv_index` et pour `minutely_15.precipitation` (un quart d'heure).

**Cause.** Open-Meteo étiquette un cumul ou une moyenne par la FIN de son intervalle (« l'heure qui précède »), contrairement aux valeurs instantanées (température, vent, code), qui sont à l'étiquette. Les données ne le disent pas ; la documentation, si.

**Solution.** Le noyau décale d'une étiquette (`heures[i]` : instantanés à l'étiquette `i`, cumuls à l'étiquette `i + 1` ; les quarts d'heure commencent à la première étiquette moins 900 s) et la requête demande 25 étiquettes horaires et 9 en quarts d'heure pour garder 24 heures complètes. Les tests comparent le résultat à des valeurs calculées à part par Python sur sept réponses réelles.

**Vu sur** PC de l'auteur, réponses réelles captées le 2026-11-07 à 00 h 12 (heure de Paris), branche `claude/meteo-appareil-f761d6`, aucune carte. **Preuve** : Seattle (la seule capture où il pleut) : étiquette +3 600 s = 1,0 mm = les quatre quarts étiquetés +900 à +3 600 s (0,3 + 0,3 + 0,2 + 0,2), l'autre convention donnerait 0,4 ; étiquette +7 200 s = 0,2 mm = 0,1 + 0,0 + 0,1 + 0,0. **Statut** : résolu dans le noyau ; non vu sur une carte.

#### Open-Meteo : dans un fuseau qui n'est pas à l'heure ronde (Terre-Neuve, Népal) les séries ne commencent pas à l'heure courante

**Symptôme.** Avec `timezone=auto`, les séries horaires et en quarts d'heure commencent 45 minutes avant `current.time` à Katmandou (UTC+5:45) et 30 minutes après à Terre-Neuve (UTC-3:30), mais exactement à `current.time` à Lyon, Sydney ou Singapour. Un écran qui cherche « l'heure qui contient maintenant » ne trouve rien au début à Terre-Neuve.

**Cause.** Les séries commencent à l'instant courant arrondi à l'heure UTC, moins le reste de `utc_offset_seconds` modulo 3 600 (signe du C) : +20 700 s donne -2 700 s, -12 600 s donne +1 800 s. Mesuré sur deux captures ; la même règle est attendue pour +5:30, +3:30, +10:30 (non captés).

**Solution.** `heure_courante`, `quart_courant` et le bloc « pluie proche » sont recalculés avec l'heure de l'appareil ; quand aucune heure ne la contient, l'indice vaut -1 et `courant.proba_precip_pct` vaut `METEO_PROBA_INCONNUE`, jamais la probabilité de l'heure voisine. Le contrat le dit (`main/meteo_etat.h`) et les tests couvrent les deux fuseaux.

**Vu sur** PC de l'auteur, captures du 2026-11-07 00 h 12 (heure de Paris), aucune carte. **Preuve** : `hourly.time[0] - current.time` = -2 700 s (Katmandou), +1 800 s (Terre-Neuve), 0 (Lyon, Sydney, Singapour). **Statut** : résolu dans le noyau ; l'écran doit accepter `heure_courante == -1`.
### Écran principal

#### Pas d'heure et un ciel presque noir pendant les 13 premières secondes

**Symptôme.** Après un démarrage, l'écran principal n'affiche pas l'heure et montre un ciel de nuit jusqu'à la première réponse SNTP (environ 13 s). L'auteur : « le fond bleu met du temps à apparaître ».

**Cause.** `ecran_principal.cpp` ne prenait l'heure qu'une fois SNTP répondu, alors que l'heure du système était déjà juste (la couche carte la reprend du RX8130 à 2,6 s après la mise sous tension, ou elle est gardée à travers un redémarrage logiciel). De plus, tant que la météo n'était pas lue, le ciel jugeait le jour et la nuit en UTC (une à deux heures de nuit de trop).

**Solution.** La règle du noyau `rtc::heure_affichable(reseau_synchronise, systeme_s)` (le réseau a donné l'heure OU l'heure du système est plausible ; testée, 2 mutations) et `decalage_secours_s`, le décalage de l'horloge passé à l'écran (commit `a740935`).

**Vu sur** Tab5 ST7121, build intégré `953328d`, 07/11/2026. **Preuve** : journal : « 09:01 » et la date 3,4 s après un redémarrage au lieu d'environ 13 s ; le ciel lui-même n'a pas été regardé par moi ; démarrage après une coupure de courant (lecture de l'horloge matérielle) non rejoué. **Statut** : résolu dans le journal ; à confirmer à l'œil.

#### Le dégradé du ciel montre des bandes visibles

**Symptôme.** Sur un ciel qui va du bleu sombre à un bleu plus clair, des paliers de couleur se voient en travers de l'écran.

**Cause.** La dalle prend du RGB565 (32 ou 64 niveaux par canal) et LVGL 9.5 n'a pas de tramage ; son dégradé (deux arrêts au plus ici, `LV_GRADIENT_MAX_STOPS 2`) est dessiné tel quel dans ce format, donc chaque niveau devient une bande.

**Solution.** Le ciel est une image RGB565 construite avec un tramage ordonné 8 × 8 (Bayer) et posée en image de fond de l'écran (`remplir_ciel_trame` et `appliquer_ciel` dans `main/ui_principal.cpp`). Elle n'est refaite que quand le moment de la journée ou la classe de temps change, et ses 1 843 200 octets (1280 × 720) vivent en PSRAM. Si l'allocation échoue, l'écran retombe sur le dégradé vertical de LVGL (des bandes, mais un écran qui marche).
- **Vu sur** : rendu PC (LVGL 9.5.0, clang), branche `claude/ecran-visuel-ba722a`, 07/11/2026 ; et un Tab5 (écran ST7121), commit `cd362eb`, 07/11/2026 (journal série seulement, voir l'entrée suivante).
- **Preuve** : rendus avant et après (les bandes disparaissent). Sur la carte : l'écran est construit (« 9 objets LVGL, 7 polices »), la PSRAM libre est inférieure de 1,8 Mo à celle de l'ancien écran (compatible avec le ciel de 1 843 200 octets, non isolé), aucune erreur d'allocation dans le journal ; les 10 premières secondes dessinent 3 images à 215 ms en moyenne, puis un changement de minute redessine une image de 10 zones en 82 ms (l'ancien écran : environ 24 ms). **Non vérifié** : les couleurs et les bandes sur la dalle, à l'œil.
- **Statut** : *résolu sur le rendu PC* ; *sur la carte : alloué et dessiné sans erreur, pas regardé à l'œil*.

#### La carte redémarre environ 14 s après le démarrage, à la première requête météo

**Symptôme.** Avec le nouvel écran principal, le Tab5 redémarre en boucle : l'écran est construit (« écran principal 1280x720 chargé »), puis, vers 13,7 s, le journal écrit `abort() was called at PC …` et « Rebooting… », et le démarrage suivant annonce « plantage (exception) » (code 4). L'ancien écran principal, construit sur le même commit de base, ne le fait pas.

**Cause (probable, non isolée allocation par allocation).** La mémoire interne s'épuise quand la première requête météo (TLS) et le serveur web démarrent ensemble. Mémoire interne libre juste avant cette requête : 109,2 Ko avec l'ancien écran, 44,9 à 45,6 Ko avec le nouveau, sur la même base (`4e7d8af`). Le réglage par défaut d'ESP-IDF `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` garde en mémoire interne toute allocation de moins de 16 Ko ; le nouvel écran dessine des chiffres bien plus grands (l'heure et la température) que l'ancien. Les deux sites d'`abort()`, décodés avec l'ELF, sont `lock_init_generic` (`newlib/locks.c:77`) et `esp_aes_intr_alloc` (`mbedtls/port/aes/dma/esp_aes_dma_core.c:117`) : tous deux arrêtent le programme quand une allocation interne échoue.

**Solution.** Les réglages mémoire du jalon 1b (commit `4f6bd0a`, branche `claude/jalon-1b-wifi-c2232d`, fichier `boards/tab5/sdkconfig.defaults.board`) : `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256` et `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`. Ils ne sont **pas** dans la branche de l'écran (fichier d'une autre session, ils arrivent avec la fusion). Mesuré avec ces deux lignes ajoutées dans un dossier de compilation à part, jamais commitées : 154,4 Ko de mémoire interne libres avant la première requête, 105,9 Ko à 154 s (minimum depuis le démarrage 96,0 Ko), aucun `abort()` sur 156 s.
- **Vu sur** : Tab5, écran ST7121 (MAC lue sur la puce avant d'écrire, non recopiée ici), ESP32-P4, commit `cd362eb` sur `4e7d8af`, 07/11/2026 de 01 h 50 à 02 h 02 ; esptool 5.3.1, application seule à `0x10000` ; binaires `061A3E69…0A28` (sans les réglages) et `F317FCFD…FFCB` (avec).
- **Preuve** : journaux série, comptés dans les fichiers. Le nouvel écran sans les réglages : 9 démarrages vus, 6 `abort()` vers 13,7 s, 1 démarrage qui a vécu 58 s avec 38 Ko libres et une requête météo refusée par mbedTLS faute de mémoire, 2 démarrages coupés par la fin de la capture avant 13 s. Avec les réglages : 2 démarrages, 0 `abort()` (70 s et 156 s). L'ancien écran sur la même base : 2 démarrages, 0 `abort()` (30 s et 66 s).
- **Statut** : *résolu avec les réglages du jalon 1b* ; *non commité sur cette branche* ; à revérifier après la fusion. La base elle-même est fragile : avec l'ancien écran, le minimum de mémoire interne libre depuis le démarrage était de 1,7 à 1,8 Ko sur cette base.

#### L'heure par heure, la tendance de pression et les cumuls de pluie sont absents sur l'appareil

**Symptôme.** L'écran principal montre la météo du moment et les jours, mais ni la bande heure par heure, ni la tendance de pression, ni le cumul de pluie, ni « pluie dans N min ».

**Cause.** Le client météo de l'appareil (`meteo_client.cpp`, session `meteo-appareil`) ne demande pas encore ces champs ni ne les garde ; l'écran ne montre que ce qu'il reçoit et n'invente jamais une valeur (la zone disparaît et les autres prennent sa place).

**Solution.** Une demande qui liste les champs (température, code et probabilité de pluie par heure ; pression à trois heures ; cumul de pluie ; précipitations à la minute) a été écrite pour la session propriétaire du client, dans le dossier de notes partagé (`ecran-visuel_besoins-meteo.md`). Le propriétaire a publié un contrat (`MeteoDetail`, commit `f9537c5` de sa branche) et, d'après son message de 02 h 10 le 07/11/2026, commité l'implémentation et la tendance de pression (commits `4489127` et `8f31fa0` ; tests PC verts et firmware compilé, jamais lancé sur une carte ; non fusionné ici). Un adaptateur de ce contrat vers la vue de l'écran, testé sur le PC contre le vrai en-tête, est dans le dossier de notes partagé (`ecran-visuel_branchement-meteo-v2.md`) ; il n'est pas dans cette branche parce qu'il inclut l'en-tête du propriétaire.
- **Vu sur** : un Tab5 (commit `cd362eb`, 07/11/2026) : l'écran montre la météo et les jours, sans ces zones. Le rendu PC montre la même chose avec la variante `donnees-minimales`.
- **Statut** : *ouvert* (en attente de l'implémentation du client et de la pose de l'adaptateur à la fusion).

#### Turc : dimanche et lundi avaient la même abréviation

**Symptôme.** Dans la semaine, « Pazar » (dimanche) et « Pazartesi » (lundi) s'écrivaient tous deux « Paz ».

**Cause.** Les trois premières lettres des deux noms sont les mêmes.

**Solution.** Les abréviations sont une table par langue (`JOURS_COURTS` dans `components/core/include/ecran/visuel_dates.hpp`), pas une coupure du nom ; le test vérifie que les sept abréviations de chaque langue sont distinctes (« Paz » pour dimanche, « Pzt » pour lundi).
- **Vu sur** : rendu PC, `fr de tr`, 07/11/2026. **Statut** : *résolu*.

#### Le rendu refuse de démarrer : « une compilation tourne déjà (ninja) »

`tools
endre.ps1` s'arrête quand `ninja` ou `esphome` tourne, pour ne pas mettre une seconde compilation lourde sur le PC. Attendre la fin de la compilation de l'autre session (verrou `build` de `verrous.py`) ou, en connaissance de cause, `-Forcer`.
