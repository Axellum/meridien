# ADR-0023 — Accueil : le mode automatique branché à l'écran (pastille, carte pluie, bandeau d'alerte agrandi), sans réglage

> **English summary.** The automatic mode's pure core (ADR-0015, `decider()`) is now **wired to the home screen**, still with **no
> setting** (nothing is pinned). A second pure piece, the **pilot** (`components/core/include/ecran/accueil_pilote.hpp`), is what the
> screen calls twice a second: it turns the device's real measurements (worst official alert already filtered, rain in N minutes,
> weather freshness, sunrise and sunset computed for the place, local offset, touch, time) into the inputs of `decider()`, keeps
> `Decision::etat` from one call to the next, resets it when the place changes, and returns what the screen must show. Inputs that
> do not exist on the device yet (activities, spots, wind threshold, sea data, low tide) are given as **unknown**, so the rules never
> switch on them; the only profile the device can draw is Weather. While a finger is down the presentation is **frozen** (10 s at
> most), so nothing is redrawn under the finger. The screen shows three things, each one LVGL object per zone, no new object:
> the **pastille** beside the place name (« Auto · pluie proche », « Auto · alerte orange »…, a role colour, never a hard-coded
> one), the **rain card** that covers the five days (countdown, strength, duration, total, bars of the coming hours) and the
> **enlarged alert banner** for an orange or red alert (alert card, small rain tile on its right, the five days reduced to a
> strip). **When both rules are true the alert composition wins** and the rain is still said by the small tile. Sixteen short
> phrases are added to the seven-language table (German, Dutch, Spanish, Italian and Turkish are **written by the AI**, not
> checked by a native speaker). All thresholds are still "Vigie's rule, to be validated by the owner", none official. Status:
> **proposed**; seen on PC renders and compiled for both boards, **never flashed, never run on a board**.

- **Statut** : **proposée**, écrite le 08/11/2026 ; à valider par Axel. Constaté (PC et compilation, aucune carte) : 1 694 tests du noyau avec AddressSanitizer et UBSan, 104 mutations toutes détectées,
  117 rendus LVGL regardés (13 états, 3 tailles, français, allemand et turc), compilations du Tab5 et de la 7B depuis un arbre propre
  (+17,6 Ko d'image, +512 octets de RAM statique, le code lié aux deux binaires)
  ([docs/mesures/accueil-auto-ecran.md](../mesures/accueil-auto-ecran.md)). **Non constaté** : tout ce qui se juge sur l'écran réel
  (le goût, la lisibilité à un mètre, le toucher), la durée d'une vraie journée, les cinq langues écrites sans relecteur.
- **Contexte** : le 08/11/2026 Axel a validé un plan en six lots pour un nouvel accueil (voir [ADR-0015](0015-accueil-mode-automatique.md)).
  Le lot 3 est le **branchement** du noyau de décision : il est livré en **deux PR empilées**. Celle-ci (A) branche `decider()` à
  l'écran **sans réglage** ; la suivante (B) ajoute le réglage « Profil » et « Profil ▸ » dans la roue d'appui long. La consigne
  cite deux esquisses, `auto-pluie` (la carte pluie) et `auto-alerte` (le bandeau agrandi) : **je ne les ai pas retrouvées** (ni dans
  le dépôt, ni dans les notes de la session qui m'a lancé). La composition suit leur description ; les nombres (bande de 92 px,
  tuile de 360 px, écart de 16 px) sont **choisis par l'auteur**, pas mesurés : à comparer aux esquisses par Axel.
- **Décision** :
  1. **Un pilote pur entre les mesures et `decider()`** : `ecran::accueil::Pilote` (`accueil_pilote.hpp`, C++17, `constexpr`, aucun
     include ESP-IDF ni LVGL, aucune allocation). `main/ecran_principal.cpp` ne décide rien : il lit les mesures (toutes les 500 ms),
     appelle `avancer()` et passe la `Presentation` rendue à l'écran. Le pilote garde l'état de la décision d'un appel au suivant
     (`Decision::etat`) et le **remet à zéro quand le lieu change** (clé du lieu = nom + position arrondie à 1e-4°, une dizaine de
     mètres) : le dernier changement de profil d'une autre ville ne retient pas celle-ci.
  2. **Ce qui est réel, ce qui est inconnu.** Réel : l'alerte (la plus grave, déjà filtrée par le niveau minimal réglé), la pluie
     (`pluie_dans_min` : 0 il pleut, > 0 minutes, -1 rien dans 2 h, -2 inconnu), la fraîcheur de la météo, le **lever et le coucher
     du soleil** du jour local (calculés par `soleil.hpp`, une fois par jour, par lieu et par décalage), le décalage local, le
     toucher, l'heure. **Inconnu** (donc jamais une bascule) : les activités, les spots, le seuil de vent, les données de mer, la
     basse mer : ils n'existent pas encore sur l'appareil (lots 4 à 6).
  3. **Seul le profil Météo se dessine.** `PROFILS_DISPONIBLES` (un bit par profil) ne contient que Météo. Un profil que les règles
     rendraient quand même est ramené à Météo (filet de sécurité) ; un profil épinglé qui n'est pas disponible vaut « pas
     d'épinglage ». Surf, Vent·Bateau et Côte deviendront disponibles avec leur dessin.
  4. **Pas de redessin sous le doigt.** ADR-0015 laissait à l'interface le soin de ne pas changer l'écran sous un toucher (le bandeau
     agrandi et la carte pluie n'étant gelés par rien dans `decider()`). Le pilote **gèle** la présentation tant qu'un doigt est
     posé, **10 s au plus** (`GEL_TOUCHER_MAX_S` : un doigt oublié ou un relâchement perdu ne cache pas une alerte). Le gel
     s'arrête tout de suite quand la personne change l'épinglage ou que le lieu change (ce sont ses gestes, pas ceux des règles).
  5. **Le dessin** (`main/ui_principal.cpp`, un objet LVGL par zone, aucun objet de plus : `ui_principal_cout()` est inchangé) :
     - **la pastille** à côté du nom du lieu : la phrase de la `Raison` dans une puce de 36 px de haut, teintée d'un **rôle** du thème
       (`Ton` : alerte = couleur de son niveau, pluie = couleur de la pluie, épinglé = accent, sinon neutre) ; épinglé, elle dit
       « Météo · épinglé » suivie de « Auto en pause ». Le nom du lieu est coupé avant la pastille (jamais l'inverse) ; sans la place
       de la pastille, rien n'est dessiné plutôt qu'un texte rogné ;
     - **la carte pluie** : recouvre la carte des cinq jours (portrait compris) : « Pluie en cours » ou « Pluie dans N min », la
       force (« Pluie faible · puis plus forte vers 11 h »), la durée (« environ 5 h », dite **seulement** si la pluie finit dans les
       données) et le cumul (« 7,4 mm en tout »), les barres des prochaines heures (9 en paysage, 8 en portrait) sur une échelle
       plancher de 3 mm/h, avec les repères « 1 mm/h » et « 3 mm/h » quand ils tiennent dans le graphique ;
     - **le bandeau d'alerte agrandi** (alerte orange ou rouge) : une grande carte d'alerte (titre du niveau, texte de l'émetteur
       sur deux lignes au plus, fenêtre horaire), la **tuile de pluie** à sa droite (« Pluie dès 13 h », « Pluie en cours »,
       « Données de 07:52 · périmées »…), les cinq jours réduits à une bande de 92 px en bas ; en portrait la tuile passe sous la
       carte. La carte d'alerte reste l'objet cliquable de l'alerte officielle (la même pastille d'avant, plus grande).
  6. **Repli, jamais une carte vide.** Le noyau peut demander ce que les données de l'écran ne soutiennent pas : un bandeau agrandi
     sans alerte officielle en main retombe sur la carte pluie, ou sur l'accueil d'avant ; une carte pluie dont l'épisode n'est pas
     dans les cumuls horaires retombe sur l'accueil d'avant. La disposition n'est recalculée que quand sa clé change.
  7. **Alerte et pluie ensemble : l'alerte passe avant.** Si les règles 1 et 2 sont vraies, la composition est celle de l'alerte
     (une alerte officielle ne cède jamais sa place) et la pluie reste dite par la petite tuile ; la pastille dit l'alerte
     (ADR-0015). Épinglé : la carte pluie disparaît, le bandeau agrandi reste.
  8. **Les textes.** Seize clés courtes dans `TEXTES` (« Auto · Météo », « Auto · alerte orange », « Auto · alerte rouge », « Auto ·
     pluie proche », « Auto · Surf », « Auto · Vent·Bateau », « Auto · Côte », « Auto · données périmées », « Météo · épinglé »,
     « Épinglé », « Auto · changement différé », « Auto en pause », « Pluie faible », « Pluie modérée », « Alerte orange »,
     « Alerte rouge ») et trois phrases à trou dans `PHRASES`, dans les **sept** langues ; un test refuse une traduction manquante,
     égale à la clé, trop longue (28 octets pour `TEXTES`) ou dont les marques de format diffèrent. Les cinq langues autres que le
     français et l'anglais sont **écrites par l'IA**.
  9. **Aucune écriture en NVS** : rien ne se garde d'un démarrage à l'autre dans ce lot (l'état de la décision repart de zéro).
- **Limites écrites** :
  - **Le profil ne change jamais aujourd'hui** : sans activité, sans spot, sans donnée de mer, `decider()` rend Météo ; seuls la
    pastille, la carte pluie et le bandeau agrandi varient. Les raisons « Surf », « Vent·Bateau », « Côte » et « changement
    différé » ont leur phrase mais ne s'affichent pas encore ;
  - **la carte pluie n'a pas d'hystérésis** (ADR-0015) : une prévision qui oscille entre 59 et 61 minutes la fait apparaître et
    disparaître ; le gel du toucher ne couvre que la présence d'un doigt ;
  - **le lever et le coucher du soleil** sont ceux du lieu et de la date de l'appareil ; sans position ou sans fuseau connus, ils
    valent « inconnu » ;
  - **les classes de force de la pluie** (faible < 1 mm/h, modérée < 3 mm/h, sinon forte) sont celles que l'accueil utilise déjà
    pour ses barres (`niveau_pluie`, ADR-0016), pas un choix de ce lot ; je ne les ai **pas** comparées aux classes officielles :
    non vérifié ;
  - le panneau « Pourquoi cet écran ? », l'état « nuit », l'indice d'appui long (« une seule fois ») ne sont **pas** faits : voir
    plus bas ;
  - les cartes ont été dessinées pour 1280×720, 1024×600 et 720×1280 ; une autre taille retombe sur la même mise en page
    proportionnelle, non regardée.
- **Alternatives écartées** :
  - *Appeler `decider()` directement depuis `main/`* : la clé du lieu, la remise à zéro de l'état, le gel sous le doigt et le calcul
    du soleil seraient dans un fichier qui ne se teste pas sur le PC ; ils sont maintenant testés et mutés.
  - *Geler seulement le profil, comme `decider()`* : l'alerte et la carte pluie sont des **ajouts** d'écran et n'étaient gelés par
    rien ; la consigne de la mission est de ne pas redessiner sous le doigt, donc le gel porte sur toute la présentation.
  - *Un fondu ou une animation à l'apparition de la carte* : refusé par la préférence du produit (transitions instantanées).
  - *Deux pastilles (une pour l'alerte, une pour la pluie)* : la consigne décrit une seule pastille ; l'alerte d'abord (ADR-0015).
  - *Cacher la pluie quand l'alerte est agrandie* : la pluie proche reste une information utile ; la tuile la garde.
  - *Un nouvel objet LVGL pour la carte pluie* : la zone du bas dessine déjà ses cinq jours dans son objet ; la carte pluie
    prend la même zone et le même objet. Zéro objet de plus (`ui_principal_cout()` inchangé).
- **À valider par Axel** (choix faits par l'auteur, pas par la consigne) :
  1. **alerte + pluie** : l'alerte gagne et la pluie passe dans la tuile (la consigne laissait ce cas à l'auteur) ;
  2. la pastille est **teintée** (rôle du thème) : alerte = couleur du niveau, pluie = couleur de la pluie, épinglé = accent ;
  3. le **gel de 10 s** sous le doigt ; au-delà, l'écran se met à jour même doigt posé ;
  4. la carte pluie compte la pluie **en cours** (« Pluie en cours ») ; la durée n'est dite que si la pluie finit dans les
     données ; les barres sont en **mm/h** sur une échelle plancher de 3 mm/h ;
  5. les phrases « Pluie faible / modérée / forte » et « puis plus forte vers 11 h » de la carte pluie (les classes, elles, sont
     celles des barres de l'accueil) ;
  6. la bande des cinq jours quand l'alerte est agrandie (nom, icône, maximum, et minimum seulement si la colonne mesure au moins
     200 px de référence, donc pas en portrait) : on y perd le ruban des températures, le vent et la capsule de pluie ;
  7. la tuile dit **« Données de 07:52 · périmées »** (sans pluie à dire) quand la météo est périmée ;
  8. les **traductions IA** de de, nl, es, it, tr ; le choix de dire « Auto · alerte orange » plutôt que « Alerte orange » seul ;
  9. **l'indice d'appui long** (« une seule fois ») : je n'ai trouvé **aucun** indice dans le dépôt (rien dans `main/`, rien dans la
     table des textes) ; je ne l'ai pas construit, comme demandé.
- **À prouver (non vérifié)** :
  - **tout ce qui se joue sur la carte** : l'effet de la pastille à un mètre, le toucher pendant un gel, la carte pluie sur une
    vraie pluie, le bandeau agrandi sur une vraie alerte orange (le bandeau officiel est lu sur le réseau) ;
  - la **mémoire et le temps de dessin** sur la carte : la compilation donne la taille du binaire et la RAM statique, pas le temps
    d'une image ; le coût d'une passe de `Pilote::avancer()` (quelques dizaines de comparaisons, un calcul du soleil par jour) n'est
    pas mesuré sur le P4 ;
  - les **cinq traductions** (de, nl, es, it, tr) par un lecteur de la langue ;
  - que le lever et le coucher du soleil calculés sont ceux que la personne voit chez elle (comparaison à une source, non faite).
- **Conséquences** :
  - La PR suivante (B) ajoute le réglage « Profil » (Auto / un profil épinglé) et « Profil ▸ » dans la roue ; le champ
    `Mesures::epingle` / `profil_epingle` du pilote est déjà là, à `false` dans cette PR.
  - Les lots 4 à 6 rendront Surf, Vent·Bateau et Côte disponibles (`PROFILS_DISPONIBLES`) et brancheront les entrées aujourd'hui
    « inconnues » ; le pilote n'a alors qu'à les lire.
  - Changer un seuil ou une phrase = changer l'en-tête, la table, les tests et `mutations_accueil_pilote.py` /
    `mutations_accueil_cartes.py` ensemble ; un test échoue si l'un diverge.
  - Ligne à l'index `docs/decisions/README.md` : `0023 — Accueil : le mode automatique branché à l'écran`.
  - À garder en tête : **l'écran montre des règles jamais jugées sur une carte ; aucun seuil ne vient d'une source officielle ;
    rien n'a été flashé.**
