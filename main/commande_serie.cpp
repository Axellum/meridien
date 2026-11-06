#include "commande_serie.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unistd.h>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ecran {

namespace {

const char *TAG = "commande_serie";

constexpr const char *kCommandeRedemarrer = "redemarrer";
constexpr const char *kCommandeMemoire = "memoire";
constexpr const char *kCommandeBlocs = "blocs";

// --- Budget de la mémoire interne (jalon 1b, provisoire) ---------------------------------------------------------------------
// « blocs » parcourt le tas interne (heap_caps_walk) et rend : le total utilisé et libre, le nombre de blocs par classe de taille,
// et les plus gros blocs utilisés (adresse, taille). Les piles de tâches, les tampons de dessin, les tampons TLS sont des blocs
// de taille reconnaissable : on les attribue par leur taille et par le journal de démarrage. Le parcours ne fait aucune
// allocation ni aucun affichage pendant qu'il tient le verrou du tas : il remplit un tableau fixe, imprimé ensuite.
constexpr int kBlocsGardes = 40;
constexpr int kClasses = 7;
constexpr uint32_t kBornesClasses[kClasses - 1] = {64, 256, 1024, 4096, 16384, 65536};

struct Bloc {
    uint32_t adresse;
    uint32_t taille;
};

struct Releve {
    Bloc gros[kBlocsGardes];
    int gardes;
    uint32_t utilise;
    uint32_t libre;
    uint32_t blocs_utilises;
    uint32_t blocs_libres;
    uint32_t libre_plus_gros;
    uint32_t classe_nombre[kClasses];
    uint32_t classe_octets[kClasses];
};

Releve g_releve;

bool parcourir(walker_heap_into_t, walker_block_info_t bloc, void *) {
    Releve &r = g_releve;
    const uint32_t taille = static_cast<uint32_t>(bloc.size);
    if (!bloc.used) {
        r.libre += taille;
        ++r.blocs_libres;
        if (taille > r.libre_plus_gros) {
            r.libre_plus_gros = taille;
        }
        return true;
    }
    r.utilise += taille;
    ++r.blocs_utilises;
    int classe = 0;
    while (classe < kClasses - 1 && taille >= kBornesClasses[classe]) {
        ++classe;
    }
    ++r.classe_nombre[classe];
    r.classe_octets[classe] += taille;
    if (r.gardes < kBlocsGardes) {
        r.gros[r.gardes++] = {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bloc.ptr)), taille};
    } else {
        int plus_petit = 0;
        for (int i = 1; i < kBlocsGardes; ++i) {
            if (r.gros[i].taille < r.gros[plus_petit].taille) {
                plus_petit = i;
            }
        }
        if (taille > r.gros[plus_petit].taille) {
            r.gros[plus_petit] = {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bloc.ptr)), taille};
        }
    }
    return true;
}

void journaliser_blocs() {
    g_releve = Releve{};
    heap_caps_walk(MALLOC_CAP_INTERNAL, parcourir, nullptr);
    const Releve &r = g_releve;
    ESP_LOGI(TAG, "BLOCS t_s=%lld utilise=%u (%u blocs) libre=%u (%u blocs, le plus gros %u)",
             static_cast<long long>(esp_timer_get_time() / 1000000), static_cast<unsigned>(r.utilise),
             static_cast<unsigned>(r.blocs_utilises), static_cast<unsigned>(r.libre),
             static_cast<unsigned>(r.blocs_libres), static_cast<unsigned>(r.libre_plus_gros));
    static const char *const noms[kClasses] = {"<64", "64-255", "256-1023", "1-4Ko", "4-16Ko", "16-64Ko", ">=64Ko"};
    for (int c = 0; c < kClasses; ++c) {
        ESP_LOGI(TAG, "BLOCS_CLASSE %s : %u blocs, %u octets", noms[c], static_cast<unsigned>(r.classe_nombre[c]),
                 static_cast<unsigned>(r.classe_octets[c]));
    }
    // Les plus gros d'abord (tri par sélection : 40 éléments).
    Bloc tries[kBlocsGardes];
    std::memcpy(tries, r.gros, sizeof(Bloc) * static_cast<std::size_t>(r.gardes));
    for (int i = 0; i < r.gardes; ++i) {
        int m = i;
        for (int j = i + 1; j < r.gardes; ++j) {
            if (tries[j].taille > tries[m].taille) {
                m = j;
            }
        }
        const Bloc temporaire = tries[i];
        tries[i] = tries[m];
        tries[m] = temporaire;
        ESP_LOGI(TAG, "BLOC rang=%d adresse=0x%08x taille=%u", i + 1, static_cast<unsigned>(tries[i].adresse),
                 static_cast<unsigned>(tries[i].taille));
    }
}

void traiter(const char *ligne) {
    if (std::strcmp(ligne, kCommandeMemoire) == 0) {
        // Une ligne « MEMOIRE … » par demande, lue par le banc du PC pour suivre la mémoire libre dans le temps (provisoire, comme
        // « redemarrer »). Rien ne tourne tant qu'elle n'est pas demandée : la mesure n'ajoute aucune activité à la carte.
        ESP_LOGI(TAG, "MEMOIRE t_s=%lld interne_libre=%u plus_gros_bloc=%u interne_min=%u psram_libre=%u dma_bloc=%u",
                 static_cast<long long>(esp_timer_get_time() / 1000000),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
        return;
    }
    if (std::strcmp(ligne, kCommandeBlocs) == 0) {
        journaliser_blocs();
        return;
    }
    if (std::strcmp(ligne, kCommandeRedemarrer) == 0) {
        ESP_LOGW(TAG, "redémarrage logiciel demandé sur la console (esp_rom_software_reset_system)");
        vTaskDelay(pdMS_TO_TICKS(200)); // laisser partir le journal sur l'USB
        board_restart();
    }
    ESP_LOGI(TAG, "commande inconnue ignorée");
}

// Console USB-Serial/JTAG sans pilote installé : stdin se lit en sondage, read() rend 0 (ou -1) quand rien n'est arrivé.
void tache_lecture(void *) {
    char ligne[32];
    std::size_t n = 0;
    for (;;) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\r' || c == '\n') {
            ligne[n] = '\0';
            if (n > 0) {
                traiter(ligne);
            }
            n = 0;
        } else if (n < sizeof(ligne) - 1) {
            ligne[n++] = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            n = 0; // ligne trop longue pour être une commande : on la jette
        }
    }
}

} // namespace

void memoire_jalon(const char *etape) {
    // Le delta est celui de la mémoire interne libre depuis l'appel précédent, TOUTES tâches confondues : pendant le démarrage les
    // tâches lancées plus tôt (réseau, serveur) allouent aussi, le delta d'une étape n'est donc exact que tant qu'elles dorment.
    static std::size_t precedent = 0;
    const std::size_t libre = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI("jalon_mem",
             "JALON_MEM t_ms=%lld etape=\"%s\" interne_libre=%u delta=%d plus_gros_bloc=%u dma_bloc=%u interne_min=%u",
             static_cast<long long>(esp_timer_get_time() / 1000), etape, static_cast<unsigned>(libre),
             precedent == 0 ? 0 : static_cast<int>(libre) - static_cast<int>(precedent),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
    precedent = libre;
}

void commande_serie_demarrer() {
    if (xTaskCreate(tache_lecture, "commande_serie", 3072, nullptr, 2, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "tâche de lecture de la console non créée");
    }
}

} // namespace ecran
