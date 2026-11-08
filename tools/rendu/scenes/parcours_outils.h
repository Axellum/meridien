// Outils des scènes « parcours » (scene_parcours.cpp) : toucher un bouton par son texte, taper au clavier, faire tourner les
// minuteurs, et dire ECHEC (code de sortie non nul, donc le rendu s'arrête) quand le comportement n'est pas celui attendu.
// Ce n'est pas du firmware : c'est un doigt virtuel pour le banc de rendu PC (hors carte). Il ne remplace pas un essai sur la carte.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "banc.h"
#include "ecran/i18n.hpp"
#include "hote_memoire.h"
#include "lvgl.h"
#include "ui_reglages.h"
#include "ui_reglages_interne.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace parcours {

inline ecran::HoteMemoire g_hote;
inline int g_fermetures = 0; // combien de fois l'interface a rendu la main à l'écran principal
inline int g_diagnostics =
    0; // combien de fois la ligne « Diagnostic » de la page Appareil a demandé l'écran de diagnostic
inline const char *g_nom = "";

[[noreturn]] inline void echec(const char *quoi) {
    std::fprintf(stderr, "ECHEC du parcours « %s » : %s\n", g_nom, quoi);
    std::exit(1);
}

inline void exiger(bool condition, const char *quoi) {
    if (!condition) {
        echec(quoi);
    }
}

// Fait tourner les minuteurs tout de suite (le banc n'avance le temps de LVGL qu'à la fin de la scène) et les suppressions différées.
inline void tourner(int fois = 3) {
    for (int i = 0; i < fois; ++i) {
        for (lv_timer_t *t = lv_timer_get_next(nullptr); t != nullptr; t = lv_timer_get_next(t)) {
            lv_timer_ready(t);
        }
        lv_timer_handler();
    }
}

inline bool visible_et(lv_obj_t *o, bool (*f)(lv_obj_t *, const void *), const void *arg, lv_obj_t **trouve) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }
    if (f(o, arg)) {
        *trouve = o;
        return true;
    }
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; ++i) {
        if (visible_et(lv_obj_get_child(o, i), f, arg, trouve)) {
            return true;
        }
    }
    return false;
}

inline lv_obj_t *chercher(bool (*f)(lv_obj_t *, const void *), const void *arg) {
    lv_obj_t *trouve = nullptr;
    visible_et(lv_screen_active(), f, arg, &trouve);
    return trouve;
}

inline bool est_label_de(lv_obj_t *o, const void *texte) {
    return lv_obj_check_type(o, &lv_label_class) &&
           std::strcmp(lv_label_get_text(o), static_cast<const char *>(texte)) == 0;
}

// Un texte (déjà dans la langue de l'écran) est-il affiché, sur un écran visible ?
inline bool affiche(const char *texte_traduit) { return chercher(est_label_de, texte_traduit) != nullptr; }

// Le texte français passe par la table de traduction, comme à l'écran.
inline bool existe(const char *texte_fr) { return affiche(ecran::i18n::tr_c(texte_fr)); }

inline lv_obj_t *bouton_de(const char *texte_traduit) {
    lv_obj_t *label = chercher(est_label_de, texte_traduit);
    for (lv_obj_t *o = label; o != nullptr; o = lv_obj_get_parent(o)) {
        if (lv_obj_check_type(o, &lv_button_class)) {
            return o;
        }
    }
    return nullptr;
}

// Un bouton désactivé (inactif) ne répond pas : comme un vrai doigt, le toucher est sans effet. Rend vrai si le bouton a répondu.
inline bool toucher_texte(const char *texte_traduit) {
    lv_obj_t *b = bouton_de(texte_traduit);
    if (b == nullptr) {
        std::fprintf(stderr, "ECHEC du parcours « %s » : pas de bouton « %s » à l'écran\n", g_nom, texte_traduit);
        std::exit(1);
    }
    if (!lv_obj_has_flag(b, LV_OBJ_FLAG_CLICKABLE) || lv_obj_has_state(b, LV_STATE_DISABLED)) {
        return false;
    }
    lv_obj_send_event(b, LV_EVENT_CLICKED, nullptr);
    tourner();
    return true;
}

inline bool toucher(const char *texte_fr) { return toucher_texte(ecran::i18n::tr_c(texte_fr)); }

inline bool est_clavier(lv_obj_t *o, const void *) { return lv_obj_check_type(o, &lv_keyboard_class); }
inline bool est_curseur(lv_obj_t *o, const void *) { return lv_obj_check_type(o, &lv_slider_class); }

// Touche une touche du clavier affiché par son libellé (« a », « Aa », « OK », « <- », « ?123 »).
inline void taper(const char *touche) {
    lv_obj_t *kb = chercher(est_clavier, nullptr);
    exiger(kb != nullptr, "pas de clavier à l'écran");
    const char *const *carte = lv_buttonmatrix_get_map(kb);
    uint32_t id = 0;
    for (std::size_t i = 0; carte[i] != nullptr && carte[i][0] != '\0'; ++i) {
        if (std::strcmp(carte[i], "\n") == 0) {
            continue;
        }
        if (std::strcmp(carte[i], touche) == 0) {
            lv_buttonmatrix_set_selected_button(kb, id);
            lv_obj_send_event(kb, LV_EVENT_VALUE_CHANGED, nullptr);
            tourner();
            return;
        }
        ++id;
    }
    std::fprintf(stderr, "ECHEC du parcours « %s » : pas de touche « %s » sur le clavier\n", g_nom, touche);
    std::exit(1);
}

inline void taper_texte(const char *lettres) {
    for (const char *c = lettres; *c != '\0'; ++c) {
        const char t[2] = {*c, '\0'};
        taper(t);
    }
}

// Pose le doigt sur le curseur de luminosité à la valeur donnée, puis le relève.
inline void glisser_curseur(int32_t valeur) {
    lv_obj_t *s = chercher(est_curseur, nullptr);
    exiger(s != nullptr, "pas de curseur à l'écran");
    lv_slider_set_value(s, valeur, LV_ANIM_OFF);
    lv_obj_send_event(s, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_send_event(s, LV_EVENT_RELEASED, nullptr);
    tourner();
}

// ---- un vrai doigt : un pointeur LVGL qui passe par la recherche d'objet et les événements du toucher (lv_indev.c) ---------------------
// Contrairement à lv_obj_send_event, il vérifie AUSSI que le toucher arrive bien sur l'objet visé (zone tactile, parents, ordre).

inline lv_indev_t *g_doigt = nullptr;
inline lv_point_t g_doigt_position{0, 0};
inline bool g_doigt_appuye = false;

inline void lire_doigt(lv_indev_t *, lv_indev_data_t *donnees) {
    donnees->point = g_doigt_position;
    donnees->state = g_doigt_appuye ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

inline void poser_doigt(int32_t x, int32_t y, bool appuye) {
    if (g_doigt == nullptr) {
        g_doigt = lv_indev_create();
        lv_indev_set_type(g_doigt, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(g_doigt, lire_doigt);
    }
    g_doigt_position = {x, y};
    g_doigt_appuye = appuye;
    lv_indev_read(g_doigt);
}

// Un appui court au point donné (coordonnées de l'écran), puis le relâchement.
inline void toucher_point(int32_t x, int32_t y) {
    poser_doigt(x, y, true);
    poser_doigt(x, y, false);
    tourner();
}

// Quel objet reçoit un toucher en ce point ? (la même recherche que celle du pointeur)
inline lv_obj_t *objet_sous_le_doigt(int32_t x, int32_t y) {
    lv_point_t p{x, y};
    return lv_indev_search_obj(lv_screen_active(), &p);
}

// ---- zones tactiles ---------------------------------------------------------------------------------------------------------------

// Ce qui répond au doigt : un bouton (rangées comprises) ou un curseur. Le clavier est mesuré touche par touche (hauteur_touche).
inline bool zone_tactile_candidate(lv_obj_t *o) {
    return lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) &&
           (lv_obj_check_type(o, &lv_button_class) || lv_obj_check_type(o, &lv_slider_class));
}

// La zone où le toucher arrive vraiment sur cet objet : sa zone de clic (élargissement compris) coupée par chaque parent, car LVGL ne
// cherche dans les enfants d'un parent que si le point est dans les limites de celui-ci (lv_indev.c:636). On s'arrête au premier
// conteneur qui défile : ce qui est dedans se met en vue en faisant défiler, seuls les parents en dessous de lui (cartes, rangées)
// bornent la zone.
inline lv_area_t zone_atteignable(lv_obj_t *o) {
    lv_area_t a;
    lv_obj_get_click_area(o, &a);
    for (lv_obj_t *p = lv_obj_get_parent(o); p != nullptr; p = lv_obj_get_parent(p)) {
        if (lv_obj_has_flag(p, LV_OBJ_FLAG_SCROLLABLE)) {
            break;
        }
        lv_area_t limites;
        lv_obj_get_coords(p, &limites);
        a.x1 = a.x1 > limites.x1 ? a.x1 : limites.x1;
        a.y1 = a.y1 > limites.y1 ? a.y1 : limites.y1;
        a.x2 = a.x2 < limites.x2 ? a.x2 : limites.x2;
        a.y2 = a.y2 < limites.y2 ? a.y2 : limites.y2;
        if (a.x2 < a.x1 || a.y2 < a.y1) {
            return {0, 0, -1, -1};
        }
    }
    return a;
}

inline const char *texte_du_bouton(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *c = lv_obj_get_child(o, i);
        if (lv_obj_check_type(c, &lv_label_class)) {
            return lv_label_get_text(c);
        }
    }
    return "";
}

// La hauteur d'une touche du clavier : les rangées se partagent la hauteur utile moins les écarts.
inline int32_t hauteur_touche(lv_obj_t *kb) {
    const char *const *carte = lv_buttonmatrix_get_map(kb);
    int32_t rangees = 1;
    for (std::size_t i = 0; carte[i] != nullptr && carte[i][0] != '\0'; ++i) {
        if (std::strcmp(carte[i], "\n") == 0) {
            ++rangees;
        }
    }
    lv_obj_update_layout(kb);
    return (lv_obj_get_content_height(kb) - (rangees - 1) * lv_obj_get_style_pad_row(kb, LV_PART_MAIN)) / rangees;
}

// Plancher des touches du clavier (à l'échelle de 720 px), sous le minimum des boutons : avec la rangée des accents (fr, nl, es, it)
// le clavier a six rangées et ne peut pas dépasser 55 % de l'écran en paysage (il reste la barre, le champ et un message) ; les
// touches y mesurent 59 px à 1280x720 au lieu de 72 (constaté par ce parcours). Exception connue, à décider : voir le compte rendu
// de la piste « réglages-écran » (rapport du 06/11/2026).
constexpr int32_t kToucheClavierMinPx = 56;

struct BilanTactile {
    int zones = 0;
    int claviers = 0;
    int32_t plus_petite = 100000; // la plus petite des zones de boutons et de curseurs, en px
    int32_t plus_petite_touche = 100000;
    int defilements = 0; // conteneurs dont le contenu dépasse
};
inline BilanTactile g_tactile;

inline void mesurer_zones(lv_obj_t *o, const char *ou, int32_t minimum, int32_t minimum_touche) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if (zone_tactile_candidate(o)) {
        const lv_area_t a = zone_atteignable(o);
        const int32_t l = lv_area_get_width(&a);
        const int32_t h = lv_area_get_height(&a);
        const int32_t petit = l < h ? l : h;
        ++g_tactile.zones;
        if (petit < g_tactile.plus_petite) {
            g_tactile.plus_petite = petit;
        }
        if (petit < minimum - 1) { // un pixel d'arrondi de l'échelle est toléré
            std::fprintf(
                stderr,
                "ECHEC du parcours « %s » : écran « %s » : zone tactile de %d x %d px (minimum %d) sur « %s »\n", g_nom,
                ou, static_cast<int>(l), static_cast<int>(h), static_cast<int>(minimum),
                lv_obj_check_type(o, &lv_slider_class) ? "(curseur)" : texte_du_bouton(o));
            std::exit(1);
        }
    }
    if (lv_obj_check_type(o, &lv_keyboard_class)) {
        const int32_t t = hauteur_touche(o);
        ++g_tactile.claviers;
        if (t < g_tactile.plus_petite_touche) {
            g_tactile.plus_petite_touche = t;
        }
        if (t < minimum_touche - 1) {
            std::fprintf(
                stderr, "ECHEC du parcours « %s » : écran « %s » : touches du clavier de %d px de haut (plancher %d)\n",
                g_nom, ou, static_cast<int>(t), static_cast<int>(minimum_touche));
            std::exit(1);
        }
    }
    // Un conteneur dont le contenu dépasse doit le montrer (barre de défilement visible). Seuls les conteneurs ordinaires comptent : un
    // texte ou un champ de saisie a son propre « contenu » (la ligne de texte) sans que ce soit une page qui défile.
    if (lv_obj_get_class(o) == &lv_obj_class && lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) &&
        lv_obj_get_scroll_dir(o) != LV_DIR_NONE && (lv_obj_get_scroll_bottom(o) > 0 || lv_obj_get_scroll_top(o) > 0)) {
        lv_area_t horizontale;
        lv_area_t verticale;
        lv_obj_get_scrollbar_area(o, &horizontale, &verticale);
        ++g_tactile.defilements;
        if (lv_area_get_width(&verticale) <= 0 || lv_area_get_height(&verticale) <= 0) {
            lv_area_t c;
            lv_obj_get_coords(o, &c);
            std::fprintf(stderr,
                         "ECHEC du parcours « %s » : écran « %s » : un contenu dépasse sans barre de défilement "
                         "visible (%s, %d x %d px, "
                         "%d px en dessous, %d px au-dessus, mode %d)\n",
                         g_nom, ou, lv_obj_check_type(o, &lv_textarea_class) ? "champ de saisie" : "conteneur",
                         static_cast<int>(lv_area_get_width(&c)), static_cast<int>(lv_area_get_height(&c)),
                         static_cast<int>(lv_obj_get_scroll_bottom(o)), static_cast<int>(lv_obj_get_scroll_top(o)),
                         static_cast<int>(lv_obj_get_scrollbar_mode(o)));
            std::exit(1);
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        mesurer_zones(lv_obj_get_child(o, i), ou, minimum, minimum_touche);
    }
}

// Mesure l'écran visible : tout bouton et tout curseur fait au moins `ecran::ui::kToucheMinPx` (à l'échelle de l'écran), les touches du
// clavier au moins `kToucheClavierMinPx`, et un contenu qui dépasse montre sa barre de défilement.
inline void verifier_zones_tactiles(const char *ou) {
    lv_obj_update_layout(lv_screen_active());
    mesurer_zones(lv_screen_active(), ou, ecran::ui::S(ecran::ui::kToucheMinPx), ecran::ui::S(kToucheClavierMinPx));
}

inline void fermer_interface() { ++g_fermetures; }

inline const char *meteo_dans(ecran::i18n::Langue langue) {
    static const char *const noms[] = {"Nuageux", "Cloudy", "Bewölkt", "Bewolkt", "Nublado", "Nuvoloso", "Bulutlu"};
    return noms[ecran::i18n::indice(langue)];
}

enum class Interface { Reglages, Installation };

// Les données d'exemple (inventées) et l'interface demandée, comme dans scene_installation.cpp.
inline void preparer(const banc::Contexte &ctx, const char *nom, Interface quoi, int64_t delai_connexion_ms = 0) {
    g_nom = nom;
    g_hote.fixer_horloge(1000000);
    g_hote.reseaux_vus = {
        {"Maison_5G", 4, true}, {"Freebox-A1B2C3", 3, true}, {"Invite", 2, false}, {"Cafe du coin", 1, true}};
    g_hote.mot_de_passe_attendu = "bonmotdepasse";
    g_hote.delai_connexion_ms = delai_connexion_ms;
    g_hote.lieux_connus = {"Lyon, Auvergne-Rhône-Alpes, France", "Lyons, Illinois, États-Unis"};
    g_hote.meteo.disponible = true;
    g_hote.meteo.temperature_c = 12.0F;
    std::snprintf(g_hote.meteo.libelle, sizeof g_hote.meteo.libelle, "%s", meteo_dans(ctx.langue));
    std::snprintf(g_hote.info.adresse, sizeof g_hote.info.adresse, "%s", "http://192.168.1.52");
    std::snprintf(g_hote.info.code, sizeof g_hote.info.code, "%s", "482913");
    g_hote.definir_nombre("langue.ecran", ecran::i18n::indice(ctx.langue));
    g_hote.definir_nombre("langue.assistant", ecran::i18n::indice(ctx.langue));

    lv_obj_t *racine = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(racine);
    lv_obj_set_size(racine, ctx.largeur, ctx.hauteur);
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);

    ecran::ContexteReglages c;
    c.racine = racine;
    c.ttf = roboto_700_start;
    c.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    c.largeur = ctx.largeur;
    c.hauteur = ctx.hauteur;
    c.hote = &g_hote;
    c.fermer = fermer_interface;
    c.ouvrir_diagnostic = [] { ++g_diagnostics; };
    if (quoi == Interface::Installation) {
        ecran::ui_installation_creer(c);
    } else {
        ecran::ui_reglages_creer(c);
    }
    tourner();
}

} // namespace parcours
