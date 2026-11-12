### Changed

- **Start-up: the glass of the home-screen cards is baked into the sky image after the first light, not before** (about
  70 ms earlier light on the Tab5 with the sky theme, estimated from the November 2026 measurement). The first image draws
  the glass directly, as before the bake existed: 165 of 165 bench images are byte-identical (`BANC_VERRE_DIFFERE=1`). Just
  after the light, the bake and one full repaint of the home screen follow (same image). Not measured on a board.
