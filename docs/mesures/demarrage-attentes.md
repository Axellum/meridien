# Démarrage : revue des attentes fixes et recouvrement (12/11/2026)

> **Summary (EN)** — Review of every fixed wait between the start of the application and the first light, on the Tab5 and the Waveshare 7B. None was shortened: two were made to **overlap** other work, with the same minimum delay. Tab5: the 120 ms the panel needs after its hardware reset now run while the clock, the power monitor, the controller detection and the DSI bus are set up. 7B: the 60 ms settle time of the GT911 touch reset now runs during the panel set-up. The 1 s wait for the Tab5 I/O expander stays: no readable state replaces it. A new `DEMARRAGE ms` log line gives every step up to the light. **Not measured on a board**: expected gain up to ~120 ms on the Tab5 and ~70 ms on the 7B, to be read in that log line.

Règle de lecture : « source » = d'où vient la valeur (lu, pas de mémoire) ; tout gain écrit ici est **estimé**, rien n'a été mesuré sur une carte.

## 1. Attentes du chemin jusqu'à la première lumière

| Attente | Valeur | Source | Décision |
| --- | --- | --- | --- |
| Tab5 : avant l'expandeur 0x43 (`EXPANDER_SETTLE_MS`, `tab5_board.c`) | 1 000 ms | ADR-0005 et `troubleshooting.md` du dépôt Tab5 : écran noir après un redémarrage logiciel sans elle, preuve par élimination (5 essais, écran regardé) ; « un sondage n'a jamais été validé » | **gardée telle quelle**. Aucun état ne la remplace : l'expandeur répond déjà sur l'I2C quand son reset reste sans effet sur la dalle. La réduire demande la re-mesure décrite par l'ADR |
| Tab5 : reset de la dalle, niveau bas (`LCD_RESET_LOW_MS`) | 10 ms | espp 10 ms ; composant `esp_lcd_st7121` (reset par broche) 10 ms ; ESPHome 5 ms | gardée (5 ms à gagner au mieux, contre une valeur de deux sources) |
| Tab5 : après le relâchement du reset de la dalle (`LCD_RESET_RECOVERY_MS`) | 120 ms | composant `esp_lcd_st7121` (`panel_st7121_reset`) : 120 ms entre le reset matériel et la première commande ; dans ce composant le bus DSI est créé **avant** le reset | **recouverte** : le délai part du relâchement ; l'horloge (RX8130), le moniteur d'alimentation, la détection du contrôleur et la création du bus DSI, de l'E/S DBI et du tampon d'image se font pendant ce temps ; `tab5_lcd_wait_reset_recovery()` n'attend que le reste, juste avant la première commande (SWRESET). Le journal dit combien |
| Tab5 : `SWRESET` puis 120 ms, commandes d'initialisation (+5 ms chacune), `SLPOUT` 120 ms | code du composant | composant `esp_lcd_st7121` 1.0.1 | non touchées (code tiers ; le `SWRESET` est aussi un filet si le reset matériel n'a pas pris) |
| Tab5 : détection du contrôleur | 10 ms × 150 au plus | `tab5_detect.c` | déjà une attente d'état (le tactile répond, ou 1,5 s) : gardée |
| Tab5 : second reset du tactile (20 + 50 ms) | seulement si la détection échoue | espp | hors chemin normal : gardée |
| Tab5 : essais de l'expandeur (150 ms) et redémarrage de reprise (1 s) | seulement en cas d'échec | `tab5_io.c` (incident du démarrage n° 65, `1a.md`) | hors chemin normal : gardées |
| 7B : reset du GT911 (`RESET_LOW_MS` / `RESET_SETTLE_MS`, `ws7b_touch.c`) | 10 + 60 ms | pilote Espressif (10 + 10 ms) rallongé par prudence | **recouverte** : l'impulsion est lancée avant la dalle (`ws7b_touch_reset_start`), les 60 ms courent pendant la création de la dalle (reset GPIO33, 120 ms, initialisation) ; `ws7b_touch_create()` n'attend que le reste. L'adresse est toujours cherchée après la dalle. Second essai : impulsion complète, comme avant |
| 7B : reset et initialisation de la dalle EK79007 | code du composant et `WS7B_EK79007_INIT` | composant, table de `ws7b_pur.h` | non touchées |
| Les deux : fin du dernier transfert avant la lumière | au plus 100 ms + 20 ms | `main.cpp` (`demarrage-7b.md`) | déjà une attente d'état + une période de balayage : gardée |

## 2. Ordre de démarrage (`main.cpp`)

`reglages_demarrer` → `board_init` → `meteo_client_relire_cache` → `ecran_principal_creer` → première image → lumière → réseau et données. Le réseau, les données, le serveur web et la console viennent déjà après la lumière. La relecture du cache coûte 5 ms (mesuré sur la 7B, `demarrage-7b.md` §6), pas les 150 à 200 ms que l'audit supposait : la loger dans l'attente de 1 s du Tab5 demanderait une nouvelle fonction de la couche carte pour 5 ms au mieux ; non fait. Ce qui coûte, c'est le premier rendu (glyphes à froid, météo) : hors de ce lot.

## 3. Gain attendu (estimé, non mesuré)

- **Tab5** : jusqu'à ~120 ms de moins entre « contrôleur détecté » et « dalle prête », selon le temps que mettent l'horloge, l'alimentation, la détection et le DSI (tout ce qui se faisait après les 120 ms se fait maintenant pendant). Si la puce tactile ne répond qu'au bout de ~120 ms après le relâchement (le ST7121 intègre le tactile et la dalle), la détection absorbe le délai et le gain se réduit à la durée de l'horloge, de l'alimentation et du DSI.
- **7B** : ~70 ms de moins entre « dalle prête » et « LVGL prêt », donc sur la première lumière.

## 4. Protocole de mesure sur carte (non fait)

Pour chaque carte, binaire `main` avant ce lot puis binaire de ce lot, application seule, 10 redémarrages logiciels par la commande série `redemarrer`, un seul `open` du port (DTR et RTS à zéro) :

1. relever la ligne `DEMARRAGE ms : …` (une par démarrage) et, sur le Tab5, la ligne `reset matériel de la dalle : N ms attendues` ; comparer « carte », « lumière » et les jalons de la carte, moyenne et extrêmes ;
2. vérifier à chaque démarrage : contrôleur détecté, tactile trouvé (7B : « Tactile : Goodix GT911 »), aucune ligne `essai` de l'expandeur, aucun second reset du tactile ;
3. l'image : Axel regarde la dalle à quelques redémarrages (image complète à l'allumage, pas d'écran noir) ;
4. un démarrage après **coupure de courant complète** sur le Tab5 (USB débranché, appui long, rebranchement) avec le journal ouvert : la réserve de `1a.md` (« image seulement après un second geste ») ne doit pas empirer.

Vu sur : aucune carte. Commits du lot sur la branche `claude/demarrage-plus-rapide-deed9d`, le 12/11/2026, compilés sur le PC (Tab5 et 7B).
