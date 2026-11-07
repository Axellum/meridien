// Verrou « un seul TLS à la fois » (contrat main/verrou_tls.h).
#include "verrou_tls.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ecran {

namespace {

// Mutex statique créé au premier appel (l'initialisation d'une variable locale statique est protégée contre deux fils).
SemaphoreHandle_t verrou() {
    static StaticSemaphore_t tampon;
    static SemaphoreHandle_t v = xSemaphoreCreateMutexStatic(&tampon);
    return v;
}

} // namespace

bool verrou_tls_prendre(uint32_t delai_ms) { return xSemaphoreTake(verrou(), pdMS_TO_TICKS(delai_ms)) == pdTRUE; }

void verrou_tls_rendre() { xSemaphoreGive(verrou()); }

} // namespace ecran
