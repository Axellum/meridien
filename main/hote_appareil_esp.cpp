// Les deux portes de l'hôte de l'appareil (ecran/hote_appareil.hpp) branchées sur ESP-IDF. Aucune règle ici : tout ce qui décide est
// dans le noyau pur et testé sur le PC. Ce fichier lit, écrit, appelle et rend la main.
//
//  - MagasinNvs   : la mémoire permanente (NVS), clés de 15 octets au plus, chaque écriture validée (nvs_commit) avant de rendre
//                   vrai. Aucune valeur n'est écrite au journal : les clés d'accès et le mot de passe Wi-Fi n'y passent jamais.
//  - PortsEsp     : l'affichage et la ville de la page web (main/reglages.*), la météo, le Wi-Fi (main/reseau_wifi.h), le
//                   téléchargement du géocodage (main/geocodage.*, sous le verrou TLS commun), la version, le redémarrage.
#include "hote_appareil_esp.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "board.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"

#include "appairage.h"
#include "ecran/hote_appareil.hpp"
#include "geocodage.h"
#include "mer_liste_client.h"
#include "meteo_etat.h"
#include "meteo_interne.h"
#include "reglages.h"
#include "reglages_alertes.h"
#include "reglages_mer.h"
#include "reseau.h"
#include "reseau_wifi.h"

namespace ecran {

namespace {

const char *TAG = "hote";

// ---- mémoire permanente -------------------------------------------------------------------------------------------------

class MagasinNvs final : public Magasin {
  public:
    bool lire_nombre(const char *espace, const char *cle, std::int32_t &valeur) override {
        nvs_handle_t h = 0;
        if (nvs_open(espace, NVS_READONLY, &h) != ESP_OK) {
            return false;
        }
        std::int32_t v = 0;
        const esp_err_t err = nvs_get_i32(h, cle, &v);
        nvs_close(h);
        if (err != ESP_OK) {
            return false;
        }
        valeur = v;
        return true;
    }

    bool ecrire_nombre(const char *espace, const char *cle, std::int32_t valeur) override {
        nvs_handle_t h = 0;
        esp_err_t err = nvs_open(espace, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_i32(h, cle, valeur);
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
        return signaler(err, espace, cle);
    }

    std::int32_t longueur_texte(const char *espace, const char *cle) override {
        nvs_handle_t h = 0;
        if (nvs_open(espace, NVS_READONLY, &h) != ESP_OK) {
            return -1;
        }
        std::size_t n = 0;
        const esp_err_t err = nvs_get_str(h, cle, nullptr, &n);
        nvs_close(h);
        return err == ESP_OK && n > 0 ? static_cast<std::int32_t>(n - 1) : -1; // n compte le zéro final
    }

    bool lire_texte(const char *espace, const char *cle, char *sortie, std::size_t capacite) override {
        nvs_handle_t h = 0;
        if (nvs_open(espace, NVS_READONLY, &h) != ESP_OK) {
            return false;
        }
        std::size_t n = capacite;
        const esp_err_t err = nvs_get_str(h, cle, sortie, &n);
        nvs_close(h);
        return err == ESP_OK;
    }

    bool ecrire_texte(const char *espace, const char *cle, const char *valeur) override {
        nvs_handle_t h = 0;
        esp_err_t err = nvs_open(espace, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_str(h, cle, valeur);
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
        return signaler(err, espace, cle);
    }

    bool effacer(const char *espace, const char *cle) override {
        nvs_handle_t h = 0;
        esp_err_t err = nvs_open(espace, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_erase_key(h, cle);
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                err = ESP_OK; // déjà absente : c'est le résultat voulu
            }
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
        return signaler(err, espace, cle);
    }

    bool effacer_espace(const char *espace) override {
        nvs_handle_t h = 0;
        esp_err_t err = nvs_open(espace, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_erase_all(h);
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
        return signaler(err, espace, "*");
    }

  private:
    // Le journal dit quel espace et quelle clé ont échoué, jamais la valeur.
    static bool signaler(esp_err_t err, const char *espace, const char *cle) {
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "mémoire permanente : %s/%s non écrit (%s)", espace, cle, esp_err_to_name(err));
        }
        return err == ESP_OK;
    }
};

// ---- le reste de l'appareil ---------------------------------------------------------------------------------------------

class PortsEsp final : public PortsAppareil {
  public:
    reglages::Affichage affichage() override { return reglages_affichage(); }
    bool enregistrer_affichage(const reglages::Affichage &nouveau) override {
        return reglages_enregistrer_affichage(nouveau);
    }
    bool zone_lire(reglages::Zone &zone) override { return reglages_zone_lire(zone); }
    bool zone_enregistrer(const reglages::Zone &zone) override { return reglages_zone_enregistrer(zone); }
    // Les alertes officielles : la source de la page web (main/reglages_alertes.*), qui prévient aussi le client réseau.
    reglages::Alertes alertes() override { return reglages_alertes(); }
    bool enregistrer_alertes(const reglages::Alertes &nouveau) override {
        return reglages_alertes_enregistrer(nouveau);
    }
    void source_meteo(char *sortie, std::size_t capacite) override { reglages_source_meteo(sortie, capacite); }
    bool source_meteo_enregistrer(std::string_view id) override { return reglages_source_meteo_enregistrer(id); }
    bool source_meteo_retablie() override { return reglages_source_meteo_retablie(); }

    // Une carte neuve : français, thème auto, 75 %, Lyon (mêmes valeurs que main/reglages.cpp et main/meteo_zone.cpp).
    void remettre_affichage_et_ville_par_defaut() override {
        reglages_enregistrer_affichage(reglages::Affichage{});
        reglages_source_meteo_remettre_auto(); // avant la ville : la ville par défaut n'a pas de pays connu
        reglages::Zone lyon;
        reglages::copier_texte("Lyon", lyon.nom, sizeof lyon.nom);
        reglages::copier_texte("Europe/Paris", lyon.fuseau, sizeof lyon.fuseau);
        lyon.latitude = 45.74906;
        lyon.longitude = 4.84789;
        reglages_zone_enregistrer(lyon);
    }

    MeteoVue meteo() override {
        MeteoEtat e = {};
        MeteoVue m;
        if (meteo_etat_lire(&e)) {
            m.valide = true;
            m.temperature_c = e.temperature_c;
            m.code_wmo = e.code_wmo;
            m.jour = e.jour;
            m.origine = meteo_etat_origine();
        }
        return m;
    }

    void wifi_scanner_lancer() override { reseau_scanner_lancer(); }
    bool wifi_scanner_en_cours() override { return reseau_scanner_en_cours(); }
    std::size_t wifi_scanner_resultats(PointAccesVu *sortie, std::size_t max) override {
        PointAcces vus[kReseauxMax];
        const std::size_t n = reseau_scanner_resultats(vus, max < kReseauxMax ? max : kReseauxMax);
        for (std::size_t i = 0; i < n; ++i) {
            std::memcpy(sortie[i].ssid, vus[i].ssid, sizeof sortie[i].ssid);
            sortie[i].dbm = vus[i].rssi_dbm;
            sortie[i].securise = vus[i].securise;
        }
        return n;
    }
    bool wifi_connecter(const char *ssid, const char *mot_de_passe, void (*rappel)(wifi::Resultat, void *),
                        void *contexte) override {
        return reseau_connecter(ssid, mot_de_passe, rappel, contexte);
    }
    void wifi_abandonner() override { reseau_abandonner(); }
    InfoWifi wifi_info() override {
        const EtatReseau e = reseau_etat();
        InfoWifi w;
        w.connecte = e.wifi_connecte && e.ip_obtenue;
        if (w.connecte) {
            reglages::copier_texte(e.ip, w.ip, sizeof w.ip);
            reseau_ssid_courant(w.ssid, sizeof w.ssid);
        }
        return w;
    }

    // La recherche de lieu passe par le seul client HTTPS du géocodage (main/geocodage.cpp : tâche d'arrière-plan, verrou TLS).
    void telecharger_lancer(const char *adresse) override { geocodage_ecran_lancer(adresse); }
    EtatTelechargement telecharger_etat() override { return geocodage_ecran_etat(); }
    std::string_view telecharger_corps() override { return geocodage_ecran_corps(); }
    void telecharger_liberer() override { geocodage_ecran_liberer(); }

    // La mer (ADR-0026) : le registre des spots est celui de la page web (main/reglages_mer.*, qui valide, écrit en NVS et réveille le
    // client réseau) ; la liste de Vigie est lue par une tâche à part (main/mer_liste_client.cpp).
    bool mer_registre(mer::Registre &sortie) override { return reglages_mer(sortie); }
    bool mer_enregistrer(const mer::Registre &nouveau) override { return reglages_mer_enregistrer(nouveau); }
    void mer_liste_lancer() override { mer_liste_lecture_lancer(); }
    ListeMer mer_liste_etat() override { return mer_liste_lecture_etat(); }
    bool mer_liste_copier(mer::liste::Liste &sortie) override { return mer_liste_lecture_copier(sortie); }

    void version(char *sortie, std::size_t capacite) override {
        const esp_app_desc_t *app = esp_app_get_description();
        reglages::copier_texte(app != nullptr ? app->version : "", sortie, capacite);
    }
    void code_appairage(char *sortie, std::size_t capacite) override {
        reglages::copier_texte(appairage_jeton(), sortie, capacite);
    }

    // Le code appartient au gestionnaire du serveur de la page : la demande est affichée tout de suite et appliquée par le serveur
    // avant sa prochaine requête (sessions fermées, verrou levé ; contrat dans appairage.h).
    void nouveau_code() override { appairage_demander_nouveau_code(); }

    void redemarrer() override {
        ESP_LOGW(TAG, "redémarrage logiciel demandé par l'écran de réglages (board_restart)");
        board_restart();
    }
    std::int64_t maintenant_ms() override { return esp_timer_get_time() / 1000; }

  private:
};

// L'hôte et ses deux portes, dans un seul bloc : ≈ 7,5 Ko (5 048 + 2 428 octets sur le Tab5, nm du 11/11/2026). En PSRAM et non en
// variables statiques, qui iraient en RAM interne : ils ne servent qu'aux tâches de l'interface et du serveur web, jamais à une
// interruption, au DMA, au Wi-Fi ou au TLS (les téléchargements passent par http_commun.cpp, avec ses propres tampons).
struct Objets {
    MagasinNvs magasin;
    PortsEsp ports;
    HoteAppareil hote;
    Objets() : hote(ports, magasin) {}
};

HoteAppareil *creer_hote() {
    void *place = heap_caps_calloc(1, sizeof(Objets), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (place == nullptr) {
        place = heap_caps_calloc(1, sizeof(Objets), MALLOC_CAP_8BIT);
    }
    if (place == nullptr) {
        // Au démarrage, avec 29 Mo de PSRAM libres : ne devrait jamais arriver. Sans hôte, ni l'écran des réglages ni la page web ne
        // peuvent marcher : on le dit et on redémarre (board_restart : remise à zéro complète, AGENTS.md).
        ESP_LOGE(TAG, "pas de place pour l'hôte de l'appareil (%u octets) : redémarrage",
                 static_cast<unsigned>(sizeof(Objets)));
        board_restart();
    }
    return &(new (place) Objets())->hote;
}

} // namespace

HoteReglages &hote_de_l_appareil() {
    // Créé au premier appel (l'initialisation d'une variable locale statique est protégée contre deux tâches).
    static HoteAppareil *const hote = creer_hote();
    return *hote;
}

} // namespace ecran
