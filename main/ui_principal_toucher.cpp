// Écran principal : le toucher (anneau sous le doigt, appui long) et la création des zones.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- tactile : anneau sous le doigt, appui long ---------------------------------------------------------------------------

namespace {

void montrer(lv_obj_t *objet, bool visible) {
    const bool cache = lv_obj_has_flag(objet, LV_OBJ_FLAG_HIDDEN);
    if (visible && cache) {
        lv_obj_remove_flag(objet, LV_OBJ_FLAG_HIDDEN);
    } else if (!visible && !cache) {
        lv_obj_add_flag(objet, LV_OBJ_FLAG_HIDDEN);
    }
}
} // namespace

void sur_toucher(lv_event_t *evenement) {
    switch (lv_event_get_code(evenement)) {
    case LV_EVENT_PRESSED:
        g.appui_long_fait = false;
        if (lv_indev_t *indev = lv_indev_active(); indev != nullptr) {
            lv_indev_get_point(indev, &g.appui_depart);
        }
        [[fallthrough]];
    case LV_EVENT_PRESSING: {
        // Après un appui long, le doigt est encore posé : LVGL continue d'envoyer les événements à cet écran. L'anneau est sur la
        // couche supérieure, donc visible sur le nouvel écran : on l'ignore jusqu'au relâchement.
        lv_indev_t *indev = lv_indev_active();
        if (g.appui_long_fait || indev == nullptr || g.anneau == nullptr) {
            return;
        }
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_obj_set_pos(g.anneau, p.x - g.diametre_anneau / 2, p.y - g.diametre_anneau / 2);
        montrer(g.anneau, true);
        break;
    }
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        g.appui_long_fait = false;
        if (g.anneau != nullptr) {
            montrer(g.anneau, false);
        }
        break;
    case LV_EVENT_LONG_PRESSED:
        if (g.appui_long != nullptr) {
            // La roue s'ouvre là où le doigt est, s'il n'a pas glissé depuis qu'il s'est posé (un glissement n'est pas un appui long).
            lv_point_t p = g.appui_depart;
            if (lv_indev_t *indev = lv_indev_active(); indev != nullptr) {
                lv_indev_get_point(indev, &p);
            }
            if (roue::a_glisse({g.appui_depart.x, g.appui_depart.y}, {p.x, p.y}, g.seuil_glissement)) {
                break;
            }
            g.appui_long_fait = true;
            if (g.anneau != nullptr) {
                montrer(g.anneau, false);
            }
            g.appui_long(p.x, p.y);
        }
        break;
    default:
        break;
    }
}

lv_obj_t *creer_zone(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    ++g.objets;
    return o;
}

} // namespace principal
} // namespace ecran
