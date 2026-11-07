// Doublures de l'HÔTE DE L'APPAREIL (ecran/hote_appareil.hpp) : une mémoire permanente et un appareil factices, en mémoire vive.
// Partagées par les tests du noyau (test/host/test_hote_appareil.cpp) et par la scène de rendu qui fait tourner les vrais écrans
// au-dessus de HoteAppareil (tools/rendu/scenes/scene_hote_appareil.cpp). Ce n'est pas du firmware : le vrai accès à la NVS,
// au Wi-Fi du C6 et au serveur de géocodage est dans main/hote_appareil_esp.cpp.
#pragma once

#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ecran/hote_appareil.hpp"

namespace ecran::faux {

// ---- mémoire factice ------------------------------------------------------------------------------------------------------

class MagasinFaux final : public Magasin {
  public:
    std::map<std::string, std::int32_t> nombres;
    std::map<std::string, std::string> textes;
    int ecritures = 0;

    static std::string nom(const char *espace, const char *cle) { return std::string(espace) + "/" + cle; }

    bool lire_nombre(const char *espace, const char *cle, std::int32_t &valeur) override {
        const auto it = nombres.find(nom(espace, cle));
        if (it == nombres.end()) {
            return false;
        }
        valeur = it->second;
        return true;
    }
    bool ecrire_nombre(const char *espace, const char *cle, std::int32_t valeur) override {
        ++ecritures;
        nombres[nom(espace, cle)] = valeur;
        return true;
    }
    std::int32_t longueur_texte(const char *espace, const char *cle) override {
        const auto it = textes.find(nom(espace, cle));
        return it == textes.end() ? -1 : static_cast<std::int32_t>(it->second.size());
    }
    bool lire_texte(const char *espace, const char *cle, char *sortie, std::size_t capacite) override {
        const auto it = textes.find(nom(espace, cle));
        if (it == textes.end() || it->second.size() + 1 > capacite) {
            return false;
        }
        std::memcpy(sortie, it->second.c_str(), it->second.size() + 1);
        return true;
    }
    bool ecrire_texte(const char *espace, const char *cle, const char *valeur) override {
        ++ecritures;
        textes[nom(espace, cle)] = valeur;
        return true;
    }
    bool effacer(const char *espace, const char *cle) override {
        nombres.erase(nom(espace, cle));
        textes.erase(nom(espace, cle));
        return true;
    }
    bool effacer_espace(const char *espace) override {
        const std::string debut = std::string(espace) + "/";
        for (auto it = nombres.begin(); it != nombres.end();) {
            it = it->first.compare(0, debut.size(), debut) == 0 ? nombres.erase(it) : std::next(it);
        }
        for (auto it = textes.begin(); it != textes.end();) {
            it = it->first.compare(0, debut.size(), debut) == 0 ? textes.erase(it) : std::next(it);
        }
        return true;
    }
    bool vide_dans(const char *espace) const {
        const std::string debut = std::string(espace) + "/";
        for (const auto &p : nombres) {
            if (p.first.compare(0, debut.size(), debut) == 0) {
                return false;
            }
        }
        for (const auto &p : textes) {
            if (p.first.compare(0, debut.size(), debut) == 0) {
                return false;
            }
        }
        return true;
    }
};

// ---- appareil factice -----------------------------------------------------------------------------------------------------

class PortsFaux final : public PortsAppareil {
  public:
    // la source de la page web
    reglages::Affichage etat_affichage;
    int enregistrements = 0;
    // les alertes officielles : la source de la page web (main/reglages_alertes.*)
    reglages::Alertes etat_alertes;
    int enregistrements_alertes = 0;
    bool alertes_ecriture_ok = true;
    int tentatives = 0; // appels, acceptés ou non
    reglages::Zone zone;
    bool zone_connue = true;
    int zones_enregistrees = 0;
    int remises_par_defaut = 0;
    std::string source = "auto"; // la source de la météo en service (main/reglages.*)
    int sources_enregistrees = 0;
    bool source_retablie = false; // comme main/reglages.* : la dernière ville a remis « Automatique »
    bool source_refusee = false;  // la mémoire permanente refuse l'écriture
    MeteoVue meteo_vue;
    // Wi-Fi
    std::vector<PointAccesVu> reseaux;
    bool scan_en_cours = false;
    int scans = 0;
    bool accepte_connexion = true;
    int connexions = 0;
    int abandons = 0;
    std::string dernier_ssid;
    std::string dernier_mdp;
    void (*rappel)(wifi::Resultat, void *) = nullptr;
    void *contexte = nullptr;
    InfoWifi info_wifi;
    // téléchargement
    EtatTelechargement dl = EtatTelechargement::Inactif;
    std::string corps;
    std::string derniere_adresse;
    int telechargements = 0;
    int liberations = 0;
    // appareil
    std::string version_texte = "0.6.0";
    std::string code = "482913";
    int nouveaux_codes = 0;
    int redemarrages = 0;
    std::int64_t horloge = 1000;

    PortsFaux() {
        reglages::copier_texte("Lyon", zone.nom, sizeof zone.nom);
        reglages::copier_texte("Auvergne-Rhône-Alpes", zone.region, sizeof zone.region);
        reglages::copier_texte("France", zone.pays, sizeof zone.pays);
        reglages::copier_texte("Europe/Paris", zone.fuseau, sizeof zone.fuseau);
        zone.latitude = 45.74846;
        zone.longitude = 4.84671;
    }

    void repondre_wifi(wifi::Resultat r) {
        if (rappel != nullptr) {
            rappel(r, contexte);
        }
    }

    reglages::Affichage affichage() override { return etat_affichage; }
    bool enregistrer_affichage(const reglages::Affichage &nouveau) override {
        ++tentatives;
        if (!reglages::luminosite_valide(nouveau.luminosite)) {
            return false; // comme main/reglages.cpp
        }
        ++enregistrements;
        etat_affichage = nouveau;
        return true;
    }
    reglages::Alertes alertes() override { return etat_alertes; }
    bool enregistrer_alertes(const reglages::Alertes &nouveau) override {
        if (!alertes_ecriture_ok) {
            return false;
        }
        ++enregistrements_alertes;
        etat_alertes = nouveau;
        return true;
    }
    bool zone_lire(reglages::Zone &z) override {
        if (!zone_connue) {
            return false;
        }
        z = zone;
        return true;
    }
    bool zone_enregistrer(const reglages::Zone &z) override {
        ++zones_enregistrees;
        zone = z;
        // Comme main/reglages.cpp : une source qui ne couvre pas le nouveau pays repasse sur « Automatique », et on s'en souvient.
        const std::string avant = source;
        source = std::string(meteo::fournisseurs::source_valable_pour(source, z.code_pays));
        source_retablie = avant != source;
        return true;
    }
    void remettre_affichage_et_ville_par_defaut() override {
        ++remises_par_defaut;
        etat_affichage = reglages::Affichage{};
        source = "auto";
        source_retablie = false;
    }
    void source_meteo(char *sortie, std::size_t capacite) override { reglages::copier_texte(source, sortie, capacite); }
    bool source_meteo_enregistrer(std::string_view id) override {
        if (source_refusee || meteo::fournisseurs::trouver(id) == nullptr) {
            return false;
        }
        ++sources_enregistrees;
        source = std::string(id);
        source_retablie = false;
        return true;
    }
    bool source_meteo_retablie() override { return source_retablie; }
    MeteoVue meteo() override { return meteo_vue; }

    void wifi_scanner_lancer() override { ++scans; }
    bool wifi_scanner_en_cours() override { return scan_en_cours; }
    std::size_t wifi_scanner_resultats(PointAccesVu *sortie, std::size_t max) override {
        std::size_t n = 0;
        for (const PointAccesVu &p : reseaux) {
            if (n < max) {
                sortie[n++] = p;
            }
        }
        return n;
    }
    bool wifi_connecter(const char *ssid, const char *mot_de_passe, void (*r)(wifi::Resultat, void *),
                        void *c) override {
        ++connexions;
        dernier_ssid = ssid;
        dernier_mdp = mot_de_passe;
        if (!accepte_connexion) {
            return false;
        }
        rappel = r;
        contexte = c;
        return true;
    }
    void wifi_abandonner() override { ++abandons; }
    InfoWifi wifi_info() override { return info_wifi; }

    void telecharger_lancer(const char *adresse) override {
        ++telechargements;
        derniere_adresse = adresse;
        dl = EtatTelechargement::EnCours;
    }
    EtatTelechargement telecharger_etat() override { return dl; }
    std::string_view telecharger_corps() override { return corps; }
    void telecharger_liberer() override { ++liberations; }

    void version(char *sortie, std::size_t capacite) override {
        reglages::copier_texte(version_texte, sortie, capacite);
    }
    void code_appairage(char *sortie, std::size_t capacite) override { reglages::copier_texte(code, sortie, capacite); }
    void nouveau_code() override { ++nouveaux_codes; }
    void redemarrer() override { ++redemarrages; }
    std::int64_t maintenant_ms() override { return horloge; }
};

inline PointAccesVu point(const char *ssid, int dbm, bool securise) {
    PointAccesVu p{};
    std::snprintf(p.ssid, sizeof p.ssid, "%s", ssid);
    p.dbm = static_cast<std::int8_t>(dbm);
    p.securise = securise;
    return p;
}

} // namespace ecran::faux
