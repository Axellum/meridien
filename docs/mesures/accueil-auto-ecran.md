# Accueil, mode automatique branché à l'écran — ce qui est constaté sur le PC et à la compilation, ce qui reste à voir sur une carte

> **Summary (EN)** — The home screen's automatic mode (pastille « Auto · … », rain card, enlarged alert banner; ADR-0023) was
> **seen on PC renders and compiled for both boards only**: 1 694 core tests with AddressSanitizer and UBSan (64 of them new for the
> pilot and the cards), 104 targeted mutations all detected, the new tests also evaluated by ESP-IDF's g++ 14.2 (syntax mode),
> 117 LVGL render variants looked at (13 states, 3 sizes, German and Turkish checked for truncation), ESP-IDF 5.5.5 builds of the
> Tab5 and the Waveshare 7B from a clean tree (+17.6 KB of flash image on each, +512 bytes of static RAM, both in `.data`).
> **Not seen on a board**: the drawing on the real panels, the touch (and the freeze under the finger), free internal RAM while
> running, drawing time, how the five AI-written languages read to a native speaker, whether the composition matches the sketches
> (they were not found).

> **Mise à jour du 09/11/2026 (ADR-0027)** : la grande carte pluie décrite ci-dessous **n'est plus choisie** ; la bande de pluie de la ligne centrale la remplace ([docs/mesures/bande-pluie.md](bande-pluie.md)). Les chiffres de cette page datent du 08/11/2026 et ne sont pas refaits ; les captures `auto-pluie-*`, `auto-jaune-et-pluie-1024x600` et `auto-alerte-rouge-et-pluie-*` ont été refaites et montrent maintenant la bande (la planche des sept langues, elle, montre encore l'ancienne carte).

**Vu sur** : PC d'Axel (Windows 11) ; clang++ 23.1.5 avec AddressSanitizer + UBSan (tests), g++ 14.2.0 croisé d'ESP-IDF en mode
« syntaxe » (mêmes tests, rien d'exécuté), clang-format 23.1.5 ; LVGL 9.5.0 (rendu PC) ; ESP-IDF 5.5.5 (compilation) ; commit `bbf6799`
(arbre propre, version `jalon-1a-tab5-st7121-284-gbbf6799`, sans « -dirty »), 08/11/2026. **Aucune carte** (ni Tab5 ni 7B), **rien flashé**,
aucun port série ouvert. La 7B reste « prévue » : le rendu 1024×600 ne simule que sa taille.

Règle de lecture : « mesuré » = relu sur une sortie d'outil de cette session ; tout le reste est une lecture de code ou **non vérifié**.

## 1. Question

Le noyau de décision du mode automatique ([ADR-0015](../decisions/0015-accueil-mode-automatique.md)) est-il branché à l'accueil de façon que
l'écran montre ce que les règles décident, sans rien casser, sans redessiner sous le doigt et sans montrer un état vide ?

## 2. Constaté

| Quoi | Résultat | Comment |
|---|---|---|
| Tests du noyau | **1 694 tests, 0 échec** (76 fichiers) ; dont 37 pour le pilote, 27 pour les cartes, 1 pour les textes | `python test/host/lancer.py`, mode « complet (compilé + exécuté) », clang++ 23.1.5, ASan + UBSan ; rejoué à chaque commit par le crochet `verifier.ps1 -Rapide` |
| Mutations ciblées | **104 sur 104 détectées** (0 problème), chacune par le test prévu ; 101 sont nouvelles (56 pilote, 45 cartes) | `python test/host/lancer.py --mutations --seulement test_accueil_pilote,test_accueil_cartes,test_visuel_textes` |
| Mêmes tests avec le compilateur d'ESP-IDF | `test_accueil_pilote.cpp` et `test_accueil_cartes.cpp` : compilation OK, `static_assert` évalués ; **mode « syntaxe », rien d'exécuté** | `python test/host/lancer.py --cxx …\riscv32-esp-elf-g++.exe --seulement test_accueil_pilote,test_accueil_cartes,test_visuel_textes` |
| Rendus | **117 PNG** = 13 états × 3 tailles (1280×720, 1024×600, 720×1280) × 3 langues (français, allemand, turc) ; **regardés** (les coupures « … » et les chevauchements ont été corrigés après ce regard : deux phrases allemande et turque raccourcies, durée et cumul sur deux lignes quand une langue, le néerlandais par exemple, est trop longue) ; 27 images gardées dans `docs/captures/ecran/` (`auto-*.png`, dont la planche des sept langues en 1024×600) | `.\tools\rendre.ps1 -Scene auto -Instant 2026-11-08T07:45:00Z` ; le pilote réel décide chaque variante |
| Compilation Tab5 | réussie : `meridien.bin` **3 012 384 octets** (28 % libres) ; avant : 2 994 768 → **+17 616 octets** (+0,59 %) | `.\tools\idf.ps1 -DBOARD=tab5 set-target esp32p4` puis `build`, depuis un arbre propre ; carte relue dans `BOARD:STRING=tab5` et `bsp_tab5` dans l'ELF (aucun `bsp_waveshare_7b`) |
| Compilation 7B | réussie : `meridien.bin` **2 992 976 octets** (29 % libres) ; avant : 2 975 232 → **+17 744 octets** (+0,60 %) | même commande avec `waveshare-7b` ; `BOARD:STRING=waveshare-7b` et `bsp_waveshare_7b` dans l'ELF (aucun `bsp_tab5`) |
| RAM statique (DIRAM) | Tab5 : 146 206 → **146 718 octets** (+512) ; 7B : 146 202 → **146 714 octets** (+512) ; **`.bss` inchangé** (39 256 / 39 040), tout est dans `.data`, dont 184 octets pour `g_pilote` | `idf.ps1 size` et `riscv32-esp-elf-nm -S` sur les ELF |
| Le code est lié au binaire | `dessiner_pastille_mode`, `dessiner_carte_pluie`, `placer_carte_alerte`, `g_pilote`, `Pilote::avancer`, `composer_carte_pluie`, `disposer_auto` sont dans les deux ELF | `riscv32-esp-elf-nm -C` (un « build OK » seul ne le prouve pas) |
| Objets LVGL de l'accueil | **aucun objet de plus** : la pastille, la carte pluie et le bandeau sont dessinés dans les objets de zone existants (`ui_principal_cout()` n'est pas modifié) | lecture du code ; **le décompte à l'exécution n'a pas été refait** |

Les chiffres « avant » viennent d'une compilation propre du commit `3434f9f` (parent de ce travail), les « après » du commit `bbf6799`.

## 3. Ce que les rendus montrent (états regardés)

| État | Ce qui s'affiche | Capture |
|---|---|---|
| Défaut | pastille « Auto · Météo » à côté du lieu, accueil inchangé | `auto-defaut-*` |
| Pluie proche / en cours | carte pluie sur les cinq jours : compte à rebours ou « Pluie en cours », force, durée (seulement si la pluie finit dans les données), cumul, barres des heures avec repères 1 et 3 mm/h | `auto-pluie-proche-*`, `auto-pluie-en-cours-*`, `auto-jaune-et-pluie-*` |
| Alerte orange / rouge | bandeau d'alerte agrandi, petite tuile de pluie à sa droite (dessous en portrait), cinq jours réduits à une bande | `auto-alerte-orange-*`, `auto-alerte-rouge-et-pluie-*`, `auto-alerte-orange-sans-pluie-*` |
| Les deux règles vraies | **l'alerte gagne**, la pluie reste dite par la tuile | `auto-alerte-rouge-et-pluie-*` |
| Alerte à titre long, alerte avec météo périmée | le titre de l'émetteur tient sur deux lignes au plus, coupé par « … » au-delà (voulu) ; météo périmée : la grande carte reste et la tuile dit « Données de 07:52 · périmées » | `auto-alerte-titre-long-*`, `auto-alerte-perimee-*` |
| Profil épinglé | « Météo · épinglé » puis « Auto en pause », pas de carte pluie ; le bandeau d'alerte agrandi reste | `auto-epingle-*`, `auto-epingle-et-alerte-*` |
| Nom de lieu très long | le lieu est coupé avant la pastille, jamais l'inverse | `auto-ville-longue-1024x600.png` |
| Sept langues | la planche des sept langues de la carte pluie tient sans « … » | `auto-planche-sept-langues-1024x600.png` |

L'épinglage se rend ici par une variante du banc (le pilote reçoit un profil épinglé) : **aucun réglage ne le fait encore** dans cette PR.

## 4. Non constaté

- **Tout ce qui se joue sur une carte** : le dessin sur les panneaux (couleurs réelles, lisibilité à un mètre et à deux), le toucher et le gel sous le doigt (10 s au plus), la mémoire interne libre en marche, le temps de dessin, le comportement sur une vraie journée.
- Les **cinq langues écrites par l'IA** (allemand, néerlandais, espagnol, italien, turc) : jamais relues par un locuteur ; deux formes ont été raccourcies pour tenir (turc « sonra %s'de şiddetlenir », allemand « %s mm gesamt »). Le néerlandais, l'espagnol et l'italien n'ont été regardés (pour les coupures) que sur la planche des sept langues en 1024×600 ; l'allemand et le turc l'ont été sur les 13 états et les 3 tailles.
- **La ressemblance avec les esquisses** `auto-pluie` et `auto-alerte` : elles n'ont pas été retrouvées. Les nombres de la composition (bande de 92 px, tuile de 360 × 88 px, écart de 16 px, bandeau d'au moins 150 px) sont **choisis par l'auteur** d'après leur description, pas mesurés.
- Les **seuils** (60 min, 3 h, 30 min…) : ceux de l'ADR-0015, « règle de Vigie, à valider », aucun n'est officiel.
- Les entrées qui n'existent pas encore sur l'appareil (activités, spots, seuil de vent, mer, basse mer) : « inconnues », donc jamais une bascule ; Surf, Vent·Bateau et Côte ne se dessinent pas.
