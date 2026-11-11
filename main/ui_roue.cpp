// La roue d'appui long : l'interface LVGL (ADR-0021). Voir ui_roue.h. Rien n'est calculé ici : la géométrie vient de roue_geometrie.hpp,
// ce qu'un toucher décide de roue_commande.hpp ; ce fichier les branche sur LVGL (un objet, un minuteur, des polices) et dessine.
#include "ui_roue.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ecran/roue_geometrie.hpp"
#include "ecran/visuel_textes.hpp"
#include "ui_meteo_dessin.h"

namespace ecran {

namespace {

namespace dz = dessin;
using roue::Point;

// ---- Ce que le dessin règle (tout en rôles de thème, jamais une couleur écrite en dur) ---------------------------------------------

// Un second anneau vide. `g.second = roue::Anneau{};` plante GCC 14.2 (ESP-IDF 5.5.5) : « internal compiler error: in gimple_add_tmp_var »
// (vu à la compilation de la 7B, 08/11/2026) ; copier une constante n'a pas de temporaire à poser.
const roue::Anneau ANNEAU_VIDE = {};

constexpr std::uint8_t OPA_BANDE =
    36; // la bande de verre sous chaque anneau (couleur : texte du thème au premier anneau, accent au second ; très transparente)
constexpr std::uint8_t OPA_DISQUE = 240; // le disque d'un bouton
constexpr std::uint8_t OPA_AURA = 46;    // la lueur autour du moyeu (couleur : accent)
constexpr std::uint8_t OPA_JAUGE = 110;  // l'anneau fin autour du moyeu
constexpr int BANDE_MARGE = 14;          // débord de la bande au-delà des boutons, en pixels du Tab5
constexpr int BANDE_PAD_DEG = 12;        // débord de la bande au-delà du premier et du dernier bouton, en degrés
constexpr int FILET = 3;                 // épaisseur du contour d'un bouton, en pixels du Tab5
constexpr int POLICE_MOT = 24; // police des mots : sa ligne (28 px) est la hauteur d'un mot de la géométrie (LEGENDE_H)
constexpr int POLICE_ICONE = 40;       // icône d'un bouton (le disque fait 72)
constexpr int POLICE_ICONE_MOYEU = 52; // icône du moyeu (le moyeu fait 120)
constexpr int POINT_FAMILLE = 6; // rayon du point qui dit « ce bouton déplie un second anneau », en pixels du Tab5
constexpr const char *POINTS_DE_SUSPENSION = "\xE2\x80\xA6"; // « … »
constexpr const char *MOT_MOYEU = "Aller à";                 // la clé française ; traduite par `traduire`

// Marge autour de ce que la roue dessine (anticrénelage des disques et de l'arc, débord d'un glyphe hors de sa boîte), en pixels.
constexpr int MARGE_ZONE = 4;
constexpr double PI = 3.14159265358979323846;

struct Etat {
    bool ouverte = false;
    // L'objet de la roue, sans fond : il dessine la roue et reçoit les touchers. Depuis le 11/11/2026 il a la taille de la roue
    // (`zone_de_la_roue`) et non plus celle de l'écran : l'ouvrir ou le fermer ne fait repeindre que cette zone, pas tout l'accueil.
    // Sa zone de toucher, elle, couvre tout l'écran (`lv_obj_set_ext_click_area`) : un toucher hors de la roue la referme toujours.
    // Son nom date du voile qu'il posait sur l'écran principal avant le 09/11/2026.
    lv_obj_t *voile = nullptr;
    lv_timer_t *minuteur = nullptr;
    const visuel::ThemePrincipal *theme = nullptr;
    int32_t largeur = 0;
    int32_t hauteur = 0;
    roue::Ecran ecran{};
    roue::Mesures m{};
    int seuil = 0;
    int n = 0;
    EntreeRoue entrees[ENTREES_ROUE_MAX] = {};
    ChoixRoue choix[ENTREES_ROUE_MAX][roue::CHOIX_MAX] = {};
    bool est_famille[ENTREES_ROUE_MAX] = {};
    roue::Demande demande;
    roue::Roue roue;
    int famille = -1; // le bouton dont le second anneau est déplié ; -1 : replié
    roue::Anneau second;
    // le toucher en cours
    Point appui;
    Point courant;
    bool appuye = false;
    bool long_fait = false;
    int sous = -1; // l'élément sous le doigt, pour le retour visuel : -1 aucun
    bool sous_second = false;
    // polices gardées d'une ouverture à l'autre
    dz::Police mot;
    dz::Police icone;
    dz::Police icone_moyeu;
    std::int32_t polices_pour = 0; // largeur d'écran pour laquelle elles ont été créées
    const std::uint8_t *ttf = nullptr;
    const std::uint8_t *mdi = nullptr;
};

Etat g;

int echelle(int reference) { return std::max(1, roue::mettre_a_l_echelle(reference, g.ecran.echelle_pm)); }

// ---- textes ---------------------------------------------------------------------------------------------------------------------------

const char *traduire(const char *francais) { return visuel::texte_c(francais); }

// Vrai si l'octet est le premier d'un caractère UTF-8 (pas un octet de continuation).
bool debut_de_caractere(unsigned char c) { return (c & 0xC0) != 0x80; }

// Copie `texte` dans `sortie` ; s'il dépasse `largeur_max` pixels, le coupe à un caractère entier et ajoute « … » jusqu'à ce que ça tienne.
void ajuster_mot(char *sortie, std::size_t cap, const char *texte, const dz::Police &police, int largeur_max) {
    std::snprintf(sortie, cap, "%s", texte != nullptr ? texte : "");
    if (largeur_max <= 0 || dz::largeur_texte(police, sortie) <= largeur_max) {
        return;
    }
    std::size_t fin = std::strlen(sortie);
    while (fin > 0) {
        do {
            --fin;
        } while (fin > 0 && !debut_de_caractere(static_cast<unsigned char>(sortie[fin])));
        sortie[fin] = '\0';
        char essai[96];
        std::snprintf(essai, sizeof essai, "%s%s", sortie, POINTS_DE_SUSPENSION);
        if (dz::largeur_texte(police, essai) <= largeur_max) {
            std::snprintf(sortie, cap, "%s", essai);
            return;
        }
    }
    std::snprintf(sortie, cap, "%s", POINTS_DE_SUSPENSION);
}

// Largeur du mot que la géométrie doit prévoir : celle du texte, bornée à la boîte d'un mot (ce qui dépasse sera coupé par « … »).
int largeur_du_mot(const char *texte) {
    if (texte == nullptr || *texte == '\0') {
        return 0;
    }
    return std::min(dz::largeur_texte(g.mot, texte), g.m.legende_l);
}

// ---- icônes ---------------------------------------------------------------------------------------------------------------------------

// Le point de code d'un caractère UTF-8 (jusqu'à 4 octets) ; 0 si la chaîne est vide ou mal formée.
std::uint32_t point_de_code(const char *utf8) {
    if (utf8 == nullptr || *utf8 == '\0') {
        return 0;
    }
    const auto *s = reinterpret_cast<const unsigned char *>(utf8);
    if (s[0] < 0x80) {
        return s[0];
    }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        return static_cast<std::uint32_t>((s[0] & 0x1F) << 6 | (s[1] & 0x3F));
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        return static_cast<std::uint32_t>((s[0] & 0x0F) << 12 | (s[1] & 0x3F) << 6 | (s[2] & 0x3F));
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        return static_cast<std::uint32_t>((s[0] & 0x07) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 |
                                          (s[3] & 0x3F));
    }
    return 0;
}

// Pose un glyphe d'icône de façon que le CENTRE DE SON DESSIN soit en (cx, cy) : la boîte de ligne d'une police d'icônes n'est pas
// centrée sur le dessin, il faut lire la boîte du glyphe (même placement que lv_draw_label : voir lv_draw_label.c, draw_letter).
void icone_centree(const dz::Pinceau &pc, const dz::Police &police, const char *utf8, int cx, int cy,
                   std::uint32_t rgb) {
    const std::uint32_t point = point_de_code(utf8);
    lv_font_glyph_dsc_t d;
    if (point == 0 || !lv_font_get_glyph_dsc(police.f, &d, point, 0)) {
        return;
    }
    const int haut_de_ligne = static_cast<int>(police.f->line_height) - static_cast<int>(police.f->base_line);
    const int x = cx - static_cast<int>(d.box_w) / 2 - static_cast<int>(d.ofs_x);
    const int y = cy + static_cast<int>(d.box_h) / 2 - haut_de_ligne + static_cast<int>(d.ofs_y);
    dz::texte(pc, police, utf8, x, y, static_cast<int>(d.adv_w) + 8, LV_TEXT_ALIGN_LEFT, rgb);
}

// ---- dessin ---------------------------------------------------------------------------------------------------------------------------

// Ramène un angle dans [0, 360[.
int dans_le_tour(int degres) { return ((degres % 360) + 360) % 360; }

// La bande de verre sous un anneau : un arc de la largeur d'un bouton, de quelques degrés au-delà des boutons extrêmes. Les angles de la
// géométrie (disposer, roue_geometrie.hpp) : y = moyeu.y - r sin(angle) quand la roue est au-dessus du moyeu (90 : en haut, sens
// trigonométrique), mais y = moyeu.y + r sin(angle) quand elle est en dessous (`dessous`, 90 : en bas). Ceux de LVGL tournent dans le
// sens horaire depuis la droite : l'angle de la géométrie tel quel en dessous, 360 - angle au-dessus.
struct ArcDeBande {
    int exterieur; // rayon extérieur, en pixels
    int epaisseur;
    int debut; // angles de LVGL, dans [0, 360[
    int fin;
};

ArcDeBande arc_de_bande(const roue::Anneau &a, int rayon, int diametre) {
    int plus_petit = a.bouton[0].angle;
    int plus_grand = a.bouton[0].angle;
    for (int i = 1; i < a.n; ++i) {
        plus_petit = std::min(plus_petit, a.bouton[i].angle);
        plus_grand = std::max(plus_grand, a.bouton[i].angle);
    }
    const int marge = echelle(BANDE_MARGE);
    const int premier = g.roue.dessous ? plus_petit : 360 - plus_grand;
    const int dernier = g.roue.dessous ? plus_grand : 360 - plus_petit;
    return {rayon + diametre / 2 + marge, diametre + 2 * marge, dans_le_tour(premier - BANDE_PAD_DEG),
            dans_le_tour(dernier + BANDE_PAD_DEG)};
}

void bande(const dz::Pinceau &pc, const roue::Anneau &a, int rayon, int diametre, std::uint32_t couleur) {
    if (a.n <= 0) {
        return;
    }
    const ArcDeBande b = arc_de_bande(a, rayon, diametre);
    dz::arc(pc, g.roue.moyeu.x, g.roue.moyeu.y, b.exterieur, b.epaisseur, b.debut, b.fin, couleur, OPA_BANDE, true);
}

void dessiner_mot(const dz::Pinceau &pc, const roue::Mot &mot, const char *texte) {
    if (mot.largeur <= 0 || texte == nullptr || *texte == '\0') {
        return;
    }
    char tampon[96];
    ajuster_mot(tampon, sizeof tampon, texte, g.mot, mot.largeur);
    // Le mot s'aligne sur son bouton : à gauche du bouton il finit au bouton, à droite il commence au bouton, sinon il est centré.
    const lv_text_align_t alignement = mot.cote == roue::Cote::Gauche   ? LV_TEXT_ALIGN_RIGHT
                                       : mot.cote == roue::Cote::Droite ? LV_TEXT_ALIGN_LEFT
                                                                        : LV_TEXT_ALIGN_CENTER;
    dz::texte(pc, g.mot, tampon, mot.x - mot.largeur / 2, mot.y - dz::hauteur_ligne(g.mot) / 2, mot.largeur, alignement,
              g.theme->texte);
}

void dessiner_bouton(const dz::Pinceau &pc, const roue::Bouton &b, const char *icone, bool presse, bool actif,
                     bool famille) {
    const visuel::ThemePrincipal &t = *g.theme;
    const int rayon = g.m.diametre_1 / 2;
    dz::disque(pc, b.x, b.y, rayon, presse ? t.accent : t.carte,
               presse ? static_cast<std::uint8_t>(LV_OPA_COVER) : OPA_DISQUE);
    dz::anneau(pc, b.x, b.y, rayon, echelle(FILET), presse || actif ? t.accent : t.filet);
    icone_centree(pc, g.icone, icone, b.x, b.y, presse ? t.texte_sur_couleur : (actif ? t.accent : t.texte));
    if (famille) { // le point : ce bouton déplie un second anneau
        const int r = echelle(POINT_FAMILLE);
        dz::disque(pc, b.x + rayon * 7 / 10, b.y - rayon * 7 / 10, r, presse ? t.texte_sur_couleur : t.texte);
    }
}

void dessiner_moyeu(const dz::Pinceau &pc) {
    const visuel::ThemePrincipal &t = *g.theme;
    const Point c = g.roue.moyeu;
    const bool famille = g.famille >= 0 && g.famille < g.n;
    dz::disque(pc, c.x, c.y, g.m.jauge / 2 + echelle(14), t.accent, OPA_AURA);
    dz::anneau(pc, c.x, c.y, g.m.jauge / 2, std::max(2, echelle(3)), t.texte, OPA_JAUGE);
    dz::disque(pc, c.x, c.y, g.m.moyeu / 2, t.carte, OPA_DISQUE);
    const char *icone = famille ? g.entrees[g.famille].icone : ICONE_MOYEU;
    icone_centree(pc, g.icone_moyeu, icone, c.x, c.y - g.m.moyeu * 14 / 100, t.accent);
    char mot[96];
    ajuster_mot(mot, sizeof mot, famille ? g.entrees[g.famille].texte : traduire(MOT_MOYEU), g.mot, g.m.jauge);
    dz::texte(pc, g.mot, mot, c.x - g.m.jauge / 2,
              c.y + g.m.moyeu * 14 / 100 - dz::hauteur_ligne(g.mot) / 2 + g.m.moyeu * 8 / 100, g.m.jauge,
              LV_TEXT_ALIGN_CENTER, t.texte);
}

void dessiner(lv_event_t *evenement) {
    lv_layer_t *couche = lv_event_get_layer(evenement);
    const dz::Pinceau pc = {couche, g.theme};
    const visuel::ThemePrincipal &t = *g.theme;
    // Pas de voile sur l'écran principal (retiré le 09/11/2026, demande d'Axel : il mettait trop de temps à s'afficher) : seule la roue est
    // dessinée, sur l'accueil tel qu'il est. Tout ce qui est dessiné ici doit tenir dans `zone_de_la_roue` (l'objet coupe le reste).
    bande(pc, g.roue.premier, g.m.rayon_1, g.m.diametre_1, t.texte);
    for (int i = 0; i < g.roue.premier.n; ++i) {
        dessiner_bouton(pc, g.roue.premier.bouton[i], g.entrees[i].icone, g.sous == i && !g.sous_second, g.famille == i,
                        g.est_famille[i]);
        // Le mot de la famille dépliée n'est pas redit : le moyeu l'affiche déjà, et son mot (posé dans l'axe du bouton, vers l'extérieur)
        // passerait sous les disques du second anneau (« Profil » perdait son « l » sous « Météo »).
        if (g.famille != i) {
            dessiner_mot(pc, g.roue.premier.bouton[i].mot, g.entrees[i].texte);
        }
    }
    dessiner_moyeu(pc);
    if (g.famille >= 0 && g.famille < g.n) {
        bande(pc, g.second, g.m.rayon_2, g.m.diametre_2, t.accent);
        for (int j = 0; j < g.second.n; ++j) {
            const ChoixRoue &choix = g.choix[g.famille][j];
            dessiner_bouton(pc, g.second.bouton[j], choix.icone, g.sous == j && g.sous_second, choix.actif, false);
            dessiner_mot(pc, g.second.bouton[j].mot, choix.texte);
        }
    }
}

// ---- la zone de la roue -------------------------------------------------------------------------------------------------------------

// Le rectangle qui contient tout ce que `dessiner` peut poser, mêmes nombres que lui (un peu plus large, jamais moins : ce qui en
// sortirait serait coupé). Le banc le vérifie : les rendus de la scène `roue` sont identiques au pixel à ceux de l'objet plein écran.
struct Boite {
    int x1 = 0;
    int y1 = 0;
    int x2 = -1;
    int y2 = -1;
    bool vide = true;

    void ajouter(int ax1, int ay1, int ax2, int ay2) {
        if (vide) {
            x1 = ax1;
            y1 = ay1;
            x2 = ax2;
            y2 = ay2;
            vide = false;
            return;
        }
        x1 = std::min(x1, ax1);
        y1 = std::min(y1, ay1);
        x2 = std::max(x2, ax2);
        y2 = std::max(y2, ay2);
    }
    void disque(int cx, int cy, int r) { ajouter(cx - r, cy - r, cx + r, cy + r); }
};

// L'arc de la bande : ses deux bouts arrondis (un disque de la demi-épaisseur, au milieu de la bande) et, à l'extérieur, chaque point
// cardinal que l'arc traverse (LVGL : 0 à droite, 90 en bas, sens horaire du début à la fin). Ce rectangle est exact.
void zone_bande(Boite &b, const roue::Anneau &a, int rayon, int diametre) {
    if (a.n <= 0) {
        return;
    }
    const ArcDeBande arc = arc_de_bande(a, rayon, diametre);
    const double milieu = arc.exterieur - arc.epaisseur / 2.0;
    const Point c = g.roue.moyeu;
    for (const int angle : {arc.debut, arc.fin}) {
        const double t = angle * PI / 180.0;
        b.disque(c.x + static_cast<int>(std::lround(milieu * std::cos(t))),
                 c.y + static_cast<int>(std::lround(milieu * std::sin(t))), arc.epaisseur / 2 + 1);
    }
    const int course = dans_le_tour(arc.fin - arc.debut);
    for (int cardinal = 0; cardinal < 360; cardinal += 90) {
        if (dans_le_tour(cardinal - arc.debut) <= course) {
            const double t = cardinal * PI / 180.0;
            const int x = c.x + static_cast<int>(std::lround(arc.exterieur * std::cos(t)));
            const int y = c.y + static_cast<int>(std::lround(arc.exterieur * std::sin(t)));
            b.ajouter(x, y, x, y);
        }
    }
}

// Les boutons d'un anneau : le disque (et son contour, posé dedans), le point de famille, le mot.
void zone_boutons(Boite &b, const roue::Anneau &a) {
    const int rayon = g.m.diametre_1 / 2; // comme dessiner_bouton
    const int h = dz::hauteur_ligne(g.mot);
    for (int i = 0; i < a.n; ++i) {
        const roue::Bouton &bt = a.bouton[i];
        b.disque(bt.x, bt.y, rayon);
        b.disque(bt.x + rayon * 7 / 10, bt.y - rayon * 7 / 10, echelle(POINT_FAMILLE));
        if (bt.mot.largeur > 0) {
            const int x = bt.mot.x - bt.mot.largeur / 2;
            const int y = bt.mot.y - h / 2;
            b.ajouter(x, y, x + bt.mot.largeur - 1, y + h - 1);
        }
    }
}

lv_area_t zone_de_la_roue() {
    Boite b;
    zone_bande(b, g.roue.premier, g.m.rayon_1, g.m.diametre_1);
    zone_boutons(b, g.roue.premier);
    // Le moyeu : son halo, le plus grand de ses disques, et son mot (comme dessiner_moyeu).
    const Point c = g.roue.moyeu;
    b.disque(c.x, c.y, g.m.jauge / 2 + echelle(14));
    const int y_mot = c.y + g.m.moyeu * 14 / 100 - dz::hauteur_ligne(g.mot) / 2 + g.m.moyeu * 8 / 100;
    b.ajouter(c.x - g.m.jauge / 2, y_mot, c.x - g.m.jauge / 2 + g.m.jauge - 1, y_mot + dz::hauteur_ligne(g.mot) - 1);
    if (g.famille >= 0 && g.famille < g.n) {
        zone_bande(b, g.second, g.m.rayon_2, g.m.diametre_2);
        zone_boutons(b, g.second);
    }
    // Avec sa marge, ramenée à l'écran (ce qui dépasse ne se voit pas).
    const int largeur = static_cast<int>(g.largeur); // int32_t est un `long` sur le RISC-V
    const int hauteur = static_cast<int>(g.hauteur);
    const lv_area_t z = {std::max(0, b.x1 - MARGE_ZONE), std::max(0, b.y1 - MARGE_ZONE),
                         std::min(largeur - 1, b.x2 + MARGE_ZONE), std::min(hauteur - 1, b.y2 + MARGE_ZONE)};
    if (b.vide || z.x1 > z.x2 || z.y1 > z.y2) {
        return {0, 0, g.largeur - 1, g.hauteur - 1}; // ne peut pas arriver ; l'écran entier plutôt qu'une roue coupée
    }
    return z;
}

// ---- ouverture, dépliement, fermeture -------------------------------------------------------------------------------------------

// Donne à l'objet la zone de la roue (elle change quand une famille se déplie ou se replie, ou que la roue est reposée) : quand elle
// change, LVGL invalide l'ancienne zone et la nouvelle.
void placer() {
    const lv_area_t z = zone_de_la_roue();
    lv_obj_set_pos(g.voile, z.x1, z.y1);
    lv_obj_set_size(g.voile, lv_area_get_width(&z), lv_area_get_height(&z));
    lv_obj_update_layout(g.voile);
}

void invalider() {
    if (g.voile != nullptr) {
        placer();
        lv_obj_invalidate(g.voile);
    }
}

void fermer_sans_action() {
    if (!g.ouverte) {
        return;
    }
    g.ouverte = false;
    if (g.minuteur != nullptr) {
        lv_timer_delete(g.minuteur);
        g.minuteur = nullptr;
    }
    if (g.voile != nullptr) {
        // Le cacher invalide sa zone, celle de la roue (l'accueil y est repeint) ; plus rien ne le dessine ni ne le touche avant sa
        // suppression, qui n'invalide plus rien (LVGL n'invalide pas un objet caché).
        lv_obj_add_flag(g.voile, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g.voile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_delete_async(g.voile); // on peut être dans son propre rappel (le toucher qui ferme)
        g.voile = nullptr;
    }
    g.famille = -1;
    g.second = ANNEAU_VIDE;
    g.appuye = false;
    g.long_fait = false;
    g.sous = -1;
}

void replier() {
    g.famille = -1;
    g.second = ANNEAU_VIDE;
}

void deplier(int rang) {
    const EntreeRoue &e = g.entrees[rang];
    int largeurs[roue::CHOIX_MAX] = {};
    const int nb = std::min(e.nb_choix, roue::CHOIX_MAX);
    for (int j = 0; j < nb; ++j) {
        largeurs[j] = largeur_du_mot(g.choix[rang][j].texte);
    }
    const roue::Depliage d = roue::deplier_famille(g.m, g.roue, g.demande, rang, nb, largeurs);
    g.roue = d.roue; // la même, ou reposée pour que le second anneau tienne
    g.second = d.second;
    g.famille = rang;
}

// Ce que la roue fait d'une décision. Les actions partent APRÈS la fermeture : elles peuvent charger un autre écran.
void appliquer(const roue::Decision &d) {
    switch (d.geste) {
    case roue::Geste::Rien:
        break;
    case roue::Geste::Fermer:
        fermer_sans_action();
        break;
    case roue::Geste::Replier:
        replier();
        break;
    case roue::Geste::Deplier:
        if (d.rang >= 0 && d.rang < g.n) {
            deplier(d.rang);
        }
        break;
    case roue::Geste::Choisir: {
        void (*action)() = d.rang >= 0 && d.rang < g.n ? g.entrees[d.rang].action : nullptr;
        fermer_sans_action();
        if (action != nullptr) {
            action();
        }
        break;
    }
    case roue::Geste::ChoisirChoix: {
        void (*action)() = d.famille >= 0 && d.famille < g.n && d.rang >= 0 && d.rang < roue::CHOIX_MAX
                               ? g.choix[d.famille][d.rang].action
                               : nullptr;
        fermer_sans_action();
        if (action != nullptr) {
            action();
        }
        break;
    }
    }
}

roue::Decision decider_ici(Point appui, Point relache, bool annule) {
    return roue::decider(g.m, g.roue, g.famille, g.second, g.est_famille, appui, relache, g.seuil, annule);
}

// L'élément sous le doigt (pour le retour visuel) : le second anneau d'abord quand il est déplié, comme `decider`.
void chercher_sous(Point p) {
    int rang = -1;
    bool second = false;
    if (g.famille >= 0 && g.famille < g.n) {
        rang = roue::bouton_sous(g.m, g.second, g.m.diametre_2, p);
        second = rang >= 0;
    }
    if (rang < 0) {
        rang = roue::bouton_sous(g.m, g.roue.premier, g.m.diametre_1, p);
    }
    if (rang != g.sous || second != g.sous_second) {
        g.sous = rang;
        g.sous_second = second;
        invalider();
    }
}

void relancer_minuteur() {
    if (g.minuteur != nullptr) {
        lv_timer_reset(g.minuteur);
    }
}

void sur_inactivite(lv_timer_t *) {
    g.minuteur = nullptr; // LVGL le supprime lui-même (une seule exécution)
    fermer_sans_action();
}

Point point_du_doigt() {
    lv_point_t p = {0, 0};
    lv_indev_t *indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_get_point(indev, &p);
    }
    return Point{static_cast<int>(p.x), static_cast<int>(p.y)};
}

void sur_voile(lv_event_t *evenement) {
    switch (lv_event_get_code(evenement)) {
    case LV_EVENT_DRAW_MAIN:
        if (g.ouverte && g.theme != nullptr) {
            dessiner(evenement);
        }
        break;
    case LV_EVENT_PRESSED:
        g.appui = g.courant = point_du_doigt();
        g.appuye = true;
        g.long_fait = false;
        relancer_minuteur();
        chercher_sous(g.appui);
        break;
    case LV_EVENT_PRESSING:
        g.courant = point_du_doigt();
        if (g.appuye) {
            if (roue::a_glisse(g.appui, g.courant, g.seuil) || g.long_fait) {
                if (g.sous != -1) { // le doigt glisse ou s'attarde : plus de retour visuel, ce ne sera pas un toucher
                    g.sous = -1;
                    invalider();
                }
            } else {
                chercher_sous(g.courant);
            }
        }
        break;
    case LV_EVENT_LONG_PRESSED:
        g.long_fait = true; // un appui long sur la roue ne fait rien (ADR-0036 du dépôt Tab5)
        break;
    case LV_EVENT_RELEASED: {
        g.courant = point_du_doigt();
        const bool etait_appuye = g.appuye;
        const bool annule = g.long_fait;
        g.appuye = false;
        g.long_fait = false;
        g.sous = -1;
        invalider();
        if (etait_appuye) {
            appliquer(decider_ici(g.appui, g.courant, annule));
        }
        break;
    }
    case LV_EVENT_PRESS_LOST:
        g.appuye = false;
        g.long_fait = false;
        g.sous = -1;
        invalider();
        break;
    default:
        break;
    }
}

// ---- polices ----------------------------------------------------------------------------------------------------------------------

bool creer_polices(const ContexteRoue &ctx) {
    if (g.mot.f != nullptr && g.polices_pour == g.largeur && g.ttf == ctx.ttf && g.mdi == ctx.mdi) {
        return true;
    }
    // Les polices d'une ouverture précédente (autre taille d'écran) ne sont pas détruites : ce cas n'arrive pas sur un appareil, dont
    // l'écran ne change pas de taille.
    g.mot = {lv_tiny_ttf_create_data(ctx.ttf, ctx.ttf_taille, echelle(POLICE_MOT)), echelle(POLICE_MOT)};
    g.icone = {lv_tiny_ttf_create_data(ctx.mdi, ctx.mdi_taille, echelle(POLICE_ICONE)), echelle(POLICE_ICONE)};
    g.icone_moyeu = {lv_tiny_ttf_create_data(ctx.mdi, ctx.mdi_taille, echelle(POLICE_ICONE_MOYEU)),
                     echelle(POLICE_ICONE_MOYEU)};
    g.polices_pour = g.largeur;
    g.ttf = ctx.ttf;
    g.mdi = ctx.mdi;
    return g.mot.f != nullptr && g.icone.f != nullptr && g.icone_moyeu.f != nullptr;
}

} // namespace

// ---- l'interface publique ------------------------------------------------------------------------------------------------------

bool ui_roue_ouvrir(const ContexteRoue &ctx, const EntreeRoue *entrees, int n, int32_t x, int32_t y) {
    if (g.ouverte || entrees == nullptr || n < 1 || n > ENTREES_ROUE_MAX || ctx.couche == nullptr ||
        ctx.ttf == nullptr || ctx.mdi == nullptr || ctx.theme == nullptr || ctx.largeur <= 0 || ctx.hauteur <= 0) {
        return false;
    }
    g.largeur = ctx.largeur;
    g.hauteur = ctx.hauteur;
    g.ecran = roue::ecran_de(ctx.largeur, ctx.hauteur);
    g.m = roue::mesures(g.ecran);
    g.seuil = roue::glissement(g.ecran);
    g.theme = ctx.theme;
    if (!creer_polices(ctx)) {
        return false;
    }
    // Les entrées sont copiées : l'appelant peut reconstruire sa table, la roue garde celle de son ouverture.
    g.n = n;
    roue::Forme formes[ENTREES_ROUE_MAX] = {};
    int mot_choix = 0;
    for (int i = 0; i < n; ++i) {
        g.entrees[i] = entrees[i];
        const int nb = std::clamp(entrees[i].nb_choix, 0, roue::CHOIX_MAX);
        g.entrees[i].nb_choix = entrees[i].choix != nullptr ? nb : 0;
        g.est_famille[i] = g.entrees[i].nb_choix > 0;
        for (int j = 0; j < g.entrees[i].nb_choix; ++j) {
            g.choix[i][j] = entrees[i].choix[j];
            mot_choix = std::max(mot_choix, largeur_du_mot(g.choix[i][j].texte));
        }
        formes[i].mot = largeur_du_mot(entrees[i].texte);
        formes[i].choix = g.entrees[i].nb_choix;
    }
    g.demande = roue::demande(formes, n, mot_choix, roue::EXIGENCE_A_L_OUVERTURE);
    g.roue = roue::ouvrir(g.m, Point{static_cast<int>(x), static_cast<int>(y)}, g.demande);
    g.famille = -1;
    g.second = ANNEAU_VIDE;
    g.appuye = false;
    g.long_fait = false;
    g.sous = -1;

    // L'objet naît à une place et une taille par défaut avant d'être posé : sans cette coupure LVGL invaliderait aussi ces zones-là.
    // Il est créé, posé et mis en page sans rien invalider ; puis seule sa zone, celle de la roue, est invalidée.
    lv_display_t *affichage = lv_obj_get_display(ctx.couche);
    lv_display_enable_invalidation(affichage, false);
    g.voile = lv_obj_create(ctx.couche);
    lv_obj_remove_style_all(g.voile);
    lv_obj_add_flag(g.voile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g.voile, LV_OBJ_FLAG_SCROLLABLE);
    // Un toucher n'importe où sur l'écran arrive à la roue, comme quand l'objet était plein écran : la marge part du bord de l'objet,
    // qui est dans l'écran ; la plus grande dimension de l'écran couvre donc l'écran entier.
    lv_obj_set_ext_click_area(g.voile, std::max(ctx.largeur, ctx.hauteur));
    lv_obj_add_event_cb(g.voile, sur_voile, LV_EVENT_ALL, nullptr);
    placer();
    lv_display_enable_invalidation(affichage, true);
    g.minuteur = lv_timer_create(sur_inactivite, static_cast<uint32_t>(roue::INACTIVITE_MS), nullptr);
    lv_timer_set_repeat_count(g.minuteur, 1);
    g.ouverte = true;
    lv_obj_invalidate(g.voile);
    return true;
}

void ui_roue_fermer() { fermer_sans_action(); }

bool ui_roue_ouverte() { return g.ouverte; }

void ui_roue_changer_theme(const visuel::ThemePrincipal &theme) {
    if (!g.ouverte) {
        return;
    }
    g.theme = &theme;
    invalider();
}

bool ui_roue_simuler_toucher(int32_t x, int32_t y) {
    if (!g.ouverte) {
        return false;
    }
    const Point p{static_cast<int>(x), static_cast<int>(y)};
    appliquer(decider_ici(p, p, false));
    invalider();
    return g.ouverte;
}

bool ui_roue_centre_bouton(int rang, int32_t *x, int32_t *y) {
    if (!g.ouverte || rang < 0 || rang >= g.roue.premier.n || x == nullptr || y == nullptr) {
        return false;
    }
    *x = g.roue.premier.bouton[rang].x;
    *y = g.roue.premier.bouton[rang].y;
    return true;
}

bool ui_roue_position(PositionRoue *position) {
    if (!g.ouverte || position == nullptr) {
        return false;
    }
    const int dx = g.roue.moyeu.x - g.roue.appui.x;
    const int dy = g.roue.moyeu.y - g.roue.appui.y;
    position->x = g.roue.moyeu.x;
    position->y = g.roue.moyeu.y;
    position->ecart =
        static_cast<int32_t>(std::lround(std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy)));
    position->tient = g.roue.tient;
    position->dessous = g.roue.dessous;
    return true;
}

bool ui_roue_centre_choix(int rang, int32_t *x, int32_t *y) {
    if (!g.ouverte || g.famille < 0 || rang < 0 || rang >= g.second.n || x == nullptr || y == nullptr) {
        return false;
    }
    *x = g.second.bouton[rang].x;
    *y = g.second.bouton[rang].y;
    return true;
}

CoutRoue ui_roue_cout() {
    const int polices =
        (g.mot.f != nullptr ? 1 : 0) + (g.icone.f != nullptr ? 1 : 0) + (g.icone_moyeu.f != nullptr ? 1 : 0);
    return {g.voile != nullptr ? 1 : 0, g.minuteur != nullptr ? 1 : 0, polices, sizeof(Etat)};
}

} // namespace ecran
