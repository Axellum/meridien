#include "http_commun.h"

#include <cstring>
#include <strings.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "miniz.h" // tinfl_decompress : ROM de l'ESP32-P4 (esp32p4.rom.ld), aucun code de décompression dans la flash

#include "ecran/gzip.hpp"
#include "verrou_tls.h"

namespace ecran::client_http {

namespace {

const char *TAG = "http";

constexpr size_t kTailleCodage =
    32; // « Content-Encoding » : « gzip », « x-gzip », « identity »… une valeur plus longue n'est pas gzip

// ---- une requête ---------------------------------------------------------------------------------------------------

struct Reception {
    uint8_t *donnees;
    size_t capacite;
    size_t n;
    bool trop;
    bool connecte;
    Memoire apres_connexion;    // mesurée une fois la connexion TLS établie
    char codage[kTailleCodage]; // valeur de l'en-tête Content-Encoding de la réponse finale (vide : absent)
    bool codage_trop_long;
};

esp_err_t sur_evenement_http(esp_http_client_event_t *evt) {
    auto *rec = static_cast<Reception *>(evt->user_data);
    if (rec == nullptr) {
        return ESP_OK;
    }
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        rec->connecte = true;
        rec->apres_connexion = mesurer_memoire();
        // Une redirection rouvre une connexion : ni le corps ni le Content-Encoding de la réponse précédente ne comptent plus.
        rec->n = 0;
        rec->trop = false;
        rec->codage[0] = '\0';
        rec->codage_trop_long = false;
        break;
    case HTTP_EVENT_ON_HEADER:
        if (evt->header_key != nullptr && evt->header_value != nullptr &&
            strcasecmp(evt->header_key, "Content-Encoding") == 0) {
            const size_t len = std::strlen(evt->header_value);
            if (len >= sizeof(rec->codage)) {
                rec->codage[0] = '\0';
                rec->codage_trop_long = true; // pas un codage connu : traité comme inconnu plus bas
            } else {
                std::memcpy(rec->codage, evt->header_value, len + 1);
                rec->codage_trop_long = false;
            }
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0 && evt->data != nullptr) {
            const size_t len = static_cast<size_t>(evt->data_len);
            if (rec->n + len > rec->capacite) {
                rec->trop = true; // réponse plus grande que prévu : on ne la lit pas à moitié
            } else {
                std::memcpy(rec->donnees + rec->n, evt->data, len);
                rec->n += len;
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

bool vide(const char *texte) { return texte == nullptr || texte[0] == '\0'; }

// ---- décompression -------------------------------------------------------------------------------------------------

struct Decompresse {
    Issue issue = Issue::Illisible; // Ok : le texte est prêt dans le tampon de sortie
    size_t octets = 0;              // octets rendus par `tinfl`
    size_t consommes = 0;           // octets du flux DEFLATE que `tinfl` a lus
    size_t flux = 0;                // octets du flux DEFLATE dans la réponse (en-tête et pied exclus)
    int64_t duree_us = 0;
};

void journaliser_refus_gzip(const char *quoi, gzip::Erreur e) {
    const std::string_view texte = gzip::libelle(e);
    ESP_LOGW(TAG, "réponse gzip refusée (%s) : %.*s", quoi, static_cast<int>(texte.size()), texte.data());
}

// Jamais « en ligne » : le décompresseur (≈ 11 Ko) est pris en PSRAM, mais l'appel garde ses variables hors de la fonction qui tient
// le tampon de réception. `fil` est la réponse reçue ENTIÈRE (en-tête gzip, flux DEFLATE, pied) ; le texte va dans `sortie`.
__attribute__((noinline)) Decompresse decompresser(const uint8_t *fil, size_t n, uint8_t *sortie, size_t capacite) {
    Decompresse d;
    const gzip::Separation sep = gzip::separer(fil, n);
    if (!sep.ok()) {
        journaliser_refus_gzip("en-tête ou pied", sep.erreur);
        return d;
    }
    d.flux = sep.taille_flux;

    // 10 992 octets sur ce RISC-V (mesuré avec le compilateur d'ESP-IDF : 3 tables de Huffman de 3 488 octets) : presque toute la
    // pile de 12 Ko de la tâche, qui doit déjà tenir la prise de contact TLS. D'où la PSRAM.
    auto *tinfl = static_cast<tinfl_decompressor *>(
        heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (tinfl == nullptr) {
        ESP_LOGE(TAG, "pas de place en PSRAM pour le décompresseur (%u octets)",
                 static_cast<unsigned>(sizeof(tinfl_decompressor)));
        d.issue = Issue::Memoire;
        return d;
    }
    tinfl_init(tinfl);
    // Flux DEFLATE BRUT (pas d'en-tête zlib : TINFL_FLAG_PARSE_ZLIB_HEADER absent), toute l'entrée fournie d'un coup
    // (TINFL_FLAG_HAS_MORE_INPUT absent), sortie d'un seul tenant assez grande pour tout le flux (NON_WRAPPING).
    size_t entree = sep.taille_flux;
    size_t ecrit = capacite;
    const int64_t debut_us = esp_timer_get_time();
    const tinfl_status etat = tinfl_decompress(tinfl, fil + sep.debut_flux, &entree, sortie, sortie, &ecrit,
                                               TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    d.duree_us = esp_timer_get_time() - debut_us;
    heap_caps_free(tinfl);
    d.consommes = entree;
    d.octets = ecrit;

    if (etat == TINFL_STATUS_HAS_MORE_OUTPUT) {
        // Le texte gonfle au-delà du tampon (réponse énorme, ou « bombe » gzip) : refusé, jamais lu à moitié.
        ESP_LOGW(TAG, "réponse gzip refusée : le texte dépasse le tampon de %u octets",
                 static_cast<unsigned>(capacite));
        d.issue = Issue::TropGrand;
        return d;
    }
    if (etat != TINFL_STATUS_DONE) {
        ESP_LOGW(TAG, "réponse gzip refusée : flux DEFLATE invalide ou incomplet (statut tinfl %d)",
                 static_cast<int>(etat));
        return d;
    }
    // Un second membre gzip ou des octets en trop derrière le premier ne passent pas ce contrôle : le pied lu (les 8 derniers
    // octets) n'est pas celui du texte rendu (ecran/gzip.hpp).
    const gzip::Erreur e = gzip::verifier_donnees(sep, sortie, ecrit);
    if (e != gzip::Erreur::Aucune) {
        journaliser_refus_gzip("contenu", e);
        return d;
    }
    d.issue = Issue::Ok;
    return d;
}

// Lit la réponse 200 reçue dans `rec` : décide si elle est compressée, la décompresse au besoin, puis la rend à `lire`. Remplit
// `r.issue`, `r.octets` et `r.gzip`. Les tampons de sortie sont rendus avant le retour.
__attribute__((noinline)) void interpreter(const Reception &rec, LireCorps lire, void *utilisateur, Reponse &r) {
    const gzip::Codage annonce = rec.codage_trop_long ? gzip::Codage::Inconnu : gzip::codage_de(rec.codage);
    const bool signature = gzip::ressemble_a_gzip(rec.donnees, rec.n);
    const gzip::Decision decision = gzip::decider(annonce, signature);
    if (decision == gzip::Decision::Refus) {
        ESP_LOGW(TAG, "réponse refusée : Content-Encoding « %s »%s, signature gzip %s", rec.codage,
                 rec.codage_trop_long ? " (trop long)" : "", signature ? "présente" : "absente");
        r.issue = Issue::Illisible;
        return;
    }

    if (decision == gzip::Decision::Brut) {
        r.octets = rec.n;
        ESP_LOGI(TAG, "réponse : HTTP %d, non compressée, %u octets sur le fil", r.statut_http,
                 static_cast<unsigned>(rec.n));
        r.issue = lire(std::string_view(reinterpret_cast<const char *>(rec.donnees), rec.n), utilisateur)
                      ? Issue::Ok
                      : Issue::Illisible;
        return;
    }

    r.gzip = true;
    auto *texte = static_cast<uint8_t *>(heap_caps_malloc(kTailleTampon, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (texte == nullptr) {
        r.issue = Issue::Memoire;
        return;
    }
    const Decompresse d = decompresser(rec.donnees, rec.n, texte, kTailleTampon);
    if (d.issue != Issue::Ok) {
        heap_caps_free(texte);
        r.issue = d.issue;
        return;
    }
    r.octets = d.octets;
    // Les deux tailles, pour juger du gain sur la carte : octets sur le fil, octets décompressés, part du fil en pour cent.
    ESP_LOGI(TAG,
             "réponse : HTTP %d, gzip, %u octets sur le fil, %u octets décompressés (%u %% du texte), décompression "
             "%lld µs ; "
             "flux DEFLATE lu sur %u octets de %u",
             r.statut_http, static_cast<unsigned>(rec.n), static_cast<unsigned>(d.octets),
             d.octets > 0 ? static_cast<unsigned>(rec.n * 100U / d.octets) : 0U, static_cast<long long>(d.duree_us),
             static_cast<unsigned>(d.consommes), static_cast<unsigned>(d.flux));
    r.issue = lire(std::string_view(reinterpret_cast<const char *>(texte), d.octets), utilisateur) ? Issue::Ok
                                                                                                   : Issue::Illisible;
    heap_caps_free(texte);
}

} // namespace

Memoire mesurer_memoire() {
    Memoire m;
    m.interne = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    m.interne_bloc = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    m.interne_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    m.psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    m.psram_bloc = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    return m;
}

Reponse obtenir(const Requete &requete, LireCorps lire, void *utilisateur) {
    Reponse r;
    if (vide(requete.adresse) || vide(requete.user_agent) || vide(requete.accept) || lire == nullptr) {
        ESP_LOGE(TAG, "requête non envoyée : adresse, User-Agent, Accept ou fonction de lecture absents");
        r.issue = Issue::Requete;
        return r;
    }

    auto *recu = static_cast<uint8_t *>(heap_caps_malloc(kTailleTampon, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (recu == nullptr) {
        r.issue = Issue::Memoire;
        return r;
    }
    Reception rec = {};
    rec.donnees = recu;
    rec.capacite = kTailleTampon;

    esp_http_client_config_t config = {};
    config.url = requete.adresse;
    config.method = HTTP_METHOD_GET;
    config.user_agent = requete.user_agent;
    config.timeout_ms = requete.delai_ms;
    config.event_handler = sur_evenement_http;
    config.user_data = &rec;
    config.crt_bundle_attach =
        esp_crt_bundle_attach;    // certificats racine du paquet d'ESP-IDF (Mozilla) : le serveur est vérifié
    config.buffer_size = 2048;    // en-têtes de réponse
    config.buffer_size_tx = 1024; // ligne de requête : 885 octets au plus avec le plus long `models=` d'Open-Meteo
                                  // (test_meteo_appareil.cpp, ADR-0010) ; 512 par défaut ne suffit pas
    config.keep_alive_enable = false;

    // Un seul TLS à la fois dans tout le firmware (verrou_tls.h) : la poignée de main demande ≈ 65 Ko de RAM interne, et la carte
    // n'en a pas assez pour deux (une recherche de ville et cette requête, par exemple). Le verrou couvre exactement
    // esp_http_client_init -> esp_http_client_cleanup ; il n'est pas tenu pendant l'interprétation de la réponse (gzip, appel de
    // l'appelant), qui a lieu après.
    if (!verrou_tls_prendre(requete.attente_verrou_tls_ms)) {
        ESP_LOGW(TAG, "verrou TLS occupé depuis plus de %u ms : essai reporté",
                 static_cast<unsigned>(requete.attente_verrou_tls_ms));
        heap_caps_free(recu);
        r.issue = Issue::Reseau;
        return r;
    }
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        verrou_tls_rendre();
        heap_caps_free(recu);
        r.issue = Issue::Reseau;
        return r;
    }
    esp_http_client_set_header(client, "Accept", requete.accept);
    // Le seul codage que l'appareil sait lire : gzip, par la ROM (tinfl). Sans cet en-tête le serveur répond en clair.
    esp_http_client_set_header(client, "Accept-Encoding", "gzip");

    const int64_t debut_us = esp_timer_get_time();
    const esp_err_t err = esp_http_client_perform(client);
    r.duree_ms = static_cast<int>((esp_timer_get_time() - debut_us) / 1000);
    r.statut_http = esp_http_client_get_status_code(client);
    const int erreur_socket = esp_http_client_get_errno(client);
    esp_http_client_cleanup(client);
    verrou_tls_rendre();
    r.octets_fil = rec.n;
    r.connecte = rec.connecte;
    r.apres_connexion = rec.apres_connexion;

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "requête échouée : %s (errno %d) après %d ms", esp_err_to_name(err), erreur_socket, r.duree_ms);
        r.issue = Issue::Reseau;
    } else if (r.statut_http == 429) {
        r.issue = Issue::TropDeRequetes;
    } else if (r.statut_http != 200) {
        r.issue = Issue::Http;
    } else if (rec.trop) {
        r.issue = Issue::TropGrand;
    } else {
        interpreter(rec, lire, utilisateur, r);
    }
    heap_caps_free(recu);
    return r;
}

} // namespace ecran::client_http
