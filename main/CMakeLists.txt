# Identifiants Wi-Fi de la compilation (phase 1b). Ils ne vivent JAMAIS dans git : tools/preparer_wifi.ps1 les lit dans le .env
# d'Axel et ecrit reglages_locaux.cmake a la racine du projet (ignore par git). Sans ce fichier la compilation reussit, le
# firmware n'a aucun identifiant, le dit au demarrage et ne se connecte pas. Le fichier genere (identifiants_wifi.h) est dans le
# dossier de construction.
# Garde : ESP-IDF evalue d'abord ce fichier en "early expansion" (mode script de CMake, sans cible ni propriete).
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(MERIDIEN_WIFI_SSID_BRUT "")
    set(MERIDIEN_WIFI_MOT_DE_PASSE_BRUT "")
    set(_reglages_locaux "${CMAKE_CURRENT_LIST_DIR}/../reglages_locaux.cmake")
    if(EXISTS "${_reglages_locaux}")
        # CMake relance seul la configuration quand un fichier lu par include() change.
        include("${_reglages_locaux}")
    endif()
    # Echappement pour une chaine C (barre oblique inverse, guillemet).
    foreach(_nom SSID MOT_DE_PASSE)
        string(REPLACE "\\" "\\\\" _valeur "${MERIDIEN_WIFI_${_nom}_BRUT}")
        string(REPLACE "\"" "\\\"" _valeur "${_valeur}")
        set(MERIDIEN_WIFI_${_nom}_C "${_valeur}")
    endforeach()
    configure_file("${CMAKE_CURRENT_LIST_DIR}/identifiants_wifi.h.in" "${CMAKE_CURRENT_BINARY_DIR}/identifiants_wifi.h" @ONLY)
endif()

idf_component_register(SRCS "main.cpp" "boot_info.cpp" "horloge.cpp" "reseau.cpp" "diagnostic_sntp.cpp" "commande_serie.cpp" "ui_bonjour.cpp"
                       "meteo_etat.cpp" "meteo_zone.cpp" "meteo_client.cpp"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}"
                       REQUIRES board_api core esp_event esp_netif esp_rom esp_system esp_timer esp_wifi freertos lwip nvs_flash
                               esp_http_client esp-tls mbedtls esp_hw_support heap
                       EMBED_FILES "assets/roboto_700.ttf")
