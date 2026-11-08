### Added

- **Sea: the page itself (nothing opens it yet)**: the "decision in 2 seconds" page for a spot — a banner, four activity cards (surf,
  kite/wing, boat, shore fishing: verdict, reason, best slot, 24 hourly bars, three criteria), a legend, the tide curve with its
  coefficient badge, the sun, and a **detail panel when a card is touched** — drawn in one LVGL object for 1280x720, 1024x600 and
  720x1280, with every word in the seven languages. What to show is decided by three pure-core files that a test can check without a
  screen: **no verdict for a sea that is absent or too old, no default "Good" for an activity that cannot be judged here, unreadable
  coastal warnings never painted as a calm banner, no age, next tide, coefficient or "now" marker without a clock, and no page at all
  rather than a page whose zones overlap**. `main/ui_mer_ouvrir.cpp` is the glue (time zone, clock, "show the sea only if it is the
  chosen spot's"); **nothing calls it yet** (the long-press wheel is the next lot) and nothing writes the spot registry, so a device
  flashed with this change shows nothing new. **Limits written down**: the German, Dutch, Spanish, Italian and Turkish texts are
  written by an AI and have not been read by a native speaker; the change to or from daylight saving time inside the 24 hours is not
  handled; whether the texts fit their zones is judged on the PC render, not on a board. Tested on: PC only (85 host tests with
  AddressSanitizer and UBSan, also evaluated with ESP-IDF's GCC 14.2; 92 targeted mutations, all detected), rendered on the PC (LVGL
  software render of 21 screens at the three sizes) and compiled for the Tab5 and the Waveshare 7B; **not flashed and not run on any
  board, the touch never tried**. Details:
  [docs/decisions/0022-mer-la-page-lot-c.md](../docs/decisions/0022-mer-la-page-lot-c.md),
  [docs/mesures/mer.md](../docs/mesures/mer.md), [docs/captures/ecran/planche-mer-1280x720.png](../docs/captures/ecran/planche-mer-1280x720.png).
