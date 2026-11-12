### Changed

- **One set of fonts for every page**: the home screen, Details, Sea, Alerts, Settings and the boot screen now share one
  TinyTTF font per pixel size (`main/ui_ressources.h`) instead of each creating its own, so a size already drawn by the home
  screen is not rasterised again by a page. On the PC render bench: 14 font instances instead of 20 and 13 KB less glyph
  memory (PSRAM on the device) after opening every page; opening times unchanged within the PC's noise; 1524 renders identical
  pixel for pixel. The touch ring was checked to repaint only its old and new box. Not tested on board.
