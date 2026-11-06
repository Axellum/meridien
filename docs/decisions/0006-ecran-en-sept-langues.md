# ADR-0006 — L'écran parle sept langues, avec le système de traduction du Tab5

- **Statut** : principe **décidé par Axel le 06/11/2026** (« écran 7 langues, vu qu'on a les bases ») ; détails **proposés**, à confirmer à la phase 2.
- **Contexte** : le firmware ESPHome du Tab5 parle déjà français (langue source), anglais, allemand, néerlandais, espagnol, italien et turc, avec un système de type gettext éprouvé : le texte français écrit dans le code est la clé, un fichier YAML plat par langue, un générateur qui produit les tables, anglais en repli. Source : `docs/translations.md` du dépôt Tab5 (lu le 06/11/2026).
- **Décision** :
  - Mêmes sept langues, même principe : `tr("texte français")`, un fichier par langue `i18n/<code>.yaml` (`"texte français": "traduction"`), un script Python qui génère une table C++ (comme `components/core/outils/generer_fuseaux.py`), repli sur l'anglais puis sur le français. L'indice de langue enregistré en NVS **ne change jamais** : on ajoute une langue à la fin.
  - `tr()` vit dans le **noyau pur** (`components/core/`, sans LVGL) donc testable sur le PC ; les tests vérifient que chaque clé existe dans chaque langue ou retombe sur le repli, et que `%d`/`%s` et `{noms}` sont conservés dans le même ordre.
  - La **page de réglages** parle les mêmes langues : un petit fichier JSON par langue servi par l'appareil (chargé à la demande), pas de texte français en dur dans `app.js`.
  - La langue de l'**écran** et celle de l'**assistant vocal** sont deux réglages distincts (reconnaissance, synthèse et consigne du LLM suivent la seconde).
  - Les traductions sont écrites par une IA depuis le français et l'anglais, comme dans le dépôt Tab5 ; le français seul est relu par l'auteur ; c'est dit dans la documentation.
- **Réutilisation** : le vocabulaire commun existe déjà dans `Tab5/lang/*.yaml` (jours, mois, libellés météo…) : repartir de ces fichiers pour les clés identiques plutôt que retraduire. Les clés du Tab5 propres à Home Assistant, aux jeux et au réveil ne sont pas reprises. L'agent `traducteur` du dépôt Tab5 fait ce travail pour ses fichiers ; sa consigne est à adapter.
- **Risques et points non vérifiés** :
  - **Polices** : le turc demande des lettres hors du latin de base (ç, ğ, ı, İ, ö, ş, ü) ; la police embarquée (Roboto Bold, `main/assets/`, branche matériel) doit les contenir : **non vérifié**. Un glyphe manquant = un carré à l'écran.
  - **Mot d'activation** : « Bonjour ESP » est français. Les mots d'activation disponibles pour les autres langues et leur licence : **non vérifié** (recherche 06). Tant que ce n'est pas tranché, la langue de l'assistant vocal peut différer de celle de l'écran.
  - **Longueur des textes** : l'allemand et le néerlandais sont plus longs que le français ; chaque écran est à contrôler sur la carte, pas seulement au PC.
- **Conséquences** : la phase 2 (écran principal) et la phase 3 (page de réglages) portent les sept langues dès le départ ; aucun texte d'écran n'est ajouté sans passer par `tr()` (règle déjà dans `AGENTS.md`).
