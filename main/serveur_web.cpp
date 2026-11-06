// Serveur de la page de réglages (contrat main/serveur_web.h).
//
// Ce fichier ne décide de rien : il lit une requête HTTP, appelle `ecran::api::Api::traiter` (noyau pur, testé sur le PC avec des
// mutations : appairage, validations, forme des réponses, phrases dans la langue de l'écran) et écrit la réponse. Les fichiers de
// la page (web/app) sont servis tels quels, compressés en gzip à la compilation (tools/embarquer_web.py), avec une étiquette de
// cache : le navigateur ne retélécharge que ce qui a changé.
//
// Un seul fil de traitement (celui d'esp_http_server) : les tampons ci-dessous ne sont donc jamais utilisés à deux endroits à la
// fois. Le géocodage (recherche d'une ville) met ce fil en attente le temps de la requête HTTPS vers Open-Meteo (délai de 8 s,
// plus l'attente éventuelle du verrou TLS commun, 30 s au plus) : une seconde requête de la page attend. Le téléchargement lui-même
// se fait dans la tâche de main/geocodage.cpp. Rien d'ici n'écrit le code d'appairage, un jeton, l'SSID ou le mot de passe Wi-Fi
// au journal.
#include "serveur_web.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string_view>

#include "board.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"

#include "appairage.h"
#include "ecran/api_reglages.hpp"
#include "ecran/http_requete.hpp"
#include "geocodage.h"
#include "meteo_etat.h"
#include "meteo_zone.h"
#include "reglages.h"
#include "reseau.h"
#include "ui_appairage.h"
#include "web_fichiers.h"

namespace ecran {

namespace {

const char *TAG = "serveur_web";

constexpr uint16_t kPort = 80;
constexpr size_t kTailleSortie = 8192;   // une réponse JSON (huit lieux ≈ 3 Ko)
constexpr size_t kTailleTravail = 16384; // réponse brute d'Open-Meteo (≈ 5 Ko pour huit lieux)
// Pile du fil du serveur. Elle a été portée à 20 Ko quand la prise de contact TLS du géocodage s'y exécutait (≈ 10 Ko pour mbedtls,
// plus ≈ 4 Ko de tampons locaux de l'API). Ce TLS est maintenant dans la tâche de main/geocodage.cpp : la marge peut être réduite
// (RAM interne rare), mais seulement après avoir mesuré la plus basse marge réelle de ce fil sur la carte, non faite ici.
constexpr uint32_t kPileServeur = 20480;
constexpr uint32_t kPileTache = 12288; // la tâche qui démarre le serveur crée aussi les polices du bandeau (TinyTTF)
constexpr UBaseType_t kPrioriteServeur = 3;
constexpr uint32_t kPasAttenteIpMs = 1000;
constexpr uint32_t kPasSurveillanceMs = 2000;

char g_nom[16] = {};                   // « ecran-f6b6 »
char g_corps[api::CORPS_MAX + 1] = {}; // corps de la requête en cours (un octet de plus : voir sur_api)
char *g_sortie = nullptr;
char *g_travail = nullptr;
httpd_handle_t g_serveur = nullptr;

// ---- l'appareil vu par l'API ---------------------------------------------------------------------------------------------

class HoteAppareil {
  public:
    reglages::Affichage affichage() const { return reglages_affichage(); }
    bool enregistrer_affichage(const reglages::Affichage &a) { return reglages_enregistrer_affichage(a); }
    bool zone_lire(reglages::Zone &z) { return reglages_zone_lire(z); }
    bool zone_enregistrer(const reglages::Zone &z) { return reglages_zone_enregistrer(z); }

    bool meteo_lire(api::MeteoApercu &m) {
        MeteoEtat e = {};
        if (!meteo_etat_lire(&e)) {
            return false;
        }
        m.valide = true;
        m.perimee = e.perime;
        m.maj_epoch_s = e.maj_epoch_s;
        m.temperature_c = static_cast<double>(e.temperature_c);
        m.code_wmo = e.code_wmo;
        m.jour = e.jour;
        return true;
    }

    api::InfoSysteme systeme() {
        const EtatReseau reseau = reseau_etat();
        api::InfoSysteme s;
        s.nom = g_nom;
        s.modele = "M5Stack Tab5";
        const esp_app_desc_t *app = esp_app_get_description();
        if (app != nullptr) {
            std::snprintf(version_, sizeof(version_), "%s", app->version);
            s.version = version_;
            if (date_iso(app->date, app->time, compile_, sizeof(compile_))) {
                s.compile_le = compile_;
            }
        }
        const esp_partition_t *partition = esp_ota_get_running_partition();
        if (partition != nullptr) {
            std::snprintf(partition_, sizeof(partition_), "%s", partition->label);
            s.partition = partition_;
        }
        s.validee =
            true; // pas encore de retour arrière automatique : une version qui répond est considérée comme validée
        s.depuis_secondes = esp_timer_get_time() / 1000000;
        s.memoire_libre = static_cast<int64_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        s.heure_synchronisee = reseau.heure_synchronisee;
        s.heure_epoch = reseau.heure_synchronisee ? static_cast<int64_t>(std::time(nullptr)) : 0;
        ZoneMeteo zone = {};
        if (::zone_lire(&zone)) {
            std::snprintf(fuseau_, sizeof(fuseau_), "%s", zone.fuseau);
            s.fuseau = fuseau_;
        }
        s.internet = reseau.ip_obtenue && reseau.heure_synchronisee; // l'heure vient d'Internet : preuve d'un accès
        return s;
    }

    api::InfoWifi wifi() {
        const EtatReseau reseau = reseau_etat();
        api::InfoWifi w;
        if (reseau.wifi_connecte) {
            w.etat = api::EtatWifi::Connecte;
            wifi_ap_record_t point = {};
            if (esp_wifi_sta_get_ap_info(&point) == ESP_OK) {
                w.force_dbm = point.rssi;
                // Le nom du réseau n'est rendu qu'à une page appairée (la route est protégée) ; il doit être de l'UTF-8 valide.
                const char *ssid = reinterpret_cast<const char *>(point.ssid);
                if (reglages::utf8_valide(ssid)) {
                    reglages::copier_texte(ssid, w.ssid, sizeof(w.ssid));
                }
            }
        } else if (reseau.coprocesseur_echec || !reseau.identifiants_presents) {
            w.etat = api::EtatWifi::Echec;
        } else {
            w.etat = api::EtatWifi::Connexion;
        }
        reglages::copier_texte(reseau.ip, w.ip, sizeof(w.ip));
        return w;
    }

    // Le téléchargement du géocodage (HTTPS, verrou TLS commun, seule l'adresse d'Open-Meteo autorisée) se fait dans la tâche du
    // module main/geocodage.cpp, à la priorité d'arrière-plan ; ce fil-ci attend la réponse sans calculer.
    api::Telechargement telecharger(std::string_view url, char *tampon, std::size_t capacite, std::size_t &taille) {
        return geocodage_telecharger(url, tampon, capacite, taille);
    }

    std::int64_t maintenant_ms() { return esp_timer_get_time() / 1000; }
    void aleatoire(std::uint8_t *sortie, std::size_t n) { esp_fill_random(sortie, n); }

  private:
    // « Nov  6 2026 » + « 11:20:00 » (__DATE__ et __TIME__ de la compilation) -> « 2026-11-06T11:20:00 ». C'est l'heure locale du PC
    // de compilation, sans « Z » : ce n'est pas de l'UTC.
    static bool date_iso(const char *date, const char *heure, char *sortie, size_t taille) {
        static const char kMois[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        if (date == nullptr || heure == nullptr || std::strlen(date) < 11 || std::strlen(heure) < 8) {
            return false;
        }
        for (int i = 0; i < 12; ++i) {
            if (std::strncmp(date, kMois + 3 * i, 3) == 0) {
                const int jour = std::atoi(date + 4);
                const int annee = std::atoi(date + 7);
                std::snprintf(sortie, taille, "%04d-%02d-%02dT%.8s", annee, i + 1, jour, heure);
                return true;
            }
        }
        return false;
    }

    char version_[32] = {};
    char compile_[24] = {};
    char partition_[20] = {};
    char fuseau_[40] = {};
};

HoteAppareil g_hote;
api::Api<HoteAppareil> g_api(g_hote);

// ---- requêtes ------------------------------------------------------------------------------------------------------------

const char *nom_methode(int m) {
    switch (m) {
    case HTTP_GET:
        return "GET";
    case HTTP_POST:
        return "POST";
    case HTTP_PUT:
        return "PUT";
    case HTTP_DELETE:
        return "DELETE";
    default:
        return "?";
    }
}

api::Methode methode_api(int m) {
    switch (m) {
    case HTTP_GET:
        return api::Methode::Get;
    case HTTP_POST:
        return api::Methode::Post;
    case HTTP_PUT:
        return api::Methode::Put;
    case HTTP_DELETE:
        return api::Methode::Delete;
    default:
        return api::Methode::Autre;
    }
}

// Le jeton de l'en-tête « Authorization: Bearer … » (vide s'il manque, est trop long ou mal formé).
std::string_view lire_jeton(httpd_req_t *req, char *tampon, size_t taille) {
    const size_t n = httpd_req_get_hdr_value_len(req, "Authorization");
    if (n == 0 || n >= taille) {
        return {};
    }
    if (httpd_req_get_hdr_value_str(req, "Authorization", tampon, taille) != ESP_OK) {
        return {};
    }
    return http::jeton_bearer(tampon);
}

// Lit tout le corps (au plus CORPS_MAX octets) dans g_corps. Faux : la connexion est inutilisable.
bool lire_corps(httpd_req_t *req, size_t longueur) {
    size_t recu = 0;
    while (recu < longueur) {
        const int r = httpd_req_recv(req, g_corps + recu, longueur - recu);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, nullptr);
            return false;
        }
        if (r <= 0) {
            return false;
        }
        recu += static_cast<size_t>(r);
    }
    return true;
}

esp_err_t repondre_api(httpd_req_t *req, const api::Reponse &rep) {
    httpd_resp_set_status(req, http::ligne_statut(rep.statut));
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    char attente[12] = {};
    if (rep.statut == 429 && rep.reessayer_dans > 0) {
        std::snprintf(attente, sizeof(attente), "%d", rep.reessayer_dans);
        httpd_resp_set_hdr(req, "Retry-After", attente);
    }
    return httpd_resp_send(req, rep.corps.data(), static_cast<ssize_t>(rep.corps.size()));
}

esp_err_t sur_api(httpd_req_t *req) {
    const http::Uri uri = http::separer_uri(req->uri);
    char entete[160];
    const std::string_view jeton = lire_jeton(req, entete, sizeof(entete));

    api::Requete rq;
    rq.methode = methode_api(req->method);
    rq.chemin = uri.chemin;
    rq.requete = uri.requete;
    rq.jeton = jeton;

    const size_t longueur = req->content_len;
    const bool trop_gros = longueur > api::CORPS_MAX;
    if (trop_gros) {
        // Le corps n'est pas lu : l'API répond 413 dès qu'elle voit une taille au-delà de CORPS_MAX (elle ne lit pas les octets).
        rq.corps = std::string_view(g_corps, api::CORPS_MAX + 1);
    } else {
        if (!lire_corps(req, longueur)) {
            return ESP_FAIL;
        }
        rq.corps = std::string_view(g_corps, longueur);
    }

    api::Tampons t;
    t.sortie = g_sortie;
    t.cap_sortie = kTailleSortie;
    t.travail = g_travail;
    t.cap_travail = kTailleTravail;
    const api::Reponse rep = g_api.traiter(rq, t);
    ESP_LOGI(TAG, "%s %.*s -> %d", nom_methode(req->method), static_cast<int>(uri.chemin.size()), uri.chemin.data(),
             rep.statut);
    const esp_err_t err = repondre_api(req, rep);
    if (trop_gros) {
        // Le corps non lu fausserait la requête suivante de cette connexion : on la ferme.
        httpd_sess_trigger_close(req->handle, httpd_req_to_sockfd(req));
    }
    return err;
}

esp_err_t sur_fichier(httpd_req_t *req) {
    const http::Uri uri = http::separer_uri(req->uri);
    const std::string_view chemin = uri.chemin == "/" ? std::string_view("/index.html") : uri.chemin;
    const FichierWeb *fichier = nullptr;
    for (unsigned i = 0; i < kNombreFichiersWeb; ++i) {
        if (chemin == kFichiersWeb[i].chemin) {
            fichier = &kFichiersWeb[i];
            break;
        }
    }
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
    if (fichier == nullptr) {
        ESP_LOGI(TAG, "GET %.*s -> 404", static_cast<int>(chemin.size()), chemin.data());
        httpd_resp_set_status(req, http::ligne_statut(404));
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_send(req, "Introuvable", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_type(req, fichier->type);
    httpd_resp_set_hdr(req, "ETag", fichier->etag);
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-cache"); // revalider à chaque fois : l'étiquette rend la réponse minuscule
    httpd_resp_set_hdr(req, "Content-Security-Policy", "frame-ancestors 'none'");

    char deja[96] = {};
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", deja, sizeof(deja)) == ESP_OK &&
        http::etag_correspond(deja, fichier->etag)) {
        ESP_LOGD(TAG, "GET %s -> 304", fichier->chemin);
        httpd_resp_set_status(req, http::ligne_statut(304));
        return httpd_resp_send(req, nullptr, 0);
    }
    // Toujours en gzip : tout navigateur le comprend, et l'appareil n'a rien à décompresser.
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    ESP_LOGD(TAG, "GET %s -> 200 (%u octets gzip)", fichier->chemin, static_cast<unsigned>(fichier->taille_gzip));
    return httpd_resp_send(req, reinterpret_cast<const char *>(fichier->gzip),
                           static_cast<ssize_t>(fichier->taille_gzip));
}

// ---- démarrage -----------------------------------------------------------------------------------------------------------

char *allouer(size_t taille) {
    void *p = heap_caps_malloc(taille, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == nullptr) {
        p = heap_caps_malloc(taille, MALLOC_CAP_8BIT);
    }
    return static_cast<char *>(p);
}

bool demarrer_http() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kPort;
    config.stack_size = kPileServeur;
    config.task_priority = kPrioriteServeur;
    config.max_uri_handlers = 8;
    // 5 connexions : la table des sockets de lwip en compte 10 (3 réservées à esp_http_server) et la météo et le géocodage en
    // prennent chacun une pour sortir.
    config.max_open_sockets = 5;
    config.lru_purge_enable = true; // un téléphone laisse des connexions ouvertes : la plus ancienne cède la place
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    config.uri_match_fn = httpd_uri_match_wildcard;
    const esp_err_t err = httpd_start(&g_serveur, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_http_server non démarré : %s", esp_err_to_name(err));
        return false;
    }
    // L'ordre compte : la première inscription qui correspond répond. L'API d'abord, puis tous les fichiers de la page.
    const struct {
        httpd_method_t methode;
        const char *chemin;
        esp_err_t (*gestionnaire)(httpd_req_t *);
    } routes[] = {
        {HTTP_GET, "/api/*", sur_api},    {HTTP_POST, "/api/*", sur_api}, {HTTP_PUT, "/api/*", sur_api},
        {HTTP_DELETE, "/api/*", sur_api}, {HTTP_GET, "/*", sur_fichier},
    };
    for (const auto &r : routes) {
        httpd_uri_t u = {};
        u.uri = r.chemin;
        u.method = r.methode;
        u.handler = r.gestionnaire;
        u.user_ctx = nullptr;
        const esp_err_t e = httpd_register_uri_handler(g_serveur, &u);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "route %s non inscrite : %s", r.chemin, esp_err_to_name(e));
            httpd_stop(g_serveur);
            g_serveur = nullptr;
            return false;
        }
    }
    return true;
}

bool demarrer_mdns() {
    esp_err_t err = mdns_init();
    if (err == ESP_OK) {
        err = mdns_hostname_set(g_nom);
    }
    if (err == ESP_OK) {
        err = mdns_instance_name_set("Écran autonome");
    }
    if (err == ESP_OK) {
        err = mdns_service_add(nullptr, "_http", "_tcp", kPort, nullptr, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS non démarré : %s (la page reste joignable par son adresse IP)", esp_err_to_name(err));
        return false;
    }
    return true;
}

void afficher_bandeau(const char *ip) {
    if (board_display_lock(2000)) {
        ui_appairage_afficher(board_get_display(), ip, g_nom, appairage_jeton());
        board_display_unlock();
    } else {
        ESP_LOGW(TAG, "bandeau d'appairage : verrou d'affichage non obtenu");
    }
}

void tache_serveur(void *) {
    // 1. Attendre l'adresse IP (jalon 1b). Sans Wi-Fi, il n'y a pas de page.
    while (!reseau_etat().ip_obtenue) {
        vTaskDelay(pdMS_TO_TICKS(kPasAttenteIpMs));
    }

    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        std::snprintf(g_nom, sizeof(g_nom), "ecran-%02x%02x", mac[4], mac[5]);
    } else {
        std::snprintf(g_nom, sizeof(g_nom), "ecran");
    }

    g_sortie = allouer(kTailleSortie);
    g_travail = allouer(kTailleTravail);
    if (g_sortie == nullptr || g_travail == nullptr) {
        ESP_LOGE(TAG, "tampons du serveur non alloués : la page n'est pas servie");
        vTaskDelete(nullptr);
        return;
    }

    // 2. Le code d'appairage est tiré avant d'ouvrir le port : aucune requête ne peut tomber sur un appareil sans code.
    g_api.appairage().nouveau_code(esp_random());
    char code[8] = {};
    const std::string_view c = g_api.appairage().code();
    std::memcpy(code, c.data(), c.size() < sizeof(code) - 1 ? c.size() : sizeof(code) - 1);
    appairage_publier_code(code);

    if (!demarrer_http()) {
        vTaskDelete(nullptr);
        return;
    }
    const bool mdns = demarrer_mdns();

    char ip[16] = {};
    std::memcpy(ip, reseau_etat().ip, sizeof(ip));
    ESP_LOGI(TAG, "page de réglages servie : http://%s/ (port %u) ; mDNS %s%s ; %u fichiers, %u octets gzip", ip,
             static_cast<unsigned>(kPort), mdns ? g_nom : "non démarré", mdns ? ".local" : "",
             static_cast<unsigned>(kNombreFichiersWeb), static_cast<unsigned>(kPoidsGzipWeb));
    afficher_bandeau(ip);

    // 3. Si l'adresse change (bail DHCP renouvelé avec une autre adresse), le bandeau la suit.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(kPasSurveillanceMs));
        const EtatReseau etat = reseau_etat();
        if (etat.ip_obtenue && etat.ip[0] != '\0' && std::strncmp(etat.ip, ip, sizeof(ip)) != 0) {
            std::memcpy(ip, etat.ip, sizeof(ip));
            ESP_LOGI(TAG, "adresse changée : http://%s/", ip);
            afficher_bandeau(ip);
        }
    }
}

} // namespace

void serveur_web_demarrer() {
    if (xTaskCreate(tache_serveur, "serveur_web", kPileTache, nullptr, kPrioriteServeur, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "tâche du serveur non créée : la page n'est pas servie");
    }
}

} // namespace ecran
