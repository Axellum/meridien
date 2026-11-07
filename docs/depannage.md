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

**Update 06/11/2026, 22:27 (same family of failure, reproduced WITH a log).** *Seen on* Tab5 ST7121, binary of commit `c1fa7f6`, start-up no. 65, right after a flash (reset by esptool's RTS signal, **not** a power cut): `pi4ioe5v6408: reset(252): Chip reset failed` → `Reset failed` → `tab5_io_init: expandeur 0x43` → `board_init: E/S` → `carte non initialisée ; arrêt (l'écran reste éteint)`. The expander at 0x43 did not answer the very first I2C transaction; the firmware made **one** attempt and stopped, **black screen** (the author: “écran noir”); a new flash gave the image back. **Hypothesis, not verified**: this is also what happened at the first start-up “without a screen” after the power cut (no. 8 → 9, not captured). **Fix** (`tab5_io.c`): up to 5 attempts 150 ms apart with an I2C bus reset in between, one log line per attempt, then at most 3 recovery restarts (RTC counter) before a stop with a readable message. Tried on the board with **simulated** failures (test binaries): 2 failures then success at attempt 3/5; 5 failures repeated → 3 restarts (`SW_SYS_RESET`, counter kept) then stop. The **real** failure was not reproduced since: the fix is not proven against it. **Series of 20 resets** (06/11/2026, 22:49–23:00, Tab5 ST7121, esptool-style RTS reset with a fresh port each time; a start-up counts only if a new start-up number and the version line appear): **before** (`c1fa7f6`, single attempt) start-ups no. 99–118, **20/20** “Carte prête” + main screen loaded in the log, 0 “Chip reset failed”; **after** (`9f82623`, with recovery) no. 78–97, **20/20**, 0 “expandeur 0x43 : essai” line (the expander answers at the first attempt every time), 0 “Chip reset failed”; the start-ups right after each flash (no. 98 and no. 119) are fine too. So the no. 65 failure is **not reproduced** and the recovery is **not proven** by this test (0/21 failures before, 0/21 after); it only shows the recovery does not slow down a normal start-up (3.03–3.08 s to “Carte prête”). “Image back” means log lines: only the author saw the screen itself. **Do not confuse**: `ESP_ERR_TIMEOUT` on the expander = a **simulated** failure from a test binary (`TAB5_AUTOTEST_EXPANDEUR`, start-ups no. 67–72, version shown `g3b68f4` because built from a dirty tree); `Chip reset failed` / `ESP_ERR_INVALID_STATE` = the **real** failure (no. 65). **Status**: still open (cause of the real failure unknown; recovery added, not proven).

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

**Solution.** Give the lock back from another PowerShell call (or with the PC's own Python, not the ESP-IDF one) and check with the `etat` command that it is free. **Seen on** the author's PC, 06/11/2026, commit `793bdfc`. **Evidence**: the traceback at the `rendre` call after `.\tools\idf.ps1 build`; the same call from a fresh call worked. **Status**: worked around.

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

**Mise à jour du 06/11/2026, 22 h 27 (même famille de panne, reproduite AVEC journal).** *Vu sur* Tab5 ST7121, binaire du commit `c1fa7f6`, démarrage n° 65, juste après un flash (remise à zéro par le signal RTS d'esptool, **pas** une coupure de courant) : `pi4ioe5v6408: reset(252): Chip reset failed` → `Reset failed` → `tab5_io_init : expandeur 0x43` → `board_init : E/S` → `carte non initialisée ; arrêt (l'écran reste éteint)`. L'expandeur 0x43 n'a pas répondu à la toute première transaction I2C ; le firmware n'a fait **qu'un** essai et s'est arrêté, **écran noir** (Axel : « écran noir ») ; un nouveau flash a rendu l'image. **Hypothèse, non vérifiée** : c'est aussi ce qui s'est passé au premier démarrage « sans écran » après la coupure (n° 8 → 9, non capturé). **Correctif** (`tab5_io.c`) : jusqu'à 5 essais espacés de 150 ms avec remise à zéro du bus I2C entre deux, une ligne de journal par essai, puis au plus 3 redémarrages de reprise (compteur RTC) avant un arrêt avec un message lisible. Essayé sur la carte avec des échecs **simulés** (binaires d'essai) : 2 échecs puis succès à l'essai 3/5 ; 5 échecs répétés → 3 redémarrages (`SW_SYS_RESET`, compteur conservé) puis arrêt. La **vraie** panne n'a pas été reproduite depuis : le correctif n'est pas prouvé contre elle. **Séries de 20 remises à zéro** (06/11/2026, 22 h 49 à 23 h 00, Tab5 ST7121, remise à zéro par RTS comme esptool avec un port neuf à chaque tour ; un démarrage ne compte que si un nouveau numéro de démarrage et la ligne de version apparaissent) : **avant** (`c1fa7f6`, un seul essai) démarrages n° 99 à 118, **20/20** « Carte prête » + écran principal chargé dans le journal, 0 « Chip reset failed » ; **après** (`9f82623`, avec reprise) n° 78 à 97, **20/20**, 0 ligne « expandeur 0x43 : essai » (l'expandeur répond du premier essai à chaque fois), 0 « Chip reset failed » ; les démarrages qui suivent chaque flash (n° 98 et n° 119) sont corrects aussi. La panne du n° 65 n'est donc **pas reproduite** et la reprise n'est **pas prouvée** par ce test (0/21 échec avant, 0/21 après) ; il montre seulement qu'elle ne ralentit pas un démarrage normal (3,03 à 3,08 s jusqu'à « Carte prête »). « Image revenue » = lignes du journal : seul Axel a vu l'écran. **À ne pas confondre** : `ESP_ERR_TIMEOUT` sur l'expandeur = un échec **simulé** par un binaire d'essai (`TAB5_AUTOTEST_EXPANDEUR`, démarrages n° 67 à 72, version affichée `g3b68f4` car compilé sur un arbre sale) ; `Chip reset failed` / `ESP_ERR_INVALID_STATE` = la **vraie** panne (n° 65). **Statut** : toujours ouvert (cause de la vraie panne inconnue ; reprise ajoutée, non prouvée).

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

**Solution.** Rendre le verrou depuis un autre appel PowerShell (ou avec le Python du PC, pas celui d'ESP-IDF) et vérifier avec la commande `etat` qu'il est libre. **Vu sur** PC de l'auteur, 06/11/2026, commit `793bdfc`. **Preuve** : la trace de l'appel `rendre` après `.\tools\idf.ps1 build` ; le même appel depuis un appel neuf a marché. **Statut** : contourné.

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
