// La page « La mer » des réglages (ADR-0026) : les spots de l'écran « Mer » se choisissent, s'ajoutent et se retirent ici, sans téléphone.
// Trois rangées (components/core/include/ecran/ecrans_reglages.hpp, page « mer ») :
//   info.mer_spots     les spots gardés ; toucher un spot l'affiche sur l'écran « Mer », « Retirer » l'ôte ;
//   action.mer_ajouter ouvre « Ajouter un spot » : la liste de Vigie, lue par l'appareil lui-même, et une recherche dedans ;
//   info.mer_vent      la plage de vent du kite, un nœud par appui sur « − » / « + ».
// Ce fichier ne décide de rien : les règles (au plus 12 spots, pas de doublon, 0 < min < max <= 60 nœuds, recherche sans accents) sont
// dans le noyau pur (ecran/mer_spots.hpp, mer_liste.hpp, mer_reglage.hpp), testé sur le PC. Ici : le dessin et les appels à l'hôte.
// Le registre est celui de la page web (/api/sea) : les deux écrivent le même, par l'hôte (mer_enregistrer).
// LVGL pur, aucun include ESP-IDF : le rendu PC compile ce fichier tel quel.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string_view>

#include "ecran/mer_liste.hpp"
#include "ecran/mer_reglage.hpp"
#include "ecran/mer_spots.hpp"
#include "ecran/texte_modele.hpp"
#include "ui_reglages_interne.h"

namespace ecran::ui {

namespace {

using Param = ecran::texte::Parametre;

HoteReglages &hote() { return *env().hote; }

// Le dernier refus à dire sur la page « La mer » (une écriture qui échoue ne reste jamais muette). C'est la clé française d'un texte ;
// la page le montre une fois, puis l'oublie.
const char *g_message = nullptr;

constexpr const char *kMessageIllisible = "Les spots ne sont pas lisibles pour le moment.";
constexpr const char *kMessageEcriture = "Enregistrement impossible : réessayez.";

// Le rang d'un spot passe dans la donnée d'un bouton.
void *rang_vers_donnee(std::size_t rang) { return reinterpret_cast<void *>(static_cast<std::uintptr_t>(rang)); }
std::size_t donnee_vers_rang(void *donnee) {
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(donnee));
}

// ---- les spots gardés ---------------------------------------------------------------------------------------------------------------

// Garde le registre modifié et redessine la page : elle montre toujours ce que l'hôte a réellement gardé.
void garder_et_redessiner(const mer::Registre &r) {
    if (!hote().mer_enregistrer(r)) {
        g_message = kMessageEcriture;
    }
    nav_reconstruire_haut();
}

void sur_spot_choisi(lv_event_t *e) {
    Gros<mer::Registre> r;
    if (!r.ok() || !hote().mer_registre(*r)) {
        g_message = kMessageIllisible;
        nav_reconstruire_haut();
        return;
    }
    if (mer::choisir(*r, donnee_vers_rang(lv_event_get_user_data(e))) != mer::ErreurRegistre::Aucune) {
        nav_reconstruire_haut(); // le registre a changé depuis le dessin (la page web) : on remontre le vrai
        return;
    }
    garder_et_redessiner(*r);
}

void sur_spot_retire(lv_event_t *e) {
    Gros<mer::Registre> r;
    if (!r.ok() || !hote().mer_registre(*r)) {
        g_message = kMessageIllisible;
        nav_reconstruire_haut();
        return;
    }
    if (mer::retirer(*r, donnee_vers_rang(lv_event_get_user_data(e))) != mer::ErreurRegistre::Aucune) {
        nav_reconstruire_haut();
        return;
    }
    garder_et_redessiner(*r);
}

// ---- la plage de vent ---------------------------------------------------------------------------------------------------------------

// Donnée d'un bouton de la plage de vent : bit 1 = la borne (0 minimum, 1 maximum), bit 0 = le sens (0 moins, 1 plus).
void *code_vent(mer::reglage::Borne borne, int pas) {
    return reinterpret_cast<void *>(
        static_cast<std::uintptr_t>((borne == mer::reglage::Borne::Max ? 2U : 0U) | (pas > 0 ? 1U : 0U)));
}

void sur_vent(lv_event_t *e) {
    const std::uintptr_t code = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(e));
    const mer::reglage::Borne borne = (code & 2U) != 0 ? mer::reglage::Borne::Max : mer::reglage::Borne::Min;
    const int pas = (code & 1U) != 0 ? mer::reglage::PAS_VENT_NOEUDS : -mer::reglage::PAS_VENT_NOEUDS;
    Gros<mer::Registre> r;
    if (!r.ok() || !hote().mer_registre(*r)) {
        g_message = kMessageIllisible;
        nav_reconstruire_haut();
        return;
    }
    // Un bouton dont le décalage serait refusé est grisé : ce refus-ci ne vient que d'un registre changé depuis le dessin.
    if (mer::reglage::decaler_vent(*r, borne, pas) != mer::ErreurRegistre::Aucune) {
        nav_reconstruire_haut();
        return;
    }
    garder_et_redessiner(*r);
}

// Un petit bouton carré « − » ou « + » (zone tactile minimale en largeur comme en hauteur).
lv_obj_t *bouton_pas(lv_obj_t *ligne, const char *signe, mer::reglage::Borne borne, int pas, const mer::Registre &r) {
    lv_obj_t *b = creer_bouton(ligne, signe, StyleBouton::Discret, sur_vent, code_vent(borne, pas));
    lv_obj_set_width(b, S(kToucheMinPx));
    lv_obj_set_style_pad_hor(b, 0, 0);
    bouton_activer(b, mer::reglage::vent_decalable(r, borne, pas));
    return b;
}

void ligne_de_vent(lv_obj_t *carte, const char *libelle, int valeur, mer::reglage::Borne borne,
                   const mer::Registre &r) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ligne = creer_ligne(carte);
    lv_obj_t *titre = creer_texte(ligne, libelle, env().f.texte, p.texte);
    lv_obj_set_flex_grow(titre, 1);
    lv_obj_set_width(titre, LV_SIZE_CONTENT);
    bouton_pas(ligne, "−", borne, -mer::reglage::PAS_VENT_NOEUDS, r); // U+2212, dans la police embarquée
    char t[8];
    std::snprintf(t, sizeof t, "%d", valeur);
    lv_obj_t *v = creer_texte(ligne, t, env().f.titre, p.texte);
    lv_obj_set_width(v, S(96));
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
    bouton_pas(ligne, "+", borne, mer::reglage::PAS_VENT_NOEUDS, r);
}

// ---- « Ajouter un spot » : l'état de l'écran ---------------------------------------------------------------------------------------

struct ContexteSpot {
    lv_obj_t *ligne = nullptr; // le champ et son bouton : cachés tant que la liste de Vigie n'est pas prête
    lv_obj_t *champ = nullptr;
    lv_obj_t *bouton = nullptr;    // « Rechercher » (taper ou appliquer), « Modifier » (retaper)
    lv_obj_t *etat = nullptr;      // une phrase : lecture en cours, refus, aucun résultat...
    lv_obj_t *resultats = nullptr; // les spots trouvés (ou le bouton « Réessayer »)
    lv_obj_t *clavier = nullptr;
    mer::liste::Liste liste;           // la copie de la liste de Vigie (16,6 Ko : jamais sur la pile)
    bool prete = false;                // `liste` est lisible
    bool dessine = false;              // les résultats ont été dessinés au moins une fois
    ListeMer vue = ListeMer::Inactive; // l'état de la lecture au dernier dessin
    char requete[mer::liste::RECHERCHE_OCTETS + 1] = {};
};

// L'écran du dessus. Un écran qu'on reconstruit (langue, thème) est supprimé APRÈS la construction du nouveau : chaque écran libère
// son propre contexte, et ne vide `g_spot` que s'il est encore le sien.
ContexteSpot *g_spot = nullptr;

void sur_suppression_ecran_spot(lv_event_t *e) {
    auto *c = static_cast<ContexteSpot *>(lv_event_get_user_data(e));
    if (g_spot == c) {
        g_spot = nullptr;
    }
    c->~ContexteSpot();
    lv_free(c);
}

void actualiser_bouton(ContexteSpot *c) {
    const bool cache = lv_obj_has_flag(c->clavier, LV_OBJ_FLAG_HIDDEN);
    bouton_changer_texte(c->bouton, (cache && c->requete[0] != '\0') ? T("Modifier") : T("Rechercher"));
}

void montrer_clavier_spot(ContexteSpot *c, bool montrer) {
    if (montrer) {
        lv_obj_remove_flag(c->clavier, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(c->clavier, LV_OBJ_FLAG_HIDDEN);
    }
    actualiser_bouton(c);
}

void sur_reessayer(lv_event_t *);

// Un toucher sur un spot de la liste : il rejoint le registre, puis on revient à la page « La mer » (qui se redessine).
void sur_resultat(lv_event_t *e) {
    ContexteSpot *c = g_spot;
    const std::size_t rang = donnee_vers_rang(lv_event_get_user_data(e));
    if (c == nullptr || !c->prete || rang >= c->liste.nombre) {
        return;
    }
    const PaletteReglages &p = *env().p;
    Gros<mer::Registre> r;
    if (!r.ok() || !hote().mer_registre(*r)) {
        lv_label_set_text(c->etat, T(kMessageIllisible));
        lv_obj_set_style_text_color(c->etat, couleur(p.avertissement), 0);
        return;
    }
    // Plein, ou déjà là : le registre a changé depuis le dessin (la page web, un autre appui). On le dit, rien n'est ajouté.
    const mer::ErreurRegistre refus = mer::liste::ajouter_de_la_liste(*r, c->liste.spots[rang]);
    if (refus != mer::ErreurRegistre::Aucune) {
        char tampon[200];
        char nombre[8];
        std::snprintf(nombre, sizeof nombre, "%u", static_cast<unsigned>(mer::REGISTRE_MAX));
        const Param ps[] = {{"n", nombre}};
        const char *phrase =
            refus == mer::ErreurRegistre::Plein
                ? remplir("Votre liste est pleine : {n} spots au plus. Retirez-en un pour en ajouter un autre.", ps, 1,
                          tampon, sizeof tampon)
            : refus == mer::ErreurRegistre::Doublon ? T("Ce spot est déjà dans votre liste.")
            : refus == mer::ErreurRegistre::Nom     ? T("Le nom d’un spot doit faire de 1 à 32 caractères.")
                                                    : T("Cet identifiant de spot n’est pas valide.");
        lv_label_set_text(c->etat, phrase);
        lv_obj_set_style_text_color(c->etat, couleur(p.avertissement), 0);
        return;
    }
    if (!hote().mer_enregistrer(*r)) {
        lv_label_set_text(c->etat, T(kMessageEcriture));
        lv_obj_set_style_text_color(c->etat, couleur(p.avertissement), 0);
        return;
    }
    nav_retour();
}

// Les spots viennent d'OpenStreetMap (ODbL) : le crédit suit la liste, comme sur la page web (ADR-0025).
void credit_des_spots(ContexteSpot *c) {
    creer_texte(c->resultats, T("Spots : © contributeurs d’OpenStreetMap, ODbL."), env().f.petite,
                env().p->texte_discret);
}

// Dessine les résultats (ou la raison pour laquelle il n'y en a pas) selon l'état de la lecture et la recherche.
void remplir_resultats(ContexteSpot *c) {
    const PaletteReglages &p = *env().p;
    lv_obj_clean(c->resultats);
    lv_obj_set_style_text_color(c->etat, couleur(p.texte_discret), 0);
    char tampon[200];
    const auto avec_reessai = [&](const char *phrase_traduite) {
        lv_label_set_text(c->etat, phrase_traduite);
        lv_obj_set_style_text_color(c->etat, couleur(p.avertissement), 0);
        lv_obj_t *b = creer_bouton(c->resultats, T("Réessayer"), StyleBouton::Discret, sur_reessayer, nullptr);
        lv_obj_set_width(b, LV_PCT(100));
    };
    switch (c->vue) {
    case ListeMer::Inactive:
    case ListeMer::EnCours:
        lv_label_set_text(c->etat, T("Chargement de la liste des spots…"));
        return;
    case ListeMer::SansConnexion:
        avec_reessai(T("Pas de connexion : la liste des spots demande Internet."));
        return;
    case ListeMer::Echec: {
        // L'adresse de Vigie se règle dans l'onglet « Alertes » de la même section : on y renvoie.
        const Param ps[] = {{"carte", T("Alertes")}};
        avec_reessai(
            remplir("Impossible de joindre Vigie pour lire la liste des spots. Vérifiez l’adresse dans « {carte} ».",
                    ps, 1, tampon, sizeof tampon));
        return;
    }
    case ListeMer::Vide:
        avec_reessai(T("Vigie ne propose aucun spot pour le moment."));
        return;
    case ListeMer::Terminee:
        break;
    }
    if (!c->prete) {
        return;
    }
    // Les spots déjà gardés restent dans la liste, grisés : on voit pourquoi ils ne s'ajoutent pas. Registre illisible : on n'en marque aucun.
    Gros<mer::Registre> reg;
    const bool registre_lu = reg.ok() && hote().mer_registre(*reg);
    std::uint8_t trouves[mer::liste::SPOTS_MAX];
    const std::size_t n = mer::liste::chercher(c->liste, c->requete, trouves, mer::liste::SPOTS_MAX);
    if (n == 0) {
        const Param ps[] = {{"q", c->requete}};
        lv_label_set_text(c->etat, remplir("Aucun spot ne correspond à “{q}”.", ps, 1, tampon, sizeof tampon));
        credit_des_spots(c);
        return;
    }
    lv_label_set_text(c->etat,
                      c->liste.tronquee ? T("La liste est longue : seuls les premiers spots sont montrés.") : "");
    for (std::size_t k = 0; k < n; ++k) {
        const mer::liste::SpotVigie &s = c->liste.spots[trouves[k]];
        const bool deja = registre_lu && mer::liste::dans_le_registre(*reg, s);
        lv_obj_t *b = creer_rangee_bouton(c->resultats, s.nom, deja ? T("Déjà ajouté") : s.lieu,
                                          deja ? nullptr : sur_resultat, rang_vers_donnee(trouves[k]));
        if (deja) {
            lv_obj_set_style_opa(b, LV_OPA_60, 0);
        }
    }
    credit_des_spots(c);
}

// Lit l'état de la lecture chez l'hôte ; s'il a changé depuis le dernier dessin, redessine. Appelé à la construction et par le minuteur.
void actualiser(ContexteSpot *c, bool forcer = false) {
    const ListeMer etat = hote().mer_liste_etat();
    if (c->dessine && !forcer && etat == c->vue) {
        return;
    }
    c->dessine = true;
    c->vue = etat;
    if (etat == ListeMer::Terminee) {
        c->prete = hote().mer_liste_copier(c->liste);
        if (!c->prete) {
            c->vue = ListeMer::Echec; // la liste annoncée prête n'a pas pu être copiée
        }
    }
    const bool prete = c->vue == ListeMer::Terminee && c->prete;
    if (prete) {
        lv_obj_remove_flag(c->ligne, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(c->ligne, LV_OBJ_FLAG_HIDDEN);
        montrer_clavier_spot(c, false);
    }
    remplir_resultats(c);
}

void appliquer_recherche(ContexteSpot *c) {
    const std::string_view saisi = lv_textarea_get_text(c->champ);
    const std::size_t n = std::min(saisi.size(), sizeof c->requete - 1);
    std::memcpy(c->requete, saisi.data(), n);
    c->requete[n] = '\0';
    montrer_clavier_spot(c, false); // les résultats prennent la place du clavier
    remplir_resultats(c);
}

void sur_valider_spot(void *) {
    if (g_spot != nullptr) {
        appliquer_recherche(g_spot);
    }
}

void sur_rechercher_spot(lv_event_t *) {
    ContexteSpot *c = g_spot;
    if (c == nullptr) {
        return;
    }
    if (lv_obj_has_flag(c->clavier, LV_OBJ_FLAG_HIDDEN)) {
        montrer_clavier_spot(c, true); // taper, ou retaper
    } else {
        appliquer_recherche(c);
    }
}

void sur_reessayer(lv_event_t *) {
    ContexteSpot *c = g_spot;
    if (c == nullptr) {
        return;
    }
    hote().mer_liste_demander(); // l'hôte laisse passer 5 s après un échec : un appui en rafale ne martèle pas Vigie
    actualiser(c, true);
}

void minuteur_spot(lv_timer_t *t) {
    if (!est_le_dessus(t)) {
        return;
    }
    auto *ecran = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
    auto *c = static_cast<ContexteSpot *>(lv_obj_get_user_data(ecran));
    if (c != nullptr) {
        actualiser(c);
    }
}

} // namespace

// ---- les trois rangées de la page --------------------------------------------------------------------------------------------------------

void rangee_mer_spots(lv_obj_t *parent) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *carte = creer_carte(parent);
    creer_texte(carte, T("Spot affiché sur l’écran"), env().f.petite, p.texte_discret);
    if (g_message != nullptr) {
        creer_texte(carte, T(g_message), env().f.petite, p.avertissement);
        g_message = nullptr;
    }
    Gros<mer::Registre> r;
    if (!r.ok() || !hote().mer_registre(*r)) {
        creer_texte(carte, T(kMessageIllisible), env().f.petite, p.avertissement);
        return;
    }
    if (r->nombre == 0) {
        creer_texte(carte, T("Aucun spot choisi : l’écran n’affiche pas la mer."), env().f.texte, p.texte);
        return;
    }
    for (std::size_t i = 0; i < r->nombre; ++i) {
        lv_obj_t *ligne = creer_ligne(carte);
        lv_obj_t *b =
            creer_bouton(ligne, r->spots[i].nom, i == r->choisi ? StyleBouton::Principal : StyleBouton::Discret,
                         sur_spot_choisi, rang_vers_donnee(i));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_width(b, 0);
        lv_obj_t *nom = lv_obj_get_child(b, 0);
        lv_label_set_long_mode(nom, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nom, LV_PCT(100));
        lv_obj_set_style_text_align(nom, LV_TEXT_ALIGN_LEFT, 0);
        creer_bouton(ligne, T("Retirer"), StyleBouton::Discret, sur_spot_retire, rang_vers_donnee(i));
    }
}

void rangee_mer_ajouter(lv_obj_t *parent) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *carte = creer_carte(parent);
    Gros<mer::Registre> r;
    const bool plein = r.ok() && hote().mer_registre(*r) && r->nombre >= mer::REGISTRE_MAX;
    lv_obj_t *b = creer_bouton(
        carte, T("Ajouter un spot"), plein ? StyleBouton::Discret : StyleBouton::Principal,
        [](lv_event_t *) {
            Entree n;
            n.type = TypeEcran::RechercheSpot;
            nav_pousser(n);
        },
        nullptr);
    lv_obj_set_width(b, LV_PCT(100));
    if (plein) {
        bouton_activer(b, false);
        char tampon[200];
        char nombre[8];
        std::snprintf(nombre, sizeof nombre, "%u", static_cast<unsigned>(mer::REGISTRE_MAX));
        const Param ps[] = {{"n", nombre}};
        creer_texte(carte,
                    remplir("Votre liste est pleine : {n} spots au plus. Retirez-en un pour en ajouter un autre.", ps,
                            1, tampon, sizeof tampon),
                    env().f.petite, p.texte_discret);
    }
}

void rangee_mer_vent(lv_obj_t *parent) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *carte = creer_carte(parent);
    creer_texte(carte, T("Plage de vent"), env().f.petite, p.texte_discret);
    Gros<mer::Registre> r;
    if (!r.ok() || !hote().mer_registre(*r)) {
        creer_texte(carte, T(kMessageIllisible), env().f.petite, p.avertissement);
        return;
    }
    ligne_de_vent(carte, T("Minimum, en nœuds"), r->vent_min_kn, mer::reglage::Borne::Min, *r);
    ligne_de_vent(carte, T("Maximum, en nœuds"), r->vent_max_kn, mer::reglage::Borne::Max, *r);
}

// ---- l'écran « Ajouter un spot » -------------------------------------------------------------------------------------------------------

lv_obj_t *construire_recherche_spot(Entree &) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard("Ajouter un spot", false, &ecran);
    lv_obj_set_scroll_dir(corps, LV_DIR_NONE);
    lv_obj_set_style_pad_all(corps, S(12), 0);

    void *memoire = lv_malloc(sizeof(ContexteSpot));
    if (memoire == nullptr) {
        creer_texte(corps, T("Mémoire insuffisante pour afficher la liste."), env().f.texte, p.avertissement);
        return ecran;
    }
    ContexteSpot *c = new (memoire) ContexteSpot();
    g_spot = c;
    lv_obj_set_user_data(ecran, c);
    lv_obj_add_event_cb(ecran, sur_suppression_ecran_spot, LV_EVENT_DELETE, c);

    c->ligne = creer_ligne(corps);
    lv_obj_t *champ = lv_textarea_create(c->ligne);
    lv_textarea_set_one_line(champ, true);
    lv_textarea_set_max_length(champ,
                               24); // 24 caractères = 96 octets au plus : la limite de la recherche (RECHERCHE_OCTETS)
    lv_textarea_set_placeholder_text(champ, T("Tapez le nom d’un spot"));
    lv_obj_set_height(champ, S(76));
    lv_obj_set_flex_grow(champ, 1);
    lv_obj_set_style_text_font(champ, env().f.texte, 0);
    lv_obj_set_style_text_color(champ, couleur(p.texte), 0);
    lv_obj_set_style_text_color(champ, couleur(p.texte_discret), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(champ, couleur(p.surface_haute), 0);
    lv_obj_set_style_bg_opa(champ, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(champ, couleur(p.accent), 0);
    lv_obj_set_style_border_width(champ, 2, 0);
    lv_obj_set_style_radius(champ, S(14), 0);
    lv_obj_set_style_pad_hor(champ, S(16), 0);
    lv_obj_set_style_pad_ver(champ, S(18), 0);
    lv_obj_remove_flag(champ, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(champ, LV_STATE_FOCUSED);
    c->champ = champ;
    champ_actuel_definir(champ);
    c->bouton = creer_bouton(c->ligne, T("Rechercher"), StyleBouton::Principal, sur_rechercher_spot, nullptr);
    lv_obj_set_height(c->bouton, S(76));

    c->etat = creer_texte(corps, "", env().f.petite, p.texte_discret);
    c->resultats = creer_colonne(corps);
    lv_obj_set_width(c->resultats, LV_PCT(100));
    lv_obj_set_height(c->resultats, 0);
    lv_obj_set_flex_grow(c->resultats, 1);
    lv_obj_set_style_pad_row(c->resultats, S(8), 0);
    lv_obj_add_flag(c->resultats, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(c->resultats, LV_DIR_VER);
    style_defilement(c->resultats);

    c->clavier = clavier_creer(corps, champ, clavier_hauteur(env().hauteur, true), sur_valider_spot, nullptr);
    lv_obj_add_flag(c->ligne, LV_OBJ_FLAG_HIDDEN); // la ligne de saisie n'apparaît que quand la liste est prête
    montrer_clavier_spot(c, false);                // au départ la liste entière, le clavier vient avec « Rechercher »

    // La liste de Vigie, lue à la demande : une liste lue il y a moins de 10 minutes est gardée telle quelle par l'hôte.
    if (hote().mer_liste_etat() != ListeMer::Terminee) {
        hote().mer_liste_demander();
    }
    actualiser(c);
    // La réponse arrive après la demande : l'état de la lecture est relu quatre fois par seconde environ.
    ajouter_timer(ecran, minuteur_spot, 300);
    return ecran;
}

} // namespace ecran::ui
