// Textes de l'écran principal. Chaque clé est un texte français de la table commune (i18n/*.yaml, ADR-0006) ; ce fichier n'en garde
// que les noms et le câblage vers i18n::tr (par visuel::texte_c).
#pragma once

#include "ecran/visuel_textes.hpp"

namespace ecran::texte_principal {

inline constexpr const char *kHeureAbsente = "--:--";

// Clés françaises (l'identifiant d'une phrase est son texte français).
inline constexpr const char *kDateAttente = "Heure du réseau en attente";
inline constexpr const char *kMeteoIndisponible = "Météo indisponible";
inline constexpr const char *kPerimee = "Données périmées";
inline constexpr const char *kExemple = "Exemple";

// Traduit une clé française dans la langue courante de l'écran (sept langues ; repli : l'anglais, puis le français).
inline const char *t(const char *francais) { return visuel::texte_c(francais); }

} // namespace ecran::texte_principal
