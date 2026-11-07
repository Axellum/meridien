// ESSAI SUR LA CARTE des réglages et de l'installation guidée, SANS DOIGT : un pointeur virtuel (un second « doigt » LVGL) rejoue
// les gestes sur le LVGL réel de l'appareil, par la même recherche d'objet que le tactile (lv_indev_search_obj), au-dessus de la vraie
// mémoire permanente (NVS), du vrai rétroéclairage et du vrai redémarrage logiciel. Les constats sont écrits au journal série, une ligne
// « AUTOTEST OK|ECHEC : ... » chacun, et une ligne « AUTOTEST FIN : ... » à la fin.
//
// Ce que cela prouve : les écrans s'ouvrent et se ferment sur la carte sans fuite de mémoire interne mesurable, un geste sur une cible
// arrive bien à l'objet visé, les valeurs écrites par l'écran sont retrouvées après un VRAI redémarrage (luminosité, texte d'un
// service, étape de l'installation), l'installation reprend à la bonne étape et ne se rouvre plus une fois terminée.
// Ce que cela NE prouve PAS : le toucher du doigt (le panneau tactile n'est pas utilisé), le rendu à l'œil (le journal liste les textes
// affichés, pas les pixels), le Wi-Fi ni la recherche de ville.
//
// JAMAIS dans le binaire normal : compilé seulement avec  .\tools\idf.ps1 -DMERIDIEN_AUTOTEST_REGLAGES=ON build  (voir
// main/CMakeLists.txt). Il se déclenche seul 12 s après le démarrage, en trois phases séparées par un redémarrage (étape dans l'espace
// NVS « autotest »), puis remet la luminosité et le texte d'origine et ne fait plus rien aux démarrages suivants.
#include <cstdio>
#include <cstring>

#include "board.h"
#include "ecran/i18n.hpp"
#include "ecran/installation.hpp"
#include "ecran/registre_reglages.hpp"
#include "ecran/texte_modele.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hote_appareil_esp.h"
#include "lvgl.h"
#include "nvs.h"
#include "reglages.h"
#include "ui_reglages.h"
#include "ui_reglages_interne.h"

extern "C" void autotest_reglages_ancre() {} // référencée par l'éditeur de liens pour que ce fichier soit gardé

namespace {

const char *TAG = "autotest";
constexpr const char *kEspace = "autotest";
constexpr int kDepartMs = 12000;
constexpr int kEssaisMax = 8;

int g_verifs = 0;
int g_echecs = 0;

void verifier(bool ok, const char *quoi) {
    ++g_verifs;
    if (!ok) {
        ++g_echecs;
    }
    ESP_LOGI(TAG, "AUTOTEST %s : %s", ok ? "OK" : "ECHEC", quoi);
}

// ---- mémoire de l'essai (survit au redémarrage) ------------------------------------------------------------------------------
std::int32_t lire(const char *cle, std::int32_t defaut) {
    nvs_handle_t h = 0;
    if (nvs_open(kEspace, NVS_READONLY, &h) != ESP_OK) {
        return defaut;
    }
    std::int32_t v = defaut;
    if (nvs_get_i32(h, cle, &v) != ESP_OK) {
        v = defaut;
    }
    nvs_close(h);
    return v;
}

void ecrire(const char *cle, std::int32_t v) {
    nvs_handle_t h = 0;
    if (nvs_open(kEspace, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, cle, v);
        nvs_commit(h);
        nvs_close(h);
    }
}

// L'installation guidée revient à « carte neuve » (aucun octet d'étape) : l'état d'avant tout essai.
void effacer_etape_installation() {
    nvs_handle_t h = 0;
    if (nvs_open("installation", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, "etape");
        nvs_commit(h);
        nvs_close(h);
    }
}

// ---- LVGL : verrou, attente, recherche d'objets ----------------------------------------------------------------------------------
bool verrou() { return board_display_lock(3000); }

// Rend la main à LVGL (ses minuteurs, ses animations, le rafraîchissement) puis reprend le verrou.
void attendre(int ms) {
    board_display_unlock();
    vTaskDelay(pdMS_TO_TICKS(ms));
    if (!verrou()) {
        ESP_LOGE(TAG, "verrou LVGL perdu");
    }
}

bool est_label_de(lv_obj_t *o, const char *texte) {
    return lv_obj_check_type(o, &lv_label_class) && std::strcmp(lv_label_get_text(o), texte) == 0;
}

lv_obj_t *trouver_label(lv_obj_t *o, const char *texte) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        return nullptr;
    }
    if (est_label_de(o, texte)) {
        return o;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *t = trouver_label(lv_obj_get_child(o, i), texte);
        if (t != nullptr) {
            return t;
        }
    }
    return nullptr;
}

lv_obj_t *trouver_type(lv_obj_t *o, const lv_obj_class_t *classe) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        return nullptr;
    }
    if (lv_obj_check_type(o, classe)) {
        return o;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *t = trouver_type(lv_obj_get_child(o, i), classe);
        if (t != nullptr) {
            return t;
        }
    }
    return nullptr;
}

lv_obj_t *bouton(const char *texte_fr) {
    for (lv_obj_t *o = trouver_label(lv_screen_active(), ecran::i18n::tr_c(texte_fr)); o != nullptr;
         o = lv_obj_get_parent(o)) {
        if (lv_obj_check_type(o, &lv_button_class)) {
            return o;
        }
    }
    return nullptr;
}

// Une ligne du journal avec les textes affichés (sans les pixels).
void lister_textes(lv_obj_t *o, char *tampon, std::size_t cap, std::size_t &n, int &nb) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) || nb >= 40) {
        return;
    }
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o);
        if (t != nullptr && t[0] != '\0' && n + 4 < cap) {
            n += static_cast<std::size_t>(std::snprintf(tampon + n, cap - n, "%s« %s »", nb == 0 ? "" : " | ", t));
            ++nb;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lister_textes(lv_obj_get_child(o, i), tampon, cap, n, nb);
    }
}

void ecran_vu(const char *quand) {
    static char tampon[900];
    std::size_t n = 0;
    int nb = 0;
    tampon[0] = '\0';
    lister_textes(lv_screen_active(), tampon, sizeof tampon, n, nb);
    ESP_LOGI(TAG, "ECRAN %s (%d textes) : %s", quand, nb, tampon);
}

void memoire(const char *quand) {
    ESP_LOGI(TAG, "MEMOIRE %s interne_libre=%u plus_gros_bloc=%u interne_min=%u psram_libre=%u", quand,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

// ---- le doigt virtuel -----------------------------------------------------------------------------------------------------------
lv_indev_t *g_doigt = nullptr;
lv_point_t g_position{0, 0};
bool g_appuye = false;

void lire_doigt(lv_indev_t *, lv_indev_data_t *donnees) {
    donnees->point = g_position;
    donnees->state = g_appuye ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void poser(int32_t x, int32_t y, bool appuye) {
    if (g_doigt == nullptr) {
        g_doigt = lv_indev_create();
        lv_indev_set_type(g_doigt, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(g_doigt, lire_doigt);
        lv_indev_set_display(g_doigt, board_get_display());
    }
    g_position = {x, y};
    g_appuye = appuye;
    lv_indev_read(g_doigt);
}

// Ce que reçoit l'objet visé pendant le geste : sert à distinguer « le toucher n'arrive pas » de « il arrive mais rien ne suit ».
int g_recu_appui = 0;
int g_recu_relache = 0;
int g_recu_clic = 0;

void espion(lv_event_t *e) {
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        ++g_recu_appui;
        break;
    case LV_EVENT_RELEASED:
        ++g_recu_relache;
        break;
    case LV_EVENT_CLICKED:
        ++g_recu_clic;
        break;
    default:
        break;
    }
}

// Le sommet de la pile d'écrans des réglages, tel que le voit le code de l'écran (type, section ou étape).
void pile_vue(const char *quand) {
    const ecran::ui::Entree *haut = ecran::ui::nav_haut();
    if (haut == nullptr) {
        ESP_LOGI(TAG, "PILE %s : vide", quand);
        return;
    }
    ESP_LOGI(TAG, "PILE %s : profondeur %d, dessus de type %d (a=%d b=%d)", quand, ecran::ui::nav_profondeur(),
             static_cast<int>(haut->type), haut->a, haut->b);
}

// Un appui court au point donné (coordonnées de l'écran), puis le relâchement, puis le temps de réagir. Si `cible` est donnée,
// on compte ce qu'elle reçoit (appui, relâchement, clic) et on le met au journal ; l'espion est retiré avant l'attente parce
// que la cible peut être supprimée par le clic.
void toucher_point(int32_t x, int32_t y, lv_obj_t *cible = nullptr, const char *nom = nullptr) {
    g_recu_appui = g_recu_relache = g_recu_clic = 0;
    if (cible != nullptr) {
        lv_obj_add_event_cb(cible, espion, LV_EVENT_ALL, nullptr);
    }
    poser(x, y, true);
    poser(x, y, false);
    if (cible != nullptr) {
        lv_obj_remove_event_cb(cible, espion);
        ESP_LOGI(TAG, "GESTE sur « %s » en (%d, %d) : appui reçu %d, relâchement reçu %d, clic reçu %d", nom,
                 static_cast<int>(x), static_cast<int>(y), g_recu_appui, g_recu_relache, g_recu_clic);
    }
    attendre(400);
}

// Touche le centre d'un objet APRÈS avoir vérifié que la recherche d'objet du toucher y trouve bien lui (ou un de ses enfants).
bool toucher_objet(lv_obj_t *o, const char *nom) {
    if (o == nullptr) {
        char phrase[96];
        std::snprintf(phrase, sizeof phrase, "« %s » est à l'écran", nom);
        verifier(false, phrase);
        return false;
    }
    lv_obj_scroll_to_view_recursive(o, LV_ANIM_OFF);
    lv_obj_update_layout(o);
    lv_area_t z;
    lv_obj_get_coords(o, &z);
    const lv_point_t centre = {(z.x1 + z.x2) / 2, (z.y1 + z.y2) / 2};
    lv_point_t p = centre;
    lv_obj_t *recu = lv_indev_search_obj(lv_screen_active(), &p);
    bool atteint = false;
    for (lv_obj_t *c = recu; c != nullptr; c = lv_obj_get_parent(c)) {
        if (c == o) {
            atteint = true;
            break;
        }
    }
    char phrase[120];
    std::snprintf(phrase, sizeof phrase, "le toucher au centre de « %s » (%d x %d px) arrive à l'objet visé", nom,
                  static_cast<int>(lv_area_get_width(&z)), static_cast<int>(lv_area_get_height(&z)));
    verifier(atteint, phrase);
    toucher_point(centre.x, centre.y, o, nom);
    pile_vue(nom);
    return atteint;
}

bool toucher_texte(const char *texte_fr) { return toucher_objet(bouton(texte_fr), texte_fr); }

// Remonte d'un écran par « Retour » jusqu'à retrouver l'écran `cible` (au plus 8 fois).
bool revenir_a(lv_obj_t *cible) {
    for (int i = 0; i < 8 && lv_screen_active() != cible; ++i) {
        lv_obj_t *retour = bouton("Retour");
        if (retour == nullptr) {
            break;
        }
        toucher_objet(retour, "Retour");
    }
    return lv_screen_active() == cible;
}

// ---- les phases -------------------------------------------------------------------------------------------------------------------

// Phase 1 : ouvrir, régler la luminosité au doigt, saisir un texte au clavier, écrire l'étape, fermer, redémarrer par le bouton.
void phase_1() {
    ecran::HoteReglages &hote = ecran::hote_de_l_appareil();
    const int lum0 = ecran::reglages_affichage().luminosite;
    ecrire("lum0", lum0);
    char phrase[160];
    std::snprintf(phrase, sizeof phrase, "départ : luminosité %d %%, installation à l'étape %d (terminée : %s)", lum0,
                  static_cast<int>(hote.progression().etape), hote.progression().terminee ? "oui" : "non");
    verifier(true, phrase);
    lv_obj_t *principal = lv_screen_active();
    memoire("avant ouverture");

    ui_reglages_ouvrir();
    memoire("réglages construits, avant le premier dessin"); // objets et styles seuls
    attendre(600);
    verifier(lv_screen_active() != principal, "les réglages s'ouvrent sur un écran à part");
    ecran_vu("liste des réglages");
    memoire("réglages ouverts"); // + les bitmaps de glyphes dessinés (cache de TinyTTF)

    // La section « Affichage » au doigt, puis le curseur de luminosité à 40 % de sa course.
    toucher_texte("Affichage");
    ecran_vu("section Affichage");
    lv_obj_t *curseur = trouver_type(lv_screen_active(), &lv_slider_class);
    verifier(curseur != nullptr, "le curseur de luminosité est affiché");
    if (curseur != nullptr) {
        lv_obj_update_layout(curseur);
        lv_area_t z;
        lv_obj_get_coords(curseur, &z);
        const int32_t x = z.x1 + (lv_area_get_width(&z) * 4) / 10;
        const int32_t y = (z.y1 + z.y2) / 2;
        lv_point_t p = {x, y};
        verifier(lv_indev_search_obj(lv_screen_active(), &p) == curseur, "le toucher sur la barre arrive au curseur");
        toucher_point(x, y);
        const int lum = ecran::reglages_affichage().luminosite;
        ecrire("lum", lum);
        std::snprintf(phrase, sizeof phrase, "la luminosité est passée de %d %% à %d %% (source de la page web)", lum0,
                      lum);
        verifier(lum != lum0 && lum >= 5 && lum <= 100, phrase);
        hote.definir_nombre("affichage.luminosite",
                            lum); // inchangé : la valeur vue par l'hôte est celle de la source unique
        verifier(hote.nombre("affichage.luminosite") == lum,
                 "l'hôte relit la même valeur que la source de la page web");
    }

    // Revenir à la liste, puis la fermer par « Retour » : l'écran principal revient, la mémoire interne ne baisse pas.
    verifier(revenir_a(principal), "« Retour » ramène l'écran principal");
    attendre(600);
    memoire("réglages fermés (1re fois)");

    // Trois ouvertures/fermetures de plus : la mémoire interne ne doit plus baisser une fois les glyphes en cache.
    unsigned apres_premiere = static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    for (int tour = 2; tour <= 4; ++tour) {
        ui_reglages_ouvrir();
        attendre(600);
        const bool ouvert = lv_screen_active() != principal;
        const bool ferme = revenir_a(principal);
        attendre(600);
        char nom[48];
        std::snprintf(nom, sizeof nom, "réglages fermés (%de fois)", tour);
        memoire(nom);
        verifier(ouvert && ferme, "ouverture puis « Retour » : l'écran principal revient");
    }
    const unsigned apres_quatrieme = static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    std::snprintf(phrase, sizeof phrase,
                  "pas de fuite de mémoire interne sur 3 ouvertures de plus : %u octets avant, %u après",
                  apres_premiere, apres_quatrieme);
    verifier(apres_quatrieme + 512 >= apres_premiere, phrase);

    // Un texte au clavier (modèle du service LLM), par l'éditeur réel.
    ui_reglages_ouvrir();
    attendre(600);
    ecran::ui_aller("section:4:1/editeur:llm.modele");
    attendre(400);
    ecran::ui_simuler_saisie("essai", true);
    attendre(400);
    char texte[128] = {};
    hote.texte("llm.modele", texte, sizeof texte);
    verifier(std::strcmp(texte, "essai") == 0, "le texte saisi est gardé par l'hôte");

    // L'étape de l'installation, comme après un « Continuer » à l'étape 2.
    hote.definir_progression({ecran::registre::Etape::Lieu, false, ecran::installation::Mode::Premiere});
    verifier(hote.progression().etape == ecran::registre::Etape::Lieu, "l'étape 3 est écrite");

    // Le bouton « Redémarrer » de la page Appareil : un vrai redémarrage logiciel.
    ecran::ui_aller("section:5");
    attendre(400);
    ecran_vu("section Appareil");
    ecrire("phase", 2);
    ESP_LOGI(TAG, "AUTOTEST PHASE 1 FINIE : %d vérification(s), %d échec(s) ; redémarrage par le bouton « Redémarrer »",
             g_verifs, g_echecs);
    ecrire("echecs", lire("echecs", 0) + g_echecs);
    ecrire("verifs", lire("verifs", 0) + g_verifs);
    toucher_texte("Redémarrer"); // la carte redémarre ici
    attendre(2000);
    ESP_LOGE(TAG, "AUTOTEST ECHEC : la carte n'a pas redémarré après « Redémarrer »");
    ecrire("phase", 1);
}

// Phase 2 (après le redémarrage) : tout est retrouvé, l'installation reprend à l'étape 3.
void phase_2() {
    ecran::HoteReglages &hote = ecran::hote_de_l_appareil();
    char phrase[160];
    const int lum = ecran::reglages_affichage().luminosite;
    const int attendue = lire("lum", -1);
    std::snprintf(phrase, sizeof phrase, "après le redémarrage, la luminosité est %d %% (écrite avant : %d %%)", lum,
                  attendue);
    verifier(lum == attendue, phrase);
    char texte[128] = {};
    hote.texte("llm.modele", texte, sizeof texte);
    verifier(std::strcmp(texte, "essai") == 0, "après le redémarrage, le texte du service est retrouvé");
    verifier(hote.progression().etape == ecran::registre::Etape::Lieu && !hote.progression().terminee,
             "après le redémarrage, l'installation en cours est retrouvée à l'étape 3");

    lv_obj_t *principal = lv_screen_active();
    memoire("avant reprise");
    ui_installation_ouvrir_si_necessaire();
    attendre(600);
    verifier(lv_screen_active() != principal, "l'installation reprend sur un écran à part");
    ecran_vu("installation reprise");
    char a[8];
    char b[8];
    char etape[96];
    std::snprintf(a, sizeof a, "%d", 3);
    std::snprintf(b, sizeof b, "%d", ecran::registre::NOMBRE_ETAPES);
    const ecran::texte::Parametre p[] = {{"n", a}, {"total", b}};
    ecran::texte::remplir(ecran::i18n::tr_c("Étape {n} sur {total}"), p, 2, etape, sizeof etape);
    verifier(trouver_label(lv_screen_active(), etape) != nullptr, "l'écran affiche l'étape 3 sur 7");

    // « Retour » à l'étape 2 : l'écran l'écrit. On met ensuite l'installation à « terminée » et on redémarre une dernière fois.
    toucher_texte("Retour");
    verifier(hote.progression().etape == ecran::registre::Etape::Wifi, "« Retour » écrit l'étape 2");
    hote.definir_progression({ecran::registre::Etape::Langue, true, ecran::installation::Mode::Premiere});
    ecrire("phase", 3);
    ecrire("echecs", lire("echecs", 0) + g_echecs);
    ecrire("verifs", lire("verifs", 0) + g_verifs);
    ESP_LOGI(TAG, "AUTOTEST PHASE 2 FINIE : %d vérification(s), %d échec(s) ; redémarrage par l'hôte", g_verifs,
             g_echecs);
    hote.redemarrer();
}

// Phase 3 : l'installation terminée ne se rouvre pas ; on remet l'état d'origine.
void phase_3() {
    ecran::HoteReglages &hote = ecran::hote_de_l_appareil();
    lv_obj_t *principal = lv_screen_active();
    verifier(hote.progression().terminee, "après le redémarrage, l'installation est terminée");
    ui_installation_ouvrir_si_necessaire();
    attendre(500);
    verifier(lv_screen_active() == principal, "l'installation terminée ne se rouvre pas");

    // Remise de l'état d'origine : luminosité et texte du service.
    const int lum0 = lire("lum0", 75);
    hote.definir_nombre("affichage.luminosite", lum0);
    hote.definir_texte("llm.modele", "");
    char texte[128] = {};
    hote.texte("llm.modele", texte, sizeof texte);
    if (texte[0] != '\0') {
        // Le registre refuse peut-être un texte vide : on retire la clé de l'espace des services (comme avant l'essai).
        nvs_handle_t h = 0;
        if (nvs_open("services", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_key(h, "llm.modele");
            nvs_commit(h);
            nvs_close(h);
        }
        texte[0] = '\0';
        hote.texte("llm.modele", texte, sizeof texte);
    }
    effacer_etape_installation(); // la progression d'installation aussi : la carte repart « neuve », comme avant l'essai
    verifier(ecran::reglages_affichage().luminosite == lum0 && texte[0] == '\0' && !hote.progression().terminee,
             "luminosité, texte du service et progression d'installation remis comme avant l'essai");
    memoire("fin");

    const int verifs = lire("verifs", 0) + g_verifs;
    const int echecs = lire("echecs", 0) + g_echecs;
    ESP_LOGI(TAG, "AUTOTEST FIN : %d vérification(s), %d échec(s)", verifs, echecs);
    ecrire("phase", 9); // fini : aucun démarrage suivant ne rejoue l'essai
}

// Un binaire d'essai compilé après le précédent rejoue tout : phase, essais et compteurs repartent de zéro et l'installation
// guidée de « carte neuve ». Sans cela, un essai fini (phase 9) ne se rejouerait jamais.
void repartir_si_binaire_neuf() {
    static const char kBinaire[] = __DATE__ " " __TIME__;
    char vu[40] = {};
    nvs_handle_t h = 0;
    if (nvs_open(kEspace, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    std::size_t taille = sizeof vu;
    const bool connu = nvs_get_str(h, "binaire", vu, &taille) == ESP_OK && std::strcmp(vu, kBinaire) == 0;
    if (!connu) {
        nvs_set_i32(h, "phase", 0);
        nvs_set_i32(h, "essais", 0);
        nvs_set_i32(h, "echecs", 0);
        nvs_set_i32(h, "verifs", 0);
        nvs_set_str(h, "binaire", kBinaire);
        nvs_commit(h);
    }
    nvs_close(h);
    if (!connu) {
        effacer_etape_installation();
        ESP_LOGI(TAG, "AUTOTEST binaire neuf (%s) : l'essai repart de la phase 1", kBinaire);
    }
}

void tache(void *) {
    vTaskDelay(pdMS_TO_TICKS(kDepartMs));
    for (int i = 0; i < 20 && board_get_display() == nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    repartir_si_binaire_neuf();
    const std::int32_t phase = lire("phase", 0);
    if (phase >= 9) {
        vTaskDelete(nullptr);
    }
    const std::int32_t essais = lire("essais", 0);
    if (essais >= kEssaisMax) {
        ESP_LOGE(TAG, "AUTOTEST ECHEC : %d essais sans fin, arrêt", static_cast<int>(essais));
        ecrire("phase", 9);
        vTaskDelete(nullptr);
    }
    ecrire("essais", essais + 1);
    ESP_LOGI(TAG, "AUTOTEST DEBUT phase %d (essai %d)", static_cast<int>(phase == 0 ? 1 : phase),
             static_cast<int>(essais + 1));
    if (!verrou()) {
        ESP_LOGE(TAG, "AUTOTEST ECHEC : verrou LVGL indisponible");
        vTaskDelete(nullptr);
    }
    if (phase == 0 || phase == 1) {
        phase_1();
    } else if (phase == 2) {
        phase_2();
    } else if (phase == 3) {
        phase_3();
    }
    board_display_unlock();
    vTaskDelete(nullptr);
}

// Se déclenche seul au démarrage : ni main.cpp ni l'écran principal n'ont à connaître cet essai.
struct Demarrage {
    Demarrage() { xTaskCreate(&tache, "autotest", 8192, nullptr, 2, nullptr); }
};
Demarrage g_demarrage;

} // namespace
