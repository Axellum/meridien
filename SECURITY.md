# Security

## English · [Français](#version-française)

---

Meridien is a personal project, shared in case it is useful. There is no promise of support, of a fix or of a reply time. No version has been released yet: only `main` is looked at.

### Reporting a problem

Do not post a security problem, a key or a password in a public issue. Use GitHub's private vulnerability reporting (the *Security* tab, “Report a vulnerability”) if it is enabled on the repository (not verified: it is a setting of the repository), otherwise contact the owner through their GitHub profile, [github.com/Axellum](https://github.com/Axellum).

### Known limits

These are things the project does **not** do, written down so that nobody is surprised. None of them is hidden in the code.

| Limit | What it means | Seen on |
|---|---|---|
| **The Wi-Fi password and the settings are stored in the flash without encryption.** | NVS encryption, flash encryption and secure boot are not enabled (no such option in `sdkconfig.defaults` nor in the board settings). Whoever holds the board can read them with `esptool read-flash`. [ADR-0003](docs/decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md) plans encrypted NVS for the secrets: **not done**, and the choice of scheme is the owner's. | read in the configuration files and in `main/reseau.cpp` (key `mdp`), 07/11/2026; no board needed |
| **The settings page is served over plain local HTTP.** | It sits behind a 6-digit pairing code shown on the device's screen (5 wrong codes lock it for 60 s; at most 4 sessions, kept in RAM). Nothing is encrypted on the way: anyone on the same Wi-Fi can read what is typed in the page, the Wi-Fi password and the API keys included. Use it on a network you trust. Contract: [docs/api-reglages.md](docs/api-reglages.md). | `main/serveur_web.cpp` (`httpd_start`, no TLS), the contract above, 07/11/2026 |
| **A binary built with the local Wi-Fi settings contains them in clear.** | `tools/preparer_wifi.ps1` can write the network name and password into the build (they never go through git). The binary then carries them as plain text. **Never share such a binary**; build without them for anything you give away. | `main/identifiants_wifi.h.in`, `tools/preparer_wifi.ps1` |
| **The radio coprocessor (ESP32-C6) is never updated.** | The project pins the Wi-Fi link to the version that works with the C6 as it comes from the factory, and never proposes to update it (an update that fails can leave the screen unusable). Flaws of that firmware are not patched here. | [AGENTS.md](AGENTS.md) rule 7, [THIRD_PARTY.md](THIRD_PARTY.md) |
| **Third-party services see your requests.** | The weather comes from Open-Meteo (its free API is for non-commercial use only). A speech or language service you configure receives its key and your requests over HTTPS. The key never leaves the device otherwise (rule 1 of the contract above). | [README](README.md#licence-and-credits), [docs/api-reglages.md](docs/api-reglages.md) |

---

## Version Française

---

Meridien est un projet personnel, partagé au cas où il servirait. Aucune promesse de support, de correction ni de délai de réponse. Aucune version n'est sortie : seul `main` est regardé.

### Signaler un problème

Ne postez pas un problème de sécurité, une clé ou un mot de passe dans une issue publique. Utilisez le signalement privé de vulnérabilités de GitHub (onglet *Security*, « Report a vulnerability ») s'il est activé sur le dépôt (non vérifié : c'est un réglage du dépôt), sinon contactez le propriétaire par son profil GitHub, [github.com/Axellum](https://github.com/Axellum).

### Limites connues

Ce sont des choses que le projet **ne fait pas**, écrites pour que personne ne soit surpris. Aucune n'est cachée dans le code.

| Limite | Ce que cela veut dire | Vu sur |
|---|---|---|
| **Le mot de passe Wi-Fi et les réglages sont gardés dans la flash sans chiffrement.** | Le chiffrement de la NVS, celui de la flash et le démarrage sécurisé ne sont pas activés (aucune option de ce genre dans `sdkconfig.defaults` ni dans les réglages des cartes). Qui tient la carte peut les lire avec `esptool read-flash`. L'[ADR-0003](docs/decisions/0003-reglages-par-page-web-et-meteo-open-meteo.md) prévoit une NVS chiffrée pour les secrets : **pas fait**, et le choix du schéma revient au propriétaire. | lu dans les fichiers de configuration et dans `main/reseau.cpp` (clé `mdp`), 07/11/2026 ; aucune carte nécessaire |
| **La page de réglages est servie en HTTP local simple.** | Elle est derrière un code d'appairage à 6 chiffres affiché sur l'écran de l'appareil (5 codes faux la verrouillent 60 s ; 4 sessions au plus, gardées en RAM). Rien n'est chiffré en route : toute personne sur le même Wi-Fi peut lire ce qu'on tape dans la page, mot de passe Wi-Fi et clés d'API compris. À utiliser sur un réseau de confiance. Contrat : [docs/api-reglages.md](docs/api-reglages.md). | `main/serveur_web.cpp` (`httpd_start`, sans TLS), le contrat ci-dessus, 07/11/2026 |
| **Un binaire compilé avec les réglages Wi-Fi locaux les contient en clair.** | `tools/preparer_wifi.ps1` peut écrire le nom et le mot de passe du réseau dans la compilation (ils ne passent jamais par git). Le binaire les porte alors en texte lisible. **Ne partagez jamais un tel binaire** ; compilez sans eux tout ce que vous donnez. | `main/identifiants_wifi.h.in`, `tools/preparer_wifi.ps1` |
| **Le coprocesseur radio (ESP32-C6) n'est jamais mis à jour.** | Le projet épingle la liaison Wi-Fi à la version qui marche avec le C6 tel qu'il sort d'usine, et ne propose jamais de le mettre à jour (une mise à jour ratée peut laisser l'écran inutilisable). Les défauts de ce micrologiciel ne sont pas corrigés ici. | [AGENTS.md](AGENTS.md) règle 7, [THIRD_PARTY.md](THIRD_PARTY.md) |
| **Des services tiers voient vos requêtes.** | La météo vient d'Open-Meteo (son API gratuite est réservée à un usage non commercial). Un service de voix ou de langage que vous configurez reçoit sa clé et vos requêtes en HTTPS. La clé ne sort jamais de l'appareil autrement (règle 1 du contrat ci-dessus). | [README](README.md#licence-et-crédits), [docs/api-reglages.md](docs/api-reglages.md) |
