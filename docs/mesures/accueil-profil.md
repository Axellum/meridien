# Accueil, réglage « Profil » et « Profil ▸ » dans la roue — ce qui est constaté sur le PC et à la compilation, ce qui reste à voir sur une carte

> **Summary (EN)** — The "Profil" setting (Auto or a pinned profile) and the "Profil ▸" family of the press-and-hold wheel
> (ADR-0023, « Complément ») were **seen on PC renders, on the fake web device and compiled for both boards only**: 1 964 core tests
> with AddressSanitizer and UBSan (20 of them new), 183 targeted mutations over the five test files this change touches, all detected
> (25 of them also run with ESP-IDF's g++ 14.2 in syntax mode), 320 checks of the web walkthrough on the fake device, 8 new wheel
> renders (five buttons: alert + sea + profile) and 16 earlier renders re-drawn byte for byte, ESP-IDF 5.5.5 builds of the Tab5 and the
> Waveshare 7B from a clean tree (+5 136 bytes of flash image on each, +192 bytes of static RAM, all in `.data`). **Not seen on a
> board**: the NVS write and its survival of a real reboot, the touch of the wheel, the legibility of the marked choice at one metre,
> the new row on the settings screen, how the AI-written languages read to a native speaker. Nothing flashed, no serial port opened.

**Vu sur** : PC d'Axel (Windows 11) ; clang++ 23.1.5 avec AddressSanitizer + UBSan (tests), g++ 14.2.0 croisé d'ESP-IDF en mode
« syntaxe » (rien d'exécuté), clang-format 23.1.5 ; LVGL 9.5.0 (rendu PC) ; Playwright et le faux appareil de `web/` (parcours) ;
ESP-IDF 5.5.5 (compilation) ; commit `6a3caaa` (arbre propre, version `jalon-1a-tab5-st7121-311-g6a3caaa`, sans « -dirty »),
08/11/2026. **Aucune carte** (ni Tab5 ni 7B), **rien flashé**, aucun port série ouvert. La 7B reste « prévue » : le rendu 1024×600 ne
simule que sa taille.

Règle de lecture : « mesuré » = relu sur une sortie d'outil de cette session ; tout le reste est une lecture de code ou **non vérifié**.

## 1. Question

Le réglage « Profil » (« Auto » : les règles de l'[ADR-0015](../decisions/0015-accueil-mode-automatique.md) décident ; « Météo » :
l'accueil reste sur la météo) se règle-t-il de bout en bout (page web, écran de réglages, roue d'appui long, NVS), sans offrir un profil
que l'appareil ne dessine pas, sans épingler par accident, et la roue tient-elle à l'écran avec une entrée de plus quand une alerte et
un spot de mer sont aussi là ?

## 2. Constaté

| Quoi | Résultat | Comment |
|---|---|---|
| Tests du noyau | **1 964 tests exécutés, 0 échec** ; dont **20 nouveaux** pour ce changement : 13 pour l'épinglage (`test_accueil_epinglage`), 2 pour l'API, 1 pour l'hôte de l'appareil, 4 pour la liste du premier anneau de la roue (plus des assertions ajoutées au registre des réglages) | crochet de commit `verifier.ps1 -Rapide` au commit de fusion `6a3caaa` : mode « complet (compilé + exécuté) », clang++ 23.1.5, ASan + UBSan |
| Mutations ciblées | **183 sur 183 détectées** (0 problème), chacune par le test prévu ; 183 = tout ce que les cinq fichiers de test touchés portent (pas seulement les nouvelles) ; les nouvelles : 13 pour l'épinglage et l'API (9 + 2 + 2), 6 pour la liste de la roue | `python test/host/lancer.py --mutations --seulement test_roue_entrees,test_accueil_epinglage,test_hote_appareil,test_api_reglages,test_registre_reglages` |
| Mêmes tests avec le compilateur d'ESP-IDF | `test_roue_entrees.cpp` et `test_accueil_epinglage.cpp` : compilation OK, `static_assert` évalués ; **mode « syntaxe », rien d'exécuté** ; **25 mutations sur 25** détectées dans ce mode | `python test/host/lancer.py --cxx …\riscv32-esp-elf-g++.exe [--mutations] --seulement test_roue_entrees,test_accueil_epinglage` |
| Page web contre le faux appareil | **320 vérifications sur 320** (sept langues, 360 px sans défilement horizontal ; pour le profil : « Auto » au départ, deux profils offerts et seulement ceux de l'appareil, « Météo » enregistré puis « Auto » rendu) ; le refus `422 profil_inconnu` est vérifié par les tests du noyau (`test_api_reglages`), pas par ce parcours ; poids : **24 375 octets gzip** pour un budget de 30 720 | `python web/tests/parcours.py` (Playwright, faux serveur Node) |
| Police des icônes de la roue | **8 glyphes**, 2 140 octets, SHA-256 `cff73de8…138c` ; les huit sont dans la table des caractères et **aucun n'est un rectangle vide** ; régénérée depuis le manifeste fusionné, jamais fusionnée à la main | `python tools/icones/generer_police_roue.py --verifier` |
| Rendus de la roue | **8 nouveaux PNG** (cinq boutons : Réglages, Détails, Alertes, Mer, Profil ; repliée, dépliée, près du coin bas droit ; 1280×720 et 1024×600 ; allemand et turc au centre à 1024×600) ; **16 rendus déjà faits redessinés après la fusion : identiques octet pour octet** | `.\tools\rendre.ps1 -Scene auto -Variante roue-profil… ` ; comparaison fichier par fichier |
| La roue tient à l'écran | journal du banc : « **tient** » pour 3, 4 et 5 boutons, repliée ou dépliée, au centre, au bord gauche et au coin bas droit, aux deux tailles des cartes ; **ne tient pas** : le coin bas droit du turc en 720×1280 (portrait, pas un usage des cartes), à 4 comme à 5 boutons | journal du banc ; voir [depannage.md](../depannage.md) |
| Compilation Tab5 | réussie : `meridien.bin` **3 101 296 octets** ; avant (`587b5ca`, la branche A fusionnée avec `origin/main`) : 3 096 160 → **+5 136 octets** | `.\tools\idf.ps1 -DBOARD=tab5 set-target esp32p4` puis `build`, depuis un arbre propre ; carte relue dans `BOARD:STRING=tab5` |
| Compilation 7B | réussie : `meridien.bin` **3 081 872 octets** ; avant : 3 076 736 → **+5 136 octets** | même commande avec `waveshare-7b` ; `BOARD:STRING=waveshare-7b` |
| Avertissements | aucun du compilateur ; les deux journaux ne contiennent que l'avertissement CMake `gdbinit.cmake` d'ESP-IDF | `grep -i warning` sur les journaux |
| Mémoire interne (DIRAM) | Tab5 : 150 718 → **150 910** (+192) ; 7B : 150 714 → **150 906** (+192) ; `.bss` inchangé (41 144 / 40 928), tout est dans `.data` ; flash : `.rodata` +3 168 et `.text` +1 772 sur les deux cartes | `idf.ps1 size` |
| D'où vient le +192 | l'état de la roue passe de **1 436 à 1 628 octets** sur la carte (le champ `actif` de chaque choix copié) ; son `sizeof` sur le PC passe de 2 192 à **2 576** | `riscv32-esp-elf-nm -S` sur les deux ELF ; `ui_roue_cout().ram_statique` au banc |
| Le code est lié au binaire | `choisir_profil`, `choisir_profil_auto`, `choisir_profil_meteo`, `composer_entrees`, `composer_choix_profil`, `ouvrir_mer`, `mer_atteignable` sont dans les deux ELF | `riscv32-esp-elf-nm -C` (un « build OK » seul ne le prouve pas) |

Le « avant » est la compilation propre de `587b5ca` (arbre `Y:\`, version `jalon-1a-tab5-st7121-306-g587b5ca`), le « après » celle de
`6a3caaa` dans le même arbre.

## 3. Ce que les rendus montrent (regardés)

- **Cinq boutons à 1024×600** (la taille de la 7B), au centre : Réglages, Détails, Alertes, Mer, Profil ▸, sans chevauchement ; le moyeu
  dit « Profil » une fois dépliée (le mot de la famille n'est plus dessiné au premier anneau) ; le second anneau porte « Auto » et
  « Météo », le choix en vigueur en jaune d'accent.
- **Près du coin bas droit à 1280×720 et 1024×600** : la roue se retourne, les cinq boutons et les deux choix restent entiers.
- **Repliée avec Météo épinglé** : « Profil » est le dernier bouton, le moyeu dit « Aller à », la pastille de l'accueil dit « Météo ·
  épinglé » avec « Auto en pause », la carte pluie ne s'ouvre pas, le bandeau d'alerte reste agrandi.
- **Turc au centre à 1024×600** : « Otomatik » et « Hava durumu » entiers.
- **Pas regardés de près** : les rendus à cinq boutons à 1280×720 (centre et repliée) et en allemand ; ils sont couverts par le journal
  du banc (« tient »), pas par l'œil.

## 4. Non vérifié — à juger sur la carte

- **Rien n'a été vu sur un écran réel** : ni le toucher de « Profil » puis de « Météo », ni l'accueil qui suit, ni la lisibilité du choix
  marqué à un mètre.
- **La NVS** : `nvs_set_u8` « profil » (avant le `commit` existant) n'a été exercée que sur le PC (tests de l'API et de l'hôte). **Pas
  essayé** : redémarrer et retrouver Météo épinglé ; dépingler depuis la page web ; relire une carte qui a l'ancien blob « affichage »
  sans la clé.
- **La rangée « Profil »** de la page « Affichage » de l'écran de réglages à 1024×600 : vue sur les rendus, pas sur la carte.
- **Le temps** entre le toucher d'un profil et le nouvel accueil, la mémoire après plusieurs ouvertures de la roue : non mesurés.
- **Les langues** : l'allemand, le néerlandais, l'espagnol, l'italien et le turc sont écrits par l'IA, sans relecteur de la langue.
- Les tests sont écrits par la même main que le code ; ils prouvent que le code fait ce que l'ADR écrit, **pas** que « Profil ▸ en
  dernier » est le bon choix : à Axel de juger.
- `lancer.py --mutations` sur **toute** la liste n'a **pas** été relancé en entier : seulement les cinq fichiers ci-dessus ; la CI
  lance le travail `mutations-noyau` à la main.
- **Aucun indice d'appui long** (« une seule fois ») n'existe dans le dépôt et **aucun n'a été construit**.
