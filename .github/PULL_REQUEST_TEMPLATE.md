## What and why / Quoi et pourquoi

<!-- One or two sentences: what changes, and why. Link an issue or an ADR if relevant. / Une ou deux phrases. -->

## Board and evidence / Carte et preuves

- Board and screen revision / Carte et révision d'écran: <!-- tab5-st7121, tab5-st7123, tab5-ili9881c, waveshare-7b, or "none" -->
- Seen on the board? / Constaté sur la carte ? <!-- yes: commit, date, what you saw on screen or in the serial log. no: write "not tested on board". -->

## Checklist

- [ ] `.\tools\verifier.ps1` passes locally (secrets, core tests actually run, independent checks, ruff, formatting), or I say below why it does not
- [ ] If `components/core/` or `test/host/` changed: `python test/host/lancer.py` shows the mode « complet (compilé + exécuté) », and `python test/host/lancer.py --mutations --seulement <the test files I touched>` detects every one of their mutations (last line pasted below). The complete campaign is never required
- [ ] I did not write "tested" or "done" without a board, a commit and a date. A green build is not "done" (see `AGENTS.md`, « Fait »)
- [ ] Commits touching firmware or a board end with the `Carte:` and `Teste-sur:` lines (see `AGENTS.md`, « Git »)
- [ ] No secret, no MAC address, no real IP address, no personal path in the diff
- [ ] No Home Assistant or ESPHome dependency; no hard-coded colour; screen texts go through the translation table ([ADR-0006](../docs/decisions/0006-ecran-en-sept-langues.md))
- [ ] A new third-party component or asset is in [`THIRD_PARTY.md`](../THIRD_PARTY.md), with its licence read from the source
- [ ] A user-visible change has a fragment `changelog.d/<branch>.md` ([format](../changelog.d/README.md)); `CHANGELOG.md` is not edited by the PR
- [ ] A new architecture decision has an ADR in `docs/decisions/` (next number)
- [ ] A symptom that was diagnosed is in `docs/depannage.md` (symptom → cause → solution, proof, status)

## Notes for the reviewer / Notes pour la relecture

<!-- Anything non-obvious, trade-offs, what is left for later. Say if an AI helped write the change (optional, but useful). / Dites si une IA a aidé (facultatif). -->
