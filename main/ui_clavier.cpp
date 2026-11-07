// Clavier à l'écran (ADR-0007, point 4) : le composant clavier de LVGL, alimenté par les dispositions du noyau pur
// (ecran/clavier.hpp : sept langues, accents, majuscule pour UNE lettre, chiffres, signes). Les touches de commande portent des
// libellés en lettres (« Aa », « ?123 », « abc », « <- », « OK ») : la police embarquée n'a ni flèches ni symboles.
// LVGL pur, aucun include ESP-IDF.
#include <cstring>

#include "ecran/clavier.hpp"
#include "ui_reglages_interne.h"

namespace ecran::ui {

namespace {

constexpr std::size_t kEntreesMax = 112; // 6 rangées de 13 touches + séparateurs + fin
constexpr std::size_t kPoolOctets = 3 * 84 * 6;

constexpr lv_style_selector_t kItemsPresse =
    static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_PRESSED);
constexpr lv_style_selector_t kItemsCommande =
    static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_USER_1);
constexpr lv_style_selector_t kItemsValider =
    static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_USER_2);
constexpr lv_style_selector_t kItemsCoche =
    static_cast<lv_style_selector_t>(LV_PART_ITEMS) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED);

constexpr const char *kLibelleMaj = "Aa";
constexpr const char *kLibelleSymboles = "?123";
constexpr const char *kLibelleLettres = "abc";
constexpr const char *kLibelleEfface = "<-";
constexpr const char *kLibelleValider = "OK";
constexpr const char *kLibelleEspace = " ";

const char *g_carte[3][kEntreesMax];
lv_buttonmatrix_ctrl_t g_ctrl[3][kEntreesMax];
char g_pool[kPoolOctets];
std::size_t g_pool_pos = 0;

lv_obj_t *g_clavier = nullptr;
lv_obj_t *g_champ = nullptr;
void (*g_valider)(void *) = nullptr;
void *g_valider_donnee = nullptr;
clavier::Mode g_mode = clavier::Mode::Minuscules;

const char *garder(std::string_view s) {
    if (g_pool_pos + s.size() + 1 > kPoolOctets) {
        return "?"; // la place est calculée pour le plus grand clavier ; ceci n'arrive pas
    }
    char *debut = &g_pool[g_pool_pos];
    std::memcpy(debut, s.data(), s.size());
    debut[s.size()] = '\0';
    g_pool_pos += s.size() + 1;
    return debut;
}

const char *libelle_de(std::string_view touche) {
    switch (clavier::controle(touche)) {
    case clavier::Controle::Maj:
        return kLibelleMaj;
    case clavier::Controle::Symboles:
        return kLibelleSymboles;
    case clavier::Controle::Lettres:
        return kLibelleLettres;
    case clavier::Controle::Efface:
        return kLibelleEfface;
    case clavier::Controle::Valider:
        return kLibelleValider;
    case clavier::Controle::Espace:
        return kLibelleEspace;
    case clavier::Controle::Aucun:
        break;
    }
    return garder(touche);
}

// Largeurs relatives (dans une même rangée) : une lettre 2, une commande 3, l'espace 8.
lv_buttonmatrix_ctrl_t reglages_touche(std::string_view touche, bool majuscule_active) {
    unsigned largeur = 2;
    unsigned drapeaux = LV_BUTTONMATRIX_CTRL_NO_REPEAT;
    switch (clavier::controle(touche)) {
    case clavier::Controle::Maj:
        largeur = 3;
        drapeaux |= LV_BUTTONMATRIX_CTRL_CUSTOM_1;
        if (majuscule_active) {
            drapeaux |= LV_BUTTONMATRIX_CTRL_CHECKED;
        }
        break;
    case clavier::Controle::Symboles:
    case clavier::Controle::Lettres:
        largeur = 3;
        drapeaux |= LV_BUTTONMATRIX_CTRL_CUSTOM_1;
        break;
    case clavier::Controle::Valider:
        largeur = 3;
        drapeaux |= LV_BUTTONMATRIX_CTRL_CUSTOM_2; // la touche « OK » prend la couleur d'accent
        break;
    case clavier::Controle::Efface:
        largeur = 3;
        drapeaux |= LV_BUTTONMATRIX_CTRL_CUSTOM_1;
        drapeaux &=
            ~static_cast<unsigned>(LV_BUTTONMATRIX_CTRL_NO_REPEAT); // l'effacement se répète si on laisse le doigt
        break;
    case clavier::Controle::Espace:
        largeur = 8;
        break;
    case clavier::Controle::Aucun:
        break;
    }
    return static_cast<lv_buttonmatrix_ctrl_t>(largeur | drapeaux);
}

void construire_cartes(i18n::Langue langue) {
    g_pool_pos = 0;
    for (std::size_t m = 0; m < clavier::NOMBRE_MODES; ++m) {
        const clavier::Mode mode = static_cast<clavier::Mode>(m);
        const clavier::Disposition d = clavier::disposition(langue, mode);
        std::size_t n = 0;
        for (std::size_t l = 0; l < d.nb && n + 2 < kEntreesMax; ++l) {
            const std::size_t touches = clavier::compter_touches(d.lignes[l]);
            for (std::size_t k = 0; k < touches && n + 2 < kEntreesMax; ++k) {
                const std::string_view t = clavier::touche(d.lignes[l], k);
                g_carte[m][n] = libelle_de(t);
                g_ctrl[m][n] = reglages_touche(t, mode == clavier::Mode::Majuscules);
                ++n;
            }
            if (l + 1 < d.nb) {
                g_carte[m][n] = "\n";
                g_ctrl[m][n] = LV_BUTTONMATRIX_CTRL_NONE; // un séparateur de rangée n'a pas de réglage propre dans LVGL
                // Le tableau de contrôle ignore les séparateurs : on l'écrit sans eux ci-dessous.
                ++n;
            }
        }
        g_carte[m][n] = "";
        // La table des contrôles ne compte QUE les boutons : on la compacte (les "\n" en sont retirés).
        std::size_t dest = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::strcmp(g_carte[m][i], "\n") != 0) {
                g_ctrl[m][dest++] = g_ctrl[m][i];
            }
        }
    }
}

lv_keyboard_mode_t mode_lvgl(clavier::Mode m) {
    return static_cast<lv_keyboard_mode_t>(static_cast<int>(LV_KEYBOARD_MODE_USER_1) + static_cast<int>(m));
}

void appliquer_mode() { lv_keyboard_set_mode(g_clavier, mode_lvgl(g_mode)); }

void sur_touche(lv_event_t *e) {
    lv_obj_t *kb = lv_event_get_target_obj(e);
    const uint32_t id = lv_keyboard_get_selected_button(kb);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE || g_champ == nullptr) {
        return;
    }
    const char *txt = lv_keyboard_get_button_text(kb, id);
    if (txt == nullptr) {
        return;
    }
    if (std::strcmp(txt, kLibelleMaj) == 0) {
        g_mode = g_mode == clavier::Mode::Majuscules ? clavier::Mode::Minuscules : clavier::Mode::Majuscules;
        appliquer_mode();
    } else if (std::strcmp(txt, kLibelleSymboles) == 0) {
        g_mode = clavier::Mode::Symboles;
        appliquer_mode();
    } else if (std::strcmp(txt, kLibelleLettres) == 0) {
        g_mode = clavier::Mode::Minuscules;
        appliquer_mode();
    } else if (std::strcmp(txt, kLibelleEfface) == 0) {
        lv_textarea_delete_char(g_champ);
    } else if (std::strcmp(txt, kLibelleValider) == 0) {
        if (g_valider != nullptr) {
            g_valider(g_valider_donnee);
        }
    } else {
        lv_textarea_add_text(g_champ, txt);        // « " " » pour l'espace, sinon le caractère de la touche
        if (g_mode == clavier::Mode::Majuscules) { // la majuscule ne vaut que pour UNE lettre
            g_mode = clavier::Mode::Minuscules;
            appliquer_mode();
        }
    }
}

void sur_suppression(lv_event_t *e) {
    // Un ancien clavier supprimé après la création du nouveau (suppression différée) ne doit pas défaire le nouveau.
    if (lv_event_get_target_obj(e) != g_clavier) {
        return;
    }
    g_clavier = nullptr;
    g_champ = nullptr;
    g_valider = nullptr;
    g_valider_donnee = nullptr;
}

} // namespace

void clavier_valider() {
    if (g_valider != nullptr) {
        g_valider(g_valider_donnee);
    }
}

int32_t clavier_hauteur(int32_t hauteur_ecran, bool avec_rangee_accents) {
    const int32_t rangees = avec_rangee_accents ? 6 : 5;
    const int32_t voulue = rangees * S(80);
    const int32_t permise =
        hauteur_ecran * 11 / 20; // jamais plus de 55 % de l'écran : il reste la barre, le champ et un message
    return voulue < permise ? voulue : permise;
}

lv_obj_t *clavier_creer(lv_obj_t *parent, lv_obj_t *textarea, int32_t hauteur_voulue, void (*sur_valider)(void *),
                        void *donnee) {
    const PaletteReglages &p = *env().p;
    const i18n::Langue langue = i18n::langue_courante();
    construire_cartes(langue);

    lv_obj_t *kb = lv_keyboard_create(parent);
    lv_obj_remove_event_cb(kb,
                           lv_keyboard_def_event_cb); // la logique par défaut ne connaît ni nos libellés ni nos cartes
    lv_keyboard_set_popovers(kb, false);
    for (std::size_t m = 0; m < clavier::NOMBRE_MODES; ++m) {
        lv_keyboard_set_map(kb, mode_lvgl(static_cast<clavier::Mode>(m)), g_carte[m], g_ctrl[m]);
    }
    g_clavier = kb;
    g_champ = textarea;
    g_valider = sur_valider;
    g_valider_donnee = donnee;
    g_mode = clavier::Mode::Minuscules;
    appliquer_mode();
    lv_keyboard_set_textarea(kb, nullptr); // la saisie passe par sur_touche, pas par la liaison de LVGL

    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_height(kb, hauteur_voulue);
    lv_obj_set_style_bg_color(kb, couleur(p.fond), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, S(6), 0);
    lv_obj_set_style_pad_row(kb, S(6), 0);
    lv_obj_set_style_pad_column(kb, S(6), 0);
    lv_obj_set_style_bg_color(kb, couleur(p.touche), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, S(10), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, env().f.touche, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, couleur(p.texte), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, couleur(p.touche_commande), kItemsCommande);
    lv_obj_set_style_bg_color(kb, couleur(p.accent), kItemsValider);
    lv_obj_set_style_text_color(kb, couleur(p.texte_sur_accent), kItemsValider);
    lv_obj_set_style_bg_color(kb, couleur(p.accent), kItemsPresse);
    lv_obj_set_style_text_color(kb, couleur(p.texte_sur_accent), kItemsPresse);
    lv_obj_set_style_bg_color(kb, couleur(p.accent), kItemsCoche);
    lv_obj_set_style_text_color(kb, couleur(p.texte_sur_accent), kItemsCoche);

    lv_obj_add_event_cb(kb, sur_touche, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(kb, sur_suppression, LV_EVENT_DELETE, nullptr);
    return kb;
}

} // namespace ecran::ui
