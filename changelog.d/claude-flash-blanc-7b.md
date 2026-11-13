### Fixed

- **Waveshare 7B: brief white flash at rest**: the panel flashed white for an instant, several times an hour, with nothing
  touched (never under ESPHome on the same board). The 7B now runs its code from PSRAM with a 256 KB L2 cache, the memory
  settings of the ESPHome firmware on this board; it costs about 142 KB of free internal RAM. The cause itself is not proven
  ([ADR-0028](../docs/decisions/0028-7b-code-en-psram-et-cache-l2-256-ko.md)). Tested on: `waveshare-7b`, 2026-11-13, trial
  binary `b9aa06e` + both settings + a diagnostic probe, no flash seen during the first 30 minutes.

*7B : le bref flash blanc au repos ne s'est plus montré pendant la première demi-heure avec le code exécuté depuis la PSRAM.*
