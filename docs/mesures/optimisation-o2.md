# Compilation en `-O2` au lieu de `-Og` — mesure avant / après sur le Tab5 (ST7121)

> **Summary (EN)** — Before/after of building with `-O2` (`CONFIG_COMPILER_OPTIMIZATION_PERF=y`) instead of ESP-IDF's default `-Og`, measured on the Tab5 ST7121 development board on 2026-11-09 (6 soft restarts per binary, same method, Wi-Fi not configured in either build).
> - First image 2210 → 2116 ms (about −95 ms), main screen 2107 → 2012 ms; CPU rotation loop 649–651 → 303–306 µs; PPA path unchanged (361 → 355 µs); worst render time of an image 394 → 355 ms; free internal RAM +5.1 kB; largest free block unchanged; no crash.
> - **Cost to watch**: the minimum free stack of the `taskLVGL` task fell from 1768 to 716 bytes (stack of 7168 bytes, the `esp_lvgl_port` default). Not fixed here.
> - Not measured: touch latency and I2C load of the 10 ms touch read (needs a finger), the Waveshare 7B (never touched), behaviour with Wi-Fi/TLS in `-O2`, the average render time (not comparable: the two windows hold a different number of images).

**Vu sur** : M5Stack Tab5, écran ST7121 (carte de développement, identifiée par sa MAC lue sur le bus USB puis par `esptool read-mac`, jamais écrite dans le dépôt), alimentation USB ; binaire « avant » `c3fd97b` compilé en `-Og` (SHA-256 `00B75E12…`, 3 093 184 o, version lue sur la carte `jalon-1a-tab5-st7121-305-gc3fd9`), binaire « après » `dca5e93` compilé en `-O2` (SHA-256 `E063D3B9…`, 3 258 032 o, version lue sur la carte `jalon-1a-tab5-st7121-309-gdca5e`) ; 09/11/2026 ; 6 démarrages par binaire, tous par la commande série `redemarrer` (le premier aussi), **sans Wi-Fi configuré** (les identifiants ne sont pas compilés dans ces binaires). Aucune autre carte n'a été touchée. Le firmware d'origine (`c3fd97b`, compilé le 09/11/2026 à 08:21:27) a été remis sur la carte à la fin et sa version relue.

Règle de lecture : ce qui n'est pas marqué « mesuré » est une lecture de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## Méthode

- Deux binaires compilés avec le même ESP-IDF 5.5.5, dans des dossiers distincts : seule la ligne `CONFIG_COMPILER_OPTIMIZATION_PERF=y` (et les trois corrections d'avertissements, qui ne changent pas le texte produit) les sépare ; le binaire « après » contient aussi la lecture tactile à 10 ms, dont l'effet n'est pas mesuré ici.
- Flash de l'application seule (0x10000) après sauvegarde et vérification du hachage ; version et heure de compilation relues sur la carte avant chaque série.
- Chaque série : 6 démarrages de 50 s, journal série horodaté par le PC ; à 40 s de chaque cycle la commande `memoire`, au dernier cycle la commande `piles`. Les lignes lues sont celles que le firmware écrit déjà (`banc de rotation`, `rétroéclairage allumé`, `JALON_MEM`, `affichage sur … ms`, `MEMOIRE`, `PILE`).

## Résultats (mesuré, moyenne des 6 démarrages sauf mention)

| Mesure | `-Og` (avant) | `-O2` (après) | Écart |
| --- | --- | --- | --- |
| Première image affichée (rétroéclairage allumé) | 2236 ms (min 2209, max 2363) | 2116 ms (min 2115, max 2117) | −120 ms en moyenne, −94 ms sur le minimum |
| Écran principal affiché (`JALON_MEM`) | 2132 ms | 2012 ms | −120 ms |
| Boucle de rotation du processeur (banc au démarrage) | 649–651 µs | 303–306 µs | environ −53 % |
| Rotation par le PPA (banc au démarrage) | 361–362 µs | 355–356 µs | inchangé (−2 %) |
| Pire rendu d'une image (maximum de la fenêtre) | 394 ms | 355 ms | −10 % |
| Mémoire interne libre, après l'écran principal | 249 039 o | 254 123 o | +5 084 o |
| Mémoire interne libre, à 40 s | 204 663 o | 209 747 o | +5 084 o |
| Plus gros bloc interne libre | 86 016 o | 86 016 o | inchangé |
| Marge minimale de la pile de `taskLVGL` (pile de 7168 o) | 1768 o | **716 o** | **−1052 o** |
| Plantage, redémarrage inattendu | aucun | aucun | |

Notes :

- Le démarrage 4 de la série « avant » a une première image à 2363 ms : une erreur connue du ré-initialisation de l'expandeur (`pi4ioe5v6408: Chip reset failed`, défaut déjà ouvert, non causé par cette PR) qui ajoute un nouvel essai d'environ 150 ms. La série « après » ne l'a pas rencontrée ; l'écart de 120 ms en moyenne contient donc ce hasard, d'où le chiffre du minimum (−94 ms) à côté.
- **Le rendu moyen par image n'est pas comparable** : 127–155 ms « avant », 180–182 ms « après », mais les fenêtres de mesure ne contiennent pas le même nombre d'images (4 ou 5 « avant », 3 « après ») ni les mêmes (la première image, la plus lourde, pèse plus dans une fenêtre courte). On ne peut ni conclure à un gain ni à une perte sur la moyenne ; seul le maximum est repris.
- La rotation du PPA ne gagne pas ce que gagne la boucle du processeur : le Tab5 utilise le PPA, la boucle du processeur n'est que le repli. Le gain de rotation n'est donc pas un gain d'affichage en fonctionnement normal ; le gain visible vient du reste du code (première image, rendu).

## Ce qui change de risque : la pile de `taskLVGL`

La marge minimale passe de 1768 à 716 octets sur une pile de 7168 octets (valeur par défaut d'`esp_lvgl_port`, que le dépôt ne change pas), soit 90 % de la pile atteinte, sur la seule séquence de démarrage et l'écran principal. Ce n'est pas un dépassement, mais la marge est mince et les écrans de réglages, la saisie et les popups n'ont pas été parcourus dans cette mesure. À faire avant de s'y fier : relever la marge après un parcours de tous les écrans (autotest des réglages, `-DMERIDIEN_AUTOTEST_REGLAGES=ON`) et, au besoin, augmenter la pile (par exemple 8192 octets, environ 1 ko de mémoire interne de moins). **Non fait ici.**

## Ce qui n'est pas vérifié

- Le délai appui → réaction à l'écran et la charge du bus I2C de la lecture tactile à 10 ms : il faut un doigt et un banc ; rien n'a été mesuré. Sur le Tab5, le rôle exact de l'interruption GPIO23 avec ce réglage n'est pas non plus vérifié.
- Le comportement avec le Wi-Fi, TLS et la météo en `-O2` : les deux binaires mesurés n'avaient pas de Wi-Fi configuré. Le firmware d'origine, remis ensuite, s'est reconnecté (`wifi_ms=7090`, `ip_ms=10133`), ce qui ne dit rien du `-O2`.
- Une durée plus longue (fuite de mémoire de quelques minutes, 30 démarrages, plusieurs heures) : 6 démarrages seulement.
- La Waveshare 7B : jamais touchée dans cette mesure ; elle reste « compilée seulement ».
