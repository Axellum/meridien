// Bouchon PC de freertos/task.h : pas de tâche sur le PC, un délai ne fait rien (le banc ne dort jamais).
#pragma once

#include "FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
