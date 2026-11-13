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

- **Statut** : **proposée**, écrite le 13/11/2026 après l'essai sur la 7B accepté par Axel (« Oui, flashe l'essai ») ; à confirmer
  par Axel sur une observation plus longue que la première demi-heure.
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
  chaque minute n'ont donné aucun flash.

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
