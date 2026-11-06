# Outils de développement

Installés le 06/11/2026 sur demande d'Axel (« installe tous les outils qu'il te faut pour maximiser rapidité et qualité »), **hors du dépôt**, sans modifier le PATH du système.

## Ce qui est installé

| Outil | Version | Où | Sert à |
|---|---|---|---|
| llvm-mingw : `clang`, `clang++`, `lld`, `llvm-symbolizer` | clang 23.1.5 (publication 20261104, ucrt x86-64) | `H:\AuxFilsDesIdees\_outils\llvm-mingw\` (`VERSION.txt` : source et SHA-256 vérifié) | compiler **et exécuter** les tests du noyau sur le PC ; détecteurs d'erreurs mémoire (AddressSanitizer) et de comportements indéfinis (UBSan) |
| `clang-format` | 23.1.5 | idem, et paquet pip (`--user`) | une seule mise en forme du C/C++ pour toutes les sessions |
| `clang-tidy` | 23.1.4 (pip) / 23.1.5 | idem | analyse statique du noyau pur |
| `pre-commit` | installé par pip (`--user`) | dossier `Scripts` de pip | lancer à la main des contrôles avant un commit ; **aucun hook n'est activé** dans le dépôt (il bloquerait des sessions en cours) |
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

## Ce qui n'est pas installé (volontairement)

- **gitleaks** (recherche de secrets) : à installer **avant toute publication** du dépôt ; le dépôt est local et sans remote.
- **Simulateur LVGL sur PC** (SDL2) et captures de référence : phase 5 du [plan](plan.md).
- **cppcheck** : `clang-tidy` couvre le besoin pour l'instant.
- Aucun outil ne remplace l'essai sur la carte : un test qui passe sur le PC (x86-64) ne dit rien de la mémoire, de l'alignement ni du temps sur le RISC-V.
