# ADR-0002 — Assistant vocal : mot d'activation sur l'appareil, puis cascade STT → LLM → TTS à trois fournisseurs indépendants

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : sans Home Assistant, c'est l'appareil qui relie micro, LLM et haut-parleur. L'utilisateur veut un LLM « local ou non ». Recherche : [02-vocal-et-llm.md](../recherche/02-vocal-et-llm.md).
- **Décision** :
  1. Sur l'appareil : ESP-SR (AFE : réduction de bruit, VAD, **AEC**) + mot d'activation WakeNet (français disponible : « Bonjour ESP »). Le mot propre à l'auteur (« okay nabu » aujourd'hui, via microWakeWord) est une décision d'Axel.
  2. Chaîne par défaut en **cascade HTTP**, avec trois points d'accès réglables séparément : STT (`/v1/audio/transcriptions`), LLM (`/v1/chat/completions`, streaming), TTS (`/v1/audio/speech`). Chacun accepte une URL, une clé et un modèle ; préréglages OpenAI, Google, LM Studio, Ollama, personnalisé. Un même code sert le local et le cloud.
  3. Le parole-à-parole (OpenAI Realtime, Gemini Live) est un **backend optionnel** ajouté plus tard, derrière la même interface.
- **Pourquoi trois fournisseurs** : LM Studio n'a aucun endpoint audio et Anthropic n'a ni STT ni TTS ; « LLM local » veut donc dire trois services (STT, LLM, TTS). Un seul fournisseur supposé tout faire serait faux pour le local.
- **Risque n°1** : l'AEC en duplex sur le Tab5 (deux codecs distincts, référence à caler sur la capture, aucun exemple trouvé). Il est levé en **phase 1** par un test mesuré avant d'écrire l'assistant. **Repli** : pas d'interruption vocale ; micro coupé pendant la réponse ; un appui sur l'écran interrompt.
- **Conséquences** : l'écran n'est « totalement indépendant » que si les trois services répondent ; hors ligne, il garde l'heure et la dernière météo, et dit clairement que l'assistant est indisponible. Coût d'usage cloud estimé 4,7 à 24 $/mois pour ~50 échanges/jour selon l'offre ; 0 en local.
