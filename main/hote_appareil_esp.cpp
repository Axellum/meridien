// Les deux portes de l'hôte de l'appareil (ecran/hote_appareil.hpp) branchées sur ESP-IDF. Aucune règle ici : tout ce qui décide est
// dans le noyau pur et testé sur le PC. Ce fichier lit, écrit, appelle et rend la main.
//
//  - MagasinNvs   : la mémoire permanente (NVS), clés de 15 octets au plus, chaque écriture validée (nvs_commit) avant de rendre
//                   vrai. Aucune valeur n'est écrite au journal : les clés d'accès et le mot de passe Wi-Fi n'y passent jamais.
//  - PortsEsp     : l'affichage et la ville de la page web (main/reglages.*), la météo, le Wi-Fi (main/reseau_wifi.h), le
//                   téléchargement du géocodage (tâche à part, tampon en PSRAM), la version, le redémarrage.
#include "hote_appareil_esp.h"

#include <cstdio>
#include <cstring>

#include "board.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "appairage.h"
#include "ecran/hote_appareil.hpp"
#include "meteo_etat.h"
#include "meteo_interne.h"
#include "reglages.h"
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

// ---- téléchargement de la recherche de lieu --------------------------------------------------------------------------------

// Une seule adresse est joignable d'ici : le géocodage d'Open-Meteo (même liste d'autorisation que la page web). La réponse tient
// dans un tampon de PSRAM rendu dès qu'elle est lue ; la tâche de téléchargement n'existe que le temps d'une recherche (8 Ko de
// pile interne pendant la poignée de main TLS, puis rendus).
class Telechargeur {
  public:
    Telechargeur() : verrou_(xSemaphoreCreateMutex()) {}

    void lancer(const char *adresse) {
        xSemaphoreTake(verrou_, portMAX_DELAY);
        std::snprintf(url_, sizeof url_, "%s", adresse);
        ++generation_;
        etat_ = EtatTelechargement::EnCours;
        taille_ = 0;
        if (tampon_ == nullptr) {
            tampon_ = static_cast<char *>(heap_caps_malloc(kCorpsMax, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (tampon_ == nullptr) {
                tampon_ = static_cast<char *>(heap_caps_malloc(kCorpsMax, MALLOC_CAP_8BIT));
            }
        }
        if (tampon_ == nullptr) {
            ESP_LOGW(TAG, "recherche de lieu : pas de mémoire pour la réponse (%u octets)",
                     static_cast<unsigned>(kCorpsMax));
            etat_ = EtatTelechargement::Echec;
        } else if (!actif_) {
            actif_ = true;
            if (xTaskCreate(&Telechargeur::tache, "lieu_dl", 8192, this, 3, nullptr) != pdPASS) {
                actif_ = false;
                etat_ = EtatTelechargement::Echec;
                ESP_LOGW(TAG, "recherche de lieu : tâche non créée (mémoire interne)");
            }
        }
        xSemaphoreGive(verrou_);
    }

    EtatTelechargement etat() {
        xSemaphoreTake(verrou_, portMAX_DELAY);
        const EtatTelechargement e = etat_;
        xSemaphoreGive(verrou_);
        return e;
    }

    std::string_view corps() {
        xSemaphoreTake(verrou_, portMAX_DELAY);
        const std::string_view v = etat_ == EtatTelechargement::Termine && tampon_ != nullptr
                                       ? std::string_view(tampon_, taille_)
                                       : std::string_view();
        xSemaphoreGive(verrou_);
        return v;
    }

    // Oublie la demande et la réponse. Si la tâche travaille encore, elle verra que sa demande est périmée, rendra le tampon et
    // s'arrêtera ; sinon le tampon est rendu tout de suite.
    void liberer() {
        xSemaphoreTake(verrou_, portMAX_DELAY);
        ++generation_;
        url_[0] = '\0';
        taille_ = 0;
        etat_ = EtatTelechargement::Inactif;
        if (!actif_) {
            heap_caps_free(tampon_);
            tampon_ = nullptr;
        }
        xSemaphoreGive(verrou_);
    }

  private:
    static constexpr std::size_t kCorpsMax = 12 * 1024; // huit lieux tiennent en 4 Ko ; le reste est une marge
    static constexpr std::size_t kAdresseMax = api::ADRESSE_GEOCODAGE_MAX + 1;
    static constexpr int kDelaiMs = 8000;
    static constexpr std::string_view kAutorise = "https://geocoding-api.open-meteo.com/";

    struct Reception {
        char *donnees;
        std::size_t capacite;
        std::size_t n;
        bool trop;
    };

    static esp_err_t sur_evenement(esp_http_client_event_t *evt) {
        auto *rec = static_cast<Reception *>(evt->user_data);
        if (rec != nullptr && evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0 && evt->data != nullptr) {
            const std::size_t len = static_cast<std::size_t>(evt->data_len);
            if (rec->n + len > rec->capacite) {
                rec->trop = true; // plus grand que prévu : une réponse à moitié lue n'est jamais utilisée
            } else {
                std::memcpy(rec->donnees + rec->n, evt->data, len);
                rec->n += len;
            }
        }
        return ESP_OK;
    }

    // Faux « résultat » = EtatTelechargement ; `n` = nombre d'octets reçus si Termine.
    static EtatTelechargement telecharger(const char *adresse, char *tampon, std::size_t &n) {
        n = 0;
        if (std::string_view(adresse).substr(0, kAutorise.size()) != kAutorise) {
            return EtatTelechargement::Echec;
        }
        if (!reseau_etat().ip_obtenue) {
            return EtatTelechargement::SansInternet;
        }
        Reception rec = {tampon, kCorpsMax, 0, false};
        esp_http_client_config_t config = {};
        config.url = adresse;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = kDelaiMs;
        config.event_handler = &Telechargeur::sur_evenement;
        config.user_data = &rec;
        config.crt_bundle_attach = esp_crt_bundle_attach; // le serveur est vérifié (certificats racine d'ESP-IDF)
        config.buffer_size = 2048;
        config.buffer_size_tx = 1536;
        config.keep_alive_enable = false;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
            return EtatTelechargement::Echec;
        }
        esp_http_client_set_header(client, "Accept", "application/json");
        const esp_err_t err = esp_http_client_perform(client);
        const int statut = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "recherche de lieu : requête échouée (%s)", esp_err_to_name(err));
            return EtatTelechargement::SansInternet;
        }
        if (statut != 200 || rec.trop) {
            ESP_LOGW(TAG, "recherche de lieu : réponse refusée (HTTP %d%s)", statut, rec.trop ? ", trop grande" : "");
            return EtatTelechargement::Echec;
        }
        n = rec.n;
        return EtatTelechargement::Termine;
    }

    static void tache(void *arg) {
        auto *t = static_cast<Telechargeur *>(arg);
        for (;;) {
            char *adresse = t->travail_; // copie à part : la demande peut être remplacée pendant le téléchargement
            xSemaphoreTake(t->verrou_, portMAX_DELAY);
            std::snprintf(adresse, kAdresseMax, "%s", t->url_);
            const std::uint32_t ma_generation = t->generation_;
            char *tampon = t->tampon_;
            xSemaphoreGive(t->verrou_);

            std::size_t n = 0;
            EtatTelechargement fin = EtatTelechargement::Inactif;
            if (adresse[0] != '\0' && tampon != nullptr) {
                fin = telecharger(adresse, tampon, n);
            }

            xSemaphoreTake(t->verrou_, portMAX_DELAY);
            if (ma_generation == t->generation_) { // la demande est toujours celle de l'écran
                t->etat_ = fin == EtatTelechargement::Inactif ? EtatTelechargement::Echec : fin;
                t->taille_ = n;
                t->actif_ = false;
                xSemaphoreGive(t->verrou_);
                break;
            }
            if (t->url_[0] == '\0') { // demande annulée (liberer) : on rend le tampon et on s'arrête
                heap_caps_free(t->tampon_);
                t->tampon_ = nullptr;
                t->actif_ = false;
                xSemaphoreGive(t->verrou_);
                break;
            }
            xSemaphoreGive(t->verrou_); // une nouvelle recherche est arrivée pendant celle-ci : on la prend
        }
        vTaskDelete(nullptr);
    }

    SemaphoreHandle_t verrou_;
    char url_[kAdresseMax] = {};
    char travail_[kAdresseMax] = {}; // lu et écrit par la tâche seule
    std::uint32_t generation_ = 0;
    bool actif_ = false;
    EtatTelechargement etat_ = EtatTelechargement::Inactif;
    char *tampon_ = nullptr;
    std::size_t taille_ = 0;
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

    void telecharger_lancer(const char *adresse) override { dl_.lancer(adresse); }
    EtatTelechargement telecharger_etat() override { return dl_.etat(); }
    std::string_view telecharger_corps() override { return dl_.corps(); }
    void telecharger_liberer() override { dl_.liberer(); }

    void version(char *sortie, std::size_t capacite) override {
        const esp_app_desc_t *app = esp_app_get_description();
        reglages::copier_texte(app != nullptr ? app->version : "", sortie, capacite);
    }
    void code_appairage(char *sortie, std::size_t capacite) override {
        reglages::copier_texte(appairage_jeton(), sortie, capacite);
    }

    // NON FAIT : le code appartient au gestionnaire d'appairage du serveur de la page (main/serveur_web.cpp, reglages-appareil) ;
    // en tirer un autre ici montrerait à l'écran un code que la page n'accepterait pas. À brancher quand le serveur l'expose.
    void nouveau_code() override {
        ESP_LOGW(TAG, "nouveau code d'appairage demandé : pas encore branché sur le serveur de la page");
    }

    void redemarrer() override {
        ESP_LOGW(TAG, "redémarrage logiciel demandé par l'écran de réglages (board_restart)");
        board_restart();
    }
    std::int64_t maintenant_ms() override { return esp_timer_get_time() / 1000; }

  private:
    Telechargeur dl_;
};

} // namespace

HoteReglages &hote_de_l_appareil() {
    static MagasinNvs magasin;
    static PortsEsp ports;
    static HoteAppareil hote(ports, magasin);
    return hote;
}

} // namespace ecran
