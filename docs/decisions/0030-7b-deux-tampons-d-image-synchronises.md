# ADR-0030 — Waveshare 7B : deux tampons d'image, bascule à la fin du balayage

> **English summary.** On the Waveshare 7B (rotation 0, the author's screen), the panel now has **two frame buffers in PSRAM**
> instead of one. LVGL draws **directly** into the one the panel is not scanning (`direct_mode`), and at the end of a render pass
> esp_lvgl_port hands the whole frame to the DSI driver and waits for the end of the current scan (`avoid_tearing`): the driver
> switches buffers only between two scans, so a full repaint of the screen (a change of sky, the glass baked again: 0.36 to 0.5 s)
> is never seen half drawn, sweeping down the panel. LVGL 9.5 copies the changed areas to the other buffer on the next pass. With a
> rotation of 180 degrees the software rotation (PPA) writes into its own output buffer, which direct mode cannot use: that build
> keeps one frame buffer and the internal-RAM bands, as before; `-DWS7B_SYNCHRO_IMAGE=0` gives the old path for a comparison.
> Cost: 1.2 MB more PSRAM (2,457,600 bytes for both buffers); the two 51,200-byte internal-RAM draw bands are no longer allocated.
> The same change makes the sky calmer (it follows a new mood only when two weather readings in a row agree) and the glass of the
> cards repaints only its own rectangles. Status: **proposed**; **not seen on the board** when written. It is aimed at the brief
> white flash of the 7B, whose cause is still not proven ([ADR-0028](0028-7b-code-en-psram-et-cache-l2-256-ko.md)).

- **Statut** : **proposée**, écrite le 25/11/2026, à la demande d'Axel (« le deux m'a l'air d'être une bonne optimisation »). Non
  vue sur la carte à l'écriture : compilée pour la 7B, tests du noyau sur le PC.
- **Vu sur** : rien encore. À voir : l'image (aucun balayage visible pendant un repeint complet), le temps d'un passage de rendu,
  la RAM interne et la PSRAM libres au démarrage, et le flash blanc sur plusieurs heures.

## Contexte

Le 25/11/2026, le bref flash blanc de la 7B est revu alors que les réglages de l'[ADR-0028](0028-7b-code-en-psram-et-cache-l2-256-ko.md)
sont en place : ils ne le corrigent pas. Axel demande aussi pourquoi « on perd le fond » à une mise à jour, et que seul ce qui
change soit redessiné.

Comment l'image arrivait à la dalle :

- **un seul tampon d'image** (`num_fbs = 1`), en PSRAM, que la dalle relit sans arrêt, environ 60 fois par seconde ;
- LVGL dessinait par **bandes de 25 lignes** en RAM interne (deux bandes de 51 200 octets), copiées dans le tampon d'image par
  DMA2D ;
- **aucune synchronisation** avec le balayage (`avoid_tearing = false`).

Un passage qui ne touche que l'horloge est court et ne se voit pas. Un **repeint complet** (changement de ciel : moment du jour,
ou ambiance tirée de l'icône météo relue toutes les 15 minutes ; verre des cartes recuit ; thème ; retour d'une page) dure
0,36 à 0,5 s sur la 7B : pendant ce temps, la dalle montre une image à moitié réécrite, qui descend de haut en bas. C'est la piste
suivie pour le flash ; elle n'est **pas prouvée** (la vidéo à 60 images par seconde du 25/11 au soir doit le dire).

## Décision

En rotation 0 (`WS7B_ROTATION=0`, le défaut, la 7B d'Axel) et avec `WS7B_SYNCHRO_IMAGE=1` (le défaut) :

- `components/bsp_waveshare_7b/ws7b_display.c` : `dpi.num_fbs = 2` (deux tampons de la taille de la dalle, en PSRAM, alloués par
  le pilote DSI d'ESP-IDF 5.5.5) ;
- `components/bsp_waveshare_7b/ws7b_board.c` : `direct_mode = true`, tampons de la taille de l'écran, `avoid_tearing = true` dans
  `lvgl_port_add_disp_dsi`.

Ce que font alors les composants (sources lues : `esp_lcd_panel_dpi.c` d'ESP-IDF 5.5.5, `esp_lvgl_port_disp.c` d'esp_lvgl_port
2.9.0, `lv_refr.c` de LVGL 9.5.0) :

1. esp_lvgl_port prend les deux tampons du pilote (`esp_lcd_dpi_panel_get_frame_buffer`) et les donne à LVGL en mode direct ;
   il n'alloue plus de tampon de dessin.
2. LVGL dessine les zones changées directement dans le tampon que la dalle ne lit pas.
3. Au dernier morceau d'un passage, esp_lvgl_port appelle `esp_lcd_panel_draw_bitmap` avec l'image entière : le pilote voit que
   l'adresse est l'un de ses tampons, écrit le cache en PSRAM (pas de copie) et note ce tampon comme le prochain à balayer. Le
   balayage en cours finit sur l'ancien ; le DMA repart sur le neuf à la fin de l'image (`mipi_dsi_dma_trans_done_cb`).
4. esp_lvgl_port attend cette fin d'image (`on_frame_buf_complete`, 16,7 ms au plus) avant de rendre la main à LVGL, qui dessine
   ensuite dans l'autre tampon.
5. Au passage suivant, LVGL recopie dans ce tampon les zones changées au passage précédent, sauf celles qu'il va redessiner de
   toute façon (`refr_sync_areas`).

Le choix du nombre de tampons est une fonction pure, `ws7b_frame_buffers()` (`include/ws7b_pur.h`), testée sur le PC
(`test_carte_7b.cpp`, 3 mutations). Au démarrage, le journal dit lequel des deux chemins tourne (`ws7b: image : …`).

Dans le même lot, deux changements qui réduisent les repeints complets eux-mêmes :

- **Ciel plus calme** (`visuel::stabiliser_ciel`, `components/core/include/ecran/visuel_echelles.hpp`) : le ciel ne suit une
  nouvelle ambiance que quand deux relevés successifs la donnent (le code du temps oscille souvent entre deux familles voisines) ;
  le premier relevé s'applique tout de suite. Le journal écrit une ligne `principal: ciel : …` à chaque changement de ciel et
  quand un relevé en propose un autre. Coût : un vrai changement de temps arrive au ciel un relevé plus tard (15 minutes).
- **Verre recuit sans tout repeindre** (`cuire_verre`, `main/ui_principal_ciel.cpp`) : quand les cartes changent de place ou de
  teinte, seuls l'ancien et le nouveau rectangle de verre sont repeints (le ciel autour est retramé au même pixel). Une image de
  ciel neuve continue de tout repeindre : elle change le style du fond.

## Conséquences

- **Mémoire** (calculée, pas mesurée) : 2 457 600 octets de PSRAM pour les deux tampons d'image, soit 1 228 800 de plus
  qu'avant ; les deux bandes de 51 200 octets de RAM interne ne sont plus allouées (environ 100 Ko de RAM interne rendus).
- **Application de la 7B** : 2 620 736 octets (ESP-IDF 5.5.5, aucun avertissement).
- **Temps** : chaque passage attend la fin du balayage en cours (16,7 ms au plus) ; LVGL dessine en PSRAM (au travers du cache)
  au lieu de la RAM interne ; le pilote écrit le cache de toute l'image (1,2 Mo) à chaque passage ; la recopie des zones vers
  l'autre tampon est faite par le processeur. Aucun de ces temps n'est mesuré.
- **Rotation 180** et `WS7B_SYNCHRO_IMAGE=0` : rien ne change (un tampon d'image, bandes en RAM interne, DMA2D).
- **Tab5** : rien ne change.
- Le banc de rendu du PC ne passe pas par la couche carte : ses images ne disent rien de ce changement. Son contrôle
  `BANC_VOILES` reconstruit l'écran à la fin pour reprendre le ciel de la variante.

**Non vérifié** : tout ce qui se voit sur la carte (absence de balayage, fluidité de la roue et des pages, temps d'un passage),
la mémoire réellement libre, et si le flash blanc disparaît.

## Alternatives écartées

- **Garder un tampon et seulement réduire les repeints complets** (ciel calme, verre par rectangles) : fait aussi, mais un repeint
  complet reste inévitable (moment du jour, thème, retour d'une page) et se verrait toujours descendre.
- **Mode `full_refresh` à deux tampons** : LVGL redessinerait tout l'écran à chaque passage, même pour une minute de l'horloge.
- **Trois tampons** : le pilote en accepte trois ; LVGL n'aurait plus à attendre la fin du balayage, pour 1,2 Mo de PSRAM de
  plus. Pas utile tant que l'attente (16,7 ms au plus) n'est pas mesurée comme gênante.
- **Le même changement en rotation 180** : la rotation par le PPA écrit dans son propre tampon ; il faudrait faire tourner l'image
  par le balayage de la dalle (registre MADCTL de l'EK79007, jamais essayé sur cette carte, [carte-7b.md](../carte-7b.md)).

## Ce qui la remettrait en cause

Un balayage encore visible pendant un repeint complet ; un passage de rendu nettement plus lent (roue, pages, carte centrale) ; un
manque de PSRAM ; un blocage de la tâche LVGL (elle attend la fin d'une image : si le flux DSI s'arrêtait, elle attendrait
toujours). Pour comparer sur la carte : `.\tools\idf.ps1 "-DWS7B_SYNCHRO_IMAGE=0" build`.
