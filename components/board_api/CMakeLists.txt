# Interface commune des cartes : un en-tete, aucune source. L'implementation est dans components/bsp_<carte>.
idf_component_register(INCLUDE_DIRS "include"
                       REQUIRES esp_rom lvgl__lvgl)
