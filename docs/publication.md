# Making the repository public

## English · [Français](#version-française)

---

> **Summary** — **The repository has been public since 08/11/2026.** Decision of the owner, 07/11/2026: **the existing repository `Axellum/meridien` itself went from private to public, history included.** No snapshot, no second repository (this replaced the decision of 06/11/2026). `python tools/controle_publication.py --historique` was run first, the checklist below was followed (its results are at the end of the list), and the flip was made on the owner's explicit go.

### Why the history goes public as it is

The owner wants to share how the project was built, and the history is that story. Rewriting it to remove a few values would also change every commit number: the documents quote them, and all of those that are the project's own exist in the history as it is (checked on 07/11/2026: 70 of the 72 short commit numbers written in the Markdown files are commits of the repository; the other two are a placeholder and a commit of a third-party project). The price is what the history holds.

Measured on 07/11/2026 on a **fresh clone of the remote**, which is exactly what would become public (`main`, the head of the closed pull request #1 and the tag `jalon-1a-tab5-st7121`: 270 commits in all), with `python tools/controle_publication.py --historique`:

| In the history | In how many commits | What it means |
|---|---|---|
| 4 board MAC addresses | 10, 5, 4 and 2 (one commit count per address) | They tell which boards the author has; not a secret. None is in the files of today. |
| The Windows user name, in a `<drive>:\Users\<name>` path | 7 | The account name of the author's PC. |
| Folders of the author's PC (the project folder, and the ESP-IDF folder on two drives) | 27, 11 and 4 | Harmless folder names. |
| 8 private IP addresses, the most frequent being the address a board had at some time | 8 for that one | Useless outside the author's home network. |
| The author's personal e-mail address | all 270 | Already public: the GitHub profile and the commits of the public repositories Tab5 and vigie-api. |
| The Wi-Fi password | 0 | Looked for by the name of its variable, never printed. The network name (under 8 characters) is not looked for: too many false hits. |
| A secret found by `gitleaks` | 0 | Checked on 07/11/2026. |

**Accepted by the owner on 07/11/2026** (“Oui, on bascule”, after this list): the MAC addresses, the PC folder names and the private IP addresses. **Not accepted**: a secret. If one is found before the flip, the history has to be rewritten or the flip waits. After the flip nothing recalls the copies already taken (forks, clones, search engines): this checklist is the last chance.

What else becomes public with the repository: the commit messages (in French), the 12 GitHub Actions runs and their logs, the closed pull request #1 (“CI : essai du workflow”), and **every version of every image** (56 image paths in the history, 42 in today's files).

### The tool

**`python tools/controle_publication.py [folder]`** looks in the text of the files **and in their names** for: a MAC address, a `<drive>:\Users\<name>` path, the value of a secret variable (`--env <.env> --variable NAME`), and any string given with `--interdit`: these are **blocking**. It also lists, **to judge**: e-mail addresses, private IP addresses and Windows drive paths (an example in a test is not a leak, a human says so). With **`--historique`** it also reads the text added or removed by **every commit** of every reference (`git log --all -p`) with the same rules, and lists the identities of the commits: in the history only the value of a secret variable blocks, the rest is “to judge” because the owner decides what stays there. `--autotest` checks the check on a folder and a two-commit repository it builds. It does **not** read images, nor the names of the files of the history, and it does not understand text: it finds strings.

### Checklist before the owner flips the switch

1. The work in progress is merged into `main`, **including the branch `docs/preparation-publication`** (`origin/main` still holds the personal path of `docs/depannage.md`, fixed on that branch), the full `.\tools\verifier.ps1` passes, the CI is green.
2. The **Status** section of the [README](../README.md#status) says what runs now, in the same words as [compatibilite.md](compatibilite.md): re-read it.
3. **Look at the images** by eye, those of today's files ([`captures/ecran/`](captures/ecran/), [`../web/captures/`](../web/captures/), `../web/esquisses/`, `design/esquisses/`) **and those that only exist in the history** (a city name, the Wi-Fi networks of the neighbours, a real name: the tool cannot see pixels). List them: `git log --all --name-only --format= -- "*.png" "*.jpg" "*.jpeg" "*.gif" "*.svg" "*.webp" | sort -u`; an old version: `git show <commit>:<path> > <file>`. **Not done yet** at the date of this page (2 images looked at).
4. **Run the history check again just before the flip** (the history moves with every merge): `python tools/controle_publication.py --historique --env <.env> --variable MERIDIEN_WIFI_MOT_DE_PASSE`, from a fresh clone of the remote for the exact public set. Read the **to judge** list against the table above: a kind of value that is not in it is a new decision.
5. **Read the logs of the Actions runs** (`gh run list`, `gh run view <id> --log`): they become public. No Actions secret or variable exists on 07/11/2026. Delete the runs you do not want shown.
6. **Build and test from a fresh clone of `main`**: `.\tools\idf.ps1 build` and `python test/host/lancer.py --mutations`. **Not done yet** at the date of this page.
7. The flip, **by the owner or on his explicit go at that moment**: *Settings → General → Danger Zone → Change repository visibility*, or `gh repo edit Axellum/meridien --visibility public --accept-visibility-change-consequences` (the flag is the one `gh repo edit --help` lists, 07/11/2026).
8. Right after: private vulnerability reporting (the [SECURITY.md](../SECURITY.md) text assumes it), secret scanning and push protection, protection of `main` with the required checks listed in [outils.md](outils.md). The first lines of `.github/workflows/ci.yml` say “private repository: the minutes are billable”: correct them then.
9. Say nothing about it before the owner decides.

**Results on the day of the flip (08/11/2026)**, point by point:

1. Everything is merged into `main` (`e7b039b`). The complete `.\tools\verifier.ps1` passed on `3fca1cc` (1,416 tests, every mutation, exit code 0, 2 h 54 min on the PC, 07/11/2026). The last commit (`e7b039b`: `std::optional::reset()` is not `constexpr` in C++17, which g++ 14.2 refuses) was checked by the 1,420 tests of a fresh clone, the 201 alert mutations and a run with the g++ 14.2 of ESP-IDF, **not** by a new complete run. CI on `e7b039b`: `tests-noyau` (both compilers), `firmware-tab5`, `ruff`, `clang-format`, `gitleaks` and `parcours-web` green; `mutations-noyau` (not required) cancelled by its 150-minute limit, see [outils.md](outils.md#continuous-integration-github-actions).
2. The README status and [compatibilite.md](compatibilite.md) were re-read and updated on 07/11/2026.
3. Done on 07/11/2026: 14 contact sheets of the images of today's files and of those that only exist in the history, looked at by eye. Example cities and fake Wi-Fi networks from the render data and the tests, a private IP and the end of a MAC address (categories accepted on 07/11/2026), the Vigie domain (public): nothing personal to block.
4. Run again from a fresh clone of the remote (`e7b039b`, with the head of the pull request): 0 blocking, 0 secret; `gitleaks` over the whole history clean.
5. The logs of the 13 Actions runs were read: no Wi-Fi password, no personal path, no e-mail address. No Actions secret or variable.
6. Tests from a fresh clone of `main`: 1,420 tests, 0 failure, 0 modified file afterwards. **Not done** from a fresh clone: `idf.ps1 build` (the Tab5 build is done by the CI job `firmware-tab5`; the 7B build has no CI job) and the full mutation run.
7. The flip, by `gh repo edit Axellum/meridien --visibility public --accept-visibility-change-consequences`, read back: `PUBLIC`, and the page answers without a token.
8. Done right after, through the GitHub API and read back: private vulnerability reporting enabled; secret scanning and push protection enabled; `main` protected (the five required checks `tests-noyau`, `ruff`, `clang-format`, `gitleaks`, `firmware-tab5`; no force-push, no deletion; administrators can still push directly, as the project's workflow does). Dependabot security updates left disabled: not asked.

### Decisions left to the owner

- **When to flip**: decided and done on 08/11/2026 (results above).
- **Encrypted NVS or not** (ADR-0003 plans it, nothing is done): not a condition of the flip; until it is done, the [SECURITY.md](../SECURITY.md) limits are what a reader is told.
- Decided on 07/11/2026: the working documents go public, most of them ([plan.md](plan.md), [cout.md](cout.md) (time, sessions, tokens), [recherche/](recherche/)); they are working notes in French and the README presents them as such.

---

## Version Française

---

> **Résumé** — **Le dépôt est public depuis le 08/11/2026.** Décision du propriétaire, 07/11/2026 : **le dépôt existant `Axellum/meridien` est passé lui-même de privé à public, historique compris.** Pas d'instantané, pas de second dépôt (cela a remplacé la décision du 06/11/2026). `python tools/controle_publication.py --historique` a été lancé d'abord, la liste ci-dessous a été suivie (ses résultats sont à la fin de la liste), et la bascule a été faite sur le go explicite du propriétaire.

### Pourquoi l'historique devient public tel quel

Le propriétaire veut partager comment le projet a été construit, et l'historique en est le récit. Le réécrire pour retirer quelques valeurs changerait aussi chaque numéro de commit : les documents les citent, et tous ceux qui sont ceux du projet existent dans l'historique tel qu'il est (constaté le 07/11/2026 : 70 des 72 numéros de commit courts écrits dans les fichiers Markdown sont des commits du dépôt ; les deux autres sont un exemple et un commit d'un projet tiers). Le prix, c'est ce que l'historique contient.

Mesuré le 07/11/2026 sur un **clone neuf du dépôt distant**, qui est exactement ce qui deviendrait public (`main`, la tête de la pull request n° 1 fermée et le tag `jalon-1a-tab5-st7121` : 270 commits en tout), avec `python tools/controle_publication.py --historique` :

| Dans l'historique | Dans combien de commits | Ce que cela veut dire |
|---|---|---|
| 4 adresses MAC de cartes | 10, 5, 4 et 2 (un nombre de commits par adresse) | Elles disent quelles cartes l'auteur possède ; pas un secret. Aucune n'est dans les fichiers d'aujourd'hui. |
| Le nom d'utilisateur Windows, dans un chemin `<lecteur>:\Users\<nom>` | 7 | Le nom du compte du PC de l'auteur. |
| Des dossiers du PC de l'auteur (le dossier du projet, et celui d'ESP-IDF sur deux lecteurs) | 27, 11 et 4 | Des noms de dossiers sans danger. |
| 8 adresses IP privées, la plus fréquente étant l'adresse qu'avait une carte à un moment | 8 pour celle-là | Sans usage hors du réseau de la maison de l'auteur. |
| L'adresse e-mail personnelle de l'auteur | les 270 | Déjà publique : le profil GitHub et les commits des dépôts publics Tab5 et vigie-api. |
| Le mot de passe Wi-Fi | 0 | Cherché par le nom de sa variable, jamais affiché. Le nom du réseau (moins de 8 caractères) n'est pas cherché : trop de faux positifs. |
| Un secret trouvé par `gitleaks` | 0 | Constaté le 07/11/2026. |

**Accepté par le propriétaire le 07/11/2026** (« Oui, on bascule », après cette liste) : les adresses MAC, les noms de dossiers du PC et les adresses IP privées. **Pas accepté** : un secret. S'il en est trouvé un avant la bascule, il faut réécrire l'historique ou attendre. Après la bascule rien ne rappelle les copies déjà prises (forks, clones, moteurs de recherche) : cette liste est la dernière chance.

Ce qui devient aussi public avec le dépôt : les messages de commit (en français), les 12 lancements de GitHub Actions et leurs journaux, la pull request n° 1 fermée (« CI : essai du workflow »), et **chaque version de chaque image** (56 chemins d'images dans l'historique, 42 dans les fichiers d'aujourd'hui).

### L'outil

**`python tools/controle_publication.py [dossier]`** cherche, dans le texte des fichiers **et dans leurs noms** : une adresse MAC, un chemin `<lecteur>:\Users\<nom>`, la valeur d'une variable secrète (`--env <.env> --variable NOM`), et toute chaîne donnée par `--interdit` : ce sont des trouvailles **bloquantes**. Il liste aussi, **à juger** : adresses e-mail, adresses IP privées et chemins de lecteur Windows (un exemple dans un test n'est pas une fuite, c'est un humain qui le dit). Avec **`--historique`** il lit en plus le texte ajouté ou retiré par **chaque commit** de toutes les références (`git log --all -p`) avec les mêmes règles, et liste les identités des commits : dans l'historique seule la valeur d'une variable secrète bloque, le reste est « à juger » parce que c'est le propriétaire qui décide de ce qui y reste. `--autotest` contrôle le contrôle sur un dossier et un dépôt de deux commits qu'il fabrique. Il **ne lit pas** les images, ni les noms des fichiers de l'historique, et ne comprend pas le texte : il trouve des chaînes.

### Liste avant que le propriétaire bascule

1. Le travail en cours est fusionné dans `main`, **branche `docs/preparation-publication` comprise** (`origin/main` contient encore le chemin personnel de `docs/depannage.md`, corrigé sur cette branche), le `.\tools\verifier.ps1` complet passe, la CI est verte.
2. La section **État** du [README](../README.md#état) dit ce qui tourne maintenant, avec les mêmes mots que [compatibilite.md](compatibilite.md#version-française) : la relire.
3. **Regarder les images** à l'œil, celles des fichiers d'aujourd'hui ([`captures/ecran/`](captures/ecran/), [`../web/captures/`](../web/captures/), `../web/esquisses/`, `design/esquisses/`) **et celles qui n'existent que dans l'historique** (un nom de ville, les réseaux Wi-Fi des voisins, un vrai nom : l'outil ne voit pas les pixels). Les lister : `git log --all --name-only --format= -- "*.png" "*.jpg" "*.jpeg" "*.gif" "*.svg" "*.webp" | sort -u` ; une ancienne version : `git show <commit>:<chemin> > <fichier>`. **Pas encore fait** à la date de cette page (2 images regardées).
4. **Relancer le contrôle de l'historique juste avant la bascule** (l'historique bouge à chaque fusion) : `python tools/controle_publication.py --historique --env <.env> --variable MERIDIEN_WIFI_MOT_DE_PASSE`, depuis un clone neuf du dépôt distant pour avoir l'ensemble exactement public. Lire la liste **à juger** en face du tableau ci-dessus : un genre de valeur qui n'y est pas est une nouvelle décision.
5. **Lire les journaux des lancements d'Actions** (`gh run list`, `gh run view <id> --log`) : ils deviennent publics. Aucun secret ni variable d'Actions n'existe le 07/11/2026. Supprimer les lancements qu'on ne veut pas montrer.
6. **Compiler et tester depuis un clone neuf de `main`** : `.\tools\idf.ps1 build` et `python test/host/lancer.py --mutations`. **Pas encore fait** à la date de cette page.
7. La bascule, **par le propriétaire ou sur son go explicite à ce moment-là** : *Settings → General → Danger Zone → Change repository visibility*, ou `gh repo edit Axellum/meridien --visibility public --accept-visibility-change-consequences` (l'option est celle que liste `gh repo edit --help`, 07/11/2026).
8. Juste après : signalement privé de vulnérabilités (le texte de [SECURITY.md](../SECURITY.md#version-française) le suppose), analyse de secrets et protection à la poussée, protection de `main` avec les contrôles requis listés dans [outils.md](outils.md). Les premières lignes de `.github/workflows/ci.yml` disent « dépôt privé : les minutes sont facturables » : les corriger alors.
9. N'en rien dire avant que le propriétaire décide.

**Résultats le jour de la bascule (08/11/2026)**, point par point :

1. Tout est fusionné dans `main` (`e7b039b`). Le `.\tools\verifier.ps1` complet a passé sur `3fca1cc` (1 416 tests, toutes les mutations, code de sortie 0, 2 h 54 min sur le PC, 07/11/2026). Le dernier commit (`e7b039b` : `std::optional::reset()` n'est pas `constexpr` en C++17, ce que g++ 14.2 refuse) a été vérifié par les 1 420 tests d'un clone neuf, les 201 mutations des alertes et un passage avec le g++ 14.2 d'ESP-IDF, **pas** par un nouveau passage complet. CI sur `e7b039b` : `tests-noyau` (deux compilateurs), `firmware-tab5`, `ruff`, `clang-format`, `gitleaks` et `parcours-web` verts ; `mutations-noyau` (non obligatoire) annulé par sa limite de 150 minutes, voir [outils.md](outils.md#intégration-continue-github-actions).
2. L'état du README et [compatibilite.md](compatibilite.md#version-française) ont été relus et mis à jour le 07/11/2026.
3. Fait le 07/11/2026 : 14 planches des images des fichiers d'aujourd'hui et de celles qui n'existent que dans l'historique, regardées à l'œil. Des villes d'exemple et de faux réseaux Wi-Fi venus des données de rendu et des tests, une IP privée et la fin d'une adresse MAC (catégories acceptées le 07/11/2026), le domaine de Vigie (public) : rien de personnel à bloquer.
4. Relancé depuis un clone neuf du dépôt distant (`e7b039b`, avec la tête de la pull request) : 0 bloquant, 0 secret ; `gitleaks` sur tout l'historique propre.
5. Les journaux des 13 lancements d'Actions ont été lus : aucun mot de passe Wi-Fi, aucun chemin personnel, aucune adresse e-mail. Aucun secret ni variable d'Actions.
6. Tests depuis un clone neuf de `main` : 1 420 tests, 0 échec, 0 fichier modifié ensuite. **Pas fait** depuis un clone neuf : `idf.ps1 build` (la compilation du Tab5 est faite par le job de CI `firmware-tab5` ; la 7B n'a pas de job de CI) et le passage complet des mutations.
7. La bascule, par `gh repo edit Axellum/meridien --visibility public --accept-visibility-change-consequences`, relue : `PUBLIC`, et la page répond sans jeton.
8. Fait juste après, par l'API de GitHub et relu : signalement privé de vulnérabilités activé ; analyse de secrets et protection à la poussée activées ; `main` protégée (les cinq contrôles requis `tests-noyau`, `ruff`, `clang-format`, `gitleaks`, `firmware-tab5` ; ni force-push ni suppression ; les administrateurs peuvent encore pousser directement, comme le fait le travail du projet). Mises à jour de sécurité de Dependabot laissées coupées : non demandées.

### Décisions qui restent au propriétaire

- **Quand basculer** : décidé et fait le 08/11/2026 (résultats ci-dessus).
- **NVS chiffrée ou non** (l'ADR-0003 la prévoit, rien n'est fait) : pas une condition de la bascule ; en attendant, ce sont les limites de [SECURITY.md](../SECURITY.md#version-française) qu'on dit au lecteur.
- Décidé le 07/11/2026 : les documents de travail deviennent publics, la plupart ([plan.md](plan.md), [cout.md](cout.md) (temps, sessions, tokens), [recherche/](recherche/)) ; ce sont des notes de travail en français et le README les présente comme telles.
