### Changed

- **Weather request through a common HTTPS client, gzip accepted**: the weather client no longer carries its own HTTP code; it
  calls a common client (`main/http_commun.{h,cpp}`) that sends `Accept-Encoding: gzip`, decompresses a gzip answer with the
  decompressor stored in the chip's ROM (`tinfl_decompress`, no code added to the flash) and checks the CRC-32 and the size
  written in the gzip trailer. Open-Meteo answers measured from the PC on 2026-11-07: 1 498 bytes instead of 4 443 for Lyon and
  1 626 instead of 4 575 for Sydney (about a third). The receive buffer grows from 16 KB to 64 KB and a 64 KB buffer plus the
  decompressor (10 992 bytes) are needed while a gzip answer is read, **all in PSRAM** (expected peak 142 064 bytes during a
  request, returned afterwards; internal RAM is not asked for anything more). An answer larger than 64 KB, or a gzip file that
  would expand beyond it, is refused as "too large". The request itself, the cache and the screens are unchanged.
  Tested on: not tested on board (PC tests and builds only; the decompressor was never run, the real time and memory cost are
  unknown). Details: [docs/mesures/client-reseau.md](../docs/mesures/client-reseau.md).

### Added

- **Gzip envelope in the pure core** (`components/core/include/ecran/gzip.hpp`): header and trailer parsing (RFC 1952, optional
  fields included), CRC-32, and the decision to take on `Content-Encoding` (gzip, none, anything else refused). 23 tests with
  AddressSanitizer + UBSan, 73 mutations detected, and a Python generator that does not use the code under test (12 made-up
  files and two real Open-Meteo answers). Tested on: PC only; builds for Tab5 (2 839 440 bytes) and Waveshare 7B (2 820 016
  bytes), never run on a board.
