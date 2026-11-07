cmake_minimum_required(VERSION 3.16)

# Carte cible = un dossier de boards/ (ADR-0005). Changer de carte :
#   supprimer sdkconfig (les defaults ne sont relus que sur un sdkconfig neuf), puis
#   .\tools\idf.ps1 -DBOARD=<carte> set-target esp32p4
# Cartes : tab5 (defaut), waveshare-7b. Le nom du dossier de boards/ prend un tiret ; le composant de la couche carte, lui, prend un
# soulignement (components/bsp_waveshare_7b) : BOARD_ID est le nom de la carte ecrit comme un identifiant.
set(BOARD "tab5" CACHE STRING "Carte cible (nom d'un dossier de boards/)")
string(REPLACE "-" "_" BOARD_ID "${BOARD}")
set(BOARD_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/sdkconfig.defaults.board")
if(NOT EXISTS "${BOARD_DEFAULTS}")
    message(FATAL_ERROR "Carte inconnue : ${BOARD} (introuvable : ${BOARD_DEFAULTS})")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/components/bsp_${BOARD_ID}/CMakeLists.txt")
    message(FATAL_ERROR "Couche carte introuvable : components/bsp_${BOARD_ID} (carte ${BOARD})")
endif()
# Reglages communs, puis ceux de la carte (flash, PSRAM, revision de puce, console, partitions).
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults;${BOARD_DEFAULTS}")

# Une seule couche carte est compilee : toutes definissent board_init() & co. Les autres sont EXCLUES, ce qui les rend aussi
# invisibles du gestionnaire de composants (leurs dependances ne sont ni telechargees ni inscrites dans le fichier de verrou).
file(GLOB _bsp_dirs RELATIVE "${CMAKE_CURRENT_LIST_DIR}/components" "${CMAKE_CURRENT_LIST_DIR}/components/bsp_*")
set(EXCLUDE_COMPONENTS "")
foreach(_bsp ${_bsp_dirs})
    if(NOT _bsp STREQUAL "bsp_${BOARD_ID}")
        list(APPEND EXCLUDE_COMPONENTS "${_bsp}")
    endif()
endforeach()

# Les composants du projet sont dans components/ (noyau pur, pilotes, interface, couche carte).
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Fichier de verrou des dependances : celui de la racine pour le Tab5 (inchange), un fichier par carte pour les autres
# (boards/<carte>/dependencies.lock), parce que les dependances d'une carte ne sont pas celles d'une autre.
if(NOT BOARD STREQUAL "tab5")
    idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD}/dependencies.lock")
endif()
project(meridien)

# La couche carte (components/bsp_<carte>) n'est requise par aucun composant : le code applicatif ne
# connait que board_api. On la lie ici, la seule ligne du projet qui nomme la carte.
target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE "idf::bsp_${BOARD_ID}")
