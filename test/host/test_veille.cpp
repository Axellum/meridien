// Tests du noyau pur : veille de l'écran (veille.hpp) — plages horaires (dont celles qui passent minuit) et décision.
#include "ecran/veille.hpp"
#include "mini.hpp"

using namespace ecran::veille;

namespace {

constexpr int hm(int h, int m) { return h * 60 + m; }

// Jours : 0 = dimanche ... 6 = samedi.
constexpr std::uint8_t jour(int j) { return static_cast<std::uint8_t>(1u << j); }

// ---- plages ------------------------------------------------------------------------------------------------------------

constexpr int plage_dans_la_journee() {
    const Plage p{static_cast<std::int16_t>(hm(13, 0)), static_cast<std::int16_t>(hm(14, 30)), kTousLesJours};
    VERIFIER(plage_valide(p));
    VERIFIER(!dans_la_plage(p, hm(12, 59), 2));
    VERIFIER(dans_la_plage(p, hm(13, 0), 2)); // le debut est inclus
    VERIFIER(dans_la_plage(p, hm(14, 29), 2));
    VERIFIER(!dans_la_plage(p, hm(14, 30), 2)); // la fin est exclue
    VERIFIER(!dans_la_plage(p, hm(23, 59), 2));
    // Seulement le lundi (bit 1) : le même créneau, un autre jour, n'en fait pas partie.
    const Plage lundi{p.debut_min, p.fin_min, 1u << 1};
    VERIFIER(dans_la_plage(lundi, hm(13, 30), 1));
    VERIFIER(!dans_la_plage(lundi, hm(13, 30), 2));
    VERIFIER(!dans_la_plage(lundi, hm(13, 30), 0));
    return 0;
}

constexpr int plage_qui_passe_minuit_tous_les_jours() {
    const Plage p{static_cast<std::int16_t>(hm(22, 0)), static_cast<std::int16_t>(hm(7, 0)), kTousLesJours};
    VERIFIER(plage_valide(p));
    VERIFIER(!dans_la_plage(p, hm(21, 59), 3));
    VERIFIER(dans_la_plage(p, hm(22, 0), 3));
    VERIFIER(dans_la_plage(p, hm(23, 59), 3));
    VERIFIER(dans_la_plage(p, hm(0, 0), 3));
    VERIFIER(dans_la_plage(p, hm(6, 59), 3));
    VERIFIER(!dans_la_plage(p, hm(7, 0), 3)); // 07:00 : l'ecran est libre
    VERIFIER(!dans_la_plage(p, hm(12, 0), 3));
    return 0;
}

// Une plage qui passe minuit appartient au jour OU ELLE COMMENCE : vendredi soir -> samedi matin.
constexpr int plage_de_nuit_appartient_au_jour_du_depart() {
    const Plage p{static_cast<std::int16_t>(hm(22, 0)), static_cast<std::int16_t>(hm(7, 0)), jour(5)}; // vendredi seul
    VERIFIER(dans_la_plage(p, hm(23, 0), 5));                                                          // vendredi 23:00
    VERIFIER(dans_la_plage(p, hm(3, 0), 6));   // samedi 03:00 : encore la nuit de vendredi
    VERIFIER(!dans_la_plage(p, hm(3, 0), 5));  // vendredi 03:00 : c'est la nuit de jeudi, pas de vendredi
    VERIFIER(!dans_la_plage(p, hm(23, 0), 6)); // samedi 23:00 : samedi n'est pas dans la liste
    VERIFIER(!dans_la_plage(p, hm(8, 0), 6));  // samedi 08:00 : la nuit est finie
    return 0;
}

// Dimanche (0) est le jour d'avant lundi (1), et samedi (6) celui d'avant dimanche (0) : le « lendemain » reboucle.
constexpr int le_lendemain_de_samedi_est_dimanche() {
    const Plage p{static_cast<std::int16_t>(hm(23, 0)), static_cast<std::int16_t>(hm(6, 0)), jour(6)}; // samedi soir
    VERIFIER(dans_la_plage(p, hm(23, 30), 6));
    VERIFIER(dans_la_plage(p, hm(2, 0), 0)); // dimanche 02:00
    VERIFIER(!dans_la_plage(p, hm(2, 0), 6));
    VERIFIER(!dans_la_plage(p, hm(2, 0), 1));
    return 0;
}

constexpr int plage_vide_et_plage_toute_la_journee() {
    const Plage vide{static_cast<std::int16_t>(hm(8, 0)), static_cast<std::int16_t>(hm(8, 0)), kTousLesJours};
    VERIFIER(!dans_la_plage(vide, hm(8, 0), 1));
    VERIFIER(!dans_la_plage(vide, hm(20, 0), 1));
    const Plage tout{0, kMinutesParJour, kTousLesJours}; // 00:00 -> 24:00
    VERIFIER(plage_valide(tout));
    VERIFIER(dans_la_plage(tout, 0, 4));
    VERIFIER(dans_la_plage(tout, 1439, 4));
    return 0;
}

constexpr int plage_invalide_ne_contient_rien() {
    const Plage mauvais_debut{1440, 100, kTousLesJours};
    const Plage mauvaise_fin{100, 1441, kTousLesJours};
    const Plage sans_jour{100, 200, 0};
    const Plage negatif{-1, 200, kTousLesJours};
    VERIFIER(!plage_valide(mauvais_debut) && !plage_valide(mauvaise_fin) && !plage_valide(sans_jour) &&
             !plage_valide(negatif));
    VERIFIER(!dans_la_plage(mauvais_debut, 50, 1));
    VERIFIER(!dans_la_plage(sans_jour, 150, 1));
    VERIFIER(!dans_la_plage(negatif, 100, 1));
    // Une heure ou un jour hors plage ne tombe jamais dans une plage.
    const Plage p{0, kMinutesParJour, kTousLesJours};
    VERIFIER(!dans_la_plage(p, -1, 1) && !dans_la_plage(p, 1440, 1) && !dans_la_plage(p, 10, -1) &&
             !dans_la_plage(p, 10, 7));
    return 0;
}

// ---- décision ----------------------------------------------------------------------------------------------------------

constexpr Reglage reglage_du_soir() {
    Reglage r;
    r.attenuer_apres_s = 60;
    r.eteindre_apres_s = 300;
    r.rendormir_apres_s = 15;
    r.plages[0] = Plage{static_cast<std::int16_t>(hm(22, 0)), static_cast<std::int16_t>(hm(7, 0)), kTousLesJours};
    r.nombre_plages = 1;
    return r;
}

constexpr Entree entree(std::uint32_t inactif, int h, int m, bool connue = true, bool occupe = false) {
    Entree e;
    e.inactif_s = inactif;
    e.heure_connue = connue;
    e.minute_du_jour = hm(h, m);
    e.jour_semaine = 2;
    e.occupe = occupe;
    return e;
}

constexpr int sans_reglage_l_ecran_reste_allume() {
    const Reglage r; // tout a zero : rien n'est arme
    VERIFIER(decider(r, entree(0, 12, 0)).etat == Etat::Allume);
    VERIFIER(decider(r, entree(100000, 3, 0)).etat == Etat::Allume);
    VERIFIER(decider(r, entree(100000, 3, 0)).cause == Cause::Aucune);
    return 0;
}

constexpr int inactivite_attenue_puis_eteint() {
    const Reglage r = reglage_du_soir();
    VERIFIER(decider(r, entree(59, 12, 0)).etat == Etat::Allume);
    const Decision d60 = decider(r, entree(60, 12, 0));
    VERIFIER(d60.etat == Etat::Attenue && d60.cause == Cause::Inactivite);
    VERIFIER(decider(r, entree(299, 12, 0)).etat == Etat::Attenue);
    const Decision d300 = decider(r, entree(300, 12, 0));
    VERIFIER(d300.etat == Etat::Eteint && d300.cause == Cause::Inactivite);
    return 0;
}

constexpr int zero_veut_dire_jamais() {
    Reglage r = reglage_du_soir();
    r.attenuer_apres_s = 0;
    VERIFIER(decider(r, entree(200, 12, 0)).etat == Etat::Allume); // pas d'attenuation
    r.eteindre_apres_s = 0;
    VERIFIER(decider(r, entree(100000, 12, 0)).etat == Etat::Allume);
    return 0;
}

constexpr int dans_la_plage_l_ecran_se_rendort_seul() {
    const Reglage r = reglage_du_soir();
    const Decision touche = decider(r, entree(3, 23, 30)); // on vient de toucher
    VERIFIER(touche.etat == Etat::Allume && touche.cause == Cause::Activite);
    VERIFIER(decider(r, entree(14, 23, 30)).etat == Etat::Allume);
    const Decision nuit = decider(r, entree(15, 23, 30));
    VERIFIER(nuit.etat == Etat::Eteint && nuit.cause == Cause::PlageHoraire);
    VERIFIER(decider(r, entree(15, 3, 0)).etat == Etat::Eteint); // apres minuit
    return 0;
}

// Hors plage, la veille par inactivite continue de marcher ; elle ne depend pas de la plage.
constexpr int hors_plage_seule_l_inactivite_compte() {
    const Reglage r = reglage_du_soir();
    VERIFIER(decider(r, entree(20, 12, 0)).etat == Etat::Allume); // 20 s > rendormir_apres_s mais pas dans la plage
    VERIFIER(decider(r, entree(15, 7, 0)).etat == Etat::Allume);  // 07:00 pile : la plage est finie
    return 0;
}

constexpr int l_heure_inconnue_desarme_les_plages() {
    const Reglage r = reglage_du_soir();
    // 23:30 « en apparence » mais l'horloge n'est pas reglee : la plage ne doit PAS eteindre l'ecran.
    VERIFIER(decider(r, entree(100, 23, 30, false)).etat == Etat::Attenue); // seule l'inactivite agit
    VERIFIER(decider(r, entree(10, 23, 30, false)).etat == Etat::Allume);
    return 0;
}

constexpr int occupe_l_emporte_sur_tout() {
    const Reglage r = reglage_du_soir();
    VERIFIER(decider(r, entree(100000, 23, 30, true, true)).etat == Etat::Allume);
    VERIFIER(decider(r, entree(100000, 23, 30, true, true)).cause == Cause::Occupe);
    VERIFIER(decider(r, entree(100000, 12, 0, true, true)).etat == Etat::Allume);
    return 0;
}

constexpr int plusieurs_plages() {
    Reglage r;
    r.rendormir_apres_s = 5;
    r.plages[0] =
        Plage{static_cast<std::int16_t>(hm(13, 0)), static_cast<std::int16_t>(hm(14, 0)), kTousLesJours}; // sieste
    r.plages[1] = Plage{static_cast<std::int16_t>(hm(22, 0)), static_cast<std::int16_t>(hm(6, 0)), kTousLesJours};
    r.nombre_plages = 2;
    VERIFIER(decider(r, entree(10, 13, 30)).etat == Etat::Eteint);
    VERIFIER(decider(r, entree(10, 23, 0)).etat == Etat::Eteint);
    VERIFIER(decider(r, entree(10, 15, 0)).etat == Etat::Allume);
    // Une plage au-dela de `nombre_plages` n'est pas lue.
    r.plages[2] = Plage{0, kMinutesParJour, kTousLesJours};
    VERIFIER(decider(r, entree(10, 15, 0)).etat == Etat::Allume);
    return 0;
}

constexpr int validite_du_reglage() {
    Reglage r = reglage_du_soir();
    VERIFIER(reglage_valide(r));
    r.attenuer_apres_s = 300; // attenuer == eteindre : l'attenuation ne se verrait jamais
    VERIFIER(!reglage_valide(r));
    r.attenuer_apres_s = 400;
    VERIFIER(!reglage_valide(r));
    r.attenuer_apres_s = 0; // pas d'attenuation : correct
    VERIFIER(reglage_valide(r));
    r = reglage_du_soir();
    r.nombre_plages = kPlagesMax + 1;
    VERIFIER(!reglage_valide(r));
    r = reglage_du_soir();
    r.plages[0].debut_min = 2000;
    VERIFIER(!reglage_valide(r));
    r = reglage_du_soir();
    r.niveau_attenue = 0; // « atténué à zéro » serait un écran éteint
    VERIFIER(!reglage_valide(r));
    r.niveau_attenue = 101;
    VERIFIER(!reglage_valide(r));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(plage_dans_la_journee)                                                                                           \
    X(plage_qui_passe_minuit_tous_les_jours)                                                                           \
    X(plage_de_nuit_appartient_au_jour_du_depart)                                                                      \
    X(le_lendemain_de_samedi_est_dimanche)                                                                             \
    X(plage_vide_et_plage_toute_la_journee)                                                                            \
    X(plage_invalide_ne_contient_rien)                                                                                 \
    X(sans_reglage_l_ecran_reste_allume)                                                                               \
    X(inactivite_attenue_puis_eteint)                                                                                  \
    X(zero_veut_dire_jamais)                                                                                           \
    X(dans_la_plage_l_ecran_se_rendort_seul)                                                                           \
    X(hors_plage_seule_l_inactivite_compte)                                                                            \
    X(l_heure_inconnue_desarme_les_plages)                                                                             \
    X(occupe_l_emporte_sur_tout)                                                                                       \
    X(plusieurs_plages)                                                                                                \
    X(validite_du_reglage)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
