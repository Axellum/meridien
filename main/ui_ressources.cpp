// Registre des polices partagées (ui_ressources.h). LVGL pur, aucun include ESP-IDF.
#include "ui_ressources.h"

namespace ecran::ressources {

namespace {

// Une entrée par (fichier, taille). 32 couvre largement les tailles de toutes les pages à une échelle d'écran (une vingtaine) ; si
// jamais elle était pleine, la police est créée quand même, sans être partagée (jamais d'écran sans texte).
constexpr int kMaxPolices = 32;
PoliceCreee g_table[kMaxPolices];
int g_nombre = 0;

lv_font_t *creer(const std::uint8_t *ttf, std::size_t ttf_taille, int32_t px) {
    return lv_tiny_ttf_create_data_ex(ttf, ttf_taille, px, LV_FONT_KERNING_NORMAL, kGlyphesEnCache);
}

} // namespace

lv_font_t *police(const std::uint8_t *ttf, std::size_t ttf_taille, int32_t px) {
    if (ttf == nullptr || ttf_taille == 0 || px < 1) {
        return nullptr;
    }
    for (int i = 0; i < g_nombre; ++i) {
        if (g_table[i].ttf == ttf && g_table[i].px == px) {
            return g_table[i].f;
        }
    }
    lv_font_t *f = creer(ttf, ttf_taille, px);
    if (f != nullptr && g_nombre < kMaxPolices) {
        g_table[g_nombre++] = {ttf, px, f};
    }
    return f;
}

int nombre_polices() { return g_nombre; }

PoliceCreee police_n(int n) { return (n >= 0 && n < g_nombre) ? g_table[n] : PoliceCreee{}; }

} // namespace ecran::ressources
