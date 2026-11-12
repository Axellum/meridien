// Allocateur de LVGL : tout l'interface (objets, styles, polices TinyTTF, étiquettes) est allouée en PSRAM, en mémoire interne
// seulement si la PSRAM est pleine. Activé par CONFIG_LV_USE_CUSTOM_MALLOC=y (sdkconfig.defaults) ; LVGL appelle ces fonctions à la
// place de malloc/realloc/free.
//
// Pourquoi : avec le malloc du système (LV_USE_CLIB_MALLOC), tout bloc de MOINS de 256 octets
// (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL, boards/tab5/sdkconfig.defaults.board) tombe en mémoire interne. Or un objet, un style ou une
// étiquette LVGL pèse 100 à 250 octets : l'écran principal coûtait 52 Ko internes (budget-memoire-interne-jalon-1b), les écrans de
// réglages encore une quarantaine. Le Wi-Fi (liaison avec le C6) et le TLS ne vivent QUE en mémoire interne : le 07/11/2026, avec les
// réglages ouverts (9,7 Ko libres), le scan Wi-Fi échouait à créer un sémaphore (« sem create failed ») et le pilote du C6 plantait
// (deux redémarrages), et la recherche de ville échouait à ouvrir sa connexion (« Failed to allocate memory »).
// L'interface n'a besoin ni de DMA ni d'une mémoire plus rapide que la PSRAM derrière le cache : elle va en PSRAM.
//
// Ce que cela ne change pas : les tampons d'écran (DMA, internes), les piles de tâches, tout ce qui est demandé avec
// MALLOC_CAP_INTERNAL ou MALLOC_CAP_DMA. Les gros blocs (≥ 256 o) allaient déjà en PSRAM.
#include "lvgl_memoire.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esp_heap_caps.h"
#include "lvgl.h" // prototypes lv_malloc_core & co (src/stdlib/lv_mem.h), liaison C

namespace {

constexpr uint32_t kPsram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr uint32_t kInterne = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

// Compteurs de ce que LVGL tient (commande série « memoire ») : LVGL n'alloue que dans sa tâche, la commande lit depuis la sienne.
std::atomic<std::uint32_t> g_octets{0};
std::atomic<std::uint32_t> g_blocs{0};

void compter_ajout(void *p) {
    if (p != nullptr) {
        g_octets.fetch_add(static_cast<std::uint32_t>(heap_caps_get_allocated_size(p)), std::memory_order_relaxed);
        g_blocs.fetch_add(1, std::memory_order_relaxed);
    }
}

void compter_retrait(void *p) {
    if (p != nullptr) {
        g_octets.fetch_sub(static_cast<std::uint32_t>(heap_caps_get_allocated_size(p)), std::memory_order_relaxed);
        g_blocs.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace

LvglMemoire lvgl_memoire() {
    return {g_octets.load(std::memory_order_relaxed), g_blocs.load(std::memory_order_relaxed)};
}

// LVGL est du C : ces fonctions doivent garder leur nom C (sans décoration C++) pour que l'édition de liens les trouve.
extern "C" {

void lv_mem_init(void) {}

void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) {
    (void)mem;
    (void)bytes;
    return nullptr; // pas de réserve à part : le tas du système fait le travail
}

void lv_mem_remove_pool(lv_mem_pool_t pool) { (void)pool; }

// PSRAM d'abord, mémoire interne en dernier recours : jamais d'écran sans objets parce que la PSRAM est pleine.
void *lv_malloc_core(size_t size) {
    void *p = heap_caps_malloc_prefer(size, 2, kPsram, kInterne);
    compter_ajout(p);
    return p;
}

void *lv_realloc_core(void *p, size_t new_size) {
    // Un realloc qui échoue garde l'ancien bloc : on ne le décompte qu'une fois le nouveau obtenu. new_size 0 : LVGL appelle
    // lv_free (lv_realloc, src/stdlib/lv_mem.c), jamais ce chemin.
    const std::uint32_t avant = p != nullptr ? static_cast<std::uint32_t>(heap_caps_get_allocated_size(p)) : 0U;
    void *neuf = heap_caps_realloc_prefer(p, new_size, 2, kPsram, kInterne);
    if (neuf != nullptr) {
        if (p != nullptr) {
            g_octets.fetch_sub(avant, std::memory_order_relaxed);
            g_blocs.fetch_sub(1, std::memory_order_relaxed);
        }
        compter_ajout(neuf);
    }
    return neuf;
}

void lv_free_core(void *p) {
    compter_retrait(p);
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) {
    (void)mon_p; // non pris en charge : la mesure se fait avec la commande série « memoire »
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

} // extern "C"
