# ADR-0028 — Waveshare 7B : le code exécuté depuis la PSRAM et un cache L2 de 256 Ko

> **English summary.** On the Waveshare 7B the firmware now runs its code and constant data **from PSRAM**
> (`CONFIG_SPIRAM_XIP_FROM_PSRAM=y`) with a **256 KB L2 cache** (`CONFIG_CACHE_L2_CACHE_256KB=y`, 128 KB before). These are the
> memory settings of the author's ESPHome firmware on the same board (`execute_from_psram: true`), and the ones Espressif's LCD FAQ
> recommends for DSI panels. Why: with the code in flash and a 128 KB L2 cache, the panel showed a **brief white flash, at rest, several
> times an hour**, never seen under ESPHome (the author, 13/11/2026). Ruled out by measurement before the change: a restart, an underrun of
> the DSI bridge, an error of the DSI host (its interrupt status registers read every millisecond), a full-screen repaint at rest, an
> HTTPS request. With both settings the author saw **no flash during the first 30 minutes** of observation; **which of the two settings
> matters, and the mechanism, are not known**. Cost, measured on the board: about **142 KB less free internal RAM** (181 KB instead of
> 323 KB once the time is received; largest DMA-capable block 121 KB instead of 262 KB); weather, alerts and sea still answer HTTP 200.
> The Tab5 is unchanged. Status: **proposed**, to be confirmed by the author over a longer observation.
>
> **Called into question on 25/11/2026**: the author saw the white flash again on the 7B running `main` `4a9a7be`, which has both
> settings. They do not fix it, so the half hour without a flash on 13/11 was chance (at two or three flashes an hour, a 30-minute
> window without one happens one time in three to five). With them the cache is no longer switched off during a flash write (ESP-IDF 5.5.5 source), so the
> mechanism this page suspected is ruled out as well. The settings are kept for now (they match the ESPHome firmware and cost no
> flash); the cause is still not proven ([below](#revu-le-25112026--le-flash-revient-avec-ces-réglages)). Filmed the same evening:
> the flash is the **backlight** getting brighter for about 50 ms with the same picture on the panel, starting about 20 ms after an
> HTTPS request began, so the HTTPS request ruled out above is a suspect again ([below](#filmé-le-25112026-au-soir--le-rétroéclairage-saute)).

- **Statut** : **proposée**, écrite le 13/11/2026 après l'essai sur la 7B accepté par Axel (« Oui, flashe l'essai ») ; **remise en
  cause le 25/11/2026** : le flash blanc est revu avec ces réglages (voir [plus bas](#revu-le-25112026--le-flash-revient-avec-ces-réglages)).
  Les réglages restent en place ; ils ne corrigent pas le flash.
- **Vu sur** : Waveshare 7B (ESP32-P4 rév. v3.2, dalle EK79007 1024 × 600), 13/11/2026, binaire d'essai = `b9aa06e` + ces deux
  réglages + une sonde de diagnostic (une tâche de 4 Ko et une file de 64 relevés, non fusionnée) ; journal série du démarrage et
  des 30 premières minutes, flashé à 23 h 11.

## Contexte

Le 13/11/2026, Axel signale que la 7B « clignote de temps en temps (petit flash blanc) » : au repos, sans toucher, plusieurs fois
par heure, seulement depuis Meridien (jamais vu avec le firmware ESPHome sur la même carte). La surface touchée (tout l'écran ou
une partie) n'est pas connue.

Exclu par mesure, avec le firmware `b9aa06e` puis avec une sonde de diagnostic (chaque passage de rendu de LVGL au journal :
zones, pixels redessinés, durée ; registres `INT_ST0` / `INT_ST1` et verrou de la PHY de l'hôte DSI lus toutes les millisecondes) :

- **un redémarrage** : 38,9 h de fonctionnement continu avant l'essai ;
- **un underrun du pont DSI** : ESP-IDF le journalise toujours (`can't fetch data from external memory fast enough`), jamais vu ;
- **une erreur de l'hôte DSI** : aucun bit levé en fonctionnement (un seul `int_st0=0x00100000` à l'initialisation de la dalle,
  relu nul à la milliseconde suivante) ;
- **un repeint plein écran au repos** : seule l'horloge est redessinée (11 % de l'écran par minute, 30 % au changement d'heure,
  60 % en 97 ms pour une météo nouvelle) ;
- **une requête HTTPS** : un flash vu à 22 h 37 tombait pendant une requête à Vigie, mais 20 minutes de requêtes Vigie forcées
  chaque minute n'ont donné aucun flash. **Remis en cause le 25/11/2026** : le flash filmé commence 20 ms après le début d'une
  requête HTTPS ([plus bas](#filmé-le-25112026-au-soir--le-rétroéclairage-saute)).

Le chemin de la dalle est le même que sous ESPHome (mêmes timings DPI, une seule image, copie DMA2D). La seule différence trouvée
est la mémoire : ESPHome exécute le code depuis la PSRAM avec un cache L2 de 256 Ko (`execute_from_psram: true`,
`CONFIG_CACHE_L2_CACHE_256KB`), Meridien l'exécutait depuis la flash avec 128 Ko. Avec le code en flash, chaque écriture en flash
(la NVS de la météo, toutes les 15 à 16 minutes environ) coupe le cache. C'est une **hypothèse** : le flash de 22 h 37 ne tombait
pas sur une écriture de la météo.

## Décision

Dans `boards/waveshare-7b/sdkconfig.defaults.board`, pour la 7B seulement :

```
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_CACHE_L2_CACHE_256KB=y
```

La taille de ligne du cache L2 reste celle par défaut (la FAQ d'Espressif conseille aussi 128 octets : pas essayé, pas changé).

## Conséquences (mesurées sur la 7B, binaire d'essai avec la sonde)

| Jalon (`JALON_MEM`) | Code en flash, L2 128 Ko | Code en PSRAM, L2 256 Ko |
|---|---|---|
| RAM interne libre, heure reçue | 323 127 o | 181 191 o |
| RAM interne libre, après `httpd_start` et mDNS | 310 695 o | 168 779 o |
| Plus gros bloc DMA, après `httpd_start` et mDNS | 262 144 o | 120 832 o |
| Minimum depuis le démarrage (30 premières minutes) | non relevé | 166 848 o |

- Météo (Open-Meteo), alertes (Vigie) et mer répondent HTTP 200 ; aucune ligne `E (` nouvelle ; rétroéclairage allumé à 393 ms.
- Application de la 7B : 2 599 824 o avec ces deux réglages, sans la sonde (2 595 312 o pour `b9aa06e`, soit +4 512 o ; 38 %
  de l'emplacement de 4 Mo libre) ; compilée sans avertissement, ESP-IDF 5.5.5.
- Le Tab5 n'est pas touché : il garde le code en flash et 128 Ko de cache.

**Non vérifié** : laquelle des deux options compte, et pourquoi ; une observation de plusieurs heures ; la RAM interne pendant
que les réglages de l'écran sont ouverts (la liste prend environ 21,5 Ko de RAM interne, [dépannage](../depannage.md)) ; la durée
des prises de contact TLS ; le débit d'image (le code lit désormais la même PSRAM que le balayage de la dalle).

## Alternatives écartées

- **Chercher la cause exacte d'abord** (filmer la dalle au téléphone et recouper avec la sonde, essayer chaque option seule) :
  plusieurs heures d'observation par essai, pour un réglage qui est déjà celui du firmware ESPHome sur cette carte. Reste possible
  si le flash revient ou si la RAM interne manque.
- **Garder 128 Ko de cache avec le code en PSRAM** : rendrait environ 128 Ko de RAM interne, mais n'est pas la configuration
  essayée ; à essayer seulement si la RAM interne vient à manquer.
- **Appliquer aussi au Tab5** : aucun flash signalé sur le Tab5 ; rien à corriger, et la RAM interne y sert davantage.

## Ce qui la remettrait en cause

Un flash blanc revu sur la 7B avec ces réglages ; un manque de RAM interne (échec TLS, `Cannot allocate memory`) ; une mesure qui
montre qu'une seule des deux options suffit.

## Revu le 25/11/2026 : le flash revient avec ces réglages

Le 25/11/2026, Axel signale que la 7B « perd l'image un très bref moment », toujours, rarement et sans qu'il puisse dire quand.
La 7B tournait `main` `4a9a7be` (`/api/status` : `jalon-1a-tab5-st7121-495-g4a9a7`), qui contient ces deux réglages depuis le
14/11/2026. Ce qui en découle :

- **Ces réglages ne corrigent pas le flash.** La demi-heure sans flash du 13/11 ne prouvait rien : à deux ou trois flashs par
  heure, une fenêtre de 30 minutes sans flash arrive entre une fois sur cinq et une fois sur trois (loi de Poisson : e^-1,5 et
  e^-1).
- **Le mécanisme supposé plus haut est exclu.** Dans ESP-IDF 5.5.5, `components/spi_flash/spi_flash_os_func_app.c` ne coupe plus
  le cache pendant une écriture en flash quand le code et les constantes sont en PSRAM (`SPI_FLASH_CACHE_NO_DISABLE`, vrai avec
  `CONFIG_SPIRAM_FETCH_INSTRUCTIONS` et `CONFIG_SPIRAM_RODATA`, que pose `CONFIG_SPIRAM_XIP_FROM_PSRAM`). L'écriture de la NVS de la
  météo ne peut donc plus geler le code ; un flash revu avec ces réglages ne vient pas de là.
- **Les réglages restent en place** : ce sont ceux du firmware ESPHome sur cette carte, ils ne coûtent pas de flash, et les retirer
  ne serait utile que si la RAM interne venait à manquer (environ 142 Ko rendus).
- **Cause : non prouvée.** La sonde de diagnostic n'a jamais tourné pendant un flash vu par quelqu'un, et elle ne voit pas le
  rétroéclairage. La piste suivie ce jour-là, un repeint complet vu en direct (changement de ciel), est écartée par le film du
  soir (ci-dessous).

### Filmé le 25/11/2026 au soir : le rétroéclairage saute

La 7B (même `main` `4a9a7be`) est filmée au téléphone à 60 images par seconde pendant 29 minutes, avec le Tab5 et un mur à côté
comme témoins, et son journal série est enregistré en même temps. Axel voit un flash ; le film le contient, et c'est le seul saut
de luminosité net des 29 minutes.

- **Ce n'est pas l'image.** Pendant 3 à 4 images (environ 50 ms), toute la dalle s'éclaircit d'un coup, zones sombres comprises
  (la case la plus sombre de la dalle passe de 39 à 62 niveaux de gris sur 255 dans le film), avec la même image dessus ; elle
  revient après un léger creux. Ni le Tab5 ni le mur ne bougent. Un repeint ou une image perdue changerait le dessin : ici c'est
  la lumière derrière la dalle qui monte, le **rétroéclairage**.
- **Il tombe au début d'une requête HTTPS.** Les horloges sont recalées par le changement de minute vu sur la dalle (l'horloge du
  téléphone avance de 1,35 s sur celle de la carte, à environ 50 ms près) : le flash commence à 22 h 04 min 05,035 s, heure de la
  carte, 20 ms après la ligne `mer: requête de la mer` (05,016 s) et avant `Certificate validated` (05,279 s). Les six autres
  requêtes du film (météo, alertes, mer) ne donnent aucun saut. Le flash vu le 13/11 à 22 h 37 tombait lui aussi pendant une
  requête HTTPS.
- **Piste** : le rétroéclairage de la 7B est réglé par une tension analogique : signal PWM de 5 kHz sur GPIO32, inversé (broche
  haute = dalle plus sombre), lissé par un filtre RC vers le pilote AP3032 (`components/bsp_waveshare_7b/ws7b_io.c`). Pendant
  que la radio Wi-Fi (ESP32-C6) émet, une baisse du 3,3 V, ou une captation radio sur ce nœud, ferait baisser cette tension et
  éclaircirait la dalle un instant ; l'effet serait plus fort à luminosité basse. C'est une hypothèse.

**Non vérifié** : le mécanisme (aucune mesure électrique) ; le lien avec les requêtes réseau (un seul flash filmé) ; la luminosité
réglée à ce moment ; le flash avec la luminosité à 100 % (la broche reste alors basse et une baisse d'alimentation ne la déplace
plus : c'est l'essai qui départage). Les optimisations de l'affichage de l'[ADR-0030](0030-7b-deux-tampons-d-image-synchronises.md)
ne peuvent pas corriger ce flash.
