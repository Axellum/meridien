### Security

- **Settings page: requests whose `Host` header does not name the device are refused** (protection against DNS rebinding).
  Accepted: the device's IP address and its `ecran-xxxx.local` name, with or without `:80`; anything else gets a bare `403`
  and the connection is closed. A name given by a home router (`ecran-xxxx.home`…) is refused too: use the address or the
  `.local` name shown on the screen. See rule 9 of [docs/api-reglages.md](../docs/api-reglages.md). Tested on: PC only
  (core tests with mutations, Playwright run-through against the fake device), not tested on board.
- **Settings page: full content security policy** (`default-src 'none'`, only the device's own script and style, no inline
  code, no framing, requests to the device and to `https:` for the sea spots), plus `nosniff` and `Referrer-Policy:
  no-referrer` on every answer. The page has no inline style any more. Tested on: PC only (Playwright: an injected inline
  script and style are not applied, no policy violation during the whole run-through), not tested on board.
- **CI: the pip tools (ruff, clang-format, playwright) are pinned by SHA-256** (`.github/exigences/`, `pip install
  --require-hashes`).

### Added

- **Test: the fake device of the settings page is compared with the real API contract** (`test/host/controle_contrat_web.py`,
  run by `test/host/lancer.py`): same routes, same errors (status, `id`, `champ`) and same response fields as
  `api_reglages.hpp` for every route the firmware serves. The fake device now also answers `illisible` to a malformed
  search, like the device.
