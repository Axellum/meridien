# Budget de la mémoire interne — Tab5 (ST7121), build intégré puis correctifs

> **Summary (EN)** — Internal-RAM budget of the integrated build (`integration/essai-1`, `4e7d8af`) on the M5Stack Tab5 (ST7121 display), measured on 2026-11-06/07 with temporary serial commands (`memoire`, `blocs`, `piles`) and per-stage log lines (`JALON_MEM`, `JALON_CLASSES`).
> - Before: 111.6 kB of internal RAM free before the first weather request and 74.3 kB once the web server was up. The TLS handshake failed (`esp-aes: Failed to allocate memory…`, `PK verify failed with error 0x4290`) and, after the first failures, the weather task kept core 0 for 14 s and tripped the task watchdog.
> - Where it goes (measured): 216 kB of display buffers (3 blocks of 72 000 B, owned by the main-screen session), 32.8 kB of ESP-IDF DMA reserve pool, about 106 kB of task stacks (20 tasks), the rest in small blocks; about 41 kB of those blocks (≥ 256 B) can live in PSRAM.
> - Fix: `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` 16384 → 256 (+65.9 kB free before the first request: 177.6 kB), mbedTLS buffers in PSRAM (about 29 kB less internal RAM during a handshake, handshake 2.8 s instead of 1.5 s), weather task at idle priority (the task watchdog stays enabled; proof with an 8-second spin task: it fires at 5 s at priority 3 and does not at priority 0).
> - Result (final binary, 07/11/2026): 178.5 kB free before the first request (minimum 95.9 kB); 30 software reboots in a row, display and backlight 30 of 30, 29 of 30 weather requests observed to the end and successful (27 in a row), 0 watchdog, 0 "Failed to allocate", 0 TLS error; 2 of 30 requests stayed silent for ≥ 10 s (one ended at 12.9 s, successfully), cause not established.
> - **Open defect**: free internal RAM still falls, −202.6 B/min over 30 minutes (−172.4 B/min after 5 min), exactly −152 B at every clock-minute change; PSRAM is flat after minute 6. Source **not resolved** (lead: the minute update of the clock label, not proven); linear extrapolation reaches 60 kB free in about 5.8 h. See §8, §7 and §11.

**Vu sur** : M5Stack Tab5, écran ST7121 (contrôleur détecté par le firmware au démarrage), coprocesseur C6 en micrologiciel 1.4.1 (jamais écrit), alimentation USB ; base `4e7d8af` (branche `integration/essai-1`), outils `86e9805` et `1d81971`, réglages `4f6bd0a`, priorité météo `2a05811` ; du 06/11/2026 23 h 30 au 07/11/2026 ; la MAC et l'adresse IP ne figurent pas ici. Les chiffres « avant » sont ceux du binaire intégré avec les seuls outils de mesure ajoutés ; les chiffres « après » sont ceux du binaire final `2a05811` (SHA-256 en §5).

Règle de lecture : ce qui n'est pas marqué « mesuré » est une lecture de source ou de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Question et critères

Question de la coordination : le build intégré (écran, Wi-Fi par le C6, SNTP, météo HTTPS, serveur web) laissait trop peu de RAM interne pour la prise de contact TLS de la météo. Où va la mémoire, que peut-on déplacer sans casser l'écran, et le correctif tient-il ?

Critères de « fait » du lot :

1. avant la première requête météo : mémoire interne libre ≥ 150 Ko et minimum depuis le démarrage ≥ 60 Ko ;
2. au moins 5 requêtes météo de suite réussies, sans chien de garde ni « Failed to allocate » ;
3. pente de la mémoire interne mesurée par `memoire` chaque minute pendant 30 min, rapportée telle quelle ;
4. 10 remises à zéro de suite sans régression de la reprise de l'expandeur 0x43.

## 2. Outils de mesure (provisoires, à retirer avant la phase 2)

| Outil | Ce qu'il donne |
| --- | --- |
| `JALON_MEM` (journal, `main.cpp`, `reseau.cpp`, `serveur_web.cpp`) | mémoire interne libre, plus gros bloc libre, plus gros bloc libre DMA et minimum depuis le démarrage à 12 étapes du démarrage, avec la différence depuis l'étape précédente |
| `JALON_CLASSES` | à chaque étape, nombre de blocs et octets utilisés par classe de taille (< 64 o, 64–255, 256–1023, 1–4 Ko, 4–16 Ko, 16–64 Ko, ≥ 64 Ko) : dit quelle classe de blocs une étape a consommée |
| `memoire` (console série) | une ligne `MEMOIRE` : libre, plus gros bloc, minimum, PSRAM libre, plus gros bloc DMA |
| `blocs` (console série) | parcours du tas interne (`heap_caps_walk`) : totaux, classes, les 64 plus gros blocs avec leur adresse ; un relevé est aussi pris juste après le démarrage du serveur web, avant la première requête TLS |
| `piles` (console série) | pour chaque tâche : base de sa pile, marge minimale restante, priorité (à rapprocher des adresses de `blocs`) ; demande `CONFIG_FREERTOS_USE_TRACE_FACILITY` |
| `tourner3` / `tourner0` (console série) | une tâche sur le cœur 0 calcule 8 s sans jamais s'endormir, à la priorité 3 ou 0 : essai du chien de garde (§6) |

Les lignes de journal de deux tâches s'entrelacent parfois (deux cœurs écrivent sur le même port) : une étape illisible dans un démarrage a été relue dans un autre démarrage du même binaire (les valeurs concordent à ± 0,1 Ko).

## 3. Où part la mémoire interne (configuration d'origine)

### 3.1 Étape par étape

Libre en Ko (mesuré, `JALON_MEM`). Les colonnes « après » sont celles du binaire final (§5), une capture. L'étape « serveur_web lancée » n'était pas lisible dans la capture « avant » (lignes entrelacées par deux cœurs) : elle est reprise de la variante e1, qui a la même configuration mémoire que le binaire « avant » (seule la priorité de la tâche `meteo` diffère). Les 20 démarrages de la série du §9 donnent, pour le binaire final, les mêmes étapes à ± 1,2 Ko près (première lecture lisible de chaque étape dans la série : 184,3 Ko à `serveur_web` lancée, 151,0 Ko après `httpd_start` et mDNS).

| Étape du démarrage | Avant : libre | Avant : Δ | Après : libre | Après : Δ | Gain | Contenu identifié |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| début de `app_main` | 504,8 | — | 505,2 | — | +0,4 | dont 32,8 de réserve DMA d'IDF (§4) |
| NVS et réglages | 493,6 | −11,2 | 503,8 | −1,4 | +10,2 | un bloc de 9 728 o (apparu à cette étape) passe en PSRAM |
| `board_init` (écran, tactile, LVGL, tampons) | 260,6 | −233,0 | 274,8 | −229,0 | +14,2 | 216,0 de tampons d'écran, 7,2 de pile `taskLVGL`, le reste en petits blocs |
| écran principal (objets, police, première image) | 180,9 | −79,7 | 221,5 | −53,2 | +40,6 | objets LVGL et polices TinyTTF : 1 400 petits blocs de moins de 64 o (+30,0), +15,9 de 64–255 o, +13,7 de 256–1023 o, +6,0 de 1–4 Ko, +6,8 de 4–16 Ko |
| tâche `reseau` lancée | 168,2 | −12,6 | 208,9 | −12,6 | +40,6 | pile 8 192 o + TCB 352 o + files |
| tâche `meteo` lancée | 155,6 | −12,6 | 196,2 | −12,6 | +40,6 | pile 12 288 o + TCB |
| tâche `serveur_web` lancée | 142,9 (variante e1, §5) | −12,6 | 183,6 | −12,6 | +40,7 | pile 12 288 o + TCB (la tâche reste vivante) |
| lien C6 (esp_hosted, SDIO) | 125,4 | −14,8 | 183,8 | +2,8 | +58,4 | piles SDIO/RPC (5 × 5 120 o au total), tampons ; le seuil épargne 17,8 Ko de plus à cette étape |
| Wi-Fi démarré (esp_wifi, lwIP, netif) | 119,3 | −6,1 | 178,0 | −5,8 | +58,7 | pile `tiT` 3 584 o |
| IP obtenue, SNTP, serveur web en route | 111,9 | −7,5 | 170,6 | −7,4 | +58,7 | |
| **première requête météo** (journal `meteo`) | **111,6** | | **178,5** | | **+66,9** | minimum depuis le démarrage : 95,5 avant, 128,6 après |
| serveur HTTP et mDNS démarrés | 74,3 | −37,6 | 149,8 | −20,8 | +75,5 | pile `httpd` 20 480 o, pile `mdns` 4 096 o, tables, tampons |

Lecture : avant le correctif, le serveur web et la requête TLS arrivaient en même temps (≈ 13,3 s après le démarrage) avec 74 Ko libres. Les 216 Ko de tampons d'écran, les piles (≈ 106 Ko) et la réserve DMA (32,8 Ko) font à eux seuls plus de 350 Ko des 505 Ko du départ.

### 3.2 Par classe de taille, à la fin de `app_main`

Octets utilisés (mesuré, `JALON_CLASSES`). Le parcours du tas compte la réserve DMA de 32 768 o à la fois dans la classe 16–64 Ko et dans le libre (§4) : la somme des classes dépasse donc un peu la mémoire réellement prise.

| Classe | Avant : blocs | Avant : octets | Après : blocs | Après : octets | Écart |
| --- | ---: | ---: | ---: | ---: | ---: |
| < 64 o | 1 644 | 35 156 | 1 649 | 35 216 | +60 |
| 64–255 o | 267 | 21 600 | 268 | 21 664 | +64 |
| 256–1023 o | 55 | 27 704 | 28 | 11 440 | −16 264 |
| 1–4 Ko | 13 | 24 000 | 8 | 16 128 | −7 872 |
| 4–16 Ko | 13 | 94 848 | 11 | 78 336 | −16 512 |
| 16–64 Ko | 1 | 32 768 | 1 | 32 768 | 0 |
| ≥ 64 Ko | 3 | 216 000 | 3 | 216 000 | 0 |
| **Total utilisé** | | **452 076** | | **411 552** | **−40 524** |
| Libre | | 139 460 | | 180 096 | +40 636 |

Ce que le seuil de 256 o a déplacé en PSRAM par `malloc()` : 40,6 Ko avant même le Wi-Fi (les blocs de 256 o et plus : cache de la NVS, tampons de police, tables). Ce que le seuil ne peut pas déplacer : les 1 649 + 268 petits blocs (≈ 57 Ko, moins de 256 o), les piles de tâches (FreeRTOS les force en interne), les tampons DMA et la réserve DMA.

### 3.3 Gros blocs identifiés

| Bloc | Octets | Propriétaire | Mesuré ou lu |
| --- | ---: | --- | --- |
| 3 blocs identiques | 3 × 72 000 | tampons d'écran : 2 tampons de dessin (720 × 50 px × 2 o) + sortie PPA (même taille, `lcd_ppa.c`) ; session de l'écran principal | mesuré (taille) ; répartition lue dans `tab5_board.c` et `lcd_ppa.c` |
| réserve DMA d'IDF | 32 768 | `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` (§4) | mesuré (présent dès `app_main`) |
| pile `httpd` | 20 480 | `serveur_web.cpp` | mesuré (`piles`) |
| piles `serveur_web` et `meteo` | 2 × 12 288 | `serveur_web.cpp`, `meteo_client.cpp` | mesuré |
| bloc apparu à l'étape NVS | 9 728 | probablement la NVS (partition de 448 Ko) | mesuré ; attribution à la NVS **non vérifiée** |
| pile `taskLVGL` | 7 168 | `esp_lvgl_port` | mesuré |
| ≈ 20 blocs de 352 o | ≈ 7 000 | probablement les TCB des 20 tâches (FreeRTOS) | blocs mesurés ; attribution **non vérifiée** |

### 3.4 Piles des tâches (binaire final, ≈ 70 s après le démarrage, une requête TLS réussie, aucune requête servie par `httpd`)

| Tâche | Octets de pile | Marge minimale mesurée | Priorité |
| --- | ---: | ---: | ---: |
| `httpd` | 20 480 | 19 800 | 3 |
| `serveur_web` | 12 288 | 9 664 | 3 |
| `meteo` | 12 288 | 7 832 | 0 (3 avant `2a05811`) |
| `taskLVGL` | 7 168 | 2 296 | 4 |
| `sdio_read`, `sdio_process_rx`, `sdio_write`, `rpc_rx`, `rpc_tx` | 5 × 5 120 | 3 324 ; 3 172 ; 4 380 ; 2 188 ; 4 368 | 23 |
| `mdns`, `esp_timer` | 4 096 chacune | 2 352 ; 3 740 | 1 ; 22 |
| `tiT` (lwIP) | 3 584 | 2 444 | 18 |
| `sdio_rx_buf`, `commande_serie` | 3 072 chacune | 1 204 ; 2 572 | 23 ; 2 |
| `sys_evt` | 2 816 | **852** | 20 |
| `Tmr Svc`, `IDLE0`, `IDLE1`, `ipc0`, `ipc1` | 2 048 ; 1 536 ; 1 536 ; 1 024 ; 1 024 | 1 716 ; 1 220 ; 1 212 ; 608 ; 760 | 1 ; 0 ; 0 ; 1 ; 24 |

À retenir (mesuré) : `serveur_web` n'a utilisé que 2,6 Ko de ses 12 288 o (le bandeau d'appairage compris) et `httpd` 0,7 Ko de 20 480 o **tant qu'aucune requête n'est servie** : la marge de `httpd` en service réel n'a pas été mesurée (non vérifié) ; `sys_evt` n'a que 852 o de marge. Réduire ces piles est un gain possible (≈ 8 Ko pour `serveur_web`, ≈ 4 Ko pour `meteo`) **non fait** : ces fichiers appartiennent à d'autres sessions et le critère 1 est atteint sans cela.

## 4. Hypothèses vérifiées dans les sources d'ESP-IDF 5.5.5

| Hypothèse | Ce que disent les sources (chemins sous `components/`) | Verdict |
| --- | --- | --- |
| Piles de tâches en PSRAM (`xTaskCreateWithCaps`, `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM`, `task_caps` d'`esp_http_server`) | L'option est activée (`sdkconfig`), `httpd_config_t.task_caps` existe (`esp_http_server/include/esp_http_server.h:57,176`, utilisé en `src/httpd_main.c:538`). **Mais** `spi_flash/cache_utils.c:127` fait `assert(esp_task_stack_is_sane_cache_disabled())` à chaque opération sur la flash, et cette fonction (`:56-65`) n'accepte qu'une pile en DRAM ; les assertions sont activées (`CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y`) | **Refusé** pour `meteo` (cache NVS), `serveur_web` et `httpd` (réglages écrits en NVS par l'API) : ces trois piles (45 Ko) doivent rester en interne tant que la NVS est écrite depuis elles. Non essayé sur la carte |
| Tampons mbedTLS : taille et placement | Entrée 16 384 o, sortie 4 096 o (`CONFIG_MBEDTLS_ASYMMETRIC_CONTENT_LEN`), `CONFIG_MBEDTLS_DYNAMIC_BUFFER` désactivé. `mbedtls/port/esp_mem.c:16-19` : mode `INTERNAL_MEM_ALLOC` → `MALLOC_CAP_INTERNAL`, mode `EXTERNAL_MEM_ALLOC` → `MALLOC_CAP_SPIRAM` | Mode externe **adopté** (§5). `DYNAMIC_BUFFER` : non essayé (inutile une fois le mode externe en place) |
| AES matériel ou logiciel | `CONFIG_MBEDTLS_HARDWARE_AES=y`. `mbedtls/port/aes/dma/esp_aes_dma_core.c:458-460` : le tableau de descripteurs DMA est **toujours** alloué en `MALLOC_CAP_DMA \| MALLOC_CAP_INTERNAL`, même si les données sont en PSRAM ; `:467` et `:489` : tampons d'alignement en interne si la donnée est en interne. Ce sont exactement les messages vus (« array of DMA descriptors » ligne 460, « start alignment buffer » ligne 469) | Cause **immédiate** des échecs d'avant : plus aucun bloc interne DMA libre au moment de l'opération AES. Passer l'AES en logiciel (`MBEDTLS_HARDWARE_AES=n`) supprimerait ce besoin ; **non essayé**, inutile avec 178 Ko libres |
| `SPIRAM_MALLOC_ALWAYSINTERNAL` | `heap/heap_caps.c:99-125` : `malloc(n)` prend l'interne d'abord si `n ≤ limite`, la PSRAM d'abord sinon, avec repli sur l'autre ; `esp_psram/Kconfig.spiram.common:97-107` (défaut 16384) ; la limite est posée au démarrage par `esp_psram.c:141`. `heap_caps_malloc_extmem_enable(limite)` la change à l'exécution | **Adopté à 256** (§5). Les piles de tâches et les tampons DMA ne sont pas concernés (ils demandent leurs capacités explicitement) |
| `SPIRAM_MALLOC_RESERVE_INTERNAL` | `esp_psram/system_layer/esp_psram.c:635-661` : un bloc interne DMA de 32 768 o est alloué puis ajouté comme région du tas (capacités `{0, DMA\|INTERNAL, DEFAULT\|8BIT\|32BIT}`) ; `Kconfig.spiram.common:117-135` : un `malloc()` ordinaire ne puise dans ce pool qu'après épuisement de tout le reste | Ce bloc figure comme **utilisé** dans le parcours du tas (classe 16–64 Ko, présent dès `app_main` : mesuré) ; que son libre soit compté dans `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` est une lecture de source (la région porte la capacité interne), **pas une mesure**. Le « libre » rapporté contient donc, d'après la source, jusqu'à 32,8 Ko que seuls les DMA, les piles et le dernier recours peuvent prendre. **Non modifié** (la baisser donnerait au plus +32 Ko aux `malloc()` ordinaires mais retirerait la protection des DMA et des piles) |
| Sérialiser les prises de contact TLS (mutex) | Le géocodage de la page de réglages (`serveur_web.cpp:153-190`, dans la tâche `httpd`) fait déjà une requête HTTPS : la météo et lui peuvent se chevaucher | Une prise de contact coûte ≈ 51 Ko internes avec mbedTLS interne (binaire `b1`) et ≈ 22,5 Ko en mode externe (`e2`). Deux à la fois : ≈ 46 Ko libres au minimum en interne (sous le critère de 60 Ko) contre ≈ 105 Ko en externe (calcul à partir de 150 Ko libres après le serveur web ; **le chevauchement n'a pas été essayé sur la carte**). Pas de mutex ajouté : le mode externe suffit sur le papier |

## 5. Essais sur la carte

Une seule variable à la fois quand c'était possible ; les cinq binaires sont conservés hors dépôt (`.cache/`). « Libre avant requête » = ligne `meteo: mémoire libre avant la requête`.

| Variante | Changement | SHA-256 | Libre avant requête | Minimum depuis le démarrage | Première requête météo | Chien de garde |
| --- | --- | --- | ---: | ---: | --- | --- |
| a | build intégré + outils de mesure (`86e9805`, trace FreeRTOS) | `C27CC0A3…118E` | 111,6 Ko | 17,9 Ko après la 1ʳᵉ requête, 0,7 Ko après la 3ᵉ | échec en 84 ms (`esp-aes` : descripteurs DMA), 2ᵉ et 3ᵉ échecs après 14 s (`PK verify failed 0x4290`) | déclenché 2 fois par requête lente |
| e1 | a + tâche météo à la priorité 0 | `2A767A0D…FB7A` | 111,3 Ko | 17,0 Ko | 3 échecs rapides (57 à 83 ms, `esp-aes`) | non déclenché (le cas lent de 14 s n'est pas revenu : **non concluant seul**) |
| b1 | a + seuil `malloc` à 256 o (appel à l'exécution) | `71052D5A…9CCE` | 177,6 Ko | 88,9 Ko (pendant la requête) | réussie, prise de contact 1,49 s | non déclenché |
| e2 | b1 + mbedTLS en PSRAM + météo priorité 0 + `tourner3`/`tourner0` | `E845F8BE…FC07` | 177,6 Ko | 95,5 Ko | réussie, prise de contact 2,76 s | `tourner3` : déclenché ; `tourner0` : non (§6) |
| **final** | réglages dans `sdkconfig.defaults*` (config régénérée), commits `4f6bd0a` et `2a05811` | `7EFD993F5DC555EB542FDEF1F02ABC51298E38D975B160EEF05F8059242419FC` (1 849 936 o) | **178,5 Ko** | **128,6 Ko avant la requête, 95,9 Ko après** | réussie, prise de contact 2,78 s | non déclenché |

Le rétroéclairage s'allume (première image affichée) à 1 858–1 901 ms avant comme après (5 démarrages « avant », 7 « après », en redémarrage logiciel) : le coût des objets passés en PSRAM n'apparaît pas sur le temps de construction de l'écran. **La fluidité des animations n'a pas été mesurée** (non vérifié ; à juger sur l'écran).

La PSRAM libre baisse d'environ 73 Ko au même instant (31,52 Mo → 31,44 Mo) : négligeable sur 31,4 Mo.

## 6. Chien de garde des tâches

Constat sur le build intégré : `task_wdt: IDLE0 (CPU 0)` avec `CPU 0: meteo`, `CPU 1: IDLE1`, toutes les 5 s pendant les 14 s d'une prise de contact ratée (`CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`, contrôle des tâches d'inactivité des deux cœurs actif).

Cause : la prise de contact TLS calcule plusieurs secondes sans s'endormir ; la tâche `meteo` était à la priorité 3, au-dessus de celle de la tâche d'inactivité (0) : quand elle occupe un cœur, la tâche `IDLE` de ce cœur ne tourne plus et ne nourrit plus le chien de garde.

Correctif (`2a05811`) : `meteo` passe à `tskIDLE_PRIORITY` (0) ; elle partage le cœur avec `IDLE` tranche par tranche (1 ms, `FREERTOS_HZ` = 1000). Le chien de garde n'est ni désactivé ni allongé.

Preuve du mécanisme, indépendante de la météo (binaire `e2`, mesuré) : `tourner3` (tâche épinglée sur le cœur 0, 8 s de calcul sans pause, priorité 3) → `task_wdt` déclenché 5,0 s après le début ; `tourner0` (même calcul, priorité 0) → aucun déclenchement. Sur la carte, la prise de contact réelle dure maintenant 1,5 à 2,8 s, sous la limite de 5 s dans les deux priorités.

## 7. Résultat contre les critères du lot

| Critère | Résultat | État |
| --- | --- | --- |
| 1. Libre avant la 1ʳᵉ requête ≥ 150 Ko ; minimum depuis le démarrage ≥ 60 Ko | 178,5 Ko ; minimum 128,6 Ko avant la requête et 95,9 Ko jusqu'à la fin de la requête (binaire final) | **constaté** |
| 2. ≥ 5 requêtes météo de suite sans chien de garde ni « Failed to allocate » | 27 démarrages de suite (`BOOT` 140 à 166) avec requête réussie ; 29 sur 30 observées jusqu'au bout, 1 sans issue observée ; 0 chien de garde, 0 « Failed to allocate », 0 erreur TLS ; plus 2 requêtes périodiques réussies pendant les 30 min (§9, §8.3) | **constaté**, avec une réserve : 2 requêtes sur 30 restent ≥ 10 s sans rien dire (une a fini à 12,9 s, l'autre n'a pas été suivie), cause non établie |
| 3. Pente sur 30 min, relevé chaque minute | −202,6 o/min brut, −172,4 o/min après 5 min (−6 824 o en 29 min, 30 relevés) ; −152 o à chaque changement de minute de l'horloge ; deux requêtes météo réussies dans la fenêtre (§8) | **mesuré et rapporté tel quel ; la mémoire continue de baisser, source non résolue (défaut ouvert)** |
| 4. 10 remises à zéro sans régression de la reprise de l'expandeur 0x43 | 30 remises à zéro logicielles de suite : « Carte prête » et rétroéclairage 30 sur 30, aucune ligne `expandeur 0x43`, aucun écran resté éteint (§9) | **constaté : pas de régression ; la reprise elle-même n'a pas été déclenchée, donc pas éprouvée** |

## 8. Pente de la mémoire interne sur 30 min

**Vu sur** : Tab5 ST7121, binaire final `7EFD993F…19FC` (commit `2a05811`, arbre propre), 07/11/2026 de 00 h 07 à 00 h 49 (heure de Paris), démarrage unique, aucune action sur la carte pendant la mesure sauf la commande `memoire`. La carte est restée en service : horloge affichée, requête météo toutes les 15 min (§8.3).

### 8.1 Relevé minute par minute

La commande `memoire` est envoyée une fois par minute pendant 30 minutes (30 relevés, 29 intervalles). « Heure affichée » = minute de l'horloge au moment du relevé.

| n° | Carte (s) | Heure affichée | Interne libre (o) | Δ interne (o) | PSRAM libre (o) | Δ PSRAM (o) |
| ---: | ---: | :---: | ---: | ---: | ---: | ---: |
| 1 | 145 | 00:07 | 128755 | — | 31370120 | — |
| 2 | 206 | 00:08 | 128251 | -504 | 31343492 | -26628 |
| 3 | 266 | 00:09 | 127747 | -504 | 31316864 | -26628 |
| 4 | 326 | 00:10 | 127123 | -624 | 31299964 | -16900 |
| 5 | 386 | 00:11 | 126959 | -164 | 31299964 | +0 |
| 6 | 446 | 00:12 | 126483 | -476 | 31272824 | -27140 |
| 7 | 506 | 00:13 | 125999 | -484 | 31245684 | -27140 |
| 8 | 566 | 00:14 | 125835 | -164 | 31245684 | +0 |
| 9 | 626 | 00:15 | 125683 | -152 | 31245684 | +0 |
| 10 | 685 | 00:16 | 125531 | -152 | 31245684 | +0 |
| 11 | 746 | 00:17 | 125379 | -152 | 31245684 | +0 |
| 12 | 805 | 00:18 | 125227 | -152 | 31245684 | +0 |
| 13 | 866 | 00:19 | 125075 | -152 | 31245684 | +0 |
| 14 | 926 | 00:20 | 124771 | -304 | 31245684 | +0 |
| 15 | 986 | 00:21 | 124367 | -404 | 31245684 | +0 |
| 16 | 1046 | 00:22 | 124215 | -152 | 31245684 | +0 |
| 17 | 1106 | 00:23 | 124295 | +80 | 31245684 | +0 |
| 18 | 1166 | 00:24 | 124143 | -152 | 31245684 | +0 |
| 19 | 1225 | 00:25 | 123991 | -152 | 31245684 | +0 |
| 20 | 1286 | 00:26 | 123839 | -152 | 31245684 | +0 |
| 21 | 1345 | 00:27 | 123687 | -152 | 31245684 | +0 |
| 22 | 1406 | 00:28 | 123535 | -152 | 31245684 | +0 |
| 23 | 1466 | 00:29 | 123383 | -152 | 31245684 | +0 |
| 24 | 1526 | 00:30 | 123079 | -304 | 31245684 | +0 |
| 25 | 1585 | 00:31 | 122943 | -136 | 31245684 | +0 |
| 26 | 1645 | 00:32 | 122791 | -152 | 31245684 | +0 |
| 27 | 1706 | 00:33 | 122639 | -152 | 31245684 | +0 |
| 28 | 1766 | 00:34 | 122487 | -152 | 31245684 | +0 |
| 29 | 1826 | 00:35 | 122335 | -152 | 31245684 | +0 |
| 30 | 1886 | 00:36 | 121931 | -404 | 31245684 | +0 |

### 8.2 Pente

- **Pente brute : −202,6 o/min** (moindres carrés sur l'heure de la carte, 30 relevés ; −6 824 o en 29 min). **Après les 5 premières minutes : −172,4 o/min** (≈ −10,3 Ko/h).
- **La mémoire interne libre n'est pas stable.** Elle baisse sans interruption pendant toute la mesure : après la mise en route (00 h 07 à 00 h 14, −164 à −624 o par minute) elle baisse de **152 o exactement à chaque minute** (16 des 29 intervalles valent −152 o ; les autres valent −136, −164, −304, −404, −476, −484, −504, −624 ou +80 o).
- La **PSRAM** libre baisse de 124 436 o entre 00 h 07 et 00 h 13 (par paliers de 17 à 27 Ko à cinq des six premières minutes), puis **ne bouge plus pendant 36 minutes** (31 245 684 o de 00 h 13 à 00 h 49). C'est compatible avec le remplissage du cache de glyphes de la police TrueType (un chiffre rendu pour la première fois est mis en cache) ; **non prouvé** (aucun relevé du cache lui-même).

### 8.3 Ce qui est expliqué, ce qui ne l'est pas

Deux requêtes météo ont eu lieu dans la fenêtre (à 930 s et 1 867 s de la carte), toutes deux réussies, sans chien de garde ni « Failed to allocate » :

| Requête | Interne avant | Interne une fois la connexion établie | Interne après | Écart sur la requête | Minimum depuis le démarrage |
| --- | ---: | ---: | ---: | ---: | ---: |
| 930 s (00 h 20) | 124 771 | 123 023 (−1 748) | 124 403 | −368 | 95 916 (inchangé) |
| 1 867 s (00 h 35) | 122 335 | 120 583 (−1 752) | 121 951 | −384 | 95 916 (inchangé) |

Une prise de contact en service, avec mbedTLS en PSRAM, ne tient donc presque rien de plus en interne une fois établie (−1,7 Ko) et n'a pas fait descendre le minimum sous celui du démarrage (95,9 Ko) : le pic est au plus de 28,9 Ko. Chaque requête laisse −368 à −384 o de libre en moins ; les deux paliers de −404 o du tableau (00 h 21 et 00 h 36) suivent chacun une requête.

**Source des −152 o par minute : non résolue.** Ce qui est établi :

1. **Elle ne vient pas de la commande `memoire`.** Trois relevés espacés de 5 min sans aucune autre commande : 121 875 o (carte à 2 019 s), 120 963 o (2 319 s, −912), 120 203 o (2 619 s, −760), soit −152 à −182 o/min, comme avec un relevé par minute. Si c'était la commande, on aurait −152 o par relevé, soit −152 o sur 5 min.
2. **Elle tombe sur le changement de minute de l'horloge.** Deux paires de relevés à 10 s d'intervalle : 23 h 38 min 10 s → 20 s (même minute) : 121 875 → 121 875 (0 o) ; 23 h 48 min 56 s → 49 min 06 s (à cheval sur le changement de minute) : 120 203 → 120 051 (**−152 o**), heure UTC du PC.
3. Elle est **interne** : la PSRAM ne bouge pas après 00 h 13. Un bloc de 152 o reste en RAM interne quel que soit le seuil `SPIRAM_MALLOC_ALWAYSINTERNAL` (256 o) : le correctif de ce lot ne la change pas, et elle n'a pas été comparée à la configuration d'origine (non mesuré).

Piste, **non prouvée** : chaque minute, `ui_principal_mettre_a_jour()` remplace le texte de l'étiquette de l'heure (`changer_texte()` → `lv_label_set_text()`, `main/ui_principal.cpp:89-93`), ce qui fait redessiner l'étiquette et rendre des glyphes TrueType ; une fuite de 152 o dans ce chemin (LVGL, `lv_tiny_ttf`, ou un objet créé et jamais libéré) serait cohérente avec ce qui précède. Aucun essai avec l'horloge figée n'a été fait, et le code appartient à la session de l'écran principal. **L'écart avec les −2,8 Ko/min vus par la session météo sur son binaire (`97520e2`) n'est pas expliqué ici** : je n'ai pas ce binaire, et celui-ci perd 0,17 Ko/min en régime.

**Conséquence, si la pente se poursuit en ligne droite (extrapolation, non mesurée au-delà de 50 min)** : à −172 o/min depuis 120,2 Ko (00 h 48), la mémoire interne libre passerait sous 60 Ko vers +5,8 h, sous 30 Ko vers +8,7 h et à zéro vers +11,6 h. **Pour un écran allumé en permanence c'est un défaut ouvert** : à suivre par la session de l'écran principal (essai horloge figée, puis `heap_caps` avec traçage des allocations pour trouver le bloc de 152 o).

## 9. Remises à zéro de suite

**Vu sur** : Tab5 ST7121, binaire final `7EFD993F…19FC` (commit `2a05811`), **30 redémarrages logiciels de suite** (commande série `redemarrer`, qui finit par `esp_rom_software_reset_system()`), `BOOT` n° 137 à 166, 07/11/2026 de 00 h 50 à 01 h 09, même banc. Série 1 : 20 cycles, 15 s d'attente après la ligne `BOOT`. Série 2 : 10 cycles, 35 s d'attente (ajoutée pour savoir si la requête sans fin du cycle 3 de la série 1 aboutit). Aucune coupure de courant.

| Mesure | Résultat sur 30 démarrages |
| --- | --- |
| Écran : « Carte prête » et rétroéclairage allumé | **30 sur 30** ; rétroéclairage à 1 858–1 859 ms |
| Reprise de l'expandeur 0x43 | **aucune ligne** `expandeur 0x43` (ni « répond à l'essai », ni « muet ») sur 30 : la reprise **n'a pas été déclenchée**, donc pas exercée ; aucun écran resté éteint |
| Lien C6 / Wi-Fi / IP | lien au premier essai 30 sur 30, 0 échec de transport, 0 coupure, cause de reset 0x03, numéros `BOOT` consécutifs |
| Heure reçue | 30 sur 30, `ntp_ms` de 11 359 à 12 328 ms ; attente après l'IP : 29 à 279 ms, médiane 33 ms |
| Requête météo lancée | 30 sur 30 |
| Requête météo terminée et réussie (« cache écrit en NVS ») | **29 sur 30** (27 de suite, `BOOT` 140 à 166) ; 1 non observée jusqu'au bout (voir plus bas) |
| `task_wdt` déclenché / « Failed to allocate » / erreur TLS | **0 / 0 / 0** |
| Libre interne avant la requête | 178,0 à 181,3 Ko sur 29 démarrages (médiane 178,1) ; 130,9 Ko au `BOOT` 157, dont l'IP est arrivée à 12,3 s au lieu de 11,4 s et la requête est partie après le démarrage du serveur web |
| Libre interne après la requête | 128,9 à 130,0 Ko ; minimum depuis le démarrage 95,9 Ko à chaque fois (atteint entre 13 et 16 s, quand le serveur web démarre et que la première requête part ; pas isolé entre les deux) |
| Durée d'une requête réussie (lancement → cache écrit) | 2,72 à 3,44 s (médiane 2,78 s), sauf une de 12,9 s |

**Requêtes lentes.** Dans 2 démarrages sur 30, la requête reste ≥ 10 s sans rien dire après « Certificate validated » (15,6 s après le démarrage) : au `BOOT` 139 (série 1, cycle 3) rien jusqu'au redémarrage suivant, 12 s plus tard ; au `BOOT` 160 (série 2, cycle 4) la requête a fini à 26,2 s, **réussie**, 12,9 s au total au lieu de 2,8 s, sans erreur ni chien de garde. La tâche `meteo` attend alors sur le réseau (priorité 0, pas de calcul) : pas de chien de garde attendu. **Cause non établie** (une retransmission TCP sur le lien Wi-Fi/C6 serait compatible avec ~10 s ; non vérifié). Le cycle 3 de la série 1 est donc écrit « sans issue observée », ni réussi ni échoué.

**Ce que cette série ne dit pas.** Les remises à zéro sont logicielles : la reprise de l'expandeur 0x43 n'a jamais eu à servir ; la coupure de courant reste non faite (reportée par Axel). Les cycles sont à ~28 s d'intervalle : ils ne couvrent pas l'état « en service depuis des heures » (§8).

Détail des 30 démarrages (extraits sans donnée personnelle : `memoire-interne-journal-carte.log`) :

Série 1 (20 cycles, 15 s d'attente) :

| Cycle | BOOT | Carte prête | Rétroéclairage (ms) | Expandeur 0x43 | Requête météo | Libre avant (Ko) | Min. (Ko) | WDT / alloc / TLS | ntp_ms |
| ---: | ---: | :---: | ---: | :---: | :---: | ---: | ---: | :---: | ---: |
| 1 | 137 | oui | 1859 | rien | réussie | 178,6 | 128,7 | 0 / 0 / 0 | 11464 |
| 2 | 138 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11396 |
| 3 | 139 | oui | 1858 | rien | **sans résultat** | 178,0 | 128,5 | 0 / 0 / 0 | 11510 |
| 4 | 140 | oui | 1858 | rien | réussie | 181,3 | 128,5 | 0 / 0 / 0 | 11682 |
| 5 | 141 | oui | 1859 | rien | réussie | 178,2 | 128,7 | 0 / 0 / 0 | 11366 |
| 6 | 142 | oui | 1859 | rien | réussie | 178,2 | 128,7 | 0 / 0 / 0 | 11364 |
| 7 | 143 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11383 |
| 8 | 144 | oui | 1859 | rien | réussie | 178,2 | 128,7 | 0 / 0 / 0 | 11372 |
| 9 | 145 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11363 |
| 10 | 146 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11359 |
| 11 | 147 | oui | 1859 | rien | réussie | 178,5 | 128,6 | 0 / 0 / 0 | 11410 |
| 12 | 148 | oui | 1858 | rien | réussie | 178,3 | 128,5 | 0 / 0 / 0 | 11423 |
| 13 | 149 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11404 |
| 14 | 150 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11422 |
| 15 | 151 | oui | 1859 | rien | réussie | 178,2 | 128,7 | 0 / 0 / 0 | 11360 |
| 16 | 152 | oui | 1858 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11412 |
| 17 | 153 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11392 |
| 18 | 154 | oui | 1858 | rien | réussie | 178,0 | 128,5 | 0 / 0 / 0 | 11404 |
| 19 | 155 | oui | 1858 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11419 |
| 20 | 156 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11394 |

Série 2 (10 cycles, 35 s d'attente) :

| Cycle | BOOT | Carte prête | Rétroéclairage (ms) | Expandeur 0x43 | Requête météo | Libre avant (Ko) | Min. (Ko) | WDT / alloc / TLS | ntp_ms |
| ---: | ---: | :---: | ---: | :---: | :---: | ---: | ---: | :---: | ---: |
| 1 | 157 | oui | 1858 | rien | réussie | 130,9 | 95,9 | 0 / 0 / 0 | 12328 |
| 2 | 158 | oui | 1858 | rien | réussie | 178,4 | 128,5 | 0 / 0 / 0 | 11400 |
| 3 | 159 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11428 |
| 4 | 160 | oui | 1858 | rien | réussie | 178,0 | 128,5 | 0 / 0 / 0 | 11418 |
| 5 | 161 | oui | 1859 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11420 |
| 6 | 162 | oui | 1858 | rien | réussie | 178,1 | 128,6 | 0 / 0 / 0 | 11461 |
| 7 | 163 | oui | 1858 | rien | réussie | 178,0 | 128,5 | 0 / 0 / 0 | 11399 |
| 8 | 164 | oui | 1858 | rien | réussie | 178,0 | 128,5 | 0 / 0 / 0 | 11398 |
| 9 | 165 | oui | 1858 | rien | réussie | 178,0 | 128,5 | 0 / 0 / 0 | 11424 |
| 10 | 166 | oui | 1859 | rien | réussie | 178,2 | 128,7 | 0 / 0 / 0 | 11441 |

## 10. Pour la session de l'écran principal (chiffres, rien n'a été changé de leur côté)

- Tampons d'écran : 3 blocs de 72 000 o = 216 000 o, soit 42,8 % des 505,2 Ko libres au début de `app_main` et 47,5 % du tas interne initial de 455 Ko. Deux sont les tampons de dessin (`LVGL_DRAW_BUF_LINES` 50 × 720 px × 2 o), le troisième est la sortie du PPA (`lcd_ppa.c`, même taille). Le banc de rotation (`tab5_orientation_bench()`, appelé par `board_init`) alloue en plus, le temps de son essai, deux tampons DMA internes qu'il libère ensuite (`tab5_orientation.c:126-132,191-192`) : ils ne sont pas dans les chiffres ci-dessus mais pèsent sur le pic de `board_init` (non mesuré).
- Le seuil de 256 o fait passer le coût de l'écran principal de 79,7 Ko à 53,2 Ko internes ; 45,9 Ko des 53,2 Ko restants sont de petits blocs de moins de 256 o (objets et styles LVGL, structures de police, mesuré par classe), que le seuil ne déplace pas.
- Marge de la pile `taskLVGL` : 2 296 o sur 7 168 o (mesurée pendant des rafraîchissements d'une demi-seconde, sans appui).

## 11. Non vérifié et suites

- **Perte de 152 o de mémoire interne à chaque minute de l'horloge (≈ 9,1 Ko/h)** : défaut ouvert, source non résolue (§8.3). Suite proposée, pour la session de l'écran principal : essai avec l'horloge figée pour confirmer la piste, puis traçage des allocations du chemin `lv_label_set_text` de l'heure. Non mesuré au-delà de 50 min ; l'extrapolation en ligne droite (60 Ko libres vers +5,8 h) n'est pas un constat. La comparaison avec la configuration d'origine n'a pas été faite.
- **Requêtes météo lentes (≥ 10 s sans rien dire)** : 2 sur 30 démarrages (§9), cause non établie, pas de chien de garde attendu (la tâche attend le réseau). À rejouer avec une trace des paquets ou du délai de lecture de la connexion pour trancher.
- **Fluidité de l'interface** avec les objets en PSRAM : non mesurée (le temps de première image est inchangé). Les relevés « une image par minute » du binaire final (12,8 à 21 ms par image) n'ont pas de pendant « avant » de même durée : comparaison **non faite**.
- **Journaux** : extraits sans donnée personnelle dans [memoire-interne-journal-carte.log](memoire-interne-journal-carte.log).
- **Deux prises de contact TLS en même temps** (météo et géocodage) : calculé, pas essayé sur la carte.
- **Marge de pile de `httpd` en service réel** (page de réglages, géocodage) : non mesurée.
- **Tailles de pile de `serveur_web` (12 288 o, 2,6 Ko utilisés) et `meteo` (12 288 o, 4,5 Ko utilisés)** : réductions possibles, non faites.
- **Niveau d'optimisation** : `CONFIG_COMPILER_OPTIMIZATION_LEVEL_DEBUG=y` (−Og) alourdit le calcul des prises de contact TLS ; un niveau de performance n'a pas été essayé.
- **Autres cartes** (7B, ST7123, ILI9881C) : non essayées. La 7B garde les mêmes réglages dans son fichier de carte à créer.
- **Coupure de courant complète** : non faite, reportée par Axel.

## 12. L'interface LVGL en PSRAM (07/11/2026)

> **Summary (EN)** — LVGL now has its own allocator (`main/lvgl_memoire.cpp`): PSRAM first, internal RAM only when PSRAM is full. Free internal RAM when the time arrives went from 74 kB to 211 kB, and with the settings open from 9.7 kB (the Wi-Fi scan then crashed the board) to 210 kB. **Seen on one board, serial log only**; drawing speed is not compared with the old allocator.

**Vu sur** : Tab5 ST7121, binaire `fix-retours-axel-07-11-base-ac8422c.meridien.bin` (SHA-256 `756A8333…DD936`, 2 834 320 o, 32 % de la partition libres), commits `ce3ab9f` (allocateur) et `f572d9f` (garde du scan), 07/11/2026 vers 9 h. « Avant » : build intégré `953328d`, journaux du 07/11/2026 de 08 h 22 à 08 h 53.

**Pourquoi.** Avec les réglages ouverts il restait 9,7 Ko libres (plus gros bloc 4,6 Ko, minimum 5,5 Ko) : le scan Wi-Fi n'a pas pu créer un sémaphore (`sem create failed`), le pilote d'`esp_hosted` a levé un `assert` (`port_esp_hosted_host_os.c:589`) et la carte a redémarré, deux fois. LVGL utilisait le `malloc` du système, qui range en mémoire interne tout bloc de moins de 256 o (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=256`) : un objet, un style ou une étiquette LVGL pèse 100 à 250 o.

| Étape (ligne `jalon_mem`) | Avant : libre / plus gros bloc / minimum | Après : libre / plus gros bloc / minimum |
| --- | --- | --- |
| après l'écran principal (objets, police, première image) | 173 071 / 60 416 / 136 092 o (coût de l'écran : 69 824 o) | 258 707 / 139 264 / 154 912 o (coût : **96 o**) |
| fin de `app_main` | 130 767 / 36 864 / 98 720 o | 215 471 / 104 448 / 148 660 o |
| heure reçue (IP, SNTP, serveur web en route) | 74 271 / 31 744 / 73 388 o | 211 407 / 92 160 / 148 500 o |
| réglages ouverts, quelques minutes après | 41 087 puis 39 927 / 15 360 / 16 308 o, puis **9 747 / 4 608 / 5 524 o** | 210 375 / 71 680 / 148 500 o (une mesure à 199 843 pendant la navigation, revenue à 210 395) |

Le minimum depuis le démarrage (148 500 o) n'a pas bougé pendant que l'auteur ouvrait les réglages, naviguait et lançait la recherche de réseaux à la main (de 9 h 02 à 9 h 10 environ) : aucun redémarrage, aucune ligne `assert`, `sem create failed` ni `Failed to allocate` dans le journal de la période.

**Ce que cela ne dit pas.**

- La vitesse de dessin n'est pas comparée à l'ancien allocateur (la PSRAM est plus lente que la mémoire interne derrière le cache). Les lignes `tab5_orientation` de la période donnent un rendu moyen de 7,8 à 15 ms par image pendant la navigation, 77 à 79 ms pour l'image fixe de l'écran principal ; elles n'ont pas de pendant « avant » de même durée.
- La perte lente de mémoire interne de §8 (152 o par minute) n'est pas remesurée.
- Le coût réel d'un scan Wi-Fi en mémoire interne n'est pas mesuré : les seuils de la garde (`kScanLibreInterneMin` 16 Ko, `kScanBlocInterneMin` 8 Ko, `main/reseau.cpp`) sont des valeurs de départ, et la garde ne s'est pas déclenchée pendant l'essai.
- Un démarrage à froid (coupure de courant) n'a pas été rejoué avec ce binaire.
