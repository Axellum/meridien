// Noyau pur : la logique de l'ACCUEIL de l'écran principal (profil Météo), celle qui se teste sans écran. C++17 standard, aucun include
// ESP-IDF ni LVGL, aucune allocation, évaluable à la compilation (test/host/test_accueil.cpp, mutations_accueil.py). Le dessin
// (main/ui_principal.cpp) ne fait que poser ce que ce fichier décide.
//
// Contenu :
//   1. l'épisode de pluie (« Pluie dès 13 h · 6,5 mm ») : à partir des cumuls horaires, recalé sur la pluie proche au quart d'heure ;
//   2. les heures et fenêtres écrites dans les phrases (« 13 h », « 14–22 h ») ;
//   3. la courbe monotone de Fritsch-Carlson en entiers : une courbe lisse qui ne dépasse jamais ses points (température des heures,
//      maximales et minimales des jours) ;
//   4. la gélule de pluie d'un jour (largeur en racine carrée, chiffre seulement s'il tient dedans) et les trois niveaux de pluie ;
//   5. les repères d'heures, les extrêmes de la courbe, l'ordonnée d'un point ;
//   6. ce que dit le bandeau (pluie, alerte, « rien à signaler », « données périmées ») ;
//   7. la mise en page des zones, en pixels de référence (1280 x 720 ; 1024 x 600 = 1280 x 750 réduit à 80 %) ;
//   8. le thème « grisé » des valeurs périmées.
// Les valeurs inconnues sont INCONNU (-1) ou INCONNU_DIX (-32768) comme dans visuel_vue.hpp : l'accueil cache ce qu'il ne connaît pas.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/heure_fr.hpp"
#include "ecran/visuel_echelles.hpp"
#include "ecran/visuel_themes.hpp"
#include "ecran/visuel_vue.hpp"

namespace ecran::accueil {

using visuel::connu;
using visuel::INCONNU;
using visuel::INCONNU_DIX;

namespace detail {

// Racine carrée entière par défaut (méthode de Newton) ; 0 pour n <= 0.
constexpr std::int64_t racine_entiere(std::int64_t n) noexcept {
    if (n <= 0)
        return 0;
    std::int64_t x = n;
    std::int64_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

// a / b arrondi au plus proche (b > 0), demi vers l'extérieur de zéro.
constexpr std::int64_t div_arrondi(std::int64_t a, std::int64_t b) noexcept {
    return (a >= 0 ? a + b / 2 : a - b / 2) / b;
}

// Petit écrivain borné, sans allocation : `trop` devient vrai si la place manque (le résultat est alors vide).
struct Tampon {
    char *sortie;
    std::size_t capacite;
    std::size_t n = 0;
    bool trop = false;
    constexpr void car(char c) noexcept {
        if (sortie == nullptr || n + 1 >= capacite) {
            trop = true;
            return;
        }
        sortie[n++] = c;
    }
    constexpr void texte(const char *s) noexcept {
        for (; *s != '\0'; ++s)
            car(*s);
    }
    constexpr void entier(int v) noexcept {
        char chiffres[12] = {};
        int k = 0;
        if (v == 0)
            chiffres[k++] = '0';
        for (; v > 0; v /= 10)
            chiffres[k++] = static_cast<char>('0' + v % 10);
        while (k > 0)
            car(chiffres[--k]);
    }
    constexpr void deux_chiffres(int v) noexcept {
        car(static_cast<char>('0' + (v / 10) % 10));
        car(static_cast<char>('0' + v % 10));
    }
    constexpr std::size_t terminer() noexcept {
        if (sortie == nullptr || capacite == 0)
            return 0;
        if (trop) {
            sortie[0] = '\0';
            return 0;
        }
        sortie[n] = '\0';
        return n;
    }
};

} // namespace detail

// ================================================================================================================ 1. pluie

// Une heure est « pluvieuse » dès 0,1 mm : en dessous, c'est de la bruine que l'accueil n'annonce pas.
inline constexpr int SEUIL_PLUIE_HEURE_DIX = 1;
// Sous cette échéance on dit « dans N min », au-delà « dès 13 h ».
inline constexpr int PLUIE_BIENTOT_MIN = 60;
// La pluie proche (quarts d'heure) couvre les deux prochaines heures : « -1 » = rien pendant ce temps.
inline constexpr int FENETRE_PLUIE_PROCHE_S = 2 * 3600;

enum class GenrePluie : std::uint8_t { INCONNU, AUCUNE, EN_COURS, BIENTOT, PLUS_TARD };

struct EpisodePluie {
    GenrePluie genre = GenrePluie::INCONNU;
    int dans_min = -1;        // minutes avant le début (0 : il pleut) ; -1 hors EN_COURS, BIENTOT, PLUS_TARD
    std::int64_t debut_s = 0; // instant UTC du début annoncé
    std::int64_t fin_s = 0;   // fin de l'épisode d'après les cumuls horaires (0 : inconnue)
    int cumul_dix_mm =
        INCONNU_DIX; // pluie de l'épisode d'après les cumuls horaires, dixièmes de mm ; inconnue : INCONNU_DIX
};

namespace detail {

// Cumule la suite des heures pluvieuses qui commence à l'indice `k` (heures contiguës dans le temps, pluie connue et >= seuil).
constexpr void cumuler_episode(const visuel::HeurePrevue *h, int n, int k, EpisodePluie &e) noexcept {
    int somme = 0;
    int i = k;
    for (; i < n; ++i) {
        if (i > k && h[i].instant_s != h[i - 1].instant_s + 3600)
            break;
        if (!connu(h[i].pluie_dix_mm) || h[i].pluie_dix_mm < SEUIL_PLUIE_HEURE_DIX)
            break;
        somme += h[i].pluie_dix_mm;
    }
    e.cumul_dix_mm = somme;
    e.fin_s = h[i - 1].instant_s + 3600;
}

constexpr bool heure_pluvieuse(const visuel::HeurePrevue &h) noexcept {
    return connu(h.pluie_dix_mm) && h.pluie_dix_mm >= SEUIL_PLUIE_HEURE_DIX;
}

} // namespace detail

// La prochaine pluie, pour la phrase « Pluie dès 13 h · 6,5 mm ».
//   `h`, `n` : cumuls horaires (ordre du temps) ; `maintenant_s` : instant UTC du moment (<= 0 : inconnu -> rien n'est dit) ;
//   `pluie_dans_min` : la pluie proche au quart d'heure (0 il pleut, > 0 minutes avant, -1 rien dans 2 h, -2 inconnue).
// La pluie proche, plus fine, fixe le DÉBUT quand elle a vu de la pluie ; quand elle dit « rien dans 2 h », les heures qui commencent
// avant cette échéance sont écartées. Le CUMUL vient toujours des heures : inconnu si aucune heure pluvieuse ne couvre ce début.
constexpr EpisodePluie episode_de_pluie(const visuel::HeurePrevue *h, int n, std::int64_t maintenant_s,
                                        int pluie_dans_min) noexcept {
    EpisodePluie e;
    if (maintenant_s <= 0 || (h == nullptr && n > 0))
        return e;
    if (pluie_dans_min >= 0) {
        e.dans_min = pluie_dans_min;
        e.debut_s = maintenant_s + static_cast<std::int64_t>(pluie_dans_min) * 60;
        e.genre = pluie_dans_min == 0
                      ? GenrePluie::EN_COURS
                      : (pluie_dans_min < PLUIE_BIENTOT_MIN ? GenrePluie::BIENTOT : GenrePluie::PLUS_TARD);
        for (int i = 0; i < n; ++i) {
            if (h[i].instant_s + 3600 > e.debut_s && detail::heure_pluvieuse(h[i])) {
                if (h[i].instant_s < e.debut_s + 3600)
                    detail::cumuler_episode(h, n, i, e);
                break;
            }
        }
        return e;
    }
    const bool sec_deux_heures = pluie_dans_min == -1;
    const std::int64_t ecarter_avant = sec_deux_heures ? maintenant_s + FENETRE_PLUIE_PROCHE_S : 0;
    bool vu = false;
    int premier = -1;
    for (int i = 0; i < n; ++i) {
        if (h[i].instant_s + 3600 <= maintenant_s || h[i].instant_s < ecarter_avant || !connu(h[i].pluie_dix_mm))
            continue;
        vu = true;
        if (detail::heure_pluvieuse(h[i])) {
            premier = i;
            break;
        }
    }
    if (premier < 0) {
        e.genre = (vu || sec_deux_heures) ? GenrePluie::AUCUNE : GenrePluie::INCONNU;
        return e;
    }
    detail::cumuler_episode(h, n, premier, e);
    if (h[premier].instant_s <= maintenant_s) {
        e.genre = GenrePluie::EN_COURS;
        e.dans_min = 0;
        e.debut_s = maintenant_s;
    } else {
        e.debut_s = h[premier].instant_s;
        e.dans_min = static_cast<int>((e.debut_s - maintenant_s + 59) / 60);
        e.genre = e.dans_min < PLUIE_BIENTOT_MIN ? GenrePluie::BIENTOT : GenrePluie::PLUS_TARD;
    }
    return e;
}

// L'accueil annonce la pluie des prochaines heures, pas celle de demain : « Pluie dès 7 h » se lirait comme une heure d'AUJOURD'HUI. Une
// pluie qui commence à plus de 12 heures (la largeur de la courbe d'heures) devient « rien à annoncer » (AUCUNE) ; les autres genres,
// et une pluie à 12 heures pile, ne changent pas. `maintenant_s` : l'instant du moment (UTC).
inline constexpr int HORIZON_PLUIE_S = 12 * 3600;
constexpr EpisodePluie dans_l_horizon(const EpisodePluie &e, std::int64_t maintenant_s) noexcept {
    if (e.genre != GenrePluie::PLUS_TARD || e.debut_s - maintenant_s <= HORIZON_PLUIE_S)
        return e;
    EpisodePluie r;
    r.genre = GenrePluie::AUCUNE;
    return r;
}

// ================================================================================================= 2. heures dans les phrases

// « 13 h » ou « 13 h 30 » en français ; « 13:00 » ailleurs. Rend les octets écrits ; 0 et sortie vide si la place manque ou l'heure
// est hors limites.
constexpr std::size_t formater_heure_phrase(const heure::HeureLocale &h, bool francais, char *sortie,
                                            std::size_t capacite) noexcept {
    detail::Tampon t{sortie, capacite};
    if (h.heure < 0 || h.heure > 23 || h.minute < 0 || h.minute > 59) {
        t.trop = true;
        return t.terminer();
    }
    if (francais) {
        t.entier(h.heure);
        t.texte(" h");
        if (h.minute != 0) {
            t.car(' ');
            t.deux_chiffres(h.minute);
        }
    } else {
        t.deux_chiffres(h.heure);
        t.car(':');
        t.deux_chiffres(h.minute);
    }
    return t.terminer();
}

// « 14–22 h » (français, heures rondes), « 14 h 30–22 h » (français sinon), « 14:00–22:00 » ailleurs. Seulement quand les deux bouts
// tombent le MÊME jour local et dans l'ordre : sinon 0 et sortie vide (l'alerte s'affiche sans fenêtre).
constexpr std::size_t formater_fenetre(const heure::HeureLocale &a, const heure::HeureLocale &b, bool francais,
                                       char *sortie, std::size_t capacite) noexcept {
    detail::Tampon t{sortie, capacite};
    const bool meme_jour = a.an == b.an && a.mois == b.mois && a.jour == b.jour;
    const bool valides = a.heure >= 0 && a.heure <= 23 && b.heure >= 0 && b.heure <= 23 && a.minute >= 0 &&
                         a.minute <= 59 && b.minute >= 0 && b.minute <= 59;
    if (!meme_jour || !valides || b.heure * 60 + b.minute <= a.heure * 60 + a.minute) {
        t.trop = true;
        return t.terminer();
    }
    if (francais) {
        const bool rondes = a.minute == 0 && b.minute == 0;
        t.entier(a.heure);
        if (!rondes) {
            t.texte(" h");
            if (a.minute != 0) {
                t.car(' ');
                t.deux_chiffres(a.minute);
            }
        }
        t.texte("\xE2\x80\x93"); // tiret demi-cadratin
        t.entier(b.heure);
        t.texte(" h");
        if (b.minute != 0) {
            t.car(' ');
            t.deux_chiffres(b.minute);
        }
    } else {
        t.deux_chiffres(a.heure);
        t.car(':');
        t.deux_chiffres(a.minute);
        t.texte("\xE2\x80\x93");
        t.deux_chiffres(b.heure);
        t.car(':');
        t.deux_chiffres(b.minute);
    }
    return t.terminer();
}

// ============================================================================================ 3. courbe monotone (Fritsch-Carlson)

inline constexpr int COURBE_MAX = 16;

struct Pt {
    int x = 0;
    int y = 0;
};

// Une courbe qui passe par ses points et ne dépasse jamais l'intervalle de deux points voisins (jamais de « bosse » entre deux
// températures). Tangentes en millièmes (dy/dx x 1000). Même méthode que la courbe d'heures du Tab5 : tangente = moyenne des deux pentes
// voisines, nulle à un extremum local, pentes des bouts = pente voisine, puis plafond de 3 fois la pente de chaque intervalle.
struct CourbeMonotone {
    int n = 0;
    Pt p[COURBE_MAX] = {};
    int t[COURBE_MAX] = {};

    // Ordonnée (pixels, arrondie) de la courbe en `x`. Hors de l'intervalle des points : l'ordonnée du point de bout.
    constexpr int y_en(int x) const noexcept {
        if (n <= 0)
            return 0;
        if (n == 1 || x <= p[0].x)
            return p[0].y;
        if (x >= p[n - 1].x)
            return p[n - 1].y;
        int i = 0;
        while (i + 2 < n && x > p[i + 1].x)
            ++i;
        return hermite(p[i], t[i], p[i + 1], t[i + 1], x);
    }

    static constexpr int hermite(const Pt &a, int ta, const Pt &b, int tb, int x) noexcept {
        const std::int64_t h = b.x - a.x;
        if (h <= 0)
            return a.y;
        const std::int64_t s = x - a.x;
        const std::int64_t T = (s * 65536 + h / 2) / h; // t dans [0, 1], en 1/65536
        const std::int64_t T2 = (T * T) >> 16;
        const std::int64_t T3 = (T2 * T) >> 16;
        const std::int64_t h00 = 2 * T3 - 3 * T2 + 65536;
        const std::int64_t h10 = T3 - 2 * T2 + T;
        const std::int64_t h01 = -2 * T3 + 3 * T2;
        const std::int64_t h11 = T3 - T2;
        const std::int64_t num = (h00 * a.y + h01 * b.y) * 1000 + (h10 * ta + h11 * tb) * h;
        return static_cast<int>(detail::div_arrondi(num, 65536LL * 1000));
    }
};

// Les points doivent être rangés par x croissant (au plus COURBE_MAX ; les suivants sont ignorés).
constexpr CourbeMonotone courbe_monotone(const Pt *pts, int n) noexcept {
    CourbeMonotone c;
    if (pts == nullptr || n <= 0)
        return c;
    c.n = n < COURBE_MAX ? n : COURBE_MAX;
    for (int i = 0; i < c.n; ++i)
        c.p[i] = pts[i];
    if (c.n < 2)
        return c;
    std::int64_t m[COURBE_MAX] = {};
    for (int i = 0; i + 1 < c.n; ++i) {
        const std::int64_t dx = c.p[i + 1].x - c.p[i].x;
        m[i] = dx > 0 ? detail::div_arrondi(static_cast<std::int64_t>(c.p[i + 1].y - c.p[i].y) * 1000, dx) : 0;
    }
    std::int64_t t[COURBE_MAX] = {};
    t[0] = m[0];
    t[c.n - 1] = m[c.n - 2];
    for (int i = 1; i + 1 < c.n; ++i)
        t[i] = (m[i - 1] * m[i] <= 0) ? 0 : detail::div_arrondi(m[i - 1] + m[i], 2);
    for (int i = 0; i + 1 < c.n; ++i) {
        if (m[i] == 0) {
            t[i] = 0;
            t[i + 1] = 0;
            continue;
        }
        const std::int64_t a = t[i] * 1000 / m[i];     // pente de départ / pente de l'intervalle, x 1000
        const std::int64_t b = t[i + 1] * 1000 / m[i]; // idem à l'arrivée
        const std::int64_t s = a * a + b * b;          // x 1e6
        if (s > 9000000) {                             // a² + b² > 9 : on ramène le couple sur le cercle de rayon 3
            const std::int64_t r = detail::racine_entiere(s); // racine x 1000
            t[i] = t[i] * 3000 / r;
            t[i + 1] = t[i + 1] * 3000 / r;
        }
    }
    for (int i = 0; i < c.n; ++i)
        c.t[i] = static_cast<int>(t[i]);
    return c;
}

// Ordonnée (pixels) d'une valeur sur l'échelle [lo, hi] -> [bas, haut] (le plus grand est en HAUT : `haut` < `bas`). Une courbe plate
// (lo == hi) est posée à mi-hauteur.
constexpr int ordonnee_de(int valeur_dix, int lo_dix, int hi_dix, int haut, int bas) noexcept {
    if (hi_dix <= lo_dix)
        return (haut + bas) / 2;
    const int v = valeur_dix < lo_dix ? lo_dix : (valeur_dix > hi_dix ? hi_dix : valeur_dix);
    return bas -
           static_cast<int>(detail::div_arrondi(static_cast<std::int64_t>(v - lo_dix) * (bas - haut), hi_dix - lo_dix));
}

// ============================================================================================================ 4. gélules de pluie

enum class NiveauPluie : std::uint8_t { FAIBLE, MOYEN, FORT };

// Moins de 1 mm : faible ; moins de 3 mm : moyen ; sinon fort (mêmes seuils que les barres de pluie des heures).
constexpr NiveauPluie niveau_pluie(int dix_mm) noexcept {
    return dix_mm < 10 ? NiveauPluie::FAIBLE : (dix_mm < 30 ? NiveauPluie::MOYEN : NiveauPluie::FORT);
}

// Teinte d'un niveau, tirée de la couleur de pluie du thème (pas de couleur en dur) : le faible est éclairci, le fort assombri.
constexpr std::uint32_t couleur_niveau_pluie(const visuel::ThemePrincipal &t, NiveauPluie n) noexcept {
    const visuel::Rgb base = visuel::depuis_hex(t.pluie);
    switch (n) {
    case NiveauPluie::FAIBLE:
        return visuel::en_hex(visuel::melanger(base, visuel::depuis_hex(0xFFFFFF), 350));
    case NiveauPluie::FORT:
        return visuel::en_hex(visuel::melanger(base, visuel::depuis_hex(0x0A2A78), 300));
    case NiveauPluie::MOYEN:
        break;
    }
    return t.pluie;
}

struct Gelule {
    int largeur = 0;
    bool avec_texte = false;
    NiveauPluie niveau = NiveauPluie::FAIBLE;
};

// La gélule de pluie d'un jour. Largeur : de `w_min` (une goutte) à `w_max` (le jour le plus pluvieux des cinq), en RACINE CARRÉE de la
// pluie (0,4 mm se voit à côté de 9 mm). Le chiffre (« 9,3 mm ») n'est écrit que dès 1 mm ET seulement s'il tient dedans : la gélule
// s'élargit jusqu'à lui (jamais en dessous de son texte), sans dépasser `w_colonne` ; si même cela ne suffit pas, pas de chiffre.
// `largeur_texte` en pixels (0 : pas de texte à poser), `marge` : de chaque côté du texte. Une pluie nulle ou inconnue n'a pas de
// gélule (l'appelant dessine alors le repère « sec », ou rien).
constexpr Gelule gelule_du_jour(int pluie_dix, int max_dix, int w_min, int w_max, int largeur_texte, int marge,
                                int w_colonne) noexcept {
    Gelule g;
    g.largeur = w_min;
    g.niveau = niveau_pluie(pluie_dix);
    if (!connu(pluie_dix) || pluie_dix <= 0 || max_dix <= 0)
        return g;
    const int p = pluie_dix > max_dix ? max_dix : pluie_dix;
    const std::int64_t racine_pm = detail::racine_entiere(static_cast<std::int64_t>(p) * 1000000 / max_dix);
    g.largeur = w_min + static_cast<int>((static_cast<std::int64_t>(w_max - w_min) * racine_pm) / 1000);
    if (pluie_dix >= 10 && largeur_texte > 0) {
        const int besoin = largeur_texte + 2 * marge;
        if (besoin <= w_colonne) {
            g.avec_texte = true;
            if (besoin > g.largeur)
                g.largeur = besoin;
        }
    }
    return g;
}

// L'ÉCHELLE des gélules : la plus forte pluie de l'affichage, mais jamais moins que `plancher_dix`. Sans ce plancher, une semaine de bruine
// (0,8 mm au plus) remplirait toutes les gélules comme une tempête : à l'écran, une pluie négligeable doit rester petite.
inline constexpr int PLANCHER_ECHELLE_JOUR_DIX = 60;  // 6 mm sur un jour
inline constexpr int PLANCHER_ECHELLE_HEURE_DIX = 30; // 3 mm sur une heure
constexpr int echelle_pluie(int max_dix, int plancher_dix) noexcept {
    return max_dix > plancher_dix ? max_dix : plancher_dix;
}

// ===================================================================================================== 5. repères et extrêmes

// Une heure sur quatre porte son étiquette, la première comprise : 12 colonnes -> les colonnes 0, 4 et 8 (« 8h », « 12h », « 16h »).
inline constexpr int PAS_REPERE_HEURES = 4;
constexpr bool est_repere_heure(int colonne, int nb_colonnes) noexcept {
    return colonne >= 0 && colonne < nb_colonnes && colonne % PAS_REPERE_HEURES == 0;
}

struct Extremes {
    int i_max = -1; // première colonne du maximum ; -1 : aucune valeur connue
    int i_min =
        -1; // première colonne du minimum ; -1 : aucune, ou égal au maximum (courbe plate : un seul point étiqueté)
};

constexpr Extremes extremes_de(const int *temp_dix, int n) noexcept {
    Extremes e;
    if (temp_dix == nullptr)
        return e;
    for (int i = 0; i < n; ++i) {
        if (!connu(temp_dix[i]))
            continue;
        if (e.i_max < 0 || temp_dix[i] > temp_dix[e.i_max])
            e.i_max = i;
        if (e.i_min < 0 || temp_dix[i] < temp_dix[e.i_min])
            e.i_min = i;
    }
    if (e.i_min == e.i_max)
        e.i_min = -1;
    return e;
}

// ============================================================================================================= 6. bandeau

enum class Puce : std::uint8_t { AUCUNE, PLUIE, ALERTE_OFFICIELLE, ALERTE_METEO, CALME, PERIMEE };

struct ContenuBandeau {
    Puce gauche =
        Puce::AUCUNE; // pluie, « rien à signaler » (centrée sur toute la largeur) ou mention de données périmées
    Puce droite = Puce::AUCUNE; // l'alerte (officielle de Vigie, sinon celle que la météo laisse deviner)
    bool droite_grisee = false; // l'alerte déduite d'une météo périmée n'est plus un avis : on la montre effacée
};

// Ce que dit le bandeau. La pluie à gauche, l'alerte à droite : chacune garde sa place (rien ne se déplace quand l'autre apparaît).
//   `meteo_connue` faux : pas de météo, donc ni pluie ni « rien à signaler » (on ne le sait pas) ; l'alerte officielle reste.
//   `perimee` : la mention « Données de 07:52 » prend la place de la pluie.
//   « Rien à signaler » SEULEMENT quand la météo sait qu'il ne pleut pas (GenrePluie::AUCUNE) et qu'aucune alerte n'est là.
constexpr ContenuBandeau composer_bandeau(bool meteo_connue, bool perimee, GenrePluie pluie, bool alerte_officielle,
                                          bool alerte_meteo) noexcept {
    ContenuBandeau c;
    if (alerte_officielle) {
        c.droite = Puce::ALERTE_OFFICIELLE;
    } else if (meteo_connue && alerte_meteo) {
        c.droite = Puce::ALERTE_METEO;
        c.droite_grisee = perimee;
    }
    if (!meteo_connue)
        return c;
    if (perimee) {
        c.gauche = Puce::PERIMEE;
        return c;
    }
    if (pluie == GenrePluie::EN_COURS || pluie == GenrePluie::BIENTOT || pluie == GenrePluie::PLUS_TARD) {
        c.gauche = Puce::PLUIE;
    } else if (pluie == GenrePluie::AUCUNE && c.droite == Puce::AUCUNE) {
        c.gauche = Puce::CALME;
    }
    return c;
}

// ============================================================================================================ 7. mise en page

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Zones {
    Rect barre;      // le lieu (et « Exemple ») ; le bouton Réglages est posé à son bout droit
    Rect horloge;    // l'heure et la date
    Rect maintenant; // icône, température et temps du moment
    Rect heures;     // la carte de la courbe d'heures
    Rect bandeau;    // la pluie et l'alerte
    Rect jours;      // la carte des cinq jours
    int extra_h = 0; // pixels de référence gagnés en hauteur au-delà de 720 (paysage) : ils vont à la carte des jours
};

inline constexpr int REF_LARGEUR = 1280;
inline constexpr int REF_HAUTEUR = 720;

// Positions en pixels de RÉFÉRENCE : `w` x `h` est l'écran divisé par l'échelle (1280 x 720 sur le Tab5, 1280 x 750 sur la 7B de
// 1024 x 600, 720 x 1280 en portrait). La hauteur gagnée sur 720 (30 sur la 7B) va tout entière à la carte des jours, dont les rangées
// se répartissent dessus (ui_principal.cpp) ; la largeur gagnée va à la carte des heures et à celle des jours.
constexpr Zones disposer(int w, int h, bool portrait) noexcept {
    Zones z;
    if (!portrait) {
        z.extra_h = h > REF_HAUTEUR ? h - REF_HAUTEUR : 0;
        const int marge = 32;
        const int haut_carte = marge + 392; // 424 : haut de la carte des jours
        z.barre = {marge, 28, 592, 44};
        z.horloge = {marge, 76, 592, 186};
        z.maintenant = {marge, 270, 592, 60};
        z.heures = {640, 28, w - 640 - marge, 302};
        z.bandeau = {marge, 346, w - 2 * marge, 62};
        z.jours = {marge, haut_carte, w - 2 * marge, h - marge - haut_carte};
        return z;
    }
    const int marge = 28;
    const int largeur = w - 2 * marge;
    z.barre = {marge, 28, largeur, 44};
    z.horloge = {marge, 76, largeur, 186};
    z.maintenant = {marge, 270, largeur, 60};
    z.heures = {marge, 346, largeur, 302};
    z.bandeau = {marge, 664, largeur, 62};
    z.jours = {marge, 742, largeur, h - marge - 742};
    z.extra_h = z.jours.h > 264 ? z.jours.h - 264 : 0;
    return z;
}

// ================================================================================================================ 8. thème grisé

// Une valeur périmée n'est plus une mesure : sans couleur et à demi effacée. `rgb` devient son gris de luminance, mêlé au fond de la
// carte (`fond_rgb`) : 60 % de gris, 40 % de fond. Le résultat reste lisible (voir le test de contraste) mais ne se prend plus pour
// une mesure en cours.
constexpr std::uint32_t griser(std::uint32_t rgb, std::uint32_t fond_rgb) noexcept {
    const visuel::Rgb c = visuel::depuis_hex(rgb);
    const int luma = (2126 * c.r + 7152 * c.g + 722 * c.b + 5000) / 10000;
    const std::uint8_t l = static_cast<std::uint8_t>(luma);
    return visuel::en_hex(visuel::melanger(visuel::depuis_hex(fond_rgb), visuel::Rgb{l, l, l}, 600));
}

// Le même thème, tous les textes et toutes les teintes grisés. Le fond, les cartes et leur opacité ne changent pas.
constexpr visuel::ThemePrincipal theme_grise(const visuel::ThemePrincipal &t) noexcept {
    visuel::ThemePrincipal g = t;
    const std::uint32_t f = t.carte;
    g.texte = griser(t.texte, f);
    g.texte_2 = griser(t.texte_2, f);
    g.texte_3 = griser(t.texte_3, f);
    g.accent = griser(t.accent, f);
    g.avertissement = griser(t.avertissement, f);
    g.pluie = griser(t.pluie, f);
    for (std::uint32_t &c : g.role)
        c = griser(c, f);
    for (std::uint32_t &c : g.metrique)
        c = griser(c, f);
    for (std::uint32_t &c : g.niveau_uv)
        c = griser(c, f);
    for (std::uint32_t &c : g.alerte)
        c = griser(c, f);
    for (std::uint32_t &c : g.officielle)
        c = griser(c, f);
    for (std::uint32_t &c : g.rampe.teinte)
        c = griser(c, f);
    return g;
}

} // namespace ecran::accueil
