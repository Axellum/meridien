#include "ui_principal.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ecran/heure_fr.hpp"
#include "palette.h"
#include "textes_principal.h"

// Écran principal en paysage (1280 x 720 sur le Tab5) :
//
//   +--------------------------------------------+------------------+
//   |                                            |  [Exemple]       |
//   |            11:40                           |  12 °C           |
//   |            jeudi 6 novembre 2026           |  Partiellement   |
//   |                                            |  nuageux         |
//   |                                            |  min 8° · max 14°|
//   +--------------------------------------------+------------------+
//
// Aucune couleur écrite en dur (rôles de palette.h), aucune icône (la licence du pack d'icônes météo n'est pas lue : texte et
// formes simples), transitions instantanées. LVGL pur : voir ui_principal.h.

namespace ecran {

namespace {

const Palette &kCouleurs = kPaletteSombre;

// Tailles de police en pixels. Roboto Bold : chiffres de 0,55 em environ, donc « 11:40 » fait ~2,5 em de large (≈ 670 px à 270).
constexpr int32_t kPoliceHeure = 270;
constexpr int32_t kPoliceDate = 52;
constexpr int32_t kPoliceTemperature = 120;
constexpr int32_t kPoliceLibelle = 40;
constexpr int32_t kPolicePetite = 32;

constexpr int32_t kMarge = 48;         // marge de l'écran
constexpr int32_t kLargeurCarte = 400; // bloc météo
constexpr int32_t kRemplissageCarte = 28;
constexpr int32_t kRayonCarte = 32;
constexpr int32_t kDiametreAnneau = 88; // retour tactile : anneau sous le doigt

struct Vue {
    lv_obj_t *heure = nullptr;
    lv_obj_t *date = nullptr;
    lv_obj_t *exemple = nullptr;
    lv_obj_t *temperature = nullptr;
    lv_obj_t *libelle = nullptr;
    lv_obj_t *min_max = nullptr;
    lv_obj_t *perimee = nullptr;
    lv_obj_t *anneau = nullptr;
    void (*appui_long)() = nullptr;
    bool appui_long_fait = false; // l'appui long a déclenché le rappel ; ignorer le doigt jusqu'au relâchement
};

Vue g;

lv_color_t couleur(uint32_t rgb) { return lv_color_hex(rgb); }

lv_obj_t *creer_etiquette(lv_obj_t *parent, const lv_font_t *police, uint32_t couleur_rgb) {
    lv_obj_t *e = lv_label_create(parent);
    lv_label_set_long_mode(e, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(e, police, 0);
    lv_obj_set_style_text_color(e, couleur(couleur_rgb), 0);
    lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
    return e;
}

// Ne redessine que si le texte a changé.
void changer_texte(lv_obj_t *etiquette, const char *neuf) {
    const char *actuel = lv_label_get_text(etiquette);
    if (actuel == nullptr || std::strcmp(actuel, neuf) != 0) {
        lv_label_set_text(etiquette, neuf);
    }
}

void montrer(lv_obj_t *objet, bool visible) {
    const bool cache = lv_obj_has_flag(objet, LV_OBJ_FLAG_HIDDEN);
    if (visible && cache) {
        lv_obj_remove_flag(objet, LV_OBJ_FLAG_HIDDEN);
    } else if (!visible && !cache) {
        lv_obj_add_flag(objet, LV_OBJ_FLAG_HIDDEN);
    }
}

// Dixièmes de degré depuis un flottant ; faux si la valeur n'est pas un nombre utilisable (NaN, infini, hors limites).
bool en_dixiemes(float degres, int &dixiemes) {
    if (!std::isfinite(degres) || degres < -99.0F || degres > 99.0F) {
        return false;
    }
    dixiemes = static_cast<int>(std::lround(degres * 10.0F));
    return true;
}

void sur_toucher(lv_event_t *evenement) {
    switch (lv_event_get_code(evenement)) {
    case LV_EVENT_PRESSED:
        g.appui_long_fait = false;
        [[fallthrough]];
    case LV_EVENT_PRESSING: {
        // Après un appui long, le doigt est encore posé : LVGL continue d'envoyer les événements à cet écran (un changement
        // d'écran ne remet pas le pointeur à zéro). L'anneau est sur la couche supérieure, donc visible sur le nouvel écran :
        // on l'ignore jusqu'au relâchement.
        lv_indev_t *indev = lv_indev_active();
        if (g.appui_long_fait || indev == nullptr || g.anneau == nullptr) {
            return;
        }
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_obj_set_pos(g.anneau, p.x - kDiametreAnneau / 2, p.y - kDiametreAnneau / 2);
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
            g.appui_long_fait = true;
            if (g.anneau != nullptr) {
                montrer(g.anneau, false);
            }
            g.appui_long();
        }
        break;
    default:
        break;
    }
}

} // namespace

bool ui_principal_creer(lv_obj_t *ecran, const uint8_t *ttf, std::size_t ttf_taille, int32_t largeur, int32_t hauteur) {
    lv_font_t *police_heure = lv_tiny_ttf_create_data(ttf, ttf_taille, kPoliceHeure);
    lv_font_t *police_date = lv_tiny_ttf_create_data(ttf, ttf_taille, kPoliceDate);
    lv_font_t *police_temperature = lv_tiny_ttf_create_data(ttf, ttf_taille, kPoliceTemperature);
    lv_font_t *police_libelle = lv_tiny_ttf_create_data(ttf, ttf_taille, kPoliceLibelle);
    lv_font_t *police_petite = lv_tiny_ttf_create_data(ttf, ttf_taille, kPolicePetite);
    if (police_heure == nullptr || police_date == nullptr || police_temperature == nullptr ||
        police_libelle == nullptr || police_petite == nullptr) {
        return false;
    }

    lv_obj_set_style_bg_color(ecran, couleur(kCouleurs.fond), 0);
    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_add_flag(ecran, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE); // un glissement ne doit pas faire défiler l'écran
    lv_obj_add_event_cb(ecran, sur_toucher, LV_EVENT_ALL, nullptr);

    // --- Horloge et date : à gauche, centrées dans l'espace laissé par le bloc météo ---
    const int32_t largeur_horloge = largeur - 3 * kMarge - kLargeurCarte;
    lv_obj_t *zone = lv_obj_create(ecran);
    lv_obj_remove_style_all(zone);
    lv_obj_remove_flag(zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(zone, largeur_horloge, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(zone, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(zone, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(zone, 8, 0);
    lv_obj_align(zone, LV_ALIGN_LEFT_MID, kMarge, 0);

    g.heure = creer_etiquette(zone, police_heure, kCouleurs.accent);
    lv_obj_set_width(g.heure, largeur_horloge);
    lv_label_set_long_mode(g.heure, LV_LABEL_LONG_CLIP);
    lv_label_set_text(g.heure, texte_principal::kHeureAbsente);
    g.date = creer_etiquette(zone, police_date, kCouleurs.texte);
    lv_obj_set_width(g.date, largeur_horloge);
    lv_label_set_text(g.date, texte_principal::kDateAttente);

    // --- Bloc météo : à droite ---
    lv_obj_t *carte = lv_obj_create(ecran);
    lv_obj_remove_style_all(carte);
    lv_obj_remove_flag(carte, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(carte, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(carte, kLargeurCarte, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(carte, couleur(kCouleurs.surface), 0);
    lv_obj_set_style_bg_opa(carte, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(carte, kRayonCarte, 0);
    lv_obj_set_style_pad_all(carte, kRemplissageCarte, 0);
    lv_obj_set_style_pad_row(carte, 6, 0);
    lv_obj_set_flex_flow(carte, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(carte, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(carte, LV_ALIGN_RIGHT_MID, -kMarge, 0);

    const int32_t largeur_utile = kLargeurCarte - 2 * kRemplissageCarte;
    g.exemple = creer_etiquette(carte, police_petite, kCouleurs.avertissement);
    lv_obj_set_width(g.exemple, largeur_utile);
    lv_label_set_text(g.exemple, texte_principal::kExemple);
    lv_obj_add_flag(g.exemple, LV_OBJ_FLAG_HIDDEN);

    g.temperature = creer_etiquette(carte, police_temperature, kCouleurs.texte);
    lv_obj_set_width(g.temperature, largeur_utile);
    lv_label_set_long_mode(g.temperature, LV_LABEL_LONG_CLIP);
    lv_label_set_text(g.temperature, "");
    lv_obj_add_flag(g.temperature, LV_OBJ_FLAG_HIDDEN);

    g.libelle = creer_etiquette(carte, police_libelle, kCouleurs.texte);
    lv_obj_set_width(g.libelle, largeur_utile);
    lv_label_set_text(g.libelle, texte_principal::kMeteoIndisponible);
    lv_obj_set_style_text_color(g.libelle, couleur(kCouleurs.texte_discret), 0);

    g.min_max = creer_etiquette(carte, police_petite, kCouleurs.texte_discret);
    lv_obj_set_width(g.min_max, largeur_utile);
    lv_label_set_text(g.min_max, "");
    lv_obj_add_flag(g.min_max, LV_OBJ_FLAG_HIDDEN);

    g.perimee = creer_etiquette(carte, police_petite, kCouleurs.avertissement);
    lv_obj_set_width(g.perimee, largeur_utile);
    lv_label_set_text(g.perimee, texte_principal::kPerimee);
    lv_obj_add_flag(g.perimee, LV_OBJ_FLAG_HIDDEN);

    // --- Retour tactile : un anneau sous le doigt, sur la couche supérieure (coordonnées d'écran exactes) ---
    lv_obj_t *couche = lv_display_get_layer_top(lv_obj_get_display(ecran));
    g.anneau = lv_obj_create(couche);
    lv_obj_remove_style_all(g.anneau);
    lv_obj_remove_flag(g.anneau, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g.anneau, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g.anneau, kDiametreAnneau, kDiametreAnneau);
    lv_obj_set_style_radius(g.anneau, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g.anneau, 6, 0);
    lv_obj_set_style_border_color(g.anneau, couleur(kCouleurs.accent), 0);
    lv_obj_add_flag(g.anneau, LV_OBJ_FLAG_HIDDEN);

    (void)hauteur; // la mise en page se cale au centre vertical : seule la largeur est utilisée
    return true;
}

void ui_principal_mettre_a_jour(const EntreesPrincipal &entrees) {
    if (g.heure == nullptr) {
        return;
    }

    // Heure et date.
    if (entrees.heure_connue) {
        changer_texte(g.heure, entrees.heure);
        changer_texte(g.date, entrees.date != nullptr ? entrees.date : "");
    } else {
        changer_texte(g.heure, texte_principal::kHeureAbsente);
        changer_texte(g.date, entrees.date_attente != nullptr ? entrees.date_attente : texte_principal::kDateAttente);
    }

    // Météo.
    const MeteoEtat &m = entrees.meteo;
    montrer(g.exemple, entrees.meteo_exemple);
    if (!m.valide) {
        montrer(g.temperature, false);
        montrer(g.min_max, false);
        montrer(g.perimee, false);
        changer_texte(g.libelle, texte_principal::kMeteoIndisponible);
        lv_obj_set_style_text_color(g.libelle, couleur(kCouleurs.texte_discret), 0);
        return;
    }

    char tampon[40];
    int dixiemes = 0;
    if (en_dixiemes(m.temperature_c, dixiemes) &&
        ecran::heure::formater_temperature(dixiemes, tampon, sizeof tampon) > 0) {
        changer_texte(g.temperature, tampon);
        montrer(g.temperature, true);
    } else {
        montrer(g.temperature, false);
    }

    changer_texte(g.libelle, m.libelle[0] != '\0' ? m.libelle : texte_principal::kMeteoIndisponible);
    lv_obj_set_style_text_color(g.libelle, couleur(kCouleurs.texte), 0);

    int mini = 0;
    int maxi = 0;
    char petit_min[16];
    char petit_max[16];
    if (en_dixiemes(m.min_c, mini) && en_dixiemes(m.max_c, maxi) &&
        ecran::heure::formater_degre(mini, petit_min, sizeof petit_min) > 0 &&
        ecran::heure::formater_degre(maxi, petit_max, sizeof petit_max) > 0) {
        char texte_min_max
            [48]; // « min -99° · max -99° » tient largement ; assez grand pour que le compilateur le prouve
        std::snprintf(texte_min_max, sizeof texte_min_max, texte_principal::kMinMax, petit_min, petit_max);
        changer_texte(g.min_max, texte_min_max);
        montrer(g.min_max, true);
    } else {
        montrer(g.min_max, false);
    }

    montrer(g.perimee, m.perime);
}

void ui_principal_sur_appui_long(void (*rappel)()) { g.appui_long = rappel; }

} // namespace ecran
