# Audio duplex, AEC et mot d'activation (ESP-SR) — recherche du 06/11/2026 (jalon 1c)

> **Summary (EN)** — Research note of 2026-11-06 for milestone 1c (duplex audio, echo cancellation and wake word with ESP-SR). Sources: the Tab5 repository, Espressif and M5Stack documentation and code, `xiaozhi-esp32`; web pages were read by agents that sometimes returned summaries (a code or register value must be re-read in the source). No code was written and no board touched.
> - Duplex is not a limit of the Tab5 hardware but of ESPHome's `i2s_audio`: the clocks are shared and each direction has its own data line; three code bases read open TX and RX together on this bus. Only code was read, nothing was measured.
> - Not established, and the main risk of 1c: how the echo reference reaches the capture on the Tab5. First test: record the four TDM channels of the ES7210 while a sine plays, to see which channel carries the speaker signal.
> - ESP-SR's echo canceller accepts only 16 kHz and 16-bit integers, while the native boards read run at 24 or 48 kHz: either duplex at 16 kHz or resampling of both microphone and reference.
> - Espressif's benchmark (at 400 MHz; the Tab5 runs at 360 MHz) gives the audio front end about 50–60 KB of internal SRAM, 0.74–0.82 MB of PSRAM and roughly 9 % + 10 % of a core.
> - The only French wake-word model is "Bonjour ESP"; a custom word is a paid Espressif service with an unpublished price. WakeNet's licence text grants use on Espressif products only, no model-specific licence file was found, and this is not legal advice: to settle before any binary is published. Other wake-word libraries (microWakeWord, Porcupine) are listed with their open points.
> - A step-by-step test plan for 1c and a "not verified" list follow. Fallback if echo cancellation fails: none, with the microphone muted during the answer (ADR-0002).

Sources : dépôt Tab5 (`docs/` et `Tab5/paquets/` de `Axellum/M5-Tab5-ESPHome-LVGL`, lecture seule), audit 7B des notes privées d'Axel (hors dépôt), documentation et code d'Espressif, M5Stack et `xiaozhi-esp32`. **Toutes les pages web ont été lues le 06/11/2026** par des agents dont l'outil rend parfois un résumé et non le texte brut : la colonne « Lu » dit *brut* ou *résumé* quand l'agent l'a précisé ; une valeur de code ou de registre est à relire dans la source avant de s'y fier. **Non vérifié** = absent des sources lues. Aucun code, aucune carte touchée. Complète [02-vocal-et-llm.md](02-vocal-et-llm.md) et [ADR-0002](../decisions/0002-assistant-vocal-en-cascade.md).

## Conclusion

1. **Le duplex n'est pas une limite du matériel du Tab5 mais de `i2s_audio` d'ESPHome** : les trois horloges sont communes, chaque sens a sa propre ligne de données (GPIO28 entrée, GPIO26 sortie). Trois bases de code lues (`esp-bsp`, `M5Tab5-UserDemo`, `xiaozhi-esp32`) créent et ouvrent TX **et** RX ensemble sur ce bus. On n'a **lu que du code**, aucune mesure.
2. **Ce qui n'est pas établi, et fait le risque de 1c : par où arrive la référence d'écho sur le Tab5.** M5Stack écrit « ES7210 AEC front end » sans décrire le chemin ; la 7B renvoie le haut-parleur sur MIC3 de l'ES7210 (audit 7B) ; les deux firmwares natifs lus ouvrent l'ES7210 en TDM 4 voies, sans qu'une ligne dise quelle voie porte le haut-parleur. **Premier essai : mesurer quelle voie TDM porte le signal du haut-parleur.**
3. **L'AEC d'ESP-SR n'accepte que 16 kHz** (« currently only 16000 is supported ») et des entiers 16 bits ; les cartes natives lues tournent à 24 ou 48 kHz. Il faudra soit un duplex à 16 kHz, soit un ré-échantillonnage des deux voies (micro **et** référence).
4. **Coût de l'AFE sur P4 (mesuré par Espressif à 400 MHz)** : ≈ 50–60 Ko de SRAM interne, ≈ 0,74–0,82 Mo de PSRAM, ≈ 9 % + 10 % d'un cœur pour un micro et une référence ; le Tab5 tourne à **360 MHz** (puce v1.3) : ces pourcentages sont à corriger d'environ ×1,1 (calcul, pas une mesure).
5. **Licence de WakeNet** : le texte de `LICENSE` accorde la permission « for use on all ESPRESSIF SYSTEMS products ». Le firmware ne tourne que sur des ESP32-P4 (Tab5 comme 7B), donc sur une puce Espressif ; la lettre du texte est respectée pour un usage personnel. **Aucun fichier de licence propre aux modèles.** Pas un avis juridique ; voir §4.
6. **Français** : seul « Bonjour ESP » existe (`wn9l_fr_bonjouresp_tts3`, `wn10_fr_bonjouresp`). Mot personnalisé : service payant d'Espressif, prix non publié.

## 1. Matériel audio du Tab5

### 1.1 Chaîne audio

| Élément | Valeur | Source | Lu |
|---|---|---|---|
| Lecture | ES8388 (I2C **0x10**), sortie → amplificateur **NS4150B** 1 W / 8 Ω, prise casque 3,5 mm | `docs/hardware.md`, `docs/tab5-pinout.md` (Tab5) | 06/11/2026 |
| Capture | ES7210 (I2C **0x40**), « dual-microphone array », « ES7210 AEC front end » | [docs.m5stack.com/en/core/Tab5](https://docs.m5stack.com/en/core/Tab5) | 06/11/2026, brut |
| Bus I2S (un seul) | MCLK **GPIO30**, BCLK **GPIO27**, WS **GPIO29**, données du P4 vers l'ES8388 **GPIO26** (DOUT), de l'ES7210 vers le P4 **GPIO28** (DIN) | `docs/tab5-pinout.md` ; recoupé par `esp-bsp` (`m5stack_tab5.h`) | 06/11/2026 |
| Ampli | `SPK_EN` = PI4IOE5V6408 **0x43, bit 1** ; casque détecté bit 7 ; I2C interne SDA 31 / SCL 32, 400 kHz | idem | 06/11/2026 |
| Réglages d'ESPHome | micro 16 kHz 16 bits mono, ES7210 `mic_gain: 30dB` ; haut-parleur 48 kHz stéréo, ES8388 ; la sortie DAC doit être sur `LINE1` pour le haut-parleur | `Tab5/paquets/tab5-hardware.yaml` ; [devices.esphome.io](https://devices.esphome.io/devices/m5stack-tab5/) | 06/11/2026 |
| Séquence d'ESPHome | rétroéclairage, attente 1 s, volume, **puis** activation de l'ampli ; sinon « loud pop » (ampli ouvert avant que l'horloge I2S soit stable) | `docs/hardware.md` (Tab5) | 06/11/2026 |
| Tampon du haut-parleur | `buffer_duration: 2000ms` : le Wi-Fi par le C6 fait des « hoquets » (SDIO) et ce tampon est le seul amortisseur en aval du décodeur | `Tab5/paquets/tab5-hardware.yaml` | 06/11/2026 |
| Expandeurs | 0x43 et 0x44 sur l'I2C interne ; **attendre 1 s** après le démarrage avant de s'en servir (reset d'écran) | `docs/troubleshooting.md` (Tab5) | 06/11/2026 |
| 7B (pour mémoire) | ES8311 mono (0x18) + ES7210 (0x40, canal gauche), I2S MCLK 13, BCLK 12, WS 10, DIN 11, DOUT 9 ; ampli NS4150B GPIO53 ; **sortie haut-parleur renvoyée sur MIC3 de l'ES7210** (« référence matérielle ») ; ES8311 sature à 100 % → volume ≤ 75 % | audit 7B n°1 ; `04-audit-depot-tab5.md` | 06/11/2026 |

### 1.2 Pas de duplex sous ESPHome : ESPHome ou matériel ?

| Fait | Source | Lu |
|---|---|---|
| ESPHome protège le bus avec **un seul mutex** (`i2s_audio.h`, `Mutex lock_`) : le premier composant démarré le tient, l'autre échoue avec `Parent bus is busy` (une fois par seconde). Démontré dans les deux sens le 05/08/2026 (alarme muette tant que le micro écoute, puis l'inverse) | `docs/decisions/0010-shared-i2s-bus-mic-speaker.md` (Tab5) | 06/11/2026 |
| « Les trois horloges sont communes, chaque sens a sa ligne de données » (matériel) | `docs/hardware.md` (Tab5) | 06/11/2026 |
| Conséquence : « Stop the alarm by voice » impossible pendant la mélodie ; pas d'effets sonores locaux avec le mot d'activation armé | ADR-0010 | 06/11/2026 |
| **Verdict** : la limite est celle de **l'implémentation d'ESPHome**. Le câblage permet TX et RX simultanés, ce que trois firmwares natifs ouvrent ensemble (§2.3). Aucune source lue ne dit que le matériel l'interdise. | — | — |

## 2. I2S en duplex sous ESP-IDF 5.5

### 2.1 Ce que dit la documentation

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Définition | full-duplex = TX et RX sur un même port, **BCLK et WS partagés** ; supporté en mode std et TDM, pas en PDM | [I2S, ESP-IDF v5.5.5 esp32p4](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/api-reference/peripherals/i2s.html) | 06/11/2026, résumé |
| Création | `i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle)` avec `I2S_NUM_AUTO` ; ou deux appels séparés sur le même `I2S_NUM_x`, qui forment le duplex | idem | 06/11/2026, résumé |
| Contrainte | commentaire d'exemple : « TX and RX channel have to be same in full-duplex mode » ; la page ne détaille pas quels champs (fréquence, largeur de slot…) | idem | 06/11/2026, résumé |
| MCLK | horloge interne séparée pour TX et RX, mais la **broche** de sortie MCLK ne s'attache qu'à un canal | idem | 06/11/2026, résumé |
| Synchronisation | le duplex ne partage que les horloges ; il ne garantit pas une écriture et une lecture synchrones | idem | 06/11/2026, résumé |
| TDM | `mclk_multiple` seulement en maître ; en slots 24 bits utiliser MCLK×384 | idem | 06/11/2026, résumé |
| Exemples officiels (v5.5.5) | `examples/peripherals/i2s/i2s_basic/i2s_std`, `i2s_basic/i2s_tdm`, `i2s_codec/i2s_es8311`, `i2s_codec/i2s_es7210_tdm` (enregistrement 4 micros en TDM vers carte SD, 48 kHz, ne lit rien ; dépend d'`esp_codec_dev ^1.3.4`) | [i2s_codec](https://github.com/espressif/esp-idf/tree/v5.5.5/examples/peripherals/i2s/i2s_codec) | 06/11/2026, résumé |
| Carte P4 de référence | **ESP32-P4-Function-EV-Board** : ES8311 seul (micro sur le codec), **aucun ES7210** sur la page ; BSP en duplex std mono, 22,05 kHz par défaut | [user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html), [BSP](https://github.com/espressif/esp-bsp/blob/master/bsp/esp32_p4_function_ev_board/esp32_p4_function_ev_board.c) | 06/11/2026, résumé |

### 2.2 Composants du registre

| Composant | Valeur | Source | Lu |
|---|---|---|---|
| `espressif/esp_codec_dev` | **1.6.2** « Latest » (« il y a 3 mois », date exacte non lue) ; 2.0.0-beta5 disponible ; ES8388 en lecture et capture, **ES7210 en capture seule** | [registre](https://components.espressif.com/components/espressif/esp_codec_dev) | 06/11/2026, résumé |
| API | `audio_codec_new_i2s_data` prend `rx_handle` et `tx_handle` ; `esp_codec_dev_new` accepte `ESP_CODEC_DEV_TYPE_IN_OUT` ; `esp_codec_dev_open` prend `sample_rate`, `channel`, `bits_per_sample` | [readme 1.6.2](https://components.espressif.com/components/espressif/esp_codec_dev/versions/1.6.2/readme) | 06/11/2026, résumé |
| `espressif/es7210` (1.0.1~1) | obsolète au profit d'`esp_codec_dev` | [registre](https://components.espressif.com/components/espressif/es7210) | 06/11/2026, résumé |
| Pilote ES7210 | `mic_selected` = 0 → MIC1+MIC2 ; `ES7210_INPUT_MIC1..4` ; TDM actif à partir de 3 micros ; gain 30 dB par défaut ; **aucun chemin « reference » ou « loopback » dans `es7210.c`** → la référence est un câblage matériel, pas un réglage logiciel | [es7210.c](https://github.com/espressif/esp-adf/blob/master/components/esp_codec_dev/device/es7210/es7210.c) | 06/11/2026, résumé |
| Pilote ES8388 | `open()` : DACPOWER=0xC0 (DAC et sorties coupés) puis mute (DACCONTROL3=0x04) ; `enable(true)` : `start()` puis `pa_power(true)`, DACPOWER=0x3C, démute ; `enable(false)` coupe d'abord l'ampli ; aucun commentaire sur le pop | [es8388.c](https://raw.githubusercontent.com/espressif/esp-adf/master/components/esp_codec_dev/device/es8388/es8388.c) | 06/11/2026, résumé |
| Ratio MCLK de l'ES8388 | 256 ; `esp-bsp`, `M5Tab5-UserDemo` et `xiaozhi-esp32` utilisent tous `mclk_multiple` 256 | idem + fichiers ci-dessous | 06/11/2026, résumé |

### 2.3 Ce que font les firmwares natifs sur Tab5 (code lu)

| Projet | I2S | ES8388 | ES7210 | Source |
|---|---|---|---|---|
| `esp-bsp` `bsp/m5stack_tab5/src/bsp_audio.c` | une création duplex `i2s_new_channel(&cfg,&tx,&rx)`, maître, `auto_clear=true` ; std Philips 16 bits **mono**, 48 kHz par défaut, pas de TDM | `DAC`, **`master_mode=false`**, `pa_pin` non relié ; `bsp_feature_enable(BSP_FEATURE_SPEAKER,true)` appelé avant | seulement `ctrl_if` → `mic_selected` par défaut (MIC1+MIC2) | [bsp_audio.c](https://raw.githubusercontent.com/espressif/esp-bsp/master/bsp/m5stack_tab5/src/bsp_audio.c) |
| `m5stack/M5Tab5-UserDemo` `platforms/tab5/components/m5stack_tab5/m5stack_tab5.c` | TX std mono ; RX en **TDM 4 slots** (`bclk_div=8`, `mclk_multiple=256`), 48 kHz ; `bsp_codec_es7210_set(48000,16,4)` et `bsp_codec_es8388_set(48000,16,2)` ouverts ensemble | 48 kHz, 16 bits, 2 voies | `mic_selected = MIC1\|MIC2\|MIC3\|MIC4` ; `bsp_codec_feed_channel()` renvoie **3** avec le commentaire `2*mic_num + ref_num` | [m5stack_tab5.c](https://raw.githubusercontent.com/m5stack/M5Tab5-UserDemo/main/platforms/tab5/components/m5stack_tab5/m5stack_tab5.c) (65 Ko : 39 Ko lus bruts, le reste résumé) |
| `78/xiaozhi-esp32` `main/boards/m5stack/tab5/tab5_audio_codec.cc` | duplex (`assert(in_rate==out_rate)`), 24 kHz ; TX std `slot_mask BOTH` ; RX en **TDM** slots 0–3, `bclk_div=8` ; MCLK ×256 des deux côtés ; DMA 6×240 | `DAC`, **`master_mode=true`**, `pa_pin=-1` | MIC1 à MIC4, gain 30 sur le canal 0 ; avec `input_reference`, le canal 1 est ajouté au masque d'ouverture | [tab5_audio_codec.cc](https://raw.githubusercontent.com/78/xiaozhi-esp32/main/main/boards/m5stack/tab5/tab5_audio_codec.cc) |
| `xiaozhi-esp32` `m5stack_tab5.cc` et `config.h` | expandeur : `OUT_SET = 0b01110110` à la construction → `SPK_EN` (bit 1) à 1 dès le départ, sans délai audio ; `AUDIO_INPUT_REFERENCE true`, 24 kHz | — | — | [m5stack_tab5.cc](https://raw.githubusercontent.com/78/xiaozhi-esp32/main/main/boards/m5stack/tab5/m5stack_tab5.cc), [config.h](https://raw.githubusercontent.com/78/xiaozhi-esp32/main/main/boards/m5stack/tab5/config.h) |

### 2.4 Contradictions entre sources (à trancher sur la carte)

| Sujet | Valeur A | Valeur B |
|---|---|---|
| ES8388 maître ou esclave | **esclave** (`master_mode=false`, `esp-bsp`) | **maître** (`master_mode=true`, `xiaozhi-esp32`). Le P4 produit BCLK et WS dans les deux cas |
| Mode de réception | std mono (`esp-bsp`) | TDM 4 slots (UserDemo, `xiaozhi-esp32`) — seul le TDM donne les 4 voies de l'ES7210 |
| Fréquence | 48 kHz (`esp-bsp`, UserDemo) | 24 kHz (`xiaozhi-esp32`) ; 16 kHz micro + 48 kHz haut-parleur en flux séparés (ESPHome). **L'AEC d'ESP-SR n'accepte que 16 kHz** |
| TX et RX « identiques » en duplex (doc ESP-IDF) | doc : même config des deux côtés | UserDemo et `xiaozhi-esp32` : TX std + RX TDM, même fréquence et même `mclk_multiple` |
| Nombre de voies de capture | 4 slots TDM ouverts | `bsp_codec_feed_channel()` = 3 |
| Mot « ES7210 AEC front end » de M5Stack | présent | la page ne décrit pas le chemin de la référence ; le PDF du schéma n'a pas été ouvert |

**Piège ES8388** : pop au démarrage. Dépôt Tab5 : activer l'ampli seulement après stabilisation de l'horloge I2S. Le pilote `esp_codec_dev` ne le traite pas par commentaire. Le contournement du registre DACPOWER (0x04 → 0x00) cité par `04-audit-depot-tab5.md` reste « à confirmer ».

## 3. ESP-SR (AFE, AEC, VAD, NS) sur ESP32-P4

### 3.1 Versions et compatibilité

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Dernière version | **2.6.1** (« il y a 1 semaine », date exacte non affichée) ; 62 versions ; 2.6.0 : WakeNet10s (annoncé le 09/10/2026) | [registre esp-sr](https://components.espressif.com/components/espressif/esp-sr) | 06/11/2026, résumé |
| Manifeste | version 2.6.1, `idf >=5.0` **sans borne maximale** ; dépend de `espressif/dl_fft >=0.6.0`, `espressif/esp-dl ~3.3.12`, `espressif/cjson ^1.7.19` ; `esp-dsp` épinglé à 1.8.0 seulement pour esp32/s2/c3/c5/c6 (**pas pour le P4**) | [idf_component.yml](https://raw.githubusercontent.com/espressif/esp-sr/master/idf_component.yml) | 06/11/2026, résumé |
| Cible P4 | AFE et WakeNet listés pour l'ESP32-P4 dans le README (cases vides dans la source lue → ambigu) ; la doc dit « WakeNet9 and WakeNet9l support ESP32, ESP32S3, and ESP32P4 chips » ; le benchmark P4 mesure WakeNet10 | [README](https://raw.githubusercontent.com/espressif/esp-sr/master/README.md), [doc WakeNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/wake_word_engine/README.html), [benchmark](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/benchmark/README.html) | 06/11/2026, brut (docs) |
| ESP-IDF 5.5.5 | manifeste `>=5.0` ; **compilation sous 5.5.5 non essayée** ; `xiaozhi-esp32` annonce IDF ≥ 5.5.2 et esp-sr ≥ 2.4.7 pour son Tab5 (recherche 02) | — | — |
| Modèles en flash | partition de données **`model`** (exemple : 6000K) flashée par le CMake d'esp-sr ; `srmodels.bin` sert surtout à Arduino. Taille des modèles français : **non trouvée** ; chargement mmap/carte SD : non couvert par la page lue | [flash_model](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/flash_model/README.html) | 06/11/2026, résumé |
| Exemples | pas de dossier `examples/` dans `esp-sr` ; renvoi vers `esp-skainet/examples` (non lu en détail) ; `esp-skainet` liste l'ESP32-P4-Function-EV pour 4 exemples | [esp-sr](https://github.com/espressif/esp-sr), [esp-skainet](https://github.com/espressif/esp-skainet) | 06/11/2026, résumé |

### 3.2 AFE : réglages et voie de référence

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Format d'entrée | `M` = micro, `R` = référence du haut-parleur, `N` = voie inutilisée ; exemple « MMNR » ; **données entrelacées par canal** ; PCM 16 bits signé | [doc AFE P4](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/audio_front_end/README.html) | 06/11/2026, résumé |
| Où vient la référence | la doc AFE dit seulement que `R` est une voie du même tampon entrelacé que les micros ; **le firmware doit donc la fournir** : soit boucle matérielle sur une voie de l'ES7210, soit copie logicielle du signal envoyé au haut-parleur | idem | 06/11/2026, résumé |
| Alignement / latence maximale toléré | **non documenté** (ni dans la page AEC, ni dans l'AFE) | [AEC P4](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/acoustic_echo_cancellation/README.html) | 06/11/2026, brut |
| Dérive d'horloge entre lecture et capture | **non documentée** (sur un seul port I2S, BCLK/WS sont partagés : même horloge — inférence, à mesurer) | idem | 06/11/2026, brut |
| Champs d'`afe_config_t` (`esp_afe_config.h`, P4) | `aec_init`, `aec_mode` (SR_LOW_COST / SR_HIGH_PERF), `aec_filter_length`, `aec_nlp_level` (défaut AGGR), `se_init`, `ns_init`/`ns_model_name`/`afe_ns_mode`, `vad_init`/`vad_mode`/`vad_model_name` (« If it is null, WebRTC VAD will be used »), `wakenet_init`/`wakenet_model_name`(`_2`)/`wakenet_mode`, `agc_init`/`agc_mode`, `afe_mode`, `afe_type`, `afe_ringbuf_size`, `memory_alloc_mode`, `afe_linear_gain` ([0,1 ; 10,0]), `output_playback_channel` (renvoie la voie de référence dans `fetch`) | [esp_afe_config.h (P4)](https://raw.githubusercontent.com/espressif/esp-sr/master/include/esp32p4/esp_afe_config.h) | 06/11/2026, brut |
| `afe_pcm_config_t` | `total_ch_num` (micros + lecture + inconnues), `mic_num`, `mic_ids`, `ref_num`, `ref_ids`, `sample_rate` | idem | 06/11/2026, brut |
| Types / modes | `AFE_TYPE_SR` (sans suppression non linéaire), `AFE_TYPE_VC`, `AFE_TYPE_VC_8K`, **`AFE_TYPE_FD` (« Full duplex scenarios »)** ; `AFE_MODE_LOW_COST/HIGH_PERF` ; mémoire `AFE_MEMORY_ALLOC_MORE_INTERNAL` / `INTERNAL_PSRAM_BALANCE` / `MORE_PSRAM` | idem | 06/11/2026, brut |
| Utilitaires | `afe_parse_input()` (sépare `mic_data` et `ref_data`), `afe_parse_data()` (entrelacé → contigu) | idem | 06/11/2026, brut |
| AEC seul : fréquence | « currently only 16000 is supported » ; tampons `int16_t` | [page AEC](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/acoustic_echo_cancellation/README.html) | 06/11/2026, brut |
| AEC seul : trame et réglages | trame **32 ms** en modes SR/FD, **16 ms** en VOIP ; `aec_get_chunksize()` ; `filter_length` « recommended value is 4, larger values consume more resources » ; la page recommande `AEC_MODE_FD_LOW_COST` pour le meilleur compromis ; exemple `mic_num=1, ref_num=1, out_num=1` | idem | 06/11/2026, brut |
| AEC : nombre de micros | « jusqu'à 2 micros » (doc AFE ; déjà dans la recherche 02 comme venant du README) | [doc AFE P4](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/audio_front_end/README.html) | 06/11/2026, résumé |
| `xiaozhi-esp32` | `AfeAudioEngine` pour ESP32-S3/P4 ; alimente l'AFE par blocs de **160 échantillons (10 ms)** à 16 kHz ; données entrelacées selon `codec->input_channels()` ; attend ≈ 120 ms au démarrage du traitement vocal. `afe_config` lui-même **non lu** (le fichier `afe_audio_processor.cc` est en 404 ; le vrai fichier est celui inclus par `engines/afe_audio_engine.h`) | [audio_service.cc](https://raw.githubusercontent.com/78/xiaozhi-esp32/main/main/audio/audio_service.cc) | 06/11/2026, brut |
| AEC actif sur Tab5 chez `xiaozhi-esp32` ? | `Kconfig.projbuild` : `USE_DEVICE_AEC` à n par défaut et la liste des cartes qui l'autorisent n'inclut pas `BOARD_TYPE_M5STACK_CORE_TAB5` ; mais `AUDIO_INPUT_REFERENCE true` est défini dans le `config.h` du Tab5 → **non tranché** | [Kconfig.projbuild](https://raw.githubusercontent.com/78/xiaozhi-esp32/main/main/Kconfig.projbuild) | 06/11/2026, résumé |

Contradiction de trame : la page AEC parle de trames de 32 ms (16 ms en VOIP) ; `xiaozhi-esp32` alimente l'AFE par 10 ms. Le tampon interne de l'AFE absorbe l'écart d'après l'API (`get_feed_chunksize`), valeur exacte **non lue** : la prendre dans l'API à l'exécution.

### 3.3 Mémoire et processeur (benchmark Espressif pour ESP32-P4, **à 400 MHz**, cache L2 256 Ko)

Pipeline mesuré : AEC + NS (nsnet2) + SE (BSS) + VAD (vadnet1_medium) + WakeNet (wn9_hilexin). Source : [benchmark](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/benchmark/README.html), lu le 06/11/2026, brut.

| Configuration | SRAM interne | PSRAM | CPU « feed » (1 cœur) | CPU « fetch » (1 cœur) |
|---|---|---|---|---|
| MR, SR, LOW_COST | 60,1 Ko | 739,7 Ko | 8,8 % | 9,8 % |
| MR, SR, HIGH_PERF | 49,1 Ko | 775,8 Ko | 9,3 % | 9,8 % |
| MR, VC, LOW_COST | 48,7 Ko | 819,7 Ko | 30,6 % | 4,7 % |
| MMNR, SR, LOW_COST | 79,1 Ko | 1 153,7 Ko | 23,7 % | 22,9 % |
| AEC seul `FD_LOW_COST` | 18,9 Ko | 102,1 Ko | 3,69 ms / 32 ms (11,5 %) | — |
| AEC seul `SR_LOW_COST` | 18,8 Ko | 64,0 Ko | 2,66 ms / 32 ms (8,3 %) | — |
| WakeNet9, 2 canaux | 16 Ko | 324 Ko | 2,6 ms / trame de 32 ms | — |
| WakeNet10, 3 canaux | 16 Ko | 523 Ko | 14,1 % | — |

Pour le Tab5 : **PSRAM 32 Mo (29,5 Mo disponibles au tas sous ESPHome)**, donc la PSRAM n'est pas le problème. **SRAM interne : 768 Ko pour tout** (LVGL, buffers DMA du lien C6 — l'issue [esp-hosted-mcu #144](https://github.com/espressif/esp-hosted-mcu/issues/144) rapporte des assertions faute de mémoire DMA interne contiguë), à surveiller avec l'AFE. Comparaison locale : `micro_wake_word` d'ESPHome tourne sur ce P4 à **5–8 % de CPU** à 360 MHz (`docs/voice_assistant.md`).

## 4. WakeNet : modèles français et licence

### 4.1 Modèles

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Familles | WakeNet9, 9s, 9l, 10, 10s ; **5 mots maximum** par modèle | [README esp-sr](https://github.com/espressif/esp-sr) | 06/11/2026, résumé |
| **Français** | `wn9l_fr_bonjouresp_tts3` (« Bonjour ESP ») et `wn10_fr_bonjouresp` (« Bonjour ESP ») : **les seules lignes `fr` de la liste** | [wakeword_list.md](https://raw.githubusercontent.com/espressif/esp-sr/master/wakeword_list.md) (généré le 29/10/2026) | 06/11/2026, brut |
| Support P4 par modèle | WakeNet9/9l : oui d'après la doc ; `wn10_fr_bonjouresp` sur P4 : **non trouvé** (le benchmark mesure WakeNet10 sur P4) ; 9s/10s : non vérifié | [doc WakeNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/wake_word_engine/README.html) | 06/11/2026, brut |
| Phrase de la liste | « Most wake word models are automatically generated and tested using the TTS pipeline. » | wakeword_list.md | 06/11/2026, brut |
| Mot personnalisé | frais **uniques**, pas de licence par appareil ; **montant non publié** (contacter `sales@espressif.com`) ; trois méthodes : synthèse vocale seule ; enregistrements Espressif + synthèse (2–3 semaines d'entraînement) ; corpus du client (≥ 20 000 entrées, 2–3 semaines) | [Customization](https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/wake_word_engine/ESP_Wake_Words_Customization.html) | 06/11/2026, résumé |
| Pages 404 | `AEC/README.html`, `model/model_loading.html`, `ESP_Wake_Words_List.html` : liste officielle par puce **non lue** | — | — |

### 4.2 Licence : texte exact

Fichier [`LICENSE`](https://raw.githubusercontent.com/espressif/esp-sr/master/LICENSE) du dépôt `espressif/esp-sr` (1 187 caractères, lu *brut* le 06/11/2026 par un second agent après une première lecture contradictoire du même passage) :

> **ESPRESSIF MIT License** — « Copyright (c) 2018 ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD ». « Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case, it is free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies […] subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies […]. THE SOFTWARE IS PROVIDED "AS IS" […] »

Autres textes lus (README, brut) : « Before any commercial use, you must ensure that you are the lawful rights holder of the relevant wake words or have obtained proper authorization from the lawful rights holder. » ; les noms, logos et marques associés aux mots d'activation cités sont la propriété de leurs détenteurs.

| Question | Constat | Statut |
|---|---|---|
| Un fichier de licence propre aux modèles ? | **Non** : `model/` contient `multinet_model`, `nsnet_model`, `target`, `vadnet_model/vadnet1_medium`, `wakenet_model`, `CMakeLists.txt`, `movemodel.py`, `pack_model.py` ; la racine n'a que le `LICENSE` ci-dessus (listes GitHub converties, éventuellement incomplètes ; `wakeword_list.md` et les sous-dossiers non ouverts) | lu, listes possiblement incomplètes |
| Réutilisation « hors produits Espressif » | le texte **limite la permission aux produits Espressif**. Le firmware ne tourne que sur ESP32-P4 : puce Espressif, sur une carte M5Stack (Tab5) ou Waveshare (7B). Aucun texte ni aucune issue lue ne tranche le cas d'une puce Espressif sur une carte tierce | lecture littérale, **pas un avis juridique** |
| Publication sur GitHub | le dépôt ne devrait contenir ni le code d'`esp-sr` ni les modèles : les prendre par `idf_component.yml` (registre). Un binaire publié ne contient `esp-sr` que sur ESP32-P4 | à décider par Axel |
| Issues GitHub d'esp-sr (« license », « commercial », « non-Espressif ») | aucun résultat pertinent (l'issue [#198](https://github.com/espressif/esp-sr/issues/198) ne parle que d'un mot personnalisé) | lu, résumé |
| Mot « Bonjour ESP » | usage commercial : démontrer qu'on est détenteur du mot ou autorisé ; usage personnel non concerné par la lettre | lu, brut |

Si WakeNet était exclu pour une raison de licence : voir §5. Le dépôt n'a pas besoin de trancher avant 1c (usage chez Axel) ; il doit trancher **avant toute publication d'un binaire**.

## 5. Autres bibliothèques de mot d'activation

| Solution | Licence (fichier lu) | Français | Sur ESP32 / P4 | Source | Lu |
|---|---|---|---|---|---|
| **microWakeWord** (TFLite Micro) | dépôt `kahrendt/microWakeWord` : Apache 2.0 (non confirmé pour le fichier d'un fork) ; modèles `esphome/micro-wake-word-models` : Apache 2.0, poids `.tflite` compris | pas de modèle français officiel (recherche 02 : un utilisateur en a entraîné un avec des échantillons Piper) ; non vérifié ici | **tourne déjà sur ce P4** : `micro_wake_word` d'ESPHome (`okay_nabu`), 5–8 % de CPU à 360 MHz (`docs/voice_assistant.md`) ; l'intégration native via `esp-tflite-micro` n'a pas été examinée | [LICENSE](https://raw.githubusercontent.com/kahrendt/microWakeWord/main/LICENSE), [modèles](https://github.com/esphome/micro-wake-word-models) | 06/11/2026, résumé |
| openWakeWord | code : Apache 2.0 ; **modèles pré-entraînés : CC BY-NC-SA 4.0** (non commercial) | **non** (« only supports English ») | mal adapté aux microcontrôleurs (le README renvoie vers microWakeWord) | [README](https://raw.githubusercontent.com/dscripka/openWakeWord/main/README.md) | 06/11/2026, résumé |
| Picovoice Porcupine | code du dépôt : Apache 2.0 ; **conditions de la clé d'accès (AccessKey) et du plan gratuit : non vérifiées** (page de prix lue de façon incomplète, FAQ en 404) | oui (9 langues dont le français) | ESP32 non nommé dans le README ; dossier `demo/mcu` non lu | [dépôt](https://github.com/Picovoice/porcupine), [LICENSE](https://github.com/Picovoice/porcupine/blob/master/LICENSE) | 06/11/2026, résumé |
| esp-tflite-micro, Edge Impulse, Sensory, Snowboy | non recherchés | — | — | — | — |

**Repli conforme à l'architecture** : AFE d'ESP-SR **sans WakeNet** (`wakenet_init=false`, AEC + NS + VAD seuls) et sortie de l'AFE envoyée à un modèle microWakeWord ; un modèle français devrait être entraîné (taille, qualité et licence des données **non évaluées**).

## 6. Plan d'essai de 1c

**Garde-fous** : même que 1b (`AGENTS.md`) ; ne commencer qu'après 1b vert (le Wi-Fi n'est pas nécessaire, mais le lien C6 consomme de la SRAM interne : mesurer l'audio **avec et sans** le Wi-Fi). Aucune mesure sans noter volume, distance et pièce. Volume du Tab5 : plafond de l'ES8388 à 0 dB (la limite de 75 % est celle de la 7B).

### 6.1 Programme minimal (une seule application d'essai, pas l'interface)

1. I2C, expandeurs (attente 1 s), horloge MCLK/BCLK/WS démarrée, **puis** `SPK_EN` (0x43 bit 1) — dans cet ordre.
2. `i2s_new_channel(&cfg, &tx, &rx)` sur un même port, MCLK 256×, `mclk` sur un seul des deux canaux ; ES8388 (sortie `LINE1`) et ES7210 par `esp_codec_dev` (version épinglée **1.6.2** au départ ; ne pas passer à 2.0.0-beta). Les deux modes de l'ES8388 (maître/esclave) à essayer.
3. Deux configurations d'horloge, une à la fois : **A** : 16 kHz des deux côtés (aucun ré-échantillonnage, mais la qualité de lecture du TTS est celle de 16 kHz) ; **B** : 48 kHz des deux côtés puis réduction à 16 kHz du micro **et** de la référence avant l'AFE.
4. Capture en TDM 4 voies (comme le UserDemo), enregistrement des 4 voies dans la PSRAM.

### 6.2 Étapes et mesures

| Étape | Mesure | Unité | Si échec |
|---|---|---|---|
| 1. **Quelle voie porte le haut-parleur ?** Jouer un sinus 1 kHz (puis 3 kHz) à volume moyen, lire les 4 voies TDM, comparer à haut-parleur coupé | niveau RMS par voie et amplitude de la raie à 1 kHz | dBFS (pleine échelle 16 bits = 32 768) | pas de voie qui suit le haut-parleur → pas de boucle matérielle : référence **logicielle** (copie du tampon envoyé) |
| 2. Délai de la boucle | corrélation croisée entre le signal envoyé et la voie de référence (ou le micro) | ms, et stabilité sur 20 démarrages | délai instable → AEC logicielle peu fiable |
| 3. Duplex stable | 10 minutes de lecture + capture continues, sans trou ni « underrun » | nombre de trous, de pops (à l'oreille d'Axel) | retour sur §6.1 point 2 (maître/esclave, ordre de démarrage) |
| 4. **Écho avant AEC** | lecture d'un bruit rose (10 s) puis d'une phrase française de 10 s, **sans parole proche**, 3 volumes (40 %, 60 %, 80 %) ; niveau RMS de la voie micro brute sur 100 Hz–7 kHz, plancher de bruit haut-parleur coupé en plus | dBFS | — |
| 5. **Écho après AEC** | même signal, même volume, sortie de l'AFE (`fetch`) ; **ERLE** = P(micro brut) − P(sortie AFE) en dB, calculé après 5 s de convergence ; relancer 3 fois | dB | essayer `AEC_MODE_FD_LOW_COST`/`SR_HIGH_PERF`, `aec_filter_length` > 4, la référence logicielle retardée du délai mesuré |
| 6. Mot pendant la lecture | « Bonjour ESP » prononcé 20 fois à ≈ 1 m **haut-parleur muet**, puis 20 fois **pendant** la lecture d'une phrase française à 60 % ; en plus une voix enregistrée jouée par un haut-parleur du PC (reproductible) | détections / 20, latence mot→événement (ms) | passer à `wn10_fr_bonjouresp`/`wn9l_fr_bonjouresp_tts3`, baisser le volume, sensibilité |
| 7. Faux déclenchements | 30 minutes de parole française (radio ou télévision) à volume normal, mot absent | déclenchements / heure | augmenter le seuil, `VAD` |
| 8. Ressources | `heap_caps_get_free_size` SRAM interne et PSRAM avant/après `esp_afe_*_create`, CPU par cœur (statistiques FreeRTOS) **avec LVGL qui affiche** | Ko, % | `memory_alloc_mode`, `AFE_MODE_LOW_COST`, épingler les tâches |

**Ce qui compte comme réussite** (seuils **proposés**, à valider par Axel après la première mesure ; ce ne sont pas des normes) :

- Étape 1 : une voie dont la raie à 1 kHz dépasse de ≥ 30 dB celle de haut-parleur coupé (sinon : référence logicielle, voir repli).
- Étape 5 : **ERLE ≥ 20 dB** à 60 % de volume en régime établi, sur les trois répétitions ; écho résiduel à moins de 6 dB au-dessus du plancher de bruit.
- Étape 6 : détection pendant la lecture ≥ 80 % de celle sans lecture (donc ≥ 16/20 si le muet donne 20/20), latence notée.
- Étape 7 : ≤ 1 faux déclenchement par heure sous parole.
- Étape 8 : SRAM interne libre ≥ 100 Ko avec Wi-Fi + AFE + écran (valeur proposée), CPU de chaque cœur < 70 %.
- Constaté **sur la carte**, deux fois (une avec une coupure d'alimentation entre-deux) — règle de « fait » du plan.

**Repli** (ADR-0002, inchangé) : **pas d'AEC** : micro coupé pendant la réponse, un appui sur l'écran interrompt ; la mesure de l'étape 5 est consignée même en échec. Replis intermédiaires, dans l'ordre : (a) référence logicielle retardée du délai mesuré ; (b) AFE sans WakeNet + microWakeWord ; (c) baisse du volume pendant l'écoute du mot (le « ducking » : coût non mesuré).

## Non vérifié

- Par quelle voie de l'ES7210 le haut-parleur du Tab5 est renvoyé (le PDF du schéma de M5Stack n'a pas été ouvert) ; le sens exact de `bsp_codec_feed_channel() = 3`.
- Alignement et latence maximale tolérés par l'AEC ; effet d'une dérive d'horloge ; `aec_filter_length` et durée couverte.
- Compilation d'esp-sr 2.6.1 (et d'`esp-dl ~3.3.12`) sous ESP-IDF 5.5.5 sur P4 ; taille des modèles `wn9l_fr_bonjouresp_tts3` et `wn10_fr_bonjouresp` ; support P4 de ce dernier.
- Maître ou esclave pour l'ES8388 sur ce Tab5 ; fréquences 16 kHz supportées par l'ES8388 et l'ES7210 à MCLK 4,096 MHz ; ré-échantillonneur recommandé pour passer à 16 kHz.
- Si `xiaozhi-esp32` active réellement l'AEC sur Tab5 ; son `afe_config` (fichier non lu).
- Licence : lecture du seul `LICENSE` (modèles inclus par défaut) ; cas d'une puce Espressif sur carte tierce non tranché ; conditions de Picovoice ; licences de `esp-dl`, `dl_fft`, `esp_codec_dev`.
- Prix et délai d'un mot personnalisé ; modèle français de microWakeWord ; faisabilité de l'AFE sans WakeNet suivi de microWakeWord.
- Pop de l'ES8388 : aucune source n'explique la cause ; l'ordre « horloge stable puis ampli » vient du seul dépôt Tab5.
- Seuils de réussite du §6.2 : proposition, aucune norme citée de mémoire.

## Ce que la piste matériel doit faire en premier

1. Écrire l'application d'essai minimale (duplex TX std + RX TDM 4 voies, ES8388 `LINE1`, ampli **après** les horloges) et **enregistrer les 4 voies pendant un sinus 1 kHz** pour savoir où arrive le haut-parleur.
2. Choisir 16 kHz des deux côtés pour la première mesure (l'AEC n'accepte que 16 kHz), puis tester 48 kHz + réduction.
3. Fournir la voie `R` à l'AFE (`input_format` « MR » ou « MMR » selon l'étape 1), mesurer ERLE (dB) sur bruit rose et phrase française, trois volumes.
4. Seulement ensuite : « Bonjour ESP » pendant la lecture, ressources avec LVGL, puis la licence à trancher avant toute publication d'un binaire.
