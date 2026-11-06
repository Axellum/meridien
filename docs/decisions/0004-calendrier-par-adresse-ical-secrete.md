# ADR-0004 — Calendrier Google (phase 2) : adresse iCal secrète

> **Summary (EN)** — Proposed on 2026-11-06, awaiting the owner's validation. Concerns version 2.
> - Goal: show the events of a Google calendar without an intermediate server.
> - Finding: Google's OAuth flow for limited-input devices does not allow the Calendar scopes; classic OAuth in "Testing" mode makes authorisations expire after 7 days, and an unverified app with sensitive scopes is capped at 100 users for life. Not viable for an open project.
> - Decision: the device reads the calendar's secret iCal (ICS) address over HTTPS. The user pastes it in the settings page; it is stored in encrypted NVS and can be revoked on Google's side ("Reset").
> - Limits to announce: freshness of about 6 to 24 h according to third-party sources (no Google source, not verified); an administrator of a work account may forbid this sharing. The ICS parser is still to evaluate (`uICAL`: MIT, but EXDATE, VTIMEZONE and ESP32 support not confirmed) and must be tried on a real calendar; plan B is an optional self-hosted relay (ICS to JSON), at the cost of independence.
> - Rejected: OAuth Calendar on the device, Google CalDAV (OAuth required), a service account (not verified, poor for a distributed product).

- **Statut** : proposée le 06/11/2026, à valider par Axel. Concerne la version 2.
- **Contexte** : afficher les événements d'un agenda Google sans serveur intermédiaire. Recherche : [03-meteo-heure-reglages-calendrier.md](../recherche/03-meteo-heure-reglages-calendrier.md), section 4.
- **Constat** : le flux OAuth « appareils à saisie limitée » de Google **n'autorise pas** les scopes Calendar. L'OAuth classique en mode « Testing » fait **expirer les autorisations au bout de 7 jours**, et une application non vérifiée avec scopes sensibles est plafonnée à 100 utilisateurs à vie. Pas viable pour un projet ouvert.
- **Décision** : lire l'**adresse secrète au format iCal (ICS)** de l'agenda, en HTTPS, depuis l'appareil. L'utilisateur la colle dans la page de réglages ; stockée en NVS chiffré ; révocable côté Google (« Reset »).
- **Limites à annoncer à l'utilisateur** : fraîcheur des données de l'ordre de **6 à 24 h** d'après des sources tierces (aucune source Google, non vérifié) ; l'administrateur d'un compte pro peut interdire ce partage.
- **Analyse de l'ICS** : parseur léger sur l'appareil (`uICAL` à évaluer : MIT, mais EXDATE, VTIMEZONE et ESP32 non confirmés) ; **tester avec un vrai agenda** (événements « toute la journée », récurrences avec exceptions, fuseaux). Plan B : relais facultatif auto-hébergeable (ICS → JSON), au prix de l'indépendance.
- **Écartés** : OAuth Calendar sur l'appareil ; CalDAV Google (OAuth obligatoire) ; compte de service (non vérifié, mauvais pour un produit distribué).
