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
                       "appairage.cpp" "reglages.cpp" "serveur_web.cpp" "geocodage.cpp" "verrou_tls.cpp" "lvgl_memoire.cpp"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}"
                       REQUIRES board_api core esp_event esp_netif esp_rom esp_system esp_timer esp_wifi freertos lwip nvs_flash
                               esp_http_client esp-tls mbedtls esp_hw_support heap
                               esp_http_server app_update esp_app_format espressif__mdns
                       EMBED_FILES "assets/roboto_700.ttf")

# Ecran principal (session ecran-principal). Ajoute par target_sources() apres l'enregistrement, et non dans la liste SRCS
# ci-dessus, pour ne pas toucher a la ligne que les autres sessions (meteo, reglages) modifient aussi : un merge sans conflit.
target_sources(${COMPONENT_LIB} PRIVATE "ecran_principal.cpp" "ui_principal.cpp" "ui_meteo_icones.cpp")
# L'ecran principal est reparti par zone (lot L18) : ui_principal.cpp orchestre, ui_principal_interne.h porte l'etat partage.
target_sources(${COMPONENT_LIB} PRIVATE "ui_principal_outils.cpp" "ui_principal_donnees.cpp" "ui_principal_disposition.cpp"
               "ui_principal_barre.cpp" "ui_principal_horloge.cpp" "ui_principal_voiles.cpp" "ui_principal_carte_heure.cpp"
               "ui_principal_heures.cpp" "ui_principal_bandeau.cpp" "ui_principal_mode_auto.cpp" "ui_principal_jours.cpp"
               "ui_principal_dessin.cpp" "ui_principal_toucher.cpp" "ui_principal_ciel.cpp")
# Les fichiers de l'ecran principal sont compiles ensemble, en un seul fichier genere (UNITY_BUILD, mode GROUP : seuls ceux du
# groupe ; les autres sources du composant restent compilees une par une). Separes, -O2 garde un exemplaire hors ligne de chaque
# fonction partagee en plus de ses copies recopiees dans les appelants : environ 18 Ko de plus (CI de la PR #52). Ensemble, le
# compilateur voit tout comme avant le decoupage. Le banc de rendu (tools/rendu/) les compile un par un : chacun reste autonome.
set(ui_principal_groupe "ui_principal.cpp" "ui_principal_outils.cpp" "ui_principal_donnees.cpp" "ui_principal_disposition.cpp"
    "ui_principal_barre.cpp" "ui_principal_horloge.cpp" "ui_principal_voiles.cpp" "ui_principal_carte_heure.cpp"
    "ui_principal_heures.cpp" "ui_principal_bandeau.cpp" "ui_principal_mode_auto.cpp" "ui_principal_jours.cpp"
    "ui_principal_dessin.cpp" "ui_principal_toucher.cpp" "ui_principal_ciel.cpp")
set_source_files_properties(${ui_principal_groupe} TARGET_DIRECTORY ${COMPONENT_LIB} PROPERTIES UNITY_GROUP "ui_principal")
set_target_properties(${COMPONENT_LIB} PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE GROUP)
# Registre des polices partagees par toutes les pages (une instance TinyTTF par taille) : LVGL pur, compile aussi par le banc de rendu.
target_sources(${COMPONENT_LIB} PRIVATE "ui_ressources.cpp")
# Page "Details" (session page-details, ADR-0020) : les sept grandeurs retirees de l'accueil, sur une page plein ecran. ui_details.cpp est
# du LVGL pur (rendu aussi sur le PC) ; ui_details_ouvrir.cpp est la colle de l'appareil. La roue d'appui long l'ouvre (ecran_principal.cpp).
target_sources(${COMPONENT_LIB} PRIVATE "ui_details.cpp" "ui_details_ouvrir.cpp")
# Alertes officielles de Vigie (session alertes-vigie) : client reseau, etat partage, reglages. Meme raison : target_sources() et non la
# liste SRCS. Le client ne depend ni de meteo_client.cpp ni du client HTTPS commun.
target_sources(${COMPONENT_LIB} PRIVATE "vigie_etat.cpp" "vigie_client.cpp" "reglages_alertes.cpp" "ui_alertes.cpp")
# Mer d'un spot (ADR-0018 et ADR-0019) : client reseau (trame de Vigie), etat partage, registre des spots. Meme raison : target_sources() et
# non la liste SRCS. Le client passe par le client HTTPS commun (http_commun.cpp).
target_sources(${COMPONENT_LIB} PRIVATE "mer_etat.cpp" "mer_client.cpp" "mer_liste_client.cpp" "reglages_mer.cpp")
# La tache reseau unique des donnees (meteo, alertes, mer, liste des spots) : une pile de 12 Ko au lieu de quatre. Meme raison :
# target_sources() et non la liste SRCS. L'ordonnancement est dans le noyau (ecran/ordonnanceur_reseau.hpp).
target_sources(${COMPONENT_LIB} PRIVATE "tache_donnees.cpp")
# Page mer (ADR-0022) : la page (LVGL pur, rendue aussi sur le PC) et sa colle (ecran de la carte, horloge, registre des spots). Elle s'ouvre
# par l'entree « Mer » de la roue d'appui long (ecran_principal.cpp, ADR-0024).
target_sources(${COMPONENT_LIB} PRIVATE "ui_mer.cpp" "ui_mer_ouvrir.cpp")
# Les pixels des icônes meteo (tools/icones/generer_icones.py -> main/assets/icones_meteo.bin) sont embarqués en flash comme la
# police, mais par target_add_binary_data() et non par EMBED_FILES : la ligne idf_component_register ci-dessus reste celle que
# les autres sessions modifient. Symboles : _binary_icones_meteo_bin_start / _end (lus par ui_meteo_icones.cpp).
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/assets/icones_meteo.bin" BINARY)
# La roue d'appui long (ADR-0021) : ui_roue.cpp (LVGL pur, rendu aussi sur le PC) et son sous-ensemble de la police d'icônes Material
# Design Icons (tools/icones/generer_police_roue.py -> main/assets/mdi_roue.ttf, 3 glyphes). Symboles : _binary_mdi_roue_ttf_start / _end.
target_sources(${COMPONENT_LIB} PRIVATE "ui_roue.cpp")
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/assets/mdi_roue.ttf" BINARY)
# Commandes serie d'ESSAI (binaire de mesure seulement, JAMAIS dans le binaire normal : OFF par defaut) :
#   .\tools\idf.ps1 -DMERIDIEN_BANC_ESSAI=ON build
# - banc_wifi.cpp : "wifi_scan", "wifi_faux", "wifi_faux_abandon", "wifi_inconnu", "wifi_bon" (docs/mesures/wifi-retour-arriere.md) ;
# - "tourner3" / "tourner0" de commande_serie.cpp : une tache occupe le coeur 0 pendant 8 s pour declencher (ou non) le chien de
#   garde (docs/mesures/memoire-interne.md section 6).
set(MERIDIEN_BANC_ESSAI OFF CACHE BOOL "Commandes serie d'essai : banc Wi-Fi, tourner3/tourner0 (jamais dans le binaire normal)")
if(MERIDIEN_BANC_ESSAI)
    target_sources(${COMPONENT_LIB} PRIVATE "banc_wifi.cpp")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MERIDIEN_BANC_ESSAI=1)
endif()
# Reglages et installation guidee (ADR-0007, session reglages-ecran) : les ecrans (LVGL pur, rendus aussi sur le PC), la colle
# qui les ouvre (ui_reglages_ouvrir.cpp : ecran de la carte, police, hote) et l'hote de l'appareil (hote_appareil_esp.cpp : memoire
# permanente, Wi-Fi, lieu, redemarrage) sont compiles tels quels. Le bouchon de ui_reglages_ouvrir() ("Reglages : bientot") a
# ete retire quand la colle est arrivee.
target_sources(${COMPONENT_LIB} PRIVATE "ui_commun.cpp" "ui_reglages.cpp" "ui_reglages_mer.cpp" "ui_installation.cpp" "ui_clavier.cpp"
               "ui_reglages_ouvrir.cpp" "hote_appareil_esp.cpp" "wifi_stocke.cpp")
# Essai des reglages SUR LA CARTE, sans doigt (main/autotest_reglages.cpp) : un pointeur virtuel rejoue les gestes sur le vrai LVGL,
# trois phases separees par un vrai redemarrage, constats au journal serie. JAMAIS dans le binaire normal (OFF par defaut) :
#   .\tools\idf.ps1 -DMERIDIEN_AUTOTEST_REGLAGES=ON build
# Le fichier n'est reference par personne : "-u" force l'editeur de liens a le garder (sinon son constructeur statique ne tourne pas).
set(MERIDIEN_AUTOTEST_REGLAGES OFF CACHE BOOL "Essai des reglages sur la carte, sans doigt (jamais dans le binaire normal)")
if(MERIDIEN_AUTOTEST_REGLAGES)
    target_sources(${COMPONENT_LIB} PRIVATE "autotest_reglages.cpp")
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,autotest_reglages_ancre")
endif()
# Client HTTPS commun (http_commun.cpp : GET, gzip par tinfl de la ROM, verrou TLS, tampons en PSRAM), appele par meteo_client.cpp.
# Ajoute par target_sources() et non dans la liste SRCS, pour ne pas toucher a la ligne que les autres sessions modifient aussi.
target_sources(${COMPONENT_LIB} PRIVATE "http_commun.cpp")
# Allocateur de LVGL (lvgl_memoire.cpp, CONFIG_LV_USE_CUSTOM_MALLOC) : c'est LVGL qui appelle ces fonctions, aucun fichier de main
# ne les reference. Or l'editeur de liens ne tire d'une bibliotheque statique (libmain.a) que les fichiers dont il a besoin AU MOMENT
# ou il la lit, et libmain.a est lue avant LVGL : sans "-u", lvgl_memoire.o reste dans l'archive et l'edition de liens echoue
# ("undefined reference to lv_malloc_core"). "-u" declare les symboles comme attendus des le depart.
foreach(_lv_symbole lv_mem_init lv_mem_deinit lv_mem_add_pool lv_mem_remove_pool lv_malloc_core lv_realloc_core lv_free_core
                    lv_mem_monitor_core lv_mem_test_core)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,${_lv_symbole}")
endforeach()

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
