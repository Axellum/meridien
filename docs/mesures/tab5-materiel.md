# Matériel du Tab5 — luminosité, veille, horloge, audio, alimentation

> **Summary (EN)** — Hardware layer of the Tab5 (ST7121): perceived backlight and sleep, RX8130 clock, ES8388/ES7210 audio, INA226 power, measured on 2026-11-07 with a bench built into the firmware (`-DTAB5_AUTOTEST_MATERIEL=1`, no finger needed, settings restored afterwards).
> - Observed on the board: **40 checks OK, 0 failed** at commit `5f797e3`; sleep timings (dim 2100 ms, off +1900 ms, wake 100 ms), midnight-crossing windows (5 cases), RTC write/read/rollover/leap year, INA226 identity, ES7210 identity, volume curve, microphones alive.
> - **Playback and capture run together on the single I2S port at 16 kHz**: 40 640 frames written = 40 640 read, 0 overflow, slowest block 9–10 ms, tone heard by the microphones (+19.8 dB over the floor). The speaker is on **LINE1**. The ESPHome neighbour's “no duplex” came from its two different rates on the shared clocks.
> - Internal RAM: the audio bus costs **6 224 bytes** while open, returned on close; nothing is taken before the first audio call. The whole batch costs **308 bytes** of free internal RAM at rest compared with the base commit (134 051 vs 134 359; largest block unchanged at 57 344).
> - 10 consecutive software resets on the normal binary: 10 of 10 reach “Carte prête”, no error line, no expander recovery.
> - **Open finding**: the battery connector reads **8.2 V** with the charger forced off — not the “no battery” seen under ESPHome (1.8–1.9 V). To be looked at physically.
> - Not verified: a real finger on the sleep veil, the 2 % backlight floor by eye, the 100 % ceiling on a weak USB port, the RX8130 voltage-loss bit, the headphone path, real speech on the microphones, a power cut with the RTC taking over.

**Vu sur** : Tab5, écran **ST7121**, ESP32-P4 révision v1.3, firmware du commit **`5f797e3`** (`git describe` : `jalon-1a-tab5-st7121-123-g5f797e3`, arbre propre), banc compilé le 07/11/2026 à 01:30:28 (heure lue sur l'appareil), essai du **07/11/2026 vers 01:35** ; binaire normal du même commit pour la série de redémarrages (même jour, 01:40). Aucune adresse MAC n'est publiée.

Règle de lecture : tout ce qui n'est pas marqué « constaté » est une lecture de source ou une décision. Ce qui n'a pas été observé est écrit **non vérifié**. La fiche de la carte, avec les sources de chaque fait, est [`../carte-tab5.md`](../carte-tab5.md).

## Ce qui est construit

| Élément | Où | Vérifié sur le PC |
|---|---|---|
| Courbe de luminosité perçue (loi de puissance, plancher et plafond propres à la carte) | `components/core/include/ecran/luminosite.hpp` | `test_luminosite.cpp` |
| Décision de veille (atténuer, éteindre, plages horaires dont celle qui passe minuit) | `components/core/include/ecran/veille.hpp` | `test_veille.cpp` |
| Horloge matérielle (trame RX8130, plausibilité, politique de reprise et d'écriture) | `components/core/include/ecran/horloge_materielle.hpp` | `test_horloge_materielle.cpp` |
| Courbe de volume perçu → pas du convertisseur | `components/core/include/ecran/volume.hpp` | `test_volume.cpp` |
| Niveaux audio (RMS, crête, écrêtage), Goertzel, sinus de test | `components/core/include/ecran/niveau_audio.hpp` | `test_niveau_audio.cpp` |
| Alimentation (registres de l'INA226, état) | `components/core/include/ecran/alimentation.hpp` | `test_alimentation.cpp` |
| 80 mutations ciblées sur ces six modules | `test/host/mutations_materiel_tab5.py` | 80 détectées sur 80 |
| Interface commune | `components/board_api/include/board.h` (ajouts seulement) | — |
| Couche carte du Tab5 | `components/bsp_tab5/` (`tab5_ecran`, `tab5_rtc`, `tab5_i2s`, `tab5_codec`, `tab5_audio`, `tab5_alim`, `tab5_i2c`, `tab5_io`) | — |
| Banc sans doigt | `components/bsp_tab5/tab5_autotest.cpp`, option CMake `TAB5_AUTOTEST_MATERIEL` | — |

Les tests du PC prouvent la logique, rien du comportement de la carte. Rejoué le 07/11/2026 à `5f797e3` : 71 tests de ces six modules (alimentation 6, horloge 19, luminosité 10, niveaux audio 13, veille 15, volume 8), 0 échec, avec détecteurs (ASan, UBSan) ; ensemble du noyau : 451 tests, 0 échec ; les 80 mutations ciblées : 80 détectées, 0 problème (`python test/host/lancer.py --seulement test_luminosite,test_veille,test_horloge_materielle,test_volume,test_niveau_audio,test_alimentation --mutations`).

## Résultat du banc (constaté, commit `5f797e3`)

`[AT-MAT] BILAN : 40 OK, 0 ECHEC`. Le banc attend 8 s après « Carte prête », ne demande aucun doigt et remet les réglages comme trouvés (luminosité 75 % avant et après, heure de la puce rendue à ±2 s).

### Luminosité (rapport cyclique relu dans le LEDC, pas une copie logicielle)

| Niveau perçu | 0 | 1 | 5 | 10 | 25 | 50 | 75 | 100 |
|---|---|---|---|---|---|---|---|---|
| Rapport cyclique (‰) | 0 | 20 | 21 | 23 | 51 | 193 | 497 | 1000 |

La courbe ne décroît jamais. **Défaut de la courbe : les niveaux 1 à 10 donnent 20 à 23 ‰**, donc aucun cran visible dans le bas d'un curseur. Non corrigé : c'est un jugement à l'œil (voir « Décisions »).

### Veille (réglage de l'essai : atténuer à 2 s, éteindre à 4 s)

| Contrôle | Constaté |
|---|---|
| Atténué | après 2100 ms (attendu ≈ 2000), 38 ‰ |
| Éteint | 1900 ms plus tard (attendu ≈ 2000), 0 ‰ |
| Réveil | rapport cyclique rétabli en 100 ms (293 ‰, comme avant) |
| Plage 22:00 → 06:00 tous les jours | 23:30 éteint, 02:00 éteint, 12:00 allumé |
| Plage du vendredi seul | samedi 02:00 éteint, vendredi 02:00 allumé (la plage appartient au jour où elle commence) |
| Toucher dans une plage | rallumé en 100 ms puis réteint 3000 ms plus tard (attendu ≈ 3000) |

**Non vérifié** : un vrai doigt sur le voile noir de la couche système (le banc appelle la fonction de réveil) ; la consommation de la dalle éteinte.

### Horloge RX8130CE

| Contrôle | Constaté |
|---|---|
| Présence | répond en 0x32 |
| Écriture puis relecture | 1 800 000 000 s écrit, relu identique |
| Avance | +2 s pour 2,1 s écoulées |
| Passage d'année | 31/12/2027 23:59:58 + 3,2 s → 1 830 297 601 (attendu ≈ 1 830 297 601) |
| Année bissextile | 28/02/2028 23:59:58 + 3,2 s → 1 835 395 201 (le 29 février existe) |
| Restitution | heure d'origine + 9 s écoulées → relu 1 794 011 833 (écart dans ±2 s) |
| Registre des drapeaux | 0x21, perte d'alimentation : non |
| Heure de la puce au démarrage | 2026-11-07 00:36 UTC, plausible |

Indirect : à chaque démarrage de la série le système avait **déjà** une heure plausible 2,2 s après le démarrage, sans réseau (le Wi-Fi n'est pas encore là) : `time()` survit donc à `esp_rom_software_reset_system()` et à un reset de la puce par l'USB, et la puce n'est alors pas relue, comme prévu. **Non vérifié** : le bit « perte de tension » (VLF) du RX8130 (pris pour le bit 1 du registre 0x1D d'après le pilote Linux) ; une coupure franche du courant sans réseau où la puce reprend la main.

### Alimentation (INA226 en 0x41)

| Contrôle | Constaté |
|---|---|
| Identité | fabricant 0x5449, puce 0x2260 (conformes) |
| Configuration | trouvée 0x4527, étalonnage trouvé 0x1000 (4096) ; réécrite avec LSB 250 µA |
| Lecture | **8206 mV, 2 mA**, état « au repos », `CHG_STAT` 0, chargeur coupé |
| Chargeur tel que trouvé | `CHG_STAT` = 0, `CHG_EN` = 0, `nCHG_QC_EN` = 1 (chargeur et charge rapide déjà arrêtés) |

**Constat à examiner** : la prémisse « l'exemplaire n'a pas de batterie » vient du firmware ESPHome voisin (1,83–1,94 V chargeur coupé). Ici, chargeur coupé, le connecteur de batterie lit **8,2 V et 2 mA**, stable pendant tout l'essai. C'est ce que montrerait un bloc lithium à deux éléments presque plein au repos ; ce n'est pas ce que montrait un connecteur vide. Soit une batterie est branchée depuis, soit autre chose tient le connecteur à 8,2 V. **À regarder physiquement** (dos ouvert). Le firmware n'allume jamais le chargeur, donc rien ne risque d'être chargé par lui.

### Audio

| Contrôle | Constaté |
|---|---|
| Ouverture | ESP_OK, codecs prêts, 16 000 Hz |
| RAM interne prise par le bus ouvert | **6 224 o** (122 159 → 115 759 o libres), rendus à la fermeture (6 184 o ; les 40 o restants sont les descripteurs I2C) ; plus gros bloc interne inchangé (53 248 o) |
| Amplificateur au repos | éteint ; recoupé après un bip de 400 ms |
| ES7210 | registres d'identité 0x3D = 0x72, 0x3E = 0x10 |
| Courbe de volume (dixièmes de dB) | niveau 0 = −960 (muet), 1 = −560, 30 = −310, 100 = −60 (plafond de la carte −6 dB) |
| Bruit des microphones | −66,0 dBFS (gauche) et −65,8 dBFS (droite) ; aucun n'est à zéro |
| Enregistrement en PSRAM | 16 000 trames sur 16 000 ; rms −65,9 / −65,4 dBFS, crête −55,2 / −53,9 dBFS, 0 écrêtage |

**Lecture et enregistrement en même temps** (bip de 1 kHz à −20 dBFS, niveau de volume 25, 2,5 s, enregistrement du micro pendant ce temps) :

| Sortie du DAC | Trames écrites / lues | Dépassements (émission / réception) | Bloc le plus lent | Ton entendu par les micros |
|---|---|---|---|---|
| LINE1 + LINE2 | 40 640 / 40 640 | 0 / 0 | 9 ms | −47,6 dBFS pour un plancher de −65,8 : **+19,8 dB** |
| LINE1 | 40 640 / 40 640 | 0 / 0 | 10 ms | −47,6 dBFS : **+19,8 dB** |
| LINE2 | 40 640 / 40 640 | 0 / 0 | 10 ms | −90,3 dBFS : **rien** (−24,5 dB sous le plancher) |

Verdict, constaté : **la lecture et l'enregistrement tournent ensemble sur ce port unique à 16 kHz, sans trou, et le bip est entendu par les micros pendant que le flux d'entrée tourne. Le haut-parleur est sur LINE1.** Le « pas de duplex » du firmware ESPHome venait de ses deux cadences différentes (48 kHz en sortie, 16 kHz en entrée) sur des horloges communes, pas du matériel. Limites : une seule passe de 2,5 s par ligne, pas d'essai de longue durée, pas de suppression d'écho (le haut-parleur est entendu par les micros, c'est le but de la mesure), pas d'essai du casque.

Deux pièges payés pendant l'essai (détail dans [`../depannage.md`](../depannage.md)) : la réception seule du port I2S expire (corrigé : la réception active aussi l'émission, sans effet audible) ; le registre d'état de l'expandeur ne montre pas une broche pilotée en sortie (`SPEAKER_EN` lu à 0 alors que le haut-parleur sonnait : le contrôle lit maintenant les registres direction / sortie / haute impédance, relevés pendant le son : 0x6F / 0x22 / 0xDD, donc bit 1 en sortie, haut, hors haute impédance).

### Démarrage et redémarrages (binaire normal du commit `5f797e3`)

Série de **10 redémarrages logiciels consécutifs** (commande série `redemarrer`, qui termine par `esp_rom_software_reset_system()`), démarrages n° 182 à 191 : **10 sur 10 arrivent à « Carte prête »** en 5,5 s environ chacun, LVGL prêt à 1616–1617 ms, cause de redémarrage `rst:0x3 (SW_SYS_RESET)` à chaque fois, **0 ligne d'erreur, 0 ligne de reprise de l'expandeur 0x43**. L'écran principal est chargé à chaque démarrage (ligne « écran principal … chargé » du journal ; **l'image sur l'écran n'a pas été regardée par un humain**).

## Mémoire interne au repos (commande série `memoire`, binaire normal)

Même carte, même lecture 7 s après « Carte prête », deux binaires compilés avec la même configuration : le commit de base (`4e7d8af`, avant ce lot) et le commit `5f797e3`. Chaque binaire a été lu deux fois avec les mêmes valeurs.

| | Base `4e7d8af` | `5f797e3` | Écart |
|---|---|---|---|
| Libre à la fin du démarrage (ligne « mémoire interne libre ») | 255 283 → 249 751 o | 254 731 → 249 375 o | −552 → −376 o |
| `memoire` : interne libre | 134 359 o | 134 051 o | **−308 o** (−0,23 %) |
| `memoire` : plus gros bloc interne | 57 344 o | 57 344 o | 0 |
| `memoire` : minimum depuis le démarrage | 97 248 o | 96 952 o | −296 o |

Lecture : le lot coûte environ **300 o** de RAM interne au repos, sans fragmenter (plus gros bloc inchangé). La répartition n'a pas été mesurée poste par poste (candidats : poignées I2C, verrous, minuteur de l'horloge, voile de la veille). Ce n'est pas nul : la consigne « ne pas l'aggraver » est tenue à 0,2 % près, pas à zéro. Le bus audio n'ajoute ses 6 224 o que le temps où il est ouvert (voir plus haut). Une seule mesure par binaire, sans variation d'un démarrage à l'autre (mêmes valeurs).

## Constaté et non vérifié

| Constaté (carte et commit ci-dessus) | Non vérifié |
|---|---|
| 40 contrôles du banc | doigt réel sur le voile de la veille |
| Duplex 16 kHz sans trou, haut-parleur sur LINE1 | longue durée du duplex ; casque (LINE2) ; parole réelle |
| 10 redémarrages logiciels sur 10 | série après coupure franche du courant |
| RTC : écriture, relecture, année, bissextile, restitution | bit VLF ; reprise de l'heure par la puce après coupure sans réseau |
| INA226 présent et conforme, 8,2 V / 2 mA chargeur coupé | présence ou non d'une batterie ; courant de charge réel |
| RAM interne prise par l'audio : 6 224 o ; lot entier : −308 o au repos par rapport à la base | consommation de la dalle éteinte ; plancher de 2 % à l'œil ; plafond de 100 % sur un port USB faible |
| Carte SD et capteur de mouvement : **non commencés** | tout |

## Décisions prises à la place d'Axel (à valider)

1. **Rétroéclairage** : plancher 2 % et plafond 100 % ; niveaux 1 à 10 quasi identiques (20–23 ‰) ; une loi exponentielle étalerait le bas de l'échelle. À juger à l'œil. Le plafond à 100 % a déjà fait redémarrer un Tab5 sur un port USB faible sous ESPHome.
2. **Volume** : 100 = 0 dB avec un plafond à −6 dB pour le Tab5 ; les niveaux 93 à 100 donnent la même atténuation (limite de la courbe, testée). Bips d'essai audibles pendant le banc de nuit (trois bips de 2,5 s au niveau 25).
3. **Veille** : réglages par défaut et plages horaires (4 au plus) définis dans la couche pure ; la dalle n'est pas mise en veille par commande DCS.
4. **Horloge** : la puce n'est écrite que depuis l'heure du réseau (au plus toutes les 6 h ou si écart > 2 s) ; le drapeau de perte d'alimentation est lu avant d'être effacé (différence avec ESPHome).
5. **Audio** : pilotes ES8388 / ES7210 minimaux écrits ici (pas `esp_codec_dev`, pour ne pas changer `dependencies.lock`) ; cadence unique de 16 kHz sur le port ; la réception active aussi l'émission.
6. **Alimentation** : `CHG_EN` forcé bas au démarrage et charge rapide arrêtée (comme ESPHome), état trouvé journalisé d'abord ; l'API s'appelle `board_power_get` (noms de l'interface commune en anglais, comme le reste de `board.h`).

## Reproduire

```powershell
# Banc (écran éteint ~15 s, bips) : compiler, flasher l'application seule, lire le journal
.\tools\idf.ps1 "-DTAB5_AUTOTEST_MATERIEL=1" build
# ... flasher build\meridien.bin à 0x10000, puis lire le port série (115200) jusqu'à la ligne « [AT-MAT] FIN »
# Revenir au binaire normal
.\tools\idf.ps1 "-DTAB5_AUTOTEST_MATERIEL=0" build

# Logique pure sur le PC
. <dossier des outils hôte>\env_hote.ps1 -Sanitizers
python test/host/lancer.py
python test/host/mutations_materiel_tab5.py
```
