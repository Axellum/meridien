### Added

- **Sea page: data credit and "not for navigation"**: a footer under the tide and sun panels names the sea data sources and
  their licence ("Sea data: Open-Meteo, Copernicus Marine, EOT20 tide (CC BY 4.0)") and says "Estimates, not for navigation"
  (ADR-0018), in the seven languages. One line in landscape, two in portrait; the bottom margin under it is smaller (4 instead of
  16 at the 720 scale) and the four activity cards are 12 px shorter at 1280x720 (10 px at 1024x600), still above their minimum
  height. A portrait screen 600 px wide now needs 1033 px of height for the page (600x1024 has none; no board shows the interface
  in portrait). Seen on: PC renders (1280x720, 1024x600, 720x1280, seven languages); not tested on board.
- **City search: GeoNames credit**: "Places: GeoNames (CC BY 4.0 license)" with the results, on the settings page (under the list,
  with links to GeoNames and to the licence) and on the device screen (status line just above the list). Seen on: PC renders and a
  browser run against the fake server; not tested on board.

### Changed

- **THIRD_PARTY.md**: the four components missing from it (mdns 1.13.1, both boards; esp_lcd_ek79007 2.0.2, esp_lcd_touch_gt911
  1.2.1 and esp_codec_dev 1.5.11, Waveshare 7B), licences read in their packages (Apache-2.0); rows for the Open-Meteo geocoding
  API (GeoNames, CC BY 4.0), Vigie's warnings and Vigie's spot list (OpenStreetMap, ODbL); the sea page is no longer described as
  missing; what each sea source asks for, with the pages read on 2026-11-11.
- **README, "What leaves the device"**: now says that Vigie receives the city's position (rounded to about 110 m) and the screen
  language every 15 minutes by default (alerts are on by default), plus the chosen spot and wind range, and that the text typed in
  the city search goes to Open-Meteo's geocoding service.
