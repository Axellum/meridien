# Couche carte du M5Stack Tab5 (ADR-0005). Fournit board.h (composant board_api). Liee au projet par le CMakeLists.txt racine
# (aucun composant ne la "requiert" : le code applicatif ne connait que board_api).
idf_component_register(
    SRCS "tab5_board.c" "tab5_io.c" "tab5_detect.c" "tab5_display.c" "tab5_touch.c" "tab5_wifi.c" "tab5_orientation.c"
         "tab5_i2c.c" "tab5_ecran.cpp" "tab5_rtc.cpp" "tab5_i2s.c" "tab5_codec.c" "tab5_audio.cpp" "tab5_alim.cpp"
         "tab5_autotest.cpp"
    PRIV_INCLUDE_DIRS "."
    REQUIRES board_api
    PRIV_REQUIRES core esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_driver_ppa esp_event esp_hw_support esp_lcd
                  esp_timer freertos heap
                  espressif__esp_hosted
                  espressif__esp_io_expander espressif__esp_io_expander_pi4ioe5v6408
                  espressif__esp_lcd_st7121 espressif__esp_lcd_touch espressif__esp_lcd_touch_st7123
                  espressif__esp_lvgl_port)

# Binaire d'ESSAI du tactile : six appuis simules au demarrage (voir tab5_orientation.c). Absent ou 0 : binaire normal.
# Usage : .\tools\idf.ps1 "-DTAB5_AUTOTEST_TACTILE=1" build ; revenir au binaire normal avec "-DTAB5_AUTOTEST_TACTILE=0"
# (le cache de CMake garde la valeur d'une compilation a l'autre).
if(TAB5_AUTOTEST_TACTILE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TAB5_AUTOTEST_TACTILE=1)
endif()

# Binaire d'ESSAI de la reprise sur l'expandeur (tab5_io.c) : fait echouer les N premiers essais de l'expandeur 0x43 a chaque
# demarrage (99 = tous : redemarrages de reprise puis arret). Usage : .\tools\idf.ps1 "-DTAB5_AUTOTEST_EXPANDEUR=2" build ;
# revenir au binaire normal avec "-DTAB5_AUTOTEST_EXPANDEUR=0" (le cache de CMake garde la valeur).
if(TAB5_AUTOTEST_EXPANDEUR)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TAB5_AUTOTEST_EXPANDEUR=${TAB5_AUTOTEST_EXPANDEUR})
endif()

# Binaire d'ESSAI du materiel (tab5_autotest.cpp) : luminosite, veille, horloge, audio (haut-parleur, microphones, lecture et
# enregistrement simultanes), alimentation, sans toucher a l'ecran. Rend le materiel dans l'etat ou il l'a trouve (sauf le temps
# ecoule). Usage : .\tools\idf.ps1 "-DTAB5_AUTOTEST_MATERIEL=1" build ; revenir au binaire normal avec "-DTAB5_AUTOTEST_MATERIEL=0"
# (le cache de CMake garde la valeur).
if(TAB5_AUTOTEST_MATERIEL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TAB5_AUTOTEST_MATERIEL=1)
endif()
