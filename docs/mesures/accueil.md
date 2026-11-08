# Accueil refait (courbe des 12 heures, bandeau, cinq jours) — ce qui est mesuré, ce qui ne l'est pas

> **Summary (EN)** — The redone home screen ([ADR-0016](../decisions/0016-accueil-aere-pluie-et-alerte-en-bandeau.md)), written on
> 2026-11-08 and **checked on the PC only**. Static sizes (compiled for the Tab5 and the 7B, ESP-IDF 5.5.5): the internal RAM taken by
> the program image (`DIRAM`) grows by 256 bytes on the Tab5, the firmware by 12.5 KB (+320 bytes of `DIRAM` on the 7B); the LVGL objects stay at 10 and the TinyTTF
> fonts go from 7 to 6. Running RAM, drawing time and looks on the real panel: **not measured** (no board was flashed). Pure core:
> tests compiled and run with AddressSanitizer and UBSan, mutation tests, Clock theme identical to before.

Branche `claude/accueil-aere-18e1ff` (tirée de `main` au commit `99a9f14`). Règle de lecture : tout ce qui n'est pas sous « Vu sur » avec une carte est **non constaté sur carte**.

## 1. Tailles : avant (`main`, `99a9f14`) et après

Compilé avec ESP-IDF 5.5.5, `CMAKE_BUILD_PARALLEL_LEVEL=4`, `idf.py size` (lu dans `build/meridien.map`). « Avant » = le même dépôt à `99a9f14` dans un worktree jetable.

| Carte | Mesure | Avant | Après | Écart |
| --- | --- | --- | --- | --- |
| Tab5 | `DIRAM` (RAM interne prise par l'image : données et code en IRAM) | 144 414 o | 144 670 o | +256 o |
| Tab5 | Flash utilisée | 2 842 806 o | 2 855 348 o | +12 542 o |
| Tab5 | `meridien.bin` | 2 948 304 o | 2 961 104 o | +12 800 o |
| 7B | `DIRAM` | 144 410 o | 144 730 o | +320 o |
| 7B | Flash utilisée | 2 822 748 o | 2 835 166 o | +12 418 o |
| 7B | `meridien.bin` | 2 928 912 o | 2 941 664 o | +12 752 o |

- **Objets LVGL : 10 avant, 10 après** (racine, six zones, bouton Réglages, pastille d'alerte, anneau tactile) : la zone du bandeau remplace celle des tuiles ; lu dans `ui_principal_cout()` (`main/ui_principal.cpp`).
- **Polices TinyTTF : 7 avant, 6 après** (avant : 190, 124, 42, 34, 28, 23 et 19 px de référence ; après : 168, 54, 36, 32, 28 et 24). La plus grande police de la température passe de 124 à 54 px, celle de l'heure de 190 à 168 px : moins de glyphes en cache à la construction. **Non mesuré** sur carte.
- **Mémoire du dessin** : `LV_OS_NONE` et une seule unité de dessin logicielle (`sdkconfig`, `CONFIG_LV_OS_NONE=y`, `CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=1`) : LVGL exécute chaque tâche de dessin au moment où elle est créée (`lv_draw_finalize_task_creation` puis `lv_draw_dispatch`, `src/draw/sw/lv_draw_sw.c`), elles ne s'empilent donc pas. Le dessin de l'accueil ne réserve rien de plus que des tableaux sur la pile (deux à sept points par courbe). C'est une lecture du code de LVGL 9.5.0 : **la RAM libre en marche n'est pas mesurée**.

## 2. Tests du noyau (PC)

- `components/core/include/ecran/accueil.hpp` : 29 tests dans `test/host/test_accueil.cpp`, évalués à la compilation (`static_assert`) puis exécutés ; compilés avec AddressSanitizer et UBSan (clang++ 23.1.5, mode « complet ») : **toute la suite du noyau passe, 1 454 tests exécutés**. Les `static_assert` de `test_accueil.cpp` et `test_visuel_textes.cpp` sont aussi évalués par le g++ 14.2 d'ESP-IDF (mode « syntaxe », la même version que la CI).
- **Mutations** (`python test/host/lancer.py --mutations --seulement test_accueil,test_vigie_bandeau,test_visuel_textes`) : **78 sur 78 détectées** par le test attendu (59 pour l'accueil, 16 pour le bandeau de Vigie, 3 pour les phrases). La passe sur **tout** le dépôt n'a pas été menée à son terme (arrêtée après plus de dix minutes : les autres sujets ne sont pas touchés par ce lot).

## 3. Rendus (banc de rendu du PC)

LVGL 9.5.0, dessin logiciel, RGB565, clang. `.\tools\rendre.ps1 -Scene principal -Variante esquisse -Taille 1280x720 -Langue fr -Instant 2026-11-08T07:45:00Z`.

- Tailles : 1280×720 (Tab5), 1024×600 (7B simulée), 720×1280 (portrait). Langues : français, allemand, turc (les textes les plus longs) ; les sept langues sont couvertes par les tests du noyau et le contrôle des traductions.
- Thèmes : Ciel, Tableau sombre, Clair (nouvelle composition) ; **Horloge : PNG identiques au pixel** à ceux de `99a9f14` sur cinq cas (1280×720 fr ×3, 1024×600 de, 720×1280 tr).
- Bascule de thème à chaud (`tools/rendu/verifier_bascule.py`) : dix paires identiques.
- États : esquisse (Lorient, dimanche 08 h 45, pluie dès 13 h, vent violent 14–22 h), calme, périmé, alerte seule, alerte et deux autres, ville la plus longue en turc et en allemand.

**Écarts avec l'esquisse retenue** (rendu du PC comparé aux PNG `accueil-1280`, `accueil-7b`, `etat-perime`, `etat-calme`) :

1. Pas de pastille « Auto · Météo » (décision d'Axel) ; le bouton Réglages (rond sombre) reste au bout droit de la zone du lieu.
2. La date porte l'année (« dimanche 8 novembre 2026 ») : c'est le format de l'appareil.
3. Le libellé du temps est celui de la table du noyau (« Partiellement nuageux », pas « Éclaircies »).
4. Les repères d'heure s'écrivent « 8h », pas « 08h » (format existant de l'appareil).
5. Les voiles sous les courbes montrent des **bandes** (des bandes étroites d'opacité croissante, LVGL ne trame pas ces dégradés en RGB565) ; la cible est lisse. À juger sur le panneau.
6. L'état périmé : la puce dit « Données de 07:52 · périmées » avec un contour plein (la cible : « hors ligne », tirets) ; l'alerte **officielle** garde ses couleurs (la cible grise une alerte déduite) ; le reste de la météo est grisé comme dans la cible.
7. Les gélules de pluie ont une échelle plancher (6 mm par jour, 3 mm par heure) : en semaine calme, la bruine reste petite.
8. L'état « nuit » n'est pas fait : aucun mécanisme n'existe.
9. En portrait, le vent des jours perd son unité (« 38 » au lieu de « 38 km/h ») et le cumul de la pluie part avant que la phrase soit coupée : la colonne est trop étroite ; l'esquisse ne dessine pas le portrait.
10. Polices : 28 px de référence pour le lieu et le bandeau (l'esquisse : 26), pour n'avoir que six polices.

**Vu sur :** banc de rendu du PC (clang 23.1.5, LVGL 9.5.0), branche `claude/accueil-aere-18e1ff`, 08/11/2026. **Aucune carte** : le rendu sur le panneau (Tab5 ST7121, ST7123, ILI9881C, Waveshare 7B), la lisibilité à 2 m, la vitesse du redessin et la RAM libre en marche ne sont **pas constatés**.
