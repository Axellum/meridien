# Attributions du noyau pur

## Table des fuseaux (`include/ecran/fuseaux_donnees.inc`)

- **posix_tz_db** — https://github.com/nayarsystems/posix_tz_db, fichier `zones.csv`, licence **MIT**.
  Version embarquée : commit `93447c0ddac304ca6672a5fd905261c7e8905159` (2025-09-29), récupérée le 06/11/2026,
  461 zones. Copie intacte du fichier et de sa licence dans [outils/posix_tz_db/](outils/posix_tz_db/) (voir `SOURCE.txt`).
  Remarque : le fichier `LICENSE` du dépôt d'origine contient encore l'en-tête non rempli « Copyright (c) [year]
  [fullname] » ; nous le recopions tel quel.
- **Base de données des fuseaux horaires (tz / IANA)** — https://github.com/eggert/tz, domaine public (les fichiers
  portent « This file is in the public domain »). Lue le 06/11/2026 pour **7 compléments** absents de posix_tz_db :
  `Europe/Kyiv` et `Pacific/Kanton` (liens du fichier `backward` vers `Europe/Kiev` et `Pacific/Enderbury`),
  `UTC` et `GMT` (liens vers `Etc/UTC` et `Etc/GMT`), `America/Ciudad_Juarez` (dernière règle de la zone =
  celle d'`America/Denver`), `America/Coyhaique` (`<-03>3`) et `Asia/Qostanay` (`<+05>-5`). Chaque complément est
  cité avec son fichier source dans `outils/generer_fuseaux.py`.
- La table est régénérée par `python components/core/outils/generer_fuseaux.py`.

Texte de la licence MIT de posix_tz_db : voir [outils/posix_tz_db/LICENSE](outils/posix_tz_db/LICENSE).

## Table météo (`include/ecran/meteo_wmo.hpp`)

- Codes météo WMO (« WMO Weather interpretation codes (WW) ») tels que listés par **Open-Meteo**,
  https://open-meteo.com/en/docs, page lue le 06/11/2026. Les données météo d'Open-Meteo sont sous licence
  **CC BY 4.0** (attribution à afficher dans l'application, voir `docs/recherche/03-…`, §1) ; le tableau des codes
  est un standard de l'Organisation météorologique mondiale. Les libellés français sont une traduction du projet.

## Données du noyau « surf » (`include/ecran/surf_*.hpp`, `test/host/donnees/surf_*`)

- **Open-Meteo** — API Marine (`marine-api.open-meteo.com/v1/marine`, vagues, houle, température de la mer, niveau de la mer,
  page lue le 07/11/2026 : https://open-meteo.com/en/docs/marine-weather-api) et API de prévision (vent à 10 m). Données sous
  licence **CC BY 4.0** (https://creativecommons.org/licenses/by/4.0/), à créditer avec un lien vers https://open-meteo.com
  partout où elles sont montrées. Les 30 réponses réelles gardées comme données de test (15 lieux × mer et vent, capturées le
  07/11/2026 par `test/host/capturer_surf.py`) sont reproduites **telles quelles, sans modification** ; le noyau les lit et arrondit
  les valeurs (nombres entiers : centimètres, dixièmes de seconde et de km/h).
- La formule du vent par rapport à la plage, le barème de la note et le meilleur créneau ne viennent d'aucune source : ils sont
  écrits par le projet ([ADR-0013](../../docs/decisions/0013-surf-donnees-note-et-limites.md)).
- La formulation de licence de chaque fournisseur de modèles de vagues derrière l'API Marine (services nationaux, ECMWF…) n'a **pas
  été examinée** : seule la licence de l'API d'Open-Meteo est citée ici.

## Données du noyau « mer » (`include/ecran/mer_*.hpp`, `test/host/donnees/mer_*`)

- **Vigie** (instance publique `vigie.auxfilsdesidees.fr`, trame embarquée `GET /v1/embedded/frame?spot=…`, lignes `SEA`, `ACT`,
  `CRIT`). Les six trames réelles gardées comme données de test (spots de la côte française, capturées le 08/11/2026 par
  `test/host/capturer_mer.py`) sont reproduites **telles quelles, sans modification**, avec les autres lignes qu'elles portent ; le noyau
  les lit et arrondit les valeurs (nombres entiers : centimètres, dixièmes).
- Ce que le bloc mer contient vient, d'après le `SOURCES.md` de `vigie-api` (lu le 08/11/2026) : de l'**API Marine d'Open-Meteo**
  (**CC BY 4.0**, https://open-meteo.com/en/licence) avec le modèle de vagues **MFWAM** de Météo-France (produit Copernicus Marine,
  « Generated using E.U. Copernicus Marine Service Information; https://doi.org/10.48670/moi-00017 ») ; d'une **marée calculée par Vigie**
  à partir du modèle **EOT20** (Hart-Davis, Piccioni, Dettmering, Schwatke, Passaro et Seitz, 2021, DGFI-TUM, **CC BY 4.0**,
  https://doi.org/10.17882/79489 ; constantes arrondies par Vigie, « pas pour la navigation »). Le coefficient de marée est une
  estimation de Vigie, pas celui du SHOM. Ces crédits sont à montrer là où la page mer montrera ces données (elle n'existe pas encore).
- Les lignes `HAZ` des mêmes trames (séismes, volcans, crues, cyclones du monde entier : EMSC, USGS, NOAA NWS, NHC…) viennent d'autres
  sources : leur licence de redistribution n'a **pas été examinée** pour ce dépôt, et aucun test n'en lit le contenu.
- Les verdicts, les critères et les seuils sont ceux de Vigie, pas d'une règle officielle ; ils ne sont **pas faits pour la sécurité**
  ([ADR-0018](../../docs/decisions/0018-mer-trame-vigie-et-vues.md)).
