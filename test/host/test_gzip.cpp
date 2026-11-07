// Tests du noyau pur : enveloppe gzip (gzip.hpp) — en-tête, pied, CRC32, décision « compressée ou non ».
//
// Trois sources de vérité, aucune ne vient du code testé :
//   - donnees/gzip_cas.inc : douze fichiers gzip fabriqués par Python (zlib + en-tête écrit à la main, relus par gzip.decompress),
//     un par particularité de la RFC 1952, avec le décalage du flux, le CRC32 et la taille calculés par zlib.crc32 / len() ;
//   - donnees/openmeteo_gzip_*.inc : les octets RÉELS d'une réponse d'Open-Meteo avec `Accept-Encoding: gzip`, et son texte
//     décompressé par Python (voir donnees/LISEZMOI.md) ;
//   - des petits tableaux écrits ici octet par octet, de la taille EXACTE du cas (sizeof) : une lecture au-delà du tampon n'est
//     alors plus une valeur fausse mais une erreur de compilation (évaluation constante), et sous AddressSanitizer à l'exécution.
// Les vecteurs du CRC32 sont ceux de la littérature (« 123456789 » -> 0xCBF43926).
//
// Ce que ces tests ne prouvent PAS : la décompression elle-même (`tinfl` de la ROM de l'ESP32-P4, main/http_commun.cpp) n'est
// exécutable que sur la carte ; ici on prouve ce qui l'entoure.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/gzip.hpp"
#include "mini.hpp"

using namespace ecran::gzip;
using sv = std::string_view;
using u8 = std::uint8_t;

namespace {

#include "donnees/gzip_cas.inc"
#include "donnees/openmeteo_gzip_lyon.inc"
#include "donnees/openmeteo_gzip_sydney.inc"

constexpr std::uint32_t crc_de(sv t) {
    std::uint32_t c = 0;
    for (const char ch : t) {
        const u8 o = static_cast<u8>(ch);
        c = crc32(&o, 1, c);
    }
    return c;
}

// ---- CRC32 ------------------------------------------------------------------------------------------------------------

constexpr int crc32_vecteurs_de_la_litterature() {
    constexpr u8 NEUF[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    VERIFIER(crc32(NEUF, sizeof NEUF) == 0xCBF43926U); // le vecteur de contrôle de tous les CRC32
    VERIFIER(crc32(nullptr, 0) == 0U);                 // rien : 0
    constexpr u8 A[] = {'a'};
    VERIFIER(crc32(A, 1) == 0xE8B7BE43U);
    constexpr u8 ABC[] = {'a', 'b', 'c'};
    VERIFIER(crc32(ABC, 3) == 0x352441C2U);
    VERIFIER(crc_de("The quick brown fox jumps over the lazy dog") == 0x414FA339U);
    VERIFIER(crc_de("The quick brown fox jumps over the lazy dog") != 0x414FA33AU); // un seul bit de différence
    // un octet à zéro compte : 0x00 seul n'est pas « rien »
    constexpr u8 ZERO[] = {0};
    VERIFIER(crc32(ZERO, 1) == 0xD202EF8DU);
    constexpr u8 FF[] = {0xFF, 0xFF, 0xFF, 0xFF};
    VERIFIER(crc32(FF, 4) == 0xFFFFFFFFU);
    return 0;
}

constexpr int crc32_en_plusieurs_morceaux_egal_en_un() {
    constexpr u8 NEUF[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    for (std::size_t coupe = 0; coupe <= sizeof NEUF; ++coupe) {
        const std::uint32_t a = crc32(NEUF, coupe);
        VERIFIER(crc32(NEUF + coupe, sizeof NEUF - coupe, a) == 0xCBF43926U);
    }
    return 0;
}

constexpr int crc32_de_70000_octets_en_blocs() {
    // 70 000 « a » : 1 093 blocs de 64 octets et un reste de 48. La valeur vient de zlib.crc32 (gzip_cas.inc).
    u8 bloc[64] = {};
    for (u8 &o : bloc)
        o = 'a';
    std::uint32_t c = 0;
    for (int i = 0; i < 1093; ++i)
        c = crc32(bloc, 64, c);
    c = crc32(bloc, 48, c);
    VERIFIER(c == GZ_REPETE_CRC_DONNEES);
    VERIFIER(1093U * 64U + 48U == GZ_REPETE_DONNEES_TAILLE);
    return 0;
}

// ---- signature et décision ----------------------------------------------------------------------------------------------

constexpr int signature_gzip_reconnue() {
    VERIFIER(ressemble_a_gzip(GZ_SIMPLE, GZ_SIMPLE_TAILLE));
    constexpr u8 SEULE[] = {0x1F};
    VERIFIER(!ressemble_a_gzip(SEULE, sizeof SEULE)); // un octet : pas encore une signature
    constexpr u8 DEUX[] = {0x1F, 0x8B};
    VERIFIER(ressemble_a_gzip(DEUX, sizeof DEUX));
    constexpr u8 FAUX1[] = {0x1E, 0x8B};
    constexpr u8 FAUX2[] = {0x1F, 0x8A};
    VERIFIER(!ressemble_a_gzip(FAUX1, sizeof FAUX1));
    VERIFIER(!ressemble_a_gzip(FAUX2, sizeof FAUX2));
    VERIFIER(!ressemble_a_gzip(nullptr, 0));
    VERIFIER(!ressemble_a_gzip(nullptr, 5));
    // le texte d'une réponse non compressée ne passe jamais pour du gzip
    constexpr u8 JSON[] = {'{', '"', 'l', 'a'};
    VERIFIER(!ressemble_a_gzip(JSON, sizeof JSON));
    VERIFIER(!ressemble_a_gzip(OM_GZ_LYON_JSON, OM_GZ_LYON_JSON_TAILLE));
    VERIFIER(OM_GZ_LYON_JSON[0] == '{');
    return 0;
}

constexpr int codage_annonce_par_l_en_tete() {
    VERIFIER(codage_de("gzip") == Codage::Gzip);
    VERIFIER(codage_de("GZIP") == Codage::Gzip); // la casse ne compte pas
    VERIFIER(codage_de("GZip") == Codage::Gzip);
    VERIFIER(codage_de("x-gzip") == Codage::Gzip);
    VERIFIER(codage_de("X-Gzip") == Codage::Gzip);
    VERIFIER(codage_de("  gzip ") == Codage::Gzip); // ni les blancs autour
    VERIFIER(codage_de("\tgzip\t") == Codage::Gzip);
    VERIFIER(codage_de("") == Codage::Aucun);
    VERIFIER(codage_de("   ") == Codage::Aucun);
    VERIFIER(codage_de("identity") == Codage::Aucun);
    VERIFIER(codage_de("Identity") == Codage::Aucun);
    VERIFIER(codage_de("br") == Codage::Inconnu);
    VERIFIER(codage_de("deflate") == Codage::Inconnu);
    VERIFIER(codage_de("zstd") == Codage::Inconnu);
    VERIFIER(codage_de("gzip, br") == Codage::Inconnu); // une liste : l'appareil n'a demandé que gzip
    VERIFIER(codage_de("gzipx") == Codage::Inconnu);
    VERIFIER(codage_de("gz") == Codage::Inconnu); // un début de « gzip » n'est pas gzip
    VERIFIER(codage_de("xgzip") == Codage::Inconnu);
    VERIFIER(codage_de("gzi") == Codage::Inconnu);
    VERIFIER(codage_de("identit") == Codage::Inconnu);
    VERIFIER(codage_de("g zip") == Codage::Inconnu);
    return 0;
}

constexpr int decision_sur_une_reponse() {
    // gzip annoncé : il faut aussi la signature
    VERIFIER(decider(Codage::Gzip, true) == Decision::Gzip);
    VERIFIER(decider(Codage::Gzip, false) == Decision::Refus);
    // rien annoncé : une réponse NON compressée reste acceptée ; la signature seule suffit à reconnaître le gzip
    VERIFIER(decider(Codage::Aucun, false) == Decision::Brut);
    VERIFIER(decider(Codage::Aucun, true) == Decision::Gzip);
    // un codage qu'on n'a pas demandé : refusé, signature ou non
    VERIFIER(decider(Codage::Inconnu, false) == Decision::Refus);
    VERIFIER(decider(Codage::Inconnu, true) == Decision::Refus);
    return 0;
}

// ---- en-tête --------------------------------------------------------------------------------------------------------------

constexpr int entete_sans_champ_facultatif() {
    const Entete e = lire_entete(GZ_SIMPLE, GZ_SIMPLE_TAILLE);
    VERIFIER(e.ok());
    VERIFIER(e.debut_flux == 10 && e.debut_flux == GZ_SIMPLE_DEBUT);
    VERIFIER(e.mtime == 0 && e.xfl == 0 && e.os == 3); // ce qu'écrit un serveur Linux (comme Open-Meteo)
    return 0;
}

constexpr int entete_mtime_xfl_os_et_ftext() {
    const Entete e = lire_entete(GZ_FTEXT, GZ_FTEXT_TAILLE);
    VERIFIER(e.ok());
    VERIFIER(e.debut_flux == 10); // FTEXT n'ajoute rien à l'en-tête
    VERIFIER(e.mtime == 1793966400U);
    VERIFIER(e.xfl == 2);
    VERIFIER(e.os == 255);
    return 0;
}

constexpr int entete_avec_chaque_champ_facultatif() {
    VERIFIER(lire_entete(GZ_FNAME, GZ_FNAME_TAILLE).ok() &&
             lire_entete(GZ_FNAME, GZ_FNAME_TAILLE).debut_flux == GZ_FNAME_DEBUT);
    VERIFIER(GZ_FNAME_DEBUT == 10 + sv("meteo.json").size() + 1);
    VERIFIER(lire_entete(GZ_FEXTRA, GZ_FEXTRA_TAILLE).ok() &&
             lire_entete(GZ_FEXTRA, GZ_FEXTRA_TAILLE).debut_flux == GZ_FEXTRA_DEBUT);
    VERIFIER(GZ_FEXTRA_DEBUT == 10 + 2 + 8);
    VERIFIER(lire_entete(GZ_FEXTRA_VIDE, GZ_FEXTRA_VIDE_TAILLE).ok() &&
             lire_entete(GZ_FEXTRA_VIDE, GZ_FEXTRA_VIDE_TAILLE).debut_flux == GZ_FEXTRA_VIDE_DEBUT);
    VERIFIER(GZ_FEXTRA_VIDE_DEBUT == 12);
    // XLEN = 300 : l'octet de poids fort de la longueur n'est pas nul
    VERIFIER(lire_entete(GZ_FEXTRA_LONG, GZ_FEXTRA_LONG_TAILLE).ok() &&
             lire_entete(GZ_FEXTRA_LONG, GZ_FEXTRA_LONG_TAILLE).debut_flux == GZ_FEXTRA_LONG_DEBUT);
    VERIFIER(GZ_FEXTRA_LONG_DEBUT == 10 + 2 + 300);
    VERIFIER(lire_entete(GZ_FCOMMENT, GZ_FCOMMENT_TAILLE).ok() &&
             lire_entete(GZ_FCOMMENT, GZ_FCOMMENT_TAILLE).debut_flux == GZ_FCOMMENT_DEBUT);
    VERIFIER(lire_entete(GZ_FHCRC, GZ_FHCRC_TAILLE).ok() &&
             lire_entete(GZ_FHCRC, GZ_FHCRC_TAILLE).debut_flux == GZ_FHCRC_DEBUT);
    VERIFIER(GZ_FHCRC_DEBUT == 12);
    // les quatre ensemble, avec MTIME, XFL et OS lus à leur place
    const Entete t = lire_entete(GZ_TOUT, GZ_TOUT_TAILLE);
    VERIFIER(t.ok() && t.debut_flux == GZ_TOUT_DEBUT);
    VERIFIER(t.mtime == 1 && t.xfl == 4 && t.os == 3);
    VERIFIER(GZ_TOUT_DEBUT == 10 + 2 + 8 + sv("a.json").size() + 1 + sv("tout").size() + 1 + 2);
    return 0;
}

constexpr int entete_refuse_chaque_defaut_d_un_vrai_fichier() {
    u8 b[GZ_SIMPLE_TAILLE] = {};
    for (std::size_t i = 0; i < GZ_SIMPLE_TAILLE; ++i)
        b[i] = GZ_SIMPLE[i];
    VERIFIER(lire_entete(b, sizeof b).ok());
    b[0] = 0x1E;
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::Signature);
    b[0] = 0x1F;
    b[1] = 0x8A;
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::Signature);
    b[1] = 0x8B;
    b[2] = 0; // « stored » n'est pas une méthode de gzip
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::Methode);
    b[2] = 9;
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::Methode);
    b[2] = 7;
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::Methode);
    b[2] = 8;
    for (const u8 bit : {0x20, 0x40, 0x80, 0xE0}) {
        b[3] = bit;
        VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::BitsReserves);
    }
    b[3] = 0x01; // FTEXT : une indication, pas un refus
    VERIFIER(lire_entete(b, sizeof b).ok());
    b[3] = 0;
    VERIFIER(lire_entete(b, sizeof b).ok());
    return 0;
}

constexpr int entete_taille_insuffisante() {
    VERIFIER(lire_entete(nullptr, 0).erreur == Erreur::TropCourt);
    VERIFIER(lire_entete(nullptr, 99).erreur == Erreur::TropCourt); // pas de tampon : jamais de lecture
    constexpr u8 UN[] = {0x1F};
    VERIFIER(lire_entete(UN, sizeof UN).erreur == Erreur::TropCourt);
    constexpr u8 DEUX[] = {0x1F, 0x8B};
    VERIFIER(lire_entete(DEUX, sizeof DEUX).erreur == Erreur::TropCourt);
    constexpr u8 NEUF[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0};
    VERIFIER(lire_entete(NEUF, sizeof NEUF).erreur == Erreur::TropCourt); // 9 octets : il manque OS
    constexpr u8 DIX[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3};
    const Entete e = lire_entete(DIX, sizeof DIX); // 10 octets : l'en-tête seul se lit
    VERIFIER(e.ok() && e.debut_flux == 10 && e.os == 3);
    // un mauvais début d'un tampon de 9 octets reste « trop court » : la taille se vérifie avant la signature
    constexpr u8 NEUF_FAUX[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    VERIFIER(lire_entete(NEUF_FAUX, sizeof NEUF_FAUX).erreur == Erreur::TropCourt);
    constexpr u8 DIX_FAUX[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    VERIFIER(lire_entete(DIX_FAUX, sizeof DIX_FAUX).erreur == Erreur::Signature);
    return 0;
}

constexpr int champs_facultatifs_tronques_sans_lecture_hors_tampon() {
    // FEXTRA (FLG = 4) : il faut deux octets de longueur, puis autant d'octets que la longueur le dit
    constexpr u8 EXTRA_ABSENT[] = {0x1F, 0x8B, 8, 4, 0, 0, 0, 0, 0, 3};
    VERIFIER(lire_entete(EXTRA_ABSENT, sizeof EXTRA_ABSENT).erreur == Erreur::ChampTronque);
    constexpr u8 EXTRA_UN_OCTET[] = {0x1F, 0x8B, 8, 4, 0, 0, 0, 0, 0, 3, 0x03};
    VERIFIER(lire_entete(EXTRA_UN_OCTET, sizeof EXTRA_UN_OCTET).erreur == Erreur::ChampTronque); // XLEN à moitié
    constexpr u8 EXTRA_TROP_LONG[] = {0x1F, 0x8B, 8, 4, 0, 0, 0, 0, 0, 3, 0x05, 0x00, 'a', 'b', 'c', 'd'};
    VERIFIER(lire_entete(EXTRA_TROP_LONG, sizeof EXTRA_TROP_LONG).erreur == Erreur::ChampTronque); // 5 annoncés, 4 là
    constexpr u8 EXTRA_JUSTE[] = {0x1F, 0x8B, 8, 4, 0, 0, 0, 0, 0, 3, 0x04, 0x00, 'a', 'b', 'c', 'd'};
    VERIFIER(lire_entete(EXTRA_JUSTE, sizeof EXTRA_JUSTE).ok());
    VERIFIER(lire_entete(EXTRA_JUSTE, sizeof EXTRA_JUSTE).debut_flux ==
             sizeof EXTRA_JUSTE); // fini pile au bout du tampon
    constexpr u8 EXTRA_MAX[] = {0x1F, 0x8B, 8, 4, 0, 0, 0, 0, 0, 3, 0xFF, 0xFF, 'a'};
    VERIFIER(lire_entete(EXTRA_MAX, sizeof EXTRA_MAX).erreur == Erreur::ChampTronque); // 65 535 annoncés, 1 là
    // FNAME (FLG = 8) et FCOMMENT (FLG = 16) : un texte fini par un zéro, zéro compris dans le tampon
    constexpr u8 NOM_SANS_ZERO[] = {0x1F, 0x8B, 8, 8, 0, 0, 0, 0, 0, 3, 'a', 'b', 'c'};
    VERIFIER(lire_entete(NOM_SANS_ZERO, sizeof NOM_SANS_ZERO).erreur == Erreur::ChampTronque);
    constexpr u8 NOM_ABSENT[] = {0x1F, 0x8B, 8, 8, 0, 0, 0, 0, 0, 3};
    VERIFIER(lire_entete(NOM_ABSENT, sizeof NOM_ABSENT).erreur == Erreur::ChampTronque);
    constexpr u8 NOM_ZERO_FINAL[] = {0x1F, 0x8B, 8, 8, 0, 0, 0, 0, 0, 3, 'a', 'b', 'c', 0};
    VERIFIER(lire_entete(NOM_ZERO_FINAL, sizeof NOM_ZERO_FINAL).ok());
    VERIFIER(lire_entete(NOM_ZERO_FINAL, sizeof NOM_ZERO_FINAL).debut_flux == 14);
    constexpr u8 NOM_VIDE[] = {0x1F, 0x8B, 8, 8, 0, 0, 0, 0, 0, 3, 0};
    VERIFIER(lire_entete(NOM_VIDE, sizeof NOM_VIDE).ok() && lire_entete(NOM_VIDE, sizeof NOM_VIDE).debut_flux == 11);
    constexpr u8 COMMENTAIRE_SANS_ZERO[] = {0x1F, 0x8B, 8, 16, 0, 0, 0, 0, 0, 3, 'x'};
    VERIFIER(lire_entete(COMMENTAIRE_SANS_ZERO, sizeof COMMENTAIRE_SANS_ZERO).erreur == Erreur::ChampTronque);
    // nom ET commentaire : le zéro du nom ne termine pas le commentaire
    constexpr u8 DEUX_TEXTES[] = {0x1F, 0x8B, 8, 24, 0, 0, 0, 0, 0, 3, 'a', 0, 'b', 0};
    VERIFIER(lire_entete(DEUX_TEXTES, sizeof DEUX_TEXTES).ok() &&
             lire_entete(DEUX_TEXTES, sizeof DEUX_TEXTES).debut_flux == 14);
    constexpr u8 COMMENTAIRE_MANQUE[] = {0x1F, 0x8B, 8, 24, 0, 0, 0, 0, 0, 3, 'a', 0, 'b', 'c'};
    VERIFIER(lire_entete(COMMENTAIRE_MANQUE, sizeof COMMENTAIRE_MANQUE).erreur == Erreur::ChampTronque);
    constexpr u8 COMMENTAIRE_ABSENT[] = {0x1F, 0x8B, 8, 24, 0, 0, 0, 0, 0, 3, 'a', 0};
    VERIFIER(lire_entete(COMMENTAIRE_ABSENT, sizeof COMMENTAIRE_ABSENT).erreur == Erreur::ChampTronque);
    return 0;
}

constexpr int crc16_de_l_entete() {
    // FHCRC (FLG = 2) : le CRC16 de l'en-tête est 0x77A7 (zlib.crc32 des 10 octets, deux octets de poids faible)
    constexpr u8 BON[] = {0x1F, 0x8B, 8, 2, 0, 0, 0, 0, 0, 3, 0xA7, 0x77};
    VERIFIER(lire_entete(BON, sizeof BON).ok() && lire_entete(BON, sizeof BON).debut_flux == 12);
    constexpr u8 FAUX_BAS[] = {0x1F, 0x8B, 8, 2, 0, 0, 0, 0, 0, 3, 0xA6, 0x77};
    VERIFIER(lire_entete(FAUX_BAS, sizeof FAUX_BAS).erreur == Erreur::CrcEntete);
    constexpr u8 FAUX_HAUT[] = {0x1F, 0x8B, 8, 2, 0, 0, 0, 0, 0, 3, 0xA7, 0x76};
    VERIFIER(lire_entete(FAUX_HAUT, sizeof FAUX_HAUT).erreur == Erreur::CrcEntete);
    constexpr u8 INVERSE[] = {0x1F, 0x8B, 8, 2, 0, 0, 0, 0, 0, 3, 0x77, 0xA7}; // petit-boutiste : pas l'ordre inverse
    VERIFIER(lire_entete(INVERSE, sizeof INVERSE).erreur == Erreur::CrcEntete);
    constexpr u8 COURT[] = {0x1F, 0x8B, 8, 2, 0, 0, 0, 0, 0, 3, 0xA7};
    VERIFIER(lire_entete(COURT, sizeof COURT).erreur == Erreur::ChampTronque);
    constexpr u8 ABSENT[] = {0x1F, 0x8B, 8, 2, 0, 0, 0, 0, 0, 3};
    VERIFIER(lire_entete(ABSENT, sizeof ABSENT).erreur == Erreur::ChampTronque);
    // le CRC16 d'un vrai fichier (calculé par Python) est celui du noyau, et il couvre TOUT l'en-tête qui précède (tout : extra, nom, commentaire)
    u8 b[GZ_TOUT_TAILLE] = {};
    for (std::size_t i = 0; i < GZ_TOUT_TAILLE; ++i)
        b[i] = GZ_TOUT[i];
    VERIFIER(lire_entete(b, sizeof b).ok());
    b[GZ_TOUT_DEBUT - 3] ^= 1; // un bit du nom de fichier
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::CrcEntete);
    b[GZ_TOUT_DEBUT - 3] ^= 1;
    b[4] ^= 1; // un bit de MTIME
    VERIFIER(lire_entete(b, sizeof b).erreur == Erreur::CrcEntete);
    return 0;
}

constexpr int entete_tronque_a_chaque_longueur() {
    // Un vrai fichier coupé à toutes les longueurs : jamais de succès tant que l'en-tête complet n'est pas là
    for (std::size_t n = 0; n < GZ_TOUT_DEBUT; ++n)
        VERIFIER(!lire_entete(GZ_TOUT, n).ok());
    VERIFIER(lire_entete(GZ_TOUT, GZ_TOUT_DEBUT).ok());
    VERIFIER(lire_entete(GZ_TOUT, GZ_TOUT_DEBUT).debut_flux == GZ_TOUT_DEBUT);
    for (std::size_t n = 0; n < GZ_FNAME_DEBUT; ++n)
        VERIFIER(!lire_entete(GZ_FNAME, n).ok());
    for (std::size_t n = 0; n < GZ_FEXTRA_DEBUT; ++n)
        VERIFIER(!lire_entete(GZ_FEXTRA, n).ok());
    return 0;
}

// ---- membre complet -------------------------------------------------------------------------------------------------------

constexpr int separer_chaque_cas_fabrique() {
#define SEPARER_CAS(NOM)                                                                                               \
    do {                                                                                                               \
        const Separation s = separer(GZ_##NOM, GZ_##NOM##_TAILLE);                                                     \
        VERIFIER(s.ok());                                                                                              \
        VERIFIER(s.debut_flux == GZ_##NOM##_DEBUT);                                                                    \
        VERIFIER(s.taille_flux == GZ_##NOM##_TAILLE - GZ_##NOM##_DEBUT - 8);                                           \
        VERIFIER(s.crc32 == GZ_##NOM##_CRC);                                                                           \
        VERIFIER(s.taille == GZ_##NOM##_ISIZE);                                                                        \
    } while (0)
    SEPARER_CAS(SIMPLE);
    SEPARER_CAS(FTEXT);
    SEPARER_CAS(FNAME);
    SEPARER_CAS(FEXTRA);
    SEPARER_CAS(FEXTRA_VIDE);
    SEPARER_CAS(FEXTRA_LONG);
    SEPARER_CAS(FCOMMENT);
    SEPARER_CAS(FHCRC);
    SEPARER_CAS(TOUT);
    SEPARER_CAS(VIDE);
    SEPARER_CAS(REPETE);
#undef SEPARER_CAS
    return 0;
}

constexpr int pied_en_petit_boutiste_et_taille_au_dela_de_65535() {
    VERIFIER(GZ_REPETE_ISIZE == 70000); // 0x00011170 : le troisième octet n'est pas nul
    const Separation s = separer(GZ_REPETE, GZ_REPETE_TAILLE);
    VERIFIER(s.ok() && s.taille == 70000);
    VERIFIER(s.crc32 == 0x1229E204U);
    // 18 octets d'en-tête, flux d'un octet, pied écrit à la main : CRC32 0x12345678 puis ISIZE 1, octet de poids faible d'abord
    constexpr u8 PIED_MAIN[] = {0x1F, 0x8B, 8,    0,    0,    0,    0,    0,    0,   3,
                                0x03, 0x78, 0x56, 0x34, 0x12, 0x01, 0x00, 0x00, 0x00};
    const Separation m = separer(PIED_MAIN, sizeof PIED_MAIN);
    VERIFIER(m.ok());
    VERIFIER(m.debut_flux == 10 && m.taille_flux == 1);
    VERIFIER(m.crc32 == 0x12345678U);
    VERIFIER(m.taille == 1U);
    constexpr u8 GRAND[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3, 0x03, 0xFF, 0xEE, 0xDD, 0xCC, 0x04, 0x03, 0x02, 0x01};
    const Separation g = separer(GRAND, sizeof GRAND);
    VERIFIER(g.crc32 == 0xCCDDEEFFU && g.taille == 0x01020304U);
    return 0;
}

constexpr int separer_refuse_un_pied_ou_un_flux_absent() {
    constexpr u8 ENTETE_SEUL[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3};
    VERIFIER(separer(ENTETE_SEUL, sizeof ENTETE_SEUL).erreur == Erreur::PiedAbsent);
    constexpr u8 PIED_COURT[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0,
                                 3,    0x03, 0, 0, 0, 0, 0, 0}; // flux d'un octet + 6 de pied
    VERIFIER(separer(PIED_COURT, sizeof PIED_COURT).erreur == Erreur::PiedAbsent);
    constexpr u8 PIED_SANS_UN[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0,
                                   3,    0,    0, 0, 0, 0, 0, 0}; // 7 octets derrière l'en-tête
    VERIFIER(separer(PIED_SANS_UN, sizeof PIED_SANS_UN).erreur == Erreur::PiedAbsent);
    constexpr u8 SANS_FLUX[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0,
                                3,    0,    0, 0, 0, 0, 0, 0, 0}; // 8 octets de pied, pas de DEFLATE
    VERIFIER(separer(SANS_FLUX, sizeof SANS_FLUX).erreur == Erreur::FluxAbsent);
    constexpr u8 FLUX_D_UN_OCTET[] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3, 0x03, 0, 0, 0, 0, 0, 0, 0, 0};
    VERIFIER(separer(FLUX_D_UN_OCTET, sizeof FLUX_D_UN_OCTET).ok());
    // un en-tête fautif reste la première erreur, même sans pied
    constexpr u8 SIGNATURE_FAUSSE[] = {0, 0, 8, 0, 0, 0, 0, 0, 0, 3};
    VERIFIER(separer(SIGNATURE_FAUSSE, sizeof SIGNATURE_FAUSSE).erreur == Erreur::Signature);
    VERIFIER(separer(nullptr, 0).erreur == Erreur::TropCourt);
    // un nom de fichier qui mange tout le tampon : pas de pied derrière
    constexpr u8 NOM_PUIS_RIEN[] = {0x1F, 0x8B, 8, 8, 0, 0, 0, 0, 0, 3, 'a', 0};
    VERIFIER(separer(NOM_PUIS_RIEN, sizeof NOM_PUIS_RIEN).erreur == Erreur::PiedAbsent);
    // le pied ne peut pas empiéter sur l'en-tête : une coupe d'un vrai fichier ne donne jamais un succès à tort
    for (std::size_t n = 0; n < GZ_TOUT_DEBUT + 9; ++n)
        VERIFIER(!separer(GZ_TOUT, n).ok());
    VERIFIER(separer(GZ_TOUT, GZ_TOUT_DEBUT + 9).ok()); // en-tête + 1 octet de flux + pied
    return 0;
}

constexpr int verifier_donnees_cas_fabriques() {
#define VERIFIER_CAS(NOM)                                                                                              \
    do {                                                                                                               \
        const Separation s = separer(GZ_##NOM, GZ_##NOM##_TAILLE);                                                     \
        VERIFIER(GZ_##NOM##_DONNEES_TAILLE == GZ_##NOM##_ISIZE);                                                       \
        VERIFIER(verifier_donnees(s, GZ_##NOM##_DONNEES, GZ_##NOM##_DONNEES_TAILLE) == Erreur::Aucune);                \
    } while (0)
    VERIFIER_CAS(SIMPLE);
    VERIFIER_CAS(FTEXT);
    VERIFIER_CAS(FNAME);
    VERIFIER_CAS(FEXTRA);
    VERIFIER_CAS(FEXTRA_VIDE);
    VERIFIER_CAS(FEXTRA_LONG);
    VERIFIER_CAS(FCOMMENT);
    VERIFIER_CAS(FHCRC);
    VERIFIER_CAS(TOUT);
    VERIFIER_CAS(VIDE); // taille 0, CRC32 0 : rien n'est un contenu valide
#undef VERIFIER_CAS
    VERIFIER(GZ_VIDE_ISIZE == 0 && GZ_VIDE_CRC == 0);
    return 0;
}

constexpr int verifier_donnees_refuse_ce_qui_differe() {
    const Separation s = separer(GZ_SIMPLE, GZ_SIMPLE_TAILLE);
    // un octet de moins, un octet de plus : la taille est la première chose vérifiée
    VERIFIER(verifier_donnees(s, GZ_SIMPLE_DONNEES, GZ_SIMPLE_DONNEES_TAILLE - 1) == Erreur::TailleDonnees);
    u8 b[GZ_SIMPLE_DONNEES_TAILLE + 1] = {};
    for (std::size_t i = 0; i < GZ_SIMPLE_DONNEES_TAILLE; ++i)
        b[i] = GZ_SIMPLE_DONNEES[i];
    VERIFIER(verifier_donnees(s, b, GZ_SIMPLE_DONNEES_TAILLE + 1) == Erreur::TailleDonnees);
    // même taille, un bit changé : le CRC32 le voit, au début, au milieu et à la fin
    VERIFIER(verifier_donnees(s, b, GZ_SIMPLE_DONNEES_TAILLE) == Erreur::Aucune);
    for (const std::size_t i : {std::size_t{0}, GZ_SIMPLE_DONNEES_TAILLE / 2, GZ_SIMPLE_DONNEES_TAILLE - 1}) {
        b[i] ^= 0x01;
        VERIFIER(verifier_donnees(s, b, GZ_SIMPLE_DONNEES_TAILLE) == Erreur::CrcDonnees);
        b[i] ^= 0x01;
    }
    // un pied refusé par `separer` reste une erreur de `separer`
    const Separation vide = separer(nullptr, 0);
    VERIFIER(verifier_donnees(vide, GZ_SIMPLE_DONNEES, GZ_SIMPLE_DONNEES_TAILLE) == Erreur::TropCourt);
    // et un pied qui ment : taille juste mais CRC32 faux, puis CRC32 juste mais taille fausse
    Separation m = s;
    m.crc32 ^= 1;
    VERIFIER(verifier_donnees(m, GZ_SIMPLE_DONNEES, GZ_SIMPLE_DONNEES_TAILLE) == Erreur::CrcDonnees);
    m = s;
    m.taille += 1;
    VERIFIER(verifier_donnees(m, GZ_SIMPLE_DONNEES, GZ_SIMPLE_DONNEES_TAILLE) == Erreur::TailleDonnees);
    return 0;
}

constexpr int deux_membres_ne_passent_pas_la_verification() {
    // gzip(a) + gzip(b) : `separer` lit le pied des 8 derniers octets (celui de b) ; les octets de a ne le vérifient pas
    const Separation s = separer(GZ_DEUX_MEMBRES, GZ_DEUX_MEMBRES_TAILLE);
    VERIFIER(s.ok());
    VERIFIER(s.taille == sv("deuxieme membre").size());
    VERIFIER(s.crc32 == crc_de("deuxieme membre"));
    VERIFIER(verifier_donnees(s, GZ_DEUX_MEMBRES_DONNEES, GZ_DEUX_MEMBRES_DONNEES_TAILLE) != Erreur::Aucune);
    VERIFIER(GZ_DEUX_MEMBRES_DEBUT == 10);
    return 0;
}

// ---- réponses réelles d'Open-Meteo --------------------------------------------------------------------------------------------

constexpr int reponse_reelle_de_lyon() {
    VERIFIER(ressemble_a_gzip(OM_GZ_LYON, OM_GZ_LYON_TAILLE));
    const Entete e = lire_entete(OM_GZ_LYON, OM_GZ_LYON_TAILLE);
    VERIFIER(e.ok() && e.debut_flux == OM_GZ_LYON_DEBUT);
    VERIFIER(e.mtime == 0 && e.xfl == 0 && e.os == 3); // ce que le serveur écrit : 1f 8b 08 00 00 00 00 00 00 03
    const Separation s = separer(OM_GZ_LYON, OM_GZ_LYON_TAILLE);
    VERIFIER(s.ok());
    VERIFIER(s.taille_flux == OM_GZ_LYON_TAILLE - 10 - 8);
    VERIFIER(s.taille == OM_GZ_LYON_JSON_TAILLE);
    VERIFIER(s.crc32 == OM_GZ_LYON_CRC);
    // le texte décompressé par Python a bien la taille et le CRC32 que le serveur a écrits dans le pied
    VERIFIER(verifier_donnees(s, OM_GZ_LYON_JSON, OM_GZ_LYON_JSON_TAILLE) == Erreur::Aucune);
    VERIFIER(OM_GZ_LYON_TAILLE < OM_GZ_LYON_JSON_TAILLE); // compressé : plus court
    VERIFIER(OM_GZ_LYON_JSON[OM_GZ_LYON_JSON_TAILLE - 1] == '}');
    return 0;
}

constexpr int reponse_reelle_de_sydney_et_croisement() {
    const Separation s = separer(OM_GZ_SYDNEY, OM_GZ_SYDNEY_TAILLE);
    VERIFIER(s.ok() && s.debut_flux == OM_GZ_SYDNEY_DEBUT);
    VERIFIER(verifier_donnees(s, OM_GZ_SYDNEY_JSON, OM_GZ_SYDNEY_JSON_TAILLE) == Erreur::Aucune);
    // le texte de Lyon n'est pas celui de Sydney : ni la taille ni le CRC32 ne passent
    VERIFIER(verifier_donnees(s, OM_GZ_LYON_JSON, OM_GZ_LYON_JSON_TAILLE) != Erreur::Aucune);
    // et une réponse tronquée du fil (un octet de moins) n'est jamais lue comme complète : le pied lu serait faux
    const Separation coupee = separer(OM_GZ_SYDNEY, OM_GZ_SYDNEY_TAILLE - 1);
    VERIFIER(coupee.ok()); // la structure tient encore...
    VERIFIER(verifier_donnees(coupee, OM_GZ_SYDNEY_JSON, OM_GZ_SYDNEY_JSON_TAILLE) !=
             Erreur::Aucune); // ...le contenu non
    return 0;
}

constexpr int libelles_des_erreurs_tous_distincts_et_non_vides() {
    constexpr Erreur TOUTES[] = {Erreur::Aucune,       Erreur::TropCourt,    Erreur::Signature,    Erreur::Methode,
                                 Erreur::BitsReserves, Erreur::ChampTronque, Erreur::CrcEntete,    Erreur::PiedAbsent,
                                 Erreur::FluxAbsent,   Erreur::CrcDonnees,   Erreur::TailleDonnees};
    for (std::size_t i = 0; i < sizeof TOUTES / sizeof TOUTES[0]; ++i) {
        VERIFIER(!libelle(TOUTES[i]).empty());
        for (std::size_t j = i + 1; j < sizeof TOUTES / sizeof TOUTES[0]; ++j)
            VERIFIER(libelle(TOUTES[i]) != libelle(TOUTES[j]));
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(crc32_vecteurs_de_la_litterature)                                                                                \
    X(crc32_en_plusieurs_morceaux_egal_en_un)                                                                          \
    X(crc32_de_70000_octets_en_blocs)                                                                                  \
    X(signature_gzip_reconnue)                                                                                         \
    X(codage_annonce_par_l_en_tete)                                                                                    \
    X(decision_sur_une_reponse)                                                                                        \
    X(entete_sans_champ_facultatif)                                                                                    \
    X(entete_mtime_xfl_os_et_ftext)                                                                                    \
    X(entete_avec_chaque_champ_facultatif)                                                                             \
    X(entete_refuse_chaque_defaut_d_un_vrai_fichier)                                                                   \
    X(entete_taille_insuffisante)                                                                                      \
    X(champs_facultatifs_tronques_sans_lecture_hors_tampon)                                                            \
    X(crc16_de_l_entete)                                                                                               \
    X(entete_tronque_a_chaque_longueur)                                                                                \
    X(separer_chaque_cas_fabrique)                                                                                     \
    X(pied_en_petit_boutiste_et_taille_au_dela_de_65535)                                                               \
    X(separer_refuse_un_pied_ou_un_flux_absent)                                                                        \
    X(verifier_donnees_cas_fabriques)                                                                                  \
    X(verifier_donnees_refuse_ce_qui_differe)                                                                          \
    X(deux_membres_ne_passent_pas_la_verification)                                                                     \
    X(reponse_reelle_de_lyon)                                                                                          \
    X(reponse_reelle_de_sydney_et_croisement)                                                                          \
    X(libelles_des_erreurs_tous_distincts_et_non_vides)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
