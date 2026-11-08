// Tests du noyau pur : la liste des spots de Vigie lue par l'appareil et la recherche dedans (mer_liste.hpp, ADR-0026).
// Les réponses d'essai sont INVENTÉES (noms et lieux imaginaires) : aucune donnée de spot réelle n'est dans le dépôt (ADR-0025). Leur
// forme est celle de `GET /v1/surf/spots` relue le 08/11/2026 : un objet avec `spots`, des champs en plus que la lecture ignore.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_liste.hpp"
#include "mini.hpp"

using namespace ecran::mer;
using namespace ecran::mer::liste;
using sv = std::string_view;

namespace {

constexpr sv REPONSE = R"({"count":3,"built_utc":"2026-11-08T00:00:00Z","spots":[
 {"id":"plage-du-phare","name":"Plage du Phare","osm_name":null,"place":"Port-Exemple","lat":48.1,"lon":-3.2,
  "facing_deg":270,"evidence":[{"osm":"node/1","name":"École","kind":"school"}]},
 {"id":"anse-des-mouettes","name":"Anse des Mouettes","osm_name":"L'Anse","place":"Saint-Imaginaire","lat":47.9,"lon":-3.0},
 {"id":"baie-d-ys","name":null,"osm_name":"Baie d'Ys","place":null,"lat":47.0,"lon":-2.0}
],"licence":"ODbL","attribution":"fictive"})";

// Tampon de texte d'essai : permet de construire une réponse de n spots à la compilation.
struct Texte {
    char t[16384] = {};
    std::size_t n = 0;
    constexpr void ajoute(sv s) {
        for (const char c : s)
            t[n++] = c;
    }
    constexpr void ajoute_nombre(std::size_t v) {
        char chiffres[8] = {};
        std::size_t k = 0;
        do {
            chiffres[k++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v != 0);
        while (k > 0)
            t[n++] = chiffres[--k];
    }
    constexpr sv vue() const { return sv(t, n); }
};

// `nb` spots « spot-<i> » de nom « Spot <i> » et de lieu « Lieu <i> ».
constexpr Texte reponse_de(std::size_t nb) {
    Texte x;
    x.ajoute(R"({"spots":[)");
    for (std::size_t i = 0; i < nb; ++i) {
        if (i != 0)
            x.ajoute(",");
        x.ajoute(R"({"id":"spot-)");
        x.ajoute_nombre(i);
        x.ajoute(R"(","name":"Spot )");
        x.ajoute_nombre(i);
        x.ajoute(R"(","place":"Lieu )");
        x.ajoute_nombre(i);
        x.ajoute(R"("})");
    }
    x.ajoute("]}");
    return x;
}

constexpr Liste lire(sv json, Issue &issue) {
    Liste l;
    issue = analyser(json, l);
    return l;
}

// ---- replier -------------------------------------------------------------------------------------------------------------------------

constexpr int le_repli_ramene_les_accents_et_les_majuscules_a_la_lettre_de_base() {
    char sortie[64] = {};
    std::size_t n = 0;
    VERIFIER(replier("Évian-Les-Bains", sortie, sizeof sortie, n) && sv(sortie, n) == "evian-les-bains");
    VERIFIER(replier("ÀÂÄÇÈÉÊËÎÏÔÖÙÛÜÑ", sortie, sizeof sortie, n) && sv(sortie, n) == "aaaceeeeiioouuun");
    VERIFIER(replier("àâäçèéêëîïôöùûüñ", sortie, sizeof sortie, n) && sv(sortie, n) == "aaaceeeeiioouuun");
    VERIFIER(replier("Œuf Æ ß Ø", sortie, sizeof sortie, n) && sv(sortie, n) == "oeuf ae ss o");
    VERIFIER(replier("Şile Çeşme Ğ ı İ", sortie, sizeof sortie, n) && sv(sortie, n) == "sile cesme g i i");
    VERIFIER(replier("Łódź Žilina Ō", sortie, sizeof sortie, n) && sv(sortie, n) == "lodz zilina o");
    return 0;
}

constexpr int le_repli_laisse_passer_le_reste() {
    char sortie[64] = {};
    std::size_t n = 0;
    VERIFIER(replier("spot-12_b", sortie, sizeof sortie, n) && sv(sortie, n) == "spot-12_b");
    VERIFIER(replier("日本 ♥", sortie, sizeof sortie, n) && sv(sortie, n) == "日本 ♥"); // hors du latin : tel quel
    VERIFIER(replier("", sortie, sizeof sortie, n) && n == 0);
    // « × » et « ÷ » ne sont pas des lettres : ils disparaissent.
    VERIFIER(replier("a×b÷c", sortie, sizeof sortie, n) && sv(sortie, n) == "abc");
    // Un octet isolé ou une séquence coupée ne fait pas planter : passés tels quels.
    VERIFIER(replier(sv("\xC3", 1), sortie, sizeof sortie, n) && n == 1);
    VERIFIER(replier(sv("a\xC5", 2), sortie, sizeof sortie, n) && n == 2);
    return 0;
}

constexpr int le_repli_refuse_un_tampon_trop_petit() {
    char sortie[4] = {};
    std::size_t n = 0;
    VERIFIER(replier("abc", sortie, sizeof sortie, n));   // 3 caractères et le zéro : juste
    VERIFIER(!replier("abcd", sortie, sizeof sortie, n)); // un de trop
    VERIFIER(!replier("abc", sortie, 3, n));              // pas de place pour le zéro final
    VERIFIER(!replier("é", sortie, 0, n));
    VERIFIER(!replier("", sortie, 0, n)); // même un texte vide a besoin de la place du zéro final
    return 0;
}

// ---- analyser ------------------------------------------------------------------------------------------------------------------------

constexpr int une_reponse_ordinaire_donne_id_nom_et_lieu() {
    Issue issue = Issue::Illisible;
    const Liste l = lire(REPONSE, issue);
    VERIFIER(issue == Issue::Ok);
    VERIFIER(l.nombre == 3 && !l.tronquee);
    VERIFIER(id_du_spot(l.spots[0]) == "plage-du-phare" && nom_du_spot(l.spots[0]) == "Plage du Phare");
    VERIFIER(lieu_du_spot(l.spots[0]) == "Port-Exemple");
    VERIFIER(id_du_spot(l.spots[1]) == "anse-des-mouettes" && nom_du_spot(l.spots[1]) == "Anse des Mouettes");
    VERIFIER(lieu_du_spot(l.spots[1]) == "Saint-Imaginaire");
    return 0;
}

constexpr int le_nom_retombe_sur_osm_name_puis_sur_le_lieu_puis_sur_l_identifiant() {
    Issue issue = Issue::Illisible;
    // `name` absent ou nul → `osm_name` (le troisième spot de REPONSE).
    const Liste l = lire(REPONSE, issue);
    VERIFIER(nom_du_spot(l.spots[2]) == "Baie d'Ys" && lieu_du_spot(l.spots[2]).empty());
    const Liste m = lire(R"({"spots":[{"id":"a-b","name":null,"osm_name":"","place":"Ker-Imaginaire"}]})", issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(m.spots[0]) == "Ker-Imaginaire"); // → `place`
    const Liste n = lire(R"({"spots":[{"id":"a-b","name":"  ","osm_name":null,"place":null}]})", issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(n.spots[0]) == "a-b" &&
             lieu_du_spot(n.spots[0]).empty());                 // → l'identifiant
    const Liste o = lire(R"({"spots":[{"id":"a-b"}]})", issue); // aucun des trois champs
    VERIFIER(issue == Issue::Ok && nom_du_spot(o.spots[0]) == "a-b");
    // Un nom qui n'est pas du texte (un nombre) passe son tour.
    const Liste p = lire(R"({"spots":[{"id":"a-b","name":42,"osm_name":"Vrai nom"}]})", issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(p.spots[0]) == "Vrai nom");
    return 0;
}

constexpr int les_espaces_autour_des_textes_sont_retires() {
    Issue issue = Issue::Illisible;
    const Liste l = lire(R"({"spots":[{"id":"a-b","name":"  Plage du Phare \n","place":"\tPort-Exemple "}]})", issue);
    VERIFIER(issue == Issue::Ok);
    VERIFIER(nom_du_spot(l.spots[0]) == "Plage du Phare" && lieu_du_spot(l.spots[0]) == "Port-Exemple");
    return 0;
}

constexpr int un_nom_trop_long_est_coupe_a_32_caracteres_sur_une_frontiere_de_caractere() {
    Issue issue = Issue::Illisible;
    // 40 lettres : coupé à 32.
    const Liste l = lire(R"({"spots":[{"id":"a-b","name":"abcdefghijklmnopqrstuvwxyzabcdefghijklmn"}]})", issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(l.spots[0]) == "abcdefghijklmnopqrstuvwxyzabcdef");
    // 40 « é » (2 octets chacun) : 32 caractères = 64 octets, jamais un « é » coupé en deux.
    const Liste m = lire(R"({"spots":[{"id":"a-b","name":"éééééééééééééééééééééééééééééééééééééééé"}]})", issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(m.spots[0]).size() == 64);
    VERIFIER(ecran::surf::nom_de_spot_valide(m.spots[0].nom, sizeof m.spots[0].nom));
    // « éa » vingt fois : 40 caractères, 60 octets (un caractère de 2 octets suivi d'un de 1) → les 16 premiers « éa », 48 octets.
    const Liste f = lire(R"({"spots":[{"id":"a-b","name":"éaéaéaéaéaéaéaéaéaéaéaéaéaéaéaéaéaéaéaéa"}]})", issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(f.spots[0]).size() == 48);
    // 40 émojis (4 octets chacun) : 32 caractères = 128 octets, le plus grand nom possible.
    const Liste e = lire(
        R"({"spots":[{"id":"a-b","name":"😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀"}]})",
        issue);
    VERIFIER(issue == Issue::Ok && nom_du_spot(e.spots[0]).size() == 128);
    // Ce que la liste garde, le registre l'accepte tel quel.
    Registre r;
    VERIFIER(ajouter_de_la_liste(r, m.spots[0]) == ErreurRegistre::Aucune);
    return 0;
}

constexpr int les_sequences_d_echappement_json_sont_decodees() {
    Issue issue = Issue::Illisible;
    const Liste l = lire(R"({"spots":[{"id":"a-b","name":"Plénitude \"Sud\"","place":"Saint-Clément"}]})", issue);
    VERIFIER(issue == Issue::Ok);
    VERIFIER(nom_du_spot(l.spots[0]) == "Plénitude \"Sud\"" && lieu_du_spot(l.spots[0]) == "Saint-Clément");
    return 0;
}

constexpr int un_lieu_trop_long_est_coupe_a_32_caracteres_et_64_octets() {
    Issue issue = Issue::Illisible;
    // 45 « é » = 90 octets : 32 « é » entiers (64 octets), jamais un « é » coupé en deux.
    const Liste l =
        lire(R"({"spots":[{"id":"a-b","name":"X","place":"ééééééééééééééééééééééééééééééééééééééééééééé"}]})", issue);
    VERIFIER(issue == Issue::Ok);
    VERIFIER(lieu_du_spot(l.spots[0]).size() == 64);
    // 40 lettres : 32 caractères (le lieu n'est pas vidé parce qu'il dépasse la règle des noms).
    const Liste m =
        lire(R"({"spots":[{"id":"a-b","name":"X","place":"abcdefghijklmnopqrstuvwxyzabcdefghijklmn"}]})", issue);
    VERIFIER(issue == Issue::Ok && lieu_du_spot(m.spots[0]) == "abcdefghijklmnopqrstuvwxyzabcdef");
    // 40 « 日 » (3 octets chacun) : la limite de 64 octets arrive avant celle de 32 caractères → 21 caractères, 63 octets.
    const Liste n = lire(
        R"({"spots":[{"id":"a-b","name":"X","place":"日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日日"}]})",
        issue);
    VERIFIER(issue == Issue::Ok && lieu_du_spot(n.spots[0]).size() == 63);
    return 0;
}

constexpr int un_lieu_abime_est_laisse_vide_sans_ecarter_le_spot() {
    Issue issue = Issue::Illisible;
    const Liste l =
        lire(R"({"spots":[{"id":"a-b","name":"Plage","place":"Port\u0001Exemple"}]})", issue); // caractère de contrôle
    VERIFIER(issue == Issue::Ok && l.nombre == 1);
    VERIFIER(nom_du_spot(l.spots[0]) == "Plage" && lieu_du_spot(l.spots[0]).empty());
    return 0;
}

constexpr int un_identifiant_invalide_ou_double_ecarte_le_spot_et_pas_les_autres() {
    Issue issue = Issue::Illisible;
    const Liste l = lire(R"({"spots":[
        {"id":"bon-un","name":"Un"},
        {"id":"Mauvais Id","name":"Deux"},
        {"name":"Sans identifiant"},
        {"id":7,"name":"Identifiant nombre"},
        {"id":"","name":"Identifiant vide"},
        {"id":"bon-un","name":"Un encore"},
        {"id":"bon-trois","name":"Trois"}
    ]})",
                         issue);
    VERIFIER(issue == Issue::Ok && l.nombre == 2);
    VERIFIER(id_du_spot(l.spots[0]) == "bon-un" && nom_du_spot(l.spots[0]) == "Un"); // le premier doublon gagne
    VERIFIER(id_du_spot(l.spots[1]) == "bon-trois");
    return 0;
}

constexpr int un_identifiant_de_plus_de_64_octets_est_ecarte() {
    Issue issue = Issue::Illisible;
    // 64 octets : accepté ; 65 : écarté.
    const Liste l = lire(R"({"spots":[
        {"id":"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl","name":"Soixante-quatre"},
        {"id":"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklm","name":"Soixante-cinq"}
    ]})",
                         issue);
    VERIFIER(issue == Issue::Ok && l.nombre == 1 && nom_du_spot(l.spots[0]) == "Soixante-quatre");
    return 0;
}

constexpr int un_element_qui_n_est_pas_un_objet_est_ecarte() {
    Issue issue = Issue::Illisible;
    const Liste l = lire(R"({"spots":[1,"deux",null,[3],{"id":"bon","name":"Bon"}]})", issue);
    VERIFIER(issue == Issue::Ok && l.nombre == 1 && id_du_spot(l.spots[0]) == "bon");
    return 0;
}

constexpr int une_reponse_qui_n_est_pas_une_liste_de_spots_est_illisible() {
    Liste l;
    VERIFIER(analyser("", l) == Issue::Illisible);
    VERIFIER(analyser("pas du json", l) == Issue::Illisible);
    VERIFIER(analyser(R"({"spots":[)", l) == Issue::Illisible);              // JSON coupé
    VERIFIER(analyser(R"([{"id":"a","name":"A"}])", l) == Issue::Illisible); // la liste sans l'objet autour
    VERIFIER(analyser(R"({"count":1})", l) == Issue::Illisible);             // pas de `spots`
    VERIFIER(analyser(R"({"spots":{"id":"a"}})", l) == Issue::Illisible);    // `spots` n'est pas une liste
    VERIFIER(analyser(R"({"spots":"a"})", l) == Issue::Illisible);
    VERIFIER(analyser("null", l) == Issue::Illisible);
    return 0;
}

constexpr int une_liste_sans_spot_utilisable_est_vide() {
    Liste l;
    VERIFIER(analyser(R"({"spots":[]})", l) == Issue::Vide && l.nombre == 0);
    VERIFIER(analyser(R"({"spots":[{"id":"Mauvais Id"},{"name":"x"},3]})", l) == Issue::Vide && l.nombre == 0);
    return 0;
}

constexpr int une_nouvelle_lecture_remplace_la_precedente() {
    Liste l;
    VERIFIER(analyser(REPONSE, l) == Issue::Ok && l.nombre == 3);
    VERIFIER(analyser(R"({"spots":[{"id":"seul","name":"Seul"}]})", l) == Issue::Ok && l.nombre == 1);
    VERIFIER(id_du_spot(l.spots[0]) == "seul");
    // Un échec de lecture ne laisse pas croire que l'ancienne liste vaut encore.
    VERIFIER(analyser("???", l) == Issue::Illisible && l.nombre == 0);
    return 0;
}

constexpr int soixante_quatre_spots_passent_le_soixante_cinquieme_est_ignore() {
    Liste l;
    const Texte juste = reponse_de(SPOTS_MAX);
    VERIFIER(analyser(juste.vue(), l) == Issue::Ok && l.nombre == SPOTS_MAX && !l.tronquee);
    VERIFIER(id_du_spot(l.spots[SPOTS_MAX - 1]) == "spot-63");
    const Texte trop = reponse_de(SPOTS_MAX + 1);
    VERIFIER(analyser(trop.vue(), l) == Issue::Ok && l.nombre == SPOTS_MAX && l.tronquee);
    VERIFIER(id_du_spot(l.spots[SPOTS_MAX - 1]) == "spot-63"); // le 65e n'a pas pris la place du 64e
    return 0;
}

// ---- chercher ------------------------------------------------------------------------------------------------------------------------

constexpr int la_recherche_ne_tient_compte_ni_des_majuscules_ni_des_accents() {
    Issue issue = Issue::Illisible;
    const Liste l = lire(R"({"spots":[
        {"id":"a-1","name":"Plage de l'Étang","place":"Saint-Clément"},
        {"id":"b-2","name":"Anse Océane","place":"Lorient"},
        {"id":"c-3","name":"La Grande Plage","place":"Çeşme-Imaginaire"}
    ]})",
                         issue);
    VERIFIER(issue == Issue::Ok);
    std::uint8_t r[8] = {};
    VERIFIER(chercher(l, "etang", r, 8) == 1 && r[0] == 0); // sans accent trouve avec accent
    VERIFIER(chercher(l, "ÉTANG", r, 8) == 1 && r[0] == 0); // avec accent et majuscules trouve aussi
    VERIFIER(chercher(l, "oceane", r, 8) == 1 && r[0] == 1);
    VERIFIER(chercher(l, "clement", r, 8) == 1 && r[0] == 0);            // dans le lieu
    VERIFIER(chercher(l, "cesme", r, 8) == 1 && r[0] == 2);              // lieu à lettre turque
    VERIFIER(chercher(l, "c-3", r, 8) == 1 && r[0] == 2);                // dans l'identifiant
    VERIFIER(chercher(l, "plage", r, 8) == 2 && r[0] == 0 && r[1] == 2); // deux résultats, dans l'ordre de la liste
    VERIFIER(chercher(l, "introuvable", r, 8) == 0);
    return 0;
}

constexpr int une_recherche_vide_donne_tous_les_spots_dans_l_ordre() {
    Liste l;
    VERIFIER(analyser(REPONSE, l) == Issue::Ok);
    std::uint8_t r[8] = {};
    VERIFIER(chercher(l, "", r, 8) == 3 && r[0] == 0 && r[1] == 1 && r[2] == 2);
    VERIFIER(chercher(l, "   ", r, 8) == 3); // des espaces seuls ne filtrent rien
    return 0;
}

constexpr int les_espaces_autour_de_la_recherche_sont_ignores() {
    Liste l;
    VERIFIER(analyser(REPONSE, l) == Issue::Ok);
    std::uint8_t r[8] = {};
    VERIFIER(chercher(l, "  mouettes ", r, 8) == 1 && r[0] == 1);
    return 0;
}

constexpr int la_recherche_s_arrete_a_max_resultats() {
    Liste l;
    const Texte x = reponse_de(10);
    VERIFIER(analyser(x.vue(), l) == Issue::Ok);
    std::uint8_t r[8] = {};
    VERIFIER(chercher(l, "spot", r, 3) == 3 && r[0] == 0 && r[2] == 2);
    VERIFIER(chercher(l, "spot", r, 0) == 0);
    VERIFIER(chercher(l, "lieu 7", r, 8) == 1 && r[0] == 7);
    return 0;
}

constexpr int une_recherche_trop_longue_ne_trouve_rien() {
    Liste l;
    VERIFIER(analyser(REPONSE, l) == Issue::Ok);
    std::uint8_t r[8] = {};
    // 97 lettres : une de plus que RECHERCHE_OCTETS.
    constexpr sv longue =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static_assert(longue.size() == RECHERCHE_OCTETS + 1);
    VERIFIER(chercher(l, longue, r, 8) == 0);
    return 0;
}

constexpr int la_recherche_dans_une_liste_vide_ne_trouve_rien() {
    Liste l;
    std::uint8_t r[8] = {};
    VERIFIER(chercher(l, "", r, 8) == 0);
    VERIFIER(chercher(l, "x", r, 8) == 0);
    return 0;
}

// ---- la requête et le rythme -----------------------------------------------------------------------------------------------------------

constexpr int la_requete_est_l_adresse_de_vigie_suivie_de_la_route_des_spots() {
    char sortie[TAILLE_REQUETE_LISTE_MAX] = {};
    const std::size_t n = construire_requete_liste("https://vigie.exemple.test", sortie, sizeof sortie);
    VERIFIER(sv(sortie, n) == "https://vigie.exemple.test/v1/surf/spots" && sortie[n] == '\0');
    // Une adresse avec un chemin et un port est reprise telle quelle (vigie::normaliser_adresse les accepte).
    const std::size_t m = construire_requete_liste("https://vigie.exemple.test:8443/vigie", sortie, sizeof sortie);
    VERIFIER(sv(sortie, m) == "https://vigie.exemple.test:8443/vigie/v1/surf/spots");
    // Rien d'autre ne part : ni position, ni identifiant.
    VERIFIER(sv(sortie, m).find('?') == sv::npos);
    return 0;
}

constexpr int la_requete_refuse_une_adresse_vide_trop_longue_ou_un_tampon_trop_petit() {
    char sortie[TAILLE_REQUETE_LISTE_MAX] = {};
    VERIFIER(construire_requete_liste("", sortie, sizeof sortie) == 0);
    VERIFIER(construire_requete_liste("https://vigie.exemple.test", nullptr, 100) == 0);
    VERIFIER(construire_requete_liste("https://vigie.exemple.test", sortie, 0) == 0);
    // 80 octets : la plus longue adresse acceptée ; 81 : refusée.
    Texte quatre_vingts;
    quatre_vingts.ajoute("https://");
    while (quatre_vingts.n < ecran::vigie::ADRESSE_MAX)
        quatre_vingts.ajoute("a");
    const std::size_t n = construire_requete_liste(quatre_vingts.vue(), sortie, sizeof sortie);
    VERIFIER(n == ecran::vigie::ADRESSE_MAX + REQUETE_LISTE.size());
    Texte quatre_vingt_un = quatre_vingts;
    quatre_vingt_un.ajoute("a");
    char trop[TAILLE_REQUETE_LISTE_MAX + 1] = {};
    VERIFIER(construire_requete_liste(quatre_vingt_un.vue(), trop, sizeof trop) == 0);
    // Un tampon d'un octet trop court : refusé, jamais une adresse coupée.
    char court[40] = {};
    VERIFIER(construire_requete_liste("https://vigie.exemple.test", court, sizeof court) == 0);
    char juste[41] = {}; // « https://vigie.exemple.test/v1/surf/spots » = 40 octets et le zéro final
    VERIFIER(construire_requete_liste("https://vigie.exemple.test", juste, sizeof juste) == 40);
    return 0;
}

constexpr int la_liste_gardee_moins_de_dix_minutes_n_est_pas_retelechargee() {
    VERIFIER(!relire_la_liste(true, 0, false, 0));
    VERIFIER(!relire_la_liste(true, LISTE_VALABLE_S - 1, false, 1000));
    VERIFIER(relire_la_liste(true, LISTE_VALABLE_S, false, 1000)); // dix minutes pleines : on relit
    VERIFIER(relire_la_liste(true, 3600, false, 3600));
    // Sans liste gardée, l'âge de la liste ne dit rien.
    VERIFIER(relire_la_liste(false, 0, false, 1000));
    // Une horloge qui recule (âge négatif) ne fait pas garder une liste douteuse.
    VERIFIER(relire_la_liste(true, -1, false, 1000));
    return 0;
}

constexpr int apres_un_echec_on_attend_quelques_secondes_avant_de_reessayer() {
    VERIFIER(!relire_la_liste(false, 0, true, 0));
    VERIFIER(!relire_la_liste(false, 0, true, PAUSE_APRES_ECHEC_S - 1));
    VERIFIER(relire_la_liste(false, 0, true, PAUSE_APRES_ECHEC_S)); // la pause est finie
    // Un essai réussi il y a une seconde ne retient personne (sans liste gardée, on relit tout de suite).
    VERIFIER(relire_la_liste(false, 0, false, 1));
    VERIFIER(relire_la_liste(false, 0, true, -1)); // horloge qui recule : on réessaie
    return 0;
}

// ---- ce que fait un toucher ----------------------------------------------------------------------------------------------------------

constexpr int ajouter_un_spot_de_la_liste_le_met_au_registre_avec_son_nom() {
    Liste l;
    VERIFIER(analyser(REPONSE, l) == Issue::Ok);
    Registre r;
    VERIFIER(!dans_le_registre(r, l.spots[1]));
    VERIFIER(ajouter_de_la_liste(r, l.spots[1]) == ErreurRegistre::Aucune);
    VERIFIER(r.nombre == 1 && r.choisi == 0); // le premier devient le spot choisi
    VERIFIER(id_du_spot(r.spots[0]) == "anse-des-mouettes" && nom_du_spot(r.spots[0]) == "Anse des Mouettes");
    VERIFIER(dans_le_registre(r, l.spots[1]) && !dans_le_registre(r, l.spots[0]));
    // Le deuxième ne bouge pas le choix.
    VERIFIER(ajouter_de_la_liste(r, l.spots[0]) == ErreurRegistre::Aucune && r.nombre == 2 && r.choisi == 0);
    return 0;
}

constexpr int ajouter_deux_fois_le_meme_spot_ou_un_treizieme_est_refuse() {
    Liste l;
    const Texte x = reponse_de(14);
    VERIFIER(analyser(x.vue(), l) == Issue::Ok);
    Registre r;
    VERIFIER(ajouter_de_la_liste(r, l.spots[0]) == ErreurRegistre::Aucune);
    VERIFIER(ajouter_de_la_liste(r, l.spots[0]) == ErreurRegistre::Doublon && r.nombre == 1); // rien n'a changé
    for (std::size_t i = 1; i < REGISTRE_MAX; ++i)
        VERIFIER(ajouter_de_la_liste(r, l.spots[i]) == ErreurRegistre::Aucune);
    VERIFIER(r.nombre == REGISTRE_MAX);
    VERIFIER(ajouter_de_la_liste(r, l.spots[REGISTRE_MAX]) == ErreurRegistre::Plein && r.nombre == REGISTRE_MAX);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(le_repli_ramene_les_accents_et_les_majuscules_a_la_lettre_de_base)                                               \
    X(le_repli_laisse_passer_le_reste)                                                                                 \
    X(le_repli_refuse_un_tampon_trop_petit)                                                                            \
    X(une_reponse_ordinaire_donne_id_nom_et_lieu)                                                                      \
    X(le_nom_retombe_sur_osm_name_puis_sur_le_lieu_puis_sur_l_identifiant)                                             \
    X(les_espaces_autour_des_textes_sont_retires)                                                                      \
    X(un_nom_trop_long_est_coupe_a_32_caracteres_sur_une_frontiere_de_caractere)                                       \
    X(les_sequences_d_echappement_json_sont_decodees)                                                                  \
    X(un_lieu_trop_long_est_coupe_a_32_caracteres_et_64_octets)                                                        \
    X(un_lieu_abime_est_laisse_vide_sans_ecarter_le_spot)                                                              \
    X(un_identifiant_invalide_ou_double_ecarte_le_spot_et_pas_les_autres)                                              \
    X(un_identifiant_de_plus_de_64_octets_est_ecarte)                                                                  \
    X(un_element_qui_n_est_pas_un_objet_est_ecarte)                                                                    \
    X(une_reponse_qui_n_est_pas_une_liste_de_spots_est_illisible)                                                      \
    X(une_liste_sans_spot_utilisable_est_vide)                                                                         \
    X(une_nouvelle_lecture_remplace_la_precedente)                                                                     \
    X(soixante_quatre_spots_passent_le_soixante_cinquieme_est_ignore)                                                  \
    X(la_recherche_ne_tient_compte_ni_des_majuscules_ni_des_accents)                                                   \
    X(une_recherche_vide_donne_tous_les_spots_dans_l_ordre)                                                            \
    X(les_espaces_autour_de_la_recherche_sont_ignores)                                                                 \
    X(la_recherche_s_arrete_a_max_resultats)                                                                           \
    X(une_recherche_trop_longue_ne_trouve_rien)                                                                        \
    X(la_recherche_dans_une_liste_vide_ne_trouve_rien)                                                                 \
    X(la_requete_est_l_adresse_de_vigie_suivie_de_la_route_des_spots)                                                  \
    X(la_requete_refuse_une_adresse_vide_trop_longue_ou_un_tampon_trop_petit)                                          \
    X(la_liste_gardee_moins_de_dix_minutes_n_est_pas_retelechargee)                                                    \
    X(apres_un_echec_on_attend_quelques_secondes_avant_de_reessayer)                                                   \
    X(ajouter_un_spot_de_la_liste_le_met_au_registre_avec_son_nom)                                                     \
    X(ajouter_deux_fois_le_meme_spot_ou_un_treizieme_est_refuse)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
