// Noyau pur : l'HÔTE DE L'APPAREIL (ADR-0007) — ce qui répond aux écrans de réglages et d'installation guidée sur le vrai matériel.
//
// Les écrans (main/ui_reglages.cpp, ui_installation.cpp) parlent à `HoteReglages` (hote_reglages.hpp). Sur l'appareil, c'est
// `HoteAppareil` qui répond. Il ne touche ni la mémoire permanente, ni le Wi-Fi, ni le réseau : il passe par deux interfaces,
// `Magasin` (clés et valeurs gardées) et `PortsAppareil` (le reste de l'appareil). main/hote_appareil_esp.cpp les branche sur
// ESP-IDF ; test/host/test_hote_appareil.cpp les branche sur des faux et vérifie TOUTES les règles d'ici sur le PC.
//
// UNE SEULE SOURCE (ADR-0007) : la langue de l'écran, celle de l'assistant, le thème, la luminosité et la ville ne sont PAS gardés
// ici. Ils vivent dans main/reglages.* et main/meteo_zone.* — ce que la page web (main/serveur_web.cpp) lit et écrit — et passent
// par `PortsAppareil::affichage / enregistrer_affichage / zone_*`. Écran et téléphone voient donc toujours la même valeur, et un
// changement fait d'un côté s'applique de l'autre (langue de `ecran::i18n`, rétroéclairage, rappel de l'interface).
// Seuls les réglages que la page web ne sait pas (encore) enregistrer sont gardés ici : les trois services de l'assistant (leurs
// préréglages, adresses, modèles, voix et clés), le Wi-Fi retenu pour le prochain démarrage et la progression de l'installation.
//
// Format en mémoire permanente (une clé NVS de 15 octets au plus ; l'espace « services » reçoit l'identifiant du registre tel quel) :
//   espace « services »     : « stt.preset » … « tts.cle »     nombre (préréglages) ou texte (adresse, modèle, voix, clé)
//   espace « wifi »         : « ssid », « mdp »                 textes ; écrits SEULEMENT quand la connexion a réussi
//   espace « installation » : « etape »                         UN octet (installation.hpp) rangé dans un entier
// Les clés d'accès et le mot de passe Wi-Fi sont enregistrés en clair dans la NVS de l'appareil (chiffrement de la NVS : pas
// activé ; décision de sécurité à prendre, voir docs/mesures/6-reglages-appareil.md). Ils ne ressortent JAMAIS : `texte()` rend un
// texte vide pour un secret, `secret_defini()` répond seulement « oui / non ».
//
// C++17 standard, aucun include ESP-IDF ni LVGL. Les méthodes sont appelées par la tâche de l'interface ; seul le rappel du
// résultat de connexion Wi-Fi (`sur_resultat_wifi`) vient d'une autre tâche et ne touche que des variables atomiques.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/api_reglages.hpp"
#include "ecran/hote_reglages.hpp"
#include "ecran/i18n.hpp"
#include "ecran/installation.hpp"
#include "ecran/lieux_geocodage.hpp"
#include "ecran/meteo_wmo.hpp"
#include "ecran/registre_reglages.hpp"
#include "ecran/reglages_valeurs.hpp"

namespace ecran {

// ---- mémoire permanente ---------------------------------------------------------------------------------------------------

class Magasin {
  public:
    virtual ~Magasin() = default;

    // Vrai si la valeur existe et se lit. Une valeur absente ou abîmée : faux (jamais une valeur inventée).
    virtual bool lire_nombre(const char *espace, const char *cle, std::int32_t &valeur) = 0;
    virtual bool ecrire_nombre(const char *espace, const char *cle, std::int32_t valeur) = 0;
    // Nombre d'octets du texte (zéro final exclu), ou -1 s'il n'existe pas.
    virtual std::int32_t longueur_texte(const char *espace, const char *cle) = 0;
    // Copie le texte dans `sortie` (zéro final compris dans `capacite`). Faux si absent ou si `capacite` est trop petite.
    virtual bool lire_texte(const char *espace, const char *cle, char *sortie, std::size_t capacite) = 0;
    virtual bool ecrire_texte(const char *espace, const char *cle, const char *valeur) = 0;
    virtual bool effacer(const char *espace, const char *cle) = 0;
    virtual bool effacer_espace(const char *espace) = 0;
};

namespace stockage {
inline constexpr const char *ESPACE_SERVICES = "services";
inline constexpr const char *ESPACE_WIFI = "wifi";
inline constexpr const char *CLE_WIFI_SSID = "ssid";
inline constexpr const char *CLE_WIFI_MOT_DE_PASSE = "mdp";
inline constexpr const char *ESPACE_INSTALLATION = "installation";
inline constexpr const char *CLE_ETAPE = "etape";
} // namespace stockage

// ---- le reste de l'appareil -----------------------------------------------------------------------------------------------

enum class EtatTelechargement : std::uint8_t { Inactif, EnCours, Termine, SansInternet, Echec };

struct PointAccesVu {
    char ssid[33];
    std::int8_t dbm;
    bool securise;
};

struct MeteoVue {
    bool valide = false;
    float temperature_c = 0;
    int code_wmo = 0;
    bool jour = true;
};

class PortsAppareil {
  public:
    virtual ~PortsAppareil() = default;

    // ---- affichage et ville : la source de la page web (main/reglages.*, main/meteo_zone.*)
    virtual reglages::Affichage affichage() = 0;
    // Enregistre PUIS applique (langue, rétroéclairage). Faux : rien n'a changé.
    virtual bool enregistrer_affichage(const reglages::Affichage &nouveau) = 0;
    virtual bool zone_lire(reglages::Zone &zone) = 0;
    virtual bool zone_enregistrer(const reglages::Zone &zone) = 0;
    // Langue, thème, luminosité et ville reviennent aux valeurs d'une carte neuve (français, auto, 75 %, Lyon).
    virtual void remettre_affichage_et_ville_par_defaut() = 0;
    virtual MeteoVue meteo() = 0;

    // ---- Wi-Fi (main/reseau_wifi.h) : rien n'est bloquant
    virtual void wifi_scanner_lancer() = 0;
    virtual bool wifi_scanner_en_cours() = 0;
    virtual std::size_t wifi_scanner_resultats(PointAccesVu *sortie, std::size_t max) = 0;
    // Faux : l'essai n'a pas pu démarrer (un autre est en cours, lien absent) ; le rappel n'est alors pas appelé. Le rappel vient
    // de la tâche réseau, une seule fois, jamais après `wifi_abandonner`.
    virtual bool wifi_connecter(const char *ssid, const char *mot_de_passe, void (*rappel)(wifi::Resultat, void *),
                                void *contexte) = 0;
    virtual void wifi_abandonner() = 0;
    virtual InfoWifi wifi_info() = 0;

    // ---- téléchargement d'une adresse (recherche de lieu), au plus un à la fois, hors de la tâche de l'interface
    virtual void telecharger_lancer(const char *adresse) = 0;
    virtual EtatTelechargement telecharger_etat() = 0;
    // La réponse, valable jusqu'à `telecharger_liberer`.
    virtual std::string_view telecharger_corps() = 0;
    virtual void telecharger_liberer() = 0;

    // ---- appareil
    virtual void version(char *sortie, std::size_t capacite) = 0;
    virtual void code_appairage(char *sortie, std::size_t capacite) = 0; // vide tant que la page n'est pas servie
    virtual void nouveau_code() = 0;
    virtual void redemarrer() = 0;
    virtual std::int64_t maintenant_ms() = 0;
};

// ---- l'hôte ---------------------------------------------------------------------------------------------------------------

// Les listes que l'écran et l'hôte partagent doivent avoir la même taille : sinon un lieu ou une langue se perdrait en route.
static_assert(sizeof(LieuTrouve::libelle) == lieux::LIBELLE_OCTETS,
              "libellé d'un lieu : même taille côté écran et côté noyau");
static_assert(kLieuxMax == lieux::CANDIDATS_MAX, "nombre de lieux proposés : même valeur côté écran et côté noyau");

class HoteAppareil final : public HoteReglages {
  public:
    HoteAppareil(PortsAppareil &ports, Magasin &magasin) : p_(ports), m_(magasin) {}

    // ---- réglages du registre
    std::int32_t nombre(std::string_view id) override {
        const registre::Reglage *r = registre::chercher(id);
        if (r == nullptr) {
            return 0;
        }
        if (id == "langue.ecran") {
            return i18n::indice(p_.affichage().langue_ecran);
        }
        if (id == "langue.assistant") {
            return i18n::indice(p_.affichage().langue_assistant);
        }
        if (id == "affichage.theme") {
            return static_cast<std::int32_t>(p_.affichage().theme);
        }
        if (id == "affichage.luminosite") {
            return p_.affichage().luminosite;
        }
        char cle[16];
        std::int32_t valeur = 0;
        if ((r->type == registre::Type::Choix || r->type == registre::Type::Nombre) && cle_services(id, cle) &&
            m_.lire_nombre(stockage::ESPACE_SERVICES, cle, valeur)) {
            return r->type == registre::Type::Choix ? registre::choix_depuis_nvs(*r, valeur)
                                                    : registre::nombre_depuis_nvs(*r, valeur);
        }
        return r->defaut;
    }

    void definir_nombre(std::string_view id, std::int32_t valeur) override {
        const registre::Reglage *r = registre::chercher(id);
        if (r == nullptr) {
            return;
        }
        // Les mêmes règles que le registre et l'API web : hors plage, hors liste, ou « pas choisi » sur un réglage obligatoire :
        // refusé, jamais enregistré (une luminosité à zéro rendrait l'écran noir).
        const bool valide = r->type == registre::Type::Nombre
                                ? registre::nombre_valide(*r, valeur)
                                : (r->type == registre::Type::Choix && valeur >= (r->facultatif ? -1 : 0) &&
                                   valeur < static_cast<std::int32_t>(r->nb_options));
        if (!valide) {
            return;
        }
        if (id == "langue.ecran" || id == "langue.assistant" || id == "affichage.theme" ||
            id == "affichage.luminosite") {
            reglages::Affichage a = p_.affichage();
            if (id == "langue.ecran") {
                a.langue_ecran = i18n::langue_depuis_indice(valeur);
            } else if (id == "langue.assistant") {
                a.langue_assistant = i18n::langue_depuis_indice(valeur);
            } else if (id == "affichage.theme") {
                a.theme = reglages::theme_depuis_indice(valeur);
            } else {
                a.luminosite = valeur;
            }
            p_.enregistrer_affichage(a); // la source de la page web : elle applique la langue et le rétroéclairage
            return;
        }
        char cle[16];
        if (cle_services(id, cle)) {
            m_.ecrire_nombre(stockage::ESPACE_SERVICES, cle, valeur);
        }
    }

    std::size_t texte(std::string_view id, char *sortie, std::size_t capacite) override {
        if (sortie == nullptr || capacite == 0) {
            return 0;
        }
        sortie[0] = '\0';
        const registre::Reglage *r = registre::chercher(id);
        if (r == nullptr || r->type == registre::Type::Secret) { // un secret ne se relit jamais
            return 0;
        }
        if (id == "lieu.ville") {
            reglages::Zone z;
            return p_.zone_lire(z) ? lieux::libelle(z, sortie, capacite) : 0;
        }
        const char *espace = nullptr;
        char cle[16];
        if (!cle_texte(id, espace, cle) || !m_.lire_texte(espace, cle, sortie, capacite)) {
            sortie[0] = '\0';
            return 0;
        }
        std::size_t n = 0;
        while (sortie[n] != '\0') {
            ++n;
        }
        return n;
    }

    void definir_texte(std::string_view id, std::string_view valeur) override {
        const registre::Reglage *r = registre::chercher(id);
        if (r == nullptr || (r->type != registre::Type::Texte && r->type != registre::Type::Secret)) {
            return; // la ville se choisit par `lieu_choisir`, jamais par un texte
        }
        if (registre::valider_texte(*r, valeur) != registre::ErreurTexte::Aucune) {
            return; // l'écran valide avant ; ce garde-fou empêche d'enregistrer ce que l'API web refuserait
        }
        const char *espace = nullptr;
        char cle[16];
        if (!cle_texte(id, espace, cle)) {
            return;
        }
        if (valeur.size() >= sizeof copie_) {
            return;
        }
        for (std::size_t i = 0; i < valeur.size(); ++i) {
            copie_[i] = valeur[i];
        }
        copie_[valeur.size()] = '\0';
        m_.ecrire_texte(espace, cle, copie_);
        effacer_octets(copie_, valeur.size() + 1); // une clé d'accès ne reste pas en mémoire
    }

    bool secret_defini(std::string_view id) override {
        const registre::Reglage *r = registre::chercher(id);
        const char *espace = nullptr;
        char cle[16];
        return r != nullptr && r->type == registre::Type::Secret && cle_texte(id, espace, cle) &&
               m_.longueur_texte(espace, cle) > 0;
    }

    void effacer_secret(std::string_view id) override {
        const registre::Reglage *r = registre::chercher(id);
        const char *espace = nullptr;
        char cle[16];
        if (r != nullptr && r->type == registre::Type::Secret && cle_texte(id, espace, cle)) {
            m_.effacer(espace, cle);
        }
    }

    // ---- Wi-Fi ------------------------------------------------------------------------------------------------------------

    void wifi_chercher() override { p_.wifi_scanner_lancer(); }
    bool wifi_recherche_en_cours() override { return p_.wifi_scanner_en_cours(); }

    std::size_t wifi_reseaux(ReseauWifi *sortie, std::size_t max) override {
        PointAccesVu vus[kReseauxMax];
        const std::size_t limite = max < kReseauxMax ? max : kReseauxMax;
        const std::size_t n = p_.wifi_scanner_resultats(vus, limite);
        for (std::size_t i = 0; i < n && i < limite; ++i) {
            reglages::copier_texte(vus[i].ssid, sortie[i].ssid, sizeof sortie[i].ssid);
            sortie[i].niveau = static_cast<std::int8_t>(api::niveau_wifi(vus[i].dbm));
            sortie[i].securise = vus[i].securise;
        }
        return n < limite ? n : limite;
    }

    void wifi_connecter(std::string_view ssid, std::string_view mot_de_passe) override {
        recu_.store(false, std::memory_order_release);
        connexion_ = ConnexionWifi{wifi::Etat::Connexion, wifi::Cause::Aucune};
        reglages::copier_texte(ssid, ssid_essai_, sizeof ssid_essai_);
        reglages::copier_texte(mot_de_passe, mdp_essai_, sizeof mdp_essai_);
        if (!p_.wifi_connecter(ssid_essai_, mdp_essai_, &HoteAppareil::sur_resultat_wifi, this)) {
            connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::Autre};
            oublier_essai();
        }
    }

    void wifi_abandonner() override {
        p_.wifi_abandonner();
        recu_.store(false, std::memory_order_release);
        if (connexion_.etat == wifi::Etat::Connexion) {
            connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::Delai};
        }
        oublier_essai();
    }

    ConnexionWifi wifi_connexion() override {
        if (connexion_.etat == wifi::Etat::Connexion && recu_.load(std::memory_order_acquire)) {
            const auto resultat = static_cast<wifi::Resultat>(resultat_.load(std::memory_order_relaxed));
            recu_.store(false, std::memory_order_release);
            switch (resultat) {
            case wifi::Resultat::Connecte:
                connexion_ = ConnexionWifi{wifi::Etat::Reussi, wifi::Cause::Aucune};
                // Le réseau n'est retenu pour le prochain démarrage QU'À CE MOMENT : un essai raté ou abandonné n'écrit rien.
                m_.ecrire_texte(stockage::ESPACE_WIFI, stockage::CLE_WIFI_SSID, ssid_essai_);
                m_.ecrire_texte(stockage::ESPACE_WIFI, stockage::CLE_WIFI_MOT_DE_PASSE, mdp_essai_);
                break;
            case wifi::Resultat::MotDePasseRefuse:
                connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::MotDePasse};
                break;
            case wifi::Resultat::ReseauIntrouvable:
                connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::Introuvable};
                break;
            default:
                connexion_ = ConnexionWifi{wifi::Etat::Echec, wifi::Cause::Autre};
                break;
            }
            oublier_essai();
        }
        return connexion_;
    }

    InfoWifi wifi_info() override { return p_.wifi_info(); }

    // ---- lieu -------------------------------------------------------------------------------------------------------------

    void lieu_chercher(std::string_view requete) override {
        p_.telecharger_liberer();
        nb_lieux_ = 0;
        recherche_ = RechercheLieu::Terminee; // sans résultat, sauf si la recherche part
        const reglages::ResultatRecherche v = reglages::valider_recherche(requete);
        if (v.erreur != reglages::ErreurRecherche::Aucune) {
            return;
        }
        api::Ecrivain e(adresse_, api::ADRESSE_GEOCODAGE_MAX);
        reglages::adresse_geocodage(v.q, p_.affichage().langue_ecran, e);
        if (!e.ok()) {
            return;
        }
        adresse_[e.taille()] = '\0';
        p_.telecharger_lancer(adresse_);
        recherche_ = RechercheLieu::EnCours;
    }

    RechercheLieu lieu_recherche() override {
        if (recherche_ == RechercheLieu::EnCours) {
            switch (p_.telecharger_etat()) {
            case EtatTelechargement::EnCours:
                break;
            case EtatTelechargement::Termine:
                nb_lieux_ = lieux::lire(p_.telecharger_corps(), lieux_, lieux::CANDIDATS_MAX);
                p_.telecharger_liberer();
                recherche_ = RechercheLieu::Terminee;
                break;
            default: // pas d'Internet, serveur injoignable, réponse refusée : l'écran ne distingue que « sans connexion »
                p_.telecharger_liberer();
                recherche_ = RechercheLieu::SansConnexion;
                break;
            }
        }
        return recherche_;
    }

    std::size_t lieu_resultats(LieuTrouve *sortie, std::size_t max) override {
        const std::size_t n = nb_lieux_ < max ? nb_lieux_ : max;
        for (std::size_t i = 0; i < n; ++i) {
            reglages::copier_texte(lieux_[i].libelle, sortie[i].libelle, sizeof sortie[i].libelle);
        }
        return n;
    }

    void lieu_choisir(std::size_t rang) override {
        if (rang < nb_lieux_) {
            p_.zone_enregistrer(
                lieux_[rang].zone); // la ville de la page web ; la météo se relance d'elle-même (meteo_zone.h)
        }
    }

    ApercuMeteo meteo_apercu() override {
        ApercuMeteo a;
        const MeteoVue m = p_.meteo();
        if (!m.valide) {
            return a;
        }
        a.disponible = true;
        a.temperature_c = m.temperature_c;
        reglages::copier_texte(i18n::tr(meteo::decrire(m.code_wmo, m.jour).libelle), a.libelle, sizeof a.libelle);
        return a;
    }

    // ---- appareil ---------------------------------------------------------------------------------------------------------

    // Le test d'un service passe par Internet (TLS) : il attend le lot de mémoire de jalon-1b. En attendant, il dit la vérité
    // (« pas encore disponible ») au lieu d'afficher un faux succès.
    void tester_service(std::string_view service) override {
        const int s = rang_service(service);
        if (s >= 0) {
            test_fait_[s] = true;
        }
    }

    ResultatTest resultat_test(std::string_view service) override {
        ResultatTest r;
        const int s = rang_service(service);
        if (s >= 0 && test_fait_[s]) {
            r.etat = EtatTest::Echec;
            reglages::copier_texte(i18n::tr("Cette fonction n’est pas encore disponible sur cet écran."), r.message,
                                   sizeof r.message);
        }
        return r;
    }

    InfoAppareil info_appareil() override {
        InfoAppareil info;
        p_.version(info.version, sizeof info.version);
        p_.code_appairage(info.code, sizeof info.code);
        const InfoWifi w = p_.wifi_info();
        if (w.connecte && w.ip[0] != '\0') {
            char adresse[sizeof info.adresse] = {};
            api::Ecrivain e(adresse, sizeof adresse - 1);
            e.brut("http://");
            e.brut(w.ip);
            if (e.ok()) {
                reglages::copier_texte(e.vue(), info.adresse, sizeof info.adresse);
            }
        }
        return info;
    }

    void nouveau_code() override { p_.nouveau_code(); }
    void redemarrer() override { p_.redemarrer(); }

    // Toutes les valeurs reviennent au défaut du registre ; la progression aussi (« première installation, étape 1 »). Le Wi-Fi
    // retenu est oublié pour le PROCHAIN démarrage ; la connexion en cours n'est pas coupée.
    void reinitialiser() override {
        m_.effacer_espace(stockage::ESPACE_SERVICES);
        m_.effacer_espace(stockage::ESPACE_WIFI);
        p_.remettre_affichage_et_ville_par_defaut();
        definir_progression(installation::apres_reinitialisation());
        nb_lieux_ = 0;
        recherche_ = RechercheLieu::Inactive;
        for (bool &fait : test_fait_) {
            fait = false;
        }
        p_.telecharger_liberer();
    }

    // ---- installation guidée ----------------------------------------------------------------------------------------------

    installation::Progression progression() override {
        std::int32_t v = 0;
        if (!m_.lire_nombre(stockage::ESPACE_INSTALLATION, stockage::CLE_ETAPE, v)) {
            return installation::Progression{}; // carte neuve : première installation, étape 1
        }
        return installation::decoder(v); // un octet abîmé ne passe jamais pour « terminée »
    }

    void definir_progression(const installation::Progression &p) override {
        m_.ecrire_nombre(stockage::ESPACE_INSTALLATION, stockage::CLE_ETAPE, installation::encoder(p));
    }

    std::int64_t maintenant_ms() override { return p_.maintenant_ms(); }

  private:
    // « stt.url » -> clé « stt.url » de l'espace « services » (15 octets au plus). Faux pour un réglage qui n'y vit pas.
    static bool cle_services(std::string_view id, char (&cle)[16]) {
        if (id.size() >= sizeof cle ||
            (id.substr(0, 4) != "stt." && id.substr(0, 4) != "llm." && id.substr(0, 4) != "tts.")) {
            return false;
        }
        for (std::size_t i = 0; i < id.size(); ++i) {
            cle[i] = id[i];
        }
        cle[id.size()] = '\0';
        return true;
    }

    static bool cle_texte(std::string_view id, const char *&espace, char (&cle)[16]) {
        if (id == "wifi.ssid") {
            espace = stockage::ESPACE_WIFI;
            return copier_cle(stockage::CLE_WIFI_SSID, cle);
        }
        if (id == "wifi.mot_de_passe") {
            espace = stockage::ESPACE_WIFI;
            return copier_cle(stockage::CLE_WIFI_MOT_DE_PASSE, cle);
        }
        espace = stockage::ESPACE_SERVICES;
        return cle_services(id, cle);
    }

    static bool copier_cle(const char *source, char (&cle)[16]) {
        std::size_t i = 0;
        for (; source[i] != '\0' && i + 1 < sizeof cle; ++i) {
            cle[i] = source[i];
        }
        cle[i] = '\0';
        return source[i] == '\0';
    }

    static int rang_service(std::string_view service) {
        for (int i = 0; i < 3; ++i) {
            if (kServices[i] == service) {
                return i;
            }
        }
        return -1;
    }

    // Efface avec une écriture que le compilateur ne peut pas retirer : le mot de passe ne reste pas en mémoire.
    static void effacer_octets(char *tampon, std::size_t n) {
        volatile char *v = tampon;
        for (std::size_t i = 0; i < n; ++i) {
            v[i] = 0;
        }
    }

    void oublier_essai() {
        effacer_octets(ssid_essai_, sizeof ssid_essai_);
        effacer_octets(mdp_essai_, sizeof mdp_essai_);
    }

    // Appelé par la tâche réseau, une fois par essai : ne fait que noter le résultat.
    static void sur_resultat_wifi(wifi::Resultat resultat, void *contexte) {
        auto *hote = static_cast<HoteAppareil *>(contexte);
        hote->resultat_.store(static_cast<int>(resultat), std::memory_order_relaxed);
        hote->recu_.store(true, std::memory_order_release);
    }

    PortsAppareil &p_;
    Magasin &m_;

    ConnexionWifi connexion_{};
    char ssid_essai_[33] = {};
    char mdp_essai_[registre::MOT_DE_PASSE_WIFI_MAX + 1] = {};
    std::atomic<int> resultat_{0};
    std::atomic<bool> recu_{false};

    // Gros tampons en membres et non sur la pile : la pile de la tâche de l'interface est comptée.
    char adresse_[api::ADRESSE_GEOCODAGE_MAX + 1] = {};
    char copie_[registre::CLE_MAX * 4 + 1] = {};

    RechercheLieu recherche_ = RechercheLieu::Inactive;
    lieux::Candidat lieux_[lieux::CANDIDATS_MAX] = {};
    std::size_t nb_lieux_ = 0;

    bool test_fait_[3] = {false, false, false};
};

} // namespace ecran
