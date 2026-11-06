# ADR-0001 — Pile : ESP-IDF natif + LVGL 9

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : écran indépendant (sans ESPHome ni Home Assistant), code « pro », deux cartes ESP32-P4 (Tab5 puis Waveshare 7B), développé avec des agents IA et jugé sur l'appareil. Recherche : [01-pile-et-qualite.md](../recherche/01-pile-et-qualite.md).
- **Décision** : ESP-IDF natif, composants du registre Espressif (`esp_lvgl_port`, `esp_lcd_*`, `esp_codec_dev`, `esp_hosted`/`esp_wifi_remote`, `esp-sr`), LVGL 9, couche carte maison (ADR-0005). **Version de départ : ESP-IDF 5.5.5**, déjà installée dans `H:\idf` et prouvée (`tools/idf.ps1`, squelette compilé pour `esp32p4` en 38 s, 204 048 octets). Migration vers 6.x décidée plus tard, quand `esp_hosted` et `esp-sr` y seront confirmés.
- **Écartés** : Arduino/PlatformIO + M5Unified/M5GFX (prototype rapide, mais pilotes et gestion du C6 opaques, moins sain pour OTA, tests et deux cartes) ; ESPHome (c'est justement ce qu'on quitte).
- **Pourquoi 5.5.x et pas 6.1** : 6.1 est la dernière (27/08/2026) mais 6.x apporte des ruptures (Picolibc par défaut) et la compatibilité d'`esp_hosted` 3.1.0 et d'`esp-sr` n'est pas confirmée ; le firmware ESPHome actuel de l'auteur tourne sur 5.5.x pour les deux cartes ; xiaozhi-esp32 demande ≥ 5.5.2 ; la CI Waveshare teste 5.5.5 et 6.0.2.
- **Conséquences** :
  - `H:\idf` appartient à ESPHome (fichier `.esphome.stamp.json`, layout différent de l'installeur officiel, `idf-env.json` qui cite encore `E:\idf`). Ne jamais le modifier. S'il change de version ou disparaît : installer un ESP-IDF à part (≈ 2-3 Go, ~15 min, estimation) et changer `$racineIdf` dans `tools/idf.ps1`.
  - `esp_hosted` / `esp_wifi_remote` **épinglés** sur la lignée compatible avec le C6 actuel (2.12.x) : décision d'Axel du 16/10/2026, pas de mise à jour du C6.
  - Exceptions C++ désactivées ; `esp_err_t` aux frontières.
