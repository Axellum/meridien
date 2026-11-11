// Tests du noyau pur : la logique de l'API de la page de réglages (api_reglages.hpp) avec un faux appareil.
// Chaque test rejoue des requêtes comme la page les envoie et lit la réponse JSON comme la page la lira.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/api_reglages.hpp"
#include "ecran/i18n.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/mer_spots.hpp"
#include "mini.hpp"

namespace api = ecran::api;
namespace mer = ecran::mer;
namespace ap = ecran::appairage;
namespace json = ecran::json;
namespace i18n = ecran::i18n;
namespace rg = ecran::reglages;
using api::Methode;
using sv = std::string_view;

namespace {

// ---- le faux appareil --------------------------------------------------------------------------------------------------

struct FauxHote {
    rg::Affichage aff{};
    rg::Zone zone{};
    bool zone_definie = true;
    bool zone_ecriture_ok = true;
    sv source = "auto"; // la source météo en service (un identifiant de la table : vue sur des données statiques)
    bool source_retablie = false; // la dernière ville a remis « Automatique »
    bool source_ecriture_ok = true;
    int enreg_source = 0;
    std::size_t nom_max = 0; // > 0 : le « NVS » garde au plus ce nombre d'octets du nom
    int enreg_affichage = 0;
    int enreg_zone = 0;
    rg::Alertes al{};
    int enreg_alertes = 0;
    bool alertes_ecriture_ok = true;
    mer::Registre registre_mer{}; // le registre des spots de la page mer, tel que l'appareil le garde
    int enreg_mer = 0;
    bool mer_ecriture_ok = true;
    bool mer_en_memoire =
        true; // faux : l'appareil n'a pas pu garder son registre en mémoire (PSRAM pleine au démarrage)
    api::MeteoApercu meteo{};
    bool meteo_dispo = false;
    api::InfoSysteme sys{};
    api::InfoWifi wifi_{};
    sv reponse_geocodage{};
    api::Telechargement dl = api::Telechargement::Ok;
    int telechargements = 0;
    char derniere_adresse[1300]{};
    std::int64_t horloge = 1000;
    std::uint8_t graine = 0;

    constexpr FauxHote() {
        rg::copier_texte("Lyon", zone.nom, sizeof zone.nom);
        rg::copier_texte("Auvergne-Rhône-Alpes", zone.region, sizeof zone.region);
        rg::copier_texte("France", zone.pays, sizeof zone.pays);
        rg::copier_texte("FR", zone.code_pays, sizeof zone.code_pays);
        zone.latitude = 45.74906;
        zone.longitude = 4.84789;
        rg::copier_texte("Europe/Paris", zone.fuseau, sizeof zone.fuseau);
        sys.nom = "ecran-f6b6";
        sys.modele = "M5Stack Tab5";
        sys.version = "0.1.0-test";
        sys.compile_le = "2026-11-06T11:20:00Z";
        sys.partition = "ota_0";
        sys.depuis_secondes = 3600;
        sys.memoire_libre = 123456;
        sys.heure_epoch = 1793960400;
        sys.heure_synchronisee = true;
        sys.fuseau = "Europe/Paris";
        sys.internet = true;
        wifi_.etat = api::EtatWifi::Connecte;
        rg::copier_texte("MonReseau", wifi_.ssid, sizeof wifi_.ssid);
        wifi_.force_dbm = -60;
        rg::copier_texte("192.168.1.20", wifi_.ip, sizeof wifi_.ip);
    }

    constexpr rg::Affichage affichage() const { return aff; }
    constexpr bool enregistrer_affichage(const rg::Affichage &a) {
        aff = a;
        ++enreg_affichage;
        return true;
    }
    constexpr rg::Alertes alertes() const { return al; }
    constexpr bool enregistrer_alertes(const rg::Alertes &a) {
        ++enreg_alertes;
        if (!alertes_ecriture_ok)
            return false;
        al = a;
        return true;
    }
    constexpr bool mer_lire(mer::Registre &r) {
        if (!mer_en_memoire)
            return false;
        r = registre_mer;
        return true;
    }
    constexpr bool mer_enregistrer(const mer::Registre &r) {
        ++enreg_mer;
        // Comme l'appareil (reglages_mer_enregistrer) : un registre que le noyau ne reconnaît pas pour valide n'est pas gardé.
        if (!mer_ecriture_ok || !mer::registre_valide(r))
            return false;
        registre_mer = r;
        return true;
    }
    constexpr bool zone_lire(rg::Zone &z) {
        if (!zone_definie)
            return false;
        z = zone;
        return true;
    }
    constexpr bool zone_enregistrer(const rg::Zone &z) {
        ++enreg_zone;
        if (!zone_ecriture_ok)
            return false;
        zone = z;
        if (nom_max > 0)
            zone.nom[nom_max] = '\0';
        zone_definie = true;
        // Comme l'appareil : une source qui ne couvre pas le pays de la nouvelle ville repasse sur « Automatique ».
        const sv apres = ecran::meteo::fournisseurs::source_valable_pour(source, z.code_pays);
        source_retablie = apres != source;
        source = apres;
        return true;
    }
    constexpr sv source_meteo() { return source; }
    constexpr bool source_meteo_enregistrer(sv id) {
        ++enreg_source;
        if (!source_ecriture_ok)
            return false;
        const std::size_t i = ecran::meteo::fournisseurs::indice_de(id);
        if (i >= ecran::meteo::fournisseurs::NOMBRE)
            return false;
        source = ecran::meteo::fournisseurs::TABLE[i].id; // l'identifiant de la table, pas la copie de la requête
        source_retablie = false;
        return true;
    }
    constexpr bool source_meteo_retablie() { return source_retablie; }
    constexpr bool meteo_lire(api::MeteoApercu &m) {
        m = meteo;
        return meteo_dispo;
    }
    constexpr api::InfoSysteme systeme() { return sys; }
    constexpr api::InfoWifi wifi() { return wifi_; }
    constexpr api::Telechargement telecharger(sv url, char *tampon, std::size_t capacite, std::size_t &taille) {
        ++telechargements;
        rg::copier_texte(url, derniere_adresse, sizeof derniere_adresse);
        if (dl != api::Telechargement::Ok)
            return dl;
        if (reponse_geocodage.size() > capacite)
            return api::Telechargement::Echec;
        for (std::size_t i = 0; i < reponse_geocodage.size(); ++i)
            tampon[i] = reponse_geocodage[i];
        taille = reponse_geocodage.size();
        return api::Telechargement::Ok;
    }
    constexpr std::int64_t maintenant_ms() { return horloge; }
    constexpr void aleatoire(std::uint8_t *sortie, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            sortie[i] = static_cast<std::uint8_t>(graine + i);
        graine = static_cast<std::uint8_t>(graine + 11);
    }
};

// Un appareil, son API, ses tampons, et la session de « la page ».
struct Banc {
    FauxHote hote;
    api::Api<FauxHote> moteur{hote};
    char sortie[8192]{};
    char travail[16384]{};
    char brouillon[4100]{}; // pour fabriquer un corps ou une requête très long
    char jeton[ap::JETON_CARACTERES + 1]{};
    mer::Registre mer_actuel{}; // les deux registres de travail de /api/sea (en PSRAM sur l'appareil)
    mer::Registre mer_neuf{};

    constexpr Banc() { moteur.appairage().nouveau_code(123456); }

    constexpr api::Tampons tampons() {
        api::Tampons t;
        t.sortie = sortie;
        t.cap_sortie = sizeof sortie;
        t.travail = travail;
        t.cap_travail = sizeof travail;
        t.mer_actuel = &mer_actuel;
        t.mer_neuf = &mer_neuf;
        return t;
    }
    constexpr api::Reponse appeler(Methode m, sv chemin, sv requete = {}, sv corps = {}, sv jeton_ = {}) {
        api::Requete rq;
        rq.methode = m;
        rq.chemin = chemin;
        rq.requete = requete;
        rq.corps = corps;
        rq.jeton = jeton_;
        return moteur.traiter(rq, tampons());
    }
    constexpr api::Reponse avec_jeton(Methode m, sv chemin, sv requete = {}, sv corps = {}) {
        return appeler(m, chemin, requete, corps, sv(jeton));
    }
    // POST /api/pair avec le bon code ; garde le jeton reçu.
    constexpr bool connecter() {
        const api::Reponse r = appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})");
        if (r.statut != 200)
            return false;
        const json::Resultat l = json::lire(r.corps);
        if (!l.ok())
            return false;
        l.valeur.champ("jeton").copier_texte(jeton, sizeof jeton);
        return sv(jeton).size() == ap::JETON_CARACTERES;
    }
    // « q= » suivi de n fois « a » (dans le brouillon).
    constexpr sv requete_longue(std::size_t n) {
        brouillon[0] = 'q';
        brouillon[1] = '=';
        for (std::size_t i = 0; i < n; ++i)
            brouillon[2 + i] = 'a';
        brouillon[2 + n] = '\0';
        return sv(brouillon, 2 + n);
    }
    constexpr sv corps_long(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            brouillon[i] = ' ';
        brouillon[n] = '\0';
        return sv(brouillon, n);
    }
};

// ---- lecture des réponses ----------------------------------------------------------------------------------------------

constexpr bool json_ok(const api::Reponse &r) { return json::lire(r.corps).ok(); }
constexpr bool existe(const api::Reponse &r, sv chemin) {
    const json::Resultat l = json::lire(r.corps);
    return l.ok() && l.valeur.trouver(chemin).existe();
}
constexpr bool texte_est(const api::Reponse &r, sv chemin, sv attendu) {
    const json::Resultat l = json::lire(r.corps);
    return l.ok() && l.valeur.trouver(chemin).texte_egal(attendu);
}
constexpr bool entier_est(const api::Reponse &r, sv chemin, std::int64_t attendu) {
    const json::Resultat l = json::lire(r.corps);
    std::int64_t v = 0;
    return l.ok() && l.valeur.trouver(chemin).en_entier(v) && v == attendu;
}
constexpr bool booleen_est(const api::Reponse &r, sv chemin, bool attendu) {
    const json::Resultat l = json::lire(r.corps);
    bool v = false;
    return l.ok() && l.valeur.trouver(chemin).en_booleen(v) && v == attendu;
}
constexpr bool decimal_est(const api::Reponse &r, sv chemin, double attendu) {
    const json::Resultat l = json::lire(r.corps);
    double v = 0;
    if (!l.ok() || !l.valeur.trouver(chemin).en_double(v))
        return false;
    const double ecart = v > attendu ? v - attendu : attendu - v;
    return ecart < 1e-9;
}
constexpr std::size_t taille_de(const api::Reponse &r, sv chemin) {
    const json::Resultat l = json::lire(r.corps);
    return l.ok() ? l.valeur.trouver(chemin).taille() : 9999;
}
// Réponse d'erreur : statut, identifiant stable, champ fautif (vide = aucun).
constexpr bool est_erreur(const api::Reponse &r, int statut, sv id, sv champ = {}) {
    if (r.statut != statut || !texte_est(r, "erreur.id", id) || !existe(r, "erreur.message"))
        return false;
    return champ.empty() ? !existe(r, "erreur.champ") : texte_est(r, "erreur.champ", champ);
}
constexpr bool contient(sv texte, sv motif) { return texte.find(motif) != sv::npos; }
constexpr void copier_message(const api::Reponse &r, sv chemin, char (&dest)[220]) {
    dest[0] = '\0';
    const json::Resultat l = json::lire(r.corps);
    if (l.ok())
        l.valeur.trouver(chemin).copier_texte(dest, sizeof dest);
}

// ---- appairage et accès ------------------------------------------------------------------------------------------------

constexpr int statut_public_sans_jeton() {
    Banc b;
    api::Reponse r = b.appeler(Methode::Get, "/api/status");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "nom", "ecran-f6b6") && texte_est(r, "modele", "M5Stack Tab5"));
    VERIFIER(texte_est(r, "version", "0.1.0-test") && texte_est(r, "mode", "station"));
    VERIFIER(texte_est(r, "appairage", "requis") && texte_est(r, "langue", "fr"));
    VERIFIER(b.connecter());
    r = b.avec_jeton(Methode::Get, "/api/status");
    VERIFIER(texte_est(r, "appairage", "ok"));
    r = b.appeler(Methode::Get, "/api/status", {}, {}, "un-jeton-qui-n-existe-pas");
    VERIFIER(texte_est(r, "appairage", "requis"));
    b.hote.aff.langue_ecran = i18n::Langue::Tr;
    VERIFIER(texte_est(b.appeler(Methode::Get, "/api/status"), "langue", "tr"));
    return 0;
}

struct Route {
    Methode m;
    sv chemin;
};
constexpr Route ROUTES_PROTEGEES[] = {
    {Methode::Get, "/api/system"},
    {Methode::Get, "/api/location"},
    {Methode::Put, "/api/location"},
    {Methode::Get, "/api/display"},
    {Methode::Put, "/api/display"},
    {Methode::Get, "/api/alerts"},
    {Methode::Put, "/api/alerts"},
    {Methode::Get, "/api/sea"},
    {Methode::Put, "/api/sea"},
    {Methode::Get, "/api/weather"},
    {Methode::Get, "/api/geocode"},
    {Methode::Get, "/api/wifi"},
    {Methode::Put, "/api/wifi"},
    {Methode::Get, "/api/wifi/scan"},
    {Methode::Get, "/api/services"},
    {Methode::Put, "/api/services/llm"},
    {Methode::Post, "/api/services/llm/test"},
    {Methode::Get, "/api/presets"},
    {Methode::Get, "/api/update"},
    {Methode::Post, "/api/update"},
    {Methode::Get, "/api/settings/export"},
    {Methode::Post, "/api/settings/import"},
    {Methode::Delete, "/api/pair"},
    {Methode::Get, "/api/inconnue"},
    {Methode::Autre, "/api/system"},
};

constexpr int sans_jeton_valide_tout_est_refuse_en_401() {
    Banc b;
    VERIFIER(b.connecter()); // une session existe : un autre jeton ne doit pas passer pour autant
    for (const Route &route : ROUTES_PROTEGEES) {
        api::Reponse r = b.appeler(route.m, route.chemin, {}, "{}");
        VERIFIER(est_erreur(r, 401, "non_appaire"));
        VERIFIER(texte_est(r, "erreur.message", "Entrez le code affiché sur l’écran de l’appareil."));
        r = b.appeler(route.m, route.chemin, {}, "{}",
                      "0000000000000000000000000000000000000000000000000000000000000000");
        VERIFIER(r.statut == 401);
    }
    VERIFIER(b.hote.enreg_zone == 0 && b.hote.enreg_affichage == 0 && b.hote.telechargements == 0);
    VERIFIER(b.hote.enreg_alertes == 0 && b.hote.enreg_mer == 0);
    return 0;
}

constexpr int bon_code_donne_un_jeton_qui_ouvre_l_acces() {
    Banc b;
    api::Reponse r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(taille_de(r, "") == 1); // le jeton, rien d'autre
    VERIFIER(b.connecter());
    VERIFIER(b.avec_jeton(Methode::Get, "/api/system").statut == 200);
    VERIFIER(b.moteur.appairage().sessions_ouvertes() == 2);
    return 0;
}

constexpr int mauvais_code_403_ou_demande_illisible_400() {
    Banc b;
    api::Reponse r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"654321"})");
    VERIFIER(est_erreur(r, 403, "code_faux", "code"));
    VERIFIER(texte_est(r, "erreur.message", "Ce code n’est pas le bon. Regardez l’écran de l’appareil."));
    VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":123456})"), 403, "code_faux",
                        "code")); // un nombre
    VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":null})"), 403, "code_faux", "code"));
    VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, "{}"), 403, "code_faux", "code"));
    VERIFIER(b.moteur.appairage().sessions_ouvertes() == 0);
    VERIFIER(
        b.connecter()); // un succès remet le compte des essais ratés à zéro (4 ratés jusque-là : le 5e verrouillerait)
    VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"12345678901234567890"})"), 403,
                        "code_faux", "code"));
    // une demande illisible n'est pas un essai de code : elle ne compte pas pour le verrou
    for (int i = 0; i < 6; ++i)
        VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, "pas du json"), 400, "illisible"));
    VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, ""), 400, "illisible"));
    VERIFIER(est_erreur(b.appeler(Methode::Post, "/api/pair", {}, R"(["123456"])"), 400, "illisible"));
    VERIFIER(b.moteur.appairage().sessions_ouvertes() == 1);
    return 0;
}

constexpr int verrou_apres_cinq_essais_avec_compte_a_rebours() {
    Banc b;
    for (int i = 0; i < 4; ++i)
        VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"000000"})").statut == 403);
    api::Reponse r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"000000"})");
    VERIFIER(est_erreur(r, 429, "verrouille") && r.reessayer_dans == 60 && entier_est(r, "erreur.reessayer_dans", 60));
    VERIFIER(texte_est(r, "erreur.message", "Trop d’essais. Réessayez dans 60 secondes."));
    b.hote.horloge += 10000; // même le bon code est refusé pendant le verrou
    r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})");
    VERIFIER(est_erreur(r, 429, "verrouille") && r.reessayer_dans == 50);
    VERIFIER(texte_est(r, "erreur.message", "Trop d’essais. Réessayez dans 50 secondes."));
    b.hote.horloge += 49500;
    r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})");
    VERIFIER(est_erreur(r, 429, "verrouille") && r.reessayer_dans == 1);
    VERIFIER(texte_est(r, "erreur.message", "Trop d’essais. Réessayez dans 1 seconde."));
    b.hote.horloge += 500;
    VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})").statut == 200);
    return 0;
}

// Des séries de 5 mauvais codes, chacune à la fin du verrou de la précédente : le verrou double, la 3e série change le code, la
// session déjà ouverte reste, l'ancien code est refusé et le nouveau (celui que l'écran montrera) accepté.
constexpr int series_ratees_verrou_qui_double_et_code_renouvele() {
    Banc b;
    VERIFIER(b.connecter());
    const std::uint32_t gen = b.moteur.appairage().generation();
    constexpr int attendu[] = {60, 120, 240};
    for (int k = 0; k < 3; ++k) {
        api::Reponse r;
        for (int i = 0; i < ap::ESSAIS_MAX; ++i)
            r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"000000"})");
        VERIFIER(est_erreur(r, 429, "verrouille") && r.reessayer_dans == attendu[k]);
        VERIFIER(entier_est(r, "erreur.reessayer_dans", attendu[k]));
        b.hote.horloge += attendu[k] * 1000;
    }
    VERIFIER(b.moteur.appairage().generation() == gen + 1 && b.moteur.appairage().code() != "123456");
    VERIFIER(b.avec_jeton(Methode::Get, "/api/system").statut == 200); // la session ouverte avant reste
    VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})").statut == 403);
    char corps[32] = "{\"code\":\"";
    const sv nouveau = b.moteur.appairage().code();
    for (std::size_t i = 0; i < nouveau.size(); ++i)
        corps[9 + i] = nouveau[i];
    corps[15] = '"';
    corps[16] = '}';
    VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, sv(corps, 17)).statut == 200);
    return 0;
}

// Le bouton « Nouveau code » de l'écran pendant un verrou : les sessions sont fermées (401), le verrou levé, le nouveau code accepté.
constexpr int nouveau_code_ferme_les_sessions_et_leve_le_verrou() {
    Banc b;
    VERIFIER(b.connecter());
    for (int i = 0; i < ap::ESSAIS_MAX; ++i)
        b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"000000"})");
    VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})").statut == 429);
    b.moteur.appairage().nouveau_code(654321);
    VERIFIER(est_erreur(b.avec_jeton(Methode::Get, "/api/system"), 401, "non_appaire"));
    VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"123456"})").statut == 403);
    VERIFIER(b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"654321"})").statut == 200);
    return 0;
}

// Le serveur ne lit le corps que pour une route publique ou un jeton valide ; sinon `traiter` sans corps répond bien 401.
constexpr int corps_lu_seulement_pour_route_publique_ou_jeton_valide() {
    Banc b;
    api::Requete rq;
    rq.methode = Methode::Post;
    rq.chemin = "/api/pair";
    VERIFIER(b.moteur.corps_a_lire(rq));
    rq.methode = Methode::Get;
    rq.chemin = "/api/status";
    VERIFIER(b.moteur.corps_a_lire(rq));
    VERIFIER(b.connecter());
    for (const Route &route : ROUTES_PROTEGEES) {
        rq.methode = route.m;
        rq.chemin = route.chemin;
        rq.jeton = {};
        VERIFIER(!b.moteur.corps_a_lire(rq));
        VERIFIER(b.appeler(route.m, route.chemin).statut == 401); // la réponse donnée sans lire le corps
        rq.jeton = "0000000000000000000000000000000000000000000000000000000000000000";
        VERIFIER(!b.moteur.corps_a_lire(rq));
        rq.jeton = sv(b.jeton);
        VERIFIER(b.moteur.corps_a_lire(rq));
    }
    // une route publique avec une autre méthode n'est pas publique
    rq.jeton = {};
    rq.methode = Methode::Put;
    rq.chemin = "/api/pair";
    VERIFIER(!b.moteur.corps_a_lire(rq));
    rq.methode = Methode::Post;
    rq.chemin = "/api/status";
    VERIFIER(!b.moteur.corps_a_lire(rq));
    return 0;
}

constexpr int fermer_la_session_retire_l_acces() {
    Banc b;
    VERIFIER(b.connecter());
    const api::Reponse r = b.avec_jeton(Methode::Delete, "/api/pair");
    VERIFIER(r.statut == 204 && r.corps.empty());
    VERIFIER(b.avec_jeton(Methode::Get, "/api/system").statut == 401);
    return 0;
}

// ---- système, Wi-Fi, services ------------------------------------------------------------------------------------------

constexpr int systeme_decrit_l_appareil() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/system");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "nom", "ecran-f6b6") && texte_est(r, "modele", "M5Stack Tab5") &&
             texte_est(r, "version", "0.1.0-test"));
    VERIFIER(texte_est(r, "compile_le", "2026-11-06T11:20:00Z") && texte_est(r, "partition", "ota_0"));
    VERIFIER(booleen_est(r, "validee", true) && entier_est(r, "depuis_secondes", 3600) &&
             entier_est(r, "memoire_libre", 123456));
    VERIFIER(entier_est(r, "heure.epoch", 1793960400) && booleen_est(r, "heure.synchronisee", true));
    VERIFIER(texte_est(r, "heure.fuseau", "Europe/Paris") && booleen_est(r, "internet", true));
    b.hote.sys.compile_le = {};
    b.hote.sys.heure_synchronisee = false;
    b.hote.sys.internet = false;
    r = b.avec_jeton(Methode::Get, "/api/system");
    VERIFIER(!existe(r, "compile_le") && booleen_est(r, "heure.synchronisee", false) &&
             booleen_est(r, "internet", false));
    return 0;
}

constexpr int wifi_en_lecture_seule_sans_secret() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/wifi");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "etat", "connecte") && texte_est(r, "ssid", "MonReseau") &&
             texte_est(r, "ip", "192.168.1.20"));
    VERIFIER(entier_est(r, "niveau", 3) && entier_est(r, "force", -60));
    VERIFIER(texte_est(r, "message", "Connecté à « MonReseau »."));
    VERIFIER(taille_de(r, "") == 6); // etat, ssid, niveau, force, ip, message : aucun mot de passe, aucun champ de plus
    b.hote.wifi_.etat = api::EtatWifi::Deconnecte;
    r = b.avec_jeton(Methode::Get, "/api/wifi");
    VERIFIER(texte_est(r, "etat", "deconnecte") && entier_est(r, "niveau", 0) && entier_est(r, "force", 0));
    VERIFIER(texte_est(r, "message", "Non connecté."));
    b.hote.wifi_.etat = api::EtatWifi::Connexion;
    VERIFIER(texte_est(b.avec_jeton(Methode::Get, "/api/wifi"), "message", "Connexion en cours…"));
    b.hote.wifi_.etat = api::EtatWifi::Echec;
    VERIFIER(texte_est(b.avec_jeton(Methode::Get, "/api/wifi"), "message", "La connexion a échoué."));
    return 0;
}

constexpr int wifi_nom_de_reseau_avec_guillemets_reste_du_json_valide() {
    Banc b;
    VERIFIER(b.connecter());
    rg::copier_texte("Bob \"Wi-Fi\" \\ é", b.hote.wifi_.ssid, sizeof b.hote.wifi_.ssid);
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/wifi");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "ssid", "Bob \"Wi-Fi\" \\ é"));
    VERIFIER(texte_est(r, "message", "Connecté à « Bob \"Wi-Fi\" \\ é »."));
    return 0;
}

constexpr int niveaux_du_signal_wifi() {
    VERIFIER(api::niveau_wifi(-30) == 4 && api::niveau_wifi(-55) == 4);
    VERIFIER(api::niveau_wifi(-56) == 3 && api::niveau_wifi(-65) == 3);
    VERIFIER(api::niveau_wifi(-66) == 2 && api::niveau_wifi(-75) == 2);
    VERIFIER(api::niveau_wifi(-76) == 1 && api::niveau_wifi(-85) == 1);
    VERIFIER(api::niveau_wifi(-86) == 0 && api::niveau_wifi(-120) == 0);
    return 0;
}

constexpr int services_presets_et_mise_a_jour_vides_mais_sans_erreur() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/services");
    VERIFIER(r.statut == 200 && json_ok(r));
    for (sv id : {"stt", "llm", "tts"}) {
        char chemin[24] = {};
        api::Ecrivain e(chemin, sizeof chemin);
        e.brut(id);
        e.brut(".configure");
        VERIFIER(booleen_est(r, e.vue(), false));
        char chemin2[24] = {};
        api::Ecrivain e2(chemin2, sizeof chemin2);
        e2.brut(id);
        e2.brut(".cle_definie");
        VERIFIER(booleen_est(r, e2.vue(), false));
    }
    r = b.avec_jeton(Methode::Get, "/api/presets");
    VERIFIER(r.statut == 200 && taille_de(r, "stt") == 0 && taille_de(r, "llm") == 0 && taille_de(r, "tts") == 0);
    VERIFIER(existe(r, "stt") && existe(r, "llm") && existe(r, "tts"));
    r = b.avec_jeton(Methode::Get, "/api/update");
    VERIFIER(r.statut == 200 && texte_est(r, "etat", "inactif") && entier_est(r, "progression", 0));
    VERIFIER(texte_est(r, "version", "0.1.0-test"));
    return 0;
}

constexpr Route ROUTES_NON_DISPONIBLES[] = {
    {Methode::Put, "/api/wifi"},
    {Methode::Get, "/api/wifi/scan"},
    {Methode::Post, "/api/wifi/scan"},
    {Methode::Put, "/api/services/stt"},
    {Methode::Put, "/api/services/llm"},
    {Methode::Put, "/api/services/tts"},
    {Methode::Post, "/api/services/stt/test"},
    {Methode::Post, "/api/services/llm/test"},
    {Methode::Post, "/api/services/tts/test"},
    {Methode::Get, "/api/settings/export"},
    {Methode::Post, "/api/settings/import"},
    {Methode::Post, "/api/update"},
    {Methode::Post, "/api/update/rollback"},
};

constexpr int fonctions_hors_perimetre_repondent_501() {
    Banc b;
    VERIFIER(b.connecter());
    for (const Route &route : ROUTES_NON_DISPONIBLES) {
        const api::Reponse r = b.avec_jeton(route.m, route.chemin, {}, "{}");
        VERIFIER(est_erreur(r, 501, "non_disponible"));
        VERIFIER(texte_est(r, "erreur.message", "Cette fonction n’est pas encore disponible sur cet écran."));
    }
    VERIFIER(est_erreur(b.avec_jeton(Methode::Get, "/api/inconnue"), 404, "inconnue"));
    VERIFIER(texte_est(b.avec_jeton(Methode::Get, "/api/inconnue"), "erreur.message", "Cette page n’existe pas."));
    VERIFIER(est_erreur(b.avec_jeton(Methode::Delete, "/api/system"), 404, "inconnue"));
    VERIFIER(est_erreur(b.avec_jeton(Methode::Autre, "/api/system"), 404, "inconnue"));
    return 0;
}

// ---- ville --------------------------------------------------------------------------------------------------------------

constexpr int ville_lue_telle_que_stockee() {
    Banc b;
    VERIFIER(b.connecter());
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "nom", "Lyon") && texte_est(r, "region", "Auvergne-Rhône-Alpes") &&
             texte_est(r, "pays", "France"));
    VERIFIER(texte_est(r, "code_pays", "FR") && texte_est(r, "fuseau", "Europe/Paris"));
    VERIFIER(decimal_est(r, "latitude", 45.74906) && decimal_est(r, "longitude", 4.84789));
    VERIFIER(texte_est(r, "posix", "CET-1CEST,M3.5.0,M10.5.0/3"));
    VERIFIER(texte_est(r, "libelle", "Lyon, Auvergne-Rhône-Alpes, France"));
    return 0;
}

constexpr int ville_sans_zone_enregistree() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.zone_definie = false;
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(r.statut == 200 && booleen_est(r, "definie", false) && !existe(r, "nom"));
    return 0;
}

constexpr sv MARSEILLE = R"({"id":"2995469","nom":"Marseille","region":"Provence-Alpes-Côte d’Azur","pays":"France",
    "code_pays":"FR","latitude":43.29695,"longitude":5.38107,"altitude":28,"fuseau":"Europe/Paris","population":873076,
    "libelle":"Marseille, Provence-Alpes-Côte d’Azur, France"})";

constexpr int ville_choisie_est_enregistree_puis_relue() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, MARSEILLE);
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "nom", "Marseille") && texte_est(r, "region", "Provence-Alpes-Côte d’Azur"));
    VERIFIER(decimal_est(r, "latitude", 43.29695) && decimal_est(r, "longitude", 5.38107));
    VERIFIER(texte_est(r, "posix", "CET-1CEST,M3.5.0,M10.5.0/3"));
    VERIFIER(b.hote.enreg_zone == 1 && sv(b.hote.zone.nom) == "Marseille");
    r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(texte_est(r, "nom", "Marseille") &&
             texte_est(r, "libelle", "Marseille, Provence-Alpes-Côte d’Azur, France"));
    return 0;
}

constexpr int la_reponse_montre_ce_qui_est_reellement_stocke() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.nom_max = 4; // le stockage raccourcit le nom
    const api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, MARSEILLE);
    VERIFIER(r.statut == 200 && texte_est(r, "nom", "Mars"));
    return 0;
}

constexpr int ville_refusee_ne_touche_pas_au_stockage() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/location", {}, corps); };
    VERIFIER(est_erreur(put(R"({"nom":"","latitude":1,"longitude":1,"fuseau":"Europe/Paris"})"), 422, "nom_invalide",
                        "nom"));
    VERIFIER(est_erreur(put(R"({"nom":"X","latitude":95,"longitude":1,"fuseau":"Europe/Paris"})"), 422,
                        "coordonnees_invalides", "latitude"));
    VERIFIER(est_erreur(put(R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"Mars/Olympus"})"), 422, "fuseau_inconnu",
                        "fuseau"));
    VERIFIER(est_erreur(put("[]"), 400, "illisible"));
    VERIFIER(est_erreur(put("{"), 400, "illisible"));
    VERIFIER(est_erreur(put(""), 400, "illisible"));
    VERIFIER(b.hote.enreg_zone == 0 && sv(b.hote.zone.nom) == "Lyon");
    return 0;
}

constexpr int echec_d_ecriture_donne_500() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.zone_ecriture_ok = false;
    const api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, MARSEILLE);
    VERIFIER(est_erreur(r, 500, "interne"));
    VERIFIER(texte_est(r, "erreur.message", "L’appareil a rencontré un problème. Réessayez."));
    VERIFIER(sv(b.hote.zone.nom) == "Lyon");
    return 0;
}

// ---- source de la météo ------------------------------------------------------------------------------------------------

constexpr sv AMSTERDAM = R"({"nom":"Amsterdam","region":"Hollande-Septentrionale","pays":"Pays-Bas","code_pays":"NL",
    "latitude":52.37403,"longitude":4.88969,"fuseau":"Europe/Amsterdam"})";

constexpr int ville_lue_avec_la_source_meteo_et_la_liste_de_son_pays() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "source_meteo", "auto") && texte_est(r, "source_meteo_nom", "Automatique") &&
             booleen_est(r, "source_meteo_retablie", false));
    // France : « Automatique » en tête, puis le seul modèle mondial que le lecteur accepte
    VERIFIER(taille_de(r, "sources_meteo") == 2);
    VERIFIER(texte_est(r, "sources_meteo[0].id", "auto") && texte_est(r, "sources_meteo[0].nom", "Automatique"));
    VERIFIER(texte_est(r, "sources_meteo[1].id", "noaa") && texte_est(r, "sources_meteo[1].nom", "NOAA GFS"));
    VERIFIER(booleen_est(r, "sources_meteo[0].direct", false) && booleen_est(r, "sources_meteo[1].direct", false));
    // Pays-Bas : le service national avant le mondial ; son nom est un nom propre, jamais traduit
    rg::copier_texte("NL", b.hote.zone.code_pays, sizeof b.hote.zone.code_pays);
    b.hote.source = "knmi";
    b.hote.aff.langue_ecran = i18n::Langue::Tr;
    r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(taille_de(r, "sources_meteo") == 3);
    VERIFIER(texte_est(r, "sources_meteo[0].id", "auto") && texte_est(r, "sources_meteo[0].nom", "Otomatik"));
    VERIFIER(texte_est(r, "sources_meteo[1].id", "knmi") && texte_est(r, "sources_meteo[1].nom", "KNMI"));
    VERIFIER(texte_est(r, "sources_meteo[2].id", "noaa"));
    VERIFIER(texte_est(r, "source_meteo", "knmi") && texte_est(r, "source_meteo_nom", "KNMI"));
    return 0;
}

// Un service lu directement (ADR-0014) n'est pas un modèle d'Open-Meteo : la liste le dit (`direct`), la page n'écrit pas « Modèle : … ».
constexpr int la_liste_distingue_un_service_lu_directement_d_un_modele_d_open_meteo() {
    Banc b;
    VERIFIER(b.connecter());
    // Norvège : « MET Norway » existe deux fois, comme modèle d'Open-Meteo (`metno`) et comme service lu directement (`metnorway`)
    rg::copier_texte("NO", b.hote.zone.code_pays, sizeof b.hote.zone.code_pays);
    b.hote.source = "metnorway";
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "source_meteo", "metnorway") && texte_est(r, "source_meteo_nom", "MET Norway (api.met.no)"));
    VERIFIER(taille_de(r, "sources_meteo") == 4);
    VERIFIER(texte_est(r, "sources_meteo[0].id", "auto") && booleen_est(r, "sources_meteo[0].direct", false));
    VERIFIER(texte_est(r, "sources_meteo[1].id", "metno") && booleen_est(r, "sources_meteo[1].direct", false));
    VERIFIER(texte_est(r, "sources_meteo[2].id", "metnorway") && booleen_est(r, "sources_meteo[2].direct", true));
    VERIFIER(texte_est(r, "sources_meteo[2].nom", "MET Norway (api.met.no)"));
    VERIFIER(texte_est(r, "sources_meteo[3].id", "noaa") && booleen_est(r, "sources_meteo[3].direct", false));
    // États-Unis : le NWS, lui aussi lu directement
    rg::copier_texte("US", b.hote.zone.code_pays, sizeof b.hote.zone.code_pays);
    b.hote.source = "nws";
    r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "source_meteo", "nws") && texte_est(r, "source_meteo_nom", "NWS (weather.gov)"));
    VERIFIER(taille_de(r, "sources_meteo") == 3);
    VERIFIER(texte_est(r, "sources_meteo[1].id", "nws") && booleen_est(r, "sources_meteo[1].direct", true));
    VERIFIER(texte_est(r, "sources_meteo[2].id", "noaa") && booleen_est(r, "sources_meteo[2].direct", false));
    return 0;
}

constexpr int source_meteo_connue_meme_sans_ville_enregistree() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.zone_definie = false;
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(booleen_est(r, "definie", false) && texte_est(r, "source_meteo", "auto"));
    VERIFIER(taille_de(r, "sources_meteo") == 2 && texte_est(r, "sources_meteo[1].id", "noaa")); // sans pays connu
    return 0;
}

constexpr int source_choisie_seule_ne_touche_pas_a_la_ville() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, R"({"source_meteo":"noaa"})");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(b.hote.enreg_source == 1 && b.hote.source == "noaa" && b.hote.enreg_zone == 0);
    VERIFIER(texte_est(r, "source_meteo", "noaa") && texte_est(r, "source_meteo_nom", "NOAA GFS") &&
             booleen_est(r, "source_meteo_retablie", false));
    VERIFIER(texte_est(r, "nom", "Lyon") && texte_est(r, "code_pays", "FR")); // la ville est relue, inchangée
    r = b.avec_jeton(Methode::Get, "/api/location");
    VERIFIER(texte_est(r, "source_meteo", "noaa"));
    // retour à « Automatique »
    r = b.avec_jeton(Methode::Put, "/api/location", {}, R"({"source_meteo":"auto"})");
    VERIFIER(r.statut == 200 && texte_est(r, "source_meteo", "auto") && b.hote.source == "auto");
    return 0;
}

constexpr int source_seule_sans_ville_enregistree_reste_possible() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.zone_definie = false;
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, R"({"source_meteo":"noaa"})");
    VERIFIER(r.statut == 200 && booleen_est(r, "definie", false) && texte_est(r, "source_meteo", "noaa"));
    VERIFIER(b.hote.enreg_zone == 0 && b.hote.source == "noaa");
    r = b.avec_jeton(Methode::Put, "/api/location", {}, R"({"source_meteo":"knmi"})"); // aucun pays connu
    VERIFIER(est_erreur(r, 422, "source_indisponible", "source_meteo"));
    return 0;
}

constexpr int source_inconnue_ou_hors_pays_est_refusee_sans_rien_changer() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/location", {}, corps); };
    VERIFIER(est_erreur(put(R"({"source_meteo":"inconnue"})"), 422, "source_inconnue", "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":""})"), 422, "source_inconnue", "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":null})"), 422, "source_inconnue", "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":5})"), 422, "source_inconnue", "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":["knmi"]})"), 422, "source_inconnue", "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":"knmi_seamless_et_beaucoup_trop_long"})"), 422, "source_inconnue",
                        "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":"AUTO"})"), 422, "source_inconnue", "source_meteo")); // la casse compte
    // Existe, mais ne couvre pas la France : refusée comme l'écran ne la propose pas
    VERIFIER(est_erreur(put(R"({"source_meteo":"knmi"})"), 422, "source_indisponible", "source_meteo"));
    VERIFIER(est_erreur(put(R"({"source_meteo":"metno"})"), 422, "source_indisponible", "source_meteo"));
    // Connue de la table mais écartée pour l'instant parce que le lecteur de l'appareil refuserait sa réponse (pas d'UV)
    VERIFIER(est_erreur(put(R"({"source_meteo":"dwd"})"), 422, "source_indisponible", "source_meteo"));
    VERIFIER(b.hote.enreg_source == 0 && b.hote.enreg_zone == 0 && b.hote.source == "auto");
    return 0;
}

constexpr int ville_et_source_ensemble_la_source_est_jugee_sur_le_nouveau_pays() {
    Banc b;
    VERIFIER(b.connecter());
    // KNMI ne couvre pas la France où la page se trouve encore, mais couvre les Pays-Bas où elle va
    sv corps_ok = R"({"nom":"Amsterdam","region":"Hollande-Septentrionale","pays":"Pays-Bas","code_pays":"NL",
        "latitude":52.37403,"longitude":4.88969,"fuseau":"Europe/Amsterdam","source_meteo":"knmi"})";
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, corps_ok);
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(b.hote.enreg_zone == 1 && b.hote.enreg_source == 1 && b.hote.source == "knmi");
    VERIFIER(texte_est(r, "nom", "Amsterdam") && texte_est(r, "source_meteo", "knmi") &&
             booleen_est(r, "source_meteo_retablie", false));
    // Une source de l'ancien pays ne passe pas : la ville n'est PAS enregistrée non plus
    sv corps_mauvais = R"({"nom":"Marseille","code_pays":"FR","latitude":43.29695,"longitude":5.38107,
        "fuseau":"Europe/Paris","source_meteo":"knmi"})";
    r = b.avec_jeton(Methode::Put, "/api/location", {}, corps_mauvais);
    VERIFIER(est_erreur(r, 422, "source_indisponible", "source_meteo"));
    VERIFIER(b.hote.enreg_zone == 1 && sv(b.hote.zone.nom) == "Amsterdam");
    // Une ville refusée est refusée avant la source : l'erreur est celle de la ville
    r = b.avec_jeton(Methode::Put, "/api/location", {},
                     R"({"nom":"","latitude":1,"longitude":1,"fuseau":"Europe/Paris","source_meteo":"inconnue"})");
    VERIFIER(est_erreur(r, 422, "nom_invalide", "nom"));
    VERIFIER(b.hote.enreg_zone == 1 && b.hote.enreg_source == 1);
    return 0;
}

constexpr int changer_de_ville_hors_pays_remet_automatique_et_la_reponse_le_dit() {
    Banc b;
    VERIFIER(b.connecter());
    rg::copier_texte("NL", b.hote.zone.code_pays, sizeof b.hote.zone.code_pays);
    b.hote.source = "knmi";
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, MARSEILLE); // la France : KNMI ne la couvre pas
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "source_meteo", "auto") && booleen_est(r, "source_meteo_retablie", true));
    VERIFIER(taille_de(r, "sources_meteo") == 2); // la liste est déjà celle du nouveau pays
    VERIFIER(booleen_est(b.avec_jeton(Methode::Get, "/api/location"), "source_meteo_retablie",
                         true)); // jusqu'au prochain choix
    r = b.avec_jeton(Methode::Put, "/api/location", {}, R"({"source_meteo":"noaa"})");
    VERIFIER(booleen_est(r, "source_meteo_retablie", false));
    // Une source qui couvre encore le pays est gardée : rien à dire
    r = b.avec_jeton(Methode::Put, "/api/location", {}, AMSTERDAM);
    VERIFIER(texte_est(r, "source_meteo", "noaa") && booleen_est(r, "source_meteo_retablie", false));
    return 0;
}

constexpr int source_non_enregistree_donne_500() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.source_ecriture_ok = false;
    const api::Reponse r = b.avec_jeton(Methode::Put, "/api/location", {}, R"({"source_meteo":"noaa"})");
    VERIFIER(est_erreur(r, 500, "interne"));
    VERIFIER(b.hote.source == "auto");
    return 0;
}

// ---- affichage ----------------------------------------------------------------------------------------------------------

constexpr int affichage_lu_avec_les_listes_de_l_appareil() {
    Banc b;
    VERIFIER(b.connecter());
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/display");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "langue_ecran", "fr") && texte_est(r, "langue_assistant", "fr") &&
             texte_est(r, "theme", "auto"));
    VERIFIER(entier_est(r, "luminosite", 75));
    VERIFIER(taille_de(r, "langues") == 7 && taille_de(r, "langues_assistant") == 7 && taille_de(r, "themes") == 3);
    VERIFIER(texte_est(r, "langues[0].code", "fr") && texte_est(r, "langues[1].code", "en") &&
             texte_est(r, "langues[6].code", "tr"));
    VERIFIER(texte_est(r, "langues[6].nom", i18n::NOMS[6]) && texte_est(r, "langues_assistant[2].code", "de"));
    VERIFIER(texte_est(r, "themes[0]", "auto") && texte_est(r, "themes[1]", "clair") &&
             texte_est(r, "themes[2]", "sombre"));
    // Le profil de l'accueil (ADR-0023) : « auto » par défaut, et seulement les profils que l'appareil dessine.
    VERIFIER(texte_est(r, "profil", "auto") && taille_de(r, "profils") == 2 && texte_est(r, "profils[0]", "auto") &&
             texte_est(r, "profils[1]", "meteo"));
    return 0;
}

constexpr int le_profil_de_l_accueil_se_regle_et_se_relit() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/display", {}, R"({"profil":"meteo"})");
    VERIFIER(r.statut == 200 && texte_est(r, "profil", "meteo") && entier_est(r, "luminosite", 75));
    VERIFIER(b.hote.aff.profil == rg::ProfilAccueil::Meteo && b.hote.enreg_affichage == 1);
    VERIFIER(b.hote.aff.theme == rg::Theme::Auto); // les autres champs ne bougent pas
    r = b.avec_jeton(Methode::Get, "/api/display");
    VERIFIER(texte_est(r, "profil", "meteo"));
    // Même valeur : rien n'est écrit.
    VERIFIER(b.avec_jeton(Methode::Put, "/api/display", {}, R"({"profil":"meteo"})").statut == 200);
    VERIFIER(b.hote.enreg_affichage == 1);
    // Le dépingler rend la main aux règles.
    r = b.avec_jeton(Methode::Put, "/api/display", {}, R"({"profil":"auto","theme":"clair"})");
    VERIFIER(r.statut == 200 && texte_est(r, "profil", "auto") && texte_est(r, "theme", "clair"));
    VERIFIER(b.hote.aff.profil == rg::ProfilAccueil::Auto && b.hote.enreg_affichage == 2);
    return 0;
}

constexpr int un_profil_inconnu_est_refuse_sans_rien_changer() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/display", {}, corps); };
    VERIFIER(put(R"({"profil":"meteo"})").statut == 200);
    VERIFIER(est_erreur(put(R"({"profil":"surf"})"), 422, "profil_inconnu", "profil")); // pas encore dessiné
    VERIFIER(est_erreur(put(R"({"profil":"Meteo"})"), 422, "profil_inconnu", "profil"));
    VERIFIER(est_erreur(put(R"({"profil":1})"), 422, "profil_inconnu", "profil"));
    VERIFIER(est_erreur(put(R"({"profil":""})"), 422, "profil_inconnu", "profil"));
    VERIFIER(est_erreur(put(R"({"profil":"auto","theme":"noir"})"), 422, "theme_inconnu", "theme"));
    VERIFIER(b.hote.aff.profil == rg::ProfilAccueil::Meteo && b.hote.enreg_affichage == 1); // le profil n'a pas bougé
    return 0;
}

constexpr int affichage_modifie_champ_par_champ() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/display", {}, R"({"luminosite":40})");
    VERIFIER(r.statut == 200 && entier_est(r, "luminosite", 40) && texte_est(r, "theme", "auto"));
    VERIFIER(b.hote.aff.luminosite == 40 && b.hote.enreg_affichage == 1);
    r = b.avec_jeton(Methode::Put, "/api/display", {},
                     R"({"theme":"sombre","langue_ecran":"de","langue_assistant":"it"})");
    VERIFIER(r.statut == 200 && texte_est(r, "theme", "sombre") && texte_est(r, "langue_ecran", "de"));
    VERIFIER(texte_est(r, "langue_assistant", "it") && entier_est(r, "luminosite", 40));
    VERIFIER(b.hote.aff.langue_ecran == i18n::Langue::De && b.hote.aff.langue_assistant == i18n::Langue::It);
    VERIFIER(b.hote.aff.theme == rg::Theme::Sombre && b.hote.enreg_affichage == 2);
    r = b.avec_jeton(Methode::Get, "/api/display");
    VERIFIER(texte_est(r, "theme", "sombre") && entier_est(r, "luminosite", 40));
    return 0;
}

constexpr int affichage_inchange_n_ecrit_rien() {
    Banc b;
    VERIFIER(b.connecter());
    VERIFIER(b.avec_jeton(Methode::Put, "/api/display", {}, R"({"luminosite":75,"theme":"auto"})").statut == 200);
    VERIFIER(b.avec_jeton(Methode::Put, "/api/display", {}, "{}").statut == 200);
    VERIFIER(b.hote.enreg_affichage == 0);
    return 0;
}

constexpr int affichage_refuse_tout_ou_rien() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/display", {}, corps); };
    VERIFIER(est_erreur(put(R"({"langue_ecran":"xx"})"), 422, "langue_ecran_inconnue", "langue_ecran"));
    VERIFIER(est_erreur(put(R"({"langue_assistant":"xx"})"), 422, "langue_assistant_inconnue", "langue_assistant"));
    VERIFIER(est_erreur(put(R"({"theme":"noir"})"), 422, "theme_inconnu", "theme"));
    VERIFIER(est_erreur(put(R"({"luminosite":3})"), 422, "luminosite", "luminosite"));
    VERIFIER(est_erreur(put(R"({"luminosite":101})"), 422, "luminosite", "luminosite"));
    VERIFIER(est_erreur(put(R"({"luminosite":"50"})"), 422, "luminosite", "luminosite"));
    VERIFIER(est_erreur(put(R"({"theme":"sombre","luminosite":3})"), 422, "luminosite",
                        "luminosite")); // « sombre » non appliqué
    VERIFIER(est_erreur(put("[]"), 400, "illisible"));
    VERIFIER(est_erreur(put("pas du json"), 400, "illisible"));
    VERIFIER(b.hote.enreg_affichage == 0 && b.hote.aff.theme == rg::Theme::Auto && b.hote.aff.luminosite == 75);
    return 0;
}

// ---- alertes officielles (docs/api-reglages.md §14) --------------------------------------------------------------------

constexpr int alertes_lues_avec_les_valeurs_par_defaut_et_les_niveaux() {
    Banc b;
    VERIFIER(b.connecter());
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/alerts");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(booleen_est(r, "actives", true) && texte_est(r, "adresse", "") && entier_est(r, "periode_min", 15));
    VERIFIER(texte_est(r, "niveau_min", "yellow"));
    VERIFIER(
        texte_est(r, "adresse_publique", "https://vigie.auxfilsdesidees.fr")); // pour l'afficher à côté d'un champ vide
    VERIFIER(taille_de(r, "niveaux") == 3 && texte_est(r, "niveaux[0]", "yellow") &&
             texte_est(r, "niveaux[1]", "orange") && texte_est(r, "niveaux[2]", "red"));
    return 0;
}

constexpr int alertes_modifiees_champ_par_champ() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/alerts", {}, corps); };
    api::Reponse r = put(R"({"periode_min":30})");
    VERIFIER(r.statut == 200 && entier_est(r, "periode_min", 30) && booleen_est(r, "actives", true));
    VERIFIER(b.hote.al.periode_min == 30 && b.hote.enreg_alertes == 1);
    r = put(R"({"actives":false,"niveau_min":"red"})");
    VERIFIER(r.statut == 200 && booleen_est(r, "actives", false) && texte_est(r, "niveau_min", "red"));
    VERIFIER(entier_est(r, "periode_min", 30) && b.hote.enreg_alertes == 2);
    r = put(
        R"({"adresse":"  HTTPS://vigie.exemple.fr/  "})"); // normalisée avant d'être gardée, et c'est elle qui est rendue
    VERIFIER(r.statut == 200 && texte_est(r, "adresse", "https://vigie.exemple.fr") && b.hote.enreg_alertes == 3);
    VERIFIER(texte_est(r, "adresse_publique", "https://vigie.auxfilsdesidees.fr")); // le défaut ne change pas
    r = put(R"({"adresse":""})");                                                   // vide : l'instance publique
    VERIFIER(r.statut == 200 && texte_est(r, "adresse", "") && b.hote.al.adresse[0] == '\0' &&
             b.hote.enreg_alertes == 4);
    r = b.avec_jeton(Methode::Get, "/api/alerts");
    VERIFIER(booleen_est(r, "actives", false) && texte_est(r, "niveau_min", "red") && entier_est(r, "periode_min", 30));
    return 0;
}

constexpr int alertes_inchangees_n_ecrivent_rien() {
    Banc b;
    VERIFIER(b.connecter());
    VERIFIER(b.avec_jeton(Methode::Put, "/api/alerts", {}, R"({"actives":true,"periode_min":15,"niveau_min":"yellow"})")
                 .statut == 200);
    VERIFIER(b.avec_jeton(Methode::Put, "/api/alerts", {}, "{}").statut == 200);
    VERIFIER(b.avec_jeton(Methode::Put, "/api/alerts", {}, R"({"adresse":"   "})").statut == 200); // vide = déjà le cas
    VERIFIER(b.hote.enreg_alertes ==
             0); // pas d'écriture en mémoire permanente, et le client réseau n'est pas réveillé pour rien
    return 0;
}

constexpr int alertes_refusees_tout_ou_rien() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/alerts", {}, corps); };
    VERIFIER(est_erreur(put(R"({"actives":"oui"})"), 422, "actives_invalide", "actives"));
    VERIFIER(est_erreur(put(R"({"actives":1})"), 422, "actives_invalide", "actives"));
    VERIFIER(est_erreur(put(R"({"adresse":"http://vigie.exemple.fr"})"), 422, "adresse_invalide", "adresse"));
    VERIFIER(est_erreur(put(R"({"adresse":"vigie.exemple.fr"})"), 422, "adresse_invalide", "adresse"));
    VERIFIER(est_erreur(put(R"({"adresse":12})"), 422, "adresse_invalide", "adresse"));
    VERIFIER(est_erreur(put(R"({"periode_min":9})"), 422, "periode_invalide", "periode_min"));
    VERIFIER(est_erreur(put(R"({"periode_min":181})"), 422, "periode_invalide", "periode_min"));
    VERIFIER(
        est_erreur(put(R"({"periode_min":15.5})"), 422, "periode_invalide", "periode_min")); // refusée, pas arrondie
    VERIFIER(est_erreur(put(R"({"periode_min":"15"})"), 422, "periode_invalide", "periode_min"));
    VERIFIER(est_erreur(put(R"({"niveau_min":"vert"})"), 422, "niveau_inconnu", "niveau_min"));
    VERIFIER(est_erreur(put(R"({"niveau_min":"Red"})"), 422, "niveau_inconnu", "niveau_min")); // les codes sont exacts
    VERIFIER(est_erreur(put(R"({"actives":false,"niveau_min":"vert"})"), 422, "niveau_inconnu",
                        "niveau_min")); // rien d'appliqué
    VERIFIER(est_erreur(put("[]"), 400, "illisible"));
    VERIFIER(est_erreur(put("pas du json"), 400, "illisible"));
    VERIFIER(b.hote.enreg_alertes == 0 && b.hote.al == rg::Alertes{});
    return 0;
}

constexpr int echec_d_ecriture_des_alertes_donne_500() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.alertes_ecriture_ok = false;
    const api::Reponse r = b.avec_jeton(Methode::Put, "/api/alerts", {}, R"({"periode_min":30})");
    VERIFIER(est_erreur(r, 500, "interne"));
    VERIFIER(b.hote.enreg_alertes == 1 &&
             b.hote.al.periode_min == 15); // l'ancien état reste, et la réponse ne dit pas 30
    return 0;
}

// ---- registre des spots de la page mer (docs/api-reglages.md §15, ADR-0025) ---------------------------------------------

constexpr sv DEUX_SPOTS = R"({"spots":[{"id":"spot-un","nom":"Un"},{"id":"spot-deux","nom":"Deux"}]})";

constexpr int mer_lue_vide_avec_ses_bornes() {
    Banc b;
    VERIFIER(b.connecter());
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/sea");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(taille_de(r, "spots") == 0 && existe(r, "spots") && texte_est(r, "choisi", ""));
    VERIFIER(entier_est(r, "vent_min_kn", 12) && entier_est(r, "vent_max_kn", 25));
    VERIFIER(entier_est(r, "spots_max", 12) &&
             entier_est(r, "vent_plafond_kn", 60)); // pour que la page n'ait aucun nombre écrit en dur
    VERIFIER(b.hote.enreg_mer == 0);
    return 0;
}

constexpr int mer_ecrite_la_liste_entiere_puis_relue_telle_que_gardee() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Put, "/api/sea", {}, DEUX_SPOTS);
    VERIFIER(r.statut == 200 && json_ok(r) && b.hote.enreg_mer == 1);
    VERIFIER(taille_de(r, "spots") == 2 && texte_est(r, "spots[0].id", "spot-un") &&
             texte_est(r, "spots[0].nom", "Un"));
    VERIFIER(texte_est(r, "spots[1].id", "spot-deux") && texte_est(r, "spots[1].nom", "Deux"));
    VERIFIER(texte_est(r, "choisi", "spot-un")); // le premier spot ajouté est le choisi
    VERIFIER(b.hote.registre_mer.nombre == 2 && mer::registre_valide(b.hote.registre_mer));
    const api::Reponse relu = b.avec_jeton(Methode::Get, "/api/sea");
    VERIFIER(relu.statut == 200 && texte_est(relu, "spots[1].id", "spot-deux") && texte_est(relu, "choisi", "spot-un"));
    VERIFIER(b.hote.enreg_mer == 1);
    return 0;
}

constexpr int mer_choix_et_plage_de_vent_champ_par_champ() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/sea", {}, corps); };
    VERIFIER(put(DEUX_SPOTS).statut == 200);
    api::Reponse r = put(R"({"choisi":"spot-deux"})");
    VERIFIER(r.statut == 200 && texte_est(r, "choisi", "spot-deux") && taille_de(r, "spots") == 2);
    VERIFIER(b.hote.registre_mer.choisi == 1 && b.hote.enreg_mer == 2);
    r = put(R"({"vent_min_kn":10})");
    VERIFIER(r.statut == 200 && entier_est(r, "vent_min_kn", 10) && entier_est(r, "vent_max_kn", 25));
    r = put(R"({"vent_max_kn":30})");
    VERIFIER(r.statut == 200 && entier_est(r, "vent_min_kn", 10) && entier_est(r, "vent_max_kn", 30));
    VERIFIER(texte_est(r, "choisi", "spot-deux") && b.hote.enreg_mer == 4);
    VERIFIER(b.hote.registre_mer.vent_min_kn == 10 && b.hote.registre_mer.vent_max_kn == 30);
    return 0;
}

constexpr int mer_sans_choisi_le_meme_spot_reste_choisi() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/sea", {}, corps); };
    VERIFIER(put(DEUX_SPOTS).statut == 200 && put(R"({"choisi":"spot-deux"})").statut == 200);
    // « spot-deux » passe au rang 2 : il reste le spot choisi
    api::Reponse r = put(
        R"({"spots":[{"id":"spot-trois","nom":"Trois"},{"id":"spot-un","nom":"Un"},{"id":"spot-deux","nom":"Deux"}]})");
    VERIFIER(r.statut == 200 && texte_est(r, "choisi", "spot-deux") && taille_de(r, "spots") == 3);
    // « spot-deux » part : le premier de la liste prend la place
    r = put(R"({"spots":[{"id":"spot-trois","nom":"Trois"},{"id":"spot-un","nom":"Un"}]})");
    VERIFIER(r.statut == 200 && texte_est(r, "choisi", "spot-trois"));
    // une liste vide coupe la mer
    r = put(R"({"spots":[]})");
    VERIFIER(r.statut == 200 && taille_de(r, "spots") == 0 && texte_est(r, "choisi", ""));
    VERIFIER(!mer::demande_du_registre(b.hote.registre_mer).presente);
    return 0;
}

constexpr int mer_inchangee_n_ecrit_rien() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/sea", {}, corps); };
    VERIFIER(put("{}").statut == 200 && b.hote.enreg_mer == 0); // un registre vide qu'on « remet » vide
    VERIFIER(put(DEUX_SPOTS).statut == 200 && b.hote.enreg_mer == 1);
    VERIFIER(put(DEUX_SPOTS).statut == 200);
    VERIFIER(put(R"({"choisi":"spot-un","vent_min_kn":12,"vent_max_kn":25})").statut == 200);
    VERIFIER(put("{}").statut == 200);
    // pas d'écriture en mémoire permanente, et le client réseau n'est pas réveillé pour rien
    VERIFIER(b.hote.enreg_mer == 1);
    return 0;
}

constexpr int mer_refusee_tout_ou_rien_avec_le_champ_fautif() {
    Banc b;
    VERIFIER(b.connecter());
    auto put = [&](sv corps) { return b.avec_jeton(Methode::Put, "/api/sea", {}, corps); };
    VERIFIER(put(DEUX_SPOTS).statut == 200);
    const int ecritures = b.hote.enreg_mer;
    VERIFIER(est_erreur(put(R"({"spots":"x"})"), 422, "spots_invalide", "spots"));
    VERIFIER(est_erreur(put(R"({"spots":[{"id":"Mauvais Id","nom":"X"}]})"), 422, "spot_invalide", "spots"));
    VERIFIER(est_erreur(put(R"({"spots":[{"id":"a","nom":""}]})"), 422, "nom_invalide", "spots"));
    VERIFIER(est_erreur(put(R"({"spots":[{"id":"a","nom":"A"},{"id":"a","nom":"B"}]})"), 422, "spot_double", "spots"));
    VERIFIER(est_erreur(put(R"({"spots":[],"choisi":"x"})"), 422, "choix_inconnu", "choisi"));
    VERIFIER(est_erreur(put(R"({"choisi":"inconnu"})"), 422, "choix_inconnu", "choisi"));
    VERIFIER(est_erreur(put(R"({"vent_min_kn":30,"vent_max_kn":20})"), 422, "vent_invalide", "vent_min_kn"));
    VERIFIER(est_erreur(put(R"({"vent_min_kn":12.5})"), 422, "vent_invalide", "vent_min_kn")); // refusé, pas arrondi
    VERIFIER(est_erreur(put(R"({"vent_max_kn":61})"), 422, "vent_invalide", "vent_max_kn"));
    VERIFIER(est_erreur(put(R"({"vent_max_kn":10})"), 422, "vent_invalide",
                        "vent_max_kn")); // sous le minimum enregistré (12)
    // des champs valides à côté d'un refus ne sont pas appliqués
    VERIFIER(
        est_erreur(put(R"({"spots":[{"id":"a","nom":"A"}],"vent_min_kn":99})"), 422, "vent_invalide", "vent_min_kn"));
    VERIFIER(est_erreur(put("[]"), 400, "illisible"));
    VERIFIER(est_erreur(put("pas du json"), 400, "illisible"));
    VERIFIER(b.hote.enreg_mer == ecritures && b.hote.registre_mer.nombre == 2 && b.hote.registre_mer.vent_min_kn == 12);
    VERIFIER(texte_est(b.avec_jeton(Methode::Get, "/api/sea"), "spots[0].id", "spot-un"));
    return 0;
}

constexpr int mer_treize_spots_trop() {
    Banc b;
    VERIFIER(b.connecter());
    // 13 objets : 13 × 24 octets, très en dessous des 4 Ko
    constexpr sv TREIZE =
        R"({"spots":[{"id":"s01","nom":"S"},{"id":"s02","nom":"S"},{"id":"s03","nom":"S"},{"id":"s04","nom":"S"},)"
        R"({"id":"s05","nom":"S"},{"id":"s06","nom":"S"},{"id":"s07","nom":"S"},{"id":"s08","nom":"S"},)"
        R"({"id":"s09","nom":"S"},{"id":"s10","nom":"S"},{"id":"s11","nom":"S"},{"id":"s12","nom":"S"},{"id":"s13","nom":"S"}]})";
    VERIFIER(est_erreur(b.avec_jeton(Methode::Put, "/api/sea", {}, TREIZE), 422, "trop_de_spots", "spots"));
    VERIFIER(b.hote.enreg_mer == 0);
    return 0;
}

constexpr int mer_douze_spots_aux_noms_les_plus_longs_tiennent_dans_la_reponse() {
    Banc b;
    VERIFIER(b.connecter());
    // 12 spots de 32 lettres « é » (64 octets chacun, le pire nom valide) et d'identifiants de 42 octets.
    char corps[4096] = {};
    std::size_t p = 0;
    auto ecrire = [&](sv t) {
        for (const char c : t)
            corps[p++] = c;
    };
    ecrire(R"({"spots":[)");
    for (int i = 0; i < 12; ++i) {
        if (i > 0)
            ecrire(",");
        ecrire(R"({"id":"s)");
        corps[p++] = static_cast<char>('a' + i);
        for (int k = 0; k < 40; ++k)
            ecrire("-");
        ecrire(R"(","nom":")");
        for (int k = 0; k < 32; ++k)
            ecrire("é");
        ecrire(R"("})");
    }
    ecrire("]}");
    const api::Reponse r = b.avec_jeton(Methode::Put, "/api/sea", {}, sv(corps, p));
    VERIFIER(r.statut == 200 && json_ok(r) && taille_de(r, "spots") == 12);
    VERIFIER(b.hote.registre_mer.nombre == 12);
    return 0;
}

constexpr int mer_un_nom_avec_des_guillemets_reste_du_json_valide() {
    Banc b;
    VERIFIER(b.connecter());
    const api::Reponse r =
        b.avec_jeton(Methode::Put, "/api/sea", {}, R"({"spots":[{"id":"a","nom":"La \"Plage\" \\ d’été"}]})");
    VERIFIER(r.statut == 200 && json_ok(r) && texte_est(r, "spots[0].nom", "La \"Plage\" \\ d’été"));
    return 0;
}

constexpr int echec_d_ecriture_du_registre_mer_donne_500() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.mer_ecriture_ok = false;
    const api::Reponse r = b.avec_jeton(Methode::Put, "/api/sea", {}, DEUX_SPOTS);
    VERIFIER(est_erreur(r, 500, "interne"));
    VERIFIER(b.hote.enreg_mer == 1 &&
             b.hote.registre_mer.nombre == 0); // l'ancien état reste, et la réponse ne dit pas deux spots
    b.hote.mer_ecriture_ok = true;
    VERIFIER(taille_de(b.avec_jeton(Methode::Get, "/api/sea"), "spots") == 0);
    return 0;
}

constexpr int registre_mer_absent_donne_500_sans_rien_ecraser() {
    Banc b;
    VERIFIER(b.connecter());
    // l'appareil n'a pas pu garder son registre en mémoire : lire comme écrire disent « problème », jamais « registre vide »
    b.hote.mer_en_memoire = false;
    VERIFIER(est_erreur(b.avec_jeton(Methode::Get, "/api/sea"), 500, "interne"));
    VERIFIER(est_erreur(b.avec_jeton(Methode::Put, "/api/sea", {}, DEUX_SPOTS), 500, "interne"));
    VERIFIER(b.hote.enreg_mer == 0);
    // les deux registres de travail manquent (mémoire du serveur) : même réponse, le reste de l'API répond
    b.hote.mer_en_memoire = true;
    api::Tampons t = b.tampons();
    t.mer_actuel = nullptr;
    t.mer_neuf = nullptr;
    api::Requete rq;
    rq.methode = Methode::Put;
    rq.chemin = "/api/sea";
    rq.corps = DEUX_SPOTS;
    rq.jeton = sv(b.jeton);
    VERIFIER(est_erreur(b.moteur.traiter(rq, t), 500, "interne"));
    rq.methode = Methode::Get;
    VERIFIER(est_erreur(b.moteur.traiter(rq, t), 500, "interne"));
    rq.chemin = "/api/alerts";
    VERIFIER(b.moteur.traiter(rq, t).statut == 200);
    VERIFIER(b.hote.enreg_mer == 0 && b.hote.registre_mer.nombre == 0);
    return 0;
}

// ---- météo --------------------------------------------------------------------------------------------------------------

constexpr int meteo_absente_ou_invalide() {
    Banc b;
    VERIFIER(b.connecter());
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(r.statut == 200 && booleen_est(r, "disponible", false) && !existe(r, "actuelle"));
    b.hote.meteo_dispo = true; // lue, mais pas valide
    r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(booleen_est(r, "disponible", false));
    return 0;
}

constexpr int meteo_valide_avec_libelle_dans_la_langue_de_l_ecran() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.meteo_dispo = true;
    b.hote.meteo.valide = true;
    b.hote.meteo.temperature_c = 14.2;
    b.hote.meteo.code_wmo = 61;
    b.hote.meteo.jour = true;
    b.hote.meteo.maj_epoch_s = 1793960000;
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(booleen_est(r, "disponible", true) && booleen_est(r, "perimee", false) &&
             entier_est(r, "mise_a_jour", 1793960000));
    VERIFIER(decimal_est(r, "actuelle.temperature", 14.2) && entier_est(r, "actuelle.code", 61) &&
             booleen_est(r, "actuelle.jour", true));
    const sv francais = ecran::meteo::decrire(61, true).libelle;
    VERIFIER(texte_est(r, "actuelle.libelle", francais));
    b.hote.aff.langue_ecran = i18n::Langue::En;
    r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(texte_est(r, "actuelle.libelle", i18n::tr(francais, i18n::Langue::En)));
    VERIFIER(!texte_est(r, "actuelle.libelle", francais));
    b.hote.meteo.perimee = true;
    VERIFIER(booleen_est(b.avec_jeton(Methode::Get, "/api/weather"), "perimee", true));
    return 0;
}

// La météo nomme le service qui l'a RÉELLEMENT donnée (licence, politesse) et dit quand c'est un repli (ADR-0014).
constexpr int la_meteo_nomme_le_service_qui_l_a_donnee_et_dit_le_repli() {
    namespace fm = ecran::meteo::fournisseurs;
    Banc b;
    VERIFIER(b.connecter());
    b.hote.meteo_dispo = true;
    b.hote.meteo.valide = true;
    b.hote.meteo.temperature_c = 9.0;
    b.hote.meteo.code_wmo = 3;
    b.hote.meteo.maj_epoch_s = 1793960000;
    // sans rien d'autre : Open-Meteo, comme avant les services directs
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(texte_est(r, "fournisseur.id", "open-meteo") && texte_est(r, "fournisseur.nom", "Open-Meteo") &&
             texte_est(r, "fournisseur.lien", "https://open-meteo.com/"));
    VERIFIER(booleen_est(r, "repli", false));
    VERIFIER(entier_est(r, "mise_a_jour", 1793960000) && entier_est(r, "actuelle.code", 3)); // le reste ne bouge pas
    // MET Norway choisi et il a répondu
    b.hote.source = "metnorway";
    b.hote.meteo.origine = fm::Famille::MetNo;
    r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(texte_est(r, "fournisseur.id", "met-norway") && texte_est(r, "fournisseur.nom", "MET Norway") &&
             texte_est(r, "fournisseur.lien", "https://api.met.no/"));
    VERIFIER(booleen_est(r, "repli", false));
    // MET Norway choisi, il n'a pas répondu : la météo vient d'Open-Meteo, et c'est dit
    b.hote.meteo.origine = fm::Famille::OpenMeteo;
    r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(texte_est(r, "fournisseur.id", "open-meteo") && booleen_est(r, "repli", true));
    // le NWS
    b.hote.source = "nws";
    b.hote.meteo.origine = fm::Famille::Nws;
    r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(texte_est(r, "fournisseur.id", "nws") && texte_est(r, "fournisseur.nom", "NWS") &&
             texte_est(r, "fournisseur.lien", "https://www.weather.gov/"));
    VERIFIER(booleen_est(r, "repli", false));
    b.hote.meteo.origine = fm::Famille::OpenMeteo;
    VERIFIER(booleen_est(b.avec_jeton(Methode::Get, "/api/weather"), "repli", true));
    // un modèle d'Open-Meteo, ou « Automatique », donné par Open-Meteo : ce n'est pas un repli
    b.hote.source = "knmi";
    VERIFIER(booleen_est(b.avec_jeton(Methode::Get, "/api/weather"), "repli", false));
    b.hote.source = "auto";
    VERIFIER(booleen_est(b.avec_jeton(Methode::Get, "/api/weather"), "repli", false));
    // pas de météo : ni fournisseur ni repli à annoncer
    b.hote.meteo_dispo = false;
    r = b.avec_jeton(Methode::Get, "/api/weather");
    VERIFIER(booleen_est(r, "disponible", false) && !existe(r, "fournisseur") && !existe(r, "repli"));
    return 0;
}

// ---- recherche de lieu --------------------------------------------------------------------------------------------------

constexpr sv GEOCODAGE_LYON = R"({"results":[
  {"id":2996944,"name":"Lyon","latitude":45.74846,"longitude":4.84671,"elevation":173.0,"feature_code":"PPLA",
   "country_code":"FR","timezone":"Europe/Paris","population":522250,"country":"France","admin1":"Auvergne-Rhône-Alpes"},
  {"id":4262045,"name":"Lyon","latitude":37.69,"longitude":-85.1,"elevation":150.0,"country_code":"US",
   "timezone":"America/Chicago","country":"United States","admin1":"Kentucky"},
  {"id":1,"name":"Sans fuseau","latitude":1.0,"longitude":2.0,"country_code":"XX"},
  {"id":2,"name":"Hors plage","latitude":95.0,"longitude":2.0,"timezone":"Europe/Paris"}
 ],"generationtime_ms":0.6})";

constexpr int recherche_de_lieu_convertie_pour_la_page() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.reponse_geocodage = GEOCODAGE_LYON;
    const api::Reponse r = b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon");
    VERIFIER(r.statut == 200 && json_ok(r));
    VERIFIER(sv(b.hote.derniere_adresse) ==
             "https://geocoding-api.open-meteo.com/v1/search?name=Lyon&count=8&language=fr&format=json");
    VERIFIER(taille_de(r, "resultats") == 2); // le lieu sans fuseau et celui aux coordonnées hors plage sont écartés
    VERIFIER(texte_est(r, "resultats[0].id", "2996944") && texte_est(r, "resultats[0].nom", "Lyon"));
    VERIFIER(texte_est(r, "resultats[0].region", "Auvergne-Rhône-Alpes") &&
             texte_est(r, "resultats[0].pays", "France"));
    VERIFIER(texte_est(r, "resultats[0].code_pays", "FR") && texte_est(r, "resultats[0].fuseau", "Europe/Paris"));
    VERIFIER(decimal_est(r, "resultats[0].latitude", 45.74846) && decimal_est(r, "resultats[0].longitude", 4.84671));
    VERIFIER(entier_est(r, "resultats[0].altitude", 173) && entier_est(r, "resultats[0].population", 522250));
    VERIFIER(texte_est(r, "resultats[0].libelle", "Lyon, Auvergne-Rhône-Alpes, France"));
    VERIFIER(texte_est(r, "resultats[1].libelle", "Lyon, Kentucky, United States") &&
             decimal_est(r, "resultats[1].longitude", -85.1));
    VERIFIER(!existe(r, "resultats[1].population"));
    return 0;
}

constexpr int recherche_envoie_la_bonne_adresse() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.reponse_geocodage =
        R"({"generationtime_ms":0.2})"; // Open-Meteo : aucune clé « results » quand rien ne correspond
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/geocode", "a=1&q=Saint-%C3%89tienne&b=2");
    VERIFIER(r.statut == 200 && taille_de(r, "resultats") == 0 && existe(r, "resultats"));
    VERIFIER(sv(b.hote.derniere_adresse) ==
             "https://geocoding-api.open-meteo.com/v1/search?name=Saint-%C3%89tienne&count=8&language=fr&format=json");
    b.hote.aff.langue_ecran = i18n::Langue::Tr;
    b.avec_jeton(Methode::Get, "/api/geocode", "q=New+York");
    VERIFIER(sv(b.hote.derniere_adresse) ==
             "https://geocoding-api.open-meteo.com/v1/search?name=New%20York&count=8&language=tr&format=json");
    b.avec_jeton(Methode::Get, "/api/geocode",
                 "q=a%26count%3D100%23x%20y"); // « & », « = », « # » dans la saisie : pas de paramètre en plus
    VERIFIER(
        sv(b.hote.derniere_adresse) ==
        "https://geocoding-api.open-meteo.com/v1/search?name=a%26count%3D100%23x%20y&count=8&language=tr&format=json");
    return 0;
}

constexpr int au_plus_huit_lieux() {
    Banc b;
    VERIFIER(b.connecter());
    char grand[2048] = {};
    api::Ecrivain e(grand, sizeof grand);
    e.ouvrir_objet();
    e.cle("results");
    e.ouvrir_tableau();
    for (int i = 0; i < 10; ++i) {
        e.ouvrir_objet();
        e.champ("name", "Ville");
        e.champ_decimal("latitude", 1.0, 1);
        e.champ_decimal("longitude", 2.0, 1);
        e.champ("timezone", "Europe/Paris");
        e.fermer_objet();
    }
    e.fermer_tableau();
    e.fermer_objet();
    VERIFIER(e.ok());
    b.hote.reponse_geocodage = e.vue();
    VERIFIER(taille_de(b.avec_jeton(Methode::Get, "/api/geocode", "q=Ville"), "resultats") == 8);
    return 0;
}

constexpr int recherche_refusee_sans_appeler_internet() {
    Banc b;
    VERIFIER(b.connecter());
    auto chercher = [&](sv requete) { return b.avec_jeton(Methode::Get, "/api/geocode", requete); };
    VERIFIER(est_erreur(chercher(""), 400, "trop_court", "q"));
    VERIFIER(est_erreur(chercher("q="), 400, "trop_court", "q"));
    VERIFIER(est_erreur(chercher("q=ab"), 400, "trop_court", "q"));
    VERIFIER(est_erreur(chercher("q=Z%C3%BC"), 400, "trop_court", "q")); // « Zü » : 2 caractères
    VERIFIER(texte_est(chercher("q=ab"), "erreur.message", "Tapez au moins 3 lettres."));
    VERIFIER(est_erreur(chercher("q=%zz"), 400, "illisible", "q"));
    VERIFIER(est_erreur(chercher("q=Ly%0Aon"), 400, "illisible", "q"));
    VERIFIER(est_erreur(chercher("q=Ly%FFon"), 400, "illisible", "q"));
    VERIFIER(est_erreur(chercher(b.requete_longue(81)), 400, "trop_long", "q"));
    VERIFIER(est_erreur(chercher(b.requete_longue(1000)), 400, "trop_long", "q"));
    VERIFIER(b.hote.telechargements == 0);
    b.hote.reponse_geocodage = R"({"results":[]})";
    VERIFIER(chercher(b.requete_longue(80)).statut == 200); // 80 est la limite
    VERIFIER(b.hote.telechargements == 1);
    return 0;
}

constexpr int recherche_sans_internet_ou_reponse_illisible() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.dl = api::Telechargement::SansInternet;
    api::Reponse r = b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon");
    VERIFIER(est_erreur(r, 503, "sans_internet"));
    VERIFIER(texte_est(r, "erreur.message", "L’appareil n’a pas accès à Internet pour chercher un lieu."));
    b.hote.dl = api::Telechargement::Echec;
    VERIFIER(est_erreur(b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon"), 503, "sans_internet"));
    b.hote.dl = api::Telechargement::Ok;
    b.hote.reponse_geocodage = "<html>502 Bad Gateway</html>";
    VERIFIER(est_erreur(b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon"), 500, "interne"));
    b.hote.reponse_geocodage = "[1,2,3]";
    VERIFIER(est_erreur(b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon"), 500, "interne"));
    return 0;
}

// ---- limites, langues, secrets ------------------------------------------------------------------------------------------

constexpr int corps_de_plus_de_4_ko_refuse_en_413() {
    Banc b;
    api::Reponse r = b.appeler(Methode::Post, "/api/pair", {}, b.corps_long(4097)); // avant même de regarder le jeton
    VERIFIER(est_erreur(r, 413, "trop_gros"));
    VERIFIER(texte_est(r, "erreur.message", "Ce fichier est trop gros pour l’appareil."));
    VERIFIER(b.connecter());
    VERIFIER(est_erreur(b.avec_jeton(Methode::Put, "/api/location", {}, b.corps_long(4097)), 413, "trop_gros"));
    VERIFIER(est_erreur(b.avec_jeton(Methode::Put, "/api/location", {}, b.corps_long(4096)), 400,
                        "illisible")); // 4096 passe la taille
    return 0;
}

constexpr int reponse_trop_grande_pour_le_tampon_n_est_jamais_tronquee() {
    Banc b;
    char petit[20] = {};
    api::Tampons t = b.tampons();
    t.sortie = petit;
    t.cap_sortie = sizeof petit;
    api::Requete rq;
    rq.chemin = "/api/status";
    const api::Reponse r = b.moteur.traiter(rq, t);
    VERIFIER(r.statut == 500 && r.corps.empty());
    return 0;
}

constexpr int jamais_de_secret_dans_les_reponses() {
    Banc b;
    VERIFIER(b.connecter());
    b.hote.meteo_dispo = true;
    b.hote.meteo.valide = true;
    b.hote.reponse_geocodage = GEOCODAGE_LYON;
    constexpr sv GET[] = {"/api/system",  "/api/location", "/api/display",  "/api/alerts",  "/api/sea",
                          "/api/weather", "/api/wifi",     "/api/services", "/api/presets", "/api/update"};
    for (sv chemin : GET) {
        const api::Reponse r = b.avec_jeton(Methode::Get, chemin);
        VERIFIER(r.statut == 200 && json_ok(r));
        for (sv interdit :
             {"password", "mot_de_passe", "motdepasse", "api_key", "apikey", "secret", "\"cle\"", "psk", "sk-"})
            VERIFIER(!contient(r.corps, interdit));
    }
    const api::Reponse g = b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon");
    VERIFIER(g.statut == 200 && !contient(g.corps, "password") && !contient(g.corps, "api_key"));
    return 0;
}

constexpr int toutes_les_reponses_sont_du_json_valide() {
    Banc b;
    api::Reponse r = b.appeler(Methode::Get, "/api/status");
    VERIFIER(json_ok(r));
    r = b.appeler(Methode::Get, "/api/system");
    VERIFIER(r.statut == 401 && json_ok(r));
    VERIFIER(b.connecter());
    for (const Route &route : ROUTES_NON_DISPONIBLES) {
        r = b.avec_jeton(route.m, route.chemin, {}, "{}");
        VERIFIER(json_ok(r));
    }
    r = b.avec_jeton(Methode::Get, "/api/inconnue");
    VERIFIER(json_ok(r));
    return 0;
}

// Toutes les phrases que l'API peut dire (api_reglages.hpp) : chacune est une clé de traduction avec une version dans chaque
// langue. Le contrôle croisé de test/host/controle_i18n.py vérifie que cette liste couvre bien tous les littéraux du fichier.
constexpr sv PHRASES[] = {
    "Ce fichier est trop gros pour l’appareil.",
    "Entrez le code affiché sur l’écran de l’appareil.",
    "Cette fonction n’est pas encore disponible sur cet écran.",
    "Cette page n’existe pas.",
    "L’appareil a rencontré un problème. Réessayez.",
    "Cette demande n’est pas lisible.",
    "Trop d’essais. Réessayez dans {n} secondes.",
    "Trop d’essais. Réessayez dans 1 seconde.",
    "Ce code n’est pas le bon. Regardez l’écran de l’appareil.",
    "Le nom de la ville n’est pas valide.",
    "Ces coordonnées ne sont pas valides.",
    "Ce fuseau horaire n’est pas connu de l’appareil.",
    "Cette langue n’est pas disponible pour l’écran.",
    "Cette langue n’est pas disponible pour l’assistant.",
    "Ce thème n’existe pas.",
    "Ce profil n’existe pas.",
    "La luminosité doit être entre 5 et 100 %.",
    "Cette recherche est trop longue.",
    "Tapez au moins 3 lettres.",
    "L’appareil n’a pas accès à Internet pour chercher un lieu.",
    "Non connecté.",
    "Connecté à « {ssid} ».",
    "Connexion en cours…",
    "La connexion a échoué.",
    "Les alertes doivent être activées ou désactivées.",
    "L’adresse de Vigie doit commencer par https://.",
    "La période doit être un nombre entier de minutes, de 10 à 180.",
    "Ce niveau d’alerte n’existe pas.",
    "Cette source météo n’existe pas.",
    "Cette source météo ne couvre pas le pays de la ville.",
    "Les spots doivent former une liste d’objets avec un identifiant et un nom.",
    "Cet identifiant de spot n’est pas valide.",
    "Le nom d’un spot doit faire de 1 à 32 caractères.",
    "Ce spot est déjà dans la liste.",
    "La liste est pleine : 12 spots au plus.",
    "Le spot choisi n’est pas dans la liste.",
    "La plage de vent doit être en nœuds entiers, de 1 à 60, le minimum sous le maximum.",
};

constexpr int chaque_phrase_existe_dans_les_sept_langues() {
    for (sv p : PHRASES) {
        VERIFIER(i18n::est_une_cle(p));
        VERIFIER(i18n::tr(p, i18n::Langue::Fr) == p);
        for (int l = 1; l < 7; ++l) {
            const sv traduit = i18n::tr(p, static_cast<i18n::Langue>(l));
            VERIFIER(!traduit.empty() && traduit != p);
            // les marqueurs {n} et {ssid} sont conservés
            VERIFIER(contient(p, "{n}") == contient(traduit, "{n}") &&
                     contient(p, "{ssid}") == contient(traduit, "{ssid}"));
        }
    }
    return 0;
}

constexpr int NB_SCENARIOS = 35;

// Un scénario d'erreur ou de message par numéro ; `chemin` dit où lire la phrase dans la réponse.
constexpr api::Reponse scenario(Banc &b, int k, sv &chemin) {
    chemin = "erreur.message";
    auto put = [&](sv route, sv corps) { return b.avec_jeton(Methode::Put, route, {}, corps); };
    switch (k) {
    case 0:
        return b.appeler(Methode::Get, "/api/system");
    case 1:
        return b.avec_jeton(Methode::Get, "/api/inconnue");
    case 2:
        return put("/api/wifi", "{}");
    case 3:
        return b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"000000"})");
    case 4:
        return put("/api/display", R"({"luminosite":3})");
    case 5:
        return put("/api/display", R"({"theme":"noir"})");
    case 6:
        return put("/api/display", R"({"langue_ecran":"xx"})");
    case 7:
        return put("/api/display", R"({"langue_assistant":"xx"})");
    case 8:
        return put("/api/location", R"({"nom":"","latitude":1,"longitude":1,"fuseau":"Europe/Paris"})");
    case 9:
        return put("/api/location", R"({"nom":"X","latitude":95,"longitude":1,"fuseau":"Europe/Paris"})");
    case 10:
        return put("/api/location", R"({"nom":"X","latitude":1,"longitude":1,"fuseau":"Mars/Olympus"})");
    case 11:
        return put("/api/location", "[]");
    case 12:
        return b.avec_jeton(Methode::Get, "/api/geocode", "q=ab");
    case 13:
        return b.avec_jeton(Methode::Get, "/api/geocode", b.requete_longue(81));
    case 14:
        b.hote.dl = api::Telechargement::SansInternet;
        return b.avec_jeton(Methode::Get, "/api/geocode", "q=Lyon");
    case 15:
        return b.avec_jeton(Methode::Put, "/api/location", {}, b.corps_long(4097));
    case 16:
        b.hote.zone_ecriture_ok = false;
        return put("/api/location", MARSEILLE);
    case 17:
    case 18:
    case 19:
    case 20:
        chemin = "message";
        b.hote.wifi_.etat = k == 17   ? api::EtatWifi::Connecte
                            : k == 18 ? api::EtatWifi::Connexion
                            : k == 19 ? api::EtatWifi::Echec
                                      : api::EtatWifi::Deconnecte;
        return b.avec_jeton(Methode::Get, "/api/wifi");
    case 22:
        return put("/api/location", R"({"source_meteo":"inconnue"})");
    case 23:
        return put("/api/location", R"({"source_meteo":"knmi"})"); // ne couvre pas la France de l'appareil d'essai
    case 21: { // verrou de l'appairage (un essai raté a déjà eu lieu au scénario 3)
        api::Reponse r;
        for (int i = 0; i < 6 && r.statut != 429; ++i)
            r = b.appeler(Methode::Post, "/api/pair", {}, R"({"code":"000000"})");
        return r;
    }
    case 24:
        return put("/api/alerts", R"({"actives":"oui"})");
    case 25:
        return put("/api/alerts", R"({"adresse":"http://vigie.exemple.fr"})");
    case 26:
        return put("/api/alerts", R"({"periode_min":9})");
    case 27:
        return put("/api/alerts", R"({"niveau_min":"vert"})");
    case 28:
        return put("/api/sea", R"({"spots":"x"})");
    case 29:
        return put("/api/sea", R"({"spots":[{"id":"Mauvais Id","nom":"X"}]})");
    case 30:
        return put("/api/sea", R"({"spots":[{"id":"a","nom":""}]})");
    case 31:
        return put("/api/sea", R"({"spots":[{"id":"a","nom":"A"},{"id":"a","nom":"B"}]})");
    case 32:
        return put(
            "/api/sea",
            R"({"spots":[{"id":"s01","nom":"S"},{"id":"s02","nom":"S"},{"id":"s03","nom":"S"},{"id":"s04","nom":"S"},)"
            R"({"id":"s05","nom":"S"},{"id":"s06","nom":"S"},{"id":"s07","nom":"S"},{"id":"s08","nom":"S"},)"
            R"({"id":"s09","nom":"S"},{"id":"s10","nom":"S"},{"id":"s11","nom":"S"},{"id":"s12","nom":"S"},{"id":"s13","nom":"S"}]})");
    case 33:
        return put("/api/sea", R"({"choisi":"inconnu"})");
    default: // 34
        return put("/api/sea", R"({"vent_min_kn":30,"vent_max_kn":20})");
    }
}

constexpr int chaque_message_de_l_api_est_traduit_dans_chaque_langue() {
    char francais[NB_SCENARIOS][220] = {};
    for (int l = 0; l < 7; ++l) {
        Banc b;
        b.hote.aff.langue_ecran = static_cast<i18n::Langue>(l);
        VERIFIER(b.connecter());
        for (int k = 0; k < NB_SCENARIOS; ++k) {
            sv chemin;
            const api::Reponse r = scenario(b, k, chemin);
            char message[220] = {};
            copier_message(r, chemin, message);
            VERIFIER(!sv(message).empty());
            if (l == 0) {
                for (std::size_t i = 0; i < sizeof message; ++i)
                    francais[k][i] = message[i];
            } else {
                VERIFIER(sv(message) != sv(francais[k])); // la phrase n'est plus en français
                if (k == 17)
                    VERIFIER(contient(message, "MonReseau")); // le nom du réseau est repris dans chaque langue
                if (k == 21)
                    VERIFIER(contient(message, "60"));
            }
        }
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(statut_public_sans_jeton)                                                                                        \
    X(sans_jeton_valide_tout_est_refuse_en_401)                                                                        \
    X(bon_code_donne_un_jeton_qui_ouvre_l_acces)                                                                       \
    X(mauvais_code_403_ou_demande_illisible_400)                                                                       \
    X(verrou_apres_cinq_essais_avec_compte_a_rebours)                                                                  \
    X(series_ratees_verrou_qui_double_et_code_renouvele)                                                               \
    X(nouveau_code_ferme_les_sessions_et_leve_le_verrou)                                                               \
    X(corps_lu_seulement_pour_route_publique_ou_jeton_valide)                                                          \
    X(fermer_la_session_retire_l_acces)                                                                                \
    X(systeme_decrit_l_appareil)                                                                                       \
    X(wifi_en_lecture_seule_sans_secret)                                                                               \
    X(wifi_nom_de_reseau_avec_guillemets_reste_du_json_valide)                                                         \
    X(niveaux_du_signal_wifi)                                                                                          \
    X(services_presets_et_mise_a_jour_vides_mais_sans_erreur)                                                          \
    X(fonctions_hors_perimetre_repondent_501)                                                                          \
    X(ville_lue_telle_que_stockee)                                                                                     \
    X(ville_sans_zone_enregistree)                                                                                     \
    X(ville_choisie_est_enregistree_puis_relue)                                                                        \
    X(la_reponse_montre_ce_qui_est_reellement_stocke)                                                                  \
    X(ville_refusee_ne_touche_pas_au_stockage)                                                                         \
    X(echec_d_ecriture_donne_500)                                                                                      \
    X(ville_lue_avec_la_source_meteo_et_la_liste_de_son_pays)                                                          \
    X(la_liste_distingue_un_service_lu_directement_d_un_modele_d_open_meteo)                                           \
    X(source_meteo_connue_meme_sans_ville_enregistree)                                                                 \
    X(source_choisie_seule_ne_touche_pas_a_la_ville)                                                                   \
    X(source_seule_sans_ville_enregistree_reste_possible)                                                              \
    X(source_inconnue_ou_hors_pays_est_refusee_sans_rien_changer)                                                      \
    X(ville_et_source_ensemble_la_source_est_jugee_sur_le_nouveau_pays)                                                \
    X(changer_de_ville_hors_pays_remet_automatique_et_la_reponse_le_dit)                                               \
    X(source_non_enregistree_donne_500)                                                                                \
    X(affichage_lu_avec_les_listes_de_l_appareil)                                                                      \
    X(le_profil_de_l_accueil_se_regle_et_se_relit)                                                                     \
    X(un_profil_inconnu_est_refuse_sans_rien_changer)                                                                  \
    X(affichage_modifie_champ_par_champ)                                                                               \
    X(affichage_inchange_n_ecrit_rien)                                                                                 \
    X(affichage_refuse_tout_ou_rien)                                                                                   \
    X(alertes_lues_avec_les_valeurs_par_defaut_et_les_niveaux)                                                         \
    X(alertes_modifiees_champ_par_champ)                                                                               \
    X(alertes_inchangees_n_ecrivent_rien)                                                                              \
    X(alertes_refusees_tout_ou_rien)                                                                                   \
    X(echec_d_ecriture_des_alertes_donne_500)                                                                          \
    X(mer_lue_vide_avec_ses_bornes)                                                                                    \
    X(mer_ecrite_la_liste_entiere_puis_relue_telle_que_gardee)                                                         \
    X(mer_choix_et_plage_de_vent_champ_par_champ)                                                                      \
    X(mer_sans_choisi_le_meme_spot_reste_choisi)                                                                       \
    X(mer_inchangee_n_ecrit_rien)                                                                                      \
    X(mer_refusee_tout_ou_rien_avec_le_champ_fautif)                                                                   \
    X(mer_treize_spots_trop)                                                                                           \
    X(mer_douze_spots_aux_noms_les_plus_longs_tiennent_dans_la_reponse)                                                \
    X(mer_un_nom_avec_des_guillemets_reste_du_json_valide)                                                             \
    X(echec_d_ecriture_du_registre_mer_donne_500)                                                                      \
    X(registre_mer_absent_donne_500_sans_rien_ecraser)                                                                 \
    X(meteo_absente_ou_invalide)                                                                                       \
    X(meteo_valide_avec_libelle_dans_la_langue_de_l_ecran)                                                             \
    X(la_meteo_nomme_le_service_qui_l_a_donnee_et_dit_le_repli)                                                        \
    X(recherche_de_lieu_convertie_pour_la_page)                                                                        \
    X(recherche_envoie_la_bonne_adresse)                                                                               \
    X(au_plus_huit_lieux)                                                                                              \
    X(recherche_refusee_sans_appeler_internet)                                                                         \
    X(recherche_sans_internet_ou_reponse_illisible)                                                                    \
    X(corps_de_plus_de_4_ko_refuse_en_413)                                                                             \
    X(reponse_trop_grande_pour_le_tampon_n_est_jamais_tronquee)                                                        \
    X(jamais_de_secret_dans_les_reponses)                                                                              \
    X(toutes_les_reponses_sont_du_json_valide)                                                                         \
    X(chaque_phrase_existe_dans_les_sept_langues)                                                                      \
    X(chaque_message_de_l_api_est_traduit_dans_chaque_langue)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
