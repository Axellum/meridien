### Changed

- **Main screen code split by zone**: `main/ui_principal.cpp` (3,062 lines) is now an orchestrator of 392 lines plus one
  file per zone of the screen (`main/ui_principal_<zone>.cpp`) and an internal header for the shared state. The code was
  moved as is: nothing changes on the screen (372 renders of the seven scenes that draw it, at 1280x720, 1024x600 and
  720x1280 in French, identical to the pixel before and after on the PC). Not tested on board.
