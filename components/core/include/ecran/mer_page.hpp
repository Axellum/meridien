// Noyau pur : la page mer COMPOSÉE. Ce que l'écran doit montrer d'une mer (mer_trame.hpp) à un instant donné, décidé sans LVGL : l'état de
// chaque carte d'activité, la teinte de chaque barre des 24 heures, les critères montrés sur une carte et leur marque dans le détail, la
// courbe de marée et son repère « maintenant », les prochaines marées, le style du bandeau. C++17 standard, aucun include ESP-IDF ni LVGL,
// aucun flottant, évaluable à la compilation (test/host/test_mer_page.cpp).
//
// La page dessine ; ce fichier décide. Ce qui est ici est ce qu'un test peut vérifier sans écran : qu'un verdict ne soit jamais montré pour
// des données trop vieilles, qu'une activité qu'on ne peut pas juger ici ne reçoive pas un « Bon » par défaut, qu'une vigilance illisible
// ne soit pas peinte comme un bandeau calme.
//
// Les couleurs ne sont PAS ici : une `Teinte` est un rôle (neutre, bon, possible, déconseillé) que la page associe à une couleur du thème.
// Les mots sont dans mer_textes.hpp.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/mer_textes.hpp"
#include "ecran/mer_trame.hpp"
#include "ecran/mer_vue.hpp"

namespace ecran::mer {

inline constexpr int RANGEES_CARTE = 3; // critères montrés sur une carte (le détail les a tous)
inline constexpr int BLOCS_MAX = 4; // le plus de blocs de points dessinés pour un critère (le surf en donne 2 au plus)
inline constexpr int ECHANTILLONS_MAREE = 49;     // un point toutes les demi-heures des 24 heures des bandes
inline constexpr std::int64_t PAS_MAREE_S = 1800; // secondes entre deux points de la courbe

// ---- teintes -----------------------------------------------------------------------------------------------------------------------
enum class Teinte : std::uint8_t { Neutre = 0, Bon = 1, Possible = 2, Deconseille = 3 };

constexpr Teinte teinte_verdict(Verdict v) noexcept {
    switch (v) {
    case Verdict::Bon:
        return Teinte::Bon;
    case Verdict::Possible:
        return Teinte::Possible;
    case Verdict::Deconseille:
        return Teinte::Deconseille;
    default:
        return Teinte::Neutre; // inconnu : jamais une couleur de verdict
    }
}

// ---- les cartes --------------------------------------------------------------------------------------------------------------------
// Les verdicts ne se montrent que pour une mer fraîche ou de quelques minutes ; rien reçu ou trop ancien : aucun verdict.
constexpr bool verdicts_montres(Fraicheur f) noexcept { return f == Fraicheur::Fraiche || f == Fraicheur::Vieillie; }

// Une activité dont le verdict est montré : elle en a un, on peut la juger ici, et la mer n'est ni absente ni trop vieille.
constexpr bool jugee(const Prevision &a, bool montres) noexcept {
    return montres && a.presente && a.disponible && a.verdict != Verdict::Inconnu;
}

enum class EtatCarte : std::uint8_t {
    SansVerdict = 0, // pas de verdict à montrer : la carte dit pourquoi (mer absente, trop vieille) ou rien
    Indisponible,    // on ne peut pas la juger ici (pêche à pied sans modèle de marée) : « Indisponible ici »
    Jugee,           // le verdict est montré
};

struct VueCarte {
    EtatCarte etat = EtatCarte::SansVerdict;
    Verdict verdict = Verdict::Inconnu; // seulement quand `etat` est `Jugee`
    bool terne = false;                 // données de quelques minutes : la carte est atténuée
    bool ouvrable = false;              // un appui ouvre le détail : elle a des critères à montrer
};

constexpr VueCarte vue_carte(const Mer &mer, int activite, Fraicheur f) noexcept {
    VueCarte v;
    if (activite < 0 || activite >= NB_ACTIVITES)
        return v;
    const Prevision &a = mer.activites[activite];
    const bool montres = verdicts_montres(f);
    v.terne = f == Fraicheur::Vieillie;
    if (jugee(a, montres)) {
        v.etat = EtatCarte::Jugee;
        v.verdict = a.verdict;
        v.ouvrable = a.nb_criteres > 0;
    } else if (montres && a.presente && !a.disponible) {
        v.etat = EtatCarte::Indisponible;
    }
    return v;
}

// La ligne sous le verdict : pourquoi ce n'est pas bon, ou pourquoi il n'y a pas de verdict. Rien reçu : « Mer : données indisponibles » ;
// trop ancien : « Données de mer trop anciennes » ; activité qu'on ne juge pas ici : « Indisponible ici ». Faux (tampon vidé) sinon.
constexpr std::size_t formater_texte_carte(const Mer &mer, int activite, Fraicheur f, Langue langue, char *sortie,
                                           std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (activite < 0 || activite >= NB_ACTIVITES) {
        p.trop = true;
        return p.finir();
    }
    if (f == Fraicheur::Rien) {
        p.texte(texte(Texte::MerIndisponible, langue));
        return p.finir();
    }
    if (f == Fraicheur::Expiree) {
        p.texte(texte(Texte::DonneesAnciennes, langue));
        return p.finir();
    }
    const Prevision &a = mer.activites[activite];
    if (a.presente && !a.disponible) {
        p.texte(texte(Texte::IndisponibleIci, langue));
        return p.finir();
    }
    if (jugee(a, true))
        return formater_raison(a, langue, sortie, capacite);
    p.trop = true;
    return p.finir();
}

// La teinte de la barre de l'heure `h` (0 à 23) : celle de son verdict le jour, neutre la nuit et quand le verdict est inconnu.
constexpr Teinte teinte_barre(const Prevision &a, int h) noexcept {
    if (h < 0 || h >= HEURES)
        return Teinte::Neutre;
    if (a.lumiere[h] == Lumiere::Nuit)
        return Teinte::Neutre;
    return teinte_verdict(a.heures[h]);
}

// L'heure « maintenant » dans les barres (0 à 23), ou -1 : jamais quand les verdicts ne sont pas montrés.
constexpr int heure_courante(const Mer &mer, Fraicheur f, std::int64_t maintenant_s) noexcept {
    return verdicts_montres(f) ? indice_heure(mer, maintenant_s) : -1;
}

// L'axe des barres (0 h / 12 h / 24 h de la bande) n'a de sens que si les barres sont montrées et que la bande a une heure de départ.
constexpr bool axe_montre(const Mer &mer, Fraicheur f) noexcept {
    return verdicts_montres(f) && mer.premiere_heure_s != 0;
}

// ---- critères ----------------------------------------------------------------------------------------------------------------------
// Une valeur qui peut s'écrire : connue, et d'une unité que ce firmware connaît.
constexpr bool valeur_montrable(const Critere_ligne &c) noexcept {
    return c.valeur_d != INCONNU && c.unite != Unite::Inconnue;
}

// Les critères d'une carte : les premiers dont la valeur est connue, sans la vigilance (le bandeau en parle) ; au plus `max`.
// Écrit leurs indices dans `indices` et rend leur nombre. Un critère inconnu n'est pas montré (jamais « 0 »).
constexpr int criteres_de_carte(const Prevision &a, int *indices, int max) noexcept {
    int n = 0;
    for (int k = 0; k < a.nb_criteres && k < CRITERES_MAX && n < max; ++k) {
        const Critere_ligne &c = a.criteres[k];
        if (c.id == Critere::Vigilance || c.id == Critere::Inconnu || !valeur_montrable(c))
            continue;
        indices[n++] = k;
    }
    return n;
}

// Ce que le détail dessine à côté d'un critère : des blocs de points (surf), une pastille de verdict, ou rien (un tiret).
enum class Marque : std::uint8_t { Rien = 0, Pastille = 1, Blocs = 2 };

struct MarqueCritere {
    Marque marque = Marque::Rien;
    Verdict verdict = Verdict::Inconnu;
    int plein = 0; // blocs pleins (points donnés), au plus `total`
    int total = 0; // blocs dessinés, au plus BLOCS_MAX
};

constexpr MarqueCritere marque_critere(const Critere_ligne &c) noexcept {
    MarqueCritere m;
    const bool points = c.points >= 0 && c.points_max > 0 && c.points <= c.points_max;
    if (points) {
        m.marque = Marque::Blocs;
        m.verdict = c.verdict;
        m.total = c.points_max < BLOCS_MAX ? c.points_max : BLOCS_MAX;
        m.plein = c.points < m.total ? c.points : m.total;
    } else if (c.verdict != Verdict::Inconnu) {
        m.marque = Marque::Pastille;
        m.verdict = c.verdict;
    }
    return m;
}

// ---- marée -------------------------------------------------------------------------------------------------------------------------
// La courbe des 24 heures des bandes : une demi-cosinusoïde entre les marées que la trame donne, là où la trame les donne (rien avant la
// première ni après la dernière, pas de pont au-dessus d'un trou). Les ordonnées sont en MILLIÈMES de la hauteur du tracé, 0 en haut
// (pleine mer) et 1000 en bas (basse mer) : la page y applique ses marges. L'abscisse du point `k` est `k * 1000 / 48`.
struct CourbeMaree {
    bool presente = false; // au moins deux points qui se suivent : une courbe à tracer
    int premier = 0;       // premier et dernier points couverts, consécutifs
    int dernier = 0;
    std::int16_t y_pm[ECHANTILLONS_MAREE] = {};
    bool repere = false;          // « maintenant » tombe sur la courbe
    std::int32_t x_pm = 0;        // millièmes de la largeur
    std::int16_t y_repere_pm = 0; // millièmes de la hauteur, bornés à 0..1000
};

constexpr CourbeMaree courbe_maree(const Mer &mer, std::int64_t maintenant_s) noexcept {
    CourbeMaree c;
    const std::int64_t t0 = mer.premiere_heure_s;
    if (!mer.valide || t0 == 0)
        return c; // moins de deux marées : `hauteur_maree_cm` ne couvre aucun instant, donc aucun point et pas de courbe (testé)
    std::int32_t hauteurs[ECHANTILLONS_MAREE] = {};
    int premier = -1;
    int dernier = -1;
    std::int32_t bas = 0;
    std::int32_t haut = 0;
    for (int k = 0; k < ECHANTILLONS_MAREE; ++k) {
        std::int32_t h = 0;
        if (!hauteur_maree_cm(mer, t0 + k * PAS_MAREE_S, h)) {
            if (premier >= 0)
                break; // le premier tronçon que la trame couvre ; aucun pont au-dessus d'un trou
            continue;
        }
        hauteurs[k] = h;
        if (premier < 0) {
            premier = k;
            bas = haut = h;
        }
        dernier = k;
        if (h < bas)
            bas = h;
        if (h > haut)
            haut = h;
    }
    if (premier < 0 || dernier <= premier)
        return c;
    // Une marée plate (moins de 10 cm) ne s'étire pas sur toute la hauteur.
    const std::int32_t amplitude = haut - bas < 10 ? 10 : haut - bas;
    c.presente = true;
    c.premier = premier;
    c.dernier = dernier;
    for (int k = premier; k <= dernier; ++k)
        c.y_pm[k] = static_cast<std::int16_t>(
            surf::division_arrondie(static_cast<std::int64_t>(haut - hauteurs[k]) * 1000, amplitude));
    std::int32_t h_maintenant = 0;
    if (maintenant_s >= t0 && maintenant_s <= t0 + 86400 && hauteur_maree_cm(mer, maintenant_s, h_maintenant)) {
        c.repere = true;
        c.x_pm = static_cast<std::int32_t>((maintenant_s - t0) * 1000 / 86400);
        std::int64_t y = surf::division_arrondie(static_cast<std::int64_t>(haut - h_maintenant) * 1000, amplitude);
        y = y < 0 ? 0 : (y > 1000 ? 1000 : y);
        c.y_repere_pm = static_cast<std::int16_t>(y);
    }
    return c;
}

// Les deux prochaines marées (pleines ou basses) qui ne sont pas passées : leurs indices dans `mer.marees`, et leur nombre (0 à 2). Sans
// horloge (`maintenant_s` nul) on ne sait pas laquelle est la prochaine : aucune.
constexpr int prochaines_marees(const Mer &mer, std::int64_t maintenant_s, int *indices) noexcept {
    if (maintenant_s == 0)
        return 0;
    const int premiere = prochaine_maree(mer, maintenant_s);
    if (premiere < 0)
        return 0;
    int n = 0;
    for (int j = 0; j < 2 && premiere + j < mer.nb_marees && premiere + j < MAREES_MAX; ++j)
        indices[n++] = premiere + j;
    return n;
}

// Le coefficient (estimé) de la prochaine pleine mer, ou -1 : sans horloge on ne sait pas laquelle est la prochaine.
constexpr int coefficient_a_montrer(const Mer &mer, std::int64_t maintenant_s) noexcept {
    return maintenant_s == 0 ? -1 : coefficient_pour(mer, maintenant_s);
}

// ---- bandeau -----------------------------------------------------------------------------------------------------------------------
// Comment le bandeau se peint. `niveau` est le niveau officiel dont la page prend la couleur (0 inconnu = neutre, 1 vert, 2 jaune,
// 3 orange, 4 rouge : l'ordre de `vigie::Niveau` et de `officielle[]` du thème). `teinte` : le fond est teinté de cette couleur.
struct StyleBandeau {
    int niveau = 0;
    bool teinte = false;
};

constexpr StyleBandeau style_bandeau(Bandeau genre, vigie::Niveau niveau) noexcept {
    StyleBandeau s;
    switch (genre) {
    case Bandeau::Vigilance:
        s.niveau = static_cast<int>(niveau);
        s.teinte = true;
        break;
    case Bandeau::VigilanceIllisible:
        // Jamais peint comme un bandeau calme : jaune, pour qu'on le remarque, sans prétendre à une vigilance qu'on n'a pas lue.
        s.niveau = static_cast<int>(vigie::Niveau::Jaune);
        break;
    default: // un état (âge, absence) ou rien : neutre
        break;
    }
    return s;
}

} // namespace ecran::mer
