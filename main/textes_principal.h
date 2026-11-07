// Textes de l'écran principal. Les phrases propres à l'écran vivent dans le noyau pur (ecran/visuel_textes.hpp : sept langues,
// testées sur le PC) ; ce fichier n'en garde que le câblage. Elles seront migrées dans la table commune (i18n/*.yaml, ADR-0006)
// quand l'écran sera stabilisé : même clé (le texte français), mêmes traductions.
#pragma once

#include "ecran/visuel_textes.hpp"

namespace ecran::texte_principal {

inline constexpr const char *kHeureAbsente = "--:--";

// Clés françaises (l'identifiant d'une phrase est son texte français).
inline constexpr const char *kDateAttente = "Heure du réseau en attente";
inline constexpr const char *kMeteoIndisponible = "Météo indisponible";
inline constexpr const char *kPerimee = "Données périmées";
inline constexpr const char *kExemple = "Exemple";
inline constexpr const char *kReglages = "Réglages";

// Traduit une clé française dans la langue courante de l'écran (sept langues ; repli : table commune, puis le français).
inline const char *t(const char *francais) { return visuel::texte_c(francais); }

} // namespace ecran::texte_principal
