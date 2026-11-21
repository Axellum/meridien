# ADR-0029 — Accueil : « est-ce normal ? » (une pastille sur la ligne du lieu) et la page « Sources » de la roue

> **English summary.** Two additions, chosen by the author among sketches. (1) **Normals.** Once a day the device asks Vigie
> (`GET /v1/climate/normals?lat=&lon=`, same address as the alerts, same on/off switch) for the Météo-France climate normals of the
> **month** at the place, and compares **today's forecast maximum** to the month's normal maximum on the screen itself: the request
> carries no temperature. The home screen shows one pill on the place line: « +1,0° sur la normale », « −4,2° sous la normale »,
> « Dans la normale » (under 1.0° of difference), « Record de chaleur battu » or « Record de froid battu » (only for a station that
> still measures). No pill outside France and its overseas territories (Vigie answers `outside_coverage`), when the answer is not for the
> local month, older than 48 h, or when a value is missing. The pill is not touchable; a « Is this normal? » page is a possible follow-up.
> (2) **Sources page.** A « Sources » button in the press-and-hold wheel (always there, after « Mer », before « Profil ») opens a page
> that lists what the screen really shows with the licence sentence of each: weather (credited to the service that actually gave it),
> places (GeoNames), Vigie alerts, the normals (Météo-France, Licence Ouverte 2.0, with the sheet's edition date), sea and spots,
> fonts and icons. The sentences live in **one** table (`credits.hpp`), also used by the settings page. Status: **proposed**; seen on PC
> renders and compiled for the 7B, never flashed.

- **Statut** : **proposée**, écrite le 21/11/2026 à la demande d'Axel (« Normales 1, crédits 3 », parmi trois esquisses de chaque).
- **Vu sur** : rendus du banc PC (`tools/rendre.ps1`, scènes `principal` et `sources`), tests du noyau sur le PC (ASan/UBSan et compilateur
  d'ESP-IDF), compilation pour la 7B ; **jamais flashée ni lancée sur une carte**.

## Contexte

Vigie sert, depuis novembre 2026, `GET /v1/climate/normals?lat=&lon=` : pour la station Météo-France la plus proche (50 km au plus,
France et outre-mer seulement), les normales 1991-2020 du **mois** et les records du mois. Hors couverture, `status` vaut
`outside_coverage` et rien n'est estimé. La réponse ne dépend que de la station et du mois (`Cache-Control: max-age=86400`) : environ
2 Ko décompressés, 0,8 Ko sur le fil (relevé du 21/11/2026, Lorient, La Réunion, Londres). Les données sont sous Licence Ouverte 2.0 :
nommer la source et la date d'édition de la fiche.

Par ailleurs, les crédits des sources étaient dispersés : la page de réglages nomme la météo et GeoNames, la page des spots OpenStreetMap,
la page mer ses données ; rien ne les rassemblait ni ne nommait les polices et les icônes.

## Décision

### Normales

1. **Une requête par jour**, sans valeur du jour dedans : l'appareil garde sa prévision et la compare lui-même (`climat_normales.hpp`,
   `climat::juger`). Premier essai 3 à 5 minutes après que le réseau et l'heure sont prêts (après la météo et les alertes), 24 h plus un
   décalage de 30 minutes au plus ensuite ; après un échec 5, 10, 20, 40 min puis 1 h ; après un 429 ou un 503 une heure au moins ; au plus une
   requête toutes les cinq minutes quoi qu'il arrive (changement de lieu compris) ; le jour de la bascule de mois, un nouvel essai dans l'heure
   (`climat_planning.hpp`).
2. **Les mêmes réglages que les alertes** (ADR-0009) : l'adresse de Vigie et l'interrupteur « Alertes officielles ». Désactivées : aucune requête, la
   réponse en mémoire est oubliée. Une cinquième source de la tâche réseau des données (`Source::Climat`, sans anti-rebond).
3. **Ce que le jugement dit**, dans cet ordre : un record de chaleur battu (maximum prévu au-dessus du record du mois) ou de froid battu
   (minimum prévu sous le record), seulement si la station mesure encore ; sinon l'écart du maximum prévu à la normale du maximum du mois :
   moins de 1,0° = « Dans la normale », sinon « +X° sur la normale » ou « −X° sous la normale » (écart au dixième, vrai signe moins).
4. **Rien plutôt qu'une erreur** : pas de pastille si la réponse n'est pas du mois local, a plus de 48 h, si une valeur manque ou sort de
   ±60 °C, hors couverture, alertes désactivées, ou si l'heure n'est pas sûre. Une valeur absente est inconnue, jamais 0.
5. **Sur l'écran** : une pastille sur la ligne du lieu, après la pastille du mode (et « Auto en pause »), avant « Exemple » ; elle prend la plus grande
   de trois polices (28, 24 puis 20 px de référence, la dernière ne servant qu'à elle) qui tient, et **disparaît** si même la plus petite ne tient pas (jamais tronquée par « … » : une phrase de normale coupée dirait
   autre chose). Teinte : un rôle du thème (chaud : avertissement, froid : pluie, normal : texte discret).
6. **Pas d'appui** sur la pastille et pas de page « Est-ce normal ? » dans ce lot : suite possible (un appui qui ouvre les chiffres : normale,
   records, station et sa distance, date d'édition).

### Page « Sources »

1. **Un bouton « Sources »** dans le premier anneau de la roue, **toujours là**, après « Mer » et avant « Profil » : il ne déplace ni « Alertes »
   ni « Mer ». Glyphe MDI `information-outline` ajouté au sous-ensemble de la police de la roue (2 140 → 2 276 octets).
2. **La page dit ce que l'écran montre vraiment** (`credits::composer`) : la météo, créditée au service qui l'a réellement donnée (Open-Meteo,
   MET Norway ou NWS ; pour le NWS, « ni approuvé ni parrainé »), les lieux (GeoNames), Vigie si les alertes sont activées (chaque alerte garde la
   licence de son émetteur, détaillée sur la page Alertes), les normales (Météo-France, Licence Ouverte 2.0, date d'édition) si une réponse du mois
   est en mémoire, la mer et les spots si un spot est choisi, les polices et icônes (Roboto, OFL 1.1 ; Material Design Icons, Apache 2.0).
3. **Une seule source de vérité** : les phrases de licence sont des clés de la table de traduction listées dans `credits.hpp`, lues par la page de
   réglages (météo, lieux), la page des spots et la page « Sources ». Un test prouve que chaque phrase est une clé de la table et que chaque
   traduction garde ses marqueurs. Le crédit de la mer reste celui de `mer_textes.hpp` (sa propre table).

## Conséquences

- Binaire de la 7B et RAM : voir la description de la PR (mesurés avant et après).
- Une requête de plus par jour vers l'instance publique de Vigie (2 Ko), à peu près rien pour le serveur.
- Les noms propres et les licences ne se traduisent pas ; les six autres langues sont écrites par une IA et ne sont relues par personne
  ([i18n/README.md](../../i18n/README.md)).
- **Non vérifié** : le comportement sur la carte (la requête réelle depuis l'appareil, la pastille sur la dalle 1024 × 600, la page au doigt) ;
  l'effet de la bascule de mois sur un appareil qui tourne depuis des jours.
- Ce que l'écran n'a pas : la station et sa distance (une station à 40 km et 500 m plus haut peut différer de plusieurs degrés ; Vigie le
  dit dans son champ `note`, l'appareil ne le montre pas), et les normales à la journée (Vigie ne donne que le mois).
