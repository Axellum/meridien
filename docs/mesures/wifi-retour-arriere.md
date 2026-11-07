# Changement de Wi-Fi avec retour arrière — Tab5 (ST7121)

> **Summary (EN)** — Wi-Fi interface wanted by the settings screen (ADR-0007, point 7): scan for networks, try a new one, **automatically go back to the old network** if the new one does not come up, and tell the screen only afterwards. API in `main/reseau_wifi.h` (same names as the settings session's stub), implementation in `main/reseau.cpp`, decisions in a pure core tested on the PC (`ecran/essai_wifi.hpp`, `ecran/points_acces.hpp`: 40 tests, 38 targeted mutations, all caught).
> - How it works: everything is decided in the system event loop (`sys_evt` task), where the Wi-Fi and IP events arrive, so the state machine needs no lock; callers only claim the trial and post a message; no extra task. The trial is bounded by the screen's own 30 s (`DELAI_CONNEXION_MS`); the same *sure* cause (password refused, network not found) seen twice ends it earlier; any other disconnect reason (including the reason 8 we cause ourselves) is neither proof nor denial, and at the 30 s limit the last sure cause seen is reported; after an abandon the roll-back still happens but the callback is not called.
> - **Observed** (Tab5 ST7121, the author's own network, 07/11/2026): the 30 s timer fires at +30 000 ms; the old network is back with its IP address 6.0 to 6.15 s after the decision in **13 roll-backs out of 13** (7 wrong passwords, 3 unknown networks, 3 abandons); an unknown network is reported after 7.2 s, a trial abandoned by the user gives no callback; a scan takes 2.85 s without dropping the connection; a device with no known network starts its Wi-Fi, scans, reports a failed trial at +30.01 s and adopts a good network in 6.1 s. **Surprise**: on this access point a wrong password gives reasons 2 and 205, and the "password refused" reason (202) came in only 1 trial out of 9, so the screen is told "other"; and a failure found only by the 30 s timer reaches the screen at about 36 s, after the screen's own 30 s counter, which ignores it (to be settled on the screen's side, see §8).
> - **Not verified**: the real settings screen calling this interface; a real change to a second working network; open, WPA3, enterprise or hidden networks; a power cut during a trial; persistence of the adopted network (the settings host writes it, this layer only reads it); any other board or C6 version.

**Vu sur** : Tab5, écran ST7121 (contrôleur détecté au démarrage), C6 en micrologiciel 1.4.1 (lu, jamais écrit), `esp_hosted` 2.12.12 côté P4 ; binaires des commits `5f17506` (série 1), `fe54f8d` (série 2) et `a244af0` (série 3, **binaire laissé sur la carte**, arbre propre hors fichiers non suivis ; SHA-256 `163035D2…DC92C`) ; 07/11/2026, entre 02 h 05 et 03 h 01 (heure locale) ; le propre réseau d'Axel (canal 1, sécurisé — le type exact n'est pas relevé —, niveau ≈ −51 dBm), aucun autre Wi-Fi. Aucun nom de réseau, mot de passe, adresse IP ni MAC n'est écrit ici. Entre les séries : `0e44cae` (test seul), `0d6a8de` (commentaires de `reseau_wifi.h`), `06b7afe` et `174b7a4` (les deux corrections de l'appareil neuf, voir §6) ; l'essai « appareil neuf » a été fait avec un binaire **local non commité** (réseau de la compilation ignoré), les autres avec les binaires des commits ci-dessus.

Règle de lecture : ce qui n'est pas marqué « mesuré » est une lecture de source ou de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Question et critères

Question de la coordination (session `reglages-ecran`, décision ADR-0007 point 7) : l'écran de réglages doit pouvoir chercher les réseaux Wi-Fi et en essayer un nouveau **sans jamais couper l'appareil du réseau qui marchait**. Une faute de frappe sur le mot de passe ne doit pas rendre l'écran inatteignable.

Critères de « fait » du lot :

1. l'interface de `reglages-ecran` est fournie sous les mêmes noms (`reseau_scanner_*`, `reseau_connecter`, `reseau_abandonner`, `reseau_ssid_courant`) ;
2. un essai raté ramène l'ancien réseau **avant** que l'écran en soit informé, automatiquement ; les 30 s sont mesurées ;
3. la logique de décision est testée sur le PC, avec des mutations ;
4. un essai avec un mot de passe faux, sur le propre réseau d'Axel, est fait sur la carte et l'appareil revient sur ce réseau ;
5. aucun SSID ni mot de passe n'apparaît dans un journal, un document ou un message.

## 2. Conception

### 2.1 Où ça tourne

Tout se **décide** dans la boucle d'événements du système (tâche `sys_evt`), la même où arrivent `WIFI_EVENT_*` et `IP_EVENT_*`. Les appelants (écran, serveur web) ne font que : vérifier leurs arguments, **réclamer** l'essai sous le verrou court de `reseau.cpp` (un seul essai à la fois), ranger le réseau demandé et **poster un message** à la boucle. Rien n'attend le Wi-Fi. Pas de tâche supplémentaire : une tâche de plus coûterait une pile de 4 à 6 Ko de mémoire interne en permanence ([budget](memoire-interne.md)) ; la pile de `sys_evt` passe à 4 096 o (`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`, +1,8 Ko), valeur de départ à confirmer par la marge mesurée (§6).

La machine à états (`components/core/include/ecran/essai_wifi.hpp`, pure, testée sur le PC) ne fait que décider ; `main/reseau.cpp` exécute (`esp_wifi_set_config`, `esp_wifi_disconnect`, minuteurs, rappel).

### 2.2 Les phases

| Phase | Ce qui s'y passe |
| --- | --- |
| Repos | aucun essai |
| Essai | l'ancien réseau est quitté, le nouveau est essayé ; minuteur de l'essai armé (30 s) |
| Retour | l'essai a échoué ou a été abandonné : l'ancien réseau est remis (`esp_wifi_set_config` puis reconnexion), minuteur du retour armé |

| Événement | Décision |
| --- | --- |
| adresse IP pendant l'essai | **Connecte** : le nouveau réseau devient le réseau courant (il sera l'« ancien » du prochain essai) ; rappel |
| coupure pendant l'essai, raison 8 | ignorée : c'est nous qui quittons l'ancien réseau (le 8 est une cause « autre ») |
| coupure, raison « sûre » vue **deux fois** (la même) | échec : retour arrière ; la cause sera dite à l'écran |
| coupure, cause « autre » (balises perdues, 2, 205…) | ni preuve ni démenti : elle n'arrête jamais l'essai et n'efface rien |
| 30 s écoulées sans adresse IP | échec avec la dernière cause **sûre** vue (sinon « autre ») : retour arrière |
| adresse IP pendant le retour | l'ancien réseau est revenu : rappel avec la cause de l'échec |
| 20 s de retour écoulées | l'écran reprend la main quand même (le Wi-Fi continue de chercher l'ancien réseau) |
| abandon (« Retour », « Annuler », délai de l'écran) | retour arrière sans rappel |
| pas d'ancien réseau (appareil neuf) | échec : rappel tout de suite, et les tentatives sur les mauvais identifiants cessent |

Causes sûres (codes de `wifi_event_sta_disconnected_t.reason`, lus dans les sources d'ESP-IDF 5.5.5) : *mot de passe refusé* = 14, 15, 202, 204 ; *réseau introuvable* = 201, 210, 211, 212 ; tout le reste = *autre*. Le C6 transmet bien le champ `reason` au P4 (lu dans `slave_wifi_std.c` de `esp_hosted`) ; **ce qu'il met dans ce champ pour un mot de passe faux est mesuré au §6**.

### 2.3 Pourquoi deux preuves, pourquoi une cause « autre » ne compte pas, pourquoi 20 s de retour

* Un seul échec isolé (brouillage, balise manquée) ne doit pas condamner un bon mot de passe : la machine exige la **même** cause sûre deux fois (`SEUIL_PREUVES = 2`). Deux causes sûres différentes ne s'additionnent pas.
* Une cause « autre » n'est ni une preuve ni un démenti. Première version de la règle : « deux fois de suite, une cause autre remet à zéro ». **La mesure (§6) l'a réfutée** : pour un mot de passe faux la radio répète 2 et 205 (« autre ») et ne dit 202 (« sûr ») qu'**une fois, à la fin** (+28,9 s) ; la règle stricte ne voyait donc jamais deux preuves de suite et un 205 tardif aurait effacé la preuve vue. Règle retenue : seules les causes sûres comptent, et au délai de 30 s on dit la **dernière cause sûre** vue.
* Pour un réseau introuvable, la règle sert : 201 à +2,8 s puis +7,2 s, échec dit à +7,2 s au lieu de 30 s.
* 20 s pour le retour : valeur de départ ; l'ancien réseau est revenu en 6,0 à 6,15 s dans les 13 retours mesurés (§6), le plafond n'a jamais servi.

### 2.4 Mémoire

Quelques centaines d'octets en mémoire interne (deux structures de 100 o pour le réseau courant et le réseau à l'essai, 864 o pour la dernière liste de réseaux, trois minuteurs), la pile de `sys_evt` +1,8 Ko. La lecture d'une recherche (jusqu'à 64 points d'accès × 130 o) se fait en PSRAM, rendue aussitôt. Le chiffre mesuré est au §6.

## 3. Interface (pour `reglages-ecran`)

`main/reseau_wifi.h`, espace de noms `ecran`, mêmes noms et mêmes types que `main/bouchons/reseau_wifi.h` de la branche `claude/reglages-ecran-aea4e2` ; `PointAcces` vient de `ecran/points_acces.hpp` (même disposition) et `ecran/changement_wifi.hpp` est repris tel quel (même blob).

Ce qui diffère de ce que le bouchon laissait supposer :

* le **rappel** est appelé depuis la tâche d'événements du système (`sys_evt`), pas depuis une « tâche réseau » (qui n'existe plus après le démarrage) : il doit rester court et ne rien attendre ;
* le rappel n'arrive qu'**après le retour de l'ancien réseau** (adresse IP) ou, s'il tarde, 20 s après la décision d'échec ; l'écran qui réessaie tout de suite trouvera donc un réseau prêt ;
* `reseau_connecter` rend aussi faux **tant qu'un essai n'est pas terminé, retour arrière compris** (jusqu'à ≈ 6 s après la décision d'échec, 20 s au plus) : un seul essai à la fois. Un écran dont le compte à rebours de 30 s est fini peut donc se voir refuser un « Réessayer » immédiat ; il doit traiter ce `false` (§8, point 2) ;
* `reseau_connecter` rend faux pour un SSID vide ou de plus de 32 octets, ou un mot de passe de 1 à 7 ou de plus de 63 caractères (WPA : 8 à 63) ;
* `reseau_ssid_courant` rend une chaîne vide pendant un essai et quand l'appareil n'est associé à rien ;
* sans réseau connu au démarrage (ni mémoire permanente, ni compilation), le Wi-Fi **démarre quand même, sans connexion** : la recherche et la saisie à l'écran en ont besoin ;
* au démarrage, `reseau.cpp` lit le réseau retenu dans la NVS (espace `wifi`, clés `ssid` et `mdp`, comme `wifi_stocke.h`) **avant** les identifiants de la compilation. La lecture est directe tant que `main/wifi_stocke.{h,cpp}` de la branche des réglages n'est pas fusionnée ; il suffira alors de la remplacer par `wifi_stocke_lire`.

## 4. Tests du noyau sur le PC (constaté)

`python test/host/lancer.py --seulement test_essai_wifi,test_points_acces` : **22 + 18 tests, 0 échec**, mode « complet » (compilé et exécuté, AddressSanitizer + UBSan). `--mutations` : **38 mutations, toutes détectées**, 0 problème (`test/host/mutations_wifi.py`). Les deux séries de raisons relevées sur la carte (§6) sont rejouées dans `serie_relevee_sur_la_carte_mot_de_passe_faux` et `serie_relevee_sur_la_carte_reseau_introuvable`.

Ce que les tests couvrent : la table des causes ; essai réussi ; mot de passe faux deux fois ; causes différentes qui ne s'additionnent pas ; cause « autre » qui n'arrête jamais l'essai, qui ne casse pas une série de preuves et qui n'efface pas une preuve au délai ; coupure provoquée par nous ignorée (y compris au milieu d'une série) ; coupures au repos et pendant le retour sans effet, même répétées ; délai sans cause, délai avec cause ; sans ancien réseau ; abandon pendant l'essai, pendant le retour, sans ancien ; délai du retour ; un seul essai à la fois ; événements hors phase sans effet ; événement tardif qui ne rappelle pas deux fois ; essai suivant reparti de zéro ; classement des réseaux (cachés, doublons de bornes, tri, liste pleine dans les trois cas, une seule place, SSID de 32 octets, SSID sans zéro final, octets non ASCII, casse).

**Non couvert par le PC** : tout ce qui touche `esp_wifi`, le C6, les minuteurs, la boucle d'événements : voir §6.

## 5. Banc sur la carte (outils provisoires, à retirer avant la phase 2)

L'écran de réglages n'est pas encore fusionné : `main/banc_wifi.{h,cpp}` ajoute des commandes à la console série (`wifi_scan`, `wifi_faux`, `wifi_faux_abandon`, `wifi_inconnu`, `wifi_bon`) qui appellent **l'interface publique** de §3. Elles utilisent le réseau compilé dans le binaire d'essai et un mot de passe faux fixe ; elles n'écrivent jamais un nom de réseau ni un mot de passe : seulement des résultats, des durées, des longueurs, des niveaux et des raisons de coupure.

| Commande | Ce qu'elle fait |
| --- | --- |
| `wifi_scan` | une recherche ; durée, nombre de réseaux, niveau et sécurité des dix premiers, lequel est le réseau courant |
| `wifi_faux` | essai du réseau courant avec un mot de passe faux ; mesure du rappel et du retour de l'adresse IP |
| `wifi_faux_abandon` | même essai, abandonné au bout de 4 s |
| `wifi_inconnu` | essai d'un réseau qui n'existe pas |
| `wifi_bon` | essai du réseau courant avec le bon mot de passe (chemin « Connecte ») |

## 6. Mesures sur la carte

Trois séries sur la même carte, avec le propre réseau d'Axel (aucun autre Wi-Fi n'existe) : la première avec le binaire du commit `5f17506` (règle « deux fois de suite »), la deuxième avec `fe54f8d` (règle finale, §2.3), la troisième avec `a244af0` (même règle, plus les deux corrections de l'appareil neuf `06b7afe` et `174b7a4`, §6 plus bas) ; la série 3 est le **binaire laissé sur la carte**. Les durées sont lues dans le journal série (`+N ms` depuis le début de l'essai) ; « retour » = temps entre la décision d'échec et l'adresse IP de l'ancien réseau. Journaux bruts conservés hors dépôt (ils contiennent l'adresse IP locale) ; extraits nettoyés (couche réseau et banc seulement, adresses masquées) dans [wifi-retour-arriere-journal-carte.log](wifi-retour-arriere-journal-carte.log).

| Essai | Série | Décision d'échec | Rappel donné à l'écran | Résultat dit | Retour de l'ancien réseau |
| --- | --- | --- | --- | --- | --- |
| mot de passe faux | 1 (`5f17506`) | délai, **+30 000 ms** | +36 112 ms | mot de passe refusé (le 202 est venu à +28 944 ms) | adresse IP **6,0 s** après la décision |
| mot de passe faux | 2 (`fe54f8d`) | délai, **+30 000 ms** | +36 095 ms | **autre** (aucun 202 avant le délai) | adresse IP **6,0 s** après la décision |
| mot de passe faux, 4 répétitions de suite | 2 (`fe54f8d`) | délai, **+30 000 ms** (4 fois) | +36 099, +36 139, +36 092, +36 113 ms | **autre** (4 fois, aucun 202) | adresse IP 6,1 à 6,15 s après la décision (4 fois) |
| mot de passe faux | 3 (`a244af0`) | délai, **+30 000 ms** | +36 128 ms | **autre** (aucun 202) | adresse IP 6,15 s après la décision |
| réseau inexistant | 1 | **+7 165 ms** (2 × raison 201) | +13 194 ms | réseau introuvable | 6,0 s |
| réseau inexistant | 2 | **+7 164 ms** (2 × raison 201) | +13 176 ms | réseau introuvable | 6,0 s |
| réseau inexistant | 3 | **+7 163 ms** (2 × raison 201) | +13 199 ms | réseau introuvable | 6,0 s |
| bon mot de passe (même réseau) | 1 | — | +6 039 ms | connecté | adresse IP à +6 049 ms |
| bon mot de passe (même réseau) | 2 | — | +6 048 ms | connecté | adresse IP à +6 049 ms |
| bon mot de passe (même réseau) | 3 | — | +6 065 ms | connecté | adresse IP à +6 099 ms |
| mot de passe faux, abandon à 4 s | 1 | abandon, +4 049 ms | **aucun** (voulu) | — | adresse IP à +10 146 ms (6,1 s après l'abandon) |
| mot de passe faux, abandon à 4 s | 2 | abandon, +4 049 ms | **aucun** (voulu) | — | adresse IP à +10 149 ms |
| mot de passe faux, abandon à 4 s | 3 | abandon, +4 049 ms | **aucun** (voulu) | — | adresse IP à +10 149 ms |

Suites de raisons de coupure relevées (la raison 8 est notre propre coupure de l'ancien réseau) :

* mot de passe faux, série 1 : 8 → 2 (+6 960 ms) → 205 (+11 373) → 2 (+19 996) → 205 (+24 410) → **202 (+28 944)** → délai ;
* mot de passe faux, séries 2 et 3 (6 essais) : 8 → 2 (+6 958) → 205 (+11 371) → 2 (+19 991) → 205 (+24 404) → délai (**pas de 202**), à 10 ms près les six fois ;
* réseau inexistant : 8 → 201 (+2 751) → 201 (+7 165) → retour arrière.

Ce que ça montre :

1. **Les 30 s sont exactes** : le minuteur de l'essai a tiré à +30 000 ms (journal : « délai de 30 s écoulé à +30000 ms ») les sept fois où un mot de passe faux a été essayé avec un ancien réseau (et les deux fois sans), et rien n'a été décidé avant (aucune cause sûre ne s'est répétée).
2. **Le retour arrière marche à chaque fois** : l'adresse IP de l'ancien réseau est revenue 6,0 à 6,15 s après la décision d'échec, **7 essais « mot de passe faux », 3 « réseau inexistant » et 3 abandons, soit 13 retours sur 13** ; le plafond de 20 s n'a jamais servi ; le journal de l'écran principal montre l'heure et la météo à jour après chaque série (lu dans le journal, **l'écran lui-même n'a pas été regardé par une personne**).
3. **La cause « mot de passe refusé » n'est presque jamais disponible sur ce point d'accès** : sur 9 essais avec un mot de passe faux (7 avec un ancien réseau, 2 sans), le 202 n'est venu qu'**une fois** (à +28,9 s, série 1) ; les 8 autres fois la radio n'a dit que 2 et 205, deux raisons que la documentation d'ESP-IDF 5.5.5 décrit seulement par « authentification expirée » et « connexion échouée » : elles ne prouvent rien (§2.3, [dépannage](../depannage.md#wi-fi)). Le résultat dit alors « autre » : choix de ne pas deviner (§8, point 3).
4. **Pour l'écran, le rappel d'un échec détecté seulement par le délai arrive à ≈ 36 s**, après son propre compte à rebours de 30 s : voir §8, point 2.
5. Le chemin « abandon » ne rappelle jamais l'écran (voulu) et reprend quand même l'ancien réseau, avec la même durée de retour (≈ 6 s après l'abandon).

### Recherche de réseaux

| Situation | Durée | Coupures pendant la recherche | Liste |
| --- | --- | --- | --- |
| connecté au réseau d'Axel (3 séries) | 2 849 ms | 0, toujours connecté après | 2 points d'accès, **1 réseau listé** (doublon retiré, le plus fort gardé, ≈ −51 dBm, sécurisé) |
| sans réseau connu (appareil neuf, 2 séries) | 2 416 à 2 449 ms | — | idem |

La recherche ne coupe donc pas la connexion en service. Aucun nom de réseau n'est journalisé (longueur, niveau, « sécurisé », « c'est le nôtre »).

### Appareil neuf (aucun réseau connu au démarrage)

Binaire **local, non commité**, qui ignore le réseau de la compilation (simule la NVS vide) ; le reste du code est celui du commit `06b7afe`. Série du 07/11/2026 vers 02 h 50.

| Étape | Constaté |
| --- | --- |
| démarrage | « réseau Wi-Fi de départ : aucun », ligne `BOOT` imprimée tout de suite, « Wi-Fi démarré sans réseau connu : pas de connexion » |
| recherche | 2 449 ms, 1 réseau listé, **aucune tentative de connexion ensuite** (avant le correctif `06b7afe` : 11 coupures « raison 201 » en 45 s) |
| essai avec mot de passe faux | pas d'ancien réseau ; suite 2, 205, 2, 205 ; délai à +30 000 ms ; **rappel à +30 013 ms** (résultat « autre ») ; ensuite plus aucune tentative |
| essai avec le bon mot de passe | connecté et adresse IP à +6 126 ms ; `reseau_ssid_courant` rend le nom (même longueur et même texte que celui compilé) ; SNTP démarré, heure reçue, écran principal « 02:51 » affiché |
| recherche une fois connecté | 2 849 ms, 0 coupure |

### Pile et mémoire

* Pile de `sys_evt` (4 096 o) : **2 512 o libres au minimum**, soit un pic de 1 584 o, relevé après 4 essais, 1 recherche et le démarrage (commande `piles`, série 2).
* Mémoire interne libre : 145 595 o (`5f17506`) et 145 611 o (`fe54f8d`) après le serveur web, stable d'un démarrage à l'autre ; 122 215 o à 631 s (série 1), 121 919 o à 481 s (série 2) et 123 543 o à 306 s (série 3) ; **minimum 94 812 o dans les trois séries**, atteint pendant la prise de contact TLS du démarrage et non pendant les essais. Le coût de la couche Wi-Fi par rapport au binaire précédent n'est pas isolé dans cette page ; la fuite connue de 152 o par minute d'horloge reste ouverte ([mémoire interne](memoire-interne.md)).

### Secrets

Le nom et le mot de passe du réseau d'Axel (variables `MERIDIEN_WIFI_SSID` et `MERIDIEN_WIFI_MOT_DE_PASSE` du `.env`) sont cherchés dans les journaux bruts par `banc_1b.py secrets` : **0 occurrence** dans les journaux des trois séries, des quatre essais supplémentaires de la série 2 et des deux essais de l'appareil neuf. Aucun chien de garde, `Guru`, `abort`, `Backtrace` ni échec d'allocation dans ces mêmes journaux (recherche par motif). Le journal brut contient l'adresse IP locale (ligne de lwIP) : il n'est pas dans le dépôt.

## 7. Ce qui n'est pas vérifié

* **L'écran de réglages réel** n'a jamais appelé cette interface (sa branche n'est pas fusionnée) : seules les commandes série du banc (§5) l'ont fait. Le texte affiché, le compte à rebours de l'écran et le rappel depuis `sys_evt` ne sont donc pas vus ensemble.
* **Un vrai changement de réseau**, avec un second Wi-Fi qui marche : non essayé (il n'existe pas, et on ne coupe jamais le réseau d'Axel). `wifi_bon` rejoue le **même** réseau avec le bon mot de passe : il prouve le chemin « Connecte » et la reprise de l'adresse IP, pas le passage d'un réseau à un autre. L'adoption du nouveau réseau comme réseau courant (`ssid_courant`) n'est donc vue que pour le même nom.
* **Les autres cas du monde réel** : réseau ouvert (mot de passe vide, accepté par l'API), WPA3, réseau d'entreprise, réseau caché, nom de réseau accentué ou de 32 octets, deux bornes pour le même nom : non essayés (le classement des doublons et des octets non ASCII est testé sur le PC seulement).
* **La lecture de la NVS au démarrage** (`lire_reseau_retenu`) : seul le cas « espace `wifi` absent » a été vu (les démarrages normaux retombent sur le réseau de la compilation). Un réseau réellement écrit dans la NVS par l'écran de réglages n'a jamais été lu ; l'« appareil neuf » est simulé par un binaire local, pas par une NVS réellement vide.
* **La mémorisation du réseau retenu** : `reseau.cpp` ne fait que **lire** la NVS (`nvs_open` en lecture seule, espace `wifi`) ; écrire le réseau adopté est le rôle de l'écran de réglages (`wifi_stocke`). Un redémarrage après un changement réussi ne retrouve le nouveau réseau que si l'écran l'a écrit : non vérifié, car le banc n'écrit rien.
* **Coupure du courant ou redémarrage pendant un essai** : non essayés. Comme l'ancien réseau reste dans la NVS tant que l'écran n'a pas écrit le nouveau, la carte devrait revenir sur l'ancien, mais cela n'a pas été vu.
* **Le C6 autre que 1.4.1** : non essayé (le C6 n'est jamais mis à jour, ADR-0007 ; `esp_hosted` est épinglé).
* **La Waveshare 7B** : aucune. Statut « prévu ».
* **Le plafond de 20 s du retour** n'a jamais servi : l'ancien réseau est revenu en 6,0 à 6,15 s à chacun des 13 retours. Le chemin « le retour traîne » est testé sur le PC (`delai_du_retour_rend_la_main_quand_meme`), pas sur la carte.

## 8. Décisions prises à la place d'Axel (à valider)

Axel dort et ne répond pas ; chaque décision ci-dessous se défait en une ligne.

1. **Seuil de deux preuves (`SEUIL_PREUVES = 2`) et plafond de retour de 20 s** : valeurs de départ choisies par prudence, puis confirmées par la mesure (§6). Pour les changer : `essai_wifi.hpp`, un nombre.
2. **Le rappel de l'écran arrive après le retour de l'ancien réseau** (≈ 6 s après la décision d'échec), pas à la décision. Raison : l'écran qui réessaie ou qui ouvre une page doit trouver un réseau prêt. **Contrepartie, à régler côté écran** : l'écran a son propre compte à rebours de 30 s (`DELAI_CONNEXION_MS`) ; passé ce délai il affiche lui-même « sans réponse » (`Cause::Delai`) et **ignore** tout résultat tardif (`sur_resultat`, `changement_wifi.hpp`). Or tout échec que seul le délai détecte (c'est le cas de 5 mots de passe faux sur 6 ici) m'arrive à +30 s et le rappel à ≈ +36 s : l'écran n'en saura rien, et `reseau_connecter` lui rendra faux (essai pas terminé) pendant ces ≈ 6 s s'il propose « Réessayer » tout de suite. Trois issues, je n'en ai appliqué aucune pour ne pas toucher le contrat de l'écran (`changement_wifi.hpp` est repris tel quel) : **(a)** un état « retour à l'ancien réseau » côté écran, qui attend le rappel après les 30 s et traite le `false` de `reseau_connecter` ; **(b)** un délai d'essai plus court que celui de l'écran (20 s : rappel à ≈ 26 s ; un Wi-Fi qui marche s'est connecté en 6 s ici, mais un point d'accès lent serait coupé) ; **(c)** ne rien changer et accepter le message générique. Recommandation : (a).
3. **Une cause « autre » ne compte ni pour ni contre** (§2.3) : décidée par la mesure de la carte, pas par la théorie. Alternative à valider : classer 2 et 205 répétés comme « mot de passe refusé » (une ligne dans `resultat_pour_raison`) ; non fait, car mesuré sur un seul point d'accès et non documenté par ESP-IDF.
4. **Le Wi-Fi démarre même sans réseau connu** (appareil neuf, NVS vide, rien dans la compilation) : sans cela l'écran ne pourrait ni chercher ni saisir un réseau. Il ne se connecte à rien et ne journalise pas d'identifiant.
5. **La NVS est lue avant les identifiants de la compilation** : le réseau retenu par la personne l'emporte sur celui du binaire de développement. Remplacer par `wifi_stocke_lire` quand la branche des réglages sera fusionnée.
6. **Pile de `sys_evt` à 4 096 o** (`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`, +1,8 Ko de mémoire interne) : pic mesuré 1 584 o (§6). 3 072 o suffiraient avec une marge de 1,4 Ko ; je garde 4 096 tant que le rappel de l'écran s'exécute dans cette tâche (on ne sait pas encore ce qu'il fera).
7. **Pas d'ADR** : un ADR est un numéro commun à toutes les sessions (risque de collision) ; la décision est consignée ici. À promouvoir en ADR par la coordination si elle le souhaite.
8. **Mot de passe : 8 à 63 caractères (ou vide)** accepté par l'API publique ; la saisie de 64 caractères hexadécimaux (clé PSK brute) n'est acceptée que pour un réseau de la compilation.
9. **`banc_wifi.*` et la configuration de suivi des tâches restent le temps de la phase 1b** ; à retirer avant la phase 2.
