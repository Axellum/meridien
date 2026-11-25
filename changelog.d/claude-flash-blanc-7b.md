### Changed

- **Waveshare 7B: code runs from PSRAM with a 256 KB L2 cache**, the memory settings of the ESPHome firmware on this board; it
  costs about 142 KB of free internal RAM. It was meant to stop a brief white flash at rest (several times an hour, never under
  ESPHome on the same board) and **does not**: no flash was seen during the first 30 minutes of the trial on 2026-11-13, but the
  flash was seen again on 2026-11-25 with `main` `4a9a7be`, which has both settings. Its cause is not proven
  ([ADR-0028](../docs/decisions/0028-7b-code-en-psram-et-cache-l2-256-ko.md),
  [troubleshooting](../docs/depannage.md#waveshare-7b-a-brief-white-flash-at-rest-several-times-an-hour)). Tested on:
  `waveshare-7b`, 2026-11-13, trial binary `b9aa06e` + both settings + a diagnostic probe; 2026-11-25, `4a9a7be`: flash seen again.

*7B : le code s'exécute depuis la PSRAM (réglages d'ESPHome) ; le bref flash blanc au repos n'est pas corrigé pour autant, il est
revu le 25/11/2026.*
