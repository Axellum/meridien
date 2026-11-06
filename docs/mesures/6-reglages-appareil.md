# Phase 6 — réglages et installation guidée branchés à l'appareil

> **Summary (EN)** — Milestone 6 of ADR-0007, state on 2026-11-07: the settings screens and the 7-step guided install built at milestone 2 (PC rendering only) are now wired to the device. **What was observed is listed in section 4 and nowhere else; everything not listed there is "not verified".**
> - Built: the device host `HoteAppareil` (pure core, `components/core/include/ecran/hote_appareil.hpp`), its ESP-IDF adapters (`main/hote_appareil_esp.cpp`: NVS, Wi-Fi contract, place search download, restart), the glue that opens the screens on a screen of their own (`main/ui_reglages_ouvrir.cpp`) and the stored Wi-Fi reader (`main/wifi_stocke.cpp`).
> - One source of truth (ADR-0007): language, theme, brightness and place go through the same functions as the web page (`reglages_*`, `reglages_zone_*`); only the services (presets, URL, model, voice, keys), the Wi-Fi kept after a success and the install progress byte have their own NVS spaces (section 2). Secrets are never read back.
> - On the PC: 479 core tests under ASan + UBSan, 45 targeted mutations of the host all detected, and a rendering scene that runs the real screens over the real host with a fake memory and device (5 journeys × 7 languages × 3 sizes = 105 renders, no failure; it fails when a fault is injected on purpose).
> - Not wired yet: the real Wi-Fi scan/connect (a stub answers "nothing found"/"impossible" until `jalon-1b` delivers `main/reseau_wifi.h`), "new pairing code" (needs a hook in `main/serveur_web.cpp`), the service tests (honest "not available yet"), the sleep setting (needs `board.h` API from the hardware track), and the call that resumes the guided install at boot (one line in `main/ecran_principal.cpp`, owned by the main-screen track).

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
| Bouchon réseau | `main/bouchons/reseau_wifi*` | compilé tant que `main/reseau_wifi.h` n'existe pas |
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

**Non vérifié** à l'écriture de cette ligne : aucune carte n'a encore exécuté ce code. Cette section est complétée dès qu'un essai a eu lieu.

## 5. Ce qui n'est pas branché, et pourquoi

| Élément | État | Ce qui manque |
| --- | --- | --- |
| Recherche et connexion Wi-Fi réelles | bouchon (répond « rien trouvé », « impossible ») | `main/reseau_wifi.h` et son implémentation par la piste `jalon-1b` (retour arrière à 30 s) |
| Recherche de ville | un téléchargeur à part dans `hote_appareil_esp.cpp` | à remplacer par `geocodage_*` + `VerrouTls` de la piste « page de réglages » dès qu'ils sont fusionnés : un seul TLS à la fois (≈65 Ko de RAM interne chacun) |
| « Nouveau code » d'appairage | journalise « pas branché » | un crochet dans `main/serveur_web.cpp` ou `appairage.*` |
| Test d'un service (STT, LLM, TTS) | dit honnêtement « pas encore disponible » | mémoire pour un second TLS et le client de chaque service |
| Réglage de veille de l'écran | absent | API de veille dans `board.h` (piste matériel du Tab5) |
| Reprise de l'installation au démarrage | `ui_installation_ouvrir_si_necessaire()` existe | un appel, une fois, après le chargement de l'écran principal (`main/ecran_principal.cpp`, autre piste) |
| Page web ouverte pendant que l'écran de réglages l'est | l'écran ne se rafraîchit pas tout seul | il relit au prochain affichage de la page |

## 6. Décisions prises pour Axel (à valider)

1. **Carte déjà en service qui reçoit ce firmware** : sans octet d'installation en mémoire, l'installation guidée s'ouvre une fois (« carte neuve »). « Quitter l'installation » la marque terminée. Alternative écartée : la considérer « terminée » dès qu'un Wi-Fi est déjà là, ce qui cacherait l'installation à quelqu'un qui n'a jamais choisi sa langue ni sa ville.
2. **Mémoire non chiffrée** (section 2).
3. **Touches du clavier à 59 px** (fr, nl, es, it) à 1280×720, au lieu de 72 : décision ouverte depuis le jalon 2.
4. **Un seul téléchargement de recherche de ville**, 8 s, 12 Ko : si la météo parle au même moment, la recherche peut dire « sans connexion » (voir section 5).

## 7. Reproduire

```powershell
. H:\AuxFilsDesIdees\_outils\env_hote.ps1 -Sanitizers
python test/host/lancer.py
.\tools\rendre.ps1 -Scene hote_appareil -Langue fr,en,de,tr -Taille 1280x720,720x1280
.\tools\idf.ps1 build
```
