// En-tête INTERNE de l'écran principal (ui_principal*.cpp) : l'état partagé et ce que les fichiers de zone se prêtent. Rien
// d'autre ne l'inclut. Le code a été réparti par zone sans changer une ligne (lot L18) ; l'API est dans ui_principal.h.
#pragma once

#include "ui_principal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "ecran/accueil.hpp"
#include "ecran/accueil_bande_pluie.hpp"
#include "ecran/accueil_cartes.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/i18n.hpp"
#include "ecran/meteo_wmo.hpp"
#include "ecran/roue_commande.hpp"
#include "ecran/soleil.hpp"
#include "ecran/visuel_astres.hpp"
#include "ecran/visuel_dates.hpp"
#include "ecran/visuel_echelles.hpp"
#include "textes_principal.h"
#include "ui_meteo_dessin.h"
#include "ui_meteo_icones.h"
#include "ui_ressources.h"

namespace ecran {
namespace principal {

using namespace visuel;
namespace dz = dessin;
using dz::Police;

enum Zone { Z_BARRE, Z_HORLOGE, Z_MAINTENANT, Z_HEURES, Z_BANDEAU, Z_SEMAINE, Z_NOMBRE };

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

// Où et comment la pastille de l'alerte officielle est dessinée (calculé quand elle change : placer_pastille_officielle).
// Les décalages `x_*` sont relatifs au bord gauche de la pastille ; `w_texte` 0 : pas de texte (place trop juste) ; `d_autres` 0 : pas de « +N ».
struct PlaceOfficielle {
    bool ok = false;
    int x = 0, y = 0, w = 0, h = 0; // en coordonnées d'écran
    int x_texte = 0, w_texte = 0;
    int x_autres = 0, d_autres = 0;
    const dz::Police *police = nullptr;
    char texte[vigie::BANDEAU_TEXTE_CAP + 4] = {};
    char autres[8] = {};
    // Le bandeau d'alerte AGRANDI (orange ou rouge, ADR-0015 règle 1) : une grande carte au lieu d'une puce. Le titre de l'émetteur est
    // alors posé sur deux lignes au plus (`texte`), la fenêtre « 14–22 h » sur la sienne (`fenetre`).
    bool grande = false;
    char fenetre[24] = {};
};

// Un voile sous une courbe, cuit en image (voir voile_entre) : l'image, l'empreinte de ce qu'elle montre, sa place à l'écran.
struct VoileCuit {
    lv_draw_buf_t *image = nullptr;
    std::uint32_t cle = 0;
    int x = 0, y = 0;
    std::uint32_t teinte = 0; // image A8 (une seule couleur) : la couleur à lui donner
};

enum : std::uint8_t { VOILE_HEURES, VOILE_JOURS, VOILE_NOMBRE };

struct Vue {
    const ThemePrincipal *theme = &kThemeCiel;
    int largeur = 0, hauteur = 0;
    bool portrait = false;
    float echelle = 1.0F;
    Police heure, temperature, titre, corps, petit, mini;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *zone[Z_NOMBRE] = {};
    lv_obj_t *pastille =
        nullptr; // pastille de l'alerte officielle : se dessine et reçoit l'appui (cachée sans alerte en vigueur)
    lv_obj_t *anneau = nullptr;
    PlaceOfficielle officielle;
    Rect rect[Z_NOMBRE];
    EntreesPrincipal e = {};
    char date[64] = {};
    char date_attente[64] = {};
    std::uint32_t empreinte[Z_NOMBRE] = {};
    std::uint32_t empreinte_ciel = 0;
    lv_draw_buf_t *ciel_image = nullptr; // le ciel tramé (voir appliquer_ciel)
    // Le verre des grandes cartes (heures, cinq jours ou pluie) est cuit dans l'image du ciel (voir cuire_verre) : ces rectangles
    // sont ceux déjà peints, que carte_de_verre ne redessine plus. `nb_verre` 0 : rien de cuit, le verre se dessine comme avant.
    Rect verre[2];
    int nb_verre = 0;
    std::uint32_t cle_verre = 0;
    std::uint32_t ciel_haut = 0, ciel_bas = 0; // les couleurs avec lesquelles l'image du ciel a été remplie
    VoileCuit voile[VOILE_NOMBRE];             // les voiles des heures et des cinq jours, cuits (voir voile_entre)
    bool est_accueil = false;                  // la composition de l'accueil ; faux : celle de l'horloge en vedette
    int extra_h = 0; // hauteur de référence gagnée au-delà de 720 (la 7B) : elle va à la carte des jours
    ThemePrincipal theme_gris = kThemeCiel; // le thème de la météo périmée (accueil::theme_grise)
    accueil::EpisodePluie pluie;            // la pluie à annoncer dans le bandeau (recalculée à chaque mise à jour)
    // Le mode automatique (ADR-0023) : ce qui a été décidé, la composition de la zone du bas qui en découle et ses rectangles.
    accueil::Presentation mode;
    accueil::Composition composition = accueil::Composition::Normale;
    std::uint32_t cle_disposition =
        0;                           // composition + alerte + tuile : quand elle change, les rectangles sont recalculés
    accueil::CartePluie carte_pluie; // le contenu de la carte pluie (ou de la tuile de pluie) ; `presente` faux : rien
    accueil::BandePluie
        bande; // la bande de pluie de la ligne centrale (ADR-0027) : une barre par quart d'heure ; `presente` faux : rien
    Rect rect_alerte;                 // la grande carte d'alerte (composition d'alerte) ; vide sinon
    Rect rect_tuile;                  // la tuile de pluie à côté d'elle ; vide sinon
    bool bas_en_bande = false;        // la zone du bas est la bande réduite des cinq jours
    bool alerte_dans_bandeau = false; // une alerte (officielle, ou déduite de la météo) occupe le bandeau
    bool tuile_dit_quelque_chose =
        false; // la composition d'alerte a de quoi remplir la tuile (pluie, ou données périmées)
    int diametre_anneau = 88;
    int objets = 0;
    void (*appui_long)(int32_t x, int32_t y) = nullptr;
    void (*ouvrir_alertes)() = nullptr;
    bool appui_long_fait = false;
    lv_point_t appui_depart = {0, 0}; // où le doigt s'est posé : un doigt qui a glissé depuis n'ouvre pas la roue
    int seuil_glissement = 0;         // en pixels de l'écran réel (roue_commande.hpp : glissement)
};

constexpr const char *TIRET = "\xE2\x80\x94"; // —

// Texte qui tient dans `largeur` : la première police de la liste qui convient, sinon la dernière, tronquée avec « … ».
struct Ajuste {
    const Police *police;
    char texte[vigie::BANDEAU_TEXTE_CAP +
               4]; // le titre d'une alerte officielle est long (jusqu'à 191 octets), la ville bien moins
};

struct Empreinte {
    std::uint32_t h = 2166136261U;
    void u(std::uint32_t v) { h = (h ^ v) * 16777619U; }
    void i(std::int64_t v) {
        u(static_cast<std::uint32_t>(v));
        u(static_cast<std::uint32_t>(v >> 32));
    }
    void s(const char *t) {
        for (; t != nullptr && *t != '\0'; ++t) {
            u(static_cast<unsigned char>(*t));
        }
        u(0xFF);
    }
};

struct Ctx {
    dz::Pinceau pc;
    int x, y, w, h; // la zone, en coordonnées d'écran
};

constexpr int COLONNES_HEURES = 12;

constexpr int COLONNES_JOURS = 5;

constexpr const char *POINT_MEDIAN = " \xC2\xB7 "; // « · » entre deux morceaux d'une phrase

// Un dégradé vertical GLOBAL : couleur et opacité (0 à 255) valent `c0`, `a0` à l'ordonnée `y0` et `c1`, `a1` à `y1`, linéairement.
struct DegradeGlobal {
    std::uint32_t c0, c1;
    int a0, a1;
    int y0, y1;
    void en(int y, std::uint32_t *rgb, std::uint8_t *opa) const {
        int pm = y1 != y0 ? (y - y0) * 1000 / (y1 - y0) : 0;
        pm = std::max(0, std::min(1000, pm));
        *rgb = en_hex(melanger(depuis_hex(c0), depuis_hex(c1), pm));
        *opa = static_cast<std::uint8_t>(a0 + (a1 - a0) * pm / 1000);
    }
};

// ui_principal.cpp
extern bool s_verre_differe;
extern Vue g;
extern int images_voile;
inline int px(int reference) {
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(reference) * g.echelle)));
}

// ui_principal_outils.cpp
void fmt_temp(char *sortie, std::size_t n, int dix);
void fmt_dixieme(char *sortie, std::size_t n, int dix);
ecran::heure::Fuseau fuseau();
ecran::heure::HeureLocale heure_du(std::int64_t instant_s);
void fmt_hhmm(char *sortie, std::size_t n, std::int64_t epoch_s);
void libelle_meteo(char *sortie, std::size_t n, int code, bool jour);
Ajuste ajuster(std::initializer_list<const Police *> polices, const char *s, int largeur);
void texte_ajuste(const dz::Pinceau &pc, std::initializer_list<const Police *> polices, const char *s, int x,
                  int y_centre, int w, lv_text_align_t align, std::uint32_t rgb);

// ui_principal_donnees.cpp
MomentJour moment_du_jour();
Ambiance ambiance_du_moment();
Alerte alerte_du_jour();
const char *texte_alerte(Alerte a);
Piece piece_alerte(Alerte a);
int premiere_heure();
std::int64_t numero_du_jour_prevu(const JourPrevu &j);
int premier_jour();

// ui_principal_disposition.cpp
void appliquer_disposition();
void fond_de_carte(const Ctx &c, int x, int y, int w, int h, int rayon);

// ui_principal_barre.cpp
int largeur_etat_exemple();
void positionner_pastille_officielle();
void dessiner_barre(const Ctx &c);

// ui_principal_horloge.cpp
void dessiner_horloge(const Ctx &c);
void dessiner_maintenant(const Ctx &c);
void dessiner_semaine(const Ctx &c);

// ui_principal_voiles.cpp
const ThemePrincipal &theme_meteo();
bool en_francais();
void texte_base(const dz::Pinceau &pc, const Police &p, const char *s, int x, int base, int w, lv_text_align_t align,
                std::uint32_t rgb);
void carte_de_verre(const Ctx &c, int rayon);
void point_de_courbe(const dz::Pinceau &pc, const ThemePrincipal &t, int x, int y, int rayon_halo, int rayon);
void tracer_courbe(const dz::Pinceau &pc, const accueil::CourbeMonotone &courbe, const std::uint32_t *teintes,
                   int epaisseur, int pas);
void voile_entre(const dz::Pinceau &pc, const accueil::CourbeMonotone &haut, const accueil::CourbeMonotone &bas,
                 int x_debut, int x_fin, int pas, const DegradeGlobal &d, VoileCuit &v);

// ui_principal_carte_heure.cpp
void dessiner_barre_accueil(const Ctx &c);
void dessiner_horloge_accueil(const Ctx &c);
void dessiner_maintenant_accueil(const Ctx &c);

// ui_principal_heures.cpp
void dessiner_heures_accueil(const Ctx &c);

// ui_principal_bandeau.cpp
void fond_de_puce(const dz::Pinceau &pc, int x, int y, int w, int h, std::uint32_t rgb, std::uint8_t opa_fond,
                  std::uint8_t opa_bord);
void icone_de_puce(const dz::Pinceau &pc, Piece piece, int x, int y, int h, std::uint32_t rgb);
void puce(const dz::Pinceau &pc, const ThemePrincipal &t, const Rect &r, std::uint32_t rgb, std::uint8_t opa_fond,
          std::uint8_t opa_bord, Piece icone, const char *texte);
void texte_de_pluie(char *sortie, std::size_t n, bool avec_cumul);
void dessiner_bandeau(const Ctx &c);
PlaceOfficielle placer_puce_officielle();

// ui_principal_mode_auto.cpp
int dessiner_pastille_mode(const Ctx &c, int x, int largeur_max);
const char *texte_force_pluie(accueil::NiveauPluie n);
void dessiner_carte_pluie(const Ctx &c);
void dessiner_tuile(const Ctx &c);
void dessiner_bande_jours(const Ctx &c);

// ui_principal_jours.cpp
void dessiner_jours(const Ctx &c);

// ui_principal_dessin.cpp
void sur_dessin(lv_event_t *evenement);
void sur_dessin_pastille(lv_event_t *evenement);
void sur_pastille(lv_event_t *evenement);

// ui_principal_toucher.cpp
void sur_toucher(lv_event_t *evenement);
lv_obj_t *creer_zone(lv_obj_t *parent);

// ui_principal_ciel.cpp
void cuire_verre();
void appliquer_ciel();

} // namespace principal
} // namespace ecran
