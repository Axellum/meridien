cmake_minimum_required(VERSION 3.16)

# Carte cible = un dossier de boards/ (ADR-0005). Changer de carte :
#   supprimer sdkconfig (les defaults ne sont relus que sur un sdkconfig neuf), puis
#   .\tools\idf.ps1 -DBOARD=<carte> set-target esp32p4
set(BOARD "tab5" CACHE STRING "Carte cible (nom d'un dossier de boards/)")
set(BOARD_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/sdkconfig.defaults.board")
if(NOT EXISTS "${BOARD_DEFAULTS}")
    message(FATAL_ERROR "Carte inconnue : ${BOARD} (introuvable : ${BOARD_DEFAULTS})")
endif()
# Reglages communs, puis ceux de la carte (flash, PSRAM, revision de puce, console, partitions).
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults;${BOARD_DEFAULTS}")

# Les composants du projet sont dans components/ (noyau pur, pilotes, interface, couche carte).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ecran_autonome)

# La couche carte (components/bsp_<carte>) n'est requise par aucun composant : le code applicatif ne
# connait que board_api. On la lie ici, la seule ligne du projet qui nomme la carte.
target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE "idf::bsp_${BOARD}")
