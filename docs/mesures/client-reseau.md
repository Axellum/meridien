# Client réseau commun et réponses gzip — compilation seulement, aucune carte

> **Summary (EN)** — The weather request now goes through a common HTTPS client (`main/http_commun.{h,cpp}`) that sends `Accept-Encoding: gzip`, decompresses a gzip answer with `tinfl_decompress` from the ESP32-P4 ROM (no decompression code added to the flash) and checks the CRC32 and size written in the gzip trailer. The gzip envelope (header, trailer, CRC32, `Content-Encoding` decision) is a pure core header, `components/core/include/ecran/gzip.hpp`, tested on the PC.
> - **Measured (PC only)**: two real Open-Meteo answers captured on 07/11/2026 are 1,498 and 1,626 bytes on the wire for 4,443 and 4,575 bytes of text (34 % and 36 %). `test_gzip.cpp`: 23 tests, 73 mutations, all detected (clang++ 23.1.5, AddressSanitizer + UBSan). Both boards build: Tab5 2,834,336 → 2,839,440 bytes (+5,104), Waveshare 7B 2,814,896 → 2,820,016 (+5,120).
> - **Not verified (no board)**: nothing of this has run on a board. In particular the ROM decompressor was never executed (flags, status on a full output buffer, bytes consumed), the real `Content-Encoding` header event of `esp_http_client`, the time and memory cost, and the behaviour on the 7B.
> - Buffers: 64 KB receive + 64 KB text + 10,992 bytes decompressor, all in PSRAM (expected peak 142,064 bytes during a gzip request, against 16,384 before); internal RAM is not asked for anything more.

**Vu sur** : PC seulement, compilation ; aucune carte. PC d'Axel (Windows 11), clang++ 23.1.5 avec AddressSanitizer + UBSan (`_outils\env_hote.ps1 -Sanitizers`), ESP-IDF 5.5.5 pour les deux compilations ; base `9c89ad5`, branche `worktree-agent-a4c19a135e3c33e73`, le 07/11/2026. La MAC et l'adresse IP ne figurent pas ici.

Règle de lecture : ce qui n'est pas marqué « mesuré » est une lecture de source ou de code ; ce qui n'a pas été observé est écrit **non vérifié**.

## 1. Question

La réponse météo riche fait 4 427 à 4 685 octets sur le fil. Peut-on la recevoir compressée, sans ajouter de code de décompression à la flash, sans toucher à la mémoire interne, et préparer un client commun pour les autres fournisseurs de météo (qui rendront 22 à 41 Ko) ?

## 2. Ce qui a été fait

| Élément | Fichier | Rôle |
|---|---|---|
| Enveloppe gzip (noyau pur) | `components/core/include/ecran/gzip.hpp` | en-tête RFC 1952 (FEXTRA, FNAME, FCOMMENT, FHCRC compris) → décalage du flux DEFLATE ; pied (CRC32 + taille) ; CRC32 ; « ressemble à du gzip » ; décision sur `Content-Encoding` |
| Tests | `test/host/test_gzip.cpp`, `mutations_gzip.py` | 23 tests (tableaux `constexpr` de la taille exacte du cas : une lecture hors tampon ne compile pas), 73 mutations |
| Données | `test/host/donnees/gzip_cas.inc`, `openmeteo_gzip_*.{gz,inc}` | 12 fichiers fabriqués par Python (`zlib`), 2 réponses réelles d'Open-Meteo ; voir `donnees/LISEZMOI.md` |
| Client commun | `main/http_commun.h`, `http_commun.cpp` | un GET HTTPS : adresse, `User-Agent` et `Accept` obligatoires de l'appelant ; `Accept-Encoding: gzip` ; décompression par la ROM ; verrou TLS ; tampons en PSRAM ; journal des tailles |
| Appelant | `main/meteo_client.cpp` | n'a plus de code HTTP : il construit l'adresse (inchangée), appelle `client_http::obtenir`, lit le texte rendu |

Choix qui ne se déduisent pas du code :

- **Taille du tampon : 64 Ko** (reçu) et 64 Ko (texte décompressé). Réponses connues : Open-Meteo riche 4 427 à 4 685 octets ; MET Norway 40 712 octets (91 pas) ; Bright Sky 22 198 octets (49 h) — ces deux derniers chiffres viennent du volet « fournisseurs » de la mission, pas remesurés ici. 64 Ko laisse ≈ 1,6 fois la plus grande ; au-delà : `Issue::TropGrand`. C'est aussi la borne d'une « bombe » gzip (quelques centaines d'octets qui gonflent de plusieurs Mo) : la décompression s'arrête au tampon.
- **Une réponse non compressée reste acceptée** (un fournisseur ou un intermédiaire peut ignorer `Accept-Encoding`). Table de décision (`gzip::decider`) : gzip annoncé et signature `1f 8b` présente → décompressée ; gzip annoncé sans signature → refusée ; rien annoncé et signature présente → décompressée (un JSON ne commence jamais par `1f`) ; rien annoncé, pas de signature → telle quelle ; tout autre codage (br, zstd, liste…) → refusée, l'appareil n'en demande aucun.
- **Un seul membre gzip.** Le pied est lu sur les 8 derniers octets : un second membre ou des octets en trop derrière le premier ne passent pas la vérification du CRC32 (testé sur un fichier de deux membres collés).
- **Le CRC32 et la taille sont vérifiés APRÈS la décompression**, sur ce que `tinfl` a rendu, contre le pied du fichier.
- **User-Agent d'Open-Meteo : un nom fixe**, `Meridien/ESP32-P4 (open-meteo-client)` : rien à formater sur la pile de 12 Ko, rien de variable ni de privé envoyé (ni version, ni identifiant). Open-Meteo n'exige pas de contact ; MET Norway l'exige et son appelant donnera le sien.
- **L'adresse n'est jamais au journal** (elle contient la position de l'appareil) : seulement les tailles et le statut.
- **Verrou TLS** pris de `esp_http_client_init` à `esp_http_client_cleanup`, comme avant ; pas pendant la décompression. `buffer_size_tx` = 1024 conservé.
- Une redirection rouvre une connexion : le corps et le `Content-Encoding` déjà reçus sont jetés à chaque `HTTP_EVENT_ON_CONNECTED`, pour qu'un en-tête d'une première réponse ne s'applique pas à la seconde.

## 3. Constaté sur le PC

### 3.1 Les deux réponses réelles (capture du 07/11/2026, `Accept-Encoding: gzip`, HTTP 200, `Content-Encoding: gzip`)

| Lieu | Sur le fil | Texte décompressé | Part du fil |
|---|---:|---:|---:|
| Lyon (45,74906 N ; 4,84789 E) | 1 498 octets | 4 443 octets | 34 % |
| Sydney (33,86785 S ; 151,20732 E) | 1 626 octets | 4 575 octets | 36 % |

En-tête gzip des deux : `1f 8b 08 00 00 00 00 00 00 03` (aucun champ facultatif, MTIME 0, système 3), donc le flux commence au décalage 10 et le pied fait les 8 derniers octets. Le CRC32 et la taille du pied sont ceux du texte décompressé par Python (`zlib.crc32`, `gzip.decompress`).

### 3.2 Tests

`python test/host/lancer.py --seulement test_gzip` : mode « complet (compilé + exécuté) », 23 tests, 0 échec, avec AddressSanitizer + UBSan. `--mutations` : 73 mutations (signature, méthode, bits réservés, chaque champ facultatif et sa troncature, CRC16 d'en-tête, pied petit-boutiste, taille supérieure à 65 535, polynôme et inversions du CRC32, décision sur `Content-Encoding`), toutes détectées, aucune équivalente conservée. Vecteurs du CRC32 de la littérature (« 123456789 » → `0xCBF43926`). `ruff` et `clang-format` sans écart.

**Ce que ces tests ne prouvent pas** : la décompression elle-même. Elle est faite par `tinfl_decompress` de la ROM du P4, qui n'a jamais été exécutée : `nm` sur le fichier ELF lié donne `4fc000f8 A tinfl_decompress` (symbole absolu en ROM, donc aucun code de décompression ajouté à la flash ; `tinfl_init` est une macro de `miniz.h`, sans symbole).

### 3.3 Compilation (ESP-IDF 5.5.5, `tools/idf.ps1`, `CMAKE_BUILD_PARALLEL_LEVEL=4`)

| Carte | Avant (base `9c89ad5`) | Après | Écart | Libre dans la partition d'application de 4 Mo (après, calculé) |
|---|---:|---:|---:|---:|
| Tab5 | 2 834 336 octets | 2 839 440 octets | +5 104 | 1 354 864 octets |
| Waveshare 7B | 2 814 896 octets | 2 820 016 octets | +5 120 | 1 374 288 octets |

SHA-256 des binaires `meridien.bin` (le Tab5 « avant » et « après » sont deux compilations du même arbre, moins et plus ce lot) :

| Binaire | SHA-256 |
|---|---|
| Tab5 avant | `592CA8A71DE1F24FBB1A95AD01756549C876876FE503496CF325A401D70AA9D1` |
| Tab5 après | `CCDAD30BEAA340C5BF03D72B4FE37C5A2F42760B2377BBB79EC4206D1A64D4FC` |
| 7B avant | `C8262D3D471A74591CA25CE87FC2257E1C81B61A1BBB1C130A9E2D4302524B02` |
| 7B après | `057CA3AA65534BD787CBDCA5633A3F38A3931F550FA4354C8927D467FABFF2D0` |

`sizeof(tinfl_decompressor)` = 10 992 octets (`0x2af0`), mesuré en compilant `miniz.h` avec le compilateur RISC-V d'ESP-IDF (`esp-14.2.0_20260121`) : c'est trop pour la pile de 12 Ko de la tâche, d'où l'allocation en PSRAM. Aucun avertissement du compilateur dans `http_commun.cpp` ni `meteo_client.cpp` pour les quatre compilations.

## 4. Mémoire prévue (calcul, **non vérifié sur carte**)

| | Avant | Après (réponse gzip) |
|---|---:|---:|
| Tampon de réception, PSRAM | 16 384 o | 65 536 o |
| Tampon de texte décompressé, PSRAM | — | 65 536 o |
| Décompresseur, PSRAM | — | 10 992 o |
| **Pic en PSRAM pendant l'essai** | 16 384 o | **142 064 o** (+125 680) |
| RAM interne demandée en plus | — | aucun tampon ; quelques structures locales (état de réception, réponse, résultat de décompression) sur la pile de la tâche, non mesurées |

Les tampons sont rendus avant le retour de `client_http::obtenir`. La prise de contact TLS (le pic de RAM interne : ≈ 65 Ko, mesuré par le jalon 1b le 06/11/2026, voir `main/verrou_tls.h`) est inchangée ; la décompression a lieu après `esp_http_client_cleanup` et la remise du verrou TLS, hors de ce pic (lecture du code, `http_commun.cpp`).

## 5. Non vérifié (à voir sur une carte)

1. `tinfl_decompress` avec `TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF`, sans en-tête zlib ni « plus d'entrée » : rend-il `TINFL_STATUS_DONE` sur les deux réponses réelles, `TINFL_STATUS_HAS_MORE_OUTPUT` sur un texte qui dépasse le tampon (lu dans la documentation de `miniz.h`, jamais exécuté), et combien d'octets du flux consomme-t-il (le journal le dit : « flux DEFLATE lu sur A octets de B ») ?
2. `esp_http_client` livre-t-il bien l'évènement `HTTP_EVENT_ON_HEADER` avec la clé `Content-Encoding` (comparée sans tenir compte de la casse) ? Sinon la signature `1f 8b` suffit encore à reconnaître le gzip (rien annoncé + signature → décompressé), mais ce chemin n'est alors pas celui qu'on croit.
3. Le gain réel (octets lus par TLS, durée de l'aller-retour, RAM interne pendant la poignée de main) et le coût de la décompression (journal : « décompression … µs »).
4. Le pic de PSRAM et la marge de la pile de la tâche (« pile de la tâche : … octets jamais utilisés » au journal).
5. La 7B : compilée, jamais essayée.
6. Un serveur qui annonce `br` ou `zstd` malgré `Accept-Encoding: gzip`, ou qui envoie un gzip piégé : refus prévu et testé sur le noyau, pas essayé de bout en bout (il faudrait un serveur local).

Journal à chercher sur la carte, balise `http` : `réponse : HTTP 200, gzip, N octets sur le fil, M octets décompressés (P % du texte), décompression X µs ; flux DEFLATE lu sur A octets de B`. Les lignes `METEO …` du client météo sont inchangées (le nombre d'octets qu'elles donnent est celui du texte décompressé). Deux changements de journal : « requête échouée » et « verrou TLS occupé » portent maintenant la balise `http` au lieu de `meteo`, et « mémoire libre une fois la connexion TLS établie » vient après les lignes de lecture de la réponse au lieu d'avant.

## 6. Hors périmètre

La recherche de lieu (`main/geocodage.cpp`, `main/hote_appareil_esp.cpp`) garde ses propres appels HTTPS : elle ne passe pas par ce client. Le client Vigie ne dépend pas de ce module.
