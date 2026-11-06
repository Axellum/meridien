# ADR-0004 — Calendrier Google (phase 2) : adresse iCal secrète

- **Statut** : proposée le 06/11/2026, à valider par Axel. Concerne la version 2.
- **Contexte** : afficher les événements d'un agenda Google sans serveur intermédiaire. Recherche : [03-meteo-heure-reglages-calendrier.md](../recherche/03-meteo-heure-reglages-calendrier.md), section 4.
- **Constat** : le flux OAuth « appareils à saisie limitée » de Google **n'autorise pas** les scopes Calendar. L'OAuth classique en mode « Testing » fait **expirer les autorisations au bout de 7 jours**, et une application non vérifiée avec scopes sensibles est plafonnée à 100 utilisateurs à vie. Pas viable pour un projet ouvert.
- **Décision** : lire l'**adresse secrète au format iCal (ICS)** de l'agenda, en HTTPS, depuis l'appareil. L'utilisateur la colle dans la page de réglages ; stockée en NVS chiffré ; révocable côté Google (« Reset »).
- **Limites à annoncer à l'utilisateur** : fraîcheur des données de l'ordre de **6 à 24 h** d'après des sources tierces (aucune source Google, non vérifié) ; l'administrateur d'un compte pro peut interdire ce partage.
- **Analyse de l'ICS** : parseur léger sur l'appareil (`uICAL` à évaluer : MIT, mais EXDATE, VTIMEZONE et ESP32 non confirmés) ; **tester avec un vrai agenda** (événements « toute la journée », récurrences avec exceptions, fuseaux). Plan B : relais facultatif auto-hébergeable (ICS → JSON), au prix de l'indépendance.
- **Écartés** : OAuth Calendar sur l'appareil ; CalDAV Google (OAuth obligatoire) ; compte de service (non vérifié, mauvais pour un produit distribué).
