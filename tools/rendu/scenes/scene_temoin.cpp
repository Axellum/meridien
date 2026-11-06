// Scène témoin des langues et des glyphes : un petit écran écrit pour le banc (pas un écran du firmware) qui prouve deux choses
// avant que les vrais écrans traduits existent : (1) la langue demandée au banc (--langue) arrive bien dans tr() du noyau pur ;
// (2) la police embarquée (Roboto Bold, TinyTTF) a les glyphes des sept langues : un glyphe manquant se voit comme un trou.
// C'est aussi le modèle le plus court d'une scène : copier ce fichier, changer les directives et la fonction construire().
//
// Directives lues par tools/rendu/rendre.py :
// rendu-sources:
// rendu-langues: toutes
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720
// rendu-sequence: temoin
#include <cstddef>
#include <cstdint>
#include <string>

#include "banc.h"
#include "ecran/i18n.hpp"
#include "lvgl.h"
#include "palette.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

const ecran::Palette &kCouleurs = ecran::kPaletteSombre;

lv_font_t *police(int32_t taille) {
    return lv_tiny_ttf_create_data(roboto_700_start, roboto_700_end - roboto_700_start, taille);
}

lv_obj_t *libelle(lv_obj_t *parent, lv_font_t *font, uint32_t couleur, const char *texte) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(couleur), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, texte);
    return l;
}

lv_obj_t *bouton(lv_obj_t *parent, lv_font_t *font, const char *texte) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_style_bg_color(b, lv_color_hex(kCouleurs.surface), 0);
    lv_obj_set_style_radius(b, 20, 0);
    lv_obj_set_style_pad_hor(b, 28, 0);
    lv_obj_set_style_pad_ver(b, 16, 0);
    lv_obj_t *t = lv_label_create(b);
    lv_obj_set_style_text_font(t, font, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(kCouleurs.bouton_texte), 0);
    lv_label_set_text(t, texte);
    return b;
}

void temoin(const banc::Contexte &) {
    namespace i18n = ecran::i18n;
    lv_font_t *grande = police(52);
    lv_font_t *moyenne = police(34);
    lv_font_t *petite = police(26);

    lv_obj_t *ecran = lv_display_get_screen_active(lv_display_get_default());
    lv_obj_set_style_bg_color(ecran, lv_color_hex(kCouleurs.fond), 0);
    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ecran, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ecran, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(ecran, 24, 0);
    lv_obj_set_style_pad_row(ecran, 14, 0);

    // Les textes viennent de la table du noyau pur, dans la langue courante (le français est la clé).
    libelle(ecran, grande, kCouleurs.accent, i18n::tr_c("Langue de l’écran"));

    std::string noms;
    for (std::size_t i = 0; i < i18n::NOMBRE_LANGUES; ++i) {
        noms += i == 0 ? "" : "  ·  ";
        noms += i18n::NOMS[i];
    }
    libelle(ecran, moyenne, kCouleurs.texte, noms.c_str());

    lv_obj_t *rangee = lv_obj_create(ecran);
    lv_obj_remove_style_all(rangee);
    lv_obj_set_size(rangee, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rangee, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rangee, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rangee, 24, 0);
    bouton(rangee, moyenne, i18n::tr_c("Continuer"));
    bouton(rangee, moyenne, i18n::tr_c("Redémarrer"));

    libelle(ecran, moyenne, kCouleurs.texte, i18n::tr_c("Luminosité"));
    libelle(ecran, moyenne, kCouleurs.texte_discret, i18n::tr_c("Mot de passe du Wi-Fi"));

    // Les glyphes à surveiller : un trou ou un carré à la place d'une lettre = la police ne l'a pas.
    libelle(ecran, petite, kCouleurs.texte_discret, "fr  àâäçéèêëîïôöùûüÿœæ  ÀÂÇÉÈÊËÎÏÔÙÛÜŸŒÆ  « » ’ … ° €");
    libelle(ecran, petite, kCouleurs.texte_discret, "de  äöüß  ÄÖÜ   nl  ëï  é   es  áéíóúñ¿¡  ÁÉÍÓÚÑ   it  àèéìòù");
    libelle(ecran, petite, kCouleurs.texte_discret, "tr  çğıöşü  ÇĞİÖŞÜ   İstanbul'da şiirsel ığdır çiğ köfte");
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"langues", "Langue courante et glyphes des sept langues", temoin},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
