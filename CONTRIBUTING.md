# Contributing

## English · [Français](#version-française)

Meridien is a personal project, shared in case it is useful, and made with the help of an AI. Issues and pull requests are
welcome, in English or French, but there is no promise of support or of a reply time, and some things will be declined
because they do not fit (home automation, Home Assistant and ESPHome are out of scope).

### Before you open a pull request

1. Read [`AGENTS.md`](AGENTS.md) (the code rules, short), [`docs/plan.md`](docs/plan.md) and the
   [decisions](docs/decisions/README.md). A choice that looks odd is probably explained there or in
   [`docs/depannage.md`](docs/depannage.md).
2. Build with ESP-IDF 5.5.5 (the version this project is built and tested with; other versions are not tested). On
   Windows, `.\tools\idf.ps1 build` needs `tools\local.ps1`: copy [`tools/local.example.ps1`](tools/local.example.ps1) and
   adapt it.
3. Run the core tests on your PC: `python test/host/lancer.py`. It must report the mode « complet (compilé + exécuté) »;
   the mode « syntaxe » runs nothing. If you changed the core or its tests, also run the mutations of the test files you
   touched: `python test/host/lancer.py --mutations --seulement <test_a,test_b>` (every mutation must be detected; paste
   the last line in the pull request). The complete mutation campaign (`.\tools\verifier.ps1 -Mutations`) takes hours
   and is never required. Tools and set-up: [`docs/outils.md`](docs/outils.md).
4. Enable the pre-commit hook once: `git config core.hooksPath .githooks`. It runs `tools/verifier.ps1 -Rapide` (secrets,
   core tests, independent checks, ruff, C/C++ formatting). Please do not bypass it with `--no-verify`: fix the cause.
5. One subject per commit. Commits that touch firmware or a board end with two lines, `Carte:` and `Teste-sur:`
   (see « Git » in `AGENTS.md`). Never write "tested" without a board, a commit and a date: a green build is not "done".
6. Add a changelog fragment `changelog.d/<branch>.md` for a user-visible change (format:
   [`changelog.d/README.md`](changelog.d/README.md)). A new architecture decision gets an ADR in `docs/decisions/`.
7. A new third-party component or asset goes into [`THIRD_PARTY.md`](THIRD_PARTY.md), with its licence read from the source.
8. Use the [pull request template](.github/PULL_REQUEST_TEMPLATE.md). Work on a branch, merge `main` into it before
   pushing, open a pull request towards `main`: it is merged (merge commit, no squash) once the required checks of
   `main`'s protection are green. Nobody pushes to `main` directly. The jobs and what each proves:
   [`docs/outils.md`](docs/outils.md#continuous-integration-github-actions).

### Please keep out of the repository

Secrets and API keys, MAC addresses, real IP addresses, personal paths and the names of your devices. Settings and keys
live in the device's NVS, never in the repository. If you find a secret or a security problem, do not post it in a public
issue: use GitHub's private vulnerability reporting (enabled on this repository on 08/11/2026), otherwise contact
the repository owner through their GitHub profile.

### Hardware

Flash only boards you own, and read the MAC before writing. Do not update the firmware of the ESP32-C6 radio coprocessor
(see rule 7 in `AGENTS.md`). The Tab5 ST7121 is the board the project is developed on; the other boards are listed with
their state in [`docs/compatibilite.md`](docs/compatibilite.md).

If you use an AI agent, point it at `AGENTS.md`.

---

## Version française

Meridien est un projet personnel, partagé au cas où il servirait, fait avec l'aide d'une IA. Issues et pull requests sont les
bienvenues, en français ou en anglais, mais sans promesse de support ni de délai de réponse, et certaines propositions seront
refusées parce qu'elles ne cadrent pas (la domotique, Home Assistant et ESPHome sont hors périmètre).

### Avant d'ouvrir une pull request

1. Lire [`AGENTS.md`](AGENTS.md) (les règles de code, courtes), [`docs/plan.md`](docs/plan.md) et les
   [décisions](docs/decisions/README.md). Un choix qui semble étrange y est probablement expliqué, ou dans
   [`docs/depannage.md`](docs/depannage.md).
2. Compiler avec ESP-IDF 5.5.5 (la version avec laquelle le projet est compilé et testé ; les autres ne sont pas testées).
   Sous Windows, `.\tools\idf.ps1 build` demande `tools\local.ps1` : copier
   [`tools/local.example.ps1`](tools/local.example.ps1) et l'adapter.
3. Lancer les tests du noyau sur le PC : `python test/host/lancer.py`. Il doit afficher le mode « complet (compilé +
   exécuté) » ; le mode « syntaxe » n'exécute rien. Si le noyau ou ses tests changent, lancer aussi les mutations des
   fichiers de test touchés : `python test/host/lancer.py --mutations --seulement <test_a,test_b>` (chaque mutation doit
   être détectée ; coller la dernière ligne dans la pull request). La campagne complète des mutations
   (`.\tools\verifier.ps1 -Mutations`) dure des heures et n'est jamais exigée. Outils et installation :
   [`docs/outils.md`](docs/outils.md).
4. Activer le crochet de commit une fois : `git config core.hooksPath .githooks`. Il lance `tools/verifier.ps1 -Rapide`
   (secrets, tests du noyau, contrôles indépendants, ruff, mise en forme C/C++). Merci de ne pas le contourner avec `--no-verify` : corriger la cause.
5. Un sujet par commit. Les commits qui touchent le firmware ou une carte se terminent par deux lignes, `Carte:` et
   `Teste-sur:` (voir « Git » dans `AGENTS.md`). Ne jamais écrire « testé » sans carte, commit et date : un build qui passe
   n'est pas « fait ».
6. Ajouter un fragment `changelog.d/<branche>.md` pour un changement visible (format :
   [`changelog.d/README.md`](changelog.d/README.md)). Une nouvelle décision d'architecture a son ADR dans `docs/decisions/`.
7. Un nouveau composant ou une nouvelle ressource tierce va dans [`THIRD_PARTY.md`](THIRD_PARTY.md), avec sa licence lue à
   la source.
8. Utiliser le [gabarit de pull request](.github/PULL_REQUEST_TEMPLATE.md). Travailler sur une branche, y fusionner
   `main` avant de pousser, ouvrir une pull request vers `main` : elle est fusionnée (merge commit, pas de squash) quand
   les contrôles requis par la protection de `main` sont verts. Personne ne pousse directement sur `main`. Les jobs et ce
   que chacun prouve : [`docs/outils.md`](docs/outils.md#intégration-continue-github-actions).

### À ne pas mettre dans le dépôt

Secrets et clés d'API, adresses MAC, adresses IP réelles, chemins personnels et noms de vos appareils. Les réglages et les
clés vivent dans la NVS de l'appareil, jamais dans le dépôt. Si vous trouvez un secret ou un problème de sécurité, ne le
postez pas dans une issue publique : utiliser le signalement privé de vulnérabilités de GitHub (activé sur ce dépôt le 08/11/2026),
sinon contacter le propriétaire du dépôt par son profil GitHub.

### Matériel

Ne flasher que des cartes qui vous appartiennent, et lire la MAC avant d'écrire. Ne pas mettre à jour le firmware du
coprocesseur radio ESP32-C6 (règle 7 d'`AGENTS.md`). Le Tab5 ST7121 est la carte sur laquelle le projet est développé ; les
autres cartes et leur état sont dans [`docs/compatibilite.md`](docs/compatibilite.md).

Si vous utilisez un agent IA, pointez-le vers `AGENTS.md`.
