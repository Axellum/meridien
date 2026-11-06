# Phase 2 — réglages et installation guidée à l'écran (rendu PC, pas encore la carte)

> **Summary (EN)** — Milestone 2 of ADR-0007, state on 2026-11-06: every setting can be changed on the device screen and a 7-step guided first install can be replayed. **Everything below was observed on the PC rendering bench (software LVGL 9.5.0, `tools/rendre.ps1`), not on a board**: no flash, no touch, no real Wi-Fi, no persistent memory yet.
> - Built: a pure core (settings registry, install progression in one byte, Wi-Fi change tracking, on-screen keyboard layouts for 7 languages including Turkish), the LVGL screens (`main/ui_reglages.cpp`, `ui_installation.cpp`, `ui_clavier.cpp`, `ui_commun.cpp`) with an in-memory host (`main/hote_memoire.cpp`), 53 new texts in 7 languages (translations written by an AI, not reviewed by a native speaker), and render scenes (installation, settings, 12 scripted "virtual finger" journeys).
> - Observed on the PC: the 16 installation screens and the 16 settings screens are legible in French, German and Turkish at 1280×720, 720×1280 and 1024×600; the 12 scripted journeys pass in 7 languages and 3 sizes (252 renders).
> - Not done: flash and any board test, the persistent-memory (NVS) adapter, the real Wi-Fi scan/connect/rollback (interface proposed in `main/bouchons/reseau_wifi.h`, to be implemented in `main/reseau.*`), a QR code on the phone step (address and code shown as text), visual polish (the author will have the look redone).

Branche `claude/reglages-ecran-aea4e2`, tirée de `jalon-1b` (165f404). **Aucune carte n'a été flashée pour ce jalon.**

Règle de lecture : tout ce qui est écrit sans la mention « constaté sur le PC » est une décision ou une lecture de source. Ce qui n'a pas été observé est écrit **non vérifié**. Les données des rendus (réseaux « Maison_5G », ville de Lyon, mot de passe, clé, adresse 192.168.1.52, code 482913) sont inventées pour les images.

## 1. Ce qui est construit

| Élément | Où | Remarque |
| --- | --- | --- |
| Registre des réglages (sections, pages, onglets, bornes, validation) | `components/core/include/ecran/registre_reglages.hpp`, `ecrans_reglages.hpp`, `presets_services.hpp` | pur, testé sur le PC ; les mêmes bornes que `docs/api-reglages.md` |
| Progression de l'installation, octet unique de mémoire permanente | `installation.hpp` | 7 étapes ; bits 0 à 3 : étape, bit 4 : « refaire » ; seules la langue et le Wi-Fi bloquent la suite |
| Changement de Wi-Fi (délai de 30 s, causes, « l'ancien réseau est conservé ») | `changement_wifi.hpp` | l'écran suit le délai ; le retour arrière réel est à `main/reseau.*` |
| Clavier à l'écran, 7 langues (le turc distingue i pointé et i sans point) | `clavier.hpp`, `main/ui_clavier.cpp` | |
| Écrans : liste, sections, onglets de l'assistant, éditeurs, liste Wi-Fi, connexion, recherche de ville, étapes | `main/ui_reglages.cpp`, `ui_installation.cpp`, `ui_commun.cpp` | LVGL pur, aucun include ESP-IDF, tailles tirées de la taille logique (pas de 1280/720 écrit) |
| Hôte des réglages (interface) et hôte en mémoire vive | `main/hote_reglages.h`, `hote_memoire.*` | l'adaptateur de mémoire permanente reste à écrire |
| Textes | `i18n/*.yaml`, `i18n_donnees.inc`, `web/app/lang/*.json` | 53 clés ajoutées dans 6 langues ; **traductions écrites par une IA** |
| Interface réseau demandée à `reseau.*` | `main/bouchons/reseau_wifi.h` + bouchon | proposition, pas du code de `reseau.cpp` |
| Scènes de rendu | `tools/rendu/scenes/scene_installation.cpp`, `scene_reglages.cpp`, `scene_parcours.cpp`, `parcours_outils.h` | le banc lui-même (`tools/rendre.ps1`) est celui de la piste « rendu-pc » |

## 2. Critères de « fait » (phase 2, ADR-0007)

| Critère | État | Preuve |
| --- | --- | --- |
| Tous les réglages modifiables à l'écran | **constaté sur le PC** (rendu), pas sur la carte | planches `docs/captures/ecran/planche-reglages-*.png` (à régénérer par la commande ci-dessous) |
| Séquence d'installation en 7 étapes, reprise après redémarrage | **constaté sur le PC** (parcours « installation-complete », « reprise-apres-redemarrage ») ; redémarrage réel **non vérifié** | le parcours écrit l'étape à chaque « Continuer » et la relit |
| « Refaire l'installation » distinct de « Réinitialiser » | **constaté sur le PC** (parcours « refaire-puis-quitter », « reinitialisation-deux-gestes ») | deux gestes, retombée au bout de 5 s |
| Retour arrière du Wi-Fi au bout de 30 s | écran **constaté sur le PC** ; retour arrière réel **non fait** (dépend de `reseau.*`) | parcours « wifi-retour-arriere » |
| Allemand (le plus long) et turc lisibles | **constaté sur le PC** à 3 tailles | planches `planche-installation-de-*`, `planche-reglages-de-*`, `…-tr-*` |
| Sur la carte : toucher, vitesse, mémoire | **non vérifié** | aucun flash |

Reproduire (PowerShell, depuis la racine du dépôt) :

```powershell
.\tools\rendre.ps1 -Scene installation -Langue fr,de,tr -Taille 1280x720,720x1280 -Planche
.\tools\rendre.ps1 -Scene reglages -Langue fr,de,tr -Taille 1280x720,720x1280,1024x600 -Planche
.\tools\rendre.ps1 -Scene parcours -Langue fr,en,de,nl,es,it,tr -Taille 1280x720,720x1280,1024x600
```

Un parcours qui échoue s'arrête avec un code de sortie non nul et dit quelle vérification a échoué.

**Vu sur** : banc de rendu PC (LVGL 9.5.0 logiciel, clang llvm-mingw), pas une carte ; branche `claude/reglages-ecran-aea4e2`, commits `788db94` (écrans) et `3a6695d` (scènes), 06/11/2026. Tests du noyau sur le PC au même commit : 333 tests exécutés (ASan + UBSan), 108 mutations des réglages sur 108 détectées.

## 3. Limites connues

- **Aucun essai sur la carte** : le rendu est du LVGL logiciel sur PC, le doigt est virtuel (`lv_obj_send_event`). Il ne prouve ni le toucher réel, ni la vitesse, ni la mémoire, ni la lisibilité à la distance d'usage.
- Le **thème « Auto »** est provisoirement sombre (le clair existe dans les rôles de `main/palette_reglages.h`, sa règle jour/nuit n'est pas décidée).
- **Pas de QR code** à l'étape « téléphone » : l'adresse et le code d'appairage sont affichés en texte.
- Les phrases de résultat de l'hôte en mémoire (« Le service répond en 0,4 s. ») sont en français seulement ; l'hôte réel les donnera dans la langue de l'écran.
- La recherche de ville laisse peu de place aux résultats au-dessus du clavier en paysage (le clavier se masque après la recherche).
- Traductions en 6 langues **écrites par une IA**, à relire par des locuteurs.
- Le look sera refait plus tard : le but ici est fonctionnel et lisible.
