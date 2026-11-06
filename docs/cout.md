# Coût : temps, sessions, tokens

> **Summary (EN)** — Cost estimate (time, sessions, tokens) dated 2026-11-06. A working document in French. It is an estimate by analogy, ± 50 %; only the rows marked "mesuré" (measured) are findings.
> - Reference figures come from the maintainer's own AI-session history (all projects): 158 Claude Code sessions between 16 and 30 Oct 2026; for the Tab5 sessions (76 % of them) about 135,000 output tokens per session; about 57 M tokens re-read from the cache per session (9.0 G / 158). Cache re-reading is the main consumer of quota, so cost depends more on the number of calls × context size than on output tokens.
> - Version 1 (phases 1 to 6): 22–33 sessions, 3.9–6.4 M output tokens, 16–26 days in sequence, about 7–10 h of the owner's time (mostly looking at the screen and testing). With the calendar (phase 7): 24–36 sessions, 4.2–6.9 M output tokens.
> - Calendar time: about 3 weeks (2 to 4) with two parallel sessions and quick physical tests, up to 6 weeks if echo cancellation or the captive portal resist. The real bottleneck is waiting for the owner at the board and the 5-hour usage window, not producing code.
> - Money: Claude runs on a flat-rate subscription (no marginal cost per token; the API price was not re-read). No purchase identified except a useful 5 V / 2 A charger for the Tab5. Running the product on cloud services: about 4.7 to 24 USD/month for ~50 exchanges a day depending on the offer (price lists read 2026-11-06); no API cost locally; weather 0 (Open-Meteo, non-commercial).
> - Drift factors that could double the figure: duplex echo cancellation not working on the Tab5, the captive portal misbehaving through the C6, the pinned `esp_hosted` being incompatible, two screen revisions that each need an init sequence, sessions that grow too long.

Chiffrage du 06/11/2026. **Estimation par analogie, pas une mesure** : ± 50 %. Seules les lignes marquées « mesuré » sont des constats.

## Ce qui sert de repère (mesuré)

| Repère | Valeur | Source |
|---|---|---|
| Claude Code du 16 au 30/10/2026, tous projets | 158 sessions, 23,6 M tokens de sortie, 9,0 G tokens relus depuis le cache | audit de l'historique des sessions (notes privées d'Axel, hors dépôt) §1-2 |
| Dont le Tab5 | 76 % des sessions, 16,2 M tokens de sortie → ≈ 135 000 tokens de sortie par session | idem |
| Relecture du contexte | ≈ 280 000 tokens relus par appel → ≈ 57 M tokens relus par session (9,0 G / 158) | idem |
| Sessions lourdes | 1,0 à 1,2 M tokens de sortie, sur 3 à 6 jours | idem |
| Durée d'une session | médiane ≈ 20 min, 10 % dépassent 5 h | audit de l'historique des sessions du 29/09/2026 (notes privées d'Axel, hors dépôt) |
| Cette préparation (phase 0) | 8 agents de recherche/audit en parallèle ≈ **353 000 tokens** (75 627 + 27 119 + 90 863 + 17 536 + 33 217 + 27 458 + 29 875 + 51 218), ~2,5 min d'horloge ; plus cette session | rapports des agents |
| Quota à l'écriture | fenêtre de 5 h à 42 %, hebdomadaire à 10 % (toutes sessions confondues, forfait Max) | `get_usage` |

La relecture du cache est le premier consommateur de quota (80 % le 28-29/09) : le coût réel dépend plus du **nombre d'appels × taille du contexte** que des tokens de sortie.

## Chiffrage par phase

Tokens de sortie par session retenus : 150 000 à 200 000 (au-dessus de la moyenne historique de 135 000 : projet neuf + mise au point sur matériel).

| Phase | Sessions | Tokens de sortie | Durée (horloge) | Temps d'Axel |
|---|---|---|---|---|
| 0. Préparation (faite) | 1 | ≈ 0,35 M dans les agents (mesuré) + cette session | 1 demi-journée | quelques minutes |
| 1. Prouver le matériel (écran, Wi-Fi/C6, duplex + AEC, portail captif) | 4 – 6 | 0,7 – 1,1 M | 3 – 5 jours | 1 – 2 h (voir l'écran, tester avec un iPhone et un Android) |
| 2. Écran principal : heure, date, météo | 3 – 4 | 0,5 – 0,8 M | 2 – 3 jours | ≈ 1 h (juger le rendu) |
| 3. Page de réglages | 4 – 6 | 0,7 – 1,1 M | 3 – 4 jours | ≈ 1,5 h (juger la page sur le téléphone) |
| 4. Assistant vocal | 6 – 9 | 1,2 – 2,0 M | 5 – 8 jours | 2 – 3 h (parler à l'appareil, faux déclenchements) |
| 5. Finition et qualité | 3 – 5 | 0,5 – 0,9 M | 2 – 4 jours | ≈ 1 h |
| 6. Compatibilité 7B (couche carte + jalons physiques) | 2 – 3 | 0,3 – 0,5 M | 1 – 2 jours | 0,5 – 1 h |
| **Version 1 (phases 1 à 6)** | **22 – 33** | **3,9 – 6,4 M** | **16 – 26 jours en séquence** | **≈ 7 – 10 h** |
| 7. Calendrier ICS (version 2) | 2 – 3 | 0,3 – 0,5 M | 1 – 2 jours | ≈ 0,5 h |
| **Total avec calendrier** | **24 – 36** | **4,2 – 6,9 M** | | |

Tokens relus du cache (à 57 M par session, moyenne mesurée) : **≈ 1,4 à 2,1 G** pour l'ensemble.

## En calendrier

- Les phases 2 et 3 peuvent se faire **en parallèle** après la phase 1 ; la 4 dépend de 1c, de 3 pour les réglages du LLM, et de 2 pour l'affichage. Avec 2 sessions en parallèle et des tests physiques rapides d'Axel : **version 1 utilisable en ≈ 3 semaines** (2 à 4 semaines), jusqu'à **6 semaines** si l'AEC ou le portail captif résistent. Le calendrier ajoute ≈ 1 semaine.
- Ce qui bloque en pratique n'est pas la production de code mais **l'attente d'Axel devant la carte** (essais physiques) et la **fenêtre de 5 h**. Ordre de grandeur de comparaison : le portage de la 7B (4 lots empilés) a pris du 12 au 16/10, soit 4 jours ; mais ici l'architecture, les pilotes et la page web sont à écrire.

## Ce que ça coûte en argent

- **Claude** : forfait Max → pas de coût marginal par token. Non converti en tarif API : le tarif courant n'a pas été relu (règle : rien de mémoire).
- **Matériel** : aucun achat nécessaire identifié. Utile : un chargeur 5 V / 2 A pour le Tab5 (le port USB du PC a causé des brownouts). Option : une troisième carte si Axel ne veut pas arrêter le Tab5 en service (prix non relu).
- **Exploitation du produit** : cloud ≈ **4,7 à 24 $/mois** pour ~50 échanges/jour selon l'offre (cascade OpenAI/Google 4,7 ; Gemini Live 5,6 ; OpenAI Realtime mini 7,5 ; Realtime 24 — grilles lues le 06/11/2026, voir [recherche/02](recherche/02-vocal-et-llm.md)) ; **local : 0 $ d'API**, PC allumé (consommation non mesurée). Météo : 0 (Open-Meteo, non commercial).

## Facteurs de dérive (ce qui pourrait doubler le chiffre)

1. L'AEC duplex ne marche pas sur Tab5 et il faut creuser (+3 à 5 sessions) ; avec le repli accepté, on économise 1 à 2 sessions.
2. Le portail captif par le C6 se comporte mal (+2 à 3 sessions) ; repli : saisie sur l'écran.
3. `esp_hosted` épinglé incompatible avec IDF 5.5 et un composant (+2 sessions).
4. Les deux révisions d'écran du Tab5 (ST7121/ST7123) demandent chacune sa séquence d'init (+1 à 2 sessions) ; l'ILI9881C d'origine n'est pas disponible pour essai.
5. Sessions trop longues : au-delà de 600 000 tokens de contexte la relecture explose (seuil de compaction déjà à 600 000).

## Leviers pour réduire

- Compiler et lire les journaux dans un sous-agent (les journaux ne passent pas dans la conversation principale) ; recherches web dans des sous-agents moins chers ; un sujet = une session ; ne pas dépasser 2 sessions lourdes en parallèle sur ce projet.
- Terminer chaque phase par un ADR/état court pour que la suivante reparte sans relire l'historique.
