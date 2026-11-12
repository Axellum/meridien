// Endurance de l'écran principal sur le PC (tools/rendu/endurance.py, docs/outils.md) : la mémoire de LVGL ne doit pas grandir
// quand l'écran tourne des jours.
//
// Compile main/ui_principal.cpp tel quel avec LVGL (même bibliothèque que le banc de rendu), puis fait passer des minutes comme
// l'appareil : à chaque minute, l'heure change et ui_principal_mettre_a_jour() est appelée (deux fois, comme les passages de
// 500 ms qui tombent dans la même minute), suivie des passages de rendu de LVGL ; toutes les 15 min une autre météo arrive (un
// cycle de quatre temps, le même chaque jour). Les allocations de LVGL passent par un compteur : ce fichier définit lv_malloc_core
// et ses voisines, comme main/lvgl_memoire.cpp sur l'appareil, de sorte que le membre de la bibliothèque qui les définit
// (stdlib/clib/lv_mem_core_clib.c) n'est pas lié.
//
// Ce que la mesure a montré (12/11/2026, docs/depannage.md) : la mémoire de LVGL grandit pendant le premier jour, c'est le
// remplissage des caches de TinyTTF (une paire de chiffres « HH:MM » encore jamais mesurée = une entrée de plus dans le cache de
// crénage de la police de l'heure : trois blocs à chaque minute pendant la première heure, puis à chaque heure nouvelle), bornés
// par CONFIG_LV_TINY_TTF_CACHE_GLYPH_CNT et CONFIG_LV_TINY_TTF_CACHE_KERNING_CNT. Une date jamais vue ajoute encore quelques
// entrées à minuit (≈ 3 Ko par jour sur le PC, de moins en moins, jusqu'à ce que l'année soit vue). Le contrôle rejoue donc LA
// MÊME journée trois fois : la première remplit les caches, la troisième doit tenir EXACTEMENT les mêmes octets et les mêmes blocs
// que la deuxième à chaque minute (les images refaites selon l'heure, le ciel et la météo changent de taille dans la journée, pas
// d'une journée rejouée à l'autre). Une vraie fuite, même de quelques octets par minute, le fait échouer : --injecter-fuite en
// simule une pour le prouver (endurance.py le vérifie à chaque passage).
//
// Ce que ce contrôle ne couvre pas : ce qui n'est pas LVGL (clients réseau, TLS, esp_hosted, lwIP), les autres pages, la taille
// des blocs sur la puce (32 bits, en-têtes du tas d'ESP-IDF) : sur le PC (64 bits) une entrée du cache de crénage pèse 108 o.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "banc.h"
#include "board.h"
#include "lvgl.h"
#include "meteo_zone.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_principal.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

// ---- compteur des allocations de LVGL --------------------------------------------------------------------------------------
// Un en-tête devant chaque bloc garde sa taille : les octets vivants sont exacts, sans dépendre de l'allocateur du PC.
struct EnTete {
    std::size_t taille;
    std::size_t bourrage; // garde l'alignement de 16 octets de malloc
};
long long g_octets = 0;
long long g_blocs = 0;

// ---- bouchons de l'interface des cartes et du temps (comme banc.cpp, sans son main) ---------------------------------------
int64_t g_instant = 0;
uint32_t g_ms = 0;
lv_display_t *g_affichage = nullptr;
ZoneMeteo g_zone = {"Lyon", 45.74906F, 4.84789F, "Europe/Paris"};
board_info_t g_carte = {"M5Stack Tab5", "ST7121", "Sitronix (version 1)", 0, 0};
board_boot_timeline_t g_jalons = {0, 0, 0, 0, 612};

uint32_t lire_tick() { return g_ms; }
void sur_flush(lv_display_t *affichage, const lv_area_t *, uint8_t *) { lv_display_flush_ready(affichage); }

} // namespace

extern "C" {

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *, size_t) { return nullptr; }
void lv_mem_remove_pool(lv_mem_pool_t) {}

void *lv_malloc_core(size_t taille) {
    auto *e = static_cast<EnTete *>(std::malloc(taille + sizeof(EnTete)));
    if (e == nullptr) {
        return nullptr;
    }
    e->taille = taille;
    g_octets += static_cast<long long>(taille);
    ++g_blocs;
    return e + 1;
}

void lv_free_core(void *p) {
    if (p == nullptr) {
        return;
    }
    EnTete *e = static_cast<EnTete *>(p) - 1;
    g_octets -= static_cast<long long>(e->taille);
    --g_blocs;
    std::free(e);
}

void *lv_realloc_core(void *p, size_t taille) {
    if (p == nullptr) {
        return lv_malloc_core(taille);
    }
    EnTete *e = static_cast<EnTete *>(p) - 1;
    const long long ancienne = static_cast<long long>(e->taille);
    auto *neuf = static_cast<EnTete *>(std::realloc(e, taille + sizeof(EnTete)));
    if (neuf == nullptr) {
        return nullptr;
    }
    neuf->taille = taille;
    g_octets += static_cast<long long>(taille) - ancienne;
    return neuf + 1;
}

void lv_mem_monitor_core(lv_mem_monitor_t *moniteur) { std::memset(moniteur, 0, sizeof *moniteur); }
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

const board_info_t *board_get_info(void) { return &g_carte; }
const board_boot_timeline_t *board_get_boot_timeline(void) { return &g_jalons; }
lv_display_t *board_get_display(void) { return g_affichage; }
bool board_display_lock(uint32_t) { return true; }
void board_display_unlock(void) {}
esp_err_t board_set_backlight_percent(int) { return ESP_OK; }
void esp_rom_software_reset_system(void) { std::exit(3); }

// std::time(nullptr) sous MinGW (voir banc.cpp) : l'instant simulé.
long long _time64(long long *sortie) {
    if (sortie != nullptr) {
        *sortie = g_instant;
    }
    return g_instant;
}

} // extern "C"

bool zone_lire(ZoneMeteo *sortie) {
    if (sortie == nullptr) {
        return false;
    }
    *sortie = g_zone;
    return true;
}

namespace {

constexpr int kMinutesParJour = 24 * 60;
constexpr int kJours = 3; // la même journée, rejouée : remplissage, puis deux journées comparées minute par minute
constexpr int kPassagesParMinute =
    2; // ui_principal_mettre_a_jour : deux passages de 500 ms au moins tombent dans une minute
constexpr int kRendusParPassage = 10; // 10 x 50 ms de LVGL : le rendu de ce qui a été invalidé
// 12/11/2026 00:00 à Lyon (UTC+1 en novembre) : la journée simulée commence à minuit local.
constexpr int64_t kDebut = 1794438000;

const donnees::Cas *const kTemps[] = {&donnees::SOLEIL, &donnees::PLUIE, &donnees::BROUILLARD, &donnees::NEIGE};

struct Releve {
    long long octets;
    long long blocs;
};

Releve g_journee[kJours][kMinutesParJour];

} // namespace

int main(int argc, char **argv) {
    int32_t largeur = 1280;
    int32_t hauteur = 720;
    bool injecter = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--taille") == 0 && i + 1 < argc) {
            int l = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &l, &h) != 2 || l < 16 || h < 16) {
                std::fprintf(stderr, "endurance : taille invalide\n");
                return 2;
            }
            largeur = l;
            hauteur = h;
        } else if (std::strcmp(argv[i], "--injecter-fuite") == 0) {
            injecter = true;
        } else {
            std::fprintf(stderr, "endurance : usage : [--taille LxH] [--injecter-fuite]\n");
            return 2;
        }
    }
    g_carte.width = largeur;
    g_carte.height = hauteur;
    if (largeur == 1024 && hauteur == 600) {
        g_carte.board_name = "Waveshare ESP32-P4 7B";
        g_carte.display_controller = "EK79007";
        g_carte.touch_controller = "GT911";
    }
    ecran::i18n::definir_langue(ecran::i18n::Langue::Fr);
    g_instant = kDebut;

    lv_init();
    lv_tick_set_cb(lire_tick);
    g_affichage = lv_display_create(largeur, hauteur);
    lv_display_set_color_format(g_affichage, LV_COLOR_FORMAT_RGB565);
    const size_t octets = static_cast<size_t>(largeur) * 50 * 2; // tampons partiels de 50 lignes, comme le Tab5
    lv_display_set_buffers(g_affichage, std::malloc(octets), nullptr, static_cast<uint32_t>(octets),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(g_affichage, sur_flush);

    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(ecran_lvgl, ressources, largeur, hauteur)) {
        std::fprintf(stderr, "endurance : écran principal non créé (police)\n");
        return 2;
    }
    lv_screen_load(ecran_lvgl);

    const banc::Contexte contexte = {largeur, hauteur, kDebut, g_affichage, ecran::i18n::Langue::Fr};
    for (int jour = 0; jour < kJours; ++jour) {
        for (int minute = 0; minute < kMinutesParJour; ++minute) {
            const int64_t decalage = static_cast<int64_t>(minute) * 60;
            g_instant = kDebut + decalage;
            const int quart = minute / 15;
            // une météo neuve au quart d'heure, la même d'une journée rejouée à l'autre
            const donnees::VueMeteo meteo =
                donnees::vue(*kTemps[quart % 4], kDebut + static_cast<int64_t>(quart) * 900);
            const ecran::EntreesPrincipal entrees = donnees::entrees(contexte, decalage, meteo);
            for (int passage = 0; passage < kPassagesParMinute; ++passage) {
                ecran::ui_principal_mettre_a_jour(entrees);
                for (int r = 0; r < kRendusParPassage; ++r) {
                    g_ms += 50;
                    lv_timer_handler();
                }
            }
            if (injecter) {
                (void)lv_malloc(108); // une fuite de 108 o par minute, jamais rendue : le contrôle doit échouer
            }
            g_journee[jour][minute] = {g_octets, g_blocs};
        }
    }

    const Releve *premier = g_journee[0];
    std::printf("endurance %dx%d : LVGL à la fin de la première journée : %lld o, %lld blocs\n",
                static_cast<int>(largeur), static_cast<int>(hauteur), premier[kMinutesParJour - 1].octets,
                premier[kMinutesParJour - 1].blocs);
    std::printf("  minutes 5 à 116 de la première journée (fenêtre de la mesure de la 7B) : %+lld o, soit %.0f o/min "
                "(caches qui "
                "se remplissent)\n",
                premier[116].octets - premier[5].octets,
                static_cast<double>(premier[116].octets - premier[5].octets) / 111.0);
    bool fuite = false;
    for (int jour = 1; jour < kJours; ++jour) {
        int ecarts = 0;
        int premier_ecart = -1;
        for (int minute = 0; minute < kMinutesParJour; ++minute) {
            const Releve &avant = g_journee[jour - 1][minute];
            const Releve &maintenant = g_journee[jour][minute];
            if (avant.octets != maintenant.octets || avant.blocs != maintenant.blocs) {
                if (premier_ecart < 0) {
                    premier_ecart = minute;
                }
                ++ecarts;
            }
        }
        const Releve &fin_avant = g_journee[jour - 1][kMinutesParJour - 1];
        const Releve &fin = g_journee[jour][kMinutesParJour - 1];
        std::printf("  journée rejouée %d contre %d : %d minute(s) différente(s) sur %d ; fin de journée %+lld o, "
                    "%+lld blocs\n",
                    jour + 1, jour, ecarts, kMinutesParJour, fin.octets - fin_avant.octets,
                    fin.blocs - fin_avant.blocs);
        // La journée 2 finit de remplir ce que la première n'a pas vu (le retour de 23 h 59 à minuit) : seule la 3e est jugée.
        if (jour >= 2 && ecarts > 0) {
            std::printf("    première différence à %02d:%02d\n", premier_ecart / 60, premier_ecart % 60);
            fuite = true;
        }
    }
    if (fuite) {
        std::printf("ECHEC : la mémoire de LVGL change d'une journée rejouée à l'autre (fuite)\n");
        return 1;
    }
    std::printf("OK : la troisième journée rejouée tient exactement la mémoire de la deuxième, minute par minute\n");
    return 0;
}
