# Waveshare ESP32-P4-WIFI6-Touch-LCD-7B — board layer

## English · [Français](#version-française)

---

> **Summary.** The native firmware has a board layer for the Waveshare 7B (`components/bsp_waveshare_7b/` + `boards/waveshare-7b/`), selected with the CMake variable `BOARD=waveshare-7b`. It implements the common interface `components/board_api/include/board.h` **unchanged**, so the core and the application are reused as they are. It **builds** (ESP-IDF 5.5.5, 1,837,760 bytes, 56 % of the 4 MB application slot free) and its pure parts are **tested on the PC** (34 tests, 33 mutations detected). **Nothing of it has been seen working on the board yet**: the 7B was not plugged into the PC during the session that wrote it. Every hardware value below is a *reading of a source*, labelled `[S]`, `[B]`, `[E]`, `[H]`, `[M]` or `[D]`, never a measurement, until [mesures/](mesures/) says otherwise with a “Seen on” line.

**Rule of this page.** A fact is written with its source. “Not verified” means no source was read or the value was never seen on the board. The 7B is the author's screen in service at home: it is flashed **only** at the author's explicit request, after a complete backup of its flash (procedure and script below).

### Sources of the facts

| Tag | Source |
|---|---|
| `[S]` | Waveshare schematic read page by page — the author's hardware audit no. 1 of 16/10/2026 (local notes, outside the repository) |
| `[B]` | Waveshare's own BSP `waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.1 (component registry): `include/bsp/*.h`, `*.c` |
| `[E]` | Espressif component `esp_lcd_ek79007` 2.0.2 |
| `[H]` | ESPHome firmware in service on this board since 15/10/2026 (its board package and the ESPHome `mipi_dsi` model for this board, ESPHome 2026.9.1) |
| `[M]` | the author's own measurements on this board, 16–19/10/2026 (C6 behaviour, backlight, speaker volume) |
| `[D]` | `esp_codec_dev` 1.5.11, `device/es8311/es8311.c` |

### What the board has

ESP32-P4 (revision v3.2, 400 MHz, read by esptool on 20/10/2026 on the author's unit — to be read again by the trial), 32 MB flash (JEDEC id `c8 40 19` given by the author, to be read again by `esptool flash-id` before the backup; the table of the ESPHome firmware in service ends at 0x2000000 = 32 MB), hexadecimal PSRAM, 7" 1024 × 600 EK79007 display on MIPI-DSI (2 lanes), GT911 touch, ES8311 (speaker, NS4150B amplifier) and ES7210 (microphones) codecs, an ESP32-C6 for Wi-Fi/Bluetooth on SDIO, a microSD slot, a battery input. **No I/O expander** (the Tab5 has one): every control line is a P4 GPIO.

### Pins and buses

| Function | GPIO / address | Source | Note |
|---|---|---|---|
| I2C SDA / SCL | 7 / 8, 400 kHz, port 1 | `[S][B]` | GT911, ES8311, ES7210 share this bus |
| I2S MCLK / BCLK / LRCK / DOUT / DIN | 13 / 12 / 10 / 9 / 11 | `[S][B]` | DOUT → ES8311 (speaker), DIN ← ES7210 (microphones) |
| Amplifier control (NS4150B) | 53, high = on | `[S][B][H]` | kept low until the first sound |
| Backlight | 32, LEDC timer 0 channel 1, 5 kHz, 10 bits, **inverted output** | `[S][B][H]` | analog dimming: 0 % is **not** black (see below) |
| Display reset | 33 | `[S][B][H]` | done by `esp_lcd_panel_reset` |
| Touch reset | 23 | `[S]` + `[H]` (`reset_pin: GPIO23`) | **discrepancy**: `[B]` declares the touch reset not connected / shared with the display reset. The code pulses GPIO23 itself and gives the driver no reset pin — see “Discrepancies” |
| Touch interrupt | **not connected to the P4** (test point TP1 only) | `[S]` | the touch is polled (LVGL timer mode) |
| C6 SDIO (slot 1, 4 bits) | CLK 18, CMD 19, D0–D3 = 14–17, 20 MHz | `[S][H]` | buffers of the transport in PSRAM |
| C6 reset | 54, active high (R33, 0 Ω to CHIP_PU) | `[S][H]` | done by `esp_hosted_connect_to_slave()` on every call |
| Console / flash port | USB-Serial/JTAG of the P4, GPIO24/25 (USB-C connector “USB”) | `[S]` | the other connector (CH343P bridge on UART0, GPIO37/38) is not used |
| Not touched by this layer | GPIO20 (battery voltage), 35 (BOOT button), 45 (microSD power) | `[S]` | listed in `ws7b_pur.h` so nobody reuses them by mistake |

I2C addresses (7 bits): GT911 **0x5D** (then **0x14** if INT was high at reset), ES8311 **0x18**, ES7210 **0x40** `[S]`. `esp_codec_dev` wants the 8-bit form (0x30, 0x80) `[D]`.

### Display (EK79007)

| Item | Value | Source |
|---|---|---|
| Resolution | 1024 × 600 native landscape, RGB565 | `[S][B]` |
| DSI | 2 lanes, **900 Mbit/s** per lane, PHY supplied by LDO channel 3 at 2.5 V | `[E]`, `[H]`; `[B]` says 1000 — see “Discrepancies” |
| DPI | 52 MHz; horizontal pulse/back/front porch 10/160/160; vertical 1/23/12 → about 60 Hz (computed and tested on the PC: `ws7b_refresh_mhz()`) | `[E]` macro, same in `[B]` and `[H]` |
| Init table | registers 0x80…0x86 = 8B, 78, 84, 88, A8, E3, 88, then “sleep out” (0x11) + 120 ms; the component emits B2 (lane count) first | `[E]`, same set as `[H]` |
| Controller detection | **not done at run time, on purpose**: the 7B has one panel model, and no DCS identifier read was ever tried on it. The name “EK79007” is a design fact, not a measurement | — |
| Frame buffer | one buffer; LVGL draws in bands and the panel is updated by copy (DMA2D), as on the Tab5 | design |

### Real “screen off” (the backlight is analog)

The backlight chain BL_CTRL → RC filter → AP3032 is analog: at 0 % the panel stays dimly lit (seen by the author on 16/10/2026, `[M]`). `board_set_backlight_percent(0)` therefore also puts the **panel itself** to sleep, with the recipe the author validated under ESPHome on 17/10/2026 (“the screen is really black, it comes back when I touch it”, `[M]`): black veil drawn on LVGL's system layer, then “display off” (0x28), then “sleep in” (0x10); on wake: “sleep out” (0x11), 120 ms, “display on” (0x29), veil removed, and **only then** the backlight goes up. The plan (steps, order, delays) is the pure function `ws7b_panel_plan()`, tested on the PC; `ws7b_io.c` only executes it. Build option `WS7B_REAL_PANEL_OFF=0` disables it (the backlight then goes down to 0 % only). **Not verified on the board**, in particular that the veil is drawn before the panel command and that the touch still wakes it.

### Touch (GT911)

The driver `esp_lcd_touch_gt911` 1.2.1 is given `mirror_x = mirror_y = 1`, `swap_xy = 0`, range 1023 × 599 `[B][H]`: both Waveshare's BSP and ESPHome invert X and Y (the GT911 is mounted upside down relative to the panel; without it “1 recognised press in 5 minutes”, `[H]`, trial of 14/10/2026). The **board layer** then turns the raw point into the **logical** point of the interface (rotation, see below) and clamps it into the panel before handing it to LVGL, so the application never sees raw coordinates. The product ID (register 0x8140) is read at start-up and must be “911”. The serial log prints one line per press: raw point on the 1024 × 600 panel → logical point on the interface. A build with `WS7B_AUTOTEST_TACTILE=1` injects five **simulated** presses (four corners and the centre) through the same path 10 s after start and logs `AUTOTEST appui k/5 … OK|ECART`: it proves the rotation arithmetic, **not** that the chip returns the right points (that needs a finger).

### Orientation (rotation)

The panel is natively landscape. Build option `WS7B_ROTATION`, **0 or 180** only (anything else stops the configuration):

| Value | Meaning | Source |
|---|---|---|
| **0** (default) | image as the panel scans it. **This is the author's unit**, mounted upside down compared with the Waveshare demos | `[M]` (“its builds use rotation 0” since 16/10/2026) |
| 180 | “normal” mounting: the direction of the Waveshare demos and the public ESPHome default | `[B]` (`ESP_LV_ADAPTER_ROTATE_180`), `[H]` (`carte_rotation: 180`) |

The rotation is done by software (LVGL + the P4 PPA accelerator, like the Tab5); the panel's own scan-direction register exists in the component but was **never tried** on this board (ESPHome disables it). 90°/270° are not supported. **The default of 0 is a decision taken in the author's place** and is to be confirmed on the unit: if the clock appears upside down, build with `"-DWS7B_ROTATION=180"`.

### Audio

ES8311 on I2S0, mono, 16 kHz, 16 bits, through `esp_codec_dev`; ES7210 for the microphones (MIC1 + MIC2). **Volume is capped**: the ES8311 register 0x32 has 0.5 dB steps, 0xBF = 0 dB, 0xFF = +32 dB, and the speaker **saturates** above 0 dB (author's trial by ear, 17/10/2026: 100 % saturates, 80 % a little, 75 % clean, `[D][M]`). The layer writes the register itself, never above 0xBF (= 75 % of the register scale), and a non-zero volume never goes under 0x66 (−44.5 dB) because below it “nothing is heard” (author, 18/10/2026, `[M]`); volume 0 mutes. The curve `ws7b_volume_reg()` is tested on the PC (never above 0xBF, monotonic, 0 mutes). A build with `WS7B_ESSAI_AUDIO=1` plays a 440 Hz tone of 300 ms at 60 % (register 0x9B, about −18 dB) and then reports the microphone level. **Not verified on the board.** Note: `board.h` has no audio API yet — the audio code is ready inside the layer and waits for milestone 1c; if the interface has to grow, the proposal goes to the Tab5 board-layer owner first.

### Wi-Fi through the ESP32-C6

`esp_hosted` 2.12.12, `esp_wifi_remote` 1.6.3, `wifi_remote_over_eppp` 0.3.3, `eppp_link` 1.1.5 are **pinned** in `components/bsp_waveshare_7b/idf_component.yml`. **The C6 firmware is never updated by this code** (decision of the author, 16/10/2026: “I am afraid of blocking the screen”). The C6 is powered permanently, so the second attempt of `board_wifi_link_up()` is a new reset of the C6 through GPIO54, not a power cycle. The factory C6 firmware **does not answer esp_hosted's version request** `[M]`; the layer therefore does not ask (`WS7B_READ_C6_VERSION=0`), and `board_wifi_info_t.version_known` stays `false`. A transport failure restarts the board with `board_restart()` after 2 s, never with `esp_restart()`.

### Memory

LVGL draw buffer: 25 lines × 1024 px × 2 bytes × 2 buffers = **100 KiB** of internal RAM (the Tab5 uses 144 KiB for 50 lines of 720 px; 50 lines at 1024 px would be 200 KiB, too much for the internal-RAM budget of milestone 1b). Option `WS7B_DRAW_BUFFER_LINES` (default 25). The frame buffer and the C6 transport buffers are in PSRAM. Free internal RAM on the board: **not measured**.

### Partitions (32 MB)

`boards/waveshare-7b/partitions.csv`: otadata 0x9000, phy_init 0xb000, `ota_0` 0x10000 (4 MB), `ota_1` 0x410000 (4 MB), `nvs` 0x810000 (0x70000). **Nothing is declared beyond 0x880000.** The ESPHome firmware in service had its NVS (Wi-Fi, API key, settings) at **0x1f90000** (0x70000) — read in the table of an earlier backup of this unit (20/10/2026; the trial prints the table of the new backup before writing anything): `app0` 0x10000 + 0xfc0000, `app1` 0xfd0000 + 0xfc0000, `nvs` 0x1f90000 + 0x70000. Flashing the native firmware overwrites ESPHome's applications (hence the backup) but leaves that NVS alone.

### Build

PowerShell, never Git Bash (`AGENTS.md`). Switching board needs a new `sdkconfig`:

```powershell
Remove-Item sdkconfig -ErrorAction SilentlyContinue
.\tools\idf.ps1 "-DBOARD=waveshare-7b" set-target esp32p4
.\tools\idf.ps1 build
```

Options (CMake cache variables): `-DWS7B_ROTATION=0|180`, `-DWS7B_DRAW_BUFFER_LINES=25`, `-DWS7B_REAL_PANEL_OFF=1|0`, `-DWS7B_READ_C6_VERSION=0|1`, `-DWS7B_AUTOTEST_TACTILE=0|1`, `-DWS7B_ESSAI_AUDIO=0|1`. The two last ones make a **test binary**, never a release. Each board has its own component lock (`boards/waveshare-7b/dependencies.lock`; the Tab5 keeps the root `dependencies.lock`), and the other board's component is excluded from the build ([ADR-0005](decisions/0005-couche-carte.md)). Wi-Fi credentials are not in the repository: `tools\preparer_wifi.ps1` writes them into an ignored file read at build time.

### Pitfalls (each one cost time somewhere)

- **A successful build proves nothing about the board** — see the rule “Done” of `AGENTS.md`.
- **Do not use `esp_restart()` on the P4**: it resets the cores only (C6 link loop seen on this board); `board_restart()` calls `esp_rom_software_reset_system()`.
- **QIO flash was abandoned on this board**; DIO 80 MHz is what ESPHome uses and what is configured here `[M]`.
- **PSRAM at 200 MHz is needed**: without it about 30 “dsi underrun” per second `[S]`.
- **The backlight output is inverted**; forgetting it gives a full-light screen at 0 %.
- **`esptool read-flash` with the stub fails at fixed sectors** on the P4: read in 2 MB pieces, split by 8 on failure, 4 KB sectors with `--no-stub` as a last resort; the chip-side `verify-flash` MD5 is the independent proof. `tools/essai_7b.py` does all of this.
- **The serial port number changes with the USB plug**: find the board by its MAC, read from the chip and compared before any write. The MAC is never written in the repository.
- **Do not update the C6.**

### Discrepancies between sources (not decided by memory)

| Subject | Sources | Choice and what would change it |
|---|---|---|
| DSI lane rate | `[E]` 900, `[H]` 900 (what runs on this board), `[B]` 1000 | **900**. Try 1000 only if the screen stays black or flickers |
| Touch reset | `[S]` + `[H]` GPIO23; `[B]` not connected, shared with the display reset | the code pulses **GPIO23** (10 ms low, 60 ms settle) and gives the driver no reset pin. If GPIO23 drives something else, the pulse is harmless on an input but **not verified**: check on the board that the GT911 answers at 0x5D after the pulse |
| Factory C6 version | esp_hosted version request unanswered `[M]` | not asked |

### Tested without the board

`test/host/test_carte_7b.cpp` (34 tests) runs the pure part `components/bsp_waveshare_7b/include/ws7b_pur.h` — pins and constants, DPI timings and refresh rate, init table, rotation and touch arithmetic, backlight duty, panel off/on plan, volume curve, buffer size — compiled by the same header the board uses (C on the board, C++ `constexpr` on the PC, evaluated by `static_assert` at compile time and run again with AddressSanitizer + UBSan). `test/host/mutations_carte_7b.py` breaks the source in 33 ways; all 33 are detected. `test/host/controle_cartes.py` checks the partition tables (inside the flash, no overlap, application slots aligned on 64 KB, otadata 0x2000), that the SDIO/C6 pins of `sdkconfig.defaults.board` and of `ws7b_pur.h` agree, and that no MAC address, private IP address or PC path is in the board files. These tests prove the *arithmetic and the tables*; they say **nothing** about what the board does.

### Physical trial: the procedure (`tools/essai_7b.py`)

One command replays the whole trial and **always** restores the original firmware at the end:

```powershell
$env:MERIDIEN_MAC_7B = "<MAC of the 7B, read on the chip>"   # never written in the repository
python tools\essai_7b.py essai --port COMx --dossier <backup folder> --build build
```

It (1) reads the MAC of the given port and refuses to go on if it differs; (2) reads the **whole** flash twice (32 MB) and requires the same SHA-256, then asks the chip itself to confirm with `verify-flash` (MD5 computed in the chip), and writes `sauvegarde_ok.json`; (3) decodes and prints the partition table and `otadata` of the firmware in service; (4) writes the bootloader (0x2000), table (0x8000), otadata (0x9000) and application (0x10000) and verifies each in the chip; (5) records the boot log and checks the milestones (display, GT911, codecs, “Carte prête”, C6 link, IP, clock), then ten consecutive resets; (6) in a `finally`: rewrites the **whole** backup, verifies it in the chip and re-reads it (same SHA-256). It never erases the chip. If the restoration cannot be verified it says so and stops (exit code 3). `python tools\essai_7b.py autotest` checks the script's own logic without any board (52 checks): decoders, hashes, safeguards, and the whole trial — backup, flash, restoration — replayed against a **simulated board** (a flash in memory with a sector the stub cannot read, a flash that changes between two reads, a restoration that fails or silently alters a byte, a wrong MAC, a deadline already passed). `python test/host/mutations_essai_7b.py` breaks the script in 11 ways; all 11 are detected.

**State on 07/11/2026**: the script was written and its autotest passes; the part that talks to the board **has not been run**, because the 7B was not connected. See [mesures/7b-materiel.md](mesures/7b-materiel.md).

---

## Version française

---

> **Résumé.** Le firmware natif a une couche carte pour la Waveshare 7B (`components/bsp_waveshare_7b/` + `boards/waveshare-7b/`), choisie par la variable CMake `BOARD=waveshare-7b`. Elle implémente l'interface commune `components/board_api/include/board.h` **sans la modifier** : le noyau et l'application sont réutilisés tels quels. Elle **compile** (ESP-IDF 5.5.5, 1 837 760 octets, 56 % de l'emplacement d'application de 4 Mo libre) et ses parties pures sont **testées sur le PC** (34 tests, 33 mutations détectées). **Rien n'a encore été vu fonctionner sur la carte** : la 7B n'était pas branchée au PC pendant la session qui l'a écrite. Chaque valeur matérielle ci-dessous est une *lecture de source*, étiquetée `[S]`, `[B]`, `[E]`, `[H]`, `[M]` ou `[D]`, jamais une mesure, tant que [mesures/](mesures/) ne dit pas le contraire avec une ligne « Vu sur ».

**Règle de cette page.** Un fait est écrit avec sa source. « Non vérifié » veut dire qu'aucune source n'a été lue ou que la valeur n'a jamais été vue sur la carte. La 7B est l'écran en service chez l'auteur : on ne la flashe **que** sur sa demande explicite, après une sauvegarde complète de sa flash (procédure et script plus bas).

### Sources des faits

| Étiquette | Source |
|---|---|
| `[S]` | schéma Waveshare lu page par page — audit matériel n° 1 de l'auteur, 16/10/2026 (notes locales, hors dépôt) |
| `[B]` | BSP de Waveshare `waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.1 (registre des composants) : `include/bsp/*.h`, `*.c` |
| `[E]` | composant d'Espressif `esp_lcd_ek79007` 2.0.2 |
| `[H]` | firmware ESPHome en service sur cette carte depuis le 15/10/2026 (son paquet carte et le modèle `mipi_dsi` d'ESPHome pour cette carte, ESPHome 2026.9.1) |
| `[M]` | mesures de l'auteur sur cette carte, 16 au 19/10/2026 (comportement du C6, rétroéclairage, volume du haut-parleur) |
| `[D]` | `esp_codec_dev` 1.5.11, `device/es8311/es8311.c` |

### Ce que la carte contient

ESP32-P4 (révision v3.2, 400 MHz, lue par esptool le 20/10/2026 sur l'exemplaire de l'auteur — à relire pendant l'essai), flash de 32 Mo (identifiant JEDEC `c8 40 19` donné par l'auteur, à relire par `esptool flash-id` avant la sauvegarde ; la table du firmware ESPHome en service finit à 0x2000000 = 32 Mo), PSRAM hexadécimale, écran EK79007 de 7" 1024 × 600 en MIPI-DSI (2 voies), tactile GT911, codecs ES8311 (haut-parleur, ampli NS4150B) et ES7210 (microphones), un ESP32-C6 pour le Wi-Fi/Bluetooth en SDIO, un lecteur microSD, une entrée batterie. **Pas d'expandeur d'E/S** (le Tab5 en a un) : chaque ligne de commande est un GPIO du P4.

### Broches et bus

| Fonction | GPIO / adresse | Source | Remarque |
|---|---|---|---|
| I2C SDA / SCL | 7 / 8, 400 kHz, port 1 | `[S][B]` | GT911, ES8311 et ES7210 partagent ce bus |
| I2S MCLK / BCLK / LRCK / DOUT / DIN | 13 / 12 / 10 / 9 / 11 | `[S][B]` | DOUT → ES8311 (haut-parleur), DIN ← ES7210 (microphones) |
| Commande de l'ampli (NS4150B) | 53, haut = actif | `[S][B][H]` | maintenue basse jusqu'au premier son |
| Rétroéclairage | 32, LEDC timer 0 canal 1, 5 kHz, 10 bits, **sortie inversée** | `[S][B][H]` | gradation analogique : 0 % n'est **pas** noir (voir plus bas) |
| Reset de l'écran | 33 | `[S][B][H]` | fait par `esp_lcd_panel_reset` |
| Reset du tactile | 23 | `[S]` + `[H]` (`reset_pin: GPIO23`) | **écart** : `[B]` déclare le reset tactile non relié / partagé avec celui de l'écran. Le code envoie lui-même l'impulsion sur GPIO23 et ne donne aucune broche de reset au pilote — voir « Écarts » |
| Interruption du tactile | **non reliée au P4** (point de test TP1 seulement) | `[S]` | le tactile est interrogé (mode minuterie de LVGL) |
| SDIO du C6 (fente 1, 4 bits) | CLK 18, CMD 19, D0 à D3 = 14 à 17, 20 MHz | `[S][H]` | tampons du transport en PSRAM |
| Reset du C6 | 54, actif haut (R33, 0 Ω vers CHIP_PU) | `[S][H]` | fait par `esp_hosted_connect_to_slave()` à chaque appel |
| Console / port de flash | USB-Serial/JTAG du P4, GPIO24/25 (prise USB-C « USB ») | `[S]` | l'autre prise (pont CH343P sur UART0, GPIO37/38) n'est pas utilisée |
| Non touchées par cette couche | GPIO20 (tension batterie), 35 (bouton BOOT), 45 (alimentation microSD) | `[S]` | listées dans `ws7b_pur.h` pour que personne ne les réaffecte par erreur |

Adresses I2C (7 bits) : GT911 **0x5D** (puis **0x14** si INT était haute au reset), ES8311 **0x18**, ES7210 **0x40** `[S]`. `esp_codec_dev` attend la forme sur 8 bits (0x30, 0x80) `[D]`.

### Écran (EK79007)

| Élément | Valeur | Source |
|---|---|---|
| Résolution | 1024 × 600 en paysage natif, RGB565 | `[S][B]` |
| DSI | 2 voies, **900 Mbit/s** par voie, PHY alimentée par le LDO canal 3 à 2,5 V | `[E]`, `[H]` ; `[B]` dit 1000 — voir « Écarts » |
| DPI | 52 MHz ; impulsion / porche arrière / porche avant horizontaux 10/160/160 ; verticaux 1/23/12 → environ 60 Hz (calculé et testé sur le PC : `ws7b_refresh_mhz()`) | macro de `[E]`, identique dans `[B]` et `[H]` |
| Table d'initialisation | registres 0x80…0x86 = 8B, 78, 84, 88, A8, E3, 88, puis « sleep out » (0x11) + 120 ms ; le composant émet B2 (nombre de voies) en premier | `[E]`, même ensemble que `[H]` |
| Détection du contrôleur | **non faite à l'exécution, volontairement** : la 7B n'a qu'un modèle de dalle et aucune lecture d'identifiant DCS n'a jamais été essayée dessus. Le nom « EK79007 » est un fait de conception, pas une mesure | — |
| Tampon d'image | un seul ; LVGL dessine par bandes et la dalle est mise à jour par copie (DMA2D), comme sur le Tab5 | conception |

### Vraie extinction de l'écran (le rétroéclairage est analogique)

La chaîne BL_CTRL → filtre RC → AP3032 est analogique : à 0 % la dalle reste faiblement éclairée (constaté par l'auteur le 16/10/2026, `[M]`). `board_set_backlight_percent(0)` met donc aussi la **dalle elle-même** en veille, avec la recette validée par l'auteur sous ESPHome le 17/10/2026 (« l'écran est bien noir, il se rallume quand je le touche », `[M]`) : voile noir dessiné sur la couche système de LVGL, puis « display off » (0x28), puis « sleep in » (0x10) ; au réveil « sleep out » (0x11), 120 ms, « display on » (0x29), voile retiré, et **seulement ensuite** le rétroéclairage remonte. Le plan (étapes, ordre, délais) est la fonction pure `ws7b_panel_plan()`, testée sur le PC ; `ws7b_io.c` ne fait que l'exécuter. L'option de compilation `WS7B_REAL_PANEL_OFF=0` la désactive (le rétroéclairage descend alors seulement à 0 %). **Non vérifié sur la carte**, en particulier que le voile est dessiné avant la commande de la dalle et que le toucher la réveille.

### Tactile (GT911)

Le pilote `esp_lcd_touch_gt911` 1.2.1 reçoit `mirror_x = mirror_y = 1`, `swap_xy = 0`, étendue 1023 × 599 `[B][H]` : le BSP de Waveshare comme ESPHome inversent X et Y (le GT911 est monté à l'envers de la dalle ; sans cela « 1 appui reconnu en 5 min », `[H]`, essai du 14/10/2026). La **couche carte** transforme ensuite le point brut en point **logique** de l'interface (rotation, voir plus bas) et le borne dans la dalle avant de le donner à LVGL : l'application ne voit jamais de coordonnées brutes. L'identifiant produit (registre 0x8140) est lu au démarrage et doit être « 911 ». Le journal série écrit une ligne par appui : point brut sur la dalle 1024 × 600 → point logique de l'interface. Un binaire compilé avec `WS7B_AUTOTEST_TACTILE=1` injecte cinq appuis **simulés** (les quatre coins et le centre) par le même chemin 10 s après le démarrage et journalise `AUTOTEST appui k/5 … OK|ECART` : cela prouve le calcul de la rotation, **pas** que la puce renvoie les bons points (il faut un doigt).

### Orientation (rotation)

La dalle est en paysage natif. Option de compilation `WS7B_ROTATION`, **0 ou 180** seulement (autre valeur : la configuration s'arrête) :

| Valeur | Sens | Source |
|---|---|---|
| **0** (défaut) | image telle que la dalle la balaie. **C'est l'exemplaire de l'auteur**, monté à l'envers par rapport aux démos Waveshare | `[M]` (« ses builds = rotation 0 » depuis le 16/10/2026) |
| 180 | montage « normal » : le sens des démos Waveshare et le défaut public d'ESPHome | `[B]` (`ESP_LV_ADAPTER_ROTATE_180`), `[H]` (`carte_rotation: 180`) |

La rotation est logicielle (LVGL + accélérateur PPA du P4, comme le Tab5) ; le registre de sens de balayage de la dalle existe dans le composant mais n'a **jamais été essayé** sur cette carte (ESPHome le désactive). 90°/270° ne sont pas pris en charge. **Le défaut 0 est une décision prise à la place de l'auteur** et reste à confirmer sur l'exemplaire : si l'heure s'affiche à l'envers, compiler avec `"-DWS7B_ROTATION=180"`.

### Audio

ES8311 sur I2S0, mono, 16 kHz, 16 bits, par `esp_codec_dev` ; ES7210 pour les microphones (MIC1 + MIC2). **Le volume est plafonné** : le registre 0x32 de l'ES8311 va par pas de 0,5 dB, 0xBF = 0 dB, 0xFF = +32 dB, et le haut-parleur **sature** au-dessus de 0 dB (essai de l'auteur à l'oreille, 17/10/2026 : 100 % sature, 80 % un peu, 75 % propre, `[D][M]`). La couche écrit elle-même le registre, jamais au-dessus de 0xBF (= 75 % de l'échelle du registre), et un volume non nul ne descend jamais sous 0x66 (−44,5 dB) car en dessous « on n'entend rien » (auteur, 18/10/2026, `[M]`) ; le volume 0 coupe. La courbe `ws7b_volume_reg()` est testée sur le PC (jamais au-dessus de 0xBF, croissante, 0 coupe). Un binaire compilé avec `WS7B_ESSAI_AUDIO=1` joue une tonalité de 440 Hz de 300 ms à 60 % (registre 0x9B, environ −18 dB) puis rapporte le niveau du microphone. **Non vérifié sur la carte.** Remarque : `board.h` n'a pas encore d'API audio — le code audio est prêt dans la couche et attend le jalon 1c ; si l'interface doit grandir, la proposition va d'abord au responsable de la couche carte du Tab5.

### Wi-Fi par l'ESP32-C6

`esp_hosted` 2.12.12, `esp_wifi_remote` 1.6.3, `wifi_remote_over_eppp` 0.3.3 et `eppp_link` 1.1.5 sont **épinglés** dans `components/bsp_waveshare_7b/idf_component.yml`. **Le micrologiciel du C6 n'est jamais mis à jour par ce code** (décision de l'auteur, 16/10/2026 : « j'ai peur de bloquer l'écran »). Le C6 est alimenté en permanence : le second essai de `board_wifi_link_up()` est donc une nouvelle remise à zéro du C6 par GPIO54, pas une coupure d'alimentation. Le micrologiciel d'origine du C6 **ne répond pas à la requête de version d'esp_hosted** `[M]` ; la couche ne la pose donc pas (`WS7B_READ_C6_VERSION=0`) et `board_wifi_info_t.version_known` reste `false`. Une panne du transport redémarre la carte par `board_restart()` après 2 s, jamais par `esp_restart()`.

### Mémoire

Tampon de dessin LVGL : 25 lignes × 1024 px × 2 octets × 2 tampons = **100 Kio** de RAM interne (le Tab5 en prend 144 Kio pour 50 lignes de 720 px ; 50 lignes à 1024 px feraient 200 Kio, trop pour le budget de RAM interne du jalon 1b). Option `WS7B_DRAW_BUFFER_LINES` (25 par défaut). Le tampon d'image et les tampons du transport du C6 sont en PSRAM. RAM interne libre sur la carte : **non mesurée**.

### Partitions (32 Mo)

`boards/waveshare-7b/partitions.csv` : otadata 0x9000, phy_init 0xb000, `ota_0` 0x10000 (4 Mo), `ota_1` 0x410000 (4 Mo), `nvs` 0x810000 (0x70000). **Rien n'est déclaré au-delà de 0x880000.** Le firmware ESPHome en service avait sa NVS (Wi-Fi, clé d'API, réglages) à **0x1f90000** (0x70000) — lu dans la table d'une sauvegarde plus ancienne de cet exemplaire (20/10/2026 ; l'essai affiche la table de la nouvelle sauvegarde avant d'écrire quoi que ce soit) : `app0` 0x10000 + 0xfc0000, `app1` 0xfd0000 + 0xfc0000, `nvs` 0x1f90000 + 0x70000. Flasher le firmware natif écrase les applications d'ESPHome (d'où la sauvegarde) mais laisse cette NVS tranquille.

### Compiler

PowerShell, jamais Git Bash (`AGENTS.md`). Changer de carte demande un `sdkconfig` neuf :

```powershell
Remove-Item sdkconfig -ErrorAction SilentlyContinue
.\tools\idf.ps1 "-DBOARD=waveshare-7b" set-target esp32p4
.\tools\idf.ps1 build
```

Options (variables de cache CMake) : `-DWS7B_ROTATION=0|180`, `-DWS7B_DRAW_BUFFER_LINES=25`, `-DWS7B_REAL_PANEL_OFF=1|0`, `-DWS7B_READ_C6_VERSION=0|1`, `-DWS7B_AUTOTEST_TACTILE=0|1`, `-DWS7B_ESSAI_AUDIO=0|1`. Les deux dernières font un **binaire d'essai**, jamais une version. Chaque carte a son verrou de composants (`boards/waveshare-7b/dependencies.lock` ; le Tab5 garde le `dependencies.lock` de la racine) et le composant de l'autre carte est exclu de la compilation ([ADR-0005](decisions/0005-couche-carte.md)). Les identifiants Wi-Fi ne sont pas dans le dépôt : `tools\preparer_wifi.ps1` les écrit dans un fichier ignoré lu à la compilation.

### Pièges (chacun a coûté du temps quelque part)

- **Une compilation réussie ne prouve rien sur la carte** — voir la règle « Fait » d'`AGENTS.md`.
- **Pas d'`esp_restart()` sur le P4** : il ne remet à zéro que les cœurs (boucle du lien C6 vue sur cette carte) ; `board_restart()` appelle `esp_rom_software_reset_system()`.
- **Le QIO de la flash a été abandonné sur cette carte** ; le DIO 80 MHz est ce qu'utilise ESPHome et ce qui est réglé ici `[M]`.
- **La PSRAM à 200 MHz est nécessaire** : sans elle environ 30 « dsi underrun » par seconde `[S]`.
- **La sortie du rétroéclairage est inversée** ; l'oublier donne un écran à pleine lumière à 0 %.
- **`esptool read-flash` avec le stub échoue à des secteurs fixes** sur le P4 : lire par morceaux de 2 Mo, couper en 8 en cas d'échec, secteurs de 4 Ko en `--no-stub` en dernier recours ; le MD5 de `verify-flash` calculé par la puce est la preuve indépendante. `tools/essai_7b.py` fait tout cela.
- **Le numéro de port série change avec la prise USB** : retrouver la carte par sa MAC, lue sur la puce et comparée avant toute écriture. La MAC n'est jamais écrite dans le dépôt.
- **Ne pas mettre à jour le C6.**

### Écarts entre sources (non tranchés de mémoire)

| Sujet | Sources | Choix et ce qui le ferait changer |
|---|---|---|
| Débit de ligne DSI | `[E]` 900, `[H]` 900 (ce qui tourne sur cette carte), `[B]` 1000 | **900**. Essayer 1000 seulement si l'écran reste noir ou scintille |
| Reset du tactile | `[S]` + `[H]` GPIO23 ; `[B]` non relié, partagé avec le reset de l'écran | le code envoie l'impulsion sur **GPIO23** (10 ms bas, 60 ms de repos) et ne donne aucune broche de reset au pilote. Si GPIO23 commande autre chose, l'impulsion est sans effet sur une entrée mais **non vérifiée** : constater sur la carte que le GT911 répond en 0x5D après l'impulsion |
| Version du C6 d'origine | requête de version d'esp_hosted sans réponse `[M]` | non demandée |

### Testé sans la carte

`test/host/test_carte_7b.cpp` (34 tests) exécute la partie pure `components/bsp_waveshare_7b/include/ws7b_pur.h` — broches et constantes, minutages DPI et cadence, table d'initialisation, calcul de la rotation et du tactile, rapport cyclique du rétroéclairage, plan d'extinction/réveil de la dalle, courbe de volume, taille des tampons — compilée par le même en-tête que celui de la carte (C sur la carte, C++ `constexpr` sur le PC, évalué par `static_assert` à la compilation puis rejoué avec AddressSanitizer + UBSan). `test/host/mutations_carte_7b.py` casse le source de 33 façons ; les 33 sont détectées. `test/host/controle_cartes.py` vérifie les tables de partitions (dans la flash, sans chevauchement, emplacements d'application alignés sur 64 Ko, otadata de 0x2000), que les broches SDIO/C6 de `sdkconfig.defaults.board` et de `ws7b_pur.h` sont d'accord, et qu'aucune adresse MAC, adresse IP privée ou chemin du PC n'est dans les fichiers de la carte. Ces tests prouvent le *calcul et les tables* ; ils ne disent **rien** de ce que fait la carte.

### Essai physique : la procédure (`tools/essai_7b.py`)

Une seule commande rejoue tout l'essai et **restaure toujours** le firmware d'origine à la fin :

```powershell
$env:MERIDIEN_MAC_7B = "<MAC de la 7B, lue sur la puce>"   # jamais écrite dans le dépôt
python tools\essai_7b.py essai --port COMx --dossier <dossier de sauvegarde> --build build
```

Elle (1) lit la MAC du port donné et refuse de continuer si elle diffère ; (2) lit la flash **entière** deux fois (32 Mo) et exige le même SHA-256, puis demande à la puce de confirmer par `verify-flash` (MD5 calculé dans la puce) et écrit `sauvegarde_ok.json` ; (3) décode et affiche la table de partitions et l'`otadata` du firmware en service ; (4) écrit le bootloader (0x2000), la table (0x8000), l'otadata (0x9000) et l'application (0x10000) et vérifie chacun dans la puce ; (5) enregistre le journal de démarrage et contrôle les jalons (écran, GT911, codecs, « Carte prête », lien C6, IP, heure), puis dix remises à zéro consécutives ; (6) dans un `finally` : réécrit la sauvegarde **entière**, la vérifie dans la puce et la relit (même SHA-256). Elle n'efface jamais la puce. Si la restauration ne peut pas être vérifiée, elle le dit et s'arrête (code de sortie 3). `python tools\essai_7b.py autotest` vérifie la logique du script sans aucune carte (52 contrôles) : décodeurs, empreintes, garde-fous, et l'essai entier — sauvegarde, flash, restauration — rejoué contre une **carte simulée** (une flash en mémoire avec un secteur que le stub ne sait pas lire, une flash qui change entre deux lectures, une restauration qui échoue ou altère un octet sans le dire, une mauvaise MAC, une heure limite déjà passée). `python test/host/mutations_essai_7b.py` casse le script de 11 façons ; les 11 sont détectées.

**État au 07/11/2026** : le script est écrit et son autotest passe ; la partie qui parle à la carte **n'a pas été exécutée**, parce que la 7B n'était pas branchée. Voir [mesures/7b-materiel.md](mesures/7b-materiel.md).
