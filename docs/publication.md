# Publishing the repository

## English · [Français](#version-française)

---

> **Summary** — Nothing is public yet. Decision of the owner, 06/11/2026: the public repository will be a **new repository created from a clean snapshot** of `main`; the current private repository stays as the archive of the history. `tools/instantane_public.ps1` makes the snapshot, `tools/controle_publication.py` checks it, and the owner alone decides to create the public repository.

### Why a snapshot and not the current history

Checked on 07/11/2026 on `main` (270 commits): the files of **98** commits contain a board MAC address, **7** commits touch a path of the author's PC, and **every** commit carries the author's personal e-mail address in its metadata. `gitleaks` finds no secret in the history, but it does not look for these things. The files of today's `main` no longer contain a MAC address or a personal path (they were removed on 06/11/2026, and `tools/controle_publication.py` confirms it); the history does. A snapshot keeps the files and drops the history.

What the public repository loses: the history, the annotated tag `jalon-1a-tab5-st7121`, the `Carte:` / `Teste-sur:` lines of the old commits. The documents quote commit numbers of the private history: they do not exist in the public repository, and [compatibilite.md](compatibilite.md) says so. The date, the board and the page of `mesures/` stay the evidence.

### The two tools

- **`python tools/controle_publication.py [folder]`** looks in the text of the files **and in their names** for: a MAC address, a `<drive>:\Users\<name>` path, the value of a secret variable (`--env <.env> --variable NAME`: the Wi-Fi password is looked for by the name of its variable and never printed), any string given with `--interdit`, and with `--historique` any commit identity that is not a GitHub `noreply` address. These are **blocking**. It also lists, **to judge**: e-mail addresses, private IP addresses (an example address in a test is not a leak, a human says so) and Windows drive paths. `--autotest` checks the check on a folder it builds. It does **not** read images, does not search a Wi-Fi network name shorter than 8 characters (too many false hits), and does not understand text: it finds strings.
- **`.\tools\instantane_public.ps1 -Destination <new folder> -Courriel <id>+Axellum@users.noreply.github.com`** (PowerShell 5.1 or 7): `git archive` of `refs/heads/main` (write the reference in full: `main` is also a folder of the repository), then the check above (any blocking finding stops it, nothing is committed), then `git init` and **one** commit under the `noreply` identity, then the check of the history. It publishes nothing and touches no remote.

### Checklist before the owner creates the public repository

1. The work in progress is merged, the full `.\tools\verifier.ps1` passes, the CI is green.
2. The **Status** section of the [README](../README.md#status) says what runs now, in the same words as [compatibilite.md](compatibilite.md): re-read it.
3. **Look at the images** by eye: [`captures/ecran/`](captures/ecran/), [`../web/captures/`](../web/captures/), `../web/esquisses/`, `design/esquisses/` (a city name, the Wi-Fi networks of the neighbours, a real name: the tool cannot see pixels).
4. Run the snapshot into a **new** folder (the account number for the `noreply` address: `gh api user --jq .id`; the form `<id>+<account>@users.noreply.github.com` is GitHub's documented one, not re-checked on 07/11/2026). Pass `-Interdit` for the real values of the owner's PC (IP addresses, MAC addresses) and `-FichierEnv` for the Wi-Fi password.
5. Read the **to judge** list of the check: each line is either an example or something to remove.
6. **Build and test from the snapshot alone** (a fresh clone of it): `.\tools\idf.ps1 build` and `python test/host/lancer.py --mutations`. **Not done yet** at the date of this page.
7. The name `Axellum/meridien` is taken by the private repository: rename it first (for example `meridien-archive`, it stays private) or give the public one another name. Owner's choice.
8. On the new repository: private vulnerability reporting (the [SECURITY.md](../SECURITY.md) text assumes it), branch protection with the required checks listed in [outils.md](outils.md), secret scanning and push protection. Then push, and wait for the CI of the new repository.
9. Say nothing about it before the owner decides.

### Decisions left to the owner

- **Encrypted NVS or not** (ADR-0003 plans it, nothing is done): until then the [SECURITY.md](../SECURITY.md) limits are what a reader is told.
- **The identity of the single commit**: name and `noreply` address.
- **The name of the public repository** and what happens to the private one (item 7).
- **Which working documents go public**: [plan.md](plan.md), [cout.md](cout.md) (time, sessions, tokens), [recherche/](recherche/) are working notes in French; the README presents them as such.

---

## Version Française

---

> **Résumé** — Rien n'est public. Décision du propriétaire, 06/11/2026 : le dépôt public sera un **dépôt neuf créé à partir d'un instantané propre** de `main` ; le dépôt privé actuel reste l'archive de l'historique. `tools/instantane_public.ps1` fait l'instantané, `tools/controle_publication.py` le contrôle, et le propriétaire seul décide de créer le dépôt public.

### Pourquoi un instantané et pas l'historique actuel

Constaté le 07/11/2026 sur `main` (270 commits) : les fichiers de **98** commits contiennent une adresse MAC de carte, **7** commits touchent un chemin du PC de l'auteur, et **chaque** commit porte l'adresse e-mail personnelle de l'auteur dans ses métadonnées. `gitleaks` ne trouve aucun secret dans l'historique, mais il ne cherche pas ces choses. Les fichiers du `main` d'aujourd'hui ne contiennent plus d'adresse MAC ni de chemin personnel (retirés le 06/11/2026, et `tools/controle_publication.py` le confirme) ; l'historique, si. Un instantané garde les fichiers et laisse l'historique.

Ce que le dépôt public perd : l'historique, le tag annoté `jalon-1a-tab5-st7121`, les lignes `Carte:` / `Teste-sur:` des anciens commits. Les documents citent des numéros de commit de l'historique privé : ils n'existent pas dans le dépôt public, et [compatibilite.md](compatibilite.md#version-française) le dit. La date, la carte et la page de `mesures/` restent la preuve.

### Les deux outils

- **`python tools/controle_publication.py [dossier]`** cherche, dans le texte des fichiers **et dans leurs noms** : une adresse MAC, un chemin `<lecteur>:\Users\<nom>`, la valeur d'une variable secrète (`--env <.env> --variable NOM` : le mot de passe Wi-Fi est cherché par le nom de sa variable et jamais affiché), toute chaîne donnée par `--interdit`, et avec `--historique` toute identité de commit qui n'est pas une adresse `noreply` de GitHub. Ce sont des trouvailles **bloquantes**. Il liste aussi, **à juger** : adresses e-mail, adresses IP privées (une adresse d'exemple dans un test n'est pas une fuite, c'est un humain qui le dit) et chemins de lecteur Windows. `--autotest` contrôle le contrôle sur un dossier qu'il fabrique. Il **ne lit pas** les images, ne cherche pas un nom de réseau Wi-Fi de moins de 8 caractères (trop de faux positifs) et ne comprend pas le texte : il trouve des chaînes.
- **`.\tools\instantane_public.ps1 -Destination <dossier neuf> -Courriel <id>+Axellum@users.noreply.github.com`** (PowerShell 5.1 ou 7) : `git archive` de `refs/heads/main` (écrire la référence en entier : `main` est aussi un dossier du dépôt), puis le contrôle ci-dessus (une trouvaille bloquante l'arrête, rien n'est commité), puis `git init` et **un** commit sous l'identité `noreply`, puis le contrôle de l'historique. Il ne publie rien et ne touche aucun dépôt distant.

### Liste avant que le propriétaire crée le dépôt public

1. Le travail en cours est fusionné, le `.\tools\verifier.ps1` complet passe, la CI est verte.
2. La section **État** du [README](../README.md#état) dit ce qui tourne maintenant, avec les mêmes mots que [compatibilite.md](compatibilite.md#version-française) : la relire.
3. **Regarder les images** à l'œil : [`captures/ecran/`](captures/ecran/), [`../web/captures/`](../web/captures/), `../web/esquisses/`, `design/esquisses/` (un nom de ville, les réseaux Wi-Fi des voisins, un vrai nom : l'outil ne voit pas les pixels).
4. Lancer l'instantané dans un dossier **neuf** (le numéro de compte pour l'adresse `noreply` : `gh api user --jq .id` ; la forme `<id>+<compte>@users.noreply.github.com` est celle que documente GitHub, non revérifiée le 07/11/2026). Passer `-Interdit` pour les vraies valeurs du PC du propriétaire (adresses IP, adresses MAC) et `-FichierEnv` pour le mot de passe Wi-Fi.
5. Lire la liste **à juger** du contrôle : chaque ligne est soit un exemple, soit à retirer.
6. **Compiler et tester depuis l'instantané seul** (un clone neuf de celui-ci) : `.\tools\idf.ps1 build` et `python test/host/lancer.py --mutations`. **Pas encore fait** à la date de cette page.
7. Le nom `Axellum/meridien` est pris par le dépôt privé : le renommer d'abord (par exemple `meridien-archive`, il reste privé) ou donner un autre nom au public. Au choix du propriétaire.
8. Sur le dépôt neuf : signalement privé de vulnérabilités (le texte de [SECURITY.md](../SECURITY.md#version-française) le suppose), protection de `main` avec les contrôles requis listés dans [outils.md](outils.md), analyse de secrets et protection à la poussée. Puis pousser, et attendre la CI du dépôt neuf.
9. N'en rien dire avant que le propriétaire décide.

### Décisions qui restent au propriétaire

- **NVS chiffrée ou non** (l'ADR-0003 la prévoit, rien n'est fait) : en attendant, ce sont les limites de [SECURITY.md](../SECURITY.md#version-française) qu'on dit au lecteur.
- **L'identité du commit unique** : nom et adresse `noreply`.
- **Le nom du dépôt public** et le sort du privé (point 7).
- **Quels documents de travail deviennent publics** : [plan.md](plan.md), [cout.md](cout.md) (temps, sessions, tokens), [recherche/](recherche/) sont des notes de travail en français ; le README les présente comme telles.
