# MET Norway et NWS lus directement : ce que disent les pages, ce que rendent les services — PC seulement, aucune carte

> **Summary (EN)** — The device can read two key-less weather services itself: **MET Norway** (Locationforecast 2.0 "complete",
> Norway, Finland, Iceland, Svalbard) and the **NWS** (api.weather.gov, United States and Puerto Rico). This page records what the
> services' own pages say (what was read and what was **not**), and what their **real answers** of 2026-11-07 contain (8 MET places,
> 6 NWS places and one place outside the NWS area), as measured on the PC.
> - **Read through a page summariser, not word for word**: User-Agent rules, rate limits, `Expires`, licences, credit. The quotation
>   marks in the summariser's report are not guaranteed; re-read the primary pages before publication.
> - **Measured (PC only)**: MET Norway answers are 60 818 to 90 783 bytes once unzipped (4 549 to 7 515 on the wire), the NWS grid
>   120 347 to 240 402 bytes: **neither fits the 64 KiB default buffer** of the common network client, so each request carries its own
>   bound. MET's `Expires` falls 310 to 1 828 s after the request; the NWS grid is valid one hour at most, its address (`/points`) 2 to
>   24 hours. MET Norway answers HTTP 304 to `If-Modified-Since` (not implemented, see ADR-0014). The NWS answers 404 outside its area.
> - **Neither service gives the UV index** (MET only gives a clear-sky one), **sunrise or sunset** (computed on the device with the
>   same calculation as for Open-Meteo, ADR-0012: within 60 s of MET's own "Sunrise 3.0" on 69 days at every latitude) — and the NWS gives **no pressure** and, for most days,
>   **no daily rain total**. Unknown stays unknown, never 0.
> - **Not verified**: anything on a board; countries other than the places listed; the 2026-11-07 headers are one sample per place.

**Vu sur** : PC seulement (Windows 11, clang++ 23.1.5 avec ASan/UBSan, `_outils\env_hote.ps1`), aucune carte ; réponses réelles captées le 07/11/2026 de 13 h 56 à 13 h 57 (heure de Paris) depuis le PC d'Axel par `test/host/capturer_fournisseurs_meteo.py` (adresses et `User-Agent` du firmware) et `capturer_soleil_metno.py` ; pages officielles lues le 07/11/2026 ; branche `claude/meteo-fournisseurs-e3cd8b`, base `4b36c71`. Compilations du 07/11/2026 : Tab5 (carte `tab5`, toutes révisions d'écran) 2 894 496 octets et Waveshare 7B 2 875 088 octets, code de sortie 0, aucun avertissement du compilateur (seul l'avertissement CMake connu « Error while generating esp_rom gdbinit », [depannage.md](../depannage.md)) ; **ni flashé ni lancé sur une carte**.

Règle de lecture : « lu » = la page a été ouverte le 07/11/2026 **par un outil qui en rend un résumé** (les guillemets de son compte rendu ne sont pas garantis mot pour mot) ; « non lu » = la page ne le dit pas, ou n'a pas pu être ouverte ; « mesuré » = vu dans une vraie réponse. Ce qui n'est pas marqué est une lecture de code.

## 1. Question

Quelles règles d'usage imposent MET Norway et le NWS à un appareil qui les interroge lui-même, que contiennent vraiment leurs réponses, et qu'est-ce qui manque pour remplir l'écran de météo sans rien inventer ?

## 2. Ce que disent les pages officielles

### MET Norway

| Fait | Page lue | État |
|---|---|---|
| Un `User-Agent` qui nomme l'application (ou le domaine) et donne un contact (e-mail ou site) est obligatoire ; sinon le blocage peut arriver sans préavis | [conditions d'utilisation](https://api.met.no/doc/TermsOfService) | lu |
| Sans `User-Agent`, ou avec un nom de bibliothèque générique (okhttp, Dalvik, fhttp, Java cités), la réponse est 403 | [documentation de Locationforecast](https://api.met.no/weatherapi/locationforecast/2.0/documentation) | lu |
| Moins de 20 requêtes par seconde au total ; au-delà, accord spécial ; le dépassement donne HTTP 429 | conditions d'utilisation | lu |
| Ne jamais répéter une requête avant l'heure de l'en-tête `Expires` | conditions d'utilisation | lu |
| Envoyer `If-Modified-Since` avec la valeur du `Last-Modified` précédent (pas une date arbitraire) ; pas de HEAD suivi d'un GET | conditions d'utilisation | lu ; la réponse 304 n'y est pas nommée (**mesurée** en direct, §3) |
| Le paramètre `nextrun` de la version 1 n'existe plus : les en-têtes HTTP le remplacent | [FAQ](https://api.met.no/doc/locationforecast/FAQ) | lu |
| Coordonnées tronquées à 4 décimales au plus (plus de décimales : 403 pour les nouveaux produits) | conditions d'utilisation et FAQ | lu ; la documentation de l'API ne fixe pas de limite et donne des exemples à 2 et à 6 décimales |
| Ne jamais planifier à l'heure ronde (XX:00) ; ajouter un décalage aléatoire de quelques minutes ; éviter aussi les fins d'exécution de modèle | FAQ et conditions d'utilisation | lu |
| Licences NLOD 2.0 et CC BY 4.0, sauf mention contraire ; créditer MET Norway (« Based on data from MET Norway » suggéré), lien apprécié | [licence](https://api.met.no/doc/License) et [crédit](https://www.met.no/en/free-meteorological-data/Licensing-and-crediting) | lu |
| CC BY 4.0 résumé par les conditions : mention de la source, lien vers la licence, **indication des modifications** | conditions d'utilisation | lu (résumé) ; **les pages de licence ne reprennent pas la mention des modifications : non lu** |
| Textes intégraux de CC BY 4.0 et de NLOD 2.0 | creativecommons.org, data.norge.no | **non lu** (liens seulement) |
| « complete » = toutes les valeurs ; percentiles 10/90 de la température et du vent, température apparente ajoutés (2021, 2026) | documentation de Locationforecast | lu |
| Indice UV : la documentation parle de prévision UV « par lieu » sans nommer le champ | documentation de Locationforecast | lu, **imprécis** ; la réponse réelle ne donne que `ultraviolet_index_clear_sky` (§4) |
| Rafales et probabilité de précipitations | documentation de Locationforecast | **non lu** (absentes de la page) ; **mesuré** : présentes seulement en zone nordique (§4) |
| Valeurs instantanées (température, nuages, pression, vent) et valeurs de période `next_1/6/12_hours` (précipitations, symboles) | FAQ | lu |
| Mise à jour toutes les heures en zone nordique, toutes les 6 h ailleurs ; ne pas répéter une requête identique avant 30 min (zone nordique) ou 3 h (ailleurs) | documentation de Locationforecast | lu. **Contradiction à trancher** : les `Expires` mesurés valent 30 minutes après le `Last-Modified` aussi à Tokyo, Sydney et Londres (§3), et Reykjavik, qui est dans notre liste, n'est peut-être pas « zone nordique » au sens de la page (**non lu**). Le plancher de l'appareil est de 30 minutes. La FAQ ne cite ni 30 min ni 3 h : elle dit d'attendre au moins la moitié de l'intervalle de mise à jour |

### NWS

| Fait | Page lue | État |
|---|---|---|
| Un `User-Agent` est requis ; son contenu est libre mais plus il est unique mieux c'est ; un site ou un e-mail de contact est recommandé ; une clé d'API le remplacera plus tard | [documentation de l'API](https://www.weather.gov/documentation/services-web-api) | lu |
| Sans `User-Agent`, un 403 est possible | [FAQ générale](https://weather-gov.github.io/api/general-faqs) | lu |
| Débit : « raisonnable », limite non publiée ; au-delà, une erreur avec nouvel essai possible, en général sous 5 s | documentation de l'API | lu ; **aucun chiffre : non lu** |
| Les réponses portent `Cache-Control` et `Last-Modified` | FAQ générale | lu ; **`Expires` n'est cité par aucune des deux pages** (présent dans toutes les réponses réelles, §3) |
| La correspondance `/points` change rarement et peut être gardée ; le bureau et la maille peuvent changer : revérifier de temps en temps | documentation de l'API et FAQ | lu ; durée exacte des `Cache-Control` de `/points` et `/gridpoints` : **non lu** (mesuré, §3) |
| Un paramètre d'adresse inconnu donne 400 ; plus de 4 décimales donne une erreur qui propose les coordonnées valides les plus proches | FAQ générale | lu |
| Zone couverte : États-Unis et territoires | documentation de l'API et FAQ | **non lu** (seule allusion : « la plupart des États-Unis » pour la précision des coordonnées) ; la réponse de `/points` hors couverture n'y est pas décrite (**mesurée** : 404, §3) |
| Pages web du NWS dans le domaine public « sauf mention contraire » | [mise en garde](https://www.weather.gov/disclaimer) | lu |
| Données de l'API « ouvertes, libres d'usage » ; **aucune licence nommée pour l'API** | documentation de l'API | lu (formulation du résumeur, à relire mot pour mot) |
| Aucune mention exigée pour un usage général ; interdit de suggérer une approbation ou une affiliation (nom et identifiant visuel protégés) ; un tiers dont l'œuvre reprend surtout du contenu du NWS doit le signaler | mise en garde | lu |
| `/gridpoints` donne les données numériques brutes de la grille de 2,5 km, en séries chronologiques dont chaque valeur est liée à un intervalle ; unité par série (`uom`) ; données brutes sur 7 jours dans `forecastGridData` | [page gridpoints](https://weather-gov.github.io/api/gridpoints) et documentation de l'API | lu ; les unités °C, mm et % ne sont pas nommées par la page (**vérifiées** sur les réponses réelles) |

### Pages non lues (échec signalé par la mission, non rouvertes ici)

`developer.yr.no/doc/FAQ/`, `api.met.no/weatherapi/weathericon/2.0/documentation` et `/legends`, `weather.gov/about/credits`, `weather.gov/news/usage`, `api.met.no/doc/uvforecast` : HTTP 404 à la date de la mission. Les 41 symboles de MET Norway viennent donc du dépôt officiel `metno/weathericons` (relevé le 07/11/2026), pas de ces pages.

## 3. Mesuré : les réponses réelles et leurs en-têtes (PC, 07/11/2026)

Une mesure par lieu, un seul jour. `Expires` − `Date` = secondes entre la réponse du serveur et le moment où elle dit qu'on peut redemander.

### MET Norway (`/weatherapi/locationforecast/2.0/complete`, `Accept-Encoding: gzip`)

| Lieu | Octets (fil → texte) | Âge de la réponse | `Expires` − `Date` | `Expires` − `Last-Modified` |
|---|---|---|---|---|
| Oslo | 5 987 → 90 722 | 0 s | 1 816 s | 1 816 s |
| Helsinki | 5 993 → 90 285 | 0 s | 1 827 s | 1 827 s |
| Longyearbyen | 5 419 → 74 426 | 0 s | 1 828 s | 1 828 s |
| Londres | 4 605 → 61 206 | 0 s | 1 818 s | 1 818 s |
| Tokyo | 4 871 → 61 081 | 0 s | 1 813 s | 1 813 s |
| Sydney | 5 011 → 62 042 | 317 s | 1 521 s | 1 839 s |
| Reykjavik | 4 549 → 60 818 | 1 093 s | 747 s | 1 841 s |
| Tromsø | 7 515 → 90 783 | 1 521 s | 310 s | 1 831 s |

- **`Expires` ≈ `Last-Modified` + 30 minutes** (1 813 à 1 841 s), pas « 30 minutes après la requête » : quand MET Norway sert une réponse déjà ancienne de son cache (âge jusqu'à 1 521 s), `Expires` tombe 310 s après la requête seulement (Tromsø). Le plancher de l'appareil (30 minutes) est plus strict que `Expires` dans trois cas sur huit (310, 747 et 1 521 s) ; dans les cinq autres, le délai suit `Expires` (`delai_apres_succes_s`, [ADR-0014](../decisions/0014-met-norway-et-nws-comme-sources.md)).
- **Pas de `Cache-Control`** dans les réponses de MET Norway ; `Content-Encoding: gzip`, `Content-Type: application/json`.
- **Requête conditionnelle** : la même requête d'Oslo avec `If-Modified-Since` égal au `Last-Modified` reçu a donné **HTTP 304** sans corps (0 octet), 2 s plus tard, avec le même `Expires`. Gain possible : 4,5 à 7,5 Ko par requête sur le fil, pour une requête toutes les 30 minutes. **Non retenu** : voir l'ADR (un `Last-Modified` à garder en mémoire, un cas 304 de plus à prouver sur carte, pour 6 Ko).
- **Taille** : jusqu'à 90 783 octets une fois décompressée (Tromsø), donc **le « 40 712 octets » de la mission est faux** (corrigé dans [ADR-0010](../decisions/0010-source-meteo-par-pays-et-client-reseau-commun.md) et [client-reseau.md](client-reseau.md)). Le tampon de 64 Ko du client réseau commun ne suffit pas : la requête de MET Norway demande 160 Ko (`TAILLE_REPONSE_METNO_MAX`, 1,8 fois la plus grande réponse mesurée).

### NWS (`/points/{lat},{lon}` puis `/gridpoints/{bureau}/{x},{y}`, `Accept-Encoding: gzip`)

| Appel | HTTP | Octets (fil → texte) | `Cache-Control` | `Expires` − `Date` |
|---|---|---|---|---|
| New York, `/points` | 200 | 1 010 → 3 913 | `public, max-age=48716, s-maxage=86400` | 48 716 s |
| Miami, `/points` | 200 | 1 006 → 3 920 | `public, max-age=60092, s-maxage=86400` | 60 092 s |
| Seattle, `/points` | 200 | 1 014 → 3 929 | `public, max-age=7380, s-maxage=86400` | 7 380 s |
| Anchorage, `/points` | 200 | 1 009 → 3 934 | `public, max-age=80019, s-maxage=86400` | 80 019 s |
| Honolulu, `/points` | 200 | 1 008 → 3 937 | `public, max-age=49090, s-maxage=86400` | 49 090 s |
| San Juan, `/points` | 200 | 1 014 → 3 930 | `public, max-age=86400, s-maxage=86400` | 86 400 s |
| New York, `/gridpoints` | 200 | 9 457 → 197 787 | `public, max-age=1521, s-maxage=3600` | 1 521 s |
| Miami, `/gridpoints` | 200 | 9 154 → 192 842 | `public, max-age=108, s-maxage=3600` | 108 s |
| Seattle, `/gridpoints` | 200 | 8 749 → 182 525 | `public, max-age=3600, s-maxage=3600` | 3 600 s |
| Anchorage, `/gridpoints` | 200 | 6 216 → 136 403 | `public, max-age=3600, s-maxage=3600` | 3 600 s |
| Honolulu, `/gridpoints` | 200 | 5 714 → 120 347 | `public, max-age=3600, s-maxage=3600` | 3 600 s |
| San Juan, `/gridpoints` | 200 | 11 209 → 240 402 | `public, max-age=3600, s-maxage=3600` | 3 600 s |
| **Lyon (hors couverture)**, `/points` | **404** | 301 → 301 | `private, max-age=86400` | 86 400 s |

- La grille se renouvelle **au plus toutes les heures** (`Expires` − `Date` de 108 s à 3 600 s) ; l'adresse de la grille (`/points`) vaut de 2 h à 24 h. L'appareil garde l'adresse **24 heures** au plus, la grille au moins **60 minutes** entre deux requêtes réussies (plancher), au plus 90 minutes.
- **Hors couverture** : Lyon donne HTTP 404 `application/problem+json` (301 octets, non compressé). Les Samoa américaines donnent HTTP 200 avec `gridId` nul (constaté en direct le 07/11/2026, pas de fixture) : le lecteur refuse proprement.
- **Guam, îles Vierges américaines, Mariannes du Nord** : le NWS y répond (constaté en direct), mais **sans fixture** ; l'appareil ne les propose donc pas encore (la liste de pays du NWS est `US PR`).
- **Taille** : la grille fait jusqu'à 240 402 octets décompressée (San Juan) et 11 209 octets sur le fil ; la requête demande 384 Ko (`TAILLE_REPONSE_NWS_GRILLE_MAX`, 1,6 fois la plus grande mesurée) ; `/points` demande 64 Ko pour 3 937 octets.
- Deux requêtes pour une météo (`/points`, puis `/gridpoints`) la première fois et après 24 h ; une seule le reste du temps.

## 4. Mesuré : ce que chaque service donne, et ce qui reste « inconnu »

Une valeur absente ou nulle devient **inconnue** (jamais 0, jamais copiée d'un autre jour) ; l'écran n'affiche alors rien à sa place.

| Grandeur | MET Norway | NWS |
|---|---|---|
| Température, humidité, vent, direction, nuages | à chaque instant | à chaque instant (valeur de l'intervalle qui contient l'instant) |
| Température ressentie | `apparent_air_temperature` si présente, sinon inconnue | `apparentTemperature` |
| Pression | pression au niveau de la mer | **inconnue : la couche `pressure` existe mais est vide** (six mailles) |
| Rafales | **zone nordique seulement** (Oslo, Helsinki, Tromsø, Longyearbyen ; pas Reykjavik, Londres, Tokyo, Sydney) | oui |
| Probabilité de précipitations | **zone nordique seulement** (Oslo, Helsinki, Tromsø ; pas Longyearbyen, Reykjavik…) | oui, mais seulement pour une heure dont l'intervalle dure 6 h au plus ; au-delà, inconnue |
| **Indice UV** | **inconnu** : MET Norway ne donne que l'UV « ciel dégagé » (`ultraviolet_index_clear_sky`), qui n'est pas l'UV prévu avec les nuages ; l'afficher sous le nom « UV » serait faux | **inconnu** : non fourni |
| Cumul de pluie | par heure ou par bloc de 6 h/12 h ; le jour est connu seulement si toutes ses heures le sont | par intervalles de 2 à 9 h, jamais par heure ; le cumul d'un jour est donc **le plus souvent inconnu** |
| Lever et coucher du soleil | **non fournis, calculés sur l'appareil** (§5) | **non fournis, calculés sur l'appareil** (§5) |
| Pluie par quarts d'heure, durée d'ensoleillement | non fournis | non fournis |
| Minimum et maximum du jour | sur les instants de la réponse : **le premier jour ne couvre que le reste de la journée** (la réponse ne remonte pas avant l'heure en cours) | sur les intervalles de température qui touchent le jour local |
| Symbole → code WMO du projet | 41 symboles officiels, chacun avec ses variantes `_day`, `_night`, `_polartwilight` ; ceux qui n'ont pas d'équivalent exact (« sleet » = neige fondue) sont ramenés à la famille de la neige (**choix à confirmer**) ; un symbole inconnu rend le code inconnu | ciel d'après `skyCover` (seuils 12,5 / 37,5 / 87,5 %, **à vérifier**) puis phénomène de la couche `weather` s'il est plus fort ; « sleet » (les grains de glace du vocabulaire du NWS) → 77, « hail » → 96 (99 si grosse grêle) : **approximations, non vérifiées** |

Le **lecteur de MET Norway** compte 85 à 91 éléments horaires puis par six heures jusqu'à dix jours (relevé du 07/11/2026) ; le lecteur lit 128 éléments au plus. Le **fuseau** du NWS vient de `/points` (nom IANA) ; celui de MET Norway est celui de la ville enregistrée.

## 5. Mesuré : lever et coucher du soleil calculés sur l'appareil

Ni MET Norway ni le NWS ne donnent le lever ni le coucher dans cette réponse ; l'appareil les calcule avec **le même calcul que pour Open-Meteo** (`soleil.hpp`, décrit une seule fois dans [ADR-0012](../decisions/0012-l-heure-affichee-est-celle-du-lieu-choisi.md) : équation du temps, déclinaison, réfraction et demi-disque du soleil, sans bibliothèque mathématique). Trois références qui ne viennent pas du code :

| Référence | Jours comparés | Plus grand écart du calcul, par latitude |
|---|---|---|
| **MET Norway « Sunrise 3.0 »** (79 réponses réelles, 24 lieux, des cercles polaires à l'équateur, changements d'heure compris ; 69 jours normaux, 3 nuits polaires, 7 jours polaires) | situations (normal / nuit polaire / jour polaire) **identiques sur les 79 jours** | ≤ 45° : **60 s** (39 jours) ; 45–60° : **60 s** (16) ; 60–66,5° : **54 s** (7) ; > 66,5° : **60 s** (7) |
| **Open-Meteo** (lever et coucher donnés pour 6 lieux × 7 jours) | 42 jours | Lyon 8 s, Seattle 7 s, Singapour 4 s, Sydney 12 s, Katmandou (+5 h 45) 6 s, Terre-Neuve (−3 h 30) 5 s ; Longyearbyen : 7 nuits polaires des deux côtés |
| **Second calcul** écrit en Python (algorithme « Almanac for Computers » de l'US Naval Observatory) | mêmes 79 jours | ≤ 45° : 119 s ; 45–60° : 55 s ; 60–66,5° : 108 s ; > 66,5° : 1 113 s — il est moins précis que le calcul de l'appareil, il ne sert qu'à vérifier à 5 minutes jusqu'à 60° |

**Tolérance retenue par les tests** : 2 minutes jusqu'à 60° de latitude, 5 minutes au-delà (le pire écart mesuré, à toutes les latitudes, est de 60 s ; MET Norway donne des minutes entières et la réfraction près de l'horizon change de plusieurs minutes avec quelques centièmes de degré). **Elle n'a pas été desserrée** quand le premier calcul (position du soleil estimée une seule fois, à midi solaire) a été remplacé le 07/11/2026 par celui d'ADR-0012 : celui-là donnait 81 s jusqu'à 60° et 257 s au-delà de 66,5° (4,3 minutes sur 5 permises), et la comparaison des deux, jour par jour, est dans ADR-0012. Les chiffres de ce paragraphe sont ceux de `test/host/mesure_ecarts_soleil.cpp` (la ligne de commande est en tête du fichier), lancé le 07/11/2026 sur les mêmes données que les tests (`test_soleil.cpp`). **Vu sur** : PC d'Axel, clang++ 23.1.5, branche `claude/soleil-unique-7e44fa` ; aucune carte.

## 6. Ce qui a été prouvé, ce qui ne l'a pas été

Prouvé sur le PC (détail et nombres exacts : [ADR-0014](../decisions/0014-met-norway-et-nws-comme-sources.md), « Preuves ») : lecture des 8 réponses MET Norway et des 6 réponses NWS (+ le 404 de Lyon) par les lecteurs du firmware, comparée à une lecture écrite en Python avec un autre algorithme et la base de fuseaux de Python ; mutations ciblées sur chaque fichier nouveau ; liste de sources filtrée par pays, API, page de réglages (parcours de navigateur contre un faux serveur) et rendus de l'écran en sept langues ; deux compilations (Tab5, 7B) sans avertissement.

**Non vérifié** :

- **Tout ce qui se passe sur une carte** : les vraies requêtes TLS vers `api.met.no` et `api.weather.gov` depuis l'appareil, la décompression de 240 Ko en PSRAM, **la marge de pile de la tâche météo (12 Ko) pendant l'analyse de la grille du NWS** et **la durée de cette analyse**, la consommation de RAM interne pendant la prise de contact TLS, le repli vers Open-Meteo sur une vraie panne, le redémarrage en cours de requête.
- Les autres lieux : huit lieux pour MET Norway et six pour le NWS, un jour (jours courts de l'hémisphère nord), une mesure par lieu. Pas d'été, pas de tempête, pas de neige forte.
- ~~Le texte exact des pages officielles (lues par résumeur) et les textes intégraux des licences CC BY 4.0 et NLOD 2.0.~~ **Relus mot pour mot le 08/11/2026** (dernière section) ; la FAQ de MET Norway n'a pas été rouverte.
- Les seuils de `skyCover` du NWS et la correspondance « sleet » / « hail » ; l'UV de MET Norway (en zone nordique, une valeur plus utile que l'UV « ciel dégagé » existe peut-être ailleurs dans la documentation : non lu).
- Ce que le NWS ou MET Norway font quand le contact du `User-Agent` est injoignable (le dépôt est public depuis le 08/11/2026, le contact est donc joignable) : aucun essai.

## 7. Relecture mot pour mot du 08/11/2026

**Vu sur** : pages ouvertes dans le navigateur intégré d'une session Claude Code le 08/11/2026 (pas par un résumeur) : conditions d'utilisation de MET Norway (dernière révision le 05/11/2026 d'après la page), licence, page « Licensing and crediting » de met.no, documentation de Locationforecast, textes intégraux de CC BY 4.0 et de NLOD 2.0 ; pour le NWS, la mise en garde, la page de l'API et la FAQ générale. **Non rouverte : la FAQ de MET Norway.** Les tableaux du §2 viennent de la lecture du 07/11/2026 ; rien n'y est contredit, sauf ce qui suit.

| Constat | Suite |
|---|---|
| MET : intervalle entre deux requêtes identiques = 30 min pour la région nordique, 3 h pour le reste du monde ; la page ne définit pas « région nordique » ; l'appareil attend 30 min pour `NO FI IS SJ` | Islande **non tranchée** (la retirer de la liste MET ou la garder) : décision d'Axel |
| MET : `If-Modified-Since` fait partie des règles par défaut | Non fait (ADR-0014, point 9) ; décision d'Axel |
| MET : les conditions se disent un accord contraignant à lire et accepter par un humain responsable ; le développeur répond de la confidentialité des utilisateurs ; navigateurs et applications mobiles : mandataire à cache | README (« Ce qui sort de l'appareil ») |
| CC BY 4.0 : texte de la licence ou lien (§3(a)(1), « de toute façon raisonnable » §3(a)(2)) ; NLOD 2.0 §5 : référence à la licence, lien vers la licence et la source si possible | La page de réglages lie le texte de chaque licence (parcours de navigateur sur le PC, 213 vérifications sur 213) ; l'écran nomme les licences sans lien |
| NWS : la page de l'API dit « données ouvertes, libres pour tout usage » ; « domaine public » est la formule de la mise en garde, pour les pages web | L'écran et la page disent « données ouvertes » |
| NWS : en cas d'indisponibilité, ne pas réessayer plus d'une fois par minute | Respecté : après un échec, le service direct est mis en pause pour son plancher complet (`main/meteo_client.cpp`, `g_natif_pas_avant_s`). Un écart « réessais à 15 s » annoncé le 08/11 avait été **écrit à tort** (lecture de commentaire, pas du code) et a été retiré |

Respecté, vérifié au texte : forme du `User-Agent`, coordonnées tronquées à 4 décimales, `Expires` respecté, pas d'appel à l'heure ronde, gzip et redirections, HTTPS seul, aucun logo ni nom Yr, NRK ou NWS, pause de 10 min après un HTTP 429, mention de la source avec « données adaptées ».
