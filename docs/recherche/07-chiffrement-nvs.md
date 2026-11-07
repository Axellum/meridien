# Chiffrer la NVS (mot de passe Wi-Fi, clés d'API, réglages) — étude du 07/11/2026

> **Summary (EN)** — Study of 2026-11-07, read from the ESP-IDF 5.5.5 sources actually used by this project (docs, Kconfig, C code); nothing was run on a board, no eFuse was burnt, nothing was flashed.
> - Today the Wi-Fi password (`wifi/mdp`), the three API keys and every setting sit in clear text in the `nvs` partition: whoever has the board **or a full flash backup** can read them.
> - A scheme without eFuse protects nothing against a flash dump (the key would sit in the same flash, or be a constant of the IDF source).
> - Recommended: **NVS encryption with keys derived by the HMAC peripheral from one eFuse key block**, no flash encryption, no secure boot, first on the Tab5 ST7121 development board only. It stops reading the flash; it does not stop someone who can flash their own firmware over USB.
> - Coded and tested on the PC only: a pure-core classifier of a raw NVS partition, a decision table that forbids opening in the wrong mode (which silently erases) and forbids creating the key without an order, and a clear → encrypted migration with proofs. 50 tests, 61 mutations. Not wired into the firmware. The irreversible step (burning the key block) awaits the owner's written agreement.

**Vu sur** : PC de développement (Windows 11), ESP-IDF 5.5.5 (sources lues le 07/11/2026), clang++ 23.1.5 ; branche `claude/nvs-chiffree-3510da`. **Aucune carte** : rien de ce qui suit n'a été essayé sur un Tab5 ni sur la 7B. Les mots « lu » et « constaté » sont employés au sens de [AGENTS.md](../../AGENTS.md) : « lu » = dans le code ou la doc, « constaté » = vu sur la carte (donc rien ici).

## Conclusion

1. **Aujourd'hui tout est en clair** : le mot de passe Wi-Fi, les trois clés d'API, les adresses des services, la ville, la progression de l'installation, le cache météo. Une lecture de la flash (`esptool read-flash`) ou une **sauvegarde complète de la flash** (exigée par AGENTS.md avant le premier flash d'une carte) les donne. D'après le code d'IDF, effacer un réglage ne retire pas ses octets de la flash (§4 ; lu, non constaté sur carte).
2. **Un schéma sans eFuse ne protège pas** : la clé serait dans la même flash que les données (ou, avec le schéma `nvs_keys` d'IDF sans chiffrement de flash, une constante écrite dans le code source d'IDF). C'est de l'obscurcissement : il arrête `strings`, pas quelqu'un qui lit le dépôt (§2.2).
3. **Recommandation : NVS chiffrée, clés dérivées par le périphérique HMAC d'une clé gravée dans UN bloc d'eFuse** (schéma « HMAC »), **sans** chiffrement de flash et **sans** démarrage sécurisé. Coût : 1 bloc d'eFuse sur 6, définitif ; la carte reste flashable par USB, sauvegardable, mise à jour par OTA, et **reste utilisable** si on renonce (on perd alors les réglages, pas la carte). Pourquoi pas plus : §2.3.
4. **Ce que ça protège** : la lecture de la flash, d'une sauvegarde, d'une carte revendue ou envoyée au SAV. **Ce que ça ne protège pas** : quelqu'un qui peut flasher son propre firmware par USB (il appelle le même périphérique HMAC et lit tout), la mémoire vive, la page de réglages en HTTP clair, le JTAG resté ouvert (§2.2).
5. **Premier essai sur le Tab5 ST7121 de développement seulement.** La 7B (révision de puce jamais lue, probablement ≥ 3.0) attend : la disposition des eFuse de clé change entre les révisions < 3.0 et ≥ 3.0 (`efuse_ll.h`, §2.4).
6. **Fait sur le PC, rien sur carte** : le noyau sait reconnaître le contenu d'une partition en lecture brute, décider s'il faut l'ouvrir et migrer sans rien perdre ni laisser de clair (§5). Il n'est branché à rien. **Reste à Axel** : accord écrit sur le schéma, puis les commandes du §6, une par une.

## 1. Ce qui est en clair aujourd'hui (lu le 07/11/2026)

| Donnée | Où | Lu dans |
|---|---|---|
| Mot de passe Wi-Fi | NVS, espace `wifi`, clé `mdp` (texte). Le pilote Wi-Fi n'en garde pas de copie : `esp_wifi_set_storage(WIFI_STORAGE_RAM)` | `main/reseau.cpp` (ligne 626), `main/wifi_stocke.cpp` |
| Trois clés d'API (STT, LLM, TTS), adresses, modèles, voix | NVS, espace `services` (13 identifiants du registre dont 3 de type secret) | `components/core/include/ecran/registre_reglages.hpp`, `hote_appareil.hpp` |
| Ville, affichage, progression de l'installation, cache météo (≤ 857 octets), compteur de démarrages | NVS, autres espaces (aucun n'est secret) | `main/meteo_zone.cpp`, `main/meteo_client.cpp`, `main/reglages.cpp`, `main/boot_info.cpp` |
| Options de sécurité | **aucune** : ni `CONFIG_NVS_ENCRYPTION`, ni chiffrement de flash, ni démarrage sécurisé dans `sdkconfig.defaults` ni dans `boards/*/sdkconfig.defaults.board` | `grep` sur ces fichiers |
| Partition | `nvs` en `0x810000`, 448 Ko, sur le Tab5 (flash 16 Mo) et la 7B (flash 32 Mo) ; sur le Tab5, la NVS d'ESPHome est gardée **inerte** en `0xf90000` (Wi-Fi, clé d'API et réglages d'ESPHome, contenu non relu ici) | `boards/*/partitions.csv` |

L'ADR-0003 (point 5) prévoit la NVS chiffrée et laisse le choix du schéma à Axel, en notant que le schéma HMAC consomme un bloc d'eFuse « définitivement ». Cette note chiffre les choix.

## 2. Les schémas possibles sur ESP32-P4 avec ESP-IDF 5.5.5

### 2.1 Comparatif

Sources : `docs/en/api-reference/storage/nvs_encryption.rst`, `docs/en/security/{flash-encryption,secure-boot-v2,security}.rst`, `components/nvs_flash/Kconfig`, `components/nvs_sec_provider/{Kconfig,nvs_sec_provider.c}`, `components/bootloader/Kconfig.projbuild`, `components/efuse/esp32p4/esp_efuse_table.csv` (ESP-IDF 5.5.5). **Lu, pas constaté.**

| | A. Sans eFuse (clé dans la flash) | **B. NVS chiffrée, clés HMAC (recommandé)** | C. Chiffrement de flash « développement » + NVS (`nvs_keys`) | D. Chiffrement de flash « release » + NVS | E. D + démarrage sécurisé v2 |
|---|---|---|---|---|---|
| eFuse brûlés | aucun | **1 bloc de clé** (`KEY0`..`KEY5`, but `HMAC_UP`), lecture et écriture protégées | `SPI_BOOT_CRYPT_CNT` (3 bits), 1 bloc de clé XTS-AES, JTAG fermés (`DIS_PAD_JTAG`, `DIS_USB_JTAG`), `DIS_DIRECT_BOOT`, `SPI_DOWNLOAD_MSPI_DIS` | ceux de C + `DIS_DOWNLOAD_MANUAL_ENCRYPT` + protection en écriture du compteur ; mode de téléchargement sécurisé | ceux de D + `SECURE_BOOT_EN` + empreinte(s) de clé dans un ou plusieurs blocs de clé |
| Réversible ? | oui | **non** pour l'eFuse ; la carte reste utilisable sans chiffrer (les réglages chiffrés deviennent illisibles : on les ressaisit) | non ; en mode développement on peut désactiver le chiffrement **une seule fois par puce** (doc, cibles autres que ESP32) | non | non (révocation de clés incluse, définitive) |
| Flasher par USB (esptool) | oui | **oui, comme aujourd'hui** | oui en clair (le chargeur chiffre au démarrage) ou `idf.py encrypted-flash` | **non** en clair ; seulement une image chiffrée sur le PC (copie de la clé requise) ou l'OTA | seulement des images signées |
| OTA | oui | **oui** | oui (chiffrée à l'écriture) | oui, c'est le chemin normal des mises à jour | oui, images signées |
| `esptool read-flash` | clair | **la NVS sort chiffrée** ; le reste (application, tables) sort en clair | tout sort chiffré (partitions marquées) | idem, sans retour possible | idem |
| Chargeur de démarrage | inchangé | **inchangé** | **grossit** (non mesuré) | idem | idem + signature |
| Protège contre | rien (obscurcissement) | lecture de la flash, d'une sauvegarde, d'une carte revendue | lecture de la flash ; **pas** contre un firmware flashé en clair par l'attaquant (le mode dit « not secure » dans la doc) | lecture de la flash et firmware flashé en clair | + firmware non signé |
| Risque principal pour Axel | fausse sécurité | brûler le mauvais bloc ; HMAC sur P4 révision 1.x jamais essayé | chargeur trop gros pour sa place ; flux de flash à refaire ; une erreur = carte en boucle de redémarrage (doc : « soft-brick ») | le même, sans retour | clé de signature perdue = plus jamais de mise à jour ; dépôt public : la clé privée ne doit jamais y entrer |

### 2.2 Franchise : ce que chaque schéma protège vraiment

- **A (rien de gravé) n'est pas une protection.** Avec le schéma `nvs_keys` d'IDF, `nvs_flash_generate_keys` écrit dans la partition de clés des **constantes** (`0xff…`, `0xee…`, `0x99…`, `0x88…`, `components/nvs_flash/src/nvs_api.cpp`, lignes 601-609) « sans chiffrement » ; ces constantes ne deviennent des clés propres à la puce que parce que la lecture de cette partition passe par le chiffrement de flash matériel. Sans chiffrement de flash, la clé effective est donc publique. Toute autre variante (clé tirée au hasard et rangée dans la flash) donne la clé à qui lit la flash. Cela arrête `strings` sur une sauvegarde, pas une personne qui a le dépôt.
- **B protège la lecture de la flash**, parce que la clé HMAC est protégée en lecture dans l'eFuse : une sauvegarde ne se déchiffre pas. **Mais** le but `HMAC_UP` sert à calculer un HMAC **remis au logiciel**, et les « graines » de dérivation sont des constantes d'IDF (`EKEY_SEED`, `TKEY_SEED`, `nvs_sec_provider.c`, lignes 115-125). Quelqu'un qui peut faire tourner son propre programme sur la carte (flasher un firmware par USB, car le démarrage sécurisé n'est pas activé) peut donc demander au périphérique les mêmes clés XTS et déchiffrer la NVS. Seul E, avec le mode de téléchargement fermé, empêche cela.
- **C est dominé par B pour notre besoin** : même faiblesse face à un attaquant qui flashe (la doc le dit : en mode développement, la ROM chiffre ce qu'on lui envoie), pour beaucoup plus d'irréversible. Chiffrer l'application n'apporte rien : le micrologiciel est public.
- **JTAG** : les eFuse de fermeture (`DIS_PAD_JTAG`, `DIS_USB_JTAG`) ne sont pas brûlés avec B. Je n'ai **pas vérifié** ce que le JTAG USB intégré donne sur le Tab5 ; si accessible, il donne la mémoire vive, clés comprises. Les fermer est irréversible et n'est pas proposé ici.

### 2.3 Pourquoi pas le chiffrement de flash ni le démarrage sécurisé maintenant

- Le flux de travail d'Axel est « compiler, flasher par USB, constater ». Le mode release le casse (images chiffrées sur le PC, copie de la clé), le mode développement le garde mais brûle plus et protège aussi peu que B.
- Le chargeur de démarrage tient dans 24 576 octets (de `0x2000` à `0x8000`, où commence la table de partitions) ; le fichier `bootloader.bin` d'un build local fait 23 088 octets, soit environ 1,5 Ko de marge (valeur lue sur un build de ce dépôt, pas sur ce commit). Le chiffrement de flash et le démarrage sécurisé ajoutent du code au chargeur : **non mesuré** (aucune compilation lourde lancée pour cette étude). S'il dépasse, il faut déplacer la table de partitions, donc reflasher chargeur et table. Pour mesurer sans flasher : `.\tools\idf.ps1 bootloader` avec les options en plus, dans un dossier de build à part.
- Le démarrage sécurisé v2 exige une clé privée de signature à garder pour toujours, signature à chaque mise à jour, un dépôt public qui n'en contient pas, et un chargeur qui ne se met plus à jour par OTA. C'est une décision de produit fini, pas de carte de développement.

### 2.4 Ce qui change entre le Tab5 et la 7B

- Tab5 ST7121 : P4 **révision v1.3**, compilé avec `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` (`docs/mesures/1a.md`). IDF dit que les révisions < 3.0 et ≥ 3.0 ont « d'énormes différences matérielles » (`Kconfig.hw_support`) et `efuse_ll.h` note que depuis la 3.00 le champ du but d'une clé passe de 4 à 5 bits. La révision de la 7B n'a **jamais été lue** (`docs/mesures/7b-materiel.md`). Les commandes d'eFuse du §6 ne se copient donc pas d'une carte à l'autre sans relire le résumé de la carte visée.
- Aucun `if (tab5)` : le mode (clair ou chiffré) est un réglage de compilation de **chaque carte** (`boards/<carte>/sdkconfig.defaults.board`), conformément à la règle 2 d'AGENTS.md. La 7B garde le clair tant que rien n'a été constaté dessus.

## 3. Recommandation et conditions

**Schéma B**, en deux temps :

1. **Sur le PC, sans carte** (faisable tout de suite après ton accord sur le schéma) : option `CONFIG_NVS_ENCRYPTION=y`, schéma HMAC, bloc choisi (`CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID` : la valeur par défaut `-1` est refusée à la compilation, `nvs_sec_provider.c` ligne 109), branchement du noyau (§5) sur `main/boot_info.cpp` et `main/hote_appareil_esp.cpp`. Compiler, ne pas flasher.
2. **Sur le Tab5 ST7121, avec ton accord pour chaque étape** : sauvegarde de la flash, lecture des eFuse, gravure de la clé, flash, constat. Détail et commandes au §6.

**Garde-fous exigés avant tout flash du firmware chiffré** :

- `nvs_flash_init()` compilé avec le chiffrement **génère les clés puis les grave** (schéma HMAC : `esp_efuse_write_key`) dès que la lecture de la configuration de sécurité échoue, quelle qu'en soit la raison, **sans rien demander** (`nvs_api.cpp`, lignes 138-152 ; `nvs_sec_provider.c`, lignes 156-190). Le firmware ne doit donc jamais l'appeler avant que `decider()` ait répondu que la clé existe (`cle_existe`). La gravure reste un acte d'Axel (§6, commande 4).
- Ouvrir la NVS dans un autre mode que celui où elle a été écrite **n'échoue pas : elle efface en silence** les entrées dont la somme de contrôle est fausse (`nvs_page.cpp`, `Page::load`, lu, non constaté). Une partition chiffrée ouverte en clair, ou l'inverse, perd donc tous ses réglages. `decider()` l'interdit (§5).
- `main/boot_info.cpp` (lignes 47-56) **reformate la NVS sur n'importe quelle erreur** d'initialisation. Avec des réglages dedans, c'est une perte de données sur la moindre erreur. Le commentaire du fichier dit lui-même de ne plus le faire « dès qu'elle portera des réglages » : c'est le cas depuis la phase 3. À corriger en même temps que le branchement.
- Après `Fait`, la lecture brute doit prouver qu'il ne reste rien de lisible (§4).

**Si Axel se trompe ou renonce** : compiler sans `CONFIG_NVS_ENCRYPTION` ; la NVS chiffrée est vue comme « illisible » et `decider()` répond `Refuser` (rien n'est ouvert, rien n'est effacé) ; pour repartir à neuf, un effacement de la partition `nvs` suffit et les réglages se ressaisissent (assistant d'installation). L'eFuse gravé reste (1 bloc de moins), la carte n'est pas abîmée. Aucune des manipulations de la 7B n'est nécessaire pour cela.

**Coût du reste** : une couche d'accès à la flash et à la NVS qui implémente `PlateformeNvs` (lecture brute par page, énumération `nvs_entry_find`, effacement physique de la partition), le branchement au démarrage, un écran ou un réglage pour « l'ordre » d'Axel, la mise à jour de la documentation. Non chiffré en temps ; une session de travail est une estimation, pas une mesure.

## 4. Pièges trouvés en lisant le code (valables quel que soit le schéma)

- **Effacer ne purge pas** : `nvs_erase_key` et `nvs_erase_all` (`main/hote_appareil_esp.cpp`, lignes 109 et 125) ne changent que la table d'états de la page ; les 32 octets de l'entrée, mot de passe compris, restent dans la flash jusqu'à ce que la page soit effacée par le ramasse-miettes ou `nvs_flash_erase` (`nvs_storage.cpp`, `nvs_page.cpp`, lu, non constaté sur carte). Une « réinitialisation complète » de l'assistant ne retire donc pas les secrets de la flash. Le test `un_secret_efface_reste_lisible_dans_la_flash` le **simule** sur une page produite par l'outil officiel d'IDF : la table d'états est mise à zéro, comme le fait l'effacement d'après le code, et le mot de passe d'essai est encore dans la page ; le classeur compte alors 11 restes. C'est un modèle de l'effacement tiré de la lecture du code, pas un effacement fait par `nvs_erase_key` sur une flash.
- **Les sauvegardes sont des fichiers secrets** : la sauvegarde complète d'avant le premier flash contient la NVS en clair. À ne jamais mettre dans le dépôt (déjà hors dépôt d'après AGENTS.md) ; avec le schéma B, une sauvegarde faite après la gravure n'a qu'une NVS chiffrée.
- **La NVS d'ESPHome du Tab5** (inerte en `0xf90000`) n'est pas touchée par ce travail et contient les secrets d'ESPHome. À traiter à part si on veut qu'il ne reste rien en clair.
- **Un échec en cours de migration** laisse les valeurs **seulement en mémoire vive** entre la purge et la fin de l'écriture : une coupure de courant à ce moment les perd. On ne perd que des réglages ressaisissables (Wi-Fi, clés, ville), mais il faut le savoir. Une partition de secours rendrait la migration sans risque, au prix d'une table de partitions modifiée donc d'un reflash : non proposé.

## 5. Ce qui est codé et testé (PC seulement)

Fichiers (tous nouveaux, rien d'existant modifié) : `components/core/include/ecran/nvs_securite.hpp`, `test/host/test_nvs_securite.cpp`, `test/host/test_nvs_migration.cpp`, `test/host/mutations_nvs_securite.py` (branché dans `test/host/lancer.py`), `test/host/capturer_nvs.py`, `test/host/donnees/{nvs_clair,nvs_chiffre,attendu_nvs}.inc`, `test/host/donnees/LISEZMOI_nvs.md`.

| Morceau | Ce qu'il fait | Tests |
|---|---|---|
| `bilan_page` / `bilan_partition` / `classer` | Lit une partition **en brut** (sans l'ouvrir, sans effet de bord) : pages vides, formatées ou étrangères, entrées lisibles en clair, entrées illisibles (chiffrées ou abîmées), **restes** effacés mais encore lisibles. Classe : `Vide`, `SansDonnee`, `Claire`, `Illisible`, `Melangee`, `Inconnue`. | 19 sur des pages **de l'outil officiel d'IDF** (`nvs_partition_gen.py generate` et `encrypt`) |
| `decider` | Table « contenu × mode × clé existe × clé essayée × ordre × restes » → `Ouvrir`, `Migrer`, `Purger`, `AttendreCle`, `AttendreOrdre`, `Refuser`. Interdit : ouvrir une partition chiffrée en clair ou l'inverse, ouvrir sans clé (la NVS la graverait), migrer sans ordre, toucher à une partition mélangée ou étrangère. | 9 dont une vérification exhaustive des 192 situations |
| `Instantane`, `migrer`, `reprendre` | Copie tout en mémoire vive, **purge physiquement**, prouve la partition vide en lecture brute, ouvre en chiffré, écrit, valide, relit chaque valeur octet pour octet, vérifie par lecture brute qu'il ne reste rien de lisible, puis efface la copie. Un échec garde la copie (seule exemplaire des valeurs) et `reprendre` repart sans doublon. | 22 avec une doublure de carte qui tombe en panne à chaque étape |

**Résultat** (07/11/2026, PC, clang++ 23.1.5 avec AddressSanitizer + UBSan, mode « complet (compilé + exécuté) ») : 50 tests, 0 échec ; 61 mutations, toutes détectées par le test visé.
**Les nombres attendus ne viennent pas du noyau** : `capturer_nvs.py` déduit du CSV d'essai le nombre d'entrées (4 espaces + 3 textes + 2 entiers + 2 d'un blob = 11 lisibles ; 15 entrées écrites) et le compare à une analyse Python indépendante de la page que l'outil officiel a produite, avant de les écrire dans `attendu_nvs.inc`.

**Ce que ces tests ne prouvent pas** (à répéter) :
- Rien n'a tourné sur une carte : ni la lecture brute d'une vraie partition, ni `nvs_entry_find`, ni l'effacement physique, ni le vrai chiffrement, ni l'eFuse. `PlateformeNvs` n'a **pas d'implémentation** sur ESP-IDF.
- Les pages viennent de l'outil officiel d'IDF 5.5.5 pour une clé jetable ; la NVS réelle d'une carte (compactée, avec des pages pleines, des blobs sur plusieurs pages) n'a jamais été classée. Les pages pleines et les blobs multipages ne sont pas dans les fixtures.
- Une entrée chiffrée dont la somme de contrôle tomberait juste par hasard (une chance sur 2^32 par entrée, de l'ordre de 10^-6 pour une partition entièrement pleine) serait comptée « lisible » : la partition serait classée mélangée et refusée, jamais effacée.
- `cle_essayee` (la clé a réellement déchiffré une entrée) doit venir d'un déchiffrement XTS-AES de la plateforme : **non codé**. Sans lui, `decider` refuse d'ouvrir une partition chiffrée existante.
- L'effacement du tampon de relecture (écriture volatile) n'est pas couvert par un test.
- L'instantané tient 48 valeurs et 4 096 octets ; le pire cas lu dans le code est d'environ 3 Ko (§ commentaire de `OCTETS_MAX`), jamais énuméré sur une carte. S'il dépasse, la migration échoue **sans rien purger**.

## 6. Commandes à valider une par une

À lancer dans PowerShell 5.1, sur le **Tab5 ST7121 de développement seulement**, chacune après ton « oui ». `<PORT>` = le port série de la carte, **retrouvé par la MAC lue sur la puce** (le numéro change avec la prise USB ; une MAC ne s'écrit jamais dans le dépôt). `<PYTHON_IDF>` = le Python de l'ESP-IDF qu'utilise `tools\idf.ps1` (dossier `penvs\5.5.5\Scripts\python.exe` sous le dossier donné par `tools\local.ps1`). Ouvrir ou fermer le port série peut réinitialiser la puce.

| N° | Commande | Effet | Irréversible ? |
|---|---|---|---|
| 0 | `& <PYTHON_IDF> -m espefuse --chip esp32p4 --port <PORT> summary` | **Lecture seule.** Montre la révision réelle, les blocs de clé libres ou utilisés et leur but, `SECURE_BOOT_EN`, `SPI_BOOT_CRYPT_CNT`, JTAG. Vérifie aussi que cet outil (v4.12.0 dans l'IDF) comprend un P4 révision 1.3 : **non vérifié**. | non |
| 1 | `& <PYTHON_IDF> -m esptool --chip esp32p4 --port <PORT> --no-stub read-flash 0 0x1000000 sauvegarde_tab5.bin` puis relecture et comparaison de la taille | Sauvegarde complète (16 Mo), **fichier secret** (NVS en clair) à garder hors du dépôt. Requise par AGENTS.md avant le premier flash d'une carte. | non |
| 2 | `.\tools\idf.ps1 build` (après le branchement du §3, étape 1) | Compile le firmware chiffré. **Ne flashe pas.** Mesure la taille. | non |
| 3 | générer 32 octets au hasard dans un fichier temporaire (`[System.Security.Cryptography.RandomNumberGenerator]` en PowerShell, ou `python -c "import secrets,sys;sys.stdout.buffer.write(secrets.token_bytes(32))" > cle.bin`) | Prépare la clé. **Ne pas garder de copie** (une copie ne servirait qu'à déchiffrer une sauvegarde de réglages ressaisissables). | non |
| 4 | `& <PYTHON_IDF> -m espefuse --chip esp32p4 --port <PORT> burn_key BLOCK_KEY<N> cle.bin HMAC_UP` | **GRAVE la clé HMAC dans le bloc `<N>`** (choisi d'après la commande 0) : lecture et écriture protégées. Demande une confirmation de espefuse. Supprimer `cle.bin` juste après. | **OUI, définitif** |
| 5 | flash du firmware chiffré par le chemin habituel (`.\tools\idf.ps1 -p <PORT> flash`) | Installe le firmware avec la migration. Première ouverture : `decider` doit répondre `Migrer` puis `Fait`. | non (réflashable) |
| 6 | `& <PYTHON_IDF> -m esptool --chip esp32p4 --port <PORT> --no-stub read-flash 0x810000 0x70000 nvs.bin` puis recherche du SSID et du mot de passe d'essai dans `nvs.bin` | **La preuve** : doit ne rien trouver. Plus la ligne du journal série « NVS partition … is encrypted » et un redémarrage qui retrouve le Wi-Fi sans ressaisie. | non |

Les commandes 0 et 1 ne brûlent rien. Les 4 et 5 sont les seules qui engagent la carte. Rien ne doit être lancé sur la 7B avant d'avoir lu sa révision (commande 0) et refait ce tableau pour elle.

## 7. Phrases à changer dans `SECURITY.md` et `docs/publication.md`

Ces deux fichiers se trouvent sur la branche non fusionnée `refs/heads/docs/preparation-publication` (commit `095a45d` à la date de cette note) ; **je n'y ai pas touché**. Les lignes citées sont celles lues sur cette branche le 07/11/2026.

**`SECURITY.md`, ligne du tableau « Le mot de passe Wi-Fi et les réglages… » (version anglaise ligne 19, française ligne 43).**
*Tant que rien n'est gravé* (vrai aujourd'hui, plus précis) :
- EN : « **The Wi-Fi password, the API keys and the settings are stored in the flash without encryption.** NVS encryption, flash encryption and secure boot are not enabled. Whoever holds the board, or a full backup of its flash (`esptool read-flash`), can read them, and, according to the ESP-IDF code, erasing a value in the settings leaves its bytes in the flash until the page is recycled (not yet seen on a board). [docs/recherche/07-chiffrement-nvs.md](docs/recherche/07-chiffrement-nvs.md) compares the schemes and recommends one (keys derived by the HMAC peripheral from one eFuse block): **not done**, burning an eFuse is irreversible and is the owner's decision. »
- FR : « **Le mot de passe Wi-Fi, les clés d'API et les réglages sont gardés dans la flash sans chiffrement.** Le chiffrement de la NVS, celui de la flash et le démarrage sécurisé ne sont pas activés. Qui tient la carte, ou une sauvegarde complète de sa flash (`esptool read-flash`), peut les lire, et, d'après le code d'IDF, effacer un réglage laisse ses octets dans la flash jusqu'au recyclage de la page (pas encore vu sur une carte). [docs/recherche/07-chiffrement-nvs.md](docs/recherche/07-chiffrement-nvs.md) compare les schémas et en recommande un (clés dérivées par le périphérique HMAC d'un bloc d'eFuse) : **pas fait**, graver un eFuse est définitif et la décision revient au propriétaire. »
*Seulement après le constat sur carte* (commande 6 du §6 ; compléter « Vu sur » avec la carte, le commit et la date réels) :
- EN : « **The Wi-Fi password, the API keys and the settings are encrypted in the flash** (NVS encryption, keys derived by the HMAC peripheral from a key burnt in one eFuse block). This stops reading the flash or a backup of it. It does **not** stop someone who can flash their own firmware over USB (secure boot and flash encryption are not enabled), nor what sits in RAM, nor the settings page, which is plain local HTTP. Values written before the encryption was enabled were wiped from the flash by the migration. »
- FR : même phrase en français. La colonne « Vu sur » ne doit citer que ce qui a été constaté.
La phrase « Whoever holds the board can read them with `esptool read-flash` » ne doit pas rester une fois la NVS chiffrée, mais reste vraie pour le reste de la flash.

**`docs/publication.md`, rubrique « Décisions qui restent au propriétaire » (version anglaise ligne 48, française ligne 98).**
*Tant que rien n'est gravé* :
- EN : « **Encrypted NVS or not** (studied on 2026-11-07, [recherche/07-chiffrement-nvs.md](recherche/07-chiffrement-nvs.md): one scheme recommended, one eFuse block; nothing burnt, nothing wired on a board): not a condition of the flip; until it is done, the [SECURITY.md](../SECURITY.md) limits are what a reader is told. »
- FR : « **NVS chiffrée ou non** (étudiée le 07/11/2026, [recherche/07-chiffrement-nvs.md](recherche/07-chiffrement-nvs.md) : un schéma recommandé, un bloc d'eFuse ; rien de gravé, rien de branché sur une carte) : pas une condition de la bascule ; en attendant, ce sont les limites de [SECURITY.md](../SECURITY.md#version-française) qu'on dit au lecteur. »
*Après constat sur carte* : retirer la ligne de cette rubrique (la décision est prise et faite) et la remplacer par la ligne « fait » correspondante.

## 8. Non vérifié

- Tout ce qui touche une carte : HMAC sur un P4 révision 1.3, prise en charge de cette révision par `espefuse` 4.12.0, état réel des eFuse du Tab5, blocs de clé libres, comportement du JTAG USB, ouverture et lecture brute de la vraie partition.
- La révision de la puce de la 7B et ses eFuse.
- La taille du chargeur de démarrage avec le chiffrement de flash ou le démarrage sécurisé.
- Que `nvs_erase_key` laisse bien les octets d'un secret dans la flash (lu dans le code, simulé sur une page fabriquée par l'outil d'IDF, jamais vu sur une carte).
- Le contenu et la taille de la NVS réelle (nombre de valeurs, octets, blobs multipages).
- Que la doc d'IDF décrit bien le comportement des cibles P4 de révision < 3.0 (les passages cités sont écrits pour « les cibles autres que ESP32 », pas pour une révision précise).

## 9. Décisions qui restent à Axel

1. Le schéma : **B** (recommandé), ou rien de gravé pour l'instant et on garde les limites de `SECURITY.md` telles quelles.
2. Si B : l'accord écrit, avec l'heure, pour chaque commande 4 et 5 du §6, et le numéro du bloc de clé (d'après la commande 0).
3. Comment « l'ordre » de chiffrer est donné : le fait de flasher ce firmware, ou un geste à l'écran. Le noyau le prend en entrée (`Situation::ordre`) ; rien n'est choisi.
