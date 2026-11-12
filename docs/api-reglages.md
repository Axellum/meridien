# Settings API / API de réglages — the device's REST contract / contrat REST de l'appareil

**English first, then French (« Français », further down).** The French text is the original; the English text is a translation of it, written by an AI and not reviewed by a native speaker. It says the same thing: where it differs, **the French text prevails**. Nothing in the English text is meant to claim more than the French one does (in particular every "not verified" stays "not verified").

**L'anglais d'abord, puis le français (« Français », plus bas).** Le texte français est l'original ; le texte anglais en est une traduction, écrite par une IA et non relue par un locuteur. Il dit la même chose : en cas de différence, **le français fait foi**. Rien dans l'anglais ne doit affirmer plus que le français (en particulier chaque « non vérifié » reste « non vérifié »).

Contents / Sommaire — English: 1 Rules · 2 Pairing · 3 Device · 4 Wi-Fi · 5 Weather zone · 6 Display and languages · 7 STT/LLM/TTS services · 8 Third-party error sentences · 9 Error format · 10 Export/import · 11 Update · 12 Static files · 13 Languages · 14 Official alerts · 15 Sea. Français : mêmes numéros, dans la partie « Français ».

---

# English

Written on 06/11/2026 (track B), languages added on 06/11/2026. **Design contract; the firmware serves a first subset of it (box below), the other routes do not exist on the device yet.** The front end and the fake server in `web/` implement all of it; the firmware will have to follow it as written. Everything written here as behaviour of the device is **not verified** until it has been observed on a board.

Original decisions: [ADR-0002](decisions/0002-assistant-vocal-en-cascade.md) (three services), [ADR-0003](decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md) (page served by the device, local HTTP, pairing token, secrets, update), [ADR-0006](decisions/0006-ecran-en-sept-langues.md) (seven languages).

> **Firmware status — milestone “réglages”, 06/11/2026: subset V1, Wi-Fi station mode only (no access point, no captive portal).** Served by `main/serveur_web.cpp` on port 80; the logic is in the pure core (`components/core/include/ecran/api_reglages.hpp`), tested on the PC with mutations. **Compiled, not yet observed on a board** (see [mesures/reglages.md](mesures/reglages.md)).
> - **Real**: `GET /api/status`, `POST` and `DELETE /api/pair`, `GET /api/system`, `GET` and `PUT /api/location`, `GET` and `PUT /api/display`, `GET` and `PUT /api/alerts` (§14), `GET` and `PUT /api/sea` (§15), `GET /api/weather`, `GET /api/geocode`, `GET /api/wifi` (read-only), the static files (gzip).
> - **Empty, without error** (the page opens without a red message): `GET /api/services` (nothing configured), `GET /api/presets` (none), `GET /api/update` (`inactif`).
> - **`501` `non_disponible`**, with the sentence “This function is not available on this screen yet.” in the screen language: Wi-Fi scan and change, saving and testing the STT / LLM / TTS services, export / import, update and rollback.
> - **Known gaps with the text below**: `GET /api/geocode` is **synchronous for the page** (the request waits up to 30 s for the answer, so it holds the server's single task: rule 7 is not kept for this one route), but the network work is done by the search module's own low-priority task, never by the server's task or the screen's; beyond 5 open connections the oldest is closed instead of answering `503` (rule 8); `validee` is always `true` (no rollback yet); `compile_le` is the local time of the build PC, without “Z”; the `fuseau` reported is the saved city's, and it is also the zone the on-screen clock uses (ADR-0012); a time zone that is not in the table of 468 names shows no time on screen (never Paris time), and `Africa/Casablanca` is a known limit of that table.

## 1. Rules that apply to every route

1. **An API key is never readable.** It enters the device only in the body of a `PUT` or `POST` (never in the address), and **never** leaves it: not in a response, not in the export, not in an error message, not in the log. Responses only say `"cle_definie": true|false`. No partial mask ("…a3f9"): not even the last characters are returned. The Wi-Fi password follows the same rule.
2. **A third-party service's response body is never copied.** A service that answers "invalid key: sk-abc…xyz" may quote the key. The device turns the HTTP status it received into a sentence of its own (§8) and does not pass on the text it received.
3. **UTF-8 JSON** (`Content-Type: application/json; charset=utf-8`), accents and "°" as they are. `Cache-Control: no-store` on all of `/api/`. Field names are in French without accents (`mot_de_passe`, `luminosite`); technical values (`fuseau`, `latitude`) are unchanged.
4. **Same origin only.** No CORS header: the page is served by the device. (The development fake server may add one.)
5. **Errors = one short sentence in the screen language** (§13), never a raw code or a trace. One single format, see §9. The page only shows `message`.
6. **Authentication**: every route except `GET /api/status`, `POST /api/pair` and the language files `GET /lang/<code>.json` (§12) requires `Authorization: Bearer <session token>` (§2). Without it: `401`.
7. **Long operations are asynchronous** (`202`, then reading the state) because `esp_http_server` handles requests in a single task by default: a 15 s "test" must not freeze the device's page. This covers the Wi-Fi scan, the Wi-Fi connection, the service tests and the update.
8. **Limits**: JSON body ≤ 4 KB (import ≤ 16 KB); at most 4 simultaneous clients (beyond that: `503`, "The device is busy, try again in a moment.").
9. **The `Host` header must name the device** (protection against DNS rebinding: a web page whose name suddenly points to the device's local address would send its requests with *its own* name). Accepted, ignoring case, with or without `:80`: the device's IPv4 address on the home Wi-Fi, its mDNS name `ecran-xxxx.local` (a final dot is allowed) and, once it exists, its address on its own commissioning access point. Anything else, on every route including the static files → `403`, the one line `Forbidden` (`text/plain`, nothing about the device), connection closed; the body is not read. Consequence: a name given by a router (`ecran-xxxx.home`, `ecran-xxxx.fritz.box`…) is refused too: use the address or the `.local` name shown on the screen. In `point_d_acces` mode (not built yet), the captive-portal redirect of §12 will answer foreign names instead. The decision is `http::hote_autorise` in the pure core (`components/core/include/ecran/http_requete.hpp`), tested on the PC with mutations; the fake server applies the same rule (its names are `127.0.0.1` and `localhost` on its port).
10. **Security headers on every response**: `X-Content-Type-Options: nosniff` and `Referrer-Policy: no-referrer` (the page's address is sent to no one, not even to the Vigie address the page reads); the page's files also carry the content policy of §12.

## 2. Pairing and session

The pairing token is a **6-digit code shown on the device's screen** (ADR-0003). The page asks for it once, exchanges it for a session token and keeps that in `sessionStorage` (never in the address).

| Route | Role |
|---|---|
| `GET /api/status` | Public. `{"nom","modele","version","mode","appairage","langue"}`: `mode` = `station` (on the home Wi-Fi) or `point_d_acces` (commissioning); `appairage` = `"requis"` or `"ok"` depending on whether the request carries a valid token; `langue` = code of the **screen language** (§6), so that the page can speak the right language before pairing. `modele` = the name of the board the firmware runs on, read from the board layer (« M5Stack Tab5 » or « Waveshare 7B »). Nothing else (no IP address, no SSID). |
| `POST /api/pair` | Body `{"code":"123456"}` → `200 {"jeton":"<32 bytes hex>"}`. Wrong code → `403`, "That code isn’t right. Look at the device’s screen." **5 failed attempts → lock** (`429`, "Too many attempts. Try again in 60 seconds.", with `reessayer_dans` and `Retry-After`). The lock lasts 60 s, then doubles with each further series of 5 failures without a success in between (120 s, 240 s… at most 1 hour). From the 3rd series in a row, each lock also draws a **new code**, shown on the screen; sessions already open stay open. A success brings the lock back to 60 s. The lock is global (not per client). |
| `DELETE /api/pair` | Closes the current session → `204`. |

The session token lives in RAM (at most 4 sessions; the oldest is closed when a fifth arrives) and disappears on restart: the page then asks for the code again. The code changes on restart, after the 3rd series of failures in a row (above), and when "New code" is touched on the screen: that button also **closes every open session** and **lifts a lock in progress**, so the person in front of the screen is never locked out by someone else on the network. The new code is shown at once; the device applies it before the next request (the old code is then refused).

A request to a protected route without a valid token gets its `401` **without its body being read**, and the connection is closed. A request body must arrive within 5 s (no byte for 5 s, or the whole body not there after 5 s → `408`, connection closed); a phone on the same Wi-Fi sends it in a few milliseconds.

**Accepted limit**: plain HTTP on the local network; the token can be read by anyone listening to that network (self-signed HTTPS ruled out, ADR-0003).

## 3. Device

`GET /api/system` →
```json
{
  "nom": "ecran-1a2b",
  "modele": "M5Stack Tab5",
  "version": "0.1.0",
  "compile_le": "2026-11-06T11:20:00Z",
  "partition": "ota_0",
  "validee": true,
  "depuis_secondes": 5421,
  "memoire_libre": 5120000,
  "heure": {"epoch": 1793960400, "synchronisee": true, "fuseau": "Europe/Paris"},
  "internet": true
}
```
`validee` is false until the new version has passed its self-test (§11). `internet` is true once the device has an IP address **and** has received the time from the network (SNTP): that is the proof of access it uses, not the success of the last request (a weather service failing for hours leaves it `true`; a late SNTP answer leaves it `false` even when the weather already arrived). `memoire_libre` is the free **internal** RAM in bytes (`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`), not the PSRAM: a few hundred kilobytes on a real device, not the 5 MB of the example.

## 4. Wi-Fi

| Route | Role |
|---|---|
| `GET /api/wifi` | Current state. |
| `POST /api/wifi/scan` | Starts a network search → `202 {"en_cours":true}`. |
| `GET /api/wifi/scan` | `{"en_cours":false,"reseaux":[{"ssid":"Maison","niveau":4,"force":-55,"securise":true}]}`; `niveau` 0 to 4 (display), `force` in dBm, sorted from strongest to weakest, duplicates merged. |
| `PUT /api/wifi` | Body `{"ssid":"Maison","mot_de_passe":"…"}` (empty password allowed for an open network) → `202 {"etat":"connexion"}`. |

`GET /api/wifi` →
```json
{"etat": "connecte", "ssid": "Maison", "niveau": 3, "force": -61, "ip": "192.168.1.52",
 "message": "Connected to “Maison”."}
```
(`192.168.1.52` is an example address, not a real network's.) `etat` ∈ `connecte`, `connexion`, `echec`, `deconnecte`, `point_d_acces`. After a `PUT`, the page reads this state every 1.5 s until `connecte` or `echec` (20 s at most). On `echec`, the device **goes back to the previous network** (or to the commissioning access point) and says so: "Password refused by “Maison”. The previous network is kept." **Access-point case**: when changing network, the phone loses the device. The `202` then carries `"adresse_apres": "http://ecran-1a2b.local"` and the page shows "Reconnect your phone to your Wi-Fi, then open http://ecran-1a2b.local." (captive portal and mDNS: not demonstrated on P4+C6, see the plan, milestone 1d).

## 5. Weather zone and time zone

| Route | Role |
|---|---|
| `GET /api/geocode?q=Lyon` | Place search through a **firmware proxy** to Open-Meteo's geocoding. |
| `GET /api/location` | Saved zone and its weather source. |
| `PUT /api/location` | Saves the chosen zone and/or weather source. |
| `GET /api/weather` | Preview of the saved zone's weather (so the page shows "it works"). |

**Search.** `q`: **at least 3 characters** after `trim` (otherwise `400`, "Type at least 3 letters."), at most 80, UTF-8 encoded for the address. The firmware calls `geocoding-api.open-meteo.com/v1/search` with `name=<q>`, `count=8`, `language=<screen language code>`, `format=json` (parameters and fields read on 06/11/2026 in open-meteo.com/en/docs/geocoding-api: `count` 10 by default and 100 at most; the response puts the places in `results` with `name`, `admin1`, `country`, `country_code`, `latitude`, `longitude`, `elevation`, `timezone`, `population`; empty fields are omitted; an error is a JSON `error`/`reason` with `400`. That the geocoding service supports each of the seven language codes: **not verified**. The firmware renames the fields as below) and returns a reduced list:
```json
{"resultats": [
  {"id": "2996944", "nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR",
   "latitude": 45.74846, "longitude": 4.84671, "altitude": 173, "fuseau": "Europe/Paris",
   "population": 522250, "libelle": "Lyon, Auvergne-Rhône-Alpes, France"}
]}
```
Empty list → `200 {"resultats":[]}` (the page says "No place found for “Lyonn”."). No Internet on the device → `503`, "The device has no Internet access to search for a place." The values above are **examples of the shape**, not data that was read. A result without `fuseau` is dropped from the list. `altitude` is only sent when the service gives a plausible one, **between −500 m and 9,000 m**: Open-Meteo writes `-9999` when the altitude is unknown (seen on a real response, “Istanbul Old Town”, 06/11/2026), and that value is not passed on.

**One search module for the page and the screen** (`main/geocodage.cpp`; reading and states in the pure core `components/core/include/ecran/geocodage.hpp`). The page's `GET /api/geocode` and the screen's search (settings and guided setup) use the **same** reader, so both show the same places with the same labels. Only one search runs at a time, **the last request wins**, and the download shares one lock with the weather request (`main/verrou_tls.h`): a city search and a weather request never run at the same time (a TLS handshake takes about 65 KB of the ~110 KB of internal RAM left). The network work never runs in the screen's task. If the lock is not free within 15 s, the search ends as “no connection”. The text typed is never written to the log. **Reader and states tested on the PC with real captured responses (7 Open-Meteo responses); firmware compiled; never run on a board. The weather client does not take the TLS lock yet** (see [mesures/reglages.md](mesures/reglages.md) §5).

**Saving.** `PUT /api/location` receives a search result as it is (fields `nom`, `region`, `pays`, `code_pays`, `latitude`, `longitude`, `fuseau`; `altitude` optional; `source_meteo` optional, see below):
- `latitude` ∈ [−90, 90], `longitude` ∈ [−180, 180] (otherwise `422`, "These coordinates aren’t valid.");
- `fuseau` must be an **IANA name present in the embedded table** (`posix_tz_db`, pure core); otherwise `422`, "The device doesn’t know this time zone.";
- the device keeps the **IANA name** (in NVS, to find the time again without a network), derives the POSIX string from it, applies `TZ`, restarts the weather, then answers with the zone read back:
```json
{"nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR",
 "latitude": 45.74846, "longitude": 4.84671, "fuseau": "Europe/Paris",
 "posix": "CET-1CEST,M3.5.0,M10.5.0/3", "libelle": "Lyon, Auvergne-Rhône-Alpes, France"}
```
`GET /api/location` returns the same shape, or `{"definie": false}` as long as no zone has been chosen (the home screen then leaves the place empty; the sentence "Choose a city in the settings page." exists in the translation table but no screen code shows it, checked on 12/11/2026).

**Weather source** (ADR-0010). The same route carries the weather source of the saved city. `GET /api/location` (and every response of `PUT /api/location`) adds `source_meteo` (stable identifier of the source in use: `auto`, or a national model such as `knmi`), `source_meteo_nom` (the name to show: "Automatic" in the screen language, a service name as it is), `source_meteo_retablie` (see below) and `sources_meteo`, the list **offered for the country of the saved city**, "Automatic" first, then the national service, then the worldwide model (the screen offers exactly the same list):
```json
{"source_meteo": "knmi", "source_meteo_nom": "KNMI", "source_meteo_retablie": false,
 "sources_meteo": [{"id": "auto", "nom": "Automatic", "direct": false}, {"id": "knmi", "nom": "KNMI", "direct": false}, {"id": "noaa", "nom": "NOAA GFS", "direct": false}]}
```
`{"definie": false}` carries the same fields (with no known country the list is "Automatic" and the worldwide model only). `PUT /api/location` accepts `source_meteo`, the identifier chosen in that list, **alone** (`{"source_meteo": "knmi"}`: the city does not change) or **with a city** (then the source is judged on the country of the NEW city; if either is refused nothing is saved). An identifier that does not exist, or that is not a string: `422`, "This weather source doesn’t exist.", field `source_meteo`. A known source that the country does not offer: `422`, "This weather source doesn’t cover the city’s country.", same field. Changing the source works like changing the city: the weather kept on the device is erased, a new request starts, the screen updates without a restart. **When the city changes to a country the current source does not cover, the device goes back to "Automatic" and says so**: `source_meteo_retablie` is `true` in that response and in the next `GET`s, until a source is chosen or the city changes again, and the screen shows the same sentence ("Source set back to “Automatic”: the previous one doesn’t cover this country."). Most sources are Open-Meteo forecast models; "Automatic" is the request without a `models` parameter, exactly as before this setting existed. Two sources are services the device reads itself, with no key (ADR-0014): MET Norway (`metnorway`: Norway, Finland, Iceland, Svalbard) and the NWS (`nws`: United States, Puerto Rico). Their entry in `sources_meteo` has `"direct": true` (`false` for all the others), so a client does not present them as an Open-Meteo "model"; when such a service does not answer, the weather comes from Open-Meteo "Automatic" in the meantime (see `GET /api/weather`). Which models are offered, and why the others are not, is in [mesures/meteo-modeles.md](mesures/meteo-modeles.md).

**Weather preview.** `GET /api/weather` →
```json
{"disponible": true, "perimee": false, "mise_a_jour": 1793960400,
 "fournisseur": {"id": "open-meteo", "nom": "Open-Meteo", "lien": "https://open-meteo.com/"}, "repli": false,
 "actuelle": {"temperature": 14.2, "code": 2, "libelle": "Partly cloudy", "jour": true}}
```
`code` = WMO code (Open-Meteo's own; the conditions of MET Norway and the NWS are mapped onto the same codes); `libelle` = short label from the pure core's WMO table **in the screen language** (§13), never the code alone. `perimee` is true when the last reading is more than **2 h** old, or when the last **3 requests in a row failed** and it is more than 30 min old, or when the device's clock cannot judge (not set yet, or gone backwards) (`meteo_synthese.hpp`: `DELAI_PERIME_APPAREIL_S`, `ECHECS_POUR_PERIME`, `AGE_PERIME_PAR_ECHECS_S`); the `actuelle` object has no felt temperature (`ressentie`): the device does not send it; `{"disponible": false}` before the first successful reading. `fournisseur` names the service that **really gave** this weather (`id`: `open-meteo`, `met-norway` or `nws`; `nom` and `lien`: what the attribution shows, the licence asks for it wherever the weather is shown). It is not always the chosen source: `repli` is `true` when the chosen source is a service read directly that did not answer, so the weather comes from Open-Meteo "Automatic" in the meantime. The screen and the page then say so ("… is unavailable: the weather comes from Open-Meteo in the meantime.") and cite Open-Meteo, not the service that failed (ADR-0014).

## 6. Display and languages

`GET /api/display` →
```json
{"langue_ecran": "fr", "langue_assistant": "fr", "theme": "auto", "luminosite": 80, "profil": "auto",
 "langues": [{"code": "fr", "nom": "Français"}, {"code": "en", "nom": "English"}, {"code": "de", "nom": "Deutsch"},
             {"code": "nl", "nom": "Nederlands"}, {"code": "es", "nom": "Español"}, {"code": "it", "nom": "Italiano"},
             {"code": "tr", "nom": "Türkçe"}],
 "langues_assistant": [{"code": "fr", "nom": "Français"}, "… same list …"],
 "themes": ["auto", "clair", "sombre"],
 "profils": ["auto", "meteo"]}
```
There are **two distinct language settings** (ADR-0006): `langue_ecran` (what the screen, the settings page and the device's sentences are written in) and `langue_assistant` (the language the voice assistant understands and speaks, §7). Both default to `fr`. `PUT /api/display` accepts the five modifiable fields (`langue_ecran`, `langue_assistant`, `theme`, `luminosite`, `profil`), each optional (absent = unchanged); `langues`, `langues_assistant`, `themes` and `profils` are read-only: **the lists come from the device**, the page does not fix them. `nom` is the language's name **in that language**. A code that is not in the list → `422`, "This language isn’t available for the screen." (`langue_ecran`) or "This language isn’t available for the assistant." (`langue_assistant`). `luminosite`: integer 5 to 100 (in %); the floor of 5 avoids a screen made unreadable from the page (otherwise `422`, "Brightness must be between 5 and 100%."). `profil` is the home screen's profile (ADR-0023): `auto` (the screen chooses by itself, from the rain and the official alerts, ADR-0015) or a pinned profile; `profils` lists **only the profiles the device can draw** (today `auto` and `meteo`), and `auto` is the default and what a corrupt stored value falls back to (never a pinned profile by accident). A code that is not in `profils` → `422`, "This profile doesn’t exist." (`profil_inconnu`). Pinning does not hide an official orange or red alert. Changes take effect on the screen as soon as the response is sent. After a change of `langue_ecran`, the following responses (messages, preset names and help, weather label, test steps) are in the new language and the page reloads its texts (§13). The former single `langue` field no longer exists (see §10 for old backups).

## 7. STT / LLM / TTS services

Three **independent** services (ADR-0002), identified by `stt`, `llm`, `tts`, each OpenAI-compatible, local or cloud. Routes the device calls under the base URL: `…/audio/transcriptions` (STT), `…/chat/completions` streaming (LLM), `…/audio/speech` (TTS), plus `…/models` for the test.

| Route | Role |
|---|---|
| `GET /api/presets` | Presets **supplied by the device** (the page hard-codes none). |
| `GET /api/services` | The three saved services, without keys. |
| `PUT /api/services/{stt\|llm\|tts}` | Saves a service. |
| `POST /api/services/{id}/test` | Starts a test → `202`. |
| `GET /api/services/{id}/test` | Result of the last test. |

`GET /api/presets` → `{"stt": [Preset…], "llm": […], "tts": […]}` with
```json
{"id": "lmstudio", "nom": "LM Studio (my PC)", "url": "http://localhost:1234/v1", "local": true,
 "cle_requise": false, "modele": "", "aide": "Start LM Studio’s local server on the PC."}
```
`nom` and `aide` are in the screen language (§13); brand names (OpenAI, Google) are not translated. `local: true` marks a service running on the user's PC: the page then reminds the user to replace `localhost` with the PC's address (nothing is blocked, it is advice).
`id` ∈ `openai`, `google`, `lmstudio`, `ollama`, `kokoro`, `personnalise` (ADR-0002 + `kokoro`); **a preset only exists for the services its provider documents**:

| Preset | Base address | Services | Source (read on 06/11/2026) |
|---|---|---|---|
| `openai` | `https://api.openai.com/v1` | stt, llm, tts | developers.openai.com/api/reference (routes `/models`, `/chat/completions`, `/audio/transcriptions`, `/audio/speech`) |
| `google` | `https://generativelanguage.googleapis.com/v1beta/openai/` | **llm only** | aistudio.google.com/docs/openai: `/models` and `/chat/completions` documented; **`/audio/transcriptions` and `/audio/speech` not documented** (differs from ADR-0002, see track B's report) |
| `lmstudio` | `http://localhost:1234/v1` (to be replaced with the PC's address) | llm | lmstudio.ai/docs/developer/openai-compat: no audio route listed; port 1234 comes from the doc's examples, "official default" not verified |
| `ollama` | `http://localhost:11434/v1` (to be replaced with the PC's address) | llm, stt, tts | docs.ollama.com/api/openai-compatibility: `/v1/audio/transcriptions` and `/v1/audio/speech` listed; which models serve them: not verified |
| `kokoro` | `http://localhost:8880/v1` | tts | README of remsky/Kokoro-FastAPI (single source) |
| `personnalise` | free | stt, llm, tts | |

`localhost` means nothing from the screen: the page asks for the PC's address (e.g. `192.168.1.20`, an example) and substitutes it before saving. No model name is suggested from memory: the `modele` field stays empty and the test's `modeles` (see below) offers the service's real list. Addition to ADR-0002: the `kokoro` preset (local French voice, research 02) exists because it is the planned local TTS.

`GET /api/services` →
```json
{"llm": {"preset": "lmstudio", "url": "http://192.168.1.20:1234/v1", "modele": "…",
         "cle_definie": false, "configure": true},
 "stt": {"preset": "openai", "url": "…", "modele": "…", "cle_definie": true, "configure": true},
 "tts": {"preset": "personnalise", "url": "…", "modele": "…", "voix": "…", "cle_definie": false, "configure": true}}
```
`voix` only exists for `tts`. **There is no `langue` field any more** (it used to exist for `stt`): speech recognition uses `langue_assistant` (§6), sent as the transcription request's `language` field. A service never set up: `{"configure": false, "cle_definie": false}`.

`PUT /api/services/{id}`: body = the fields to change.

| Field | Rule |
|---|---|
| `preset` | one of the `id`s above; |
| `url` | `http://` or `https://`, no credentials in the address (`user:pwd@` refused: "Remove the username from the address: the key is entered separately."), 200 characters at most, the trailing `/` is removed; |
| `modele`, `voix` | short text (80 characters); a `langue` field sent by an old client is ignored; |
| `cle` | **write-only.** Present and non-empty = replaces the key; **absent = key unchanged**; |
| `effacer_cle` | `true` = deletes the saved key. |

Response: the service read back (same shape as `GET`), hence **without** `cle`. Changing `preset` does not keep the previous preset's key: it is erased (an OpenAI key must never go to another server), unless the same body brings a new one.

**Test.** `POST /api/services/{id}/test` accepts an optional body `{"url","modele","voix","cle"}`: it tries these values **without saving them** (the page tests what is in the form); an absent field takes the saved value, `cle` included. The test does, in order:
1. **Reach**: `GET {url}/models` (a `404`/`405` answer counts as "reached, but does not list its models": a valid local service);
2. **Minimal real trial**: LLM = a one-token exchange; TTS = synthesis of the fixed word "Bonjour" (the duration and size received are reported, nothing is played); STT = transcription of half a second of silence, in the assistant language.

`GET /api/services/{id}/test` →
```json
{"etat": "termine", "ok": true, "duree_ms": 412,
 "message": "The service replies in 0.4 s.",
 "etapes": [{"nom": "Reach the service", "ok": true},
            {"nom": "Response test", "ok": true}],
 "modeles": ["…", "…"]}
```
`etat` ∈ `inactif`, `en_cours`, `termine`; on failure, `ok:false` and `message` gives **the cause and what to do** (§8): "The service rejects the key. Check it."; "The service isn’t responding. Is it running, and on the same network?"; "This model doesn’t exist on this service." `modeles` (up to 50 names, as returned by `…/models`) lets the page offer a list rather than a field to guess. The last test's result is kept in RAM until the next one.

## 8. Error sentences for a third-party service (cause → message)

| What the device observes | Message |
|---|---|
| connection refused / timeout | "The service isn’t responding. Is it running, and on the same network?" |
| host name not found | "This address can’t be found." |
| TLS certificate refused | "This service’s certificate isn’t recognized." |
| `401`, `403` | "The service rejects the key. Check it." |
| `404` on the **real trial** route (step 2 of the test, §7: `chat/completions`, `audio/speech` or `audio/transcriptions`); a `404`/`405` on `…/models` (step 1) is **not** an error | "This address doesn’t look like a compatible service. “/v1” may be missing." |
| `404`/`400` mentioning the model | "This model doesn’t exist on this service." |
| `429` | "The service is limiting requests. Try again in a moment." |
| `5xx` | "The service has a problem on its side." |
| non-conforming response (not JSON, no audio) | "The service replied with something unexpected." |

## 9. Error format

`4xx`/`5xx`: `{"erreur": {"id": "code_faux", "message": "That code isn’t right. Look at the device’s screen.", "champ": "code"}}`. `message` is shown as it is, **in the screen language**. `id` is a stable identifier for tests and debugging, **never shown**, and never translated. `champ` (optional) names the faulty field so that the page can highlight it.

| Status | When | Typical message |
|---|---|---|
| `400` | malformed request (`illisible`, `trop_long`, `trop_court`) | "This request can’t be read." |
| `401` | no valid token (`non_appaire`) | "Enter the code shown on the device’s screen." |
| `403` | wrong code (`code_faux`) | "That code isn’t right. Look at the device’s screen." |
| `404` | unknown `/api/` route (`inconnue`) | "This page doesn’t exist." |
| `408` | request body not received within 5 s (§2) | none: esp_http_server's own answer, outside this format |
| `413` | body too big (`trop_gros`) | "This file is too big for the device." |
| `422` | value refused | one sentence per field (§5, §6, §7) |
| `429` | pairing lock | "Too many attempts. Try again in 60 seconds." |
| `500` | internal fault | "The device ran into a problem. Try again." |
| `501` | function not available yet on this screen (`non_disponible`) | "This feature isn’t available on this screen yet." |
| `503` | no Internet for a place search (`sans_internet`, §5) | "The device has no Internet access to search for a place." |

Outside this format: a **static file** that does not exist (`/lang/xx.json` included, §12) gets a `404` in plain text (`text/plain`, the French word « Introuvable », whatever the screen language); a method the server does not register (only `GET`, `POST`, `PUT` and `DELETE` are) gets esp_http_server's own `405`. Checked in `main/serveur_web.cpp` on 12/11/2026.

## 10. Export and import of the settings

| Route | Role |
|---|---|
| `GET /api/settings/export` | JSON file to download (`Content-Disposition: attachment; filename="reglages-ecran-YYYY-MM-DD.json"`). |
| `POST /api/settings/import?simuler=1` | Checks the file and says what would change, **without applying anything**. |
| `POST /api/settings/import` | Applies. |

Content of the export: zone, display, services — **never** a key, nor a Wi-Fi password (Wi-Fi is not in it at all: another network asks for its own password):
```json
{"format": "ecran-autonome-reglages", "version": 1, "exporte_le": "2026-11-06T12:00:00Z",
 "zone": {"nom": "Lyon", "…": "…", "fuseau": "Europe/Paris"},
 "affichage": {"langue_ecran": "fr", "langue_assistant": "fr", "theme": "auto", "luminosite": 80, "profil": "auto"},
 "services": {"llm": {"preset": "lmstudio", "url": "…", "modele": "…"}},
 "cles_a_ressaisir": ["stt"]}
```
A backup made before the two languages were separated has a single `affichage.langue`: on import it is read as `langue_ecran` (and `langue_assistant` stays as it is). `cles_a_ressaisir` lists the services that had a key: the page says "Remember to re-enter the key for: Understand my voice."

Import: the file goes through the **same validations** as the `PUT`s above, field by field; a file of another `format` or of a newer `version` → `422`, "This file doesn’t come from this screen." Response (simulation or not):
```json
{"simulation": true, "appliques": ["zone", "affichage", "services.llm"],
 "ignores": [{"cle": "services.stt.cle", "message": "Keys are not imported."}],
 "erreurs": [], "cles_a_ressaisir": ["stt"]}
```
A `cle`, `mot_de_passe` or equivalent field in the file is **ignored and reported**, never saved. All validation errors are reported and nothing is applied if `erreurs` is not empty (all or nothing).

## 11. Update through the page, with rollback

| Route | Role |
|---|---|
| `GET /api/update` | Update state. |
| `POST /api/update` | Body = firmware binary image (`Content-Type: application/octet-stream`, `Content-Length` mandatory). |
| `POST /api/update/rollback` | Goes back to the previous version. |

`GET /api/update` →
```json
{"etat": "inactif", "progression": 0, "version": "0.1.0", "validee": true,
 "precedente": {"version": "0.0.9", "disponible": true},
 "dernier_resultat": {"issue": "validee", "version": "0.1.0", "message": "Update successful."}}
```
`etat` ∈ `inactif`, `envoi`, `verification`, `redemarrage`, `echec`; `progression` in % of the upload. `dernier_resultat.issue` ∈ `validee`, `annulee` (automatic rollback: "The new version didn’t start correctly: the old one was restored."), `refusee`.

`POST /api/update`: refuses before writing if `Content-Length` exceeds the destination partition (`413`); refuses an image that is not for this application or this chip (`422`, "This file isn’t firmware for this screen."); writes with `esp_ota_*` into the inactive partition; verifies the image (`esp_ota_end`); answers `200 {"etat":"redemarrage","dans_secondes":3}` then restarts. **A failed upload changes nothing**: the old partition remains the boot one.

**Automatic rollback**: the new version starts "pending validation" (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) and only declares itself valid (`esp_ota_mark_app_valid_cancel_rollback`) after a **self-test** (screen initialised, Wi-Fi tried, HTTP server listening); otherwise the device restarts on the old one. `POST /api/update/rollback` does the same on demand (`200 {"etat":"redemarrage"}`, or `409`, "There is no previous version to restore."). After any restart, the page reconnects by itself by reading `GET /api/status` every 2 s, and shows `dernier_resultat`.

**To be proved on hardware** (plan, phase 3): uploading a deliberately broken image goes back to the old one by rollback. None of this has been observed to date.

## 12. Static files

The front end is served as it is by `esp_http_server`: `GET /` → `index.html`, resources with `Content-Encoding: gzip` and `ETag` = the first 16 hexadecimal digits of the SHA-256 of the gzipped bytes (`tools/embarquer_web.py`), so a file that did not change keeps its tag across firmware versions. In `point_d_acces` mode, any unknown address redirects (`302`) to `/` (captive portal). No `/api/` route is cached.

**Cache and content policy.** The page's files: `Cache-Control: no-cache` (the browser revalidates each time; the `ETag` makes the answer a `304` of a few bytes), `/api/` and the `404`: `no-store`. The files carry `Content-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self' https:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'; object-src 'none'` (constant `http::POLITIQUE_CONTENU`, read as it is by the fake server). No `'unsafe-inline'`: `index.html` has no inline script, no `style` attribute and no `onclick`, and `app.js` builds the page with the DOM, never with HTML text; a script or a style injected into the page is therefore not run (checked by the Playwright run-through). `connect-src … https:` because the page itself reads the list of sea spots from the Vigie address the user set (§15, `https://` required); `img-src 'self'` for the tab icon, the page's icons being `<symbol>` elements of the HTML.

**Language files.** `GET /lang/<code>.json` (`<code>` ∈ `fr`, `en`, `de`, `nl`, `es`, `it`, `tr`) returns the settings page's texts in that language: a flat JSON object `{"<id>": "<text>"}` (`Content-Type: application/json; charset=utf-8`, gzip like the other files). These files are **public** (no token: the page needs them before pairing) and contain only interface texts. They are generated from `i18n/page.yaml` and `i18n/<code>.yaml` by `components/core/outils/generer_i18n.py`, with the fallback (this language, then English, then French) already applied: a file is always complete. An unknown code → `404` (in `point_d_acces` mode, the usual redirect). The page loads one file at a time (between 3,018 bytes for `en` and 3,396 bytes for `de` gzipped at level 9, measured on 12/11/2026 on the files generated in `web/app/lang/` at commit `4b822cf`; 2,067 to 2,338 bytes on 06/11/2026).

## 13. Languages

Seven languages (ADR-0006), in this fixed order and with these **indices, which never change** (they are stored in NVS): `fr` 0, `en` 1, `de` 2, `nl` 3, `es` 4, `it` 5, `tr` 6. A new language takes the next number.

- **Screen language (`langue_ecran`)**: the language of the screen's texts, of the settings page, and of every sentence this API returns (§1 rule 5): error messages, preset names and help, weather labels, test steps, update results. The page follows it: it reads `langue` in `GET /api/status` (public), loads `/lang/<code>.json`, and reloads its texts and the device's sentences when the user changes this setting.
- **Assistant language (`langue_assistant`)**: the language the assistant understands (speech recognition) and speaks. Independent of the screen language; there is no separate "spoken language" field on the STT service.
- **Fallback**, on the device as on the page: the requested language, then English, then the French text itself. A sentence is therefore never empty or replaced by a code. If a language file cannot be loaded, the page uses English, then French; if none loads, it shows the text identifiers (visible and easy to spot).
- **Who wrote the translations**: French is the author's language and the only one he reviews. The six others (en, de, nl, es, it, tr) were written by an AI (Claude) from French and English, **not reviewed by a speaker**; the dates and a few labels are reused as they are from the Tab5 project's language files, which have been used on a real screen. German and Dutch text length on the real screen, and rendering on a board, are **not verified**.
- **Open-Meteo attribution.** Open-Meteo's data are under CC BY 4.0 (research note 03: licence read, attribution wording "not specified"; the plan makes the credit a condition of the licence). The page therefore always shows, in the page's language, "Weather data: Open-Meteo (CC BY 4.0 license)" with a link to `https://open-meteo.com/` (text id `meteo.attribution` in `i18n/page.yaml`). The **device's own screen** shows the credit of the service that really gave the weather (Open-Meteo, MET Norway or the NWS, `fournisseur` of §5) in its « Place and weather » settings card (see [THIRD_PARTY.md](../THIRD_PARTY.md)); whether the home screen must show it as well is not decided in this document.

## 14. Official alerts (Vigie)

Written on 07/11/2026 with [ADR-0009](decisions/0009-alertes-officielles-par-vigie.md). The screen shows the official weather alerts of the author's own service, Vigie, **only while one is in force**; these four settings drive it. They are one group, like the display.

`GET /api/alerts` →
```json
{"actives": true, "adresse": "", "periode_min": 15, "niveau_min": "yellow",
 "adresse_publique": "https://vigie.auxfilsdesidees.fr",
 "niveaux": ["yellow", "orange", "red"]}
```
`PUT /api/alerts` accepts the four modifiable fields (`actives`, `adresse`, `periode_min`, `niveau_min`), each optional (absent = unchanged), and answers like `GET` with the saved values. **All or nothing**: if one value is refused, nothing is saved.

| Field | Rule |
|---|---|
| `actives` | `true` (default) or `false`. `false`: no request at all, no banner, and the answer in memory is forgotten. A string or a number is refused. |
| `adresse` | the base address of the Vigie service, **`https://` required** (`http://` is refused: a warning read in clear could be forged). Empty or blank = the public instance (`adresse_publique`, the default; `adresse` is then returned as `""`). At most 80 bytes, ASCII only, no space, no `?`, `#`, `@` or `\`; a port and a path are allowed; spaces and a trailing `/` around it are removed and the scheme is lower-cased. |
| `periode_min` | whole minutes **from 10 to 180**, default 15. `15.5` is refused, not rounded. The public instance is rate-limited and "not meant to build a service on": the device sends **one request per period**, never a burst at start-up, and backs off after an error. |
| `niveau_min` | `yellow` (default), `orange` or `red`: the lowest level shown on the screen. |

What is sent to the Vigie address: the chosen city's position rounded to three decimals (about 110 m) and the screen language (`GET <adresse>/v1/vigilances?lat=…&lon=…&lang=<screen language>,en`). Nothing else, no key, no device identifier.

`422` errors (field in `champ`): `actives_invalide` ("Alerts must be switched on or off."), `adresse_invalide` ("The Vigie address must start with https://."), `periode_invalide` ("The period must be a whole number of minutes, from 10 to 180."), `niveau_inconnu` ("This alert level doesn't exist.").

Changing a value reaches the network client within about 5 s; it then asks Vigie at once if the request changed, but never twice within one minute.

## 15. Sea (Vigie)

Written on 08/11/2026 with [ADR-0025](decisions/0025-mer-ecriture-du-registre-des-spots-lot-e.md). The sea page of the screen ([ADR-0022](decisions/0022-mer-la-page-lot-c.md)) shows the sea of **one chosen spot**, read from Vigie ([ADR-0019](decisions/0019-mer-client-et-registre-de-spots.md)). The device keeps a short list of spots (12 at most) and says which one is chosen; this group sets them. Until a spot is chosen the device sends nothing to Vigie for the sea, and the long-press wheel has no "Sea" button ([ADR-0024](decisions/0024-mer-l-entree-de-la-roue-lot-d.md)).

`GET /api/sea` →
```json
{"spots": [{"id": "an-id-of-vigies-list", "nom": "A beach"}], "choisi": "an-id-of-vigies-list",
 "vent_min_kn": 12, "vent_max_kn": 25, "spots_max": 12, "vent_plafond_kn": 60}
```
`choisi` is `""` and `spots` is `[]` on a device where nothing was chosen. The values above are **examples of the shape**, not data that was read.

`PUT /api/sea` accepts the four modifiable fields (`spots`, `choisi`, `vent_min_kn`, `vent_max_kn`), each optional (absent = unchanged), and answers like `GET` with the saved values. **All or nothing**: if one value is refused, nothing is saved.

| Field | Rule |
|---|---|
| `spots` | the **whole** list, in the order to keep: 0 to 12 objects `{"id": …, "nom": …}` (other fields of an object are ignored). `id`: the identifier of the spot in Vigie's list, lower-case letters, digits, `-` and `_`, 64 bytes at most. `nom`: the name to show, 1 to 32 characters, well-formed UTF-8, no control character, not only spaces. Two objects with the same `id` are refused. An empty list switches the sea off (no request, no "Sea" button). |
| `choisi` | the `id` of the spot whose sea the screen shows; it must be in the list as it will be after the change. `""` only for an empty list. Absent: the device stays on the same spot if it is still in the list, otherwise on the first one. |
| `vent_min_kn`, `vent_max_kn` | the wind range, in **whole knots**, that Vigie uses for the kite verdict: `0 < min < max <= 60` (defaults 12 and 25). If only one is sent, the pair (the one sent, the one saved) is judged. `12.5` is refused, not rounded. |

**Where the list of spots comes from.** The device does **not** serve it. The page reads it from Vigie itself: `GET <address>/v1/surf/spots`, where the address is `adresse` of `GET /api/alerts` or, when it is empty, `adresse_publique` (51 spots on 08/11/2026, 20 405 bytes, 4 695 gzipped). The page keeps from each spot only its `id`, its `name` and its `place` (to tell two beaches of the same name apart), and sends the device `id` and `nom`. The device does not check that an `id` is in Vigie's list: an unknown spot makes Vigie answer 404 and the device asks again at its normal pace (ADR-0019). Vigie's answers carry the credit of the spot data (OpenStreetMap contributors, ODbL): the page shows it next to the list.

What the device sends to Vigie for the sea: `GET <address>/v1/embedded/frame?device_type=generic&spot=<id>`, plus `&wind_min_kn=…&wind_max_kn=…` when the range is not 12 to 25. No position, no key, no device identifier.

`422` errors (field in `champ`): `spots_invalide` ("The spots must be a list of objects with an identifier and a name."), `spot_invalide` ("This spot identifier isn’t valid."), `nom_invalide` ("A spot name must have 1 to 32 characters."), `spot_double` ("This spot is already in the list."), `trop_de_spots` ("The list is full: 12 spots at most."), `choix_inconnu` ("The chosen spot isn’t in the list."), `vent_invalide` ("The wind range must be whole knots from 1 to 60, the minimum below the maximum."). `champ` is `spots` for the first five, `choisi` for `choix_inconnu`, and for `vent_invalide` `vent_min_kn` when the request carries a `vent_min_kn`, otherwise `vent_max_kn` (the pair is judged as a whole). A request is refused with `400` `illisible` when its body is not a JSON object. A spot name that contains `\u0000` is not refused: the JSON reader turns it into U+FFFD, like everywhere else on the device. Two writers (the page and, later, the screen) are not coordinated: the last write wins.

Changing a value reaches the sea client within about 5 s; it then asks Vigie at once if the request changed, but never twice within one minute.

---

# Français

Rédigé le 06/11/2026 (piste B), langues ajoutées le 06/11/2026. **Contrat de conception ; le firmware en sert un premier sous-ensemble (encadré ci-dessous), les autres routes n'existent pas encore sur l'appareil.** Le front et le faux serveur de `web/` l'implémentent en entier ; le firmware devra le respecter tel quel. Tout ce qui est écrit ici comme comportement de l'appareil est **non vérifié** tant qu'il n'a pas été constaté sur une carte.

Décisions d'origine : [ADR-0002](decisions/0002-assistant-vocal-en-cascade.md) (trois services), [ADR-0003](decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md) (page servie par l'appareil, HTTP local, jeton d'appairage, secrets, mise à jour), [ADR-0006](decisions/0006-ecran-en-sept-langues.md) (sept langues).

> **État du firmware — jalon « réglages », 06/11/2026 : sous-ensemble V1, mode station Wi-Fi seulement (ni point d'accès, ni portail captif).** Servi par `main/serveur_web.cpp` sur le port 80 ; la logique est dans le noyau pur (`components/core/include/ecran/api_reglages.hpp`), testée sur le PC avec des mutations. **Compilé, pas encore constaté sur une carte** (voir [mesures/reglages.md](mesures/reglages.md)).
> - **Réel** : `GET /api/status`, `POST` et `DELETE /api/pair`, `GET /api/system`, `GET` et `PUT /api/location`, `GET` et `PUT /api/display`, `GET` et `PUT /api/alerts` (§14), `GET` et `PUT /api/sea` (§15), `GET /api/weather`, `GET /api/geocode`, `GET /api/wifi` (lecture seule), les fichiers statiques (gzip).
> - **Vide, sans erreur** (la page s'ouvre sans message rouge) : `GET /api/services` (rien de réglé), `GET /api/presets` (aucun), `GET /api/update` (`inactif`).
> - **`501` `non_disponible`**, avec la phrase « Cette fonction n’est pas encore disponible sur cet écran. » dans la langue de l'écran : scan et changement de Wi-Fi, enregistrement et test des services STT / LLM / TTS, export / import, mise à jour et retour arrière.
> - **Écarts connus avec le texte ci-dessous** : `GET /api/geocode` est **synchrone pour la page** (la requête attend la réponse jusqu'à 30 s, donc garde l'unique tâche du serveur : la règle 7 n'est pas tenue pour cette seule route), mais le travail réseau est fait par la tâche d'arrière-plan du module de recherche, jamais par celle du serveur ni par celle de l'écran ; au-delà de 5 connexions ouvertes, la plus ancienne est fermée au lieu de répondre `503` (règle 8) ; `validee` vaut toujours `true` (pas encore de retour arrière) ; `compile_le` est l'heure locale du PC de compilation, sans « Z » ; le `fuseau` rendu est celui de la ville enregistrée, et c'est aussi celui que l'horloge de l'écran utilise (ADR-0012) ; un fuseau absent de la table de 468 noms n'affiche aucune heure à l'écran (jamais l'heure de Paris), et `Africa/Casablanca` est une limite connue de cette table.

## 1. Règles qui valent pour toutes les routes

1. **Une clé API n'est jamais lisible.** Elle n'entre dans l'appareil que par le corps d'une requête `PUT` ou `POST` (jamais dans l'adresse), et n'en sort **jamais** : ni dans une réponse, ni dans l'export, ni dans un message d'erreur, ni dans le journal. Les réponses disent seulement `"cle_definie": true|false`. Aucun masque partiel (« …a3f9 ») : même les derniers caractères ne sont pas renvoyés. Le mot de passe Wi-Fi suit la même règle.
2. **Corps de réponse d'un service tiers jamais recopié.** Un service qui répond « clé invalide : sk-abc…xyz » peut citer la clé. L'appareil traduit le statut HTTP reçu en une phrase à lui (§8), il ne transmet pas le texte reçu.
3. **JSON UTF-8** (`Content-Type: application/json; charset=utf-8`), accents et « ° » tels quels. `Cache-Control: no-store` sur tout `/api/`. Noms de champs en français sans accent (`mot_de_passe`, `luminosite`), valeurs techniques (`fuseau`, `latitude`) inchangées.
4. **Même origine seulement.** Pas d'en-tête CORS : la page est servie par l'appareil. (Le faux serveur de développement peut l'ajouter.)
5. **Erreurs = une phrase courte dans la langue de l'écran** (§13), jamais un code brut ni une trace. Format unique, voir §9. La page n'affiche que `message`.
6. **Authentification** : toutes les routes sauf `GET /api/status`, `POST /api/pair` et les fichiers de langue `GET /lang/<code>.json` (§12) exigent `Authorization: Bearer <jeton de session>` (§2). Sans cela : `401`.
7. **Opérations longues = asynchrones** (`202` puis lecture de l'état) parce que `esp_http_server` traite les requêtes dans une seule tâche par défaut : un « tester » de 15 s ne doit pas figer la page de l'appareil. Concerne le scan Wi-Fi, la connexion Wi-Fi, les tests de service et la mise à jour.
8. **Limites** : corps JSON ≤ 4 Ko (import ≤ 16 Ko) ; 4 clients simultanés au plus (au-delà : `503`, « L’appareil est occupé, réessayez dans un instant. »).
9. **L'en-tête `Host` doit nommer l'appareil** (protection contre le rebinding DNS : une page d'Internet dont le nom pointe soudain vers l'adresse locale de l'appareil enverrait ses requêtes avec *son* nom). Acceptés, sans tenir compte de la casse, avec ou sans `:80` : l'adresse IPv4 de l'appareil sur le Wi-Fi de la maison, son nom mDNS `ecran-xxxx.local` (un point final est toléré) et, quand il existera, son adresse sur son propre point d'accès d'installation. Tout le reste, sur toutes les routes, fichiers statiques compris → `403`, la seule ligne `Forbidden` (`text/plain`, rien sur l'appareil), connexion fermée ; le corps n'est pas lu. Conséquence : un nom donné par une box (`ecran-xxxx.home`, `ecran-xxxx.fritz.box`…) est refusé aussi : utiliser l'adresse ou le nom `.local` affichés à l'écran. En mode `point_d_acces` (pas encore fait), la redirection du portail captif du §12 répondra aux noms étrangers à la place. La décision est `http::hote_autorise` dans le noyau pur (`components/core/include/ecran/http_requete.hpp`), testée sur le PC avec des mutations ; le faux serveur applique la même règle (ses noms sont `127.0.0.1` et `localhost` sur son port).
10. **En-têtes de sécurité sur toutes les réponses** : `X-Content-Type-Options: nosniff` et `Referrer-Policy: no-referrer` (l'adresse de la page n'est envoyée à personne, pas même à l'adresse de Vigie que la page lit) ; les fichiers de la page portent en plus la politique de contenu du §12.

## 2. Appairage et session

Le jeton d'appairage est un **code à 6 chiffres affiché sur l'écran de l'appareil** (ADR-0003). La page le demande une fois, l'échange contre un jeton de session et le garde en `sessionStorage` (jamais dans l'adresse).

| Route | Rôle |
|---|---|
| `GET /api/status` | Public. `{"nom","modele","version","mode","appairage","langue"}` : `mode` = `station` (sur le Wi-Fi de la maison) ou `point_d_acces` (mise en service) ; `appairage` = `"requis"` ou `"ok"` selon que la requête porte un jeton valide ; `langue` = code de la **langue de l'écran** (§6), pour que la page parle la bonne langue avant l'appairage. `modele` = le nom de la carte sur laquelle tourne le firmware, lu dans la couche carte (« M5Stack Tab5 » ou « Waveshare 7B »). Rien d'autre (pas d'adresse IP, pas de SSID). |
| `POST /api/pair` | Corps `{"code":"123456"}` → `200 {"jeton":"<32 octets hex>"}`. Mauvais code → `403`, « Ce code n’est pas le bon. Regardez l’écran de l’appareil. ». **5 essais ratés → verrou** (`429`, « Trop d’essais. Réessayez dans 60 secondes. », avec `reessayer_dans` et `Retry-After`). Le verrou dure 60 s, puis double à chaque nouvelle série de 5 échecs sans succès entre deux (120 s, 240 s… une heure au plus). À partir de la 3e série de suite, chaque verrou tire aussi un **nouveau code**, affiché à l'écran ; les sessions déjà ouvertes restent. Un succès ramène le verrou à 60 s. Le verrou est global (pas par client). |
| `DELETE /api/pair` | Ferme la session courante → `204`. |

Le jeton de session vit en RAM (4 sessions au plus, la plus ancienne est fermée si une cinquième arrive) et disparaît au redémarrage : la page redemande alors le code. Le code change au redémarrage, après la 3e série d'échecs de suite (ci-dessus), et quand on touche « Nouveau code » sur l'écran : ce bouton **ferme aussi toutes les sessions ouvertes** et **lève un verrou en cours**, pour que la personne devant l'écran ne soit jamais enfermée dehors par quelqu'un d'autre sur le réseau. Le nouveau code s'affiche tout de suite ; l'appareil l'applique avant la requête suivante (l'ancien est alors refusé).

Une requête vers une route protégée sans jeton valide reçoit son `401` **sans que son corps soit lu**, et la connexion est fermée. Le corps d'une requête doit arriver en 5 s (5 s sans un octet, ou le corps incomplet au bout de 5 s → `408`, connexion fermée) ; un téléphone sur le même Wi-Fi l'envoie en quelques millisecondes.

**Limite assumée** : HTTP en clair sur le réseau local, le jeton peut être lu par quiconque écoute ce réseau (HTTPS auto-signé écarté, ADR-0003).

## 3. Appareil

`GET /api/system` →
```json
{
  "nom": "ecran-1a2b",
  "modele": "M5Stack Tab5",
  "version": "0.1.0",
  "compile_le": "2026-11-06T11:20:00Z",
  "partition": "ota_0",
  "validee": true,
  "depuis_secondes": 5421,
  "memoire_libre": 5120000,
  "heure": {"epoch": 1793960400, "synchronisee": true, "fuseau": "Europe/Paris"},
  "internet": true
}
```
`validee` est faux tant que la nouvelle version n'a pas passé son auto-test (§11). `internet` est vrai dès que l'appareil a une adresse IP **et** a reçu l'heure du réseau (SNTP) : c'est sa preuve d'accès, pas le succès de la dernière requête (un service météo en échec depuis des heures le laisse à `true` ; une heure SNTP tardive le laisse à `false` alors que la météo est déjà arrivée). `memoire_libre` est la RAM **interne** libre en octets (`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`), pas la PSRAM : quelques centaines de kilo-octets sur un vrai appareil, pas les 5 Mo de l'exemple.

## 4. Wi-Fi

| Route | Rôle |
|---|---|
| `GET /api/wifi` | État courant. |
| `POST /api/wifi/scan` | Lance une recherche de réseaux → `202 {"en_cours":true}`. |
| `GET /api/wifi/scan` | `{"en_cours":false,"reseaux":[{"ssid":"Maison","niveau":4,"force":-55,"securise":true}]}` ; `niveau` 0 à 4 (affichage), `force` en dBm, triés du plus fort au plus faible, doublons fusionnés. |
| `PUT /api/wifi` | Corps `{"ssid":"Maison","mot_de_passe":"…"}` (mot de passe vide permis pour un réseau ouvert) → `202 {"etat":"connexion"}`. |

`GET /api/wifi` →
```json
{"etat": "connecte", "ssid": "Maison", "niveau": 3, "force": -61, "ip": "192.168.1.52",
 "message": "Connecté à « Maison »."}
```
(`192.168.1.52` est une adresse d'exemple, pas celle d'un vrai réseau.) `etat` ∈ `connecte`, `connexion`, `echec`, `deconnecte`, `point_d_acces`. Après `PUT`, la page lit cet état toutes les 1,5 s jusqu'à `connecte` ou `echec` (20 s au plus). Sur `echec`, l'appareil **revient au réseau précédent** (ou au point d'accès de mise en service) et le dit : « Mot de passe refusé par « Maison ». L’ancien réseau est conservé. » **Cas du point d'accès** : en changeant de réseau, le téléphone perd l'appareil. Le `202` porte alors `"adresse_apres": "http://ecran-1a2b.local"` et la page affiche « Reconnectez votre téléphone à votre Wi-Fi, puis ouvrez http://ecran-1a2b.local. » (portail captif et mDNS : non démontrés sur P4+C6, voir plan, jalon 1d).

## 5. Zone météo et fuseau

| Route | Rôle |
|---|---|
| `GET /api/geocode?q=Lyon` | Recherche de lieu par un **proxy du firmware** vers le géocodage d'Open-Meteo. |
| `GET /api/location` | Zone enregistrée et sa source météo. |
| `PUT /api/location` | Enregistre la zone choisie et/ou la source météo. |
| `GET /api/weather` | Aperçu de la météo de la zone enregistrée (pour que la page montre « ça marche »). |

**Recherche.** `q` : **3 caractères au moins** après `trim` (sinon `400`, « Tapez au moins 3 lettres. »), 80 au plus, en UTF-8 encodé pour l'adresse. Le firmware appelle `geocoding-api.open-meteo.com/v1/search` avec `name=<q>`, `count=8`, `language=<code de la langue de l'écran>`, `format=json` (paramètres et champs lus le 06/11/2026 dans open-meteo.com/en/docs/geocoding-api : `count` 10 par défaut et 100 au plus ; la réponse met les lieux dans `results` avec `name`, `admin1`, `country`, `country_code`, `latitude`, `longitude`, `elevation`, `timezone`, `population` ; les champs vides sont omis ; erreur = JSON `error`/`reason` en `400`. Que ce géocodage accepte chacun des sept codes de langue : **non vérifié**. Le firmware renomme les champs comme ci-dessous) et renvoie une liste réduite :
```json
{"resultats": [
  {"id": "2996944", "nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR",
   "latitude": 45.74846, "longitude": 4.84671, "altitude": 173, "fuseau": "Europe/Paris",
   "population": 522250, "libelle": "Lyon, Auvergne-Rhône-Alpes, France"}
]}
```
Liste vide → `200 {"resultats":[]}` (la page dit « Aucun lieu trouvé pour « Lyonn ». »). Sans Internet côté appareil → `503`, « L’appareil n’a pas accès à Internet pour chercher un lieu. ». Les valeurs ci-dessus sont des **exemples de forme**, pas des données lues. Un résultat sans `fuseau` est écarté de la liste. `altitude` n'est envoyée que si le service en donne une plausible, **entre −500 m et 9 000 m** : Open-Meteo écrit `-9999` quand l'altitude est inconnue (vu sur une réponse réelle, « Istanbul Old Town », 06/11/2026), et cette valeur n'est pas transmise.

**Un seul module de recherche pour la page et l'écran** (`main/geocodage.cpp` ; lecture et états dans le noyau pur `components/core/include/ecran/geocodage.hpp`). Le `GET /api/geocode` de la page et la recherche de l'écran (réglages et installation guidée) utilisent le **même** lecteur : les deux affichent les mêmes lieux avec les mêmes libellés. Une seule recherche à la fois, **la dernière demande gagne**, et le téléchargement partage un verrou avec la requête météo (`main/verrou_tls.h`) : une recherche de ville et une requête météo ne se font jamais en même temps (une poignée de main TLS prend environ 65 Ko des ~110 Ko de RAM interne restants). Le travail réseau ne se fait jamais dans la tâche de l'écran. Si le verrou n'est pas libre au bout de 15 s, la recherche se termine en « sans connexion ». Le texte tapé n'est jamais écrit au journal. **Lecteur et états testés sur le PC avec de vraies réponses capturées (7 réponses d'Open-Meteo) ; firmware compilé ; jamais exécuté sur une carte. Le client météo ne prend pas encore le verrou TLS** (voir [mesures/reglages.md](mesures/reglages.md) §5).

**Enregistrement.** `PUT /api/location` reçoit un résultat de recherche tel quel (champs `nom`, `region`, `pays`, `code_pays`, `latitude`, `longitude`, `fuseau`; `altitude` facultatif ; `source_meteo` facultatif, voir plus bas) :
- `latitude` ∈ [−90, 90], `longitude` ∈ [−180, 180] (sinon `422`, « Ces coordonnées ne sont pas valides. ») ;
- `fuseau` doit être un **nom IANA présent dans la table embarquée** (`posix_tz_db`, noyau pur) ; sinon `422`, « Ce fuseau horaire n’est pas connu de l’appareil. » ;
- l'appareil garde le **nom IANA** (en NVS, pour retrouver l'heure sans réseau) et en déduit la chaîne POSIX, applique `TZ`, relance la météo, puis répond avec la zone relue :
```json
{"nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR",
 "latitude": 45.74846, "longitude": 4.84671, "fuseau": "Europe/Paris",
 "posix": "CET-1CEST,M3.5.0,M10.5.0/3", "libelle": "Lyon, Auvergne-Rhône-Alpes, France"}
```
`GET /api/location` renvoie la même forme, ou `{"definie": false}` tant qu'aucune zone n'est choisie (l'accueil laisse alors le lieu vide ; la phrase « Choisissez une ville dans la page de réglages. » existe dans la table de traduction mais aucun code de l'écran ne l'affiche, vérifié le 12/11/2026).

**Source météo** (ADR-0010). La même route porte la source météo de la ville enregistrée. `GET /api/location` (et toutes les réponses de `PUT /api/location`) ajoute `source_meteo` (identifiant stable de la source en service : `auto`, ou un modèle national comme `knmi`), `source_meteo_nom` (le nom à afficher : « Automatique » dans la langue de l'écran, un nom de service tel quel), `source_meteo_retablie` (voir plus bas) et `sources_meteo`, la liste **proposée pour le pays de la ville enregistrée**, « Automatique » en tête, puis le service national, puis le modèle mondial (l'écran propose exactement la même liste) :
```json
{"source_meteo": "knmi", "source_meteo_nom": "KNMI", "source_meteo_retablie": false,
 "sources_meteo": [{"id": "auto", "nom": "Automatique", "direct": false}, {"id": "knmi", "nom": "KNMI", "direct": false}, {"id": "noaa", "nom": "NOAA GFS", "direct": false}]}
```
`{"definie": false}` porte les mêmes champs (sans pays connu, la liste est « Automatique » et le modèle mondial seulement). `PUT /api/location` accepte `source_meteo`, l'identifiant choisi dans cette liste, **seul** (`{"source_meteo": "knmi"}` : la ville ne change pas) ou **avec une ville** (la source est alors jugée sur le pays de la NOUVELLE ville ; si l'une des deux est refusée, rien n'est enregistré). Un identifiant qui n'existe pas, ou qui n'est pas une chaîne : `422`, « Cette source météo n’existe pas. », champ `source_meteo`. Une source connue que le pays ne propose pas : `422`, « Cette source météo ne couvre pas le pays de la ville. », même champ. Changer de source se passe comme changer de ville : la météo gardée sur l'appareil est effacée, une nouvelle requête part, l'écran se met à jour sans redémarrage. **Quand la ville passe dans un pays que la source actuelle ne couvre pas, l'appareil revient sur « Automatique » et le dit** : `source_meteo_retablie` vaut `true` dans cette réponse et dans les `GET` suivants, jusqu'à ce qu'une source soit choisie ou que la ville change encore, et l'écran affiche la même phrase (« Source remise sur « Automatique » : l’ancienne ne couvre pas ce pays. »). La plupart des sources sont des modèles de prévision d'Open-Meteo ; « Automatique » est la requête sans paramètre `models`, exactement comme avant ce réglage. Deux sources sont des services que l'appareil lit lui-même, sans clé (ADR-0014) : MET Norway (`metnorway` : Norvège, Finlande, Islande, Svalbard) et le NWS (`nws` : États-Unis, Porto Rico). Leur entrée de `sources_meteo` porte `"direct": true` (`false` pour toutes les autres) : un client ne les présente donc pas comme un « modèle » d'Open-Meteo ; quand un tel service ne répond pas, la météo vient d'Open-Meteo « Automatique » en attendant (voir `GET /api/weather`). Les modèles proposés, et pourquoi les autres ne le sont pas, sont dans [mesures/meteo-modeles.md](mesures/meteo-modeles.md).

**Aperçu météo.** `GET /api/weather` →
```json
{"disponible": true, "perimee": false, "mise_a_jour": 1793960400,
 "fournisseur": {"id": "open-meteo", "nom": "Open-Meteo", "lien": "https://open-meteo.com/"}, "repli": false,
 "actuelle": {"temperature": 14.2, "code": 2, "libelle": "Partiellement nuageux", "jour": true}}
```
`code` = code WMO (celui d'Open-Meteo ; les conditions de MET Norway et du NWS sont ramenées aux mêmes codes), `libelle` = libellé court du noyau pur (table WMO) **dans la langue de l'écran** (§13), jamais le code seul. `perimee` vrai quand la dernière lecture a plus de **2 h**, ou quand les **3 dernières requêtes ont échoué de suite** et qu'elle a plus de 30 min, ou quand l'horloge de l'appareil ne permet pas d'en juger (pas encore réglée, ou revenue en arrière) (`meteo_synthese.hpp` : `DELAI_PERIME_APPAREIL_S`, `ECHECS_POUR_PERIME`, `AGE_PERIME_PAR_ECHECS_S`) ; l'objet `actuelle` n'a pas de température ressentie (`ressentie`) : l'appareil ne l'envoie pas ; `{"disponible": false}` avant la première lecture réussie. `fournisseur` nomme le service qui a **réellement donné** cette météo (`id` : `open-meteo`, `met-norway` ou `nws` ; `nom` et `lien` : ce que l'attribution affiche, la licence l'exige partout où la météo est montrée). Ce n'est pas toujours la source choisie : `repli` vaut `true` quand la source choisie est un service lu directement qui n'a pas répondu, la météo vient alors d'Open-Meteo « Automatique » en attendant. L'écran et la page le disent (« … est indisponible : la météo vient d’Open-Meteo en attendant. ») et citent Open-Meteo, pas le service en échec (ADR-0014).

## 6. Affichage et langues

`GET /api/display` →
```json
{"langue_ecran": "fr", "langue_assistant": "fr", "theme": "auto", "luminosite": 80, "profil": "auto",
 "langues": [{"code": "fr", "nom": "Français"}, {"code": "en", "nom": "English"}, {"code": "de", "nom": "Deutsch"},
             {"code": "nl", "nom": "Nederlands"}, {"code": "es", "nom": "Español"}, {"code": "it", "nom": "Italiano"},
             {"code": "tr", "nom": "Türkçe"}],
 "langues_assistant": [{"code": "fr", "nom": "Français"}, "… même liste …"],
 "themes": ["auto", "clair", "sombre"],
 "profils": ["auto", "meteo"]}
```
Il y a **deux réglages de langue distincts** (ADR-0006) : `langue_ecran` (la langue des textes de l'écran, de la page de réglages et des phrases de l'appareil) et `langue_assistant` (la langue que l'assistant vocal comprend et parle, §7). Les deux valent `fr` par défaut. `PUT /api/display` accepte les cinq champs modifiables (`langue_ecran`, `langue_assistant`, `theme`, `luminosite`, `profil`), chacun facultatif (absent = inchangé) ; `langues`, `langues_assistant`, `themes` et `profils` sont en lecture seule : **les listes viennent de l'appareil**, la page ne les fixe pas. `nom` est le nom de la langue **dans cette langue**. Un code absent de la liste → `422`, « Cette langue n’est pas disponible pour l’écran. » (`langue_ecran`) ou « Cette langue n’est pas disponible pour l’assistant. » (`langue_assistant`). `luminosite` : entier 5 à 100 (en %) ; le plancher à 5 évite un écran rendu illisible depuis la page (sinon `422`, « La luminosité doit être entre 5 et 100 %. »). `profil` est le profil de l'écran d'accueil (ADR-0023) : `auto` (l'écran choisit seul, d'après la pluie et les alertes officielles, ADR-0015) ou un profil épinglé ; `profils` ne liste **que les profils que l'appareil sait dessiner** (aujourd'hui `auto` et `meteo`), et `auto` est le défaut, comme la valeur de repli d'une valeur abîmée en mémoire (jamais un profil épinglé par accident). Un code absent de `profils` → `422`, « Ce profil n’existe pas. » (`profil_inconnu`). Épingler ne cache pas une alerte officielle orange ou rouge. Les changements s'appliquent à l'écran dès la réponse. Après un changement de `langue_ecran`, les réponses suivantes (messages, noms et aides des préréglages, libellé météo, étapes de test) sont dans la nouvelle langue et la page recharge ses textes (§13). L'ancien champ unique `langue` n'existe plus (voir §10 pour les anciennes sauvegardes).

## 7. Services STT / LLM / TTS

Trois services **indépendants** (ADR-0002), identifiés par `stt`, `llm`, `tts`, chacun compatible OpenAI, local ou cloud. Routes appelées par l'appareil sous l'URL de base : `…/audio/transcriptions` (STT), `…/chat/completions` en streaming (LLM), `…/audio/speech` (TTS), plus `…/models` pour le test.

| Route | Rôle |
|---|---|
| `GET /api/presets` | Préréglages **fournis par l'appareil** (la page n'en code aucun). |
| `GET /api/services` | Les trois services enregistrés, sans clé. |
| `PUT /api/services/{stt\|llm\|tts}` | Enregistre un service. |
| `POST /api/services/{id}/test` | Lance un test → `202`. |
| `GET /api/services/{id}/test` | Résultat du dernier test. |

`GET /api/presets` → `{"stt": [Preset…], "llm": […], "tts": […]}` avec
```json
{"id": "lmstudio", "nom": "LM Studio (mon PC)", "url": "http://localhost:1234/v1", "local": true,
 "cle_requise": false, "modele": "", "aide": "Lancez le serveur local de LM Studio sur le PC."}
```
`nom` et `aide` sont dans la langue de l'écran (§13) ; les noms de marque (OpenAI, Google) ne se traduisent pas. `local: true` marque un service qui tourne sur le PC de l'utilisateur : la page rappelle alors de remplacer `localhost` par l'adresse du PC (rien n'est bloqué, c'est un conseil).
`id` ∈ `openai`, `google`, `lmstudio`, `ollama`, `kokoro`, `personnalise` (ADR-0002 + `kokoro`) ; **un préréglage n'existe que pour les services que son fournisseur documente** :

| Préréglage | Adresse de base | Services | Source (lue le 06/11/2026) |
|---|---|---|---|
| `openai` | `https://api.openai.com/v1` | stt, llm, tts | developers.openai.com/api/reference (routes `/models`, `/chat/completions`, `/audio/transcriptions`, `/audio/speech`) |
| `google` | `https://generativelanguage.googleapis.com/v1beta/openai/` | **llm seulement** | aistudio.google.com/docs/openai : `/models` et `/chat/completions` documentés ; **`/audio/transcriptions` et `/audio/speech` non documentés** (écart avec l'ADR-0002, voir le rapport de piste B) |
| `lmstudio` | `http://localhost:1234/v1` (à remplacer par l'adresse du PC) | llm | lmstudio.ai/docs/developer/openai-compat : aucune route audio listée ; le port 1234 vient des exemples de la doc, « défaut officiel » non vérifié |
| `ollama` | `http://localhost:11434/v1` (à remplacer par l'adresse du PC) | llm, stt, tts | docs.ollama.com/api/openai-compatibility : `/v1/audio/transcriptions` et `/v1/audio/speech` listées ; quels modèles les servent : non vérifié |
| `kokoro` | `http://localhost:8880/v1` | tts | README de remsky/Kokoro-FastAPI (source unique) |
| `personnalise` | libre | stt, llm, tts | |

`localhost` n'a aucun sens depuis l'écran : la page demande l'adresse du PC (ex. `192.168.1.20`, un exemple) et la substitue avant d'enregistrer. Aucun nom de modèle n'est proposé de mémoire : le champ `modele` reste vide et `modeles` du test (§ ci-dessous) propose la liste réelle du service. Ajout par rapport à l'ADR-0002 : le préréglage `kokoro` (voix française locale, recherche 02) existe parce que c'est le TTS local prévu.

`GET /api/services` →
```json
{"llm": {"preset": "lmstudio", "url": "http://192.168.1.20:1234/v1", "modele": "…",
         "cle_definie": false, "configure": true},
 "stt": {"preset": "openai", "url": "…", "modele": "…", "cle_definie": true, "configure": true},
 "tts": {"preset": "personnalise", "url": "…", "modele": "…", "voix": "…", "cle_definie": false, "configure": true}}
```
`voix` n'existe que pour `tts`. **Il n'y a plus de champ `langue`** (il existait pour `stt`) : la reconnaissance vocale prend `langue_assistant` (§6), envoyée comme champ `language` de la requête de transcription. Un service jamais réglé : `{"configure": false, "cle_definie": false}`.

`PUT /api/services/{id}` : corps = les champs à changer.

| Champ | Règle |
|---|---|
| `preset` | l'un des `id` ci-dessus ; |
| `url` | `http://` ou `https://`, sans identifiant dans l'adresse (`user:mdp@` refusé : « Retirez l’identifiant de l’adresse : la clé se saisit à part. »), 200 caractères au plus, le `/` final est retiré ; |
| `modele`, `voix` | texte court (80 caractères) ; un champ `langue` envoyé par un ancien client est ignoré ; |
| `cle` | **écriture seule.** Présent et non vide = remplace la clé ; **absent = clé inchangée** ; |
| `effacer_cle` | `true` = supprime la clé enregistrée. |

Réponse : le service relu (même forme que `GET`), donc **sans** `cle`. Changer `preset` ne conserve pas la clé du préréglage précédent : elle est effacée (une clé OpenAI ne doit jamais partir vers un autre serveur), sauf si le même corps en apporte une nouvelle.

**Test.** `POST /api/services/{id}/test` accepte un corps facultatif `{"url","modele","voix","cle"}` : il essaie ces valeurs **sans les enregistrer** (la page teste ce qui est dans le formulaire) ; un champ absent prend la valeur enregistrée, `cle` compris. Le test fait, dans l'ordre :
1. **Joindre** : `GET {url}/models` (une réponse `404`/`405` compte comme « joint, mais ne liste pas ses modèles » : service local valide) ;
2. **Essai réel minimal** : LLM = un échange d'un jeton ; TTS = synthèse du mot fixe « Bonjour » (la durée et la taille reçues sont rapportées, rien n'est joué) ; STT = transcription d'une demi-seconde de silence, dans la langue de l'assistant.

`GET /api/services/{id}/test` →
```json
{"etat": "termine", "ok": true, "duree_ms": 412,
 "message": "Le service répond en 0,4 s.",
 "etapes": [{"nom": "Joindre le service", "ok": true},
            {"nom": "Essai de réponse", "ok": true}],
 "modeles": ["…", "…"]}
```
`etat` ∈ `inactif`, `en_cours`, `termine` ; en échec, `ok:false` et `message` donne **la cause et le geste** (§8) : « Le service refuse la clé. Vérifiez-la. » ; « Le service ne répond pas. Est-il lancé et sur le même réseau ? » ; « Ce modèle n’existe pas sur ce service. » `modeles` (jusqu'à 50 noms, tels que renvoyés par `…/models`) permet à la page de proposer une liste plutôt qu'un champ à deviner. Le résultat du dernier test est gardé en RAM jusqu'au suivant.

## 8. Phrases d'erreur d'un service tiers (cause → message)

| Ce que l'appareil observe | Message |
|---|---|
| connexion refusée / délai dépassé | « Le service ne répond pas. Est-il lancé et sur le même réseau ? » |
| nom d'hôte introuvable | « Cette adresse est introuvable. » |
| certificat TLS refusé | « Le certificat de ce service n’est pas reconnu. » |
| `401`, `403` | « Le service refuse la clé. Vérifiez-la. » |
| `404` sur la route de l'**essai réel** (étape 2 du test, §7 : `chat/completions`, `audio/speech` ou `audio/transcriptions`) ; un `404`/`405` sur `…/models` (étape 1) n'est **pas** une erreur | « Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v1 ». » |
| `404`/`400` mentionnant le modèle | « Ce modèle n’existe pas sur ce service. » |
| `429` | « Le service limite les appels. Réessayez dans un instant. » |
| `5xx` | « Le service a un problème de son côté. » |
| réponse non conforme (pas du JSON, pas d'audio) | « Le service a répondu autre chose que prévu. » |

## 9. Format d'erreur

`4xx`/`5xx` : `{"erreur": {"id": "code_faux", "message": "Ce code n’est pas le bon. Regardez l’écran de l’appareil.", "champ": "code"}}`. `message` est affiché tel quel, **dans la langue de l'écran**. `id` est un identifiant stable pour les tests et la mise au point, **jamais affiché**, jamais traduit. `champ` (facultatif) désigne le champ fautif pour que la page le surligne.

| Statut | Quand | Message type |
|---|---|---|
| `400` | requête mal formée (`illisible`, `trop_long`, `trop_court`) | « Cette demande n’est pas lisible. » |
| `401` | pas de jeton valide (`non_appaire`) | « Entrez le code affiché sur l’écran de l’appareil. » |
| `403` | mauvais code (`code_faux`) | « Ce code n’est pas le bon. Regardez l’écran de l’appareil. » |
| `404` | route de `/api/` inconnue (`inconnue`) | « Cette page n’existe pas. » |
| `408` | corps de la requête pas reçu en 5 s (§2) | aucun : réponse d'esp_http_server lui-même, hors de ce format |
| `413` | corps trop gros (`trop_gros`) | « Ce fichier est trop gros pour l’appareil. » |
| `422` | valeur refusée | une phrase par champ (§5, §6, §7) |
| `429` | verrou de l'appairage | « Trop d’essais. Réessayez dans 60 secondes. » |
| `500` | défaut interne | « L’appareil a rencontré un problème. Réessayez. » |
| `501` | fonction pas encore disponible sur cet écran (`non_disponible`) | « Cette fonction n’est pas encore disponible sur cet écran. » |
| `503` | pas d'Internet pour chercher un lieu (`sans_internet`, §5) | « L’appareil n’a pas accès à Internet pour chercher un lieu. » |

Hors de ce format : un **fichier statique** qui n'existe pas (`/lang/xx.json` compris, §12) reçoit un `404` en texte brut (`text/plain`, le mot français « Introuvable », quelle que soit la langue de l'écran) ; une méthode que le serveur n'inscrit pas (seules `GET`, `POST`, `PUT` et `DELETE` le sont) reçoit le `405` d'esp_http_server lui-même. Vérifié dans `main/serveur_web.cpp` le 12/11/2026.

## 10. Export et import des réglages

| Route | Rôle |
|---|---|
| `GET /api/settings/export` | Fichier JSON à télécharger (`Content-Disposition: attachment; filename="reglages-ecran-AAAA-MM-JJ.json"`). |
| `POST /api/settings/import?simuler=1` | Vérifie le fichier et dit ce qui changerait, **sans rien appliquer**. |
| `POST /api/settings/import` | Applique. |

Contenu de l'export : zone, affichage, services — **jamais** de clé, ni de mot de passe Wi-Fi (le Wi-Fi n'y figure pas du tout : un autre réseau demande son propre mot de passe) :
```json
{"format": "ecran-autonome-reglages", "version": 1, "exporte_le": "2026-11-06T12:00:00Z",
 "zone": {"nom": "Lyon", "…": "…", "fuseau": "Europe/Paris"},
 "affichage": {"langue_ecran": "fr", "langue_assistant": "fr", "theme": "auto", "luminosite": 80, "profil": "auto"},
 "services": {"llm": {"preset": "lmstudio", "url": "…", "modele": "…"}},
 "cles_a_ressaisir": ["stt"]}
```
Une sauvegarde faite avant la séparation des deux langues porte un seul `affichage.langue` : à l'import, il est lu comme `langue_ecran` (et `langue_assistant` reste telle quelle). `cles_a_ressaisir` liste les services qui avaient une clé : la page dit « Pensez à ressaisir la clé de : Comprendre ma voix. ».

Import : le fichier passe par les **mêmes validations** que les `PUT` ci-dessus, champ par champ ; un fichier d'un autre `format` ou d'une `version` plus récente → `422`, « Ce fichier ne vient pas de cet écran. ». Réponse (simulation ou non) :
```json
{"simulation": true, "appliques": ["zone", "affichage", "services.llm"],
 "ignores": [{"cle": "services.stt.cle", "message": "Les clés ne s’importent pas."}],
 "erreurs": [], "cles_a_ressaisir": ["stt"]}
```
Un champ `cle`, `mot_de_passe` ou équivalent dans le fichier est **ignoré et signalé**, jamais enregistré. Toutes les erreurs de validation sont rapportées et rien n'est appliqué si `erreurs` n'est pas vide (tout ou rien).

## 11. Mise à jour par la page, avec retour arrière

| Route | Rôle |
|---|---|
| `GET /api/update` | État de la mise à jour. |
| `POST /api/update` | Corps = image binaire du firmware (`Content-Type: application/octet-stream`, `Content-Length` obligatoire). |
| `POST /api/update/rollback` | Revient à la version précédente. |

`GET /api/update` →
```json
{"etat": "inactif", "progression": 0, "version": "0.1.0", "validee": true,
 "precedente": {"version": "0.0.9", "disponible": true},
 "dernier_resultat": {"issue": "validee", "version": "0.1.0", "message": "Mise à jour réussie."}}
```
`etat` ∈ `inactif`, `envoi`, `verification`, `redemarrage`, `echec`; `progression` en % de l'envoi. `dernier_resultat.issue` ∈ `validee`, `annulee` (retour arrière automatique : « La nouvelle version n’a pas démarré correctement : l’ancienne a été remise. »), `refusee`.

`POST /api/update` : refuse avant d'écrire si `Content-Length` dépasse la partition de destination (`413`) ; refuse une image qui n'est pas pour cette application ou cette puce (`422`, « Ce fichier n’est pas un firmware pour cet écran. ») ; écrit par `esp_ota_*` dans la partition inactive ; vérifie l'image (`esp_ota_end`) ; répond `200 {"etat":"redemarrage","dans_secondes":3}` puis redémarre. **Un envoi qui échoue ne change rien** : l'ancienne partition reste celle du démarrage.

**Retour arrière automatique** : la nouvelle version démarre « en attente de validation » (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) et ne se déclare valide (`esp_ota_mark_app_valid_cancel_rollback`) qu'après un **auto-test** (écran initialisé, Wi-Fi tenté, serveur HTTP à l'écoute) ; sinon l'appareil redémarre sur l'ancienne. `POST /api/update/rollback` fait la même chose à la demande (`200 {"etat":"redemarrage"}`, ou `409`, « Il n’y a pas de version précédente à remettre. »). Après tout redémarrage, la page se reconnecte seule en lisant `GET /api/status` toutes les 2 s, et affiche `dernier_resultat`.

**À prouver sur matériel** (plan, phase 3) : un envoi d'une image volontairement cassée revient à l'ancienne par rollback. Rien de tout cela n'est constaté à ce jour.

## 12. Fichiers statiques

Le front est servi tel quel par `esp_http_server` : `GET /` → `index.html`, ressources avec `Content-Encoding: gzip` et `ETag` = les 16 premiers chiffres hexadécimaux du SHA-256 des octets compressés (`tools/embarquer_web.py`), donc un fichier qui n'a pas changé garde son étiquette d'une version du firmware à l'autre. En mode `point_d_acces`, toute adresse inconnue redirige (`302`) vers `/` (portail captif). Aucune route de `/api/` n'est mise en cache.

**Cache et politique de contenu.** Fichiers de la page : `Cache-Control: no-cache` (le navigateur revalide à chaque fois ; l'`ETag` fait de la réponse un `304` de quelques octets) ; `/api/` et le `404` : `no-store`. Les fichiers portent `Content-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self' https:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'; object-src 'none'` (constante `http::POLITIQUE_CONTENU`, lue telle quelle par le faux serveur). Pas d'`'unsafe-inline'` : `index.html` n'a ni script en ligne, ni attribut `style`, ni `onclick`, et `app.js` construit la page par le DOM, jamais par du texte HTML ; un script ou un style injecté dans la page n'est donc pas exécuté (vérifié par le parcours Playwright). `connect-src … https:` parce que la page lit elle-même la liste des spots de la mer à l'adresse de Vigie réglée par l'utilisateur (§15, `https://` obligatoire) ; `img-src 'self'` pour l'icône d'onglet, les icônes de la page étant des `<symbol>` du HTML.

**Fichiers de langue.** `GET /lang/<code>.json` (`<code>` ∈ `fr`, `en`, `de`, `nl`, `es`, `it`, `tr`) renvoie les textes de la page de réglages dans cette langue : un objet JSON plat `{"<id>": "<texte>"}` (`Content-Type: application/json; charset=utf-8`, gzip comme les autres fichiers). Ces fichiers sont **publics** (sans jeton : la page en a besoin avant l'appairage) et ne contiennent que des textes d'interface. Ils sont générés depuis `i18n/page.yaml` et `i18n/<code>.yaml` par `components/core/outils/generer_i18n.py`, avec le repli (cette langue, puis l'anglais, puis le français) déjà appliqué : un fichier est toujours complet. Un code inconnu → `404` (en mode `point_d_acces`, la redirection habituelle). La page charge un fichier à la fois (entre 3 018 octets pour `en` et 3 396 octets pour `de` compressés au niveau 9, mesuré le 12/11/2026 sur les fichiers générés dans `web/app/lang/` au commit `4b822cf` ; 2 067 à 2 338 octets le 06/11/2026).

## 13. Langues

Sept langues (ADR-0006), dans cet ordre figé et avec ces **indices qui ne changent jamais** (ils sont enregistrés en NVS) : `fr` 0, `en` 1, `de` 2, `nl` 3, `es` 4, `it` 5, `tr` 6. Une langue nouvelle prend le numéro suivant.

- **Langue de l'écran (`langue_ecran`)** : la langue des textes de l'écran, de la page de réglages, et de chaque phrase que cette API renvoie (§1 règle 5) : messages d'erreur, noms et aides des préréglages, libellés météo, étapes de test, résultats de mise à jour. La page la suit : elle lit `langue` dans `GET /api/status` (public), charge `/lang/<code>.json`, et recharge ses textes et les phrases de l'appareil quand l'utilisateur change ce réglage.
- **Langue de l'assistant (`langue_assistant`)** : la langue que l'assistant comprend (reconnaissance vocale) et parle. Indépendante de la langue de l'écran ; le service STT n'a plus de champ « langue parlée » à part.
- **Repli**, sur l'appareil comme sur la page : la langue demandée, puis l'anglais, puis le texte français lui-même. Une phrase n'est donc jamais vide ni remplacée par un code. Si un fichier de langue ne se charge pas, la page prend l'anglais, puis le français ; si aucun ne se charge, elle montre les identifiants de texte (visible et facile à repérer).
- **Qui a écrit les traductions** : le français est la langue de l'auteur et la seule qu'il relit. Les six autres (en, de, nl, es, it, tr) sont écrites par une IA (Claude) depuis le français et l'anglais, **non relues par un locuteur** ; les dates et quelques libellés sont repris tels quels des fichiers de langue du projet Tab5, qui ont servi sur un vrai écran. La longueur des textes allemands et néerlandais sur l'écran réel, et le rendu sur une carte, sont **non vérifiés**.
- **Attribution d'Open-Meteo.** Les données d'Open-Meteo sont sous licence CC BY 4.0 (note de recherche 03 : licence lue, formulation d'attribution « non précisée » ; le plan fait de la mention une condition de la licence). La page affiche donc toujours, dans la langue de la page, « Données météo : Open-Meteo (licence CC BY 4.0) » avec un lien vers `https://open-meteo.com/` (identifiant de texte `meteo.attribution` dans `i18n/page.yaml`). **L'écran de l'appareil** montre le crédit du service qui a réellement donné la météo (Open-Meteo, MET Norway ou le NWS, `fournisseur` du §5) dans sa carte de réglages « Lieu et météo » (voir [THIRD_PARTY.md](../THIRD_PARTY.md)) ; si l'accueil doit le montrer aussi n'est pas décidé dans ce document.

## 14. Alertes officielles (Vigie)

Écrit le 07/11/2026 avec l'[ADR-0009](decisions/0009-alertes-officielles-par-vigie.md). L'écran montre les alertes météo officielles du service de l'auteur, Vigie, **seulement tant qu'une est en vigueur** ; ces quatre réglages le pilotent. Ils forment un groupe, comme l'affichage.

`GET /api/alerts` →
```json
{"actives": true, "adresse": "", "periode_min": 15, "niveau_min": "yellow",
 "adresse_publique": "https://vigie.auxfilsdesidees.fr",
 "niveaux": ["yellow", "orange", "red"]}
```
`PUT /api/alerts` accepte les quatre champs modifiables (`actives`, `adresse`, `periode_min`, `niveau_min`), chacun facultatif (absent = inchangé), et répond comme `GET` avec les valeurs enregistrées. **Tout ou rien** : si une valeur est refusée, rien n'est enregistré.

| Champ | Règle |
|---|---|
| `actives` | `true` (défaut) ou `false`. `false` : aucune requête, aucun bandeau, et la réponse gardée en mémoire est oubliée. Un texte ou un nombre est refusé. |
| `adresse` | l'adresse de base du service Vigie, **`https://` obligatoire** (`http://` est refusé : un avertissement lu en clair pourrait être falsifié). Vide ou faite d'espaces = l'instance publique (`adresse_publique`, le défaut ; `adresse` est alors rendue `""`). Au plus 80 octets, ASCII seulement, ni espace, ni `?`, `#`, `@` ou `\` ; un port et un chemin sont permis ; les espaces autour et un `/` final sont retirés et le schéma est mis en minuscules. |
| `periode_min` | minutes entières **de 10 à 180**, 15 par défaut. `15.5` est refusé, pas arrondi. L'instance publique est limitée et « n'est pas faite pour bâtir un service dessus » : l'appareil envoie **une requête par période**, jamais de rafale au démarrage, et s'éloigne après une erreur. |
| `niveau_min` | `yellow` (défaut), `orange` ou `red` : le niveau le plus bas montré à l'écran. |

Ce qui part vers l'adresse de Vigie : la position de la ville choisie arrondie à trois décimales (environ 110 m) et la langue de l'écran (`GET <adresse>/v1/vigilances?lat=…&lon=…&lang=<langue de l'écran>,en`). Rien d'autre, aucune clé, aucun identifiant de l'appareil.

Erreurs `422` (champ dans `champ`) : `actives_invalide` (« Les alertes doivent être activées ou désactivées. »), `adresse_invalide` (« L’adresse de Vigie doit commencer par https://. »), `periode_invalide` (« La période doit être un nombre entier de minutes, de 10 à 180. »), `niveau_inconnu` (« Ce niveau d’alerte n’existe pas. »).

Un changement arrive au client réseau en 5 s environ ; il interroge alors Vigie tout de suite si la requête a changé, mais jamais deux fois en moins d'une minute.

## 15. Mer (Vigie)

Écrit le 08/11/2026 avec l'[ADR-0025](decisions/0025-mer-ecriture-du-registre-des-spots-lot-e.md). La page mer de l'écran ([ADR-0022](decisions/0022-mer-la-page-lot-c.md)) montre la mer d'**un seul spot choisi**, lue chez Vigie ([ADR-0019](decisions/0019-mer-client-et-registre-de-spots.md)). L'appareil garde une courte liste de spots (12 au plus) et dit lequel est choisi ; ce groupe les règle. Tant qu'aucun spot n'est choisi, l'appareil n'envoie rien à Vigie pour la mer, et la roue d'appui long n'a pas de bouton « Mer » ([ADR-0024](decisions/0024-mer-l-entree-de-la-roue-lot-d.md)).

`GET /api/sea` →
```json
{"spots": [{"id": "un-identifiant-de-la-liste-de-vigie", "nom": "Une plage"}], "choisi": "un-identifiant-de-la-liste-de-vigie",
 "vent_min_kn": 12, "vent_max_kn": 25, "spots_max": 12, "vent_plafond_kn": 60}
```
`choisi` vaut `""` et `spots` vaut `[]` sur un appareil où rien n'a été choisi. Les valeurs ci-dessus sont des **exemples de forme**, pas des données lues.

`PUT /api/sea` accepte les quatre champs modifiables (`spots`, `choisi`, `vent_min_kn`, `vent_max_kn`), chacun facultatif (absent = inchangé), et répond comme `GET` avec les valeurs enregistrées. **Tout ou rien** : si une valeur est refusée, rien n'est enregistré.

| Champ | Règle |
|---|---|
| `spots` | la liste **entière**, dans l'ordre à garder : 0 à 12 objets `{"id": …, "nom": …}` (les autres champs d'un objet sont ignorés). `id` : l'identifiant du spot dans la liste de Vigie, lettres minuscules, chiffres, `-` et `_`, 64 octets au plus. `nom` : le nom affiché, 1 à 32 caractères, UTF-8 bien formé, aucun caractère de contrôle, pas seulement des espaces. Deux objets de même `id` sont refusés. Une liste vide coupe la mer (aucune requête, aucun bouton « Mer »). |
| `choisi` | l'`id` du spot dont l'écran montre la mer ; il doit être dans la liste telle qu'elle sera après le changement. `""` seulement pour une liste vide. Absent : l'appareil reste sur le même spot s'il est toujours dans la liste, sinon sur le premier. |
| `vent_min_kn`, `vent_max_kn` | la plage de vent, en **nœuds entiers**, que Vigie utilise pour le verdict du kite : `0 < min < max <= 60` (défauts 12 et 25). Si un seul est envoyé, le couple (celui envoyé, celui enregistré) est jugé. `12.5` est refusé, pas arrondi. |

**D'où vient la liste des spots.** L'appareil ne la sert **pas**. La page la lit elle-même chez Vigie : `GET <adresse>/v1/surf/spots`, où l'adresse est `adresse` de `GET /api/alerts` ou, si elle est vide, `adresse_publique` (51 spots le 08/11/2026, 20 405 octets, 4 695 en gzip). La page ne garde de chaque spot que son `id`, son `name` et son `place` (pour distinguer deux plages de même nom) et envoie à l'appareil `id` et `nom`. L'appareil ne vérifie pas qu'un `id` est dans la liste de Vigie : un spot inconnu fait répondre 404 à Vigie et l'appareil redemande à son rythme normal (ADR-0019). Les réponses de Vigie portent le crédit des données de spots (contributeurs d'OpenStreetMap, ODbL) : la page l'affiche à côté de la liste.

Ce que l'appareil envoie à Vigie pour la mer : `GET <adresse>/v1/embedded/frame?device_type=generic&spot=<id>`, plus `&wind_min_kn=…&wind_max_kn=…` quand la plage n'est pas 12 à 25. Aucune position, aucune clé, aucun identifiant de l'appareil.

Erreurs `422` (champ dans `champ`) : `spots_invalide` (« Les spots doivent former une liste d’objets avec un identifiant et un nom. »), `spot_invalide` (« Cet identifiant de spot n’est pas valide. »), `nom_invalide` (« Le nom d’un spot doit faire de 1 à 32 caractères. »), `spot_double` (« Ce spot est déjà dans la liste. »), `trop_de_spots` (« La liste est pleine : 12 spots au plus. »), `choix_inconnu` (« Le spot choisi n’est pas dans la liste. »), `vent_invalide` (« La plage de vent doit être en nœuds entiers, de 1 à 60, le minimum sous le maximum. »). `champ` vaut `spots` pour les cinq premières, `choisi` pour `choix_inconnu`, et pour `vent_invalide` `vent_min_kn` si la requête porte un `vent_min_kn`, sinon `vent_max_kn` (le couple est jugé d'un bloc). Un corps qui n'est pas un objet JSON est refusé en `400` `illisible`. Un nom de spot qui contient `\u0000` n'est pas refusé : le lecteur JSON le transforme en U+FFFD, comme partout ailleurs sur l'appareil. Deux écrivains (la page et, plus tard, l'écran) ne sont pas coordonnés : la dernière écriture gagne.

Un changement atteint le client de la mer en 5 s environ ; il interroge alors Vigie tout de suite si la requête a changé, mais jamais deux fois en une minute.
