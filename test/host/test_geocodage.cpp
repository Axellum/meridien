// Tests du noyau pur : recherche de lieu (geocodage.hpp) — lecture de la réponse d'Open-Meteo, JSON de la page, ville choisie,
// état d'une recherche demandée par l'écran.
//
// Les réponses lues sont de VRAIES captures du service (donnees/geocodage_*.json, voir donnees/LISEZMOI_geocodage.md). Ce qu'on
// doit y lire est calculé par Python (json.loads), pas par le noyau : donnees/attendu_geocodage.inc, généré par
// capturer_geocodage.py. Les cas limites (lieux à écarter, bornes de l'altitude, 8 lieux au plus) sont écrits à la main, sur du
// JSON fabriqué.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/api_json.hpp"
#include "ecran/api_reglages.hpp"
#include "ecran/geocodage.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/reglages_valeurs.hpp"
#include "mini.hpp"

namespace geo = ecran::geocodage;
namespace api = ecran::api;
namespace json = ecran::json;
namespace rg = ecran::reglages;
using sv = std::string_view;

namespace {

struct AttenduLieu {
    bool id_connu;
    long long id;
    sv nom, region, pays, code_pays, fuseau;
    double latitude, longitude;
    bool altitude_connue;
    double altitude;
    bool population_connue;
    long long population;
    sv libelle;
};

constexpr sv MARSEILLE =
#include "donnees/geocodage_marseille.inc"
    ;
constexpr sv SAINT_ETIENNE =
#include "donnees/geocodage_saint_etienne.inc"
    ;
constexpr sv SPRINGFIELD =
#include "donnees/geocodage_springfield.inc"
    ;
constexpr sv ZURICH =
#include "donnees/geocodage_zurich.inc"
    ;
constexpr sv ISTANBUL =
#include "donnees/geocodage_istanbul.inc"
    ;
constexpr sv PARIS =
#include "donnees/geocodage_paris.inc"
    ;
constexpr sv AUCUN_RESULTAT =
#include "donnees/geocodage_aucun_resultat.inc"
    ;

#include "donnees/attendu_geocodage.inc"

struct Capture {
    sv json;
    const AttenduLieu *attendu;
    std::size_t n;
};

constexpr Capture CAPTURES[] = {
    {MARSEILLE, ATTENDU_MARSEILLE, N_MARSEILLE},
    {SAINT_ETIENNE, ATTENDU_SAINT_ETIENNE, N_SAINT_ETIENNE},
    {SPRINGFIELD, ATTENDU_SPRINGFIELD, N_SPRINGFIELD},
    {ZURICH, ATTENDU_ZURICH, N_ZURICH},
    {ISTANBUL, ATTENDU_ISTANBUL, N_ISTANBUL},
    {PARIS, ATTENDU_PARIS, N_PARIS},
    {AUCUN_RESULTAT, ATTENDU_AUCUN_RESULTAT, N_AUCUN_RESULTAT},
};

constexpr bool proche(double a, double b, double tolerance = 1e-9) { return (a > b ? a - b : b - a) <= tolerance; }

constexpr bool lieu_conforme(const geo::Lieu &l, const AttenduLieu &a) {
    return l.id_connu == a.id_connu && l.id == a.id && sv(l.nom) == a.nom && sv(l.region) == a.region &&
           sv(l.pays) == a.pays && sv(l.code_pays) == a.code_pays && sv(l.fuseau) == a.fuseau &&
           proche(l.latitude, a.latitude) && proche(l.longitude, a.longitude) &&
           l.altitude_connue == a.altitude_connue && (!a.altitude_connue || proche(l.altitude, a.altitude)) &&
           l.population_connue == a.population_connue && (!a.population_connue || l.population == a.population) &&
           sv(l.libelle) == a.libelle;
}

// ---- lecture des captures réelles ----------------------------------------------------------------------------------------------

constexpr int captures_lues_comme_python() {
    for (const Capture &c : CAPTURES) {
        geo::Lieu lieux[geo::LIEUX_MAX] = {};
        std::size_t n = 99;
        VERIFIER(geo::analyser(c.json, lieux, geo::LIEUX_MAX, n));
        VERIFIER(n == c.n);
        for (std::size_t i = 0; i < n; ++i)
            VERIFIER(lieu_conforme(lieux[i], c.attendu[i]));
    }
    return 0;
}

constexpr int quelques_valeurs_connues_en_clair() {
    // Les valeurs de captures_lues_comme_python viennent de Python ; celles-ci sont écrites à la main d'après la capture, pour
    // que le test ne dépende pas que du générateur.
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(MARSEILLE, l, geo::LIEUX_MAX, n) && n == 8);
    VERIFIER(sv(l[0].nom) == "Marseille" && sv(l[0].region) == "Région Provence-Alpes-Côte d'Azur" &&
             sv(l[0].pays) == "France");
    VERIFIER(sv(l[0].code_pays) == "FR" && sv(l[0].fuseau) == "Europe/Paris" && l[0].id == 2995469 &&
             l[0].population == 877215);
    VERIFIER(sv(l[0].libelle) == "Marseille, Région Provence-Alpes-Côte d'Azur, France");
    VERIFIER(sv(l[2].libelle) == "Marseilles, Illinois, États-Unis" && sv(l[2].fuseau) == "America/Chicago");
    VERIFIER(!l[3].population_connue); // Marseille (Haïti) : le service ne donne pas d'habitants

    VERIFIER(geo::analyser(SAINT_ETIENNE, l, geo::LIEUX_MAX, n) && n == 8);
    VERIFIER(sv(l[0].nom) == "Saint-Étienne" && sv(l[0].libelle) == "Saint-Étienne, Auvergne-Rhône-Alpes, France");

    VERIFIER(geo::analyser(ZURICH, l, geo::LIEUX_MAX, n) && n == 8);
    VERIFIER(sv(l[0].nom) == "Zürich" && sv(l[0].pays) == "Schweiz" && sv(l[0].fuseau) == "Europe/Zurich");

    VERIFIER(geo::analyser(ISTANBUL, l, geo::LIEUX_MAX, n) && n == 6); // seulement 6 lieux dans la réponse
    VERIFIER(sv(l[0].nom) == "İstanbul" && sv(l[0].libelle) == "İstanbul, İstanbul, Türkiye Cumhuriyeti" &&
             l[0].population == 15701602);

    VERIFIER(geo::analyser(PARIS, l, geo::LIEUX_MAX, n) && n == 8);
    VERIFIER(sv(l[0].libelle) == "Paris, Île-de-France, France" && sv(l[1].libelle) == "Paris, Texas, États-Unis");
    return 0;
}

constexpr int aucun_resultat_donne_une_liste_vide() {
    // Quand le service ne trouve rien, l'objet n'a pas de « results » du tout (capture réelle).
    VERIFIER(AUCUN_RESULTAT.find("results") == sv::npos);
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 99;
    VERIFIER(geo::analyser(AUCUN_RESULTAT, l, geo::LIEUX_MAX, n));
    VERIFIER(n == 0);
    char sortie[64] = {};
    api::Ecrivain e(sortie, sizeof sortie);
    VERIFIER(geo::convertir(AUCUN_RESULTAT, e));
    VERIFIER(e.vue() == R"({"resultats":[]})");
    return 0;
}

constexpr int altitude_inconnue_du_service_n_est_pas_transmise() {
    // Capture réelle : « Istanbul Old Town » arrive avec "elevation":-9999.0 (valeur « inconnue » de GeoNames).
    VERIFIER(ISTANBUL.find(R"("elevation":-9999.0)") != sv::npos);
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(ISTANBUL, l, geo::LIEUX_MAX, n) && n == 6);
    VERIFIER(sv(l[4].nom) == "Istanbul Old Town" && !l[4].altitude_connue);
    VERIFIER(l[0].altitude_connue && proche(l[0].altitude, 39.0) && l[3].altitude_connue &&
             proche(l[3].altitude, 125.0));
    char sortie[8192] = {};
    api::Ecrivain e(sortie, sizeof sortie);
    VERIFIER(geo::ecrire_resultats(l, n, e));
    const json::Resultat lu = json::lire(e.vue());
    VERIFIER(lu.ok());
    const json::Valeur liste = lu.valeur.champ("resultats");
    VERIFIER(liste.taille() == 6);
    VERIFIER(liste.element(0).champ("altitude").existe());
    VERIFIER(!liste.element(4).champ("altitude").existe()); // -9999 ne part pas vers la page
    return 0;
}

constexpr int altitude_bornes() {
    VERIFIER(geo::altitude_plausible(-500.0) && geo::altitude_plausible(0.0) && geo::altitude_plausible(8848.0) &&
             geo::altitude_plausible(9000.0));
    VERIFIER(!geo::altitude_plausible(-500.1) && !geo::altitude_plausible(9000.1) && !geo::altitude_plausible(-9999.0));
    return 0;
}

// ---- JSON de la page ------------------------------------------------------------------------------------------------------------

constexpr int json_de_la_page_relu_champ_par_champ() {
    for (const Capture &c : CAPTURES) {
        geo::Lieu lieux[geo::LIEUX_MAX] = {};
        std::size_t n = 0;
        VERIFIER(geo::analyser(c.json, lieux, geo::LIEUX_MAX, n));
        char sortie[8192] = {};
        api::Ecrivain e(sortie, sizeof sortie);
        VERIFIER(geo::ecrire_resultats(lieux, n, e));
        const json::Resultat lu = json::lire(e.vue());
        VERIFIER(lu.ok() && lu.valeur.genre() == json::Genre::Objet);
        const json::Valeur liste = lu.valeur.champ("resultats");
        VERIFIER(liste.genre() == json::Genre::Tableau && liste.taille() == n);
        for (std::size_t i = 0; i < n; ++i) {
            const json::Valeur v = liste.element(i);
            const geo::Lieu &l = lieux[i];
            char texte_id[24] = {};
            double x = 0;
            std::int64_t entier = 0;
            VERIFIER(v.champ("id").texte_egal(l.id_connu ? api::entier_en_texte(l.id, texte_id) : sv{}));
            VERIFIER(v.champ("nom").texte_egal(l.nom) && v.champ("region").texte_egal(l.region) &&
                     v.champ("pays").texte_egal(l.pays) && v.champ("code_pays").texte_egal(l.code_pays) &&
                     v.champ("fuseau").texte_egal(l.fuseau) && v.champ("libelle").texte_egal(l.libelle));
            VERIFIER(v.champ("latitude").en_double(x) && proche(x, l.latitude, 1e-5));
            VERIFIER(v.champ("longitude").en_double(x) && proche(x, l.longitude, 1e-5));
            VERIFIER(v.champ("altitude").existe() == l.altitude_connue);
            if (l.altitude_connue)
                VERIFIER(v.champ("altitude").en_double(x) && proche(x, l.altitude, 0.5));
            VERIFIER(v.champ("population").existe() == l.population_connue);
            if (l.population_connue)
                VERIFIER(v.champ("population").en_entier(entier) && entier == l.population);
        }
    }
    return 0;
}

constexpr int convertir_ecrit_ce_que_ecrit_resultats() {
    for (const Capture &c : CAPTURES) {
        geo::Lieu lieux[geo::LIEUX_MAX] = {};
        std::size_t n = 0;
        VERIFIER(geo::analyser(c.json, lieux, geo::LIEUX_MAX, n));
        char a[8192] = {};
        char b[8192] = {};
        api::Ecrivain ea(a, sizeof a);
        api::Ecrivain eb(b, sizeof b);
        VERIFIER(geo::ecrire_resultats(lieux, n, ea) && geo::convertir(c.json, eb));
        VERIFIER(ea.vue() == eb.vue());
        // et la page (api::convertir_geocodage) passe par le même chemin
        char p[8192] = {};
        api::Ecrivain ep(p, sizeof p);
        VERIFIER(api::convertir_geocodage(c.json, ep) && ep.vue() == ea.vue());
    }
    return 0;
}

constexpr int reponse_trop_grande_pour_le_tampon_est_refusee() {
    char petit[64] = {};
    api::Ecrivain e(petit, sizeof petit);
    VERIFIER(!geo::convertir(PARIS, e)); // jamais une réponse tronquée envoyée comme valide
    return 0;
}

// ---- réponses illisibles et lieux à écarter (JSON fabriqué) ---------------------------------------------------------------------

constexpr int reponse_illisible_refusee() {
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 99;
    VERIFIER(!geo::analyser("", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(!geo::analyser("<html>502 Bad Gateway</html>", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(!geo::analyser("[1,2,3]", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(!geo::analyser("42", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(!geo::analyser(R"({"results":[{"name":"Lyo)", l, geo::LIEUX_MAX, n) && n == 0); // texte coupé
    // un objet sans « results », ou avec autre chose qu'un tableau, est une réponse sans lieu (pas une erreur)
    VERIFIER(geo::analyser("{}", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(geo::analyser(R"({"results":"non"})", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(geo::analyser(R"({"results":[]})", l, geo::LIEUX_MAX, n) && n == 0);
    VERIFIER(geo::analyser(R"({"results":[1,"a",null,[]]})", l, geo::LIEUX_MAX, n) && n == 0);
    return 0;
}

constexpr int lieux_invalides_ecartes() {
    // Le premier lieu est bon ; chacun des suivants a UN défaut ; le dernier est bon aussi.
    constexpr sv TEXTE = R"({"results":[
      {"name":"Bon","latitude":45.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"latitude":45.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"","latitude":45.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"SansFuseau","latitude":45.0,"longitude":4.0},
      {"name":"FuseauVide","latitude":45.0,"longitude":4.0,"timezone":""},
      {"name":"SansLatitude","longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"SansLongitude","latitude":4.0,"timezone":"Europe/Paris"},
      {"name":"LatitudeTexte","latitude":"45","longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"Lat91","latitude":91.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"Lat-91","latitude":-91.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"Lon181","latitude":45.0,"longitude":181.0,"timezone":"Europe/Paris"},
      {"name":"Lon-181","latitude":45.0,"longitude":-181.0,"timezone":"Europe/Paris"},
      {"name":"Cloche\u0007","latitude":45.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"Sept\u007f","latitude":45.0,"longitude":4.0,"timezone":"Europe/Paris"},
      {"name":"Pole","latitude":90.0,"longitude":-180.0,"timezone":"Etc/GMT"}
    ]})";
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(TEXTE, l, geo::LIEUX_MAX, n));
    VERIFIER(n == 2);
    VERIFIER(sv(l[0].nom) == "Bon" && sv(l[1].nom) == "Pole");
    VERIFIER(proche(l[1].latitude, 90.0) && proche(l[1].longitude, -180.0)); // les bornes sont acceptées
    return 0;
}

constexpr int region_et_pays_invalides_laisses_vides() {
    constexpr sv TEXTE = R"({"results":[
      {"name":"A","admin1":"Reg\u0007","country":"Pays","latitude":1.0,"longitude":2.0,"timezone":"Europe/Paris"},
      {"name":"B","admin1":"Region","country":"Pa\u0001ys","latitude":1.0,"longitude":2.0,"timezone":"Europe/Paris"},
      {"name":"C","admin1":null,"country":12,"latitude":1.0,"longitude":2.0,"timezone":"Europe/Paris"}
    ]})";
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(TEXTE, l, geo::LIEUX_MAX, n) && n == 3);
    VERIFIER(sv(l[0].region).empty() && sv(l[0].libelle) == "A, Pays");
    VERIFIER(sv(l[1].pays).empty() && sv(l[1].libelle) == "B, Region");
    VERIFIER(sv(l[2].region).empty() && sv(l[2].pays).empty() && sv(l[2].libelle) == "C");
    return 0;
}

constexpr int au_plus_huit_lieux() {
    char texte[4096] = {};
    api::Ecrivain e(texte, sizeof texte);
    e.ouvrir_objet();
    e.cle("results");
    e.ouvrir_tableau();
    for (int i = 0; i < 12; ++i) {
        e.ouvrir_objet();
        e.champ("name", i < 10 ? "Ville0" : "Ville1");
        e.champ_decimal("latitude", 10.0 + i, 1);
        e.champ_decimal("longitude", 20.0, 1);
        e.champ("timezone", "Europe/Paris");
        e.fermer_objet();
    }
    e.fermer_tableau();
    e.fermer_objet();
    VERIFIER(e.ok());
    geo::Lieu l[geo::LIEUX_MAX + 4] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(e.vue(), l, geo::LIEUX_MAX + 4, n) && n == 8); // 12 reçus, 8 gardés
    VERIFIER(proche(l[7].latitude, 17.0));
    VERIFIER(geo::analyser(e.vue(), l, 3, n) && n == 3); // un tampon plus petit borne aussi
    VERIFIER(geo::analyser(e.vue(), l, 0, n) && n == 0);
    char page[8192] = {};
    api::Ecrivain ep(page, sizeof page);
    VERIFIER(geo::convertir(e.vue(), ep));
    const json::Resultat lu = json::lire(ep.vue());
    VERIFIER(lu.ok() && lu.valeur.champ("resultats").taille() == 8);
    return 0;
}

constexpr int champs_facultatifs_absents() {
    constexpr sv TEXTE = R"({"results":[
      {"name":"Sans id","latitude":1.0,"longitude":2.0,"timezone":"Europe/Paris"},
      {"id":5,"name":"Population negative","population":-5,"latitude":1.0,"longitude":2.0,"timezone":"Europe/Paris"},
      {"id":"x","name":"Id texte","population":12.5,"latitude":1.0,"longitude":2.0,"timezone":"Europe/Paris"}
    ]})";
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(TEXTE, l, geo::LIEUX_MAX, n) && n == 3);
    VERIFIER(!l[0].id_connu && !l[0].altitude_connue && !l[0].population_connue && sv(l[0].code_pays).empty());
    VERIFIER(l[1].id_connu && l[1].id == 5 && !l[1].population_connue);
    VERIFIER(!l[2].id_connu && !l[2].population_connue);
    char page[2048] = {};
    api::Ecrivain e(page, sizeof page);
    VERIFIER(geo::ecrire_resultats(l, n, e));
    const json::Resultat lu = json::lire(e.vue());
    VERIFIER(lu.ok());
    const json::Valeur liste = lu.valeur.champ("resultats");
    VERIFIER(liste.element(0).champ("id").texte_egal("")); // l'identifiant absent est écrit vide, comme avant ce jalon
    VERIFIER(liste.element(1).champ("id").texte_egal("5"));
    VERIFIER(!liste.element(0).champ("altitude").existe() && !liste.element(0).champ("population").existe());
    return 0;
}

constexpr int nom_trop_long_coupe_sur_un_caractere() {
    char texte[1024] = {};
    api::Ecrivain e(texte, sizeof texte);
    e.ouvrir_objet();
    e.cle("results");
    e.ouvrir_tableau();
    e.ouvrir_objet();
    char nom[301] = {};
    for (std::size_t i = 0; i + 1 < sizeof nom; i += 2) { // 150 fois « é » (2 octets)
        nom[i] = static_cast<char>(0xC3);
        nom[i + 1] = static_cast<char>(0xA9);
    }
    e.champ("name", nom);
    e.champ_decimal("latitude", 1.0, 1);
    e.champ_decimal("longitude", 2.0, 1);
    e.champ("timezone", "Europe/Paris");
    e.fermer_objet();
    e.fermer_tableau();
    e.fermer_objet();
    VERIFIER(e.ok());
    geo::Lieu l[geo::LIEUX_MAX] = {};
    std::size_t n = 0;
    VERIFIER(geo::analyser(e.vue(), l, geo::LIEUX_MAX, n) && n == 1);
    VERIFIER(sv(l[0].nom).size() == geo::NOM_OCTETS - 1); // 80 octets = 40 « é » entiers, jamais un demi-caractère
    VERIFIER(rg::utf8_valide(l[0].nom) && rg::nombre_de_caracteres(l[0].nom) == 40);
    VERIFIER(sv(l[0].libelle) == sv(l[0].nom));
    return 0;
}

// ---- le lieu choisi devient la ville --------------------------------------------------------------------------------------------

constexpr int vers_zone_identique_a_la_validation_de_la_page() {
    // Pour chaque lieu réel : ce que `vers_zone` accepte, `valider_zone` l'accepte (sur le JSON que la page renvoie), avec la même
    // ville ; et inversement.
    for (const Capture &c : CAPTURES) {
        geo::Lieu lieux[geo::LIEUX_MAX] = {};
        std::size_t n = 0;
        VERIFIER(geo::analyser(c.json, lieux, geo::LIEUX_MAX, n));
        char sortie[8192] = {};
        api::Ecrivain e(sortie, sizeof sortie);
        VERIFIER(geo::ecrire_resultats(lieux, n, e));
        const json::Resultat lu = json::lire(e.vue());
        VERIFIER(lu.ok());
        const json::Valeur liste = lu.valeur.champ("resultats");
        for (std::size_t i = 0; i < n; ++i) {
            const rg::ResultatZone ecran = geo::vers_zone(lieux[i]);
            const rg::ResultatZone page = rg::valider_zone(liste.element(i));
            VERIFIER(ecran.erreur ==
                     rg::ErreurZone::Aucune); // tous les fuseaux des captures sont dans la table de l'appareil
            VERIFIER(page.erreur == ecran.erreur);
            VERIFIER(sv(ecran.zone.nom) == sv(page.zone.nom) && sv(ecran.zone.region) == sv(page.zone.region) &&
                     sv(ecran.zone.pays) == sv(page.zone.pays) && sv(ecran.zone.code_pays) == sv(page.zone.code_pays) &&
                     sv(ecran.zone.fuseau) == sv(page.zone.fuseau));
            VERIFIER(proche(ecran.zone.latitude, page.zone.latitude, 1e-5) &&
                     proche(ecran.zone.longitude, page.zone.longitude, 1e-5));
        }
    }
    return 0;
}

constexpr geo::Lieu lieu_valide() {
    geo::Lieu l{};
    rg::copier_texte("Lyon", l.nom, sizeof l.nom);
    rg::copier_texte("Auvergne-Rhône-Alpes", l.region, sizeof l.region);
    rg::copier_texte("France", l.pays, sizeof l.pays);
    rg::copier_texte("FR", l.code_pays, sizeof l.code_pays);
    rg::copier_texte("Europe/Paris", l.fuseau, sizeof l.fuseau);
    l.latitude = 45.74846;
    l.longitude = 4.84671;
    return l;
}

constexpr int vers_zone_remplit_la_ville() {
    const rg::ResultatZone r = geo::vers_zone(lieu_valide());
    VERIFIER(r.erreur == rg::ErreurZone::Aucune);
    VERIFIER(sv(r.zone.nom) == "Lyon" && sv(r.zone.region) == "Auvergne-Rhône-Alpes" && sv(r.zone.pays) == "France");
    VERIFIER(sv(r.zone.code_pays) == "FR" && sv(r.zone.fuseau) == "Europe/Paris");
    VERIFIER(proche(r.zone.latitude, 45.74846) && proche(r.zone.longitude, 4.84671));
    return 0;
}

constexpr int vers_zone_refuse_dans_l_ordre_nom_coordonnees_fuseau() {
    geo::Lieu l = lieu_valide();
    l.nom[0] = '\0';
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Nom);
    l = lieu_valide();
    rg::copier_texte("   ", l.nom, sizeof l.nom); // que des blancs
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Nom);
    l = lieu_valide();
    l.nom[1] = '\x07'; // octet de contrôle
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Nom);
    l = lieu_valide();
    l.nom[0] = static_cast<char>(0xFF); // UTF-8 invalide
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Nom);
    l = lieu_valide();
    l.latitude = 90.5;
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Coordonnees);
    l = lieu_valide();
    l.longitude = -180.5;
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Coordonnees);
    l = lieu_valide();
    rg::copier_texte("Mars/Olympus", l.fuseau, sizeof l.fuseau); // pas dans la table embarquée
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Fuseau);
    // plusieurs défauts : le premier de l'ordre nom, coordonnées, fuseau l'emporte (comme valider_zone)
    l = lieu_valide();
    l.nom[0] = '\0';
    l.latitude = 99.0;
    rg::copier_texte("Mars/Olympus", l.fuseau, sizeof l.fuseau);
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Nom);
    l = lieu_valide();
    l.latitude = 99.0;
    rg::copier_texte("Mars/Olympus", l.fuseau, sizeof l.fuseau);
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Coordonnees);
    return 0;
}

constexpr int vers_zone_nom_long_et_champs_facultatifs() {
    geo::Lieu l = lieu_valide();
    // 40 fois « é » = 80 octets, 40 caractères : accepté, gardé jusqu'à 47 octets (taille du nom de la zone) sans couper un caractère
    for (std::size_t i = 0; i < 40; ++i) {
        l.nom[2 * i] = static_cast<char>(0xC3);
        l.nom[2 * i + 1] = static_cast<char>(0xA9);
    }
    l.nom[80] = '\0';
    rg::ResultatZone r = geo::vers_zone(l);
    VERIFIER(r.erreur == rg::ErreurZone::Aucune);
    VERIFIER(sv(r.zone.nom).size() == 46 &&
             rg::utf8_valide(r.zone.nom)); // 23 « é » : 47 octets couperaient le 24e en deux
    // 80 caractères d'un octet : la limite du nom est incluse
    l = lieu_valide();
    for (std::size_t i = 0; i < 80; ++i)
        l.nom[i] = 'a';
    l.nom[80] = '\0';
    VERIFIER(geo::vers_zone(l).erreur == rg::ErreurZone::Aucune && sv(geo::vers_zone(l).zone.nom).size() == 47);
    // région invalide -> vide ; code pays avec un chiffre -> vide ; pays inchangé
    l = lieu_valide();
    l.region[2] = '\x01';
    rg::copier_texte("F1", l.code_pays, 4);
    r = geo::vers_zone(l);
    VERIFIER(r.erreur == rg::ErreurZone::Aucune && sv(r.zone.region).empty() && sv(r.zone.code_pays).empty());
    VERIFIER(sv(r.zone.pays) == "France");
    l = lieu_valide();
    rg::copier_texte("F1", l.code_pays, 4);
    VERIFIER(sv(geo::vers_zone(l).zone.code_pays).empty());
    rg::copier_texte("fra", l.code_pays, 4); // trois lettres (minuscules comprises) : gardé tel quel
    VERIFIER(sv(geo::vers_zone(l).zone.code_pays) == "fra");
    return 0;
}

// ---- l'état d'une recherche demandée par l'écran --------------------------------------------------------------------------------

constexpr geo::Lieu lieu_nomme(sv nom) {
    geo::Lieu l = lieu_valide();
    l.nom[0] = '\0';
    rg::copier_texte(nom, l.nom, sizeof l.nom);
    return l;
}

constexpr int recherche_cycle_normal() {
    geo::Recherche r;
    VERIFIER(r.etat() == geo::Etat::Inactive && r.nombre() == 0);
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(!r.prendre(texte, numero)); // rien à faire
    VERIFIER(r.demander("  Lyon ") == rg::ErreurRecherche::Aucune);
    VERIFIER(r.etat() == geo::Etat::EnCours);
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.resultats(sortie, geo::LIEUX_MAX) == 0); // pas de résultat tant qu'elle n'est pas terminée
    VERIFIER(r.prendre(texte, numero));
    VERIFIER(sv(texte) == "Lyon");       // rognée
    VERIFIER(!r.prendre(texte, numero)); // déjà prise
    VERIFIER(r.etat() == geo::Etat::EnCours);
    const geo::Lieu trouves[2] = {lieu_nomme("Lyon"), lieu_nomme("Lyon-bis")};
    VERIFIER(r.terminer(numero, geo::Issue::Trouvee, trouves, 2));
    VERIFIER(r.etat() == geo::Etat::Terminee && r.nombre() == 2);
    VERIFIER(r.resultats(sortie, geo::LIEUX_MAX) == 2 && sv(sortie[0].nom) == "Lyon" &&
             sv(sortie[1].nom) == "Lyon-bis");
    VERIFIER(r.resultats(sortie, 1) == 1); // le tampon de l'appelant borne la copie
    VERIFIER(r.resultats(sortie, 0) == 0);
    return 0;
}

constexpr int recherche_sans_resultat_est_terminee() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.demander("xqzvbnmwkj") == rg::ErreurRecherche::Aucune && r.prendre(texte, numero));
    VERIFIER(r.terminer(numero, geo::Issue::Trouvee, nullptr, 0));
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.etat() == geo::Etat::Terminee &&
             r.resultats(sortie, geo::LIEUX_MAX) == 0); // « aucun résultat », pas « sans connexion »
    return 0;
}

constexpr int recherche_sans_connexion() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune && r.prendre(texte, numero));
    const geo::Lieu inutiles[1] = {lieu_nomme("Ne doit pas sortir")};
    VERIFIER(r.terminer(numero, geo::Issue::SansConnexion, inutiles, 1));
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.etat() == geo::Etat::SansConnexion && r.nombre() == 0 && r.resultats(sortie, geo::LIEUX_MAX) == 0);
    // et on peut recommencer
    VERIFIER(r.demander("Paris") == rg::ErreurRecherche::Aucune && r.etat() == geo::Etat::EnCours);
    return 0;
}

constexpr int la_derniere_demande_gagne() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t premier = 0;
    std::uint32_t second = 0;
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune &&
             r.prendre(texte, premier));                          // « Lyon » part sur le réseau
    VERIFIER(r.demander("Paris") == rg::ErreurRecherche::Aucune); // pendant ce temps : « Paris »
    const geo::Lieu lyon[1] = {lieu_nomme("Lyon")};
    VERIFIER(!r.terminer(premier, geo::Issue::Trouvee, lyon, 1)); // la réponse de « Lyon » est jetée
    VERIFIER(r.etat() == geo::Etat::EnCours && r.resultats(sortie, geo::LIEUX_MAX) == 0);
    VERIFIER(r.prendre(texte, second) && sv(texte) == "Paris" && second != premier);
    VERIFIER(!r.terminer(premier, geo::Issue::SansConnexion, nullptr, 0)); // même un échec de l'ancienne est jeté
    VERIFIER(r.etat() == geo::Etat::EnCours);
    const geo::Lieu paris[1] = {lieu_nomme("Paris")};
    VERIFIER(r.terminer(second, geo::Issue::Trouvee, paris, 1));
    VERIFIER(r.resultats(sortie, geo::LIEUX_MAX) == 1 && sv(sortie[0].nom) == "Paris");
    return 0;
}

constexpr int deux_demandes_avant_prise_ne_font_qu_une() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune);
    VERIFIER(r.demander("Nantes") == rg::ErreurRecherche::Aucune);
    VERIFIER(r.prendre(texte, numero) && sv(texte) == "Nantes");
    VERIFIER(!r.prendre(texte, numero));
    return 0;
}

constexpr int une_demande_ne_se_termine_qu_une_fois() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune && r.prendre(texte, numero));
    const geo::Lieu lyon[2] = {lieu_nomme("Lyon"), lieu_nomme("Lyon-bis")};
    VERIFIER(r.terminer(numero, geo::Issue::Trouvee, lyon, 2));
    VERIFIER(!r.terminer(numero, geo::Issue::SansConnexion, nullptr, 0)); // la seconde fin est ignorée
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.etat() == geo::Etat::Terminee && r.resultats(sortie, geo::LIEUX_MAX) == 2);
    VERIFIER(!r.terminer(numero, geo::Issue::Trouvee, lyon, 1));
    VERIFIER(r.nombre() == 2);
    return 0;
}

constexpr int nouvelle_demande_efface_les_resultats() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune && r.prendre(texte, numero));
    const geo::Lieu lyon[1] = {lieu_nomme("Lyon")};
    VERIFIER(r.terminer(numero, geo::Issue::Trouvee, lyon, 1) && r.nombre() == 1);
    VERIFIER(r.demander("Paris") == rg::ErreurRecherche::Aucune);
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.nombre() == 0 && r.resultats(sortie, geo::LIEUX_MAX) == 0 && r.etat() == geo::Etat::EnCours);
    return 0;
}

constexpr int texte_refuse_abandonne_la_recherche() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    geo::Lieu sortie[geo::LIEUX_MAX] = {};
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune && r.prendre(texte, numero));
    VERIFIER(r.demander("ab") == rg::ErreurRecherche::TropCourte);
    VERIFIER(r.etat() == geo::Etat::Terminee && r.nombre() == 0 && r.resultats(sortie, geo::LIEUX_MAX) == 0);
    VERIFIER(!r.prendre(texte, numero));
    const geo::Lieu lyon[1] = {lieu_nomme("Lyon")};
    VERIFIER(!r.terminer(numero, geo::Issue::Trouvee, lyon, 1)); // la réponse de la recherche abandonnée est jetée
    VERIFIER(r.etat() == geo::Etat::Terminee && r.nombre() == 0);
    // une demande en attente (pas encore prise) est elle aussi abandonnée
    VERIFIER(r.demander("Paris") == rg::ErreurRecherche::Aucune);
    VERIFIER(r.demander("  ") == rg::ErreurRecherche::TropCourte);
    VERIFIER(!r.prendre(texte, numero));
    return 0;
}

constexpr int validation_du_texte_demande() {
    geo::Recherche r;
    VERIFIER(r.demander("") == rg::ErreurRecherche::TropCourte);
    VERIFIER(r.demander("Li") == rg::ErreurRecherche::TropCourte);
    VERIFIER(r.demander("Lyo") == rg::ErreurRecherche::Aucune);
    char quatre_vingts[81] = {};
    for (std::size_t i = 0; i < 80; ++i)
        quatre_vingts[i] = 'a';
    VERIFIER(r.demander(quatre_vingts) == rg::ErreurRecherche::Aucune);
    char quatre_vingt_un[82] = {};
    for (std::size_t i = 0; i < 81; ++i)
        quatre_vingt_un[i] = 'a';
    VERIFIER(r.demander(quatre_vingt_un) == rg::ErreurRecherche::TropLongue);
    VERIFIER(r.demander("Ly\x01on") == rg::ErreurRecherche::Illisible);
    VERIFIER(r.demander("Ly\xFF"
                        "on") == rg::ErreurRecherche::Illisible);
    // 80 caractères accentués = 160 octets : accepté, et la demande prise est complète
    char accents[161] = {};
    for (std::size_t i = 0; i < 80; ++i) {
        accents[2 * i] = static_cast<char>(0xC3);
        accents[2 * i + 1] = static_cast<char>(0xA9);
    }
    VERIFIER(r.demander(accents) == rg::ErreurRecherche::Aucune);
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.prendre(texte, numero) && sv(texte).size() == 160);
    return 0;
}

constexpr int plus_de_huit_resultats_bornes() {
    geo::Recherche r;
    char texte[rg::RECHERCHE_OCTETS_MAX + 1] = {};
    std::uint32_t numero = 0;
    VERIFIER(r.demander("Lyon") == rg::ErreurRecherche::Aucune && r.prendre(texte, numero));
    geo::Lieu douze[12] = {};
    for (std::size_t i = 0; i < 12; ++i)
        douze[i] = lieu_nomme("Lyon");
    VERIFIER(r.terminer(numero, geo::Issue::Trouvee, douze, 12) && r.nombre() == geo::LIEUX_MAX);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(captures_lues_comme_python)                                                                                      \
    X(quelques_valeurs_connues_en_clair)                                                                               \
    X(aucun_resultat_donne_une_liste_vide)                                                                             \
    X(altitude_inconnue_du_service_n_est_pas_transmise)                                                                \
    X(altitude_bornes)                                                                                                 \
    X(json_de_la_page_relu_champ_par_champ)                                                                            \
    X(convertir_ecrit_ce_que_ecrit_resultats)                                                                          \
    X(reponse_trop_grande_pour_le_tampon_est_refusee)                                                                  \
    X(reponse_illisible_refusee)                                                                                       \
    X(lieux_invalides_ecartes)                                                                                         \
    X(region_et_pays_invalides_laisses_vides)                                                                          \
    X(au_plus_huit_lieux)                                                                                              \
    X(champs_facultatifs_absents)                                                                                      \
    X(nom_trop_long_coupe_sur_un_caractere)                                                                            \
    X(vers_zone_identique_a_la_validation_de_la_page)                                                                  \
    X(vers_zone_remplit_la_ville)                                                                                      \
    X(vers_zone_refuse_dans_l_ordre_nom_coordonnees_fuseau)                                                            \
    X(vers_zone_nom_long_et_champs_facultatifs)                                                                        \
    X(recherche_cycle_normal)                                                                                          \
    X(recherche_sans_resultat_est_terminee)                                                                            \
    X(recherche_sans_connexion)                                                                                        \
    X(la_derniere_demande_gagne)                                                                                       \
    X(deux_demandes_avant_prise_ne_font_qu_une)                                                                        \
    X(une_demande_ne_se_termine_qu_une_fois)                                                                           \
    X(nouvelle_demande_efface_les_resultats)                                                                           \
    X(texte_refuse_abandonne_la_recherche)                                                                             \
    X(validation_du_texte_demande)                                                                                     \
    X(plus_de_huit_resultats_bornes)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
