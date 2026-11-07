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

idf_component_register(SRCS "main.cpp" "boot_info.cpp" "horloge.cpp" "reseau.cpp" "sntp_rapide.cpp" "commande_serie.cpp" "ui_bonjour.cpp"
                       "meteo_etat.cpp" "meteo_zone.cpp" "meteo_client.cpp"
                       "appairage.cpp" "reglages.cpp" "serveur_web.cpp" "ui_appairage.cpp"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}"
                       REQUIRES board_api core esp_event esp_netif esp_rom esp_system esp_timer esp_wifi freertos lwip nvs_flash
                               esp_http_client esp-tls mbedtls esp_hw_support heap
                               esp_http_server app_update esp_app_format espressif__mdns
                       EMBED_FILES "assets/roboto_700.ttf")

# Ecran principal (session ecran-principal). Ajoute par target_sources() apres l'enregistrement, et non dans la liste SRCS
# ci-dessus, pour ne pas toucher a la ligne que les autres sessions (meteo, reglages) modifient aussi : un merge sans conflit.
target_sources(${COMPONENT_LIB} PRIVATE "ecran_principal.cpp" "ui_principal.cpp")
# Banc d'essai PROVISOIRE de l'interface Wi-Fi (commandes serie "wifi_scan", "wifi_faux"...), a retirer avant la phase 2.
target_sources(${COMPONENT_LIB} PRIVATE "banc_wifi.cpp")
# Contrat n 1 (main/meteo_etat.h), propriete de la session meteo-appareil. Tant que ce fichier n'existe pas, un BOUCHON
# (main/bouchons/, valeurs d'exemple marquees "Exemple" a l'ecran) permet de compiler. Des que le vrai fichier est la, le
# bouchon n'est plus ni compile ni dans le chemin d'inclusion : le dossier main/bouchons/ est alors a supprimer.
set(_meridien_bouchons FALSE)
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/meteo_etat.h")
    target_sources(${COMPONENT_LIB} PRIVATE "bouchons/meteo_etat_bouchon.cpp")
    set(_meridien_bouchons TRUE)
endif()
# Meme principe pour ui_reglages_ouvrir() (ADR-0007), definie par la session reglages-ecran : le bouton "Reglages" de l'ecran
# principal l'appelle ; le bouchon affiche "Reglages : bientot" deux secondes. Le test porte sur le fichier qui DEFINIT la
# fonction (la colle main/ui_reglages_ouvrir.cpp), pas sur main/ui_reglages.h : la session reglages-ecran livre l'en-tete avant
# la colle, et tester l'en-tete laisserait ui_reglages_ouvrir() indefinie (erreur d'edition de liens) entre les deux. Le bouchon
# garde son propre en-tete (bouchons/ui_reglages.h, trouve en premier depuis bouchons/) ; les fichiers de main/ trouvent
# main/ui_reglages.h en premier quand il existe.
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/ui_reglages_ouvrir.cpp")
    target_sources(${COMPONENT_LIB} PRIVATE "bouchons/ui_reglages_bouchon.cpp")
    set(_meridien_bouchons TRUE)
endif()
if(_meridien_bouchons)
    target_include_directories(${COMPONENT_LIB} PRIVATE "bouchons")
endif()

# La page de reglages (web/app) est embarquee dans le firmware, compressee en gzip, par tools/embarquer_web.py : le fichier
# C++ genere (web_fichiers.cpp) est dans le dossier de construction, jamais dans git. Il est refait des qu'un fichier de
# web/app ou le generateur change.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    idf_build_get_property(_python PYTHON)
    set(_web_dossier "${CMAKE_CURRENT_LIST_DIR}/../web/app")
    set(_web_generateur "${CMAKE_CURRENT_LIST_DIR}/../tools/embarquer_web.py")
    set(_web_cpp "${CMAKE_CURRENT_BINARY_DIR}/web_fichiers.cpp")
    file(GLOB_RECURSE _web_sources CONFIGURE_DEPENDS "${_web_dossier}/*")
    add_custom_command(OUTPUT "${_web_cpp}"
                       COMMAND ${_python} "${_web_generateur}" "${_web_dossier}" "${_web_cpp}"
                       DEPENDS ${_web_sources} "${_web_generateur}"
                       COMMENT "Page de reglages : web/app -> gzip dans le firmware"
                       VERBATIM)
    target_sources(${COMPONENT_LIB} PRIVATE "${_web_cpp}")
endif()
