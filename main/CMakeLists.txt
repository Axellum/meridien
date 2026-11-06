idf_component_register(SRCS "main.cpp" "boot_info.cpp" "ui_bonjour.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES board_api nvs_flash esp_timer esp_system
                       EMBED_FILES "assets/roboto_700.ttf")
