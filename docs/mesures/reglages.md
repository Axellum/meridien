# Jalon « réglages » — l'appareil sert lui-même la page de réglages (Tab5, mode station)

> **Summary (EN)** — Milestone "settings": the device itself serves the settings page (`web/app`, 9 files, 30 327 bytes gzip, budget 30 720) and the first subset of its API (`docs/api-reglages.md`), over Wi-Fi in station mode only (no access point, no captive portal), plus mDNS `ecran-xxxx.local`. **Built and tested on the PC; not yet observed on any board** — every "on the board" row below is "non vérifié" until the owner's physical trial (section 4) is recorded here. A working document in French; every measured line carries a "Vu sur" line.

Piste « réglages-appareil », branche `claude/reglages-appareil-b3aad6`. Carte visée : M5Stack Tab5 (écran ST7121). La 7B reste **prévue** (aucun essai physique).

Règle de lecture : ce qui est écrit sans la mention « constaté » est une décision ou une lecture de source. Ce qui n'a pas été observé est écrit **non vérifié**. Ni le nom du Wi-Fi, ni le mot de passe, ni l'adresse IP réelle, ni la MAC, ni un code d'appairage ne figurent dans cette page ni dans le dépôt.

## 1. Ce qui est livré

| Élément | Choix | Où |
| --- | --- | --- |
| Serveur | `esp_http_server`, port 80, un seul fil de traitement (pile 16 Ko : la recherche de ville fait sa prise de contact TLS dans ce fil), 5 connexions ouvertes au plus, la plus ancienne fermée au-delà | `main/serveur_web.cpp` |
| Page | `web/app` (9 fichiers) compressée en gzip niveau 9 à la compilation, date fixée à zéro (octets reproductibles), `Content-Encoding: gzip`, `ETag` = empreinte des octets compressés, `If-None-Match` → `304` | `tools/embarquer_web.py`, `main/CMakeLists.txt` |
| API | sous-ensemble V1 de `docs/api-reglages.md` (encadré « État du firmware » de ce document) ; toute la logique est dans le noyau pur, testée sur le PC | `components/core/include/ecran/api_reglages.hpp`, `reglages_valeurs.hpp`, `appairage.hpp`, `http_requete.hpp`, `api_json.hpp` |
| Appairage | code à 6 chiffres tiré à chaque démarrage (`esp_random`), 5 essais puis verrou de 60 s, 4 sessions au plus, comparaison à temps constant ; rien en NVS : un redémarrage change le code et ferme les sessions | `appairage.hpp`, `main/appairage.h` |
| Ville | écrite et lue par `zone_ecrire` / `zone_lire` de `main/meteo_zone.h` (aucune seconde clé de zone) ; seuls la région et le pays (affichage de la page) sont gardés à part, liés au nom et à la position | `main/reglages.cpp` |
| Affichage | langue de l'écran, langue de l'assistant (indices fixes fr0 en1 de2 nl3 es4 it5 tr6), thème, luminosité 5 à 100 % en NVS (espace `reglages`, un blob de 5 octets) ; la luminosité est appliquée tout de suite et au démarrage | `main/reglages.cpp`, `main/main.cpp` |
| Recherche de ville | l'appareil interroge lui-même le géocodage d'Open-Meteo en HTTPS (certificats vérifiés, délai de 8 s, réponse bornée à 16 Ko) ; seule cette adresse est autorisée dans le code | `main/serveur_web.cpp` |
| mDNS | `ecran-xxxx.local` (xxxx = 4 derniers caractères hexadécimaux de l'adresse MAC Wi-Fi), service `_http._tcp` ; composant `espressif/mdns` **1.13.1** (registre, épinglé) | `main/idf_component.yml`, `dependencies.lock` |
| Écran | bandeau provisoire en bas : adresse, nom `.local`, code ; posé sur la couche supérieure de LVGL, ne modifie pas l'écran principal | `main/ui_appairage.cpp`, `main/textes_reglages.h` |
| Hors périmètre (réponse `501`) | changer de Wi-Fi, services STT/LLM/TTS, export/import, mise à jour, retour arrière ; ni point d'accès ni portail captif | encadré de `docs/api-reglages.md` |

Textes du bandeau : `main/textes_reglages.h`, **en français seulement et hors de la table `tr()`** (provisoires ; `ecran-principal` les remplace en plaçant le code dans son écran). Les phrases de l'API, elles, passent toutes par `tr()` dans les sept langues (garde `test/host/controle_i18n.py`).

## 2. Constaté sur le PC et à la compilation

**Vu sur** : PC de compilation (Windows 11), aucune carte, commits `3c864c4` (noyau) et `986ce13` (appareil), 06/11/2026.

| Constat | Résultat |
| --- | --- |
| Tests du noyau sur le PC (clang++ avec AddressSanitizer + UBSan, mode « complet » : compilé **et** exécuté) | 380 tests exécutés, tous réussis (hook de commit) |
| Mutations des modules de la page (appairage, validations, HTTP, API ; `lancer.py --mutations --seulement …`) | 75 mutations, toutes détectées, 0 problème |
| Compilation du firmware (`idf.py build`, ESP-IDF 5.5.5, `espressif/mdns` 1.13.1 téléchargé et compilé) | code de sortie 0 ; `meridien.bin` = 1 811 888 octets sur une partition d'application de 4 Mo |
| Binaire compilé depuis un arbre propre au commit `986ce13` | SHA-256 `7c79fb863b258fd623465c571770685e35dc66a4407dd3ebc5e64cdf4372e917` ; version attendue sur l'appareil (ligne `app_init` du démarrage) : `jalon-1a-tab5-st7121-59-g986ce13`. Écart avec le binaire flashé par la piste météo (`55644aa`) : uniquement les ajouts de ce jalon (diff des sources lu) |
| Poids de la page embarquée | 30 327 octets gzip pour 91 473 octets bruts (9 fichiers) ≤ budget de 30 720 |
| `web/tests/parcours.py` contre le **faux** appareil, après la modification du script | 119 vérifications sur 119 réussies (comportement d'origine inchangé) |

## 3. Réserves connues avant l'essai (sources lues, pas de mesure)

- **mDNS sur le lien Wi-Fi du C6** : la diffusion multi-adresses (224.0.0.251) passe par `esp_hosted` ; qu'elle atteigne le téléphone n'est **pas démontré**. Android résout mal `.local` selon les versions. L'adresse IP affichée à l'écran est le chemin sûr.
- **Recherche de ville** : elle garde l'unique fil du serveur jusqu'à 8 s. Pendant ce temps une autre requête de la page attend. La prise de contact TLS tourne dans le fil du serveur, éventuellement en même temps que celle du client météo : la RAM interne libre pendant les deux n'est pas mesurée.
- **Heure de l'écran** : l'horloge reste fixée sur Europe/Paris (`horloge.cpp`, non touché ici). Choisir une ville d'un autre fuseau enregistre bien la ville et son fuseau, mais l'heure affichée à l'écran ne suit pas encore.
- **Thème et langue de l'écran** sont enregistrés et la langue courante du noyau est mise à jour, mais l'interface actuelle ne les utilise pas encore (textes de l'écran principal en français fixe, palette sombre fixe). Seule la **luminosité** a un effet visible tout de suite.
- **Position du bandeau** : bas de l'écran (156 px de haut, 6 px de marge) ; il ne recouvre pas le bouton « Redémarrer » d'après les coordonnées lues, **non vérifié à l'écran**.
- `validee` (page « Appareil ») vaut toujours `true` : pas de retour arrière automatique.

## 4. Essai physique à faire par Axel (rien n'est constaté tant que ce n'est pas écrit ici)

Prérequis : le binaire de ce commit flashé **sur accord d'Axel pour ce binaire** (la demande est faite par la session coordinatrice) ; le téléphone sur le **même Wi-Fi** que l'écran.

| # | Geste | On regarde | Résultat |
| --- | --- | --- | --- |
| 1 | Laisser l'écran démarrer, attendre l'adresse | un bandeau en bas de l'écran avec « Réglages depuis un téléphone », `http://…`, `ou http://ecran-….local` et « Code : 6 chiffres » | **non vérifié** |
| 2 | Sur le téléphone, taper l'adresse du bandeau | la page « Appairage » s'ouvre (champ du code) | **non vérifié** |
| 3 | Taper le code du bandeau | la page de réglages s'ouvre, ville affichée « Lyon » (défaut) | **non vérifié** |
| 4 | Dans « Ville », taper `Marseille`, choisir « Marseille, Provence-Alpes-Côte d'Azur, France » | la ville affichée devient Marseille, une température apparaît | **non vérifié** |
| 5 | Redémarrer l'écran (bouton « Redémarrer » de l'écran principal) | l'écran revient, le bandeau affiche un **nouveau code** | **non vérifié** |
| 6 | Sur le téléphone, rouvrir l'adresse, taper le nouveau code | la ville affichée est toujours **Marseille** (relue après redémarrage) | **non vérifié** |
| 7 | Choisir « Lyon » comme au point 4 | la ville affichée redevient Lyon | **non vérifié** |
| 8 | Glisser la luminosité à 30 % | l'écran s'assombrit tout de suite | **non vérifié** |

Remarque : l'appareil a déjà « Lyon » par défaut (contrat `meteo_zone.h`) ; c'est pourquoi l'essai passe d'abord par une autre ville pour que l'enregistrement se voie.

Rejouer le parcours automatique contre l'appareil (depuis le PC, même Wi-Fi) :

```powershell
$env:URL_APPAREIL = "http://<adresse du bandeau>"
$env:CODE_APPAIRAGE = "<code du bandeau>"
python web/tests/parcours.py
```

Les étapes que le firmware ne fait pas encore y sont affichées « SAUTÉE » avec leur raison ; elles ne comptent pas comme réussies.
