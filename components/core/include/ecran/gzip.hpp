// Noyau pur : lire l'enveloppe d'un fichier gzip (RFC 1952) et décider si une réponse HTTP est compressée. C++17 standard, aucun
// include ESP-IDF, aucun flottant, évaluable à la compilation (les tests de test/host/ s'en servent).
//
// Ce fichier ne DÉCOMPRESSE pas : il dit où commence le flux DEFLATE brut dans la réponse reçue et ce que le pied annonce (CRC32 et
// taille des octets décompressés). La décompression elle-même est faite sur l'appareil par `tinfl_decompress` de la ROM de
// l'ESP32-P4 (main/http_commun.cpp), qui rend `tinfl` un flux brut : c'est ici que l'on saute l'en-tête gzip, et c'est ici que
// l'on vérifie, après coup, que ce que `tinfl` a rendu est bien ce que le serveur a envoyé (CRC32 et taille).
//
// Forme d'un membre gzip (RFC 1952, § 2.3) :
//   ID1 0x1F | ID2 0x8B | CM (8 = deflate) | FLG | MTIME (4 octets, petit-boutiste) | XFL | OS      = 10 octets
//   [FEXTRA : XLEN (2 octets) puis XLEN octets] [FNAME : texte fini par un zéro] [FCOMMENT : idem] [FHCRC : 2 octets]
//   flux DEFLATE
//   CRC32 (4 octets, petit-boutiste) | ISIZE (4 octets, petit-boutiste : taille décompressée modulo 2^32)
// FLG : bit 0 FTEXT (indication, ignorée), bit 1 FHCRC, bit 2 FEXTRA, bit 3 FNAME, bit 4 FCOMMENT, bits 5 à 7 réservés (à zéro).
//
// Règles tenues ici (une réponse de serveur n'est jamais crue) :
//   - aucune lecture hors du tampon donné, quel que soit son contenu ; aucune addition sur des tailles qui puisse déborder ;
//   - signature, méthode, bits réservés, champs facultatifs tronqués, CRC16 de l'en-tête (FHCRC) faux : refusés, avec la cause ;
//   - le pied doit tenir derrière l'en-tête et le flux DEFLATE ne doit pas être vide ;
//   - un seul membre : le pied lu est celui des 8 DERNIERS octets. Un second membre ou des octets en trop derrière le premier ne
//     passent pas la vérification du CRC32 : refusés aussi, sans code à part.
// Le CRC32 et la taille ne se vérifient qu'APRÈS la décompression (`verifier_donnees`) : un flux qui gonfle au-delà du tampon est
// refusé avant, par le tampon lui-même (main/http_commun.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace ecran::gzip {

inline constexpr std::size_t ENTETE_MIN = 10; // octets : ID1 ID2 CM FLG MTIME(4) XFL OS
inline constexpr std::size_t PIED = 8;        // octets : CRC32 puis ISIZE

inline constexpr std::uint8_t ID1 = 0x1F;
inline constexpr std::uint8_t ID2 = 0x8B;
inline constexpr std::uint8_t METHODE_DEFLATE = 8;

inline constexpr std::uint8_t FLG_FTEXT = 0x01;
inline constexpr std::uint8_t FLG_FHCRC = 0x02;
inline constexpr std::uint8_t FLG_FEXTRA = 0x04;
inline constexpr std::uint8_t FLG_FNAME = 0x08;
inline constexpr std::uint8_t FLG_FCOMMENT = 0x10;
inline constexpr std::uint8_t FLG_RESERVES = 0xE0;

enum class Erreur : std::uint8_t {
    Aucune,
    TropCourt,     // moins de 10 octets : pas même un en-tête
    Signature,     // ne commence pas par 0x1F 0x8B
    Methode,       // CM différent de 8 (deflate)
    BitsReserves,  // un des bits 5 à 7 de FLG est levé
    ChampTronque,  // FEXTRA, FNAME ou FCOMMENT dépasse la fin du tampon
    CrcEntete,     // FHCRC : le CRC16 écrit n'est pas celui de l'en-tête
    PiedAbsent,    // pas la place pour les 8 octets du pied derrière l'en-tête
    FluxAbsent,    // en-tête et pied, mais aucun octet de DEFLATE entre les deux
    CrcDonnees,    // le CRC32 des octets décompressés n'est pas celui du pied
    TailleDonnees, // la taille des octets décompressés n'est pas celle du pied
};

// Phrase courte pour le journal série (une constante : jamais d'allocation).
constexpr std::string_view libelle(Erreur e) noexcept {
    switch (e) {
    case Erreur::Aucune:
        return "aucune erreur";
    case Erreur::TropCourt:
        return "réponse gzip trop courte";
    case Erreur::Signature:
        return "signature gzip absente";
    case Erreur::Methode:
        return "méthode de compression inconnue";
    case Erreur::BitsReserves:
        return "bits réservés de l'en-tête gzip levés";
    case Erreur::ChampTronque:
        return "champ facultatif de l'en-tête gzip tronqué";
    case Erreur::CrcEntete:
        return "CRC de l'en-tête gzip faux";
    case Erreur::PiedAbsent:
        return "pied gzip absent";
    case Erreur::FluxAbsent:
        return "flux gzip vide";
    case Erreur::CrcDonnees:
        return "CRC32 des données décompressées faux";
    case Erreur::TailleDonnees:
        return "taille des données décompressées fausse";
    }
    return "erreur inconnue";
}

// ---- CRC32 ------------------------------------------------------------------------------------------------------------

namespace detail {

// Table du CRC32 d'IEEE 802.3 / gzip (polynôme réfléchi 0xEDB88320), calculée à la compilation : 1 Ko en lecture seule.
struct TableCrc32 {
    std::uint32_t valeur[256];
    constexpr TableCrc32() noexcept : valeur() {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1U) != 0 ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            valeur[i] = c;
        }
    }
};
inline constexpr TableCrc32 TABLE_CRC32{};

constexpr std::uint32_t lire_u32(const std::uint8_t *p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

constexpr std::uint16_t lire_u16(const std::uint8_t *p) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8));
}

} // namespace detail

// CRC32 de `n` octets. Pour un calcul en plusieurs morceaux, repasser le résultat précédent : crc32(b, nb, crc32(a, na)) est le
// CRC32 de a suivi de b. Vecteur de référence : « 123456789 » -> 0xCBF43926.
constexpr std::uint32_t crc32(const std::uint8_t *p, std::size_t n, std::uint32_t crc = 0) noexcept {
    std::uint32_t c = ~crc;
    for (std::size_t i = 0; i < n; ++i)
        c = detail::TABLE_CRC32.valeur[(c ^ p[i]) & 0xFFU] ^ (c >> 8);
    return ~c;
}

// ---- en-tête ----------------------------------------------------------------------------------------------------------

// Vrai si le tampon commence par la signature gzip (0x1F 0x8B). Un JSON ne commence jamais par l'octet 0x1F : une réponse non
// compressée ne peut pas être prise pour du gzip.
constexpr bool ressemble_a_gzip(const std::uint8_t *p, std::size_t n) noexcept {
    return p != nullptr && n >= 2 && p[0] == ID1 && p[1] == ID2;
}

struct Entete {
    Erreur erreur = Erreur::Aucune;
    std::size_t debut_flux = 0; // décalage du premier octet du flux DEFLATE brut
    std::uint32_t mtime = 0;    // date de modification (secondes UNIX), 0 = inconnue
    std::uint8_t xfl = 0;       // 2 = compression maximale, 4 = la plus rapide (indication)
    std::uint8_t os = 0;        // système qui a compressé (3 = Unix, 255 = inconnu)
    constexpr bool ok() const noexcept { return erreur == Erreur::Aucune; }
};

namespace detail {

constexpr Entete refus(Erreur e) noexcept {
    Entete r;
    r.erreur = e;
    return r;
}

} // namespace detail

// Lit l'en-tête (champs facultatifs compris) et rend le décalage du flux DEFLATE. Ne regarde pas le pied : voir `separer`.
constexpr Entete lire_entete(const std::uint8_t *p, std::size_t n) noexcept {
    if (p == nullptr || n < ENTETE_MIN)
        return detail::refus(Erreur::TropCourt);
    if (p[0] != ID1 || p[1] != ID2)
        return detail::refus(Erreur::Signature);
    if (p[2] != METHODE_DEFLATE)
        return detail::refus(Erreur::Methode);
    const std::uint8_t flg = p[3];
    if ((flg & FLG_RESERVES) != 0)
        return detail::refus(Erreur::BitsReserves);
    Entete e;
    e.mtime = detail::lire_u32(p + 4);
    e.xfl = p[8];
    e.os = p[9];
    std::size_t pos = ENTETE_MIN;
    if ((flg & FLG_FEXTRA) != 0) {
        if (n - pos < 2) // n >= pos ici : la soustraction ne déborde pas
            return detail::refus(Erreur::ChampTronque);
        const std::size_t xlen = detail::lire_u16(p + pos);
        pos += 2;
        if (xlen > n - pos)
            return detail::refus(Erreur::ChampTronque);
        pos += xlen;
    }
    for (const std::uint8_t drapeau : {FLG_FNAME, FLG_FCOMMENT}) {
        if ((flg & drapeau) == 0)
            continue;
        while (pos < n && p[pos] != 0)
            ++pos;
        if (pos >= n) // pas de zéro final : le champ n'est pas terminé
            return detail::refus(Erreur::ChampTronque);
        ++pos; // le zéro
    }
    if ((flg & FLG_FHCRC) != 0) {
        if (n - pos < 2)
            return detail::refus(Erreur::ChampTronque);
        // RFC 1952 : les deux octets de poids faible du CRC32 de tout l'en-tête qui précède ce champ.
        if ((crc32(p, pos) & 0xFFFFU) != detail::lire_u16(p + pos))
            return detail::refus(Erreur::CrcEntete);
        pos += 2;
    }
    e.debut_flux = pos;
    return e;
}

// ---- membre complet ---------------------------------------------------------------------------------------------------

struct Separation {
    Erreur erreur = Erreur::Aucune;
    std::size_t debut_flux = 0;  // décalage du premier octet DEFLATE
    std::size_t taille_flux = 0; // octets de DEFLATE (jusqu'au pied exclu)
    std::uint32_t crc32 = 0;     // CRC32 que le pied annonce pour les octets décompressés
    std::uint32_t taille = 0;    // ISIZE : taille décompressée annoncée, modulo 2^32
    constexpr bool ok() const noexcept { return erreur == Erreur::Aucune; }
};

// En-tête, flux et pied d'un membre gzip complet reçu en entier dans [p, p + n).
constexpr Separation separer(const std::uint8_t *p, std::size_t n) noexcept {
    Separation s;
    const Entete e = lire_entete(p, n);
    if (!e.ok()) {
        s.erreur = e.erreur;
        return s;
    }
    // e.debut_flux <= n : la soustraction ne déborde pas
    if (n - e.debut_flux < PIED) {
        s.erreur = Erreur::PiedAbsent;
        return s;
    }
    const std::size_t flux = n - e.debut_flux - PIED;
    if (flux == 0) {
        s.erreur = Erreur::FluxAbsent;
        return s;
    }
    s.debut_flux = e.debut_flux;
    s.taille_flux = flux;
    s.crc32 = detail::lire_u32(p + n - PIED);
    s.taille = detail::lire_u32(p + n - 4);
    return s;
}

// Compare ce que `tinfl` a rendu (`donnees`, `taille` octets) au pied lu par `separer`. Aucune erreur = le serveur et l'appareil
// voient les mêmes octets. La taille se compare modulo 2^32, comme l'ISIZE.
constexpr Erreur verifier_donnees(const Separation &s, const std::uint8_t *donnees, std::size_t taille) noexcept {
    if (!s.ok())
        return s.erreur;
    if (static_cast<std::uint32_t>(taille) != s.taille)
        return Erreur::TailleDonnees;
    if (crc32(donnees, taille) != s.crc32)
        return Erreur::CrcDonnees;
    return Erreur::Aucune;
}

// ---- compressée ou non ? ------------------------------------------------------------------------------------------------

// Ce que l'en-tête de réponse `Content-Encoding` annonce.
enum class Codage : std::uint8_t {
    Aucun,   // absent, vide ou « identity »
    Gzip,    // « gzip » ou « x-gzip »
    Inconnu, // autre chose (br, deflate, zstd, une liste…) : l'appareil n'en a demandé aucun
};

namespace detail {

constexpr char minuscule(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

constexpr bool egal_sans_casse(std::string_view a, std::string_view minuscules) noexcept {
    if (a.size() != minuscules.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (minuscule(a[i]) != minuscules[i])
            return false;
    }
    return true;
}

} // namespace detail

// Valeur de l'en-tête `Content-Encoding` -> codage. Les blancs autour et la casse sont sans importance (RFC 9110 § 8.4.1).
constexpr Codage codage_de(std::string_view valeur) noexcept {
    while (!valeur.empty() && (valeur.front() == ' ' || valeur.front() == '\t'))
        valeur.remove_prefix(1);
    while (!valeur.empty() && (valeur.back() == ' ' || valeur.back() == '\t'))
        valeur.remove_suffix(1);
    if (valeur.empty() || detail::egal_sans_casse(valeur, "identity"))
        return Codage::Aucun;
    if (detail::egal_sans_casse(valeur, "gzip") || detail::egal_sans_casse(valeur, "x-gzip"))
        return Codage::Gzip;
    return Codage::Inconnu;
}

enum class Decision : std::uint8_t {
    Brut,  // le corps est la réponse telle quelle
    Gzip,  // à décompresser
    Refus, // incohérent ou inconnu : réponse refusée
};

// Décision sur une réponse : `annonce` = ce que `Content-Encoding` dit, `signature_gzip` = ce que les deux premiers octets montrent.
//   gzip annoncé et signature absente : refus (réponse corrompue ou en-tête menteur) ;
//   rien annoncé mais signature présente : gzip (un intermédiaire a pu retirer l'en-tête ; un JSON ne commence pas par 0x1F) ;
//   codage inconnu : refus (on n'a demandé que gzip).
constexpr Decision decider(Codage annonce, bool signature_gzip) noexcept {
    switch (annonce) {
    case Codage::Gzip:
        return signature_gzip ? Decision::Gzip : Decision::Refus;
    case Codage::Aucun:
        return signature_gzip ? Decision::Gzip : Decision::Brut;
    case Codage::Inconnu:
        break;
    }
    return Decision::Refus;
}

} // namespace ecran::gzip
