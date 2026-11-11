### Changed

- **Lighter firmware, same screen**: the weather icons only ship the sizes the screen actually picks, LVGL no longer
  builds the widgets, blend formats and built-in font the screen does not use, and the Roboto text font loses its
  hinting and substitution tables (every character kept). `meridien.bin` goes from 3,300,528 to 2,612,736 bytes on the
  Tab5 and from 3,278,736 to 2,590,896 bytes on the 7B (-21 %); every bench render is identical to the pixel. Not tested
  on board.
