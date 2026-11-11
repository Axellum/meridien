### Changed

- **One translation path**: the local translation tables of the main screen, the Details page, the wheel, the alerts page,
  the sea page and the short week days (`visuel_textes.hpp`, `grandeurs.hpp`, `vigie_textes.hpp`, `mer_textes.hpp`,
  `visuel_dates.hpp`, `main/textes_1b.h`) now live in `i18n/*.yaml` like every other screen text; their functions are only
  names for `i18n::tr`. Four words that had two translations depending on the screen now have one (“Pluie forte” in German,
  “Météo indisponible” and “Pluie modérée” in Turkish, “Rafales” in Spanish). *Un seul chemin de traduction : les tables
  locales sont ramenées dans `i18n/*.yaml`.* Not tested on board.
- **Keys with a context**: a French word with two meanings gets a key `context|text` (`"vent|faible"` light,
  `"mer|faible"` low, `"coefficient|faible"` small; `"jour court|mer"` Wednesday is not `"Mer"` the sea page). The context is
  never shown. *Clés à contexte.*
- **Diagnostic screen translated** (Settings > Device > Diagnostics): its 22 texts, the restart cause, the date and the time
  follow the screen language. *L'écran de diagnostic est traduit.* Not tested on board.
- **Register**: French says « vous », the six other languages use the informal form everywhere (two sentences fixed in
  German, Dutch, Spanish and Turkish); `controle_i18n.py` now checks it. *Registre : vous en français, tu ailleurs.*
- **Render bench**: every text drawn is checked for room (cut, shortened, off screen, outside its frame) and listed in
  `debordements.tsv` next to the PNG files. *Contrôle de place au banc de rendu.*

### Removed

- The 19 short day and month keys copied from the Tab5 (« Dim »… « Déc ») and two sentences nothing showed any more: no
  screen read them, and « Mer » (Wednesday) blocked « Mer » (the sea). *Clés mortes retirées.*
