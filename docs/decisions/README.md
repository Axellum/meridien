# Architecture decisions (ADR) / Décisions d'architecture

## English · [Français](#version-française)

A non-obvious decision gets one numbered page here (next number), with the context, the choice, the alternatives that were
rejected and what would reopen it. Read the relevant page **before** "fixing" something that looks wrong. The pages are
written in French; each of 0001 to 0005, 0008, 0009, 0010, 0012, 0013, 0014, 0015, 0016 and 0017 opens with a short English summary. A summary says no more than the French text
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
| [0008](0008-ecran-principal-par-zones-themes-et-ciel-trame.md) | Main screen: one drawing object per zone, themes and decisions in the pure core, a dithered sky, original icons | proposed, awaiting the owner's validation (seen on PC renders only) |
| [0009](0009-alertes-officielles-par-vigie.md) | Official weather alerts read from Vigie, shown only while one is in force, the issuer's words never rewritten | proposed, awaiting the owner's validation (checked on the PC only, not seen on a board) |
| [0010](0010-source-meteo-par-pays-et-client-reseau-commun.md) | Weather source chosen by country (Open-Meteo national models), one common network client | proposed, awaiting the owner's validation (seen on PC only) |
| [0012](0012-l-heure-affichee-est-celle-du-lieu-choisi.md) | The time on screen is the time of the chosen place: the clock, forecast hours, sunrise and sunset, day names and alert hours follow the saved zone's time zone | proposed, awaiting the owner's validation (seen on PC only, never on a board) |
| [0013](0013-surf-donnees-note-et-limites.md) | Surf: spots typed by the person, Open-Meteo marine and wind data, offshore / cross-shore / onshore classes, an indicative tide and a house 0-100 score; only the pure core exists, no screen yet | proposed, awaiting the owner's validation (seen on PC only, never on a board) |
| [0014](0014-met-norway-et-nws-comme-sources.md) | MET Norway and the NWS read directly, key-less, as weather sources; fallback on Open-Meteo | proposed, awaiting the owner's validation (seen on PC only, compiled for both boards, never run on a board) |
| [0015](0015-accueil-mode-automatique.md) | Home screen: the automatic mode picks the profile (Weather, Surf, Wind·Boat, Coast) and says why; enlarged alert banner and rain card as additions; guards (one change per 30 min, never during a touch, stale data, pin); only the pure core exists, no screen yet | proposed, awaiting the owner's validation (seen on PC only, never on a board) |
| [0016](0016-accueil-aere-pluie-et-alerte-en-bandeau.md) | Home screen redone: 12-hour curve, one banner for the rain to come and the official alert, five days; one LVGL object per zone, logic in the pure core | proposed, awaiting the owner's validation (seen on PC renders only, compiled for both boards, never run on a board) |
| [0017](0017-roue-d-appui-long-geometrie-pure.md) | Press-and-hold wheel: the Tab5's geometry ported into the pure core (hub, rings, words, touch), the hub sliding away from the finger when the wheel does not fit | proposed, awaiting the owner's validation (seen on PC only, never on a board or a screen) |
| [0020](0020-page-details-sept-grandeurs.md) | Page « Détails » : les sept grandeurs retirées de l'accueil sur une page plein écran, « — » pour une grandeur absente, les dernières valeurs grisées pour une météo périmée, texte jamais sous 24 pixels réels, noyau pur + dessin LVGL + colle séparés ; rien ne l'ouvre encore | proposée, à valider par Axel (vue sur des rendus du PC seulement, compilée pour les deux cartes, jamais lancée sur une carte) |

Status as of 2026-11-06. "Proposed" means written by the AI agent from the research notes and not yet settled by the owner;
the code follows it in the meantime. The open questions (wake word, local STT/TTS, project name, repository, licence, ESP-IDF
version, calendar freshness) are listed in [the plan](../plan.md#décisions-à-prendre-par-axel) (in French, with an English
summary at the top).

---

## Version française

Une décision non évidente = une page numérotée ici (numéro suivant), avec le contexte, le choix, les alternatives écartées et ce qui la remettrait en cause. À lire **avant** de « corriger » quelque chose qui a l'air faux. Les pages sont en français ; chacune de 0001 à 0005, 0008, 0009, 0010, 0012, 0013, 0014, 0015, 0016 et 0017 commence par un court résumé en anglais, qui n'en dit pas plus que le texte français, statut « proposée » compris.

| N° | Décision | Statut |
|---|---|---|
| [0001](0001-pile-esp-idf-natif.md) | Pile : ESP-IDF natif + LVGL 9 | proposée, à valider par Axel |
| [0002](0002-assistant-vocal-en-cascade.md) | Assistant vocal : mot d'activation sur l'appareil, puis cascade STT → LLM → TTS à trois fournisseurs indépendants | proposée, à valider par Axel |
| [0003](0003-reglages-par-page-web-et-meteo-open-meteo.md) | Réglages par une page servie par l'appareil ; météo Open-Meteo | proposée, à valider par Axel |
| [0004](0004-calendrier-par-adresse-ical-secrete.md) | Calendrier Google (phase 2) : adresse iCal secrète | proposée, à valider par Axel |
| [0005](0005-couche-carte.md) | Une couche carte : développer sur le Tab5, garder la 7B compilable | proposée, à valider par Axel |
| [0006](0006-ecran-en-sept-langues.md) | L'écran parle sept langues, avec le système de traduction du Tab5 | principe décidé par Axel (06/11/2026), détails proposés |
| [0007](0007-reglages-a-l-ecran-et-installation-guidee.md) | Tous les réglages à l'écran, la page web en second chemin, installation guidée au premier démarrage et rejouable | principe décidé par Axel (06/11/2026), détails proposés |
| [0008](0008-ecran-principal-par-zones-themes-et-ciel-trame.md) | Écran principal : un objet de dessin par zone, thèmes et décisions dans le noyau pur, un ciel tramé, des icônes originales | proposée, à valider par Axel (vue sur rendus PC seulement) |
| [0009](0009-alertes-officielles-par-vigie.md) | Alertes officielles lues chez Vigie, montrées seulement quand elles sont en vigueur, mots de l'émetteur jamais réécrits | proposée, à valider par Axel (constatée sur le PC seulement, jamais vue sur une carte) |
| [0010](0010-source-meteo-par-pays-et-client-reseau-commun.md) | Source de la météo choisie par pays (modèles nationaux d'Open-Meteo), un client réseau commun | proposée, à valider par Axel (vue sur le PC seulement) |
| [0012](0012-l-heure-affichee-est-celle-du-lieu-choisi.md) | L'heure affichée est celle du lieu choisi : l'horloge, les heures de la prévision, le lever et le coucher du soleil, les noms de jours et les heures des alertes suivent le fuseau de la zone enregistrée | proposée, à valider par Axel (vue sur le PC seulement, jamais sur une carte) |
| [0013](0013-surf-donnees-note-et-limites.md) | Surf : spots saisis par la personne, données mer et vent d'Open-Meteo, vent offshore / de travers / onshore, une marée indicative et un barème maison de 0 à 100 ; seul le noyau pur existe, pas encore d'écran | proposée, à valider par Axel (vue sur le PC seulement, jamais sur une carte) |
| [0014](0014-met-norway-et-nws-comme-sources.md) | MET Norway et le NWS lus directement, sans clé, comme sources météo ; repli sur Open-Meteo | proposée, à valider par Axel (vue sur le PC seulement, compilée pour les deux cartes, jamais lancée sur une carte) |
| [0015](0015-accueil-mode-automatique.md) | Accueil : le mode automatique choisit le profil (Météo, Surf, Vent·Bateau, Côte) et dit pourquoi ; bandeau d'alerte agrandi et carte pluie en ajouts ; garde-fous (un changement par 30 min, jamais pendant un toucher, donnée périmée, épinglage) ; seul le noyau pur existe, pas encore d'écran | proposée, à valider par Axel (vue sur le PC seulement, jamais sur une carte) |
| [0016](0016-accueil-aere-pluie-et-alerte-en-bandeau.md) | Accueil refait : courbe des 12 heures, un bandeau pour la pluie à venir et l'alerte officielle, cinq jours ; un objet LVGL par zone, la logique dans le noyau pur | proposée, à valider par Axel (vu sur des rendus du PC seulement, compilé pour les deux cartes, jamais lancé sur une carte) |
| [0017](0017-roue-d-appui-long-geometrie-pure.md) | Roue d'appui long : la géométrie du Tab5 portée dans le noyau pur (moyeu, anneaux, mots, toucher), le moyeu qui s'écarte du doigt quand la roue ne tient pas | proposée, à valider par Axel (vue sur le PC seulement, jamais sur une carte ni sur un écran) |

Statut au 06/11/2026. « Proposée » = écrite par l'agent d'après les recherches, pas encore tranchée par Axel ; le code s'y conforme en attendant. Les questions ouvertes (mot d'activation, STT/TTS locaux, nom du projet, dépôt, licence, version d'ESP-IDF, fraîcheur du calendrier) sont listées dans [le plan](../plan.md#décisions-à-prendre-par-axel).
