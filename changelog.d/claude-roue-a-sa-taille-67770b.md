### Changed

- **Press-and-hold wheel: opening and closing it no longer repaints the whole home screen.** The wheel's LVGL object is now
  the size of the wheel (its touch area still covers the screen, so a touch outside still closes it); on the PC test bench
  opening or closing invalidates about 13 % of the screen instead of 100 %, and the wheel renders are pixel-identical in
  1280×720, 1024×600 and 720×1280 ([docs/mesures/roue.md](docs/mesures/roue.md) § 7). Not tested on board: the time saved
  on the Tab5 or the 7B has not been measured.
