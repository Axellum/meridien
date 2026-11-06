# Couche carte du M5Stack Tab5 (ADR-0005). Fournit board.h (composant board_api). Liee au projet par le CMakeLists.txt racine
# (aucun composant ne la "requiert" : le code applicatif ne connait que board_api).
idf_component_register(
    SRCS "tab5_board.c" "tab5_io.c" "tab5_detect.c" "tab5_display.c" "tab5_touch.c"
    PRIV_INCLUDE_DIRS "."
    REQUIRES board_api
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_hw_support esp_lcd esp_timer
                  espressif__esp_io_expander espressif__esp_io_expander_pi4ioe5v6408
                  espressif__esp_lcd_st7121 espressif__esp_lcd_touch espressif__esp_lcd_touch_st7123
                  espressif__esp_lvgl_port)
