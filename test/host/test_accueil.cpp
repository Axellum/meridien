// Tests du noyau pur : la logique de l'accueil (ecran/accueil.hpp). Pluie annoncée, heures dites, courbe monotone (comparée à une
// implémentation en flottants écrite à part), gélules de pluie, bandeau, mise en page, thème grisé. Chaque test dit ce qu'il garantit ;
// test/host/mutations_accueil.py casse le code un morceau à la fois et vérifie que le bon test échoue.
#include <cstdint>

#include "ecran/accueil.hpp"
#include "mini.hpp"

using namespace ecran::accueil;
using ecran::heure::HeureLocale;
using ecran::visuel::HeurePrevue;
using ecran::visuel::ThemePrincipal;

namespace {

constexpr bool texte_egal(const char *a, const char *b) {
    std::size_t i = 0;
    for (; a[i] != '\0' && b[i] != '\0'; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return a[i] == b[i];
}

// ================================================================================================================ la pluie

constexpr std::int64_t T0 =
    1794034800; // un multiple de 3600 : « 08:00 » du jour de l'esquisse (le fuseau n'intervient pas ici)
constexpr std::int64_t H = 3600;

struct Jeu {
    HeurePrevue h[24];
    int n = 0;
};

constexpr Jeu jeu(const int *dix, int n) {
    Jeu j{};
    for (int i = 0; i < n; ++i) {
        j.h[i].instant_s = T0 + i * H;
        j.h[i].pluie_dix_mm = dix[i];
    }
    j.n = n;
    return j;
}

// Les 24 heures de l'esquisse (de 08 h à 07 h) : de la pluie de 13 h à 17 h (0,4 + 1,6 + 3,1 + 1,2 + 0,2 = 6,5 mm), puis de 22 h à 01 h.
constexpr int ESQUISSE[24] = {0, 0, 0, 0, 0, 4, 16, 31, 12, 2, 0, 0, 0, 0, 5, 14, 9, 0, 0, 0, 0, 0, 0, 0};
constexpr std::int64_t HUIT_45 = T0 + 45 * 60;

constexpr int la_pluie_de_l_esquisse_commence_a_13_h_et_cumule_6_5_mm() {
    const Jeu j = jeu(ESQUISSE, 24);
    const EpisodePluie e = episode_de_pluie(j.h, j.n, HUIT_45, -1);
    VERIFIER(e.genre == GenrePluie::PLUS_TARD);
    VERIFIER(e.debut_s == T0 + 5 * H);
    VERIFIER(e.dans_min == 255);
    VERIFIER(e.cumul_dix_mm == 65); // pas 65 + 28 : le second épisode n'est pas contigu
    VERIFIER(e.fin_s == T0 + 10 * H);
    return 0;
}

constexpr int la_pluie_proche_fixe_le_debut_et_les_heures_le_cumul() {
    const Jeu j = jeu(ESQUISSE, 24);
    // dans 25 minutes : annoncé « dans 25 min », sans cumul (aucune heure pluvieuse ne couvre 09 h 10)
    EpisodePluie e = episode_de_pluie(j.h, j.n, HUIT_45, 25);
    VERIFIER(e.genre == GenrePluie::BIENTOT && e.dans_min == 25 && e.debut_s == HUIT_45 + 25 * 60);
    VERIFIER(e.cumul_dix_mm == INCONNU_DIX);
    // dans 90 minutes : « dès » une heure, toujours sans cumul
    e = episode_de_pluie(j.h, j.n, HUIT_45, 90);
    VERIFIER(e.genre == GenrePluie::PLUS_TARD && e.dans_min == 90 && e.cumul_dix_mm == INCONNU_DIX);
    // l'heure de 09 h pluvieuse couvre le début annoncé à 09 h 10 : son cumul est repris
    int dix[24] = {};
    dix[1] = 30;
    const Jeu k = jeu(dix, 24);
    e = episode_de_pluie(k.h, k.n, HUIT_45, 25);
    VERIFIER(e.genre == GenrePluie::BIENTOT && e.cumul_dix_mm == 30 && e.fin_s == T0 + 2 * H);
    // il pleut : cumul de la suite d'heures pluvieuses qui contient maintenant
    dix[0] = 20;
    dix[1] = 50;
    const Jeu m = jeu(dix, 24);
    e = episode_de_pluie(m.h, m.n, HUIT_45, 0);
    VERIFIER(e.genre == GenrePluie::EN_COURS && e.dans_min == 0 && e.debut_s == HUIT_45 && e.cumul_dix_mm == 70);
    return 0;
}

constexpr int rien_dans_deux_heures_ecarte_les_heures_qui_commencent_avant() {
    int dix[24] = {};
    dix[1] = 50; // 09 h : avant 10 h 45 (08 h 45 + 2 h), donc écartée
    dix[3] = 20; // 11 h : retenue
    Jeu j = jeu(dix, 24);
    EpisodePluie e = episode_de_pluie(j.h, j.n, HUIT_45, -1);
    VERIFIER(e.genre == GenrePluie::PLUS_TARD && e.debut_s == T0 + 3 * H && e.dans_min == 135 && e.cumul_dix_mm == 20);
    dix[3] = 0; // plus rien après : sec
    j = jeu(dix, 24);
    e = episode_de_pluie(j.h, j.n, HUIT_45, -1);
    VERIFIER(e.genre == GenrePluie::AUCUNE && e.cumul_dix_mm == INCONNU_DIX);
    // la même pluie de 09 h SANS la pluie proche (donnée absente, -2) est annoncée
    e = episode_de_pluie(j.h, j.n, HUIT_45, -2);
    VERIFIER(e.genre == GenrePluie::BIENTOT && e.dans_min == 15 && e.cumul_dix_mm == 50);
    return 0;
}

constexpr int sans_pluie_proche_les_heures_decident_du_moment() {
    int dix[24] = {};
    dix[0] = 30; // 08 h : l'heure en cours
    Jeu j = jeu(dix, 24);
    EpisodePluie e = episode_de_pluie(j.h, j.n, HUIT_45, -2);
    VERIFIER(e.genre == GenrePluie::EN_COURS && e.dans_min == 0 && e.debut_s == HUIT_45 && e.cumul_dix_mm == 30);
    dix[0] = 0;
    dix[5] = 30; // 13 h
    j = jeu(dix, 24);
    e = episode_de_pluie(j.h, j.n, HUIT_45, -2);
    VERIFIER(e.genre == GenrePluie::PLUS_TARD && e.dans_min == 255);
    e = episode_de_pluie(j.h, j.n, HUIT_45 + 30, -2);
    VERIFIER(e.dans_min == 255); // 254,5 minutes : arrondi à l'échéance suivante, jamais à la minute d'avant
    // une heure qui commence EXACTEMENT maintenant est en cours ; une qui se termine EXACTEMENT maintenant est finie
    dix[5] = 0;
    dix[1] = 40;
    j = jeu(dix, 24);
    VERIFIER(episode_de_pluie(j.h, j.n, T0 + H, -2).genre == GenrePluie::EN_COURS);
    VERIFIER(episode_de_pluie(j.h, j.n, T0 + 2 * H, -2).genre == GenrePluie::AUCUNE);
    // une heure terminée ne compte plus : à 10 h 05 la pluie de 08 h est derrière
    dix[1] = 0;
    dix[0] = 80;
    j = jeu(dix, 24);
    e = episode_de_pluie(j.h, j.n, T0 + 2 * H + 300, -2);
    VERIFIER(e.genre == GenrePluie::AUCUNE);
    return 0;
}

constexpr int l_episode_s_arrete_au_trou_et_a_l_heure_seche() {
    int dix[24] = {};
    dix[5] = 10;
    dix[6] = 10;
    dix[8] = 10; // après une heure sèche
    Jeu j = jeu(dix, 24);
    EpisodePluie e = episode_de_pluie(j.h, j.n, HUIT_45, -1);
    VERIFIER(e.cumul_dix_mm == 20 && e.fin_s == T0 + 7 * H);
    // un trou dans les INSTANTS (heure manquante) coupe aussi l'épisode, même si les montants se suivent
    dix[7] = 10;
    dix[8] = 0;
    j = jeu(dix, 24);
    j.h[7].instant_s += H; // l'heure 07 manque : le créneau suivant est à 09
    e = episode_de_pluie(j.h, j.n, HUIT_45, -1);
    VERIFIER(e.cumul_dix_mm == 20 && e.fin_s == T0 + 7 * H);
    // le seuil est 0,1 mm : 0,1 est de la pluie, 0 n'en est pas
    int un[24] = {};
    un[4] = 1;
    j = jeu(un, 24);
    VERIFIER(episode_de_pluie(j.h, j.n, HUIT_45, -1).genre == GenrePluie::PLUS_TARD);
    un[4] = 0;
    j = jeu(un, 24);
    VERIFIER(episode_de_pluie(j.h, j.n, HUIT_45, -1).genre == GenrePluie::AUCUNE);
    return 0;
}

constexpr int une_pluie_inconnue_n_est_ni_de_la_pluie_ni_un_ciel_sec() {
    int inconnu[24] = {};
    for (int &v : inconnu)
        v = INCONNU_DIX;
    const Jeu j = jeu(inconnu, 24);
    VERIFIER(episode_de_pluie(j.h, j.n, HUIT_45, -2).genre == GenrePluie::INCONNU);
    VERIFIER(episode_de_pluie(j.h, 0, HUIT_45, -2).genre == GenrePluie::INCONNU);
    VERIFIER(episode_de_pluie(nullptr, 0, HUIT_45, -2).genre == GenrePluie::INCONNU);
    // l'heure du moment est inconnue : on ne dit rien
    const Jeu e = jeu(ESQUISSE, 24);
    VERIFIER(episode_de_pluie(e.h, e.n, 0, -1).genre == GenrePluie::INCONNU);
    // la pluie proche dit « rien dans 2 h » même sans heures : c'est un fait
    VERIFIER(episode_de_pluie(j.h, 0, HUIT_45, -1).genre == GenrePluie::AUCUNE);
    // et elle suffit à dire « il pleut » sans heures
    const EpisodePluie p = episode_de_pluie(j.h, 0, HUIT_45, 0);
    VERIFIER(p.genre == GenrePluie::EN_COURS && p.cumul_dix_mm == INCONNU_DIX);
    return 0;
}

constexpr int la_limite_entre_bientot_et_plus_tard_est_une_heure() {
    const Jeu j = jeu(ESQUISSE, 24);
    VERIFIER(episode_de_pluie(j.h, j.n, HUIT_45, 59).genre == GenrePluie::BIENTOT);
    VERIFIER(episode_de_pluie(j.h, j.n, HUIT_45, 60).genre == GenrePluie::PLUS_TARD);
    return 0;
}

constexpr int une_pluie_a_plus_de_douze_heures_n_est_pas_annoncee() {
    const Jeu j = jeu(ESQUISSE, 24);
    // Début à 13 h (4 h 15 plus tard) : annoncée telle quelle.
    const EpisodePluie proche = episode_de_pluie(j.h, j.n, HUIT_45, -2);
    VERIFIER(proche.genre == GenrePluie::PLUS_TARD);
    VERIFIER(dans_l_horizon(proche, HUIT_45).genre == GenrePluie::PLUS_TARD);
    VERIFIER(dans_l_horizon(proche, HUIT_45).cumul_dix_mm == 65);
    // La limite : 12 h pile reste annoncée, une seconde de plus non (la phrase se lirait comme une heure d'aujourd'hui).
    EpisodePluie e = proche;
    e.debut_s = HUIT_45 + 12 * 3600; // écrit en clair : la constante ne se vérifie pas avec elle-même
    VERIFIER(HORIZON_PLUIE_S == 12 * 3600);
    VERIFIER(dans_l_horizon(e, HUIT_45).genre == GenrePluie::PLUS_TARD);
    e.debut_s = HUIT_45 + 12 * 3600 + 1;
    const EpisodePluie loin = dans_l_horizon(e, HUIT_45);
    VERIFIER(loin.genre == GenrePluie::AUCUNE);
    VERIFIER(loin.dans_min == -1 && loin.cumul_dix_mm == ecran::visuel::INCONNU_DIX);
    // Les autres genres ne dépendent pas de l'horizon.
    for (const GenrePluie g : {GenrePluie::INCONNU, GenrePluie::AUCUNE, GenrePluie::EN_COURS, GenrePluie::BIENTOT}) {
        EpisodePluie x;
        x.genre = g;
        x.debut_s = HUIT_45 + 5 * HORIZON_PLUIE_S;
        VERIFIER(dans_l_horizon(x, HUIT_45).genre == g);
    }
    return 0;
}

// ======================================================================================================= heures dans les phrases

constexpr HeureLocale heure(int h, int m, int jour = 8) {
    HeureLocale x;
    x.an = 2026;
    x.mois = 11;
    x.jour = jour;
    x.heure = h;
    x.minute = m;
    return x;
}

constexpr int une_heure_de_phrase_est_ronde_en_francais_et_en_chiffres_ailleurs() {
    char s[16] = {};
    VERIFIER(formater_heure_phrase(heure(13, 0), true, s, sizeof s) == 4 && texte_egal(s, "13 h"));
    VERIFIER(formater_heure_phrase(heure(7, 0), true, s, sizeof s) == 3 && texte_egal(s, "7 h"));
    VERIFIER(formater_heure_phrase(heure(13, 30), true, s, sizeof s) > 0 && texte_egal(s, "13 h 30"));
    VERIFIER(formater_heure_phrase(heure(7, 5), true, s, sizeof s) > 0 && texte_egal(s, "7 h 05"));
    VERIFIER(formater_heure_phrase(heure(13, 0), false, s, sizeof s) == 5 && texte_egal(s, "13:00"));
    VERIFIER(formater_heure_phrase(heure(7, 5), false, s, sizeof s) > 0 && texte_egal(s, "07:05"));
    VERIFIER(formater_heure_phrase(heure(24, 0), true, s, sizeof s) == 0 && s[0] == '\0');
    char court[3] = {};
    VERIFIER(formater_heure_phrase(heure(13, 0), true, court, sizeof court) == 0 && court[0] == '\0');
    VERIFIER(formater_heure_phrase(heure(13, 0), true, nullptr, 0) == 0);
    return 0;
}

constexpr int la_fenetre_d_une_alerte_n_existe_que_dans_le_meme_jour() {
    char s[32] = {};
    VERIFIER(formater_fenetre(heure(14, 0), heure(22, 0), true, s, sizeof s) > 0 && texte_egal(s, "14\xE2\x80\x93"
                                                                                                  "22 h"));
    VERIFIER(formater_fenetre(heure(14, 30), heure(22, 0), true, s, sizeof s) > 0 && texte_egal(s, "14 h 30\xE2\x80\x93"
                                                                                                   "22 h"));
    VERIFIER(formater_fenetre(heure(14, 0), heure(22, 15), true, s, sizeof s) > 0 && texte_egal(s, "14 h\xE2\x80\x93"
                                                                                                   "22 h 15"));
    VERIFIER(formater_fenetre(heure(14, 0), heure(22, 0), false, s, sizeof s) > 0 && texte_egal(s, "14:00\xE2\x80\x93"
                                                                                                   "22:00"));
    // une alerte qui finit le lendemain, à l'envers, ou à l'heure de son début : pas de fenêtre
    VERIFIER(formater_fenetre(heure(14, 0), heure(6, 0, 9), true, s, sizeof s) == 0 && s[0] == '\0');
    VERIFIER(formater_fenetre(heure(14, 0), heure(22, 0, 9), true, s, sizeof s) ==
             0); // le lendemain, plus tard dans la journée
    VERIFIER(formater_fenetre(heure(22, 0), heure(14, 0), true, s, sizeof s) == 0);
    VERIFIER(formater_fenetre(heure(14, 0), heure(14, 0), true, s, sizeof s) == 0);
    VERIFIER(formater_fenetre(heure(14, 0), heure(25, 0), true, s, sizeof s) == 0);
    return 0;
}

// ============================================================================================================ courbe monotone

// Référence en flottants : la fonction `cheminLisse` de l'esquisse (frames de ecran_autonome, lib.js), transcrite telle quelle.
constexpr double racine_d(double v) {
    if (v <= 0.0)
        return 0.0;
    double y = v > 1.0 ? v : 1.0;
    for (int i = 0; i < 60; ++i)
        y = 0.5 * (y + v / y);
    return y;
}

constexpr double y_reference(const Pt *p, int n, int x) {
    double m[COURBE_MAX] = {};
    double t[COURBE_MAX] = {};
    for (int i = 0; i + 1 < n; ++i)
        m[i] = static_cast<double>(p[i + 1].y - p[i].y) / static_cast<double>(p[i + 1].x - p[i].x);
    t[0] = m[0];
    t[n - 1] = m[n - 2];
    for (int i = 1; i + 1 < n; ++i)
        t[i] = m[i - 1] * m[i] <= 0.0 ? 0.0 : (m[i - 1] + m[i]) / 2.0;
    for (int i = 0; i + 1 < n; ++i) {
        if (m[i] == 0.0) {
            t[i] = 0.0;
            t[i + 1] = 0.0;
        } else {
            const double a = t[i] / m[i];
            const double b = t[i + 1] / m[i];
            const double s = a * a + b * b;
            if (s > 9.0) {
                const double k = 3.0 / racine_d(s);
                t[i] = k * a * m[i];
                t[i + 1] = k * b * m[i];
            }
        }
    }
    int i = 0;
    while (i + 2 < n && x > p[i + 1].x)
        ++i;
    const double h = p[i + 1].x - p[i].x;
    const double u = (x - p[i].x) / h;
    const double u2 = u * u;
    const double u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * p[i].y + (u3 - 2 * u2 + u) * h * t[i] + (-2 * u3 + 3 * u2) * p[i + 1].y +
           (u3 - u2) * h * t[i + 1];
}

constexpr double valeur_absolue(double v) { return v < 0 ? -v : v; }

// Les 12 températures de l'esquisse (dixièmes), à 47 px d'écart, et trois autres formes qui sollicitent chaque règle.
constexpr int TEMPS_12[12] = {100, 110, 120, 130, 140, 140, 130, 120, 120, 110, 100, 100};
constexpr int MARCHE[5] = {0, 0, 0, 1000, 1000}; // plat puis saut : tangentes nulles, plafond de 3 fois la pente
constexpr int CREUX[5] = {500, 100, 300, 100,
                          500}; // un extremum à chaque point, de pentes inégales : tangente nulle partout
constexpr int RAIDE[6] = {0, 10, 20, 400, 410, 1000}; // pentes très différentes d'un intervalle au suivant
constexpr int SAUT[5] = {0, 30, 60, 300,
                         330}; // un saut après une pente douce : le plafond de 3 fois la pente est atteint

constexpr CourbeMonotone courbe_de(const int *y, int n, int pas) {
    Pt p[COURBE_MAX] = {};
    for (int i = 0; i < n; ++i)
        p[i] = {24 + pas * i, y[i]};
    return courbe_monotone(p, n);
}

constexpr int la_courbe_passe_par_ses_points() {
    const CourbeMonotone c = courbe_de(TEMPS_12, 12, 47);
    for (int i = 0; i < 12; ++i)
        VERIFIER(c.y_en(24 + 47 * i) == TEMPS_12[i]);
    const CourbeMonotone d = courbe_de(RAIDE, 6, 100);
    for (int i = 0; i < 6; ++i)
        VERIFIER(d.y_en(24 + 100 * i) == RAIDE[i]);
    return 0;
}

constexpr int la_courbe_entiere_suit_la_reference_en_flottants_a_un_pixel_pres() {
    const struct {
        const int *y;
        int n;
        int pas;
    } formes[] = {{TEMPS_12, 12, 47}, {MARCHE, 5, 120}, {CREUX, 5, 90}, {RAIDE, 6, 100}, {SAUT, 5, 100}};
    for (const auto &f : formes) {
        const CourbeMonotone c = courbe_de(f.y, f.n, f.pas);
        Pt p[COURBE_MAX] = {};
        for (int i = 0; i < f.n; ++i)
            p[i] = {24 + f.pas * i, f.y[i]};
        for (int x = 24; x <= 24 + f.pas * (f.n - 1); x += 3) {
            const double ref = y_reference(p, f.n, x);
            VERIFIER(valeur_absolue(c.y_en(x) - ref) <= 1.0);
        }
    }
    return 0;
}

constexpr int la_courbe_ne_depasse_jamais_ses_points_et_ne_recule_pas() {
    const struct {
        const int *y;
        int n;
        int pas;
    } formes[] = {{TEMPS_12, 12, 47}, {MARCHE, 5, 120}, {CREUX, 5, 90}, {RAIDE, 6, 100}, {SAUT, 5, 100}};
    for (const auto &f : formes) {
        const CourbeMonotone c = courbe_de(f.y, f.n, f.pas);
        for (int i = 0; i + 1 < f.n; ++i) {
            const int bas = f.y[i] < f.y[i + 1] ? f.y[i] : f.y[i + 1];
            const int haut = f.y[i] < f.y[i + 1] ? f.y[i + 1] : f.y[i];
            int precedent = c.y_en(24 + f.pas * i);
            for (int x = 24 + f.pas * i; x <= 24 + f.pas * (i + 1); ++x) {
                const int y = c.y_en(x);
                VERIFIER(y >= bas && y <= haut);                                  // jamais de bosse entre deux points
                VERIFIER(f.y[i + 1] >= f.y[i] ? y >= precedent : y <= precedent); // et jamais de recul
                precedent = y;
            }
        }
    }
    return 0;
}

constexpr int un_extremum_est_a_plat_et_les_bouts_suivent_leur_pente() {
    const CourbeMonotone c = courbe_de(CREUX, 5, 90);
    for (int i = 1; i + 1 < 5; ++i)
        VERIFIER(c.t[i] == 0);
    const CourbeMonotone d = courbe_de(TEMPS_12, 12, 47);
    VERIFIER(d.t[0] == 213 && d.t[11] == 0); // 1 dixième tous les 47 px au départ ; arrêt à plat sur le palier final
    VERIFIER(d.t[4] == 0 && d.t[5] == 0);    // le palier de 14° est plat
    // le plafond de 3 fois la pente : après un palier, la tangente d'un saut ne dépasse jamais 3 fois sa pente
    const CourbeMonotone e = courbe_de(MARCHE, 5, 120);
    VERIFIER(e.t[3] <= 3 * (1000 * 1000 / 120) && e.t[2] <= 3 * (1000 * 1000 / 120));
    return 0;
}

constexpr int une_courbe_trop_courte_ou_vide_ne_plante_pas() {
    VERIFIER(courbe_monotone(nullptr, 5).n == 0);
    VERIFIER(courbe_monotone(nullptr, 5).y_en(10) == 0);
    const Pt un[1] = {{50, 77}};
    const CourbeMonotone c = courbe_monotone(un, 1);
    VERIFIER(c.n == 1 && c.y_en(0) == 77 && c.y_en(500) == 77);
    const Pt deux[2] = {{0, 0}, {100, 100}};
    const CourbeMonotone d = courbe_monotone(deux, 2);
    VERIFIER(d.y_en(50) == 50 && d.y_en(-10) == 0 && d.y_en(1000) == 100); // hors des points : le point de bout
    // des points en désordre (deux x égaux) : pas de division par zéro, la courbe reste définie
    const Pt egaux[3] = {{0, 0}, {50, 10}, {50, 20}};
    return courbe_monotone(egaux, 3).y_en(25) == courbe_monotone(egaux, 3).y_en(25) ? 0 : __LINE__;
}

constexpr int l_ordonnee_d_une_valeur_va_du_bas_au_haut() {
    VERIFIER(ordonnee_de(100, 100, 140, 124, 230) == 230); // le plus froid en bas
    VERIFIER(ordonnee_de(140, 100, 140, 124, 230) == 124); // le plus chaud en haut
    VERIFIER(ordonnee_de(120, 100, 140, 124, 230) == 177); // le milieu
    VERIFIER(ordonnee_de(50, 100, 140, 124, 230) == 230);  // hors échelle : bornée
    VERIFIER(ordonnee_de(900, 100, 140, 124, 230) == 124);
    VERIFIER(ordonnee_de(120, 120, 120, 124, 230) == 177); // courbe plate : à mi-hauteur
    return 0;
}

// ========================================================================================================== gélules et niveaux

constexpr int la_gelule_suit_la_racine_de_la_pluie_et_le_chiffre_tient_dedans() {
    // l'esquisse : 9,3 mm est le plus pluvieux des cinq jours ; goutte de 26 px, plus grande gélule de 138 px ; texte de 80 px, marge 16
    Gelule g = gelule_du_jour(93, 93, 26, 138, 80, 16, 232);
    VERIFIER(g.largeur == 138 && g.avec_texte && g.niveau == NiveauPluie::FORT);
    // 5,1 mm : la racine donne 108 px, mais le chiffre demande 112 : la gélule s'élargit jusqu'à lui
    g = gelule_du_jour(51, 93, 26, 138, 80, 16, 232);
    VERIFIER(g.avec_texte && g.largeur == 112 && g.niveau == NiveauPluie::FORT);
    // 0,4 mm : une petite goutte, jamais de chiffre sous 1 mm
    g = gelule_du_jour(4, 93, 26, 138, 80, 16, 232);
    VERIFIER(!g.avec_texte && g.largeur == 49 && g.niveau == NiveauPluie::FAIBLE);
    // la colonne est trop étroite pour le chiffre : on le retire, la gélule garde sa largeur
    g = gelule_du_jour(51, 93, 26, 138, 80, 16, 100);
    VERIFIER(!g.avec_texte && g.largeur == 108);
    VERIFIER(gelule_du_jour(51, 93, 26, 138, 80, 16, 112).avec_texte);  // 112 = 80 + 2 x 16 : tient juste
    VERIFIER(!gelule_du_jour(51, 93, 26, 138, 80, 16, 111).avec_texte); // un pixel de moins : ne tient plus
    // pluie nulle, inconnue ou échelle nulle : la largeur de la goutte, sans texte
    VERIFIER(gelule_du_jour(0, 93, 26, 138, 80, 16, 232).largeur == 26);
    VERIFIER(!gelule_du_jour(INCONNU_DIX, 93, 26, 138, 80, 16, 232).avec_texte);
    VERIFIER(!gelule_du_jour(50, 0, 26, 138, 80, 16, 232).avec_texte);
    // une pluie plus forte que l'échelle ne dépasse pas la plus grande gélule
    VERIFIER(gelule_du_jour(500, 93, 26, 138, 0, 16, 232).largeur == 138);
    // plus il pleut, plus la gélule est large
    int precedent = 0;
    for (int dix = 1; dix <= 93; ++dix) {
        const int l = gelule_du_jour(dix, 93, 26, 138, 0, 16, 232).largeur;
        VERIFIER(l >= precedent);
        precedent = l;
    }
    return 0;
}

constexpr int une_pluie_negligeable_ne_remplit_pas_l_echelle() {
    VERIFIER(PLANCHER_ECHELLE_JOUR_DIX == 60 && PLANCHER_ECHELLE_HEURE_DIX == 30);
    // une semaine de bruine (0,8 mm au plus) : l'échelle reste à 6 mm, la gélule de 0,8 mm reste petite
    VERIFIER(echelle_pluie(8, PLANCHER_ECHELLE_JOUR_DIX) == 60);
    VERIFIER(gelule_du_jour(8, echelle_pluie(8, PLANCHER_ECHELLE_JOUR_DIX), 26, 138, 0, 16, 232).largeur ==
             66);                                                       // 26 + 112 x racine(8/60)
    VERIFIER(gelule_du_jour(8, 8, 26, 138, 0, 16, 232).largeur == 138); // sans plancher : pleine
    // l'échelle suit la pluie dès qu'elle dépasse le plancher, et le plancher lui-même est inclus
    VERIFIER(echelle_pluie(93, PLANCHER_ECHELLE_JOUR_DIX) == 93);
    VERIFIER(echelle_pluie(60, PLANCHER_ECHELLE_JOUR_DIX) == 60);
    VERIFIER(echelle_pluie(61, PLANCHER_ECHELLE_JOUR_DIX) == 61);
    VERIFIER(echelle_pluie(2, PLANCHER_ECHELLE_HEURE_DIX) == 30);
    VERIFIER(echelle_pluie(31, PLANCHER_ECHELLE_HEURE_DIX) == 31);
    VERIFIER(echelle_pluie(0, PLANCHER_ECHELLE_HEURE_DIX) == 30);
    return 0;
}

constexpr int les_niveaux_de_pluie_changent_a_1_et_a_3_mm() {
    VERIFIER(niveau_pluie(1) == NiveauPluie::FAIBLE && niveau_pluie(9) == NiveauPluie::FAIBLE);
    VERIFIER(niveau_pluie(10) == NiveauPluie::MOYEN && niveau_pluie(29) == NiveauPluie::MOYEN);
    VERIFIER(niveau_pluie(30) == NiveauPluie::FORT && niveau_pluie(500) == NiveauPluie::FORT);
    return 0;
}

constexpr double TableLineaireValeur(int i) {
    const double c = i / 255.0;
    if (c <= 0.04045)
        return c / 12.92;
    const double v = (c + 0.055) / 1.055;
    double y = 1.0; // v^2,4 = v^2 * (v^2)^(1/5), racine cinquième par la méthode de Newton
    const double v2 = v * v;
    for (int k = 0; k < 80; ++k) {
        const double y2 = y * y;
        const double y4 = y2 * y2;
        y = y - (y4 * y - v2) / (5.0 * y4);
    }
    return v2 * y;
}

constexpr double luminance(std::uint32_t rgb) {
    return 0.2126 * TableLineaireValeur((rgb >> 16) & 0xFF) + 0.7152 * TableLineaireValeur((rgb >> 8) & 0xFF) +
           0.0722 * TableLineaireValeur(rgb & 0xFF);
}

constexpr double contraste(std::uint32_t a, std::uint32_t b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

constexpr int les_teintes_de_pluie_se_distinguent_et_le_texte_reste_lisible_dans_chaque_theme() {
    for (const ThemePrincipal *t : ecran::visuel::THEMES) {
        const std::uint32_t faible = couleur_niveau_pluie(*t, NiveauPluie::FAIBLE);
        const std::uint32_t moyen = couleur_niveau_pluie(*t, NiveauPluie::MOYEN);
        const std::uint32_t fort = couleur_niveau_pluie(*t, NiveauPluie::FORT);
        VERIFIER(moyen == t->pluie);
        VERIFIER(luminance(faible) > luminance(moyen) && luminance(moyen) > luminance(fort));
        // le chiffre posé sur une gélule : gros texte gras (24 px), 3:1 au moins ; 4,5:1 sur la plus foncée
        VERIFIER(contraste(t->texte_sur_couleur, moyen) >= 3.0);
        VERIFIER(contraste(t->texte_sur_couleur, fort) >= 4.5);
    }
    return 0;
}

// ===================================================================================================== repères et extrêmes

constexpr int une_heure_sur_quatre_porte_son_etiquette() {
    VERIFIER(est_repere_heure(0, 12) && est_repere_heure(4, 12) && est_repere_heure(8, 12));
    VERIFIER(!est_repere_heure(1, 12) && !est_repere_heure(3, 12) && !est_repere_heure(12, 12) &&
             !est_repere_heure(-4, 12));
    VERIFIER(est_repere_heure(0, 1) && !est_repere_heure(4, 4));
    return 0;
}

constexpr int le_maximum_et_le_minimum_sont_les_premiers_de_la_courbe() {
    Extremes e = extremes_de(TEMPS_12, 12);
    VERIFIER(e.i_max == 4 && e.i_min == 0); // 14° à la colonne 4 (la première), 10° à la colonne 0 (la première)
    const int plate[4] = {120, 120, 120, 120};
    e = extremes_de(plate, 4);
    VERIFIER(e.i_max == 0 && e.i_min == -1); // un seul point étiqueté
    const int trous[5] = {INCONNU_DIX, 90, INCONNU_DIX, 130, 100};
    e = extremes_de(trous, 5);
    VERIFIER(e.i_max == 3 && e.i_min == 1);
    const int rien[2] = {INCONNU_DIX, INCONNU_DIX};
    e = extremes_de(rien, 2);
    VERIFIER(e.i_max == -1 && e.i_min == -1);
    VERIFIER(extremes_de(nullptr, 3).i_max == -1);
    const int negatifs[3] = {-50, -120, -30};
    e = extremes_de(negatifs, 3);
    VERIFIER(e.i_max == 2 && e.i_min == 1);
    return 0;
}

// ============================================================================================================== le bandeau

constexpr int le_bandeau_garde_la_pluie_a_gauche_et_l_alerte_a_droite() {
    // pluie annoncée + alerte officielle : les deux, chacune à sa place
    ContenuBandeau c = composer_bandeau(true, false, GenrePluie::PLUS_TARD, true, false);
    VERIFIER(c.gauche == Puce::PLUIE && c.droite == Puce::ALERTE_OFFICIELLE && !c.droite_grisee);
    // alerte officielle seule : la pluie reste vide, à gauche
    c = composer_bandeau(true, false, GenrePluie::AUCUNE, true, false);
    VERIFIER(c.gauche == Puce::AUCUNE && c.droite == Puce::ALERTE_OFFICIELLE);
    // l'alerte officielle l'emporte sur celle que la météo laisse deviner
    c = composer_bandeau(true, false, GenrePluie::EN_COURS, true, true);
    VERIFIER(c.droite == Puce::ALERTE_OFFICIELLE);
    c = composer_bandeau(true, false, GenrePluie::BIENTOT, false, true);
    VERIFIER(c.gauche == Puce::PLUIE && c.droite == Puce::ALERTE_METEO && !c.droite_grisee);
    return 0;
}

constexpr int rien_a_signaler_seulement_quand_on_sait_qu_il_ne_pleut_pas() {
    VERIFIER(composer_bandeau(true, false, GenrePluie::AUCUNE, false, false).gauche == Puce::CALME);
    // pluie inconnue : on ne dit pas « rien à signaler »
    ContenuBandeau c = composer_bandeau(true, false, GenrePluie::INCONNU, false, false);
    VERIFIER(c.gauche == Puce::AUCUNE && c.droite == Puce::AUCUNE);
    // une alerte, même déduite : pas de « rien à signaler »
    VERIFIER(composer_bandeau(true, false, GenrePluie::AUCUNE, false, true).gauche == Puce::AUCUNE);
    VERIFIER(composer_bandeau(true, false, GenrePluie::AUCUNE, true, false).gauche == Puce::AUCUNE);
    // pas de météo : rien, sauf l'alerte officielle (elle vient d'ailleurs)
    c = composer_bandeau(false, false, GenrePluie::AUCUNE, false, true);
    VERIFIER(c.gauche == Puce::AUCUNE && c.droite == Puce::AUCUNE);
    c = composer_bandeau(false, false, GenrePluie::INCONNU, true, false);
    VERIFIER(c.gauche == Puce::AUCUNE && c.droite == Puce::ALERTE_OFFICIELLE);
    return 0;
}

constexpr int des_donnees_perimees_prennent_la_place_de_la_pluie_et_effacent_l_alerte_deduite() {
    ContenuBandeau c = composer_bandeau(true, true, GenrePluie::PLUS_TARD, false, true);
    VERIFIER(c.gauche == Puce::PERIMEE && c.droite == Puce::ALERTE_METEO && c.droite_grisee);
    // une alerte officielle ne devient pas périmée parce que la météo l'est : elle garde sa couleur
    c = composer_bandeau(true, true, GenrePluie::AUCUNE, true, true);
    VERIFIER(c.gauche == Puce::PERIMEE && c.droite == Puce::ALERTE_OFFICIELLE && !c.droite_grisee);
    // périmées et sans alerte : jamais « rien à signaler »
    VERIFIER(composer_bandeau(true, true, GenrePluie::AUCUNE, false, false).gauche == Puce::PERIMEE);
    return 0;
}

// ============================================================================================================ mise en page

constexpr bool dedans(const Rect &r, int w, int h) {
    return r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 && r.x + r.w <= w && r.y + r.h <= h;
}
constexpr bool se_chevauchent(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

constexpr int les_zones_tiennent_dans_l_ecran_sans_se_marcher_dessus() {
    const struct {
        int w, h;
        bool portrait;
    } ecrans[] = {{1280, 720, false}, {1280, 750, false}, {1536, 720, false},
                  {1280, 768, false}, {720, 1280, true},  {720, 1500, true}};
    for (const auto &e : ecrans) {
        const Zones z = disposer(e.w, e.h, e.portrait);
        const Rect *r[6] = {&z.barre, &z.horloge, &z.maintenant, &z.heures, &z.bandeau, &z.jours};
        for (int i = 0; i < 6; ++i) {
            VERIFIER(dedans(*r[i], e.w, e.h));
            for (int k = i + 1; k < 6; ++k)
                VERIFIER(!se_chevauchent(*r[i], *r[k]));
        }
    }
    return 0;
}

constexpr int la_hauteur_gagnee_sur_720_va_a_la_carte_des_jours() {
    Zones z = disposer(1280, 720, false);
    VERIFIER(z.extra_h == 0 && z.jours.y == 424 && z.jours.h == 264 && z.heures.w == 608 && z.heures.h == 302);
    VERIFIER(z.bandeau.y == 346 && z.bandeau.h == 62 && z.bandeau.w == 1216);
    VERIFIER(z.jours.y + z.jours.h == 720 - 32); // 32 de marge sous la carte des jours
    // la 7B : 1024 x 600 = 1280 x 750 de référence, les 30 de plus vont aux jours, rien d'autre ne bouge
    const Zones b = disposer(1280, 750, false);
    VERIFIER(b.extra_h == 30 && b.jours.h == 294 && b.jours.y == 424 && b.heures.h == 302 && b.bandeau.y == 346);
    VERIFIER(b.jours.y + b.jours.h == 750 - 32);
    // un écran plus large : la carte des heures et celle des jours s'élargissent, la colonne de gauche non
    const Zones l = disposer(1536, 720, false);
    VERIFIER(l.heures.w == 864 && l.jours.w == 1472 && l.horloge.w == 592);
    // le portrait empile tout et donne le reste aux jours
    const Zones p = disposer(720, 1280, true);
    VERIFIER(p.heures.y > p.maintenant.y && p.bandeau.y > p.heures.y && p.jours.y > p.bandeau.y &&
             p.jours.h == 1280 - 28 - 742);
    return 0;
}

// ================================================================================================================ thème grisé

constexpr int ecart_de_teinte(std::uint32_t rgb) {
    const int r = (rgb >> 16) & 0xFF;
    const int g = (rgb >> 8) & 0xFF;
    const int b = rgb & 0xFF;
    const int haut = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const int bas = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return haut - bas;
}

constexpr int un_theme_grise_n_a_plus_de_couleur_mais_reste_lisible() {
    for (const ThemePrincipal *t : ecran::visuel::THEMES) {
        const ThemePrincipal g = theme_grise(*t);
        // le fond, la carte et l'opacité ne changent pas
        VERIFIER(g.fond == t->fond && g.carte == t->carte && g.carte_opa == t->carte_opa && g.filet == t->filet);
        VERIFIER(g.disposition == t->disposition && g.ciel_dynamique == t->ciel_dynamique);
        // plus de teinte : l'écart entre canaux reste celui du fond de la carte
        VERIFIER(ecart_de_teinte(g.texte) <= 12 && ecart_de_teinte(g.accent) <= 12 && ecart_de_teinte(g.pluie) <= 12);
        for (int i = 1; i < ecran::visuel::NB_ROLES; ++i)
            VERIFIER(ecart_de_teinte(g.role[i]) <= 12);
        for (int i = 0; i < ecran::visuel::NB_METRIQUES; ++i)
            VERIFIER(ecart_de_teinte(g.metrique[i]) <= 12);
        for (int i = 0; i < 5; ++i)
            VERIFIER(ecart_de_teinte(g.rampe.teinte[i]) <= 12 && ecart_de_teinte(g.officielle[i]) <= 12);
        // à demi effacé mais lisible (3:1 pour le texte principal et le secondaire)
        VERIFIER(contraste(g.texte, t->carte) >= 3.0 && contraste(g.texte_2, t->carte) >= 3.0);
        // et différent de l'original : un texte grisé n'est plus le texte
        VERIFIER(g.texte != t->texte);
    }
    return 0;
}

constexpr int griser_garde_la_luminance_a_60_pour_cent() {
    // le blanc sur un fond noir : 60 % de blanc
    VERIFIER(griser(0xFFFFFF, 0x000000) == 0x999999 || griser(0xFFFFFF, 0x000000) == 0x9A9A9A);
    // le noir sur un fond blanc : 40 % de blanc
    VERIFIER(griser(0x000000, 0xFFFFFF) == 0x666666 || griser(0x000000, 0xFFFFFF) == 0x676767);
    // le gris de luminance : un rouge pur est plus sombre qu'un vert pur
    VERIFIER((griser(0xFF0000, 0x000000) & 0xFF) < (griser(0x00FF00, 0x000000) & 0xFF));
    return 0;
}

// =========================================================================================== icônes et colonnes des heures

constexpr int une_icone_a_la_premiere_heure_puis_a_chaque_changement_de_temps() {
    using ecran::visuel::IdIcone;
    // Même temps toute la journée : une seule icône, à la première heure.
    const IdIcone calme[6] = {IdIcone::pluie, IdIcone::pluie, IdIcone::pluie,
                              IdIcone::pluie, IdIcone::pluie, IdIcone::pluie};
    for (int i = 0; i < 6; ++i)
        VERIFIER(a_une_icone_heure(calme, 6, i) == (i == 0));
    // Un temps qui change : une nouvelle icône à l'heure du changement, rien quand il dure, et le retour au premier temps en est un.
    const IdIcone change[7] = {IdIcone::soleil_jour, IdIcone::soleil_jour, IdIcone::nuageux_jour, IdIcone::nuageux_jour,
                               IdIcone::pluie,       IdIcone::pluie,       IdIcone::soleil_jour};
    const bool attendu[7] = {true, false, true, false, true, false, true};
    for (int i = 0; i < 7; ++i)
        VERIFIER(a_une_icone_heure(change, 7, i) == attendu[i]);
    // Le jour qui passe à la nuit change l'icône (la lune remplace le soleil) : c'est un changement.
    const IdIcone nuit[3] = {IdIcone::soleil_jour, IdIcone::lune_nuit, IdIcone::lune_nuit};
    VERIFIER(a_une_icone_heure(nuit, 3, 1) && !a_une_icone_heure(nuit, 3, 2));
    // Hors plage ou sans tableau : jamais d'icône.
    VERIFIER(!a_une_icone_heure(change, 7, -1) && !a_une_icone_heure(change, 7, 7) &&
             !a_une_icone_heure(nullptr, 7, 0));
    VERIFIER(!a_une_icone_heure(change, 0, 0));
    return 0;
}

constexpr int deux_colonnes_voisines_n_ont_jamais_la_meme_couleur_dans_aucun_theme() {
    for (const ThemePrincipal *t : ecran::visuel::THEMES) {
        // la première colonne (l'heure en cours) est en accent
        VERIFIER(couleur_de_colonne(*t, 0) == t->accent);
        // les suivantes : cyan (vent), vert (humidité), violet (pression), dans cet ordre, et la quatrième colonne reprend la couleur
        // de la deuxième
        using ecran::visuel::Metrique;
        VERIFIER(couleur_de_colonne(*t, 1) == t->metrique[static_cast<int>(Metrique::VENT)]);
        VERIFIER(couleur_de_colonne(*t, 2) == t->metrique[static_cast<int>(Metrique::HUMIDITE)]);
        VERIFIER(couleur_de_colonne(*t, 3) == t->metrique[static_cast<int>(Metrique::PRESSION)]);
        VERIFIER(couleur_de_colonne(*t, 1) == couleur_de_colonne(*t, 1 + NB_COULEURS_COLONNES));
        for (int i = 0; i < 11; ++i)
            VERIFIER(couleur_de_colonne(*t, i) != couleur_de_colonne(*t, i + 1));
        // et les trois couleurs sont différentes entre elles, comme de l'accent
        VERIFIER(couleur_de_colonne(*t, 1) != couleur_de_colonne(*t, 2) &&
                 couleur_de_colonne(*t, 2) != couleur_de_colonne(*t, 3) &&
                 couleur_de_colonne(*t, 1) != couleur_de_colonne(*t, 3));
    }
    // un numéro négatif (colonne inconnue) retombe sur l'accent, sans sortir d'un tableau
    VERIFIER(couleur_de_colonne(ecran::visuel::kThemeCiel, -3) == ecran::visuel::kThemeCiel.accent);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(la_pluie_de_l_esquisse_commence_a_13_h_et_cumule_6_5_mm)                                                         \
    X(la_pluie_proche_fixe_le_debut_et_les_heures_le_cumul)                                                            \
    X(rien_dans_deux_heures_ecarte_les_heures_qui_commencent_avant)                                                    \
    X(sans_pluie_proche_les_heures_decident_du_moment)                                                                 \
    X(l_episode_s_arrete_au_trou_et_a_l_heure_seche)                                                                   \
    X(une_pluie_inconnue_n_est_ni_de_la_pluie_ni_un_ciel_sec)                                                          \
    X(la_limite_entre_bientot_et_plus_tard_est_une_heure)                                                              \
    X(une_pluie_a_plus_de_douze_heures_n_est_pas_annoncee)                                                             \
    X(une_heure_de_phrase_est_ronde_en_francais_et_en_chiffres_ailleurs)                                               \
    X(la_fenetre_d_une_alerte_n_existe_que_dans_le_meme_jour)                                                          \
    X(la_courbe_passe_par_ses_points)                                                                                  \
    X(la_courbe_entiere_suit_la_reference_en_flottants_a_un_pixel_pres)                                                \
    X(la_courbe_ne_depasse_jamais_ses_points_et_ne_recule_pas)                                                         \
    X(un_extremum_est_a_plat_et_les_bouts_suivent_leur_pente)                                                          \
    X(une_courbe_trop_courte_ou_vide_ne_plante_pas)                                                                    \
    X(l_ordonnee_d_une_valeur_va_du_bas_au_haut)                                                                       \
    X(la_gelule_suit_la_racine_de_la_pluie_et_le_chiffre_tient_dedans)                                                 \
    X(une_pluie_negligeable_ne_remplit_pas_l_echelle)                                                                  \
    X(les_niveaux_de_pluie_changent_a_1_et_a_3_mm)                                                                     \
    X(les_teintes_de_pluie_se_distinguent_et_le_texte_reste_lisible_dans_chaque_theme)                                 \
    X(une_heure_sur_quatre_porte_son_etiquette)                                                                        \
    X(le_maximum_et_le_minimum_sont_les_premiers_de_la_courbe)                                                         \
    X(le_bandeau_garde_la_pluie_a_gauche_et_l_alerte_a_droite)                                                         \
    X(rien_a_signaler_seulement_quand_on_sait_qu_il_ne_pleut_pas)                                                      \
    X(des_donnees_perimees_prennent_la_place_de_la_pluie_et_effacent_l_alerte_deduite)                                 \
    X(les_zones_tiennent_dans_l_ecran_sans_se_marcher_dessus)                                                          \
    X(la_hauteur_gagnee_sur_720_va_a_la_carte_des_jours)                                                               \
    X(un_theme_grise_n_a_plus_de_couleur_mais_reste_lisible)                                                           \
    X(griser_garde_la_luminance_a_60_pour_cent)                                                                        \
    X(une_icone_a_la_premiere_heure_puis_a_chaque_changement_de_temps)                                                 \
    X(deux_colonnes_voisines_n_ont_jamais_la_meme_couleur_dans_aucun_theme)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
