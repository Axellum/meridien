// Noyau pur : la BANDE DE PLUIE de l'accueil (ADR-0027) — la ligne centrale, sous l'heure, qui montre la pluie des deux prochaines
// heures en une barre par QUART D'HEURE, comme la bande « Pluie dans l'heure » du Tab5 sous ESPHome, mais avec le pas de l'API
// (Open-Meteo donne des quarts d'heure, pas des pas de cinq minutes). C++17 standard, aucun include ESP-IDF ni LVGL, aucune
// allocation, évaluable à la compilation (test_accueil_bande_pluie.cpp).
//
// Ce que ce fichier calcule : à partir des quarts d'heure reçus (début du premier quart, pluie de chaque quart en dixièmes de mm) et de
// l'heure du moment, les barres à dessiner (du quart EN COURS jusqu'à deux heures plus loin), leur niveau (faible, moyen, fort), la
// position de « maintenant » et du repère « dans une heure », et trois faits pour la phrase : la pluie est-elle en cours, quand
// finit-elle, quelle est sa force. Il ne dessine rien et n'écrit aucun texte (la phrase est composée par l'écran, dans la langue de
// la personne).
//
// RÈGLES (écrites ici pour que l'écran ne les contredise pas) :
//   - un quart d'heure est « pluvieux » dès 0,1 mm (la bruine compte, comme dans la pluie proche de meteo_etat.h) ;
//   - le NIVEAU d'une barre est celui de la pluie ramenée à l'HEURE (x4) : moins de 1 mm/h faible, moins de 3 mm/h moyen, sinon fort
//     (les mêmes seuils que les barres des heures, `niveau_pluie`) ;
//   - la bande commence au quart d'heure qui CONTIENT le moment : le quart déjà écoulé que l'appareil garde n'est pas montré ;
//   - la bande n'est PRÉSENTE que s'il pleut dans l'un de ses quarts : sans pluie, l'écran garde sa phrase d'avant (« Rien à signaler »,
//     « Pluie dès 15 h »…) ; des quarts qui ne couvrent pas le moment (trop anciens, ou pas encore commencés) : jamais de bande ;
//   - un quart SANS valeur est inconnu : une barre vide, jamais une barre sèche, et il ne dit pas « la pluie finit » ;
//   - la fin de la pluie en cours est le début du premier quart sec qui la suit ; si la pluie dure jusqu'au bout de la bande (ou si
//     un quart inconnu la coupe), la fin est inconnue (-1) : l'écran ne dit pas « fin dans N min ».
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/accueil.hpp"
#include "ecran/visuel_vue.hpp"

namespace ecran::accueil {

inline constexpr int QUARTS_BANDE_MAX = 8;              // deux heures
inline constexpr std::int64_t QUART_S = 900;            // le pas de l'API (secondes)
inline constexpr int SEUIL_QUART_DIX = 1;               // 0,1 mm sur un quart d'heure : de la pluie, bruine comprise
inline constexpr int PLANCHER_ECHELLE_QUART_DIX_H = 30; // 3 mm/h : en dessous, une bruine reste une petite barre
inline constexpr std::int64_t HEURE_S = 3600;

struct BarreQuart {
    std::int64_t debut_s = 0;           // début du quart d'heure (instant UTC)
    int dix_mm = visuel::INCONNU_DIX;   // pluie du quart d'heure, dixièmes de mm ; inconnue : INCONNU_DIX
    int dix_mm_h = visuel::INCONNU_DIX; // la même pluie ramenée à l'heure (x4), dixièmes de mm/h : niveau et hauteur
    NiveauPluie niveau = NiveauPluie::FAIBLE;
    bool pluvieux = false; // au moins 0,1 mm
};

struct BandePluie {
    bool presente = false;
    int n = 0; // barres : du quart en cours, 1 à QUARTS_BANDE_MAX
    BarreQuart barre[QUARTS_BANDE_MAX];
    int echelle_dix_h = PLANCHER_ECHELLE_QUART_DIX_H; // la plus forte pluie de l'affichage (mm/h x10), au moins 3 mm/h
    int maintenant_pm = -1;                           // « maintenant » dans la bande, en ‰ de sa largeur (0 à 999)
    int plus_une_heure_pm = -1;                       // « dans une heure » dans la bande, en ‰ ; -1 : hors de la bande
    bool en_cours = false;                            // le quart en cours est pluvieux
    int premier = -1;                                 // indice du premier quart pluvieux de la bande
    int fin_dans_min = -1; // pluie en cours seulement : minutes avant sa fin ; -1 : inconnue ou au-delà de la bande
    NiveauPluie force = NiveauPluie::FAIBLE; // la force du quart en cours s'il pleut, sinon du premier quart pluvieux
};

// La bande. `debut_quarts_s` : début du premier quart d'heure gardé (VueMeteo::quarts_debut_s) ; `dix_quarts`, `nb` : la pluie de chaque
// quart d'heure en dixièmes de mm ; `maintenant_s` : l'instant du moment (UTC).
constexpr BandePluie composer_bande_pluie(std::int64_t debut_quarts_s, const int *dix_quarts, int nb,
                                          std::int64_t maintenant_s) noexcept {
    BandePluie b;
    if (dix_quarts == nullptr || nb <= 0 || debut_quarts_s <= 0 || maintenant_s <= 0 || maintenant_s < debut_quarts_s)
        return b;
    const std::int64_t courant_64 = (maintenant_s - debut_quarts_s) / QUART_S;
    if (courant_64 >= nb)
        return b; // les quarts gardés sont tous passés : ils ne couvrent plus le moment
    const int courant = static_cast<int>(courant_64);
    int n = nb - courant;
    if (n > QUARTS_BANDE_MAX)
        n = QUARTS_BANDE_MAX;
    int maxi_h = 0;
    for (int i = 0; i < n; ++i) {
        const int indice = courant + i;
        BarreQuart &q = b.barre[i];
        q.debut_s = debut_quarts_s + static_cast<std::int64_t>(indice) * QUART_S;
        const int dix = dix_quarts[indice];
        if (dix >= 0) { // une valeur négative (INCONNU_DIX) est « on ne sait pas »
            q.dix_mm = dix;
            q.dix_mm_h = dix * 4;
            q.niveau = niveau_pluie(q.dix_mm_h);
            q.pluvieux = dix >= SEUIL_QUART_DIX;
            if (q.dix_mm_h > maxi_h)
                maxi_h = q.dix_mm_h;
        }
    }
    b.n = n;
    b.echelle_dix_h = echelle_pluie(maxi_h, PLANCHER_ECHELLE_QUART_DIX_H);

    const std::int64_t largeur_s = static_cast<std::int64_t>(n) * QUART_S;
    b.maintenant_pm = static_cast<int>((maintenant_s - b.barre[0].debut_s) * 1000 / largeur_s);
    const std::int64_t dans_une_heure_s = maintenant_s + HEURE_S - b.barre[0].debut_s;
    b.plus_une_heure_pm = dans_une_heure_s < largeur_s ? static_cast<int>(dans_une_heure_s * 1000 / largeur_s) : -1;

    for (int i = 0; i < n; ++i) {
        if (b.barre[i].pluvieux) {
            b.premier = i;
            break;
        }
    }
    if (b.premier < 0)
        return b; // pas de pluie dans la bande : pas de bande
    b.presente = true;
    b.en_cours = b.barre[0].pluvieux;
    b.force = b.barre[b.en_cours ? 0 : b.premier].niveau;
    if (b.en_cours) {
        for (int i = 1; i < n; ++i) {
            if (b.barre[i].dix_mm < 0)
                break; // un quart inconnu : on ne dit pas que la pluie finit
            if (!b.barre[i].pluvieux) {
                const std::int64_t reste_s = b.barre[i].debut_s - maintenant_s;
                b.fin_dans_min = reste_s > 0 ? static_cast<int>((reste_s + 59) / 60) : 0;
                break;
            }
        }
    }
    return b;
}

// Hauteur d'une barre dans un graphique de `hauteur` pixels : en RACINE CARRÉE de la pluie sur l'échelle de la bande (une bruine de
// 0,4 mm/h se voit à côté de 6 mm/h), jamais moins que `plancher` pixels pour une pluie présente (une goutte doit se voir), jamais
// plus que `hauteur`. Pluie nulle ou inconnue : 0 (l'appelant dessine alors le repère « sec », ou rien).
constexpr int hauteur_de_barre_quart(int dix_mm_h, int echelle_dix_h, int hauteur, int plancher) noexcept {
    if (dix_mm_h <= 0 || echelle_dix_h <= 0 || hauteur <= 0)
        return 0;
    if (plancher > hauteur)
        plancher = hauteur;
    const int p = dix_mm_h > echelle_dix_h ? echelle_dix_h : dix_mm_h;
    const std::int64_t racine_pm = detail::racine_entiere(static_cast<std::int64_t>(p) * 1000000 / echelle_dix_h);
    const int h = plancher + static_cast<int>(static_cast<std::int64_t>(hauteur - plancher) * racine_pm / 1000);
    return h < plancher ? plancher : (h > hauteur ? hauteur : h);
}

} // namespace ecran::accueil
