### Fixed

- **"New code" on the screen now works** (Settings, card « Réglages depuis un téléphone »): it used to only write a log line.
  It now draws a new pairing code, shown at once, closes every session opened from a phone and lifts a pairing lock in
  progress; the device applies it before the next request, so the old code is refused. Tested on the PC
  (`test/host/test_appairage_ecran.cpp`); not tested on board.

### Security

- **Pairing lock that grows**: 5 wrong codes still lock pairing for 60 s, but each further series of 5 failures without a
  success doubles the lock (120 s, 240 s… at most 1 hour), and from the 3rd series in a row each lock also changes the code
  shown on the screen (open sessions stay). The lock is global: the person in front of the screen lifts it with "New code".
  See [docs/api-reglages.md](../docs/api-reglages.md) §2. Not tested on board.
- **The settings page no longer reads the body of a request without a valid token** on a protected route: the `401` is given
  at once and the connection closed. A request body must arrive within 5 s (no byte for 5 s, or the whole body after 5 s →
  `408`); the receive timeout went from 10 s to 5 s. Not tested on board.
