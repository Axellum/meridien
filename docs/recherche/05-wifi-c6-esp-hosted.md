# Wi-Fi par le C6 (esp_hosted), SoftAP et portail captif — recherche du 06/11/2026 (jalons 1b et 1d)

> **Summary (EN)** — Research note of 2026-11-06 for milestones 1b (Wi-Fi through the ESP32-C6) and 1d (SoftAP and captive portal). Sources are web pages summarised by agents (a quoted value must be re-read in the source before relying on it), the Tab5 repository and Home Assistant history (read-only). Nothing was run on a board.
> - The Tab5's C6 very probably carries the factory ESP-Hosted firmware 1.4.1 (the "C6 Version" sensor reads 1.4.1; M5Stack's demo ships that binary). The pairing proven on this tablet is the ESPHome one: host `esp_hosted` 2.12.12, SDIO 20 MHz, reset on GPIO15 active high, PSRAM buffers; about fifty boots without Wi-Fi loss since 2026-09-30 according to the Tab5 repository.
> - A host newer than the C6 is only a warning in the code; what it announces (RPC timeouts) is a risk, not a measured fact. The C6 must never be written to.
> - SoftAP: an official example exists for P4 + C6, but no source read shows it with a C6 left on 1.4.1, nor with a captive portal; this is what 1d must measure. `espressif/dns_server` is not in the registry: the capturing DNS server has to be copied from ESP-IDF's `captive_portal` example.
> - Most likely trap in 1b: the C6 is not powered unless the I/O expander sets `wifi_power` (bit 0 of the 0x44 expander) before `esp_hosted` starts.
> - The note also lists known bugs and recovery after a reboot (C6 stuck after a transport failure, SDIO state errors, memory), what phones do with captive portals (third-party sources: the official Android and Apple pages could not be read), a 1b test plan including 20 consecutive reboots, and a long "not verified" list (for example SoftAP and DHCP behaviour with a C6 on 1.4.1, and the reset polarity: active high in ESPHome, active low at M5Stack).

Sources : lecture du dépôt Tab5 (dossier `docs/` de `Axellum/M5-Tab5-ESPHome-LVGL`, lecture seule), historique de Home Assistant (lecture seule), registre et dépôts Espressif. **Toutes les pages web ont été lues le 06/11/2026** par des agents de recherche dont l'outil rend parfois un résumé et non le texte brut : un code ou une valeur cités ici sont à relire dans le fichier source avant d'en dépendre (colonne « Lu » : *brut* ou *résumé* quand l'agent l'a dit). **Vérifié** = vu dans une source citée. **Non vérifié** = absent des sources lues ou non confirmé. Rien n'a été exécuté sur une carte.

## Conclusion

1. **Le C6 du Tab5 porte très probablement le firmware d'usine ESP-Hosted « 1.4.1 »** : le capteur « C6 Version » vaut `1.4.1` dans l'historique de Home Assistant (30/10 et 01/11/2026) et le dépôt de démonstration de M5Stack livre `ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin`. Le couple d'origine de M5Stack est **hôte `esp_hosted` 1.4.0 + `esp_wifi_remote` 0.8.5 (ESP-IDF 5.4.2) avec ce C6 1.4.1**.
2. **Le couple prouvé sur cette tablette avec ce C6** est celui du firmware ESPHome : hôte `esp_hosted` **2.12.12**, SDIO 20 MHz, reset GPIO15 **actif haut**, PSRAM pour les buffers. Une cinquantaine de démarrages sans perte du Wi-Fi depuis le 30/09/2026 (dépôt Tab5, `docs/hardware.md`). C'est le point de départ de 1b, pas la 3.1.0.
3. **Un hôte plus récent que le C6 n'est qu'un avertissement** dans le code (`compare_fw_version`, valeur de retour ignorée) ; ce que le message annonce (« RPC timeouts ») est un risque, pas un fait mesuré.
4. **SoftAP : un exemple officiel existe pour P4 + C6** (`examples/wifi/softap`), appelé par l'API `esp_wifi` classique. **Aucune source lue ne le montre avec un C6 resté en 1.4.1**, ni avec un portail captif. C'est exactement ce que 1d doit mesurer.
5. **`espressif/dns_server` n'existe pas dans le registre** (introuvable) : le serveur DNS capturant est un composant *local* de l'exemple `captive_portal` d'ESP-IDF (licence Unlicense OR CC0-1.0) ; il faut le recopier dans le dépôt.
6. **Le plus probable piège de 1b : le C6 n'est pas alimenté** si l'expandeur n'a pas mis `wifi_power` (PI4IOE 0x44, bit 0) à 1 avant `esp_hosted` — c'est la cause réelle d'un `sdmmc_init_ocr 0x107` rapporté sur Tab5.

## 1. Versions : ce que le C6 d'usine permet

### 1.1 Version du C6 du Tab5

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Capteur « C6 Version » du firmware ESPHome (dans Home Assistant) | `1.4.1` du 30/10/2026 08:56 au 01/11/2026 (états lus sur 10 lignes d'historique) | historique de Home Assistant, lecture seule, via l'API | 06/11/2026 |
| Quelle tablette ? | **Non vérifié.** L'entité du capteur n'est pas rattachée à une MAC dans ce que j'ai lu. La première tablette (ST7123) est au SAV depuis le 16/10 (`docs/plan.md`) : à ces dates c'est très probablement le Tab5 ST7121 de développement. L'autre entité de version du C6 reste « Unknown » | idem | 06/11/2026 |
| Note de l'autre projet Tab5 : « 2.12.12 sur le P4, 1.4.1 sur le C6 », aucun délai RPC vu | dépôt Tab5, `docs/hardware.md` (« Notes from another Tab5 project »), tablette de l'auteur au 07/10/2026 | fichier local | 06/11/2026 |
| Firmware C6 de la démo M5Stack | `platforms/tab5/wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin` | [M5Tab5-UserDemo/…/wifi_c6_fw](https://github.com/m5stack/M5Tab5-UserDemo/tree/main/platforms/tab5/wifi_c6_fw) | 06/11/2026, résumé |
| Démo M5Stack côté hôte | `esp_hosted` **1.4.0**, `esp_wifi_remote` **0.8.5**, `idf >=5.3`, verrou sur ESP-IDF **5.4.2** | [dependencies.lock](https://raw.githubusercontent.com/m5stack/M5Tab5-UserDemo/main/platforms/tab5/dependencies.lock) | 06/11/2026, résumé |

→ Pas besoin de lire la tablette pour avoir la version : `1.4.1`. **Le seul point à confirmer est que c'est bien la tablette ST7121 de développement.** Lecture la moins risquable : le journal série du premier démarrage natif (l'événement `ESP_HOSTED_EVENT_CP_INIT` / la version du coprocesseur rapportée par `esp_hosted`), qui ne demande aucune écriture ; à défaut, l'entité HA de la tablette quand elle y revient. Ne rien écrire sur le C6 (décision d'Axel du 16/10/2026).

### 1.2 Versions du registre (lues par l'API du registre)

| Composant | Versions utiles | Contraintes | Source | Lu |
|---|---|---|---|---|
| `espressif/esp_hosted` | 3.1.0 (05/11/2026), 3.0.10 (22/10), 3.0.9 (26/09) ; **2.12.12** (« il y a 3 mois », date exacte non lue) ; branche `release/v2.x` : 2.13.1 | 3.0.10 et 3.1.0 : `idf >=5.5`, `esp32p4` listé. Branche 2.x : `idf >=5.3` | [API du registre](https://components.espressif.com/api/components/espressif/esp_hosted), [idf_component.yml de release/v2.x](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/release/v2.x/idf_component.yml) | 06/11/2026, résumé (liste tronquée : 3 versions sur 8) |
| `espressif/esp_wifi_remote` | 1.6.5 (23/09/2026), 1.6.4 (10/08), 1.6.3 (20/07), 1.6.2 (09/07), 1.6.1 (04/06), 1.6.0 (25/05) | 1.6.5 : `idf >=5.3`, `esp_hosted >=2.11` pour esp32p4, `wifi_remote_over_eppp >=0.1`. **1.6.3 limitait `esp_hosted` à `<3.0`** ; la borne de 1.6.5 n'a pas été lue | [API du registre](https://components.espressif.com/api/components/espressif/esp_wifi_remote), [idf_component.yml](https://raw.githubusercontent.com/espressif/esp-wifi-remote/main/components/esp_wifi_remote/idf_component.yml) | 06/11/2026, résumé |
| Hôte ≥ 2.11.0 | à utiliser avec `esp_wifi_remote` ≥ **1.3.1** (fuite mémoire à l'init du netif sinon) | [migration.md](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/main/docs/migration.md) | 06/11/2026, résumé |

**Contradiction à noter avec la recherche 01** : `01-pile-et-qualite.md` donne « 3.1.0 / 1.6.5 » comme dernières versions (confirmé par l'API du registre ci-dessus) et dit « compatibilité de 3.1.0 avec IDF 5.5 non vérifiée » : le manifeste dit désormais `idf >=5.5`, donc **la 3.1.0 compile sur 5.5.5 d'après son manifeste**, mais sa compatibilité avec un C6 1.4.1 reste non démontrée.

### 1.3 Compatibilité hôte / C6

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Règle officielle | « Make sure you use the same version of the Master and Slave code » (dépôt `release/v2.x`) | [troubleshooting.md](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/release/v2.x/docs/troubleshooting.md) | 06/11/2026, résumé |
| Tableau hôte/esclave | **Pas de tableau général.** Seul le guide de migration dit : hôte 3.x avec CP 0.0.6+, 1.x ou 2.x « rétrocompatible » mais mise à jour du CP recommandée ; hôte <3.x avec CP 3.x non supporté ; ordre de migration CP puis hôte. Version 3.0.0 « breaking » (RPC-V2 pour Linux/MCU) | [migration.md](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/main/docs/migration.md), [changelog.md](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/main/docs/changelog.md) | 06/11/2026, résumé |
| Hôte 2.12.x avec C6 1.4.1 | **Aucune source ne l'affirme.** Preuve par l'usage : ESPHome (hôte 2.12.12, C6 1.4.1) fait du Wi-Fi station sur cette tablette | `docs/hardware.md` (Tab5) | 06/11/2026 |
| `compare_fw_version()` | compare majeur.minor ; hôte > CP : `ESP_LOGW` « Version mismatch: Host […] > Co-proc […] ==> Upgrade co-proc to avoid RPC timeouts » ; valeur de retour ignorée, l'init continue ; supprimable par `CONFIG_ESP_HOSTED_FW_VERSION_MISMATCH_WARNING_SUPPRESS` | [transport_drv.c (release/v2.x)](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/release/v2.x/host/drivers/transport/transport_drv.c) | 06/11/2026, résumé |
| Effets d'un CP ancien | s'il ne rapporte pas la taille de transfert, l'hôte avertit « may not be compatible » et n'envoie pas la configuration de taille ; idem pour le mode SDIO (défaut : paquet) | idem | 06/11/2026, résumé |
| Fonctions RPC absentes d'un CP 1.4.1 (déduit du tableau par version de release) | DhcpDnsStatus (2.0.17), ITWT (2.2.2), entreprise/EAP (2.4.0), DPP (2.4.3), MAC d'interface (2.5.2), OTAActivate (2.6.0), CustomRpc (2.8.1), GPIO distants (2.10.0), WifiScanParams (2.12.0), WifiDisablePmfConfig (2.12.9)… `WifiApGetStaList` et `SetPs` existent depuis 0.0.6 | [implemented_rpcs.md](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/release/v2.x/docs/implemented_rpcs.md) | 06/11/2026, résumé |
| L'API de configuration du SoftAP (`esp_wifi_set_config(WIFI_IF_AP)`) existe-t-elle côté C6 1.4.1 ? | **Non vérifié** : aucune ligne « SoftAP » dans le tableau lu pour les versions ≥ 1.4 ; l'entrée « 2.1.11 : Fixed SoftAP operation issues » du changelog laisse penser que le SoftAP existe avant, sans dire dans quelle version | [changelog 2.7.3](https://components.espressif.com/components/espressif/esp_hosted/versions/2.7.3/changelog) | 06/11/2026, résumé |

### 1.4 Recommandation d'épinglage (à confirmer par l'essai de 1b)

| Rang | Couple | Pourquoi | Risque |
|---|---|---|---|
| **A (départ)** | `esp_hosted == 2.12.12` + `esp_wifi_remote` ≥ 1.3.1 qui accepte cette borne | seul couple prouvé en Wi-Fi station sur cette tablette avec ce C6 (ESPHome) ; 2.12.12 est dans le dépôt Tab5 et déjà validé par la note « Jiuhai » (Kconfig lu en 2.12.12) | on ne sait pas quelle version d'`esp_wifi_remote` ESPHome tire : **à lire dans le `dependencies.lock` du dossier de build d'ESPHome** (lecture seule, sur le PC) avant de choisir ; SoftAP non prouvé avec ce C6 |
| B (repli) | `esp_hosted == 1.4.0` + `esp_wifi_remote == 0.8.5` | couple exact de M5Stack avec ce firmware C6 1.4.1 (hôte et esclave de même version) | verrouillé sur ESP-IDF 5.4.2 ; compilation sous 5.5.5 **non vérifiée** ; SDIO 40 MHz dans leur `sdkconfig` (voir 2.3) |
| C (ne pas prendre sans essai) | `esp_hosted` 3.0.x/3.1.0 | `idf >=5.5` et `esp32p4` au manifeste | l'hôte 3.x annonce une rétrocompatibilité avec un CP 1.x/2.x mais aucun test lu avec 1.4.1 ; RPC-V2 ; `xiaozhi-esp32` exige IDF ≥ 6.0.1 avec les versions 2.12.11 + 1.6.4 (contrainte du projet, pas du composant) |

Dans `idf_component.yml` du firmware : épingler en **version exacte** (`"2.12.12"` et la version d'`esp_wifi_remote` retenue), jamais `^`, et committer `dependencies.lock`.

## 2. Configuration exacte du Tab5 (et ce qui change sur la 7B)

### 2.1 Broches

| Fonction | Tab5 | Waveshare 7B | Source |
|---|---|---|---|
| SDIO CLK / CMD | GPIO 12 / 13 | GPIO 18 / 19 | `docs/tab5-pinout.md`, `docs/waveshare-7b.md` (Tab5), recoupé par le `sdkconfig` de M5Stack (CLK 12, CMD 13, D0 11, D1 10, D2 9, D3 8) |
| SDIO D0 / D1 / D2 / D3 | GPIO 11 / 10 / 9 / 8 | GPIO 14 / 15 / 16 / 17 | idem |
| Slot SDMMC | `slot: 1` (ESPHome) | non lu | `Tab5/paquets/tab5-hardware.yaml` |
| Fréquence SDIO | 20 MHz (ESPHome) | non lu | idem |
| Reset du C6 | **GPIO 15** | **GPIO 54** (R33 0 Ω sur CHIP_PU) | idem + audit 7B n°1 |
| Alimentation du C6 | PI4IOE5V6408 **0x44, bit 0** (`wifi_power`) ; allumé par le firmware ESPHome au démarrage | pas d'expandeur ; **le C6 partage le 3,3 V du P4** : impossible à couper par logiciel | `docs/tab5-pinout.md`, `docs/troubleshooting.md` |
| Antenne interne/externe | PI4IOE 0x43, bit 0 | — | `docs/tab5-pinout.md` |
| Autre fil du C6 | GPIO 14 relié à l'IO2 du C6 (non utilisé en SDIO) | GPIO 6 ↔ IO2 (R31 0 Ω, non utilisé en SDIO) | M5Stack PinMap via `tab5-pinout.md` ; audit 7B |
| Flash direct du C6 | non lu | connecteur H4 « C6-UART » (TXD, RXD, GND, IO9) | audit 7B |

### 2.2 Contradictions entre sources (à trancher par l'essai, sur la carte)

| Sujet | Valeur A | Valeur B | Ce qu'on en tire |
|---|---|---|---|
| **Polarité du reset du C6 (GPIO15)** | **actif haut** : `esp32_hosted: active_high: true`, le C6 est revenu à chaque démarrage (≈ 50 depuis le 30/09/2026) — `Tab5/paquets/tab5-hardware.yaml`, `docs/hardware.md` | **actif bas** : `CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW=y` dans le `sdkconfig` de M5Stack (hôte 1.4.0) ; le préréglage `ESP32P4_TAB5_C6_BOARD` de la note « Jiuhai » le passerait aussi en actif bas (« sinon le C6 ne sort jamais du reset ») | **non expliqué** (le dépôt Tab5 le dit aussi). Partir de l'**actif haut** (prouvé ici) ; ne tester l'actif bas qu'en cas d'échec. La 7B demande aussi `active_high: true` (« Pas de Wi-Fi, `sdmmc_card_init failed` » si actif bas) |
| **Fréquence SDIO** | 20 MHz (ESPHome ; un utilisateur de l'issue #127 n'est stable qu'à 20 MHz) | 40 MHz (démo M5Stack, hôte 1.4.0) ; la doc du composant cite 50 MHz max et une plage testée jusqu'à 20 MHz | partir de **20 MHz** |
| **Broche de reset par défaut d'`esp_hosted`** | 54 sur tout P4 sauf préréglage Tab5 (note « Jiuhai », Kconfig lu en 2.12.12) | 15 pour le Tab5 (M5Stack) | il faut **fixer explicitement** `CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=15` et vérifier par `grep GPIO_RESET_SLAVE sdkconfig` — sinon l'impulsion part sur la 54 et le C6 n'est jamais réinitialisé |
| **Options de polarité du reset en 3.x** | présentes en 2.x (`RESET_ACTIVE_HIGH`/`LOW`) | « supprimées » en 3.x d'après le changelog 3.0.8 (résumé) | de plus, un argument pour **ne pas** monter en 3.x sans essai |

### 2.3 Séquence d'allumage du C6 à reproduire (ordre des opérations)

1. I2C interne (SDA 31, SCL 32, 400 kHz) ; **attendre l'expandeur** : le reset de l'écran passe par le même PI4IOE5V6408 et il faut **1 s** après le démarrage avant qu'il soit fiable (dépôt Tab5, `docs/troubleshooting.md`, « Black screen after a software reboot »).
2. **PI4IOE 0x44 bit 0 (`wifi_power`) à 1** avant d'initialiser `esp_hosted`. Source : issue [esp-hosted-mcu #127](https://github.com/espressif/esp-hosted-mcu/issues/127) (Tab5, `sdmmc_init_ocr 0x107`) : « le C6 n'était pas alimenté (load switch commandé par la broche PD0 du PI4IOE5V6408 0x44) » — lu en résumé. Le BSP `esp-bsp` nomme cette broche `BSP_WIFI_EN` (bit 0 de l'expandeur), sans GPIO de reset.
3. Choix d'antenne (0x43 bit 0) : valeur par défaut du dépôt Tab5 à reprendre (« déclarée, pas exposée » : valeur exacte **non vérifiée**).
4. `esp_hosted` : reset du C6 par GPIO15 (`SLAVE_RESET_ON_EVERY_HOST_BOOTUP=y`, impulsion active haut de **1 500 ms** observée dans le `sdkconfig` d'ESPHome 2026.9 — `docs/troubleshooting.md`), puis init SDIO slot 1 à 20 MHz.
5. Tampons SDIO en PSRAM (`use_psram: true` dans ESPHome ; `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y` chez `xiaozhi-esp32` ; équivalence entre les deux supposée, non vérifiée). **Piège** : issues [#252](https://github.com/espressif/esp-hosted-mcu/issues/252) / [#167](https://github.com/espressif/esp-hosted-mcu/issues/167) (mempool PSRAM non aligné sur 128 octets avec le cache L2 en 128 B ; avis d'utilisateurs, non validés) — voir la section « Non vérifié ».

## 3. Bugs connus et reprise après redémarrage

| Symptôme | Cause connue | Parade | Source |
|---|---|---|---|
| **7B : boucle de démarrage** après quelques redémarrages logiciels (`sdmmc_req … 0x107`, `H_SDIO_DRV: sdio_read_task: Failed to read data`, puis `rst:0xc` SW_CPU_RESET) | l'état fautif est côté P4 et survit à `esp_restart()` (qui ne remet à zéro que les 2 cœurs) ; le C6 est sain (reset par GPIO54 à chaque démarrage) | terminer chaque redémarrage logiciel par **`esp_rom_software_reset_system()`** (reset du cœur numérique, `rst:0x3`). Constaté sur la 7B : **20 redémarrages de suite sans erreur** après correctif, la même série bouclait au 8ᵉ avant | `docs/troubleshooting.md` (Tab5) ; code `Tab5/ecran/tab5_lien_c6.cpp` (gestionnaire d'arrêt enregistré dans un constructeur) |
| **Tab5 : écran noir et réseau absent après un OTA** (05/08/2026, un seul cas) | le lien SDIO n'est pas remonté (« ESP-Hosted link not yet up » ≈ 50 fois/s qui affament LVGL) ; cause exacte inconnue | coupure d'alimentation ; **le même reset de cœur est appliqué au Tab5 depuis le 16/10, jamais essayé sur Tab5** | idem |
| « ESP-Hosted link not yet up » **une seule fois** par démarrage | normal | ne pas s'en alarmer (≈ 10 s dans une capture série du 07/10, Wi-Fi connecté ≈ 17 s après le reset) | idem |
| **C6 bloqué après `TRANSPORT_FAILURE`** et redémarrage de l'hôte ; trois impulsions CHIP_PU ne le débloquent pas | un mainteneur Espressif (27/10/2026) l'attribue au contrôleur SDMMC du P4 (`esp_restart()` pendant un DMA SDMMC) ; correctif « en revue » dans ESP-IDF, non publié | contournements cités : `esp_rom_software_reset_system()`, couper puis rétablir l'alimentation du C6 (`BSP_WIFI_EN`, 500 ms puis 3 s) | [esp-hosted-mcu #240](https://github.com/espressif/esp-hosted-mcu/issues/240) (ouverte), résumé |
| Récupération d'un C6 bloqué (`send_op_cond 0x107`) | — | « seule une vraie coupure d'alimentation a marché » | [PR #250](https://github.com/espressif/esp-hosted-mcu/pull/250) (ouverte) |
| `Unrecoverable host sdio state` après minutes ou heures | pistes d'utilisateurs : `Cache_WriteBack_Addr` non réentrant, mempool PSRAM non aligné 128 o, matériel | retour à 2.9.3 cité comme stable ; mode streaming en 2.12.13 a tenu pour un utilisateur | [#167](https://github.com/espressif/esp-hosted-mcu/issues/167) (ouverte), résumé |
| Assertions `sdio_rx_get_buffer` / `transport_drv_sta_tx` | pas assez de mémoire DMA interne contiguë ; les buffers DMA du driver SDIO restent en SRAM interne même avec la PSRAM | à surveiller pendant les essais audio (1c) qui consomment de la SRAM interne | [#144](https://github.com/espressif/esp-hosted-mcu/issues/144) (ouverte), résumé |
| P4+C6 Waveshare, hôte 2.12.8 : boucle de redémarrage, même après retour à 2.12.1 | cause jamais identifiée | éviter 2.12.8 ; **2.12.12 est la version du dépôt Tab5** | [esphome #16692](https://github.com/esphome/esphome/issues/16692) (fermée « not planned » le 02/09/2026), résumé |
| Reboot en boucle P4+C6 avec esp_hosted (ESPHome) | non résolu dans ce qui a été lu | — | [esphome #10393](https://github.com/esphome/esphome/issues/10393), résumé |
| Mauvaises broches SDIO (cartes de dev) | `sdmmc_card_init failed` | broches du Tab5 : section 2.1 | [arduino-esp32 #11404](https://github.com/espressif/arduino-esp32/issues/11404) (fermée) |

**Règles de redémarrage pour le firmware natif** (déjà dans `AGENTS.md` du dépôt) : tout redémarrage logiciel se termine par `esp_rom_software_reset_system()` ; ne jamais redémarrer parce que le serveur manque ; ouvrir/fermer le port série USB peut réinitialiser la puce (ne pas conclure à une instabilité sur un reset en fin de capture).

**Non essayé (hypothèse du dépôt Tab5, confirmée par l'issue #240)** : couper puis rétablir `wifi_power` (PI4IOE 0x44 bit 0) comme reprise **logicielle** d'un lien C6 bloqué sur Tab5. À mesurer en 1b (sur la 7B ce n'est pas possible).

## 4. SoftAP et portail captif (1d)

### 4.1 SoftAP par `esp_wifi_remote` / `esp_hosted`

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Exemples officiels de l'hôte | `examples/wifi/` contient `apsta`, `dpp`, `enterprise`, `iperf`, `itwt`, `scan`, `softap`, `sta` ; hôte P4 avec C6 présenté comme référence | [esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu), [examples/wifi](https://github.com/espressif/esp-hosted-mcu/tree/main/examples/wifi) | 06/11/2026, résumé |
| Code de l'exemple SoftAP (côté hôte, v3.0.0, cibles dont P4) | `esp_netif_create_default_wifi_ap()`, `esp_wifi_set_mode(WIFI_MODE_AP)`, `esp_wifi_set_config(WIFI_IF_AP, …)` ; **aucun code propre à hosted** ; nombre max de clients = `CONFIG_ESP_MAX_STA_CONN` | [softap_example_main.c](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/main/examples/wifi/softap/mcu_host/main/softap_example_main.c), [softap mcu_host 3.0.0](https://components.espressif.com/components/espressif/esp_hosted/versions/3.0.0/examples/wifi/softap/mcu_host) | 06/11/2026, résumé |
| DHCP du SoftAP | le README de l'exemple dit que l'**hôte** démarre le serveur DHCP quand le netif AP démarre (192.168.4.1/24) — la pile lwIP est sur le P4 ; emplacement exact à relire dans le code | [README softap](https://raw.githubusercontent.com/espressif/esp-hosted-mcu/main/examples/wifi/softap/README.md) | 06/11/2026, résumé |
| README d'`esp_wifi_remote` | ne mentionne ni AP ni APSTA ; « full compatibility with the standard esp_wifi API » ; pile TCP/IP seulement côté hôte avec esp-hosted ; **config du slave non modifiable à l'exécution** ; débit TCP ≈ 50 Mbit/s (hosted), 20 (eppp), 2 (AT) | [esp-wifi-remote](https://github.com/espressif/esp-wifi-remote), [blog Espressif 09/2025](https://developer.espressif.com/blog/2025/09/esp-wifi-remote/) | 06/11/2026, résumé |
| Événements | le C6 envoie un événement RPC `AP_STACONNECTED` à l'hôte à chaque client (exemple `cp` v3.0.7) | [softap cp 3.0.7](https://components.espressif.com/components/espressif/esp_hosted/versions/3.0.7/examples/wifi/softap/cp) | 06/11/2026, résumé |
| Issue [#37](https://github.com/espressif/esp-hosted-mcu/issues/37) « SoftAP and DHCP Server » | **ouverte** (12/03/2025) ; **deux C6 en SPI** : le client n'obtient pas d'IP ; réponse Espressif du 20/03/2025 : l'exemple SoftAP gère le DHCP, conseil de prendre des puces différentes pour l'hôte et le coprocesseur | idem | 06/11/2026, résumé |
| Issue [arduino-esp32 #12149](https://github.com/espressif/arduino-esp32/issues/12149) | SoftAP OK sous Arduino, échec sous IDF + bibliothèque Arduino sur P4+C6 : transport hosted initialisé deux fois | idem | 06/11/2026, résumé via recherche |
| `wifi_provisioning` / `network_provisioning` sur P4+C6 | **aucun exemple officiel trouvé** ; seul le DPP (QR code) est documenté pour P4+C6 (2.4.3) | — | 06/11/2026 |
| Limites : nombre de clients, canal, mode APSTA avec STA déjà connectée | **non vérifié** (aucune valeur dans les pages lues) | — | — |
| Cas voisin (C6 seul, pas P4) | une STA sans IP tant qu'aucun client ne rejoint l'AP en APSTA | [esp-idf #12478](https://github.com/espressif/esp-idf/issues/12478) | 06/11/2026, résumé |

**Cela suffit à dire** que le SoftAP côté P4 est un appel `esp_wifi` ordinaire routé vers le C6 par `esp_wifi_remote`, et qu'Espressif en fournit un exemple sur P4. **Cela ne démontre pas** qu'il marche avec un C6 1.4.1, ni avec un portail captif : aucune démonstration SoftAP + portail captif sur P4+C6 n'a été trouvée (recherche dans `esp-hosted-mcu`, `esp-idf`, `esp-wifi-remote` ; **`xiaozhi-esp32` non cherché**). Il faut assembler l'exemple SoftAP d'`esp_hosted` et l'exemple `captive_portal` d'ESP-IDF.

### 4.2 Portail captif côté ESP-IDF

| Fait | Valeur | Source | Lu |
|---|---|---|---|
| Composant `espressif/dns_server` du registre | **introuvable** (l'API renvoie 404, la recherche ne le liste pas). Le `dns_server` est un composant **local** de l'exemple, sans version de registre | [captive_portal (v5.5.1)](https://github.com/espressif/esp-idf/tree/v5.5.1/examples/protocols/http_server/captive_portal) | 06/11/2026, résumé |
| Licence du `dns_server` | `Unlicense OR CC0-1.0` (SPDX dans l'en-tête, © Espressif 2021-2023) | [dns_server.h (release/v5.5)](https://raw.githubusercontent.com/espressif/esp-idf/release/v5.5/examples/protocols/http_server/captive_portal/components/dns_server/include/dns_server.h) | 06/11/2026, résumé |
| API | `start_dns_server(dns_server_config_t*)` → handle ou NULL ; `stop_dns_server(handle)` ; macro `DNS_SERVER_CONFIG_SINGLE(name, netif_key)` ; `DNS_SERVER_MAX_ITEMS` = 1 par défaut ; **seules les requêtes A** sont traitées ; les chaînes ne sont pas copiées | idem | 06/11/2026, résumé |
| Capture de tout | `DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF")` répond à toute requête A avec l'IP du netif AP | [main.c (release/v5.5)](https://raw.githubusercontent.com/espressif/esp-idf/release/v5.5/examples/protocols/http_server/captive_portal/main/main.c) | 06/11/2026, résumé |
| Réponse HTTP de l'exemple | le handler 404 renvoie **« 303 See Other », `Location: /`**, avec un corps « Redirect to the captive portal » ; le commentaire du code dit que le corps est nécessaire car iOS ne détecte pas le portail avec une redirection sans contenu | idem | 06/11/2026, résumé |
| Option DHCP 114 (RFC 8910) | active si `CONFIG_ESP_ENABLE_DHCP_CAPTIVEPORTAL` : `esp_netif_dhcps_stop`, `esp_netif_dhcps_option(…, ESP_NETIF_CAPTIVEPORTAL_URI, "http://<ip>")`, `esp_netif_dhcps_start` | idem | 06/11/2026, résumé |
| README de l'exemple | DNS vers l'IP de l'AP + redirection HTTP ; **les requêtes HTTPS ne sont pas redirigées** ; l'option 114 évite les redirections DNS et gère mieux HTTPS/HSTS ; les deux méthodes se combinent | [README](https://github.com/espressif/esp-idf/tree/v5.5.1/examples/protocols/http_server/captive_portal) | 06/11/2026, résumé |
| Version de l'exemple lue | `release/v5.5` et `v5.5.1` ; **v5.5.5 non lue** | — | — |
| Alternatives tierces | `espp/dns_server` (MIT, classe C++, API différente, version : 1.0.36 ou 1.4.0 selon la source) ; `nordesems/esp-captive-portal` 1.3.0 (option 114 par défaut, `httpd_uri_match_wildcard`) | [espp/dns_server](https://components.espressif.com/components/espp/dns_server/versions/1.0.36/readme?language=), [nordesems](https://components.espressif.com/components/nordesems/esp-captive-portal/versions/1.3.0/readme) | 06/11/2026, résumé |
| RFC 8910 | option DHCPv4 **114** = URI de l'API RFC 8908, 255 octets max ; HTTPS recommandé, non imposé ; l'URI ne devrait pas contenir de littéral IP « sauf cas limités » | [RFC 8910](https://www.rfc-editor.org/rfc/rfc8910.html) | 06/11/2026, résumé |

### 4.3 Ce que font les téléphones (sources tierces : **les pages officielles Android et Apple n'ont pas pu être lues**)

| Système | Sonde | Réaction | Source | Statut |
|---|---|---|---|---|
| Android | `connectivitycheck.gstatic.com/generate_204` (principale) ; `clients2.google.com/generate_204`, `connectivitycheck.android.com` en secours ; `clients3.google.com` aussi cité | 204 = internet disponible ; redirection = portail ; un 200 avec corps ne déclenche pas de façon fiable la fenêtre | [i3-connect](https://docs.i3-connect.com/how-to/how-android-detects-portals-and-connectivity), [Purple](https://www.purple.ai/guides/captive-portal-detection-cna-android-ncsi) | tiers, **non vérifié** |
| Android 11+ | API « Captive Portal » par l'option DHCP 114 | — | idem | tiers, non vérifié |
| iOS/macOS | `http://captive.apple.com/hotspot-detect.html` ; réponse attendue : HTML avec « Success » ; toute autre réponse ouvre le CNA ; le CNA ne suit pas une redirection HTTPS | — | [Wikipédia (miroir)](https://wiki.riteme.site/wiki/Captive_portal), Purple | tiers, non vérifié |
| Windows | sonde HTTP comparée au contenu attendu « Microsoft Connect Test » ; URL `msftconnecttest.com/connecttest.txt` à confirmer | — | [Microsoft NCSI](https://learn.microsoft.com/en-us/windows-server/networking/ncsi/ncsi-overview) | officiel (partiel) |
| SoftAP sans internet ni portail | Android affiche « connecté, sans internet » et peut dépriorisser le réseau / basculer sur les données mobiles (source : un ticket de suivi tiers) ; iOS « peut quitter le réseau » : **aucune source** | — | [ticket](https://support.piramalfoundation.org/jira/browse/STOP-449) | non vérifié |

→ **Ne jamais répondre 204 à `generate_204`** (ce serait dire « pas de portail »). Répondre à toute requête HTTP inconnue par une redirection **avec un corps non vide** vers `http://<IP de l'AP>/`, **et** annoncer l'URL du portail par l'option DHCP 114 si `esp_netif` l'accepte sur le netif AP hosted. Il n'y a **aucune source primaire** qui tranche 302 contre 303 ; l'exemple d'Espressif utilise 303.

### 4.4 Retours d'expérience (ESP32 en général, pas P4+C6)

| Fait | Source | Lu |
|---|---|---|
| Projet CDFER : DNS qui redirige tout. Réussi sur Android 9–12 et iOS 9–16 avec quelques échecs ; OnePlus 6/Android 11 : « cellular must be off » ; macOS après Big Sur : pas de popup ; le projet n'implémente pas le DHCP/RA requis par iOS 14+ ; 4 clients max testés ; Windows 11 : la charge DNS augmente avec les onglets | [captive-portal-esp32](https://github.com/CDFER/captive-portal-esp32) | 06/11/2026, résumé |
| Une redirection dans le handler de sonde ouvre la page, mais la bannière du portail revient sur Android | [esp32.com](https://esp32.com/viewtopic.php?p=143180) | 06/11/2026, résumé de recherche |
| Le DNS doit démarrer **après** l'AP ; scanner pendant que l'AP tourne coupe brièvement la radio | [espboards.dev](https://www.espboards.dev/troubleshooting/issues/wifi/esp32-captive-portal-issues/) | 06/11/2026, résumé |
| Plusieurs pannes viennent d'un `esp_netif_create_default_wifi_ap()` oublié ou appelé avant `esp_event_loop_create_default()` | [esp-idf #6108](https://github.com/espressif/esp-idf/issues/6108) | 06/11/2026, résumé |

## 5. SNTP et fuseau : seulement ce qui change ici

Le fond est dans [03-meteo-heure-reglages-calendrier.md](03-meteo-heure-reglages-calendrier.md) §2 (`esp_netif_sntp_init`, `TZ` POSIX, dernier fuseau en NVS). Ce qui change avec le C6 :

- La pile lwIP et le client SNTP tournent **sur le P4** ; seul le Wi-Fi est distant. Les événements `IP_EVENT_STA_GOT_IP` viennent du netif créé par `esp_wifi_remote` : démarrer SNTP à cet événement (`esp_netif_sntp_init` après l'IP), pas avant.
- Le C6 met ≈ 17 s à donner le Wi-Fi après un reset (capture série du 07/10/2026, dépôt Tab5) : **l'écran doit afficher l'heure du RTC dès le démarrage** (RX8130 en 0x32, UTC, écrit après chaque synchronisation dans le firmware ESPHome) et ne pas attendre le réseau. La 7B n'a pas de RTC : heure absente tant que SNTP n'a pas répondu.
- `esp_netif_sntp_sync_wait()` ne doit jamais bloquer le démarrage ni l'écran (le produit tourne hors ligne).
- Critère de 1b « heure correcte après redémarrage logiciel et après coupure » : après **coupure sans réseau**, seule la RTC (Tab5) donne l'heure ; sur la 7B, l'heure est fausse jusqu'au SNTP — noter la différence plutôt que de la compter comme un échec.

## 6. Plan d'essai de 1b

**Garde-fous (`AGENTS.md`)** : pas de flash sans demande explicite d'Axel, sauvegarde faite le 06/11 (volontairement partielle, recette jamais essayée), MAC lue avant écriture, verrou `tab5-st7121`, une seule compilation à la fois, `CMAKE_BUILD_PARALLEL_LEVEL=4`, chargeur 5 V/2 A (pas un port USB de PC faible).

| Étape | Action | Constat attendu (preuve) | Si échec |
|---|---|---|---|
| 0 | Lire (PC, lecture seule) le `dependencies.lock` / `sdkconfig` du dossier de build ESPHome pour noter la version exacte d'`esp_wifi_remote` et les `CONFIG_ESP_HOSTED_*` du couple prouvé | 2 versions notées dans `docs/mesures/1b.md` | coupler 2.12.12 avec la dernière 1.x d'`esp_wifi_remote` qui l'accepte |
| 1 | Programme minimal : I2C + expandeur (attente 1 s), `wifi_power`=1, `esp_hosted` épinglé, SDIO 20 MHz, reset GPIO15 actif haut, `esp_wifi` station | journal : « ESP-Hosted link », version du co-processeur lue (**doit dire 1.4.1** ; confirme la tablette), « Version mismatch » éventuel noté (pas une erreur) | 0x107 : vérifier `wifi_power`, puis `grep GPIO_RESET_SLAVE sdkconfig`, puis polarité du reset |
| 2 | Connexion au Wi-Fi de la maison (identifiants en NVS, jamais dans le dépôt) | IP obtenue ; temps écoulé entre le reset et l'IP (≈ 17 s dans ESPHome) | essayer l'étape B de 1.4 |
| 3 | `esp_netif_sntp_init` après l'IP ; afficher heure et date | heure correcte comparée à une horloge externe ; **écart en secondes** noté | — |
| 4 | Redémarrage logiciel unique par `esp_rom_software_reset_system()` ; heure à l'écran | `rst:0x3` dans le journal série, IP et heure revenues | noter le numéro du cycle et la dernière ligne série ; voir le critère ci-dessous |
| 5 | **Coupure d'alimentation** (débrancher, 15 s, rebrancher) | heure du RTC à l'écran sans réseau, puis SNTP | — |
| 6 | **20 redémarrages de suite** : boucle pilotée par le PC | critère ci-dessous | passer à l'étape de repli |

**Critère « 20 redémarrages de suite sans perte du lien »** : sur 20 cycles consécutifs (`esp_rom_software_reset_system()` déclenché depuis le firmware, un toutes les ≈ 60 s après l'IP), **chaque** démarrage satisfait : (a) `rst:0x3` lu sur le port série ; (b) IP obtenue ; (c) heure SNTP valide ; (d) aucun `sdmmc_req … 0x107`, aucun `Failed to read data`, aucun `TRANSPORT_FAILURE`. Mesure : le firmware écrit dans le journal série une ligne par démarrage (`BOOT n rst=0x3 c6=<version> ip_ms=<ms> sntp_ms=<ms>`) et le PC compte les 20 lignes. **Un seul cycle sans IP au bout de 60 s = échec** de la série (la 7B avait bouclé au 8ᵉ puis au 3ᵉ cycle de séries antérieures). Compléter par **un cycle de coupure d'alimentation** tous les 10 cycles logiciels. Rappel : ouvrir/fermer le port série peut réinitialiser la puce — capturer sans fermer (`tools/capture_serie.py` du dépôt Tab5 utilise la MAC pour retrouver le port) ou compter côté firmware seulement.

**Repli** (déjà au plan) : autre version d'`esp_hosted` (1.4 → B, puis 3.x → C) ; couper/rétablir `wifi_power` pour débloquer le C6 sans débrancher ; **ne jamais mettre à jour le C6**.

**Essais ajoutés pour 1d** (après 1b vert) : (i) `WIFI_MODE_AP` seul puis `WIFI_MODE_APSTA` (la station reste connectée pendant que le SoftAP sert la page) ; (ii) DHCP : un téléphone obtient 192.168.4.x ; (iii) DNS « * » + redirection 303 avec corps + option 114 ; (iv) test par Axel sur un Android et un iPhone, constaté (fenêtre de portail ou non) ; (v) consigner le nombre de clients simultanés réellement acceptés.

## Non vérifié

- La tablette dont le capteur « C6 Version » de Home Assistant donne `1.4.1` est-elle la ST7121 de développement ? (probable, non lié à une MAC).
- Compatibilité de l'hôte 2.12.12, 1.4.0 ou 3.x avec le C6 en 1.4.1 pour le **SoftAP** et le DHCP ; présence de l'API AP côté C6 1.4.1.
- Version d'`esp_wifi_remote` utilisée par ESPHome ; borne `esp_hosted` de `esp_wifi_remote` 1.6.5 ; compilation d'`esp_hosted` 1.4.0 + `esp_wifi_remote` 0.8.5 sous ESP-IDF 5.5.5.
- Le préréglage `ESP32P4_TAB5_C6_BOARD` d'`esp_hosted` (Kconfig de l'hôte en 404 ; seule la note « Jiuhai » du dépôt Tab5 le décrit).
- Polarité du reset (actif haut chez ESPHome, actif bas chez M5Stack).
- Le correctif ESP-IDF cité dans l'issue #240 (SDMMC + `esp_restart()`) : état et version d'ESP-IDF qui le contient.
- Nombre de clients, canal, APSTA, DHCP du SoftAP sur C6 ; `esp_netif_dhcps_option(ESP_NETIF_CAPTIVEPORTAL_URI)` sur le netif AP créé par `esp_wifi_remote`.
- Exemple `captive_portal` en IDF 5.5.5 (lu en `release/v5.5` et `v5.5.1`).
- Documentation primaire Android et Apple de la détection de portail ; le comportement d'iOS qui quitte un SoftAP sans internet ; la différence 302 / 303.
- Alignement 128 octets du mempool PSRAM d'`esp_hosted` et son interaction avec `CONFIG_CACHE_L2_CACHE_LINE_*` (avis d'utilisateurs seulement).
- `xiaozhi-esp32` pour un SoftAP/portail sur P4 : non cherché.

## Ce que la piste matériel doit faire en premier

1. Lire (PC, sans toucher à la carte) le `dependencies.lock` et le `sdkconfig` du dossier de build ESPHome pour copier le couple prouvé `esp_hosted` 2.12.12 / `esp_wifi_remote` et les `CONFIG_ESP_HOSTED_*`.
2. Mettre `wifi_power` (PI4IOE 0x44 bit 0) à 1 **avant** `esp_hosted`, fixer `GPIO_RESET_SLAVE=15`, reset actif haut, SDIO 20 MHz, tampons en PSRAM, puis vérifier par `grep` dans le `sdkconfig` généré.
3. Lire dans le journal série la version du C6 (attendue `1.4.1`) ; n'écrire rien sur le C6.
4. Terminer tout redémarrage par `esp_rom_software_reset_system()` et mesurer 20 cycles avec une ligne `BOOT n …` par démarrage ; ne passer à 1d (SoftAP + DNS « * » + 303 avec corps + option 114) qu'après 1b vert.
