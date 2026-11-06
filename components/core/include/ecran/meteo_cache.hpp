// Noyau pur : le cache de la NVS = un `Releve` (format de meteo_openmeteo.hpp) précédé de la POSITION pour laquelle il a été
// demandé. Sans cette position, un changement de zone ferait afficher la météo de l'ancien lieu avec le nom du nouveau.
//
// Format v1, petit-boutiste : 'Z' 'K' | version(1) | latitude_e5(i32) | longitude_e5(i32) | `serialiser()` du relevé |
// CRC-32 (4 octets) de tout ce qui précède. Le relevé garde sa propre marque, version et somme de contrôle : la somme
// de l'enveloppe couvre en plus la position.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/meteo_openmeteo.hpp"
#include "ecran/meteo_requete.hpp"

namespace ecran::meteo {

inline constexpr std::size_t TAILLE_CACHE_ZONE_MAX = 3 + 4 + 4 + TAILLE_CACHE_MAX + 4;

constexpr std::size_t serialiser_cache(const Releve &r, std::int32_t latitude_e5, std::int32_t longitude_e5,
                                       std::uint8_t *sortie, std::size_t capacite) noexcept {
    constexpr std::size_t ENTETE = 3 + 4 + 4;
    if (sortie == nullptr || capacite < ENTETE + 4)
        return 0;
    if (latitude_e5 < -LATITUDE_E5_MAX || latitude_e5 > LATITUDE_E5_MAX || longitude_e5 < -LONGITUDE_E5_MAX ||
        longitude_e5 > LONGITUDE_E5_MAX) {
        return 0; // une position invraisemblable ne se met pas en cache
    }
    detail::Ecrivain w{sortie, capacite};
    w.u8('Z');
    w.u8('K');
    w.u8(1);
    w.u32(static_cast<std::uint32_t>(latitude_e5));
    w.u32(static_cast<std::uint32_t>(longitude_e5));
    const std::size_t releve = serialiser(r, sortie + ENTETE, capacite - ENTETE - 4);
    if (releve == 0)
        return 0;
    w.n += releve;
    w.u32(detail::crc32(sortie, w.n));
    return w.trop ? 0 : w.n;
}

struct LectureCache {
    Releve releve{};
    std::int32_t latitude_e5 = 0;
    std::int32_t longitude_e5 = 0;
    Diagnostic diag{};
    constexpr bool ok() const noexcept { return diag.ok(); }
};

constexpr LectureCache deserialiser_cache(const std::uint8_t *donnees, std::size_t taille) noexcept {
    constexpr std::size_t ENTETE = 3 + 4 + 4;
    LectureCache out;
    if (donnees == nullptr || taille < ENTETE + 4) {
        out.diag = detail::echec(Erreur::Corrompu, "cache");
        return out;
    }
    if (donnees[0] != 'Z' || donnees[1] != 'K') {
        out.diag = detail::echec(Erreur::Corrompu, "cache.marque");
        return out;
    }
    if (donnees[2] != 1) {
        out.diag = detail::echec(Erreur::VersionInconnue, "cache.version");
        return out;
    }
    const std::uint32_t attendu = static_cast<std::uint32_t>(donnees[taille - 4]) |
                                  (static_cast<std::uint32_t>(donnees[taille - 3]) << 8) |
                                  (static_cast<std::uint32_t>(donnees[taille - 2]) << 16) |
                                  (static_cast<std::uint32_t>(donnees[taille - 1]) << 24);
    if (detail::crc32(donnees, taille - 4) != attendu) {
        out.diag = detail::echec(Erreur::Corrompu, "cache.crc");
        return out;
    }
    detail::Lecteur32 rd{donnees, taille - 4};
    rd.n = 3;
    const std::int32_t lat = rd.i32();
    const std::int32_t lon = rd.i32();
    if (lat < -LATITUDE_E5_MAX || lat > LATITUDE_E5_MAX || lon < -LONGITUDE_E5_MAX || lon > LONGITUDE_E5_MAX) {
        out.diag = detail::echec(Erreur::HorsPlage, "cache.position");
        return out;
    }
    const Lecture lu = deserialiser(donnees + ENTETE, taille - ENTETE - 4);
    if (!lu.ok()) {
        out.diag = lu.diag;
        return out;
    }
    out.releve = lu.releve;
    out.latitude_e5 = lat;
    out.longitude_e5 = lon;
    return out;
}

} // namespace ecran::meteo
