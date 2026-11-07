// Banc de rendu hors carte : affichage LVGL en mémoire, bouchons de l'interface des cartes, boucle de rendu, export PPM.
// Rendu LVGL logiciel sur le PC : ce n'est pas la dalle (docs/outils.md, « Ce que le rendu ne couvre pas »).
#include "banc.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "board.h"
#include "ecran/i18n.hpp"
#include "lvgl.h"
#include "meteo_zone.h"

namespace {

// Même découpage que components/bsp_tab5 : tampons partiels de 50 lignes de la largeur de l'écran.
constexpr int32_t kLignesParTampon = 50;
constexpr uint32_t kPasMs = 50;
constexpr int kNombrePas =
    60; // 3 s simulées : les minuteurs de 500 ms de l'interface passent, aucune animation ne reste

int64_t g_instant = 0;
uint32_t g_ms = 0;
int32_t g_largeur = 0;
int32_t g_hauteur = 0;
std::vector<uint16_t> g_image; // RGB565, ce que LVGL envoie à la dalle
lv_display_t *g_affichage = nullptr;

ZoneMeteo g_zone = {"Lyon", 45.74906F, 4.84789F, "Europe/Paris"}; // la zone d'usine (main/meteo_zone.cpp)

board_info_t g_carte = {"M5Stack Tab5", "ST7121", "Sitronix (version 1)", 0, 0};
board_boot_timeline_t g_jalons = {0, 0, 0, 0, 612};

uint32_t lire_tick() { return g_ms; }

// Recopie la zone dessinée par LVGL dans l'image complète (le rôle de la copie DMA2D vers le tampon de la dalle).
void sur_flush(lv_display_t *affichage, const lv_area_t *zone, uint8_t *pixels) {
    const int32_t largeur_zone = lv_area_get_width(zone);
    const uint16_t *source = reinterpret_cast<const uint16_t *>(pixels);
    for (int32_t y = zone->y1; y <= zone->y2; ++y, source += largeur_zone) {
        if (y < 0 || y >= g_hauteur) {
            continue;
        }
        for (int32_t x = zone->x1; x <= zone->x2; ++x) {
            if (x >= 0 && x < g_largeur) {
                g_image[static_cast<size_t>(y) * g_largeur + x] = source[x - zone->x1];
            }
        }
    }
    lv_display_flush_ready(affichage);
}

// La dalle reçoit 5-6-5 bits ; on les étend à 8 bits en répétant les bits de poids fort. Ce qu'une dalle réelle en fait
// (gamma, dithering interne) n'est pas simulé.
bool ecrire_ppm(const char *chemin) {
    std::FILE *fichier = std::fopen(chemin, "wb");
    if (fichier == nullptr) {
        return false;
    }
    std::fprintf(fichier, "P6\n%d %d\n255\n", static_cast<int>(g_largeur), static_cast<int>(g_hauteur));
    std::vector<uint8_t> ligne(static_cast<size_t>(g_largeur) * 3);
    for (int32_t y = 0; y < g_hauteur; ++y) {
        for (int32_t x = 0; x < g_largeur; ++x) {
            const uint16_t p = g_image[static_cast<size_t>(y) * g_largeur + x];
            const unsigned r = (p >> 11) & 0x1F;
            const unsigned v = (p >> 5) & 0x3F;
            const unsigned b = p & 0x1F;
            ligne[static_cast<size_t>(x) * 3 + 0] = static_cast<uint8_t>((r << 3) | (r >> 2));
            ligne[static_cast<size_t>(x) * 3 + 1] = static_cast<uint8_t>((v << 2) | (v >> 4));
            ligne[static_cast<size_t>(x) * 3 + 2] = static_cast<uint8_t>((b << 3) | (b >> 2));
        }
        std::fwrite(ligne.data(), 1, ligne.size(), fichier);
    }
    std::fclose(fichier);
    return true;
}

// Identité de la carte selon la taille de l'écran demandée : les chaînes viennent de docs/compatibilite.md (7B) et de
// components/bsp_tab5 (Tab5). Une taille inconnue garde l'identité du Tab5 : seule la géométrie compte alors.
void choisir_carte_selon_taille() {
    if (g_largeur == 1024 && g_hauteur == 600) {
        g_carte.board_name = "Waveshare ESP32-P4 7B";
        g_carte.display_controller = "EK79007";
        g_carte.touch_controller = "GT911";
    }
}

bool lire_taille(const char *texte, int32_t &largeur, int32_t &hauteur) {
    int l = 0, h = 0;
    if (std::sscanf(texte, "%dx%d", &l, &h) != 2 || l < 16 || h < 16 || l > 4096 || h > 4096) {
        return false;
    }
    largeur = l;
    hauteur = h;
    return true;
}

} // namespace

// --- interface commune des cartes (components/board_api/include/board.h), bouchonnée --------------------------------------
extern "C" {

const board_info_t *board_get_info(void) { return &g_carte; }
const board_boot_timeline_t *board_get_boot_timeline(void) { return &g_jalons; }
lv_display_t *board_get_display(void) { return g_affichage; }
bool board_display_lock(uint32_t) { return true; } // un seul fil : rien à verrouiller
void board_display_unlock(void) {}
esp_err_t board_set_backlight_percent(int) { return ESP_OK; }

// Appelé par board_restart() (inline) : un redémarrage n'a pas de sens sur le PC.
void esp_rom_software_reset_system(void) {
    std::fprintf(stderr, "banc : redémarrage demandé par l'écran, ignoré\n");
    std::exit(3);
}

// std::time(nullptr) de l'interface (ui_bonjour.cpp lit l'heure ainsi) : sous MinGW, time() est un renvoi vers _time64. On rend
// l'instant fixé, sinon le PNG changerait à chaque seconde. Non déclaré ici (<ctime> le déclare importé de la bibliothèque C).
long long _time64(long long *sortie) {
    if (sortie != nullptr) {
        *sortie = g_instant;
    }
    return g_instant;
}
}

namespace banc {

void definir_carte(const char *nom, const char *controleur_ecran, const char *controleur_tactile) {
    g_carte.board_name = nom;
    g_carte.display_controller = controleur_ecran;
    g_carte.touch_controller = controleur_tactile;
}

void definir_duree_demarrage_ms(int32_t ms) { g_jalons.lvgl_ready_ms = ms; }

void definir_zone(const char *nom, float latitude, float longitude, const char *fuseau_iana) {
    std::snprintf(g_zone.nom, sizeof g_zone.nom, "%s", nom);
    g_zone.latitude = latitude;
    g_zone.longitude = longitude;
    std::snprintf(g_zone.fuseau, sizeof g_zone.fuseau, "%s", fuseau_iana);
}

} // namespace banc

// meteo_zone.h : sur l'appareil, la zone vient de la NVS (main/meteo_zone.cpp) ; ici, celle que la scène a posée.
bool zone_lire(ZoneMeteo *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    *sortie = g_zone;
    return true;
}

int main(int argc, char **argv) {
    const char *variante = nullptr;
    const char *sortie = nullptr;
    bool lister = false;
    ecran::i18n::Langue langue = ecran::i18n::Langue::Fr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--lister") == 0) {
            lister = true;
        } else if (std::strcmp(argv[i], "--variante") == 0 && i + 1 < argc) {
            variante = argv[++i];
        } else if (std::strcmp(argv[i], "--taille") == 0 && i + 1 < argc) {
            if (!lire_taille(argv[++i], g_largeur, g_hauteur)) {
                std::fprintf(stderr, "banc : taille invalide (attendu LxH)\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--langue") == 0 && i + 1 < argc) {
            const auto trouvee = ecran::i18n::langue_depuis_code(argv[++i]);
            if (!trouvee) {
                std::fprintf(stderr, "banc : langue inconnue : %s (fr en de nl es it tr)\n", argv[i]);
                return 2;
            }
            langue = *trouvee;
        } else if (std::strcmp(argv[i], "--instant") == 0 && i + 1 < argc) {
            g_instant = std::strtoll(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--sortie") == 0 && i + 1 < argc) {
            sortie = argv[++i];
        } else {
            std::fprintf(stderr, "banc : argument inconnu : %s\n", argv[i]);
            return 2;
        }
    }

    if (lister) {
        for (std::size_t i = 0; i < banc::kNombreScenes; ++i) {
            std::printf("%s\t%s\n", banc::kScenes[i].variante, banc::kScenes[i].legende);
        }
        return 0;
    }
    const banc::Scene *scene = nullptr;
    for (std::size_t i = 0; variante != nullptr && i < banc::kNombreScenes; ++i) {
        if (std::strcmp(banc::kScenes[i].variante, variante) == 0) {
            scene = &banc::kScenes[i];
        }
    }
    if (scene == nullptr || sortie == nullptr || g_largeur == 0) {
        std::fprintf(
            stderr,
            "banc : usage : --variante X --taille LxH [--langue fr] --instant EPOCH --sortie fichier.ppm | --lister\n");
        return 2;
    }

    g_carte.width = g_largeur;
    g_carte.height = g_hauteur;
    choisir_carte_selon_taille();
    ecran::i18n::definir_langue(langue);
    g_image.assign(static_cast<size_t>(g_largeur) * g_hauteur, 0);

    lv_init();
    lv_tick_set_cb(lire_tick);
    g_affichage = lv_display_create(g_largeur, g_hauteur);
    lv_display_set_color_format(g_affichage, LV_COLOR_FORMAT_RGB565);
    const size_t octets = static_cast<size_t>(g_largeur) * kLignesParTampon * 2;
    lv_display_set_buffers(g_affichage, std::malloc(octets), nullptr, static_cast<uint32_t>(octets),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(g_affichage, sur_flush);

    // BANC_CHRONO=1 : temps de construction de l'écran et du premier rendu complet, sur le PC (durées relatives entre écrans ou
    // entre versions du dessin ; elles ne disent rien de la vitesse de la carte, voir docs/outils.md).
    const bool chrono = std::getenv("BANC_CHRONO") != nullptr;
    using horloge_pc = std::chrono::steady_clock;
    const banc::Contexte contexte = {g_largeur, g_hauteur, g_instant, g_affichage, langue};
    const auto t0 = horloge_pc::now();
    scene->construire(contexte);
    const auto t1 = horloge_pc::now();

    for (int pas = 0; pas < kNombrePas; ++pas) {
        g_ms += kPasMs;
        lv_timer_handler();
        if (chrono && pas == 0) {
            const auto t2 = horloge_pc::now();
            const auto us = [](horloge_pc::time_point a, horloge_pc::time_point b) {
                return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(b - a).count());
            };
            std::fprintf(stderr, "chrono PC : construction %lld us, premier rendu complet %lld us\n", us(t0, t1),
                         us(t1, t2));
        }
    }
    if (!ecrire_ppm(sortie)) {
        std::fprintf(stderr, "banc : écriture impossible : %s\n", sortie);
        return 1;
    }
    return 0;
}
