// Client HTTPS commun : un aller-retour GET qui rend le corps de la réponse, décompressé s'il est en gzip.
//
// Le client météo (main/meteo_client.cpp) l'appelle ; les autres fournisseurs de données sur Internet le feront aussi. Ce module ne
// connaît ni Open-Meteo ni aucun autre serveur : l'appelant donne l'adresse, le User-Agent et l'en-tête Accept, et reçoit le texte.
// Ce qu'il fait pour l'appelant :
//   - envoie `Accept-Encoding: gzip` et, si le serveur répond en gzip, rend le texte DÉCOMPRESSÉ (la réponse d'Open-Meteo passe de
//     4,4 Ko à 1,5 Ko sur le fil) ; une réponse non compressée reste acceptée telle quelle ;
//   - décompresse avec `tinfl_decompress` de la ROM de l'ESP32-P4 (aucun code de décompression de plus dans la flash) et vérifie le
//     CRC32 et la taille écrits dans le pied du fichier gzip (noyau pur : components/core/include/ecran/gzip.hpp, testé sur le PC) ;
//   - prend le verrou TLS commun (verrou_tls.h) autour de la connexion : de `esp_http_client_init` à `esp_http_client_cleanup`, et
//     pas pendant la décompression ;
//   - garde les tampons (réponse reçue, texte décompressé, décompresseur de 11 Ko) en PSRAM, jamais sur la pile de 12 Ko de la
//     tâche (la prise de contact TLS en demande plusieurs Ko), et les rend avant de sortir ;
//   - ne met JAMAIS l'adresse au journal (elle contient la position de l'appareil) : seulement les tailles et le statut.
// Une réponse de serveur n'est jamais crue : trop grande, gzip qui gonfle au-delà du tampon, flux ou pied faux, `Content-Encoding`
// inconnu ou menteur : refusée avec une issue précise, jamais lue à moitié.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::client_http {

// Taille du tampon de réception ET du tampon de texte décompressé : 64 Ko chacun, en PSRAM, rendus après chaque essai.
// Pourquoi 64 Ko : la plus grande réponse connue des fournisseurs de météo tient dans les deux tiers du tampon. Relevé du
// 07/11/2026 (test/host/donnees/) : Open-Meteo, réponse riche (courant, 24 heures, 7 jours, quarts d'heure) 4 427 à 4 685 octets
// bruts, 1 300 à 1 600 octets gzippés environ (mesuré ici : Lyon 1 498 → 4 443, Sydney 1 626 → 4 575) ; MET Norway 40 712 octets
// (91 pas de temps) et Bright Sky 22 198 octets (49 heures) : ces deux derniers chiffres viennent du volet « fournisseurs » de la
// mission, pas remesurés ici. 64 Ko laisse donc ≈ 1,6 fois la plus grande réponse ; une réponse plus grande est REFUSÉE
// (`Issue::TropGrand`), jamais lue à moitié. C'est aussi la borne d'une « bombe » gzip : un flux de quelques centaines d'octets qui
// gonfle de plusieurs mégaoctets s'arrête au tampon. Pourquoi en PSRAM : un tampon de plusieurs Ko en RAM interne mangerait le budget
// de la prise de contact TLS (docs/mesures/memoire-interne.md).
inline constexpr size_t kTailleTampon = 64 * 1024;

enum class Issue : uint8_t {
    Ok,
    Memoire,        // pas de place en PSRAM pour un tampon
    Requete,        // adresse, User-Agent ou Accept absents (ou fonction de lecture absente) : rien n'est envoyé
    Reseau,         // DNS, connexion, TLS, délai dépassé ou verrou TLS occupé trop longtemps
    Http,           // le serveur a répondu autre chose que 200
    TropDeRequetes, // HTTP 429
    TropGrand,      // réponse (ou texte décompressé) au-delà du tampon
    Illisible, // 200, mais la réponse n'est pas celle qu'on attend : gzip faux, Content-Encoding inconnu, lecture refusée
};

// Mémoire libre à un instant : l'appelant la journalise autour de la requête (la prise de contact TLS est le pic de RAM interne).
struct Memoire {
    size_t interne = 0;      // octets libres en RAM interne
    size_t interne_bloc = 0; // plus grand bloc libre en RAM interne
    size_t interne_min = 0;  // minimum de RAM interne libre depuis le démarrage
    size_t psram = 0;        // octets libres en PSRAM
    size_t psram_bloc = 0;   // plus grand bloc libre en PSRAM
};

Memoire mesurer_memoire();

struct Requete {
    // Les trois premiers champs sont OBLIGATOIRES (texte non vide, vivant pendant l'appel) : sans eux la requête n'est pas envoyée
    // (`Issue::Requete`). Un fournisseur peut refuser une requête sans identité (MET Norway exige un User-Agent qui l'identifie).
    const char *adresse = nullptr; // « https://… »
    const char *user_agent =
        nullptr;                  // qui parle : nom de l'appareil et, si le fournisseur l'exige, un moyen de le joindre
    const char *accept = nullptr; // type de réponse attendu, par exemple « application/json »
    int delai_ms = 15000;         // connexion, TLS et lecture : au-delà, l'essai est un échec
    uint32_t attente_verrou_tls_ms = 40000; // une recherche de ville tient le verrou 8 s au plus (geocodage.cpp)
};

struct Reponse {
    Issue issue = Issue::Reseau;
    int statut_http = 0;
    size_t octets_fil = 0; // octets reçus sur le fil (compressés ou non)
    size_t octets = 0;     // octets du texte rendu à l'appelant (décompressé)
    bool gzip = false;     // la réponse était compressée
    int duree_ms = 0;      // l'aller-retour réseau seulement (la décompression n'y est pas)
    bool connecte = false; // la connexion TLS a été établie
    Memoire
        apres_connexion{}; // mémoire mesurée une fois la connexion TLS établie : là où la RAM interne est la plus sollicitée
};

// Reçoit le texte décompressé. `corps` n'existe que pendant l'appel : le copier ou en extraire ce qu'il faut avant de rendre la
// main. Rend faux si la réponse n'est pas celle qu'on attend (l'issue est alors `Issue::Illisible`).
using LireCorps = bool (*)(std::string_view corps, void *utilisateur);

// Un GET complet. Les tampons sont pris en PSRAM et rendus avant de sortir ; `lire` est appelée une seule fois, seulement pour une
// réponse 200 complète et valide, hors du verrou TLS (la connexion est déjà fermée). L'issue `Ok` veut dire que `lire` a rendu vrai.
Reponse obtenir(const Requete &requete, LireCorps lire, void *utilisateur);

} // namespace ecran::client_http
