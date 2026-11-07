# Ressources embarquées

| Fichier | Origine | SHA-256 | Licence |
|---|---|---|---|
| `roboto_700.ttf` | copie telle quelle de `Tab5/fonts/roboto_700.ttf` du dépôt Tab5, Roboto Bold servie par Google Fonts le 08/10/2026 | `35757eaa996b3359022f47f164a67ee9eac04971af24c5b8fbd4b89e844ff190` | SIL Open Font License 1.1 (`OFL.txt`, copie du fichier du dépôt Tab5, qui porte les avis de copyright par famille) |
| `icones_meteo.bin` | **généré** par `python tools/icones/generer_icones.py` : dessins originaux du dépôt (aucune icône tierce), 825 032 octets d'images A8 (alpha seul, recolorées à l'affichage), avec un en-tête `MIC1` et une table (pièce, taille, largeur, hauteur, décalage). Régénérer, puis `--verifier` pour contrôler que le fichier et ses tables (`components/core/include/ecran/visuel_icones_table.inc`, `main/ui_meteo_icones_table.inc`) correspondent aux dessins | `a071b01727b73d28d471a3c062447942b24c8b8af4421f1514f3f02d99f7d5c0` | aucune licence tierce : code du dépôt |

La police est rendue à l'exécution par TinyTTF (LVGL) : tout le Latin-1 (accents, « ° ») est disponible sans sous-ensemble à préparer. Les polices intégrées de LVGL s'arrêtent à l'ASCII.

Les icônes sont lues en place, dans la flash (`main/ui_meteo_icones.cpp` bâtit leurs descripteurs LVGL sans copier les pixels) : 0 octet de RAM pour les pixels. Le binaire est embarqué par `target_add_binary_data()` dans `main/CMakeLists.txt` (symboles `_binary_icones_meteo_bin_start` et `_end`).
