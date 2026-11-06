// Tests du noyau pur : écriture JSON (api_json.hpp) et validation des valeurs de la page de réglages (reglages_valeurs.hpp).
#include <limits>
#include <string_view>

#include "ecran/api_json.hpp"
#include "ecran/reglages_valeurs.hpp"
#include "mini.hpp"

namespace api = ecran::api;
namespace rg = ecran::reglages;
namespace json = ecran::json;
namespace i18n = ecran::i18n;
using sv = std::string_view;

namespace {

// ---- api_json.hpp ----------------------------------------------------------------------------------------------------

constexpr int objets_tableaux_et_virgules() {
    char b[128] = {};
    api::Ecrivain e(b, sizeof b);
    e.ouvrir_objet();
    e.champ_entier("a", 1);
    e.cle("b");
    e.ouvrir_tableau();
    e.booleen(true);
    e.nul();
    e.chaine("x");
    e.fermer_tableau();
    e.cle("c");
    e.ouvrir_objet();
    e.fermer_objet();
    e.champ_booleen("d", false);
    e.fermer_objet();
    VERIFIER(e.ok());
    VERIFIER(e.vue() == R"({"a":1,"b":[true,null,"x"],"c":{},"d":false})");
    return 0;
}

constexpr int tableau_d_objets() {
    char b[64] = {};
    api::Ecrivain e(b, sizeof b);
    e.ouvrir_tableau();
    for (int i = 0; i < 3; ++i) {
        e.ouvrir_objet();
        e.champ_entier("i", i);
        e.fermer_objet();
    }
    e.fermer_tableau();
    VERIFIER(e.vue() == R"([{"i":0},{"i":1},{"i":2}])");
    return 0;
}

constexpr int chaines_echappees_accents_conserves() {
    char b[128] = {};
    api::Ecrivain e(b, sizeof b);
    e.chaine("guillemet \" antislash \\ ligne\n tab\t retour\r");
    VERIFIER(e.vue() == "\"guillemet \\\" antislash \\\\ ligne\\n tab\\t retour\\r\"");
    char c[64] = {};
    api::Ecrivain f(c, sizeof c);
    f.chaine("\x01\x1f");
    VERIFIER(f.vue() == "\"\\u0001\\u001f\"");
    char d[64] = {};
    api::Ecrivain g(d, sizeof d);
    g.chaine("Café, 21 °C, Türkçe"); // UTF-8 tel quel (contrat §1 règle 3)
    VERIFIER(g.vue() == "\"Café, 21 °C, Türkçe\"");
    return 0;
}

constexpr int nombres_entiers() {
    char b[64] = {};
    api::Ecrivain e(b, sizeof b);
    e.ouvrir_tableau();
    e.entier(0);
    e.entier(-7);
    e.entier(1793960400);
    e.entier(-9223372036854775807LL - 1);
    e.fermer_tableau();
    VERIFIER(e.vue() == "[0,-7,1793960400,-9223372036854775808]");
    return 0;
}

constexpr int nombres_decimaux_sans_printf() {
    char b[160] = {};
    api::Ecrivain e(b, sizeof b);
    e.ouvrir_tableau();
    e.decimal(14.2, 1);
    e.decimal(45.74846, 5);
    e.decimal(4.0, 5);
    e.decimal(-0.04, 1); // arrondi à zéro : pas de « -0 »
    e.decimal(-3.25, 1); // moitié : loin de zéro
    e.decimal(4.84789, 5);
    e.decimal(0.5, 0); // 1 : sans décimale demandée
    e.decimal(100.0, 2);
    e.fermer_tableau();
    VERIFIER(e.vue() == "[14.2,45.74846,4,0,-3.3,4.84789,1,100]");
    return 0;
}

constexpr int nombre_invalide_ecrit_null() {
    char b[64] = {};
    api::Ecrivain e(b, sizeof b);
    e.ouvrir_tableau();
    e.decimal(2e9, 1);
    e.decimal(-2e9, 1);
    e.decimal(std::numeric_limits<double>::quiet_NaN(), 1);
    e.decimal(std::numeric_limits<double>::infinity(), 1);
    e.decimal(1.5, 12); // trop de décimales demandées
    e.fermer_tableau();
    VERIFIER(e.vue() == "[null,null,null,null,null]");
    return 0;
}

constexpr int debordement_sans_ecriture_hors_tampon() {
    char b[8] = {};
    api::Ecrivain e(b, 5); // capacité déclarée plus petite que le tableau : rien ne doit être écrit au-delà
    e.chaine("abcdefgh");
    VERIFIER(!e.ok());
    VERIFIER(e.taille() == 5);
    VERIFIER(b[5] == 0 && b[6] == 0 && b[7] == 0); // rien au-delà de la capacité déclarée
    return 0;
}

// ---- texte -----------------------------------------------------------------------------------------------------------

constexpr int utf8_bien_forme() {
    VERIFIER(rg::utf8_valide(""));
    VERIFIER(rg::utf8_valide("Lyon"));
    VERIFIER(rg::utf8_valide("Zürich"));
    VERIFIER(rg::utf8_valide("São Paulo"));
    VERIFIER(rg::utf8_valide("東京"));
    VERIFIER(rg::utf8_valide("\xF0\x9F\x8C\xA7")); // U+1F327, 4 octets
    return 0;
}

constexpr int utf8_mal_forme_refuse() {
    VERIFIER(!rg::utf8_valide("\x80"));             // continuation seule
    VERIFIER(!rg::utf8_valide("\xC0\x80"));         // forme trop longue
    VERIFIER(!rg::utf8_valide("\xC1\xBF"));         // forme trop longue
    VERIFIER(!rg::utf8_valide("\xE0\x80\x80"));     // forme trop longue
    VERIFIER(!rg::utf8_valide("\xE2\x82"));         // coupé au milieu
    VERIFIER(!rg::utf8_valide("\xC3"));             // coupé à la fin
    VERIFIER(!rg::utf8_valide("\xED\xA0\x80"));     // substitut U+D800
    VERIFIER(!rg::utf8_valide("\xF4\x90\x80\x80")); // au-delà de U+10FFFF
    VERIFIER(!rg::utf8_valide("\xF5\x80\x80\x80"));
    VERIFIER(!rg::utf8_valide("\xFF"));
    VERIFIER(!rg::utf8_valide(sv("a\0b", 3))); // octet 0
    // le texte continue après la vue : la vue s'arrête où elle s'arrête, rien ne doit être lu au-delà
    const char suite[4] = {'\xE2', '\x82', '\x82', 'x'};
    VERIFIER(rg::utf8_valide(sv(suite, 3)));  // E2 82 82 = U+2082
    VERIFIER(!rg::utf8_valide(sv(suite, 2))); // il manque le troisième octet
    VERIFIER(!rg::utf8_valide(sv(suite, 1)));
    const char deux[3] = {'\xC3', '\xA9', 'x'};
    VERIFIER(rg::utf8_valide(sv(deux, 2)));
    VERIFIER(!rg::utf8_valide(sv(deux, 1))); // C3 seul
    return 0;
}

constexpr int caracteres_et_controles() {
    VERIFIER(rg::nombre_de_caracteres("Zü") == 2);
    VERIFIER(rg::nombre_de_caracteres("東京") == 2);
    VERIFIER(rg::nombre_de_caracteres("") == 0);
    VERIFIER(rg::sans_controle("Lyon, France"));
    VERIFIER(!rg::sans_controle("a\tb"));
    VERIFIER(!rg::sans_controle("a\nb"));
    VERIFIER(!rg::sans_controle("a\x7f"));
    return 0;
}

constexpr int rognage_et_coupure_sur_frontiere() {
    VERIFIER(rg::rogner("  Lyon \t\r\n") == "Lyon");
    VERIFIER(rg::rogner("   ") == "");
    VERIFIER(rg::rogner("a b") == "a b");
    VERIFIER(rg::couper_utf8("abcdef", 3) == "abc");
    VERIFIER(rg::couper_utf8("abc", 10) == "abc");
    VERIFIER(rg::couper_utf8("éé", 3) == "é"); // 3 octets : le second « é » ne tient pas, on ne le coupe pas en deux
    VERIFIER(rg::couper_utf8("éé", 4) == "éé");
    VERIFIER(rg::couper_utf8("é", 1) == "");
    char b[5] = {};
    VERIFIER(rg::copier_texte("Zürich", b, sizeof b) == 4); // « Zür » = 4 octets
    VERIFIER(sv(b) == "Zür");
    char c[2] = {};
    VERIFIER(rg::copier_texte("ab", c, sizeof c) == 1 && sv(c) == "a");
    return 0;
}

// ---- adresse web -----------------------------------------------------------------------------------------------------

constexpr int parametre_simple_et_decode() {
    char q[64] = {};
    rg::Parametre p = rg::parametre_decode("q=Lyon", "q", q, sizeof q);
    VERIFIER(p.trouve && !p.tronque && !p.invalide && p.taille == 4 && sv(q) == "Lyon");
    p = rg::parametre_decode("a=1&q=Saint-%C3%89tienne&b=2", "q", q, sizeof q);
    VERIFIER(p.trouve && sv(q) == "Saint-Étienne");
    p = rg::parametre_decode("q=New+York", "q", q, sizeof q);
    VERIFIER(sv(q) == "New York");
    p = rg::parametre_decode("q=%e6%9d%b1%e4%ba%ac", "q", q, sizeof q); // minuscules acceptées
    VERIFIER(sv(q) == "東京");
    return 0;
}

constexpr int parametre_absent_vide_ou_voisin() {
    char q[64] = {};
    VERIFIER(!rg::parametre_decode("", "q", q, sizeof q).trouve);
    VERIFIER(!rg::parametre_decode("a=1&b=2", "q", q, sizeof q).trouve);
    VERIFIER(!rg::parametre_decode("qq=x", "q", q, sizeof q).trouve); // un autre nom qui commence pareil
    rg::Parametre p = rg::parametre_decode("q=", "q", q, sizeof q);
    VERIFIER(p.trouve && p.taille == 0 && sv(q) == "");
    p = rg::parametre_decode("q", "q", q, sizeof q); // sans « = »
    VERIFIER(p.trouve && p.taille == 0);
    p = rg::parametre_decode("qq=x&q=y", "q", q, sizeof q);
    VERIFIER(p.trouve && sv(q) == "y");
    return 0;
}

constexpr int parametre_mal_forme_ou_trop_long() {
    char q[8] = {};
    VERIFIER(rg::parametre_decode("q=%zz", "q", q, sizeof q).invalide);
    VERIFIER(rg::parametre_decode("q=%4", "q", q, sizeof q).invalide);
    VERIFIER(rg::parametre_decode("q=ab%", "q", q, sizeof q).invalide);
    // la vue s'arrête avant « A » : le chiffre manquant ne doit pas être pris juste après la fin
    VERIFIER(rg::parametre_decode(sv("q=%4A", 4), "q", q, sizeof q).invalide);
    VERIFIER(!rg::parametre_decode(sv("q=%4A", 5), "q", q, sizeof q).invalide);
    rg::Parametre p = rg::parametre_decode("q=0123456789", "q", q, sizeof q); // 7 octets utiles dans 8
    VERIFIER(p.trouve && p.tronque && p.taille == 7 && sv(q) == "0123456");
    p = rg::parametre_decode("q=0123456", "q", q, sizeof q);
    VERIFIER(!p.tronque && p.taille == 7);
    return 0;
}

constexpr int encodage_url() {
    char b[96] = {};
    api::Ecrivain e(b, sizeof b);
    rg::encoder_url("Zürich nord", e);
    VERIFIER(e.vue() == "Z%C3%BCrich%20nord");
    char c[96] = {};
    api::Ecrivain f(c, sizeof c);
    rg::encoder_url("a-b_c.d~e AZ09", f);
    VERIFIER(f.vue() == "a-b_c.d~e%20AZ09");
    char d[96] = {};
    api::Ecrivain g(d, sizeof d);
    rg::encoder_url("a&b=c?d#e/f%g+h", g); // rien de ce qui change le sens d'une adresse ne passe tel quel
    VERIFIER(g.vue() == "a%26b%3Dc%3Fd%23e%2Ff%25g%2Bh");
    return 0;
}

constexpr int adresse_du_geocodage() {
    char b[256] = {};
    api::Ecrivain e(b, sizeof b);
    rg::adresse_geocodage("Lyon", i18n::Langue::Fr, e);
    VERIFIER(e.ok());
    VERIFIER(e.vue() == "https://geocoding-api.open-meteo.com/v1/search?name=Lyon&count=8&language=fr&format=json");
    char c[256] = {};
    api::Ecrivain f(c, sizeof c);
    rg::adresse_geocodage("São Paulo", i18n::Langue::Tr, f);
    VERIFIER(f.vue() ==
             "https://geocoding-api.open-meteo.com/v1/search?name=S%C3%A3o%20Paulo&count=8&language=tr&format=json");
    return 0;
}

// ---- recherche de lieu -----------------------------------------------------------------------------------------------

constexpr int recherche_trois_caracteres_au_moins() {
    VERIFIER(rg::valider_recherche("").erreur == rg::ErreurRecherche::TropCourte);
    VERIFIER(rg::valider_recherche("Ly").erreur == rg::ErreurRecherche::TropCourte);
    VERIFIER(rg::valider_recherche("Zü").erreur == rg::ErreurRecherche::TropCourte); // 2 caractères, 3 octets
    VERIFIER(rg::valider_recherche("   ").erreur == rg::ErreurRecherche::TropCourte);
    VERIFIER(rg::valider_recherche(" Ly ").erreur == rg::ErreurRecherche::TropCourte); // après rognage
    VERIFIER(rg::valider_recherche("Lyo").erreur == rg::ErreurRecherche::Aucune);
    VERIFIER(rg::valider_recherche("東京都").erreur == rg::ErreurRecherche::Aucune); // 3 caractères, 9 octets
    return 0;
}

constexpr int recherche_rognee_et_plafonnee() {
    rg::ResultatRecherche r = rg::valider_recherche("  Lyon  ");
    VERIFIER(r.erreur == rg::ErreurRecherche::Aucune && r.q == "Lyon");
    char quatre_vingts[82] = {};
    for (int i = 0; i < 80; ++i)
        quatre_vingts[i] = 'a';
    VERIFIER(rg::valider_recherche(quatre_vingts).erreur == rg::ErreurRecherche::Aucune);
    quatre_vingts[80] = 'a';
    VERIFIER(rg::valider_recherche(quatre_vingts).erreur == rg::ErreurRecherche::TropLongue);
    char accents[200] = {}; // 80 « é » = 160 octets : accepté, ce sont les caractères qui comptent
    for (int i = 0; i < 80; ++i) {
        accents[2 * i] = '\xC3';
        accents[2 * i + 1] = '\xA9';
    }
    VERIFIER(rg::valider_recherche(accents).erreur == rg::ErreurRecherche::Aucune);
    return 0;
}

constexpr int recherche_illisible() {
    VERIFIER(rg::valider_recherche("Ly\non").erreur == rg::ErreurRecherche::Illisible);
    VERIFIER(rg::valider_recherche("Ly\x01on").erreur == rg::ErreurRecherche::Illisible);
    VERIFIER(rg::valider_recherche("Ly\xFFon").erreur == rg::ErreurRecherche::Illisible);
    VERIFIER(rg::valider_recherche("Ly\xC3").erreur == rg::ErreurRecherche::Illisible);
    return 0;
}

// ---- luminosité, thème -----------------------------------------------------------------------------------------------

constexpr int luminosite_de_5_a_100() {
    VERIFIER(!rg::luminosite_valide(0));
    VERIFIER(!rg::luminosite_valide(4));
    VERIFIER(rg::luminosite_valide(5));
    VERIFIER(rg::luminosite_valide(75));
    VERIFIER(rg::luminosite_valide(100));
    VERIFIER(!rg::luminosite_valide(101));
    VERIFIER(!rg::luminosite_valide(-20));
    return 0;
}

constexpr int luminosite_lue_en_nvs_jamais_noire() {
    VERIFIER(rg::luminosite_depuis_nvs(0) == rg::LUMINOSITE_DEFAUT); // un NVS vide ou abîmé n'éteint pas l'écran
    VERIFIER(rg::luminosite_depuis_nvs(4) == rg::LUMINOSITE_DEFAUT);
    VERIFIER(rg::luminosite_depuis_nvs(300) == rg::LUMINOSITE_DEFAUT);
    VERIFIER(rg::luminosite_depuis_nvs(5) == 5);
    VERIFIER(rg::luminosite_depuis_nvs(100) == 100);
    VERIFIER(rg::LUMINOSITE_DEFAUT == 75);
    return 0;
}

constexpr int themes_connus_et_indices() {
    VERIFIER(rg::theme_depuis_code("auto") == rg::Theme::Auto);
    VERIFIER(rg::theme_depuis_code("clair") == rg::Theme::Clair);
    VERIFIER(rg::theme_depuis_code("sombre") == rg::Theme::Sombre);
    VERIFIER(!rg::theme_depuis_code("Sombre")); // la casse compte
    VERIFIER(!rg::theme_depuis_code(""));
    VERIFIER(!rg::theme_depuis_code("noir"));
    VERIFIER(rg::code(rg::Theme::Sombre) == "sombre");
    VERIFIER(rg::theme_depuis_indice(2) == rg::Theme::Sombre);
    VERIFIER(rg::theme_depuis_indice(3) == rg::Theme::Auto);
    VERIFIER(rg::theme_depuis_indice(-1) == rg::Theme::Auto);
    VERIFIER(rg::theme_depuis_indice(3, rg::Theme::Clair) == rg::Theme::Clair);
    return 0;
}

// ---- affichage -------------------------------------------------------------------------------------------------------

constexpr rg::ResultatAffichage affichage_de(sv corps, const rg::Affichage &actuel = {}) {
    const json::Resultat r = json::lire(corps);
    return rg::valider_affichage(r.valeur, actuel);
}

constexpr int affichage_champs_presents_seulement() {
    rg::Affichage a;
    a.langue_ecran = i18n::Langue::De;
    a.theme = rg::Theme::Clair;
    a.luminosite = 60;
    rg::ResultatAffichage r = affichage_de(R"({"luminosite":40})", a);
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune);
    VERIFIER(r.valeur.luminosite == 40 && r.valeur.langue_ecran == i18n::Langue::De &&
             r.valeur.theme == rg::Theme::Clair);
    r = affichage_de(R"({"langue_ecran":"tr","langue_assistant":"it","theme":"sombre","luminosite":100})", a);
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune);
    VERIFIER(r.valeur.langue_ecran == i18n::Langue::Tr && r.valeur.langue_assistant == i18n::Langue::It);
    VERIFIER(r.valeur.theme == rg::Theme::Sombre && r.valeur.luminosite == 100);
    r = affichage_de("{}", a); // rien à changer
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune && r.valeur == a);
    r = affichage_de(R"({"inconnu":1,"langues":[]})", a); // champs en lecture seule ou inconnus : ignorés
    VERIFIER(r.erreur == rg::ErreurAffichage::Aucune && r.valeur == a);
    return 0;
}

constexpr int affichage_langues_inconnues_refusees() {
    VERIFIER(affichage_de(R"({"langue_ecran":"xx"})").erreur == rg::ErreurAffichage::LangueEcran);
    VERIFIER(affichage_de(R"({"langue_ecran":"FR"})").erreur == rg::ErreurAffichage::LangueEcran); // la casse compte
    VERIFIER(affichage_de(R"({"langue_ecran":""})").erreur == rg::ErreurAffichage::LangueEcran);
    VERIFIER(affichage_de(R"({"langue_ecran":1})").erreur == rg::ErreurAffichage::LangueEcran);
    VERIFIER(affichage_de(R"({"langue_ecran":null})").erreur == rg::ErreurAffichage::LangueEcran);
    VERIFIER(affichage_de(R"({"langue_ecran":"francais-tres-long"})").erreur == rg::ErreurAffichage::LangueEcran);
    VERIFIER(affichage_de(R"({"langue_assistant":"xx"})").erreur == rg::ErreurAffichage::LangueAssistant);
    VERIFIER(affichage_de(R"({"langue_assistant":true})").erreur == rg::ErreurAffichage::LangueAssistant);
    return 0;
}

constexpr int affichage_theme_et_luminosite_refuses() {
    VERIFIER(affichage_de(R"({"theme":"noir"})").erreur == rg::ErreurAffichage::Theme);
    VERIFIER(affichage_de(R"({"theme":"Sombre"})").erreur == rg::ErreurAffichage::Theme);
    VERIFIER(affichage_de(R"({"theme":2})").erreur == rg::ErreurAffichage::Theme);
    VERIFIER(affichage_de(R"({"luminosite":4})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":0})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":101})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":-5})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":50.5})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":"50"})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":null})").erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(affichage_de(R"({"luminosite":1e2})").erreur == rg::ErreurAffichage::Luminosite); // pas un entier écrit
    VERIFIER(affichage_de(R"({"luminosite":5})").erreur == rg::ErreurAffichage::Aucune);
    VERIFIER(affichage_de(R"({"luminosite":100})").erreur == rg::ErreurAffichage::Aucune);
    return 0;
}

constexpr int affichage_tout_ou_rien() {
    rg::Affichage a;
    a.luminosite = 60;
    const rg::ResultatAffichage r = affichage_de(R"({"langue_ecran":"de","theme":"sombre","luminosite":3})", a);
    VERIFIER(r.erreur == rg::ErreurAffichage::Luminosite);
    VERIFIER(r.valeur == a); // « de » et « sombre » ne sont pas appliqués
    return 0;
}

// ---- ville enregistrée -----------------------------------------------------------------------------------------------

constexpr rg::ResultatZone zone_de(sv corps) {
    const json::Resultat r = json::lire(corps);
    return rg::valider_zone(r.valeur);
}

constexpr int zone_envoyee_par_la_page() {
    // Un résultat de recherche tel que la page le renvoie : champs en plus (id, altitude, population, libelle) ignorés.
    const rg::ResultatZone r =
        zone_de(R"({"id":"2996944","nom":"Lyon","region":"Auvergne-Rhône-Alpes","pays":"France",
        "code_pays":"FR","latitude":45.74846,"longitude":4.84671,"altitude":173,"fuseau":"Europe/Paris",
        "population":522250,"libelle":"Lyon, Auvergne-Rhône-Alpes, France"})");
    VERIFIER(r.erreur == rg::ErreurZone::Aucune);
    VERIFIER(sv(r.zone.nom) == "Lyon" && sv(r.zone.region) == "Auvergne-Rhône-Alpes" && sv(r.zone.pays) == "France");
    VERIFIER(sv(r.zone.code_pays) == "FR" && sv(r.zone.fuseau) == "Europe/Paris");
    VERIFIER(r.zone.latitude > 45.748 && r.zone.latitude < 45.749 && r.zone.longitude > 4.846 &&
             r.zone.longitude < 4.847);
    return 0;
}

constexpr int zone_champs_facultatifs_absents() {
    const rg::ResultatZone r = zone_de(R"({"nom":"Lyon","latitude":45.7,"longitude":4.8,"fuseau":"Europe/Paris"})");
    VERIFIER(r.erreur == rg::ErreurZone::Aucune);
    VERIFIER(sv(r.zone.region) == "" && sv(r.zone.pays) == "" && sv(r.zone.code_pays) == "");
    return 0;
}

constexpr int zone_nom_refuse() {
    constexpr sv fin = R"(,"latitude":1,"longitude":1,"fuseau":"Europe/Paris"})";
    char b[400] = {};
    auto avec_nom = [&](sv nom) {
        api::Ecrivain e(b, sizeof b);
        e.brut(R"({"nom":)");
        e.chaine(nom);
        e.brut(fin);
        return zone_de(e.vue());
    };
    VERIFIER(avec_nom("").erreur == rg::ErreurZone::Nom);
    VERIFIER(avec_nom("   ").erreur == rg::ErreurZone::Nom);
    VERIFIER(avec_nom("Ly\non").erreur == rg::ErreurZone::Nom);
    VERIFIER(avec_nom("Ly\x01on").erreur == rg::ErreurZone::Nom);
    VERIFIER(avec_nom("Ly\xFF").erreur == rg::ErreurZone::Nom);
    char quatre_vingt_un[82] = {};
    for (int i = 0; i < 81; ++i)
        quatre_vingt_un[i] = 'a';
    VERIFIER(avec_nom(quatre_vingt_un).erreur == rg::ErreurZone::Nom);
    quatre_vingt_un[80] = '\0';
    VERIFIER(avec_nom(quatre_vingt_un).erreur == rg::ErreurZone::Aucune);
    VERIFIER(zone_de(R"({"latitude":1,"longitude":1,"fuseau":"Europe/Paris"})").erreur == rg::ErreurZone::Nom);
    VERIFIER(zone_de(R"({"nom":12,"latitude":1,"longitude":1,"fuseau":"Europe/Paris"})").erreur == rg::ErreurZone::Nom);
    VERIFIER(zone_de(R"({"nom":null,"latitude":1,"longitude":1,"fuseau":"Europe/Paris"})").erreur ==
             rg::ErreurZone::Nom);
    return 0;
}

constexpr int zone_nom_coupe_sur_une_frontiere_de_caractere() {
    // 40 « é » = 80 octets : accepté (40 caractères), mais l'appareil n'en garde que 47 octets, soit 23 « é ».
    char deux_octets[100] = {};
    for (int i = 0; i < 40; ++i) {
        deux_octets[2 * i] = '\xC3';
        deux_octets[2 * i + 1] = '\xA9';
    }
    char b[300] = {};
    api::Ecrivain e(b, sizeof b);
    e.brut(R"({"nom":)");
    e.chaine(deux_octets);
    e.brut(R"(,"latitude":1,"longitude":1,"fuseau":"Europe/Paris"})");
    const rg::ResultatZone r = zone_de(e.vue());
    VERIFIER(r.erreur == rg::ErreurZone::Aucune);
    VERIFIER(sv(r.zone.nom).size() == 46 && rg::utf8_valide(r.zone.nom) && rg::nombre_de_caracteres(r.zone.nom) == 23);
    return 0;
}

constexpr int zone_coordonnees_refusees() {
    auto z = [](sv lat, sv lon) {
        char b[200] = {};
        api::Ecrivain e(b, sizeof b);
        e.brut(R"({"nom":"X","latitude":)");
        e.brut(lat);
        e.brut(R"(,"longitude":)");
        e.brut(lon);
        e.brut(R"(,"fuseau":"Europe/Paris"})");
        return zone_de(e.vue()).erreur;
    };
    VERIFIER(z("45", "4") == rg::ErreurZone::Aucune);
    VERIFIER(z("90", "180") == rg::ErreurZone::Aucune);
    VERIFIER(z("-90", "-180") == rg::ErreurZone::Aucune);
    VERIFIER(z("90.0001", "4") == rg::ErreurZone::Coordonnees);
    VERIFIER(z("-91", "4") == rg::ErreurZone::Coordonnees);
    VERIFIER(z("45", "180.5") == rg::ErreurZone::Coordonnees);
    VERIFIER(z("45", "-181") == rg::ErreurZone::Coordonnees);
    VERIFIER(z("\"45\"", "4") == rg::ErreurZone::Coordonnees); // une chaîne n'est pas un nombre
    VERIFIER(z("null", "4") == rg::ErreurZone::Coordonnees);
    VERIFIER(z("45", "true") == rg::ErreurZone::Coordonnees);
    VERIFIER(z("1e999", "4") == rg::ErreurZone::Coordonnees); // hors de la plage d'un double
    VERIFIER(zone_de(R"({"nom":"X","longitude":4,"fuseau":"Europe/Paris"})").erreur == rg::ErreurZone::Coordonnees);
    VERIFIER(zone_de(R"({"nom":"X","latitude":4,"fuseau":"Europe/Paris"})").erreur == rg::ErreurZone::Coordonnees);
    return 0;
}

constexpr int zone_fuseau_inconnu_refuse() {
    VERIFIER(zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"Mars/Olympus"})").erreur ==
             rg::ErreurZone::Fuseau);
    VERIFIER(zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"europe/paris"})").erreur ==
             rg::ErreurZone::Fuseau);
    VERIFIER(zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":""})").erreur == rg::ErreurZone::Fuseau);
    VERIFIER(zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":3})").erreur == rg::ErreurZone::Fuseau);
    VERIFIER(zone_de(R"({"nom":"X","latitude":1,"longitude":1})").erreur == rg::ErreurZone::Fuseau);
    VERIFIER(
        zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"Europe/Parisssssssssssssssssssssssssssssssssssss"})")
            .erreur == rg::ErreurZone::Fuseau); // plus long que le tampon
    VERIFIER(
        zone_de(R"({"nom":"X","latitude":-34,"longitude":-58,"fuseau":"America/Argentina/Buenos_Aires"})").erreur ==
        rg::ErreurZone::Aucune);
    return 0;
}

constexpr int zone_ordre_des_verifications() {
    // nom, puis coordonnées, puis fuseau : le premier défaut est celui qu'on dit
    VERIFIER(zone_de(R"({"nom":"","latitude":999,"longitude":1,"fuseau":"x"})").erreur == rg::ErreurZone::Nom);
    VERIFIER(zone_de(R"({"nom":"X","latitude":999,"longitude":1,"fuseau":"x"})").erreur == rg::ErreurZone::Coordonnees);
    VERIFIER(zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"x"})").erreur == rg::ErreurZone::Fuseau);
    return 0;
}

constexpr int zone_champs_d_affichage_invalides_deviennent_vides() {
    const rg::ResultatZone r = zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"Europe/Paris",
        "region":12,"pays":"Fr\u0001ance","code_pays":"<b>"})");
    VERIFIER(r.erreur == rg::ErreurZone::Aucune); // ils ne servent qu'à la page : pas de refus pour eux
    VERIFIER(sv(r.zone.region) == "" && sv(r.zone.pays) == "" && sv(r.zone.code_pays) == "");
    const rg::ResultatZone s =
        zone_de(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"Europe/Paris","code_pays":"FRAN"})");
    VERIFIER(sv(s.zone.code_pays) == "");
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(objets_tableaux_et_virgules)                                                                                     \
    X(tableau_d_objets)                                                                                                \
    X(chaines_echappees_accents_conserves)                                                                             \
    X(nombres_entiers)                                                                                                 \
    X(nombres_decimaux_sans_printf)                                                                                    \
    X(nombre_invalide_ecrit_null)                                                                                      \
    X(debordement_sans_ecriture_hors_tampon)                                                                           \
    X(utf8_bien_forme)                                                                                                 \
    X(utf8_mal_forme_refuse)                                                                                           \
    X(caracteres_et_controles)                                                                                         \
    X(rognage_et_coupure_sur_frontiere)                                                                                \
    X(parametre_simple_et_decode)                                                                                      \
    X(parametre_absent_vide_ou_voisin)                                                                                 \
    X(parametre_mal_forme_ou_trop_long)                                                                                \
    X(encodage_url)                                                                                                    \
    X(adresse_du_geocodage)                                                                                            \
    X(recherche_trois_caracteres_au_moins)                                                                             \
    X(recherche_rognee_et_plafonnee)                                                                                   \
    X(recherche_illisible)                                                                                             \
    X(luminosite_de_5_a_100)                                                                                           \
    X(luminosite_lue_en_nvs_jamais_noire)                                                                              \
    X(themes_connus_et_indices)                                                                                        \
    X(affichage_champs_presents_seulement)                                                                             \
    X(affichage_langues_inconnues_refusees)                                                                            \
    X(affichage_theme_et_luminosite_refuses)                                                                           \
    X(affichage_tout_ou_rien)                                                                                          \
    X(zone_envoyee_par_la_page)                                                                                        \
    X(zone_champs_facultatifs_absents)                                                                                 \
    X(zone_nom_refuse)                                                                                                 \
    X(zone_nom_coupe_sur_une_frontiere_de_caractere)                                                                   \
    X(zone_coordonnees_refusees)                                                                                       \
    X(zone_fuseau_inconnu_refuse)                                                                                      \
    X(zone_ordre_des_verifications)                                                                                    \
    X(zone_champs_d_affichage_invalides_deviennent_vides)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
