// Installation guidée en sept étapes (ADR-0007, point 5) : langue, Wi-Fi, ville, affichage, assistant, téléphone, récapitulatif.
// Chaque étape est tirée du registre des réglages (même rangées que les pages de réglages) ; seul l'entourage est propre à
// l'installation : indicateur « Étape n sur 7 », Retour / Plus tard / Continuer, reprise après un redémarrage (la progression
// tient en un octet que l'hôte garde), « Quitter » seulement quand l'installation est refaite depuis les réglages.
// LVGL pur, aucun include ESP-IDF.
#include <cstdio>
#include <cstring>

#include "ecran/texte_modele.hpp"
#include "ui_reglages.h"
#include "ui_reglages_interne.h"

namespace ecran::ui {

namespace {

using Param = ecran::texte::Parametre;
using installation::Mode;
using registre::Etape;

bool g_wifi_abandonne = false; // « Continuer sans Wi-Fi » touché explicitement

HoteReglages &hote() { return *env().hote; }

const char *titre_etape(int etape) {
    switch (static_cast<Etape>(etape)) {
    case Etape::Langue:
        return T("Choisissez votre langue");
    case Etape::Wifi:
        return T("Connectez-vous au Wi-Fi");
    case Etape::Lieu:
        return T("Choisissez votre ville");
    case Etape::Affichage:
        return T("Réglez l’affichage");
    case Etape::Assistant:
        return T("Assistant vocal");
    case Etape::Telephone:
        return T("Téléphone");
    case Etape::Recapitulatif:
        return T("Récapitulatif");
    case Etape::Aucune:
        break;
    }
    return "";
}

installation::Progression progression_de(int etape) { return {static_cast<Etape>(etape), false, mode_installation()}; }

// La personne a fini (ou quitte) : plus d'installation à montrer au prochain démarrage.
void terminer_installation() {
    hote().definir_progression({Etape::Langue, true, Mode::Premiere});
    if (mode_installation() == Mode::Refaire) {
        // Venue des réglages : elle y retourne.
        nav_vider();
        Entree liste;
        liste.type = TypeEcran::Liste;
        nav_pousser(liste);
    } else {
        nav_vider();
        if (env().fermer != nullptr) {
            env().fermer();
        }
    }
}

void aller_a_etape(int etape) {
    Entree e;
    e.type = TypeEcran::Etape;
    e.a = etape;
    nav_remplacer(e);
}

void sur_continuer(lv_event_t *ev) {
    const int etape = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(ev)));
    const installation::Progression suivante = installation::avancer(progression_de(etape));
    if (suivante.terminee) {
        terminer_installation();
        return;
    }
    hote().definir_progression(suivante); // un redémarrage ici reprend à l'étape suivante
    aller_a_etape(installation::numero(suivante.etape));
}

void sur_retour(lv_event_t *ev) {
    const int etape = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(ev)));
    const installation::Progression avant = installation::reculer(progression_de(etape));
    hote().definir_progression(avant);
    aller_a_etape(installation::numero(avant.etape));
}

void sur_quitter(lv_event_t *) { terminer_installation(); }

void sur_sans_wifi(lv_event_t *ev) {
    g_wifi_abandonne = true; // choix explicite : l'étape suivante peut venir
    sur_continuer(ev);
}

lv_obj_t *creer_rangee_fixe(lv_obj_t *parent) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

// Une rangée du registre, par identifiant (même dessin que dans les réglages).
void rangee_registre(lv_obj_t *corps, std::string_view id) {
    const registre::Reglage *r = registre::chercher(id);
    const registre::Rangee *g = registre::rangee_du_reglage(id);
    if (r != nullptr && g != nullptr) {
        rangee_pour(corps, *r, *g);
    }
}

// Une ligne « libellé — valeur » du récapitulatif (lecture seule).
void ligne_recap(lv_obj_t *corps, const char *libelle, const char *valeur) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *c = lv_obj_create(corps);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c, couleur(p.surface), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, S(14), 0);
    lv_obj_set_style_pad_all(c, S(16), 0);
    lv_obj_set_style_pad_column(c, S(14), 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = creer_texte(c, libelle, env().f.petite, p.texte_discret);
    lv_obj_set_width(l, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(l, LV_PCT(45), 0);
    lv_obj_t *v = creer_texte(c, valeur, env().f.texte, p.texte);
    lv_obj_set_width(v, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(v, 1);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
}

void corps_langue(lv_obj_t *corps) {
    // Lisible par quelqu'un qui ne lit pas la langue d'affichage actuelle : les sept noms, dans leur propre langue.
    creer_texte(corps, "Langue · Language · Sprache · Taal · Idioma · Lingua · Dil", env().f.petite,
                env().p->texte_discret);
    rangee_registre(corps, "langue.ecran");
}

void corps_wifi(lv_obj_t *corps, int etape) {
    const PaletteReglages &p = *env().p;
    rangee_action_ou_info(corps, "info.wifi");
    rangee_registre(corps, "wifi.ssid");
    if (!hote().wifi_info().connecte) {
        creer_texte(corps, T("Sans Wi-Fi, l’heure et la météo ne seront pas à jour."), env().f.petite, p.texte_discret);
        lv_obj_t *b = creer_bouton(corps, T("Continuer sans Wi-Fi"), StyleBouton::Discret, sur_sans_wifi,
                                   reinterpret_cast<void *>(static_cast<intptr_t>(etape)));
        lv_obj_set_width(b, LV_PCT(100));
    }
}

void corps_lieu(lv_obj_t *corps) {
    rangee_registre(corps, "lieu.ville");
    rangee_action_ou_info(corps, "info.meteo");
}

void corps_affichage(lv_obj_t *corps) {
    rangee_registre(corps, "affichage.theme");
    rangee_registre(corps, "affichage.luminosite");
}

void corps_assistant(lv_obj_t *corps, int onglet) {
    creer_texte(corps, T("L’assistant vocal est facultatif : vous pouvez le régler plus tard."), env().f.petite,
                env().p->texte_discret);
    rangee_registre(corps, "langue.assistant");
    construire_corps_section(corps, registre::Section::AssistantVocal, onglet);
}

void corps_telephone(lv_obj_t *corps) {
    creer_texte(
        corps, T("Réglez aussi depuis votre téléphone : sur le même Wi-Fi, ouvrez cette adresse, puis entrez le code."),
        env().f.petite, env().p->texte_discret);
    rangee_action_ou_info(corps, "info.telephone");
}

void corps_recapitulatif(lv_obj_t *corps) {
    const PaletteReglages &p = *env().p;
    HoteReglages &h = hote();
    creer_texte(corps, T("Tout est prêt."), env().f.titre, p.succes);
    char valeur[96];

    resume_reglage(*registre::chercher("langue.ecran"), valeur, sizeof valeur);
    ligne_recap(corps, T("Langue"), valeur);

    const InfoWifi w = h.wifi_info();
    ligne_recap(corps, T("Wi-Fi"), w.connecte ? w.ssid : T("Non connecté"));

    resume_reglage(*registre::chercher("lieu.ville"), valeur, sizeof valeur);
    ligne_recap(corps, T("Ville"), valeur);

    resume_reglage(*registre::chercher("affichage.theme"), valeur, sizeof valeur);
    ligne_recap(corps, T("Thème"), valeur);

    if (h.nombre("llm.preset") >= 0) {
        resume_reglage(*registre::chercher("llm.preset"), valeur, sizeof valeur);
        ligne_recap(corps, T("Assistant vocal"), valeur);
    } else {
        ligne_recap(corps, T("Assistant vocal"), T("Plus tard"));
    }
}

lv_obj_t *creer_bouton_pied(lv_obj_t *pied, const char *texte, StyleBouton style, lv_event_cb_t cb, int etape,
                            int poids) {
    lv_obj_t *b = creer_bouton(pied, texte, style, cb, reinterpret_cast<void *>(static_cast<intptr_t>(etape)));
    lv_obj_set_height(b, S(76));
    lv_obj_set_width(b, 0);
    lv_obj_set_flex_grow(b, poids);
    lv_obj_set_style_pad_hor(b, S(10), 0);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    if (l != nullptr) {
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    return b;
}

} // namespace

lv_obj_t *construire_etape(Entree &e) {
    const PaletteReglages &p = *env().p;
    HoteReglages &h = hote();
    const int etape = e.a >= 1 && e.a <= registre::NOMBRE_ETAPES ? e.a : 1;
    e.a = etape;
    const Mode mode = mode_installation();

    lv_obj_t *ecran = lv_obj_create(env().racine);
    lv_obj_remove_style_all(ecran);
    lv_obj_set_size(ecran, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ecran, couleur(p.fond), 0);
    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(ecran, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE);

    // ---- en-tête : « Étape n sur 7 », titre, avancement
    lv_obj_t *entete = lv_obj_create(ecran);
    lv_obj_remove_style_all(entete);
    lv_obj_set_width(entete, LV_PCT(100));
    lv_obj_set_height(entete, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(entete, couleur(p.surface), 0);
    lv_obj_set_style_bg_opa(entete, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(entete, S(18), 0);
    lv_obj_set_style_pad_ver(entete, S(12), 0);
    lv_obj_set_style_pad_row(entete, S(8), 0);
    lv_obj_set_flex_flow(entete, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(entete, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *haut = creer_rangee_fixe(entete);
    char n[8];
    char total[8];
    std::snprintf(n, sizeof n, "%d", etape);
    std::snprintf(total, sizeof total, "%d", registre::NOMBRE_ETAPES);
    const Param pe[] = {{"n", n}, {"total", total}};
    char tampon[64];
    lv_obj_t *indicateur = creer_texte(haut, remplir("Étape {n} sur {total}", pe, 2, tampon, sizeof tampon),
                                       env().f.petite, p.texte_discret);
    lv_obj_set_width(indicateur, LV_SIZE_CONTENT);
    if (installation::sortie_libre(mode)) {
        creer_bouton(haut, T("Quitter l’installation"), StyleBouton::Discret, sur_quitter, nullptr);
    }
    creer_texte(entete, titre_etape(etape), env().f.titre, p.texte);

    lv_obj_t *avancement = lv_obj_create(entete);
    lv_obj_remove_style_all(avancement);
    lv_obj_set_width(avancement, LV_PCT(100));
    lv_obj_set_height(avancement, S(8));
    lv_obj_set_flex_flow(avancement, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(avancement, S(6), 0);
    lv_obj_remove_flag(avancement, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 1; i <= registre::NOMBRE_ETAPES; ++i) {
        lv_obj_t *seg = lv_obj_create(avancement);
        lv_obj_remove_style_all(seg);
        lv_obj_set_height(seg, LV_PCT(100));
        lv_obj_set_flex_grow(seg, 1);
        lv_obj_set_style_radius(seg, S(4), 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(seg, couleur(i <= etape ? p.accent : p.separateur), 0);
    }

    // ---- corps : le contenu de l'étape (deuxième enfant de l'écran : le défilement est gardé d'un redessin à l'autre)
    lv_obj_t *corps = lv_obj_create(ecran);
    lv_obj_remove_style_all(corps);
    lv_obj_set_width(corps, LV_PCT(100));
    lv_obj_set_flex_grow(corps, 1);
    lv_obj_set_flex_flow(corps, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(corps, S(16), 0);
    lv_obj_set_style_pad_row(corps, S(12), 0);
    lv_obj_set_scroll_dir(corps, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(corps, LV_SCROLLBAR_MODE_OFF);

    switch (static_cast<Etape>(etape)) {
    case Etape::Langue:
        corps_langue(corps);
        break;
    case Etape::Wifi:
        corps_wifi(corps, etape);
        break;
    case Etape::Lieu:
        corps_lieu(corps);
        break;
    case Etape::Affichage:
        corps_affichage(corps);
        break;
    case Etape::Assistant:
        corps_assistant(corps, e.b);
        break;
    case Etape::Telephone:
        corps_telephone(corps);
        break;
    case Etape::Recapitulatif:
        corps_recapitulatif(corps);
        break;
    case Etape::Aucune:
        break;
    }

    // ---- pied : Retour / Plus tard / Continuer (ou Terminer)
    lv_obj_t *pied = lv_obj_create(ecran);
    lv_obj_remove_style_all(pied);
    lv_obj_set_width(pied, LV_PCT(100));
    lv_obj_set_height(pied, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pied, couleur(p.surface), 0);
    lv_obj_set_style_bg_opa(pied, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(pied, S(12), 0);
    lv_obj_set_style_pad_column(pied, S(12), 0);
    lv_obj_set_flex_flow(pied, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(pied, LV_OBJ_FLAG_SCROLLABLE);
    if (installation::retour_possible(static_cast<Etape>(etape))) {
        creer_bouton_pied(pied, T("Retour"), StyleBouton::Discret, sur_retour, etape, 1);
    }
    if (installation::facultative(static_cast<Etape>(etape))) {
        creer_bouton_pied(pied, T("Plus tard"), StyleBouton::Discret, sur_continuer, etape, 1);
    }
    const installation::Contexte ctx = {langue_choisie(), h.wifi_info().connecte, g_wifi_abandonne};
    const bool dernier = static_cast<Etape>(etape) == Etape::Recapitulatif;
    lv_obj_t *suite = creer_bouton_pied(pied, dernier ? T("Terminer") : T("Continuer"), StyleBouton::Principal,
                                        sur_continuer, etape, 2);
    bouton_activer(suite, installation::peut_continuer(static_cast<Etape>(etape), ctx, mode));

    ajouter_minuteur_page(ecran); // les essais de service de l'étape « assistant » se terminent tout seuls
    return ecran;
}

void installation_reinitialiser_etat() { g_wifi_abandonne = false; }

} // namespace ecran::ui

namespace ecran {

void ui_installation_creer(const ContexteReglages &ctx) {
    ui::nav_vider();
    ui::env_initialiser(ctx.racine, ctx.ttf, ctx.ttf_taille, ctx.largeur, ctx.hauteur, ctx.hote, ctx.fermer);
    i18n::definir_langue(i18n::langue_depuis_indice(ctx.hote->nombre("langue.ecran")));
    ui::installation_reinitialiser_etat();
    ui::definir_langue_choisie(false);
    const installation::Progression p = ctx.hote->progression();
    ui::definir_mode_installation(p.mode);
    ui::Entree e;
    e.type = ui::TypeEcran::Etape;
    e.a = p.terminee ? 1 : installation::numero(p.etape);
    ui::nav_pousser(e);
}

} // namespace ecran
