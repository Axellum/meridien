### Changed

- **NWS credit says « open data »**: next to NWS weather, the screen and the settings page now read « open data » instead of
  « public domain » (the NWS API page says its information is intended to be open data, free to use for any purpose; « public domain »
  is the wording for NWS web pages). Seven languages, the translations are not reviewed by a native speaker. Not seen on a board nor on
  a rendered screen.
- **The settings page links the text of each licence**: in the weather credit, « CC BY 4.0 » and « NLOD 2.0 » now open the licence text.
  Tested on: the PC browser run-through (`web/tests/parcours.py`), 213 checks out of 213, 2026-11-08, commit of this branch; not on a board.
- **README and THIRD_PARTY**: what the device sends to the weather services (IP address, coordinates of the chosen place), that MET
  Norway's terms bind whoever runs the firmware, and what the word-for-word reading of the MET Norway and NWS terms on 2026-11-08
  settled and left open (Iceland, `If-Modified-Since`).
