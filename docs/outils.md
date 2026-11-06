# Outils de développement

Installés le 06/11/2026 sur demande d'Axel (« installe tous les outils qu'il te faut pour maximiser rapidité et qualité »), **hors du dépôt**, sans modifier le PATH du système.

## Ce qui est installé

| Outil | Version | Où | Sert à |
|---|---|---|---|
| llvm-mingw : `clang`, `clang++`, `lld`, `llvm-symbolizer` | clang 23.1.5 (publication 20261104, ucrt x86-64) | `H:\AuxFilsDesIdees\_outils\llvm-mingw\` (`VERSION.txt` : source et SHA-256 vérifié) | compiler **et exécuter** les tests du noyau sur le PC ; détecteurs d'erreurs mémoire (AddressSanitizer) et de comportements indéfinis (UBSan) |
| `clang-format` | 23.1.5 | idem, et paquet pip (`--user`) | une seule mise en forme du C/C++ pour toutes les sessions |
| `clang-tidy` | 23.1.4 (pip) / 23.1.5 | idem | analyse statique du noyau pur |
| `gitleaks` | 8.30.1 | `H:\AuxFilsDesIdees\_outils\gitleaks\` (`VERSION.txt` : source et SHA-256 vérifié) | recherche de secrets dans l'index (avant commit) et dans l'historique |
| `pre-commit` (paquet pip) | installé par pip (`--user`) | dossier `Scripts` de pip | **non utilisé** : le crochet du dépôt est un petit script maison (`.githooks/pre-commit`), sans téléchargement de crochets tiers |
| `ruff`, `pytest`, `playwright`, `esptool`, `pyserial`, Node | déjà présents avant | — | tests Python, parcours de la page, flash, serveur du faux appareil |

Téléchargement d'llvm-mingw : <https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104>, archive de 191 037 046 octets, SHA-256 `552191b5…1b40f` identique à celui publié par GitHub ; extraite, l'archive est gardée dans `_outils\_telechargements\`.

## Se servir des outils

```powershell
. H:\AuxFilsDesIdees\_outils\env_hote.ps1 -Sanitizers   # PATH + CXX pour cette session PowerShell seulement
python test/host/lancer.py                              # doit afficher : mode « complet (compilé + exécuté) »
python test/host/lancer.py --mutations                  # chaque mutation du noyau doit être détectée
clang-format --dry-run -Werror components/core/include/ecran/*.hpp
```

Sans `-Sanitizers`, `CXX` pointe sur `clang++` simple (plus rapide, sans détecteurs) : utile pour les mutations. `clang++-san.cmd` ajoute `-fsanitize=address,undefined`.

## Contrôles automatiques

| Quoi | Comment | Bloque ? |
|---|---|---|
| **Avant chaque commit** | crochet `.githooks/pre-commit` → `tools/verifier.ps1 -Rapide` (ce qui est dans l'index seulement) | oui pour les secrets, les tests du noyau et ruff ; non pour la mise en forme (avertissement) |
| **À la demande, tout le dépôt** | `.\tools\verifier.ps1` (ajouter `-Web` pour le parcours Playwright de la page, qui demande le réseau) | idem, plus les mutations |

- Le crochet est activé dans le dépôt par `git config core.hooksPath .githooks` (fait le 06/11/2026). Il est vu par tous les worktrees, mais le dossier `.githooks` n'existe dans un worktree qu'après y avoir fusionné `main` : avant cela, lancer `.\tools\verifier.ps1` à la main.
- **Ne jamais** contourner avec `--no-verify` : corriger la cause. Le crochet refuse aussi un commit si le lanceur de tests n'est pas en mode « complet » (rien d'exécuté).
- Preuve du 06/11/2026 : un faux jeton Slack dans l'index → commit refusé (gitleaks, code 1) ; une assertion cassée (`NOMBRE_CODES == 30`) → commit refusé (code 1, erreur du compilateur sur la ligne) ; dans les deux cas aucun commit créé et l'arbre est revenu propre. Vérification complète sur `main` : secrets OK, 23 tests exécutés, 18 mutations sur 18, ruff OK, mise en forme 235 écarts en avertissement.
- `ruff.toml` est propre au projet : sans lui ruff héritait d'une configuration plus stricte (règles de sécurité du dépôt Tab5) trouvée plus haut dans `H:\AuxFilsDesIdees`.
- `.clang-format` est la **cible** de mise en forme (LLVM, 4 espaces, 120 colonnes). Le code existant n'y est pas conforme (tableaux alignés à la main) : un seul commit de reformatage, à part, après la fusion des pistes en cours ; en attendant, c'est un avertissement.

## Ce qui n'est pas installé (volontairement)

- **Simulateur LVGL sur PC** (SDL2) et captures de référence : phase 5 du [plan](plan.md).
- **cppcheck** : `clang-tidy` couvre le besoin pour l'instant.
- Aucun outil ne remplace l'essai sur la carte : un test qui passe sur le PC (x86-64) ne dit rien de la mémoire, de l'alignement ni du temps sur le RISC-V.
