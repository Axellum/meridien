# Dépannage — incidents déjà diagnostiqués

Tenu au fil des sessions. **À lire avant de re-diagnostiquer un symptôme qui ressemble à l'un de ceux-ci** ; plusieurs coûtent des heures quand on les cherche à nouveau.

Format de chaque entrée : **Symptôme → Cause → Solution**, puis trois lignes :
- **Vu sur** : carte, révision d'écran, MAC, commit, date (voir [compatibilité](compatibilite.md#version-du-firmware-et-de-la-carte--comment-on-la-note)).
- **Preuve** : comment on l'a constaté (journal, écran, mesure, relecture).
- **Statut** : *résolu*, *contourné*, *ouvert* ou *connu ailleurs, pas encore vu ici*.

Une entrée « connue ailleurs » vient du firmware ESPHome du Tab5 ([son dépannage](../../00ProjetTab/docs/troubleshooting.md)) : la cause matérielle est la même, mais elle n'a pas encore été revue avec ce firmware natif.

## Sommaire

- [Ouverts](#ouverts)
- [Outils et compilation](#outils-et-compilation)
- [Puce, écran et tactile](#puce-écran-et-tactile)
- [Flash, port série et sauvegarde](#flash-port-série-et-sauvegarde)
- [Page de réglages et noyau](#page-de-réglages-et-noyau)

---

## Ouverts

### Après une coupure de courant, l'image n'apparaît qu'après un second geste

**Symptôme.** Coupure de courant du Tab5, puis remise : le premier démarrage ne montre pas d'image ; l'image « Bonjour » et la mire s'affichent après un second geste d'Axel (démarrage n° 9 propre, ST7121 détecté). Le premier démarrage « sans écran » **n'est pas dans le journal série** : le compteur de démarrages passe de 8 à 9.

**Cause.** **Non trouvée** au 06/11/2026. Le journal ne contient qu'un démarrage après la coupure (n° 9), complet et propre : ST7121 détecté au 1er sondage, première image, rétroéclairage à 75 % à 1598 ms. Donc soit le premier démarrage n'a pas lancé ce firmware, soit il n'a pas été capturé (tablette sur batterie, sans port série). Hypothèses **non vérifiées** : l'alimentation USB du Tab5 n'allume peut-être pas la tablette sans appui sur le bouton ; le reset de l'écran par l'expandeur (qui a besoin d'une seconde, par analogie avec ESPHome) ne semble pas en cause puisque le n° 9 est propre. La cause lue du rallumage est « chien de garde » (`HP_SYS_HP_WDT_RESET`) aux **deux** remises sous tension observées : peut-être la cause normale d'un rallumage de cette carte, non vérifié, aucun plantage (`Guru`, `abort`, `Backtrace`) vu.

**Solution.** Aucune appliquée. Pour l'utilisateur : après une coupure complète, un appui court sur le bouton allume la mire. Le critère de 1a (« image après redémarrage **et** après coupure ») est donc constaté **avec réserve** ; l'essai pas à pas (rebrancher l'USB sans toucher aucun bouton, attendre 30 s, noter ce qui s'affiche, port série ouvert) est à refaire au jalon 1b avant d'attribuer la cause au firmware ou à la carte. Axel a choisi de ne pas le refaire pendant 1a.

**Vu sur** Tab5 ST7121, MAC `E8:F6:0A:E2:F6:B6`, firmware natif commit `e7a4803`, 06/11/2026. **Preuve** : journal du démarrage n° 9, constat d'Axel ; détail dans [mesures/1a.md](mesures/1a.md). **Statut** : ouvert.

### Retour à ESPHome jamais essayé

La recette de remise en état (trois écritures : début de flash, application, NVS) est dans le `LISEZMOI.md` de `_sauvegardes\tab5_st7121_avant_ecran_autonome_2026-11-06\`. Elle n'a **jamais été jouée** : tant qu'elle ne l'est pas, le retour est « non vérifié ». **Statut** : ouvert.

---

## Outils et compilation

### `export.ps1` d'ESP-IDF échoue, puis « python_env doesn't exist »

**Symptôme.** L'`export.ps1` de l'ESP-IDF de `H:\idf` s'arrête (il cherche `python_env\idf5.5_py3.13_env`) ; ensuite `idf.py set-target` répond « python_env doesn't exist ».

**Cause.** Cet ESP-IDF est installé et possédé par ESPHome, avec une autre organisation de l'environnement Python (`penvs\5.5.5`) que celle attendue par les scripts d'Espressif.

**Solution.** `tools/idf.ps1` n'utilise pas `export.ps1` : il appelle `tools\idf.py` avec le Python de `H:\idf\penvs\5.5.5`, pose le `PATH` des outils (cmake, ninja, ccache, GCC RISC-V) et la variable `IDF_PYTHON_ENV_PATH`. Ne rien modifier dans `H:\idf`.

**Vu sur** PC d'Axel (Windows 11), ESP-IDF 5.5.5, 06/11/2026. **Preuve** : squelette compilé en 38 s, 204 048 octets (non flashé). **Statut** : résolu.

### Avertissements et chemins morts sans effet

- « Error while generating esp_rom gdbinit » au début d'une compilation : inoffensif, la compilation réussit. *Statut : constaté sur le squelette.*
- `idf-env.json` de `H:\idf` cite encore `E:\idf` (chemin mort, fichier d'ESPHome non corrigé) : sans effet sur nos compilations jusqu'ici. *Statut : non vérifié au-delà.*

### Le PC plante pendant une compilation

**Cause.** Trop de tâches en parallèle (le PC a déjà planté à 6) ou deux compilations à la fois.

**Solution.** `CMAKE_BUILD_PARALLEL_LEVEL=4` (posé par `tools/idf.ps1`), **une seule compilation ESP-IDF à la fois**, depuis PowerShell (jamais Git Bash). Après un plantage : `git fsck`. Les sessions qui n'ont pas besoin de compiler (page web, noyau, recherche) ne compilent pas.

**Vu sur** PC d'Axel : planté à 6 tâches en parallèle (consigne d'Axel, compilations ESPHome), jamais encore avec ce projet. **Statut** : contourné par la limite à 4.

### Une modification de `sdkconfig.defaults*` n'a aucun effet

**Cause.** Les valeurs par défaut ne sont relues que lorsque `sdkconfig` n'existe pas.

**Solution.** Supprimer `sdkconfig`, relancer `.\tools\idf.ps1 set-target esp32p4`, puis vérifier la valeur dans le `sdkconfig` généré (pas dans le fichier de défauts). **Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : résolu.

### Un script PowerShell est refusé : suppression visant `H:\idf`

Le garde-fou de l'outil de session refuse toute commande contenant un `Remove-Item` sur `H:\idf`, même dans un script écrit ailleurs. Ne pas supprimer dans les scripts ; déplacer ou ignorer. **Statut** : contourné.

---

## Puce, écran et tactile

### ESP-IDF refuse de démarrer sur la puce du Tab5

**Cause.** La puce du Tab5 est un ESP32-P4 **révision 1.3** ; ESP-IDF 5.5.5 exige par défaut une révision ≥ 3.1.

**Solution.** Dans `boards/tab5/sdkconfig.defaults.board` : `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` et `CONFIG_ESP32P4_REV_MIN_100=y` (vérifié dans le `sdkconfig` généré : `CONFIG_ESP32P4_REV_MIN_FULL=100`). Les réglages de puce sont propres à chaque carte : la 7B (révision 3.x) a les siens.

**Vu sur** Tab5 ST7121, 06/11/2026 (relevé par la piste matériel). **Statut** : résolu.

### Quelle vitesse de ligne DSI pour le ST7121 ? Quatre sources se contredisent

| Source | Valeur |
|---|---|
| esp-bsp (`m5stack_tab5`) | 965 Mbit/s |
| ESPHome, modèle `M5STACK-TAB5-ST7121` (celui qui fait tourner cette tablette) | 965 Mbit/s |
| espp | 900 Mbit/s |
| macro du composant `esp_lcd_st7121` | 1300 Mbit/s |

**Solution.** 965 : c'est ce qui fait déjà fonctionner la tablette sous ESPHome. 900 puis 1300 seulement si l'écran reste noir. Marges DPI concordantes sur quatre sources : horloge 70 MHz, horizontales 40/2/40, verticales 24/20/200, 720 × 1280 en RGB565. **Vu sur** Tab5 ST7121. **Statut** : résolu — 965 Mbit/s est la valeur finale, l'image s'affiche avec elle (constaté le 06/11/2026, commit `e7a4803`) ; les essais à 900 et 1300 n'ont pas été faits, inutiles.

### Le composant `esp_lcd_st7123` 1.0.2 ne convient pas au Tab5 tel quel

Ses macros décrivent une dalle 720 × 1560, pas celle du Tab5. La table d'initialisation du Tab5 ST7123 est dans l'esp-bsp (`disp_init_data_1.h`), à porter quand l'exemplaire sera revenu du SAV. Le ST7123 et l'ILI9881C sont **détectés** (journal + `ESP_ERR_NOT_SUPPORTED`), pas initialisés. **Statut** : connu, non traité.

### Les accents s'affichent en carrés ou manquent

**Cause.** Les polices intégrées de LVGL s'arrêtent à l'ASCII.

**Solution.** Police TinyTTF embarquée (Roboto Bold, licence OFL, `main/assets/`). Toute icône ou glyphe affiché doit exister dans la police chargée : une icône MDI n'est jamais un rectangle vide. **Statut** : résolu (accents) ; icônes : à vérifier quand elles arrivent.

### Le point tactile est décalé vers la gauche

**Symptôme.** Premier essai sur le Tab5 : le point suit le doigt mais décalé vers la gauche ; un glissement pouvait faire défiler l'écran.

**Cause.** Le point était posé sur l'écran dont le remplissage haut (90 px) décale les objets flottants : LVGL les place par rapport à la zone de contenu du parent.

**Solution.** Point sur la couche supérieure (sans remplissage), écran non défilable, et une **mire** (cadre sur les bords + cinq cibles aux coins et au centre) pour mesurer le décalage restant entre le tactile et l'image à partir des appuis du journal série.

**Vu sur** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Preuve** : constat d'Axel (le point est sous son doigt) et cinq appuis sur des cibles connues : écart moyen (−6, −4) px, de −12 à +10 px sur quatre cibles et −26 px en x sur la cinquième (haut droit), sans étalonnage appliqué (détail dans [mesures/1a.md](mesures/1a.md)). **Statut** : résolu ; précision de l'ordre d'un doigt.

### Aucun journal par l'USB, ou une erreur de compilation sur un `|` entre deux drapeaux LVGL

- **Pas de journal** : il faut `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (la console UART par défaut n'est pas câblée à l'USB du Tab5). Posé dans `boards/tab5/sdkconfig.defaults.board`.
- **`A | B` sur deux drapeaux LVGL** : en C++ le résultat est un `int` que LVGL refuse ; un appel par drapeau. De même, les macros de configuration d'Espressif (initialiseurs désignés dans un ordre que C++ refuse) : la couche carte (`components/bsp_tab5/`) est donc écrite en **C**, l'application en C++.

**Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : résolu.

### Journal `swap_xy is not supported by this panel` au démarrage

Émis par `esp_lvgl_port` à l'initialisation ; sans effet constaté sur l'image (orientation portrait native fixe, aucune rotation logicielle). Cause non explorée. **Vu sur** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Statut** : connu, sans effet constaté.

### Marge faible du bootloader et test de la PSRAM

Le bootloader occupe 0x5a30 octets dans ses 24 Kio : **6 % de marge** seulement, à surveiller si on ajoute des options. Le démarrage prend environ 2,9 s de la remise à zéro à la première image, dont ~0,95 s de test de la PSRAM (`CONFIG_SPIRAM_MEMTEST=y`) et 1 s d'attente de l'expandeur : deux gains possibles, **à mesurer avant de sacrifier quoi que ce soit**, un changement à la fois. **Vu sur** Tab5 ST7121, 06/11/2026. **Statut** : connu, pas traité.

### Écran noir après un redémarrage logiciel

*Connu ailleurs ; ici, un seul redémarrage logiciel essayé et réussi (n° 2 du 06/11/2026, `SW_SYS_RESET`, image revenue, Tab5 ST7121), pas de série de 20.* Sur le firmware ESPHome, la broche de reset de l'écran passe par l'expandeur PI4IOE5V6408, qui a besoin d'environ une seconde pour être fiable : reset trop tôt = écran noir. Le firmware natif reprend l'attente de 1 s avant le reset (propre à la carte : la 7B n'en a pas, son reset est une broche du P4). Sur P4, `esp_restart()` ne remet à zéro que les cœurs : terminer par `esp_rom_software_reset_system()`. À vérifier au jalon 1b par 20 redémarrages.

---

## Flash, port série et sauvegarde

### `read-flash` échoue : « Packet content transfer stopped »

**Cause.** Lecture de toute la flash avec le stub d'esptool sur une zone qui ne répond pas ; ici aussi, une erreur d'offset (la NVS n'est pas au début de la flash).

**Solution.** Lire les partitions utiles aux offsets lus dans la **table de partitions de la carte** (ici la NVS d'ESPHome est en `0xf90000`), deux fois, comparer les SHA-256 ; repli `--no-stub` si le stub échoue ; `verify-flash` (somme MD5 calculée par la puce) pour prouver qu'un fichier rangé est identique à la flash. **Vu sur** Tab5 ST7121, esptool 5.3.1, 06/11/2026. **Statut** : résolu (sauvegarde partielle faite, voulue par Axel : le code est dans git, pas la NVS).

### Le port COM n'est pas celui qu'on croit

**Cause.** Le numéro change avec la prise USB ; le 06/11/2026 le Tab5 était sur COM11, et COM14 était un **ESP32-S3 d'un autre projet** (MAC `1C:DB:D4:40:07:64`).

**Solution.** Toujours retrouver le port par la **MAC** (`serial.tools.list_ports`, numéro de série USB) et la relire avant d'écrire. Prendre le verrou de l'appareil et du port (`C:\Users\axell\.tab5\verrous.py`). **Statut** : contourné.

### Ouvrir le port série redémarre la carte

Ouvrir ou fermer le port USB peut réinitialiser la puce. Effet de bord constaté le 06/11 : le redémarrage USB a ramené le volume du firmware ESPHome de 85 à 100 (le firmware ESPHome ne le conserve apparemment pas ; non corrigé). Ouvrir le port avec `dtr=False, rts=False` avant `open()` quand on veut écouter sans réinitialiser.

### Capture série sous Windows : rien ne s'affiche, ou le compteur de démarrages monte seul

- Une impulsion RTS n'est transmise que si DTR est aussi mis à jour (esptool le fait) ; une lecture lancée sans délai après l'ouverture bloque.
- `esptool flash-id` et `read-mac` **remettent la puce à zéro à chaque appel** : le compteur de démarrages du journal monte (le n° 3 du 06/11 est probablement celui-là). Ne pas les lancer pendant un essai de redémarrage.
- Un USB-C mal enfoncé laisse la tablette allumée **sur batterie, sans port série** (Windows : `Present = False` pour la MAC) : aucun journal, donc aucune cause lisible pour ce qui se passe pendant ce temps.

**Vu sur** Tab5 ST7121, esptool 5.3.1, 06/11/2026. **Statut** : contourné.

---

## Page de réglages et noyau

### Aucun compilateur hôte sur le PC : les tests du noyau ne s'exécutent pas

**Cause.** Pas de `gcc`, `clang` ni `cl` installé (ni Visual Studio) sur le PC d'Axel.

**Solution provisoire (jusqu'au 06/11/2026).** Les tests sont des fonctions `constexpr` évaluées **à la compilation** (`static_assert`) par le g++ croisé RISC-V d'ESP-IDF en `-fsyntax-only`.

**Solution définitive.** llvm-mingw (clang 23.1.5) installé sur H: le 06/11/2026, voir [outils.md](outils.md) : `. H:\AuxFilsDesIdees\_outils\env_hote.ps1 -Sanitizers` puis `python test/host/lancer.py` compile, lie et **exécute** les tests, avec AddressSanitizer et UBSan.

**Vu sur** PC d'Axel (Windows 11), `main` au 06/11/2026. **Preuve** : un programme fautif volontaire (lecture hors d'un tableau) est détecté par AddressSanitizer avec la ligne et le lieu d'allocation ; les 23 tests du noyau passent exécutés (avec et sans détecteurs) ; 18 mutations sur 18 détectées. Après la fusion des pistes, le même contrôle exécute 252 tests (ASan + UBSan) et détecte 206 mutations sur 206. **Limite** : le PC est en x86-64 ; un test réussi sur le PC ne prouve pas le comportement du RISC-V de la carte (tailles, alignement, mémoire disponible) : le constat sur carte reste nécessaire. **Statut** : résolu.

### Un 404 sur l'essai d'un service LLM : erreur ou pas ?

Un 404/405 sur `GET /models` n'est **pas** une erreur (service local valide : tous n'exposent pas cette route). Seul un 404 sur la route de l'essai réel donne la phrase « il manque peut-être /v1 » (contrat, [api-reglages.md](api-reglages.md)). **Statut** : résolu dans le contrat.

### Le faux service recopie la clé reçue dans son erreur

Voulu : le faux service répond 401 avec un corps qui recopie la clé, pour **prouver** que l'appareil ne retransmet jamais le texte d'un service tiers. Contrôle du contrôle : un essai de mutation fait échouer deux vérifications.

### Le parcours web demande le réseau

`python web/tests/parcours.py` appelle vraiment Open-Meteo (recherche de lieu, météo) : sans Internet il échoue. Rejoué le 06/11/2026 par la session de coordination : 49/49 vérifications, code de sortie 0. La feuille de style utilise `:has()` : sur un téléphone ancien, seul le contour rouge d'un champ invalide manquerait (Safari iOS et Firefox : non vérifié).
