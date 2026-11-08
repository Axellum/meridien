// Tests du noyau pur : ce que la pastille de l'écran principal dit d'une alerte officielle (vigie_bandeau.hpp). Sur de VRAIES réponses de
// Vigie (donnees/vigie_*.inc, captées le 2026-11-07) et sur des alertes construites à la main pour les cas que le réel ne montre pas.
// Pas de static_assert : une `Alertes` pèse ≈ 70 Ko, trop pour une évaluation à la compilation ; seule l'exécution fait échouer.
#include <cstddef>
#include <cstring>
#include <string_view>

#include "ecran/vigie_alertes.hpp"
#include "ecran/vigie_bandeau.hpp"
#include "ecran/vigie_vue.hpp"
#include "mini.hpp"

using ecran::i18n::Langue;
using namespace ecran::vigie;

namespace {

using sv = std::string_view;

constexpr sv OSLO =
#include "donnees/vigie_oslo.inc"
    ;
constexpr sv LA_PALMA =
#include "donnees/vigie_la_palma.inc"
    ;
constexpr sv HAMBOURG =
#include "donnees/vigie_hambourg.inc"
    ;
constexpr sv LONDRES =
#include "donnees/vigie_londres.inc"
    ;

constexpr std::int64_t OSLO_S = 1794041558; // server_utc de la capture
constexpr std::int64_t LA_PALMA_S = 1794041552;
constexpr std::int64_t HAMBOURG_S = 1794041593;
constexpr std::int64_t LONDRES_S = 1794041564;

Alertes g_a; // espace de travail, hors pile

bool lire(sv json, Langue langue) { return analyser(json, ecran::i18n::code(langue), g_a) == Statut::Ok; }

void poser(char *dest, std::size_t cap, sv texte) {
    std::size_t n = texte.size() < cap - 1 ? texte.size() : cap - 1;
    std::memcpy(dest, texte.data(), n);
    dest[n] = '\0';
}

// Une alerte à la main : en vigueur sur toute la durée utile des essais (début 1000, fin 100000).
Alerte alerte(Niveau niveau, Phenomene p, sv titre, sv langue = "fr") {
    Alerte x;
    x.niveau = niveau;
    x.phenomene = p;
    x.debut = 1000;
    x.fin = 100000;
    if (!titre.empty()) {
        x.nb_textes = 1;
        poser(x.textes[0].langue, sizeof x.textes[0].langue, langue);
        poser(x.textes[0].titre, sizeof x.textes[0].titre, titre);
    }
    return x;
}

void a_la_main(std::initializer_list<Alerte> liste) {
    vider(g_a);
    g_a.statut = Statut::Ok;
    for (const Alerte &x : liste)
        g_a.alertes[g_a.nombre++] = x;
}

Bandeau bandeau_de(Langue langue, NiveauMin minimum = NiveauMin::Jaune) {
    const Vue v = composer(g_a, 5000, 4990, minimum);
    return composer_bandeau(g_a, v, langue);
}

int pas_d_alerte_pas_de_pastille() {
    a_la_main({});
    Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(!b.present && b.autres == 0 && b.texte[0] == '\0' && b.niveau == Niveau::Inconnu);

    VERIFIER(lire(HAMBOURG, Langue::Fr));
    b = composer_bandeau(g_a, composer(g_a, HAMBOURG_S, HAMBOURG_S, NiveauMin::Jaune), Langue::Fr);
    VERIFIER(!b.present && b.texte[0] == '\0');

    // Une réponse trop ancienne, ou une heure inconnue : rien.
    VERIFIER(lire(OSLO, Langue::En));
    b = composer_bandeau(g_a, composer(g_a, OSLO_S + 7 * 3600, OSLO_S, NiveauMin::Jaune), Langue::En);
    VERIFIER(!b.present);
    b = composer_bandeau(g_a, composer(g_a, 0, OSLO_S, NiveauMin::Jaune), Langue::En);
    VERIFIER(!b.present);
    return 0;
}

int la_vraie_alerte_d_oslo_se_dit_avec_les_mots_de_l_emetteur() {
    VERIFIER(lire(OSLO, Langue::Fr));
    const Vue v = composer(g_a, OSLO_S, OSLO_S, NiveauMin::Jaune);
    VERIFIER(v.nombre == 1);
    const Bandeau b = composer_bandeau(g_a, v, Langue::Fr);
    VERIFIER(b.present && b.niveau == Niveau::Jaune && b.autres == 0);
    // Pas de texte français pour cette alerte : l'anglais de l'émetteur, tel quel.
    VERIFIER(sv(b.texte) ==
             "Ice, yellow level, Lowland areas and valleys in southeastern Norway, 2026-11-06T15:00:00+00:00, "
             "2026-11-09T10:00:00+00:00");
    VERIFIER(b.empreinte != 0 &&
             b.empreinte != empreinte(g_a, v, "fr")); // celle de la page, plus le texte de la pastille
    VERIFIER(b.empreinte == composer_bandeau(g_a, v, Langue::Fr).empreinte);
    return 0;
}

int les_alertes_de_la_palma_disent_la_plus_grave_et_comptent_les_autres() {
    VERIFIER(lire(LA_PALMA, Langue::Fr));
    // À l'instant de la capture, aucune des neuf n'a commencé (la première commence à 1794052800) : pas de pastille.
    VERIFIER(!composer_bandeau(g_a, composer(g_a, LA_PALMA_S, LA_PALMA_S, NiveauMin::Jaune), Langue::Fr).present);

    // 1794086000 : trois avertissements orange (début 1794085200, fin 1794095999) et un jaune sont en vigueur ; les quatre autres
    // commencent à 1794096000, le jaune de 1794052800 a fini à 1794085199 (même lecture que le contrôle Python, attendu_vigie_la_palma.inc).
    const std::int64_t t = 1794086000;
    Vue v = composer(g_a, t, t, NiveauMin::Jaune);
    VERIFIER(v.nombre == 4);
    Bandeau b = composer_bandeau(g_a, v, Langue::Fr);
    VERIFIER(b.present && b.niveau == Niveau::Orange && b.autres == 3 && b.texte[0] != '\0');
    VERIFIER(b.niveau == g_a.alertes[v.indices[0]].niveau);

    // Un niveau minimal plus haut retire des alertes, et le compte avec.
    v = composer(g_a, t, t, NiveauMin::Orange);
    b = composer_bandeau(g_a, v, Langue::Fr);
    VERIFIER(v.nombre == 3 && b.present && b.niveau == Niveau::Orange && b.autres == 2);
    v = composer(g_a, t, t, NiveauMin::Rouge);
    VERIFIER(!composer_bandeau(g_a, v, Langue::Fr).present);
    return 0;
}

int londres_dit_son_brouillard_en_anglais_quand_l_ecran_est_en_allemand() {
    VERIFIER(lire(LONDRES, Langue::De));
    const Vue v = composer(g_a, LONDRES_S, LONDRES_S, NiveauMin::Jaune);
    VERIFIER(v.nombre == 1);
    const Bandeau b = composer_bandeau(g_a, v, Langue::De);
    VERIFIER(b.present && b.niveau == Niveau::Jaune);
    VERIFIER(b.texte[0] != '\0');
    return 0;
}

int sans_mot_de_l_emetteur_la_pastille_prend_les_mots_de_vigie_dans_la_langue_de_l_ecran() {
    a_la_main({alerte(Niveau::Orange, Phenomene::Pluie, "")});
    Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(b.present && sv(b.texte) == "Pluie \xC2\xB7 Orange");
    b = bandeau_de(Langue::En);
    VERIFIER(sv(b.texte) == "Rain \xC2\xB7 Orange");
    b = bandeau_de(Langue::Tr);
    VERIFIER(sv(b.texte) == "Yağmur \xC2\xB7 Turuncu");
    // Niveau inconnu : le niveau n'est pas écrit, seul le phénomène l'est.
    a_la_main({alerte(Niveau::Inconnu, Phenomene::Vent, "")});
    b = bandeau_de(Langue::De);
    VERIFIER(b.present && sv(b.texte) == "Wind" && b.niveau == Niveau::Inconnu);
    return 0;
}

int l_evenement_puis_le_phenomene_d_origine_passent_avant_les_mots_de_vigie() {
    Alerte x = alerte(Niveau::Jaune, Phenomene::Brouillard, "");
    poser(x.evenement, sizeof x.evenement, "Niebla");
    poser(x.phenomene_original, sizeof x.phenomene_original, "Fog");
    a_la_main({x});
    Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(sv(b.texte) == "Niebla");
    x.evenement[0] = '\0';
    a_la_main({x});
    b = bandeau_de(Langue::Fr);
    VERIFIER(sv(b.texte) == "Fog");
    return 0;
}

int le_titre_choisit_la_langue_de_l_ecran_puis_l_anglais() {
    Alerte x = alerte(Niveau::Jaune, Phenomene::Vent, "Vent violent", "fr");
    x.nb_textes = 2;
    poser(x.textes[1].langue, sizeof x.textes[1].langue, "en");
    poser(x.textes[1].titre, sizeof x.textes[1].titre, "Severe wind");
    a_la_main({x});
    VERIFIER(sv(bandeau_de(Langue::Fr).texte) == "Vent violent");
    VERIFIER(sv(bandeau_de(Langue::En).texte) == "Severe wind");
    VERIFIER(sv(bandeau_de(Langue::Nl).texte) == "Severe wind"); // pas de néerlandais : l'anglais
    return 0;
}

int la_pastille_tient_sur_une_ligne_sans_changer_un_mot() {
    a_la_main({alerte(Niveau::Jaune, Phenomene::Vent, "  Vent\nviolent\r\n\tsur   la c\xC3\xB4te  ")});
    const Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(sv(b.texte) == "Vent violent sur la c\xC3\xB4te"); // un seul espace, rien aux bords, l'accent est resté
    return 0;
}

int un_titre_fait_de_blancs_se_replie_sur_les_mots_de_vigie() {
    a_la_main({alerte(Niveau::Rouge, Phenomene::Orage, " \n\t ")});
    const Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(b.present && sv(b.texte) == "Orage \xC2\xB7 Rouge");
    return 0;
}

int la_plus_grave_est_celle_qui_parle_et_les_autres_se_comptent() {
    a_la_main({alerte(Niveau::Jaune, Phenomene::Vent, "Vent jaune"),
               alerte(Niveau::Rouge, Phenomene::Orage, "Orage rouge"),
               alerte(Niveau::Orange, Phenomene::Pluie, "Pluie orange")});
    Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(b.present && b.niveau == Niveau::Rouge && sv(b.texte) == "Orage rouge" && b.autres == 2);
    // À partir d'orange, le jaune n'entre plus : une de moins.
    b = bandeau_de(Langue::Fr, NiveauMin::Orange);
    VERIFIER(b.niveau == Niveau::Rouge && b.autres == 1);
    // À partir de rouge, elle est seule.
    b = bandeau_de(Langue::Fr, NiveauMin::Rouge);
    VERIFIER(b.niveau == Niveau::Rouge && b.autres == 0);
    return 0;
}

int une_alerte_pas_encore_commencee_ou_deja_finie_ne_fait_pas_de_pastille() {
    Alerte x = alerte(Niveau::Orange, Phenomene::Vent, "Vent");
    x.debut = 6000; // commence après « maintenant » (5000)
    a_la_main({x});
    VERIFIER(!bandeau_de(Langue::Fr).present);
    x.debut = 1000;
    x.fin = 5000; // finit à « maintenant » : la fin est exclue
    a_la_main({x});
    VERIFIER(!bandeau_de(Langue::Fr).present);
    x.fin = 5001;
    a_la_main({x});
    VERIFIER(bandeau_de(Langue::Fr).present);
    return 0;
}

int l_empreinte_change_avec_la_langue_le_niveau_et_le_texte() {
    a_la_main({alerte(Niveau::Orange, Phenomene::Vent, "Vent")});
    const std::uint32_t base = bandeau_de(Langue::Fr).empreinte;
    VERIFIER(base == bandeau_de(Langue::Fr).empreinte);
    a_la_main({alerte(Niveau::Rouge, Phenomene::Vent, "Vent")});
    VERIFIER(base != bandeau_de(Langue::Fr).empreinte);
    a_la_main({alerte(Niveau::Orange, Phenomene::Vent, "Vents")});
    VERIFIER(base != bandeau_de(Langue::Fr).empreinte);
    // Une alerte de plus, même moins grave : le compte « +1 » change.
    a_la_main({alerte(Niveau::Orange, Phenomene::Vent, "Vent"), alerte(Niveau::Jaune, Phenomene::Pluie, "Pluie")});
    VERIFIER(base != bandeau_de(Langue::Fr).empreinte);
    // Un changement que seule la page montre (la description de l'émetteur) change aussi l'empreinte : la page n'est pas bâtie sur une
    // description périmée sous prétexte que la pastille, elle, n'a pas changé.
    Alerte y = alerte(Niveau::Orange, Phenomene::Vent, "Vent");
    poser(y.textes[0].description, sizeof y.textes[0].description, "Rafales jusqu'a 90 km/h");
    a_la_main({y});
    VERIFIER(sv(bandeau_de(Langue::Fr).texte) == "Vent");
    VERIFIER(base != bandeau_de(Langue::Fr).empreinte);
    // Sans mot de l'émetteur, les mots de Vigie suivent la langue : l'empreinte aussi (sinon la pastille resterait dans l'ancienne langue).
    a_la_main({alerte(Niveau::Orange, Phenomene::Vent, "")});
    const std::uint32_t derive_fr = bandeau_de(Langue::Fr).empreinte;
    VERIFIER(derive_fr != bandeau_de(Langue::En).empreinte);
    return 0;
}

int la_copie_d_une_ligne_garde_la_place_du_zero_final() {
    // Un octet-témoin juste après la capacité : la copie ne l'écrase pas, pas même avec le zéro final.
    char tampon[16];
    for (char &c : tampon)
        c = '#';
    const std::size_t n = detail::une_ligne(sv("abcdefghijkl"), tampon, 8);
    VERIFIER(n == 7 && sv(tampon, 7) == "abcdefg" && tampon[7] == '\0' && tampon[8] == '#');
    // L'espace posé avant un mot compte dans la place : « abcdef gh » ne peut pas y mettre l'espace ET le « g » (il manquerait le zéro).
    for (char &c : tampon)
        c = '#';
    VERIFIER(detail::une_ligne(sv("abcdef gh"), tampon, 8) == 6 && sv(tampon, 6) == "abcdef" && tampon[6] == '\0' &&
             tampon[8] == '#');
    // Pas de sortie ou pas de place : rien d'écrit.
    for (char &c : tampon)
        c = '#';
    VERIFIER(detail::une_ligne(sv("abc"), tampon, 0) == 0 &&
             tampon[0] == '#'); // avant le cas nul : sans garde, celui-ci échoue sans planter
    VERIFIER(detail::une_ligne(sv("abc"), nullptr, 8) == 0);
    return 0;
}

int la_fenetre_de_l_alerte_vient_de_son_debut_et_de_sa_fin_ou_de_leurs_reperes() {
    // `alerte()` pose début 1000 et fin 100000 : ce sont eux que la pastille reprend
    a_la_main({alerte(Niveau::Jaune, Phenomene::Vent, "Vent violent")});
    Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(b.present && b.debut_s == 1000 && b.fin_s == 100000);
    // sans `debut` ni `fin`, l'instant d'effet (effective) et la fin de validité (expires) les remplacent
    Alerte x = alerte(Niveau::Jaune, Phenomene::Vent, "Vent violent");
    x.debut.reset();
    x.fin.reset();
    x.effectif = 1200;
    x.expire = 90000;
    a_la_main({x});
    b = bandeau_de(Langue::Fr);
    VERIFIER(b.present && b.debut_s == 1200 && b.fin_s == 90000);
    // `debut` l'emporte sur `effectif`, `fin` sur `expire`
    x.debut = 1100;
    x.fin = 80000;
    a_la_main({x});
    b = bandeau_de(Langue::Fr);
    VERIFIER(b.debut_s == 1100 && b.fin_s == 80000);
    // aucun repère : inconnus (0)
    Alerte y = alerte(Niveau::Jaune, Phenomene::Vent, "Vent violent");
    y.debut = 1000; // l'alerte doit être en vigueur pour avoir une pastille ; la fin, elle, est inconnue
    y.fin.reset();
    y.expire.reset();
    a_la_main({y});
    b = bandeau_de(Langue::Fr);
    VERIFIER(b.present && b.debut_s == 1000 && b.fin_s == 0);
    return 0;
}

int l_empreinte_de_la_pastille_suit_la_fenetre_meme_quand_la_page_ne_la_voit_pas() {
    // deux alertes qui ne diffèrent que par `effectif` (début de repli) : la page n'en voit pas la différence, la pastille si
    Alerte x = alerte(Niveau::Jaune, Phenomene::Vent, "Vent violent");
    x.debut.reset();
    x.effectif = 1200;
    a_la_main({x});
    const Bandeau a = bandeau_de(Langue::Fr);
    x.effectif = 1300;
    a_la_main({x});
    const Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(a.present && b.present && a.debut_s == 1200 && b.debut_s == 1300);
    VERIFIER(a.empreinte != b.empreinte);
    return 0;
}

int la_pastille_ne_depasse_jamais_son_tampon() {
    char long_titre[BANDEAU_TEXTE_CAP + 40];
    for (std::size_t i = 0; i + 1 < sizeof long_titre; ++i)
        long_titre[i] = (i % 7 == 6) ? ' ' : 'a';
    long_titre[sizeof long_titre - 1] = '\0';
    a_la_main({alerte(Niveau::Jaune, Phenomene::Vent, long_titre)});
    const Bandeau b = bandeau_de(Langue::Fr);
    VERIFIER(b.present);
    VERIFIER(std::strlen(b.texte) < BANDEAU_TEXTE_CAP);
    VERIFIER(b.texte[0] == 'a');
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(pas_d_alerte_pas_de_pastille)                                                                                    \
    X(la_vraie_alerte_d_oslo_se_dit_avec_les_mots_de_l_emetteur)                                                       \
    X(les_alertes_de_la_palma_disent_la_plus_grave_et_comptent_les_autres)                                             \
    X(londres_dit_son_brouillard_en_anglais_quand_l_ecran_est_en_allemand)                                             \
    X(sans_mot_de_l_emetteur_la_pastille_prend_les_mots_de_vigie_dans_la_langue_de_l_ecran)                            \
    X(l_evenement_puis_le_phenomene_d_origine_passent_avant_les_mots_de_vigie)                                         \
    X(le_titre_choisit_la_langue_de_l_ecran_puis_l_anglais)                                                            \
    X(la_pastille_tient_sur_une_ligne_sans_changer_un_mot)                                                             \
    X(un_titre_fait_de_blancs_se_replie_sur_les_mots_de_vigie)                                                         \
    X(la_plus_grave_est_celle_qui_parle_et_les_autres_se_comptent)                                                     \
    X(une_alerte_pas_encore_commencee_ou_deja_finie_ne_fait_pas_de_pastille)                                           \
    X(l_empreinte_change_avec_la_langue_le_niveau_et_le_texte)                                                         \
    X(la_copie_d_une_ligne_garde_la_place_du_zero_final)                                                               \
    X(la_fenetre_de_l_alerte_vient_de_son_debut_et_de_sa_fin_ou_de_leurs_reperes)                                      \
    X(l_empreinte_de_la_pastille_suit_la_fenetre_meme_quand_la_page_ne_la_voit_pas)                                    \
    X(la_pastille_ne_depasse_jamais_son_tampon)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
