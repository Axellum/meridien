### Added

- **Sea: the long-press wheel opens the page**: the wheel's first ring gets an optional last button, **"Sea"** (a "waves" icon, the word in
  the seven languages), which opens the sea page of the chosen spot. It is the **last** button, so "Settings", "Details" and "Alerts" keep
  their rank, and it exists **only when a spot is chosen in the spot registry**: an empty registry shows no button towards a page with
  nothing to say. Which buttons exist, and in which order, is now decided by a tested pure-core file shared with the render bench, so the
  bench cannot show an order the firmware does not have. **The fourth button has a price**: the wheel fan is wider, so near the top-left
  corner the hub sits 220 px from the finger on the Tab5 (162 px before) and 177 px on the 7B (123 px before). **On a real device the
  button does not show yet: nothing writes the spot registry** (next lot), and if the page refuses to open (unreadable time zone for the
  place, say) the button shows nothing and does not say so. Tested on: PC only (8 host tests, 10 targeted mutations, all detected; the
  seven existing wheel renders are byte-identical to the ones before the change; five new wheel variants and a touch run "long press,
  Sea, close, Settings still reachable" on the render bench) and compiled for the Tab5 and the Waveshare 7B without any warning (+45 968
  and +45 952 bytes: the page is now linked); **not flashed and not run on any board, the touch never tried**. Details:
  [docs/decisions/0024-mer-l-entree-de-la-roue-lot-d.md](../docs/decisions/0024-mer-l-entree-de-la-roue-lot-d.md),
  [docs/mesures/mer.md](../docs/mesures/mer.md), [docs/captures/ecran/roue-centre-avec-mer-1280x720.png](../docs/captures/ecran/roue-centre-avec-mer-1280x720.png).
