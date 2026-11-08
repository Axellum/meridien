# ADR-0015 — Accueil : le mode automatique choisit le profil et dit pourquoi

> **English summary.** The pure core of the home screen's **automatic mode** is written and tested on the PC only: one function,
> `decider(Entrees) -> Decision` (`components/core/include/ecran/accueil_auto.hpp`), with no clock, no global state, no text, no
> colour, no network, no setting storage and nothing in `main/`. The time and every input arrive as arguments; the answer is the
> profile of the bottom zone (Weather, Surf, Wind·Boat, Coast), two **reasons** as enumerations (never text: the words will come
> from the seven-language table), two flags (enlarged alert banner, rain card with a countdown) and the state to give back at
> the next call. **Rules, by priority** (all thresholds are named constants, a proposal "Vigie's rule, adjustable, to be validated
> by the owner"): (1) an official **orange or red** alert in force enlarges the banner whatever the profile or the pin; (2) rain in
> **less than 60 min** shows the rain card unless a profile is pinned; (3) **Surf**: a surf spot AND the Surf activity ticked AND
> fresh sea data AND an outing slot (sunrise to 10:00, or 16:00 to sunset, local time); (4) **Wind·Boat**: activity ticked AND wind
> strictly above the set threshold; (5) **Coast**: a foot-fishing spot AND the next low tide within 3 h; (6) otherwise Weather.
> **Guards**: at most one profile change per 30 min (29 min 59 s: no, 30 min: yes), never during a touch, **stale data** sends the
> screen back to Weather at once with the reason "stale data", a **pinned** profile wins over 2 to 6 until unpinned, a house with
> no saved spot never switches to Surf or Coast, and an unknown input never triggers a switch. Status: **proposed**; PC only,
> never on a board; the choices the author had to make are listed under "À valider par Axel".

- **Statut** : **proposée**, écrite le 08/11/2026 ; à valider par Axel. Constaté (PC seulement, aucune carte) : 48 tests du noyau
  compilés et exécutés avec AddressSanitizer + UBSan (clang++ 23.1.5) et évalués à la compilation par g++ 14.2 ; 98 mutations
  ciblées, toutes détectées par le test attendu ([docs/mesures/accueil-auto.md](../mesures/accueil-auto.md)). **Non constaté** :
  tout ce qui touche l'écran, la carte, le toucher réel, la durée d'une journée réelle, et le **goût** : les règles et les
  seuils ci-dessous sont une proposition, jugée sur l'écran réel par Axel, jamais sur ce texte.
- **Contexte** : le 08/11/2026 Axel a validé un plan en six lots pour un nouvel accueil : un accueil à **profils** (Météo, Surf,
  Vent·Bateau, Côte) dont le choix est **automatique** selon les données reçues, ou **épinglé** à la main (appui long, roue).
  Les esquisses de référence (`auto-pluie`, `auto-alerte`, `auto-surf`, `etat-epingle`, `auto-regles`) montrent la pastille « Auto
  · Surf » / « Auto · pluie proche » / « Auto · alerte orange » / « Surf · épinglé », le panneau « Pourquoi cet écran ? » et la
  planche « Règles du mode automatique ». Cet ADR couvre le **lot 3, noyau pur seulement** : les données de mer, les verdicts
  Vent·Bateau et pêche à pied n'existent pas encore côté appareil, donc la fonction reçoit des **entrées abstraites** (booléens,
  minutes, dixièmes de km/h, instants), déjà calculées ailleurs.
- **Décision** :
  1. **Une fonction pure** `ecran::accueil::decider(const Entrees &) -> Decision`, `constexpr`, `noexcept`, C++17, sans horloge ni
     état global : le temps est une entrée, l'état d'un appel au suivant est rendu dans `Decision::etat` et redonné dans
     `Entrees::etat`. Elle ne calcule ni la houle, ni la marée, ni le vent d'une activité ; elle calcule seulement le **créneau de
     sortie** à partir du lever et du coucher du soleil (`soleil.hpp`) et du décalage local.
  2. **Les règles**, de la plus forte à la plus faible. 1 et 2 **s'ajoutent** à l'écran ; 3 à 6 **choisissent la zone du bas** :

     | # | Règle | Effet | Seuil (constante) |
     |---|---|---|---|
     | 1 | alerte officielle **orange ou rouge** en vigueur | `bandeau_agrandi`, quel que soit le profil, l'épinglage, le délai ou un toucher | `ALERTE_NIVEAU_MIN` = orange |
     | 2 | pluie dans **moins de** 60 min (en cours : 0) ET prévisions fraîches ; **pas** de carte si un profil est épinglé | `carte_pluie` : elle recouvre la zone du bas, le profil continue dessous | `PLUIE_PROCHE_MAX_MIN` = 60 (59 oui, 60 non) |
     | 3 | **Surf** : spot de surf enregistré ET activité Surf cochée ET données de mer fraîches ET créneau de sortie | profil Surf | créneau : du lever à 10 h (exclue), de 16 h (incluse) au coucher (exclu), heures locales : `CRENEAU_MATIN_FIN_S`, `CRENEAU_SOIR_DEBUT_S` |
     | 4 | **Vent·Bateau** : activité cochée ET vent **strictement** au-dessus du seuil réglé ET prévisions fraîches | profil Vent·Bateau (aucun spot exigé) | seuil = **réglage** de la personne (non réglé : jamais de bascule) |
     | 5 | **Côte** : spot de pêche à pied enregistré ET prochaine basse mer dans **180 min au plus** (180 oui, 181 non) ET données de mer fraîches | profil Côte | `BASSE_MER_MAX_MIN` = 180 |
     | 6 | sinon | profil Météo, raison `Defaut` | — |

  3. **Garde-fous** :
     - **un changement de profil au plus toutes les 30 min** (`CHANGEMENT_ECART_MIN_S` = 1 800 s) : 29 min 59 s plus tard, rien ;
       30 min pile, oui. Le profil que les règles voudraient changer est **gardé**, avec la raison `ChangementDiffere`. Le
       **premier** choix (aucun changement noté) est libre ;
     - **jamais pendant un toucher en cours** (le profil attend, y compris pour une donnée périmée) ; **jamais sans heure** (heure
       0 ou négative : aucun changement) ;
     - **donnée périmée** : si les règles 3 à 5 auraient choisi un profil dont la donnée est **périmée**, retour à Météo avec la
       raison `DonneesPerimees`, **tout de suite** (le délai de 30 min ne s'applique pas ; le toucher, si). Une donnée
       **inconnue** (jamais reçue) n'est **pas** périmée : aucune bascule, raison `Defaut`. Une règle bloquée par une donnée
       périmée laisse la place à la suivante si la donnée de celle-ci est fraîche ;
     - **épinglage** : un profil épinglé gagne sur 2 à 6 jusqu'à ce qu'on le désépingle ; il ne supprime pas le bandeau agrandi ;
       le désépinglage reprend les règles **tout de suite** (l'état rendu pendant l'épinglage n'a pas de dernier changement) ;
     - **maison sans spot** : Surf et Côte ne sont jamais choisis sans spot enregistré (Vent·Bateau lit le vent du lieu de
       l'écran et n'en demande pas) ;
     - **entrées manquantes** : niveau d'alerte `Inconnu`, minutes -1, fraîcheur `Inconnue`, lever ou coucher 0, heure 0, seuil de
       vent non réglé : jamais une bascule ; `Entrees{}` donne Météo sans drapeau ; un `Profil` hors table (épinglage corrompu en
       NVS) vaut « pas d'épinglage », un état hors table vaut Météo. La fonction ne lève rien et ne lit rien hors de ses entrées.
  4. **Deux raisons, jamais du texte** (`Raison`, énumération ; une phrase par valeur dans la table des sept langues, plus tard) :
     `raison` pour la **pastille** (l'alerte d'abord, puis la pluie proche, sinon la raison du profil ; épinglé : `Epingle`) et
     `raison_profil` pour le panneau « Pourquoi cet écran ? » (la raison du seul profil, toujours remplie : l'alerte et la pluie
     ne font pas oublier pourquoi la zone du bas est Surf). Valeurs : `Defaut`, `AlerteOrange`, `AlerteRouge`, `PluieProche`,
     `Surf`, `VentBateau`, `Cote`, `DonneesPerimees`, `Epingle`, `ChangementDiffere`.
  5. **Ce que l'appelant devra fournir** (rien n'est branché : `main/` n'utilise pas ce noyau) : `niveau_alerte` ← le plus grave
     des alertes en vigueur (`vigie::Vue::pire`, déjà filtré par le réglage « niveau minimal ») ; `pluie_dans_min` ←
     `meteo::PluieProche::dans_min` ; `fraicheur_meteo` ← `connue` et `perimee` de la même structure ; `lever_s` / `coucher_s` ←
     `soleil::LeverCoucher` du jour local ; `decalage_local_s` ← le décalage en vigueur du lieu ; le reste vient des réglages
     (activités, spots, seuil de vent, épinglage), du toucher et de la marée (lot suivant).
- **Limites écrites** (reprises dans l'en-tête, pour que l'écran ne les contredise pas) :
  - « **aube** » = le **lever du soleil** : `soleil.hpp` ne calcule pas le crépuscule ; une sortie de pointe du jour commence
    souvent avant le lever ;
  - le créneau exige un lever **et** un coucher du **jour local** de l'instant : un lever d'hier, un coucher de demain, un jour ou
    une nuit polaire n'en donnent pas. Un lever après 10 h ou un coucher avant 16 h (hiver très au nord) vide la fenêtre ; un
    coucher après minuit local aussi. 10 h et 16 h sont lues avec le décalage en vigueur **à l'instant** : sur le jour d'un
    changement d'heure, avant le changement, la lecture est décalée d'une heure (aucun créneau n'existe alors) ;
  - **Côte ne regarde que la prochaine basse mer** : cinq minutes après l'étale, la basse mer suivante est à ~12 h et Côte
    s'éteint (la pêche à pied dure pourtant des deux côtés de l'étale) ;
  - **la carte pluie n'a pas d'hystérésis** : une prévision qui oscille entre 59 et 61 minutes la fait apparaître et disparaître ;
    sous une pluie continue elle reste affichée (en cours = 0 min) ;
  - **le bandeau agrandi et la carte pluie ne sont pas gelés par le délai de 30 min ni par un toucher** : ce sont des ajouts
    d'écran, pas des changements de profil ; c'est à l'interface de ne pas redessiner sous le doigt ;
  - **aucun seuil officiel n'existe** pour le surf, le kite, le bateau ou la pêche à pied : tout est « règle de Vigie, réglable,
    à valider » et l'écran ne dit jamais « sûr ».
- **Alternatives écartées** :
  - *Une machine à états avec hystérésis sur chaque seuil* (entrer à 55 min, sortir à 75) : ce n'est pas demandé et ajoute des
    seuils à régler ; le garde-fou de 30 min porte déjà sur le profil. Écrit comme limite pour la carte pluie.
  - *Calculer la fraîcheur, le vent ou la marée dans ce noyau* : ces notions appartiennent à d'autres lots (client Vigie, marées,
    surf) ; ici elles arrivent déjà calculées, ce qui rend la décision testable sans réseau ni horloge.
  - *Un seul champ « raison »* : la pastille dit l'alerte ou la pluie, mais le panneau doit encore pouvoir dire pourquoi la zone du
    bas est Surf ; deux champs coûtent peu et évitent de perdre l'information.
  - *Une fraîcheur booléenne* : elle ne distingue pas « jamais reçue » de « périmée », deux cas que la proposition traite
    différemment (aucune bascule ; retour à Météo avec une raison).
  - *Un état gardé dans le noyau* (variable globale) : interdit par la consigne (pas d'état global) et intestable.
- **À valider par Axel** (choix faits par l'auteur de ce noyau, pas par l'esquisse) :
  1. une donnée **périmée** ramène à Météo **sans attendre** les 30 min (montrer du périmé pour du frais est pire qu'un
     va-et-vient) ; le toucher, lui, attend ;
  2. « périmée » ≠ « inconnue » ; une règle n'est dite « bloquée par une donnée périmée » que si **toutes ses autres conditions**
     sont remplies ;
  3. la pastille dit **l'alerte, puis la pluie, puis le profil** ; épinglé, elle dit « épinglé » même sous alerte (le bandeau
     agrandi suffit à l'alerte) ;
  4. la carte pluie compte la pluie **en cours** (0 min) ; sans hystérésis ; non gelée par le toucher ni le délai ;
  5. le **désépinglage** reprend les règles tout de suite, sans 30 min ;
  6. Côte s'éteint **à l'étale** (prochaine basse mer seulement), 180 min comprises ; faut-il le garder 1 ou 2 h après ?
  7. « aube » = lever du soleil, créneaux à demi-ouverts [début, fin[, 10 h et 16 h fixes (pas réglables) ;
  8. le vent doit être **strictement** au-dessus du seuil ; un seuil non réglé ne bascule jamais (la valeur par défaut du réglage
     reste à fixer par le lot « réglages ») ;
  9. Vent·Bateau n'exige aucun spot ni créneau, Côte n'exige aucune activité cochée (la planche ne les cite pas) ;
  10. une règle bloquée par une donnée périmée **laisse la place** à la suivante dont la donnée est fraîche.
- **À prouver (non vérifié)** :
  - **tout ce qui touche l'écran et la carte** : rien n'appelle ce noyau (`main/` n'est pas touché), aucune compilation ESP-IDF
    n'a été lancée pour ce changement ; le coût sur le P4 n'est pas mesuré (quelques dizaines de comparaisons entières par appel) ;
  - **le bon goût des seuils** (60 min, 3 h, 30 min, 10 h / 16 h) : à juger sur l'écran réel pendant une à deux semaines, comme
    pour le barème du surf (ADR-0013) ; aucun d'eux ne vient d'une source ;
  - que les **textes** de la pastille et du panneau, non écrits ici, tiennent en sept langues ;
  - que les données réelles (pluie, alertes, soleil) donnent bien les entrées décrites au point 5 de la décision.
- **Conséquences** :
  - Les lots suivants brancheront `decider()` : appel périodique (par exemple une fois par seconde ou à chaque donnée reçue, avec le
    toucher courant), dessin du profil rendu, pastille et panneau à partir des deux raisons, mémoire de `Decision::etat` entre deux
    appels (et sa remise à zéro si le lieu change), réglages d'épinglage, d'activités et de seuil de vent (registre de réglages,
    ADR-0007), table des phrases en sept langues pour les dix `Raison`.
  - Changer un seuil = changer l'en-tête, ce tableau et `mutations_accueil_auto.py` ensemble ; un test échoue si l'un diverge.
  - Les mutations de ce noyau sont dans `test/host/mutations_accueil_auto.py`, lues par `lancer.py --mutations` (donc par le job
    `mutations-noyau` de la CI, lancé à la main).
  - Ligne à l'index `docs/decisions/README.md` : `0015 — Accueil : le mode automatique choisit le profil et dit pourquoi`.
  - À garder en tête : **les règles et les seuils sont une proposition jamais jugée sur un écran ; aucune ne vient d'une source
    officielle ; la fonction est testée, pas branchée.**
