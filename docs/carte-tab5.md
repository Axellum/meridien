# M5Stack Tab5 board sheet: pins, buses, addresses, traps

## English · [Français](#fiche-de-la-carte-m5stack-tab5)

---

Written on 07/11/2026 for the **Tab5, ST7121 revision** (the development board). Code: [`components/bsp_tab5/`](../components/bsp_tab5/) (board layer) and [`components/core/`](../components/core/) (pure logic, tested on the PC). Common interface: [`components/board_api/include/board.h`](../components/board_api/include/board.h) ([ADR-0005](decisions/0005-couche-carte.md)).

**How to read the status of a fact.** *Source* = read in a file on 06–07/11/2026 (the file is named); *Seen* = observed on the board (the line “Seen on” of [`mesures/tab5-materiel.md`](mesures/tab5-materiel.md) gives board, commit and date); *Not verified* = nothing proves it yet. Nothing here comes from memory.

Sources: ESPHome firmware of the neighbouring Tab5 project (`paquets/tab5-hardware.yaml`, `tab5-sensors-diagnostics.yaml`, `tab5-sensors-domotique.yaml`, which run on THIS board), ESPHome 2026.9.0 components `es8388`, `es7210`, `rx8130`, `ina226`, Espressif BSP `m5stack_tab5` 1.3.2, community BSP `espp`.

### Buses and chips

| Bus / chip | Pins or address | Role | Status |
|---|---|---|---|
| I2C (`I2C_NUM_1`) | SDA GPIO31, SCL GPIO32, 400 kHz | everything below except the audio data | Source |
| Expander PI4IOE5V6408 | I2C **0x43** | bit 0 antenna choice (**not touched**), bit 1 `SPEAKER_EN`, bit 2 external 5 V (**not touched**), bit 4 `LCD_RST`, bit 5 `TOUCH_RST`, bit 7 headphone detect (input, 1 = plugged) | Source |
| Expander PI4IOE5V6408 | I2C **0x44** | bit 0 `WIFI_EN` (power of the ESP32-C6), bit 3 USB 5 V (**not touched**), bit 5 `nCHG_QC_EN` (fast charge, active low), bit 6 `CHG_STAT` (input), bit 7 `CHG_EN` (charger) | Source |
| ES8388 audio DAC | I2C **0x10** | speaker and headphones (outputs LOUT1/ROUT1, LOUT2/ROUT2) | Source + Seen: **the speaker is on LINE1** (LINE2 alone: nothing heard by the microphones); headphone path (LINE2) not tried |
| ES7210 microphone ADC | I2C **0x40** | the two microphones | Source + Seen: identity registers 0x3D = 0x72, 0x3E = 0x10 |
| INA226 current monitor | I2C **0x41** | battery connector: bus voltage, shunt 5 mΩ, up to 8.192 A (LSB 250 µA, calibration 4096) | Source + Seen: manufacturer 0x5449 and die 0x2260 as expected |
| RX8130CE clock | I2C **0x32** | hardware clock, UTC, backed by a supercapacitor | Source |
| Sitronix touch | I2C 0x55, interrupt GPIO23 | version register 1 = ST7121, 3 = ST7123 | Source + Seen (earlier milestones) |
| Audio I2S (one port, shared) | MCLK GPIO30, BCLK GPIO27, WS GPIO29, DOUT GPIO26 (to the ES8388), DIN GPIO28 (from the ES7210) | playback and capture share the three clocks | Source |
| Backlight | GPIO22, LEDC, 10 bits, 1 kHz | brightness | Source |
| ESP32-C6 reset | GPIO15 | pulse produced by `esp_hosted` | Source |

### Backlight: perceived brightness

The application asks for a **perceived level 0–100**, not a duty cycle. `board_set_backlight_percent(50)` looks “half bright”: the duty cycle follows a power law (exponent 2.5, integer arithmetic, [`luminosite.hpp`](../components/core/include/ecran/luminosite.hpp)) between a floor and a ceiling that belong to the board.

- **Floor 2 % of the duty cycle (level 1)** and **ceiling 100 % (level 100)**: the floor is an **estimate to be judged by eye** (no flicker? a clean start-up?), never measured without a camera. *Not verified.*
- Level 0 = backlight off.
- **Seen** (duty cycle read back from the LEDC, not a software copy): 0 % → 0 ‰, 1 % → 20 ‰, 5 % → 21 ‰, 10 % → 23 ‰, 25 % → 51 ‰, 50 % → 193 ‰, 75 % → 497 ‰, 100 % → 1000 ‰. **Weak point of this curve: levels 1 to 10 give 20 to 23 ‰, so a slider would show no visible step there.** Left as is (a decision for the author, to judge by eye: the alternative is an exponential law that spreads the low levels).
- A Tab5 on a weak USB port already **restarted by brown-out at 100 %** under ESPHome (08/10/2026, neighbouring project): if it happens here, lower `kCourbeTab5` in [`tab5_ecran.cpp`](../components/bsp_tab5/tab5_ecran.cpp) (one line) rather than in the application.
- After a restart the level is 0 and the application switches the backlight on once its first frame is drawn (as before). Nothing is stored here between two starts, so this layer cannot leave the screen off after a restart.

### Sleep (dimming, switching off, time windows)

Pure decision in [`veille.hpp`](../components/core/include/ecran/veille.hpp); mechanism in [`tab5_ecran.cpp`](../components/bsp_tab5/tab5_ecran.cpp).

- After `dim_after_s` without touch: dimmed to `dim_level`. After `off_after_s`: off. Time windows (up to 4, local time) force the screen off; **a window that crosses midnight belongs to the day it starts** (a Friday 22:00 → 06:00 window covers Saturday 02:00). A touch inside a window lights the screen for `relight_after_s`.
- The board does not know the time zone: the application gives the **local** time (`board_sleep_set_local_time_source`). Without it the windows are ignored.
- **Off = backlight at 0 and an opaque black veil on LVGL's system layer.** The veil exists so that the touch which *wakes* the screen does not press the button that is underneath in the dark; on wake it turns transparent but stays clickable until the finger is released, then it is removed. A 100 ms LVGL timer does the job, no extra task.
- **No panel sleep command (DCS 0x28/0x10)**: not tried on ST7121/ST7123 (touch and display are one chip, TDDI). To revisit only if the consumption of the switched-off panel matters.
- **Seen** (on-board test with a 2 s / 4 s setting): dimmed after 2100 ms, off 1900 ms later, awake again 100 ms after the wake call; a 22:00 → 06:00 window gave “off” at 23:30 and at 02:00, “on” at 12:00; a Friday-only window gave “off” on Saturday 02:00 and “on” on Friday 02:00; a touch inside a window relit the screen and it went off again 3000 ms later. **Not seen: a real finger on the veil** (the test calls the wake function; the physical touch-wake test is still to do).

### Hardware clock (RX8130CE)

Rules and tests: [`horloge_materielle.hpp`](../components/core/include/ecran/horloge_materielle.hpp); chip access: [`tab5_rtc.cpp`](../components/bsp_tab5/tab5_rtc.cpp).

- **At start-up** the system takes the chip's time **only if** the chip is readable, the time is plausible (2026–2099) and the chip did not report a power loss, **and** the system has no plausible time of its own.
- **The chip is written only from network time** (first synchronisation, then at most every 6 h, or at once if the chip is more than 2 s off), **never from itself**. It never overwrites a more recent network time. The SNTP callback slot belongs to the application, so the board layer **observes** the system clock every 5 s: a jump of more than 2 s, or a change from “absurd” to “plausible” that it did not cause, means “the network set the time”. `board_rtc_note_network_time()` lets the application say so at once.
- **Difference with ESPHome**: ESPHome clears the flag register (0x1D) at every start-up and so loses the information “the chip lost its power”. Here the flag is **read first** and cleared only after a network write.
- The bit that means “voltage loss” (VLF) is taken as **bit 1** of 0x1D (Linux driver `rtc-rx8130`): **not verified** on the datasheet; the raw value is logged at start-up, and the plausibility check protects anyway (a chip that lost its backup restarts before 2026 and is never used).
- **Seen**: the chip answers at 0x32; it ticks at 1 Hz (+2 s over 2.1 s); the flag register read 0x21 (no power loss); a written value is read back unchanged; 31/12/2027 23:59:58 rolls over to 2028 and 28/02/2028 23:59:58 gives 29/02 (leap year handled); the original time is restored within ±2 s after the test.
- **Seen, indirect**: after a software reset without network the system time was still plausible, so `time()` **survives `esp_rom_software_reset_system()`** (the chip's time was then not taken back, as designed). A power cut with no network, where the chip takes over, has **not been done**.

### Audio

Pure parts (volume curve, levels in dBFS, Goertzel, test tone) in [`volume.hpp`](../components/core/include/ecran/volume.hpp) and [`niveau_audio.hpp`](../components/core/include/ecran/niveau_audio.hpp); drivers in [`tab5_codec.c`](../components/bsp_tab5/tab5_codec.c), bus in [`tab5_i2s.c`](../components/bsp_tab5/tab5_i2s.c), policy in [`tab5_audio.cpp`](../components/bsp_tab5/tab5_audio.cpp).

- **One rate for the whole bus: 16 000 Hz, stereo, 16 bits**, MCLK = 256 × fs. Playback and capture share MCLK, BCLK and WS, so they cannot run at different rates at the same time (the ESPHome firmware plays at 48 kHz and listens at 16 kHz, hence its “no duplex”). The codecs also know 48 kHz (coefficient table); the board layer does not expose it yet.
- **Own minimal drivers** instead of `esp_codec_dev`: that component is not in the repository, adding it changes `dependencies.lock` (shared with another session) and weighs more than needed. The start-up sequences are those of the ESPHome components, which run on this board.
- **Lazy opening**: nothing is configured and no RAM is taken until the first audio call. The I2S DMA buffers are small (4 × 160 frames per direction) because the DMA needs internal RAM; recordings and the sine table are in PSRAM. The exact internal RAM taken is measured at opening (`board_audio_get_info().internal_ram_taken`).
- **The amplifier (`SPEAKER_EN`) is on only while a sound plays.** The DAC is muted between two sounds.
- **Perceived volume 0–100** (0 = mute, 1 = −50 dB, 100 = 0 dB, quadratic in dB), mapped to the DAC's 0.5 dB steps, with a **ceiling of −6 dB** for the Tab5 (an estimate of prudence, to be set by ear: the speaker is a few centimetres from the microphones). Limit of the curve, documented and tested: above level 50 the curve is finer than the DAC's step, so **levels 93 to 100 give the same attenuation**.
- The ES8388 ADC is kept powered as in ESPHome (the DAC takes its LRCK from it).
- **Speaker output line: LINE1 (Seen).** A 1 kHz tone at level 25 was heard by the microphones about 20 dB above their noise floor on LINE1 alone and on LINE1 + LINE2, and not at all on LINE2 alone. The code still enables both by default (LINE2 is probably the headphone path: **not tried, not verified**).
- **Playback and capture at the same time work on this single port at 16 kHz (Seen).** 2.5 s of 1 kHz tone out while recording in: 40 640 frames written = 40 640 frames read, 0 overflow in either direction, slowest block 9–10 ms, and the tone was heard by the microphones. The ESPHome neighbour's “no duplex” comes from its two different rates (48 kHz out, 16 kHz in) on the shared clocks, not from the hardware. *Limit:* one 2.5 s run per line, no long-run test; the echo of the speaker in the microphones is of course present (no cancellation here).
- **Receiving alone times out; receiving enables sending too.** With only the receive channel enabled the read timed out every time (cause not elucidated); `tab5_i2s_enable()` therefore enables the transmit channel whenever the receive one is enabled (it sends silence, the DAC is muted and the amplifier off). Measured effect: none audible.
- **Internal RAM** (command `memoire`, free internal RAM, largest block): 122 159 B / 53 248 B before the first audio call, 115 759 B with the bus open (**6 224 B taken**), 121 943 B after closing (6 184 B returned; the 40 B difference is the I2C device handles). The PSRAM buffers do not count.
- **Recording**: 1 s (16 000 stereo frames) into PSRAM, noise floor about −66 dBFS rms, peak −55 dBFS, no clipping. Sensitivity to real speech: not measured (nobody spoke).

### Power

[`alimentation.hpp`](../components/core/include/ecran/alimentation.hpp) (conversions and state) and [`tab5_alim.cpp`](../components/bsp_tab5/tab5_alim.cpp).

- **“The author's unit has no battery” is NOT confirmed by the measurement.** The voltage read is the one of the **battery connector**, not of the USB-C port. Without battery, charger off, the ESPHome firmware read 1.83–1.94 V; with the charger on, 4.2 V, 5.7 V or an oscillation 4.2 ↔ 8.4 V. **On 07/11/2026 this firmware read 8 206 mV and +2 mA with the charger forced off** (identical over the whole test, INA226 identity registers correct). That is what a two-cell lithium pack nearly full and idle would show, and it is not what an empty connector showed under ESPHome. Either a battery is now connected, or something else holds the connector at 8.2 V: **to be looked at physically (open the back)**. Until then the state reads “idle” and the application must not infer anything from it. “A battery is present” means only **> 5.5 V with the charger off**.
- **Nothing tells whether the USB-C port is powered.** The API does not pretend to.
- This firmware **never switches the charger on**: at start-up `CHG_EN` is driven low and fast charge (`nCHG_QC_EN`, active low) is stopped, as in the ESPHome firmware. The state of these pins **as found** is logged before they are forced. `CHG_STAT` is read with a pull-down (as in ESPHome) and, without battery, ESPHome saw it announce “charging”: do not rely on it. Mounting a battery later is a decision to take (ADR), not something this layer does by itself.
- Current sign: **negative = charging**, positive = discharging (M5Unified).

### Traps

- **The expander 0x43 needs about 1 s after power-up** before the screen reset is reliable (kept in `board_init`), and creating its driver **resets** it. Writes from several tasks go through one lock ([`tab5_io.c`](../components/bsp_tab5/tab5_io.c)).
- **Do not touch** expander 0x43 bits 0 and 2 and 0x44 bit 3 (antenna, external 5 V, USB 5 V).
- **PI4IOE5V6408 registers (read in the driver, three traps seen on the board).** (1) After a reset every pin is an input and in high impedance. A pin counts as an input as long as its high-impedance bit (0x07) is 1, whatever the direction bit (0x03) says. (2) The `esp_io_expander` driver **refuses to set the level of a pin whose cached direction is input** (“can't set level in input mode”): set the direction first, then the level, then the push-pull mode. The pin takes the output register's value (0 after a reset) for an instant. (3) **The pin state register (0x0F) does not show a pin driven as an output**: SPEAKER_EN read 0 while the speaker was sounding. To check an output, read direction (0x03), output (0x05) and high impedance (0x07) (`tab5_expander_dump()`): during a tone, 0x43 reads dir = 0x6F, output = 0x22, high impedance = 0xDD, i.e. bit 1 is an output, high, not in high impedance.
- **Registers as found at start-up** (07/11/2026): expander 0x43 dir 0xEF, output 0x20, high impedance 0xDF, pull-up 0x10, state 0x10; expander 0x44 dir 0x9F, output 0x01, high impedance 0x7E, state 0x20 (`CHG_STAT` 0, `CHG_EN` 0, `nCHG_QC_EN` 1).
- **Never update the C6** (decision of 16/10/2026).
- **Software restart**: end with `esp_rom_software_reset_system()`; on the P4 `esp_restart()` resets the cores only.
- Opening or closing the USB serial port can reset the chip.
- The serial port number changes with the USB socket: find the board by its MAC, read on the chip.

### What the on-board trial settled, and what it did not

Results, board, commit and date: [`mesures/tab5-materiel.md`](mesures/tab5-materiel.md). **Settled (Seen, 07/11/2026):** the speaker is on LINE1; playback and capture run together at 16 kHz on the single port; the audio bus takes 6 224 bytes of internal RAM while open; `time()` survives a software reset; the INA226 and ES7210 identity registers; the registers of the two expanders. **Not settled:** the meaning of the RX8130 voltage-loss bit, the polarity of `CHG_STAT` (it read 0 with the charger off, nothing more), whether a battery is connected (8.2 V read), the 2 % backlight floor by eye, the 100 % ceiling on a weak USB port, the headphone path, the real touch on the sleep veil, a power cut with the RTC taking over. **Not started:** the SD card and the motion sensor.

---

## Fiche de la carte M5Stack Tab5

[English](#m5stack-tab5-board-sheet-pins-buses-addresses-traps) · Français

---

Écrite le 07/11/2026 pour le **Tab5, révision ST7121** (la carte de développement). Code : [`components/bsp_tab5/`](../components/bsp_tab5/) (couche carte) et [`components/core/`](../components/core/) (logique pure, testée sur le PC). Interface commune : [`components/board_api/include/board.h`](../components/board_api/include/board.h) ([ADR-0005](decisions/0005-couche-carte.md)).

**Comment lire l'état d'un fait.** *Source* = lu dans un fichier les 06–07/11/2026 (le fichier est nommé) ; *Constaté* = observé sur la carte (la ligne « Vu sur » de [`mesures/tab5-materiel.md`](mesures/tab5-materiel.md) donne carte, commit et date) ; *Non vérifié* = rien ne le prouve encore. Rien ici ne vient de la mémoire.

Sources : firmware ESPHome du projet voisin (`paquets/tab5-hardware.yaml`, `tab5-sensors-diagnostics.yaml`, `tab5-sensors-domotique.yaml`, qui tournent sur CETTE carte), composants ESPHome 2026.9.0 `es8388`, `es7210`, `rx8130`, `ina226`, BSP d'Espressif `m5stack_tab5` 1.3.2, BSP communautaire `espp`.

### Bus et puces

| Bus / puce | Broches ou adresse | Rôle | État |
|---|---|---|---|
| I2C (`I2C_NUM_1`) | SDA GPIO31, SCL GPIO32, 400 kHz | tout ce qui suit sauf les données audio | Source |
| Expandeur PI4IOE5V6408 | I2C **0x43** | bit 0 choix de l'antenne (**non touché**), bit 1 `SPEAKER_EN`, bit 2 5 V externe (**non touché**), bit 4 `LCD_RST`, bit 5 `TOUCH_RST`, bit 7 détection de la prise casque (entrée, 1 = fiche insérée) | Source |
| Expandeur PI4IOE5V6408 | I2C **0x44** | bit 0 `WIFI_EN` (alimentation de l'ESP32-C6), bit 3 USB 5 V (**non touché**), bit 5 `nCHG_QC_EN` (charge rapide, actif bas), bit 6 `CHG_STAT` (entrée), bit 7 `CHG_EN` (chargeur) | Source |
| DAC audio ES8388 | I2C **0x10** | haut-parleur et casque (sorties LOUT1/ROUT1, LOUT2/ROUT2) | Source + Constaté : **le haut-parleur est sur LINE1** (LINE2 seule : rien entendu par les microphones) ; chemin du casque (LINE2) non essayé |
| CAN des microphones ES7210 | I2C **0x40** | les deux microphones | Source + Constaté : registres d'identité 0x3D = 0x72, 0x3E = 0x10 |
| Moniteur de courant INA226 | I2C **0x41** | connecteur de batterie : tension du bus, shunt 5 mΩ, jusqu'à 8,192 A (LSB 250 µA, étalonnage 4096) | Source + Constaté : fabricant 0x5449 et puce 0x2260 conformes |
| Horloge RX8130CE | I2C **0x32** | horloge matérielle, UTC, secourue par un supercondensateur | Source |
| Tactile Sitronix | I2C 0x55, interruption GPIO23 | registre de version 1 = ST7121, 3 = ST7123 | Source + Constaté (jalons précédents) |
| Audio I2S (un port, partagé) | MCLK GPIO30, BCLK GPIO27, WS GPIO29, DOUT GPIO26 (vers l'ES8388), DIN GPIO28 (depuis l'ES7210) | lecture et enregistrement partagent les trois horloges | Source |
| Rétroéclairage | GPIO22, LEDC, 10 bits, 1 kHz | luminosité | Source |
| Reset de l'ESP32-C6 | GPIO15 | impulsion produite par `esp_hosted` | Source |

### Rétroéclairage : luminosité perçue

L'application demande un **niveau perçu de 0 à 100**, pas un rapport cyclique. `board_set_backlight_percent(50)` « paraît à moitié » : le rapport cyclique suit une loi de puissance (exposant 2,5, arithmétique entière, [`luminosite.hpp`](../components/core/include/ecran/luminosite.hpp)) entre un plancher et un plafond propres à la carte.

- **Plancher 2 % du rapport cyclique (niveau 1)** et **plafond 100 % (niveau 100)** : le plancher est une **estimation à juger à l'œil** (pas de scintillement ? allumage franc ?), jamais mesurée sans caméra. *Non vérifié.*
- Niveau 0 = rétroéclairage éteint.
- **Constaté** (rapport cyclique relu dans le LEDC, pas une copie logicielle) : 0 % → 0 ‰, 1 % → 20 ‰, 5 % → 21 ‰, 10 % → 23 ‰, 25 % → 51 ‰, 50 % → 193 ‰, 75 % → 497 ‰, 100 % → 1000 ‰. **Point faible de cette courbe : les niveaux 1 à 10 donnent 20 à 23 ‰, donc un curseur n'y montrerait aucun cran visible.** Laissé tel quel (décision pour l'auteur, à juger à l'œil : l'alternative est une loi exponentielle qui étale le bas de l'échelle).
- Un Tab5 sur un port USB faible a déjà **redémarré par chute de tension à 100 %** sous ESPHome (08/10/2026, projet voisin) : si cela se reproduit ici, baisser `kCourbeTab5` dans [`tab5_ecran.cpp`](../components/bsp_tab5/tab5_ecran.cpp) (une ligne) plutôt que dans l'application.
- Après un redémarrage le niveau est 0 et l'application rallume le rétroéclairage une fois sa première image dessinée (comme avant). Rien n'est gardé ici d'un démarrage à l'autre : cette couche ne peut donc pas laisser l'écran éteint après un redémarrage.

### Veille (atténuation, extinction, plages horaires)

Décision pure dans [`veille.hpp`](../components/core/include/ecran/veille.hpp) ; mécanisme dans [`tab5_ecran.cpp`](../components/bsp_tab5/tab5_ecran.cpp).

- Après `dim_after_s` sans toucher : atténué à `dim_level`. Après `off_after_s` : éteint. Les plages horaires (4 au plus, heure locale) forcent l'extinction ; **une plage qui passe minuit appartient au jour où elle commence** (une plage du vendredi 22 h → 6 h couvre le samedi 2 h). Un toucher dans une plage rallume l'écran pendant `relight_after_s`.
- La carte ne connaît pas le fuseau horaire : l'application donne l'heure **locale** (`board_sleep_set_local_time_source`). Sans elle, les plages sont ignorées.
- **Éteint = rétroéclairage à 0 et un voile noir opaque sur la couche système de LVGL.** Le voile sert à ce que le toucher qui *réveille* l'écran n'appuie pas sur le bouton qui se trouve dessous dans le noir ; au réveil il devient transparent mais reste cliquable jusqu'au relâchement du doigt, puis il est retiré. Un minuteur LVGL de 100 ms fait le travail, pas de tâche en plus.
- **Pas de commande de veille de la dalle (DCS 0x28/0x10)** : non essayée sur ST7121/ST7123 (tactile et affichage sont une seule puce, TDDI). À reprendre seulement si la consommation de la dalle éteinte devient un sujet.
- **Constaté** (essai sur carte avec un réglage 2 s / 4 s) : atténué après 2100 ms, éteint 1900 ms plus tard, réveillé 100 ms après l'appel de réveil ; une plage 22 h → 6 h a donné « éteint » à 23 h 30 et à 2 h, « allumé » à 12 h ; une plage du vendredi seul a donné « éteint » le samedi à 2 h et « allumé » le vendredi à 2 h ; un toucher dans une plage a rallumé l'écran, qui s'est réteint 3000 ms plus tard. **Non constaté : un vrai doigt sur le voile** (l'essai appelle la fonction de réveil ; l'essai physique du réveil au toucher reste à faire).

### Horloge matérielle (RX8130CE)

Règles et tests : [`horloge_materielle.hpp`](../components/core/include/ecran/horloge_materielle.hpp) ; accès à la puce : [`tab5_rtc.cpp`](../components/bsp_tab5/tab5_rtc.cpp).

- **Au démarrage** le système prend l'heure de la puce **seulement si** la puce est lisible, l'heure plausible (2026–2099) et la puce n'a pas signalé de perte d'alimentation, **et** que le système n'a pas d'heure plausible à lui.
- **La puce n'est écrite que depuis l'heure du réseau** (première synchronisation, puis au plus toutes les 6 h, ou aussitôt si la puce s'écarte de plus de 2 s), **jamais depuis elle-même**. Elle n'écrase jamais une heure du réseau plus récente. Le rappel SNTP appartient à l'application : la couche carte **observe** donc l'horloge du système toutes les 5 s ; un saut de plus de 2 s, ou un passage de « absurde » à « plausible » qu'elle n'a pas causé, vaut « le réseau a réglé l'heure ». `board_rtc_note_network_time()` permet à l'application de le dire aussitôt.
- **Différence avec ESPHome** : ESPHome efface le registre des drapeaux (0x1D) à chaque démarrage et perd donc l'information « la puce a perdu son alimentation ». Ici le drapeau est **lu d'abord** et n'est effacé qu'après une écriture venue du réseau.
- Le bit « perte de tension » (VLF) est pris pour le **bit 1** de 0x1D (pilote Linux `rtc-rx8130`) : **non vérifié** sur la fiche ; la valeur brute est journalisée au démarrage et la plausibilité protège de toute façon (une puce qui a perdu sa pile repart avant 2026 et n'est jamais reprise).
- **Constaté** : la puce répond en 0x32 ; elle compte à 1 Hz (+2 s en 2,1 s) ; le registre des drapeaux valait 0x21 (pas de perte d'alimentation) ; une valeur écrite est relue à l'identique ; 31/12/2027 23:59:58 passe en 2028 et 28/02/2028 23:59:58 donne le 29/02 (année bissextile bien traitée) ; l'heure d'origine est rendue à ±2 s après l'essai.
- **Constaté, indirect** : après un redémarrage logiciel sans réseau, l'heure du système était encore plausible, donc `time()` **survit à `esp_rom_software_reset_system()`** (l'heure de la puce n'est alors pas reprise, comme prévu). Une coupure franche du courant sans réseau, où la puce reprend la main, **n'a pas été faite**.

### Audio

Parties pures (courbe de volume, niveaux en dBFS, Goertzel, sinus de test) dans [`volume.hpp`](../components/core/include/ecran/volume.hpp) et [`niveau_audio.hpp`](../components/core/include/ecran/niveau_audio.hpp) ; pilotes dans [`tab5_codec.c`](../components/bsp_tab5/tab5_codec.c), bus dans [`tab5_i2s.c`](../components/bsp_tab5/tab5_i2s.c), politique dans [`tab5_audio.cpp`](../components/bsp_tab5/tab5_audio.cpp).

- **Une seule cadence pour tout le bus : 16 000 Hz, stéréo, 16 bits**, MCLK = 256 × fs. Lecture et enregistrement partagent MCLK, BCLK et WS : pas de cadences différentes en même temps (le firmware ESPHome joue à 48 kHz et écoute à 16 kHz, d'où son « pas de duplex »). Les codecs savent aussi 48 kHz (table de coefficients) ; la couche carte ne l'expose pas encore.
- **Pilotes minimaux écrits ici** plutôt que `esp_codec_dev` : ce composant n'est pas dans le dépôt, l'ajouter change `dependencies.lock` (partagé avec une autre session) et pèse plus que nécessaire. Les séquences de mise en route sont celles des composants ESPHome, qui tournent sur cette carte.
- **Ouverture paresseuse** : rien n'est configuré et aucune RAM n'est prise avant le premier appel audio. Les tampons DMA de l'I2S sont petits (4 × 160 trames par sens) car le DMA exige la RAM interne ; les enregistrements et la table du sinus sont en PSRAM. La RAM interne réellement prise est mesurée à l'ouverture (`board_audio_get_info().internal_ram_taken`).
- **L'amplificateur (`SPEAKER_EN`) n'est allumé que pendant un son.** Le DAC est en sourdine entre deux sons.
- **Volume perçu 0–100** (0 = muet, 1 = −50 dB, 100 = 0 dB, quadratique en dB), converti en pas de 0,5 dB du DAC, avec un **plafond de −6 dB** pour le Tab5 (estimation de prudence, à régler à l'oreille : le haut-parleur est à quelques centimètres des microphones). Limite de la courbe, documentée et testée : au-dessus du niveau 50 la courbe est plus fine que le pas du DAC, donc **les niveaux 93 à 100 donnent la même atténuation**.
- Le CAN de l'ES8388 reste alimenté comme dans ESPHome (le DAC y prend son LRCK).
- **Ligne de sortie du haut-parleur : LINE1 (constaté).** Un son de 1 kHz au niveau 25 a été entendu par les microphones environ 20 dB au-dessus de leur bruit de fond sur LINE1 seule et sur LINE1 + LINE2, et pas du tout sur LINE2 seule. Le code active quand même les deux par défaut (LINE2 est probablement le chemin du casque : **non essayé, non vérifié**).
- **Lecture et enregistrement en même temps fonctionnent sur ce port unique à 16 kHz (constaté).** 2,5 s de son de 1 kHz en sortie pendant l'enregistrement : 40 640 trames écrites = 40 640 trames lues, 0 dépassement dans les deux sens, bloc le plus lent 9–10 ms, et le son a été entendu par les microphones. Le « pas de duplex » du voisin ESPHome vient de ses deux cadences différentes (48 kHz en sortie, 16 kHz en entrée) sur les horloges communes, pas du matériel. *Limite :* une seule passe de 2,5 s par ligne, pas d'essai de longue durée ; l'écho du haut-parleur est évidemment présent dans les microphones (pas d'annulation ici).
- **La réception seule expire ; la réception active aussi l'émission.** Avec seulement le canal de réception activé, la lecture expirait à chaque fois (cause non élucidée) ; `tab5_i2s_enable()` active donc le canal d'émission dès que celui de réception l'est (il envoie du silence, le DAC est en sourdine et l'amplificateur éteint). Effet mesuré : rien d'audible.
- **RAM interne** (commande `memoire`, RAM interne libre, plus gros bloc) : 122 159 o / 53 248 o avant le premier appel audio, 115 759 o bus ouvert (**6 224 o pris**), 121 943 o après fermeture (6 184 o rendus ; les 40 o d'écart sont les descripteurs I2C). Les tampons de la PSRAM ne comptent pas.
- **Enregistrement** : 1 s (16 000 trames stéréo) dans la PSRAM, bruit de fond d'environ −66 dBFS rms, crête −55 dBFS, sans écrêtage. Sensibilité à une vraie voix : non mesurée (personne n'a parlé).

### Alimentation

[`alimentation.hpp`](../components/core/include/ecran/alimentation.hpp) (conversions et état) et [`tab5_alim.cpp`](../components/bsp_tab5/tab5_alim.cpp).

- **« L'exemplaire d'Axel n'a pas de batterie » n'est PAS confirmé par la mesure.** La tension lue est celle du **connecteur de batterie**, pas du port USB-C. Sans batterie, chargeur coupé, le firmware ESPHome y a lu 1,83 à 1,94 V ; chargeur allumé, 4,2 V, 5,7 V ou une oscillation 4,2 ↔ 8,4 V. **Le 07/11/2026 ce firmware a lu 8 206 mV et +2 mA avec le chargeur forcé à l'arrêt** (identique pendant tout l'essai, registres d'identité de l'INA226 corrects). C'est ce que montrerait un bloc lithium à deux éléments presque plein au repos, et pas ce que montrait un connecteur vide sous ESPHome. Soit une batterie est branchée depuis, soit autre chose tient le connecteur à 8,2 V : **à regarder physiquement (dos ouvert)**. D'ici là l'état se lit « au repos » et l'application ne doit rien en déduire. « Une batterie est présente » veut dire seulement **> 5,5 V chargeur coupé**.
- **Aucune puce ne dit si le port USB-C est alimenté.** L'API ne prétend pas le faire.
- Ce firmware **n'allume jamais le chargeur** : au démarrage `CHG_EN` est forcé bas et la charge rapide (`nCHG_QC_EN`, actif bas) arrêtée, comme le firmware ESPHome. L'état de ces broches **tel que trouvé** est journalisé avant qu'elles soient forcées. `CHG_STAT` est lu avec un tirage vers le bas (comme ESPHome) et, sans batterie, ESPHome l'a vu annoncer « en charge » : ne pas s'y fier. Monter une batterie plus tard est une décision à prendre (ADR), pas quelque chose que cette couche fait d'elle-même.
- Sens du courant : **négatif = charge**, positif = décharge (M5Unified).

### Pièges

- **L'expandeur 0x43 a besoin d'environ 1 s après la mise sous tension** avant que le reset de l'écran soit fiable (conservé dans `board_init`), et créer son pilote le **remet à zéro**. Les écritures depuis plusieurs tâches passent par un seul verrou ([`tab5_io.c`](../components/bsp_tab5/tab5_io.c)).
- **Ne pas toucher** aux bits 0 et 2 de l'expandeur 0x43 ni au bit 3 du 0x44 (antenne, 5 V externe, 5 V USB).
- **Registres du PI4IOE5V6408 (lus dans le pilote, trois pièges vus sur la carte).** (1) Après une remise à zéro toutes les broches sont des entrées en haute impédance. Une broche compte comme entrée tant que son bit de haute impédance (0x07) vaut 1, quoi que dise le bit de direction (0x03). (2) Le pilote `esp_io_expander` **refuse d'écrire le niveau d'une broche dont la direction mémorisée est « entrée »** (« can't set level in input mode ») : poser la direction d'abord, puis le niveau, puis le mode push-pull. La broche prend un instant la valeur du registre de sortie (0 après une remise à zéro). (3) **Le registre d'état des broches (0x0F) ne montre pas une broche pilotée en sortie** : SPEAKER_EN était lu à 0 alors que le haut-parleur sonnait. Pour vérifier une sortie, lire direction (0x03), sortie (0x05) et haute impédance (0x07) (`tab5_expander_dump()`) : pendant un son, 0x43 lit dir = 0x6F, sortie = 0x22, haute impédance = 0xDD, soit le bit 1 en sortie, haut, hors haute impédance.
- **Registres tels que trouvés au démarrage** (07/11/2026) : expandeur 0x43 dir 0xEF, sortie 0x20, haute impédance 0xDF, tirage haut 0x10, état 0x10 ; expandeur 0x44 dir 0x9F, sortie 0x01, haute impédance 0x7E, état 0x20 (`CHG_STAT` 0, `CHG_EN` 0, `nCHG_QC_EN` 1).
- **Ne jamais mettre à jour le C6** (décision du 16/10/2026).
- **Redémarrage logiciel** : terminer par `esp_rom_software_reset_system()` ; sur le P4 `esp_restart()` ne remet à zéro que les cœurs.
- Ouvrir ou fermer le port série USB peut réinitialiser la puce.
- Le numéro du port série change avec la prise USB : retrouver la carte par sa MAC, lue sur la puce.

### Ce que l'essai sur carte a tranché, et ce qu'il n'a pas tranché

Résultats, carte, commit et date : [`mesures/tab5-materiel.md`](mesures/tab5-materiel.md). **Tranché (constaté, 07/11/2026) :** le haut-parleur est sur LINE1 ; lecture et enregistrement tournent ensemble à 16 kHz sur le port unique ; le bus audio prend 6 224 o de RAM interne tant qu'il est ouvert ; `time()` survit à un redémarrage logiciel ; les registres d'identité de l'INA226 et de l'ES7210 ; les registres des deux expandeurs. **Non tranché :** le sens du bit « perte de tension » du RX8130, la polarité de `CHG_STAT` (il a lu 0 chargeur coupé, rien de plus), la présence ou non d'une batterie (8,2 V lus), le plancher de 2 % du rétroéclairage à l'œil, le plafond de 100 % sur un port USB faible, le chemin du casque, le vrai toucher sur le voile de la veille, une coupure du courant où la puce reprend la main. **Non commencés :** la carte SD et le capteur de mouvement.
