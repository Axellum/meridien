// FICHIER GÉNÉRÉ par tools/icones/generer_icones.py : ne pas modifier à la main (--verifier le détecterait).
// Table des pièces d'icônes et composition des icônes météo ; les pixels sont dans main/assets/icones_meteo.bin.
// Inclus par ecran/visuel_icones.hpp.

// Identifiants des pièces, dans l'ordre du binaire.
#define ECRAN_VISUEL_PIECES(X) \
    X(soleil, METEO) \
    X(soleil_p, METEO) \
    X(soleil_haut_p, METEO) \
    X(lune, METEO) \
    X(lune_p, METEO) \
    X(lune_haut_p, METEO) \
    X(nuage, METEO) \
    X(nuage_bas, METEO) \
    X(nuage_haut, METEO) \
    X(nuage_averse, METEO) \
    X(nuage_arriere, METEO) \
    X(nuage_devant, METEO) \
    X(bruine, METEO) \
    X(pluie, METEO) \
    X(pluie_forte, METEO) \
    X(averse, METEO) \
    X(averse_forte, METEO) \
    X(verglas_pluie, METEO) \
    X(verglas_glace, METEO) \
    X(neige, METEO) \
    X(neige_forte, METEO) \
    X(grains, METEO) \
    X(averse_neige, METEO) \
    X(eclair, METEO) \
    X(grele, METEO) \
    X(brume, METEO) \
    X(thermometre, INFO) \
    X(ressenti, INFO) \
    X(humidite, INFO) \
    X(parapluie, INFO) \
    X(pluie_mm, INFO) \
    X(vent, INFO) \
    X(pression, INFO) \
    X(uv, INFO) \
    X(lever, INFO) \
    X(coucher, INFO) \
    X(lune_info, INFO) \
    X(epingle, INFO) \
    X(alerte, INFO) \
    X(gel, INFO) \
    X(chaleur, INFO) \
    X(hors_ligne, INFO) \
    X(reglages, INFO) \
    X(horloge, INFO) \
    X(nuage_info, INFO) \
    X(phase_00, LUNE) \
    X(phase_01, LUNE) \
    X(phase_02, LUNE) \
    X(phase_03, LUNE) \
    X(phase_04, LUNE) \
    X(phase_05, LUNE) \
    X(phase_06, LUNE) \
    X(phase_07, LUNE)

// Échelle des tailles par famille (choix de la taille dessinée pour une place).
inline constexpr int TAILLES_METEO[] = {44, 60, 84, 120, 168, 232};
inline constexpr int TAILLES_INFO[] = {28, 40, 56, 80};
inline constexpr int TAILLES_LUNE[] = {64, 112};
// Tailles embarquées dans le binaire (sous-ensemble de l'échelle) : une autre taille choisie n'a pas de pixels.
inline constexpr int EMBARQUEES_METEO[] = {44, 120, 168};
inline constexpr int EMBARQUEES_INFO[] = {28, 40, 56};
inline constexpr int EMBARQUEES_LUNE[] = {64};
inline constexpr int NB_PHASES_LUNE = 8;

// Composition : nom de l'icône, puis jusqu'à 3 couples (pièce, rôle de teinte).
#define ECRAN_VISUEL_ICONES(X) \
    X(inconnue, 1, nuage, nuage, AUCUNE, AUCUN, AUCUNE, AUCUN) \
    X(soleil_jour, 1, soleil, soleil, AUCUNE, AUCUN, AUCUNE, AUCUN) \
    X(lune_nuit, 1, lune, lune, AUCUNE, AUCUN, AUCUNE, AUCUN) \
    X(peu_nuageux_jour, 2, soleil_p, soleil, nuage_bas, nuage, AUCUNE, AUCUN) \
    X(peu_nuageux_nuit, 2, lune_p, lune, nuage_bas, nuage, AUCUNE, AUCUN) \
    X(nuageux_jour, 2, soleil_p, soleil, nuage_bas, nuage, AUCUNE, AUCUN) \
    X(nuageux_nuit, 2, lune_p, lune, nuage_bas, nuage, AUCUNE, AUCUN) \
    X(couvert, 2, nuage_arriere, nuage_sombre, nuage_devant, nuage, AUCUNE, AUCUN) \
    X(brouillard, 2, nuage_haut, nuage, brume, brume, AUCUNE, AUCUN) \
    X(bruine, 2, nuage_haut, nuage, bruine, pluie, AUCUNE, AUCUN) \
    X(pluie_verglacante, 3, nuage_haut, nuage_sombre, verglas_pluie, pluie, verglas_glace, neige) \
    X(pluie, 2, nuage_haut, nuage_sombre, pluie, pluie, AUCUNE, AUCUN) \
    X(pluie_forte, 2, nuage_haut, nuage_sombre, pluie_forte, pluie, AUCUNE, AUCUN) \
    X(neige, 2, nuage_haut, nuage, neige, neige, AUCUNE, AUCUN) \
    X(neige_forte, 2, nuage_haut, nuage, neige_forte, neige, AUCUNE, AUCUN) \
    X(grains_de_neige, 2, nuage_haut, nuage, grains, neige, AUCUNE, AUCUN) \
    X(averses_jour, 3, soleil_haut_p, soleil, nuage_averse, nuage_sombre, averse, pluie) \
    X(averses_nuit, 3, lune_haut_p, lune, nuage_averse, nuage_sombre, averse, pluie) \
    X(averses_violentes, 2, nuage_haut, nuage_sombre, averse_forte, pluie, AUCUNE, AUCUN) \
    X(averses_de_neige_jour, 3, soleil_haut_p, soleil, nuage_averse, nuage, averse_neige, neige) \
    X(averses_de_neige_nuit, 3, lune_haut_p, lune, nuage_averse, nuage, averse_neige, neige) \
    X(orage, 2, nuage_haut, nuage_sombre, eclair, eclair, AUCUNE, AUCUN) \
    X(orage_grele, 3, nuage_haut, nuage_sombre, eclair, eclair, grele, neige)
