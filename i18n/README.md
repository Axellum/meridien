# i18n — les sept langues de l'écran

Décision et mesures : [docs/decisions/0006-ecran-en-sept-langues.md](../docs/decisions/0006-ecran-en-sept-langues.md).

Le texte **français** écrit dans le code est la clé (façon gettext, comme sur le Tab5) : `tr("Redémarrer")`. Chaque
langue est un fichier `i18n/<code>.yaml` ; `components/core/outils/generer_i18n.py` les transforme en une table C++ triée
et en un petit JSON par langue pour la page de réglages.

| Indice | Code | Langue | Fichier |
|---:|---|---|---|
| 0 | `fr` | Français | [fr.yaml](fr.yaml) (métadonnées seules : le français EST la clé) |
| 1 | `en` | English | [en.yaml](en.yaml) |
| 2 | `de` | Deutsch | [de.yaml](de.yaml) |
| 3 | `nl` | Nederlands | [nl.yaml](nl.yaml) |
| 4 | `es` | Español | [es.yaml](es.yaml) |
| 5 | `it` | Italiano | [it.yaml](it.yaml) |
| 6 | `tr` | Türkçe | [tr.yaml](tr.yaml) |

**L'indice ne change jamais** : il est enregistré en NVS, et c'est l'ordre du sélecteur « Langue » du Tab5. Une langue
nouvelle prend le numéro suivant, à la fin.

## Qui a écrit quoi

- Le **français** est la langue de l'auteur : c'est la seule relue par lui.
- Les **six autres** sont écrites par une IA (Claude) depuis le français et l'anglais. **Aucune n'a été relue par un
  locuteur.** Les dates (jours, mois) et quelques libellés (« Redémarrer », « Orage », « Brouillard »…) sont repris tels
  quels des fichiers `lang/` du Tab5 (dépôt `00ProjetTab`), qui ont, eux, servi sur un écran. Tout le reste est du
  vocabulaire nouveau : à relire avant de s'y fier.
- Les contrôles automatiques prouvent que les marqueurs sont intacts, que rien ne manque et que l'encodage est valide ;
  ils ne prouvent **pas** qu'une traduction est juste, ni qu'elle tient sur l'écran.

## Format d'un fichier

```yaml
_langue: Deutsch
_code: de
_index: 2
_statut: complet

# --- Écran : dates (reprises du Tab5) ---
"Dim": "So"
"Redémarrer": "Neu starten"
```

- Un mapping plat : une ligne = `"texte français": "traduction"`, en guillemets doubles. Seuls échappements permis :
  antislash-guillemet et double antislash. Les caractères invisibles (espace insécable U+00A0) s'écrivent tels quels.
- `# --- Titre ---` ouvre une section. Celles dont le titre commence par « Écran » sont ce que **l'écran** affiche (elles
  seront contrôlées contre la police embarquée) ; les autres (« Page de réglages », « Appareil ») ne passent que par la
  page web ou l'API.
- Mêmes clés, même ordre, mêmes sections dans tous les fichiers de langue complets.
- **Marqueurs** (`%d`, `%s`, `%u`, `%02d`… et `{noms}`) : identiques à la clé et dans le même ordre. Une traduction qui
  les change ferait afficher n'importe quoi (ou planter un `printf`) : le générateur et les tests le refusent.
- Tutoiement partout, comme sur le Tab5.
- Repli : langue demandée, puis anglais, puis le texte français lui-même. Un fichier de langue peut donc être incomplet
  (`_statut: partiel`) sans casser l'écran ; il n'y en a aucun aujourd'hui.

## `page.yaml` : les textes de la page de réglages

`web/app/app.js` ne contient aucun texte français : il demande des **identifiants** (`t('ville.titre')`). `page.yaml`
associe chaque identifiant au texte français, qui est une clé de `<code>.yaml` comme les autres. Le JSON de chaque
langue (`web/app/lang/<code>.json`) est la jointure des deux, avec le repli anglais → français déjà appliqué. Un
identifiant ne change jamais de sens.

## Ajouter ou changer un texte

1. Écrire le texte français dans le code (`tr("…")`) ou l'identifiant dans `app.js` (et sa ligne dans `page.yaml`).
2. Ajouter la ligne dans **chacun** des six fichiers de langue, dans la même section et au même rang.
3. Régénérer, puis vérifier (PowerShell) :

```powershell
python components/core/outils/generer_i18n.py              # écrit i18n_donnees.inc et web/app/lang/*.json
python components/core/outils/generer_i18n.py --verifier   # sources cohérentes et sorties à jour, sans rien écrire
. H:\AuxFilsDesIdees\_outils\env_hote.ps1 -Sanitizers
python test/host/lancer.py                                 # dont test_i18n.cpp
```

Les sorties générées (`components/core/include/ecran/i18n_donnees.inc`, `web/app/lang/*.json`) sont versionnées : ne pas
les modifier à la main, `--verifier` le détecterait.

## Ajouter une langue

Un nouveau fichier `i18n/<code>.yaml` avec l'indice suivant (7, 8…), puis ajouter le code à `ORDRE_FIGE` du générateur,
à `CODES`/`NOMS`/`Langue` de `i18n.hpp` et aux tests de `test/host/test_i18n.cpp`. Vérifier d'abord que la police
embarquée contient les caractères de la langue.
