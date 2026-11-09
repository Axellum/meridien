# Cache du verre des cartes de l'accueil : mesure sur le Tab5

> **Summary (EN)** — Baking the glass of the hours and five-day cards into the sky image cuts the full main-screen repaint of the Tab5 ST7121 from a 384 ms worst frame (143.9 ms mean per frame) to 329 ms (125.7 ms), about −14 %. The first frame appears about 70 ms later (2288 → ~2358 ms) because the bake runs once at start-up. Internal RAM is unchanged (−64 B), the `taskLVGL` stack margin drops by 128 B (1288 → 1160 B). The PC render bench shows 156/156 identical images.

## Résultats (mesuré)

Journal série de la carte, ligne `affichage sur … ms` (rendu des 3 premières images, 56 zones) ; 6 démarrages par binaire par la commande `redemarrer`, sans Wi-Fi configuré.

| Mesure | Avant (`main` 1069571) | Après (cette PR) | Écart |
| --- | --- | --- | --- |
| Rendu moyen par image (accueil) | 143,9 ms (143,5 à 144,4 ; 4 relevés) | 124,7 ms (121,8 à 126,3 ; 4 relevés propres) | −13 % |
| Rendu maximal d'une image | 384,3 ms (383,6 à 385,3) | 329,4 ms (327,6 à 331,0) | **−14 %** |
| Première image (rétroéclairage allumé) | 2288 ms (2286 à 2289 ; 6 démarrages) | 2358 ms (2356 à 2376 ; 6 démarrages ; un septième à 2511) | **+70 ms** (cuisson du verre au démarrage) |
| Mémoire interne libre à 40 s | 207 727 o | 207 663 o | −64 o |
| Marge minimale de la pile `taskLVGL` (8192 o) | 1288 o | 1160 o | −128 o |
| PSRAM libre à 40 s | ≈ 29,58 Mo | ≈ 29,58 Mo | inchangée (l'image du ciel existait déjà) |

Un relevé « après » sort du lot (16 images, 367 zones, moyenne 297 ms, max 386 ms) : un démarrage où l'écran a été repeint plus souvent ; non expliqué, écarté de la moyenne, conservé ici. Deux démarrages « avant » et un « après » n'ont pas écrit la ligne `affichage sur`.

**Vu sur** : Tab5 révision d'écran ST7121 (carte de développement, port COM11, MAC lue sur la puce, non écrite ici), 09/11/2026. Binaire « avant » `main` `1069571` (3 298 320 o, version lue `jalon-1a-tab5-st7121-354-g10695`), « après » `ea850c2` (3 299 520 o, version lue `jalon-1a-tab5-st7121-355-gea850`), `-O2`, même ESP-IDF 5.5.5.

## Rendu identique (banc du PC)

156 rendus sur 156 identiques pixel pour pixel (scènes `principal`, `principal-themes`, `auto`, fr, 1280×720 et 1024×600), avec des tampons de 50 lignes et avec 28 lignes (le Tab5). Contre-épreuve : cuisson décalée d'un pixel → 104 sur 156 diffèrent.

## Ce qui n'est pas vérifié

- **L'image sur la dalle** : non comparée à l'œil ni par photo ; la preuve est celle du banc.
- La 7B n'a pas été mesurée ; autres langues et 720×1280 non comparés avant/après.
- Série courte (6 démarrages, 4 à 5 relevés par binaire), pas de Wi-Fi ni de météo réelle.
- La carte portait avant cet essai le firmware ESPHome `axellum.tab5-ha-hmi 3.9.1-video-st7121` ; sur demande d'Axel (« 2 »), il a été écrasé **sans sauvegarde** de la flash (bootloader, table des partitions et application de Meridien `main` écrits). Il n'a pas été remis ; la carte porte `main` 1069571.
