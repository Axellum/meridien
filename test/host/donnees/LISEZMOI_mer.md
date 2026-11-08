# Données des tests « mer »

Ce dossier sert à `test_mer_trame.cpp`, `test_mer_vue.cpp` et `test_mer_variantes.cpp` (noyau pur
`components/core/include/ecran/mer_trame.hpp` et `mer_vue.hpp`). Contexte, décisions et limites :
[ADR-0018](../../../docs/decisions/0018-mer-trame-vigie-et-vues.md) ; ce qui a été constaté et ce qui ne l'est pas :
[docs/mesures/mer.md](../../../docs/mesures/mer.md).

## Les trames réelles (capturées le 08/11/2026 vers 13 h 15 UTC depuis le PC d'Axel)

Six spots de la côte française, **une trame chacun**, le corps de `GET /v1/embedded/frame?device_type=generic&spot=<id>` tel que
l'instance publique de Vigie l'a rendu, octet pour octet (le contrat est `docs/embedded-frame.md` du dépôt `vigie-api`). L'adresse
publique est lue dans `vigie_requete.hpp` par le script de capture, jamais recopiée. **Aucune position GPS dans la requête** : les
zones et les avertissements du monde que l'API rend sans elle font partie de la trame, et le décodeur de la mer doit les sauter.

| Fichier | Spot (identifiant chez Vigie) | Ce qu'il apporte aux tests |
|---|---|---|
| `mer_hossegor` | Soorts-Hossegor, plage Sud (`soorts-hossegor-plage-sud-point-d-or`) | le spot du contrat : marée de 4 m, six marées, trois coefficients |
| `mer_la_torche` | Beuzec, plage de la Torche (`beuzec-plage-de-la-torche`) | Bretagne ouest : surf « possible » (`wind.strong`), kite « possible » (`gust_gap.gusty`) |
| `mer_rochebonne` | Saint-Malo, plage de Rochebonne (`saint-malo-plage-de-rochebonne`) | Manche : très grand marnage (hauteurs de marée hors de l'ordinaire) |
| `mer_port_vieux` | Biarritz, plage du Port Vieux (`biarritz-plage-du-port-vieux`) | plage abritée : autres verdicts qu'un spot ouvert |
| `mer_govelle` | Le Pouliguen, plage de la Govelle (`le-pouliguen-plage-de-la-govelle`) | plage orientée au sud, houle d'ouest filtrée |
| `mer_ondarraitz` | Hendaye, Ondarraitz (`hendaye-ondarraitz`) | frontière espagnole : marée des Pyrénées-Atlantiques |

Les six trames portent chacune une ligne `SEA`, quatre `ACT` et quinze `CRIT`, plus les lignes que le décodeur ne lit pas
(`HEAD`, `ZONE`, `RAIN5M`, `DAY`, `HAZ`) : il doit les sauter sans perdre le lien entre une activité et ses critères.
L'instant de la ligne `SEA` (« généré le ») est pris pour « maintenant » dans les tests : la trame n'est valable qu'à cet instant.

| Produit | Par | Rôle |
|---|---|---|
| `mer_<nom>.txt`, `mer_captures.json` | `capturer_mer.py` (appelle le réseau : à lancer à la main, pas dans le contrôle) | la donnée réelle, et sa provenance (adresse, instant, taille, lignes par balise) |
| `mer_<nom>.inc` | `controle_python_mer.py` (**généré, ne pas modifier à la main**) | la trame en littéral C++ pour `#include` dans un test |
| `attendu_mer_<nom>.inc` | `controle_python_mer.py` (**généré**) | ce que la lecture indépendante en Python attend (CRC, nom, heures, marées, coefficients, verdicts, critères) |

`python test/host/controle_python_mer.py --verifier` échoue si un `.inc` n'est pas à jour ; le lanceur (`lancer.py`) l'appelle à la fin
d'une exécution complète.

## Ce que la seconde lecture en Python prouve, et ce qu'elle ne prouve pas

- **Indépendante** du noyau C++ : le CRC32 vient de `zlib`, la découpe des lignes, des marées et des bandes est écrite à part. Un défaut
  de CRC, d'indice de champ, de signe ou d'arrondi de hauteur dans le C++ y est vu.
- **Pas indépendante pour le contrat** : le lecteur Python et le lecteur C++ ont été écrits par la même personne, à partir du même
  texte du contrat (`docs/embedded-frame.md`). Si cette lecture du contrat est fausse, les deux le sont de la même façon.
- **Une seule journée de capture, et aucun avertissement côtier dedans** : au moment de la capture les six trames disent « vigilances
  lues, aucun avertissement » (`ok`, niveau vide), et aucun spot n'a un surf « bon » (« possible » à la Torche, « déconseillé » ailleurs).
  Ces cas (avertissement orange ou rouge, surf « bon ») sont couverts par des trames **réécrites à la main et resignées**
  (`test_mer_variantes.cpp`, dont l'en-tête le dit), pas par des données réelles.
- Une trame change avec l'heure de la requête et avec les évolutions de Vigie : refaire une capture change les valeurs ; il faut alors
  relancer `controle_python_mer.py` (et relire l'écart avant de le valider).

**Vu sur** : PC (Windows 11), captures du 08/11/2026 de 13:15:38 à 13:15:53 UTC (instants de chaque trame dans `mer_captures.json`),
aucune carte. Ces trames n'ont pas été reçues par une carte.

## Licences et crédits des données

Les trames sont gardées **telles que Vigie les a rendues**, sans modification. Ce qu'elles contiennent vient des sources de Vigie, dont les
licences et les crédits sont dans le `SOURCES.md` du dépôt `vigie-api` (lu le 08/11/2026 pour ce qui suit) :

- la houle et le vent du bloc `SEA`/`ACT`/`CRIT` : modèle MFWAM de Météo-France (produit Copernicus Marine) et vent, via l'**API Marine
  d'Open-Meteo** ; données sous licence **CC BY 4.0** ; crédit demandé : « Weather data by Open-Meteo.com », puis « Generated using
  E.U. Copernicus Marine Service Information » avec le DOI du produit ;
- la marée du bloc `SEA` : **calculée par Vigie** à partir du modèle **EOT20** (Hart-Davis et al., 2021, DGFI-TUM), licence **CC BY 4.0**,
  constantes arrondies par Vigie, « pas pour la navigation » ; le coefficient est une estimation de Vigie, pas celui du SHOM ;
- les lignes `HAZ` (séismes, volcans, crues, cyclones du monde entier) viennent d'autres sources (EMSC, USGS, NOAA NWS, NHC…) : **leur
  licence de redistribution n'a pas été examinée pour ce dépôt**. Elles ne sont lues par aucun test pour leur contenu.

Les crédits sont repris dans [THIRD_PARTY.md](../../../THIRD_PARTY.md) et [components/core/ATTRIBUTION.md](../../../components/core/ATTRIBUTION.md).
