// Tests du noyau pur : le registre des spots de la page mer (mer_spots.hpp). Ce qu'on peut ajouter, retirer, choisir ; ce qui est refusé
// et pourquoi ; ce que la NVS relue devient quand elle est abîmée ; la requête qui part pour le spot choisi.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "ecran/mer_spots.hpp"
#include "mini.hpp"

using namespace ecran::mer;

namespace {

constexpr std::string_view HOTE = "https://vigie.exemple.fr";

constexpr bool egal(const char *a, std::string_view b) { return std::string_view(a) == b; }

// Les opérations du noyau modifient le registre EN PLACE et rendent l'erreur ; ces enveloppes gardent l'ancien registre et rendent les deux,
// ce qui permet d'écrire « le refus n'a rien changé » en une ligne.
struct Res {
    ErreurRegistre erreur = ErreurRegistre::Aucune;
    Registre valeur{};
};
constexpr Res ajouter_val(const Registre &actuel, std::string_view id, std::string_view nom) {
    Res r;
    r.valeur = actuel;
    r.erreur = ecran::mer::ajouter(r.valeur, id, nom);
    return r;
}
constexpr Res retirer_val(const Registre &actuel, std::size_t indice) {
    Res r;
    r.valeur = actuel;
    r.erreur = ecran::mer::retirer(r.valeur, indice);
    return r;
}
constexpr Res choisir_val(const Registre &actuel, std::size_t indice) {
    Res r;
    r.valeur = actuel;
    r.erreur = ecran::mer::choisir(r.valeur, indice);
    return r;
}
constexpr Res regler_vent_val(const Registre &actuel, std::int64_t min_noeuds, std::int64_t max_noeuds) {
    Res r;
    r.valeur = actuel;
    r.erreur = ecran::mer::regler_vent(r.valeur, min_noeuds, max_noeuds);
    return r;
}

// Le client ne garde pas le registre, seulement sa demande : ces deux enveloppes disent ce que la demande donne pour un registre.
constexpr bool meme_requete(const Registre &a, const Registre &b) {
    return meme_demande(demande_du_registre(a), demande_du_registre(b));
}
constexpr std::size_t requete_du_registre(std::string_view adresse, const Registre &r, char *sortie,
                                          std::size_t capacite) {
    return requete_de_la_demande(adresse, demande_du_registre(r), sortie, capacite);
}

// Un registre bâti avec l'API publique du noyau ; un refus fait échouer le test qui l'a demandé (rend un registre vide).
constexpr Registre avec(std::initializer_list<std::string_view> ids) {
    Registre r;
    for (const std::string_view id : ids) {
        const Res a = ajouter_val(r, id, id); // le nom est l'identifiant : lisible et valide
        if (a.erreur != ErreurRegistre::Aucune)
            return Registre{};
        r = a.valeur;
    }
    return r;
}

constexpr Registre trois() { return avec({"hossegor-sud", "la-torche", "port-vieux"}); }

// Un registre plein de douze spots « spot-a » à « spot-l ».
constexpr Registre douze() {
    Registre r;
    for (int i = 0; i < 12; ++i) {
        const char id[7] = {'s', 'p', 'o', 't', '-', static_cast<char>('a' + i), '\0'};
        const Res a = ajouter_val(r, id, id);
        if (a.erreur != ErreurRegistre::Aucune)
            return Registre{};
        r = a.valeur;
    }
    return r;
}

constexpr bool est(const Registre &r, std::size_t i, std::string_view id) { return egal(r.spots[i].id, id); }

// ---- l'état de départ ------------------------------------------------------------------------------------------------------------

constexpr int un_appareil_neuf_ne_demande_rien() {
    const Registre r;
    VERIFIER(r.nombre == 0 && r.choisi == 0);
    VERIFIER(r.vent_min_kn == 12 && r.vent_max_kn == 25);
    VERIFIER(spot_choisi(r) == nullptr);
    VERIFIER(registre_valide(r));
    char t[TAILLE_REQUETE_MAX] = {};
    VERIFIER(requete_du_registre(HOTE, r, t, sizeof t) == 0);
    return 0;
}

constexpr int le_registre_garde_douze_spots_comme_les_spots_de_surf() {
    VERIFIER(REGISTRE_MAX == 12);
    VERIFIER(REGISTRE_MAX == ecran::surf::SPOTS_MAX);
    return 0;
}

// ---- ajouter -------------------------------------------------------------------------------------------------------------------

constexpr int le_premier_spot_ajoute_devient_le_spot_choisi() {
    const Res a = ajouter_val(Registre{}, "hossegor-sud", "Hossegor Sud");
    VERIFIER(a.erreur == ErreurRegistre::Aucune);
    VERIFIER(a.valeur.nombre == 1 && a.valeur.choisi == 0);
    VERIFIER(egal(a.valeur.spots[0].id, "hossegor-sud") && egal(a.valeur.spots[0].nom, "Hossegor Sud"));
    const SpotMer *s = spot_choisi(a.valeur);
    VERIFIER(s != nullptr && egal(s->id, "hossegor-sud"));
    // Un registre vide mais abîmé (choix resté à 3) : le premier spot ajouté est quand même le spot choisi.
    Registre abime;
    abime.choisi = 3;
    const Res b = ajouter_val(abime, "la-torche", "La Torche");
    VERIFIER(b.erreur == ErreurRegistre::Aucune && b.valeur.nombre == 1 && b.valeur.choisi == 0);
    VERIFIER(spot_choisi(b.valeur) != nullptr && egal(spot_choisi(b.valeur)->id, "la-torche"));
    return 0;
}

constexpr int un_spot_de_plus_ne_change_pas_le_choix() {
    Registre r = avec({"hossegor-sud"});
    const Res a = ajouter_val(r, "la-torche", "La Torche");
    VERIFIER(a.erreur == ErreurRegistre::Aucune);
    VERIFIER(a.valeur.nombre == 2 && a.valeur.choisi == 0);
    VERIFIER(est(a.valeur, 0, "hossegor-sud") && est(a.valeur, 1, "la-torche"));
    // Choisi le deuxième, un troisième ne le déplace pas.
    r = choisir_val(a.valeur, 1).valeur;
    const Res b = ajouter_val(r, "port-vieux", "Port-Vieux");
    VERIFIER(b.erreur == ErreurRegistre::Aucune && b.valeur.choisi == 1);
    return 0;
}

constexpr int le_treizieme_spot_est_refuse() {
    const Registre pleine = douze();
    VERIFIER(pleine.nombre == 12);
    const Res a = ajouter_val(pleine, "spot-z", "Spot Z");
    VERIFIER(a.erreur == ErreurRegistre::Plein);
    VERIFIER(egaux(a.valeur, pleine));
    return 0;
}

constexpr int un_doublon_est_refuse_meme_avec_un_autre_nom() {
    const Registre r = trois();
    const Res a = ajouter_val(r, "la-torche", "Un autre nom");
    VERIFIER(a.erreur == ErreurRegistre::Doublon && egaux(a.valeur, r));
    // Un identifiant qui commence pareil n'est pas un doublon.
    VERIFIER(ajouter_val(r, "la-torche-2", "La Torche 2").erreur == ErreurRegistre::Aucune);
    VERIFIER(ajouter_val(r, "la-torch", "La Torch").erreur == ErreurRegistre::Aucune);
    return 0;
}

constexpr int un_identifiant_qui_n_est_pas_un_slug_est_refuse() {
    const Registre r = trois();
    for (const std::string_view id : {"", "Hossegor", "hossegor sud", "a/b", "a?b", "a&b", "a.b", "ho\xC3\xA9"}) {
        const Res a = ajouter_val(r, id, "Nom");
        VERIFIER(a.erreur == ErreurRegistre::Identifiant);
        VERIFIER(egaux(a.valeur, r));
    }
    // 64 octets passent, 65 non.
    char l64[65] = {};
    for (std::size_t i = 0; i < 64; ++i)
        l64[i] = 'a';
    VERIFIER(ajouter_val(r, std::string_view(l64, 64), "Nom").erreur == ErreurRegistre::Aucune);
    char l65[66] = {};
    for (std::size_t i = 0; i < 65; ++i)
        l65[i] = 'a';
    VERIFIER(ajouter_val(r, std::string_view(l65, 65), "Nom").erreur == ErreurRegistre::Identifiant);
    // Un octet nul au milieu n'est pas un identifiant.
    const char avec_nul[] = {'a', '\0', 'b'};
    VERIFIER(ajouter_val(r, std::string_view(avec_nul, 3), "Nom").erreur == ErreurRegistre::Identifiant);
    return 0;
}

constexpr int un_nom_vide_trop_long_ou_mal_forme_est_refuse() {
    const Registre r = trois();
    VERIFIER(ajouter_val(r, "nouveau", "").erreur == ErreurRegistre::Nom);
    VERIFIER(ajouter_val(r, "nouveau", "   ").erreur == ErreurRegistre::Nom);           // seulement des espaces
    VERIFIER(ajouter_val(r, "nouveau", "Nom\nde plage").erreur == ErreurRegistre::Nom); // caractère de contrôle
    VERIFIER(ajouter_val(r, "nouveau", "Nom\xC3").erreur == ErreurRegistre::Nom);       // UTF-8 coupé
    VERIFIER(ajouter_val(r, "nouveau", "Nom\xC0\x80").erreur == ErreurRegistre::Nom);   // forme trop longue
    const char avec_nul[] = {'N', 'o', '\0', 'm'};
    VERIFIER(ajouter_val(r, "nouveau", std::string_view(avec_nul, 4)).erreur == ErreurRegistre::Nom);
    // 32 caractères passent, 33 non.
    char c32[33] = {};
    for (std::size_t i = 0; i < 32; ++i)
        c32[i] = 'n';
    VERIFIER(ajouter_val(r, "nouveau", std::string_view(c32, 32)).erreur == ErreurRegistre::Aucune);
    char c33[34] = {};
    for (std::size_t i = 0; i < 33; ++i)
        c33[i] = 'n';
    VERIFIER(ajouter_val(r, "nouveau", std::string_view(c33, 33)).erreur == ErreurRegistre::Nom);
    // Un refus ne change rien.
    VERIFIER(egaux(ajouter_val(r, "nouveau", "").valeur, r));
    return 0;
}

constexpr int les_accents_et_les_longs_noms_en_octets_tiennent() {
    // 32 caractères de 4 octets : 128 octets et le zéro final, la limite du tampon.
    char c4[129] = {};
    for (std::size_t i = 0; i < 32; ++i) {
        c4[4 * i] = static_cast<char>(0xF0);
        c4[4 * i + 1] = static_cast<char>(0x9D);
        c4[4 * i + 2] = static_cast<char>(0x84);
        c4[4 * i + 3] = static_cast<char>(0x9E);
    }
    const Res a = ajouter_val(Registre{}, "musique", std::string_view(c4, 128));
    VERIFIER(a.erreur == ErreurRegistre::Aucune && a.valeur.spots[0].nom[127] == static_cast<char>(0x9E));
    VERIFIER(ajouter_val(Registre{}, "plage", "Plage d'Hossegor \xE2\x80\x94 Les Cavaliers").erreur ==
             ErreurRegistre::Aucune);
    VERIFIER(ajouter_val(Registre{}, "plage", "\xC3\x89t\xC3\xA9 \xE2\x80\x94 Port-Vieux").erreur ==
             ErreurRegistre::Aucune);
    return 0;
}

// ---- retirer -------------------------------------------------------------------------------------------------------------------

constexpr int retirer_un_spot_avant_le_choix_garde_le_meme_spot_choisi() {
    Registre r = choisir_val(trois(), 2).valeur; // port-vieux
    const Res a = retirer_val(r, 0);
    VERIFIER(a.erreur == ErreurRegistre::Aucune && a.valeur.nombre == 2 && a.valeur.choisi == 1);
    VERIFIER(egal(spot_choisi(a.valeur)->id, "port-vieux"));
    VERIFIER(est(a.valeur, 0, "la-torche") && est(a.valeur, 1, "port-vieux"));
    // Avec un spot après le choix, le choix ne retombe pas par hasard sur le bon spot : il doit avoir reculé d'un cran.
    const Registre quatre = choisir_val(avec({"spot-a", "spot-b", "spot-c", "spot-d"}), 2).valeur; // spot-c
    const Res b = retirer_val(quatre, 0);
    VERIFIER(b.valeur.nombre == 3 && b.valeur.choisi == 1 && egal(spot_choisi(b.valeur)->id, "spot-c"));
    const Res c = retirer_val(quatre, 1);
    VERIFIER(c.valeur.nombre == 3 && c.valeur.choisi == 1 && egal(spot_choisi(c.valeur)->id, "spot-c"));
    return 0;
}

constexpr int retirer_un_spot_apres_le_choix_ne_le_bouge_pas() {
    const Res a = retirer_val(trois(), 2); // le choix est hossegor-sud
    VERIFIER(a.valeur.nombre == 2 && a.valeur.choisi == 0 && egal(spot_choisi(a.valeur)->id, "hossegor-sud"));
    const Res b = retirer_val(choisir_val(trois(), 1).valeur, 2);
    VERIFIER(b.valeur.choisi == 1 && egal(spot_choisi(b.valeur)->id, "la-torche"));
    return 0;
}

constexpr int retirer_le_spot_choisi_passe_au_suivant_ou_au_dernier() {
    // Au milieu : le suivant prend sa place.
    const Res a = retirer_val(choisir_val(trois(), 1).valeur, 1);
    VERIFIER(a.valeur.nombre == 2 && a.valeur.choisi == 1 && egal(spot_choisi(a.valeur)->id, "port-vieux"));
    // Au début : le suivant aussi.
    const Res b = retirer_val(trois(), 0);
    VERIFIER(b.valeur.choisi == 0 && egal(spot_choisi(b.valeur)->id, "la-torche"));
    // À la fin : le dernier qui reste.
    const Res c = retirer_val(choisir_val(trois(), 2).valeur, 2);
    VERIFIER(c.valeur.nombre == 2 && c.valeur.choisi == 1 && egal(spot_choisi(c.valeur)->id, "la-torche"));
    return 0;
}

constexpr int retirer_le_dernier_spot_vide_le_registre_et_ne_demande_plus_rien() {
    const Res a = retirer_val(avec({"hossegor-sud"}), 0);
    VERIFIER(a.erreur == ErreurRegistre::Aucune && a.valeur.nombre == 0 && a.valeur.choisi == 0);
    VERIFIER(spot_choisi(a.valeur) == nullptr);
    char t[TAILLE_REQUETE_MAX] = {};
    VERIFIER(requete_du_registre(HOTE, a.valeur, t, sizeof t) == 0);
    return 0;
}

constexpr int la_case_liberee_est_remise_a_zero() {
    const Res a = retirer_val(trois(), 0);
    VERIFIER(a.valeur.spots[2].id[0] == '\0' && a.valeur.spots[2].nom[0] == '\0');
    VERIFIER(egaux(a.valeur, avec({"la-torche", "port-vieux"})));
    return 0;
}

constexpr int retirer_ou_choisir_hors_de_la_liste_est_un_refus() {
    const Registre r = trois();
    VERIFIER(retirer_val(r, 3).erreur == ErreurRegistre::Indice && egaux(retirer_val(r, 3).valeur, r));
    VERIFIER(retirer_val(r, 99).erreur == ErreurRegistre::Indice);
    VERIFIER(retirer_val(Registre{}, 0).erreur == ErreurRegistre::Indice);
    VERIFIER(choisir_val(r, 3).erreur == ErreurRegistre::Indice && egaux(choisir_val(r, 3).valeur, r));
    VERIFIER(choisir_val(Registre{}, 0).erreur == ErreurRegistre::Indice);
    return 0;
}

// ---- choisir et le vent --------------------------------------------------------------------------------------------------------

constexpr int choisir_change_le_spot_demande_et_rien_d_autre() {
    const Registre r = trois();
    const Res a = choisir_val(r, 2);
    VERIFIER(a.erreur == ErreurRegistre::Aucune && a.valeur.choisi == 2 && a.valeur.nombre == 3);
    VERIFIER(egal(spot_choisi(a.valeur)->id, "port-vieux"));
    VERIFIER(a.valeur.vent_min_kn == 12 && a.valeur.vent_max_kn == 25);
    VERIFIER(choisir_val(r, 0).erreur == ErreurRegistre::Aucune);
    return 0;
}

constexpr int la_plage_de_vent_se_regle_dans_les_bornes_de_l_api() {
    const Registre r = trois();
    const Res a = regler_vent_val(r, 8, 30);
    VERIFIER(a.erreur == ErreurRegistre::Aucune && a.valeur.vent_min_kn == 8 && a.valeur.vent_max_kn == 30);
    VERIFIER(a.valeur.nombre == 3 && a.valeur.choisi == 0);
    VERIFIER(regler_vent_val(r, 59, 60).erreur == ErreurRegistre::Aucune);
    for (const std::int64_t m : {0, -3, 12, 40, 60}) {
        const Res b = regler_vent_val(r, m, 12);
        VERIFIER(b.erreur == ErreurRegistre::Vent && egaux(b.valeur, r));
    }
    VERIFIER(regler_vent_val(r, 12, 61).erreur == ErreurRegistre::Vent);
    VERIFIER(regler_vent_val(r, 12, 12).erreur == ErreurRegistre::Vent);
    return 0;
}

// ---- « la même requête » -------------------------------------------------------------------------------------------------------

constexpr int la_meme_requete_ne_regarde_que_le_spot_choisi_et_le_vent() {
    const Registre a = trois();
    VERIFIER(meme_requete(a, a));
    // D'autres spots dans la liste, un autre nom, un autre ordre : la mer demandée est la même.
    Registre b = avec({"autre", "hossegor-sud"});
    b = choisir_val(b, 1).valeur;
    VERIFIER(meme_requete(a, b) && meme_requete(b, a));
    Registre c = a;
    c.spots[0].nom[0] = 'X';
    VERIFIER(meme_requete(a, c));
    // Un autre spot choisi : non.
    VERIFIER(!meme_requete(a, choisir_val(a, 1).valeur));
    // Une autre plage de vent : non, même d'une seule borne.
    VERIFIER(!meme_requete(a, regler_vent_val(a, 8, 25).valeur));
    VERIFIER(!meme_requete(a, regler_vent_val(a, 12, 26).valeur));
    return 0;
}

constexpr int sans_spot_choisi_rien_n_est_demande_et_le_vent_n_y_change_rien() {
    const Registre vide;
    const Registre vide_autre_vent = regler_vent_val(Registre{}, 5, 40).valeur;
    VERIFIER(meme_requete(vide, vide_autre_vent));
    VERIFIER(!meme_requete(vide, trois()) && !meme_requete(trois(), vide));
    return 0;
}

// ---- la demande du client ------------------------------------------------------------------------------------------------------

constexpr int la_demande_est_le_spot_choisi_et_le_vent_et_rien_d_autre() {
    const DemandeMer vide = demande_du_registre(Registre{});
    VERIFIER(!vide.presente && vide.spot[0] == '\0' && vide.vent_min_kn == 12 && vide.vent_max_kn == 25);
    Registre r = regler_vent_val(choisir_val(trois(), 1).valeur, 7, 41).valeur;
    const DemandeMer d = demande_du_registre(r);
    VERIFIER(d.presente && egal(d.spot, "la-torche") && d.vent_min_kn == 7 && d.vent_max_kn == 41);
    // L'identifiant est copié en entier, zéro final compris, jusqu'à 64 octets.
    char l64[65] = {};
    for (std::size_t i = 0; i < 64; ++i)
        l64[i] = 'q';
    Res a = ajouter_val(Registre{}, std::string_view(l64, 64), "Long");
    VERIFIER(a.erreur == ErreurRegistre::Aucune);
    const DemandeMer longue = demande_du_registre(a.valeur);
    VERIFIER(longue.presente && egal(longue.spot, std::string_view(l64, 64)) && longue.spot[64] == '\0');
    return 0;
}

constexpr int deux_demandes_sont_la_meme_si_spot_et_vent_sont_les_memes() {
    const DemandeMer a = demande_du_registre(trois());
    VERIFIER(meme_demande(a, a));
    DemandeMer b = a;
    b.spot[0] = 'x';
    VERIFIER(!meme_demande(a, b));
    b = a;
    b.vent_min_kn = 13;
    VERIFIER(!meme_demande(a, b));
    b = a;
    b.vent_max_kn = 26;
    VERIFIER(!meme_demande(a, b));
    b = a;
    b.presente = false;
    VERIFIER(!meme_demande(a, b) && !meme_demande(b, a));
    // Deux demandes absentes sont les mêmes, quoi qu'elles contiennent d'autre.
    DemandeMer c = b;
    c.spot[0] = 'z';
    c.vent_min_kn = 3;
    VERIFIER(meme_demande(b, c));
    // Un identifiant qui n'est qu'un début de l'autre n'est pas le même.
    DemandeMer p = a;
    p.spot[std::string_view(p.spot).size() - 1] = '\0';
    VERIFIER(!meme_demande(a, p));
    return 0;
}

// ---- la requête du registre ----------------------------------------------------------------------------------------------------

constexpr int la_requete_est_celle_du_spot_choisi_avec_son_vent() {
    char t[TAILLE_REQUETE_MAX] = {};
    Registre r = trois();
    std::size_t n = requete_du_registre(HOTE, r, t, sizeof t);
    VERIFIER(std::string_view(t, n) ==
             "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=hossegor-sud");
    r = choisir_val(r, 2).valeur;
    n = requete_du_registre(HOTE, r, t, sizeof t);
    VERIFIER(std::string_view(t, n) ==
             "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=port-vieux");
    r = regler_vent_val(r, 10, 22).valeur;
    n = requete_du_registre(HOTE, r, t, sizeof t);
    VERIFIER(
        std::string_view(t, n) ==
        "https://vigie.exemple.fr/v1/embedded/frame?device_type=generic&spot=port-vieux&wind_min_kn=10&wind_max_kn=22");
    // Un tampon trop petit : zéro.
    VERIFIER(requete_du_registre(HOTE, r, t, 20) == 0);
    return 0;
}

// ---- la NVS --------------------------------------------------------------------------------------------------------------------

// Un registre « brut » comme la NVS le rend : on écrit les cases à la main, sans passer par ajouter.
constexpr void poser(Registre &r, std::size_t i, std::string_view id, std::string_view nom) {
    for (std::size_t k = 0; k < ID_OCTETS; ++k)
        r.spots[i].id[k] = k < id.size() ? id[k] : '\0';
    for (std::size_t k = 0; k < NOM_OCTETS; ++k)
        r.spots[i].nom[k] = k < nom.size() ? nom[k] : '\0';
}

constexpr int un_registre_construit_par_les_operations_est_valide() {
    VERIFIER(registre_valide(trois()));
    VERIFIER(registre_valide(douze()));
    VERIFIER(registre_valide(choisir_val(trois(), 2).valeur));
    VERIFIER(registre_valide(regler_vent_val(trois(), 8, 30).valeur));
    VERIFIER(registre_valide(retirer_val(trois(), 1).valeur));
    VERIFIER(registre_valide(retirer_val(avec({"a"}), 0).valeur));
    return 0;
}

constexpr int un_registre_intact_relu_de_la_nvs_ne_change_pas() {
    const Registre r = regler_vent_val(choisir_val(trois(), 1).valeur, 9, 33).valeur;
    VERIFIER(egaux(registre_depuis_nvs(r), r));
    return 0;
}

constexpr int une_entree_abimee_est_ecartee_et_les_autres_sont_gardees() {
    Registre brut;
    brut.nombre = 4;
    poser(brut, 0, "hossegor-sud", "Hossegor Sud");
    poser(brut, 1, "Majuscule", "Mauvais id"); // identifiant refusé
    poser(brut, 2, "la-torche", "");           // nom vide
    poser(brut, 3, "port-vieux", "Port-Vieux");
    brut.choisi = 3;
    const Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 2);
    VERIFIER(est(n, 0, "hossegor-sud") && est(n, 1, "port-vieux"));
    VERIFIER(n.choisi == 1); // le spot choisi est toujours port-vieux
    VERIFIER(registre_valide(n));
    return 0;
}

constexpr int si_le_spot_choisi_est_ecarte_le_premier_est_choisi() {
    Registre brut;
    brut.nombre = 3;
    poser(brut, 0, "hossegor-sud", "Hossegor Sud");
    poser(brut, 1, "Mauvais", "Mauvais");
    poser(brut, 2, "port-vieux", "Port-Vieux");
    brut.choisi = 1;
    const Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 2 && n.choisi == 0);
    return 0;
}

constexpr int un_choix_hors_de_la_liste_retombe_sur_le_premier() {
    Registre brut = trois();
    brut.choisi = 7;
    const Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 3 && n.choisi == 0);
    VERIFIER(!registre_valide(brut));
    return 0;
}

constexpr int un_doublon_relu_est_ecarte() {
    Registre brut;
    brut.nombre = 3;
    poser(brut, 0, "la-torche", "La Torche");
    poser(brut, 1, "la-torche", "Encore");
    poser(brut, 2, "port-vieux", "Port-Vieux");
    brut.choisi = 2;
    const Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 2 && est(n, 0, "la-torche") && egal(n.spots[0].nom, "La Torche"));
    VERIFIER(n.choisi == 1 && est(n, 1, "port-vieux"));
    return 0;
}

constexpr int plus_de_douze_entrees_annoncees_ne_lisent_que_les_douze_premieres() {
    Registre brut = douze();
    brut.nombre = 200;
    const Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 12 && est(n, 11, "spot-l"));
    VERIFIER(!registre_valide(brut));
    return 0;
}

constexpr int un_texte_sans_zero_final_est_ecarte() {
    Registre brut;
    brut.nombre = 2;
    for (std::size_t k = 0; k < ID_OCTETS; ++k)
        brut.spots[0].id[k] = 'a'; // aucun zéro : le tableau est plein
    poser(brut, 1, "la-torche", "La Torche");
    Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 1 && est(n, 0, "la-torche"));
    // Idem pour le nom.
    Registre b2;
    b2.nombre = 2;
    poser(b2, 0, "la-torche", "x");
    for (std::size_t k = 0; k < NOM_OCTETS; ++k)
        b2.spots[0].nom[k] = 'n';
    poser(b2, 1, "port-vieux", "Port-Vieux");
    n = registre_depuis_nvs(b2);
    VERIFIER(n.nombre == 1 && est(n, 0, "port-vieux"));
    return 0;
}

constexpr int ce_qui_suit_le_zero_final_est_nettoye() {
    Registre brut;
    brut.nombre = 1;
    poser(brut, 0, "la-torche", "La Torche");
    brut.spots[0].id[20] = 'Z'; // des restes après le zéro
    brut.spots[0].nom[60] = 'Z';
    const Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.nombre == 1 && n.spots[0].id[20] == '\0' && n.spots[0].nom[60] == '\0');
    VERIFIER(registre_valide(brut)); // des restes après le zéro ne rendent pas le registre invalide
    return 0;
}

constexpr int une_plage_de_vent_abimee_retombe_sur_les_valeurs_de_vigie() {
    Registre brut = trois();
    brut.vent_min_kn = 40;
    brut.vent_max_kn = 20;
    Registre n = registre_depuis_nvs(brut);
    VERIFIER(n.vent_min_kn == 12 && n.vent_max_kn == 25 && n.nombre == 3);
    VERIFIER(!registre_valide(brut));
    brut.vent_min_kn = 0;
    brut.vent_max_kn = 30;
    VERIFIER(registre_depuis_nvs(brut).vent_min_kn == 12);
    brut.vent_min_kn = 5;
    brut.vent_max_kn = 61;
    n = registre_depuis_nvs(brut);
    VERIFIER(n.vent_min_kn == 12 && n.vent_max_kn == 25);
    // Une plage valide est gardée.
    brut.vent_max_kn = 60;
    n = registre_depuis_nvs(brut);
    VERIFIER(n.vent_min_kn == 5 && n.vent_max_kn == 60);
    return 0;
}

constexpr int un_registre_a_corriger_n_est_pas_valide() {
    Registre dup = trois();
    poser(dup, 1, "hossegor-sud", "Copie");
    VERIFIER(!registre_valide(dup));
    Registre mauvais = trois();
    poser(mauvais, 2, "Port Vieux", "Port-Vieux");
    VERIFIER(!registre_valide(mauvais));
    Registre choix = trois();
    choix.choisi = 3;
    VERIFIER(!registre_valide(choix));
    Registre vide;
    vide.choisi = 1; // une liste vide a toujours le choix 0
    VERIFIER(!registre_valide(vide));
    // Une plage de vent que l'API refuserait (422) : le registre n'est pas enregistrable tel quel.
    Registre vent = trois();
    vent.vent_min_kn = 30; // plus grand que la borne haute
    VERIFIER(!registre_valide(vent));
    vent.vent_min_kn = 12;
    vent.vent_max_kn = 61;
    VERIFIER(!registre_valide(vent));
    vent.vent_max_kn = 60;
    VERIFIER(registre_valide(vent));
    return 0;
}

constexpr int un_choix_hors_de_la_liste_ne_designe_aucun_spot() {
    Registre r = trois();
    r.choisi = 3; // juste après le dernier : la case existe dans le tableau mais n'est pas un spot
    VERIFIER(spot_choisi(r) == nullptr);
    VERIFIER(!demande_du_registre(r).presente);
    VERIFIER(!registre_valide(r));
    r.choisi = 255;
    VERIFIER(spot_choisi(r) == nullptr);
    Registre trop = trois();
    trop.nombre = 13; // plus d'entrées annoncées qu'il n'y a de cases
    VERIFIER(spot_choisi(trop) == nullptr);
    VERIFIER(!demande_du_registre(trop).presente);
    return 0;
}

constexpr int deux_registres_ne_sont_egaux_que_s_ils_disent_la_meme_chose() {
    const Registre a = trois();
    VERIFIER(egaux(a, trois()));
    // Un nom différent, un identifiant différent, une liste plus courte, un autre choix, une autre plage de vent.
    Registre b = a;
    poser(b, 1, "la-torche", "Un autre nom");
    VERIFIER(!egaux(a, b));
    b = a;
    poser(b, 1, "la-torche-2", "la-torche"); // le nom de trois() est l'identifiant : seul l'identifiant change
    VERIFIER(!egaux(a, b));
    VERIFIER(!egaux(a, retirer_val(a, 2).valeur));
    VERIFIER(!egaux(retirer_val(a, 2).valeur, a)); // la plus courte des deux est un début de l'autre
    VERIFIER(!egaux(a, choisir_val(a, 1).valeur));
    b = a;
    b.vent_min_kn = 13;
    VERIFIER(!egaux(a, b));
    b = a;
    b.vent_max_kn = 26;
    VERIFIER(!egaux(a, b));
    // Ce qui est au-delà de `nombre` ne compte pas.
    b = a;
    poser(b, 7, "reste", "Reste");
    VERIFIER(egaux(a, b));
    return 0;
}

constexpr int un_texte_sans_zero_final_n_est_egal_a_rien() {
    // Un identifiant qui remplit tout son tableau n'a pas de zéro final : le registre est abîmé, il n'est égal à rien, pas même à lui-même.
    Registre x = trois();
    for (std::size_t k = 0; k < ID_OCTETS; ++k)
        x.spots[0].id[k] = 'a';
    VERIFIER(!egaux(x, x));
    Registre y = trois();
    for (std::size_t k = 0; k < NOM_OCTETS; ++k)
        y.spots[0].nom[k] = 'n';
    VERIFIER(!egaux(y, y));
    // Idem pour la demande : une demande au spot sans zéro final n'est la même que personne.
    DemandeMer d = demande_du_registre(trois());
    VERIFIER(meme_demande(d, d));
    for (std::size_t k = 0; k < ID_OCTETS; ++k)
        d.spot[k] = 'a';
    VERIFIER(!meme_demande(d, d));
    return 0;
}

constexpr int les_operations_agissent_en_place_et_un_refus_ne_touche_a_rien() {
    Registre r;
    VERIFIER(ecran::mer::ajouter(r, "la-torche", "La Torche") == ErreurRegistre::Aucune);
    VERIFIER(r.nombre == 1 && egal(r.spots[0].id, "la-torche"));
    VERIFIER(ecran::mer::ajouter(r, "Pas un slug", "Nom") == ErreurRegistre::Identifiant);
    VERIFIER(ecran::mer::ajouter(r, "la-torche", "Encore") == ErreurRegistre::Doublon);
    VERIFIER(ecran::mer::ajouter(r, "port-vieux", "") == ErreurRegistre::Nom);
    VERIFIER(ecran::mer::choisir(r, 4) == ErreurRegistre::Indice);
    VERIFIER(ecran::mer::retirer(r, 4) == ErreurRegistre::Indice);
    VERIFIER(ecran::mer::regler_vent(r, 30, 10) == ErreurRegistre::Vent);
    VERIFIER(r.nombre == 1 && r.choisi == 0 && egal(r.spots[0].id, "la-torche") && egal(r.spots[0].nom, "La Torche"));
    VERIFIER(r.spots[1].id[0] == '\0' && r.vent_min_kn == 12 && r.vent_max_kn == 25);
    VERIFIER(ecran::mer::regler_vent(r, 10, 20) == ErreurRegistre::Aucune && r.vent_min_kn == 10 &&
             r.vent_max_kn == 20);
    VERIFIER(ecran::mer::retirer(r, 0) == ErreurRegistre::Aucune && r.nombre == 0);
    return 0;
}

constexpr int le_verdict_rapide_est_celui_de_la_reconstruction() {
    // registre_valide n'a pas de copie à faire ; elle doit pourtant dire la même chose que « rien n'a changé après registre_depuis_nvs ».
    Registre cas[16];
    int n = 0;
    cas[n++] = Registre{};
    cas[n++] = trois();
    cas[n++] = douze();
    cas[n++] = choisir_val(trois(), 2).valeur;
    cas[n] = trois();
    cas[n++].choisi = 3;
    cas[n] = trois();
    cas[n++].nombre = 13;
    cas[n] = trois();
    poser(cas[n], 1, "hossegor-sud", "Copie");
    ++n;
    cas[n] = trois();
    poser(cas[n], 2, "Port Vieux", "Port-Vieux");
    ++n;
    cas[n] = trois();
    cas[n++].vent_min_kn = 0;
    cas[n] = trois();
    cas[n++].vent_max_kn = 61;
    cas[n] = Registre{};
    cas[n++].choisi = 1;
    cas[n] = trois();
    poser(cas[n], 7, "reste", "Reste"); // une entrée au-delà de `nombre` : ne compte pas
    ++n;
    cas[n] = trois();
    poser(cas[n], 0, "la-torche", ""); // nom vide
    ++n;
    cas[n] = trois();
    cas[n++].vent_min_kn = 30; // 30 > 25 : plage inversée
    for (int i = 0; i < n; ++i)
        VERIFIER(registre_valide(cas[i]) == egaux(registre_depuis_nvs(cas[i]), cas[i]));
    // Et ces verdicts-là sont bien ceux qu'on attend (pas deux fonctions fausses de la même façon).
    VERIFIER(registre_valide(cas[0]) && registre_valide(cas[1]) && registre_valide(cas[2]) && registre_valide(cas[3]));
    VERIFIER(!registre_valide(cas[4]) && !registre_valide(cas[5]) && !registre_valide(cas[6]) &&
             !registre_valide(cas[7]));
    VERIFIER(!registre_valide(cas[8]) && !registre_valide(cas[9]) && !registre_valide(cas[10]));
    VERIFIER(registre_valide(cas[11]));
    VERIFIER(!registre_valide(cas[12]) && !registre_valide(cas[13]));
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(un_appareil_neuf_ne_demande_rien)                                                                                \
    X(le_registre_garde_douze_spots_comme_les_spots_de_surf)                                                           \
    X(le_premier_spot_ajoute_devient_le_spot_choisi)                                                                   \
    X(un_spot_de_plus_ne_change_pas_le_choix)                                                                          \
    X(le_treizieme_spot_est_refuse)                                                                                    \
    X(un_doublon_est_refuse_meme_avec_un_autre_nom)                                                                    \
    X(un_identifiant_qui_n_est_pas_un_slug_est_refuse)                                                                 \
    X(un_nom_vide_trop_long_ou_mal_forme_est_refuse)                                                                   \
    X(les_accents_et_les_longs_noms_en_octets_tiennent)                                                                \
    X(retirer_un_spot_avant_le_choix_garde_le_meme_spot_choisi)                                                        \
    X(retirer_un_spot_apres_le_choix_ne_le_bouge_pas)                                                                  \
    X(retirer_le_spot_choisi_passe_au_suivant_ou_au_dernier)                                                           \
    X(retirer_le_dernier_spot_vide_le_registre_et_ne_demande_plus_rien)                                                \
    X(la_case_liberee_est_remise_a_zero)                                                                               \
    X(retirer_ou_choisir_hors_de_la_liste_est_un_refus)                                                                \
    X(choisir_change_le_spot_demande_et_rien_d_autre)                                                                  \
    X(la_plage_de_vent_se_regle_dans_les_bornes_de_l_api)                                                              \
    X(la_meme_requete_ne_regarde_que_le_spot_choisi_et_le_vent)                                                        \
    X(sans_spot_choisi_rien_n_est_demande_et_le_vent_n_y_change_rien)                                                  \
    X(la_demande_est_le_spot_choisi_et_le_vent_et_rien_d_autre)                                                        \
    X(deux_demandes_sont_la_meme_si_spot_et_vent_sont_les_memes)                                                       \
    X(la_requete_est_celle_du_spot_choisi_avec_son_vent)                                                               \
    X(un_registre_construit_par_les_operations_est_valide)                                                             \
    X(un_registre_intact_relu_de_la_nvs_ne_change_pas)                                                                 \
    X(une_entree_abimee_est_ecartee_et_les_autres_sont_gardees)                                                        \
    X(si_le_spot_choisi_est_ecarte_le_premier_est_choisi)                                                              \
    X(un_choix_hors_de_la_liste_retombe_sur_le_premier)                                                                \
    X(un_doublon_relu_est_ecarte)                                                                                      \
    X(plus_de_douze_entrees_annoncees_ne_lisent_que_les_douze_premieres)                                               \
    X(un_texte_sans_zero_final_est_ecarte)                                                                             \
    X(ce_qui_suit_le_zero_final_est_nettoye)                                                                           \
    X(une_plage_de_vent_abimee_retombe_sur_les_valeurs_de_vigie)                                                       \
    X(un_registre_a_corriger_n_est_pas_valide)                                                                         \
    X(un_choix_hors_de_la_liste_ne_designe_aucun_spot)                                                                 \
    X(deux_registres_ne_sont_egaux_que_s_ils_disent_la_meme_chose)                                                     \
    X(un_texte_sans_zero_final_n_est_egal_a_rien)                                                                      \
    X(les_operations_agissent_en_place_et_un_refus_ne_touche_a_rien)                                                   \
    X(le_verdict_rapide_est_celui_de_la_reconstruction)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
