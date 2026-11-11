# Phase 6 — réglages et installation guidée branchés à l'appareil

> **Summary (EN)** — Milestone 6 of ADR-0007, state on 2026-11-07: the settings screens and the 7-step guided install built at milestone 2 (PC rendering only) are now wired to the device. **What was observed is listed in section 4 and nowhere else; everything not listed there is "not verified".**
> - Built: the device host `HoteAppareil` (pure core, `components/core/include/ecran/hote_appareil.hpp`), its ESP-IDF adapters (`main/hote_appareil_esp.cpp`: NVS, Wi-Fi contract, place search download, restart), the glue that opens the screens on a screen of their own (`main/ui_reglages_ouvrir.cpp`) and the stored Wi-Fi reader (`main/wifi_stocke.cpp`).
> - One source of truth (ADR-0007): language, theme, brightness and place go through the same functions as the web page (`reglages_*`, `reglages_zone_*`); only the services (presets, URL, model, voice, keys), the Wi-Fi kept after a success and the install progress byte have their own NVS spaces (section 2). Secrets are never read back.
> - On the PC: 479 core tests under ASan + UBSan, 45 targeted mutations of the host all detected, and a rendering scene that runs the real screens over the real host with a fake memory and device (5 journeys × 7 languages × 3 sizes = 105 renders, no failure; it fails when a fault is injected on purpose).
> - On the board (Tab5 ST7121, commit `176de0d`, 2026-11-07, **without a finger**: a virtual pointer replays gestures through LVGL's own object search, section 4): 29 checks, 0 failures. The screens open on a screen of their own and close back to the main screen four times in a row; the brightness slider writes the value the web page reads; after a **real software restart** the brightness, a service text and the install step are back; the install resumes at step 3 and does not reopen once finished.
> - **Not verified on the board**: a real finger, the look of the screen, the keyboard keys one by one, the real Wi-Fi, the city search, a power cut. **Internal RAM is almost zero (20 bytes at the lowest) while the weather's TLS handshake runs and the settings are open** (section 4.3); the TLS fix belongs to the `jalon-1b` track and is not verified here.
> - Not wired yet: the real Wi-Fi scan/connect (a stub answers "nothing found"/"impossible" until `jalon-1b` delivers `main/reseau_wifi.h`), "new pairing code" (needs a hook in `main/serveur_web.cpp`), the service tests (honest "not available yet"), the sleep setting (the API exists on the hardware track's branch, not merged), and the call that resumes the guided install at boot (written on the main-screen track's branch, not merged, not verified with this glue).

Branche `claude/reglages-ecran-aea4e2`. Règle de lecture : ce qui est **constaté** est dans la section 4, avec carte, commit et date (« Vu sur »). Le reste est écrit **non vérifié**.

## 1. Ce qui est construit

| Élément | Où | Remarque |
| --- | --- | --- |
| Interface de l'hôte des réglages | `components/core/include/ecran/hote_reglages.hpp` | déplacée de `main/` pour être testée sur le PC avec l'hôte de l'appareil |
| Hôte de l'appareil (toute la logique) | `components/core/include/ecran/hote_appareil.hpp` | `HoteAppareil` au-dessus de deux interfaces : `Magasin` (mémoire permanente) et `PortsAppareil` (affichage, ville, météo, Wi-Fi, téléchargement, redémarrage) ; testé sur le PC |
| Lecture des lieux du géocodage | `components/core/include/ecran/lieux_geocodage.hpp` | réponse d'Open-Meteo → libellés courts + zone ; testé sur des réponses écrites à la main |
| Adaptateurs ESP-IDF | `main/hote_appareil_esp.cpp` | NVS (rien d'autre que l'espace et la clé au journal), téléchargement HTTPS d'une seule adresse autorisée (tampon de 12 Ko en PSRAM, 8 s), Wi-Fi par `reseau_wifi.h`, version, redémarrage logiciel |
| Colle : ouvrir les écrans | `main/ui_reglages_ouvrir.cpp` | écran LVGL à part ; « Retour » au premier écran recharge l'écran d'avant et supprime celui-ci avec sa pile |
| Wi-Fi retenu | `main/wifi_stocke.cpp` | relit l'espace `wifi` pour `reseau.*` (jalon 1b) au prochain démarrage |
| Bouchon réseau | `main/bouchons/reseau_wifi*` | compilé tant que `main/reseau_wifi.h` n'existe pas ; *supprimé le 11/11/2026 (le vrai fichier existe, le bouchon n'était plus compilé)* |
| Scène de rendu sur l'hôte de l'appareil | `tools/rendu/scenes/scene_hote_appareil.cpp`, `test/host/hote_appareil_faux.hpp` | les vrais écrans au-dessus de la vraie logique, mémoire et appareil factices |

## 2. Mémoire permanente (NVS) : qui garde quoi

Une seule source par valeur (ADR-0007). Une clé NVS fait 15 octets au plus.

| Valeur | Où elle vit | Qui la lit |
| --- | --- | --- |
| Langue de l'écran et de l'assistant, thème, luminosité | `reglages.*` (les mêmes fonctions que la page web) | écran, page web, démarrage |
| Ville (nom, région, pays, fuseau, latitude, longitude) | `meteo_zone.*` via `reglages_zone_*` | écran, page web, météo |
| Presets et réglages des services (adresse, modèle, voix) | espace `services`, une clé par identifiant du registre (`llm.url`…) ; preset = entier (−1 : absent), texte = chaîne | écran, `/api/services` de la page (à brancher par la piste « page de réglages ») |
| Clés d'accès (`stt.cle`, `llm.cle`, `tts.cle`) | espace `services`, chaîne | jamais relues vers l'écran ni la page : seul « définie / non définie » ressort |
| Wi-Fi retenu après un succès | espace `wifi`, clés `ssid` et `mdp` | `reseau.*` au démarrage (`wifi_stocke_lire`) |
| Étape de l'installation guidée | espace `installation`, clé `etape` : un octet (bits 0 à 3 : étape de 1 à 7, ou 8 pour « terminée » ; bit 4 : « refaire ») ; un octet absent ou abîmé donne « carte neuve », jamais « terminée » | écran |

Le Wi-Fi n'est écrit **que** sur le résultat « connecté » : un mot de passe refusé, un délai dépassé ou un abandon n'écrivent rien, l'ancien réseau reste.
La NVS n'est **pas chiffrée** : le mot de passe Wi-Fi et les clés d'accès sont en clair dans la flash. Décision de sécurité à confirmer par Axel (chiffrement de la NVS = clé dans la flash ou eFuse, hors de ce jalon).

## 3. Sur le PC

| Quoi | Résultat | Commande |
| --- | --- | --- |
| Tests du noyau (ASan + UBSan, mode « complet ») | 479 exécutés, 0 échec (commit `ae4d9bb`) | `python test/host/lancer.py` |
| Mutations de l'hôte de l'appareil | 45 sur 45 détectées (les tests de l'hôte ont des méthodes virtuelles : l'exécutable muté est exécuté, pas seulement compilé) | `python test/host/lancer.py --mutations --seulement hote_appareil` |
| Scène de rendu sur l'hôte de l'appareil | 5 parcours × 7 langues × 3 tailles (1280×720, 720×1280, 1024×600) = 105 rendus, 0 échec ; avec une panne volontaire dans l'hôte (Wi-Fi retenu aussi sur un refus), le parcours « Wi-Fi retenu après succès » s'arrête en échec (code 1) | `.\tools\rendre.ps1 -Scene hote_appareil` |

Ce que ces essais prouvent : l'écran écrit par la bonne porte, sans copie ; un secret ne ressort pas ; un Wi-Fi n'est retenu qu'après un succès ; l'installation reprend après un « redémarrage » de l'hôte.
Ce qu'ils ne prouvent pas : la NVS, le Wi-Fi du C6, le serveur de géocodage et le toucher réels.

## 4. Sur la carte

**Vu sur** : Tab5 révision d'écran **ST7121**, binaire d'essai compilé au commit `176de0d` (SHA-256 `C09A6CC7…8D6F2B`, 1 914 496 octets), le 07/11/2026 de 02:19 à 02:21, flashé en **app seule** à `0x10000` par `esptool` 5.3.1 (« Hash of data verified »), carte retrouvée par sa MAC lue sur la puce (non publiée). Puis le binaire normal du même commit (SHA-256 `E7579259…07DD00D`) a été remis et vérifié ; sa version lue sur l'appareil, au journal de démarrage : `App version: jalon-1a-tab5-st7121-136-g176de` (la carte tourne donc avec ce binaire normal, réglages inclus, écran principal de ma branche).

**L'essai est SANS DOIGT.** Un pointeur virtuel rejoue les gestes dans le vrai LVGL de la carte, par la même recherche d'objet que le tactile (`lv_indev_search_obj`), au-dessus de la vraie NVS, du vrai rétroéclairage et d'un vrai redémarrage logiciel. Le panneau tactile n'est pas utilisé. Le code est `main/autotest_reglages.cpp`, compilé seulement avec `-DMERIDIEN_AUTOTEST_REGLAGES=ON` (désactivé par défaut, jamais dans le binaire normal).

### 4.1 Constaté (29 vérifications, 0 échec, sur le journal série)

| Constat | Ligne du journal |
| --- | --- |
| Les réglages s'ouvrent sur un écran à part ; la liste montre les 6 sections avec la valeur de chacune (« Lyon », « Français », « Auto »…) | `ECRAN liste des réglages (12 textes)` |
| Un toucher au centre de « Affichage » (1248 × 83 px) arrive à la rangée : appui, relâchement et clic reçus (1, 1, 1) ; la pile passe à la section n° 3 ; la page montre le thème et la luminosité | `GESTE sur « Affichage »…`, `PILE Affichage : …a=3` |
| Un toucher à 40 % de la course du curseur écrit la luminosité (75 % → 43 %) ; l'hôte relit la même valeur que la source de la page web | `la luminosité est passée de 75 % à 43 %` |
| « Retour » (145 × 72 px) revient à la liste, puis à l'écran principal ; quatre ouvertures/fermetures de suite : l'écran principal revient chaque fois | `PILE Retour : vide` ×4 |
| Un texte validé dans l'éditeur (modèle du service LLM) est gardé par l'hôte ; l'étape 3 de l'installation est écrite | `le texte saisi est gardé par l'hôte`, `l'étape 3 est écrite` |
| Le bouton « Redémarrer » (1216 × 72 px) redémarre la carte (reset logiciel, cause `SW_SYS_RESET`) | `rst:0x3 (SW_SYS_RESET)` |
| **Après ce vrai redémarrage** : luminosité (43 %), texte du service et étape 3 de l'installation sont retrouvés | `après le redémarrage, … retrouvé(e)` ×3 |
| L'installation reprend sur un écran à part, à « Étape 3 sur 7 » ; « Retour » écrit l'étape 2 | `installation reprise (7 textes)`, `« Retour » écrit l'étape 2` |
| Une installation marquée « terminée » ne se rouvre pas après un redémarrage ; luminosité, texte et progression sont remis comme avant l'essai | `l'installation terminée ne se rouvre pas` |
| Pas de fuite de mémoire interne sur trois ouvertures/fermetures de plus : 7 027 octets libres après la première fermeture, 7 307 après la quatrième (la mesure est bruitée par le TLS de la météo, voir 4.3 ; elle ne baisse pas) | `pas de fuite de mémoire interne…` |

### 4.2 Non constaté

- **Le doigt.** Le toucher réel, la taille physique des cibles sous le doigt et le défilement au doigt ne sont pas essayés : à faire par Axel.
- **Le rendu à l'œil.** Le journal liste les textes affichés, pas les pixels (lisibilité, couleurs, icônes MDI, retours à la ligne).
- **Les touches du clavier une à une.** L'essai pose le texte dans le champ puis valide (`ui_simuler_saisie`) ; les touches ne sont pas touchées.
- **Le Wi-Fi réel** (bouchon), **la recherche de ville** (le téléchargeur n'a pas été lancé), **le test d'un service**, **la veille**, **le « Nouveau code »**.
- **Une coupure de courant** (seul un redémarrage logiciel a eu lieu) et **le chiffrement de la NVS** (absent).
- **L'ouverture des réglages pendant le TLS de la météo** : voir 4.3.

### 4.3 Mémoire interne : ce que l'essai a montré, et ce qui reste ouvert

| Mesure (RAM interne libre) | Valeur | Essai |
| --- | --- | --- |
| Construire la liste (objets et styles LVGL) | −8,7 à −8,9 Ko | `8f6c28d`, `176de0d` |
| Premier dessin de la liste (cache de glyphes et divers) | −12,6 Ko interne, **−21,9 Ko en PSRAM** (les bitmaps de glyphes, redirigés par `e686310`) | `8f6c28d` (97 Ko libres avant) |
| Total d'une ouverture de la liste | **≈ 21,5 Ko de mémoire interne** + ≈ 22 Ko de PSRAM | `8f6c28d` |
| Même ouverture quand la poignée de main TLS de la météo est en cours (≈ 13 à 18 s après le démarrage) | RAM interne libre à **535 octets**, plus gros bloc **160 octets**, minimum **20 octets** ; erreurs `mdns_send: Cannot allocate memory` | `176de0d` (27,7 Ko libres avant) |
| Binaire **normal**, réglages jamais ouverts | 96 787 o libres juste avant la requête de la météo (13 s, journal `meteo`), puis `esp-aes: Failed to allocate memory for the array of DMA descriptors` et `meteo: essai raté (HTTP 0)` ; 37 Ko libres à 39 s | `7b435eb`, `8f6c28d`, `176de0d` |

Lecture, avec ses limites : (1) le gros consommateur de mémoire interne au démarrage est la poignée de main TLS de la météo (≈ 65 à 76 Ko), pas l'écran ; (2) l'écran de réglages prend lui ≈ 21,5 Ko de mémoire interne, ce qui suffit à la vider pendant que le TLS est en cours ; (3) sans l'écran, le TLS échoue déjà à cause de la mémoire interne, sur cette branche. Aucun plantage ni redémarrage inattendu n'a été vu pendant les essais (l'essai va à son terme), mais **la marge est à zéro** tant que le TLS tient sa mémoire en interne. Le correctif du TLS (mbedTLS en PSRAM) est le commit `4f6bd0a` de la piste `jalon-1b`, non fusionné ici : **non vérifié** qu'il suffise. Mesurer après la fusion : même essai, mêmes lignes `MEMOIRE`.

Ce que ce jalon a fait pour la mémoire : `e686310` envoie les bitmaps de glyphes de **toutes** les polices TinyTTF en PSRAM (effet global, voulu) ; sa vraie place est l'initialisation de la carte, avant le premier dessin (à proposer au propriétaire). Le message de ce commit dit « ne vide plus la mémoire interne » : c'est plus fort que ce qui est mesuré ; la mesure ci-dessus fait foi.

## 5. Ce qui n'est pas branché, et pourquoi

| Élément | État | Ce qui manque |
| --- | --- | --- |
| Recherche et connexion Wi-Fi réelles | bouchon (répond « rien trouvé », « impossible ») | `main/reseau_wifi.h` et son implémentation par la piste `jalon-1b` (retour arrière à 30 s) |
| Recherche de ville | un téléchargeur à part dans `hote_appareil_esp.cpp` | à remplacer par `geocodage_*` + `VerrouTls` de la piste « page de réglages » dès qu'ils sont fusionnés : un seul TLS à la fois (≈65 Ko de RAM interne chacun) |
| « Nouveau code » d'appairage | journalise « pas branché » | un crochet dans `main/serveur_web.cpp` ou `appairage.*` |
| Test d'un service (STT, LLM, TTS) | dit honnêtement « pas encore disponible » | mémoire pour un second TLS et le client de chaque service |
| Réglage de veille de l'écran | absent | l'API existe (`board_sleep_configure()`, `board_sleep_set_local_time_source()`, commit `9a1a58a` de la branche `claude/materiel-tab5-d5c11a`, non fusionnée) ; elle ne se relit pas côté carte, la valeur de référence est celle de la NVS : rappeler `board_sleep_configure()` au démarrage puis à chaque changement. À brancher après la fusion |
| Reprise de l'installation au démarrage | `ui_installation_ouvrir_si_necessaire()` existe et marche (essai, 4.1) | l'appel est écrit par la piste de l'écran principal (commit `cd362eb` de `claude/ecran-visuel-ba722a`, une fois, juste après `lv_screen_load()`), non fusionné ; **non vérifié** avec cette colle réelle. À la fusion, supprimer `main/bouchons/ui_reglages.h` et `ui_reglages_bouchon.cpp` de cette piste |
| Page web ouverte pendant que l'écran de réglages l'est | l'écran ne se rafraîchit pas tout seul | il relit au prochain affichage de la page |

## 6. Décisions prises pour Axel (à valider)

1. **Carte déjà en service qui reçoit ce firmware** : sans octet d'installation en mémoire, l'installation guidée s'ouvre une fois (« carte neuve »). « Quitter l'installation » la marque terminée. Alternative écartée : la considérer « terminée » dès qu'un Wi-Fi est déjà là, ce qui cacherait l'installation à quelqu'un qui n'a jamais choisi sa langue ni sa ville.
2. **Mémoire non chiffrée** (section 2).
3. **Touches du clavier à 59 px** (fr, nl, es, it) à 1280×720, au lieu de 72 : décision ouverte depuis le jalon 2.
4. **Un seul téléchargement de recherche de ville**, 8 s, 12 Ko : si la météo parle au même moment, la recherche peut dire « sans connexion » (voir section 5).
5. **Glyphes en PSRAM pour toutes les polices** (`e686310`) : effet global, voulu, posé à la première ouverture des réglages ; sa vraie place est l'initialisation de la carte, avant le premier dessin (à proposer au propriétaire, pas fait ici).
6. **Marge de mémoire interne nulle pendant le TLS de la météo** (section 4.3) : je n'ai pas touché au TLS, à `sdkconfig` ni à l'allocation de LVGL (pas ma voie). À décider avec les pistes `jalon-1b` / matériel : mbedTLS en PSRAM (`4f6bd0a`), puis, si la marge reste trop juste, tous les objets LVGL en PSRAM (allocation personnalisée) ou le seuil `SPIRAM_MALLOC_ALWAYSINTERNAL`. À mesurer avant de choisir.
7. **Plage de luminosité 5 à 100 %** : avec la courbe perçue du matériel (plancher 2 %), les niveaux 5 à 10 % donnent presque la même luminosité (21 à 23 ‰, mesure de la piste matériel). Je n'ai pas changé la plage ; relever le minimum du curseur est un choix de goût à valider sur l'écran réel.
8. **L'essai de la carte est sans doigt** : un binaire d'essai (option désactivée par défaut) rejoue les gestes. Il se rejoue à chaque binaire neuf et remet l'état d'avant (luminosité, texte, progression d'installation). Il ne remplace pas le toucher d'Axel avec son doigt sur les cibles de 72 px.

## 7. Reproduire

```powershell
. .\tools\trouver_outils.ps1; . "$(Trouver-Outils)\env_hote.ps1" -Sanitizers
python test/host/lancer.py
.\tools\rendre.ps1 -Scene hote_appareil -Langue fr,en,de,tr -Taille 1280x720,720x1280
.\tools\idf.ps1 build
# l'essai sur la carte (binaire d'essai, jamais le binaire normal) : à lancer sous le verrou de la carte, flash de l'app seule
.\tools\idf.ps1 -DMERIDIEN_AUTOTEST_REGLAGES=ON build
# puis flasher build\meridien.bin à 0x10000 et lire le journal série : une ligne « AUTOTEST OK|ECHEC » par vérification,
# « AUTOTEST FIN : N vérification(s), M échec(s) » à la fin (trois phases séparées par de vrais redémarrages, ~1 min)
```

Piège de l'essai : LVGL tourne le point lu par un pointeur selon la rotation de l'écran (`lv_indev.c`, `lv_display_rotate_point`) parce que les pilotes tactiles donnent des coordonnées brutes ; le doigt virtuel doit donc donner l'inverse (`vers_brut()` dans `main/autotest_reglages.cpp`). Sans cela, « Affichage » ouvre « Appareil » et « Retour » ne touche rien (deux premiers essais du 07/11/2026, `7b435eb` et `8f6c28d`) : voir [dépannage](../depannage.md).
