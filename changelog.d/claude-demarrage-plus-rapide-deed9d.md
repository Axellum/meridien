### Changed

- **Start-up: the fixed reset waits now overlap other work instead of being waited idle** (same minimum delays, none
  shortened). Tab5: the 120 ms the panel needs after its hardware reset now run while the clock, the power monitor, the
  controller detection and the DSI bus are set up; only what is left is waited, right before the first DSI command. 7B: the
  GT911 touch reset is started before the panel, so its 60 ms settle time runs during the panel set-up (over 120 ms of the
  panel's own waits) instead of after it. The 1 s wait for the Tab5 I/O expander is unchanged: no readable state replaces it
  (ADR-0005 of the Tab5 repository). Not measured on a board.

### Added

- **One `DEMARRAGE ms` line per boot on the serial log**: settings, board (with its own milestones), weather cache, screen
  created, first image rendered, backlight on, in milliseconds since the application started, to compare two binaries.
