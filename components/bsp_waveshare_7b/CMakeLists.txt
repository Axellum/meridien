# Couche carte de la Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (ADR-0005). Fournit board.h (composant board_api). Liee au projet par le
# CMakeLists.txt racine (aucun composant ne la "requiert" : le code applicatif ne connait que board_api).
# Documentation : docs/carte-7b.md. Valeurs et leurs sources : include/ws7b_pur.h.
idf_component_register(
    SRCS "ws7b_board.c" "ws7b_io.c" "ws7b_display.c" "ws7b_touch.c" "ws7b_wifi.c" "ws7b_audio.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES board_api
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_event esp_hw_support esp_lcd esp_timer freertos
                  espressif__esp_hosted
                  espressif__esp_lcd_ek79007 espressif__esp_lcd_touch espressif__esp_lcd_touch_gt911
                  espressif__esp_lvgl_port espressif__esp_codec_dev)

# --- Options de compilation (cache de CMake : une valeur donnee reste jusqu'a ce qu'on en donne une autre) ----------------------
# Rotation de l'interface : 0 (defaut) = la 7B d'Axel, montee a l'envers par rapport aux demos Waveshare ; 180 = montage des demos.
#   .\tools\idf.ps1 "-DWS7B_ROTATION=180" build
set(WS7B_ROTATION "0" CACHE STRING "Rotation de l'interface : 0 ou 180 degres")
if(NOT WS7B_ROTATION STREQUAL "0" AND NOT WS7B_ROTATION STREQUAL "180")
    message(FATAL_ERROR "WS7B_ROTATION doit valoir 0 ou 180 (recu : ${WS7B_ROTATION})")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE WS7B_UI_ROTATION_DEGREES=${WS7B_ROTATION})

# Tampon de dessin LVGL (lignes de 1024 px, double tampon, RAM interne). Defaut dans ws7b_internal.h.
set(WS7B_DRAW_BUFFER_LINES "25" CACHE STRING "Hauteur du tampon de dessin LVGL, en lignes")
target_compile_definitions(${COMPONENT_LIB} PRIVATE WS7B_DRAW_BUFFER_LINES=${WS7B_DRAW_BUFFER_LINES})

# 0 : un pourcentage de luminosite nul ne touche qu'au retroeclairage (analogique : jamais tout a fait eteint).
set(WS7B_REAL_PANEL_OFF "1" CACHE STRING "1 : 0 % eteint aussi la dalle (voile noir + commandes DCS 0x28 et 0x10)")
target_compile_definitions(${COMPONENT_LIB} PRIVATE WS7B_REAL_PANEL_OFF=${WS7B_REAL_PANEL_OFF})

# 1 : demander au C6 sa version (RPC esp_hosted). Le C6 d'origine n'y repond pas : defaut 0.
set(WS7B_READ_C6_VERSION "0" CACHE STRING "1 : lire la version du micrologiciel du C6 (RPC esp_hosted)")
target_compile_definitions(${COMPONENT_LIB} PRIVATE WS7B_READ_C6_VERSION=${WS7B_READ_C6_VERSION})

# Binaires d'ESSAI (absents du binaire normal ; revenir au binaire normal avec =0, le cache garde la valeur) :
#   WS7B_AUTOTEST_TACTILE : cinq appuis simules 10 s apres le demarrage (ws7b_touch.c).
#   WS7B_ESSAI_AUDIO      : tonalite de 300 ms puis niveau du micro au demarrage (ws7b_audio.c).
if(WS7B_AUTOTEST_TACTILE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WS7B_AUTOTEST_TACTILE=1)
endif()
if(WS7B_ESSAI_AUDIO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WS7B_ESSAI_AUDIO=1)
endif()
