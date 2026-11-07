// Noyau pur : lecture de l'en-tête et de la fin d'un flux gzip (RFC 1952). C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
//
// Pourquoi : Vigie répond en gzip dès que le corps dépasse ≈ 500 octets (« Content-Encoding: gzip », corps en morceaux), mais pas
// en dessous (les petites réponses arrivent en clair, avec « Content-Length ») : l'appareil doit reconnaître les deux. `esp_http_client`
// ne décompresse rien ; le client (main/vigie_client.cpp) saute l'en-tête gzip, donne le flux DEFLATE brut à `tinfl_decompress` (ROM de
// l'ESP32-P4, esp_rom/include/miniz.h), puis compare le nombre d'octets obtenus à la taille annoncée par la fin du flux (« ISIZE »).
// La longueur de l'en-tête est variable (nom de fichier, commentaire, champ « extra », CRC d'en-tête optionnels) : d'où ce fichier,
// testé sur les vraies réponses compressées de Vigie (test/host/test_vigie_requete.cpp, qui couvre aussi la requête et le planning).
// Espace de noms propre (`ecran::gzip_flux`) : une autre piste (client réseau commun, ecran/gzip.hpp) lit aussi le gzip, avec en plus
// le CRC-32 ; les deux fichiers ne se gênent pas, et le jour où le client commun sert Vigie, celui-ci disparaît.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ecran::gzip_flux {

inline constexpr std::size_t ENTETE_MIN = 10; // en-tête fixe
inline constexpr std::size_t FIN = 8;         // CRC-32 puis ISIZE, 4 octets chacun

// Vrai si `d` commence comme un flux gzip : octets magiques 1F 8B, méthode 8 (DEFLATE).
constexpr bool est_gzip(const std::uint8_t *d, std::size_t n) noexcept {
    return d != nullptr && n >= 3 && d[0] == 0x1F && d[1] == 0x8B && d[2] == 8;
}

// Longueur de l'en-tête (c'est aussi l'index du premier octet DEFLATE), ou 0 si `d` n'est pas un flux gzip valide, si un drapeau
// réservé est levé, si l'en-tête est coupé ou s'il ne reste pas la place du flux et de la fin (au moins 1 + FIN octets après).
constexpr std::size_t longueur_entete(const std::uint8_t *d, std::size_t n) noexcept {
    if (!est_gzip(d, n) || n < ENTETE_MIN + FIN)
        return 0;
    const std::uint8_t drapeaux = d[3];
    if ((drapeaux & 0xE0) != 0)
        return 0; // bits réservés
    std::size_t p = ENTETE_MIN;
    if (drapeaux & 0x04) { // FEXTRA : deux octets de longueur (petit-boutiste), puis les données
        if (p + 2 > n)
            return 0;
        p += 2 + (static_cast<std::size_t>(d[p]) | (static_cast<std::size_t>(d[p + 1]) << 8));
    }
    for (const std::uint8_t drapeau :
         {std::uint8_t(0x08), std::uint8_t(0x10)}) { // FNAME, FCOMMENT : chaînes terminées par un zéro
        if (!(drapeaux & drapeau))
            continue;
        while (p < n && d[p] != 0)
            ++p;
        if (p >= n)
            return 0;
        ++p;
    }
    if (drapeaux & 0x02) // FHCRC
        p += 2;
    if (p + 1 + FIN > n)
        return 0;
    return p;
}

// Taille décompressée annoncée par la fin du flux (ISIZE, modulo 2^32), ou -1 si le flux est trop court pour en avoir une.
constexpr std::int64_t taille_annoncee(const std::uint8_t *d, std::size_t n) noexcept {
    if (d == nullptr || n < ENTETE_MIN + FIN)
        return -1;
    const std::uint8_t *f = d + n - 4;
    return static_cast<std::int64_t>(static_cast<std::uint32_t>(f[0]) | (static_cast<std::uint32_t>(f[1]) << 8) |
                                     (static_cast<std::uint32_t>(f[2]) << 16) |
                                     (static_cast<std::uint32_t>(f[3]) << 24));
}

} // namespace ecran::gzip_flux
