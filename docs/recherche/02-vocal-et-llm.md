# Assistant vocal et LLM — recherche du 06/11/2026

> **Summary (EN)** — Research note of 2026-11-06 on the voice assistant and LLM options (one web-research agent plus facts from the Tab5 repository). Prices are USD from official price lists read that day, not converted, and subject to change.
> - Conclusion: on the device, an ESP-SR audio layer (WakeNet wake word, VAD, echo cancellation, noise reduction); by default an HTTP cascade STT → LLM → TTS with three independent endpoints (`/v1/audio/transcriptions`, streaming `/v1/chat/completions`, `/v1/audio/speech`); speech-to-speech (OpenAI Realtime, Gemini Live) as an optional backend later (ADR-0002).
> - "Local LLM" means three services, not one: LM Studio has no audio endpoint, Anthropic has no STT or TTS, Ollama's compatibility page lists the audio routes (which models really serve them: not tested). A local French voice option is Kokoro; reliable local Whisper or Piper options were not found (not verified).
> - Wake word: ESP-SR lists the ESP32-P4; the only French model is "Bonjour ESP"; its licence text is MIT-style but limited to Espressif products and model licences are undocumented; microWakeWord has no official French model. Choosing the word is the owner's decision.
> - Biggest risk: duplex audio with echo cancellation on the Tab5 (two separate codecs, a reference signal to align with the capture, no Tab5 or P4 example found). Under ESPHome a single I2S mutex forbids duplex; natively it may work, which is not yet proven. Fallback: no voice interruption, microphone muted during the answer, a tap on the screen interrupts.
> - Starting points: `xiaozhi-esp32` (MIT, has a Tab5 board) and Espressif's `openai_demo`; Voice PE only for its state machine.
> - Estimated cloud cost for ~50 exchanges a day: about 4.7 to 24 USD/month depending on the offer; no API cost locally (the PC must stay on). Latency and RAM/TLS needs on the P4: not verified.

Source : 1 agent de recherche web + faits du dépôt Tab5. Prix en USD lus sur les grilles officielles le 06/11/2026 (non convertis en euros : taux non vérifié ; ces prix évoluent, les pages n'affichent pas de date).

## Conclusion

1. **Sur l'appareil** : couche audio ESP-SR/AFE (mot d'activation WakeNet, VAD, AEC, réduction de bruit).
2. **Chaîne par défaut en cascade** STT → LLM → TTS en HTTP, trois points d'accès **indépendants** (chacun local ou cloud) : `/v1/audio/transcriptions`, `/v1/chat/completions` en streaming, `/v1/audio/speech`. Même code pour un serveur local et pour le cloud, coût le plus bas. Voir ADR-0002.
3. **Parole-à-parole** (OpenAI Realtime, Gemini Live) : backend **optionnel** plus tard (WebSocket TLS, PCM) ; plus cher et plus complexe.
4. **Départ du code** : `xiaozhi-esp32` (MIT, carte Tab5 présente) et l'exemple `openai_demo` d'Espressif pour la gestion audio ; de Voice PE, seulement la machine à états.
5. **Risque n°1 à lever en premier** : l'AEC en duplex sur le Tab5 (voir plus bas).

## « LLM local » = trois services, pas un

- **LM Studio** expose chat, responses, embeddings, completions et **aucun endpoint audio** (doc LM Studio, openai-compat).
- **Ollama** : sa page de compatibilité liste `/v1/audio/speech` et `/v1/audio/transcriptions` (confirmé par un second fetch du dépôt). *Surprenant, non testé* : quels modèles servent réellement ces routes.
- Voix française locale : **Kokoro** (`ff_siwis`, `fm_gabriel`, serveur compatible OpenAI sur le port 8880 ; `ff_siwis` repose sur < 11 h de données, CC BY 4.0). whisper.cpp/faster-whisper en API OpenAI et Piper en français : **rien de fiable trouvé, non vérifié**.
- **Anthropic n'a ni STT ni TTS** (liste de fonctionnalités de l'API) : Claude reste utilisable comme LLM du milieu, avec STT/TTS tiers.

→ La page de réglages propose donc **trois cartes (STT, LLM, TTS)** avec préréglages (OpenAI, Google, LM Studio, Ollama, personnalisé) et un bouton « tester » par carte.

## Mot d'activation

- **ESP-SR 2.6.x** liste l'ESP32-P4 pour WakeNet, MultiNet et l'AFE. Le français existe : `wn10_fr_bonjouresp` et `wn9l_fr_bonjouresp_tts3` (« Bonjour ESP ») ; WakeNet10 couvre 8 langues depuis le 30/09/2026 ; l'entraînement de mots personnalisés en français est annoncé (« TTS Pipeline V3 », 23/04/2026), coût et licence non documentés.
- **MultiNet** (commandes) : chinois et anglais seulement — sans importance, le LLM comprend la parole transcrite.
- Licence : texte MIT mais **limité aux produits Espressif** ; licence des modèles non documentée.
- **microWakeWord** (Apache 2.0, TFLite Micro) : pas de modèle français officiel (un utilisateur en a entraîné un avec des échantillons Piper) ; fil du forum HA « Is MicroWakeWord dead? » à prendre en compte. Licence d'openWakeWord : non vérifiée.
- Mot actuel d'Axel sur le firmware ESPHome : « okay nabu » (microWakeWord). Choisir le mot est une décision d'Axel (« Bonjour ESP » est imposé par les modèles officiels).

## AEC et duplex (le gros risque)

- Dans ESPHome, micro et haut-parleur partagent **un seul mutex I2S** : duplex impossible, donc pas d'AEC ni d'interruption (barge-in) — limite d'ESPHome. **En natif, l'I2S peut être en duplex intégral** (non encore prouvé sur le Tab5).
- L'AFE accepte `input_format="MR"` (1 micro + référence haut-parleur), PCM 16 bits 16 kHz entrelacé ; l'AEC traite au plus 2 micros. Un nouvel AEC full-duplex a été annoncé le 28/04/2026 (README esp-sr).
- `openai_demo` est vérifié par défaut sur ESP32-S3-Korvo-2 « pour obtenir l'AEC » (Opus + AEC) ; sa doc montre un build `esp32_p4_function_ev_board`.
- **Tab5** : deux codecs distincts (ES7210 micros, ES8388 haut-parleur) ; il faut recueillir la référence côté I2S (ou via un canal de rebouclage de l'ES7210) et la caler précisément sur la capture. **Aucun exemple Tab5/P4 d'AEC avec canal de référence trouvé.** Sur la 7B, la carte renvoie déjà le haut-parleur sur MIC3 de l'ES7210 (audit 7B du dépôt Tab5).
- **Repli si l'AEC échoue** : pas d'interruption vocale ; couper le micro pendant la réponse et laisser un appui sur l'écran interrompre (comportement actuel du Tab5).

## Parole-à-parole

- **Gemini Live** : WebSocket `wss://generativelanguage.googleapis.com/ws/…BidiGenerateContent`, entrée PCM 16 kHz little-endian en base64, jetons éphémères disponibles.
- **OpenAI Realtime** : WebRTC (navigateurs, mobiles) ou WebSocket (serveurs de confiance) ; formats audio non confirmés par la page lue.
- Latence mesurée et besoins RAM/TLS sur ESP32-P4 : **non vérifiés**.

## Projets de référence

| Projet | Licence | État | À reprendre |
|---|---|---|---|
| `78/xiaozhi-esp32` | MIT | actif (1 203 commits) ; dossier `m5stack/tab5` + 3 cartes P4 Waveshare (la 7B n'y est pas) | pilotes écran/codec Tab5, protocoles WebSocket et MQTT+UDP, intégration ESP-SR ; son README Tab5 demande IDF ≥ 5.5.2 et esp-sr ≥ 2.4.7 |
| `espressif/esp-webrtc-solution` | non lue | 215 commits, aucune release | `openai_demo`, `esp_capture`, Opus, AEC |
| `esphome/home-assistant-voice-pe` | non lue | 373 commits (dev) | machine à états seulement (code ESPHome) |

`esp-brookesia` et `esp-gmf` : non examinés.

## Coût cloud estimé (hypothèse : 1 500 échanges/mois ≈ 50/jour, 10 s de parole en entrée et 10 s en sortie, sans historique audio)

| Offre | Formule | $/mois |
|---|---|---|
| gpt-realtime 2.1 (32 $/64 $ par M de jetons) | 1500 × (100×32 + 200×64) / 10⁶ | ≈ 24 |
| gpt-realtime-2.1-mini (10 $/20 $) | 1500 × (100×10 + 200×20) / 10⁶ | ≈ 7,5 |
| Gemini 3.8 Live (3 $/12 $, 25 jetons/s) | 1500 × (250×3 + 250×12) / 10⁶ | ≈ 5,6 |
| Cascade : gpt-4o-mini-transcribe + gpt-5-mini + Gemini 3.8 flash TTS | 1500 × (0,0005 + 0,0004 + 0,00225) | ≈ 4,7 |

L'historique de conversation est refacturé à chaque tour en Realtime (non inclus). Prix Gemini 3.8 flash et son TTS : ×2 au 01/01/2027. Une page tierce donnait 4 $/16 $ pour « GPT Realtime » : écartée au profit de la grille officielle. **En local : 0 € d'API**, le PC doit rester allumé (consommation non mesurée).

## Pièges

- Le Tab5 impose IDF ≥ 5.5.2 et un esp-sr récent ; un firmware P4X ne se flashe pas sur une révision 1.x.
- Le Wi-Fi passe par le C6 (ESP-Hosted) : latence en plus.
- « Bonjour ESP » est un mot imposé ; un mot de marque = entraînement personnalisé.
- Non vérifié : licences d'esp-webrtc-solution, de Voice PE, d'openWakeWord, des modèles WakeNet ; coût d'un entraînement ; taux de support réel de `/v1/audio/*` par Ollama.
