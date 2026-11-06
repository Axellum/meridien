# ADR-0003 — Réglages par une page servie par l'appareil ; météo Open-Meteo

> **Summary (EN)** — Proposed on 2026-11-06, awaiting the owner's validation.
> - Settings: a page served by the device with `esp_http_server` over local HTTP, protected by a pairing token shown on the screen (self-signed HTTPS rejected: browser warning, about 40 KB of RAM per socket); stable address through mDNS; first set-up through a SoftAP and captive portal (to prototype: not shown to work on P4 + C6) with a fallback of on-screen entry and a QR code.
> - A light front end (Preact or plain JS), gzip, mobile first, light/dark theme, developed against a fake server on a PC. Geocoding goes through a proxy in the firmware because a phone on the SoftAP often has no internet (Open-Meteo's CORS is open, checked on 2026-11-06, so a fallback from the page is possible).
> - Weather: Open-Meteo, no key (the complete request measured 2,839 bytes), refreshed every 15–30 min, cached in NVS, "stale" fallback when offline. Free for non-commercial use only: selling the screen would need a paid plan or another source.
> - Secrets (API keys, iCal link) in encrypted NVS, never returned in clear by the API nor exported; the encryption scheme (flash encryption or HMAC, the latter consuming an eFuse block permanently) is postponed and needs the owner's agreement. Update through the page (`esp_ota_*`), two slots, automatic rollback after a self-test; Secure Boot v2 postponed (irreversible).
> - Time: SNTP plus an embedded `posix_tz_db` table (MIT) to go from an IANA name to a POSIX string; the last time zone is kept in NVS. Consequence: every setting has a page, an API and a safe default; none is compiled in.

- **Statut** : proposée le 06/11/2026, à valider par Axel.
- **Contexte** : plus de Home Assistant pour régler quoi que ce soit. Axel veut « une belle page web, claire et facile » pour la zone géographique de la météo et le paramétrage du LLM. Recherche : [03-meteo-heure-reglages-calendrier.md](../recherche/03-meteo-heure-reglages-calendrier.md).
- **Décision** :
  1. **Page servie par `esp_http_server`** sur l'appareil, en **HTTP local** protégé par un **jeton d'appairage affiché sur l'écran** (HTTPS auto-signé écarté : avertissement navigateur, ~40 Ko de RAM par socket). Adresse stable par mDNS. Mise en service par SoftAP + portail captif (à prototyper : non démontré sur P4+C6), avec repli par saisie sur l'écran et QR code.
  2. **Front léger** (Preact ou JS vanille + CSS moderne), gzip, mobile d'abord, thème clair/sombre, français ; développé contre un faux serveur sur PC.
  3. **Géocodage par un proxy dans le firmware** (`/api/geocode?q=`) plutôt que depuis le navigateur : le téléphone n'a souvent pas Internet pendant le SoftAP, et ça ne dépend pas du CORS. (Le CORS d'Open-Meteo est pourtant ouvert, `*`, **vérifié** le 06/11/2026 : repli possible depuis la page.) Recherche par préfixe ≥ 3 caractères avec anti-rebond.
  4. **Météo : Open-Meteo** (sans clé, 2 839 octets la requête complète mesurés), rafraîchie toutes les 15-30 min, cache en NVS, repli « périmé » hors ligne. Gratuit **non commercial** seulement : si l'écran était vendu, il faudrait un plan payant ou une autre source.
  5. **Secrets** (clés API, lien iCal) en NVS chiffré, jamais renvoyés en clair par l'API ni exportés. Le choix du schéma de chiffrement (flash encryption ou HMAC : le second consomme un bloc eFuse **définitivement**) est **reporté** et demandera l'accord d'Axel.
  6. **Mise à jour par la page** (`esp_ota_*`), deux slots, rollback automatique après auto-test. **Secure Boot v2 reporté** (irréversible).
- **Heure** : SNTP + table `posix_tz_db` (MIT) embarquée pour passer du nom IANA à la chaîne POSIX ; dernier fuseau gardé en NVS.
- **Conséquences** : tout réglage a une page, une API et un défaut sûr ; aucun réglage ne se compile.
