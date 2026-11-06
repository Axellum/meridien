# Waveshare 7B — couche carte : ce qui est constaté, ce qui ne l'est pas

> **Summary (EN)** — Board layer of the Waveshare ESP32-P4 7B, session of 2026-11-07, branch `claude/materiel-7b-b8f38e`. **Observed on the PC only**: the 7B firmware builds (ESP-IDF 5.5.5, `meridien.bin` 1,837,760 bytes, 56 % of the 4 MB application slot free); 414 core tests run with AddressSanitizer + UBSan, of which 34 are the 7B's; 33 mutations of the 7B source out of 33 detected; the physical-trial script passes its 27-check self-test and its otadata decoder agrees with the real otadata of an older backup of this unit. **Nothing was observed on the board**: the 7B was not plugged into the PC during the session, so the flash backup, the test flash, the screen/touch/backlight/audio/Wi-Fi tests, the ten resets and the restoration were **not done**. Every hardware value in the code is a reading of a source (see [carte-7b.md](../carte-7b.md)), not a measurement. The 7B column of the [compatibility table](../compatibilite.md) says “Written, not tested on a board” accordingly.

Piste matériel, branche `claude/materiel-7b-b8f38e`, commits `52f6372` (couche carte), `fc96dd8` (tests), `c48c418` (script d'essai). Carte visée : Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, l'écran en service chez l'auteur (firmware ESPHome).

Règle de lecture : tout ce qui est écrit sans la mention « constaté » est une décision ou une lecture de source, pas une mesure. Ce qui n'a pas été observé est écrit **non vérifié** ou **non fait**.

## 1. Constaté — sur le PC, sans carte

**Vu sur** : le PC d'Axel (Windows 11), ESP-IDF 5.5.5, clang 23.1.5 x86-64 (tests), esptool 5.3.1, Python 3.13, commits `52f6372`, `fc96dd8` et `c48c418`, 07/11/2026. **Aucune carte.**

| Quoi | Résultat constaté | Limite |
| --- | --- | --- |
| Compilation `BOARD=waveshare-7b` | réussie (arbre du commit `c48c418`, 42 s en incrémental) ; `meridien.bin` = 0x1c06c0 octets (1 837 760), il reste 0x23f940 octets (56 %) dans l'emplacement d'application de 4 Mo ; bootloader 0x5d10 octets, 0x2f0 (3 %) de marge ; aucun avertissement venant de `ws7b` ni de `bsp_waveshare_7b` | construit **sans identifiants Wi-Fi** (`reglages_locaux.cmake` absent : le binaire le dit au démarrage et ne se connecte pas) ; compiler ne prouve rien sur la carte |
| Tab5 inchangé | `board.h` n'est pas modifiée ; le CMake de la racine garde le verrou `dependencies.lock` du Tab5 ; une compilation du Tab5 a réussi dans la session avant les dernières retouches de la couche 7B (qu'elle ne contient pas) | non rejouée sur le commit final (le verrou de compilation était tenu par une autre session) |
| Tests du noyau | 414 tests exécutés avec AddressSanitizer + UBSan, 0 échec (crochet de commit de `fc96dd8`), dont 34 pour la 7B (`test/host/test_carte_7b.cpp`) : broches, minutages DPI et cadence (≈ 60 Hz), table d'initialisation, rotation et point tactile, rapport cyclique, plan d'extinction/réveil de la dalle, courbe de volume plafonnée à 0 dB, tampons | prouvent le calcul et les tables, pas le comportement de la carte |
| Mutations | 33 mutations du source de la 7B, 33 détectées | idem |
| Contrôles croisés | tables de partitions dans la flash et sans chevauchement, broches SDIO/C6 d'accord entre `sdkconfig.defaults.board` et `ws7b_pur.h`, aucune MAC / IP privée / chemin du PC dans les fichiers de la carte | idem |
| Script d'essai | `python tools/essai_7b.py autotest` : 27 contrôles sur 27 (décodage de l'otadata et de la table, découpe des lectures, empreintes, plages d'écriture, jalons du journal, garde-fous : pas de MAC → refus, pas de sauvegarde confirmée → refus, sauvegarde altérée → refus) | la partie qui parle à la carte n'a **pas** été exécutée |
| Décodeur d'otadata | confronté à l'otadata **réelle** d'une ancienne sauvegarde de cet exemplaire (20/10/2026, fichier d'en-tête de 64 Ko) : séquences 5 et 4, CRC valides toutes les deux, emplacement 0 choisi ; table lue : `app0` 0x10000 + 0xfc0000, `app1` 0xfd0000 + 0xfc0000, `nvs` 0x1f90000 + 0x70000 | lecture d'un **fichier**, pas de la carte ; l'état actuel de la flash est inconnu tant qu'une nouvelle sauvegarde n'est pas lue |

## 2. Non constaté — tout ce qui touche la carte

La 7B **n'était pas branchée** au PC pendant la session. Les trois ports série présents appartenaient à d'autres cartes (le Tab5 tenu par une autre session, une carte inconnue, un troisième port) : aucun n'a été ouvert ni touché, et aucun n'a été pris pour la 7B (la MAC se lit sur la puce, jamais ailleurs). Donc :

| Étape prévue par la mission | État |
| --- | --- |
| (a) sauvegarde de la flash entière (32 Mo), deux lectures de même SHA-256, `verify-flash` ; état de la 7B dans Home Assistant avant | **non fait** |
| (b) écriture du binaire d'essai de Meridien (0x2000, 0x8000, 0x9000, 0x10000) | **non fait** |
| (c) image à l'écran ; tactile sur 5 points (décalage moyen) ; rétroéclairage ; tonalité ≤ 75 % ; lien C6, Wi-Fi, adresse IP, SNTP ; dix remises à zéro de suite | **non fait — non vérifié** |
| (d) restauration de la sauvegarde, `verify-flash`, relecture, démarrage de la 7B, entités de nouveau disponibles dans Home Assistant | **sans objet : rien n'a été écrit, la 7B n'a pas été touchée** |

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

## 5. À remplir au premier essai physique

| Mesure | Valeur | Vu sur (carte, révision d'écran, commit, date) |
| --- | --- | --- |
| Révision de la puce, taille de flash lues | — | — |
| Table de partitions de la sauvegarde (NVS d'ESPHome) | — | — |
| Contrôleur d'écran, image affichée, sens de l'image | — | — |
| GT911 : adresse, identifiant produit, décalage moyen sur 5 points | — | — |
| Rétroéclairage 0 / 50 / 100 %, dalle vraiment noire à 0 % | — | — |
| Tonalité d'essai (≤ 75 %), niveau du microphone | — | — |
| Lien C6, association Wi-Fi, adresse IP, horloge (temps depuis le démarrage) | — | — |
| Dix remises à zéro de suite (réussies / 10) | — | — |
| Restauration : `verify-flash`, SHA-256 de la relecture, démarrage, Home Assistant | — | — |
