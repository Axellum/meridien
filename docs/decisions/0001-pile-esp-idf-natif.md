# ADR-0001 — Pile : ESP-IDF natif + LVGL 9

> **Summary (EN)** — Proposed on 2026-11-06, awaiting the owner's validation.
> - Decision: native ESP-IDF with Espressif registry components (`esp_lvgl_port`, `esp_lcd_*`, `esp_codec_dev`, `esp_hosted`/`esp_wifi_remote`, `esp-sr`), LVGL 9 and an in-house board layer (ADR-0005). Starting version ESP-IDF 5.5.5, already installed on the maintainer's PC (by ESPHome) and proven there: an empty skeleton compiled for `esp32p4` in 38 s (204,048 bytes). Moving to 6.x is postponed until `esp_hosted` and `esp-sr` are confirmed to work with it.
> - Context: a screen independent of ESPHome and Home Assistant, professional-quality code, two ESP32-P4 boards (Tab5, then Waveshare 7B), built with AI agents and judged on the device.
> - Rejected: Arduino/PlatformIO with M5Unified/M5GFX (quick prototype, but opaque drivers and C6 handling, weaker for OTA, tests and two boards) and ESPHome (what the project is leaving).
> - Why 5.5.x and not 6.1: 6.1 is the latest (2026-08-27) but 6.x brings breaking changes (Picolibc by default) and the compatibility of `esp_hosted` 3.1.0 and `esp-sr` is not confirmed; the current ESPHome firmware runs on 5.5.x on both boards; `xiaozhi-esp32` needs 5.5.2 or later; the Waveshare CI tests 5.5.5 and 6.0.2.
> - Consequences: the ESP-IDF install on the maintainer's PC belongs to ESPHome and must never be modified; `esp_hosted` and `esp_wifi_remote` are pinned to the 2.12.x line (no C6 update, owner's decision of 2026-10-16); C++ exceptions are off and `esp_err_t` is used at the boundaries.

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : écran indépendant (sans ESPHome ni Home Assistant), code « pro », deux cartes ESP32-P4 (Tab5 puis Waveshare 7B), développé avec des agents IA et jugé sur l'appareil. Recherche : [01-pile-et-qualite.md](../recherche/01-pile-et-qualite.md).
- **Décision** : ESP-IDF natif, composants du registre Espressif (`esp_lvgl_port`, `esp_lcd_*`, `esp_codec_dev`, `esp_hosted`/`esp_wifi_remote`, `esp-sr`), LVGL 9, couche carte maison (ADR-0005). **Version de départ : ESP-IDF 5.5.5**, déjà installée sur le PC d'Axel (par ESPHome) et prouvée (`tools/idf.ps1`, squelette compilé pour `esp32p4` en 38 s, 204 048 octets). Migration vers 6.x décidée plus tard, quand `esp_hosted` et `esp-sr` y seront confirmés.
- **Écartés** : Arduino/PlatformIO + M5Unified/M5GFX (prototype rapide, mais pilotes et gestion du C6 opaques, moins sain pour OTA, tests et deux cartes) ; ESPHome (c'est justement ce qu'on quitte).
- **Pourquoi 5.5.x et pas 6.1** : 6.1 est la dernière (27/08/2026) mais 6.x apporte des ruptures (Picolibc par défaut) et la compatibilité d'`esp_hosted` 3.1.0 et d'`esp-sr` n'est pas confirmée ; le firmware ESPHome actuel de l'auteur tourne sur 5.5.x pour les deux cartes ; xiaozhi-esp32 demande ≥ 5.5.2 ; la CI Waveshare teste 5.5.5 et 6.0.2.
- **Conséquences** :
  - Le dossier ESP-IDF du PC d'Axel appartient à ESPHome (fichier `.esphome.stamp.json`, layout différent de l'installeur officiel, `idf-env.json` qui cite encore le chemin d'un autre disque, mort). Ne jamais le modifier. S'il change de version ou disparaît : installer un ESP-IDF à part (≈ 2-3 Go, ~15 min, estimation) et changer `IdfRacine` dans `tools/local.ps1`.
  - `esp_hosted` / `esp_wifi_remote` **épinglés** sur la lignée compatible avec le C6 actuel (2.12.x) : décision d'Axel du 16/10/2026, pas de mise à jour du C6.
  - Exceptions C++ désactivées ; `esp_err_t` aux frontières.
