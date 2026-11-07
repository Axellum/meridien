#include "ui_principal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/meteo_wmo.hpp"
#include "ecran/visuel_astres.hpp"
#include "ecran/visuel_dates.hpp"
#include "ecran/visuel_echelles.hpp"
#include "textes_principal.h"
#include "ui_meteo_dessin.h"
#include "ui_meteo_icones.h"

// Écran principal. Chaque ZONE est un seul objet LVGL qui dessine tout son contenu dans l'événement LV_EVENT_DRAW_MAIN : sept
// objets en tout (quatre autres pour le fond, le bouton Réglages, l'anneau tactile), au lieu d'une centaine d'étiquettes. Un
// objet LVGL coûte de la RAM interne (le point étroit de cette carte), une tâche de dessin non.
//
//   paysage (1280 x 720)                      portrait (720 x 1280)
//   +-- barre : ville · alerte · état · ⚙ -+  +-- barre --------------+
//   | horloge + date  |  maintenant        |  | horloge + date        |
//   +-----------------+--------------------+  | maintenant            |
//   | vent · pluie · humidité · pression · UV · soleil · lune           | grandeurs en grille
//   | heures : icône, température, pluie, selon la largeur              | heures
//   | semaine : 7 jours, barre min-max sur une échelle commune          | semaine en lignes
//
// Une zone n'est redessinée que si ses données ont changé (empreinte par zone) : l'horloge change à la minute, le reste à la
// mise à jour de la météo. Ce qui est inconnu est caché ou remplacé par « — », jamais inventé.
//
// Couleurs, opacités, dispositions : visuel_themes.hpp (aucune couleur écrite ici). Textes : visuel_textes.hpp (sept langues).
// Tout ce qui se calcule sans LVGL est dans le noyau pur (visuel_*.hpp, testé sur le PC).

namespace ecran {

namespace {

using namespace visuel;
namespace dz = dessin;
using dz::Police;

enum Zone { Z_BARRE, Z_HORLOGE, Z_MAINTENANT, Z_METRIQUES, Z_HEURES, Z_SEMAINE, Z_NOMBRE };

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
};

struct Vue {
    const ThemePrincipal *theme = &kThemeCiel;
    int largeur = 0, hauteur = 0;
    bool portrait = false;
    float echelle = 1.0F;
    Police heure, temperature, titre, corps, petit, mini, nano;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *zone[Z_NOMBRE] = {};
    lv_obj_t *bouton = nullptr;
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
    bool a_heures = false;
    int diametre_anneau = 88;
    int objets = 0;
    void (*appui_long)() = nullptr;
    void (*ouvrir_reglages)() = nullptr;
    void (*ouvrir_alertes)() = nullptr;
    bool appui_long_fait = false;
};

Vue g;

int px(int reference) { return std::max(1, static_cast<int>(std::lround(static_cast<float>(reference) * g.echelle))); }

// ---- petits formats ----------------------------------------------------------------------------------------------------

constexpr const char *DEGRE = "\xC2\xB0";     // °
constexpr const char *TIRET = "\xE2\x80\x94"; // —

char virgule_decimale() { return i18n::langue_courante() == i18n::Langue::En ? '.' : ','; }

// Température en dixièmes -> « 12° », « −3° » ; « — » si inconnue.
void fmt_temp(char *sortie, std::size_t n, int dix) {
    char nombre[16];
    if (!connu(dix) || formater_temperature(dix, nombre, sizeof nombre) == 0) {
        std::snprintf(sortie, n, "%s", TIRET);
        return;
    }
    std::snprintf(sortie, n, "%s%s", nombre, DEGRE);
}

// Valeur en dixièmes -> « 0,4 » ou « 12 » (sans décimale quand elle est nulle).
void fmt_dixieme(char *sortie, std::size_t n, int dix) {
    if (dix % 10 == 0) {
        std::snprintf(sortie, n, "%d", dix / 10);
    } else {
        std::snprintf(sortie, n, "%d%c%d", dix / 10, virgule_decimale(), dix % 10 < 0 ? -(dix % 10) : dix % 10);
    }
}

int minute_locale(std::int64_t epoch_s, int decalage_s) {
    std::int64_t local = (epoch_s + decalage_s) % 3600;
    if (local < 0) {
        local += 3600;
    }
    return static_cast<int>(local / 60);
}

void fmt_hhmm(char *sortie, std::size_t n, std::int64_t epoch_s, int decalage_s) {
    if (epoch_s <= 0) {
        std::snprintf(sortie, n, "%s", TIRET);
        return;
    }
    std::snprintf(sortie, n, "%02d:%02d", heure_locale(epoch_s, decalage_s), minute_locale(epoch_s, decalage_s));
}

// Libellé météo de l'instant, dans la langue de l'écran (le code WMO est la source, jamais un libellé déjà traduit).
void libelle_meteo(char *sortie, std::size_t n, int code, bool jour) {
    const meteo::Condition d = meteo::decrire(code, jour);
    const std::string_view texte = i18n::tr(d.libelle);
    std::snprintf(sortie, n, "%.*s", static_cast<int>(texte.size()), texte.data());
}

// Texte qui tient dans `largeur` : la première police de la liste qui convient, sinon la dernière, tronquée avec « … ».
struct Ajuste {
    const Police *police;
    char texte[vigie::BANDEAU_TEXTE_CAP +
               4]; // le titre d'une alerte officielle est long (jusqu'à 191 octets), la ville bien moins
};

Ajuste ajuster(std::initializer_list<const Police *> polices, const char *s, int largeur) {
    Ajuste a = {};
    std::snprintf(a.texte, sizeof a.texte, "%s", s != nullptr ? s : "");
    for (const Police *p : polices) {
        a.police = p;
        if (dz::largeur_texte(*p, a.texte) <= largeur) {
            return a;
        }
    }
    // Aucune ne convient : on coupe la dernière à la limite d'un caractère UTF-8 complet, on pose « … ».
    std::size_t n = std::strlen(a.texte);
    while (n > 0 && dz::largeur_texte(*a.police, a.texte) + dz::largeur_texte(*a.police, "\xE2\x80\xA6") > largeur) {
        --n;
        while (n > 0 && (static_cast<unsigned char>(a.texte[n]) & 0xC0) == 0x80) {
            --n;
        }
        a.texte[n] = '\0';
    }
    if (n + 4 < sizeof a.texte) {
        std::memcpy(a.texte + n, "\xE2\x80\xA6", 4);
    }
    return a;
}

// Pose un texte ajusté, centré verticalement sur `y_centre`.
void texte_ajuste(const dz::Pinceau &pc, std::initializer_list<const Police *> polices, const char *s, int x,
                  int y_centre, int w, lv_text_align_t align, std::uint32_t rgb) {
    const Ajuste a = ajuster(polices, s, w);
    dz::texte(pc, *a.police, a.texte, x, y_centre - dz::hauteur_ligne(*a.police) / 2, w, align, rgb);
}

// ---- empreintes (pour ne redessiner que ce qui change) -------------------------------------------------------------------

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

// ---- données dérivées ---------------------------------------------------------------------------------------------------

int decalage() { return g.e.meteo.connue ? g.e.meteo.decalage_utc_s : g.e.decalage_secours_s; }

MomentJour moment_du_jour() {
    const VueMeteo &m = g.e.meteo;
    if (m.connue && m.lever_s > 0) {
        return arc_soleil(g.e.maintenant_s, m.lever_s, m.coucher_s, m.jour).moment;
    }
    if (m.connue) {
        return m.jour ? MomentJour::JOUR : MomentJour::NUIT;
    }
    if (g.e.maintenant_s > 0) { // pas de météo : l'heure locale décide
        const int h = heure_locale(g.e.maintenant_s, decalage());
        return (h >= 8 && h < 18) ? MomentJour::JOUR : MomentJour::NUIT;
    }
    return MomentJour::NUIT;
}

Ambiance ambiance_du_moment() {
    const VueMeteo &m = g.e.meteo;
    return m.connue ? ambiance_pour_code(m.code, m.jour) : Ambiance::COUVERT;
}

Alerte alerte_du_jour() {
    const VueMeteo &m = g.e.meteo;
    if (!m.connue) {
        return Alerte::AUCUNE;
    }
    EntreesAlerte a;
    a.min_dix = m.min_dix;
    a.max_dix = m.max_dix;
    a.code_wmo = m.code;
    a.proba_pluie_pct = m.nb_jours > 0 ? m.jours[0].proba_pct : m.proba_pluie_pct;
    a.pluie_jour_dix_mm = m.nb_jours > 0 ? m.jours[0].pluie_dix_mm : INCONNU_DIX;
    a.rafales_dix_kmh = m.rafales_dix_kmh;
    return alerte_principale(a);
}

const char *texte_alerte(Alerte a) {
    switch (a) {
    case Alerte::GEL:
        return texte_principal::t("Gel");
    case Alerte::CHALEUR:
        return texte_principal::t("Forte chaleur");
    case Alerte::VENT_FORT:
        return texte_principal::t("Vent fort");
    case Alerte::PLUIE_FORTE:
        return texte_principal::t("Pluie forte");
    case Alerte::ORAGE:
        return texte_principal::t("Orage");
    case Alerte::AUCUNE:
        break;
    }
    return "";
}

Piece piece_alerte(Alerte a) {
    switch (a) {
    case Alerte::GEL:
        return Piece::gel;
    case Alerte::CHALEUR:
        return Piece::chaleur;
    case Alerte::VENT_FORT:
        return Piece::vent;
    case Alerte::PLUIE_FORTE:
        return Piece::parapluie;
    default:
        return Piece::alerte;
    }
}

// L'heure (indice dans `heures`) à partir de laquelle on montre les colonnes : la première dont l'heure n'est pas écoulée.
int premiere_heure() {
    const VueMeteo &m = g.e.meteo;
    for (int i = 0; i < m.nb_heures; ++i) {
        if (m.heures[i].instant_s + 3600 > g.e.maintenant_s) {
            return i;
        }
    }
    return m.nb_heures;
}

// Le premier jour à montrer : on saute ceux qui sont finis (relevé gardé en cache depuis la veille).
int premier_jour() {
    const VueMeteo &m = g.e.meteo;
    for (int i = 0; i < m.nb_jours; ++i) {
        if (m.jours[i].date_s + 86400 > g.e.maintenant_s) {
            return i;
        }
    }
    return m.nb_jours;
}

// ---- mise en page -------------------------------------------------------------------------------------------------------
//
// Hauteurs de référence en paysage (720 px) : barre 48, haut 196, grandeurs 126, heure par heure 134, sept jours le reste (≈ 160).
// Sans prévision heure par heure, la place libérée va au haut et aux grandeurs ; les jours centrent leur contenu dans leur carte.

constexpr int H_TUILE = 126; // hauteur d'une tuile de grandeur, en pixels de référence
constexpr int H_HAUT = 196;  // horloge et météo du moment, en paysage

struct Cotes {
    int marge, ecart, barre_h;
};

Cotes cotes() { return {px(g.portrait ? 28 : 36), px(10), px(48)}; }

void calculer_disposition() {
    const Cotes c = cotes();
    const int utile = g.largeur - 2 * c.marge;
    const int y0 = px(10);
    g.rect[Z_BARRE] = {c.marge, y0, utile - px(64), c.barre_h}; // le bouton Réglages occupe la droite
    int y = y0 + c.barre_h + px(4);
    const int bas = g.hauteur - px(12);
    if (g.theme->disposition == Disposition::HORLOGE) {
        // Composition calme : l'heure géante au centre, la météo du moment dessous, les sept jours en bas ; ni grandeurs ni
        // heure par heure (leurs zones restent vides, donc cachées).
        // En paysage : 262 px pour l'heure + la date, 196 pour la carte du moment (sans quoi « Ressenti » est coupé), et les
        // 168 qui restent suffisent à la carte des sept jours (celle de la composition complète en fait 160).
        const int h_horloge = g.portrait ? px(330) : px(262);
        g.rect[Z_HORLOGE] = {c.marge, y, utile, h_horloge};
        y += h_horloge + c.ecart;
        const int h_maintenant = g.portrait ? px(230) : px(196);
        const int w_maintenant = g.portrait ? utile : std::min(utile, px(600));
        g.rect[Z_MAINTENANT] = {(g.largeur - w_maintenant) / 2, y, w_maintenant, h_maintenant};
        y += h_maintenant + c.ecart;
        g.rect[Z_METRIQUES] = {};
        g.rect[Z_HEURES] = {};
        g.rect[Z_SEMAINE] = {c.marge, y, utile, bas - y};
        return;
    }
    const int h_heures = g.a_heures ? px(134) : 0;
    if (!g.portrait) {
        const int h_metriques = px(H_TUILE) + (g.a_heures ? 0 : px(20));
        const int h_haut = px(H_HAUT) + (g.a_heures ? 0 : px(36));
        const int w_horloge = utile * 47 / 100;
        g.rect[Z_HORLOGE] = {c.marge, y, w_horloge, h_haut};
        g.rect[Z_MAINTENANT] = {c.marge + w_horloge + c.ecart, y, utile - w_horloge - c.ecart, h_haut};
        y += h_haut + c.ecart;
        g.rect[Z_METRIQUES] = {c.marge, y, utile, h_metriques};
        y += h_metriques + c.ecart;
    } else {
        const int h_horloge = px(200);
        const int h_maintenant = px(230);
        const int h_metriques = 2 * px(H_TUILE) + c.ecart;
        g.rect[Z_HORLOGE] = {c.marge, y, utile, h_horloge};
        y += h_horloge + c.ecart;
        g.rect[Z_MAINTENANT] = {c.marge, y, utile, h_maintenant};
        y += h_maintenant + c.ecart;
        g.rect[Z_METRIQUES] = {c.marge, y, utile, h_metriques};
        y += h_metriques + c.ecart;
    }
    g.rect[Z_HEURES] = {c.marge, y, utile, h_heures};
    if (g.a_heures) {
        y += h_heures + c.ecart;
    }
    g.rect[Z_SEMAINE] = {c.marge, y, utile, bas - y};
}

void positionner_pastille_officielle(); // plus bas, avec le dessin de la barre

void appliquer_disposition() {
    calculer_disposition();
    for (int z = 0; z < Z_NOMBRE; ++z) {
        if (g.zone[z] == nullptr) {
            continue;
        }
        const Rect &r = g.rect[z];
        const bool vide = r.w <= 0 || r.h <= 0;
        if (vide) {
            lv_obj_add_flag(g.zone[z], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(g.zone[z], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g.zone[z], r.x, r.y);
            lv_obj_set_size(g.zone[z], r.w, r.h);
        }
    }
    const Cotes c = cotes();
    const int cote = px(48);
    lv_obj_set_size(g.bouton, cote, cote);
    lv_obj_set_pos(g.bouton, g.largeur - c.marge - cote, px(10));
    positionner_pastille_officielle(); // la barre a pu changer de taille
}

// ---- zones : briques communes --------------------------------------------------------------------------------------------

struct Ctx {
    dz::Pinceau pc;
    int x, y, w, h; // la zone, en coordonnées d'écran
};

void fond_de_carte(const Ctx &c, int x, int y, int w, int h, int rayon) {
    const ThemePrincipal &t = *g.theme;
    if (t.carte_opa > 0) {
        dz::rect(c.pc, x, y, w, h, rayon, t.carte, t.carte_opa);
    }
}

// ---- zone : barre du haut ------------------------------------------------------------------------------------------------

// Place que prennent à droite de la barre les deux indications d'état (exemple, périmée) : le dessin de la barre et le calcul de la
// place de la pastille de l'alerte officielle s'appuient sur les mêmes valeurs, pour ne jamais se chevaucher.
int largeur_etat_exemple() {
    return dz::largeur_texte(g.petit, texte_principal::t(texte_principal::kExemple)) + px(28);
}

int largeur_etat_perimee() { return dz::largeur_texte(g.petit, texte_principal::t(texte_principal::kPerimee)); }

int bord_droit_libre(int x_barre, int w_barre) {
    int droite = x_barre + w_barre;
    if (g.e.meteo.exemple) {
        droite -= largeur_etat_exemple() + px(12);
    }
    if (g.e.meteo.perimee) {
        droite -= largeur_etat_perimee() + px(30) + px(22);
    }
    return droite;
}

// Où finit la ville dans la barre (épingle, texte ajusté au tiers de la barre au plus, marge) : la pastille de l'alerte commence après,
// jamais dessus. Sans ville, la barre est libre dès son bord. Mêmes calculs que dessiner_barre.
int bord_gauche_libre(int x_barre, int w_barre) {
    const VueMeteo &m = g.e.meteo;
    if (m.ville[0] == '\0') {
        return x_barre;
    }
    const Ajuste a = ajuster({&g.corps, &g.petit}, m.ville, w_barre / 3);
    return x_barre + px(30) + px(8) + std::min(w_barre / 3, dz::largeur_texte(*a.police, a.texte)) + px(24);
}

// Calcule la pastille de l'alerte officielle (ADR-0009) : au centre de la barre, jamais sur la ville ni sur les
// indications d'état ; son texte est celui de l'émetteur, coupé par « … » s'il est trop long (la page des alertes le donne en entier) ;
// « +N » quand d'autres alertes sont en vigueur. Si la place manque pour du texte, il reste l'icône et le « +N » : une alerte en
// vigueur ne disparaît jamais de l'écran faute de place.
PlaceOfficielle placer_pastille_officielle() {
    PlaceOfficielle p;
    const vigie::Bandeau &b = g.e.bandeau;
    const Rect &r = g.rect[Z_BARRE];
    if (!b.present || r.w <= 0 || r.h <= 0) {
        return p;
    }
    const int icone = px(30);
    const int pad_g = px(12);
    const int ecart = px(10);
    const int droite = bord_droit_libre(r.x, r.w);
    const int gauche = bord_gauche_libre(r.x, r.w);
    const int dispo = droite - gauche;
    int d_autres = 0;
    if (b.autres > 0) {
        std::snprintf(p.autres, sizeof p.autres, "+%u", static_cast<unsigned>(b.autres));
        d_autres = std::max(px(30), dz::largeur_texte(g.mini, p.autres) + px(14));
    }
    const int pad_d = d_autres > 0 ? px(5) : px(20);
    const int fixe = pad_g + icone + pad_d + (d_autres > 0 ? ecart + d_autres : 0);
    const int texte_max = dispo - fixe - ecart;
    if (texte_max >= px(60) && b.texte[0] != '\0') {
        const Ajuste a = ajuster({&g.corps, &g.petit}, b.texte, texte_max);
        std::snprintf(p.texte, sizeof p.texte, "%s", a.texte);
        p.police = a.police;
        p.w_texte = std::min(texte_max, dz::largeur_texte(*a.police, a.texte));
    }
    int x = pad_g + icone;
    if (p.w_texte > 0) {
        x += ecart;
        p.x_texte = x;
        x += p.w_texte;
    }
    if (d_autres > 0) {
        x += ecart;
        p.x_autres = x;
        p.d_autres = d_autres;
        x += d_autres;
    }
    p.w = x + pad_d;
    p.h = px(40);
    p.y = r.y + r.h / 2 - p.h / 2;
    p.x = std::max(r.x + (r.w - p.w) / 2, gauche);
    if (p.x + p.w > droite) {
        p.x = droite - p.w;
    }
    p.ok = p.x >= r.x;
    return p;
}

// Met l'objet de la pastille à sa place (ou le cache). À appeler quand l'alerte, la langue, les indications d'état ou la disposition
// changent : la zone de la barre est redessinée au même moment (empreinte).
void positionner_pastille_officielle() {
    g.officielle = placer_pastille_officielle();
    if (g.pastille == nullptr) {
        return;
    }
    const PlaceOfficielle &p = g.officielle;
    if (!p.ok) {
        if (!lv_obj_has_flag(g.pastille, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(g.pastille, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    lv_obj_set_pos(g.pastille, p.x, p.y);
    lv_obj_set_size(g.pastille, p.w, p.h);
    lv_obj_remove_flag(g.pastille, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(g.pastille);
}

void dessiner_barre(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int centre = c.y + c.h / 2;
    const int pastille = px(30);
    int x = c.x;
    // La ville, à gauche, avec une épingle.
    if (m.ville[0] != '\0') {
        icones::piece(c.pc.couche, Piece::epingle, pastille, x, centre - pastille / 2, t.texte_2);
        x += pastille + px(8);
        const Ajuste a = ajuster({&g.corps, &g.petit}, m.ville, c.w / 3);
        dz::texte(c.pc, *a.police, a.texte, x, centre - dz::hauteur_ligne(*a.police) / 2, c.w / 3, LV_TEXT_ALIGN_LEFT,
                  t.texte_2);
    }
    // À droite : l'état des données (périmées, exemple), avant le bouton Réglages.
    int droite = c.x + c.w;
    if (m.exemple) {
        const char *s = texte_principal::t(texte_principal::kExemple);
        const int w = largeur_etat_exemple();
        dz::rect(c.pc, droite - w, centre - px(18), w, px(36), px(18), t.avertissement);
        dz::texte(c.pc, g.petit, s, droite - w, centre - dz::hauteur_ligne(g.petit) / 2, w, LV_TEXT_ALIGN_CENTER,
                  t.texte_sur_couleur);
        droite -= w + px(12);
    }
    if (m.perimee) {
        const char *s = texte_principal::t(texte_principal::kPerimee);
        const int w = largeur_etat_perimee();
        dz::texte(c.pc, g.petit, s, droite - w, centre - dz::hauteur_ligne(g.petit) / 2, w, LV_TEXT_ALIGN_RIGHT,
                  t.avertissement);
        icones::piece(c.pc.couche, Piece::hors_ligne, pastille, droite - w - pastille - px(6), centre - pastille / 2,
                      t.avertissement);
        droite -= w + pastille + px(22);
    }
    // Au centre : l'alerte officielle de Vigie quand il y en a une en vigueur (objet g.pastille, dessiné par-dessus cette zone) ;
    // sinon l'alerte déduite de la météo du jour, une pastille de la couleur de l'alerte.
    const Alerte al = g.e.bandeau.present ? Alerte::AUCUNE : alerte_du_jour();
    if (al != Alerte::AUCUNE) {
        const char *s = texte_alerte(al);
        const int w_texte = dz::largeur_texte(g.corps, s);
        const int w = pastille + px(10) + w_texte + px(34);
        const int h = px(40);
        int x_pastille = c.x + (c.w - w) / 2;
        x_pastille = std::max(x_pastille, c.x + c.w / 3 + px(20));
        if (x_pastille + w <= droite) {
            dz::rect(c.pc, x_pastille, centre - h / 2, w, h, h / 2, couleur_alerte(t, al));
            icones::piece(c.pc.couche, piece_alerte(al), pastille, x_pastille + px(12), centre - pastille / 2,
                          t.texte_sur_couleur);
            dz::texte(c.pc, g.corps, s, x_pastille + px(12) + pastille + px(10),
                      centre - dz::hauteur_ligne(g.corps) / 2, w_texte + px(4), LV_TEXT_ALIGN_LEFT,
                      t.texte_sur_couleur);
        }
    }
}

// ---- zone : horloge et date ----------------------------------------------------------------------------------------------

void dessiner_horloge(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const char *heure = g.e.heure_connue ? g.e.heure : texte_principal::kHeureAbsente;
    const lv_text_align_t align =
        (g.portrait || t.disposition == Disposition::HORLOGE) ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT;
    // Le bloc « chiffres + date » est centré dans la zone (la hauteur de la zone varie selon que les prévisions sont là ou non).
    const int h_bloc = dz::hauteur_chiffres(g.heure) + px(18) + dz::hauteur_chiffres(g.titre);
    const int haut_chiffres = c.y + std::max(px(2), (c.h - h_bloc) / 2);
    dz::texte_par_les_chiffres(c.pc, g.heure, heure, c.x, haut_chiffres, c.w, align, t.texte);
    const int bas_chiffres = haut_chiffres + dz::hauteur_chiffres(g.heure);
    const char *date = g.e.heure_connue && g.e.date != nullptr && g.e.date[0] != '\0'
                           ? g.e.date
                           : (g.e.date_attente != nullptr && g.e.date_attente[0] != '\0'
                                  ? g.e.date_attente
                                  : texte_principal::t(texte_principal::kDateAttente));
    const int y_date = bas_chiffres + px(18);
    const Ajuste a = ajuster({&g.titre, &g.corps, &g.petit}, date, c.w);
    dz::texte_par_les_chiffres(c.pc, *a.police, a.texte, c.x, y_date, c.w, align, t.texte_2);
}

// ---- zone : la météo du moment ------------------------------------------------------------------------------------------

void triangle_haut_bas(const dz::Pinceau &pc, int cx, int cy, int r, bool haut, std::uint32_t rgb) {
    if (haut) {
        dz::triangle(pc, cx, cy - r, cx - r, cy + r * 3 / 4, cx + r, cy + r * 3 / 4, rgb);
    } else {
        dz::triangle(pc, cx, cy + r, cx - r, cy - r * 3 / 4, cx + r, cy - r * 3 / 4, rgb);
    }
}

// « ▲ 16° » ou « ▼ 6° » centré verticalement sur `y_centre`, dans la couleur de la rampe de températures.
void ligne_extreme(const Ctx &c, int x, int y_centre, bool haut, int dix) {
    const ThemePrincipal &t = *g.theme;
    char s[24];
    fmt_temp(s, sizeof s, dix);
    const int r = px(9);
    triangle_haut_bas(c.pc, x + r, y_centre, r, haut, couleur_temperature(dix, t.rampe));
    dz::texte(c.pc, g.corps, s, x + 2 * r + px(8), y_centre - dz::hauteur_ligne(g.corps) / 2,
              dz::largeur_texte(g.corps, s) + px(4), LV_TEXT_ALIGN_LEFT, t.texte);
}

void dessiner_maintenant(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    fond_de_carte(c, c.x, c.y, c.w, c.h, px(28));
    const int pad = px(12);
    if (!m.connue) {
        const int cote = std::min(c.h - 2 * pad - dz::hauteur_ligne(g.corps), px(120));
        icones::meteo(c.pc.couche, IdIcone::inconnue, cote, c.x + (c.w - cote) / 2, c.y + pad, t, 110);
        texte_ajuste(c.pc, {&g.corps, &g.petit}, texte_principal::t(texte_principal::kMeteoIndisponible), c.x + pad,
                     c.y + c.h - pad - dz::hauteur_ligne(g.corps) / 2, c.w - 2 * pad, LV_TEXT_ALIGN_CENTER, t.texte_2);
        return;
    }
    const int cote = std::min(c.h - 2 * pad, c.w * 40 / 100);
    const int icone_x = c.x + pad;
    icones::meteo(c.pc.couche, icone_pour_code(m.code, m.jour), cote, icone_x, c.y + (c.h - cote) / 2, t);
    const int x0 = icone_x + cote + px(14);
    const int w = c.x + c.w - pad - x0;

    const int h_chiffres = dz::hauteur_chiffres(g.temperature);
    const int h_libelle = dz::hauteur_ligne(g.titre);
    const int h_detail = dz::hauteur_ligne(g.petit);
    const int bloc = h_chiffres + px(8) + h_libelle + px(2) + h_detail;
    int y = c.y + std::max(px(6), (c.h - bloc) / 2);

    // La température, très grande, dans la couleur de la rampe ; le maximum et le minimum du jour à sa droite.
    char temp[24];
    fmt_temp(temp, sizeof temp, m.temp_dix);
    const std::uint32_t couleur_temp = connu(m.temp_dix) ? couleur_temperature(m.temp_dix, t.rampe) : t.texte;
    dz::texte_par_les_chiffres(c.pc, g.temperature, temp, x0, y, w, LV_TEXT_ALIGN_LEFT, couleur_temp);
    if (connu(m.min_dix) && connu(m.max_dix)) {
        const int x_extremes = x0 + dz::largeur_texte(g.temperature, temp) + px(18);
        if (x_extremes + px(104) <=
            x0 + w) { // sinon (nombre très large) on ne les montre pas plutôt que de les écraser
            ligne_extreme(c, x_extremes, y + h_chiffres * 27 / 100, true, m.max_dix);
            ligne_extreme(c, x_extremes, y + h_chiffres * 73 / 100, false, m.min_dix);
        }
    }
    y += h_chiffres + px(8);

    // Le libellé du temps, puis le ressenti.
    char libelle[64];
    libelle_meteo(libelle, sizeof libelle, m.code, m.jour);
    texte_ajuste(c.pc, {&g.titre, &g.corps, &g.petit}, libelle, x0, y + h_libelle / 2, w, LV_TEXT_ALIGN_LEFT, t.texte);
    y += h_libelle + px(2);
    if (connu(m.ressenti_dix)) {
        char ressenti[24], ligne[64];
        fmt_temp(ressenti, sizeof ressenti, m.ressenti_dix);
        std::snprintf(ligne, sizeof ligne, "%s %s", texte_principal::t("Ressenti"), ressenti);
        texte_ajuste(c.pc, {&g.petit, &g.mini}, ligne, x0, y + h_detail / 2, w, LV_TEXT_ALIGN_LEFT, t.texte_2);
    }
}

// ---- zone : grandeurs (vent, pluie, humidité, pression, UV, soleil, lune) ------------------------------------------------------
//
// Une tuile = l'icône de la grandeur (en couleur), sa valeur en gros, une ligne de détail. Pas de libellé : l'icône dit de quoi on
// parle dans toutes les langues, et un mot de plus ne tiendrait pas dans « Luftfeuchtigkeit » ou « Basınç ».

struct Tuile {
    Rect r;
    Metrique grandeur;
};

// Les sept tuiles : une rangée en paysage (largeurs en poids), une grille de 4 colonnes en portrait (le soleil en occupe deux).
int placer_tuiles(const Ctx &c, Tuile *t) {
    static constexpr Metrique ORDRE[7] = {Metrique::VENT, Metrique::PLUIE,  Metrique::HUMIDITE, Metrique::PRESSION,
                                          Metrique::UV,   Metrique::SOLEIL, Metrique::LUNE};
    const int ecart = px(10);
    if (!g.portrait) {
        static constexpr int POIDS[7] = {140, 130, 130, 160, 135, 235, 206}; // en pixels de référence (somme 1136)
        int total = 0;
        for (int p : POIDS) {
            total += p;
        }
        const int net = c.w - 6 * ecart;
        int x = c.x;
        for (int i = 0; i < 7; ++i) {
            const int w = (i == 6) ? (c.x + c.w - x) : net * POIDS[i] / total;
            t[i] = {{x, c.y, w, c.h}, ORDRE[i]};
            x += w + ecart;
        }
        return 7;
    }
    const int col = (c.w - 3 * ecart) / 4;
    const int ligne = (c.h - ecart) / 2;
    const auto cellule = [&](int cx, int cy, int span) {
        return Rect{c.x + cx * (col + ecart), c.y + cy * (ligne + ecart), span * col + (span - 1) * ecart, ligne};
    };
    t[0] = {cellule(0, 0, 1), Metrique::VENT};
    t[1] = {cellule(1, 0, 1), Metrique::PLUIE};
    t[2] = {cellule(2, 0, 1), Metrique::HUMIDITE};
    t[3] = {cellule(3, 0, 1), Metrique::PRESSION};
    t[4] = {cellule(0, 1, 1), Metrique::UV};
    t[5] = {cellule(1, 1, 2), Metrique::SOLEIL};
    t[6] = {cellule(3, 1, 1), Metrique::LUNE};
    return 7;
}

int pad_tuile() { return px(10); }
int icone_tuile_px() { return px(38); }

// L'icône de la grandeur en haut à gauche ; rend l'ordonnée de la ligne de valeur.
int icone_tuile(const Ctx &c, const Rect &r, Piece piece, Metrique grandeur) {
    icones::piece(c.pc.couche, piece, icone_tuile_px(), r.x + pad_tuile(), r.y + pad_tuile(),
                  couleur_metrique(*g.theme, grandeur));
    return r.y + pad_tuile() + icone_tuile_px();
}

// La valeur en gros, suivie de son unité (ou d'un mot) en petit, posée sur la même ligne de base.
void valeur_tuile(const Ctx &c, const Rect &r, int y, const char *valeur, const char *unite, std::uint32_t rgb) {
    const ThemePrincipal &t = *g.theme;
    const int w = r.w - 2 * pad_tuile();
    const int w_valeur = dz::largeur_texte(g.corps, valeur);
    dz::texte(c.pc, g.corps, valeur, r.x + pad_tuile(), y, w, LV_TEXT_ALIGN_LEFT, rgb);
    if (unite == nullptr || unite[0] == '\0') {
        return;
    }
    const Police *u = &g.mini;
    if (w_valeur + px(5) + dz::largeur_texte(*u, unite) > w) {
        u = &g.nano; // l'unité garde sa place : on la réduit avant de la couper
    }
    const int ligne_de_base = y + dz::hauteur_ligne(g.corps) - g.corps.f->base_line;
    const int y_unite = ligne_de_base - (dz::hauteur_ligne(*u) - u->f->base_line);
    dz::texte(c.pc, *u, unite, r.x + pad_tuile() + w_valeur + px(5), y_unite, std::max(0, w - w_valeur - px(5)),
              LV_TEXT_ALIGN_LEFT, t.texte_3);
}

int y_detail(int y_valeur) { return y_valeur + dz::hauteur_ligne(g.corps); }

void detail_tuile(const Ctx &c, const Rect &r, int y_valeur, const char *texte, std::uint32_t rgb) {
    texte_ajuste(c.pc, {&g.mini, &g.nano}, texte, r.x + pad_tuile(), y_detail(y_valeur) + dz::hauteur_ligne(g.mini) / 2,
                 r.w - 2 * pad_tuile(), LV_TEXT_ALIGN_LEFT, rgb);
}

void tuile_vent(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int y = icone_tuile(c, r, Piece::vent, Metrique::VENT);
    char v[16];
    if (connu(m.vent_dix_kmh)) {
        std::snprintf(v, sizeof v, "%d", arrondi_dix(m.vent_dix_kmh));
    } else {
        std::snprintf(v, sizeof v, "%s", TIRET);
    }
    valeur_tuile(c, r, y, v, connu(m.vent_dix_kmh) ? "km/h" : "", t.texte);
    // Flèche de direction (vers où le vent souffle), dans un cercle, en haut à droite.
    if (m.direction_deg >= 0) {
        const int rayon = px(16);
        const int cx = r.x + r.w - pad_tuile() - rayon - px(2);
        const int cy = r.y + pad_tuile() + icone_tuile_px() / 2;
        dz::anneau(c.pc, cx, cy, rayon + px(3), px(2), t.filet);
        const double a = angle_fleche_vent(m.direction_deg) * 3.14159265358979 / 180.0;
        const auto point = [&](double angle, int rayon_point) {
            return lv_point_t{cx + static_cast<int>(std::lround(std::sin(angle) * rayon_point)),
                              cy + static_cast<int>(std::lround(-std::cos(angle) * rayon_point))};
        };
        const lv_point_t pointe = point(a, rayon);
        const lv_point_t g1 = point(a + 2.5, rayon * 7 / 10);
        const lv_point_t g2 = point(a - 2.5, rayon * 7 / 10);
        dz::triangle(c.pc, pointe.x, pointe.y, g1.x, g1.y, g2.x, g2.y, couleur_metrique(t, Metrique::VENT));
    }
    char d[48];
    if (connu(m.rafales_dix_kmh) && connu(m.vent_dix_kmh) && m.rafales_dix_kmh > m.vent_dix_kmh + 50) {
        std::snprintf(d, sizeof d, "%s %d", texte_principal::t("Rafales"), arrondi_dix(m.rafales_dix_kmh));
    } else if (m.direction_deg >= 0) {
        static constexpr const char *SECTEURS[8] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
        std::snprintf(d, sizeof d, "%s", texte_principal::t(SECTEURS[secteur_vent(m.direction_deg)]));
    } else {
        d[0] = '\0';
    }
    detail_tuile(c, r, y, d, t.texte_2);
}

void tuile_pluie(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int y = icone_tuile(c, r, Piece::parapluie, Metrique::PLUIE);
    char v[16];
    if (m.proba_pluie_pct >= 0) {
        std::snprintf(v, sizeof v, "%d", m.proba_pluie_pct);
    } else {
        std::snprintf(v, sizeof v, "%s", TIRET);
    }
    valeur_tuile(c, r, y, v, m.proba_pluie_pct >= 0 ? "%" : "", t.texte);
    char d[48] = "";
    switch (pluie_proche(m.pluie_dans_min)) {
    case PluieProche::EN_COURS:
        std::snprintf(d, sizeof d, "%s", texte_principal::t("Il pleut"));
        break;
    case PluieProche::BIENTOT:
        std::snprintf(d, sizeof d, texte_principal::t("dans %d min"), m.pluie_dans_min);
        break;
    case PluieProche::AUCUNE:
        std::snprintf(d, sizeof d, "%s", texte_principal::t("Sec 2 h"));
        break;
    case PluieProche::INCONNUE:
        if (connu(m.pluie_dix_mm)) {
            char mm[16];
            fmt_dixieme(mm, sizeof mm, m.pluie_dix_mm);
            std::snprintf(d, sizeof d, "%s mm", mm);
        }
        break;
    }
    detail_tuile(c, r, y, d, t.texte_2);
}

void tuile_humidite(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int y = icone_tuile(c, r, Piece::humidite, Metrique::HUMIDITE);
    char v[16];
    if (m.humidite_pct >= 0) {
        std::snprintf(v, sizeof v, "%d", m.humidite_pct);
    } else {
        std::snprintf(v, sizeof v, "%s", TIRET);
    }
    valeur_tuile(c, r, y, v, m.humidite_pct >= 0 ? "%" : "", t.texte);
    // Jauge fine : un rail et son remplissage, sur la ligne de détail.
    const int h = px(8);
    const int w = r.w - 2 * pad_tuile();
    const int yb = y_detail(y) + dz::hauteur_ligne(g.mini) / 2 - h / 2;
    dz::rect(c.pc, r.x + pad_tuile(), yb, w, h, h / 2, t.filet);
    if (m.humidite_pct >= 0) {
        dz::rect(c.pc, r.x + pad_tuile(), yb, std::max(h, w * m.humidite_pct / 100), h, h / 2,
                 couleur_metrique(t, Metrique::HUMIDITE));
    }
}

void tuile_pression(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int y = icone_tuile(c, r, Piece::pression, Metrique::PRESSION);
    char v[16];
    if (connu(m.pression_dix_hpa)) {
        std::snprintf(v, sizeof v, "%d", arrondi_dix(m.pression_dix_hpa));
    } else {
        std::snprintf(v, sizeof v, "%s", TIRET);
    }
    valeur_tuile(c, r, y, v, connu(m.pression_dix_hpa) ? "hPa" : "", t.texte);
    // Tendance sur trois heures : un triangle en haut à droite, et trois mots sous la valeur.
    const TendancePression tend = tendance_pression(m.tendance_dix_hpa_3h);
    if (tend != TendancePression::INCONNUE) {
        const int cx = r.x + r.w - pad_tuile() - px(14);
        const int cy = r.y + pad_tuile() + icone_tuile_px() / 2;
        const std::uint32_t rgb = couleur_metrique(t, Metrique::PRESSION);
        if (tend == TendancePression::HAUSSE) {
            triangle_haut_bas(c.pc, cx, cy, px(11), true, rgb);
        } else if (tend == TendancePression::BAISSE) {
            triangle_haut_bas(c.pc, cx, cy, px(11), false, rgb);
        } else {
            dz::trait(c.pc, cx - px(11), cy, cx + px(11), cy, px(4), rgb);
        }
        const char *mot = tend == TendancePression::HAUSSE
                              ? "En hausse"
                              : (tend == TendancePression::BAISSE ? "En baisse" : "Stable");
        detail_tuile(c, r, y, texte_principal::t(mot), t.texte_2);
    }
}

const char *texte_uv(CategorieUv cat) {
    switch (cat) {
    case CategorieUv::FAIBLE:
        return texte_principal::t("Faible");
    case CategorieUv::MODERE:
        return texte_principal::t("Modéré");
    case CategorieUv::ELEVE:
        return texte_principal::t("Élevé");
    case CategorieUv::TRES_ELEVE:
        return texte_principal::t("Très élevé");
    case CategorieUv::EXTREME:
        return texte_principal::t("Extrême");
    case CategorieUv::INCONNU:
        break;
    }
    return "";
}

void tuile_uv(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int y = icone_tuile(c, r, Piece::uv, Metrique::UV);
    const CategorieUv cat = categorie_uv(m.uv_dix);
    char v[16];
    if (cat != CategorieUv::INCONNU) {
        std::snprintf(v, sizeof v, "%d", arrondi_dix(m.uv_dix));
    } else {
        std::snprintf(v, sizeof v, "%s", TIRET);
    }
    valeur_tuile(c, r, y, v, texte_uv(cat), cat != CategorieUv::INCONNU ? couleur_uv(t, cat) : t.texte);
    // Cinq segments qui se remplissent jusqu'au niveau.
    const int h = px(8);
    const int ecart = px(4);
    const int w = r.w - 2 * pad_tuile();
    const int seg = (w - 4 * ecart) / 5;
    const int yb = y_detail(y) + dz::hauteur_ligne(g.mini) / 2 - h / 2;
    const int niveau = cat == CategorieUv::INCONNU ? 0 : static_cast<int>(cat);
    for (int i = 0; i < 5; ++i) {
        dz::rect(c.pc, r.x + pad_tuile() + i * (seg + ecart), yb, seg, h, h / 2, i < niveau ? t.niveau_uv[i] : t.filet);
    }
}

// Soleil : les deux icônes lever et coucher aux coins, l'arc du jour entre elles avec le soleil à sa position, les deux heures dessous.
void tuile_soleil(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int pad = pad_tuile();
    const int ico = icone_tuile_px();
    const bool connu_soleil = m.lever_s > 0 && m.coucher_s > m.lever_s;
    icones::piece(c.pc.couche, Piece::lever, ico, r.x + pad, r.y + pad, couleur_metrique(t, Metrique::SOLEIL));
    icones::piece(c.pc.couche, Piece::coucher, ico, r.x + r.w - pad - ico, r.y + pad,
                  couleur_metrique(t, Metrique::SOLEIL));
    // L'arc : demi-cercle entre les deux icônes.
    const int h_heure = dz::hauteur_ligne(g.corps);
    const int rayon = std::min(px(48), (r.w - 2 * pad - 2 * ico) / 2 - px(6));
    const int cx = r.x + r.w / 2;
    const int cy = r.y + r.h - pad - h_heure - px(8);
    dz::arc(c.pc, cx, cy, rayon, px(4), 180, 360, t.filet, LV_OPA_COVER, true);
    dz::trait(c.pc, cx - rayon - px(8), cy, cx + rayon + px(8), cy, px(2), t.filet, LV_OPA_COVER, false);
    if (connu_soleil) {
        const ArcSoleil a = arc_soleil(g.e.maintenant_s, m.lever_s, m.coucher_s, m.jour);
        if (a.position_pm >= 0) {
            const int fin = 180 + a.position_pm * 180 / 1000;
            if (fin > 182) {
                dz::arc(c.pc, cx, cy, rayon, px(4), 180, fin, couleur_metrique(t, Metrique::SOLEIL));
            }
            const double ang = fin * 3.14159265358979 / 180.0;
            const int sx = cx + static_cast<int>(std::lround(std::cos(ang) * rayon));
            const int sy = cy + static_cast<int>(std::lround(std::sin(ang) * rayon));
            dz::disque(c.pc, sx, sy, px(11), couleur_metrique(t, Metrique::SOLEIL), 90);
            dz::disque(c.pc, sx, sy, px(7), couleur_metrique(t, Metrique::SOLEIL));
        }
    }
    char lever[16], coucher[16];
    fmt_hhmm(lever, sizeof lever, connu_soleil ? m.lever_s : 0, decalage());
    fmt_hhmm(coucher, sizeof coucher, connu_soleil ? m.coucher_s : 0, decalage());
    const int y = r.y + r.h - pad - h_heure;
    const int moitie = (r.w - 2 * pad) / 2;
    dz::texte(c.pc, g.corps, lever, r.x + pad, y, moitie, LV_TEXT_ALIGN_LEFT, t.texte);
    dz::texte(c.pc, g.corps, coucher, r.x + r.w - pad - moitie, y, moitie, LV_TEXT_ALIGN_RIGHT, t.texte);
}

// Lune : la phase du jour dessinée (calcul local), son éclairement en gros et son nom dessous (sur deux lignes si besoin).
void tuile_lune(const Ctx &c, const Rect &r) {
    const ThemePrincipal &t = *g.theme;
    const int pad = pad_tuile();
    const PhaseLune p = phase_lune(g.e.maintenant_s);
    const int cote = px(64);
    icones::lune(c.pc.couche, p.indice, cote, r.x + pad, r.y + pad, couleur_metrique(t, Metrique::LUNE));
    char pct[16];
    std::snprintf(pct, sizeof pct, "%d", p.eclairement_pct);
    const int x = r.x + pad + cote + px(8);
    const int w_val = dz::largeur_texte(g.corps, pct);
    const int y = r.y + pad + (cote - dz::hauteur_ligne(g.corps)) / 2;
    dz::texte(c.pc, g.corps, pct, x, y, r.x + r.w - pad - x, LV_TEXT_ALIGN_LEFT, t.texte);
    const int ligne_de_base = y + dz::hauteur_ligne(g.corps) - g.corps.f->base_line;
    dz::texte(c.pc, g.mini, "%", x + w_val + px(4), ligne_de_base - (dz::hauteur_ligne(g.mini) - g.mini.f->base_line),
              px(30), LV_TEXT_ALIGN_LEFT, t.texte_3);
    static constexpr const char *NOMS[8] = {"Nouvelle lune",       "Premier croissant", "Premier quartier",
                                            "Gibbeuse croissante", "Pleine lune",       "Gibbeuse décroissante",
                                            "Dernier quartier",    "Dernier croissant"};
    const char *nom = texte_principal::t(NOMS[p.indice]);
    const int w = r.w - 2 * pad;
    const Police &police =
        dz::largeur_texte(g.mini, nom) <= w ? g.mini : g.nano; // sinon LVGL passe à la ligne dans `w`
    const int y_nom = r.y + pad + cote + px(2);
    dz::texte(c.pc, police, nom, r.x + pad, y_nom, w, LV_TEXT_ALIGN_LEFT, t.texte_2, LV_OPA_COVER, 2);
}

void dessiner_metriques(const Ctx &c) {
    Tuile tuiles[7];
    const int n = placer_tuiles(c, tuiles);
    for (int i = 0; i < n; ++i) {
        const Rect &r = tuiles[i].r;
        fond_de_carte(c, r.x, r.y, r.w, r.h, px(20));
        switch (tuiles[i].grandeur) {
        case Metrique::VENT:
            tuile_vent(c, r);
            break;
        case Metrique::PLUIE:
            tuile_pluie(c, r);
            break;
        case Metrique::HUMIDITE:
            tuile_humidite(c, r);
            break;
        case Metrique::PRESSION:
            tuile_pression(c, r);
            break;
        case Metrique::UV:
            tuile_uv(c, r);
            break;
        case Metrique::SOLEIL:
            tuile_soleil(c, r);
            break;
        case Metrique::LUNE:
            tuile_lune(c, r);
            break;
        case Metrique::NOMBRE:
            break;
        }
    }
}

// ---- zone : heure par heure ------------------------------------------------------------------------------------------------

void dessiner_heures(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    fond_de_carte(c, c.x, c.y, c.w, c.h, px(24));
    const int debut = premiere_heure();
    const int disponibles = m.nb_heures - debut;
    if (disponibles <= 0) {
        return;
    }
    const int pad = px(8);
    const int largeur_mini = px(g.portrait ? 84 : 88);
    const int colonnes = std::max(1, std::min(disponibles, (c.w - 2 * pad) / largeur_mini));
    const int col_w = (c.w - 2 * pad) / colonnes;
    const int x0 = c.x + pad + ((c.w - 2 * pad) - col_w * colonnes) / 2;
    const int h_barre = px(5);
    const int h_texte = dz::hauteur_ligne(g.mini) + dz::hauteur_ligne(g.petit) + px(8) + h_barre;
    const int h_icone = std::max(px(28), std::min(px(42), c.h - 2 * pad - h_texte));
    const int bloc = h_texte + h_icone;
    const int y_haut = c.y + std::max(pad, (c.h - bloc) / 2);
    const bool francais = i18n::langue_courante() == i18n::Langue::Fr;
    for (int i = 0; i < colonnes; ++i) {
        const HeurePrevue &h = m.heures[debut + i];
        const int x = x0 + i * col_w;
        if (i > 0) { // un filet léger entre deux colonnes
            dz::rect(c.pc, x, c.y + pad + px(6), px(1), c.h - 2 * pad - px(12), 0, t.filet, 120);
        }
        char libelle[16];
        std::snprintf(libelle, sizeof libelle, francais ? "%dh" : "%d:00", heure_locale(h.instant_s, decalage()));
        int y = y_haut;
        dz::texte(c.pc, g.mini, libelle, x, y, col_w, LV_TEXT_ALIGN_CENTER, i == 0 ? t.accent : t.texte_2);
        y += dz::hauteur_ligne(g.mini) + px(2);
        icones::meteo(c.pc.couche, icone_pour_code(h.code, h.jour), h_icone, x + (col_w - h_icone) / 2, y, t);
        y += h_icone + px(2);
        char temp[24];
        fmt_temp(temp, sizeof temp, h.temp_dix);
        dz::texte(c.pc, g.petit, temp, x, y, col_w, LV_TEXT_ALIGN_CENTER,
                  connu(h.temp_dix) ? couleur_temperature(h.temp_dix, t.rampe) : t.texte_3);
        y += dz::hauteur_ligne(g.petit) + px(4);
        // Barre de pluie : proportionnelle à la probabilité, au moins 2 px dès qu'elle n'est pas nulle.
        if (h.proba_pct >= 0) {
            const int bw = col_w * 6 / 10;
            const int bx = x + (col_w - bw) / 2;
            dz::rect(c.pc, bx, y, bw, h_barre, h_barre / 2, t.filet);
            dz::rect(c.pc, bx, y, hauteur_barre_pct(h.proba_pct, bw), h_barre, h_barre / 2, t.pluie);
        }
    }
}

// ---- zone : sept jours -------------------------------------------------------------------------------------------------------

void nom_du_jour(char *sortie, std::size_t n, const JourPrevu &j, bool aujourdhui) {
    if (aujourdhui) {
        std::snprintf(sortie, n, "%s", texte_principal::t("Aujourd'hui"));
        return;
    }
    // trois premiers points de code du nom du jour dans la langue de l'écran (« ven », « Fre », « Cum »)
    jour_court(i18n::langue_courante(), jour_semaine_locale(j.date_s + 12 * 3600, decalage()), sortie, n);
}

void dessiner_semaine(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    fond_de_carte(c, c.x, c.y, c.w, c.h, px(24));
    const int debut = premier_jour();
    const int n = m.nb_jours - debut;
    if (n <= 0) {
        return;
    }
    int mins[VUE_JOURS_MAX], maxs[VUE_JOURS_MAX];
    for (int i = 0; i < n; ++i) {
        mins[i] = m.jours[debut + i].min_dix;
        maxs[i] = m.jours[debut + i].max_dix;
    }
    const Echelle ech = echelle_semaine(mins, maxs, static_cast<std::size_t>(n));
    const int pad = px(10);
    const bool lignes = g.portrait; // portrait : une ligne par jour ; paysage : une colonne par jour
    const int h_barre = px(8);
    // Contenu d'une colonne, pour le centrer dans la carte.
    const int h_icone = px(44);
    const int h_colonne =
        dz::hauteur_ligne(g.petit) + px(2) + h_icone + px(2) + dz::hauteur_ligne(g.corps) + px(6) + h_barre;
    for (int i = 0; i < n; ++i) {
        const JourPrevu &j = m.jours[debut + i];
        const bool aujourdhui = j.date_s <= g.e.maintenant_s && g.e.maintenant_s < j.date_s + 86400;
        char jour[24];
        nom_du_jour(jour, sizeof jour, j, aujourdhui);
        char bas[24], haut[24], proba[16];
        fmt_temp(bas, sizeof bas, j.min_dix);
        fmt_temp(haut, sizeof haut, j.max_dix);
        std::snprintf(proba, sizeof proba, "%d %%", j.proba_pct);
        const std::uint32_t c_bas = connu(j.min_dix) ? couleur_temperature(j.min_dix, t.rampe) : t.texte_3;
        const std::uint32_t c_haut = connu(j.max_dix) ? couleur_temperature(j.max_dix, t.rampe) : t.texte_3;
        const std::uint32_t c_jour = aujourdhui ? t.accent : t.texte_2;
        if (!lignes) {
            const int col_w = (c.w - 2 * pad) / n;
            const int x = c.x + pad + i * col_w;
            const int cx = x + col_w / 2;
            if (i > 0) {
                dz::rect(c.pc, x, c.y + pad + px(6), px(1), c.h - 2 * pad - px(12), 0, t.filet, 120);
            }
            int y = c.y + std::max(pad, (c.h - h_colonne) / 2);
            texte_ajuste(c.pc, {&g.petit, &g.mini}, jour, x + px(4), y + dz::hauteur_ligne(g.petit) / 2, col_w - px(8),
                         LV_TEXT_ALIGN_CENTER, c_jour);
            y += dz::hauteur_ligne(g.petit) + px(2);
            icones::meteo(c.pc.couche, icone_pour_code(j.code, true), h_icone, cx - h_icone / 2, y, t);
            if (j.proba_pct >= 20) { // probabilité de pluie : à droite de l'icône, seulement quand elle compte
                dz::texte(c.pc, g.mini, proba, cx + h_icone / 2 - px(2), y + h_icone - dz::hauteur_ligne(g.mini),
                          col_w / 2, LV_TEXT_ALIGN_LEFT, couleur_metrique(t, Metrique::PLUIE));
            }
            y += h_icone + px(2);
            // maximum (gros) et minimum (petit) de part et d'autre de l'axe de la colonne
            const int base = y + dz::hauteur_ligne(g.corps) - g.corps.f->base_line;
            dz::texte(c.pc, g.corps, haut, x, y, col_w / 2 - px(3), LV_TEXT_ALIGN_RIGHT, c_haut);
            dz::texte(c.pc, g.petit, bas, cx + px(6), base - (dz::hauteur_ligne(g.petit) - g.petit.f->base_line),
                      col_w / 2 - px(6), LV_TEXT_ALIGN_LEFT, t.texte_2);
            y += dz::hauteur_ligne(g.corps) + px(6);
            // barre min-max sur l'échelle commune
            const int bw = col_w * 7 / 10;
            const int bx = x + (col_w - bw) / 2;
            dz::rect(c.pc, bx, y, bw, h_barre, h_barre / 2, t.filet);
            if (connu(j.min_dix) && connu(j.max_dix)) {
                const int a = position_sur_echelle(j.min_dix, ech, bw);
                const int b = position_sur_echelle(j.max_dix, ech, bw);
                dz::rect_degrade_h(c.pc, bx + a, y, std::max(h_barre, b - a), h_barre, h_barre / 2, c_bas, c_haut);
            }
        } else {
            const int ligne_h = (c.h - 2 * pad) / n;
            const int centre = c.y + pad + i * ligne_h + ligne_h / 2;
            const int w_jour = px(172);
            texte_ajuste(c.pc, {&g.petit, &g.mini}, jour, c.x + pad + px(8), centre, w_jour, LV_TEXT_ALIGN_LEFT,
                         c_jour);
            const int icone = std::min(ligne_h - px(4), px(46));
            icones::meteo(c.pc.couche, icone_pour_code(j.code, true), icone, c.x + pad + w_jour + px(4),
                          centre - icone / 2, t);
            const int w_t = px(64);
            const int x_bas = c.x + pad + w_jour + icone + px(24);
            const int x_proba = c.x + c.w - pad - px(78);
            const int x_haut = x_proba - px(12) - w_t;
            const int bx = x_bas + w_t + px(12);
            const int bw = x_haut - px(12) - bx;
            dz::texte(c.pc, g.petit, bas, x_bas, centre - dz::hauteur_ligne(g.petit) / 2, w_t, LV_TEXT_ALIGN_RIGHT,
                      t.texte_2);
            if (bw > 0) {
                dz::rect(c.pc, bx, centre - h_barre / 2, bw, h_barre, h_barre / 2, t.filet);
                if (connu(j.min_dix) && connu(j.max_dix)) {
                    const int a = position_sur_echelle(j.min_dix, ech, bw);
                    const int b = position_sur_echelle(j.max_dix, ech, bw);
                    dz::rect_degrade_h(c.pc, bx + a, centre - h_barre / 2, std::max(h_barre, b - a), h_barre,
                                       h_barre / 2, c_bas, c_haut);
                }
            }
            dz::texte(c.pc, g.petit, haut, x_haut, centre - dz::hauteur_ligne(g.petit) / 2, w_t, LV_TEXT_ALIGN_LEFT,
                      c_haut);
            if (j.proba_pct >= 0) {
                dz::texte(c.pc, g.mini, proba, x_proba, centre - dz::hauteur_ligne(g.mini) / 2, px(78),
                          LV_TEXT_ALIGN_RIGHT, couleur_metrique(t, Metrique::PLUIE));
            }
        }
    }
}

// ---- dispatcher de dessin -----------------------------------------------------------------------------------------------------

void sur_dessin(lv_event_t *evenement) {
    lv_layer_t *couche = lv_event_get_layer(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    const int zone = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(evenement)));
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const Ctx c = {{couche, g.theme}, a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a)};
    switch (zone) {
    case Z_BARRE:
        dessiner_barre(c);
        break;
    case Z_HORLOGE:
        dessiner_horloge(c);
        break;
    case Z_MAINTENANT:
        dessiner_maintenant(c);
        break;
    case Z_METRIQUES:
        dessiner_metriques(c);
        break;
    case Z_HEURES:
        dessiner_heures(c);
        break;
    case Z_SEMAINE:
        dessiner_semaine(c);
        break;
    default:
        break;
    }
}

void sur_dessin_bouton(lv_event_t *evenement) {
    lv_layer_t *couche = lv_event_get_layer(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const dz::Pinceau pc = {couche, g.theme};
    const ThemePrincipal &t = *g.theme;
    const int w = lv_area_get_width(&a);
    const bool appuye = lv_obj_has_state(objet, LV_STATE_PRESSED);
    const int cx = a.x1 + w / 2;
    const int cy = a.y1 + w / 2;
    dz::disque(pc, cx, cy, w / 2, appuye ? t.accent : (t.carte_opa > 0 ? t.carte : t.filet),
               appuye ? LV_OPA_COVER : std::max<int>(t.carte_opa, 160));
    icones::piece(couche, Piece::reglages, w * 6 / 10, cx - w * 3 / 10, cy - w * 3 / 10,
                  appuye ? t.texte_sur_couleur : t.texte);
}

// La pastille de l'alerte officielle : un seul objet qui se dessine (couleur du niveau, icône, texte de l'émetteur, « +N ») et reçoit
// l'appui. Elle voit son propre état « appuyé » : un voile clair le montre, sans redessiner toute la barre.
void sur_dessin_pastille(lv_event_t *evenement) {
    const PlaceOfficielle &p = g.officielle;
    if (!p.ok) {
        return;
    }
    lv_layer_t *couche = lv_event_get_layer(evenement);
    lv_obj_t *objet = lv_event_get_target_obj(evenement);
    lv_area_t a;
    lv_obj_get_coords(objet, &a);
    const dz::Pinceau pc = {couche, g.theme};
    const ThemePrincipal &t = *g.theme;
    const int centre = a.y1 + p.h / 2;
    const int icone = px(30);
    const std::uint32_t fond = couleur_officielle(t, static_cast<int>(g.e.bandeau.niveau));
    dz::rect(pc, a.x1, a.y1, p.w, p.h, p.h / 2, fond);
    if (lv_obj_has_state(objet, LV_STATE_PRESSED)) {
        dz::rect(pc, a.x1, a.y1, p.w, p.h, p.h / 2, t.texte_sur_couleur, 70);
    }
    icones::piece(couche, Piece::alerte, icone, a.x1 + px(12), centre - icone / 2, t.texte_sur_couleur);
    if (p.w_texte > 0 && p.police != nullptr) {
        dz::texte(pc, *p.police, p.texte, a.x1 + p.x_texte, centre - dz::hauteur_ligne(*p.police) / 2,
                  p.w_texte + px(4), LV_TEXT_ALIGN_LEFT, t.texte_sur_couleur);
    }
    if (p.d_autres > 0) {
        dz::disque(pc, a.x1 + p.x_autres + p.d_autres / 2, centre, p.d_autres / 2, t.texte_sur_couleur);
        dz::texte(pc, g.mini, p.autres, a.x1 + p.x_autres, centre - dz::hauteur_ligne(g.mini) / 2, p.d_autres,
                  LV_TEXT_ALIGN_CENTER, fond);
    }
}

void sur_pastille(lv_event_t *evenement) {
    if (lv_event_get_code(evenement) == LV_EVENT_CLICKED && g.ouvrir_alertes != nullptr) {
        g.ouvrir_alertes();
    }
}

// ---- tactile : anneau sous le doigt, appui long ---------------------------------------------------------------------------

void montrer(lv_obj_t *objet, bool visible) {
    const bool cache = lv_obj_has_flag(objet, LV_OBJ_FLAG_HIDDEN);
    if (visible && cache) {
        lv_obj_remove_flag(objet, LV_OBJ_FLAG_HIDDEN);
    } else if (!visible && !cache) {
        lv_obj_add_flag(objet, LV_OBJ_FLAG_HIDDEN);
    }
}

void sur_toucher(lv_event_t *evenement) {
    switch (lv_event_get_code(evenement)) {
    case LV_EVENT_PRESSED:
        g.appui_long_fait = false;
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

void sur_bouton_reglages(lv_event_t *evenement) {
    if (lv_event_get_code(evenement) == LV_EVENT_CLICKED && g.ouvrir_reglages != nullptr) {
        g.ouvrir_reglages();
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

// ---- fond : dégradé du ciel ---------------------------------------------------------------------------------------------------

// Le dégradé du ciel est vertical et sombre : en RGB565 (5 bits de rouge et de bleu, 6 de vert) un dégradé que LVGL calcule tel
// quel montre des bandes horizontales, une marche de 4 à 8 niveaux sur 8 bits tous les quelques dizaines de lignes. On le dessine
// donc une fois dans une image RGB565 TRAMÉE (matrice de Bayer 8x8 : chaque marche devient un mélange fin des deux teintes voisines,
// que l'oeil prend pour la teinte intermédiaire). L'image fait la taille de l'écran (1,8 Mo à 1280 x 720 : la PSRAM, jamais la RAM
// interne) et n'est refaite que quand le ciel change (matin, soir, nuit, temps) : quelques dizaines de millisecondes. Si
// l'allocation échoue, on retombe sur le dégradé de LVGL (avec ses bandes) plutôt que sur un écran sans fond.
constexpr std::uint8_t BAYER_8[8][8] = {{0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
                                        {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
                                        {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
                                        {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};

void remplir_ciel_trame(lv_draw_buf_t *buf, std::uint32_t haut, std::uint32_t bas) {
    const int h = static_cast<int>(buf->header.h);
    const int w = static_cast<int>(buf->header.w);
    const std::int32_t a[3] = {static_cast<std::int32_t>((haut >> 16) & 0xFF) << 8,
                               static_cast<std::int32_t>((haut >> 8) & 0xFF) << 8,
                               static_cast<std::int32_t>(haut & 0xFF) << 8};
    const std::int32_t d[3] = {(static_cast<std::int32_t>((bas >> 16) & 0xFF) << 8) - a[0],
                               (static_cast<std::int32_t>((bas >> 8) & 0xFF) << 8) - a[1],
                               (static_cast<std::int32_t>(bas & 0xFF) << 8) - a[2]};
    const int dernier = h > 1 ? h - 1 : 1;
    for (int y = 0; y < h; ++y) {
        // composantes de la ligne en 8 bits + 8 bits de fraction
        const std::int32_t r = a[0] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[0]) * y / dernier);
        const std::int32_t gv = a[1] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[1]) * y / dernier);
        const std::int32_t b = a[2] + static_cast<std::int32_t>(static_cast<std::int64_t>(d[2]) * y / dernier);
        auto *ligne = reinterpret_cast<std::uint16_t *>(static_cast<std::uint8_t *>(buf->data) +
                                                        static_cast<std::size_t>(y) * buf->header.stride);
        const std::uint8_t *seuil = BAYER_8[y & 7];
        for (int x = 0; x < w; ++x) {
            const std::int32_t s = seuil[x & 7];   // 0 à 63 : fraction d'une marche de quantification
            std::int32_t qr = (r + s * 32) >> 11;  // marche de 8 niveaux = 2048 en 8.8 (rouge et bleu : 5 bits)
            std::int32_t qg = (gv + s * 16) >> 10; // marche de 4 niveaux = 1024 (vert : 6 bits)
            std::int32_t qb = (b + s * 32) >> 11;
            qr = qr > 31 ? 31 : qr;
            qg = qg > 63 ? 63 : qg;
            qb = qb > 31 ? 31 : qb;
            ligne[x] = static_cast<std::uint16_t>((qr << 11) | (qg << 5) | qb);
        }
    }
}

void appliquer_ciel() {
    const ThemePrincipal &t = *g.theme;
    if (!t.ciel_dynamique) {
        lv_obj_set_style_bg_image_src(g.ecran, nullptr, 0);
        lv_obj_set_style_bg_color(g.ecran, dz::couleur(t.fond), 0);
        lv_obj_set_style_bg_grad_dir(g.ecran, LV_GRAD_DIR_NONE, 0);
        if (g.ciel_image !=
            nullptr) { // venu d'un thème à ciel : le style ne le référence plus, on rend ses 1,8 Mo de PSRAM
            lv_draw_buf_destroy(g.ciel_image);
            g.ciel_image = nullptr;
        }
        return;
    }
    const Degrade d = ciel(moment_du_jour(), ambiance_du_moment());
    // Le dégradé de LVGL reste réglé : c'est le repli si l'image tramée ne peut pas être créée.
    lv_obj_set_style_bg_color(g.ecran, dz::couleur(d.haut), 0);
    lv_obj_set_style_bg_grad_color(g.ecran, dz::couleur(d.bas), 0);
    lv_obj_set_style_bg_grad_dir(g.ecran, LV_GRAD_DIR_VER, 0);
    lv_draw_buf_t *neuf =
        lv_draw_buf_create(static_cast<std::uint32_t>(g.largeur), static_cast<std::uint32_t>(g.hauteur),
                           LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (neuf == nullptr) {
        return;
    }
    remplir_ciel_trame(neuf, d.haut, d.bas);
    lv_obj_set_style_bg_image_src(g.ecran, neuf, 0);
    lv_obj_set_style_bg_image_opa(g.ecran, LV_OPA_COVER, 0);
    if (g.ciel_image != nullptr) { // l'ancienne image n'est plus référencée par le style : on peut la rendre
        lv_draw_buf_destroy(g.ciel_image);
    }
    g.ciel_image = neuf;
}

Police creer_police(const std::uint8_t *ttf, std::size_t taille, int reference) {
    Police p;
    p.px = px(reference);
    p.f = lv_tiny_ttf_create_data(ttf, taille, p.px);
    return p;
}

} // namespace

bool ui_principal_creer(lv_obj_t *ecran, const RessourcesPrincipal &ressources, int32_t largeur, int32_t hauteur,
                        const ThemePrincipal &theme) {
    if (largeur <= 0 || hauteur <= 0 || ressources.ttf == nullptr) {
        return false;
    }
    if (g.ciel_image != nullptr) { // une construction précédente (l'écran qui la montrait est supprimé par l'appelant)
        lv_draw_buf_destroy(g.ciel_image);
    }
    g = Vue{};
    g.theme = &theme;
    g.largeur = largeur;
    g.hauteur = hauteur;
    g.portrait = hauteur > largeur;
    const float ref_l = g.portrait ? 720.0F : 1280.0F;
    const float ref_h = g.portrait ? 1280.0F : 720.0F;
    g.echelle = std::min(static_cast<float>(largeur) / ref_l, static_cast<float>(hauteur) / ref_h);
    g.diametre_anneau = px(88);
    g.ecran = ecran;

    // Tailles de référence : l'heure et la température se lisent de loin, le reste est dimensionné pour la carte de 5 pouces.
    // L'heure : 190 px dans la composition complète, 270 px (250 en portrait, 664 px de large) quand elle est la vedette.
    const bool vedette = theme.disposition == Disposition::HORLOGE;
    g.heure = creer_police(ressources.ttf, ressources.ttf_taille, vedette ? (g.portrait ? 250 : 270) : 190);
    g.temperature = creer_police(ressources.ttf, ressources.ttf_taille, g.portrait ? 128 : 124);
    g.titre = creer_police(ressources.ttf, ressources.ttf_taille, 42);
    g.corps = creer_police(ressources.ttf, ressources.ttf_taille, 34);
    g.petit = creer_police(ressources.ttf, ressources.ttf_taille, 28);
    g.mini = creer_police(ressources.ttf, ressources.ttf_taille, 23);
    g.nano = creer_police(ressources.ttf, ressources.ttf_taille, 19); // unités et détails de dernier rang
    if (g.heure.f == nullptr || g.temperature.f == nullptr || g.titre.f == nullptr || g.corps.f == nullptr ||
        g.petit.f == nullptr || g.mini.f == nullptr || g.nano.f == nullptr) {
        return false;
    }

    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_add_flag(ecran, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE); // un glissement ne doit pas faire défiler l'écran
    lv_obj_add_event_cb(ecran, sur_toucher, LV_EVENT_ALL, nullptr);
    g.objets = 1;

    for (int z = 0; z < Z_NOMBRE; ++z) {
        g.zone[z] = creer_zone(ecran);
        lv_obj_add_event_cb(g.zone[z], sur_dessin, LV_EVENT_DRAW_MAIN,
                            reinterpret_cast<void *>(static_cast<std::intptr_t>(z)));
    }

    // Bouton Réglages (ADR-0007) : rond, en haut à droite, zone tactile élargie. Un appui long sur le bouton ne bascule pas l'écran :
    // les événements d'un bouton ne remontent pas à l'écran.
    g.bouton = creer_zone(ecran);
    lv_obj_add_flag(g.bouton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(g.bouton, px(28));
    lv_obj_add_event_cb(g.bouton, sur_bouton_reglages, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(g.bouton, sur_dessin_bouton, LV_EVENT_DRAW_MAIN, nullptr);

    // Pastille de l'alerte officielle (ADR-0009) : cachée tant qu'aucune alerte n'est en vigueur (pas de bandeau permanent), placée par
    // positionner_pastille_officielle. Comme le bouton Réglages, ses événements ne remontent pas à l'écran : pas d'appui long dessus.
    g.pastille = creer_zone(ecran);
    lv_obj_add_flag(g.pastille, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g.pastille, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_ext_click_area(g.pastille, px(12));
    lv_obj_add_event_cb(g.pastille, sur_pastille, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(g.pastille, sur_dessin_pastille, LV_EVENT_DRAW_MAIN, nullptr);

    // Retour tactile : un anneau sous le doigt, sur la couche supérieure (coordonnées d'écran exactes).
    lv_obj_t *couche = lv_display_get_layer_top(lv_obj_get_display(ecran));
    g.anneau = lv_obj_create(couche);
    lv_obj_remove_style_all(g.anneau);
    lv_obj_remove_flag(g.anneau, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g.anneau, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g.anneau, g.diametre_anneau, g.diametre_anneau);
    lv_obj_set_style_radius(g.anneau, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g.anneau, px(6), 0);
    lv_obj_set_style_border_color(g.anneau, dz::couleur(theme.accent), 0);
    lv_obj_add_flag(g.anneau, LV_OBJ_FLAG_HIDDEN);
    ++g.objets;

    appliquer_disposition();
    appliquer_ciel();
    return true;
}

void ui_principal_mettre_a_jour(const EntreesPrincipal &entrees) {
    if (g.ecran == nullptr) {
        return;
    }
    g.e = entrees;
    std::snprintf(g.date, sizeof g.date, "%s", entrees.date != nullptr ? entrees.date : "");
    std::snprintf(g.date_attente, sizeof g.date_attente, "%s",
                  entrees.date_attente != nullptr ? entrees.date_attente : "");
    g.e.date = g.date;
    g.e.date_attente = g.date_attente;

    const bool heures = g.e.meteo.connue && g.e.meteo.nb_heures - premiere_heure() > 0;
    bool tout = false;
    if (heures != g.a_heures) {
        g.a_heures = heures;
        appliquer_disposition();
        tout = true;
    }

    const VueMeteo &m = g.e.meteo;
    const int langue = i18n::indice(i18n::langue_courante());
    const auto base = [&](Empreinte &e) {
        e.u(static_cast<std::uint32_t>(langue));
        e.u(m.connue);
    };
    std::uint32_t neuf[Z_NOMBRE];
    {
        Empreinte e;
        base(e);
        e.s(m.ville);
        e.u(m.perimee);
        e.u(m.exemple);
        e.u(static_cast<std::uint32_t>(alerte_du_jour()));
        e.u(g.e.bandeau.present);
        e.u(g.e.bandeau.empreinte); // texte, niveau et nombre d'autres alertes (noyau : vigie::composer_bandeau)
        neuf[Z_BARRE] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(g.e.heure_connue);
        e.s(g.e.heure);
        e.s(g.e.date);
        e.s(g.e.date_attente);
        neuf[Z_HORLOGE] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(m.code);
        e.u(m.jour);
        e.u(m.temp_dix);
        e.u(m.ressenti_dix);
        e.u(m.min_dix);
        e.u(m.max_dix);
        neuf[Z_MAINTENANT] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(m.vent_dix_kmh);
        e.u(m.rafales_dix_kmh);
        e.u(m.direction_deg);
        e.u(m.humidite_pct);
        e.u(m.pression_dix_hpa);
        e.u(m.tendance_dix_hpa_3h);
        e.u(m.uv_dix);
        e.u(m.proba_pluie_pct);
        e.u(m.pluie_dix_mm);
        e.u(m.pluie_dans_min);
        e.i(m.lever_s);
        e.i(m.coucher_s);
        e.u(m.decalage_utc_s);
        // la position du soleil et la phase de la lune changent avec l'heure : on les range par minute et par jour
        e.i(g.e.maintenant_s / 60);
        neuf[Z_METRIQUES] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(m.decalage_utc_s);
        e.u(premiere_heure());
        for (int i = 0; i < m.nb_heures; ++i) {
            e.i(m.heures[i].instant_s);
            e.u(m.heures[i].temp_dix);
            e.u(m.heures[i].code);
            e.u(m.heures[i].jour);
            e.u(m.heures[i].proba_pct);
        }
        neuf[Z_HEURES] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(m.decalage_utc_s);
        e.u(premier_jour());
        e.i(g.e.maintenant_s / 3600); // « aujourd'hui » change avec le jour, pas à la minute
        for (int i = 0; i < m.nb_jours; ++i) {
            e.i(m.jours[i].date_s);
            e.u(m.jours[i].code);
            e.u(m.jours[i].min_dix);
            e.u(m.jours[i].max_dix);
            e.u(m.jours[i].proba_pct);
        }
        neuf[Z_SEMAINE] = e.h;
    }
    Empreinte ciel_e;
    ciel_e.u(static_cast<std::uint32_t>(moment_du_jour()));
    ciel_e.u(static_cast<std::uint32_t>(ambiance_du_moment()));
    if (ciel_e.h != g.empreinte_ciel) {
        g.empreinte_ciel = ciel_e.h;
        appliquer_ciel();
        tout = true;
    }
    for (int z = 0; z < Z_NOMBRE; ++z) {
        if (tout || neuf[z] != g.empreinte[z]) {
            g.empreinte[z] = neuf[z];
            if (g.zone[z] != nullptr) {
                lv_obj_invalidate(g.zone[z]);
            }
            if (z == Z_BARRE) {
                positionner_pastille_officielle(); // l'alerte, la langue ou une indication d'état a changé
            }
        }
    }
    if (tout) {
        lv_obj_invalidate(g.ecran);
    }
}

bool ui_principal_changer_theme(const ThemePrincipal &theme) {
    if (g.ecran == nullptr || g.theme == nullptr) {
        return false;
    }
    if (&theme == g.theme) {
        return true;
    }
    // L'horloge en vedette a sa propre police et sa propre disposition (créées à la construction) : pas de bascule à chaud.
    if ((theme.disposition == Disposition::HORLOGE) != (g.theme->disposition == Disposition::HORLOGE)) {
        return false;
    }
    g.theme = &theme;
    lv_obj_set_style_border_color(g.anneau, dz::couleur(theme.accent), 0);
    appliquer_ciel();
    for (int z = 0; z < Z_NOMBRE; ++z) {
        lv_obj_invalidate(g.zone[z]);
    }
    lv_obj_invalidate(g.bouton);
    lv_obj_invalidate(g.pastille);
    lv_obj_invalidate(g.ecran);
    return true;
}

void ui_principal_sur_appui_long(void (*rappel)()) { g.appui_long = rappel; }

void ui_principal_sur_reglages(void (*rappel)()) { g.ouvrir_reglages = rappel; }

void ui_principal_sur_alertes(void (*rappel)()) { g.ouvrir_alertes = rappel; }

CoutEcran ui_principal_cout() { return {g.objets, 7}; }

} // namespace ecran
