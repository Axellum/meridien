# API de réglages — contrat REST de l'appareil

Rédigé le 06/11/2026 (piste B). **Contrat de conception : aucune route n'existe encore sur l'appareil.** Le front et le faux serveur de `web/` l'implémentent ; le firmware (phase 3) devra le respecter tel quel. Tout ce qui est écrit ici comme comportement de l'appareil est **non vérifié** tant qu'il n'a pas été constaté sur une carte.

Décisions d'origine : [ADR-0002](decisions/0002-assistant-vocal-en-cascade.md) (trois services), [ADR-0003](decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md) (page servie par l'appareil, HTTP local, jeton d'appairage, secrets, mise à jour).

## 1. Règles qui valent pour toutes les routes

1. **Une clé API n'est jamais lisible.** Elle n'entre dans l'appareil que par le corps d'une requête `PUT` ou `POST` (jamais dans l'adresse), et n'en sort **jamais** : ni dans une réponse, ni dans l'export, ni dans un message d'erreur, ni dans le journal. Les réponses disent seulement `"cle_definie": true|false`. Aucun masque partiel (« …a3f9 ») : même les derniers caractères ne sont pas renvoyés. Le mot de passe Wi-Fi suit la même règle.
2. **Corps de réponse d'un service tiers jamais recopié.** Un service qui répond « clé invalide : sk-abc…xyz » peut citer la clé. L'appareil traduit le statut HTTP reçu en une phrase à lui (§8), il ne transmet pas le texte reçu.
3. **JSON UTF-8** (`Content-Type: application/json; charset=utf-8`), accents et « ° » tels quels. `Cache-Control: no-store` sur tout `/api/`. Noms de champs en français sans accent (`mot_de_passe`, `luminosite`), valeurs techniques (`fuseau`, `latitude`) inchangées.
4. **Même origine seulement.** Pas d'en-tête CORS : la page est servie par l'appareil. (Le faux serveur de développement peut l'ajouter.)
5. **Erreurs = une phrase courte en français**, jamais un code brut ni une trace. Format unique, voir §9. La page n'affiche que `message`.
6. **Authentification** : toutes les routes sauf `GET /api/status` et `POST /api/pair` exigent `Authorization: Bearer <jeton de session>` (§2). Sans cela : `401`.
7. **Opérations longues = asynchrones** (`202` puis lecture de l'état) parce que `esp_http_server` traite les requêtes dans une seule tâche par défaut : un « tester » de 15 s ne doit pas figer la page de l'appareil. Concerne le scan Wi-Fi, la connexion Wi-Fi, les tests de service et la mise à jour.
8. **Limites** : corps JSON ≤ 4 Ko (import ≤ 16 Ko) ; 4 clients simultanés au plus (au-delà : `503`, « L'appareil est occupé, réessayez dans un instant. »).

## 2. Appairage et session

Le jeton d'appairage est un **code à 6 chiffres affiché sur l'écran de l'appareil** (ADR-0003). La page le demande une fois, l'échange contre un jeton de session et le garde en `sessionStorage` (jamais dans l'adresse).

| Route | Rôle |
|---|---|
| `GET /api/status` | Public. `{"nom","modele","version","mode","appairage"}` : `mode` = `station` (sur le Wi-Fi de la maison) ou `point_d_acces` (mise en service) ; `appairage` = `"requis"` ou `"ok"` selon que la requête porte un jeton valide. Rien d'autre (pas d'adresse IP, pas de SSID). |
| `POST /api/pair` | Corps `{"code":"123456"}` → `200 {"jeton":"<32 octets hex>"}`. Mauvais code → `403`, « Ce code n'est pas le bon. Regardez l'écran de l'appareil. ». **5 essais ratés → verrou 60 s** (`429`, « Trop d'essais. Réessayez dans 60 secondes. », avec `reessayer_dans`). |
| `DELETE /api/pair` | Ferme la session courante → `204`. |

Le jeton de session vit en RAM (4 sessions au plus, la plus ancienne est fermée si une cinquième arrive) et disparaît au redémarrage : la page redemande alors le code. Le code change au redémarrage et quand on touche « Nouveau code » sur l'écran. **Limite assumée** : HTTP en clair sur le réseau local, le jeton peut être lu par quiconque écoute ce réseau (HTTPS auto-signé écarté, ADR-0003).

## 3. Appareil

`GET /api/system` →
```json
{
  "nom": "ecran-8e87",
  "modele": "M5Stack Tab5",
  "version": "0.1.0",
  "compile_le": "2026-11-06T11:20:00Z",
  "partition": "ota_0",
  "validee": true,
  "depuis_secondes": 5421,
  "memoire_libre": 5120000,
  "heure": {"epoch": 1793960400, "synchronisee": true, "fuseau": "Europe/Paris"},
  "internet": true
}
```
`validee` est faux tant que la nouvelle version n'a pas passé son auto-test (§7). `internet` = dernière requête sortante réussie (météo ou test).

## 4. Wi-Fi

| Route | Rôle |
|---|---|
| `GET /api/wifi` | État courant. |
| `POST /api/wifi/scan` | Lance une recherche de réseaux → `202 {"en_cours":true}`. |
| `GET /api/wifi/scan` | `{"en_cours":false,"reseaux":[{"ssid":"Maison","niveau":4,"force":-55,"securise":true}]}` ; `niveau` 0 à 4 (affichage), `force` en dBm, triés du plus fort au plus faible, doublons fusionnés. |
| `PUT /api/wifi` | Corps `{"ssid":"Maison","mot_de_passe":"…"}` (mot de passe vide permis pour un réseau ouvert) → `202 {"etat":"connexion"}`. |

`GET /api/wifi` →
```json
{"etat": "connecte", "ssid": "Maison", "niveau": 3, "force": -61, "ip": "192.168.0.52",
 "message": "Connecté à « Maison »."}
```
`etat` ∈ `connecte`, `connexion`, `echec`, `deconnecte`, `point_d_acces`. Après `PUT`, la page lit cet état toutes les 1,5 s jusqu'à `connecte` ou `echec` (20 s au plus). Sur `echec`, l'appareil **revient au réseau précédent** (ou au point d'accès de mise en service) et le dit : « Mot de passe refusé par « Maison ». L'ancien réseau est conservé. » **Cas du point d'accès** : en changeant de réseau, le téléphone perd l'appareil. Le `202` porte alors `"adresse_apres": "http://ecran-8e87.local"` et la page affiche « Reconnectez votre téléphone à votre Wi-Fi, puis ouvrez cette adresse. » (portail captif et mDNS : non démontrés sur P4+C6, voir plan, jalon 1d).

## 5. Zone météo et fuseau

| Route | Rôle |
|---|---|
| `GET /api/geocode?q=Lyon` | Recherche de lieu par un **proxy du firmware** vers le géocodage d'Open-Meteo. |
| `GET /api/location` | Zone enregistrée. |
| `PUT /api/location` | Enregistre la zone choisie. |
| `GET /api/weather` | Aperçu de la météo de la zone enregistrée (pour que la page montre « ça marche »). |

**Recherche.** `q` : **3 caractères au moins** après `trim` (sinon `400`, « Tapez au moins 3 lettres. »), 80 au plus, en UTF-8 encodé pour l'adresse. Le firmware appelle `geocoding-api.open-meteo.com/v1/search` avec `name=<q>`, `count=8`, `language=fr`, `format=json` (paramètres et champs lus le 06/11/2026 dans open-meteo.com/en/docs/geocoding-api : `count` 10 par défaut et 100 au plus ; la réponse met les lieux dans `results` avec `name`, `admin1`, `country`, `country_code`, `latitude`, `longitude`, `elevation`, `timezone`, `population` ; les champs vides sont omis ; erreur = JSON `error`/`reason` en `400`. Le firmware les renomme comme ci-dessous) et renvoie une liste réduite :
```json
{"resultats": [
  {"id": "2996944", "nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR",
   "latitude": 45.74846, "longitude": 4.84671, "altitude": 173, "fuseau": "Europe/Paris",
   "population": 522250, "libelle": "Lyon, Auvergne-Rhône-Alpes, France"}
]}
```
Liste vide → `200 {"resultats":[]}` (la page dit « Aucun lieu trouvé pour « Lyonn ». »). Sans Internet côté appareil → `503`, « L'appareil n'a pas accès à Internet pour chercher un lieu. ». Les valeurs ci-dessus sont des **exemples de forme**, pas des données lues. Un résultat sans `fuseau` est écarté de la liste.

**Enregistrement.** `PUT /api/location` reçoit un résultat de recherche tel quel (champs `nom`, `region`, `pays`, `code_pays`, `latitude`, `longitude`, `fuseau`; `altitude` facultatif) :
- `latitude` ∈ [−90, 90], `longitude` ∈ [−180, 180] (sinon `422`, « Ces coordonnées ne sont pas valides. ») ;
- `fuseau` doit être un **nom IANA présent dans la table embarquée** (`posix_tz_db`, noyau pur) ; sinon `422`, « Ce fuseau horaire n'est pas connu de l'appareil. » ;
- l'appareil garde le **nom IANA** (en NVS, pour retrouver l'heure sans réseau) et en déduit la chaîne POSIX, applique `TZ`, relance la météo, puis répond avec la zone relue :
```json
{"nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR",
 "latitude": 45.74846, "longitude": 4.84671, "fuseau": "Europe/Paris",
 "posix": "CET-1CEST,M3.5.0,M10.5.0/3", "libelle": "Lyon, Auvergne-Rhône-Alpes, France"}
```
`GET /api/location` renvoie la même forme, ou `{"definie": false}` tant qu'aucune zone n'est choisie (l'écran dit alors « Choisissez une ville dans la page de réglages. »).

**Aperçu météo.** `GET /api/weather` →
```json
{"disponible": true, "perimee": false, "mise_a_jour": 1793960400,
 "actuelle": {"temperature": 14.2, "ressentie": 13.1, "code": 2, "libelle": "Partiellement nuageux", "jour": true}}
```
`code` = code WMO d'Open-Meteo, `libelle` = libellé court français du noyau pur (table WMO), jamais le code seul. `perimee` vrai quand la dernière lecture a plus de 3 h ; `{"disponible": false}` avant la première lecture réussie.

## 6. Affichage

`GET /api/display` →
```json
{"langue": "fr", "theme": "auto", "luminosite": 80,
 "langues": [{"code": "fr", "nom": "Français"}],
 "themes": ["auto", "clair", "sombre"]}
```
`PUT /api/display` accepte les trois champs modifiables (`langue`, `theme`, `luminosite`), chacun facultatif (absent = inchangé) ; `langues` et `themes` sont en lecture seule : **la liste des langues vient de l'appareil**, la page ne la fixe pas. `luminosite` : entier 5 à 100 (en %) ; le plancher à 5 évite un écran rendu illisible depuis la page (sinon `422`, « La luminosité doit être entre 5 et 100 %. »). Les changements s'appliquent à l'écran dès la réponse.

## 7. Services STT / LLM / TTS

Trois services **indépendants** (ADR-0002), identifiés par `stt`, `llm`, `tts`, chacun compatible OpenAI, local ou cloud. Routes appelées par l'appareil sous l'URL de base : `…/audio/transcriptions` (STT), `…/chat/completions` en streaming (LLM), `…/audio/speech` (TTS), plus `…/models` pour le test.

| Route | Rôle |
|---|---|
| `GET /api/presets` | Préréglages **fournis par l'appareil** (la page n'en code aucun). |
| `GET /api/services` | Les trois services enregistrés, sans clé. |
| `PUT /api/services/{stt\|llm\|tts}` | Enregistre un service. |
| `POST /api/services/{id}/test` | Lance un test → `202`. |
| `GET /api/services/{id}/test` | Résultat du dernier test. |

`GET /api/presets` → `{"stt": [Preset…], "llm": […], "tts": […]}` avec
```json
{"id": "lmstudio", "nom": "LM Studio (sur mon PC)", "url": "http://localhost:1234/v1", "local": true,
 "cle_requise": false, "modele": "", "aide": "Lancez le serveur local de LM Studio sur le PC."}
```
`local: true` marque un service qui tourne sur le PC de l'utilisateur : la page rappelle alors de remplacer `localhost` par l'adresse du PC (rien n'est bloqué, c'est un conseil).
`id` ∈ `openai`, `google`, `lmstudio`, `ollama`, `kokoro`, `personnalise` (ADR-0002 + `kokoro`) ; **un préréglage n'existe que pour les services que son fournisseur documente** :

| Préréglage | Adresse de base | Services | Source (lue le 06/11/2026) |
|---|---|---|---|
| `openai` | `https://api.openai.com/v1` | stt, llm, tts | developers.openai.com/api/reference (routes `/models`, `/chat/completions`, `/audio/transcriptions`, `/audio/speech`) |
| `google` | `https://generativelanguage.googleapis.com/v1beta/openai/` | **llm seulement** | aistudio.google.com/docs/openai : `/models` et `/chat/completions` documentés ; **`/audio/transcriptions` et `/audio/speech` non documentés** (écart avec l'ADR-0002, voir le rapport de piste B) |
| `lmstudio` | `http://localhost:1234/v1` (à remplacer par l'adresse du PC) | llm | lmstudio.ai/docs/developer/openai-compat : aucune route audio listée ; le port 1234 vient des exemples de la doc, « défaut officiel » non vérifié |
| `ollama` | `http://localhost:11434/v1` (à remplacer par l'adresse du PC) | llm, stt, tts | docs.ollama.com/api/openai-compatibility : `/v1/audio/transcriptions` et `/v1/audio/speech` listées ; quels modèles les servent : non vérifié |
| `kokoro` | `http://localhost:8880/v1` | tts | README de remsky/Kokoro-FastAPI (source unique) |
| `personnalise` | libre | stt, llm, tts | |

`localhost` n'a aucun sens depuis l'écran : la page demande l'adresse du PC (ex. `192.168.0.20`) et la substitue avant d'enregistrer. Aucun nom de modèle n'est proposé de mémoire : le champ `modele` reste vide et `modeles` du test (§ ci-dessous) propose la liste réelle du service. Ajout par rapport à l'ADR-0002 : le préréglage `kokoro` (voix française locale, recherche 02) existe parce que c'est le TTS local prévu.

`GET /api/services` →
```json
{"llm": {"preset": "lmstudio", "url": "http://192.168.0.20:1234/v1", "modele": "…",
         "cle_definie": false, "configure": true},
 "stt": {"preset": "openai", "url": "…", "modele": "…", "langue": "fr", "cle_definie": true, "configure": true},
 "tts": {"preset": "personnalise", "url": "…", "modele": "…", "voix": "…", "cle_definie": false, "configure": true}}
```
`langue` n'existe que pour `stt` (indice de langue de la transcription), `voix` que pour `tts`. Un service jamais réglé : `{"configure": false, "cle_definie": false}`.

`PUT /api/services/{id}` : corps = les champs à changer.

| Champ | Règle |
|---|---|
| `preset` | l'un des `id` ci-dessus ; |
| `url` | `http://` ou `https://`, sans identifiant dans l'adresse (`user:mdp@` refusé : « Retirez l'identifiant de l'adresse : la clé se saisit à part. »), 200 caractères au plus, le `/` final est retiré ; |
| `modele`, `voix`, `langue` | texte court (80 caractères) ; |
| `cle` | **écriture seule.** Présent et non vide = remplace la clé ; **absent = clé inchangée** ; |
| `effacer_cle` | `true` = supprime la clé enregistrée. |

Réponse : le service relu (même forme que `GET`), donc **sans** `cle`. Changer `preset` ne conserve pas la clé du préréglage précédent : elle est effacée (une clé OpenAI ne doit jamais partir vers un autre serveur), sauf si le même corps en apporte une nouvelle.

**Test.** `POST /api/services/{id}/test` accepte un corps facultatif `{"url","modele","voix","langue","cle"}` : il essaie ces valeurs **sans les enregistrer** (la page teste ce qui est dans le formulaire) ; un champ absent prend la valeur enregistrée, `cle` compris. Le test fait, dans l'ordre :
1. **Joindre** : `GET {url}/models` (une réponse `404`/`405` compte comme « joint, mais ne liste pas ses modèles » : service local valide) ;
2. **Essai réel minimal** : LLM = un échange d'un jeton ; TTS = synthèse de « Bonjour » (la durée et la taille reçues sont rapportées, rien n'est joué) ; STT = transcription d'une demi-seconde de silence.

`GET /api/services/{id}/test` →
```json
{"etat": "termine", "ok": true, "duree_ms": 412,
 "message": "Le service répond en 0,4 s.",
 "etapes": [{"nom": "Joindre le service", "ok": true},
            {"nom": "Essai de réponse", "ok": true}],
 "modeles": ["…", "…"]}
```
`etat` ∈ `inactif`, `en_cours`, `termine` ; en échec, `ok:false` et `message` donne **la cause et le geste** (§8) : « Le service refuse la clé. Vérifiez-la. » ; « Le service ne répond pas. Est-il lancé et sur le même réseau ? » ; « Ce modèle n'existe pas sur ce service. » `modeles` (jusqu'à 50 noms, tels que renvoyés par `…/models`) permet à la page de proposer une liste plutôt qu'un champ à deviner. Le résultat du dernier test est gardé en RAM jusqu'au suivant.

## 8. Phrases d'erreur d'un service tiers (cause → message)

| Ce que l'appareil observe | Message |
|---|---|
| connexion refusée / délai dépassé | « Le service ne répond pas. Est-il lancé et sur le même réseau ? » |
| nom d'hôte introuvable | « Cette adresse est introuvable. » |
| certificat TLS refusé | « Le certificat de ce service n'est pas reconnu. » |
| `401`, `403` | « Le service refuse la clé. Vérifiez-la. » |
| `404` sur la route de l'**essai réel** (étape 2 du test, §7 : `chat/completions`, `audio/speech` ou `audio/transcriptions`) ; un `404`/`405` sur `…/models` (étape 1) n'est **pas** une erreur | « Cette adresse ne ressemble pas à un service compatible. Il manque peut-être « /v1 ». » |
| `404`/`400` mentionnant le modèle | « Ce modèle n'existe pas sur ce service. » |
| `429` | « Le service limite les appels. Réessayez dans un instant. » |
| `5xx` | « Le service a un problème de son côté. » |
| réponse non conforme (pas du JSON, pas d'audio) | « Le service a répondu autre chose que prévu. » |

## 9. Format d'erreur

`4xx`/`5xx` : `{"erreur": {"id": "jeton_invalide", "message": "Ce code n'est pas le bon.", "champ": "code"}}`. `message` est affiché tel quel. `id` est un identifiant stable pour les tests et la mise au point, **jamais affiché**. `champ` (facultatif) désigne le champ fautif pour que la page le surligne.

| Statut | Quand | Message type |
|---|---|---|
| `400` | requête mal formée | « Cette demande n'est pas lisible. » |
| `401` | pas de jeton valide | « Entrez le code affiché sur l'écran de l'appareil. » |
| `403` | mauvais code | « Ce code n'est pas le bon. Regardez l'écran de l'appareil. » |
| `404` | route inconnue | « Cette page n'existe pas. » |
| `413` | corps trop gros | « Ce fichier est trop gros pour l'appareil. » |
| `422` | valeur refusée | une phrase par champ (§5, §6, §7) |
| `429` | verrou de l'appairage | « Trop d'essais. Réessayez dans 60 secondes. » |
| `500` | défaut interne | « L'appareil a rencontré un problème. Réessayez. » |
| `503` | occupé / pas d'Internet | « L'appareil est occupé, réessayez dans un instant. » |

## 10. Export et import des réglages

| Route | Rôle |
|---|---|
| `GET /api/settings/export` | Fichier JSON à télécharger (`Content-Disposition: attachment; filename="reglages-ecran-AAAA-MM-JJ.json"`). |
| `POST /api/settings/import?simuler=1` | Vérifie le fichier et dit ce qui changerait, **sans rien appliquer**. |
| `POST /api/settings/import` | Applique. |

Contenu de l'export : zone, affichage, services — **jamais** de clé, ni de mot de passe Wi-Fi (le Wi-Fi n'y figure pas du tout : un autre réseau demande son propre mot de passe) :
```json
{"format": "ecran-autonome-reglages", "version": 1, "exporte_le": "2026-11-06T12:00:00Z",
 "zone": {"nom": "Lyon", "…": "…", "fuseau": "Europe/Paris"},
 "affichage": {"langue": "fr", "theme": "auto", "luminosite": 80},
 "services": {"llm": {"preset": "lmstudio", "url": "…", "modele": "…"}},
 "cles_a_ressaisir": ["stt"]}
```
`cles_a_ressaisir` liste les services qui avaient une clé : la page dit « Pensez à ressaisir la clé de : voix → texte. ».

Import : le fichier passe par les **mêmes validations** que les `PUT` ci-dessus, champ par champ ; un fichier d'un autre `format` ou d'une `version` plus récente → `422`, « Ce fichier ne vient pas de cet écran. ». Réponse (simulation ou non) :
```json
{"simulation": true, "appliques": ["zone", "affichage", "services.llm"],
 "ignores": [{"cle": "services.stt.cle", "message": "Les clés ne s'importent pas."}],
 "erreurs": [], "cles_a_ressaisir": ["stt"]}
```
Un champ `cle`, `mot_de_passe` ou équivalent dans le fichier est **ignoré et signalé**, jamais enregistré. Toutes les erreurs de validation sont rapportées et rien n'est appliqué si `erreurs` n'est pas vide (tout ou rien).

## 11. Mise à jour par la page, avec retour arrière

| Route | Rôle |
|---|---|
| `GET /api/update` | État de la mise à jour. |
| `POST /api/update` | Corps = image binaire du firmware (`Content-Type: application/octet-stream`, `Content-Length` obligatoire). |
| `POST /api/update/rollback` | Revient à la version précédente. |

`GET /api/update` →
```json
{"etat": "inactif", "progression": 0, "version": "0.1.0", "validee": true,
 "precedente": {"version": "0.0.9", "disponible": true},
 "dernier_resultat": {"issue": "validee", "version": "0.1.0", "message": "Mise à jour réussie."}}
```
`etat` ∈ `inactif`, `envoi`, `verification`, `redemarrage`, `echec`; `progression` en % de l'envoi. `dernier_resultat.issue` ∈ `validee`, `annulee` (retour arrière automatique : « La nouvelle version n'a pas démarré correctement : l'ancienne a été remise. »), `refusee`.

`POST /api/update` : refuse avant d'écrire si `Content-Length` dépasse la partition de destination (`413`) ; refuse une image qui n'est pas pour cette application ou cette puce (`422`, « Ce fichier n'est pas un firmware pour cet écran. ») ; écrit par `esp_ota_*` dans la partition inactive ; vérifie l'image (`esp_ota_end`) ; répond `200 {"etat":"redemarrage","dans_secondes":3}` puis redémarre. **Un envoi qui échoue ne change rien** : l'ancienne partition reste celle du démarrage.

**Retour arrière automatique** : la nouvelle version démarre « en attente de validation » (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) et ne se déclare valide (`esp_ota_mark_app_valid_cancel_rollback`) qu'après un **auto-test** (écran initialisé, Wi-Fi tenté, serveur HTTP à l'écoute) ; sinon l'appareil redémarre sur l'ancienne. `POST /api/update/rollback` fait la même chose à la demande (`200 {"etat":"redemarrage"}`, ou `409`, « Il n'y a pas de version précédente à remettre. »). Après tout redémarrage, la page se reconnecte seule en lisant `GET /api/status` toutes les 2 s, et affiche `dernier_resultat`.

**À prouver sur matériel** (plan, phase 3) : un envoi d'une image volontairement cassée revient à l'ancienne par rollback. Rien de tout cela n'est constaté à ce jour.

## 12. Fichiers statiques

Le front est servi tel quel par `esp_http_server` : `GET /` → `index.html`, ressources avec `Content-Encoding: gzip` et `ETag` = identifiant de version du firmware. En mode `point_d_acces`, toute adresse inconnue redirige (`302`) vers `/` (portail captif). Aucune route de `/api/` n'est mise en cache.
