# Décisions d'architecture (ADR)

Une décision non évidente = une page numérotée ici (numéro suivant), avec le contexte, le choix, les alternatives écartées et ce qui la remettrait en cause. À lire **avant** de « corriger » quelque chose qui a l'air faux.

| N° | Décision | Statut |
|---|---|---|
| [0001](0001-pile-esp-idf-natif.md) | Pile : ESP-IDF natif + LVGL 9 | proposée, à valider par Axel |
| [0002](0002-assistant-vocal-en-cascade.md) | Assistant vocal : mot d'activation sur l'appareil, puis cascade STT → LLM → TTS à trois fournisseurs indépendants | proposée, à valider par Axel |
| [0003](0003-reglages-par-page-web-et-meteo-open-meteo.md) | Réglages par une page servie par l'appareil ; météo Open-Meteo | proposée, à valider par Axel |
| [0004](0004-calendrier-par-adresse-ical-secrete.md) | Calendrier Google (phase 2) : adresse iCal secrète | proposée, à valider par Axel |
| [0005](0005-couche-carte.md) | Une couche carte : développer sur le Tab5, garder la 7B compilable | proposée, à valider par Axel |
| [0006](0006-ecran-en-sept-langues.md) | L'écran parle sept langues, avec le système de traduction du Tab5 | principe décidé par Axel (06/11/2026), détails proposés |

Statut au 06/11/2026. « Proposée » = écrite par l'agent d'après les recherches, pas encore tranchée par Axel ; le code s'y conforme en attendant. Les questions ouvertes (mot d'activation, STT/TTS locaux, nom du projet, dépôt, licence, version d'ESP-IDF, fraîcheur du calendrier) sont listées dans [le plan](../plan.md#décisions-à-prendre-par-axel).
