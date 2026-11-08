// Tests du noyau pur : l'écriture du registre des spots par l'API de la page de réglages (mer_api.hpp, docs/api-reglages.md §15).
// Chaque test applique un corps JSON, comme la page l'envoie, à un registre et lit ce que le validateur en fait : le registre obtenu, ou
// LAQUELLE des sept erreurs, avec l'ancien registre intact (tout ou rien).
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ecran/json_mini.hpp"
#include "ecran/mer_api.hpp"
#include "ecran/mer_spots.hpp"
#include "mini.hpp"

using namespace ecran::mer;
namespace json = ecran::json;
using sv = std::string_view;

namespace {

struct Res {
    bool lisible =
        false; // le corps de l'essai est du JSON : un essai mal écrit ne passe pas pour un refus du validateur
    ErreurApi erreur = ErreurApi::Aucune;
    Registre valeur{};
};

constexpr Res appliquer(sv corps, const Registre &actuel) {
    Res r;
    const json::Resultat l = json::lire(corps);
    r.lisible = l.ok();
    if (r.lisible)
        r.erreur = valider_mer(l.valeur, actuel, r.valeur);
    return r;
}

constexpr bool egal(const char *a, sv b) { return sv(a) == b; }

// Un registre de trois spots, le deuxième choisi, plage 12 à 25 : « un », « deux », « trois ».
constexpr Registre trois_spots() {
    Registre r;
    ajouter(r, "spot-un", "Un");
    ajouter(r, "spot-deux", "Deux");
    ajouter(r, "spot-trois", "Trois");
    choisir(r, 1);
    return r;
}

// `n` spots « spot-00 », « spot-01 »… écrits dans le tampon, noms de 32 lettres « é » (64 octets : le pire cas d'un nom valide).
template <std::size_t N> constexpr sv corps_de_spots(char (&tampon)[N], int n) {
    std::size_t p = 0;
    auto ecrire = [&](sv t) {
        for (const char c : t)
            tampon[p++] = c;
    };
    ecrire(R"({"spots":[)");
    for (int i = 0; i < n; ++i) {
        if (i > 0)
            ecrire(",");
        ecrire(R"({"id":"spot-)");
        tampon[p++] = static_cast<char>('0' + i / 10);
        tampon[p++] = static_cast<char>('0' + i % 10);
        ecrire(R"(","nom":")");
        for (int k = 0; k < 32; ++k)
            ecrire("é");
        ecrire(R"("})");
    }
    ecrire("]}");
    tampon[p] = '\0';
    return sv(tampon, p);
}

// Un texte de `n` fois la lettre `c`, entre guillemets JSON, pour fabriquer un identifiant ou un nom trop long.
template <std::size_t N> constexpr sv corps_avec_texte(char (&tampon)[N], sv avant, char c, std::size_t n, sv apres) {
    std::size_t p = 0;
    for (const char x : avant)
        tampon[p++] = x;
    for (std::size_t i = 0; i < n; ++i)
        tampon[p++] = c;
    for (const char x : apres)
        tampon[p++] = x;
    tampon[p] = '\0';
    return sv(tampon, p);
}

constexpr int un_objet_vide_ne_change_rien() {
    for (const Registre &actuel : {Registre{}, trois_spots()}) {
        const Res r = appliquer("{}", actuel);
        VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && egaux(r.valeur, actuel));
    }
    return 0;
}

constexpr int la_liste_entiere_est_prise_dans_l_ordre_et_le_premier_est_choisi() {
    const Res r = appliquer(R"({"spots":[{"id":"b","nom":"Bé"},{"id":"a","nom":"A"}]})", Registre{});
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.nombre == 2);
    VERIFIER(egal(r.valeur.spots[0].id, "b") && egal(r.valeur.spots[0].nom, "Bé"));
    VERIFIER(egal(r.valeur.spots[1].id, "a") && egal(r.valeur.spots[1].nom, "A"));
    VERIFIER(r.valeur.choisi == 0 && spot_choisi(r.valeur) != nullptr && egal(spot_choisi(r.valeur)->id, "b"));
    VERIFIER(registre_valide(r.valeur));
    return 0;
}

constexpr int la_nouvelle_liste_remplace_l_ancienne() {
    const Res r = appliquer(R"({"spots":[{"id":"seul","nom":"Seul"}]})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.nombre == 1);
    VERIFIER(egal(r.valeur.spots[0].id, "seul"));
    VERIFIER(r.valeur.spots[1].id[0] == '\0' && r.valeur.spots[2].id[0] == '\0'); // rien de l'ancienne liste ne reste
    VERIFIER(r.valeur.vent_min_kn == 12 && r.valeur.vent_max_kn == 25);           // la plage n'a pas bougé
    return 0;
}

constexpr int sans_choisi_le_meme_spot_reste_choisi_s_il_est_toujours_la() {
    // « deux » était choisi (rang 1) ; dans la nouvelle liste il est au rang 2.
    const Res r = appliquer(
        R"({"spots":[{"id":"spot-trois","nom":"Trois"},{"id":"spot-un","nom":"Un"},{"id":"spot-deux","nom":"Deux"}]})",
        trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.choisi == 2);
    VERIFIER(egal(spot_choisi(r.valeur)->id, "spot-deux"));
    return 0;
}

constexpr int sans_choisi_et_le_spot_choisi_parti_le_premier_est_choisi() {
    const Res r =
        appliquer(R"({"spots":[{"id":"spot-trois","nom":"Trois"},{"id":"spot-un","nom":"Un"}]})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.choisi == 0);
    VERIFIER(egal(spot_choisi(r.valeur)->id, "spot-trois"));
    return 0;
}

constexpr int choisi_nomme_un_spot_de_la_liste_telle_qu_elle_sera() {
    Res r = appliquer(R"({"choisi":"spot-trois"})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.choisi == 2 && r.valeur.nombre == 3);
    r = appliquer(R"({"spots":[{"id":"x","nom":"X"},{"id":"y","nom":"Y"}],"choisi":"y"})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.choisi == 1 &&
             egal(spot_choisi(r.valeur)->id, "y"));
    // un spot de l'ancienne liste qui n'est plus dans la nouvelle ne peut pas être choisi
    r = appliquer(R"({"spots":[{"id":"x","nom":"X"}],"choisi":"spot-deux"})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Choix);
    return 0;
}

constexpr int un_choix_hors_de_la_liste_ou_mal_forme_est_refuse() {
    for (const sv corps : {R"({"choisi":"inconnu"})", R"({"choisi":""})", R"({"choisi":12})", R"({"choisi":null})",
                           R"({"choisi":["spot-un"]})", R"({"choisi":"Spot-Un"})"}) {
        const Res r = appliquer(corps, trois_spots());
        VERIFIER(r.lisible && r.erreur == ErreurApi::Choix && egaux(r.valeur, trois_spots()));
    }
    // sur une liste vide, seul « » est acceptable
    VERIFIER(appliquer(R"({"choisi":"x"})", Registre{}).erreur == ErreurApi::Choix);
    VERIFIER(appliquer(R"({"spots":[],"choisi":"x"})", trois_spots()).erreur == ErreurApi::Choix);
    const Res vide = appliquer(R"({"spots":[],"choisi":""})", trois_spots());
    VERIFIER(vide.lisible && vide.erreur == ErreurApi::Aucune && vide.valeur.nombre == 0 && vide.valeur.choisi == 0);
    VERIFIER(appliquer(R"({"choisi":""})", Registre{}).erreur == ErreurApi::Aucune);
    char tampon[128] = {};
    VERIFIER(appliquer(corps_avec_texte(tampon, R"({"choisi":")", 'a', 80, R"("})"), trois_spots()).erreur ==
             ErreurApi::Choix); // plus long que le tampon de lecture
    return 0;
}

constexpr int une_liste_vide_coupe_la_mer_sans_toucher_a_la_plage() {
    Registre actuel = trois_spots();
    regler_vent(actuel, 10, 30);
    const Res r = appliquer(R"({"spots":[]})", actuel);
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.nombre == 0 && r.valeur.choisi == 0);
    VERIFIER(spot_choisi(r.valeur) == nullptr && !demande_du_registre(r.valeur).presente);
    VERIFIER(r.valeur.vent_min_kn == 10 && r.valeur.vent_max_kn == 30);
    return 0;
}

constexpr int spots_qui_n_est_pas_une_liste_d_objets_est_refuse() {
    for (const sv corps : {R"({"spots":"x"})", R"({"spots":{}})", R"({"spots":null})", R"({"spots":5})",
                           R"({"spots":[1]})", R"({"spots":["a"]})", R"({"spots":[[]]})", R"({"spots":[null]})"}) {
        const Res r = appliquer(corps, trois_spots());
        VERIFIER(r.lisible && r.erreur == ErreurApi::Spots && egaux(r.valeur, trois_spots()));
    }
    return 0;
}

constexpr int un_identifiant_absent_mal_ecrit_ou_trop_long_est_refuse() {
    for (const sv corps :
         {R"({"spots":[{"nom":"X"}]})", R"({"spots":[{"id":3,"nom":"X"}]})", R"({"spots":[{"id":null,"nom":"X"}]})",
          R"({"spots":[{"id":"","nom":"X"}]})", R"({"spots":[{"id":"Majuscule","nom":"X"}]})",
          R"({"spots":[{"id":"avec espace","nom":"X"}]})", R"({"spots":[{"id":"accentué","nom":"X"}]})",
          R"({"spots":[{"id":"a/b","nom":"X"}]})", R"({"spots":[{"id":"a\u0000b","nom":"X"}]})"}) {
        const Res r = appliquer(corps, trois_spots());
        VERIFIER(r.lisible && r.erreur == ErreurApi::Identifiant && egaux(r.valeur, trois_spots()));
    }
    char tampon[256] = {};
    // 64 octets : le maximum, accepté ; 65 : refusé ; 90 : refusé aussi (plus long que le tampon de lecture)
    VERIFIER(
        appliquer(corps_avec_texte(tampon, R"({"spots":[{"id":")", 'a', 64, R"(","nom":"X"}]})"), Registre{}).erreur ==
        ErreurApi::Aucune);
    VERIFIER(
        appliquer(corps_avec_texte(tampon, R"({"spots":[{"id":")", 'a', 65, R"(","nom":"X"}]})"), Registre{}).erreur ==
        ErreurApi::Identifiant);
    VERIFIER(
        appliquer(corps_avec_texte(tampon, R"({"spots":[{"id":")", 'a', 90, R"(","nom":"X"}]})"), Registre{}).erreur ==
        ErreurApi::Identifiant);
    // « - » et « _ » et les chiffres sont permis
    VERIFIER(appliquer(R"({"spots":[{"id":"plage-9_bis","nom":"X"}]})", Registre{}).erreur == ErreurApi::Aucune);
    return 0;
}

constexpr int un_nom_absent_vide_trop_long_ou_abime_est_refuse() {
    for (const sv corps : {R"({"spots":[{"id":"a"}]})", R"({"spots":[{"id":"a","nom":4}]})",
                           R"({"spots":[{"id":"a","nom":null}]})", R"({"spots":[{"id":"a","nom":""}]})",
                           R"({"spots":[{"id":"a","nom":"   "}]})", R"({"spots":[{"id":"a","nom":"deux\nlignes"}]})"}) {
        const Res r = appliquer(corps, trois_spots());
        VERIFIER(r.lisible && r.erreur == ErreurApi::Nom && egaux(r.valeur, trois_spots()));
    }
    char tampon[512] = {};
    // 32 caractères : le maximum ; 33 : refusé ; 200 octets : refusé aussi (plus long que le tampon de lecture)
    VERIFIER(
        appliquer(corps_avec_texte(tampon, R"({"spots":[{"id":"a","nom":")", 'n', 32, R"("}]})"), Registre{}).erreur ==
        ErreurApi::Aucune);
    VERIFIER(
        appliquer(corps_avec_texte(tampon, R"({"spots":[{"id":"a","nom":")", 'n', 33, R"("}]})"), Registre{}).erreur ==
        ErreurApi::Nom);
    VERIFIER(
        appliquer(corps_avec_texte(tampon, R"({"spots":[{"id":"a","nom":")", 'n', 200, R"("}]})"), Registre{}).erreur ==
        ErreurApi::Nom);
    // les caractères comptent, pas les octets : 32 « é » font 64 octets et passent
    char douze[4096] = {};
    VERIFIER(appliquer(corps_de_spots(douze, 1), Registre{}).erreur == ErreurApi::Aucune);
    return 0;
}

constexpr int deux_spots_de_meme_identifiant_sont_refuses() {
    Res r = appliquer(R"({"spots":[{"id":"a","nom":"A"},{"id":"a","nom":"A"}]})", Registre{});
    VERIFIER(r.lisible && r.erreur == ErreurApi::Doublon);
    r = appliquer(R"({"spots":[{"id":"a","nom":"Un nom"},{"id":"b","nom":"B"},{"id":"a","nom":"Un autre"}]})",
                  trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Doublon && egaux(r.valeur, trois_spots()));
    // le même NOM sous deux identifiants est permis (deux plages de même nom dans deux lieux)
    r = appliquer(R"({"spots":[{"id":"a","nom":"La Plage"},{"id":"b","nom":"La Plage"}]})", Registre{});
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.nombre == 2);
    return 0;
}

constexpr int douze_spots_passent_treize_sont_trop() {
    char corps[4096] = {};
    Res r = appliquer(corps_de_spots(corps, 12), Registre{});
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.nombre == 12 && registre_valide(r.valeur));
    VERIFIER(egal(r.valeur.spots[11].id, "spot-11"));
    r = appliquer(corps_de_spots(corps, 13), trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Plein && egaux(r.valeur, trois_spots()));
    r = appliquer(corps_de_spots(corps, 30), trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Plein);
    return 0;
}

constexpr int la_plage_de_vent_est_en_noeuds_entiers_et_ordonnee() {
    Res r = appliquer(R"({"vent_min_kn":10,"vent_max_kn":30})", Registre{});
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.vent_min_kn == 10 && r.valeur.vent_max_kn == 30);
    for (const sv corps : {R"({"vent_min_kn":12.5,"vent_max_kn":30})", R"({"vent_min_kn":10,"vent_max_kn":30.5})",
                           R"({"vent_min_kn":"10","vent_max_kn":30})", R"({"vent_min_kn":10,"vent_max_kn":null})",
                           R"({"vent_min_kn":20,"vent_max_kn":20})", R"({"vent_min_kn":30,"vent_max_kn":20})",
                           R"({"vent_min_kn":0,"vent_max_kn":20})", R"({"vent_min_kn":-5,"vent_max_kn":20})",
                           R"({"vent_min_kn":10,"vent_max_kn":61})", R"({"vent_min_kn":10,"vent_max_kn":1e2})"}) {
        r = appliquer(corps, trois_spots());
        VERIFIER(r.lisible && r.erreur == ErreurApi::Vent && egaux(r.valeur, trois_spots()));
    }
    r = appliquer(R"({"vent_min_kn":1,"vent_max_kn":60})", Registre{}); // les deux extrêmes permis
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.vent_min_kn == 1 && r.valeur.vent_max_kn == 60);
    return 0;
}

constexpr int un_seul_vent_envoye_est_juge_avec_celui_du_registre() {
    // registre à 12 et 25
    Res r = appliquer(R"({"vent_min_kn":24})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.vent_min_kn == 24 && r.valeur.vent_max_kn == 25);
    r = appliquer(R"({"vent_max_kn":60})", trois_spots());
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.vent_min_kn == 12 && r.valeur.vent_max_kn == 60);
    VERIFIER(appliquer(R"({"vent_min_kn":25})", trois_spots()).erreur == ErreurApi::Vent); // 25 n'est pas sous 25
    VERIFIER(appliquer(R"({"vent_max_kn":12})", trois_spots()).erreur == ErreurApi::Vent);
    VERIFIER(appliquer(R"({"vent_min_kn":12.5})", trois_spots()).erreur == ErreurApi::Vent);
    return 0;
}

constexpr int un_refus_n_applique_rien_meme_pour_les_champs_valides() {
    VERIFIER(
        egaux(appliquer(R"({"spots":[{"id":"a","nom":"A"}],"choisi":"nope"})", trois_spots()).valeur, trois_spots()));
    VERIFIER(
        egaux(appliquer(R"({"spots":[{"id":"a","nom":"A"}],"vent_min_kn":99})", trois_spots()).valeur, trois_spots()));
    VERIFIER(egaux(appliquer(R"({"choisi":"spot-trois","vent_min_kn":99})", trois_spots()).valeur, trois_spots()));
    VERIFIER(appliquer(R"({"spots":[{"id":"a","nom":"A"}],"vent_min_kn":99})", trois_spots()).erreur ==
             ErreurApi::Vent);
    // la liste est lue avant le choix : une liste refusée est le premier refus rendu
    VERIFIER(appliquer(R"({"spots":[{"id":"a"}],"choisi":"nope"})", trois_spots()).erreur == ErreurApi::Nom);
    return 0;
}

constexpr int les_champs_inconnus_sont_ignores() {
    const Res r = appliquer(R"({"truc":1,"spots":[{"id":"a","nom":"A","lieu":"ici","n":[1]}],"autre":{}})", Registre{});
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && r.valeur.nombre == 1);
    return 0;
}

constexpr int un_envoi_identique_a_l_etat_donne_un_registre_egal() {
    const Registre actuel = trois_spots();
    const Res r = appliquer(
        R"({"spots":[{"id":"spot-un","nom":"Un"},{"id":"spot-deux","nom":"Deux"},{"id":"spot-trois","nom":"Trois"}],)"
        R"("choisi":"spot-deux","vent_min_kn":12,"vent_max_kn":25})",
        actuel);
    VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && egaux(r.valeur, actuel)); // la route n'écrit alors rien
    return 0;
}

constexpr int tout_registre_accepte_est_un_registre_valide() {
    char douze[4096] = {};
    for (const sv corps :
         {R"({})", R"({"spots":[]})", R"({"choisi":"spot-un"})", R"({"vent_min_kn":3,"vent_max_kn":9})",
          R"({"spots":[{"id":"a","nom":"A"},{"id":"b","nom":"B"}],"choisi":"b","vent_max_kn":40})"}) {
        const Res r = appliquer(corps, trois_spots());
        VERIFIER(r.lisible && r.erreur == ErreurApi::Aucune && registre_valide(r.valeur));
    }
    const Res r = appliquer(corps_de_spots(douze, 12), trois_spots());
    VERIFIER(r.erreur == ErreurApi::Aucune && registre_valide(r.valeur));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(un_objet_vide_ne_change_rien)                                                                                    \
    X(la_liste_entiere_est_prise_dans_l_ordre_et_le_premier_est_choisi)                                                \
    X(la_nouvelle_liste_remplace_l_ancienne)                                                                           \
    X(sans_choisi_le_meme_spot_reste_choisi_s_il_est_toujours_la)                                                      \
    X(sans_choisi_et_le_spot_choisi_parti_le_premier_est_choisi)                                                       \
    X(choisi_nomme_un_spot_de_la_liste_telle_qu_elle_sera)                                                             \
    X(un_choix_hors_de_la_liste_ou_mal_forme_est_refuse)                                                               \
    X(une_liste_vide_coupe_la_mer_sans_toucher_a_la_plage)                                                             \
    X(spots_qui_n_est_pas_une_liste_d_objets_est_refuse)                                                               \
    X(un_identifiant_absent_mal_ecrit_ou_trop_long_est_refuse)                                                         \
    X(un_nom_absent_vide_trop_long_ou_abime_est_refuse)                                                                \
    X(deux_spots_de_meme_identifiant_sont_refuses)                                                                     \
    X(douze_spots_passent_treize_sont_trop)                                                                            \
    X(la_plage_de_vent_est_en_noeuds_entiers_et_ordonnee)                                                              \
    X(un_seul_vent_envoye_est_juge_avec_celui_du_registre)                                                             \
    X(un_refus_n_applique_rien_meme_pour_les_champs_valides)                                                           \
    X(les_champs_inconnus_sont_ignores)                                                                                \
    X(un_envoi_identique_a_l_etat_donne_un_registre_egal)                                                              \
    X(tout_registre_accepte_est_un_registre_valide)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
