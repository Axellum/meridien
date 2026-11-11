#include "ui_meteo_icones.h"

#include <cstddef>

#include "esp_log.h"
#include "ui_meteo_dessin.h"

// Le binaire des pixels (main/assets/icones_meteo.bin), embarqué comme la police (EMBED_FILES de main/CMakeLists.txt) ; sur le
// banc de rendu, tools/rendu/rendre.py pose les mêmes symboles.
extern const std::uint8_t icones_meteo_octets[] asm("_binary_icones_meteo_bin_start");

namespace ecran::icones {

namespace {

const char *TAG = "icones";

struct Entree {
    std::uint16_t piece;
    std::uint16_t taille;
    std::int16_t x0; // position de l'image rognée dans le carré de l'icône
    std::int16_t y0;
    lv_image_dsc_t dsc;
};

// Une ligne par image, triée par (pièce, taille), écrite par le générateur. Tout est constant : la table et les descripteurs
// vivent en flash. L'en-tête d'image est écrit dans l'ordre des champs d'une machine petit-boutiste (magic, cf, flags, w, h,
// stride, réservé) : c'est celui de l'ESP32-P4 et du PC ; LVGL inverse cet ordre sur une machine gros-boutiste, qui n'est pas
// une cible de ce projet.
#define ICONE_ENTREE(p, t, w, h, x0, y0, decalage)                                                                     \
    {static_cast<std::uint16_t>(p),                                                                                    \
     static_cast<std::uint16_t>(t),                                                                                    \
     static_cast<std::int16_t>(x0),                                                                                    \
     static_cast<std::int16_t>(y0),                                                                                    \
     {{LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_A8, 0, (w), (h), (w), 0},                                                \
      static_cast<std::uint32_t>((w) * (h)),                                                                           \
      icones_meteo_octets + (decalage),                                                                                \
      nullptr,                                                                                                         \
      nullptr}},

static_assert(LV_COLOR_FORMAT_A8 == 0x0E, "format A8 attendu par le générateur");

const Entree kEntrees[] = {
#include "ui_meteo_icones_table.inc"
};
#undef ICONE_ENTREE

constexpr int kNombre = static_cast<int>(sizeof(kEntrees) / sizeof(kEntrees[0]));

const Entree *chercher(int piece, int taille) {
    int bas = 0;
    int haut = kNombre;
    const std::uint32_t cle = (static_cast<std::uint32_t>(piece) << 16) | static_cast<std::uint32_t>(taille);
    while (bas < haut) {
        const int milieu = bas + (haut - bas) / 2;
        const std::uint32_t k = (static_cast<std::uint32_t>(kEntrees[milieu].piece) << 16) | kEntrees[milieu].taille;
        if (k == cle) {
            return &kEntrees[milieu];
        }
        if (k < cle) {
            bas = milieu + 1;
        } else {
            haut = milieu;
        }
    }
    return nullptr;
}

int taille_choisie(visuel::Piece p, int cote) {
    switch (visuel::famille(p)) {
    case visuel::Famille::METEO:
        return visuel::taille_meteo(cote);
    case visuel::Famille::INFO:
        return visuel::taille_info(cote);
    case visuel::Famille::LUNE:
        return visuel::taille_lune(cote);
    }
    return 0;
}

} // namespace

int taille_dessinee(visuel::Piece p, int cote) { return p == visuel::Piece::AUCUNE ? 0 : taille_choisie(p, cote); }

void piece(lv_layer_t *couche, visuel::Piece p, int cote, int x, int y, std::uint32_t rgb, std::uint8_t opa) {
    if (couche == nullptr || p == visuel::Piece::AUCUNE || cote <= 0 || opa == LV_OPA_TRANSP) {
        return;
    }
    const int taille = taille_choisie(p, cote);
    if (taille == 0) {
        // La place choisit une taille de l'échelle que le binaire n'embarque pas (tools/icones/generer_icones.py) : rien n'est
        // dessiné. Signalé une fois par démarrage (le dessin repasse à chaque image) ; le banc de rendu échoue sur ce mot.
        static bool signale = false;
        if (!signale) {
            signale = true;
            ESP_LOGE(TAG, "ICONE_ABSENTE : pièce %d, place de %d px, taille non embarquée", static_cast<int>(p), cote);
        }
        return;
    }
    const Entree *e = chercher(static_cast<int>(p), taille);
    if (e == nullptr || e->dsc.header.w == 0 || e->dsc.header.h == 0) {
        return;
    }
    // L'image générée peut être plus petite que la place (on ne l'étire jamais) : on la centre dans le carré demandé.
    const int decalage = (cote - taille) / 2;
    const int px = x + decalage + e->x0;
    const int py = y + decalage + e->y0;
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = &e->dsc;
    d.recolor = lv_color_hex(rgb);
    d.recolor_opa = LV_OPA_COVER;
    d.opa = opa;
    const lv_area_t zone = {px, py, px + static_cast<int>(e->dsc.header.w) - 1,
                            py + static_cast<int>(e->dsc.header.h) - 1};
    // Même test de zone que les autres éléments de l'accueil (ui_meteo_dessin.h) : hors de la bande repeinte, pas de tâche de dessin.
    if (dessin::hors_zone(couche, zone.x1, zone.y1, zone.x2, zone.y2)) {
        return;
    }
    lv_draw_image(couche, &d, &zone);
}

void meteo(lv_layer_t *couche, visuel::IdIcone icone, int cote, int x, int y, const visuel::ThemePrincipal &theme,
           std::uint8_t opa) {
    const visuel::Composition &c = visuel::composition(icone);
    for (int i = 0; i < c.nb; ++i) {
        piece(couche, c.couches[i].piece, cote, x, y, visuel::teinte_role(theme, c.couches[i].role), opa);
    }
}

void lune(lv_layer_t *couche, int indice, int cote, int x, int y, std::uint32_t rgb) {
    piece(couche, visuel::piece_phase_lune(indice), cote, x, y, rgb);
}

int nombre_images() { return kNombre; }

} // namespace ecran::icones
