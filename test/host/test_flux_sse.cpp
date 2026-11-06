// Tests du noyau pur : décodeur incrémental de flux SSE (flux_sse.hpp).
#include "ecran/assistant_texte.hpp"
#include "ecran/flux_sse.hpp"
#include "mini.hpp"

using ecran::assistant::DecodeurSse;
using ecran::assistant::EvenementSse;
namespace utf8 = ecran::assistant::utf8;

namespace {

constexpr bool egal(std::string_view a, std::string_view b) { return a == b; }

// Enregistre les événements reçus sous forme d'un texte comparable : type US données US drapeaux RS.
struct Enregistreur {
    char t[2048] = {};
    std::size_t n = 0;
    std::size_t nb = 0;
    bool utf8_valide = true;
    constexpr void ajouter(std::string_view s) {
        for (char c : s) {
            if (n < sizeof(t))
                t[n++] = c;
        }
    }
    constexpr void operator()(const EvenementSse &e) {
        ajouter(e.type);
        ajouter("\x1f");
        ajouter(e.donnees);
        ajouter("\x1f");
        ajouter(e.fin ? "F" : "-");
        ajouter(e.trop_long ? "T" : "-");
        ajouter("\x1e");
        ++nb;
        utf8_valide = utf8_valide && utf8::valide(e.donnees);
    }
    constexpr std::string_view vue() const { return std::string_view(t, n); }
};

using Decodeur = DecodeurSse<256>;

// Flux d'essai qui couvre : JSON à la façon d'OpenAI avec accents/euro/émoji (octets multiples), CRLF, commentaire,
// data sur plusieurs lignes, type d'événement, ligne vide multiple et [DONE].
constexpr std::string_view FLUX = ": keep-alive\r\n\r\n"
                                  "data: {\"c\":\"é€😀\"}\r\n\r\n"
                                  "event: message\n"
                                  "data: l1\n"
                                  "data: l2\r"
                                  "\r"
                                  "id: 7\n"
                                  "data:x\n"
                                  "\n"
                                  "\n"
                                  "data: [DONE]\r\n\r\n"
                                  "data: ignoré\n\n";

constexpr int jouer_en_un_morceau(std::string_view flux, Enregistreur &rec, Decodeur &d) {
    d.alimenter(flux, rec);
    return 0;
}

// --- cas simples -------------------------------------------------------------------------------------------

constexpr int flux_d_openai_avec_done() {
    Decodeur d;
    Enregistreur rec;
    const std::size_t n = d.alimenter("data: {\"choices\":[{\"delta\":{\"content\":\"Bon\"}}]}\n\n"
                                      "data: {\"choices\":[{\"delta\":{\"content\":\"jour\"}}]}\n\n"
                                      "data: [DONE]\n\n",
                                      rec);
    VERIFIER(n == 3 && rec.nb == 3);
    VERIFIER(d.termine());
    VERIFIER(egal(rec.vue(), "\x1f{\"choices\":[{\"delta\":{\"content\":\"Bon\"}}]}\x1f--\x1e"
                             "\x1f{\"choices\":[{\"delta\":{\"content\":\"jour\"}}]}\x1f--\x1e"
                             "\x1f[DONE]\x1f"
                             "F-\x1e"));
    return 0;
}

constexpr int data_sur_plusieurs_lignes_est_joint_par_un_retour_a_la_ligne() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter("data: a\ndata: b\ndata: c\n\n", rec);
    VERIFIER(rec.nb == 1);
    VERIFIER(egal(rec.vue(), "\x1f"
                             "a\nb\nc\x1f--\x1e"));
    return 0;
}

constexpr int les_trois_fins_de_ligne_sont_equivalentes() {
    for (std::string_view flux :
         {std::string_view("data: x\n\ndata: y\n\n"), std::string_view("data: x\r\n\r\ndata: y\r\n\r\n"),
          std::string_view("data: x\r\rdata: y\r\r"), std::string_view("data: x\r\n\ndata: y\n\r\n")}) {
        Decodeur d;
        Enregistreur rec;
        d.alimenter(flux, rec);
        VERIFIER(rec.nb == 2);
        VERIFIER(egal(rec.vue(), "\x1f"
                                 "x\x1f--\x1e\x1f"
                                 "y\x1f--\x1e"));
    }
    // et une fin de ligne CRLF ne sépare pas deux lignes data d'un même événement
    for (std::string_view flux : {std::string_view("data: x\r\ndata: y\r\n\r\n"),
                                  std::string_view("data: x\ndata: y\n\n"), std::string_view("data: x\rdata: y\r\r")}) {
        Decodeur d;
        Enregistreur rec;
        d.alimenter(flux, rec);
        VERIFIER(rec.nb == 1);
        VERIFIER(egal(rec.vue(), "\x1f"
                                 "x\ny\x1f--\x1e"));
    }
    return 0;
}

constexpr int cr_et_lf_dans_deux_morceaux_ne_font_qu_une_fin_de_ligne() {
    // « data: x\r » | « \n » | « \r\n » : si le LF isolé comptait pour une ligne vide, l'événement partirait trop tôt.
    Decodeur d;
    Enregistreur rec;
    d.alimenter("data: x\r", rec);
    VERIFIER(rec.nb == 0);
    d.alimenter("\n", rec);
    VERIFIER(rec.nb == 0); // toujours pas de ligne vide
    d.alimenter("\r", rec);
    VERIFIER(rec.nb == 1);                 // la ligne vide (CR) expédie
    d.alimenter("\ndata: y\r\n\r\n", rec); // son LF est avalé, puis un second événement
    VERIFIER(rec.nb == 2);
    VERIFIER(egal(rec.vue(), "\x1f"
                             "x\x1f--\x1e\x1f"
                             "y\x1f--\x1e"));
    return 0;
}

constexpr int commentaires_et_champs_inconnus_sont_ignores() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter(": ping\n\n:\n\n: encore\ndata: z\nid: 5\nretry: 100\nfoo: bar\ndataa: non\nfoo\n\n", rec);
    VERIFIER(rec.nb == 1);
    VERIFIER(egal(rec.vue(), "\x1f"
                             "z\x1f--\x1e"));
    // un commentaire entre deux lignes data n'ajoute rien à l'événement
    Decodeur e;
    Enregistreur r2;
    e.alimenter("data: a\n: commentaire\ndata: b\n\n", r2);
    VERIFIER(egal(r2.vue(), "\x1f"
                            "a\nb\x1f--\x1e"));
    // un nom de champ bien plus long que le tampon de nom est ignoré (sans déborder)
    Decodeur f;
    Enregistreur r3;
    f.alimenter("abcdefghijklmnopqrstuvwxyz: non\ndatadatadatadata: non\ndata: oui\n\n", r3);
    VERIFIER(egal(r3.vue(), "\x1f"
                            "oui\x1f--\x1e"));
    return 0;
}

constexpr int un_seul_espace_apres_les_deux_points_est_retire() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter("data:a\n\ndata: b\n\ndata:  c\n\ndata:\td\n\n", rec);
    VERIFIER(egal(rec.vue(), "\x1f"
                             "a\x1f--\x1e\x1f"
                             "b\x1f--\x1e\x1f"
                             " c\x1f--\x1e\x1f"
                             "\td\x1f--\x1e"));
    return 0;
}

constexpr int deux_points_dans_la_valeur_sont_conserves() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter("data: {\"a\":\"b:c\"}\n\n", rec);
    VERIFIER(egal(rec.vue(), "\x1f{\"a\":\"b:c\"}\x1f--\x1e"));
    return 0;
}

constexpr int type_d_evenement_puis_oubli_sans_donnees() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter("event: ping\ndata: 1\n\n", rec);
    d.alimenter("event: seul\n\n", rec); // pas de data : rien n'est expédié ET le type est oublié
    d.alimenter("data: 2\n\n", rec);
    VERIFIER(rec.nb == 2);
    VERIFIER(egal(rec.vue(), "ping\x1f"
                             "1\x1f--\x1e\x1f"
                             "2\x1f--\x1e"));
    return 0;
}

constexpr int data_vide_est_un_evenement_a_donnees_vides() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter("data:\n\ndata\n\ndata: \n\n", rec);
    VERIFIER(rec.nb == 3);
    VERIFIER(egal(rec.vue(), "\x1f\x1f--\x1e\x1f\x1f--\x1e\x1f\x1f--\x1e"));
    // et des lignes vides seules n'expédient rien
    Decodeur e;
    Enregistreur r2;
    e.alimenter("\n\n\r\n\r\r", r2);
    VERIFIER(r2.nb == 0 && !e.evenement_incomplet());
    return 0;
}

constexpr int rien_n_est_lu_apres_done() {
    Decodeur d;
    Enregistreur rec;
    const std::size_t n = d.alimenter("data: [DONE]\n\ndata: x\n\n", rec);
    VERIFIER(n == 1 && d.termine());
    VERIFIER(d.alimenter("data: y\n\n", rec) == 0);
    VERIFIER(rec.nb == 1);
    d.remettre_a_zero();
    VERIFIER(!d.termine());
    VERIFIER(d.alimenter("data: y\n\n", rec) == 1);
    return 0;
}

constexpr int done_doit_etre_exact() {
    Decodeur d;
    Enregistreur rec;
    d.alimenter("data: [DONE]x\n\ndata: [done]\n\ndata: [DONE]\n\n", rec);
    VERIFIER(rec.nb == 3);
    VERIFIER(egal(rec.vue(), "\x1f[DONE]x\x1f--\x1e\x1f[done]\x1f--\x1e\x1f[DONE]\x1f"
                             "F-\x1e"));
    return 0;
}

constexpr int evenement_incomplet_a_l_arret_du_flux() {
    Decodeur d;
    Enregistreur rec;
    VERIFIER(!d.evenement_incomplet());
    d.alimenter("data: abc", rec);
    VERIFIER(rec.nb == 0 && d.evenement_incomplet());
    d.alimenter("\n", rec);
    VERIFIER(rec.nb == 0 && d.evenement_incomplet()); // il manque la ligne vide
    d.alimenter("\n", rec);
    VERIFIER(rec.nb == 1 && !d.evenement_incomplet());
    return 0;
}

constexpr int evenement_trop_gros_est_signale_pas_tronque() {
    DecodeurSse<16> d;
    Enregistreur rec;
    d.alimenter("data: 0123456789ABCDEF\n\n", rec);  // 16 octets : tient pile
    d.alimenter("data: 0123456789ABCDEFG\n\n", rec); // 17 : trop long
    d.alimenter("data: ok\n\n", rec);                // et le décodeur est reparti
    VERIFIER(rec.nb == 3);
    VERIFIER(egal(rec.vue(), "\x1f"
                             "0123456789ABCDEF\x1f--\x1e\x1f\x1f-T\x1e\x1f"
                             "ok\x1f--\x1e"));
    // deux lignes data qui dépassent ensemble
    Enregistreur r2;
    DecodeurSse<16> e;
    e.alimenter("data: 01234567\ndata: 01234567\n\ndata: [DONE]\n\n", r2);
    VERIFIER(egal(r2.vue(), "\x1f\x1f-T\x1e\x1f[DONE]\x1f"
                            "F-\x1e"));
    return 0;
}

constexpr int type_trop_long_est_tronque_sans_deborder() {
    DecodeurSse<64, 4> d;
    Enregistreur rec;
    d.alimenter("event: abcdefgh\ndata: x\n\n", rec);
    VERIFIER(egal(rec.vue(), "abcd\x1f"
                             "x\x1f--\x1e"));
    return 0;
}

// --- coupures n'importe où ---------------------------------------------------------------------------------

constexpr int le_flux_d_essai_en_un_morceau() {
    Decodeur d;
    Enregistreur rec;
    jouer_en_un_morceau(FLUX, rec, d);
    VERIFIER(d.termine());
    VERIFIER(rec.nb == 4);
    VERIFIER(rec.utf8_valide);
    VERIFIER(egal(rec.vue(), "\x1f{\"c\":\"é€😀\"}\x1f--\x1e"
                             "message\x1f"
                             "l1\nl2\x1f--\x1e"
                             "\x1f"
                             "x\x1f--\x1e"
                             "\x1f[DONE]\x1f"
                             "F-\x1e"));
    return 0;
}

constexpr int coupure_a_chaque_position_donne_le_meme_resultat() {
    Decodeur ref;
    Enregistreur attendu;
    jouer_en_un_morceau(FLUX, attendu, ref);
    for (std::size_t coupe = 0; coupe <= FLUX.size(); ++coupe) {
        Decodeur d;
        Enregistreur rec;
        d.alimenter(FLUX.substr(0, coupe), rec);
        d.alimenter(FLUX.substr(coupe), rec);
        VERIFIER(rec.utf8_valide);
        VERIFIER(egal(rec.vue(), attendu.vue()));
        VERIFIER(d.termine());
    }
    return 0;
}

constexpr int morceaux_de_toutes_les_tailles_donnent_le_meme_resultat() {
    Decodeur ref;
    Enregistreur attendu;
    jouer_en_un_morceau(FLUX, attendu, ref);
    for (std::size_t taille = 1; taille <= FLUX.size(); ++taille) { // taille 1 = octet par octet
        Decodeur d;
        Enregistreur rec;
        for (std::size_t i = 0; i < FLUX.size(); i += taille)
            d.alimenter(FLUX.substr(i, taille), rec);
        VERIFIER(rec.utf8_valide);
        VERIFIER(egal(rec.vue(), attendu.vue()));
    }
    return 0;
}

constexpr int coupures_au_milieu_d_un_caractere_utf8() {
    // « é€😀 » = 2 + 3 + 4 octets : on coupe entre chaque octet de chaque caractère.
    constexpr std::string_view flux = "data: é€😀\n\n";
    for (std::size_t coupe = 0; coupe <= flux.size(); ++coupe) {
        Decodeur d;
        Enregistreur rec;
        d.alimenter(flux.substr(0, coupe), rec);
        d.alimenter(flux.substr(coupe), rec);
        VERIFIER(rec.nb == 1 && rec.utf8_valide);
        VERIFIER(egal(rec.vue(), "\x1f"
                                 "é€😀\x1f--\x1e"));
    }
    return 0;
}

constexpr int morceaux_vides_sont_sans_effet() {
    Decodeur d;
    Enregistreur rec;
    VERIFIER(d.alimenter("", rec) == 0);
    d.alimenter("data: a", rec);
    VERIFIER(d.alimenter("", rec) == 0);
    d.alimenter("\n\n", rec);
    VERIFIER(rec.nb == 1);
    return 0;
}

// Octet par octet : un événement est livré à l'octet qui termine sa ligne vide, ni avant ni après. Le flux est
// découpé en segments dont on connaît, par construction, le nombre cumulé d'événements livrés à leur dernier octet
// (attente écrite à la main, indépendante du décodeur) ; le décodeur est nourri d'UN octet à la fois.
constexpr int chaque_evenement_est_livre_des_son_dernier_octet() {
    struct Segment {
        std::string_view octets;
        std::size_t evenements_apres;
    };
    constexpr Segment SEGMENTS[] = {
        // CRLF : la ligne vide se termine dès son CR (WHATWG), l'événement part à cet octet-là, sans attendre le LF ;
        // données de 2, 3 et 4 octets par caractère
        {"data: {\"c\":\"é€😀\"}\r\n\r", 1},
        {"\n", 1},                 // ce LF se rattache au CR précédent : pas de nouvel événement
        {": pause\n", 1},          // un commentaire ne livre rien
        {"data: B\n\n", 2},        // LF seul
        {"data: C\r\r", 3},        // CR seul : livré au 2e CR, sans attendre un LF éventuel
        {"\n", 3},                 // le LF d'un CRLF dont le CR vient d'être lu
        {"data: [DONE]\r\n\r", 4}, // fin du flux
        {"\n", 4},                 // ignoré : le flux est fini
    };
    Decodeur d;
    Enregistreur rec;
    std::size_t avant = 0;
    for (const Segment &s : SEGMENTS) {
        for (std::size_t i = 0; i < s.octets.size(); ++i) {
            d.alimenter(s.octets.substr(i, 1), rec);
            const bool dernier = i + 1 == s.octets.size();
            VERIFIER(rec.nb == (dernier ? s.evenements_apres : avant));
        }
        avant = s.evenements_apres;
    }
    VERIFIER(d.termine() && rec.utf8_valide); // jamais un caractère coupé livré à l'appelant
    VERIFIER(egal(rec.vue(), "\x1f{\"c\":\"é€😀\"}\x1f--\x1e"
                             "\x1f"
                             "B\x1f--\x1e"
                             "\x1f"
                             "C\x1f--\x1e"
                             "\x1f[DONE]\x1f"
                             "F-\x1e"));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_evenement_est_livre_des_son_dernier_octet)                                                                \
    X(flux_d_openai_avec_done)                                                                                         \
    X(data_sur_plusieurs_lignes_est_joint_par_un_retour_a_la_ligne)                                                    \
    X(les_trois_fins_de_ligne_sont_equivalentes)                                                                       \
    X(cr_et_lf_dans_deux_morceaux_ne_font_qu_une_fin_de_ligne)                                                         \
    X(commentaires_et_champs_inconnus_sont_ignores)                                                                    \
    X(un_seul_espace_apres_les_deux_points_est_retire)                                                                 \
    X(deux_points_dans_la_valeur_sont_conserves)                                                                       \
    X(type_d_evenement_puis_oubli_sans_donnees)                                                                        \
    X(data_vide_est_un_evenement_a_donnees_vides)                                                                      \
    X(rien_n_est_lu_apres_done)                                                                                        \
    X(done_doit_etre_exact)                                                                                            \
    X(evenement_incomplet_a_l_arret_du_flux)                                                                           \
    X(evenement_trop_gros_est_signale_pas_tronque)                                                                     \
    X(type_trop_long_est_tronque_sans_deborder)                                                                        \
    X(le_flux_d_essai_en_un_morceau)                                                                                   \
    X(coupure_a_chaque_position_donne_le_meme_resultat)                                                                \
    X(morceaux_de_toutes_les_tailles_donnent_le_meme_resultat)                                                         \
    X(coupures_au_milieu_d_un_caractere_utf8)                                                                          \
    X(morceaux_vides_sont_sans_effet)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
