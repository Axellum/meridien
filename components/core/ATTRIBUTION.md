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
