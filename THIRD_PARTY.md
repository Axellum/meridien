# Third-party components / Composants tiers

*English first, then « Version française » below.*

Meridien is released under the MIT licence ([LICENSE](LICENSE)). It uses, or plans to use, the components listed here.
Shared in case it is useful, and made with the help of an AI: check the primary sources before you redistribute anything.
**This is not legal advice.**

How to read this file. **Read** = the licence file or the source headers of the exact version named were opened on
2026-11-06 (the sources are named in each row). **Not verified** = not read, so do not rely on it. Versions come from
[`dependencies.lock`](dependencies.lock) and from the sources themselves, not from memory.

## In the firmware today

| Component | Version | Licence (SPDX) | Read in | Notes |
|---|---|---|---|---|
| [ESP-IDF](https://github.com/espressif/esp-idf) | 5.5.5 | Apache-2.0 | `LICENSE` of the 5.5.5 tree ([link](https://github.com/espressif/esp-idf/blob/v5.5.5/LICENSE)); `docs/en/COPYRIGHT.rst` of the tree says original code is Apache-2.0 | ESP-IDF also includes third-party code under other licences, listed in that `COPYRIGHT.rst`. **Not reviewed here: not verified.** |
| [LVGL](https://github.com/lvgl/lvgl) | 9.5.0 | MIT | `LICENCE.txt` of the package (« Copyright (c) 2025 LVGL Kft ») | LVGL bundles optional third-party libraries (its `COPYRIGHTS.md`). In the `sdkconfig` of the build of 2026-11-06 only the software renderer and TinyTTF were enabled (rows below). |
| stb_truetype, inside LVGL's TinyTTF (`src/libs/tiny_ttf`) | as bundled with LVGL 9.5.0 | MIT **or** public domain (Unlicense), at your choice | `src/libs/tiny_ttf/LICENSE.txt` (« Copyright (c) 2017 Sean Barrett ») | Renders the embedded Roboto Bold at run time. |
| Montserrat 14 px, LVGL's built-in font, with Font Awesome 5 Free symbols | in LVGL 9.5.0 | OFL-1.1 | `scripts/built_in_font/font_license/Montserrat/OFL.txt` (« Copyright 2011 The Montserrat Project Authors ») and `.../FontAwesome5/LICENSE.txt` (fonts under OFL 1.1, « Copyright (c) 2022 Fonticons, Inc. ») | Linked in the image of the build of 2026-11-06 (`lv_font_montserrat_14` in the map file); it is the only built-in size enabled there. |
| [esp_lvgl_port](https://github.com/espressif/esp-bsp/tree/master/components/esp_lvgl_port) | 2.9.0 | Apache-2.0 | `license.txt` and the SPDX headers of 53 source files; 4 other files of the package carry CC0-1.0 (not examined) | LVGL glue for ESP-IDF. |
| [esp_lcd_st7121](https://github.com/espressif/esp-iot-solution/tree/master/components/display/lcd/esp_lcd_st7121) | 1.0.1 | Apache-2.0 | `license.txt` and SPDX headers | Display driver (Tab5, ST7121 screen). |
| [esp_lcd_touch_st7123](https://github.com/espressif/esp-iot-solution/tree/master/components/display/lcd_touch/esp_lcd_touch_st7123) | 1.0.2 | Apache-2.0 | `license.txt` and SPDX headers | Touch driver (Tab5). |
| [esp_lcd_touch](https://github.com/espressif/esp-bsp/tree/master/components/lcd_touch/esp_lcd_touch) | 1.2.1 | Apache-2.0 | `license.txt` and SPDX headers | Pulled in by the touch driver. |
| [esp_io_expander_pi4ioe5v6408](https://github.com/espressif/esp-bsp/tree/master/components/io_expander/esp_io_expander_pi4ioe5v6408) | 1.0.1 | Apache-2.0 | `license.txt` and SPDX headers | I/O expander driver (Tab5). |
| [esp_io_expander](https://github.com/espressif/esp-bsp/tree/master/components/io_expander/esp_io_expander) | 1.2.1 | Apache-2.0 | `license.txt` and SPDX headers | Pulled in by the previous one. |
| [cmake_utilities](https://github.com/espressif/esp-iot-solution/tree/master/tools/cmake_utilities) | 0.5.3 | Apache-2.0 | `license.txt` and SPDX header | Build-time dependency of the two `esp_lcd_*` packages. |
| [esp_hosted](https://github.com/espressif/esp-hosted-mcu) (ESP-Hosted-MCU, host side) | 2.12.12 | Apache-2.0 | `LICENSE` (Apache License 2.0 text) of the package; SPDX headers: 195 files Apache-2.0, the others CC0-1.0 or « Unlicense OR CC0-1.0 » (not examined) | Wi-Fi link to the ESP32-C6 over SDIO. Pinned to the pair already running on this tablet under ESPHome 2026.9.1: the component hash in `dependencies.lock` is the one in ESPHome's lock (compared 2026-11-06). The firmware of the C6 is read, never written. |
| [esp_wifi_remote](https://github.com/espressif/esp-wifi-remote) | 1.6.3 | Apache-2.0 | `LICENSE` of the package; SPDX headers: 310 files Apache-2.0, the others CC0-1.0 or « Unlicense OR CC0-1.0 » (not examined) | Routes the `esp_wifi` API to the C6. Same hash comparison as above. |
| wifi_remote_over_eppp (same repository) | 0.3.3 | Apache-2.0 | `license: Apache-2.0` in its `idf_component.yml` and 5 SPDX headers; **no `LICENSE` file in this package** (not verified beyond those) | Required by `esp_wifi_remote`; the Wi-Fi library selected in this build is esp_hosted, not EPPP. |
| [eppp_link](https://github.com/espressif/esp-protocols/tree/master/components/eppp_link) | 1.1.5 | Apache-2.0 | `LICENSE` of the package; SPDX headers: 16 files Apache-2.0 (others not examined) | Pulled in by `wifi_remote_over_eppp`. |
| [esp_serial_slave_link](https://github.com/espressif/idf-extra-components/tree/master/esp_serial_slave_link) | 1.1.2 | Apache-2.0 | `LICENSE` of the package; SPDX headers: 13 files Apache-2.0 | Pulled in by `eppp_link`. |
| Roboto Bold (`main/assets/roboto_700.ttf`) | 3.015 (font name table) | OFL-1.1 | [`main/assets/OFL.txt`](main/assets/OFL.txt); name table of the font file (« Copyright 2011 The Roboto Project Authors », licence URL <https://openfontlicense.org>) | Embedded as is, according to [`main/assets/README.md`](main/assets/README.md); the SHA-256 given there matches the file (checked 2026-11-06). Source of the family: <https://github.com/googlefonts/roboto-classic>. The `OFL.txt` copied there also carries notices for other families that this repository does not ship. |
| Weather icons of the main screen (`main/assets/icones_meteo.bin`) | generated by `tools/icones/generer_icones.py` of this repository | none (original: drawings written as code in this repository, under its licence) | the generator and the drawings it contains | **No third-party artwork and no icon font.** The shapes (sun, moon, cloud, rain, snow, bolt, fog) are drawn from primitives and rasterised to A8 images; they were not compared with any existing icon set (**not verified**). They are **not** taken from the « The Weather is Nice Today » pack of the *Planned* section, which stays unused and unread. The generator uses Pillow (MIT-CMU, a development tool on the PC, not shipped). |
| Material Design Icons (`main/assets/mdi_roue.ttf`, **a modified subset of 4 glyphs** of `materialdesignicons-webfont.ttf`) | the font's name table says « Version 1.0 » (the package version is not in the file: **not verified**) | Apache-2.0 (fonts) | [`main/assets/Apache-2.0.txt`](main/assets/Apache-2.0.txt) (standard licence text, copied from the ESP-IDF 5.5.5 tree); <https://raw.githubusercontent.com/Templarian/MaterialDesign/master/LICENSE> (« Pictogrammers Free License »: « All web and desktop fonts are distributed under the Apache 2.0 license ») and <https://pictogrammers.com/docs/general/license/>, both read 2026-11-08 | Used only by the press-and-hold wheel (ADR-0021): cog-outline, bell-outline, compass-rose. The icons of the « Brand / Logos » category are **not** covered by that licence; none is used. The file is a **modified file** (a subset made with `fontTools`; the outlines are the originals): see [`main/assets/README.md`](main/assets/README.md). No `NOTICE` file was found at the root of the source repository (full file list: not verified). Weather icons stay the original drawings above (ADR-0008). |
| [posix_tz_db](https://github.com/nayarsystems/posix_tz_db) (`zones.csv`) | commit `93447c0` (2025-09-29) | MIT | [`components/core/outils/posix_tz_db/LICENSE`](components/core/outils/posix_tz_db/LICENSE) | Source of the time-zone table `fuseaux_donnees.inc`. The upstream licence file leaves its copyright line unfilled (« [year] [fullname] »), so no holder is named. Details, and the 7 zones added from the tz database: [`components/core/ATTRIBUTION.md`](components/core/ATTRIBUTION.md). |
| [tz database (IANA)](https://github.com/eggert/tz) | data read 2026-11-06 | public domain | upstream `LICENSE`: files are in the public domain, except `date.c`, `newstrftime.3`, `strftime.c` (BSD 3-clause), which this project does not use | Only data lines are used, for the 7 additions listed in [`components/core/ATTRIBUTION.md`](components/core/ATTRIBUTION.md). |
| [Open-Meteo](https://open-meteo.com/) (weather API; no client library) | API v1 | **CC-BY-4.0** (API data) | <https://open-meteo.com/en/licence> and <https://open-meteo.com/en/terms>, both read 2026-11-06 | **Attribution is a condition**, see below. Also covers the real API responses kept as test fixtures in `test/host/donnees/` and the WMO weather-code table (`meteo_wmo.hpp`, codes as listed on <https://open-meteo.com/en/docs>). The surf core (`surf_*.hpp`, [ADR-0013](docs/decisions/0013-surf-donnees-note-et-limites.md)) also uses the **Marine API** (`marine-api.open-meteo.com`: waves, swell, sea-surface temperature, sea level) and the wind of the forecast API; its 30 real responses are kept in `test/host/donnees/surf_*.json` (captured 2026-11-07). The licence wording of each wave-model provider behind the Marine API was **not examined**; only the API's CC BY 4.0 is cited. |
| [MET Norway Locationforecast 2.0](https://api.met.no/weatherapi/locationforecast/2.0/documentation) (weather API, no key; no client library) | API 2.0 | **CC BY 4.0** and **NLOD 2.0** (Norwegian Licence for Open Government Data), "unless otherwise noted" | <https://api.met.no/doc/License>, <https://api.met.no/doc/TermsOfService> (last revised 2026-11-05 according to the page), the Locationforecast documentation, the licensing and crediting page of met.no and the full texts of CC BY 4.0 and NLOD 2.0, all read word for word on 2026-11-08 (first read 2026-11-07 through a page summariser); the MET Norway FAQ was not reopened | **Credit is a condition**, see below. Also covers the real API responses kept as test fixtures (`test/host/donnees/metno_*.json`) and the « Sunrise 3.0 » answers used to check the sunrise/sunset calculation (`soleil_metno_reference.json`). |
| [NWS API](https://www.weather.gov/documentation/services-web-api) (`api.weather.gov`, no key; no client library) | — | NWS web pages: public domain "unless specifically noted otherwise"; the API page says all the information presented via the API is intended to be open data, free to use for any purpose. **No licence is named for the API itself.** | <https://www.weather.gov/disclaimer>, <https://www.weather.gov/documentation/services-web-api> and the general FAQ <https://weather-gov.github.io/api/general-faqs>, read word for word on 2026-11-08 (first read 2026-11-07 through a page summariser) | No credit required for general use; the NWS name and logo must not suggest an endorsement, see below. Also covers the real API responses kept as test fixtures (`test/host/donnees/nws_*.json`). |
| Vigie embedded frame (`GET /v1/embedded/frame?spot=…` on the public instance `vigie.auxfilsdesidees.fr`, no key; no client library) | contract read 2026-11-08 | the frame is Vigie's own output; the data behind its sea block carry **CC BY 4.0**: Open-Meteo Marine with Météo-France's MFWAM wave model (Copernicus Marine), and a tide computed by Vigie from **EOT20** (Hart-Davis et al. 2021, DGFI-TUM) | `SOURCES.md` of `vigie-api` (rows Open-Meteo Marine, MFWAM, EOT20), read 2026-11-08 | The sea core (`mer_*.hpp`, [ADR-0018](docs/decisions/0018-mer-trame-vigie-et-vues.md)) reads the `SEA`, `ACT` and `CRIT` lines. The six real frames kept in `test/host/donnees/mer_*.txt` (captured 2026-11-08) are reproduced **unchanged**, with the other lines they carry. **Credits Vigie asks for, to show wherever the sea data is shown (the page does not exist yet)**: the Open-Meteo credit, then « Generated using E.U. Copernicus Marine Service Information; https://doi.org/10.48670/moi-00017 », and « Tides computed by Vigie from the EOT20 ocean tide model (Hart-Davis et al. 2021, DGFI-TUM, SEANOE, https://doi.org/10.17882/79489), CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/); constants kept near the coasts and rounded by Vigie. Not for navigation ». The tide coefficient is Vigie's estimate, not SHOM's. **Not examined**: the licences of the other sources whose lines (`HAZ`: earthquakes, volcanoes, floods, cyclones from EMSC, USGS, NOAA NWS, NHC…) are in the same frames; no test reads their content. |

### Open-Meteo: what the licence asks, and where we stand

- The licence page asks for credit, a link to the licence ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)) and a
  mention of changes (we parse and round the values). It also asks for a **link next to every place where the data are
  displayed**, for example the text « Weather data by Open-Meteo.com » linking to <https://open-meteo.com/>.
- The terms page offers the free API for **non-commercial use** only. It lists personal home automation and apps with no
  subscription or advertising as non-commercial, and subscriptions or advertising as commercial. Anyone who redistributes
  a product built on this firmware is responsible for their own use of the API.
- **Status in this repository (2026-11-06): not done yet.** `README.md` names Open-Meteo in a table, but neither the README
  nor the settings page (`web/app`) shows the credit and the link. It is planned (`docs/plan.md`, « dépôt propre », item 2).
  *Update 2026-11-07 (weather source by country, [ADR-0010](docs/decisions/0010-source-meteo-par-pays-et-client-reseau-commun.md)):*
  the sentence above is partly out of date: the settings page now shows the credit with its link (`web/app/app.js`) and
  the README carries it. The device's own screen has no link to follow; its « Place and weather » card shows « Weather
  data: Open-Meteo (CC BY 4.0 licence) » and, when a national model is chosen, « Model: <name> » (seen on PC renders only,
  not on a board). Whether a text without a link is enough for the licence page's wording is **not settled**.
  *Update 2026-11-07 (surf core):* there is no surf page on any screen yet, so nothing is displayed. When one is built it must
  carry the same credit and licence mention next to the wave, tide and wind data, and say that the values are model estimates.

### MET Norway and the NWS: what they ask, and where we stand

*First read 2026-11-07 through a page summariser, then read again word for word on 2026-11-08 from the pages themselves; what was and was not read is in [docs/mesures/fournisseurs-meteo.md](docs/mesures/fournisseurs-meteo.md). Decision: [ADR-0014](docs/decisions/0014-met-norway-et-nws-comme-sources.md).*

- **MET Norway, licence.** The licence page names NLOD 2.0 and CC BY 4.0 "unless otherwise noted" and asks for a credit to MET Norway
  (suggested wording « Based on data from MET Norway »); a link to the source is appreciated. The terms page sums CC BY 4.0 up as:
  credit, link to the licence, **indicate changes**. We do change the data (values rounded to tenths, conditions mapped onto WMO
  codes, sunrise and sunset computed on the device), so the text shown says so. The terms of service themselves quote the CC BY
  duties (credit, link to the licence, indicate changes). CC BY 4.0 §3(a) lets the credit be given « in any reasonable manner » for the
  medium, for example a link to a resource that carries the information; NLOD 2.0 §5 asks for a reference to the licence and, where
  practically possible, a link to the licence and to the source. The device screen cannot carry a link: it names the licences and says
  the data are adapted; the settings page links the service and, since 2026-11-08, the text of each licence. That this is enough is the
  project's reading of the licences, **not legal advice**.
- **MET Norway, rules of use that bind the device** (terms of service and Locationforecast documentation): an identifying
  `User-Agent` (a request without one, or with a generic library name, is answered 403), fewer than 20 requests per second, never
  repeat a request before the `Expires` time, coordinates truncated to at most 4 decimals, no schedule on the hour. Ours names the
  application and the repository `github.com/Axellum/meridien`; **that repository has been public since 08/11/2026, so the contact is reachable
  (what MET Norway does with it was not tried)** ([docs/publication.md](docs/publication.md)). Anyone who redistributes a binary must put their own contact there.
- **MET Norway, what the word-for-word reading of 2026-11-08 added** (terms of service, Locationforecast documentation): (a) identical
  requests no more often than every 30 minutes for the Nordic region and every third hour for the rest of the world: the device waits at
  least 30 minutes for Norway, Finland, Svalbard and Iceland, and the page does not define « Nordic region », so **Iceland is not
  settled**; (b) the terms list `If-Modified-Since` among their default rules: the device does **not** send it
  ([ADR-0014](docs/decisions/0014-met-norway-et-nws-comme-sources.md), point 9); (c) the terms call themselves a legally binding
  agreement that a human in authority must read, understand and accept, and say that whoever generates code with an AI tool must make
  sure it follows them: the owner of each device accepts them for it ([README.md](README.md)); (d) browsers and mobile apps are asked to go
  through a caching proxy of their own, which becomes compulsory with heavy traffic: this device calls the API directly, about two
  requests an hour each.
- **NWS, credit.** No credit is required for general use. The NWS asks that nothing suggests an endorsement or an affiliation (its
  name and logo are protected); the screen and the page therefore add « This device is neither approved nor sponsored by the
  National Weather Service. » next to the data. « Public domain » is the wording of the disclaimer page, which covers the NWS web pages; the
  API page says all the information presented via the API is intended to be open data, free to use for any purpose, and names no
  licence: the screen and the page therefore say « open data » (since 2026-11-08; « public domain » before).
- **NWS, rules of use**: an identifying `User-Agent` (site or e-mail contact), a "reasonable" rate that is not published (an error
  with a retry in about 5 seconds is documented), the `Cache-Control` headers are the guide for how long to keep an answer. The disclaimer asks, when a service is unavailable, for
  retries no more often than every minute: the device leaves a failed service alone for its whole minimum interval (60 minutes for the NWS) and
  asks Open-Meteo in the meantime.
- **Status in this repository (2026-11-07).** The screen's « Place and weather » card and the settings page show the credit of the
  service that **really gave** the weather (and Open-Meteo's, not the failed service's, during a fallback). Seen on PC renders and a
  browser run against a fake server only, **not on a board**. Since 2026-11-08 the page also links the text of CC BY 4.0 and NLOD 2.0
  (browser run-through on the PC, 213 checks out of 213; not on a board).

## Planned, not in the build yet

| Component | Version | Licence | Read in | Notes |
|---|---|---|---|---|
| [esp-sr](https://github.com/espressif/esp-sr) (WakeNet wake word, audio front end) | 2.6.1 is the latest seen (`idf_component.yml` on `master`, read 2026-11-06); not in `dependencies.lock` | « ESPRESSIF MIT License »: MIT-style text, but the permission is granted for use on Espressif products | [`LICENSE`](https://github.com/espressif/esp-sr/blob/master/LICENSE) on `master`, read 2026-11-06 | This firmware only runs on ESP32-P4, an Espressif chip, but whether that covers a chip on a third-party board is **not settled** (`docs/recherche/06-audio-duplex-aec-esp-sr.md`, §4). No separate licence file was found for the models. To settle **before any binary is published**. Licences of its own dependencies (`esp-dl`, `dl_fft`, `cjson`, `esp-dsp`): **not verified**. |
| Weather icon pack « The Weather is Nice Today » (Laura Reen, icon-icons.com), via the `IconeMeteo.ttf` font of the [Tab5 project](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL) | not in this repository | **not verified** | Tab5 `Tab5/fonts/icone_meteo/README.md`: the drawings are retouched and « the pack's terms are on its page » | The pack page answered HTTP 403 to an automated request on 2026-11-06 and was not read. Axel noted « licence ok » on 2026-11-06 (`docs/plan.md`) but the text has not been read. **Do not add the icons to this repository until the licence text is read and attached here.** |

## Not listed

Development tools that are not distributed with the repository (host compiler, gitleaks, ruff, Playwright, Node, esptool…)
are not listed. The settings page (`web/app`) declares no third-party font, image or script (`web/README.md`); its
icons are drawn in the project and were not compared with any existing icon set (**not verified**).

---

# Version française

Meridien est publié sous licence MIT ([LICENSE](LICENSE)). Il utilise, ou prévoit d'utiliser, les composants ci-dessous.
Partagé au cas où ça servirait, fait avec l'aide d'une IA : relire les sources d'origine avant toute redistribution.
**Ce n'est pas un avis juridique.**

**Lu** = le fichier de licence ou les en-têtes de source de la version citée ont été ouverts le 06/11/2026 (la source est
nommée à chaque ligne). **Non vérifié** = non lu : ne pas s'y fier. Les versions viennent de
[`dependencies.lock`](dependencies.lock) et des sources, pas de la mémoire.

## Dans le firmware aujourd'hui

| Composant | Version | Licence (SPDX) | Lu dans | Remarques |
|---|---|---|---|---|
| ESP-IDF | 5.5.5 | Apache-2.0 | `LICENSE` de l'arbre 5.5.5 ; `docs/en/COPYRIGHT.rst` de l'arbre : le code d'origine est sous Apache-2.0 | Contient aussi du code tiers sous d'autres licences, listées dans ce `COPYRIGHT.rst`. **Non passé en revue ici : non vérifié.** |
| LVGL | 9.5.0 | MIT | `LICENCE.txt` du paquet (« Copyright (c) 2025 LVGL Kft ») | Embarque des bibliothèques tierces facultatives (`COPYRIGHTS.md`). Dans le `sdkconfig` du build du 06/11/2026, seuls le rendu logiciel et TinyTTF étaient activés. |
| stb_truetype, dans TinyTTF de LVGL | celle livrée avec LVGL 9.5.0 | MIT **ou** domaine public (Unlicense), au choix | `src/libs/tiny_ttf/LICENSE.txt` (« Copyright (c) 2017 Sean Barrett ») | Dessine Roboto Bold à l'exécution. |
| Montserrat 14 px (police intégrée de LVGL) avec symboles Font Awesome 5 Free | dans LVGL 9.5.0 | OFL-1.1 | `scripts/built_in_font/font_license/Montserrat/OFL.txt` et `.../FontAwesome5/LICENSE.txt` (polices sous OFL 1.1, « Copyright (c) 2022 Fonticons, Inc. ») | Présente dans l'image du build du 06/11/2026 (fichier `.map`) ; seule taille intégrée activée. |
| esp_lvgl_port | 2.9.0 | Apache-2.0 | `license.txt` et en-têtes SPDX de 53 fichiers source ; 4 autres fichiers du paquet portent CC0-1.0 (non examinés) | Liaison LVGL / ESP-IDF. |
| esp_lcd_st7121 | 1.0.1 | Apache-2.0 | `license.txt` et en-têtes SPDX | Pilote d'écran (Tab5, écran ST7121). |
| esp_lcd_touch_st7123 | 1.0.2 | Apache-2.0 | idem | Pilote tactile (Tab5). |
| esp_lcd_touch | 1.2.1 | Apache-2.0 | idem | Tiré par le pilote tactile. |
| esp_io_expander_pi4ioe5v6408 | 1.0.1 | Apache-2.0 | idem | Pilote de l'expandeur d'E/S (Tab5). |
| esp_io_expander | 1.2.1 | Apache-2.0 | idem | Tiré par le précédent. |
| cmake_utilities | 0.5.3 | Apache-2.0 | idem | Dépendance de compilation des deux paquets `esp_lcd_*`. |
| esp_hosted (ESP-Hosted-MCU, côté hôte) | 2.12.12 | Apache-2.0 | `LICENSE` (texte de l'Apache License 2.0) du paquet ; en-têtes SPDX : 195 fichiers Apache-2.0, les autres CC0-1.0 ou « Unlicense OR CC0-1.0 » (non examinés) | Lien Wi-Fi avec l'ESP32-C6 par SDIO. Épinglé sur le couple déjà en service sur cette tablette sous ESPHome 2026.9.1 : l'empreinte du composant dans `dependencies.lock` est celle du verrou d'ESPHome (comparées le 06/11/2026). Le micrologiciel du C6 est lu, jamais écrit. |
| esp_wifi_remote | 1.6.3 | Apache-2.0 | `LICENSE` du paquet ; en-têtes SPDX : 310 fichiers Apache-2.0, les autres CC0-1.0 ou « Unlicense OR CC0-1.0 » (non examinés) | Aiguille l'API `esp_wifi` vers le C6. Même comparaison d'empreinte. |
| wifi_remote_over_eppp (même dépôt) | 0.3.3 | Apache-2.0 | `license: Apache-2.0` dans son `idf_component.yml` et 5 en-têtes SPDX ; **pas de fichier `LICENSE` dans ce paquet** (non vérifié au-delà) | Exigé par `esp_wifi_remote` ; la bibliothèque Wi-Fi choisie dans ce build est esp_hosted, pas EPPP. |
| eppp_link | 1.1.5 | Apache-2.0 | `LICENSE` du paquet ; en-têtes SPDX : 16 fichiers Apache-2.0 (autres non examinés) | Tiré par `wifi_remote_over_eppp`. |
| esp_serial_slave_link | 1.1.2 | Apache-2.0 | `LICENSE` du paquet ; en-têtes SPDX : 13 fichiers Apache-2.0 | Tiré par `eppp_link`. |
| Roboto Bold (`main/assets/roboto_700.ttf`) | 3.015 (table de noms de la police) | OFL-1.1 | [`main/assets/OFL.txt`](main/assets/OFL.txt) ; table de noms de la police (« Copyright 2011 The Roboto Project Authors », URL de licence <https://openfontlicense.org>) | Embarquée telle quelle d'après [`main/assets/README.md`](main/assets/README.md) ; le SHA-256 qui y figure correspond au fichier (vérifié le 06/11/2026). Le `OFL.txt` copié porte aussi des avis d'autres familles que ce dépôt ne livre pas. |
| Icônes météo de l'écran principal (`main/assets/icones_meteo.bin`) | générées par `tools/icones/generer_icones.py` de ce dépôt | aucune (originales : dessins écrits en code dans ce dépôt, sous sa licence) | le générateur et les dessins qu'il contient | **Aucun dessin tiers et aucune police d'icônes.** Les formes (soleil, lune, nuage, pluie, neige, éclair, brume) sont tracées à partir de primitives et rastérisées en images A8 ; elles n'ont pas été comparées à un jeu d'icônes existant (**non vérifié**). Elles ne viennent **pas** du pack « The Weather is Nice Today » de la section *Prévus*, qui reste inutilisé et non lu. Le générateur utilise Pillow (MIT-CMU, outil de développement sur le PC, non livré). |
| Material Design Icons (`main/assets/mdi_roue.ttf`, **sous-ensemble modifié de 4 glyphes** de `materialdesignicons-webfont.ttf`) | la table de noms de la police dit « Version 1.0 » (le numéro de version du paquet n'est pas dans le fichier : **non vérifié**) | Apache-2.0 (polices) | [`main/assets/Apache-2.0.txt`](main/assets/Apache-2.0.txt) (texte standard de la licence, copié de l'arbre ESP-IDF 5.5.5) ; <https://raw.githubusercontent.com/Templarian/MaterialDesign/master/LICENSE> (« Pictogrammers Free License » : « All web and desktop fonts are distributed under the Apache 2.0 license ») et <https://pictogrammers.com/docs/general/license/>, lues le 08/11/2026 | Sert seulement à la roue d'appui long (ADR-0021) : cog-outline, bell-outline, compass-rose. Les icônes de la catégorie « Brand / Logos » ne sont **pas** couvertes par cette licence ; aucune n'est utilisée. Le fichier est un **fichier modifié** (un sous-ensemble fait avec `fontTools` ; les contours sont ceux d'origine) : voir [`main/assets/README.md`](main/assets/README.md). Aucun fichier `NOTICE` trouvé à la racine du dépôt d'origine (liste complète des fichiers : non vérifiée). Les icônes météo restent les dessins originaux ci-dessus (ADR-0008). |
| posix_tz_db (`zones.csv`) | commit `93447c0` (29/09/2025) | MIT | [`components/core/outils/posix_tz_db/LICENSE`](components/core/outils/posix_tz_db/LICENSE) | Source de la table des fuseaux. La ligne de copyright du dépôt d'origine n'est pas remplie (« [year] [fullname] ») : aucun titulaire nommé. Détail et 7 zones ajoutées depuis la base tz : [`components/core/ATTRIBUTION.md`](components/core/ATTRIBUTION.md). |
| Base tz (IANA) | données lues le 06/11/2026 | domaine public | `LICENSE` d'origine : domaine public, sauf `date.c`, `newstrftime.3`, `strftime.c` (BSD 3 clauses), que ce projet n'utilise pas | Seules des lignes de données sont reprises (7 ajouts, voir `ATTRIBUTION.md`). |
| Open-Meteo (API météo, sans bibliothèque) | API v1 | **CC-BY-4.0** (données de l'API) | <https://open-meteo.com/en/licence> et <https://open-meteo.com/en/terms>, lues le 06/11/2026 | **L'attribution est une condition**, voir ci-dessous. Couvre aussi les vraies réponses de l'API gardées comme données de test (`test/host/donnees/`) et la table des codes météo WMO. Le noyau « surf » (`surf_*.hpp`, [ADR-0013](docs/decisions/0013-surf-donnees-note-et-limites.md)) utilise aussi l'**API Marine** (`marine-api.open-meteo.com` : vagues, houle, température de la mer, niveau de la mer) et le vent de l'API de prévision ; ses 30 vraies réponses sont gardées dans `test/host/donnees/surf_*.json` (capturées le 07/11/2026). La formulation de licence de chaque fournisseur de modèles de vagues derrière l'API Marine n'a **pas été examinée** ; seule la CC BY 4.0 de l'API est citée. |
| MET Norway Locationforecast 2.0 (API météo, sans clé ; pas de bibliothèque) | API 2.0 | **CC BY 4.0** et **NLOD 2.0** (licence norvégienne des données publiques), « sauf mention contraire » | <https://api.met.no/doc/License>, <https://api.met.no/doc/TermsOfService> (dernière révision le 05/11/2026 d'après la page), la documentation de Locationforecast, la page « Licensing and crediting » de met.no et les textes intégraux de CC BY 4.0 et de NLOD 2.0, lus mot pour mot le 08/11/2026 (première lecture le 07/11/2026 par un résumeur de pages) ; la FAQ de MET Norway n'a pas été rouverte | **La mention de la source est une condition**, voir ci-dessous. Couvre aussi les vraies réponses de l'API gardées comme données de test (`test/host/donnees/metno_*.json`) et les réponses « Sunrise 3.0 » qui servent à contrôler le calcul du lever et du coucher (`soleil_metno_reference.json`). |
| API du NWS (`api.weather.gov`, sans clé ; pas de bibliothèque) | — | pages web du NWS : domaine public « sauf mention contraire » ; la page de l'API dit que toutes les informations qu'elle présente sont destinées à être des données ouvertes, libres pour tout usage. **Aucune licence n'est nommée pour l'API elle-même.** | <https://www.weather.gov/disclaimer>, <https://www.weather.gov/documentation/services-web-api> et la FAQ générale <https://weather-gov.github.io/api/general-faqs>, lues mot pour mot le 08/11/2026 (première lecture le 07/11/2026 par un résumeur de pages) | Aucune mention exigée pour un usage général ; le nom et le logo du NWS ne doivent pas suggérer une approbation, voir ci-dessous. Couvre aussi les vraies réponses de l'API gardées comme données de test (`test/host/donnees/nws_*.json`). |
| Trame embarquée de Vigie (`GET /v1/embedded/frame?spot=…` sur l'instance publique `vigie.auxfilsdesidees.fr`, sans clé ; pas de bibliothèque) | contrat lu le 08/11/2026 | la trame est la production de Vigie ; les données de son bloc mer sont sous **CC BY 4.0** : Open-Meteo Marine avec le modèle de vagues MFWAM de Météo-France (Copernicus Marine), et une marée calculée par Vigie à partir d'**EOT20** (Hart-Davis et al. 2021, DGFI-TUM) | `SOURCES.md` de `vigie-api` (lignes Open-Meteo Marine, MFWAM, EOT20), lu le 08/11/2026 | Le noyau « mer » (`mer_*.hpp`, [ADR-0018](docs/decisions/0018-mer-trame-vigie-et-vues.md)) lit les lignes `SEA`, `ACT` et `CRIT`. Les six trames réelles gardées dans `test/host/donnees/mer_*.txt` (capturées le 08/11/2026) sont reproduites **telles quelles**, avec les autres lignes qu'elles portent. **Crédits demandés par Vigie, à montrer partout où la mer est montrée (la page n'existe pas encore)** : le crédit d'Open-Meteo, puis « Generated using E.U. Copernicus Marine Service Information; https://doi.org/10.48670/moi-00017 », et « Tides computed by Vigie from the EOT20 ocean tide model (Hart-Davis et al. 2021, DGFI-TUM, SEANOE, https://doi.org/10.17882/79489), CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/); constants kept near the coasts and rounded by Vigie. Not for navigation ». Le coefficient de marée est une estimation de Vigie, pas celui du SHOM. **Non examinées** : les licences des autres sources dont les lignes (`HAZ` : séismes, volcans, crues, cyclones de l'EMSC, de l'USGS, du NOAA NWS, du NHC…) sont dans les mêmes trames ; aucun test n'en lit le contenu. |

### Open-Meteo : ce que la licence demande, et où on en est

- La page de licence demande la mention de la source, un lien vers la licence
  ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)) et l'indication des modifications (nous analysons et
  arrondissons les valeurs). Elle demande aussi un **lien à côté de chaque endroit où les données s'affichent**, par
  exemple le texte « Weather data by Open-Meteo.com » pointant vers <https://open-meteo.com/>.
- Les conditions d'utilisation réservent l'API gratuite à un **usage non commercial** : la domotique personnelle et les
  applications sans abonnement ni publicité y sont rangées ; abonnement ou publicité = commercial. Qui redistribue un
  produit fondé sur ce firmware répond de son propre usage de l'API.
- **État dans ce dépôt (06/11/2026) : pas encore fait.** Le `README.md` nomme Open-Meteo dans un tableau, mais ni lui ni la
  page de réglages (`web/app`) n'affichent la mention et le lien. C'est prévu (`docs/plan.md`, « dépôt propre », point 2).
  *Mise à jour du 07/11/2026 (source de la météo par pays, [ADR-0010](docs/decisions/0010-source-meteo-par-pays-et-client-reseau-commun.md)) :*
  la phrase ci-dessus est en partie périmée : la page de réglages montre désormais la mention avec son lien
  (`web/app/app.js`) et le README la porte. L'écran de l'appareil n'a pas de lien à suivre ; sa carte « Lieu et météo »
  montre « Données météo : Open-Meteo (licence CC BY 4.0) » et, quand un modèle national est choisi, « Modèle : <nom> » (vu
  sur des rendus PC seulement, pas sur une carte). Qu'un texte sans lien suffise à la formulation de la page de licence
  n'est **pas tranché**.
  *Mise à jour du 07/11/2026 (noyau « surf ») :* aucune page de surf n'existe encore sur un écran, donc rien n'est affiché. Quand
  elle sera construite, elle devra porter la même mention de source et de licence à côté des données de vagues, de marée et de vent,
  et dire que ce sont des estimations de modèles.

### MET Norway et le NWS : ce qu'ils demandent, et où on en est

*Lu d'abord le 07/11/2026 par un résumeur de pages, puis relu mot pour mot le 08/11/2026 sur les pages elles-mêmes ; ce qui a été lu et ce qui ne l'a pas été : [docs/mesures/fournisseurs-meteo.md](docs/mesures/fournisseurs-meteo.md). Décision : [ADR-0014](docs/decisions/0014-met-norway-et-nws-comme-sources.md).*

- **MET Norway, licence.** La page de licence nomme NLOD 2.0 et CC BY 4.0 « sauf mention contraire » et demande de créditer MET Norway
  (formulation suggérée : « Based on data from MET Norway ») ; un lien vers la source est apprécié. La page des conditions résume
  CC BY 4.0 ainsi : mention de la source, lien vers la licence, **indication des modifications**. Nous modifions bien les données
  (valeurs arrondies au dixième, conditions ramenées aux codes WMO, lever et coucher du soleil calculés sur l'appareil) : le
  texte affiché le dit. Les conditions d'utilisation citent elles-mêmes les obligations de CC BY
  (mention de la source, lien vers la licence, indication des modifications). CC BY 4.0 §3(a) laisse donner la mention « de toute façon
  raisonnable » selon le support, par exemple un lien vers une ressource qui porte l'information ; NLOD 2.0 §5 demande une référence à la
  licence et, si c'est pratiquement possible, un lien vers la licence et vers la source. L'écran de l'appareil ne peut pas porter de lien :
  il nomme les licences et dit que les données sont adaptées ; la page de réglages lie le service et, depuis le 08/11/2026, le texte de
  chaque licence. Que cela suffise est la lecture des licences faite par le projet, **pas un avis juridique**.
- **MET Norway, règles d'usage qui lient l'appareil** (conditions d'utilisation et documentation de Locationforecast) : un
  `User-Agent` qui identifie (une requête sans lui, ou avec un nom de bibliothèque générique, reçoit 403), moins de 20 requêtes par
  seconde, ne jamais répéter une requête avant l'heure d'`Expires`, coordonnées tronquées à 4 décimales au plus, aucune
  planification à l'heure pile. Le nôtre nomme l'application et le dépôt `github.com/Axellum/meridien` ; **ce dépôt est public
  depuis le 08/11/2026, donc le contact est joignable (ce que MET Norway en fait n'a pas été essayé)** ([docs/publication.md](docs/publication.md)). Qui redistribue un binaire y met son propre contact.
- **MET Norway, ce que la relecture mot pour mot du 08/11/2026 a ajouté** (conditions d'utilisation, documentation de Locationforecast) :
  (a) pas de requête identique plus souvent que toutes les 30 minutes pour la région nordique et toutes les 3 heures pour le reste du
  monde : l'appareil attend au moins 30 minutes pour la Norvège, la Finlande, Svalbard et l'Islande, et la page ne définit pas « région
  nordique », donc **l'Islande n'est pas tranchée** ; (b) les conditions rangent `If-Modified-Since` parmi leurs règles par défaut :
  l'appareil ne l'envoie **pas** ([ADR-0014](docs/decisions/0014-met-norway-et-nws-comme-sources.md), point 9) ; (c) les conditions se
  disent un accord juridiquement contraignant qu'un humain responsable doit lire, comprendre et accepter, et disent que celui qui
  génère du code avec une IA doit s'assurer qu'il les respecte : le propriétaire de chaque appareil les accepte pour lui
  ([README.md](README.md)) ; (d) les navigateurs et les applications mobiles doivent passer par un mandataire à cache à eux, obligatoire
  en cas de gros trafic : cet appareil appelle l'API directement, environ deux requêtes par heure chacun.
- **NWS, mention.** Aucune mention n'est exigée pour un usage général. Le NWS demande que rien ne suggère une approbation ou une
  affiliation (son nom et son logo sont protégés) : l'écran et la page ajoutent donc « Cet appareil n’est ni approuvé ni parrainé par le
  National Weather Service. » à côté des données. « Domaine public » est la formulation de la page de mise en garde, qui vise les pages web du NWS ; la
  page de l'API dit que toutes les informations qu'elle présente sont destinées à être des données ouvertes, libres pour tout usage, sans
  nommer de licence : l'écran et la page disent donc « données ouvertes » (depuis le 08/11/2026 ; « domaine public » avant).
- **NWS, règles d'usage** : un `User-Agent` qui identifie (site ou contact e-mail), un débit « raisonnable » non publié (une erreur
  avec nouvel essai possible en 5 secondes environ est documentée), les en-têtes `Cache-Control` indiquent combien de temps garder une réponse. La mise en garde demande, quand un service est
  indisponible, de ne pas réessayer plus d'une fois par minute : l'appareil laisse un service en échec tranquille pendant tout son écart
  minimum (60 minutes pour le NWS) et interroge Open-Meteo en attendant.
- **État dans ce dépôt (07/11/2026).** La carte « Lieu et météo » de l'écran et la page de réglages montrent la mention du service
  qui a **réellement donné** la météo (et celle d'Open-Meteo, pas celle du service en échec, pendant un repli). Vu sur des rendus PC
  et un parcours de navigateur contre un faux serveur seulement, **pas sur une carte**. Depuis le 08/11/2026 la page lie aussi le texte de
  CC BY 4.0 et de NLOD 2.0 (parcours de navigateur sur le PC, 213 vérifications sur 213 ; pas sur une carte).

## Prévus, pas encore dans la compilation

| Composant | Version | Licence | Lu dans | Remarques |
|---|---|---|---|---|
| esp-sr (mot d'activation WakeNet, traitement audio) | 2.6.1 est la plus récente vue (`idf_component.yml` de `master`, lu le 06/11/2026) ; absent de `dependencies.lock` | « ESPRESSIF MIT License » : texte de type MIT, mais la permission vaut pour l'usage sur des produits Espressif | [`LICENSE`](https://github.com/espressif/esp-sr/blob/master/LICENSE) de `master`, lu le 06/11/2026 | Le firmware ne tourne que sur ESP32-P4, une puce Espressif ; que cela couvre une puce sur une carte tierce n'est **pas tranché** (`docs/recherche/06-audio-duplex-aec-esp-sr.md`, §4). Aucun fichier de licence propre aux modèles trouvé. À trancher **avant toute publication d'un binaire**. Licences de ses dépendances (`esp-dl`, `dl_fft`, `cjson`, `esp-dsp`) : **non vérifiées**. |
| Pack d'icônes météo « The Weather is Nice Today » (Laura Reen, icon-icons.com), via la police `IconeMeteo.ttf` du projet Tab5 | absent de ce dépôt | **non vérifiée** | `Tab5/fonts/icone_meteo/README.md` du projet Tab5 : dessins retouchés, « conditions sur la page du pack » | La page du pack a répondu HTTP 403 à une requête automatique le 06/11/2026 : non lue. Axel a noté « licence ok » le 06/11/2026 (`docs/plan.md`), mais le texte n'a pas été lu. **Ne pas ajouter ces icônes au dépôt avant d'avoir lu le texte de la licence et de l'y avoir joint.** |

## Non listés

Les outils de développement qui ne sont pas livrés avec le dépôt (compilateur hôte, gitleaks, ruff, Playwright, Node,
esptool…) ne sont pas listés. La page de réglages (`web/app`) ne déclare ni police, ni image, ni script tiers
(`web/README.md`) ; ses icônes sont dessinées dans le projet et n'ont pas été comparées à un jeu d'icônes existant
(**non vérifié**).
