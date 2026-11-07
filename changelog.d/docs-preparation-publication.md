### Added

- **`SECURITY.md`**: how to report a problem, and the security limits written down (Wi-Fi password and settings in the flash without encryption, settings page over plain local HTTP behind a 6-digit code, a binary built with the local Wi-Fi settings contains them, radio coprocessor never updated, third-party services). Read in the configuration files and in `main/reseau.cpp` and `main/serveur_web.cpp` on 2026-11-07; nothing measured on a board.
- **`tools/controle_publication.py`**: looks for what must not leave the private repository (MAC addresses, personal paths, the Wi-Fi password by the name of its variable, commit identities) and lists what a human has to judge (private IP addresses, e-mail addresses). `--autotest` checks the check. Run on the tree of 2026-11-07 it found four blocking items, now fixed.
- **`tools/instantane_public.ps1`** and **`docs/publication.md`**: the procedure for the public copy of the repository, decided by the owner on 2026-11-06 as a new repository made from a clean snapshot (the history of `main` holds MAC addresses in 98 of 270 commits, a personal path in 7, and the author's personal e-mail address in all of them). Nothing is published.

### Changed

- **README status** brought up to date (it still described a “Bonjour” screen and a plan for Wi-Fi), and the repository is no longer described as private there; `AGENTS.md` points to `docs/publication.md` for any change of visibility.
- A Windows user path in `docs/depannage.md` and the board's real IP address in `docs/mesures/meteo-journal-carte.log` were replaced (`%USERPROFILE%`, `192.168.1.52`, the example address used elsewhere in the documents).
