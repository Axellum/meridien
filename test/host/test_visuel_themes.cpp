// Tests du noyau pur : les thèmes de l'écran principal. Ce qui compte pour lire l'écran à deux mètres est MESURÉ ici : le contraste
// (rapport de luminance WCAG 2.x) de chaque texte et de chaque icône contre ce qu'il y a derrière, pour les 28 ciels possibles
// (4 moments x 7 temps) et pour les fonds pleins. Seuils : 4,5:1 pour du texte, 3:1 pour une icône, une barre ou un gros chiffre.
//
// Ce que le test ne dit PAS : que les couleurs soient belles (le jugement se fait sur les rendus de docs/design/) ni qu'elles
// restent lisibles pour un daltonien (non mesuré).
#include <cstdint>
#include <string_view>

#include "ecran/visuel_themes.hpp"
#include "mini.hpp"

using namespace ecran::visuel;

namespace {

// ---- luminance relative WCAG, sans bibliothèque mathématique ----

// Racine cinquième de v (0 à 1) par la méthode de Newton ; x^2,4 = x^2 * (x^2)^(1/5).
constexpr double racine5(double v) {
    if (v <= 0.0)
        return 0.0;
    double y = 1.0;
    for (int i = 0; i < 80; ++i) {
        const double y2 = y * y;
        const double y4 = y2 * y2;
        y = y - (y4 * y - v) / (5.0 * y4);
    }
    return y;
}

constexpr double puissance_2_4(double x) { return x * x * racine5(x * x); }

struct TableLineaire {
    double v[256];
};

constexpr TableLineaire construire_table() {
    TableLineaire t{};
    for (int i = 0; i < 256; ++i) {
        const double c = i / 255.0;
        t.v[i] = c <= 0.04045 ? c / 12.92 : puissance_2_4((c + 0.055) / 1.055);
    }
    return t;
}

constexpr TableLineaire LINEAIRE = construire_table();

constexpr double luminance(std::uint32_t rgb) {
    return 0.2126 * LINEAIRE.v[(rgb >> 16) & 0xFF] + 0.7152 * LINEAIRE.v[(rgb >> 8) & 0xFF] +
           0.0722 * LINEAIRE.v[rgb & 0xFF];
}

constexpr double contraste(std::uint32_t a, std::uint32_t b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

constexpr double TEXTE = 4.5;
constexpr double ICONE = 3.0;

// Les arrière-plans possibles d'un thème : pour le thème à ciel, 28 x 2 (haut et bas du dégradé) ; sinon le fond plein.
// `f(couleur_derriere_les_cartes, couleur_de_la_carte_posée_dessus)` pour chacun.
template <typename F> constexpr bool pour_chaque_arriere_plan(const ThemePrincipal &t, F f) {
    if (!t.ciel_dynamique)
        return f(t.fond, carte_sur(t, t.fond));
    for (int m = 0; m < 4; ++m) {
        for (int a = 0; a < static_cast<int>(Ambiance::NOMBRE); ++a) {
            const Degrade d = ciel(static_cast<MomentJour>(m), static_cast<Ambiance>(a));
            if (!f(d.haut, carte_sur(t, d.haut)) || !f(d.bas, carte_sur(t, d.bas)))
                return false;
        }
    }
    return true;
}

constexpr int le_texte_garde_4_5_pour_1_sur_les_cartes_de_chaque_theme() {
    for (const ThemePrincipal *t : THEMES) {
        VERIFIER(pour_chaque_arriere_plan(*t, [&](std::uint32_t, std::uint32_t carte) {
            return contraste(t->texte, carte) >= TEXTE && contraste(t->texte_2, carte) >= TEXTE &&
                   contraste(t->texte_3, carte) >= TEXTE && contraste(t->avertissement, carte) >= TEXTE;
        }));
    }
    return 0;
}

constexpr int l_heure_et_la_date_posees_directement_sur_le_ciel_gardent_4_5_pour_1() {
    for (const ThemePrincipal *t : THEMES) {
        VERIFIER(pour_chaque_arriere_plan(*t, [&](std::uint32_t fond, std::uint32_t) {
            return contraste(t->texte, fond) >= TEXTE && contraste(t->texte_2, fond) >= TEXTE;
        }));
    }
    return 0;
}

constexpr int chaque_icone_garde_3_pour_1_sur_la_carte_et_sur_le_fond() {
    for (const ThemePrincipal *t : THEMES) {
        VERIFIER(pour_chaque_arriere_plan(*t, [&](std::uint32_t fond, std::uint32_t carte) {
            for (int r = 1; r < NB_ROLES; ++r) { // le rôle 0 (AUCUN) n'a pas de teinte
                if (contraste(t->role[r], carte) < ICONE || contraste(t->role[r], fond) < ICONE)
                    return false;
            }
            return true;
        }));
    }
    return 0;
}

constexpr int les_couleurs_des_grandeurs_des_barres_et_des_niveaux_gardent_3_pour_1() {
    for (const ThemePrincipal *t : THEMES) {
        VERIFIER(pour_chaque_arriere_plan(*t, [&](std::uint32_t fond, std::uint32_t carte) {
            for (int m = 0; m < NB_METRIQUES; ++m) {
                if (contraste(t->metrique[m], carte) < ICONE || contraste(t->metrique[m], fond) < ICONE)
                    return false;
            }
            for (int u = 0; u < 5; ++u) {
                if (contraste(t->niveau_uv[u], carte) < ICONE)
                    return false;
            }
            for (int i = 0; i < 5; ++i) {
                if (contraste(t->rampe.teinte[i], carte) < ICONE)
                    return false;
            }
            return contraste(t->pluie, carte) >= ICONE && contraste(t->accent, carte) >= ICONE &&
                   contraste(t->filet, carte) >= 1.2;
        }));
    }
    return 0;
}

constexpr int le_texte_d_une_alerte_se_lit_sur_sa_pastille_et_la_pastille_sur_le_fond() {
    for (const ThemePrincipal *t : THEMES) {
        for (int a = 1; a < 6; ++a) {
            VERIFIER(contraste(t->texte_sur_couleur, t->alerte[a]) >= TEXTE);
        }
        VERIFIER(pour_chaque_arriere_plan(*t, [&](std::uint32_t fond, std::uint32_t carte) {
            for (int a = 1; a < 6; ++a) {
                if (contraste(t->alerte[a], fond) < ICONE || contraste(t->alerte[a], carte) < ICONE)
                    return false;
            }
            return true;
        }));
    }
    return 0;
}

constexpr int le_texte_d_une_alerte_officielle_se_lit_sur_sa_pastille_et_la_pastille_sur_le_fond() {
    const int niveaux[] = {0, 2, 3, 4}; // inconnu, jaune, orange, rouge : le vert (1) n'est jamais montré
    for (const ThemePrincipal *t : THEMES) {
        for (const int n : niveaux) {
            VERIFIER(contraste(t->texte_sur_couleur, t->officielle[n]) >= TEXTE);
        }
        VERIFIER(pour_chaque_arriere_plan(*t, [&](std::uint32_t fond, std::uint32_t carte) {
            for (const int n : niveaux) {
                if (contraste(t->officielle[n], fond) < ICONE || contraste(t->officielle[n], carte) < ICONE)
                    return false;
            }
            return true;
        }));
        // Les trois niveaux se distinguent, et « inconnu » n'emprunte la couleur d'aucun d'eux (on ne devine pas la gravité).
        VERIFIER(t->officielle[2] != t->officielle[3] && t->officielle[3] != t->officielle[4] &&
                 t->officielle[2] != t->officielle[4]);
        VERIFIER(t->officielle[0] != t->officielle[2] && t->officielle[0] != t->officielle[3] &&
                 t->officielle[0] != t->officielle[4]);
    }
    return 0;
}

constexpr int la_mesure_de_contraste_donne_les_valeurs_connues_de_la_norme() {
    // Noir sur blanc : 21:1 ; gris #767676 sur blanc : 4,54:1 (le seuil classique) ; deux couleurs identiques : 1:1.
    VERIFIER(contraste(0x000000, 0xFFFFFF) > 20.9 && contraste(0x000000, 0xFFFFFF) < 21.1);
    VERIFIER(contraste(0x767676, 0xFFFFFF) > 4.52 && contraste(0x767676, 0xFFFFFF) < 4.56);
    VERIFIER(contraste(0x123456, 0x123456) > 0.999 && contraste(0x123456, 0x123456) < 1.001);
    VERIFIER(contraste(0xFFFFFF, 0x000000) == contraste(0x000000, 0xFFFFFF)); // symétrique
    return 0;
}

constexpr int le_ciel_suit_le_moment_et_le_temps() {
    // La nuit est plus sombre que le jour, un temps couvert plus gris (moins saturé) qu'un temps clair.
    for (int a = 0; a < static_cast<int>(Ambiance::NOMBRE); ++a) {
        const Ambiance temps = static_cast<Ambiance>(a);
        VERIFIER(luminance(ciel(MomentJour::NUIT, temps).bas) < luminance(ciel(MomentJour::JOUR, temps).bas));
        VERIFIER(luminance(ciel(MomentJour::NUIT, temps).haut) < luminance(ciel(MomentJour::JOUR, temps).haut));
    }
    VERIFIER(ciel(MomentJour::JOUR, Ambiance::CLAIR).haut ==
             CIEL_BASE[2].haut); // clair : le dégradé de base, sans mélange
    VERIFIER(ciel(MomentJour::JOUR, Ambiance::ORAGE).bas != ciel(MomentJour::JOUR, Ambiance::CLAIR).bas);
    // l'orage est plus sombre que le beau temps, en plein jour
    VERIFIER(luminance(ciel(MomentJour::JOUR, Ambiance::ORAGE).bas) <
             luminance(ciel(MomentJour::JOUR, Ambiance::CLAIR).bas));
    // deux ciels différents au moins par couple moment/temps voisin : on ne répète pas le même dégradé pour deux temps
    for (int m = 0; m < 4; ++m) {
        for (int a = 1; a < static_cast<int>(Ambiance::NOMBRE); ++a) {
            const Degrade clair = ciel(static_cast<MomentJour>(m), Ambiance::CLAIR);
            const Degrade autre = ciel(static_cast<MomentJour>(m), static_cast<Ambiance>(a));
            VERIFIER(clair.haut != autre.haut || clair.bas != autre.bas);
        }
    }
    return 0;
}

constexpr int les_themes_sont_complets_et_distincts() {
    VERIFIER(NB_THEMES == static_cast<int>(sizeof(THEMES) / sizeof(THEMES[0])));
    int ciel_dyn = 0;
    for (int i = 0; i < NB_THEMES; ++i) {
        VERIFIER(THEMES[i] != nullptr && THEMES[i]->nom != nullptr && THEMES[i]->nom[0] != '\0');
        if (THEMES[i]->ciel_dynamique) {
            ++ciel_dyn;
            VERIFIER(THEMES[i]->disposition == Disposition::CIEL);
        }
        for (int j = i + 1; j < NB_THEMES; ++j) {
            VERIFIER(std::string_view(THEMES[i]->nom) != std::string_view(THEMES[j]->nom));
        }
        // chaque rôle d'icône a une teinte (non noire : une teinte 0 serait un oubli) sauf le rôle AUCUN
        for (int r = 1; r < NB_ROLES; ++r) {
            VERIFIER(THEMES[i]->role[r] != 0);
        }
        for (int a = 1; a < 6; ++a) {
            VERIFIER(THEMES[i]->alerte[a] != 0);
        }
        for (int n = 0; n < 5; ++n) {
            VERIFIER(THEMES[i]->officielle[n] != 0);
        }
        // trois niveaux de texte distincts et décroissants en contraste
        VERIFIER(THEMES[i]->texte != THEMES[i]->texte_2 && THEMES[i]->texte_2 != THEMES[i]->texte_3);
    }
    VERIFIER(ciel_dyn == 1);                               // un seul thème à ciel
    VERIFIER(THEMES[0]->disposition == Disposition::CIEL); // le thème par défaut
    return 0;
}

constexpr int les_accesseurs_par_role_lisent_le_bon_tableau() {
    VERIFIER(teinte_role(kThemeCiel, Role::pluie) == kThemeCiel.role[5]);
    VERIFIER(teinte_role(kThemeCiel, Role::soleil) == kThemeCiel.role[1]);
    VERIFIER(couleur_metrique(kThemeCiel, Metrique::VENT) == kThemeCiel.metrique[0]);
    VERIFIER(couleur_metrique(kThemeCiel, Metrique::LUNE) == kThemeCiel.metrique[6]);
    VERIFIER(couleur_uv(kThemeCiel, CategorieUv::FAIBLE) == kThemeCiel.niveau_uv[0]);
    VERIFIER(couleur_uv(kThemeCiel, CategorieUv::EXTREME) == kThemeCiel.niveau_uv[4]);
    VERIFIER(couleur_uv(kThemeCiel, CategorieUv::INCONNU) == kThemeCiel.texte_3);
    VERIFIER(couleur_alerte(kThemeCiel, Alerte::GEL) == kThemeCiel.alerte[1]);
    VERIFIER(couleur_alerte(kThemeCiel, Alerte::ORAGE) == kThemeCiel.alerte[5]);
    VERIFIER(couleur_officielle(kThemeCiel, 2) == kThemeCiel.officielle[2]);
    VERIFIER(couleur_officielle(kThemeCiel, 4) == kThemeCiel.officielle[4]);
    VERIFIER(couleur_officielle(kThemeCiel, -1) == kThemeCiel.officielle[0]); // hors plage : le neutre
    VERIFIER(couleur_officielle(kThemeCiel, 5) == kThemeCiel.officielle[0]);
    // une carte pleine garde sa teinte, une carte transparente prend celle du fond
    VERIFIER(carte_sur(kThemeTableauSombre, 0x123456) == kThemeTableauSombre.carte);
    ThemePrincipal t = kThemeCiel;
    t.carte_opa = 0;
    VERIFIER(carte_sur(t, 0x123456) == 0x123456);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(le_texte_garde_4_5_pour_1_sur_les_cartes_de_chaque_theme)                                                        \
    X(l_heure_et_la_date_posees_directement_sur_le_ciel_gardent_4_5_pour_1)                                            \
    X(chaque_icone_garde_3_pour_1_sur_la_carte_et_sur_le_fond)                                                         \
    X(les_couleurs_des_grandeurs_des_barres_et_des_niveaux_gardent_3_pour_1)                                           \
    X(le_texte_d_une_alerte_se_lit_sur_sa_pastille_et_la_pastille_sur_le_fond)                                         \
    X(le_texte_d_une_alerte_officielle_se_lit_sur_sa_pastille_et_la_pastille_sur_le_fond)                              \
    X(la_mesure_de_contraste_donne_les_valeurs_connues_de_la_norme)                                                    \
    X(le_ciel_suit_le_moment_et_le_temps)                                                                              \
    X(les_themes_sont_complets_et_distincts)                                                                           \
    X(les_accesseurs_par_role_lisent_le_bon_tableau)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
