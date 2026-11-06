# Noyau pur : C++ standard, aucun include ESP-IDF, en-têtes seulement.
# Les tests tournent sur le PC, sans ESP-IDF : voir test/host/lancer.py.
# NON VÉRIFIÉ : ce fichier n'a pas été compilé par ESP-IDF (aucune compilation ESP-IDF dans la piste B).
idf_component_register(INCLUDE_DIRS "include")
