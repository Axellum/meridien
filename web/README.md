# Page de réglages (piste B)

Page servie par l'appareil lui-même (HTTP local), ouverte depuis un téléphone. Contrat avec le firmware : [docs/api-reglages.md](../docs/api-reglages.md). Aspect retenu par Axel : l'esquisse B, « une page de cartes » ([esquisses/](esquisses/)).

| Dossier | Contenu |
|---|---|
| `app/` | La page : `index.html`, `style.css`, `app.js`. Sans framework, sans étape de compilation, sans police ni image externe. `app.js` n'écrit aucun texte d'écran. |
| `app/lang/<code>.json` | Un petit fichier par langue (fr, en, de, nl, es, it, tr), **généré** par `components/core/outils/generer_i18n.py` depuis `i18n/page.yaml` et `i18n/<code>.yaml` : ne pas les modifier à la main. La page ne charge que celui dont elle a besoin. |
| `faux-serveur/serveur.mjs` | Faux appareil (Node, aucune dépendance) qui répond comme l'API du contrat, avec de vrais appels à Open-Meteo, les vraies tables du noyau (`components/core/`) et les mêmes fichiers `i18n/<code>.yaml` que le firmware : ses phrases sortent dans la langue de l'écran. |
| `tests/parcours.py` | Parcours Playwright (Chromium, rendu téléphone 390 × 844) contre le faux appareil. |
| `tests/controle_langues.py` | Contrôle sans navigateur : `app.js` n'écrit aucun texte français, chaque identifiant de `page.yaml` est cité, chaque texte passé à `T()` dans le faux serveur est une clé des fichiers de langue. Lancé aussi par `test/host/lancer.py`. |
| `captures/` | Captures et mesures écrites par le parcours (`poids.json`, `reglages-exporte.json`). |
| `esquisses/` | Les trois esquisses proposées à Axel. |

## Lancer

Faux appareil seul, pour regarder la page dans un navigateur (PowerShell) :

```powershell
$env:CODE_APPAIRAGE = "246810"; node web/faux-serveur/serveur.mjs
```

Ouvrir <http://127.0.0.1:8080>, entrer le code. Variables : `PORT` (8080), `PORT_SERVICE` (PORT + 1, faux service compatible OpenAI), `CODE_APPAIRAGE`, `CLE_MOCK` (clé acceptée par le faux service, `cle-de-test-123` par défaut), `VERROU_S`, `REBOOT_MS`. Le faux service répond `401` avec un corps qui **recopie la clé reçue** : c'est voulu, pour prouver que l'appareil ne transmet jamais le texte d'un service tiers.

Parcours complet (réseau requis : recherche de lieu et météo appellent vraiment Open-Meteo) :

```powershell
python web/tests/parcours.py
```

Prérequis : Node, Python avec `playwright` et son Chromium. Le script lance et arrête le faux appareil, écrit `web/captures/`, rend un tableau OK / ÉCHEC et un code de sortie non nul au moindre échec.

## Ce que le parcours prouve (129 vérifications, toutes OK au dernier passage, code de sortie 0)

- Appairage : mauvais code → phrase française ; bon code → page ; 5 essais ratés → verrou 429, qui se lève.
- « Lyon » (3 lettres seulement envoyées, vraie recherche Open-Meteo) → zone enregistrée Lyon, `Europe/Paris`, POSIX `CET-1CEST,M3.5.0,M10.5.0/3`, coordonnées proches de Lyon ; la météo affichée est celle qu'Open-Meteo donne en direct au même moment, avec le libellé de la table WMO du noyau.
- Service LLM : clé refusée → phrase lisible sans le texte du service ; bonne clé → « Le service répond… » et liste des modèles ; service injoignable → phrase du §8 ; enregistrement → après rechargement la clé n'est plus affichée.
- **Secret** : la clé saisie n'apparaît dans aucune des réponses de l'API (30 vues), ni dans le DOM, ni dans un champ de la page, ni dans l'export téléchargé. Contrôle du contrôle : la clé a bien voyagé dans des requêtes, et un essai de mutation (le faux appareil recopie le corps du service) fait échouer deux vérifications.
- Export sans clé ni mot de passe, avec `cles_a_ressaisir` ; import : simulation puis application, une clé glissée dans le fichier est ignorée et signalée.
- Écran (thème, luminosité), Wi-Fi (mot de passe refusé → ancien réseau conservé ; réseau ouvert), mise à jour (fichier qui n'est pas un firmware refusé ; image cassée → retour arrière automatique annoncé ; bonne image ; retour arrière demandé) avec reconnexion et nouvel appairage après redémarrage.
- Alertes officielles (10 vérifications, `GET`/`PUT /api/alerts`, [docs/api-reglages.md](../docs/api-reglages.md) §14) : lecture avec l'adresse du service public pour le champ vide ; « Orange », « Non » et 45 min enregistrés sur l'appareil ; une adresse en `http://` refusée avec une phrase et rien d'enregistré ; `  HTTPS://Vigie.Exemple.fr//  ` enregistrée sous sa forme normalisée (`https://Vigie.Exemple.fr`) et réécrite dans le champ ; un champ vide remet le service public ; pas de défilement horizontal ; réglages remis comme avant le pas.
- Pas de défilement horizontal à 360, 390 et 430 px ; aucune erreur JavaScript ; aucun « null / undefined / NaN » affiché ; aucune requête hors de l'appareil.
- **Sept langues** (69 vérifications, plus une sur le poids) : l'appareil propose fr, en, de, nl, es, it, tr dans cet ordre, chacune sous son propre nom ; français par défaut pour l'écran **et** pour l'assistant ; deux sélecteurs distincts (changer l'un ne change pas l'autre) ; au départ seul `fr.json` est demandé, puis `de.json` quand on passe à l'allemand (pas les cinq autres). Pour chacune des sept langues : onglets, titre de carte et titre de l'onglet du navigateur dans la langue, aucun identifiant brut (`ville.titre`) affiché, aucun texte français visible (les six autres), libellé météo = un libellé de la table WMO du noyau traduit, pas de défilement horizontal à 360 px. Une phrase qui vient de l'appareil (code faux) arrive en allemand telle que l'appareil l'envoie. **Repli** : `de.json` introuvable → page en anglais ; `de.json` et `en.json` introuvables → page en français.
- **Attribution Open-Meteo (CC BY 4.0)** : visible dans la carte « Ville » dans la langue de la page, « Open-Meteo » est un lien vers `https://open-meteo.com/` (vérifié pour chacune des sept langues et pour les deux replis).

Les textes de la page viennent de traductions écrites par une IA ; seul le français est relu par l'auteur. Ces vérifications prouvent que le bon texte est affiché au bon endroit, pas que la traduction est juste ni que les phrases allemandes et néerlandaises tiennent sur l'écran de l'appareil (non vérifié).

## Poids

Mesuré sur la réponse gzip du faux appareil (`encodedBodySize`), le 06/11/2026 : `index.html` 1 037 + `style.css` 2 409 + `app.js` 11 304 = **14 750 octets** ; avec un fichier de langue (`fr.json`, 2 161) : **16 911 octets** (56 089 octets bruts). **Avec la carte des alertes (07/11/2026)** : `index.html` 1 086 + `style.css` 2 409 + `app.js` 12 307 = **15 802 octets** ; avec `fr.json` (2 421) : **18 223 octets** (61 951 octets bruts), pour un budget annoncé de 30 Ko (30 720). Les sept fichiers de langue pèsent entre 2 067 octets (`en`) et 2 338 octets (`de`) compressés au niveau 9 : la page avec l'allemand, la plus lourde, fait 17 088 octets. Sur l'appareil, ces fichiers seront à servir déjà compressés (§12 du contrat).

## Choix qui s'écartent de l'esquisse ou du contrat

- Un service (comprendre, réfléchir, parler) s'enregistre avec le bouton « Enregistrer » et non à chaque frappe : une saisie de clé ou d'adresse à moitié tapée ne doit pas partir vers l'appareil. Le thème et la luminosité, eux, s'enregistrent dès qu'on les change.
- La page ne contient aucune couleur en dur hors des jetons de `style.css` ; clair et sombre suivent le téléphone. Aucune transition ni fondu (préférence du produit).

## Non vérifié

- Tout ce qui touche à l'appareil réel : le serveur HTTP du firmware, la compression servie par l'appareil, la durée réelle d'un redémarrage, le portail de mise en service Wi-Fi.
- Autres navigateurs que Chromium (Safari iOS, Firefox) : la feuille de style utilise `:has()`, absent des navigateurs de téléphone anciens (sans effet sur le reste de la page : seul le contour rouge d'un champ invalide manquerait).
- Les lecteurs d'écran : les rôles et `aria-*` sont posés mais n'ont pas été écoutés.
