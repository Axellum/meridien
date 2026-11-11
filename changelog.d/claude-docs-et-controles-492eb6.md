### Changed

- **Contributing: the rule before a push is the one really followed.** The full core test run (« complet » mode) and the
  mutations of the test files a branch touches (`lancer.py --mutations --seulement …`), then a pull request merged by merge
  commit once the required checks are green; the complete mutation campaign stays possible by hand
  (`.\tools\verifier.ps1 -Mutations`) and is never required ([AGENTS.md](../AGENTS.md), [CONTRIBUTING.md](../CONTRIBUTING.md),
  [docs/outils.md](../docs/outils.md)).
- **Documentation brought back in line with the code and the boards**: README (the Waveshare 7B runs Meridien; none of the
  six translations has been reviewed by a native speaker), [docs/compatibilite.md](../docs/compatibilite.md) (rows for the
  Details page, the automatic mode, the rain strip and the sea page), dated notes on the ADR statuses and an explanation of
  the missing ADR 0011 ([docs/decisions/README.md](../docs/decisions/README.md)), the settings API contract
  ([docs/api-reglages.md](../docs/api-reglages.md): `perimee` after 2 h, what `internet` and `memoire_libre` really are, no
  `ressentie`, the real error identifiers and statuses, the ETag, the size of the language files). Not tested on board
  (documentation only).

### Fixed

- **Checks that nothing ran any more now run on every commit and in the CI**: the `--verifier` mode of the scripts that
  compute expected values in Python, the icons and fonts, the Markdown links (13 broken links repaired) and the 7B trial
  script's self-test and mutations (`test/host/controles_independants.py`); one of those mutations had silently become
  inapplicable and is fixed. The test captures are checked out with LF line endings on Windows too.
