// Bouchon PC de freertos/FreeRTOS.h : l'écran n'utilise que la conversion de millisecondes en ticks.
#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
