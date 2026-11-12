### Added

- **What LVGL holds, in the serial command `memoire`**: two more fields, `lvgl_octets` and `lvgl_blocs`, counted by the LVGL
  allocator, to tell a real leak from the TinyTTF caches filling up during the first day (the fall of about 200 B per minute
  of free PSRAM seen on the 7B, explained on the PC in [docs/depannage.md](../docs/depannage.md)). A PC check,
  `python tools/rendu/endurance.py`, runs three simulated days of the main screen and fails if its memory grows on a day
  already seen. Not tested on board.
