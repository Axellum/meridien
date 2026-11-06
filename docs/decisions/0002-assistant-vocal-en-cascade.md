# ADR-0002 — Assistant vocal : mot d'activation sur l'appareil, puis cascade STT → LLM → TTS à trois fournisseurs indépendants

> **Summary (EN)** — Proposed on 2026-11-06, awaiting the owner's validation.
> - Decision: on the device, ESP-SR (noise reduction, VAD, **echo cancellation**) plus a WakeNet wake word (French available: "Bonjour ESP"; choosing a custom word is left to the owner). The default chain is an HTTP cascade with three separately configurable endpoints, STT (`/v1/audio/transcriptions`), streaming LLM (`/v1/chat/completions`) and TTS (`/v1/audio/speech`), each with a URL, a key and a model; presets OpenAI, Google, LM Studio, Ollama, custom. One code path serves local and cloud.
> - Speech-to-speech (OpenAI Realtime, Gemini Live) is an optional backend added later behind the same interface.
> - Why three providers: LM Studio has no audio endpoint and Anthropic has no STT or TTS, so a "local LLM" means three services.
> - Risk no. 1: duplex echo cancellation on the Tab5 (two separate codecs, a reference to align with the capture, no example found); it is tested with measurements in phase 1 before the assistant is written. Fallback: no voice interruption, microphone muted during the answer, a tap on the screen interrupts.
> - Consequences: the screen is only fully independent if the three services answer; offline it keeps the time and the last weather and says clearly that the assistant is unavailable. Estimated cloud cost 4.7 to 24 USD/month for about 50 exchanges a day depending on the offer; zero locally.

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : sans Home Assistant, c'est l'appareil qui relie micro, LLM et haut-parleur. L'utilisateur veut un LLM « local ou non ». Recherche : [02-vocal-et-llm.md](../recherche/02-vocal-et-llm.md).
- **Décision** :
  1. Sur l'appareil : ESP-SR (AFE : réduction de bruit, VAD, **AEC**) + mot d'activation WakeNet (français disponible : « Bonjour ESP »). Le mot propre à l'auteur (« okay nabu » aujourd'hui, via microWakeWord) est une décision d'Axel.
  2. Chaîne par défaut en **cascade HTTP**, avec trois points d'accès réglables séparément : STT (`/v1/audio/transcriptions`), LLM (`/v1/chat/completions`, streaming), TTS (`/v1/audio/speech`). Chacun accepte une URL, une clé et un modèle ; préréglages OpenAI, Google, LM Studio, Ollama, personnalisé. Un même code sert le local et le cloud.
  3. Le parole-à-parole (OpenAI Realtime, Gemini Live) est un **backend optionnel** ajouté plus tard, derrière la même interface.
- **Pourquoi trois fournisseurs** : LM Studio n'a aucun endpoint audio et Anthropic n'a ni STT ni TTS ; « LLM local » veut donc dire trois services (STT, LLM, TTS). Un seul fournisseur supposé tout faire serait faux pour le local.
- **Risque n°1** : l'AEC en duplex sur le Tab5 (deux codecs distincts, référence à caler sur la capture, aucun exemple trouvé). Il est levé en **phase 1** par un test mesuré avant d'écrire l'assistant. **Repli** : pas d'interruption vocale ; micro coupé pendant la réponse ; un appui sur l'écran interrompt.
- **Conséquences** : l'écran n'est « totalement indépendant » que si les trois services répondent ; hors ligne, il garde l'heure et la dernière météo, et dit clairement que l'assistant est indisponible. Coût d'usage cloud estimé 4,7 à 24 $/mois pour ~50 échanges/jour selon l'offre ; 0 en local.
