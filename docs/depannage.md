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

**Cause.** Inexpliquée au 06/11/2026. Piste à vérifier d'abord, par analogie avec le firmware ESPHome (écran noir après redémarrage, reset de l'écran par l'expandeur qui a besoin d'une seconde pour se stabiliser) : non vérifiée avec ce firmware.

**Solution.** Aucune. La piste matériel refait la coupure pas à pas pour voir le premier démarrage ; ce que le critère de 1a exige (« image après redémarrage **et** après coupure ») n'est donc pas encore pleinement constaté.

**Vu sur** Tab5 ST7121, MAC `E8:F6:0A:E2:F6:B6`, firmware natif de la phase 1a (commit à relever dans `mesures/1a.md`), 06/11/2026. **Preuve** : rapport de la piste matériel, journal série. **Statut** : ouvert.

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

**Solution.** 965 : c'est ce qui fait déjà fonctionner la tablette sous ESPHome. 900 puis 1300 seulement si l'écran reste noir. Marges DPI concordantes sur quatre sources : horloge 70 MHz, horizontales 40/2/40, verticales 24/20/200, 720 × 1280 en RGB565. **Vu sur** Tab5 ST7121. **Statut** : choix fait ; la valeur finale du code est à lire dans `mesures/1a.md`.

### Le composant `esp_lcd_st7123` 1.0.2 ne convient pas au Tab5 tel quel

Ses macros décrivent une dalle 720 × 1560, pas celle du Tab5. La table d'initialisation du Tab5 ST7123 est dans l'esp-bsp (`disp_init_data_1.h`), à porter quand l'exemplaire sera revenu du SAV. Le ST7123 et l'ILI9881C sont **détectés** (journal + `ESP_ERR_NOT_SUPPORTED`), pas initialisés. **Statut** : connu, non traité.

### Les accents s'affichent en carrés ou manquent

**Cause.** Les polices intégrées de LVGL s'arrêtent à l'ASCII.

**Solution.** Police TinyTTF embarquée (Roboto Bold, licence OFL, `main/assets/`). Toute icône ou glyphe affiché doit exister dans la police chargée : une icône MDI n'est jamais un rectangle vide. **Statut** : résolu (accents) ; icônes : à vérifier quand elles arrivent.

### Le point tactile est décalé vers la gauche

**Symptôme.** Premier essai sur le Tab5 : le point suit le doigt mais décalé vers la gauche ; un glissement pouvait faire défiler l'écran.

**Cause.** Le point était posé sur l'écran dont le remplissage haut (90 px) décale les objets flottants : LVGL les place par rapport à la zone de contenu du parent.

**Solution.** Point sur la couche supérieure (sans remplissage), écran non défilable, et une **mire** (cadre sur les bords + cinq cibles aux coins et au centre) pour mesurer le décalage restant entre le tactile et l'image à partir des appuis du journal série.

**Vu sur** Tab5 ST7121, commit `e7a4803`, 06/11/2026. **Preuve** : message du commit ; mesure du décalage résiduel dans `mesures/1a.md`. **Statut** : corrigé dans le code, mesure à confirmer.

### Écran noir après un redémarrage logiciel

*Connu ailleurs, pas encore vu ici.* Sur le firmware ESPHome, la broche de reset de l'écran passe par l'expandeur PI4IOE5V6408, qui a besoin d'environ une seconde pour être fiable : reset trop tôt = écran noir. Le firmware natif reprend l'attente de 1 s avant le reset (propre à la carte : la 7B n'en a pas, son reset est une broche du P4). Sur P4, `esp_restart()` ne remet à zéro que les cœurs : terminer par `esp_rom_software_reset_system()`. À vérifier au jalon 1b par 20 redémarrages.

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

---

## Page de réglages et noyau

### Aucun compilateur hôte sur le PC : les tests du noyau ne s'exécutent pas

**Cause.** Pas de `gcc`, `clang` ni `cl` installé. **Solution.** Les tests sont des fonctions `constexpr` évaluées **à la compilation** (`static_assert`) par le g++ croisé RISC-V d'ESP-IDF en `-fsyntax-only` (`python test/host/lancer.py`), avec une **preuve par mutations** (18 modifications du noyau, 18 détectées). **Limite** : un test évalué à la compilation prouve la logique, pas l'exécution sur la carte. **Statut** : contourné.

### Un 404 sur l'essai d'un service LLM : erreur ou pas ?

Un 404/405 sur `GET /models` n'est **pas** une erreur (service local valide : tous n'exposent pas cette route). Seul un 404 sur la route de l'essai réel donne la phrase « il manque peut-être /v1 » (contrat, [api-reglages.md](api-reglages.md)). **Statut** : résolu dans le contrat.

### Le faux service recopie la clé reçue dans son erreur

Voulu : le faux service répond 401 avec un corps qui recopie la clé, pour **prouver** que l'appareil ne retransmet jamais le texte d'un service tiers. Contrôle du contrôle : un essai de mutation fait échouer deux vérifications.

### Le parcours web demande le réseau

`python web/tests/parcours.py` appelle vraiment Open-Meteo (recherche de lieu, météo) : sans Internet il échoue. Rejoué le 06/11/2026 par la session de coordination : 49/49 vérifications, code de sortie 0. La feuille de style utilise `:has()` : sur un téléphone ancien, seul le contour rouge d'un champ invalide manquerait (Safari iOS et Firefox : non vérifié).
