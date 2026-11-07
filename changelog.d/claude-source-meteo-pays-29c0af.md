### Added

- **Weather source chosen by country**: a new setting, "Weather source" (under "Place and weather" on the screen, on the phone
  page and in `/api/location` as `source_meteo` with the list `sources_meteo`), lets you ask Open-Meteo for a national model
  (`models=`) instead of its automatic choice. "Automatic" is the default and sends exactly the request used before (same
  bytes, tested). The list is filtered by the country of the chosen city and always starts with "Automatic". Only the models
  whose real answer the current reader accepts are offered: **MET Norway** (Norway, Sweden, Finland, Denmark, Iceland,
  Estonia), **KNMI** (Netherlands, Belgium, Luxembourg) and the global **NOAA GFS** (everywhere); so in France the menu is
  "Automatic" and "NOAA GFS" only. Ten more national models (Météo-France, DWD, Met Office, DMI, MeteoSwiss, JMA, ECCC,
  GeoSphere Austria, ItaliaMeteo, CHMI) and ECMWF IFS are in the table with the limit measured on their real answer (no UV
  index, no wind gusts, no rain probability, fewer than seven days) and are **not offered**: the strict reader refuses a whole
  answer when one of those is missing. Offering them needs a decision on relaxing that reader. Changing the source acts like
  changing the city (stored answer cleared, new request, no restart); moving to a city whose country the source does not
  cover puts the setting back on "Automatic" and says so on the screen and on the phone page. The model's name is shown next
  to the Open-Meteo attribution (CC BY 4.0). 29 real answers (one per model and country) are kept as test data, the table
  and its link with the reader are tested on the PC (19 tests for the table; the 212 mutations of the five test suites this
  change touches are all detected, 0 problem).
  Tested on: PC only (tests, mutations, and PC renders of the screens in seven languages). Compiled on 07/11/2026 for the
  Tab5 (`tab5-st7121`, 2 851 712 bytes) and for the Waveshare 7B (2 832 320 bytes), exit code 0; **not flashed and not run on
  any board**: what the screen, the phone page and the real Open-Meteo answers do on a board is not verified. Details: [docs/decisions/0010-source-meteo-par-pays-et-client-reseau-commun.md](../docs/decisions/0010-source-meteo-par-pays-et-client-reseau-commun.md),
  [docs/mesures/meteo-modeles.md](../docs/mesures/meteo-modeles.md), [docs/api-reglages.md](../docs/api-reglages.md) section 5.
  German and Turkish texts are still not reviewed by a speaker of the language.
