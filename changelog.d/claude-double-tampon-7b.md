### Changed

- **Waveshare 7B: the picture changes in one go**: at rotation 0 the panel has two frame buffers in PSRAM; LVGL draws into the
  one the panel is not showing and the DSI driver switches at the end of a scan, so a full repaint (a change of sky, the glass
  of the cards) is no longer seen sweeping down the screen. About 100 KB of internal RAM no longer used for drawing, 1.2 MB more
  PSRAM ([ADR-0030](../docs/decisions/0030-7b-deux-tampons-d-image-synchronises.md)). Aimed at the brief white flash of the 7B,
  whose cause is not proven. Tested on: not tested on board (built for `waveshare-7b`, core tests on the PC).
- **A calmer sky** (both boards): the background follows a new weather mood only when two weather readings in a row agree (the code often
  swings between two neighbouring families); each change of sky is written to the serial log (`principal: ciel : …`).
  Tested on: not tested on board.
- **The glass of the cards repaints only its rectangles** (both boards) when the cards move or change colour, instead of the whole screen.
  Tested on: not tested on board.

*7B : l'image change d'un coup (deux tampons d'image), le ciel ne change plus à chaque petite variation de la météo, et le
verre des cartes ne fait plus repeindre tout l'écran. Rien de tout cela n'est encore vu sur la carte.*
