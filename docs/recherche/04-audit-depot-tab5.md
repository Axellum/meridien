# Audit du dépôt Tab5 existant (ESPHome) — 06/11/2026

Deux audits en lecture seule du dépôt Tab5 (`Axellum/M5-Tab5-ESPHome-LVGL`, public), pour reprendre ce qui sert et ne pas repayer les pièges. Chemins relatifs à ce dépôt. Les `chemin:ligne` sont ceux rendus par les agents (non revérifiés un par un). Dépôt sous **licence MIT** (`LICENSE:1`) : reprise libre du code.

## A. Matériel : Tab5 contre 7B (à abstraire dans la couche carte)

Sources : `Tab5/paquets/tab5-hardware.yaml` (TH), `Tab5/paquets/carte-waveshare-7b.yaml` (7B), audit 7B n°1 (notes privées d'Axel, hors dépôt, A1), `docs/hardware.md` (HW).

| Fonction | Tab5 | 7B |
|---|---|---|
| SoC | P4 v1.3 d'après HW:48, 360 MHz (TH:13, 52) | P4 v3 (v3.2 sur l'exemplaire d'Axel), 400 MHz |
| Flash / PSRAM | 16 Mo ; 32 Mo hex 200 MHz | 32 Mo (GD25Q256) ; 32 Mo hex 200 MHz |
| Écran | portrait natif 720×1280 (tourné 270° pour 1280×720) ; contrôleur ST7123/ST7121/ILI9881C ; reset par expandeur (0x43 bit 4) | EK79007 7", 1024×600 paysage, 2 voies DSI ; reset GPIO33 |
| PHY DSI | `esp_ldo` canal 3, 2,5 V | idem |
| Rétroéclairage | LEDC GPIO22, 1 kHz PWM | LEDC GPIO32, 5 kHz, inversé (analogique : l'image reste visible à 0 %) |
| Tactile | ST7123/ST7121 (famille Sitronix) ; INT GPIO23 ; reset par expandeur (0x43 bit 5) | GT911 en 0x5D ou 0x14 ; reset GPIO23 ; INT non relié |
| I2C | SDA 31, SCL 32, 400 kHz | SDA 7, SCL 8, 400 kHz |
| Codec sortie | ES8388 (0x10), stéréo 48 kHz | ES8311 (0x18), mono |
| Codec micro | ES7210 (0x40), 16 kHz, 30 dB | ES7210 (0x40), 30 dB, canal gauche |
| I2S | MCLK 30, BCLK 27, WS 29, DIN 28, DOUT 26 | MCLK 13, BCLK 12, WS 10, DIN 11, DOUT 9 |
| Ampli HP | NS4150B, enable = expandeur 0x43 bit 1 | NS4150B, GPIO53 |
| Expandeurs | PI4IOE5V6408 en 0x43 et 0x44 (antenne, ampli, 5 V externe, resets écran/tactile, casque, alim Wi-Fi/USB, charge) | aucun |
| RTC / IMU / jauge | RX8130 (0x32), BMI270 (0x68), INA226 (0x41) | aucun des trois (support de pile ML1220 rechargeable) |
| C6 (SDIO) | CLK 12, CMD 13, D0-D3 = 11, 10, 9, 8 ; reset GPIO15 actif haut ; C6 1.4.1 | CLK 18, CMD 19, D0-D3 = 14-17 ; reset GPIO54 actif haut ; version illisible |
| Batterie | pack 2S d'origine ; charge via expandeur 0x44 | connecteur J4, ETA6098 ; tension GPIO20 (pont ÷3), pas de jauge |
| Divers 7B | — | microSD 4 bits (39-44, alim GPIO45 bas = allumée), BOOT GPIO35, CAN 21/22, RS-485 26/27, 17 GPIO libres |

**À abstraire** : écran (contrôleur, résolution, orientation, reset), rétroéclairage (broche, fréquence, inversion, PWM contre analogique, extinction réelle), tactile (puce, INT, reset, miroirs), audio (codec de sortie, stéréo/mono, ampli, plage de volume : la 7B sature à 100 %, limiter à `volume_max` 75 % / `volume_min` 40 %), expandeurs (absents de la 7B), options RTC/IMU/jauge/chargeur, broches du C6, fréquence CPU et flash, délai de démarrage (1 s sur Tab5 pour l'expandeur, 0 sur la 7B).

**Manque dans le dépôt** : timings MIPI-DSI du Tab5 et séquences d'initialisation (ESPHome les tire de ses modèles `M5STACK-TAB5-ST7123/-ST7121` et `WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B`) → les prendre dans les composants Espressif (`esp_lcd_st7123`, `esp_lcd_st7121`, `esp_lcd_ek79007`) ou le BSP Waveshare.

**Contradictions / incertitudes relevées** : fréquence du Tab5 ST7121 (400 MHz dans les notes privées d'Axel (hors dépôt), 360 MHz forcé par le firmware TH:52-53) ; commentaires périmés dans `carte-waveshare-7b.yaml` (« pas de batterie », reset C6 « à confirmer ») ; la ST7121 « n'a jamais été testée » selon `ecran-st7121.yaml:9-14` alors qu'Axel en a une en service depuis le 21/10 ; reset ES8388 : contournement du registre DACPOWER (0x04 → 0x00) à confirmer avant d'écrire le pilote ; ILI9881C compilée en CI, jamais lancée sur tablette ; tension batterie 7B non testée ; sens RS-485 de la 7B à essayer.

À lire en entier avant d'écrire un pilote : `Tab5/paquets/tab5-hardware.yaml` (ses commentaires portent les pièges), `carte-waveshare-7b.yaml`, l'audit 7B n°1, `Tab5/paquets/tab5-sensors-diagnostics.yaml` (rôle des broches d'expandeur, séquence de charge), `tab5-sensors-domotique.yaml` (ampli et casque), `docs/hardware.md`, `Tab5/ecran/tab5_dalle.cpp` (commandes DCS d'extinction), `Tab5/ecran/tab5_lien_c6.cpp` (reset CORE du lien SDIO), `docs/troubleshooting.md`, `docs/waveshare-7b.md`.

## B. Reprendre / réécrire / oublier

| Élément | Verdict | Détail |
|---|---|---|
| `Tab5/fonts/IconeMeteo.ttf` | **reprendre** | 12 glyphes, ~10 Ko ; icônes composées de deux glyphes superposés colorés par rôle de palette (`Tab5/fonts/icone_meteo/README.md`). **Licence non vérifiée** : dessins du pack « The Weather is Nice Today » de Laura Reen, conditions sur la page du pack → à vérifier avant diffusion |
| `materialdesignicons-webfont.ttf` | reprendre en sous-ensemble | chaque icône doit être dans `glyphs:` sinon rectangle vide ; licence non vérifiée (le `OFL.txt` lu couvre les polices Google) |
| Polices de texte (`Tab5/fonts/*.ttf`, 24 fichiers) | reprendre | SIL OFL 1.1 ; jeu Latin-1 de 224 glyphes (accents, « ° ») défini dans `Tab5/paquets/tab5-styles.yaml:36-50` ; la police des températures n'a que `+-0123456789: °C.%—` |
| Jetons de thème `Tab5/socle/tab5_tokens.h` (struct `Palette`) + `tab5_themes_data.h` | **reprendre tel quel** | 21 thèmes garantis par un `static_assert`, C++ pur, généré par `tools/gen_themes.py` |
| Langues `Tab5/socle/tab5_i18n.h` + `Tab5/lang/*.yaml` + `tools/gen_i18n.py` | **reprendre** | le texte français est la clé (`tr()`, `tr_ctx()`, `tr_fill()`, `tr_noop()`), C++ pur sans allocation, 7 langues (fr, en, de, es, it, nl, tr) ; oublier la partie « ce que lit Home Assistant » (`I18nPourHa`) |
| Logique météo | **réécrire** | HA faisait tout (`HomeAssistant_Config/packages/tab5_meteo_sources.yaml`) ; reprendre seulement les calques de `kMeteoIcons` (`Tab5/ecran/tab5_forecast.cpp:49`) et les libellés français (`Tab5/ecran/tab5_meteo.cpp:78-88`) ; table WMO à écrire |
| Mises en page carte météo/horloge/date | réécrire | géométrie dérivée de l'écran de chaque carte (`Tab5/socle/tab5_geometrie.h`, ADR-0061) ; reprendre seulement `tab5_core.h` (`day_long_utf8`, formats de date, `tab5_heure_valide()`) |
| Logique vocale côté écran | réécrire, garder le principe | états IDLE gris « Prêt », LISTENING vert « Écoute… », THINKING orange « Analyse… » (`Tab5/ecran/tab5_assist.h:33-35`) ; pas d'état « réponse » dédié (non vérifié) |
| `tab5_barre.h`, `tab5_modele_ha.h`, `I18nPourHa` | **oublier** | propres à HA |
| Outils : rendu hors tablette (ADR-0021, `tools/rendu/ecrans.py`), tests C++ du socle (`tools/test_tab5_socle.cpp`, `test_parse.cpp`, libFuzzer), `tools/check_tab5_code_rules.py` | porter l'idée | le rendu dépend d'ESPHome `host` → porter vers LVGL + SDL |
| Préférences de produit (`AGENTS.md` du dépôt Tab5, section « Product preferences ») | reprendre | sauf « réglé depuis Home Assistant » |

## C. Pièges déjà payés, vrais aussi en natif

1. **Écran noir après un redémarrage logiciel (Tab5)** : le reset de la dalle passe par l'expandeur PI4IOE5V6408, pas prêt tout de suite → attendre 1 s avant le reset d'écran (0 sur la 7B). `docs/troubleshooting.md:13-19`, ADR-0005.
2. **Écran noir et réseau absent après un OTA** : le lien SDIO du C6 ne revient pas ; journaux « ESP-Hosted link not yet up » ~50 fois par seconde qui affament LVGL ; cause exacte inconnue (un seul cas documenté, 05/08) ; parade : couper l'alimentation. `troubleshooting.md:114-134`. Le P4 réinitialise déjà le C6 à chaque boot (`CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP=y`, GPIO15, 1 500 ms).
3. **7B : boucle de démarrage** après un redémarrage logiciel (`esp_restart()` ne remet à zéro que les cœurs du P4) → finir par `esp_rom_software_reset_system()`. `troubleshooting.md:140-158`.
4. **Redémarrages tout seuls toutes les 15-20 min** dans ESPHome sans client API : en natif, ne jamais redémarrer pour une simple absence de serveur — le produit tourne hors ligne.
5. Une réinitialisation « chien de garde / bouton » sans rapport de plantage est un **démarrage normal** (bouton d'alimentation), pas un crash. `troubleshooting.md:170-176`.
6. **Pas d'alpha PNG sur la dalle MIPI-DSI** : plantage constaté sur P4 → fonds opaques précuits, icônes en police vectorielle (ADR-0004 du dépôt Tab5).
7. **Bus I2S partagé** : dans ESPHome, micro et haut-parleur ne peuvent jamais tourner ensemble (mutex unique). En natif, duplex à prouver (voir `02-vocal-et-llm.md`).
8. **Extinction d'écran (7B)** : rétroéclairage analogique jamais à zéro → voile noir LVGL + DCS 0x28 + « sleep in » 0x10 ; au réveil 0x11, attente 120 ms, 0x29. Les dalles du Tab5 ne se mettent pas en veille (ADR-0064).
9. **7B** : ES8311 sature à 100 % (volume max 75 %) ; ~30 « lcd.dsi underrun » par seconde sans PSRAM 200 MHz ; tactile : rotation 180° et reset du GT911 sur GPIO23.
10. **Rotation logicielle** : 82-83,5 ms par écran entier à 180° contre 68,7-70,2 ms à 0° (ADR-0012) → orientation native de préférence.
11. **Caractères hors Latin-1** : rectangle vide ; normaliser les espaces Unicode insécables.
12. **Fuseau** conservé en NVS (ADR-0020).
13. **Brownouts du Tab5** : alimenté par un port USB du PC (courant inconnu), la doc exige 5 V / 2 A ; sans batterie la tablette redémarre au retour de l'USB. À tenir en compte pour les essais : chargeur correct.
14. Ouvrir/fermer le port série USB peut réinitialiser la puce.

## D. Ce que le dépôt n'avait pas (ESPHome ou HA le faisaient) et qu'il faut écrire

Provisioning Wi-Fi · page de réglages · stockage chiffré des secrets · SNTP + fuseau + heure d'été · OTA (partitions, validation, retour arrière ; la clé de signature RSA-3072 d'ESPHome n'existe pas ici) · journal consultable depuis la page · client HTTPS vers le STT/LLM/TTS (certificats, délais, reprise) · météo directe (appel, table WMO, cache, repli hors ligne).
