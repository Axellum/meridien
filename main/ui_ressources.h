// Ressources d'interface partagées par toutes les pages (LVGL pur, aucun include ESP-IDF : compilé aussi par le banc de rendu PC).
//
// Les polices TrueType : UNE instance TinyTTF par (fichier, taille en pixels) pour tout l'appareil. Avant, chaque page créait les siennes
// (l'accueil, Détails, Mer, Réglages, Alertes, l'écran de démarrage) : une même taille existait jusqu'à trois fois, chacune avec son cache
// de glyphes à remplir à froid. Désormais un « 36 px » rastérisé par l'accueil sert tel quel à Détails et aux réglages. Mêmes octets TTF,
// même taille, même crénage : les glyphes sont identiques au pixel.
//
// Une police n'est jamais détruite (TinyTTF se libère mal et les pages se rouvrent) : la table ne fait que grandir, d'au plus une entrée
// par taille réellement utilisée. À n'appeler que dans la tâche LVGL (verrou d'affichage pris), comme tout le reste de l'interface.
#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

namespace ecran::ressources {

// Glyphes gardés en cache par police (métriques et images A8). Mesuré au banc (scène « ouvertures », docs/mesures/) : la taille la plus
// partagée en sert au plus 74 sur toutes les pages et les sept langues ; 128 laisse de la marge sans qu'un glyphe chassé ne soit
// rastérisé à nouveau à chaque image. Même valeur que CONFIG_LV_TINY_TTF_CACHE_GLYPH_CNT : rien ne change pour une page seule.
inline constexpr std::size_t kGlyphesEnCache = 128;

// La police `ttf` (octets TrueType embarqués, gardés par l'appelant pour toute la vie du programme) à `px` pixels. Nulle si TinyTTF ne
// peut pas la créer (mémoire) ou si `px` < 1 ; un appel suivant réessaie.
lv_font_t *police(const std::uint8_t *ttf, std::size_t ttf_taille, int32_t px);

// Inventaire, pour la mesure (banc, diagnostic) : nombre de polices créées et la n-ième (ordre de création).
struct PoliceCreee {
    const std::uint8_t *ttf = nullptr;
    int32_t px = 0;
    lv_font_t *f = nullptr;
};
int nombre_polices();
PoliceCreee police_n(int n);

} // namespace ecran::ressources
