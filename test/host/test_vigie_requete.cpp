// Tests du noyau pur « Vigie, côté requête » : adresse de base (vigie_requete.hpp), cadence des requêtes (vigie_planning.hpp) et en-tête
// gzip des réponses (gzip_flux.hpp).
//
// Les adresses attendues viennent de capturer_vigie.py, qui a reçu les réponses de Vigie avec ELLES (donnees/attendu_vigie_urls.inc, relues
// par controle_python_vigie.py : l'adresse construite par le script Python est celle que l'index des captures a enregistrée). Les flux gzip
// sont les octets REÇUS sur le fil (donnees/vigie_<lieu>.gz.inc) ; la longueur de leur en-tête et leur taille décompressée sont lues par
// Python avec zlib, flux DEFLATE vérifié (donnees/attendu_vigie_<lieu>.gz.inc). Les cas limites sont écrits à la main.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/gzip_flux.hpp"
#include "ecran/vigie_planning.hpp"
#include "ecran/vigie_requete.hpp"
#include "mini.hpp"

using namespace ecran::vigie;
using sv = std::string_view;

namespace {

struct Lieu {
    sv nom;
    std::int32_t lat_e5;
    std::int32_t lon_e5;
    sv url;
};
constexpr Lieu LIEUX[] = {
#include "donnees/attendu_vigie_urls.inc"
};

struct AttGz {
    std::size_t entete;
    long long taille;
};

constexpr std::uint8_t GZ_ANCONA[] = {
#include "donnees/vigie_ancona.gz.inc"
};
constexpr AttGz ATT_GZ_ANCONA =
#include "donnees/attendu_vigie_ancona.gz.inc"
    ;
constexpr sv JSON_ANCONA =
#include "donnees/vigie_ancona.inc"
    ;

constexpr std::uint8_t GZ_BODO[] = {
#include "donnees/vigie_bodo.gz.inc"
};
constexpr AttGz ATT_GZ_BODO =
#include "donnees/attendu_vigie_bodo.gz.inc"
    ;
constexpr sv JSON_BODO =
#include "donnees/vigie_bodo.inc"
    ;

constexpr std::uint8_t GZ_EBERSWALDE[] = {
#include "donnees/vigie_eberswalde.gz.inc"
};
constexpr AttGz ATT_GZ_EBERSWALDE =
#include "donnees/attendu_vigie_eberswalde.gz.inc"
    ;
constexpr sv JSON_EBERSWALDE =
#include "donnees/vigie_eberswalde.inc"
    ;

constexpr std::uint8_t GZ_FUNCHAL[] = {
#include "donnees/vigie_funchal.gz.inc"
};
constexpr AttGz ATT_GZ_FUNCHAL =
#include "donnees/attendu_vigie_funchal.gz.inc"
    ;
constexpr sv JSON_FUNCHAL =
#include "donnees/vigie_funchal.inc"
    ;

constexpr std::uint8_t GZ_LA_PALMA[] = {
#include "donnees/vigie_la_palma.gz.inc"
};
constexpr AttGz ATT_GZ_LA_PALMA =
#include "donnees/attendu_vigie_la_palma.gz.inc"
    ;
constexpr sv JSON_LA_PALMA =
#include "donnees/vigie_la_palma.inc"
    ;

constexpr std::uint8_t GZ_LONDRES[] = {
#include "donnees/vigie_londres.gz.inc"
};
constexpr AttGz ATT_GZ_LONDRES =
#include "donnees/attendu_vigie_londres.gz.inc"
    ;
constexpr sv JSON_LONDRES =
#include "donnees/vigie_londres.inc"
    ;

constexpr std::uint8_t GZ_OSLO[] = {
#include "donnees/vigie_oslo.gz.inc"
};
constexpr AttGz ATT_GZ_OSLO =
#include "donnees/attendu_vigie_oslo.gz.inc"
    ;
constexpr sv JSON_OSLO =
#include "donnees/vigie_oslo.inc"
    ;

constexpr std::uint8_t GZ_TOKYO[] = {
#include "donnees/vigie_tokyo.gz.inc"
};
constexpr AttGz ATT_GZ_TOKYO =
#include "donnees/attendu_vigie_tokyo.gz.inc"
    ;
constexpr sv JSON_TOKYO =
#include "donnees/vigie_tokyo.inc"
    ;

constexpr std::uint8_t GZ_VALENCE[] = {
#include "donnees/vigie_valence.gz.inc"
};
constexpr AttGz ATT_GZ_VALENCE =
#include "donnees/attendu_vigie_valence.gz.inc"
    ;
constexpr sv JSON_VALENCE =
#include "donnees/vigie_valence.inc"
    ;

constexpr sv HAMBOURG =
#include "donnees/vigie_hambourg.inc"
    ;

// ---- adresse de base -------------------------------------------------------------------------------------------------

constexpr bool adresse_ok(sv saisie, sv attendue) {
    char b[128] = {};
    const std::size_t n = normaliser_adresse(saisie, b, sizeof b);
    return n == attendue.size() && sv(b, n) == attendue && b[n] == '\0';
}
constexpr bool adresse_refusee(sv saisie) {
    char b[128] = {};
    return normaliser_adresse(saisie, b, sizeof b) == 0;
}

constexpr int adresses_valables_nettoyees() {
    VERIFIER(adresse_ok("https://vigie.auxfilsdesidees.fr", "https://vigie.auxfilsdesidees.fr"));
    VERIFIER(adresse_ok("  https://vigie.example.org/  ", "https://vigie.example.org"));
    VERIFIER(adresse_ok("\thttps://vigie.example.org\t", "https://vigie.example.org"));
    VERIFIER(adresse_ok("https://vigie.example.org///", "https://vigie.example.org"));
    VERIFIER(adresse_ok("HTTPS://Vigie.Example.org",
                        "https://Vigie.Example.org")); // le schéma passe en minuscules, l'hôte reste tel quel
    VERIFIER(adresse_ok("https://192.168.0.16:8443/vigie", "https://192.168.0.16:8443/vigie"));
    VERIFIER(adresse_ok("https://localhost", "https://localhost"));
    VERIFIER(adresse_ok(ADRESSE_PUBLIQUE, ADRESSE_PUBLIQUE));
    return 0;
}

constexpr int adresses_refusees() {
    VERIFIER(adresse_refusee(""));
    VERIFIER(adresse_refusee("   "));
    VERIFIER(adresse_refusee("https://"));
    VERIFIER(adresse_refusee("https:///"));
    VERIFIER(adresse_refusee(
        "http://vigie.example.org")); // pas de clair : un avertissement lu en clair pourrait être falsifié
    VERIFIER(adresse_refusee("vigie.example.org"));
    VERIFIER(adresse_refusee("ftp://vigie.example.org"));
    VERIFIER(adresse_refusee("https:/vigie.example.org"));
    VERIFIER(adresse_refusee("https:// vigie.example.org"));
    VERIFIER(adresse_refusee("https://vigie example.org"));
    VERIFIER(adresse_refusee("https://vigie.example.org?x=1")); // les paramètres sont ceux de l'appareil
    VERIFIER(adresse_refusee("https://vigie.example.org#haut"));
    VERIFIER(adresse_refusee("https://moi:secret@vigie.example.org")); // pas d'identifiants dans l'adresse
    VERIFIER(adresse_refusee("https://vigie.example.org\\x"));
    VERIFIER(adresse_refusee("https://vigie.\xC3\xA9.org")); // octet >= 0x80
    VERIFIER(adresse_refusee("https://-vigie.example.org"));
    VERIFIER(adresse_refusee("https://vigie.example.org\n"));
    return 0;
}

constexpr int la_longueur_de_l_adresse_est_bornee() {
    char t[200] = {};
    for (std::size_t i = 0; i < sizeof t; ++i)
        t[i] = 'a';
    const sv https = "https://";
    for (std::size_t i = 0; i < https.size(); ++i)
        t[i] = https[i];
    char b[200] = {};
    VERIFIER(normaliser_adresse(sv(t, ADRESSE_MAX), b, sizeof b) == ADRESSE_MAX);
    VERIFIER(normaliser_adresse(sv(t, ADRESSE_MAX + 1), b, sizeof b) == 0);
    // Un tampon trop petit refuse, sans rien écrire au-delà : il faut la place du zéro.
    char petit[ADRESSE_MAX] = {};
    VERIFIER(normaliser_adresse(sv(t, ADRESSE_MAX), petit, ADRESSE_MAX) == 0);
    char juste[ADRESSE_MAX + 1] = {};
    VERIFIER(normaliser_adresse(sv(t, ADRESSE_MAX), juste, sizeof juste) == ADRESSE_MAX);
    VERIFIER(normaliser_adresse("https://a.fr", nullptr, 10) == 0);
    VERIFIER(normaliser_adresse("https://a.fr", b, 0) == 0);
    return 0;
}

// ---- adresse de la requête ---------------------------------------------------------------------------------------------

constexpr bool requete_egale(sv adresse, std::int32_t lat, std::int32_t lon, sv langue, sv attendue) {
    char b[TAILLE_REQUETE_MAX] = {};
    const std::size_t n = construire_requete(adresse, lat, lon, langue, b, sizeof b);
    return n == attendue.size() && sv(b, n) == attendue && b[n] == '\0';
}
constexpr bool requete_refusee(sv adresse, std::int32_t lat, std::int32_t lon, sv langue) {
    char b[TAILLE_REQUETE_MAX] = {};
    return construire_requete(adresse, lat, lon, langue, b, sizeof b) == 0;
}

constexpr int les_adresses_sont_celles_des_captures_reelles() {
    for (const Lieu &l : LIEUX) {
        char b[TAILLE_REQUETE_MAX] = {};
        const std::size_t n = construire_requete(ADRESSE_PUBLIQUE, l.lat_e5, l.lon_e5, "fr", b, sizeof b);
        VERIFIER(n == l.url.size());
        VERIFIER(sv(b, n) == l.url);
    }
    VERIFIER(sizeof(LIEUX) / sizeof(LIEUX[0]) >= 10); // le tableau n'est pas vide par accident
    return 0;
}

constexpr int coordonnees_arrondies_au_millieme_la_moitie_loin_de_zero() {
    const sv base = "https://v.fr";
    VERIFIER(requete_egale(base, 4574906, 484789, "fr",
                           "https://v.fr/v1/vigilances?lat=45.749&lon=4.848&lang=fr,en")); // Lyon
    VERIFIER(requete_egale(base, -3386785, 15120732, "fr",
                           "https://v.fr/v1/vigilances?lat=-33.868&lon=151.207&lang=fr,en")); // Sydney
    VERIFIER(
        requete_egale(base, 12345, 12350, "fr",
                      "https://v.fr/v1/vigilances?lat=0.123&lon=0.124&lang=fr,en")); // 123,45 -> 123 ; 123,5 -> 124
    VERIFIER(requete_egale(base, -12345, -12350, "fr", "https://v.fr/v1/vigilances?lat=-0.123&lon=-0.124&lang=fr,en"));
    VERIFIER(requete_egale(base, 12349, 12350, "fr", "https://v.fr/v1/vigilances?lat=0.123&lon=0.124&lang=fr,en"));
    VERIFIER(requete_egale(base, 0, 0, "fr", "https://v.fr/v1/vigilances?lat=0.000&lon=0.000&lang=fr,en"));
    VERIFIER(requete_egale(base, -40, 40, "fr",
                           "https://v.fr/v1/vigilances?lat=0.000&lon=0.000&lang=fr,en")); // pas de « -0.000 »
    VERIFIER(requete_egale(base, -49, -50, "fr", "https://v.fr/v1/vigilances?lat=0.000&lon=-0.001&lang=fr,en"));
    VERIFIER(
        requete_egale(base, 9000000, 18000000, "fr", "https://v.fr/v1/vigilances?lat=90.000&lon=180.000&lang=fr,en"));
    VERIFIER(requete_egale(base, -9000000, -18000000, "fr",
                           "https://v.fr/v1/vigilances?lat=-90.000&lon=-180.000&lang=fr,en"));
    return 0;
}

constexpr int la_langue_de_l_ecran_puis_l_anglais() {
    const sv base = "https://v.fr";
    VERIFIER(requete_egale(base, 100000, 200000, "de", "https://v.fr/v1/vigilances?lat=1.000&lon=2.000&lang=de,en"));
    VERIFIER(requete_egale(base, 100000, 200000, "tr", "https://v.fr/v1/vigilances?lat=1.000&lon=2.000&lang=tr,en"));
    VERIFIER(requete_egale(base, 100000, 200000, "en",
                           "https://v.fr/v1/vigilances?lat=1.000&lon=2.000&lang=en")); // pas « en,en »
    return 0;
}

constexpr int coordonnees_ou_langue_invalides_sont_refusees() {
    const sv base = "https://v.fr";
    VERIFIER(requete_refusee(base, 9000001, 0, "fr"));
    VERIFIER(requete_refusee(base, -9000001, 0, "fr"));
    VERIFIER(requete_refusee(base, 0, 18000001, "fr"));
    VERIFIER(requete_refusee(base, 0, -18000001, "fr"));
    VERIFIER(requete_refusee(base, 0, 0, ""));
    VERIFIER(requete_refusee(base, 0, 0, "f"));
    VERIFIER(requete_refusee(base, 0, 0, "fra"));
    VERIFIER(requete_refusee(base, 0, 0, "FR"));
    VERIFIER(requete_refusee(base, 0, 0, "f1"));
    VERIFIER(requete_refusee(base, 0, 0, "fr,en&x=")); // pas d'injection de paramètre par la langue
    VERIFIER(requete_refusee("", 0, 0, "fr"));
    return 0;
}

constexpr int le_tampon_juste_suffit_et_un_octet_de_moins_refuse() {
    char b[TAILLE_REQUETE_MAX] = {};
    const sv attendue = "https://v.fr/v1/vigilances?lat=1.000&lon=2.000&lang=fr,en";
    VERIFIER(construire_requete("https://v.fr", 100000, 200000, "fr", b, attendue.size() + 1) == attendue.size());
    VERIFIER(construire_requete("https://v.fr", 100000, 200000, "fr", b, attendue.size()) == 0);
    VERIFIER(construire_requete("https://v.fr", 100000, 200000, "fr", nullptr, sizeof b) == 0);
    VERIFIER(construire_requete("https://v.fr", 100000, 200000, "fr", b, 0) == 0);
    return 0;
}

constexpr int la_plus_longue_requete_tient_dans_le_tampon_prevu() {
    char base[ADRESSE_MAX + 1] = {};
    for (std::size_t i = 0; i < ADRESSE_MAX; ++i)
        base[i] = 'a';
    char b[TAILLE_REQUETE_MAX] = {};
    const std::size_t n = construire_requete(sv(base, ADRESSE_MAX), -9000000, -18000000, "fr", b, sizeof b);
    VERIFIER(n > 0);
    VERIFIER(n + 1 ==
             TAILLE_REQUETE_MAX); // « -90.000 » et « -180.000 » sont les plus longs : le tampon est exactement plein
    VERIFIER(construire_requete(sv(base, ADRESSE_MAX + 1), 0, 0, "fr", b, sizeof b) == 0);
    VERIFIER(
        TAILLE_REQUETE_MAX <=
        160); // la ligne de requête tient largement dans le tampon d'envoi de esp_http_client (1024 octets demandés)
    return 0;
}

// ---- cadence ---------------------------------------------------------------------------------------------------------

constexpr int la_periode_est_bornee_entre_10_et_180_minutes() {
    VERIFIER(periode_s(15) == 900);
    VERIFIER(periode_s(PERIODE_DEFAUT_MIN) == 900);
    VERIFIER(periode_s(10) == 600);
    VERIFIER(periode_s(9) == 600);
    VERIFIER(periode_s(0) == 600);
    VERIFIER(periode_s(-5) == 600);
    VERIFIER(periode_s(180) == 10800);
    VERIFIER(periode_s(181) == 10800);
    VERIFIER(periode_s(2000000000) == 10800);
    return 0;
}

constexpr int le_premier_essai_attend_une_a_deux_minutes() {
    VERIFIER(delai_premier_essai_s(0) == 60);
    VERIFIER(delai_premier_essai_s(255) == 120);
    for (int a = 0; a <= 255; ++a) {
        const std::int64_t d = delai_premier_essai_s(static_cast<std::uint8_t>(a));
        VERIFIER(d >= 60 && d <= 120);
    }
    return 0;
}

constexpr int apres_un_succes_une_periode_plus_au_plus_un_dixieme() {
    VERIFIER(delai_avant_essai_s(0, false, 900, 0) == 900);
    VERIFIER(delai_avant_essai_s(0, false, 900, 255) == 990);
    VERIFIER(delai_avant_essai_s(0, false, 600, 255) == 660);
    VERIFIER(delai_avant_essai_s(0, false, 10800, 0) == 10800);
    for (int a = 0; a <= 255; ++a) {
        const std::int64_t d = delai_avant_essai_s(0, false, 900, static_cast<std::uint8_t>(a));
        VERIFIER(d >= 900 && d <= 990);
    }
    return 0;
}

constexpr int apres_un_echec_on_double_jusqu_a_la_periode() {
    VERIFIER(delai_avant_essai_s(1, false, 900, 0) == 60);
    VERIFIER(delai_avant_essai_s(2, false, 900, 0) == 120);
    VERIFIER(delai_avant_essai_s(3, false, 900, 0) == 240);
    VERIFIER(delai_avant_essai_s(4, false, 900, 0) == 480);
    VERIFIER(delai_avant_essai_s(5, false, 900, 0) == 900); // 960 plafonné à la période
    VERIFIER(delai_avant_essai_s(6, false, 900, 0) == 900);
    VERIFIER(delai_avant_essai_s(1000000, false, 900, 0) == 900);
    VERIFIER(delai_avant_essai_s(2147483647, false, 900, 0) == 900);
    VERIFIER(delai_avant_essai_s(1, false, 600, 0) == 60);
    VERIFIER(delai_avant_essai_s(10, false, 600, 0) == 600);
    return 0;
}

constexpr int trop_de_requetes_impose_au_moins_trente_minutes() {
    VERIFIER(delai_avant_essai_s(0, true, 900, 0) == 1800);
    VERIFIER(delai_avant_essai_s(1, true, 900, 0) == 1800);
    VERIFIER(delai_avant_essai_s(3, true, 900, 0) == 1800);
    VERIFIER(delai_avant_essai_s(1, true, 900, 255) == 1980);
    VERIFIER(delai_avant_essai_s(0, true, 10800, 0) == 10800); // la période est déjà plus longue
    VERIFIER(delai_avant_essai_s(1, true, 10800, 0) == 1800);  // un échec avec 429 : 30 min, pas la période
    VERIFIER(delai_avant_essai_s(1, false, 900, 0) == 60);     // sans le 429 : le premier réessai
    return 0;
}

constexpr int jamais_moins_d_une_minute_entre_deux_requetes() {
    for (int echecs = 0; echecs < 12; ++echecs) {
        for (int a = 0; a <= 255; a += 51) {
            VERIFIER(delai_avant_essai_s(echecs, false, 600, static_cast<std::uint8_t>(a)) >= ESPACEMENT_MIN_S);
            VERIFIER(delai_avant_essai_s(echecs, true, 600, static_cast<std::uint8_t>(a)) >= ESPACEMENT_MIN_S);
        }
    }
    VERIFIER(delai_avant_essai_s(1, false, 10, 0) == 60); // même avec une période absurde
    VERIFIER(delai_avant_essai_s(0, false, 0, 0) == 60);
    return 0;
}

// ---- gzip ------------------------------------------------------------------------------------------------------------

constexpr std::uint8_t FIN_VIDE[] = {0, 0, 0, 0, 0, 0, 0, 0}; // CRC-32 et ISIZE factices

// Construit « en-tête + un octet DEFLATE + fin » : renvoie la taille écrite.
constexpr std::size_t flux(std::uint8_t *d, std::uint8_t drapeaux, const std::uint8_t *milieu, std::size_t n_milieu) {
    std::size_t p = 0;
    d[p++] = 0x1F;
    d[p++] = 0x8B;
    d[p++] = 8;
    d[p++] = drapeaux;
    for (int i = 0; i < 6; ++i)
        d[p++] = 0; // mtime, xfl, os
    for (std::size_t i = 0; i < n_milieu; ++i)
        d[p++] = milieu[i];
    d[p++] = 0x03; // un octet de flux DEFLATE
    for (std::uint8_t o : FIN_VIDE)
        d[p++] = o;
    return p;
}

constexpr int un_en_tete_sans_option_vaut_dix_octets() {
    std::uint8_t d[64] = {};
    const std::size_t n = flux(d, 0, nullptr, 0);
    VERIFIER(n == 19);
    VERIFIER(ecran::gzip_flux::est_gzip(d, n));
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10);
    return 0;
}

constexpr int le_nom_le_commentaire_l_extra_et_le_crc_allongent_l_en_tete() {
    std::uint8_t d[96] = {};
    const std::uint8_t nom[] = {'a', '.', 'j', 's', 'o', 'n', 0};
    std::size_t n = flux(d, 0x08, nom, sizeof nom); // FNAME
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10 + 7);
    const std::uint8_t commentaire[] = {'c', 'e', 0};
    n = flux(d, 0x10, commentaire, sizeof commentaire); // FCOMMENT
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10 + 3);
    const std::uint8_t extra[] = {5, 0, 1, 2, 3, 4, 5}; // longueur 5, petit-boutiste, puis 5 octets
    n = flux(d, 0x04, extra, sizeof extra);             // FEXTRA
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10 + 2 + 5);
    const std::uint8_t crc[] = {0x12, 0x34};
    n = flux(d, 0x02, crc, sizeof crc); // FHCRC
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10 + 2);
    const std::uint8_t tout[] = {3, 0,   9,   9, 9,    'n',
                                 0, 'c', 'c', 0, 0x12, 0x34}; // extra de 3, nom, commentaire, CRC : dans cet ordre
    n = flux(d, 0x1E, tout, sizeof tout);
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10 + 2 + 3 + 2 + 3 + 2);
    const std::uint8_t extra_long[] = {0x00, 0x01}; // longueur 256 (0x0100) : le deuxième octet pèse 256
    std::uint8_t grand[400] = {};
    std::size_t p = 0;
    grand[p++] = 0x1F;
    grand[p++] = 0x8B;
    grand[p++] = 8;
    grand[p++] = 0x04;
    for (int i = 0; i < 6; ++i)
        grand[p++] = 0;
    grand[p++] = extra_long[0];
    grand[p++] = extra_long[1];
    p += 256;
    grand[p++] = 0x03;
    p += 8;
    VERIFIER(ecran::gzip_flux::longueur_entete(grand, p) == 10 + 2 + 256);
    return 0;
}

constexpr int un_en_tete_coupe_ou_invalide_est_refuse() {
    std::uint8_t d[96] = {};
    const std::uint8_t nom_sans_zero[] = {'a', 'b'};
    std::size_t n = flux(d, 0x08, nom_sans_zero, sizeof nom_sans_zero);
    // Nom jamais terminé dans l'en-tête : le premier zéro est dans la fin factice (faite de zéros), il ne reste plus la place du flux : refusé.
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 0);
    const std::uint8_t extra_trop_long[] = {0xFF, 0xFF};
    n = flux(d, 0x04, extra_trop_long, sizeof extra_trop_long);
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 0);
    n = flux(d, 0x20, nullptr, 0); // bit réservé
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 0);
    n = flux(d, 0x80, nullptr, 0);
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 0);
    n = flux(d, 0, nullptr, 0);
    d[0] = 0x1E; // mauvais octet magique
    VERIFIER(!ecran::gzip_flux::est_gzip(d, n) && ecran::gzip_flux::longueur_entete(d, n) == 0);
    d[0] = 0x1F;
    d[1] = 0x8A;
    VERIFIER(!ecran::gzip_flux::est_gzip(d, n) && ecran::gzip_flux::longueur_entete(d, n) == 0);
    d[1] = 0x8B;
    d[2] = 7; // méthode inconnue
    VERIFIER(!ecran::gzip_flux::est_gzip(d, n) && ecran::gzip_flux::longueur_entete(d, n) == 0);
    d[2] = 8;
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10);
    VERIFIER(ecran::gzip_flux::longueur_entete(nullptr, 100) == 0);
    VERIFIER(ecran::gzip_flux::longueur_entete(d, 0) == 0);
    return 0;
}

constexpr int il_faut_la_place_d_un_octet_de_flux_et_de_la_fin() {
    std::uint8_t d[32] = {};
    const std::size_t n = flux(d, 0, nullptr, 0); // 10 + 1 + 8
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n) == 10);
    VERIFIER(ecran::gzip_flux::longueur_entete(d, n - 1) == 0); // plus d'octet de flux
    VERIFIER(ecran::gzip_flux::longueur_entete(d, 10) == 0);
    VERIFIER(ecran::gzip_flux::longueur_entete(d, 3) == 0);
    return 0;
}

constexpr int la_taille_annoncee_se_lit_en_petit_boutiste() {
    std::uint8_t d[32] = {};
    std::size_t n = flux(d, 0, nullptr, 0);
    d[n - 4] = 0x78;
    d[n - 3] = 0x56;
    d[n - 2] = 0x34;
    d[n - 1] = 0x12;
    VERIFIER(ecran::gzip_flux::taille_annoncee(d, n) == 0x12345678LL);
    d[n - 4] = d[n - 3] = d[n - 2] = d[n - 1] = 0xFF;
    VERIFIER(ecran::gzip_flux::taille_annoncee(d, n) == 4294967295LL);
    d[n - 4] = d[n - 3] = d[n - 2] = d[n - 1] = 0;
    VERIFIER(ecran::gzip_flux::taille_annoncee(d, n) == 0);
    VERIFIER(ecran::gzip_flux::taille_annoncee(d, 17) == -1);
    VERIFIER(ecran::gzip_flux::taille_annoncee(nullptr, 100) == -1);
    return 0;
}

constexpr int le_json_en_clair_n_est_pas_du_gzip() {
    const std::uint8_t clair[] = {'{', '"', 'v', 'e', 'r', 's', 'i', 'o', 'n', '"'};
    VERIFIER(!ecran::gzip_flux::est_gzip(clair, sizeof clair));
    VERIFIER(ecran::gzip_flux::longueur_entete(clair, sizeof clair) == 0);
    // La plus petite réponse réelle de Vigie (hambourg, 426 octets) arrive sans compression : elle commence par « { ».
    VERIFIER(HAMBOURG.size() > 0 && HAMBOURG[0] == '{');
    VERIFIER(!ecran::gzip_flux::est_gzip(static_cast<const std::uint8_t *>(nullptr), 0));
    return 0;
}

template <std::size_t N> constexpr int reel(const std::uint8_t (&octets)[N], const AttGz &attendu, sv json) {
    VERIFIER(ecran::gzip_flux::est_gzip(octets, N));
    VERIFIER(ecran::gzip_flux::longueur_entete(octets, N) == attendu.entete);
    VERIFIER(ecran::gzip_flux::taille_annoncee(octets, N) == attendu.taille);
    VERIFIER(static_cast<std::size_t>(attendu.taille) ==
             json.size());     // la taille annoncée est celle du corps décompressé
    VERIFIER(N < json.size()); // et le flux est bien plus court : la compression sert
    return 0;
}

constexpr int les_flux_reels_ont_la_longueur_d_en_tete_et_la_taille_lues_par_zlib() {
    VERIFIER(reel(GZ_ANCONA, ATT_GZ_ANCONA, JSON_ANCONA) == 0);
    VERIFIER(reel(GZ_BODO, ATT_GZ_BODO, JSON_BODO) == 0);
    VERIFIER(reel(GZ_EBERSWALDE, ATT_GZ_EBERSWALDE, JSON_EBERSWALDE) == 0);
    VERIFIER(reel(GZ_FUNCHAL, ATT_GZ_FUNCHAL, JSON_FUNCHAL) == 0);
    VERIFIER(reel(GZ_LA_PALMA, ATT_GZ_LA_PALMA, JSON_LA_PALMA) == 0);
    VERIFIER(reel(GZ_LONDRES, ATT_GZ_LONDRES, JSON_LONDRES) == 0);
    VERIFIER(reel(GZ_OSLO, ATT_GZ_OSLO, JSON_OSLO) == 0);
    VERIFIER(reel(GZ_TOKYO, ATT_GZ_TOKYO, JSON_TOKYO) == 0);
    VERIFIER(reel(GZ_VALENCE, ATT_GZ_VALENCE, JSON_VALENCE) == 0);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(adresses_valables_nettoyees)                                                                                     \
    X(adresses_refusees)                                                                                               \
    X(la_longueur_de_l_adresse_est_bornee)                                                                             \
    X(les_adresses_sont_celles_des_captures_reelles)                                                                   \
    X(coordonnees_arrondies_au_millieme_la_moitie_loin_de_zero)                                                        \
    X(la_langue_de_l_ecran_puis_l_anglais)                                                                             \
    X(coordonnees_ou_langue_invalides_sont_refusees)                                                                   \
    X(le_tampon_juste_suffit_et_un_octet_de_moins_refuse)                                                              \
    X(la_plus_longue_requete_tient_dans_le_tampon_prevu)                                                               \
    X(la_periode_est_bornee_entre_10_et_180_minutes)                                                                   \
    X(le_premier_essai_attend_une_a_deux_minutes)                                                                      \
    X(apres_un_succes_une_periode_plus_au_plus_un_dixieme)                                                             \
    X(apres_un_echec_on_double_jusqu_a_la_periode)                                                                     \
    X(trop_de_requetes_impose_au_moins_trente_minutes)                                                                 \
    X(jamais_moins_d_une_minute_entre_deux_requetes)                                                                   \
    X(un_en_tete_sans_option_vaut_dix_octets)                                                                          \
    X(le_nom_le_commentaire_l_extra_et_le_crc_allongent_l_en_tete)                                                     \
    X(un_en_tete_coupe_ou_invalide_est_refuse)                                                                         \
    X(il_faut_la_place_d_un_octet_de_flux_et_de_la_fin)                                                                \
    X(la_taille_annoncee_se_lit_en_petit_boutiste)                                                                     \
    X(le_json_en_clair_n_est_pas_du_gzip)                                                                              \
    X(les_flux_reels_ont_la_longueur_d_en_tete_et_la_taille_lues_par_zlib)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
