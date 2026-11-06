# Architecture decisions (ADR) / Décisions d'architecture

## English · [Français](#version-française)

A non-obvious decision gets one numbered page here (next number), with the context, the choice, the alternatives that were
rejected and what would reopen it. Read the relevant page **before** "fixing" something that looks wrong. The pages are
written in French; each of 0001 to 0005 opens with a short English summary. A summary says no more than the French text
below it, including its "proposed" status.

| No. | Decision | Status |
|---|---|---|
| [0001](0001-pile-esp-idf-natif.md) | Stack: native ESP-IDF + LVGL 9 | proposed, awaiting the owner's validation |
| [0002](0002-assistant-vocal-en-cascade.md) | Voice assistant: wake word on the device, then an STT → LLM → TTS cascade with three independent providers | proposed, awaiting the owner's validation |
| [0003](0003-reglages-par-page-web-et-meteo-open-meteo.md) | Settings through a page served by the device; Open-Meteo weather | proposed, awaiting the owner's validation |
| [0004](0004-calendrier-par-adresse-ical-secrete.md) | Google calendar (version 2): secret iCal address | proposed, awaiting the owner's validation |
| [0005](0005-couche-carte.md) | One board layer: develop on the Tab5, keep the Waveshare 7B compilable | proposed, awaiting the owner's validation |
| [0006](0006-ecran-en-sept-langues.md) | The screen speaks seven languages, with the Tab5's translation system | principle decided by the owner (2026-11-06), details proposed |
| [0007](0007-reglages-a-l-ecran-et-installation-guidee.md) | Every setting on the screen, the web page as a second way, a replayable guided first-start set-up | principle decided by the owner (2026-11-06), details proposed |

Status as of 2026-11-06. "Proposed" means written by the AI agent from the research notes and not yet settled by the owner;
the code follows it in the meantime. The open questions (wake word, local STT/TTS, project name, repository, licence, ESP-IDF
version, calendar freshness) are listed in [the plan](../plan.md#décisions-à-prendre-par-axel) (in French, with an English
summary at the top).

---

## Version française

Une décision non évidente = une page numérotée ici (numéro suivant), avec le contexte, le choix, les alternatives écartées et ce qui la remettrait en cause. À lire **avant** de « corriger » quelque chose qui a l'air faux. Les pages sont en français ; chacune de 0001 à 0005 commence par un court résumé en anglais, qui n'en dit pas plus que le texte français, statut « proposée » compris.

| N° | Décision | Statut |
|---|---|---|
| [0001](0001-pile-esp-idf-natif.md) | Pile : ESP-IDF natif + LVGL 9 | proposée, à valider par Axel |
| [0002](0002-assistant-vocal-en-cascade.md) | Assistant vocal : mot d'activation sur l'appareil, puis cascade STT → LLM → TTS à trois fournisseurs indépendants | proposée, à valider par Axel |
| [0003](0003-reglages-par-page-web-et-meteo-open-meteo.md) | Réglages par une page servie par l'appareil ; météo Open-Meteo | proposée, à valider par Axel |
| [0004](0004-calendrier-par-adresse-ical-secrete.md) | Calendrier Google (phase 2) : adresse iCal secrète | proposée, à valider par Axel |
| [0005](0005-couche-carte.md) | Une couche carte : développer sur le Tab5, garder la 7B compilable | proposée, à valider par Axel |
| [0006](0006-ecran-en-sept-langues.md) | L'écran parle sept langues, avec le système de traduction du Tab5 | principe décidé par Axel (06/11/2026), détails proposés |
| [0007](0007-reglages-a-l-ecran-et-installation-guidee.md) | Tous les réglages à l'écran, la page web en second chemin, installation guidée au premier démarrage et rejouable | principe décidé par Axel (06/11/2026), détails proposés |

Statut au 06/11/2026. « Proposée » = écrite par l'agent d'après les recherches, pas encore tranchée par Axel ; le code s'y conforme en attendant. Les questions ouvertes (mot d'activation, STT/TTS locaux, nom du projet, dépôt, licence, version d'ESP-IDF, fraîcheur du calendrier) sont listées dans [le plan](../plan.md#décisions-à-prendre-par-axel).
