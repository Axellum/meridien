// Ce que LVGL tient en mémoire en ce moment, compté par son allocateur (lvgl_memoire.cpp). Lu par la commande série « memoire » :
// une baisse de la PSRAM libre qui se retrouve ici vient de l'interface (objets, styles, caches de glyphes de TinyTTF), sinon
// d'ailleurs (clients réseau, TLS, esp_hosted, lwIP). Voir docs/depannage.md, « La PSRAM libre baisse d'environ 200 o par minute ».
#pragma once

#include <cstdint>

struct LvglMemoire {
    std::uint32_t octets; // taille utile des blocs vivants (heap_caps_get_allocated_size), PSRAM et interne ensemble
    std::uint32_t blocs;  // nombre de blocs vivants
};

LvglMemoire lvgl_memoire();
