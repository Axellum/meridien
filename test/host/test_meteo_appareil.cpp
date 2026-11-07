// Tests du noyau pur « météo sur l'appareil » : adresse de la requête (meteo_requete.hpp), règles de zone
// (meteo_zone_regles.hpp), synthèse et « périmé » (meteo_synthese.hpp), rythme des essais (meteo_planning.hpp) et cache de la NVS
// (meteo_cache.hpp). Les valeurs météo viennent de la VRAIE réponse de Paris capturée le 06/11/2026 (donnees/openmeteo_paris.inc,
// dont les valeurs attendues sont calculées par Python : voir donnees/LISEZMOI.md) ; les cas limites sont écrits à la main.
#include <cstdint>
#include <limits>
#include <string_view>

#include "ecran/meteo_cache.hpp"
#include "ecran/meteo_openmeteo.hpp"
#include "ecran/meteo_planning.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_synthese.hpp"
#include "ecran/meteo_zone_regles.hpp"
#include "mini.hpp"

using namespace ecran::meteo;
using sv = std::string_view;

namespace {

constexpr sv PARIS =
#include "donnees/openmeteo_paris.inc"
    ;
constexpr std::int64_t RECU = 1793966500; // 100 s après current.time de la réponse de Paris
constexpr Releve PARIS_R = analyser(PARIS, RECU).releve;

// Écrite À LA MAIN, volontairement : si la requête change par accident, ce test le voit. La requête riche (courant complet,
// 24 heures, 7 jours, pluie par quarts d'heure) : voir meteo_requete.hpp pour la raison de chaque paramètre.
constexpr sv URL_LYON =
    "https://api.open-meteo.com/v1/forecast?latitude=45.74906&longitude=4.84789"
    "&current=temperature_2m,apparent_temperature,is_day,precipitation,weather_code,cloud_cover,wind_speed_10m,"
    "wind_direction_10m,wind_gusts_10m,pressure_msl,relative_humidity_2m"
    "&hourly=temperature_2m,apparent_temperature,relative_humidity_2m,precipitation_probability,precipitation,"
    "weather_code,cloud_cover,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,is_day&forecast_hours=25"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,"
    "sunrise,sunset,sunshine_duration,uv_index_max,precipitation_sum,snowfall_sum,precipitation_hours,"
    "precipitation_probability_max,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant"
    "&minutely_15=precipitation&forecast_minutely_15=9"
    "&forecast_days=7&timezone=auto&timeformat=unixtime";

constexpr double NAN_ = std::numeric_limits<double>::quiet_NaN();
constexpr double INF_ = std::numeric_limits<double>::infinity();

// ---- adresse de la requête ---------------------------------------------------------------------------------------

constexpr int requete_de_lyon_exacte() {
    char tampon[TAILLE_REQUETE_MAX] = {};
    const std::size_t n = construire_requete(4574906, 484789, tampon, sizeof(tampon));
    VERIFIER(n == URL_LYON.size());
    VERIFIER(sv(tampon, n) == URL_LYON);
    VERIFIER(tampon[n] == '\0');
    return 0;
}

constexpr bool debut_de(sv texte, sv attendu) {
    return texte.size() >= attendu.size() && texte.substr(0, attendu.size()) == attendu;
}

constexpr int requete_coordonnees_negatives_et_chiffres_de_remplissage() {
    char t[TAILLE_REQUETE_MAX] = {};
    std::size_t n = construire_requete(-3386785, 15120732, t, sizeof(t)); // Sydney
    VERIFIER(n != 0);
    VERIFIER(
        debut_de(sv(t, n), "https://api.open-meteo.com/v1/forecast?latitude=-33.86785&longitude=151.20732&current="));
    n = construire_requete(0, 0, t, sizeof(t));
    VERIFIER(debut_de(sv(t, n), "https://api.open-meteo.com/v1/forecast?latitude=0.00000&longitude=0.00000&current="));
    n = construire_requete(-1, 1, t, sizeof(t)); // les zéros de tête de la fraction ne disparaissent pas
    VERIFIER(debut_de(sv(t, n), "https://api.open-meteo.com/v1/forecast?latitude=-0.00001&longitude=0.00001&current="));
    n = construire_requete(100000, -100000, t, sizeof(t)); // un degré pile
    VERIFIER(debut_de(sv(t, n), "https://api.open-meteo.com/v1/forecast?latitude=1.00000&longitude=-1.00000&current="));
    n = construire_requete(-50, 7005, t, sizeof(t));
    VERIFIER(debut_de(sv(t, n), "https://api.open-meteo.com/v1/forecast?latitude=-0.00050&longitude=0.07005&current="));
    n = construire_requete(-9000000, -18000000, t, sizeof(t));
    VERIFIER(
        debut_de(sv(t, n), "https://api.open-meteo.com/v1/forecast?latitude=-90.00000&longitude=-180.00000&current="));
    return 0;
}

constexpr int requete_aux_bornes_et_refus_hors_bornes() {
    char t[TAILLE_REQUETE_MAX] = {};
    // Les plus longues coordonnées remplissent exactement TAILLE_REQUETE_SANS_MODELE_MAX (adresse « Automatique »).
    VERIFIER(construire_requete(-9000000, -18000000, t, sizeof(t)) == TAILLE_REQUETE_SANS_MODELE_MAX - 1);
    VERIFIER(construire_requete(9000000, 18000000, t, sizeof(t)) ==
             TAILLE_REQUETE_SANS_MODELE_MAX - 3); // sans les deux signes
    VERIFIER(construire_requete(9000001, 0, t, sizeof(t)) == 0);
    VERIFIER(construire_requete(-9000001, 0, t, sizeof(t)) == 0);
    VERIFIER(construire_requete(0, 18000001, t, sizeof(t)) == 0);
    VERIFIER(construire_requete(0, -18000001, t, sizeof(t)) == 0);
    VERIFIER(construire_requete(std::numeric_limits<std::int32_t>::min(), 0, t, sizeof(t)) == 0);
    return 0;
}

constexpr int requete_refuse_une_place_insuffisante_sans_deborder() {
    char t[TAILLE_REQUETE_MAX + 8] = {};
    const std::size_t n = URL_LYON.size();
    VERIFIER(construire_requete(4574906, 484789, t, n + 1) == n); // juste assez : l'adresse et le zéro
    VERIFIER(construire_requete(4574906, 484789, t, n) == 0);     // un octet de moins : refusé
    VERIFIER(construire_requete(4574906, 484789, t, 0) == 0);
    VERIFIER(construire_requete(4574906, 484789, t, 1) == 0);
    VERIFIER(construire_requete(4574906, 484789, nullptr, 100) == 0);
    // Rien n'est écrit après la fin annoncée : on remplit de 'X' et on regarde.
    char x[URL_LYON.size() + 8] = {};
    for (char &c : x)
        c = 'X';
    VERIFIER(construire_requete(4574906, 484789, x, n + 1) == n);
    VERIFIER(x[n] == '\0' && x[n + 1] == 'X' && x[n + 7] == 'X');
    return 0;
}

constexpr int requete_et_ses_en_tetes_depassent_le_tampon_d_envoi_par_defaut() {
    // esp_http_client écrit la LIGNE de requête (« GET /chemin?… HTTP/1.1 ») d'un coup dans son tampon d'envoi, 512 octets par
    // défaut, et échoue (« Out of buffer ») si elle n'y tient pas ; les en-têtes (Host, User-Agent…) partent ensuite par
    // tranches de ce même tampon (esp_http_client.c d'ESP-IDF 5.5.5, esp_http_client_request_send). L'adresse seule fait 863 octets
    // au plus, la première ligne 851 : 512 ne suffit pas, main/meteo_client.cpp demande buffer_size_tx = 1024.
    constexpr std::size_t SCHEMA_ET_HOTE = sv("https://api.open-meteo.com").size();
    constexpr std::size_t PREMIERE_LIGNE_MAX =
        sv("GET ").size() + (TAILLE_REQUETE_SANS_MODELE_MAX - 1 - SCHEMA_ET_HOTE) + sv(" HTTP/1.1\r\n").size();
    VERIFIER(TAILLE_REQUETE_SANS_MODELE_MAX == 863);
    VERIFIER(PREMIERE_LIGNE_MAX == 851);
    VERIFIER(PREMIERE_LIGNE_MAX > 512);         // le tampon par défaut ne suffit pas
    VERIFIER(PREMIERE_LIGNE_MAX + 100 <= 1024); // marge d'au moins 100 octets sous 1024 (en-têtes inclus dans la marge)
    // Avec le plus long modèle de la table : « &models=italia_meteo_arpae_icon_2i » ajoute 34 caractères, la première ligne en
    // fait 885, la marge sous 1024 reste de 139 octets (au moins 100 exigés).
    VERIFIER(MODELES_PARAMETRE_MAX == 34);
    VERIFIER(TAILLE_REQUETE_MAX == 897);
    constexpr std::size_t PREMIERE_LIGNE_AVEC_MODELE_MAX = PREMIERE_LIGNE_MAX + MODELES_PARAMETRE_MAX;
    VERIFIER(PREMIERE_LIGNE_AVEC_MODELE_MAX == 885);
    VERIFIER(PREMIERE_LIGNE_AVEC_MODELE_MAX + 100 <= 1024);
    // Et la VRAIE plus longue adresse, construite : coordonnées les plus longues et modèle le plus long.
    char t[TAILLE_REQUETE_MAX] = {};
    const std::size_t n = construire_requete(-9000000, -18000000, t, sizeof(t), "italia_meteo_arpae_icon_2i");
    VERIFIER(n == TAILLE_REQUETE_MAX - 1);
    VERIFIER(sv("GET ").size() + (n - SCHEMA_ET_HOTE) + sv(" HTTP/1.1\r\n").size() == PREMIERE_LIGNE_AVEC_MODELE_MAX);
    return 0;
}

// ---- paramètre models= (source météo choisie) ---------------------------------------------------------------------------

constexpr int requete_sans_modele_est_celle_d_avant_a_l_octet_pres() {
    // « Automatique » (modèle vide) : exactement l'adresse écrite à la main ci-dessus, avec ou sans l'argument explicite.
    char a[TAILLE_REQUETE_MAX] = {};
    char b[TAILLE_REQUETE_MAX] = {};
    const std::size_t na = construire_requete(4574906, 484789, a, sizeof(a));
    const std::size_t nb = construire_requete(4574906, 484789, b, sizeof(b), sv{});
    VERIFIER(na == URL_LYON.size() && nb == na);
    VERIFIER(sv(a, na) == URL_LYON && sv(b, nb) == URL_LYON);
    VERIFIER(sv(a, na).find("models") == sv::npos);
    return 0;
}

constexpr int requete_avec_un_modele_ajoute_seulement_la_fin() {
    char t[TAILLE_REQUETE_MAX] = {};
    std::size_t n = construire_requete(4574906, 484789, t, sizeof(t), "meteoswiss_icon_seamless");
    VERIFIER(n == URL_LYON.size() + 32);
    // Tout ce qui précède est l'adresse d'avant, au caractère près ; le modèle est la dernière chose.
    VERIFIER(sv(t, URL_LYON.size()) == URL_LYON);
    VERIFIER(sv(t + URL_LYON.size(), 32) == sv("&models=meteoswiss_icon_seamless"));
    VERIFIER(t[n] == '\0');
    n = construire_requete(4574906, 484789, t, sizeof(t), "gfs_seamless");
    VERIFIER(n == URL_LYON.size() + sv("&models=gfs_seamless").size());
    VERIFIER(sv(t + URL_LYON.size(), n - URL_LYON.size()) == sv("&models=gfs_seamless"));
    return 0;
}

constexpr int requete_refuse_un_modele_douteux() {
    char t[TAILLE_REQUETE_MAX] = {};
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "a,b") == 0);   // deux modèles : variables renommées
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "a&b=c") == 0); // un paramètre glissé
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "gfs seamless") == 0); // espace
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "GFS_SEAMLESS") ==
             0); // majuscules : jamais celles de l'API
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "gfs%5Fseamless") == 0); // « % »
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "gfs\n") == 0);
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "gfs-seamless") ==
             0); // « - » n'est dans aucun identifiant
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "italia_meteo_arpae_icon_2ix") ==
             0); // 27 caractères : trop long
    VERIFIER(construire_requete(4574906, 484789, t, sizeof(t), "italia_meteo_arpae_icon_2i") !=
             0); // 26 : le plus long permis
    VERIFIER(modeles_valide(sv{}) && modeles_valide("gfs_seamless") && !modeles_valide("gfs_seamless,icon_seamless"));
    return 0;
}

constexpr int requete_avec_un_modele_ne_deborde_pas() {
    char t[TAILLE_REQUETE_MAX + 8] = {};
    const std::size_t n = URL_LYON.size() + sv("&models=ecmwf_ifs025").size();
    VERIFIER(construire_requete(4574906, 484789, t, n + 1, "ecmwf_ifs025") == n); // juste assez
    VERIFIER(construire_requete(4574906, 484789, t, n, "ecmwf_ifs025") == 0);     // un octet de moins : refusé
    VERIFIER(construire_requete(4574906, 484789, t, URL_LYON.size() + 1, "ecmwf_ifs025") ==
             0); // la place de l'adresse seule
    VERIFIER(construire_requete(4574906, 484789, t, URL_LYON.size() + 1) == URL_LYON.size()); // …suffit sans modèle
    VERIFIER(construire_requete(9000001, 0, t, sizeof(t), "ecmwf_ifs025") == 0); // les bornes de coordonnées restent
    return 0;
}

// ---- règles de zone ----------------------------------------------------------------------------------------------

constexpr int degres_vers_e5_arrondi_et_bornes() {
    std::int32_t e5 = 0;
    VERIFIER(latitude_vers_e5(static_cast<double>(45.74906F), e5) && e5 == 4574906);
    VERIFIER(longitude_vers_e5(static_cast<double>(4.84789F), e5) && e5 == 484789);
    VERIFIER(latitude_vers_e5(static_cast<double>(-33.86785F), e5) && e5 == -3386785);
    VERIFIER(longitude_vers_e5(static_cast<double>(151.20732F), e5) && e5 == 15120732);
    VERIFIER(latitude_vers_e5(0.000004, e5) && e5 == 0);
    VERIFIER(latitude_vers_e5(0.000006, e5) && e5 == 1);
    VERIFIER(latitude_vers_e5(-0.000006, e5) && e5 == -1); // l'arrondi s'éloigne de zéro des deux côtés
    VERIFIER(latitude_vers_e5(90.0, e5) && e5 == 9000000);
    VERIFIER(latitude_vers_e5(-90.0, e5) && e5 == -9000000);
    VERIFIER(longitude_vers_e5(180.0, e5) && e5 == 18000000);
    VERIFIER(longitude_vers_e5(-180.0, e5) && e5 == -18000000);
    VERIFIER(!latitude_vers_e5(90.000001, e5));
    VERIFIER(!latitude_vers_e5(-90.5, e5));
    VERIFIER(!longitude_vers_e5(180.000001, e5));
    VERIFIER(!longitude_vers_e5(-181.0, e5));
    VERIFIER(latitude_vers_e5(100.0, e5) == false); // une longitude de 100 n'est pas une latitude
    VERIFIER(longitude_vers_e5(100.0, e5) && e5 == 10000000);
    return 0;
}

constexpr int degres_non_finis_refuses() {
    std::int32_t e5 = 12345;
    VERIFIER(!latitude_vers_e5(NAN_, e5));
    VERIFIER(!longitude_vers_e5(NAN_, e5));
    VERIFIER(!latitude_vers_e5(INF_, e5));
    VERIFIER(!latitude_vers_e5(-INF_, e5));
    VERIFIER(!longitude_vers_e5(INF_, e5));
    VERIFIER(e5 == 12345); // une valeur refusée ne touche pas la sortie
    return 0;
}

constexpr int nom_de_zone_valide_ou_non() {
    VERIFIER(nom_de_zone_valide("Lyon", 48));
    VERIFIER(nom_de_zone_valide("Saint-\xC3\x89tienne", 48));     // « Saint-Étienne » en UTF-8
    VERIFIER(nom_de_zone_valide("\xE5\x8C\x97\xE4\xBA\xAC", 48)); // trois octets par lettre
    VERIFIER(nom_de_zone_valide("\xF0\x9F\x8F\xA0 Maison", 48));  // quatre octets
    VERIFIER(nom_de_zone_valide("a", 2));
    VERIFIER(!nom_de_zone_valide("", 48));
    VERIFIER(!nom_de_zone_valide("   ", 48));
    VERIFIER(!nom_de_zone_valide("Lyon\n", 48));
    VERIFIER(!nom_de_zone_valide("Ly\ton", 48));
    VERIFIER(!nom_de_zone_valide("Ly\x7F"
                                 "on",
                                 48));
    VERIFIER(!nom_de_zone_valide(nullptr, 48));
    VERIFIER(!nom_de_zone_valide("Lyon", 1));
    return 0;
}

constexpr int nom_de_zone_utf8_mal_forme_refuse() {
    VERIFIER(!nom_de_zone_valide("Lyo\xC3", 48)); // coupé au milieu d'une lettre
    VERIFIER(!nom_de_zone_valide("\x89"
                                 "a",
                                 48));                     // octet de continuation isolé
    VERIFIER(!nom_de_zone_valide("\xC0\x80", 48));         // « NUL » en forme trop longue
    VERIFIER(!nom_de_zone_valide("\xC1\xBF", 48));         // forme trop longue de « ? »
    VERIFIER(!nom_de_zone_valide("\xE0\x80\x80", 48));     // forme trop longue sur 3 octets
    VERIFIER(!nom_de_zone_valide("\xED\xA0\x80", 48));     // substitut UTF-16 (U+D800)
    VERIFIER(!nom_de_zone_valide("\xF0\x80\x80\x80", 48)); // forme trop longue sur 4 octets
    VERIFIER(!nom_de_zone_valide("\xF4\x90\x80\x80", 48)); // au-delà de U+10FFFF
    VERIFIER(!nom_de_zone_valide("\xFF", 48));
    VERIFIER(nom_de_zone_valide("\xED\x9F\xBF", 48));     // U+D7FF, juste avant les substituts : bien formé
    VERIFIER(nom_de_zone_valide("\xF4\x8F\xBF\xBF", 48)); // U+10FFFF : bien formé
    return 0;
}

constexpr int nom_de_zone_sans_zero_final_refuse() {
    const char plein[4] = {'L', 'y', 'o', 'n'}; // pas de zéro dans les 4 octets
    VERIFIER(!nom_de_zone_valide(plein, 4));
    const char juste[5] = {'L', 'y', 'o', 'n', '\0'};
    VERIFIER(nom_de_zone_valide(juste, 5));
    char long_[48] = {};
    for (int i = 0; i < 47; ++i)
        long_[i] = 'a';
    VERIFIER(nom_de_zone_valide(long_, 48)); // 47 lettres et le zéro
    long_[47] = 'a';
    VERIFIER(!nom_de_zone_valide(long_, 48)); // 48 lettres : pas de place pour le zéro
    return 0;
}

constexpr int fuseau_de_zone_valide_ou_non() {
    VERIFIER(fuseau_de_zone_valide("Europe/Paris", 40));
    VERIFIER(fuseau_de_zone_valide("America/Argentina/Buenos_Aires", 40));
    VERIFIER(fuseau_de_zone_valide("Etc/GMT+5", 40));
    VERIFIER(fuseau_de_zone_valide("Etc/GMT-14", 40));
    VERIFIER(!fuseau_de_zone_valide("", 40));
    VERIFIER(!fuseau_de_zone_valide("Europe Paris", 40));
    VERIFIER(!fuseau_de_zone_valide("Europe/Paris\n", 40));
    VERIFIER(!fuseau_de_zone_valide("Europe/\xC3\xA9", 40));
    VERIFIER(!fuseau_de_zone_valide("../etc", 40));
    VERIFIER(!fuseau_de_zone_valide(nullptr, 40));
    const char plein[3] = {'U', 'T', 'C'};
    VERIFIER(!fuseau_de_zone_valide(plein, 3));
    VERIFIER(fuseau_de_zone_valide("UTC", 4));
    return 0;
}

// ---- synthèse et « périmé » ---------------------------------------------------------------------------------------

constexpr int synthese_de_paris() {
    VERIFIER(PARIS_R.nb_jours == 7 && PARIS_R.temperature_dixiemes == 116);
    const Synthese s = synthetiser(PARIS_R, RECU + 60, 0);
    VERIFIER(!s.perime);
    VERIFIER(s.maj_s == RECU);
    VERIFIER(s.temperature_dixiemes == 116 && s.code_wmo == 3 && s.jour);
    VERIFIER(s.libelle == sv("Couvert"));
    VERIFIER(s.minmax_connus && s.min_dixiemes == 73 && s.max_dixiemes == 128); // jours[0] de Paris, calculé par Python
    return 0;
}

constexpr int libelle_selon_le_code_et_le_jour() {
    Releve r = PARIS_R;
    r.code_wmo = 0;
    r.jour = true;
    VERIFIER(synthetiser(r, RECU, 0).libelle == sv("Ciel dégagé"));
    r.code_wmo = 61;
    VERIFIER(synthetiser(r, RECU, 0).libelle == sv("Pluie faible"));
    r.code_wmo = 7; // absent du tableau d'Open-Meteo
    VERIFIER(synthetiser(r, RECU, 0).libelle == sv("Météo inconnue"));
    // Le libellé tient dans les 40 octets de MeteoEtat::libelle, zéro compris (tous les codes, jour et nuit).
    for (int code = 0; code < 100; ++code) {
        VERIFIER(decrire(code, true).libelle.size() < 40);
        VERIFIER(decrire(code, false).libelle.size() < 40);
    }
    return 0;
}

constexpr int peremption_apres_deux_heures() {
    VERIFIER(!est_perime(PARIS_R, RECU, 0));
    VERIFIER(!est_perime(PARIS_R, RECU + 7199, 0));
    VERIFIER(!est_perime(PARIS_R, RECU + 7200, 0)); // pile deux heures : encore frais
    VERIFIER(est_perime(PARIS_R, RECU + 7201, 0));
    VERIFIER(est_perime(PARIS_R, RECU + 6 * 3600 + 1, 0));
    VERIFIER(est_perime(PARIS_R, RECU + 30 * 86400, 0));
    return 0;
}

constexpr int peremption_apres_des_echecs_repetes() {
    VERIFIER(!est_perime(PARIS_R, RECU + 1801, 2)); // deux échecs seulement
    VERIFIER(!est_perime(PARIS_R, RECU + 1800, 3)); // trois échecs mais moins de 30 min (pile) : pas encore
    VERIFIER(est_perime(PARIS_R, RECU + 1801, 3));
    VERIFIER(est_perime(PARIS_R, RECU + 3000, 50));
    VERIFIER(!est_perime(PARIS_R, RECU + 100, 50)); // box redémarrée : une réponse toute fraîche n'est pas périmée
    VERIFIER(!est_perime(PARIS_R, RECU + 1801, 0)); // pas d'échec : 30 min, c'est normal (cycle de 15 min)
    return 0;
}

constexpr int peremption_quand_l_horloge_est_incertaine() {
    VERIFIER(est_perime(PARIS_R, 0, 0)); // horloge non réglée (début de l'année 1970)
    VERIFIER(est_perime(PARIS_R, 1000, 0));
    VERIFIER(!est_perime(PARIS_R, RECU - 300, 0)); // dans la tolérance de 5 min
    VERIFIER(est_perime(PARIS_R, RECU - 301, 0));  // l'heure a reculé de plus de 5 min depuis la réponse
    VERIFIER(est_perime(PARIS_R, RECU - 86400, 0));
    return 0;
}

constexpr int min_max_du_jour_et_replis() {
    // Au milieu du 3e jour (jours[2] : min 4,3 °C, max 8,9 °C) : on prend le jour qui contient l'heure actuelle.
    const std::int64_t jour2 = PARIS_R.jours[2].date_s + 3600;
    Synthese s = synthetiser(PARIS_R, jour2, 0);
    VERIFIER(s.minmax_connus && s.min_dixiemes == 43 && s.max_dixiemes == 89);
    VERIFIER(s.perime); // la réponse a ~1,5 jour : périmée, mais lisible
    // Au-delà des sept jours : retombe sur le jour de la mesure (jours[0]).
    s = synthetiser(PARIS_R, RECU + 30 * 86400, 0);
    VERIFIER(s.minmax_connus && s.min_dixiemes == 73 && s.max_dixiemes == 128);
    // Sans prévision par jour : min et max valent la température du moment, et `minmax_connus` le dit.
    Releve sans = PARIS_R;
    sans.nb_jours = 0;
    s = synthetiser(sans, RECU, 0);
    VERIFIER(!s.minmax_connus && s.min_dixiemes == 116 && s.max_dixiemes == 116);
    return 0;
}

constexpr int dixiemes_en_texte_francais() {
    char t[16] = {};
    std::size_t n = ecrire_dixiemes(116, t, sizeof(t));
    VERIFIER(n == 4 && sv(t, n) == sv("11,6") && t[n] == '\0');
    n = ecrire_dixiemes(-5, t, sizeof(t));
    VERIFIER(sv(t, n) == sv("-0,5")); // le signe d'une valeur entre -1 et 0 ne se perd pas
    n = ecrire_dixiemes(0, t, sizeof(t));
    VERIFIER(sv(t, n) == sv("0,0"));
    n = ecrire_dixiemes(5, t, sizeof(t));
    VERIFIER(sv(t, n) == sv("0,5"));
    n = ecrire_dixiemes(-123, t, sizeof(t));
    VERIFIER(sv(t, n) == sv("-12,3"));
    n = ecrire_dixiemes(1000, t, sizeof(t));
    VERIFIER(sv(t, n) == sv("100,0"));
    n = ecrire_dixiemes(-900, t, sizeof(t));
    VERIFIER(sv(t, n) == sv("-90,0"));
    VERIFIER(ecrire_dixiemes(116, t, 5) == 4); // « 11,6 » et le zéro : 5 octets
    VERIFIER(ecrire_dixiemes(116, t, 4) == 0);
    VERIFIER(ecrire_dixiemes(-5, t, 5) == 4);
    VERIFIER(ecrire_dixiemes(-5, t, 4) == 0);
    VERIFIER(ecrire_dixiemes(5, t, 0) == 0);
    VERIFIER(ecrire_dixiemes(5, nullptr, 8) == 0);
    return 0;
}

// ---- rythme des essais -------------------------------------------------------------------------------------------

constexpr int planning_normal_et_reessais() {
    VERIFIER(delai_avant_essai_s(0, false, 0) == 900);
    VERIFIER(delai_avant_essai_s(0, false, 255) == 990);
    VERIFIER(delai_avant_essai_s(0, false, 128) == 945); // 900 + 90 × 128 / 255
    VERIFIER(delai_avant_essai_s(-4, false, 0) == 900);  // un compteur négatif vaut « aucun échec »
    VERIFIER(delai_avant_essai_s(1, false, 0) == 15);
    VERIFIER(delai_avant_essai_s(1, false, 255) == 16);
    VERIFIER(delai_avant_essai_s(2, false, 0) == 30);
    VERIFIER(delai_avant_essai_s(3, false, 0) == 60);
    VERIFIER(delai_avant_essai_s(4, false, 0) == 120);
    VERIFIER(delai_avant_essai_s(5, false, 0) == 240);
    VERIFIER(delai_avant_essai_s(6, false, 0) == 480);
    VERIFIER(delai_avant_essai_s(7, false, 0) == 900); // 960 plafonné à 15 min
    VERIFIER(delai_avant_essai_s(8, false, 0) == 900);
    VERIFIER(delai_avant_essai_s(1000000, false, 0) == 900);
    VERIFIER(delai_avant_essai_s(std::numeric_limits<int>::max(), false, 255) == 990);
    return 0;
}

constexpr int planning_jamais_a_l_envers_ni_plus_court_que_le_pas() {
    std::int64_t precedent = 0;
    for (int n = 1; n <= 40; ++n) {
        const std::int64_t d = delai_avant_essai_s(n, false, 0);
        VERIFIER(d >= precedent);
        VERIFIER(d >= PREMIER_REESSAI_S && d <= PERIODE_S);
        precedent = d;
    }
    for (int alea = 0; alea <= 255; ++alea) { // le décalage n'ajoute jamais plus d'un dixième
        const std::int64_t d = delai_avant_essai_s(3, false, static_cast<std::uint8_t>(alea));
        VERIFIER(d >= 60 && d <= 66);
    }
    return 0;
}

constexpr int planning_apres_un_trop_de_requetes() {
    VERIFIER(delai_avant_essai_s(0, true, 0) == 900); // déjà plus long que le minimum
    VERIFIER(delai_avant_essai_s(1, true, 0) == 600);
    VERIFIER(delai_avant_essai_s(3, true, 0) == 600);
    VERIFIER(delai_avant_essai_s(6, true, 0) == 600); // 480 < 600
    VERIFIER(delai_avant_essai_s(7, true, 0) == 900);
    VERIFIER(delai_avant_essai_s(1, true, 255) == 660);
    return 0;
}

// ---- cache de la NVS ---------------------------------------------------------------------------------------------

constexpr int cache_aller_retour_avec_la_position() {
    std::uint8_t buf[TAILLE_CACHE_ZONE_MAX] = {};
    const std::size_t n = serialiser_cache(PARIS_R, 4574906, 484789, buf, sizeof(buf));
    VERIFIER(n != 0 && n <= TAILLE_CACHE_ZONE_MAX);
    const LectureCache lu = deserialiser_cache(buf, n);
    VERIFIER(lu.ok());
    VERIFIER(lu.latitude_e5 == 4574906 && lu.longitude_e5 == 484789);
    VERIFIER(lu.releve.recu_a_s == RECU && lu.releve.temperature_dixiemes == 116 && lu.releve.code_wmo == 3);
    VERIFIER(lu.releve.nb_jours == 7 && lu.releve.jours[2].min_dixiemes == 43 &&
             lu.releve.jours[6].max_dixiemes == 159);
    VERIFIER(sv(lu.releve.fuseau) == sv("Europe/Paris"));
    // Positions négatives et aux bornes.
    const std::size_t m = serialiser_cache(PARIS_R, -9000000, -18000000, buf, sizeof(buf));
    VERIFIER(m != 0);
    const LectureCache b = deserialiser_cache(buf, m);
    VERIFIER(b.ok() && b.latitude_e5 == -9000000 && b.longitude_e5 == -18000000);
    return 0;
}

constexpr int cache_refuse_toute_alteration() {
    std::uint8_t buf[TAILLE_CACHE_ZONE_MAX] = {};
    const std::size_t n = serialiser_cache(PARIS_R, 4574906, 484789, buf, sizeof(buf));
    VERIFIER(n != 0);
    for (std::size_t i = 0; i < n; ++i) { // un seul bit changé, à chaque octet : toujours refusé (position comprise)
        std::uint8_t copie[TAILLE_CACHE_ZONE_MAX] = {};
        for (std::size_t j = 0; j < n; ++j)
            copie[j] = buf[j];
        copie[i] = static_cast<std::uint8_t>(copie[i] ^ 0x10U);
        VERIFIER(!deserialiser_cache(copie, n).ok());
    }
    VERIFIER(!deserialiser_cache(buf, n - 1).ok()); // tronqué
    VERIFIER(!deserialiser_cache(buf, 0).ok());
    VERIFIER(!deserialiser_cache(buf, 7).ok());
    VERIFIER(!deserialiser_cache(nullptr, n).ok());
    std::uint8_t trop[TAILLE_CACHE_ZONE_MAX + 4] = {};
    for (std::size_t j = 0; j < n; ++j)
        trop[j] = buf[j];
    VERIFIER(!deserialiser_cache(trop, n + 1).ok()); // un octet de plus
    return 0;
}

constexpr int cache_refuse_un_autre_format_et_une_position_invraisemblable() {
    // Le relevé seul (format de meteo_openmeteo.hpp) n'est pas un cache : il manque la position.
    std::uint8_t seul[TAILLE_CACHE_MAX] = {};
    const std::size_t s = serialiser(PARIS_R, seul, sizeof(seul));
    VERIFIER(s != 0);
    VERIFIER(deserialiser_cache(seul, s).diag.erreur == Erreur::Corrompu);

    std::uint8_t buf[TAILLE_CACHE_ZONE_MAX] = {};
    const std::size_t n = serialiser_cache(PARIS_R, 4574906, 484789, buf, sizeof(buf));
    VERIFIER(n != 0);
    // Version inconnue, avec une somme de contrôle recalculée : refusée pour sa version, pas pour sa somme.
    buf[2] = 2;
    std::uint32_t crc = detail::crc32(buf, n - 4);
    for (int i = 0; i < 4; ++i)
        buf[n - 4 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((crc >> (8 * i)) & 0xFFU);
    VERIFIER(deserialiser_cache(buf, n).diag.erreur == Erreur::VersionInconnue);
    buf[2] = 1;
    // Latitude de 90,00001 degrés avec une bonne somme de contrôle : refusée comme invraisemblable.
    const std::uint32_t lat = 9000001;
    for (int i = 0; i < 4; ++i)
        buf[3 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((lat >> (8 * i)) & 0xFFU);
    crc = detail::crc32(buf, n - 4);
    for (int i = 0; i < 4; ++i)
        buf[n - 4 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((crc >> (8 * i)) & 0xFFU);
    const LectureCache l = deserialiser_cache(buf, n);
    VERIFIER(!l.ok() && l.diag.erreur == Erreur::HorsPlage);
    // On n'écrit pas non plus un tel cache.
    VERIFIER(serialiser_cache(PARIS_R, 9000001, 0, buf, sizeof(buf)) == 0);
    VERIFIER(serialiser_cache(PARIS_R, 0, -18000001, buf, sizeof(buf)) == 0);
    return 0;
}

constexpr int cache_refuse_une_place_insuffisante() {
    std::uint8_t buf[TAILLE_CACHE_ZONE_MAX] = {};
    const std::size_t n = serialiser_cache(PARIS_R, 4574906, 484789, buf, sizeof(buf));
    VERIFIER(n != 0);
    VERIFIER(serialiser_cache(PARIS_R, 4574906, 484789, buf, n) == n);     // juste assez
    VERIFIER(serialiser_cache(PARIS_R, 4574906, 484789, buf, n - 1) == 0); // un octet de moins
    VERIFIER(serialiser_cache(PARIS_R, 4574906, 484789, buf, 10) == 0);
    VERIFIER(serialiser_cache(PARIS_R, 4574906, 484789, nullptr, 500) == 0);
    Releve faux = PARIS_R;
    faux.temperature_dixiemes = 9999; // un relevé invraisemblable ne va pas en NVS
    VERIFIER(serialiser_cache(faux, 4574906, 484789, buf, sizeof(buf)) == 0);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(requete_de_lyon_exacte)                                                                                          \
    X(requete_coordonnees_negatives_et_chiffres_de_remplissage)                                                        \
    X(requete_aux_bornes_et_refus_hors_bornes)                                                                         \
    X(requete_refuse_une_place_insuffisante_sans_deborder)                                                             \
    X(requete_et_ses_en_tetes_depassent_le_tampon_d_envoi_par_defaut)                                                  \
    X(requete_sans_modele_est_celle_d_avant_a_l_octet_pres)                                                            \
    X(requete_avec_un_modele_ajoute_seulement_la_fin)                                                                  \
    X(requete_refuse_un_modele_douteux)                                                                                \
    X(requete_avec_un_modele_ne_deborde_pas)                                                                           \
    X(degres_vers_e5_arrondi_et_bornes)                                                                                \
    X(degres_non_finis_refuses)                                                                                        \
    X(nom_de_zone_valide_ou_non)                                                                                       \
    X(nom_de_zone_utf8_mal_forme_refuse)                                                                               \
    X(nom_de_zone_sans_zero_final_refuse)                                                                              \
    X(fuseau_de_zone_valide_ou_non)                                                                                    \
    X(synthese_de_paris)                                                                                               \
    X(libelle_selon_le_code_et_le_jour)                                                                                \
    X(peremption_apres_deux_heures)                                                                                    \
    X(peremption_apres_des_echecs_repetes)                                                                             \
    X(peremption_quand_l_horloge_est_incertaine)                                                                       \
    X(min_max_du_jour_et_replis)                                                                                       \
    X(dixiemes_en_texte_francais)                                                                                      \
    X(planning_normal_et_reessais)                                                                                     \
    X(planning_jamais_a_l_envers_ni_plus_court_que_le_pas)                                                             \
    X(planning_apres_un_trop_de_requetes)                                                                              \
    X(cache_aller_retour_avec_la_position)                                                                             \
    X(cache_refuse_toute_alteration)                                                                                   \
    X(cache_refuse_un_autre_format_et_une_position_invraisemblable)                                                    \
    X(cache_refuse_une_place_insuffisante)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
