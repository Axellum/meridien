# Alertes officielles de Vigie — coût d'une requête, rythme, « en vigueur », ce qui reste à voir sur la carte

> **Summary (EN)** — Official alerts from Axel's own service Vigie, written on 2026-11-07 and **checked on the PC only**.
> The device asks `GET <Vigie>/v1/vigilances?lat=…&lon=…&lang=<screen language>,en` once per period (15 min by default, 10 min
> at the very least) and keeps the answer in PSRAM. Ten REAL answers (public instance, 2026-11-07 08:52–08:53 UTC) weigh
> 426 to 19 238 bytes of JSON and 365 to 1 976 bytes on the wire; without the `lang` filter the heaviest one is 26 190
> bytes. The compact `/v1/embedded/frame` (2 554 bytes, 911 gzipped) only carries the worst level and counts: rejected. 96
> requests a day at the default period. **Not seen on a board**: the drawing, a real alert, the TLS cost on the Tab5, the 7B.
> Decisions that are the owner's to validate are listed in section 6.

Branche `claude/alertes-vigie-9b13fe` (tirée de `main` au commit `9c89ad5`). Carte visée : M5Stack Tab5 (écran ST7121) ; la Waveshare 7B n'est que compilée. Règle de lecture : tout ce qui n'est pas écrit « constaté » est une décision ou une lecture de source ; ce qui n'a pas été observé sur la carte est écrit **non vérifié**. Décision : [ADR-0009](../decisions/0009-alertes-officielles-par-vigie.md).

## 1. Format de la requête : mesure

Deux routes de Vigie auraient pu servir. Mesurées le 07/11/2026 sur l'instance publique (`https://vigie.auxfilsdesidees.fr`, lecture seule) :

| Route | Taille | Ce qu'elle donne | Verdict |
| --- | --- | --- | --- |
| `GET /v1/embedded/frame` (trame compacte pour les écrans de Vigie : `HEAD`, `ZONE`, `RAIN5M`, `DAY`, `HAZ`, `END`, CRC32) | 2 554 octets, 911 en gzip | le pire niveau et des comptes, **pas** les avertissements un par un | écartée : pas de titre, de période ni d'émetteur par avertissement |
| `GET /v1/vigilances?lat=…&lon=…` sans `lang` | jusqu'à 26 190 octets (La Palma, 9 avertissements) | tout, en huit langues pour les services allemand et espagnol | écartée : lourde, et la plupart des mots ne seraient jamais lus |
| `GET /v1/vigilances?lat=…&lon=…&lang=<écran>,en` | 426 à 19 238 octets de JSON ; **365 à 1 976 octets sur le fil** | tout ce que la page montre, en deux langues au plus | **retenue** |

Les dix réponses réelles (`test/host/donnees/vigie_*.json`, provenance et en-têtes dans `vigie_captures.json`) :

| Lieu | Avertissements | JSON (octets) | Sur le fil (octets) | Compressé ? |
| --- | ---: | ---: | ---: | --- |
| La Palma (ES) | 9 | 19 238 | 1 883 | gzip, chunked |
| Oslo (NO) | 1 | 4 516 | 1 976 | gzip, chunked |
| Londres (GB) | 1 | 3 656 | 1 699 | gzip, chunked |
| Bodø (NO) | 1 | 3 185 | 1 476 | gzip, chunked |
| Eberswalde (DE) | 1 | 2 797 | 1 212 | gzip, chunked |
| Valence (FR) | 0 | 1 331 | 732 | gzip, chunked |
| Funchal (PT) | 0 | 1 024 | 632 | gzip, chunked |
| Ancône (IT) | 0 | 1 019 | 622 | gzip, chunked |
| Tokyo (JP) | 0 | 713 | 365 | gzip, chunked |
| Hambourg (DE) | 0 | 426 | 426 | **non** : sous ≈ 500 octets le serveur ne compresse pas |

Sans le filtre `lang` (captées à la main le même jour vers 08:41 UTC, conservées telles quelles) : Eberswalde 7 623 octets au lieu de 2 797 ; La Palma 26 190 au lieu de 19 238. Toutes les réponses : `Cache-Control: public, max-age=60, s-maxage=120`.

**Conséquences pour l'appareil** : un tampon de réception de 64 Ko et un tampon de texte de 64 Ko, tous deux en PSRAM, rendus après chaque essai (la plus grosse réponse mesurée tient en 1 976 octets sur le fil et 19 238 décompressée) ; `Accept-Encoding: gzip` ; l'en-tête gzip est lu par le noyau (`gzip_flux.hpp`) et les octets sont gonflés par `tinfl_decompress` de la ROM (aucune bibliothèque de plus). Une réponse qui ne tient pas dans les tampons est refusée et comptée comme un échec, jamais coupée.

## 2. Coût d'une requête et choix de la période

| Quantité | Valeur | Source |
| --- | --- | --- |
| Adresse de la requête (Lyon, langue `fr`) | ≈ 70 octets (`/v1/vigilances?lat=45.764&lon=4.836&lang=fr,en`) | `vigie_requete.hpp` ; les 10 adresses d'essai sont dans `attendu_vigie_urls.inc` |
| Données reçues par requête | 365 à 1 976 octets sur le fil (médiane des dix : 972) | tableau de la section 1 |
| Requêtes par jour à 15 min (défaut) | 96 | calcul |
| Données reçues par jour à 15 min | de l'ordre de 35 Ko à 190 Ko | calcul : 96 × 365 à 96 × 1 976 |
| Poignée de main TLS | ≈ 22,5 Ko de RAM interne avec les tampons mbedTLS en PSRAM, ≈ 51 Ko avec mbedTLS en interne ; la consigne de la coordination compte ≈ 65 Ko (un seul à la fois : `verrou_tls`) | `docs/mesures/memoire-interne.md`, pas de ligne « Vu sur » reprise ici ; **non remesurée avec Vigie** |
| Cache de Vigie | 60 s (`max-age`), 120 s côté relais | en-têtes des dix réponses |

**Période : 15 minutes par défaut, 10 au minimum, 180 au maximum.** Raisons : une alerte officielle change à l'échelle de l'heure, pas de la minute ; la réponse est gardée 60 s par le cache de Vigie, donc interroger plus vite que ça ne sert à rien ; l'instance publique est limitée par adresse IP et « pas faite pour bâtir un service dessus ». 15 min est le rythme de la météo ; les deux se partagent le verrou TLS (une poignée de main à la fois), et le premier essai attend 60 à 120 s après que le réseau et l'heure sont prêts (la météo part la première). **Non vérifié sur la carte** : que ce rythme ne fasse pas descendre la RAM interne libre sous la marge de la météo ; la mesure à faire est dans la section 5.

Rythme de réessai (noyau pur, testé) : après un échec 1, 2, 4 min… plafonnés à la période ; après un HTTP 429 ou 503 jamais moins de 30 min ; jusqu'à un dixième de décalage aléatoire ; au moins 60 s entre deux requêtes quoi qu'il arrive (changement de langue ou de réglage compris).

## 3. Ce qui est « en vigueur »

Règles de [ADR-0009](../decisions/0009-alertes-officielles-par-vigie.md), points 3 et 4, appliquées par `vigie_vue.hpp` :

- réponse de moins de 6 h ; sinon l'écran se tait (on ne sait plus si l'avertissement tient) ;
- ni annulée, ni « verte » ; le niveau atteint le minimum du réglage ; un niveau inconnu seulement pour « jaune » ;
- début passé (`onset`, à défaut `effective` ; inconnu = commencée) ; fin non passée (`ends`, à défaut `expires` ; **exclue**, sauf à `hh:59:59` qui reste la dernière seconde de l'alerte ; inconnue = en vigueur tant que la réponse est fraîche) ;
- tri : gravité décroissante, puis la plus ancienne.

Constaté sur les réponses réelles : des avertissements qui se suivent à la seconde près (une fin à `hh:59:59` chez AEMET, un début à `hh:00:00` chez DWD). Avec une fin strictement exclue, **l'écran se taisait une seconde** à ce changement d'heure (instant 1794085199 de La Palma : plus aucun avertissement en vigueur, alors que les trois orangés commencent à ...200). **Corrigé le 07/11/2026** par `fin_exclue_s` (`vigie_vue.hpp`) : une fin écrite à `hh:59:59` compte comme la dernière seconde de l'alerte, toute autre fin reste exclue (`hh:00:00` du DWD, `hh:39:59`…). À 1794085199 le jaune « vent » est donc encore montré, à ...200 les orangés prennent le relais ; la seconde lecture en Python applique la même règle et quatre instants de contrôle de La Palma ont changé en conséquence (les autres lieux : aucun changement). Une hystérésis (garder la pastille 2 s de plus) n'a pas été choisie : elle aurait gardé toute alerte deux secondes après sa fin. Limite : si un émetteur écrivait un `hh:59:59` voulu comme fin exclue, son alerte resterait une seconde de trop ; aucun cas vu dans les dix réponses. **Non vu sur la carte** (PC : tests du noyau, ADR-0009). Chaque lieu du tableau de la section 1 donne un cas que les tests du noyau rejouent à plusieurs instants (`attendu_vigie_*.inc`, calculés à part par un second lecteur écrit en Python seul).

## 4. Ce qui est constaté, et où

**Vu sur** : PC d'Axel (Windows 11), clang des outils hôte (dossier trouvé par `tools\trouver_outils.ps1`), ASan + UBSan ; branche `claude/alertes-vigie-9b13fe` ; réponses de l'instance publique de Vigie du 07/11/2026 08:52–08:53 UTC. **Aucune carte.**

| Contrôle | Résultat | Commande |
| --- | --- | --- |
| Tests du noyau des alertes (lecture, vue, textes, requête, planning, thèmes, pastille, réglages) | tout passe, mode « complet (compilé + exécuté) » | `python test/host/lancer.py --seulement reglages_alertes,vigie,visuel_themes` |
| Mutations du noyau des alertes | 240 sur 240 détectées, 0 problème | `python test/host/lancer.py --mutations --seulement reglages_alertes,vigie,visuel_themes` |
| Suite complète du noyau | 963 tests exécutés avec ASan + UBSan, tout passe (mode « complet »), puis par le crochet de chaque commit | `python test/host/lancer.py` |
| Mutations des réglages (hôte de l'appareil, API, registre) | 47 + 23 + 48 = 118 sur 118 détectées, 0 problème | `python test/host/lancer.py --mutations --seulement test_hote_appareil` (idem `test_api_reglages`, `test_registre_reglages`) |
| Page du téléphone (Chromium 390×844 contre le faux appareil) | 129 vérifications sur 129 (dont 10 pour les alertes) ; poids gzip de la page avec le français 18 223 octets pour un budget de 30 720 | `python web/tests/parcours.py` |
| Rendus PC | scène `alertes` (9 variantes : pastille et page pour Oslo, La Palma et Eberswalde, page vide, pastille dans tous ses états, écran sans alerte) et deux scènes de réglages (`lieu-alertes`, `alertes-adresse-saisie`), en français et allemand (réglages) et en français, anglais, allemand et turc (alertes), 720×1280 | `.\tools\rendre.ps1 -Scene alertes` et `-Scene reglages -Variante lieu-alertes,alertes-adresse-saisie` |
| Seconde lecture indépendante | les `.inc` attendus sont à jour | `python test/host/controle_python_vigie.py --verifier` |
| Police | les 790 caractères de la table du noyau sont tous dans `roboto_700.ttf` | `controle_python_vigie.py` |

### Compilation (constatée, rien n'est flashé)

**Vu sur** : PC d'Axel, ESP-IDF 5.5.5 (installation d'ESPHome, non modifiée), `CMAKE_BUILD_PARALLEL_LEVEL=4`, commit `4081efb`, 07/11/2026 12:25–12:27, verrou `build` tenu puis rendu. Aucune carte.

| Carte | Fichier | Octets | SHA-256 | Avertissements |
| --- | --- | ---: | --- | ---: |
| Tab5 (`tab5`) | `build/meridien.bin` | 2 885 536 | `624210351B878636FBA8450CCDD07DBFC7CDAA9D2FA296229CB7B1B9D376EA39` | 0 |
| Waveshare 7B | `build/7b/meridien.bin` | 2 866 224 | `AD46ECC2D631D76C629F0EC74EE05C191B8C9BB660F64B429071985F4DC64F26` | 0 |

Copies, avec les `.elf` (pour décoder un plantage), dans le dossier de notes partagé `alertes-vigie-binaires\`. L'emplacement d'application des deux cartes fait 4 Mo : le Tab5 en prend ≈ 69 % (≈ 31 % libres). Par rapport à `main` au commit `9c89ad5` (Tab5 2 834 336 octets, 7B 2 814 896 octets, d'après la note de passation de l'étape 1, non remesurés ici), le firmware des alertes pèse ≈ **51 Ko de plus** sur chaque carte (+1,8 %). Cette compilation prouve que le code compile et se lie ; elle ne prouve **rien** sur le comportement : la 7B n'a été que compilée (aucun essai physique), et le Tab5 n'a pas été flashé non plus.

**Ce que les rendus PC ont fait corriger.** La fin d'une alerte (« dimanche 8 novembre · 00:59 ») passait à la ligne après les deux-points (« 00: » puis « 59 ») parce que le moteur de texte de l'écran traite « : » comme une espace ; l'heure passe donc en premier (« 00:59 · dimanche 8 novembre »), voir [depannage.md](../depannage.md). Les rendus ne sont **pas** une photo de la carte : couleurs, lisibilité à 2 m et appui sont à juger sur la dalle (section 5).

## 5. À faire sur la carte (non vérifié)

1. Voir la pastille et la page sur la dalle (Tab5 ST7121), à 2 m : couleurs des quatre niveaux dans les quatre thèmes, lisibilité, appui sur la pastille, retour par « Fermer ». Les rendus du PC ne sont pas une photo de la carte.
2. Recevoir une vraie alerte : choisir une ville qui en a une (La Palma ou Oslo à la date des captures ; les alertes changent d'une heure à l'autre) et lire, sur le journal série, `alertes : …` à chaque réponse.
3. Mesurer la RAM interne libre avant et après la poignée de main TLS de Vigie, et la plus basse marge de la pile de la tâche (12 288 octets).
4. Couper le Wi-Fi : la pastille doit disparaître quand la réponse a plus de 6 h, pas avant ; la remettre : la requête repart dans la minute, pas plus tôt.
5. Régler le niveau minimal, la période et l'adresse depuis l'écran **et** depuis la page du téléphone, vérifier qu'ils se voient des deux côtés et que la requête change (journal série).
6. La 7B : compilée seulement (section 7). **Rien n'est dit de son comportement.**

## 6. Décisions à valider par Axel

- La pastille remplace la pastille météo dérivée de la barre du haut quand une alerte est en vigueur ; elle ne dit que la plus grave, avec « +N » pour les autres.
- Pas d'alerte « à venir » : seulement ce qui est en vigueur.
- 15 min par défaut (96 requêtes par jour et par adresse IP).
- Les mots de l'émetteur ne sont jamais réécrits ; un texte avec un caractère que la police ne dessine pas est écarté en entier.
- Rouge pastel (`0xFF9A90`) pour le niveau rouge du thème « ciel » (un rouge franc tombait à 2,29:1).
- Les traductions de l'allemand, du néerlandais, de l'espagnol, de l'italien et du turc sont écrites par l'agent, non relues.
- L'horloge de l'appareil est codée sur Europe/Paris : les heures écrites sur la page seraient celles de Paris pour une ville d'un autre fuseau.

## 7. Notes pour Vigie

Rien ne manque à Vigie pour l'écran : le titre, le texte, la période, le niveau, l'émetteur, la couverture et l'attribution sont dans la réponse. Un seul point, qui n'est pas bloquant : sous ≈ 500 octets la réponse n'est pas compressée (Hambourg : 426 octets en clair), ce que le client accepte. Rien n'a été demandé ni déployé côté serveur (un déploiement du VPS public demande l'accord d'Axel).
