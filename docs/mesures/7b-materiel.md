# Waveshare 7B — couche carte : ce qui est constaté, ce qui ne l'est pas

> **Summary (EN)** — Board layer of the Waveshare ESP32-P4 7B, session of 2026-11-07, branch `claude/materiel-7b-b8f38e`. **Observed on the PC only**: the 7B firmware builds (ESP-IDF 5.5.5, `meridien.bin` 1,837,760 bytes, 56 % of the 4 MB application slot free); 414 core tests run with AddressSanitizer + UBSan, of which 34 are the 7B's; 33 mutations of the 7B source out of 33 detected; the physical-trial script passes its 52-check self-test (the whole trial, restoration included, replayed against a simulated flash; 11 mutations of the script out of 11 detected) and its otadata decoder agrees with the real otadata of an older backup of this unit. **Nothing was observed on the board**: the 7B was not plugged into the PC during the session, so the flash backup, the test flash, the screen/touch/backlight/audio/Wi-Fi tests, the ten resets and the restoration were **not done**. Every hardware value in the code is a reading of a source (see [carte-7b.md](../carte-7b.md)), not a measurement. The 7B column of the [compatibility table](../compatibilite.md) says “Written, not tested on a board” accordingly.

> **Update, 2026-11-07 (noon) — first physical trial done** (four attempts, the last one complete). Meridien boots on the 7B and reaches every milestone in the serial log; ten resets out of ten succeed; ESPHome was restored and proved identical. The trial found and fixed one real bug (an `abort()` in the DSI clock setup, 11 boots out of 11) and one limit of the flash backup over USB-Serial/JTAG (a 256-byte block that never reads). **Not looked at**: the picture on the panel, a real finger on the touch panel, backlight levels, tone and microphone. Sections 1 to 4 below describe the state **before** the trial; the results are in section 5 and the story in section 6.

Piste matériel, branche `claude/materiel-7b-b8f38e`, commits `52f6372` (couche carte), `fc96dd8` (tests), `c48c418` (script d'essai). Carte visée : Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, l'écran en service chez l'auteur (firmware ESPHome).

Règle de lecture : tout ce qui est écrit sans la mention « constaté » est une décision ou une lecture de source, pas une mesure. Ce qui n'a pas été observé est écrit **non vérifié** ou **non fait**.

## 1. Constaté — sur le PC, sans carte

**Vu sur** : le PC d'Axel (Windows 11), ESP-IDF 5.5.5, clang 23.1.5 x86-64 (tests), esptool 5.3.1, Python 3.13, commits `52f6372`, `fc96dd8` et `c48c418`, 07/11/2026. **Aucune carte.**

| Quoi | Résultat constaté | Limite |
| --- | --- | --- |
| Compilation `BOARD=waveshare-7b` | réussie (arbre du commit `c48c418`, 42 s en incrémental) ; `meridien.bin` = 0x1c06c0 octets (1 837 760), il reste 0x23f940 octets (56 %) dans l'emplacement d'application de 4 Mo ; bootloader 0x5d10 octets, 0x2f0 (3 %) de marge ; aucun avertissement venant de `ws7b` ni de `bsp_waveshare_7b` | construit **sans identifiants Wi-Fi** (`reglages_locaux.cmake` absent : le binaire le dit au démarrage et ne se connecte pas) ; compiler ne prouve rien sur la carte |
| Tab5 inchangé | `board.h` n'est pas modifiée ; compilation du Tab5 (`-B build_tab5 -DBOARD=tab5`, `sdkconfig.tab5` à part) réussie sur l'arbre du commit `093ac94` : `meridien.bin` 0x1c2710 octets (56 % de l'emplacement libre), bootloader 0x5a30 octets (6 % de marge, la valeur déjà notée pour le Tab5) ; le `dependencies.lock` de la racine n'a pas bougé (`git status` propre après la compilation) | compilation seulement : aucun flash, le Tab5 n'a pas été touché |
| Tests du noyau | 414 tests exécutés avec AddressSanitizer + UBSan, 0 échec (crochet de commit de `fc96dd8`), dont 34 pour la 7B (`test/host/test_carte_7b.cpp`) : broches, minutages DPI et cadence (≈ 60 Hz), table d'initialisation, rotation et point tactile, rapport cyclique, plan d'extinction/réveil de la dalle, courbe de volume plafonnée à 0 dB, tampons | prouvent le calcul et les tables, pas le comportement de la carte |
| Mutations | 33 mutations du source de la 7B, 33 détectées | idem |
| Contrôles croisés | tables de partitions dans la flash et sans chevauchement, broches SDIO/C6 d'accord entre `sdkconfig.defaults.board` et `ws7b_pur.h`, aucune MAC / IP privée / chemin du PC dans les fichiers de la carte | idem |
| Script d'essai | `python tools/essai_7b.py autotest` : 52 contrôles sur 52 (décodage de l'otadata et de la table, découpe des lectures, empreintes, plages d'écriture, jalons du journal, garde-fous, et l'essai entier rejoué contre une **carte simulée** : secteur illisible avec le stub, flash qui change entre deux lectures, restauration qui échoue ou qui altère un octet, mauvaise MAC, heure limite passée) ; `python test/host/mutations_essai_7b.py` : 11 mutations du script sur 11 détectées | une flash **simulée** n'est pas la carte : la partie qui parle à la vraie 7B n'a **pas** été exécutée |
| Décodeur d'otadata | confronté à l'otadata **réelle** d'une ancienne sauvegarde de cet exemplaire (20/10/2026, fichier d'en-tête de 64 Ko) : séquences 5 et 4, CRC valides toutes les deux, emplacement 0 choisi ; table lue : `app0` 0x10000 + 0xfc0000, `app1` 0xfd0000 + 0xfc0000, `nvs` 0x1f90000 + 0x70000 | lecture d'un **fichier**, pas de la carte ; l'état actuel de la flash est inconnu tant qu'une nouvelle sauvegarde n'est pas lue |

## 2. Non constaté — tout ce qui touche la carte (état avant l'essai du 07/11/2026 à midi)

La 7B **n'était pas branchée** au PC pendant la session. Les trois ports série présents appartenaient à d'autres cartes (le Tab5 tenu par une autre session, une carte inconnue, un troisième port) : aucun n'a été ouvert ni touché, et aucun n'a été pris pour la 7B (la MAC se lit sur la puce, jamais ailleurs). Donc :

| Étape prévue par la mission | État |
| --- | --- |
| (a) sauvegarde de la flash entière (32 Mo), deux lectures de même SHA-256, `verify-flash` ; état de la 7B dans Home Assistant avant | **non fait** |
| (b) écriture du binaire d'essai de Meridien (0x2000, 0x8000, 0x9000, 0x10000) | **non fait** |
| (c) image à l'écran ; tactile sur 5 points (décalage moyen) ; rétroéclairage ; tonalité ≤ 75 % ; lien C6, Wi-Fi, adresse IP, SNTP ; dix remises à zéro de suite | **non fait — non vérifié** |
| (d) restauration de la sauvegarde, `verify-flash`, relecture, démarrage de la 7B, entités de nouveau disponibles dans Home Assistant | **sans objet : rien n'a été écrit, la 7B n'a pas été touchée** |

**État de la 7B dans Home Assistant (lecture seule, 07/11/2026 vers 00 h 55, rien n'a été modifié dans Home Assistant).** Les 16 entités de son satellite Assist sont soit `unavailable` (haut-parleur, microphone, écoute, mot d'activation, vue), soit `unknown` (luminosité, veille, rotation, connexion), soit sans signification de présence (chiffrement `on`, assistant `preferred`). Les trois dont la date a été lue ont changé pour la dernière fois le 06/11/2026 à 12 h 50 (heure de Paris) : **rien n'indique que la 7B soit en ligne depuis ce moment**. Cause (carte éteinte, débranchée, hors Wi-Fi, ou redémarrage de Home Assistant sans reconnexion) : **non vérifiée**. Ce n'est donc pas un état « avant » comparable à celui d'un écran en service : si la restauration est faite plus tard, la preuve sera « les entités redeviennent disponibles et l'uptime repart », et il faudra d'abord savoir si elles l'étaient avant.

Conséquence : le firmware ESPHome de la 7B n'a pas été modifié ; ce qui est en service chez l'auteur est exactement ce qui y était avant la session.

Valeurs lues dans les sources et jamais vues sur la carte (liste complète et sources : [carte-7b.md](../carte-7b.md)) : broches, adresses I2C, minutages DPI et débit de ligne (900 Mbit/s, le BSP de Waveshare dit 1000), table d'initialisation de l'EK79007, reset du GT911 par GPIO23, sens des miroirs du tactile, polarité du rétroéclairage, recette d'extinction réelle de la dalle, courbe de volume, broches SDIO et polarité du reset du C6, révision de la puce (v3.2 lue le 20/10/2026), taille de flash (32 Mo).

## 3. Pour rejouer l'essai (quand la 7B est branchée)

```powershell
$env:MERIDIEN_MAC_7B = "<MAC de la 7B, lue sur la puce>"      # jamais écrite dans le dépôt
.\tools\preparer_wifi.ps1 -FichierEnv <chemin du .env>         # identifiants Wi-Fi dans un fichier ignoré
.\tools\idf.ps1 "-DWS7B_AUTOTEST_TACTILE=1" build              # binaire d'essai (cinq appuis simulés)
python tools\essai_7b.py essai --port COMx --dossier <dossier de sauvegarde> --build build --limite 06:30
```

Avant : prendre le verrou de la 7B et celui du port (script de verrous propre au PC d'Axel, hors dépôt) ; lire l'état de la 7B dans Home Assistant **en lecture seule**. Après : la commande restaure d'elle-même ; vérifier ensuite dans Home Assistant que les entités de la 7B sont de nouveau disponibles et que son temps de fonctionnement a redémarré. Le tactile réel demande un **doigt** : le journal de la 7B écrit une ligne « brut → logique » par appui, à relever à la main.

## 4. Décisions prises à la place d'Axel (à valider)

1. **Rotation par défaut = 0** (montage de la 7B d'Axel, à l'envers par rapport aux démos Waveshare) ; 180 par `-DWS7B_ROTATION=180`.
2. **Débit de ligne DSI = 900 Mbit/s** (composant d'Espressif et firmware ESPHome en service), pas les 1000 du BSP de Waveshare.
3. **Reset du tactile par GPIO23** (schéma et ESPHome) ; le BSP de Waveshare dit le contraire.
4. **Un tactile absent n'arrête pas le démarrage** : l'écran reste utilisable, le journal le dit.
5. **`board_set_backlight_percent(0)` éteint aussi la dalle** (voile, 0x28, 0x10) : le rétroéclairage analogique ne devient jamais noir. Désactivable (`-DWS7B_REAL_PANEL_OFF=0`).
6. **Micrologiciel du C6 jamais mis à jour ni interrogé** (version non demandée).
7. **Flash 32 Mo, DIO 80 MHz, PSRAM 200 MHz, CPU 400 MHz** (la mission supposait 16 Mo).
8. **Tampon de dessin de 25 lignes** (100 Kio en RAM interne) au lieu des 50 lignes du Tab5.

## 5. Résultats du premier essai physique (07/11/2026, midi)

**Vu sur** : Waveshare ESP32-P4 7B (l'exemplaire de l'auteur, MAC non écrite ici), dalle EK79007 1024 × 600, `main` = `9c89ad5` + la correction `eac14d2`, binaire d'essai SHA-256 `CF28C4F0…` (2 815 744 octets, 33 % de l'emplacement libre), cinq appuis tactiles simulés, Wi-Fi compilé ; journal série par l'USB natif (USB-Serial/JTAG), 07/11/2026 de 12 h 28 à 12 h 33 (heure de Paris). « Journal » = ce que le firmware écrit, **pas** ce qu'un œil a vu.

| Mesure | Valeur | Preuve |
| --- | --- | --- |
| Révision de la puce, taille de flash lues | ESP32-P4 révision v3.2, flash 32 Mo (`flash-id`), PSRAM 32 Mo à 200 MHz, CPU 400 MHz | esptool 5.3.1 et journal de démarrage |
| Table de partitions de la sauvegarde (NVS d'ESPHome) | `otadata` 0x9000 (+0x2000), `phy_init` 0xb000, `app0` 0x10000 (+0xfc0000), `app1` 0xfd0000 (+0xfc0000), `nvs` 0x1f90000 (+0x70000) ; otadata : séquences 3 (emplacement 0) et 2, CRC valides, **`app0` en service** | lue sur la carte, décodée par `tools/essai_7b.py` |
| Contrôleur d'écran, image affichée, sens de l'image | EK79007 **annoncé, non détecté** ; DSI 2 voies à 900 Mbit/s, pixels à 52 MHz, 60,4 Hz, 1024 × 600 RGB565 ; rétroéclairage à 75 % et « première image affichée » à 685 ms (journal). **Image non regardée à l'œil**, sens non vu (rotation 0) | journal |
| GT911 : adresse, identifiant produit, décalage moyen sur 5 points | adresse 0x5d, identifiant « 911 » (0x39 0x31 0x31), version de configuration 91 ; **4 appuis simulés sur 5 vus dans le journal** (la capture s'est arrêtée avant le cinquième), 4 « OK », décalage 0 aux quatre coins. Les appuis sont **injectés** (coordonnées brutes) : le chemin lecture du GT911 → coordonnées n'est pas éprouvé, **aucun doigt** | journal |
| Rétroéclairage 0 / 50 / 100 %, dalle vraiment noire à 0 % | **non essayé** | — |
| Tonalité d'essai (≤ 75 %), niveau du microphone | **non essayés** ; ES8311 et ES7210 répondent sur le bus I2C | journal |
| Lien C6, association Wi-Fi, adresse IP, horloge (temps depuis le démarrage) | lien C6 à 2 422 ms (esp_hosted côté P4 2.12.12, micrologiciel du C6 non demandé : l'avertissement « Co-proc [0.0.0] » est attendu) ; association à 7 329 ms (canal 1) ; adresse IP à 10 389 ms ; horloge réglée +53 ms après le lancement de SNTP ; RSSI −31 dBm ; 0 coupure, 0 échec de transport | journal |
| Mémoire interne libre | 316 Ko une fois l'heure reçue (minimum depuis le démarrage 309 Ko) | journal (`jalon_mem`) |
| Dix remises à zéro de suite (réussies / 10) | **10 / 10**, chacune jusqu'à « Carte prête » sans `abort`. Ce sont des remises à zéro par RTS (USB natif), **pas une coupure de courant** | journal de chacune |
| Restauration : `verify-flash`, SHA-256 de la relecture, démarrage, Home Assistant | écriture de 0x0 à 0x471000 (4 657 152 octets), « Hash of data verified » puis `verify-flash` de la puce identique (SHA-256 de l'image `c084c951…`). **Pas de relecture** (impossible, voir §6). ESPHome revenu : même MAC, version `3.9.0-main-9bb801e` et même heure de compilation qu'avant, 74 entités toutes avec un état, aucun réglage changé, aucun second redémarrage en 60 s ; Home Assistant revoit « Sens de l'écran » = « Upside down » | `tools/prouver_flash.py --apres`, Home Assistant |
| Journal : lignes d'erreur restantes | `lcd_panel: esp_lcd_panel_swap_xy: swap_xy is not supported by this panel` (une fois, rotation 0 : sans effet vu) ; `system_api: 0 mac type is incorrect (not found)` (une fois, 12 s après le démarrage : **cause non cherchée**) | journal |

## 6. Le récit de l'essai (quatre tentatives)

1. **11 h 59 — sauvegarde refusée, rien d'écrit.** La copie complète de la flash est **impossible** par l'USB-Serial/JTAG : les 256 octets de `0xdcf00` ne se lisent jamais (`Packet content transfer stopped`, avec et sans stub), alors que 0x100 octets se lisent juste à côté (bissecté à la main). Le script a refusé d'écrire, comme prévu. Ce bloc est dans `app0`, et `app0` est prouvé identique à `firmware.ota.bin` du firmware en service par `verify-flash` (la puce calcule le MD5, « digest matched »).
2. **Correction du script : sauvegarde « à portée limitée »** (`--reference`). Gardés et lus deux fois : `0x0`–`0x10000` (amorce, table, otadata, phy_init) et la NVS d'ESPHome (448 Ko, par précaution) ; l'application en service est remplacée par le fichier de référence, et la **puce** prouve que « début lu + fichier » est identique à la flash sur 0x0–0x471000. Les « restes » (octets de `app0` après la fin de l'image) ne sont ni gardés ni restaurés : aucun firmware ne les lit, et Meridien y écrit sa NVS (0x810000). Avant d'écrire, le script refuse tout essai qui entamerait une autre partition de l'ancien firmware. Autotest : 9 vérifications de plus.
3. **12 h 11 — écriture réussie, essai refusé à tort.** `otadata` (0x9000) avait changé après le redémarrage : le nouveau firmware y inscrit son état. Le script le prenait pour un échec d'écriture (esptool l'avait pourtant vérifié) et a restauré : restauration prouvée, ESPHome revenu. Le contrôle d'`otadata` après redémarrage est retiré.
4. **12 h 15 — Meridien plante 11 fois sur 11.** `abort()` dans `mipi_dsi_ll_set_phy_pllref_clock_source`, appelée par `esp_lcd_new_dsi_bus`, juste après « après la NVS et les réglages ». Cause : `ws7b_display.c` demandait la source d'horloge `MIPI_DSI_PHY_CLK_SRC_DEFAULT` (PLL_F20M, « réservée aux puces < v3.0 ») alors que le build de la 7B exige une puce ≥ v3.1. Correction `eac14d2` : source laissée au pilote (`phy_clk_src = 0`). Le Tab5 n'est pas touché (son build force les puces v1.x). Restauration prouvée, ESPHome revenu.
5. **12 h 28 — essai complet réussi** (résultats au §5), ESPHome restauré et prouvé à 12 h 33.

**Ce que ça change pour la suite.** La sauvegarde « complète » du plan n'existe pas pour cette carte par ce port : la preuve est le MD5 de la puce contre un fichier de référence **gardé** (`~/.tab5/elf/<version>/firmware.ota.bin`). Pour un autre firmware en service, il faut son fichier avant de commencer. La prise « USB TO UART » (pont série, **non essayée ici**) lirait peut-être tout ; à essayer si une copie complète devient nécessaire.

**Non vérifié** : l'image sur la dalle (personne n'a regardé), le tactile réel, le rétroéclairage, la dalle noire à 0 %, la tonalité et le microphone, un démarrage après coupure de courant, la longue durée, la cause de l'erreur `mac type is incorrect`, le sens de l'image avec le montage à l'envers de l'exemplaire (rotation compilée 0 : à confirmer à l'œil).
