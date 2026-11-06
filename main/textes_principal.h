// Textes français de l'écran principal (phase 2) : PAS ENCORE dans la table de traduction (i18n/*.yaml, ADR-0006).
//
// Pourquoi : ajouter une clé à la table touche les sept fichiers de langue, la table générée, les fichiers de la page de
// réglages et les décomptes des tests du noyau (test_i18n.cpp), qui appartiennent à d'autres sessions. Les textes ci-dessous
// sont à migrer vers la table (clé = ce texte français) quand l'écran principal sera stabilisé, puis ce fichier disparaît.
// Ce qui existe déjà dans la table n'est pas répété ici : les libellés météo arrivent déjà traduits dans MeteoEtat::libelle.
#pragma once

namespace ecran::texte_principal {

inline constexpr const char *kHeureAbsente = "--:--";
inline constexpr const char *kDateAttente = "Heure du réseau en attente";
inline constexpr const char *kMeteoIndisponible = "Météo indisponible";
inline constexpr const char *kPerimee = "Données périmées";
inline constexpr const char *kExemple = "Exemple";
inline constexpr const char *kMinMax = "min %s · max %s"; // %s : « 8° », « 14° » (formater_degre)
inline constexpr const char *kReglages =
    "Réglages"; // bouton de l'écran principal (ADR-0007) ; la table n'a pas cette clé seule

} // namespace ecran::texte_principal
