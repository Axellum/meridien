# Pile de développement et qualité — recherche du 06/11/2026

> **Summary (EN)** — Research note of 2026-11-06 on the development stack and quality tooling (two web-research agents plus checks on the maintainer's PC). "Vérifié" = confirmed by a command that day; "Non vérifié" = from a single source, to confirm before relying on it.
> - Conclusion: native ESP-IDF + Espressif registry components + LVGL 9 through `esp_lvgl_port` + an in-house board layer (ADR-0001). Arduino/PlatformIO with M5Unified is faster for a prototype but its drivers and C6 handling are more opaque, so it is less sound for OTA, tests and two boards.
> - Versions read that day: ESP-IDF 6.1 (27 Aug), 6.0.3, 5.5.6; LVGL 9.6.0; `esp_lvgl_port` 2.9.0; `esp_hosted` 3.1.0 / `esp_wifi_remote` 1.6.5. Verified on the PC: ESP-IDF 5.5.5 (GCC 14.2.0), which includes MIPI-DSI support for the P4.
> - Board sources: the Tab5 display changed over time (ILI9881C + GT911, then ST7123, then ST7121) and sources contradict each other about the BSP's drivers; the Waveshare 7B repository is a model for per-chip-revision sdkconfig files; there is no registry component for the RX8130 RTC.
> - Constraint: the owner decided on 2026-10-16 never to update the C6 coprocessor, so `esp_hosted` and `esp_wifi_remote` must be pinned to versions compatible with the C6 firmware already on the boards.
> - P4 pitfalls listed: tearing and rotation (a fixed panel orientation avoids them), PSRAM settings, Tab5 screen-reset timing, the battery charger, Secure Boot ECDSA disabled on the P4. A proposed (not official) layout separates a pure C++ core testable on a PC from drivers, UI and board layers.
> - Testing: no QEMU for the P4, Wokwi P4 is beta, an LVGL PC simulator with screenshot comparison helps (beware: it creates a missing reference, a false green), a hardware bench is still needed. CI and reliability: ESP-IDF CI action, OTA rollback after a self-test (confirming too early defeats it), core dump in flash, C++ exceptions off. Several tool suggestions are marked "not verified".

Sources : 2 agents de recherche web (pile ; architecture et qualité) + vérifications faites sur le PC d'Axel. **Vérifié** = constaté par une commande ce jour-là. **Non vérifié** = affirmé par une source seule, à confirmer avant de s'y appuyer.

## Conclusion

ESP-IDF natif + composants du registre Espressif + LVGL 9 via `esp_lvgl_port` + une couche carte maison. Ni Arduino/PlatformIO ni M5Unified : plus rapide pour un prototype, mais pilotes et gestion du C6 plus opaques, donc moins sain pour OTA, tests et deux cartes. Voir ADR-0001.

## Versions (lues le 06/11/2026)

| Élément | Version | Source |
|---|---|---|
| ESP-IDF | 6.1 « Latest » (27/08) ; 6.0.3 (02/09) ; 5.5.6 (16/10) | github.com/espressif/esp-idf/releases |
| ESP-IDF sur le PC | **5.5.5, vérifié** (`idf.py --version`, GCC RISC-V 14.2.0, CMake 3.30.2, Ninja 1.12.1) dans le dossier ESP-IDF installé par ESPHome sur le PC d'Axel | `tools/idf.ps1` |
| LVGL | 9.6.0 (16/09) | github.com/lvgl/lvgl/releases |
| `esp_lvgl_port` | 2.9.0 (LVGL 8 et 9) ; `esp_lvgl_adapter` (IDF ≥ 5.5, gère tearing et rotation) | components.espressif.com |
| `esp_hosted` / `esp_wifi_remote` | 3.1.0 / 1.6.5 | components.espressif.com |
| IDF 6.1 et révision de puce | P4 v3.0 par défaut ; une puce plus ancienne demande `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` | notes de version IDF |

Choix de version : voir ADR-0001 (5.5.x pour démarrer ; 6.x plus tard, migration avec ruptures notamment Picolibc).

## Cartes, pilotes, exemples

- **Tab5** : BSP `espressif/m5stack_tab5` 1.0.0 dans esp-bsp (IDF ≥ 5.4, `esp_lvgl_port ^2`, `esp_codec_dev ~1.5`). *Contradiction entre sources* : l'une dit que ses pilotes sont ST7123 + touch ST7123, l'autre ILI9881C + GT911 → lire son `idf_component.yml` avant de s'en servir. BSP communautaire `espp/m5stack-tab5` 1.4.0 : **détecte ILI9881, ST7123 et ST7121** en lisant le registre de version du tactile Sitronix à l'adresse 0x55 (1 = ST7121, 3 = ST7123 ; GT911 à 0x14 = ILI9881 d'origine) ; les trois ont des séquences d'init et des débits de lignes DSI différents.
- **Écran Tab5** d'après la doc M5Stack : ILI9881C+GT911 à l'origine, ST7123 depuis le 14/10/2025, ST7121 depuis le 28/04/2026 (type lu sur l'autocollant au dos). Les deux tablettes d'Axel : ST7123 (en SAV) et ST7121 (en service).
- **Exemples** : `m5stack/M5Tab5-UserDemo` (IDF 5.4.2, build PC SDL2 à côté du build IDF : bonne référence pour le simulateur) ; `78/xiaozhi-esp32` (MIT, dossier `boards/m5stack/tab5`).
- **Waveshare 7B** : `waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B` : 18 exemples IDF, CI sur IDF 5.5.5 et 6.0.2, BSP géré `waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.1 (aucun BSP Waveshare dans esp-bsp), dossier `config/` avec `esp32p4_rev1_3.defaults` et `esp32p4_rev3_x.defaults` : **modèle à reprendre** pour les `sdkconfig` par révision de puce.
- **Pilotes du registre** : `esp_lcd_st7123` 1.0.2, `esp_lcd_st7121` 1.0.1 (+ fork `m5stack/esp_lcd_st7121`), `esp_lcd_ek79007` 2.0.2. ILI9881C, GT911, ES8388, ES7210, ES8311 : présents d'après la matrice d'esp-bsp (non ouverts un par un). **RX8130 (RTC) : aucun composant** → écrire (quelques dizaines de lignes d'I2C).
- IDF 5.5.5 contient bien `esp_lcd/dsi` (MIPI-DSI) et le port `esp32p4` (**vérifié**).

## Wi-Fi par le C6 — contrainte propre à Axel

`esp_hosted` hôte et esclave **doivent avoir des versions compatibles** (mise à jour du C6 par `esp_hosted_slave_ota()` ou par l'UART). Or **Axel a décidé le 16/10/2026 de ne jamais mettre à jour le C6** (7B : version illisible ; Tab5 : 1.4.1). Le firmware ESPHome actuel marche avec esp_hosted 2.12.12 côté hôte. → **Épingler `esp_hosted` et `esp_wifi_remote` sur la lignée 2.12.x compatible, ne pas prendre 3.1.0 sans test.** Compatibilité de 3.1.0 avec IDF 5.5 et 6.1 : non vérifiée. Échec de mise à jour du C6 par SDIO signalé sur d'autres cartes P4 : si un jour il le faut, prévoir un accès UART (connecteur H4 « C6-UART » sur la 7B).

## Pièges du P4 en natif

- **Tearing et rotation** : avec `esp_lvgl_adapter` sans anti-tearing, la rotation est refusée sur DSI ; rotation non nulle ou triple buffer = 3 frame buffers (le BSP espp offre `CONFIG_M5STACK_TAB5_DPI_NUM_FBS`). Une orientation fixe câblée dans le panneau évite le problème (cohérent avec ADR-0012 du dépôt Tab5 : la rotation logicielle coûte 82 ms contre 69 ms à 0°).
- **PSRAM** : l'adapter conseille `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, `CONFIG_CACHE_L2_CACHE_256KB=y`, `CONFIG_CACHE_L2_CACHE_LINE_128B=y`. PSRAM 200 MHz : `CONFIG_SPIRAM_SPEED_200M` + `CONFIG_IDF_EXPERIMENTAL_FEATURES` (source communautaire seule, non vérifié dans les Kconfig). Sur la 7B, sans PSRAM 200 MHz, ~30 « lcd.dsi underrun » par seconde (dépôt Tab5).
- **Reset de l'écran Tab5** : la broche LCD_RST (PI4IO, bit 4) doit être push-pull ou open-drain quand elle passe à bas, entrée pull-up quand elle passe à haut, sinon l'I2C devient instable. Détecter le contrôleur avant d'initialiser le DSI.
- **Chargeur de batterie du Tab5** : à activer seulement si une batterie est présente, sinon bruit dans le haut-parleur.
- **Secure Boot ECDSA** désactivé sur le P4 : ne pas compter dessus.
- Non couvert par les sources : pièges DMA2D/PPA, fréquence CPU 360/400 MHz (à lire dans les Kconfig d'IDF).

## Architecture proposée (ma proposition, pas un modèle officiel)

```
main/                  app_main seulement, câblage
components/core/       C++ pur, sans include IDF (testable sur PC)
components/drivers/    pilotes, retournent esp_err_t
components/ui/         écrans LVGL
components/bsp_tab5/   couche carte (+ bsp_waveshare_7b/)
boards/<carte>/sdkconfig.defaults.board
test/host/  test/ui_sim/  test/target/
tools/  .github/workflows/  docs/
```

Référence de couche carte : `esp-brookesia` (`boards/<fabricant>/<carte>/` avec `board_info.yaml`, `board_devices.yaml`, `board_peripherals.yaml`, `sdkconfig.defaults.board`, `setup_device.c`).

## Tests

- **Cible `linux` d'IDF** (FreeRTOS POSIX + CMock) : encore expérimentale (`COMPONENTS main` obligatoire, Ruby pour les mocks). Ne remplace pas le matériel. Alternative : Catch2/GoogleTest hors IDF sur `components/core/`.
- **QEMU : pas de P4** (ESP32, S3, C3 seulement). **Wokwi** : P4 en bêta (puces 3.1 ou 1.0), I2S non implémenté, aucun écran → utile pour la logique seulement, pas pour le rendu MIPI-DSI.
- **Simulateur LVGL PC** : `lv_port_pc_eclipse` (SDL) ; approche SDL2 de M5Tab5-UserDemo. **Captures** : `lv_test_screenshot_compare` (PNG 32 bits, écrit `*_err.png` en cas d'écart). *Piège* : elle **crée la référence si elle manque** → faux « vert » ; versionner les références et refuser leur création en CI.
- Un banc sur matériel (auto-hébergé) reste nécessaire pour le MIPI-DSI et le tactile.

## CI, fiabilité

- `espressif/esp-idf-ci-action@v1` (images Docker `espressif/idf`, entrée `esp_idf_version` = tag Docker existant) ; `idf-ci` pour multi-cibles et pytest ; `esp-idf-sbom` (expérimental, SPDX + NVD) ; `upload-components-ci-action@v2` (OIDC) pour publier des composants.
- **Rollback OTA** : `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` ; nouvelle image « en attente de vérification », `esp_ota_mark_app_valid_cancel_rollback()` après auto-test, `esp_ota_mark_app_invalid_rollback_and_reboot()` si l'auto-test échoue. *Piège* : confirmer trop tôt rend le rollback inutile.
- **Core dump en flash** : partition dédiée dans une table personnalisée ; `idf.py coredump-info` côté PC.
- **Exceptions C++** désactivées par défaut (coût de taille et lenteur du lancement) → `esp_err_t` aux frontières.
- **Non vérifié** (cité de mémoire par l'agent) : `idf.py size` avec budget, clang-tidy/cppcheck/gitleaks en CI, gestion de `idf_component.yml` par Renovate/Dependabot (plan B : règle regex), mode sans échec, télémétrie locale ; arborescences d'esp-box, xiaozhi-esp32, WLED-MM : non consultées.
