# Page Détails — ce qui est constaté sur le PC, ce qui reste à voir sur une carte

> **Summary (EN)** — The "Details" page (the seven quantities removed from the home screen, ADR-0020) **seen on PC renders and
> compiled for both boards only**: 49 core tests with AddressSanitizer and UBSan, 70 mutations all caught, 20 LVGL render variants
> looked at by eye (sheets in 1280x720, 1024x600, 720x1280; German and Turkish in 1024x600; three themes), ESP-IDF 5.5.5 build for the Tab5 and the 7B.
> **Not seen on a board**: legibility at 2 m, the touch on "Close", free internal RAM while the page is open, drawing time.

**Vu sur** : PC d'Axel (Windows 11), clang++ 23.1.5 (tests), ESP-IDF 5.5.5 (compilation), LVGL 9.5.0 (rendu), branche `claude/page-details-6ccd6f`, 08/11/2026 ; **aucune carte** (ni Tab5 ni 7B), rien flashé. La 7B reste « prévue » : le rendu 1024×600 ne simule que sa taille.

## Constaté

| Quoi | Résultat | Comment |
|---|---|---|
| Tests du noyau (`ecran/grandeurs.hpp`) | 49 tests, 0 échec | `python test/host/lancer.py --seulement test_grandeurs`, mode « complet (compilé + exécuté) », AddressSanitizer + UBSan |
| Mutations | 70 mutations, 0 problème | `... --seulement test_grandeurs --mutations` : chacune est détectée par le test nommé |
| Rendus | 20 variantes regardées : planches français en 1280×720, 1024×600 et 720×1280 ; allemand et turc en 1024×600 | `.\tools\rendre.ps1 -Scene details` ; trois planches gardées dans `docs/captures/ecran/` |
| Compilation Tab5 et 7B | réussies : Tab5 `meridien.bin` 0x2d2ed0 octets (29 % libre), 7B 0x2ce2e0 octets (30 % libre) | `.\tools\idf.ps1 build` ; 7B : `"-DBOARD=waveshare-7b" set-target esp32p4` puis `build` |
| Mémoire statique de la page | 54 octets de bss dans les deux objets (36 + 18) ; le reste est en PSRAM pendant l'ouverture | `riscv32-esp-elf-size` sur les `.obj` ; **effet sur le binaire final non mesuré** (rien ne référence la page) |

## Non constaté

- La lisibilité à 2 m sur l'écran réel (valeurs à 76 de référence, 61 réels sur la 7B ; textes à 24 réels au moins).
- Le temps de dessin de la page ouverte, la mémoire interne libre pendant l'ouverture et après la fermeture.
- Un doigt sur « Fermer » (144 × 64 pixels réels sur la 7B, 180 × 80 sur le Tab5).
- Les textes des sept langues sur l'écran réel (non relus par un locuteur natif).
