# Changelog fragments / Fragments du journal des changements

A pull request does not edit `CHANGELOG.md`: it adds **its own file in this folder**, so that two pull requests open at the
same time never conflict on the same lines. *Une pull request n'écrit pas dans `CHANGELOG.md` : elle dépose son entrée dans
un fichier à elle, ici, pour que deux PR ouvertes en même temps n'entrent pas en conflit.*

## Writing an entry / Écrire son entrée

1. **File name**: the branch name, `/` changed to `-`, in lower case (`feature/weather-icons` →
   `feature-weather-icons.md`).
2. **Content**: a piece of the future changelog, in English (add a French line if you like), written like this:

   ```markdown
   ### Added

   - **Short title**: what changes, for whom, and where it was seen. Tested on: `tab5-st7121`, 2026-11-06, commit `abc1234`
     (or: not tested on board).
   ```

   Allowed headings, in this order (Keep a Changelog): `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`.
   Several headings in one fragment are fine if the PR does several things. Links are relative to **this folder**
   (`../docs/outils.md`), so that they work where the fragment is read on GitHub and `tools/controle_liens.py` can check
   them; whoever gathers the fragments into `CHANGELOG.md` removes the leading `../`. *Les liens partent de ce dossier
   (`../docs/outils.md`) ; qui rassemble les fragments dans `CHANGELOG.md` retire le `../` de tête.*
3. **What counts**: anything a user of the screen or of the settings page can notice. Internal chores, typo fixes and
   test-only changes need no fragment.
4. The same rule as everywhere in this repository: never write "tested" without a board, a commit and a date.

*Nom du fichier : le nom de la branche, `/` changé en `-`, en minuscules. Contenu : un morceau du futur journal, en
anglais (une ligne en français si vous voulez), sous un titre `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed` ou
`Security`. Ne jamais écrire « testé » sans carte, commit et date.*

## Assembling / Assembler

There is no assembly script yet. At release time, whoever cuts the release merges the fragments **by hand** into
`CHANGELOG.md` (created with the first release), newest entry first under each heading, then deletes the merged fragments.
This `README.md` stays. If a script is added later, describe it here.

*Pas encore de script d'assemblage : à la publication d'une version, la personne qui la fait fusionne les fragments à la
main dans `CHANGELOG.md` (créé avec la première version), l'entrée la plus récente en premier sous chaque titre, puis
supprime les fragments fusionnés.*
