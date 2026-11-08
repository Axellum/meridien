// Tests de l'HÔTE DE L'APPAREIL (ecran/hote_appareil.hpp) avec une mémoire et un appareil factices : les règles qui décident de ce que
// l'écran enregistre, relit, oublie et refuse. Exécutés à l'exécution seulement (l'hôte est une classe à méthodes virtuelles :
// pas évaluable à la compilation en C++17, contrairement au reste du noyau) ; le mode « syntaxe » de lancer.py ne les exécute donc pas.
//
// Ce que ces tests prouvent : l'écran écrit langue, thème, luminosité et ville par la MÊME porte que la page web et ne garde en
// double aucune de ces valeurs ; une luminosité qui éteindrait l'écran est refusée ; un secret ne ressort jamais ; un réseau Wi-Fi
// n'est retenu que si la connexion a réussi (ni refus, ni abandon, ni réponse tardive) ; un octet d'installation abîmé ne passe pas
// pour « terminée » ; la recherche de lieu ne part pas pour une saisie trop courte et libère sa réponse ; « réinitialiser » efface
// ce que l'hôte garde ; un test de service ne se déclare jamais réussi sans avoir eu lieu.
// Ce qu'ils NE prouvent PAS : que la NVS, le Wi-Fi du C6 ou le serveur de géocodage se comportent ainsi (c'est main/hote_appareil_esp.cpp,
// à constater sur la carte), ni qu'une clé reste lisible après une mise à jour du firmware.
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ecran/hote_appareil.hpp"
#include "hote_appareil_faux.hpp"
#include "mini.hpp"

using namespace ecran;
using namespace ecran::faux;
using sv = std::string_view;

namespace {

struct Banc {
    MagasinFaux magasin;
    PortsFaux ports;
    HoteAppareil hote{ports, magasin};
    // Un redémarrage : même mémoire permanente et mêmes réglages de la page, hôte neuf.
    HoteAppareil redemarre() { return HoteAppareil(ports, magasin); }
};

constexpr sv GEOCODAGE_LYON = R"({"results":[
  {"id":2996944,"name":"Lyon","latitude":45.74846,"longitude":4.84671,"country_code":"FR","timezone":"Europe/Paris",
   "country":"France","admin1":"Auvergne-Rhône-Alpes"},
  {"id":4262045,"name":"Lyon","latitude":37.69,"longitude":-85.1,"country_code":"US","timezone":"America/Chicago",
   "country":"United States","admin1":"Kentucky"},
  {"id":1,"name":"Sans fuseau","latitude":1.0,"longitude":2.0}]})";

// ---- affichage et ville : la source de la page web ----------------------------------------------------------------------

int affichage_passe_par_la_porte_de_la_page_web() {
    Banc b;
    b.hote.definir_nombre("langue.ecran", 2); // Deutsch
    b.hote.definir_nombre("langue.assistant", 1);
    b.hote.definir_nombre("affichage.theme", 2);
    b.hote.definir_nombre("affichage.luminosite", 40);
    VERIFIER(b.ports.enregistrements == 4);
    VERIFIER(b.ports.etat_affichage.langue_ecran == i18n::Langue::De &&
             b.ports.etat_affichage.langue_assistant == i18n::Langue::En);
    VERIFIER(b.ports.etat_affichage.theme == reglages::Theme::Sombre && b.ports.etat_affichage.luminosite == 40);
    VERIFIER(b.hote.nombre("langue.ecran") == 2 && b.hote.nombre("langue.assistant") == 1);
    VERIFIER(b.hote.nombre("affichage.theme") == 2 && b.hote.nombre("affichage.luminosite") == 40);
    // Un changement fait ailleurs (la page web) se voit tout de suite : l'hôte n'en garde aucune copie.
    b.ports.etat_affichage.luminosite = 90;
    b.ports.etat_affichage.langue_ecran = i18n::Langue::Tr;
    VERIFIER(b.hote.nombre("affichage.luminosite") == 90 &&
             b.hote.nombre("langue.ecran") == i18n::indice(i18n::Langue::Tr));
    // Et il ne double rien dans sa propre mémoire.
    VERIFIER(b.magasin.nombres.empty() && b.magasin.textes.empty());
    return 0;
}

int un_seul_champ_change_a_la_fois() {
    Banc b;
    b.ports.etat_affichage = {i18n::Langue::Nl, i18n::Langue::Es, reglages::Theme::Clair, 60};
    b.hote.definir_nombre("affichage.luminosite", 30);
    VERIFIER(b.ports.etat_affichage.langue_ecran == i18n::Langue::Nl &&
             b.ports.etat_affichage.langue_assistant == i18n::Langue::Es);
    VERIFIER(b.ports.etat_affichage.theme == reglages::Theme::Clair && b.ports.etat_affichage.luminosite == 30);
    b.hote.definir_nombre("affichage.theme", 1);
    VERIFIER(b.ports.etat_affichage.luminosite == 30 && b.ports.etat_affichage.theme == reglages::Theme::Clair);
    return 0;
}

int luminosite_qui_eteindrait_l_ecran_est_refusee() {
    Banc b;
    b.ports.etat_affichage.luminosite = 60;
    for (int v : {0, 1, 4, -1, 101, 255, 100000}) {
        b.hote.definir_nombre("affichage.luminosite", v);
    }
    // L'hôte refuse lui-même : il ne compte pas sur la porte de la page web pour le faire (deux garde-fous valent mieux qu'un).
    VERIFIER(b.ports.tentatives == 0 && b.ports.enregistrements == 0 && b.ports.etat_affichage.luminosite == 60);
    b.hote.definir_nombre("affichage.luminosite", 5);
    VERIFIER(b.ports.etat_affichage.luminosite == 5);
    b.hote.definir_nombre("affichage.luminosite", 100);
    VERIFIER(b.ports.etat_affichage.luminosite == 100 && b.ports.enregistrements == 2);
    return 0;
}

int choix_hors_liste_ou_inconnu_est_refuse() {
    Banc b;
    b.hote.definir_nombre("langue.ecran", 7); // il n'y a que sept langues : 0 à 6
    b.hote.definir_nombre("langue.ecran", -1);
    b.hote.definir_nombre("affichage.theme", 3);
    b.hote.definir_nombre("stt.preset", 3);
    b.hote.definir_nombre("reglage.inconnu", 1);
    b.hote.definir_nombre("wifi.ssid", 1); // un texte n'est pas un nombre
    VERIFIER(b.ports.enregistrements == 0 && b.magasin.nombres.empty());
    VERIFIER(b.hote.nombre("reglage.inconnu") == 0);
    // « pas encore choisi » (-1) n'existe que pour un réglage facultatif.
    for (std::size_t i = 0; i < registre::NOMBRE_REGLAGES; ++i) {
        const registre::Reglage &r = registre::REGISTRE[i];
        if (r.type != registre::Type::Choix || r.id.substr(0, 4) == "lang" || r.id.substr(0, 4) == "affi") {
            continue;
        }
        MagasinFaux m;
        PortsFaux p;
        HoteAppareil h(p, m);
        h.definir_nombre(r.id, -1);
        VERIFIER(m.nombres.empty() == !r.facultatif);
        h.definir_nombre(r.id, static_cast<std::int32_t>(r.nb_options));
        VERIFIER(m.nombres.size() == (r.facultatif ? 1u : 0u));
    }
    return 0;
}

int ville_affichee_est_celle_de_la_page_web() {
    Banc b;
    char t[96];
    VERIFIER(b.hote.texte("lieu.ville", t, sizeof t) == sv("Lyon, Auvergne-Rhône-Alpes, France").size());
    VERIFIER(sv(t) == "Lyon, Auvergne-Rhône-Alpes, France");
    b.ports.zone.region[0] = '\0';
    b.ports.zone.pays[0] = '\0'; // ville enregistrée sans complément (par un autre chemin)
    VERIFIER(b.hote.texte("lieu.ville", t, sizeof t) == 4 && sv(t) == "Lyon");
    b.ports.zone_connue = false;
    VERIFIER(b.hote.texte("lieu.ville", t, sizeof t) == 0 && t[0] == '\0');
    return 0;
}

// ---- services et secrets --------------------------------------------------------------------------------------------------

int services_gardes_et_relus_apres_un_redemarrage() {
    Banc b;
    b.hote.definir_nombre("llm.preset", 3);
    b.hote.definir_texte("llm.url", "http://192.168.1.20:11434/v1");
    b.hote.definir_texte("llm.modele", "mistral-small");
    b.hote.definir_texte("tts.voix", "ff_siwis");
    HoteAppareil apres = b.redemarre();
    char t[registre::URL_MAX * 4 + 1];
    VERIFIER(apres.nombre("llm.preset") == 3);
    VERIFIER(apres.texte("llm.url", t, sizeof t) == 28 && sv(t) == "http://192.168.1.20:11434/v1");
    VERIFIER(apres.texte("llm.modele", t, sizeof t) > 0 && sv(t) == "mistral-small");
    VERIFIER(apres.texte("tts.voix", t, sizeof t) > 0 && sv(t) == "ff_siwis");
    VERIFIER(apres.texte("stt.url", t, sizeof t) == 0 && t[0] == '\0'); // jamais réglé : vide, pas une valeur inventée
    VERIFIER(apres.nombre("stt.preset") == registre::chercher("stt.preset")->defaut);
    return 0;
}

int un_texte_trop_petit_pour_la_valeur_n_est_jamais_rendu_a_moitie() {
    Banc b;
    b.hote.definir_texte("llm.modele", "mistral-small");
    char court[6];
    VERIFIER(b.hote.texte("llm.modele", court, sizeof court) == 0 && court[0] == '\0');
    VERIFIER(b.hote.texte("llm.modele", nullptr, 10) == 0 && b.hote.texte("llm.modele", court, 0) == 0);
    return 0;
}

int un_secret_ne_ressort_jamais() {
    Banc b;
    b.hote.definir_texte("llm.cle", "sk-test-0123456789");
    b.hote.definir_texte("wifi.mot_de_passe", "motdepasse-wifi");
    char t[300];
    std::memset(t, 'x', sizeof t);
    VERIFIER(b.hote.texte("llm.cle", t, sizeof t) == 0 && t[0] == '\0');
    std::memset(t, 'x', sizeof t);
    VERIFIER(b.hote.texte("wifi.mot_de_passe", t, sizeof t) == 0 && t[0] == '\0');
    VERIFIER(b.hote.secret_defini("llm.cle") && b.hote.secret_defini("wifi.mot_de_passe"));
    VERIFIER(!b.hote.secret_defini("stt.cle") &&
             !b.hote.secret_defini("llm.url")); // « jamais défini » et « pas un secret »
    b.hote.effacer_secret("llm.cle");
    VERIFIER(!b.hote.secret_defini("llm.cle") && b.hote.secret_defini("wifi.mot_de_passe"));
    b.hote.effacer_secret("llm.url"); // effacer un texte ordinaire par cette porte est sans effet
    b.hote.definir_texte("llm.url", "http://x.y");
    b.hote.effacer_secret("llm.url");
    VERIFIER(b.magasin.longueur_texte("services", "llm.url") == 10);
    return 0;
}

int texte_refuse_par_le_registre_n_est_jamais_enregistre() {
    Banc b;
    b.hote.definir_texte("wifi.mot_de_passe", "court");      // moins de 8 caractères
    b.hote.definir_texte("wifi.ssid", std::string(33, 'a')); // plus de 32 octets
    b.hote.definir_texte("llm.url", std::string(201, 'a'));  // plus de 200 caractères
    b.hote.definir_texte("llm.modele", "mo\ndele");          // octet de contrôle
    b.hote.definir_texte("stt.preset", "openai");            // un choix n'est pas un texte
    b.hote.definir_texte("lieu.ville", "Lyon");              // la ville se choisit dans une liste
    b.hote.definir_texte("affichage.luminosite", "50");
    b.hote.definir_texte("inconnu", "x");
    VERIFIER(b.magasin.textes.empty() && b.magasin.nombres.empty());
    b.hote.definir_texte("wifi.mot_de_passe", "huitcars"); // 8 : la limite basse passe
    b.hote.definir_texte("wifi.mot_de_passe", "");         // vide : réseau ouvert, permis
    VERIFIER(!b.hote.secret_defini("wifi.mot_de_passe"));
    VERIFIER(b.magasin.textes.count("wifi/mdp") == 1);
    return 0;
}

// ---- Wi-Fi ---------------------------------------------------------------------------------------------------------------

int reseaux_vus_en_barres_et_au_plus_seize() {
    Banc b;
    b.ports.reseaux = {point("Maison", -50, true), point("Voisin", -70, true), point("Loin", -90, false)};
    ReseauWifi r[kReseauxMax];
    VERIFIER(b.hote.wifi_reseaux(r, kReseauxMax) == 3);
    VERIFIER(sv(r[0].ssid) == "Maison" && r[0].niveau == 4 && r[0].securise);
    VERIFIER(r[1].niveau == 2 && r[2].niveau == 0 && !r[2].securise);
    b.ports.reseaux.clear();
    for (int i = 0; i < 20; ++i) {
        b.ports.reseaux.push_back(point(("Reseau" + std::to_string(i)).c_str(), -60, true));
    }
    VERIFIER(b.hote.wifi_reseaux(r, kReseauxMax) == kReseauxMax);
    VERIFIER(b.hote.wifi_reseaux(r, 5) == 5);
    b.hote.wifi_chercher();
    b.ports.scan_en_cours = true;
    VERIFIER(b.ports.scans == 1 && b.hote.wifi_recherche_en_cours());
    return 0;
}

int reseau_retenu_seulement_apres_une_connexion_reussie() {
    Banc b;
    b.hote.wifi_connecter("Maison_5G", "motdepasse1");
    VERIFIER(b.ports.dernier_ssid == "Maison_5G" && b.ports.dernier_mdp == "motdepasse1" && b.ports.connexions == 1);
    VERIFIER(b.hote.wifi_connexion().etat == wifi::Etat::Connexion);
    VERIFIER(b.magasin.vide_dans("wifi")); // essai en cours : rien n'est encore retenu
    b.ports.repondre_wifi(wifi::Resultat::MotDePasseRefuse);
    ConnexionWifi c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Echec && c.cause == wifi::Cause::MotDePasse && b.magasin.vide_dans("wifi"));
    b.hote.wifi_connecter("Maison_5G", "motdepasse2");
    b.ports.repondre_wifi(wifi::Resultat::ReseauIntrouvable);
    c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Echec && c.cause == wifi::Cause::Introuvable && b.magasin.vide_dans("wifi"));
    b.hote.wifi_connecter("Maison_5G", "motdepasse3");
    b.ports.repondre_wifi(wifi::Resultat::Autre);
    c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Echec && c.cause == wifi::Cause::Autre && b.magasin.vide_dans("wifi"));
    b.hote.wifi_connecter("Maison_5G", "motdepasse4");
    b.ports.repondre_wifi(wifi::Resultat::Connecte);
    c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Reussi && c.cause == wifi::Cause::Aucune);
    VERIFIER(b.magasin.textes["wifi/ssid"] == "Maison_5G" && b.magasin.textes["wifi/mdp"] == "motdepasse4");
    VERIFIER(b.hote.wifi_connexion().etat == wifi::Etat::Reussi); // le résultat reste lisible
    return 0;
}

int abandon_ne_retient_rien_meme_si_la_reponse_arrive_apres() {
    Banc b;
    b.hote.wifi_connecter("Maison_5G", "motdepasse1");
    b.hote.wifi_abandonner();
    VERIFIER(b.ports.abandons == 1);
    ConnexionWifi c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Echec && c.cause == wifi::Cause::Delai);
    b.ports.repondre_wifi(wifi::Resultat::Connecte); // réponse tardive : la couche réseau ne devrait plus la donner
    c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Echec && c.cause == wifi::Cause::Delai && b.magasin.vide_dans("wifi"));
    // Abandonner sans essai en cours ne fabrique pas d'échec.
    Banc autre;
    autre.hote.wifi_abandonner();
    VERIFIER(autre.hote.wifi_connexion().etat == wifi::Etat::Inactif);
    return 0;
}

int essai_impossible_a_demarrer_est_un_echec_tout_de_suite() {
    Banc b;
    b.ports.accepte_connexion = false;
    b.hote.wifi_connecter("Maison_5G", "motdepasse1");
    const ConnexionWifi c = b.hote.wifi_connexion();
    VERIFIER(c.etat == wifi::Etat::Echec && c.cause == wifi::Cause::Autre && b.magasin.vide_dans("wifi"));
    return 0;
}

int reseau_ouvert_se_retient_sans_mot_de_passe() {
    Banc b;
    b.hote.wifi_connecter("Cafe", "");
    VERIFIER(b.ports.dernier_mdp.empty());
    b.ports.repondre_wifi(wifi::Resultat::Connecte);
    VERIFIER(b.hote.wifi_connexion().etat == wifi::Etat::Reussi);
    VERIFIER(b.magasin.textes["wifi/ssid"] == "Cafe" && b.magasin.textes["wifi/mdp"].empty());
    VERIFIER(!b.hote.secret_defini("wifi.mot_de_passe")); // vide : pas de mot de passe
    return 0;
}

// ---- recherche de lieu ----------------------------------------------------------------------------------------------------

int recherche_de_lieu_du_depart_au_choix() {
    Banc b;
    b.hote.lieu_chercher("Lyon");
    VERIFIER(b.ports.telechargements == 1 && b.hote.lieu_recherche() == RechercheLieu::EnCours);
    VERIFIER(b.ports.derniere_adresse.find("name=Lyon") != std::string::npos);
    VERIFIER(b.ports.derniere_adresse.find("language=fr") != std::string::npos);
    b.ports.dl = EtatTelechargement::EnCours;
    VERIFIER(b.hote.lieu_recherche() == RechercheLieu::EnCours);
    const int avant = b.ports.liberations;
    b.ports.corps = std::string(GEOCODAGE_LYON);
    b.ports.dl = EtatTelechargement::Termine;
    VERIFIER(b.hote.lieu_recherche() == RechercheLieu::Terminee);
    VERIFIER(b.ports.liberations == avant + 1); // la réponse est rendue dès qu'elle est lue
    LieuTrouve l[kLieuxMax];
    VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 2);
    VERIFIER(sv(l[0].libelle) == "Lyon, Auvergne-Rhône-Alpes, France" &&
             sv(l[1].libelle) == "Lyon, Kentucky, United States");
    VERIFIER(b.hote.lieu_resultats(l, 1) == 1);
    b.hote.lieu_choisir(1);
    VERIFIER(b.ports.zones_enregistrees == 1 && sv(b.ports.zone.region) == "Kentucky" &&
             sv(b.ports.zone.fuseau) == "America/Chicago");
    VERIFIER(b.ports.zone.longitude < -85.0);
    char t[96];
    b.hote.texte("lieu.ville", t, sizeof t);
    VERIFIER(sv(t) == "Lyon, Kentucky, United States"); // l'écran relit la ville de la page web
    return 0;
}

int choisir_un_rang_qui_n_existe_pas_ne_change_rien() {
    Banc b;
    b.hote.lieu_chercher("Lyon");
    b.ports.corps = std::string(GEOCODAGE_LYON);
    b.ports.dl = EtatTelechargement::Termine;
    b.hote.lieu_recherche();
    b.hote.lieu_choisir(2); // il y a deux lieux : 0 et 1
    b.hote.lieu_choisir(7);
    b.hote.lieu_choisir(static_cast<std::size_t>(-1));
    VERIFIER(b.ports.zones_enregistrees == 0);
    return 0;
}

int saisie_trop_courte_ou_illisible_ne_telecharge_rien() {
    Banc b;
    for (sv q : {sv(""), sv("ab"), sv("  a "), sv("Ly\non"), sv("Z\xFF\xFF")}) {
        b.hote.lieu_chercher(q);
        VERIFIER(b.hote.lieu_recherche() == RechercheLieu::Terminee);
        LieuTrouve l[kLieuxMax];
        VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 0);
    }
    VERIFIER(b.ports.telechargements == 0);
    b.hote.lieu_chercher(std::string(81, 'a')); // 81 caractères : trop long, comme pour la page
    VERIFIER(b.ports.telechargements == 0);
    return 0;
}

int recherche_sans_reponse_dit_sans_connexion_et_rend_la_memoire() {
    for (EtatTelechargement e :
         {EtatTelechargement::SansInternet, EtatTelechargement::Echec, EtatTelechargement::Inactif}) {
        Banc b;
        b.hote.lieu_chercher("Lyon");
        b.ports.dl = e;
        VERIFIER(b.hote.lieu_recherche() == RechercheLieu::SansConnexion);
        VERIFIER(b.ports.liberations >= 2); // une avant de partir, une à l'arrivée
        LieuTrouve l[kLieuxMax];
        VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 0);
    }
    return 0;
}

int reponse_illisible_donne_zero_lieu_sans_erreur() {
    Banc b;
    b.hote.lieu_chercher("Lyon");
    b.ports.corps = "<html>502</html>";
    b.ports.dl = EtatTelechargement::Termine;
    VERIFIER(b.hote.lieu_recherche() == RechercheLieu::Terminee);
    LieuTrouve l[kLieuxMax];
    VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 0);
    return 0;
}

int nouvelle_recherche_efface_l_ancienne() {
    Banc b;
    b.hote.lieu_chercher("Lyon");
    b.ports.corps = std::string(GEOCODAGE_LYON);
    b.ports.dl = EtatTelechargement::Termine;
    b.hote.lieu_recherche();
    LieuTrouve l[kLieuxMax];
    VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 2);
    b.hote.lieu_chercher("Pari");
    b.ports.dl = EtatTelechargement::EnCours;
    VERIFIER(b.hote.lieu_recherche() == RechercheLieu::EnCours);
    VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 0); // pas de lieux de l'ancienne recherche pendant la nouvelle
    b.hote.lieu_choisir(0);
    VERIFIER(b.ports.zones_enregistrees == 0);
    return 0;
}

int langue_de_l_ecran_dicte_celle_de_la_recherche() {
    Banc b;
    b.ports.etat_affichage.langue_ecran = i18n::Langue::Tr;
    b.hote.lieu_chercher("Istanbul");
    VERIFIER(b.ports.derniere_adresse.find("language=tr") != std::string::npos);
    return 0;
}

// ---- source de la météo ---------------------------------------------------------------------------------------------------

// Choisit dans GEOCODAGE_LYON la ville de ce rang : 0 = Lyon (France), 1 = Lyon (Kentucky, États-Unis).
void choisir_ville(Banc &b, std::size_t rang) {
    b.hote.lieu_chercher("Lyon");
    b.ports.corps = std::string(GEOCODAGE_LYON);
    b.ports.dl = EtatTelechargement::Termine;
    b.hote.lieu_recherche();
    b.hote.lieu_choisir(rang);
}

void fixer_pays(Banc &b, sv code) {
    reglages::copier_texte(code, b.ports.zone.code_pays, sizeof b.ports.zone.code_pays);
}

std::size_t position(sv id) { return meteo::fournisseurs::indice_de(id); }

// Les identifiants de la liste proposée, séparés par une espace (« auto knmi noaa »).
std::string propose(Banc &b) {
    const meteo::fournisseurs::Liste l = b.hote.sources_meteo();
    std::string s;
    for (std::size_t i = 0; i < l.n; ++i) {
        s += (i != 0 ? " " : "");
        s += meteo::fournisseurs::TABLE[l.indices[i]].id;
    }
    return s;
}

int source_meteo_automatique_au_depart_et_liste_selon_le_pays() {
    Banc b;
    VERIFIER(b.hote.nombre("meteo.source") == 0 && b.ports.source == "auto");
    VERIFIER(propose(b) == "auto noaa"); // ville sans pays connu : « Automatique » et le modèle mondial seulement
    fixer_pays(b, "NL");
    VERIFIER(propose(b) == "auto knmi noaa"); // le modèle national d'abord, puis le mondial
    fixer_pays(b, "NO");
    VERIFIER(propose(b) == "auto metno metnorway noaa");
    fixer_pays(b, "FR");
    VERIFIER(propose(b) == "auto noaa");
    fixer_pays(
        b,
        "nl"); // la casse ne compte pas (le géocodage rend des majuscules, une ancienne mémoire peut avoir des minuscules)
    VERIFIER(propose(b) == "auto knmi noaa");
    fixer_pays(b, "NLD"); // trois lettres : pas un code de pays, donc pas la liste d'un autre
    VERIFIER(propose(b) == "auto noaa");
    b.ports.zone_connue = false; // la ville n'est pas lisible : on ne devine pas
    VERIFIER(propose(b) == "auto noaa");
    return 0;
}

int choisir_une_source_la_garde_par_la_porte_de_la_page_web() {
    Banc b;
    fixer_pays(b, "NL");
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("knmi")));
    VERIFIER(b.ports.sources_enregistrees == 1 && b.ports.source == "knmi");
    VERIFIER(b.hote.nombre("meteo.source") == static_cast<std::int32_t>(position("knmi")));
    VERIFIER(b.magasin.nombres.empty() && b.magasin.textes.empty()); // l'hôte n'en garde aucune copie
    HoteAppareil apres = b.redemarre(); // un redémarrage : la source est toujours celle de la page
    VERIFIER(apres.nombre("meteo.source") == static_cast<std::int32_t>(position("knmi")));
    b.hote.definir_nombre("meteo.source", 0); // retour à « Automatique »
    VERIFIER(b.ports.source == "auto" && b.ports.sources_enregistrees == 2 && b.hote.nombre("meteo.source") == 0);
    return 0;
}

int une_source_qui_ne_couvre_pas_le_pays_ou_hors_liste_est_refusee() {
    Banc b;
    fixer_pays(b, "NL");
    b.hote.definir_nombre("meteo.source",
                          static_cast<std::int32_t>(position("metno"))); // norvégien, ville néerlandaise
    b.hote.definir_nombre("meteo.source",
                          static_cast<std::int32_t>(position("dwd"))); // allemand, et que le lecteur refuse
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("meteofrance")));
    b.hote.definir_nombre("meteo.source", -1); // « pas choisi » : « Automatique » est obligatoire, pas de vide
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(meteo::fournisseurs::NOMBRE));
    b.hote.definir_nombre("meteo.source", 1000000);
    VERIFIER(b.ports.sources_enregistrees == 0 && b.ports.source == "auto");
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("noaa"))); // mondial : proposé partout
    VERIFIER(b.ports.sources_enregistrees == 1 && b.ports.source == "noaa");
    return 0;
}

int une_ecriture_refusee_ne_change_pas_la_source() {
    Banc b;
    fixer_pays(b, "NL");
    b.ports.source_refusee = true; // la mémoire permanente ne répond pas
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("knmi")));
    VERIFIER(b.ports.source == "auto" && b.hote.nombre("meteo.source") == 0);
    return 0;
}

int une_source_inconnue_en_memoire_se_lit_automatique() {
    Banc b;
    b.ports.source = "modele_d_un_autre_firmware";
    VERIFIER(b.hote.nombre("meteo.source") == 0);
    b.ports.source = "";
    VERIFIER(b.hote.nombre("meteo.source") == 0);
    return 0;
}

int changer_de_ville_hors_pays_remet_automatique_et_le_dit() {
    Banc b;
    fixer_pays(b, "NL");
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("knmi")));
    VERIFIER(!b.hote.source_meteo_retablie());
    choisir_ville(b, 0); // Lyon, France : KNMI ne couvre pas la France
    VERIFIER(b.ports.source == "auto" && b.hote.nombre("meteo.source") == 0);
    VERIFIER(b.hote.source_meteo_retablie()); // l'écran le dit : jamais un retour muet
    // La liste proposée est déjà celle du nouveau pays.
    VERIFIER(propose(b) == "auto noaa");
    // Le message s'efface dès que la source est choisie...
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("noaa")));
    VERIFIER(!b.hote.source_meteo_retablie() && b.ports.source == "noaa");
    // ... ou que la ville change encore.
    fixer_pays(b, "NL");
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("knmi")));
    choisir_ville(b, 0);
    VERIFIER(b.hote.source_meteo_retablie());
    choisir_ville(b, 1);
    VERIFIER(!b.hote.source_meteo_retablie());
    return 0;
}

int changer_de_ville_garde_la_source_quand_elle_convient_encore() {
    Banc b;
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("noaa"))); // mondiale
    choisir_ville(b, 0);
    VERIFIER(b.ports.source == "noaa" && !b.hote.source_meteo_retablie());
    choisir_ville(b, 1);
    VERIFIER(b.ports.source == "noaa" && !b.hote.source_meteo_retablie());
    // « Automatique » ne se « rétablit » jamais : il n'y a rien à dire.
    b.hote.definir_nombre("meteo.source", 0);
    choisir_ville(b, 0);
    VERIFIER(b.ports.source == "auto" && !b.hote.source_meteo_retablie());
    return 0;
}

// ---- météo, appareil, test de service -------------------------------------------------------------------------------------

int apercu_meteo_dans_la_langue_de_l_ecran() {
    Banc b;
    VERIFIER(!b.hote.meteo_apercu().disponible);
    b.ports.meteo_vue = {true, 17.5F, 0, true};
    ApercuMeteo a = b.hote.meteo_apercu();
    VERIFIER(a.disponible && a.temperature_c > 17.4F && a.temperature_c < 17.6F && a.libelle[0] != '\0');
    const std::string fr = a.libelle;
    i18n::definir_langue(i18n::Langue::En);
    a = b.hote.meteo_apercu();
    i18n::definir_langue(i18n::Langue::Fr);
    VERIFIER(std::string(a.libelle) != fr && a.libelle[0] != '\0'); // la phrase suit la langue de l'écran
    return 0;
}

int test_de_service_ne_se_dit_jamais_reussi_sans_avoir_eu_lieu() {
    Banc b;
    for (sv s : kServices) {
        VERIFIER(b.hote.resultat_test(s).etat == EtatTest::Inactif);
    }
    b.hote.tester_service("llm");
    VERIFIER(b.hote.resultat_test("llm").etat == EtatTest::Echec && b.hote.resultat_test("llm").message[0] != '\0');
    VERIFIER(b.hote.resultat_test("stt").etat == EtatTest::Inactif &&
             b.hote.resultat_test("tts").etat == EtatTest::Inactif);
    b.hote.tester_service("inconnu");
    VERIFIER(b.hote.resultat_test("inconnu").etat == EtatTest::Inactif);
    return 0;
}

int informations_de_l_appareil() {
    Banc b;
    InfoAppareil i = b.hote.info_appareil();
    VERIFIER(sv(i.version) == "0.6.0" && sv(i.code) == "482913" &&
             i.adresse[0] == '\0'); // pas de Wi-Fi : pas d'adresse
    b.ports.info_wifi.connecte = true;
    std::snprintf(b.ports.info_wifi.ip, sizeof b.ports.info_wifi.ip, "%s", "192.168.0.31");
    i = b.hote.info_appareil();
    VERIFIER(sv(i.adresse) == "http://192.168.0.31");
    b.ports.info_wifi.connecte = false; // une adresse gardée d'avant la coupure ne s'affiche pas
    VERIFIER(b.hote.info_appareil().adresse[0] == '\0');
    b.ports.code = "";
    VERIFIER(b.hote.info_appareil().code[0] == '\0'); // la page n'est pas encore servie
    b.hote.nouveau_code();
    b.hote.redemarrer();
    VERIFIER(b.ports.nouveaux_codes == 1 && b.ports.redemarrages == 1);
    VERIFIER(b.hote.maintenant_ms() == 1000);
    return 0;
}

// ---- installation guidée --------------------------------------------------------------------------------------------------

int progression_se_reprend_apres_un_redemarrage() {
    Banc b;
    VERIFIER(b.hote.progression() == installation::Progression{}); // carte neuve : première installation, étape 1
    for (registre::Etape e : {registre::Etape::Wifi, registre::Etape::Lieu, registre::Etape::Recapitulatif}) {
        for (installation::Mode m : {installation::Mode::Premiere, installation::Mode::Refaire}) {
            const installation::Progression p{e, false, m};
            b.hote.definir_progression(p);
            HoteAppareil apres = b.redemarre();
            VERIFIER(apres.progression() == p);
        }
    }
    const installation::Progression fin =
        installation::terminer({registre::Etape::Recapitulatif, false, installation::Mode::Premiere});
    b.hote.definir_progression(fin);
    HoteAppareil apres = b.redemarre();
    VERIFIER(apres.progression().terminee && !installation::installation_a_montrer(apres.progression()));
    VERIFIER(b.magasin.nombres.size() == 1); // UN seul octet en mémoire permanente
    return 0;
}

int octet_d_installation_abime_ne_passe_pas_pour_terminee() {
    Banc b;
    for (std::int32_t v : {0, 9, 15, 31 + 1, 99, -1, -5, 0x7FFFFFFF, 0x0F}) {
        b.magasin.nombres["installation/etape"] = v;
        const installation::Progression p = b.hote.progression();
        VERIFIER(!p.terminee);
        VERIFIER(installation::installation_a_montrer(p));
        VERIFIER(p == installation::Progression{});
    }
    b.magasin.nombres["installation/etape"] = 8; // « terminée » écrit par nous : celle-là est crue
    VERIFIER(b.hote.progression().terminee);
    return 0;
}

int reinitialiser_efface_ce_que_l_hote_garde() {
    Banc b;
    b.hote.definir_nombre("stt.preset", 1);
    b.hote.definir_texte("stt.url", "http://x.y/z");
    b.hote.definir_texte("llm.cle", "sk-test-0123456789");
    b.hote.wifi_connecter("Maison_5G", "motdepasse1");
    b.ports.repondre_wifi(wifi::Resultat::Connecte);
    b.hote.wifi_connexion();
    b.hote.definir_progression(
        installation::terminer({registre::Etape::Recapitulatif, false, installation::Mode::Premiere}));
    b.hote.definir_nombre("affichage.luminosite", 20);
    fixer_pays(b, "NL");
    b.hote.definir_nombre("meteo.source", static_cast<std::int32_t>(position("knmi")));
    b.hote.tester_service("llm");
    b.hote.lieu_chercher("Lyon");
    VERIFIER(!b.magasin.vide_dans("services") && !b.magasin.vide_dans("wifi"));
    b.hote.reinitialiser();
    VERIFIER(b.ports.source == "auto" && b.hote.nombre("meteo.source") == 0 && !b.hote.source_meteo_retablie());
    VERIFIER(b.magasin.vide_dans("services") && b.magasin.vide_dans("wifi"));
    VERIFIER(b.ports.remises_par_defaut == 1 && b.ports.etat_affichage.luminosite == reglages::LUMINOSITE_DEFAUT);
    VERIFIER(b.hote.progression() == installation::apres_reinitialisation() && !b.hote.progression().terminee);
    VERIFIER(!b.hote.secret_defini("llm.cle") &&
             b.hote.nombre("stt.preset") == registre::chercher("stt.preset")->defaut);
    VERIFIER(b.hote.resultat_test("llm").etat == EtatTest::Inactif);
    VERIFIER(b.hote.lieu_recherche() == RechercheLieu::Inactive);
    LieuTrouve l[kLieuxMax];
    VERIFIER(b.hote.lieu_resultats(l, kLieuxMax) == 0);
    return 0;
}

int valeur_abimee_en_memoire_donne_le_defaut() {
    Banc b;
    const std::int32_t defaut = registre::chercher("stt.preset")->defaut;
    for (std::int32_t v : {99, -7, 3, 1000000}) { // « stt.preset » n'a que trois options (0 à 2)
        b.magasin.nombres["services/stt.preset"] = v;
        VERIFIER(b.hote.nombre("stt.preset") == defaut);
    }
    b.magasin.nombres["services/stt.preset"] = 2;
    VERIFIER(b.hote.nombre("stt.preset") == 2);
    return 0;
}

// ---- alertes officielles : la source de la page web ----------------------------------------------------------------------

int alertes_passent_par_la_porte_de_la_page_web() {
    Banc b;
    VERIFIER(b.hote.nombre("alertes.actives") == 0 && b.hote.nombre("alertes.niveau") == 0); // Oui, jaune
    VERIFIER(b.hote.nombre("alertes.periode") == 15);
    b.hote.definir_nombre("alertes.actives", 1); // Non
    VERIFIER(!b.ports.etat_alertes.actives && b.ports.enregistrements_alertes == 1);
    b.hote.definir_nombre("alertes.niveau", 2); // rouge
    VERIFIER(b.ports.etat_alertes.niveau_min == vigie::NiveauMin::Rouge && !b.ports.etat_alertes.actives);
    b.hote.definir_nombre("alertes.periode", 45);
    VERIFIER(b.ports.etat_alertes.periode_min == 45 && b.ports.etat_alertes.niveau_min == vigie::NiveauMin::Rouge);
    VERIFIER(b.hote.nombre("alertes.actives") == 1 && b.hote.nombre("alertes.niveau") == 2);
    VERIFIER(b.hote.nombre("alertes.periode") == 45 && b.ports.enregistrements_alertes == 3);
    b.hote.definir_nombre("alertes.actives", 0);
    VERIFIER(b.ports.etat_alertes.actives && b.hote.nombre("alertes.actives") == 0);
    // Un changement fait ailleurs (la page web) se voit tout de suite : l'hôte n'en garde aucune copie.
    b.ports.etat_alertes.periode_min = 90;
    b.ports.etat_alertes.niveau_min = vigie::NiveauMin::Orange;
    VERIFIER(b.hote.nombre("alertes.periode") == 90 && b.hote.nombre("alertes.niveau") == 1);
    // Rien de ces réglages ne va dans la mémoire permanente de l'hôte : la source est une seule.
    VERIFIER(b.magasin.nombres.empty() && b.magasin.textes.empty());
    return 0;
}

int alertes_hors_plage_ou_hors_liste_sont_refusees() {
    Banc b;
    for (std::int32_t v : {-1, 2, 99})
        b.hote.definir_nombre("alertes.actives", v); // deux options : 0 et 1 ; « pas choisi » n'existe pas ici
    for (std::int32_t v : {-1, 3, 99})
        b.hote.definir_nombre("alertes.niveau", v); // trois niveaux : 0 à 2
    for (std::int32_t v : {-5, 0, 9, 181, 100000})  // la période : de 10 à 180
        b.hote.definir_nombre("alertes.periode", v);
    VERIFIER(b.ports.enregistrements_alertes == 0 && b.ports.etat_alertes == reglages::Alertes{});
    b.hote.definir_nombre("alertes.periode", 10);
    b.hote.definir_nombre("alertes.periode", 180);
    VERIFIER(b.ports.enregistrements_alertes == 2 && b.ports.etat_alertes.periode_min == 180);
    return 0;
}

int adresse_des_alertes_saisie_sur_l_ecran() {
    Banc b;
    char t[96];
    VERIFIER(b.hote.texte("alertes.adresse", t, sizeof t) == 0 && t[0] == '\0'); // vide : l'instance publique
    b.hote.definir_texte("alertes.adresse", "HTTPS://vigie.exemple.fr/");        // normalisée avant d'être gardée
    VERIFIER(b.ports.enregistrements_alertes == 1);
    VERIFIER(b.hote.texte("alertes.adresse", t, sizeof t) == sv("https://vigie.exemple.fr").size());
    VERIFIER(sv(t) == "https://vigie.exemple.fr");
    VERIFIER(sv(b.ports.etat_alertes.adresse) == "https://vigie.exemple.fr");
    // Ce que l'API refuserait n'est pas enregistré non plus : http://, pas de schéma, mot de passe dans l'adresse, trop long.
    for (sv refusee : {sv("http://vigie.exemple.fr"), sv("vigie.exemple.fr"), sv("https://moi:mdp@vigie.exemple.fr"),
                       sv("https://vigie.exemple.fr/?a=b"), sv("https://")}) {
        b.hote.definir_texte("alertes.adresse", refusee);
    }
    b.hote.definir_texte("alertes.adresse", "https://" + std::string(73, 'a')); // 81 octets : plus que le maximum
    b.hote.definir_texte("alertes.adresse", "https://vigie\n.exemple.fr");      // octet de contrôle
    VERIFIER(b.ports.enregistrements_alertes == 1 && sv(b.ports.etat_alertes.adresse) == "https://vigie.exemple.fr");
    // Une adresse trop grande pour le tampon n'est jamais rendue à moitié.
    char court[8];
    VERIFIER(b.hote.texte("alertes.adresse", court, sizeof court) == 0 && court[0] == '\0');
    // Vide, ou faite d'espaces : l'instance publique.
    b.hote.definir_texte("alertes.adresse", "   ");
    VERIFIER(b.ports.enregistrements_alertes == 2 && b.ports.etat_alertes.adresse[0] == '\0');
    VERIFIER(b.hote.texte("alertes.adresse", t, sizeof t) == 0 && t[0] == '\0');
    // Une adresse change seule : le reste des alertes reste ce qu'il était.
    b.hote.definir_nombre("alertes.periode", 60);
    b.hote.definir_texte("alertes.adresse", "https://vigie.exemple.fr:8443/vigie");
    VERIFIER(b.ports.etat_alertes.periode_min == 60 &&
             sv(b.ports.etat_alertes.adresse) == "https://vigie.exemple.fr:8443/vigie");
    VERIFIER(b.magasin.textes.empty()); // la source est une seule
    return 0;
}

int reinitialiser_remet_les_alertes_par_defaut() {
    Banc b;
    b.hote.reinitialiser(); // déjà par défaut : rien à écrire, le client réseau n'est pas réveillé pour rien
    VERIFIER(b.ports.enregistrements_alertes == 0);
    b.hote.definir_nombre("alertes.actives", 1);
    b.hote.definir_nombre("alertes.niveau", 2);
    b.hote.definir_nombre("alertes.periode", 120);
    b.hote.definir_texte("alertes.adresse", "https://vigie.exemple.fr");
    VERIFIER(b.ports.enregistrements_alertes == 4 && !(b.ports.etat_alertes == reglages::Alertes{}));
    b.hote.reinitialiser();
    VERIFIER(b.ports.enregistrements_alertes == 5 && b.ports.etat_alertes == reglages::Alertes{});
    VERIFIER(b.hote.nombre("alertes.actives") == 0 && b.hote.nombre("alertes.periode") == 15);
    return 0;
}

// ---- la mer (ADR-0026) -------------------------------------------------------------------------------------------------

int le_registre_de_la_mer_passe_par_la_porte_de_la_page_web() {
    Banc b;
    VERIFIER(mer::ajouter(b.ports.registre_mer, "spot-a", "Spot A") == mer::ErreurRegistre::Aucune);
    mer::Registre lu;
    VERIFIER(b.hote.mer_registre(lu) && lu.nombre == 1 &&
             mer::detail::longueur_bornee(lu.spots[0].id, sizeof lu.spots[0].id) == 6);
    // Un changement fait ailleurs (la page web) se voit tout de suite : l'hôte n'en garde aucune copie.
    VERIFIER(mer::ajouter(b.ports.registre_mer, "spot-b", "Spot B") == mer::ErreurRegistre::Aucune);
    VERIFIER(b.hote.mer_registre(lu) && lu.nombre == 2);
    // Un registre illisible ne laisse pas croire que `sortie` est à jour.
    b.ports.registre_mer_lisible = false;
    lu = mer::Registre{};
    VERIFIER(!b.hote.mer_registre(lu) && lu.nombre == 0);
    return 0;
}

int enregistrer_la_mer_passe_par_la_porte_et_un_refus_ne_change_rien() {
    Banc b;
    mer::Registre r;
    VERIFIER(mer::ajouter(r, "spot-a", "Spot A") == mer::ErreurRegistre::Aucune);
    VERIFIER(b.hote.mer_enregistrer(r) && b.ports.enregistrements_mer == 1 && b.ports.registre_mer.nombre == 1);
    // Une plage de vent refusée par l'API l'est aussi par l'écran.
    mer::Registre mauvais = r;
    mauvais.vent_min_kn = 30;
    mauvais.vent_max_kn = 20;
    VERIFIER(!b.hote.mer_enregistrer(mauvais) && b.ports.enregistrements_mer == 1 &&
             b.ports.registre_mer.vent_min_kn == 12);
    // Une écriture qui échoue (mémoire permanente) est annoncée, pas avalée.
    b.ports.ecriture_mer_ok = false;
    VERIFIER(!b.hote.mer_enregistrer(r) && b.ports.enregistrements_mer == 1);
    return 0;
}

int la_liste_de_vigie_se_demande_et_ne_se_copie_qu_une_fois_terminee() {
    Banc b;
    mer::liste::Liste copie;
    VERIFIER(b.hote.mer_liste_etat() == ListeMer::Inactive);
    VERIFIER(!b.hote.mer_liste_copier(copie)); // rien n'a été demandé
    b.hote.mer_liste_demander();
    VERIFIER(b.ports.demandes_liste_mer == 1 && b.hote.mer_liste_etat() == ListeMer::EnCours);
    VERIFIER(!b.hote.mer_liste_copier(copie) && copie.nombre == 0); // pas prête : rien de copié
    VERIFIER(mer::liste::analyser(R"({"spots":[{"id":"spot-a","name":"Spot A","place":"Port-Exemple"}]})",
                                  b.ports.liste_mer) == mer::liste::Issue::Ok);
    b.ports.liste_mer_etat = ListeMer::Terminee;
    VERIFIER(b.hote.mer_liste_etat() == ListeMer::Terminee);
    VERIFIER(b.hote.mer_liste_copier(copie) && copie.nombre == 1);
    VERIFIER(mer::liste::id_du_spot(copie.spots[0]) == "spot-a" &&
             mer::liste::lieu_du_spot(copie.spots[0]) == "Port-Exemple");
    // Les autres issues sont rendues telles quelles : l'écran dit chacune avec sa phrase.
    for (const ListeMer e : {ListeMer::Vide, ListeMer::SansConnexion, ListeMer::Echec}) {
        b.ports.liste_mer_etat = e;
        VERIFIER(b.hote.mer_liste_etat() == e && !b.hote.mer_liste_copier(copie));
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(affichage_passe_par_la_porte_de_la_page_web)                                                                     \
    X(un_seul_champ_change_a_la_fois)                                                                                  \
    X(luminosite_qui_eteindrait_l_ecran_est_refusee)                                                                   \
    X(choix_hors_liste_ou_inconnu_est_refuse)                                                                          \
    X(ville_affichee_est_celle_de_la_page_web)                                                                         \
    X(services_gardes_et_relus_apres_un_redemarrage)                                                                   \
    X(un_texte_trop_petit_pour_la_valeur_n_est_jamais_rendu_a_moitie)                                                  \
    X(un_secret_ne_ressort_jamais)                                                                                     \
    X(texte_refuse_par_le_registre_n_est_jamais_enregistre)                                                            \
    X(reseaux_vus_en_barres_et_au_plus_seize)                                                                          \
    X(reseau_retenu_seulement_apres_une_connexion_reussie)                                                             \
    X(abandon_ne_retient_rien_meme_si_la_reponse_arrive_apres)                                                         \
    X(essai_impossible_a_demarrer_est_un_echec_tout_de_suite)                                                          \
    X(reseau_ouvert_se_retient_sans_mot_de_passe)                                                                      \
    X(recherche_de_lieu_du_depart_au_choix)                                                                            \
    X(source_meteo_automatique_au_depart_et_liste_selon_le_pays)                                                       \
    X(choisir_une_source_la_garde_par_la_porte_de_la_page_web)                                                         \
    X(une_source_qui_ne_couvre_pas_le_pays_ou_hors_liste_est_refusee)                                                  \
    X(une_ecriture_refusee_ne_change_pas_la_source)                                                                    \
    X(une_source_inconnue_en_memoire_se_lit_automatique)                                                               \
    X(changer_de_ville_hors_pays_remet_automatique_et_le_dit)                                                          \
    X(changer_de_ville_garde_la_source_quand_elle_convient_encore)                                                     \
    X(choisir_un_rang_qui_n_existe_pas_ne_change_rien)                                                                 \
    X(saisie_trop_courte_ou_illisible_ne_telecharge_rien)                                                              \
    X(recherche_sans_reponse_dit_sans_connexion_et_rend_la_memoire)                                                    \
    X(reponse_illisible_donne_zero_lieu_sans_erreur)                                                                   \
    X(nouvelle_recherche_efface_l_ancienne)                                                                            \
    X(langue_de_l_ecran_dicte_celle_de_la_recherche)                                                                   \
    X(apercu_meteo_dans_la_langue_de_l_ecran)                                                                          \
    X(test_de_service_ne_se_dit_jamais_reussi_sans_avoir_eu_lieu)                                                      \
    X(informations_de_l_appareil)                                                                                      \
    X(progression_se_reprend_apres_un_redemarrage)                                                                     \
    X(octet_d_installation_abime_ne_passe_pas_pour_terminee)                                                           \
    X(reinitialiser_efface_ce_que_l_hote_garde)                                                                        \
    X(valeur_abimee_en_memoire_donne_le_defaut)                                                                        \
    X(alertes_passent_par_la_porte_de_la_page_web)                                                                     \
    X(alertes_hors_plage_ou_hors_liste_sont_refusees)                                                                  \
    X(adresse_des_alertes_saisie_sur_l_ecran)                                                                          \
    X(reinitialiser_remet_les_alertes_par_defaut)                                                                      \
    X(le_registre_de_la_mer_passe_par_la_porte_de_la_page_web)                                                         \
    X(enregistrer_la_mer_passe_par_la_porte_et_un_refus_ne_change_rien)                                                \
    X(la_liste_de_vigie_se_demande_et_ne_se_copie_qu_une_fois_terminee)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
