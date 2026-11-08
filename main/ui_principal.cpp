#include "ui_principal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "ecran/accueil.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/i18n.hpp"
#include "ecran/meteo_wmo.hpp"
#include "ecran/soleil.hpp"
#include "ecran/visuel_astres.hpp"
#include "ecran/visuel_dates.hpp"
#include "ecran/visuel_echelles.hpp"
#include "textes_principal.h"
#include "ui_meteo_dessin.h"
#include "ui_meteo_icones.h"

// Écran principal. Chaque ZONE est un seul objet LVGL qui dessine tout son contenu dans l'événement LV_EVENT_DRAW_MAIN : six zones, le
// bouton Réglages, la pastille de l'alerte officielle et l'anneau tactile, au lieu d'une centaine d'étiquettes. Un objet LVGL coûte de
// la RAM interne (le point étroit de cette carte), une tâche de dessin non.
//
//   accueil (thèmes « ciel », « tableau sombre », « clair »), paysage 1280 x 720
//   +-- lieu ----------- ⚙ --+--------------------------+
//   | heure                  |  courbe des 12 prochaines |
//   | date                   |  heures : icônes, pluie,  |
//   | météo du moment        |  maximum et minimum       |
//   +-- bandeau : la pluie ---+--- l'alerte -------------+
//   | cinq jours : nom et icône, 14° / 8°, ruban des températures, vent, pluie |
//
// Le thème « horloge » garde sa composition d'avant (l'heure géante, la météo du moment, les sept jours).
//
// Une zone n'est redessinée que si ses données ont changé (empreinte par zone) : l'horloge change à la minute, le reste à la mise à
// jour de la météo. Ce qui est inconnu est caché ou remplacé par « — », jamais inventé. Une météo périmée est dessinée avec le thème
// grisé (ecran/accueil.hpp).
//
// Couleurs, opacités, dispositions : visuel_themes.hpp et accueil.hpp (aucune couleur écrite ici). Textes : visuel_textes.hpp (sept
// langues). Tout ce qui se calcule sans LVGL est dans le noyau pur (visuel_*.hpp, accueil.hpp, testé sur le PC).

namespace ecran {

namespace {

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
};

struct Vue {
    const ThemePrincipal *theme = &kThemeCiel;
    int largeur = 0, hauteur = 0;
    bool portrait = false;
    float echelle = 1.0F;
    Police heure, temperature, titre, corps, petit, mini;
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
    bool est_accueil = false;            // la composition de l'accueil ; faux : celle de l'horloge en vedette
    int extra_h = 0; // hauteur de référence gagnée au-delà de 720 (la 7B) : elle va à la carte des jours
    ThemePrincipal theme_gris = kThemeCiel; // le thème de la météo périmée (accueil::theme_grise)
    accueil::EpisodePluie pluie;            // la pluie à annoncer dans le bandeau (recalculée à chaque mise à jour)
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

// ---- l'heure du lieu (ADR-0012) -----------------------------------------------------------------------------------------
//
// Chaque instant affiché est converti avec les règles du fuseau du lieu, au décalage en vigueur à CET instant (ecran/heure_lieu.hpp,
// ecran/heure_fr.hpp) : pas un décalage unique pour toute la fenêtre de prévision.

ecran::heure::Fuseau fuseau() {
    if (g.e.fuseau_connu) {
        return g.e.fuseau;
    }
    // Nom de fuseau absent de la table (enregistrement ancien) : on garde le décalage de la réponse météo, fixe, faute de mieux.
    return ecran::heure::fuseau_fixe(g.e.meteo.connue ? g.e.meteo.decalage_utc_s : 0);
}

ecran::heure::HeureLocale heure_du(std::int64_t instant_s) {
    return ecran::heure::en_heure_locale(instant_s, fuseau());
}

// « 07:12 » ; « — » quand l'instant est inconnu.
void fmt_hhmm(char *sortie, std::size_t n, std::int64_t epoch_s) {
    if (epoch_s <= 0 || ecran::heure::formater_hhmm(heure_du(epoch_s), sortie, n) == 0) {
        std::snprintf(sortie, n, "%s", TIRET);
    }
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

// Lever et coucher du soleil du jour LOCAL du lieu, calculés (NOAA, noyau pur). Le calcul coûte quelques dizaines de microsecondes et
// l'empreinte du ciel le demande à chaque passage : le dernier résultat est gardé tant que le jour local, le lieu et le décalage
// ne changent pas.
soleil::LeverCoucher lever_coucher_du_lieu(const ecran::heure::HeureLocale &local) {
    static struct {
        bool valide = false;
        std::int64_t numero_jour = 0;
        double latitude = 0;
        double longitude = 0;
        std::int32_t decalage_s = 0;
        soleil::LeverCoucher resultat;
    } cache;
    const std::int64_t numero = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());
    if (!cache.valide || cache.numero_jour != numero || cache.latitude != g.e.latitude_deg ||
        cache.longitude != g.e.longitude_deg || cache.decalage_s != local.decalage_s) {
        cache.resultat = soleil::lever_coucher(g.e.latitude_deg, g.e.longitude_deg, local.an, local.mois, local.jour,
                                               local.decalage_s);
        cache.numero_jour = numero;
        cache.latitude = g.e.latitude_deg;
        cache.longitude = g.e.longitude_deg;
        cache.decalage_s = local.decalage_s;
        cache.valide = true;
    }
    return cache.resultat;
}

// Jour, aube, crépuscule ou nuit SANS météo (démarrage, réseau absent). Avec la position du lieu : le lever et le coucher du soleil du
// jour local, calculés. Sans position : la plage fixe 8 h à 18 h de l'heure locale (limite connue, docs/mesures/). Sans fuseau
// connu on ne devine rien : nuit, comme quand l'heure n'est pas encore sûre.
MomentJour moment_sans_meteo() {
    if (g.e.maintenant_s <= 0 || !g.e.fuseau_connu) {
        return MomentJour::NUIT;
    }
    const ecran::heure::HeureLocale local = heure_du(g.e.maintenant_s);
    if (g.e.position_connue) {
        const soleil::LeverCoucher lc = lever_coucher_du_lieu(local);
        switch (lc.situation) {
        case soleil::Situation::Normal:
            return arc_soleil(g.e.maintenant_s, lc.lever_s, lc.coucher_s, true).moment;
        case soleil::Situation::JourPolaire:
            return MomentJour::JOUR;
        case soleil::Situation::NuitPolaire:
            return MomentJour::NUIT;
        }
    }
    return (local.heure >= 8 && local.heure < 18) ? MomentJour::JOUR : MomentJour::NUIT;
}

MomentJour moment_du_jour() {
    const VueMeteo &m = g.e.meteo;
    if (m.connue && m.lever_s > 0) {
        return arc_soleil(g.e.maintenant_s, m.lever_s, m.coucher_s, m.jour).moment;
    }
    if (m.connue) {
        return m.jour ? MomentJour::JOUR : MomentJour::NUIT;
    }
    return moment_sans_meteo();
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

// Le jour LOCAL d'un jour de prévision (`date_s` est son minuit local, calculé par la météo avec UN décalage) : on lit son midi, qu'un
// changement d'heure d'une heure ne sort jamais du jour.
std::int64_t numero_du_jour_prevu(const JourPrevu &j) {
    return ecran::heure::numero_jour_local(j.date_s + 12 * 3600, fuseau());
}

// Le premier jour à montrer : on saute ceux qui sont finis (relevé gardé en cache depuis la veille). « Fini » se juge sur le jour
// civil du lieu, pas sur 86 400 s : un jour de changement d'heure dure 23 ou 25 heures.
int premier_jour() {
    const VueMeteo &m = g.e.meteo;
    const std::int64_t aujourdhui = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());
    for (int i = 0; i < m.nb_jours; ++i) {
        if (numero_du_jour_prevu(m.jours[i]) >= aujourdhui) {
            return i;
        }
    }
    return m.nb_jours;
}

// ---- mise en page -------------------------------------------------------------------------------------------------------
//
// Deux compositions. L'ACCUEIL (thèmes « ciel », « tableau sombre », « clair ») : les zones sont calculées par le noyau pur
// (accueil::disposer, testé sur le PC) en pixels de référence, puis mises à l'échelle de l'écran ; la hauteur gagnée sur 720 (la 7B)
// va à la carte des jours. L'HORLOGE en vedette garde sa composition d'avant : l'heure géante au centre, la météo du moment dessous,
// les sept jours en bas, ni courbe d'heures ni bandeau.

struct Cotes {
    int marge, ecart, barre_h;
};

Cotes cotes() { return {px(g.portrait ? 28 : 36), px(10), px(48)}; }

// Un rectangle en pixels de référence -> pixels de l'écran, par ses BORDS (deux zones qui se touchent en référence se touchent encore).
Rect en_pixels(const accueil::Rect &r) {
    const int x0 = px(r.x);
    const int y0 = px(r.y);
    return {x0, y0, px(r.x + r.w) - x0, px(r.y + r.h) - y0};
}

void calculer_disposition() {
    if (g.est_accueil) {
        const int ref_l = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.largeur) / g.echelle)));
        const int ref_h = std::max(1, static_cast<int>(std::lround(static_cast<float>(g.hauteur) / g.echelle)));
        const accueil::Zones z = accueil::disposer(ref_l, ref_h, g.portrait);
        g.extra_h = z.extra_h;
        g.rect[Z_BARRE] = en_pixels(z.barre);
        g.rect[Z_HORLOGE] = en_pixels(z.horloge);
        g.rect[Z_MAINTENANT] = en_pixels(z.maintenant);
        g.rect[Z_HEURES] = en_pixels(z.heures);
        g.rect[Z_BANDEAU] = en_pixels(z.bandeau);
        g.rect[Z_SEMAINE] = en_pixels(z.jours);
        return;
    }
    // Composition calme : l'heure géante au centre, la météo du moment dessous, les sept jours en bas ; ni courbe d'heures ni bandeau
    // (leurs zones restent vides, donc cachées).
    // En paysage : 262 px pour l'heure + la date, 196 pour la carte du moment (sans quoi « Ressenti » est coupé), et les
    // 168 qui restent suffisent à la carte des sept jours.
    const Cotes c = cotes();
    const int utile = g.largeur - 2 * c.marge;
    const int y0 = px(10);
    g.rect[Z_BARRE] = {c.marge, y0, utile - px(64), c.barre_h}; // le bouton Réglages occupe la droite
    int y = y0 + c.barre_h + px(4);
    const int bas = g.hauteur - px(12);
    const int h_horloge = g.portrait ? px(330) : px(262);
    g.rect[Z_HORLOGE] = {c.marge, y, utile, h_horloge};
    y += h_horloge + c.ecart;
    const int h_maintenant = g.portrait ? px(230) : px(196);
    const int w_maintenant = g.portrait ? utile : std::min(utile, px(600));
    g.rect[Z_MAINTENANT] = {(g.largeur - w_maintenant) / 2, y, w_maintenant, h_maintenant};
    y += h_maintenant + c.ecart;
    g.rect[Z_HEURES] = {};
    g.rect[Z_BANDEAU] = {};
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
    const int cote = px(48);
    lv_obj_set_size(g.bouton, cote, cote);
    if (g.est_accueil) { // au bout droit de la zone du lieu, à droite de l'heure
        const Rect &b = g.rect[Z_BARRE];
        lv_obj_set_pos(g.bouton, b.x + b.w - cote, b.y + (b.h - cote) / 2);
    } else {
        lv_obj_set_pos(g.bouton, g.largeur - cotes().marge - cote, px(10));
    }
    positionner_pastille_officielle(); // la barre ou le bandeau a pu changer de taille
}

PlaceOfficielle placer_puce_officielle(); // plus bas, avec le dessin du bandeau

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
    g.officielle = g.est_accueil ? placer_puce_officielle() : placer_pastille_officielle();
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

// ---- l'accueil (thèmes « ciel », « tableau sombre », « clair ») -------------------------------------------------------------
//
// La composition de l'esquisse retenue : à gauche le lieu, l'heure, la date et la météo du moment ; à droite la courbe des 12 prochaines
// heures ; au milieu le bandeau (la pluie à gauche, l'alerte à droite) ; en bas les cinq jours avec leur vent et leur pluie. Les
// positions sont celles de l'esquisse, en pixels de RÉFÉRENCE (1280 x 720) : `px()` les met à l'échelle de l'écran. Rien n'est animé.
// Une valeur de la météo périmée est dessinée avec le thème grisé (theme_meteo) ; l'heure, la date et le lieu ne le sont pas.

constexpr int COLONNES_HEURES = 12;
constexpr int COLONNES_JOURS = 5;
constexpr const char *POINT_MEDIAN = " \xC2\xB7 "; // « · » entre deux morceaux d'une phrase

// Le thème des valeurs de la météo : grisé quand elle est périmée (une valeur périmée n'est plus une mesure).
const ThemePrincipal &theme_meteo() { return g.e.meteo.perimee ? g.theme_gris : *g.theme; }

bool en_francais() { return i18n::langue_courante() == i18n::Langue::Fr; }

// Texte posé par sa LIGNE DE BASE, comme dans l'esquisse (les polices n'ont pas toutes le même vide sous la ligne de base).
void texte_base(const dz::Pinceau &pc, const Police &p, const char *s, int x, int base, int w, lv_text_align_t align,
                std::uint32_t rgb) {
    dz::texte(pc, p, s, x, base - (dz::hauteur_ligne(p) - p.f->base_line), w, align, rgb);
}

// Le verre d'une carte : la teinte du thème et un filet clair à peine visible.
void carte_de_verre(const Ctx &c, int rayon) {
    const ThemePrincipal &t = *g.theme;
    if (t.carte_opa > 0) {
        dz::rect_bord(c.pc, c.x, c.y, c.w, c.h, rayon, t.carte, t.carte_opa, t.texte, 20, px(2));
    }
}

// Un point de la courbe : un halo de la teinte de la carte, puis un point de la couleur du texte.
void point_de_courbe(const dz::Pinceau &pc, const ThemePrincipal &t, int x, int y, int rayon_halo, int rayon) {
    dz::disque(pc, x, y, rayon_halo, t.carte);
    dz::disque(pc, x, y, rayon, t.texte);
}

// Le tracé d'une courbe monotone en petits traits arrondis, chacun de la couleur de la température qu'il traverse (mélange entre les
// teintes de ses deux points).
void tracer_courbe(const dz::Pinceau &pc, const accueil::CourbeMonotone &courbe, const std::uint32_t *teintes,
                   int epaisseur, int pas) {
    for (int i = 0; i + 1 < courbe.n; ++i) {
        const int x0 = courbe.p[i].x;
        const int x1 = courbe.p[i + 1].x;
        const Rgb a = depuis_hex(teintes[i]);
        const Rgb b = depuis_hex(teintes[i + 1]);
        for (int xa = x0; xa < x1;) {
            const int xb = std::min(x1, xa + pas);
            const int milieu_pm = x1 > x0 ? ((xa + xb) / 2 - x0) * 1000 / (x1 - x0) : 0;
            dz::trait(pc, xa, courbe.y_en(xa), xb, courbe.y_en(xb), epaisseur, en_hex(melanger(a, b, milieu_pm)));
            xa = xb;
        }
    }
}

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

// Le voile entre deux courbes (`haut` au-dessus de `bas`), de `x_debut` à `x_fin`, en bandes de `pas` pixels dont chaque bout reprend le
// dégradé global : le même résultat qu'un seul dégradé sous toute la courbe, sans polygone.
void voile_entre(const dz::Pinceau &pc, const accueil::CourbeMonotone &haut, const accueil::CourbeMonotone &bas,
                 int x_debut, int x_fin, int pas, const DegradeGlobal &d) {
    for (int x = x_debut; x < x_fin; x += pas) {
        const int w = std::min(pas, x_fin - x);
        const int xm = x + w / 2;
        const int y_haut = haut.y_en(xm);
        const int y_bas = bas.y_en(xm);
        if (y_bas <= y_haut) {
            continue;
        }
        std::uint32_t c_haut, c_bas;
        std::uint8_t a_haut, a_bas;
        d.en(y_haut, &c_haut, &a_haut);
        d.en(y_bas, &c_bas, &a_bas);
        dz::rect_degrade_v(pc, x, y_haut, w, y_bas - y_haut, c_haut, a_haut, c_bas, a_bas);
    }
}

// ---- zone : lieu ----

void dessiner_barre_accueil(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const int centre = c.y + c.h / 2;
    // Le bouton Réglages occupe le bout droit de la zone ; « Exemple » se pose après le lieu.
    int bord = c.x + c.w - px(48) - px(16);
    const int w_exemple = m.exemple ? largeur_etat_exemple() : 0;
    if (m.exemple) {
        bord -= w_exemple + px(16);
    }
    int x_fin = c.x + px(6);
    if (m.ville[0] != '\0') {
        const int icone = px(28);
        icones::piece(c.pc.couche, Piece::epingle, icone, c.x + px(6), centre - icone / 2, t.texte_2);
        const int x_texte = c.x + px(44);
        const Ajuste a = ajuster({&g.petit, &g.mini}, m.ville, std::max(px(40), bord - x_texte));
        const int w = std::min(bord - x_texte, dz::largeur_texte(*a.police, a.texte));
        dz::texte(c.pc, *a.police, a.texte, x_texte, centre - dz::hauteur_ligne(*a.police) / 2,
                  std::max(px(40), w) + px(4), LV_TEXT_ALIGN_LEFT, t.texte_2);
        x_fin = x_texte + w;
    }
    if (m.exemple) {
        const int x = x_fin + px(16);
        dz::rect(c.pc, x, centre - px(18), w_exemple, px(36), px(18), t.avertissement);
        dz::texte(c.pc, g.petit, texte_principal::t(texte_principal::kExemple), x,
                  centre - dz::hauteur_ligne(g.petit) / 2, w_exemple, LV_TEXT_ALIGN_CENTER, t.texte_sur_couleur);
    }
}

// ---- zone : l'heure et la date ----

void dessiner_horloge_accueil(const Ctx &c) {
    const ThemePrincipal &t = *g.theme;
    const char *heure = g.e.heure_connue ? g.e.heure : texte_principal::kHeureAbsente;
    const lv_text_align_t align = g.portrait ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT;
    const int retrait = g.portrait ? 0 : px(4);
    dz::texte_par_les_chiffres(c.pc, g.heure, heure, c.x + retrait, c.y + px(11), c.w - retrait, align, t.texte);
    const char *date = g.e.heure_connue && g.e.date != nullptr && g.e.date[0] != '\0'
                           ? g.e.date
                           : (g.e.date_attente != nullptr && g.e.date_attente[0] != '\0'
                                  ? g.e.date_attente
                                  : texte_principal::t(texte_principal::kDateAttente));
    const int retrait_date = g.portrait ? 0 : px(10);
    const Ajuste a = ajuster({&g.titre, &g.corps, &g.petit}, date, c.w - retrait_date);
    // La ligne de base de la date est à 178 px de référence sous le haut de la zone.
    dz::texte_par_les_chiffres(c.pc, *a.police, a.texte, c.x + retrait_date,
                               c.y + px(178) - dz::hauteur_chiffres(*a.police), c.w - retrait_date, align, t.texte_2);
}

// ---- zone : la météo du moment ----

void dessiner_maintenant_accueil(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const VueMeteo &m = g.e.meteo;
    const int icone = px(56);
    const int x_icone = c.x + px(12);
    const int y_icone = c.y + px(2);
    if (!m.connue) {
        icones::meteo(c.pc.couche, IdIcone::inconnue, icone, x_icone, y_icone, t, 110);
        const int x = x_icone + icone + px(24);
        texte_ajuste(c.pc, {&g.corps, &g.petit, &g.mini}, texte_principal::t(texte_principal::kMeteoIndisponible), x,
                     c.y + c.h / 2, c.x + c.w - x, LV_TEXT_ALIGN_LEFT, t.texte_2);
        return;
    }
    icones::meteo(c.pc.couche, icone_pour_code(m.code, m.jour), icone, x_icone, y_icone, t);
    char temp[24];
    fmt_temp(temp, sizeof temp, m.temp_dix);
    const int x_temp = c.x + px(80);
    dz::texte_par_les_chiffres(c.pc, g.temperature, temp, x_temp, c.y + px(8), c.x + c.w - x_temp, LV_TEXT_ALIGN_LEFT,
                               connu(m.temp_dix) ? couleur_temperature(m.temp_dix, t.rampe) : t.texte);
    // « Éclaircies · ressenti 9° » : le ressenti saute quand la place manque, le libellé est alors coupé s'il le faut.
    const int x_texte = x_temp + dz::largeur_texte(g.temperature, temp) + px(20);
    const int w = c.x + c.w - x_texte;
    if (w < px(40)) {
        return;
    }
    char libelle[64];
    libelle_meteo(libelle, sizeof libelle, m.code, m.jour);
    char phrase[128];
    std::snprintf(phrase, sizeof phrase, "%s", libelle);
    if (connu(m.ressenti_dix)) {
        char ressenti[24], mention[48], complet[128];
        fmt_temp(ressenti, sizeof ressenti, m.ressenti_dix);
        std::snprintf(mention, sizeof mention, texte_principal::t("ressenti %s"), ressenti);
        std::snprintf(complet, sizeof complet, "%s%s%s", libelle, POINT_MEDIAN, mention);
        if (dz::largeur_texte(g.mini, complet) <= w) {
            std::snprintf(phrase, sizeof phrase, "%s", complet);
        }
    }
    const Ajuste a = ajuster({&g.mini}, phrase, w);
    // La ligne de base du texte est à 43 px de référence sous le haut de la zone.
    dz::texte_par_les_chiffres(c.pc, *a.police, a.texte, x_texte, c.y + px(43) - dz::hauteur_chiffres(*a.police), w,
                               LV_TEXT_ALIGN_LEFT, t.texte_2);
}

// ---- zone : la courbe des heures ----

void dessiner_heures_accueil(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const VueMeteo &m = g.e.meteo;
    carte_de_verre(c, px(28));
    const int debut = premiere_heure();
    const int n = std::min(COLONNES_HEURES, m.nb_heures - debut);
    if (!m.connue || n <= 0) { // pas de prévision heure par heure : la carte le dit par un tiret
        texte_ajuste(c.pc, {&g.titre}, TIRET, c.x, c.y + c.h / 2, c.w, LV_TEXT_ALIGN_CENTER, t.texte_3);
        return;
    }
    const HeurePrevue *h = m.heures + debut;
    const int pad_x = px(20);
    const int largeur = c.w - 2 * pad_x;
    const int pas = largeur / n;
    const auto x_col = [&](int i) { return c.x + pad_x + largeur * (2 * i + 1) / (2 * n); };

    int temps[COLONNES_HEURES];
    int pluie_max_dix = 0;
    for (int i = 0; i < n; ++i) {
        temps[i] = h[i].temp_dix;
        if (connu(h[i].pluie_dix_mm) && h[i].pluie_dix_mm > pluie_max_dix) {
            pluie_max_dix = h[i].pluie_dix_mm;
        }
    }
    const bool a_pluie = pluie_max_dix > 0;
    const int y_icones = c.y + px(33); // centre des icônes
    const int y_haut = c.y + px(96);   // ordonnée du maximum
    const int y_pied = c.y + px(256);  // pied des barres de pluie et du voile
    // Ordonnée du minimum : sous le point, son chiffre (36 px) doit rester au-dessus des étiquettes d'heures (286), pluie ou non.
    const int y_bas = c.y + (a_pluie ? px(202) : px(208));
    const int y_etiquette = c.y + px(286); // ligne de base des heures

    // La colonne de l'heure en cours, teintée d'accent.
    dz::rect(c.pc, c.x + pad_x + px(1), c.y + px(14), pas - px(2), px(276), px(14), t.accent, 31);

    // Les points de la courbe : les heures dont la température est connue.
    accueil::Pt pts[COLONNES_HEURES];
    std::uint32_t teintes[COLONNES_HEURES];
    int k = 0;
    int bas_dix = 0, haut_dix = 0;
    for (int i = 0; i < n; ++i) {
        if (!connu(temps[i])) {
            continue;
        }
        if (k == 0 || temps[i] < bas_dix) {
            bas_dix = temps[i];
        }
        if (k == 0 || temps[i] > haut_dix) {
            haut_dix = temps[i];
        }
        pts[k] = {x_col(i), 0};
        teintes[k] = couleur_temperature(temps[i], t.rampe);
        ++k;
    }
    for (int i = 0, j = 0; i < n; ++i) {
        if (connu(temps[i])) {
            pts[j++].y = accueil::ordonnee_de(temps[i], bas_dix, haut_dix, y_haut, y_bas);
        }
    }
    const accueil::CourbeMonotone courbe = accueil::courbe_monotone(pts, k);

    // Le voile sous la courbe : de la teinte moyenne, opaque à 34 % en haut, nul au pied.
    if (k >= 2) {
        int somme = 0;
        for (int i = 0; i < n; ++i) {
            if (connu(temps[i])) {
                somme += temps[i];
            }
        }
        const std::uint32_t moyenne = couleur_temperature(somme / k, t.rampe);
        int y_min = y_pied;
        for (int j = 0; j < k; ++j) {
            y_min = std::min(y_min, pts[j].y);
        }
        const DegradeGlobal d = {moyenne, moyenne, 87, 0, y_min, y_pied};
        // Le « bas » du voile est une droite au pied : une courbe à deux points à la même ordonnée.
        const accueil::Pt droite[2] = {{pts[0].x, y_pied}, {pts[k - 1].x, y_pied}};
        voile_entre(c.pc, courbe, accueil::courbe_monotone(droite, 2), pts[0].x, pts[k - 1].x, std::max(3, px(8)), d);
    }

    // Les barres de pluie : arrondies, de hauteur en racine carrée de la pluie, teintées selon l'intensité.
    int hauteur_barre[COLONNES_HEURES] = {};
    if (a_pluie) {
        const int largeur_barre = std::min(px(20), pas / 2);
        for (int i = 0; i < n; ++i) {
            if (!connu(h[i].pluie_dix_mm) || h[i].pluie_dix_mm <= 0) {
                continue;
            }
            // `gelule_du_jour` donne ici la HAUTEUR (de 8 à 44 px de référence) : la même racine carrée que la largeur des gélules des jours.
            const accueil::Gelule b = accueil::gelule_du_jour(
                h[i].pluie_dix_mm, accueil::echelle_pluie(pluie_max_dix, accueil::PLANCHER_ECHELLE_HEURE_DIX), px(8),
                px(44), 0, 0, 0);
            hauteur_barre[i] = b.largeur;
            dz::rect(c.pc, x_col(i) - largeur_barre / 2, y_pied - b.largeur, largeur_barre, b.largeur,
                     largeur_barre / 2, accueil::couleur_niveau_pluie(t, b.niveau));
        }
    }

    if (k >= 2) {
        tracer_courbe(c.pc, courbe, teintes, std::max(2, px(5)), std::max(3, px(6)));
    }

    // Les icônes du temps de chaque heure.
    const int cote_icone = px(38);
    for (int i = 0; i < n; ++i) {
        icones::meteo(c.pc.couche, icone_pour_code(h[i].code, h[i].jour), cote_icone, x_col(i) - cote_icone / 2,
                      y_icones - cote_icone / 2, t);
    }

    // Le maximum (au-dessus de son point) et le minimum (en dessous, sauf si la barre de pluie de sa colonne y monte : au-dessus).
    const accueil::Extremes ex = accueil::extremes_de(temps, n);
    const auto etiquette = [&](int i, bool en_haut) {
        const int x = x_col(i);
        const int y = accueil::ordonnee_de(temps[i], bas_dix, haut_dix, y_haut, y_bas);
        point_de_courbe(c.pc, t, x, y, px(10), px(6));
        char s[24];
        fmt_temp(s, sizeof s, temps[i]);
        if (!en_haut && hauteur_barre[i] > 0 && y_pied - hauteur_barre[i] < y + px(36) + px(4)) {
            en_haut = true; // sinon le chiffre s'écrirait sur la barre
        }
        const int base = en_haut ? y - px(18) : y + px(36);
        texte_base(c.pc, g.mini, s, x - px(40), base, px(80), LV_TEXT_ALIGN_CENTER,
                   couleur_temperature(temps[i], t.rampe));
    };
    if (ex.i_max >= 0) {
        etiquette(ex.i_max, true);
    }
    if (ex.i_min >= 0) {
        etiquette(ex.i_min, false);
    }

    // Une heure sur quatre porte son étiquette, la première en accent.
    const bool francais = en_francais();
    for (int i = 0; i < n; ++i) {
        if (!accueil::est_repere_heure(i, n)) {
            continue;
        }
        char libelle[16];
        ecran::heure::formater_etiquette_heure(heure_du(h[i].instant_s), francais, libelle, sizeof libelle);
        texte_base(c.pc, g.mini, libelle, x_col(i) - px(40), y_etiquette, px(80), LV_TEXT_ALIGN_CENTER,
                   i == 0 ? t.accent : t.texte_3);
    }
}

// ---- zone : le bandeau (la pluie, l'alerte) ----

// Le bandeau est fait de deux moitiés qui gardent chacune leur place : la pluie (ou la mention des données périmées) à gauche,
// l'alerte à droite.
Rect moitie_du_bandeau(bool droite) {
    const Rect &r = g.rect[Z_BANDEAU];
    const int ecart = px(16);
    const int w = (r.w - ecart) / 2;
    return droite ? Rect{r.x + w + ecart, r.y, r.w - w - ecart, r.h} : Rect{r.x, r.y, w, r.h};
}

// Verre teinté d'une puce : le fond à 20 %, le contour à 65 % de sa couleur (`opa_*` en 0 à 255).
void fond_de_puce(const dz::Pinceau &pc, int x, int y, int w, int h, std::uint32_t rgb, std::uint8_t opa_fond,
                  std::uint8_t opa_bord) {
    dz::rect_bord(pc, x, y, w, h, h / 2, rgb, opa_fond, rgb, opa_bord, std::max(1, px(2)));
}

// L'icône d'une puce : 30 px de référence, centrée dans le carré de gauche de la puce.
void icone_de_puce(const dz::Pinceau &pc, Piece piece, int x, int y, int h, std::uint32_t rgb) {
    const int cote = px(30);
    icones::piece(pc.couche, piece, cote, x + h / 2 - cote / 2, y + h / 2 - cote / 2, rgb);
}

// Une coche dans un rond (« rien à signaler ») : la police n'a pas ce signe, on le trace.
void coche_dans_rond(const dz::Pinceau &pc, int cx, int cy, int rayon, std::uint32_t rgb) {
    const int e = std::max(2, px(2));
    dz::anneau(pc, cx, cy, rayon, e, rgb);
    dz::trait(pc, cx - rayon * 45 / 100, cy + rayon * 5 / 100, cx - rayon * 10 / 100, cy + rayon * 40 / 100, e, rgb);
    dz::trait(pc, cx - rayon * 10 / 100, cy + rayon * 40 / 100, cx + rayon * 50 / 100, cy - rayon * 35 / 100, e, rgb);
}

// Une puce complète : verre teinté, icône, texte (coupé par « … » s'il le faut).
void puce(const dz::Pinceau &pc, const ThemePrincipal &t, const Rect &r, std::uint32_t rgb, std::uint8_t opa_fond,
          std::uint8_t opa_bord, Piece icone, const char *texte) {
    fond_de_puce(pc, r.x, r.y, r.w, r.h, rgb, opa_fond, opa_bord);
    icone_de_puce(pc, icone, r.x, r.y, r.h, rgb);
    const int x_texte = r.x + r.h + px(4);
    texte_ajuste(pc, {&g.petit, &g.mini}, texte, x_texte, r.y + r.h / 2, r.x + r.w - px(24) - x_texte,
                 LV_TEXT_ALIGN_LEFT, t.texte);
}

// « Pluie dès 13 h · 6,5 mm », « Pluie dans 25 min · 2,1 mm », « Pluie en cours · 3,2 mm » (le cumul s'il est connu et demandé).
void texte_de_pluie(char *sortie, std::size_t n, bool avec_cumul) {
    const accueil::EpisodePluie &e = g.pluie;
    char base[64] = {};
    switch (e.genre) {
    case accueil::GenrePluie::EN_COURS:
        std::snprintf(base, sizeof base, "%s", texte_principal::t("Pluie en cours"));
        break;
    case accueil::GenrePluie::BIENTOT:
        std::snprintf(base, sizeof base, texte_principal::t("Pluie dans %d min"), e.dans_min);
        break;
    case accueil::GenrePluie::PLUS_TARD: {
        char heure[16] = {};
        accueil::formater_heure_phrase(heure_du(e.debut_s), en_francais(), heure, sizeof heure);
        std::snprintf(base, sizeof base, texte_principal::t("Pluie dès %s"), heure);
        break;
    }
    default:
        break;
    }
    if (avec_cumul && connu(e.cumul_dix_mm) && e.cumul_dix_mm > 0) {
        char mm[16];
        fmt_dixieme(mm, sizeof mm, e.cumul_dix_mm);
        std::snprintf(sortie, n, "%s%s%s mm", base, POINT_MEDIAN, mm);
    } else {
        std::snprintf(sortie, n, "%s", base);
    }
}

void dessiner_bandeau(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const ThemePrincipal &normal = *g.theme;
    const VueMeteo &m = g.e.meteo;
    const Alerte al = alerte_du_jour();
    const accueil::ContenuBandeau cb =
        accueil::composer_bandeau(m.connue, m.perimee, g.pluie.genre, g.e.bandeau.present, al != Alerte::AUCUNE);
    const Rect gauche = moitie_du_bandeau(false);
    switch (cb.gauche) {
    case accueil::Puce::PLUIE: {
        char s[96];
        texte_de_pluie(s, sizeof s, true);
        if (dz::largeur_texte(g.mini, s) > gauche.w - gauche.h - px(4) - px(24)) {
            texte_de_pluie(s, sizeof s,
                           false); // le cumul est le détail : il part avant que la phrase soit coupée par « … »
        }
        puce(c.pc, t, gauche, t.pluie, 51, 166, Piece::parapluie, s);
        break;
    }
    case accueil::Puce::PERIMEE: {
        char hhmm[16], s[96];
        fmt_hhmm(hhmm, sizeof hhmm, m.instant_s);
        std::snprintf(s, sizeof s, texte_principal::t("Données de %s · périmées"), hhmm);
        puce(c.pc, normal, gauche, normal.avertissement, 51, 166, Piece::hors_ligne, s);
        break;
    }
    case accueil::Puce::CALME: {
        // Centrée sur toute la largeur : il n'y a rien d'autre à dire.
        const Rect r = {c.x + (c.w - gauche.w) / 2, gauche.y, gauche.w, gauche.h};
        fond_de_puce(c.pc, r.x, r.y, r.w, r.h, normal.texte_3, 26, 64);
        coche_dans_rond(c.pc, r.x + r.h / 2, r.y + r.h / 2, px(13), normal.texte_3);
        const int x_texte = r.x + r.h + px(4);
        texte_ajuste(c.pc, {&g.petit, &g.mini}, texte_principal::t("Rien à signaler dans les prochaines heures"),
                     x_texte, r.y + r.h / 2, r.x + r.w - px(24) - x_texte, LV_TEXT_ALIGN_LEFT, normal.texte);
        break;
    }
    default:
        break;
    }
    if (cb.droite == accueil::Puce::ALERTE_METEO) {
        // L'alerte déduite de la météo du jour : pas d'appui (seule l'alerte officielle ouvre la page des alertes).
        puce(c.pc, t, moitie_du_bandeau(true), couleur_alerte(t, al), 51, 166, piece_alerte(al), texte_alerte(al));
    }
    // L'alerte officielle est dessinée par l'objet g.pastille, posé sur cette moitié (il reçoit l'appui).
}

// ---- l'alerte officielle (objet g.pastille, posé sur la moitié droite du bandeau) ----

// Le texte de la puce : le titre de l'émetteur, suivi de la fenêtre « 14–22 h » quand l'alerte tient dans un seul jour. Trop long, le
// titre est coupé par « … » et la fenêtre reste (c'est elle qui dit QUAND) ; sans place pour les deux, le titre seul.
void texte_officielle(const vigie::Bandeau &b, int largeur, char *sortie, std::size_t n, const Police **police) {
    char fenetre[24] = {};
    if (b.debut_s > 0 && b.fin_s > 0) {
        accueil::formater_fenetre(heure_du(b.debut_s), heure_du(b.fin_s), en_francais(), fenetre, sizeof fenetre);
    }
    const char *liaison = fenetre[0] != '\0' ? POINT_MEDIAN : "";
    for (const Police *p : {&g.petit, &g.mini}) {
        std::snprintf(sortie, n, "%s%s%s", b.texte, liaison, fenetre);
        *police = p;
        if (dz::largeur_texte(*p, sortie) <= largeur) {
            return;
        }
    }
    const Police &p = g.mini;
    const int w_fenetre = fenetre[0] != '\0' ? dz::largeur_texte(p, liaison) + dz::largeur_texte(p, fenetre) : 0;
    if (largeur - w_fenetre < px(80)) {
        const Ajuste a = ajuster({&p}, b.texte, largeur);
        std::snprintf(sortie, n, "%s", a.texte);
        return;
    }
    const Ajuste a = ajuster({&p}, b.texte, largeur - w_fenetre);
    std::snprintf(sortie, n, "%s%s%s", a.texte, liaison, fenetre);
}

PlaceOfficielle placer_puce_officielle() {
    PlaceOfficielle p;
    const vigie::Bandeau &b = g.e.bandeau;
    const Rect &zone = g.rect[Z_BANDEAU];
    if (!b.present || zone.w <= 0 || zone.h <= 0) {
        return p;
    }
    const Rect r = moitie_du_bandeau(true);
    p.x = r.x;
    p.y = r.y;
    p.w = r.w;
    p.h = r.h;
    int d_autres = 0;
    if (b.autres > 0) {
        std::snprintf(p.autres, sizeof p.autres, "+%u", static_cast<unsigned>(b.autres));
        d_autres = std::max(px(30), dz::largeur_texte(g.mini, p.autres) + px(14));
        p.d_autres = d_autres;
        p.x_autres = p.w - px(18) - d_autres;
    }
    const int x_texte = p.h + px(4);
    const int bord = d_autres > 0 ? p.x_autres - px(10) : p.w - px(24);
    const int w_dispo = bord - x_texte;
    if (w_dispo >= px(60) && b.texte[0] != '\0') {
        const Police *police = nullptr;
        texte_officielle(b, w_dispo, p.texte, sizeof p.texte, &police);
        p.police = police;
        p.w_texte = std::min(w_dispo, dz::largeur_texte(*police, p.texte));
        p.x_texte = x_texte;
    }
    p.ok = true;
    return p;
}

// ---- zone : les cinq jours ----

// Une flèche de vent à deux triangles (l'esquisse : pointe, deux ailes, un creux) qui pointe VERS où le vent souffle.
void fleche_vent(const dz::Pinceau &pc, int cx, int cy, int taille, int angle_deg, std::uint32_t rgb) {
    const double a = angle_deg * 3.14159265358979 / 180.0;
    const double s = std::sin(a);
    const double co = std::cos(a);
    const double k = taille / 2.0;
    const auto pt = [&](double x, double y) {
        return lv_point_t{cx + static_cast<int>(std::lround((x * co - y * s) * k)),
                          cy + static_cast<int>(std::lround((x * s + y * co) * k))};
    };
    const lv_point_t pointe = pt(0.0, -1.0);
    const lv_point_t aile_d = pt(0.62, 0.85);
    const lv_point_t creux = pt(0.0, 0.42);
    const lv_point_t aile_g = pt(-0.62, 0.85);
    dz::triangle(pc, pointe.x, pointe.y, aile_d.x, aile_d.y, creux.x, creux.y, rgb);
    dz::triangle(pc, pointe.x, pointe.y, creux.x, creux.y, aile_g.x, aile_g.y, rgb);
}

void dessiner_jours(const Ctx &c) {
    const ThemePrincipal &t = theme_meteo();
    const VueMeteo &m = g.e.meteo;
    carte_de_verre(c, px(28));
    const int debut = premier_jour();
    const int n = std::min(COLONNES_JOURS, m.nb_jours - debut);
    if (n <= 0) {
        return;
    }
    const JourPrevu *jours = m.jours + debut;
    const std::int64_t numero_aujourdhui = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());

    // Les rangées : leurs ordonnées de référence (sous le haut de la carte) croissent avec la hauteur gagnée sur 720 (la 7B : 30 px),
    // et le surplus au-delà de 90 px (portrait) est réparti en haut et en bas.
    const int gagne = std::min(g.extra_h, 90);
    const int marge_haut = g.extra_h > 90 ? (g.extra_h - 90) / 2 : 0;
    const auto rang = [&](int base, int par_30) { return c.y + px(base + par_30 * gagne / 30 + marge_haut); };
    const int y_icone = rang(36, 2);       // centre de la rangée « nom + icône »
    const int y_temps = rang(97, 6);       // ligne de base de « 14° / 8° »
    const int y_ruban_haut = rang(113, 8); // le ruban : les maximales...
    const int y_ruban_bas = rang(170, 20); // ... et les minimales
    const int y_vent = rang(200, 24);      // centre de la rangée du vent
    const int y_pluie = rang(236, 30);     // centre de la rangée de la pluie

    const int x0 = c.x + px(24);
    const int largeur = c.w - 2 * px(24);
    const int pas = largeur / n;
    const auto x_col = [&](int i) { return x0 + largeur * (2 * i + 1) / (2 * n); };

    // Aujourd'hui : la colonne teintée d'accent, comme l'heure en cours de la courbe d'heures.
    if (numero_du_jour_prevu(jours[0]) == numero_aujourdhui) {
        dz::rect(c.pc, x_col(0) - pas / 2 + px(4), c.y + px(8), pas - px(8), c.h - px(16), px(20), t.accent, 26);
    }

    // Le ruban entre les maximales (courbe du haut) et les minimales (courbe du bas), sur une échelle commune.
    accueil::Pt p_haut[COLONNES_JOURS], p_bas[COLONNES_JOURS];
    std::uint32_t t_haut[COLONNES_JOURS], t_bas[COLONNES_JOURS];
    int nb = 0;
    int plus_bas_dix = 0, plus_haut_dix = 0;
    int somme_haut = 0, somme_bas = 0;
    for (int i = 0; i < n; ++i) {
        if (!connu(jours[i].max_dix) || !connu(jours[i].min_dix)) {
            continue;
        }
        if (nb == 0 || jours[i].min_dix < plus_bas_dix) {
            plus_bas_dix = jours[i].min_dix;
        }
        if (nb == 0 || jours[i].max_dix > plus_haut_dix) {
            plus_haut_dix = jours[i].max_dix;
        }
        somme_haut += jours[i].max_dix;
        somme_bas += jours[i].min_dix;
        p_haut[nb] = {x_col(i), 0};
        p_bas[nb] = {x_col(i), 0};
        t_haut[nb] = couleur_temperature(jours[i].max_dix, t.rampe);
        t_bas[nb] = couleur_temperature(jours[i].min_dix, t.rampe);
        ++nb;
    }
    for (int i = 0, j = 0; i < n; ++i) {
        if (connu(jours[i].max_dix) && connu(jours[i].min_dix)) {
            p_haut[j].y =
                accueil::ordonnee_de(jours[i].max_dix, plus_bas_dix, plus_haut_dix, y_ruban_haut, y_ruban_bas);
            p_bas[j].y = accueil::ordonnee_de(jours[i].min_dix, plus_bas_dix, plus_haut_dix, y_ruban_haut, y_ruban_bas);
            ++j;
        }
    }
    if (nb >= 2) {
        const accueil::CourbeMonotone c_haut = accueil::courbe_monotone(p_haut, nb);
        const accueil::CourbeMonotone c_bas = accueil::courbe_monotone(p_bas, nb);
        const DegradeGlobal d = {couleur_temperature(somme_haut / nb, t.rampe),
                                 couleur_temperature(somme_bas / nb, t.rampe),
                                 87,
                                 31,
                                 y_ruban_haut,
                                 y_ruban_bas};
        voile_entre(c.pc, c_haut, c_bas, p_haut[0].x, p_haut[nb - 1].x, std::max(3, px(8)), d);
        tracer_courbe(c.pc, c_haut, t_haut, std::max(2, px(5)), std::max(3, px(8)));
        tracer_courbe(c.pc, c_bas, t_bas, std::max(2, px(5)), std::max(3, px(8)));
    }
    for (int j = 0; j < nb; ++j) {
        point_de_courbe(c.pc, t, p_haut[j].x, p_haut[j].y, px(9), px(5));
        point_de_courbe(c.pc, t, p_bas[j].x, p_bas[j].y, px(9), px(5));
    }

    int pluie_max_dix = 0;
    for (int i = 0; i < n; ++i) {
        if (connu(jours[i].pluie_dix_mm) && jours[i].pluie_dix_mm > pluie_max_dix) {
            pluie_max_dix = jours[i].pluie_dix_mm;
        }
    }

    for (int i = 0; i < n; ++i) {
        const JourPrevu &j = jours[i];
        const int cx = x_col(i);
        const bool aujourdhui = numero_du_jour_prevu(j) == numero_aujourdhui;

        // Le nom à gauche, l'icône à droite, le tout centré sur la colonne.
        char nom[24];
        jour_court(i18n::langue_courante(), ecran::heure::jour_semaine_du_jour(j.date_s, fuseau()), nom, sizeof nom);
        if (nom[0] >= 'a' && nom[0] <= 'z') {
            nom[0] = static_cast<char>(nom[0] - 'a' +
                                       'A'); // « Dim », comme « Fre » et « Cum » (la table donne « dim » en français)
        }
        const int cote = px(52);
        const int w_nom = std::min(dz::largeur_texte(g.petit, nom), std::max(px(20), pas - cote - px(22)));
        const int xg = cx - (w_nom + px(14) + cote) / 2;
        texte_base(c.pc, g.petit, nom, xg, y_icone + px(10), w_nom + px(4), LV_TEXT_ALIGN_LEFT,
                   aujourdhui ? t.accent : t.texte_2);
        icones::meteo(c.pc.couche, icone_pour_code(j.code, true), cote, xg + w_nom + px(14), y_icone - cote / 2, t);

        // « 14° / 8° » : la maximale grande, la minimale petite, chacune de la couleur de sa température.
        char haut[24], bas[24];
        fmt_temp(haut, sizeof haut, j.max_dix);
        fmt_temp(bas, sizeof bas, j.min_dix);
        const Police *p_grand = &g.corps;
        const Police *p_petit = &g.petit;
        const int sep = px(10);
        int w_total = dz::largeur_texte(*p_grand, haut) + 2 * sep + dz::largeur_texte(*p_petit, "/") +
                      dz::largeur_texte(*p_petit, bas);
        if (w_total > pas - px(8)) { // colonne étroite : une taille de moins
            p_grand = &g.petit;
            p_petit = &g.mini;
            w_total = dz::largeur_texte(*p_grand, haut) + 2 * sep + dz::largeur_texte(*p_petit, "/") +
                      dz::largeur_texte(*p_petit, bas);
        }
        int x = cx - w_total / 2;
        texte_base(c.pc, *p_grand, haut, x, y_temps, dz::largeur_texte(*p_grand, haut) + px(4), LV_TEXT_ALIGN_LEFT,
                   connu(j.max_dix) ? couleur_temperature(j.max_dix, t.rampe) : t.texte_3);
        x += dz::largeur_texte(*p_grand, haut) + sep;
        texte_base(c.pc, *p_petit, "/", x, y_temps, dz::largeur_texte(*p_petit, "/") + px(4), LV_TEXT_ALIGN_LEFT,
                   t.filet);
        x += dz::largeur_texte(*p_petit, "/") + sep;
        texte_base(c.pc, *p_petit, bas, x, y_temps, dz::largeur_texte(*p_petit, bas) + px(4), LV_TEXT_ALIGN_LEFT,
                   connu(j.min_dix) ? couleur_temperature(j.min_dix, t.rampe) : t.texte_3);

        // Le vent : la flèche (vers où il souffle) et sa force.
        if (connu(j.vent_dix_kmh)) {
            char vitesse[24];
            std::snprintf(vitesse, sizeof vitesse, "%d km/h", arrondi_dix(j.vent_dix_kmh));
            const int fleche = px(30);
            const bool avec_fleche = j.direction_deg >= 0;
            const int w_fleche = avec_fleche ? fleche + px(10) : 0;
            if (w_fleche + dz::largeur_texte(g.mini, vitesse) > pas - px(8)) {
                std::snprintf(vitesse, sizeof vitesse, "%d",
                              arrondi_dix(j.vent_dix_kmh)); // colonne étroite (portrait) : sans l'unité
            }
            const int w_vitesse = dz::largeur_texte(g.mini, vitesse);
            const int w_tout = w_fleche + w_vitesse;
            int xv = cx - w_tout / 2;
            if (avec_fleche) {
                fleche_vent(c.pc, xv + fleche / 2, y_vent, fleche, angle_fleche_vent(j.direction_deg),
                            couleur_metrique(t, Metrique::VENT));
                xv += fleche + px(10);
            }
            texte_base(c.pc, g.mini, vitesse, xv, y_vent + px(8), w_vitesse + px(4), LV_TEXT_ALIGN_LEFT, t.texte_2);
        }

        // La pluie du jour : une gélule (largeur en racine carrée de la pluie) ; le chiffre dedans seulement s'il y tient ; un petit
        // trait pour un jour sec ; rien quand la pluie est inconnue.
        if (connu(j.pluie_dix_mm) && j.pluie_dix_mm > 0) {
            char mm[24];
            fmt_dixieme(mm, sizeof mm, j.pluie_dix_mm);
            std::snprintf(mm + std::strlen(mm), sizeof mm - std::strlen(mm), " mm");
            const accueil::Gelule gl = accueil::gelule_du_jour(
                j.pluie_dix_mm, accueil::echelle_pluie(pluie_max_dix, accueil::PLANCHER_ECHELLE_JOUR_DIX), px(26),
                std::min(px(138), pas - px(8)), dz::largeur_texte(g.mini, mm), px(16), pas - px(8));
            const int h_gelule = px(30);
            dz::rect(c.pc, cx - gl.largeur / 2, y_pluie - h_gelule / 2, gl.largeur, h_gelule, h_gelule / 2,
                     accueil::couleur_niveau_pluie(t, gl.niveau));
            if (gl.avec_texte) {
                texte_base(c.pc, g.mini, mm, cx - gl.largeur / 2, y_pluie + px(9), gl.largeur, LV_TEXT_ALIGN_CENTER,
                           t.texte_sur_couleur);
            }
        } else if (connu(j.pluie_dix_mm)) {
            dz::rect(c.pc, cx - px(13), y_pluie - px(4), px(26), px(8), px(4), t.texte, 36);
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
    jour_court(i18n::langue_courante(), ecran::heure::jour_semaine_du_jour(j.date_s, fuseau()), sortie, n);
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
    const std::int64_t numero_aujourdhui = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());
    const int pad = px(10);
    const bool lignes = g.portrait; // portrait : une ligne par jour ; paysage : une colonne par jour
    const int h_barre = px(8);
    // Contenu d'une colonne, pour le centrer dans la carte.
    const int h_icone = px(44);
    const int h_colonne =
        dz::hauteur_ligne(g.petit) + px(2) + h_icone + px(2) + dz::hauteur_ligne(g.corps) + px(6) + h_barre;
    for (int i = 0; i < n; ++i) {
        const JourPrevu &j = m.jours[debut + i];
        const bool aujourdhui = numero_du_jour_prevu(j) == numero_aujourdhui;
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
        if (g.est_accueil) {
            dessiner_barre_accueil(c);
        } else {
            dessiner_barre(c);
        }
        break;
    case Z_HORLOGE:
        if (g.est_accueil) {
            dessiner_horloge_accueil(c);
        } else {
            dessiner_horloge(c);
        }
        break;
    case Z_MAINTENANT:
        if (g.est_accueil) {
            dessiner_maintenant_accueil(c);
        } else {
            dessiner_maintenant(c);
        }
        break;
    case Z_HEURES:
        dessiner_heures_accueil(c);
        break;
    case Z_BANDEAU:
        dessiner_bandeau(c);
        break;
    case Z_SEMAINE:
        if (g.est_accueil) {
            dessiner_jours(c);
        } else {
            dessiner_semaine(c);
        }
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
    if (g.est_accueil) {
        // Dans le bandeau : la même puce que les autres (verre teinté du niveau), jamais grisée : un avis en vigueur reste un avis.
        const std::uint32_t rgb = couleur_officielle(t, static_cast<int>(g.e.bandeau.niveau));
        fond_de_puce(pc, a.x1, a.y1, p.w, p.h, rgb, 51, 166);
        if (lv_obj_has_state(objet, LV_STATE_PRESSED)) {
            dz::rect(pc, a.x1, a.y1, p.w, p.h, p.h / 2, t.texte, 40);
        }
        icone_de_puce(pc, Piece::alerte, a.x1, a.y1, p.h, rgb);
        if (p.w_texte > 0 && p.police != nullptr) {
            dz::texte(pc, *p.police, p.texte, a.x1 + p.x_texte, centre - dz::hauteur_ligne(*p.police) / 2,
                      p.w_texte + px(4), LV_TEXT_ALIGN_LEFT, t.texte);
        }
        if (p.d_autres > 0) {
            dz::disque(pc, a.x1 + p.x_autres + p.d_autres / 2, centre, p.d_autres / 2, rgb);
            dz::texte(pc, g.mini, p.autres, a.x1 + p.x_autres, centre - dz::hauteur_ligne(g.mini) / 2, p.d_autres,
                      LV_TEXT_ALIGN_CENTER, t.texte_sur_couleur);
        }
        return;
    }
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

    // Tailles de référence. L'accueil : l'heure 168, la température du moment 54, la date 36, le texte des jours 32 et 28, le plus petit
    // texte 24 (19 px réels sur la 7B). L'horloge en vedette : l'heure 270 px (250 en portrait, 664 px de large) et la température 124.
    g.est_accueil = theme.disposition != Disposition::HORLOGE;
    g.theme_gris = accueil::theme_grise(theme);
    if (g.est_accueil) {
        g.heure = creer_police(ressources.ttf, ressources.ttf_taille, 168);
        g.temperature = creer_police(ressources.ttf, ressources.ttf_taille, 54);
        g.titre = creer_police(ressources.ttf, ressources.ttf_taille, 36);
        g.corps = creer_police(ressources.ttf, ressources.ttf_taille, 32);
        g.petit = creer_police(ressources.ttf, ressources.ttf_taille, 28);
        g.mini = creer_police(ressources.ttf, ressources.ttf_taille, 24);
    } else {
        g.heure = creer_police(ressources.ttf, ressources.ttf_taille, g.portrait ? 250 : 270);
        g.temperature = creer_police(ressources.ttf, ressources.ttf_taille, g.portrait ? 128 : 124);
        g.titre = creer_police(ressources.ttf, ressources.ttf_taille, 42);
        g.corps = creer_police(ressources.ttf, ressources.ttf_taille, 34);
        g.petit = creer_police(ressources.ttf, ressources.ttf_taille, 28);
        g.mini = creer_police(ressources.ttf, ressources.ttf_taille, 23);
    }
    if (g.heure.f == nullptr || g.temperature.f == nullptr || g.titre.f == nullptr || g.corps.f == nullptr ||
        g.petit.f == nullptr || g.mini.f == nullptr) {
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

    const VueMeteo &m = g.e.meteo;
    // La pluie à annoncer : les cumuls horaires, recalés sur la pluie proche au quart d'heure, bornés aux 12 prochaines heures.
    g.pluie = accueil::dans_l_horizon(
        accueil::episode_de_pluie(m.heures, m.nb_heures, g.e.maintenant_s, m.pluie_dans_min), g.e.maintenant_s);

    bool tout = false;
    const int langue = i18n::indice(i18n::langue_courante());
    const auto base = [&](Empreinte &e) {
        e.u(static_cast<std::uint32_t>(langue));
        e.u(m.connue);
        e.u(m.perimee); // une météo périmée est dessinée grisée
    };
    // Le lieu change de fuseau (autre ville choisie sur l'écran ou sur la page web) : les heures, les jours et le soleil se redessinent.
    const std::uint32_t cle_fuseau = ecran::heure::cle_du_fuseau(fuseau());
    std::uint32_t neuf[Z_NOMBRE];
    {
        Empreinte e;
        base(e);
        e.s(m.ville);
        e.u(m.exemple);
        if (!g.est_accueil) { // la composition de l'horloge dit encore l'alerte et la fraîcheur dans sa barre
            e.u(static_cast<std::uint32_t>(alerte_du_jour()));
            e.u(g.e.bandeau.present);
            e.u(g.e.bandeau.empreinte); // texte, niveau et nombre d'autres alertes (noyau : vigie::composer_bandeau)
        }
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
        e.u(cle_fuseau); // l'étiquette de chaque colonne est l'heure locale de son instant
        e.u(premiere_heure());
        for (int i = 0; i < m.nb_heures; ++i) {
            e.i(m.heures[i].instant_s);
            e.u(m.heures[i].temp_dix);
            e.u(m.heures[i].code);
            e.u(m.heures[i].jour);
            e.u(m.heures[i].pluie_dix_mm);
        }
        neuf[Z_HEURES] = e.h;
    }
    {
        // Le bandeau : ce que dit la pluie (genre, minutes, heure de début, cumul), l'heure du relevé périmé, l'alerte déduite, l'alerte
        // officielle (dessinée par la pastille, que le changement de ce bandeau repositionne).
        Empreinte e;
        base(e);
        e.u(cle_fuseau);
        e.u(static_cast<std::uint32_t>(g.pluie.genre));
        e.u(g.pluie.genre == accueil::GenrePluie::BIENTOT ? g.pluie.dans_min : 0); // « dans N min » change à la minute
        e.i(g.pluie.genre == accueil::GenrePluie::PLUS_TARD ? g.pluie.debut_s / 60 : 0);
        e.u(g.pluie.cumul_dix_mm);
        e.i(m.perimee ? m.instant_s / 60 : 0); // « Données de 07:52 »
        e.u(static_cast<std::uint32_t>(alerte_du_jour()));
        e.u(g.e.bandeau.present);
        e.u(g.e.bandeau.empreinte);
        neuf[Z_BANDEAU] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(cle_fuseau); // les noms des jours sont ceux du lieu
        e.u(premier_jour());
        // « aujourd'hui » change à minuit LOCAL, pas à une heure ronde de l'UTC (Kolkata : 18 h 30 UTC) : on range le numéro du jour local
        e.i(ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau()));
        for (int i = 0; i < m.nb_jours; ++i) {
            e.i(m.jours[i].date_s);
            e.u(m.jours[i].code);
            e.u(m.jours[i].min_dix);
            e.u(m.jours[i].max_dix);
            e.u(m.jours[i].proba_pct);
            e.u(m.jours[i].pluie_dix_mm);
            e.u(m.jours[i].vent_dix_kmh);
            e.u(m.jours[i].direction_deg);
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
            if (z == (g.est_accueil ? Z_BANDEAU : Z_BARRE)) {
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
    // L'horloge en vedette a ses propres polices et sa propre disposition (créées à la construction) : pas de bascule à chaud.
    if ((theme.disposition == Disposition::HORLOGE) != (g.theme->disposition == Disposition::HORLOGE)) {
        return false;
    }
    g.theme = &theme;
    g.theme_gris = accueil::theme_grise(theme);
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

CoutEcran ui_principal_cout() { return {g.objets, 6}; }

} // namespace ecran
