// Textes du bandeau d'appairage de la page de réglages (main/ui_appairage.cpp). Provisoires, en français seulement : ils passent
// par la table de traduction (ADR-0006, `ecran::i18n::tr`) quand `ecran-principal` place le code dans l'écran principal. Accents
// et « ° » toujours (AGENTS.md règle 5).
#pragma once

namespace ecran::textes_reglages {

inline constexpr const char *kBandeauTitre = "Réglages depuis un téléphone";
inline constexpr const char *kBandeauAdresse = "http://%s";
inline constexpr const char *kBandeauNom = "ou http://%s.local";
inline constexpr const char *kBandeauCode = "Code : %s";

} // namespace ecran::textes_reglages
