// Noyau pur : ce que la zone du bas de l'accueil devient quand le mode automatique ajoute quelque chose (ADR-0015 règles 1 et 2, ADR-0023) :
//   - la CARTE PLUIE (pluie dans moins de 60 min, hors épinglage) : elle recouvre la carte des cinq jours, avec le compte à rebours, la
//     force de la pluie, sa durée, son cumul et les barres des prochaines heures ;
//   - le BANDEAU D'ALERTE AGRANDI (alerte orange ou rouge en vigueur) : une grande carte d'alerte, une petite tuile de pluie à sa droite
//     et les cinq jours réduits à une bande en bas.
// Ce fichier calcule les rectangles (en pixels de RÉFÉRENCE, comme `accueil::disposer`) et le contenu de la carte pluie (des nombres et
// des énumérations, jamais du texte ni une couleur) ; le dessin est dans main/ui_principal.cpp. C++17 standard, aucun include ESP-IDF ni
// LVGL, évaluable à la compilation (test_accueil_cartes.cpp, mutations_accueil_cartes.py).
//
// SI LES DEUX SONT VRAIS ENSEMBLE (alerte orange ou rouge ET pluie proche), la composition est celle de l'ALERTE : une alerte officielle ne
// cède jamais sa place, et la pluie reste dite par la petite tuile (« Pluie dans 12 min · 6,5 mm »). La pastille, elle, dit déjà l'alerte
// d'abord (ADR-0015). C'est un choix d'interface, signalé à Axel (ADR-0023, « À valider par Axel »), pas une règle de `decider()`.
//
// Les nombres de ce fichier (92 px de bande, 360 px de tuile, 16 px d'écart, neuf colonnes) sont CHOISIS par l'auteur d'après la
// description des esquisses `auto-pluie` et `auto-alerte` : ces images n'ont pas été retrouvées (ni dans le dépôt ni dans les notes de la
// session), donc rien n'a été mesuré dessus. Une proposition, jugée sur l'écran réel.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/accueil.hpp"
#include "ecran/accueil_pilote.hpp"
#include "ecran/visuel_vue.hpp"

namespace ecran::accueil {

// ---- la composition ----------------------------------------------------------------------------------------------------------------

enum class Composition : std::uint8_t {
    Normale,       // l'accueil d'avant : bandeau (pluie à gauche, alerte à droite) et cinq jours
    CartePluie,    // la carte pluie remplace les cinq jours
    AlerteAgrandie // la grande carte d'alerte, la tuile de pluie, les cinq jours en bande
};

// L'alerte passe avant la pluie : ensemble, c'est la composition de l'alerte (voir l'en-tête).
constexpr Composition composition_de(const Presentation &p) noexcept {
    if (p.bandeau_agrandi)
        return Composition::AlerteAgrandie;
    if (p.carte_pluie)
        return Composition::CartePluie;
    return Composition::Normale;
}

// ---- les rectangles -----------------------------------------------------------------------------------------------------------------

inline constexpr int ECART_CARTES = 16;   // entre la carte d'alerte, la tuile et la bande des jours
inline constexpr int BANDE_JOURS_H = 92;  // les cinq jours réduits à une bande : nom, icône, maximum
inline constexpr int TUILE_PLUIE_W = 360; // la tuile de pluie à droite de la carte d'alerte (paysage)
inline constexpr int TUILE_PLUIE_H = 88;  // la tuile de pluie sous la carte d'alerte (portrait)
inline constexpr int ALERTE_H_MIN =
    150; // sous cette hauteur la grande carte d'alerte n'a plus la place de ses trois lignes

struct ZonesAuto {
    Rect bandeau;              // la zone du bandeau (Z_BANDEAU) : vide (largeur 0) quand rien n'y est dit
    Rect bas;                  // la zone du bas (Z_SEMAINE) : les cinq jours, la carte pluie, ou la bande des jours
    Rect alerte;               // où l'alerte officielle est posée (l'objet `pastille`) : vide s'il n'y a pas d'alerte
    Rect tuile;                // la tuile de pluie de la composition d'alerte : vide s'il n'y a rien à dire de la pluie
    bool bas_en_bande = false; // `bas` est la bande réduite des cinq jours
    bool bas_en_carte_pluie = false;
};

namespace detail {

constexpr bool vide(const Rect &r) noexcept { return r.w <= 0 || r.h <= 0; }

} // namespace detail

// Les zones d'une composition. `w` x `h` : l'écran en pixels de référence (comme `disposer`). `alerte` : une alerte officielle (ou
// déduite de la météo) occupe le bandeau. `tuile` : la composition d'alerte a quelque chose à dire de la pluie (ou des données
// périmées) ; sinon la carte d'alerte prend toute la largeur. La composition NORMALE n'est pas calculée ici : c'est `disposer`, que
// l'écran garde tel quel (pixel pour pixel).
constexpr ZonesAuto disposer_auto(int w, int h, bool portrait, Composition composition, bool alerte,
                                  bool tuile) noexcept {
    ZonesAuto a;
    const Zones z = disposer(w, h, portrait);
    switch (composition) {
    case Composition::Normale:
        a.bandeau = z.bandeau;
        a.bas = z.jours;
        return a;
    case Composition::CartePluie:
        if (alerte) {
            // L'alerte garde sa bande, en pleine largeur : la pluie est dite par la carte, pas par la moitié gauche.
            a.bandeau = z.bandeau;
            a.alerte = z.bandeau;
            a.bas = z.jours;
        } else {
            // Rien dans le bandeau : la carte monte prendre sa place (une carte plus haute, jamais un trou).
            a.bas = {z.bandeau.x, z.bandeau.y, z.bandeau.w, z.jours.y + z.jours.h - z.bandeau.y};
        }
        a.bas_en_carte_pluie = true;
        return a;
    case Composition::AlerteAgrandie:
        break;
    }
    // La bande des cinq jours garde le bas de l'écran ; la carte d'alerte et la tuile se partagent ce qui est entre le haut de la zone
    // du bandeau et cette bande.
    const int bas_ecran = z.jours.y + z.jours.h;
    a.bas = {z.jours.x, bas_ecran - BANDE_JOURS_H, z.jours.w, BANDE_JOURS_H};
    a.bas_en_bande = true;
    const int haut = z.bandeau.y;
    const int zone_h = a.bas.y - ECART_CARTES - haut;
    a.bandeau = {z.bandeau.x, haut, z.bandeau.w, zone_h};
    if (!portrait) {
        const int alerte_w = tuile ? z.bandeau.w - TUILE_PLUIE_W - ECART_CARTES : z.bandeau.w;
        a.alerte = {z.bandeau.x, haut, alerte_w, zone_h};
        if (tuile)
            a.tuile = {z.bandeau.x + alerte_w + ECART_CARTES, haut, TUILE_PLUIE_W, zone_h};
    } else {
        const int alerte_h = tuile ? zone_h - TUILE_PLUIE_H - ECART_CARTES : zone_h;
        a.alerte = {z.bandeau.x, haut, z.bandeau.w, alerte_h};
        if (tuile)
            a.tuile = {z.bandeau.x, haut + alerte_h + ECART_CARTES, z.bandeau.w, TUILE_PLUIE_H};
    }
    return a;
}

// ---- la carte pluie ------------------------------------------------------------------------------------------------------------------

inline constexpr int BARRES_MAX =
    12; // l'écran en montre neuf en paysage, huit en portrait : jamais plus que la courbe des heures

struct BarrePluie {
    std::int64_t instant_s = 0; // début de l'heure
    int dix_mm = INCONNU_DIX;   // pluie de l'heure, dixièmes de mm ; inconnue : INCONNU_DIX
    NiveauPluie niveau = NiveauPluie::FAIBLE;
};

// Ce que dit la carte pluie. `genre` : EN_COURS (« Pluie en cours ») ou BIENTOT (« Pluie dans 12 min »), les deux seuls genres qui
// font une carte (ADR-0015 : moins de 60 min).
struct CartePluie {
    bool presente = false;
    GenrePluie genre = GenrePluie::INCONNU;
    int dans_min = -1;
    int duree_h =
        0; // « environ 5 h » : 0 quand on ne sait pas (la pluie dépasse les données, ou aucune heure ne la couvre)
    int cumul_dix_mm = INCONNU_DIX; // « 6,5 mm en tout » ; inconnu ou nul : pas de puce
    bool force_connue = false;      // la force de l'heure du début est connue
    NiveauPluie force = NiveauPluie::FAIBLE;
    bool plus_forte_plus_tard = false; // une heure ultérieure du même épisode est d'un niveau plus fort
    std::int64_t pic_s = 0;            // début de cette heure (instant UTC) : « vers 15 h »
    int n = 0;                         // barres : les heures à partir de l'heure en cours
    BarrePluie barre[BARRES_MAX];
    int echelle_dix = PLANCHER_ECHELLE_HEURE_DIX; // la plus forte pluie de l'affichage, au moins 3 mm
};

// Les seuils des repères de la carte (dixièmes de mm par heure) : la pluie est « modérée » dès 1 mm, « forte » dès 3 mm (niveau_pluie).
inline constexpr int SEUIL_MODEREE_DIX = 10;
inline constexpr int SEUIL_FORTE_DIX = 30;

// La carte pluie. `episode` : celui que l'écran annonce (accueil::dans_l_horizon) ; `h`, `nh` : les cumuls horaires ; `maintenant_s` :
// l'instant du moment (UTC) ; `colonnes` : le nombre de barres que l'écran a la place de montrer (2 à BARRES_MAX).
constexpr CartePluie composer_carte_pluie(const EpisodePluie &episode, const visuel::HeurePrevue *h, int nh,
                                          std::int64_t maintenant_s, int colonnes) noexcept {
    CartePluie c;
    if (episode.genre != GenrePluie::EN_COURS && episode.genre != GenrePluie::BIENTOT)
        return c;
    c.presente = true;
    c.genre = episode.genre;
    c.dans_min = episode.dans_min;
    if (h == nullptr || nh <= 0 || maintenant_s <= 0)
        return c;
    if (colonnes > BARRES_MAX)
        colonnes = BARRES_MAX;

    // Les barres : à partir de l'heure en cours (la première dont la fin n'est pas passée).
    int premiere = nh;
    for (int i = 0; i < nh; ++i) {
        if (h[i].instant_s + 3600 > maintenant_s) {
            premiere = i;
            break;
        }
    }
    int fort = 0;
    for (int i = premiere; i < nh && c.n < colonnes; ++i) {
        BarrePluie &b = c.barre[c.n++];
        b.instant_s = h[i].instant_s;
        b.dix_mm = visuel::connu(h[i].pluie_dix_mm) ? h[i].pluie_dix_mm : INCONNU_DIX;
        b.niveau = visuel::connu(h[i].pluie_dix_mm) ? niveau_pluie(h[i].pluie_dix_mm) : NiveauPluie::FAIBLE;
        if (b.dix_mm > fort)
            fort = b.dix_mm;
    }
    c.echelle_dix = echelle_pluie(fort, PLANCHER_ECHELLE_HEURE_DIX);

    // L'épisode : l'heure qui couvre le début annoncé, puis les heures pluvieuses qui la suivent sans trou.
    int debut = -1;
    for (int i = 0; i < nh; ++i) {
        if (h[i].instant_s + 3600 > episode.debut_s && detail::heure_pluvieuse(h[i])) {
            if (h[i].instant_s < episode.debut_s + 3600)
                debut = i;
            break;
        }
    }
    if (debut >= 0) {
        c.force_connue = true;
        c.force = niveau_pluie(h[debut].pluie_dix_mm);
        int maxi = debut;
        int fin = debut;
        for (int i = debut + 1; i < nh && h[i].instant_s == h[i - 1].instant_s + 3600 && detail::heure_pluvieuse(h[i]);
             ++i) {
            fin = i;
            if (h[i].pluie_dix_mm > h[maxi].pluie_dix_mm)
                maxi = i;
        }
        if (niveau_pluie(h[maxi].pluie_dix_mm) > c.force) {
            c.plus_forte_plus_tard = true;
            c.pic_s = h[maxi].instant_s;
        }
        // La durée n'est dite que si l'épisode FINIT dans les données : une pluie qui touche la dernière heure connue dure peut-être bien plus.
        const bool coupee_par_les_donnees = fin == nh - 1;
        if (!coupee_par_les_donnees && episode.fin_s > episode.debut_s) {
            const int minutes = static_cast<int>((episode.fin_s - episode.debut_s) / 60);
            c.duree_h = (minutes + 30) / 60 < 1 ? 1 : (minutes + 30) / 60;
        }
    }
    if (visuel::connu(episode.cumul_dix_mm) && episode.cumul_dix_mm > 0)
        c.cumul_dix_mm = episode.cumul_dix_mm;
    return c;
}

// Hauteur d'une barre dans un graphique de `hauteur` pixels : proportionnelle à la pluie sur l'échelle de la carte, mais jamais moins que
// `plancher` pixels pour une pluie présente (une goutte doit se voir), jamais plus que `hauteur`. Pluie nulle ou inconnue : 0.
constexpr int hauteur_de_barre(int dix_mm, int echelle_dix, int hauteur, int plancher) noexcept {
    if (!visuel::connu(dix_mm) || dix_mm <= 0 || echelle_dix <= 0 || hauteur <= 0)
        return 0;
    const int h = static_cast<int>(static_cast<std::int64_t>(dix_mm) * hauteur / echelle_dix);
    const int borne = h < plancher ? plancher : h;
    return borne > hauteur ? hauteur : borne;
}

// Ordonnée (en pixels depuis le bas du graphique) d'un repère de la pluie, sur la même échelle que les barres.
constexpr int hauteur_du_repere(int seuil_dix, int echelle_dix, int hauteur) noexcept {
    if (echelle_dix <= 0 || hauteur <= 0)
        return 0;
    const int h = static_cast<int>(static_cast<std::int64_t>(seuil_dix) * hauteur / echelle_dix);
    return h > hauteur ? hauteur : h;
}

// Un repère est tracé seulement s'il est DANS le graphique (sur l'échelle plancher de 3 mm, « forte » est tout en haut).
constexpr bool repere_visible(int seuil_dix, int echelle_dix) noexcept { return echelle_dix >= seuil_dix; }

} // namespace ecran::accueil
