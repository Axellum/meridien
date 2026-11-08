// Écrans de réglages (ADR-0007) : liste des sections, une page par section, éditeurs plein écran (clavier, réseaux Wi-Fi,
// connexion avec retour arrière, recherche de ville). Tout est dessiné à partir des tableaux du noyau pur (registre, pages) ; un
// réglage de plus dans le registre apparaît ici sans toucher ce fichier. Application immédiate : aucun bouton « Enregistrer ».
// LVGL pur, aucun include ESP-IDF : le rendu PC compile ce fichier tel quel.
#include "ui_reglages.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ecran/changement_wifi.hpp"
#include "ecran/presets_services.hpp"
#include "ecran/reglages_alertes.hpp"
#include "ecran/texte_modele.hpp"
#include "ecran/visuel_textes.hpp"
#include "ui_reglages_interne.h"

namespace ecran::ui {

namespace {

using Param = ecran::texte::Parametre;

constexpr int kPileMax = 8;
Entree g_pile[kPileMax];
int g_n = 0;
char g_ssid[40] = {};
lv_obj_t *g_champ_actuel = nullptr; // le champ de saisie de l'éditeur affiché (pour le rendu PC)
installation::Mode g_mode_installation = installation::Mode::Premiere;
bool g_langue_choisie = false;
installation::EtatConfirmation g_confirmation;
EtatTest g_tests_vus[3] = {EtatTest::Inactif, EtatTest::Inactif, EtatTest::Inactif};

HoteReglages &hote() { return *env().hote; }

lv_obj_t *construire_liste(Entree &e);
lv_obj_t *construire_section(Entree &e);

lv_obj_t *construire_selon_le_type(Entree &e) {
    switch (e.type) {
    case TypeEcran::Liste:
        return construire_liste(e);
    case TypeEcran::Section:
        return construire_section(e);
    case TypeEcran::EditeurTexte:
        return construire_editeur_texte(e);
    case TypeEcran::ListeWifi:
        return construire_liste_wifi(e);
    case TypeEcran::ConnexionWifi:
        return construire_connexion_wifi(e);
    case TypeEcran::RechercheVille:
        return construire_recherche_ville(e);
    case TypeEcran::ListeSource:
        return construire_liste_source(e);
    case TypeEcran::Etape:
        return construire_etape(e);
    }
    return nullptr;
}

// Le corps défilant d'un écran standard est son deuxième enfant (après la barre du haut).
lv_obj_t *corps_de(lv_obj_t *ecran) {
    return ecran != nullptr && lv_obj_get_child_count(ecran) > 1 ? lv_obj_get_child(ecran, 1) : nullptr;
}

lv_obj_t *construire(Entree &e) {
    lv_obj_t *obj = construire_selon_le_type(e);
    e.neuf = false;
    lv_obj_t *corps = corps_de(obj);
    if (corps != nullptr && e.scroll_y > 0) {
        lv_obj_update_layout(obj);
        lv_obj_scroll_to_y(corps, e.scroll_y, LV_ANIM_OFF); // une valeur a changé : la page reste où elle était
    }
    return obj;
}

void cacher(Entree &e) {
    if (e.obj != nullptr) {
        lv_obj_add_flag(e.obj, LV_OBJ_FLAG_HIDDEN);
    }
}

// L'ancien écran disparaît tout de suite aux yeux et au toucher ; sa suppression se fait au prochain tour de LVGL (jamais dans
// l'événement d'un de ses propres boutons).
void supprimer(lv_obj_t *obj) {
    if (obj != nullptr) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_delete_async(obj);
    }
}

void garder_defilement(Entree &e) {
    lv_obj_t *corps = corps_de(e.obj);
    e.scroll_y = corps != nullptr ? lv_obj_get_scroll_y(corps) : 0;
}

void sur_suppression_champ(lv_event_t *e) {
    if (lv_event_get_target_obj(e) == g_champ_actuel) {
        g_champ_actuel = nullptr;
    }
}

void depiler() {
    if (g_n <= 0) {
        return;
    }
    supprimer(g_pile[g_n - 1].obj);
    g_pile[g_n - 1] = Entree{};
    --g_n;
}

} // namespace

lv_obj_t *champ_actuel() { return g_champ_actuel; }

// ---- pile d'écrans ----------------------------------------------------------------------------------------------------------

Entree *nav_haut() { return g_n > 0 ? &g_pile[g_n - 1] : nullptr; }
int nav_profondeur() { return g_n; }

void nav_pousser(const Entree &e) {
    if (g_n >= kPileMax) {
        return;
    }
    Entree &neuf = g_pile[g_n];
    neuf = e;
    neuf.obj = nullptr;
    neuf.neuf = true;
    if (g_n > 0) {
        cacher(g_pile[g_n - 1]);
    }
    ++g_n;
    neuf.obj = construire(neuf);
}

void nav_remplacer(const Entree &e) {
    if (g_n == 0) {
        nav_pousser(e);
        return;
    }
    Entree &haut = g_pile[g_n - 1];
    supprimer(haut.obj);
    haut = e;
    haut.obj = nullptr;
    haut.neuf = true;
    haut.obj = construire(haut);
}

// Le dessous de la pile est toujours redessiné à son retour : une valeur a pu changer pendant qu'il était caché (nom du Wi-Fi,
// ville choisie, texte saisi), et un écran caché n'est pas à jour.
void nav_retour() {
    if (g_n <= 1) {
        if (g_n == 1 && env().fermer != nullptr) {
            env().fermer();
        }
        return;
    }
    Entree &dessous = g_pile[g_n - 2];
    garder_defilement(dessous);
    depiler();
    nav_reconstruire_haut();
}

void nav_retour_jusqu_a_une_base() {
    while (g_n > 1 && g_pile[g_n - 1].type != TypeEcran::Section && g_pile[g_n - 1].type != TypeEcran::Etape &&
           g_pile[g_n - 1].type != TypeEcran::Liste) {
        depiler();
    }
    nav_reconstruire_haut();
}

void nav_reconstruire_haut() {
    if (g_n == 0) {
        return;
    }
    Entree &haut = g_pile[g_n - 1];
    garder_defilement(haut);
    supprimer(haut.obj);
    haut.obj = construire(haut);
}

void nav_reconstruire() {
    for (int i = 0; i < g_n; ++i) {
        garder_defilement(g_pile[i]);
        supprimer(g_pile[i].obj);
        g_pile[i].obj = construire(g_pile[i]);
        if (i + 1 < g_n) {
            cacher(g_pile[i]);
        }
    }
}

void nav_vider() {
    for (int i = 0; i < g_n; ++i) {
        supprimer(g_pile[i].obj);
        g_pile[i] = Entree{};
    }
    g_n = 0;
    g_champ_actuel = nullptr;
}

const char *ssid_en_cours() { return g_ssid; }

void definir_ssid_en_cours(std::string_view ssid) {
    const std::size_t n = std::min(ssid.size(), sizeof g_ssid - 1);
    std::memcpy(g_ssid, ssid.data(), n);
    g_ssid[n] = '\0';
}

installation::Mode mode_installation() { return g_mode_installation; }
void definir_mode_installation(installation::Mode m) { g_mode_installation = m; }
bool langue_choisie() { return g_langue_choisie; }
void definir_langue_choisie(bool vrai) { g_langue_choisie = vrai; }

// ---- écran standard ---------------------------------------------------------------------------------------------------------

void bouton_retour_action(lv_event_t *) { nav_retour(); }

lv_obj_t *creer_ecran_standard(const char *titre, bool titre_deja_traduit, lv_obj_t **ecran_sortie,
                               lv_obj_t **barre_sortie, lv_event_cb_t action_retour) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ecran = lv_obj_create(env().racine);
    lv_obj_remove_style_all(ecran);
    lv_obj_set_size(ecran, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ecran, couleur(p.fond), 0);
    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(ecran, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *barre = lv_obj_create(ecran);
    lv_obj_remove_style_all(barre);
    lv_obj_set_width(barre, LV_PCT(100));
    lv_obj_set_height(barre, S(88));
    lv_obj_set_style_bg_color(barre, couleur(p.surface), 0);
    lv_obj_set_style_bg_opa(barre, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(barre, S(14), 0);
    lv_obj_set_style_pad_column(barre, S(16), 0);
    lv_obj_set_flex_flow(barre, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(barre, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(barre, LV_OBJ_FLAG_SCROLLABLE);
    creer_bouton(barre, T("Retour"), StyleBouton::Discret,
                 action_retour != nullptr ? action_retour : bouton_retour_action, nullptr);
    lv_obj_t *libelle = lv_label_create(barre);
    lv_label_set_long_mode(libelle, LV_LABEL_LONG_DOT);
    lv_label_set_text(libelle, titre_deja_traduit ? titre : T(titre));
    lv_obj_set_style_text_font(libelle, env().f.titre, 0);
    lv_obj_set_style_text_color(libelle, couleur(p.texte), 0);
    lv_obj_set_flex_grow(libelle, 1);

    lv_obj_t *corps = lv_obj_create(ecran);
    lv_obj_remove_style_all(corps);
    lv_obj_set_width(corps, LV_PCT(100));
    lv_obj_set_flex_grow(corps, 1);
    lv_obj_set_flex_flow(corps, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(corps, S(16), 0);
    lv_obj_set_style_pad_row(corps, S(12), 0);
    lv_obj_set_scroll_dir(corps, LV_DIR_VER);
    style_defilement(corps);

    if (ecran_sortie != nullptr) {
        *ecran_sortie = ecran;
    }
    if (barre_sortie != nullptr) {
        *barre_sortie = barre;
    }
    return corps;
}

namespace {
void sur_suppression_minuteur(lv_event_t *e) { lv_timer_delete(static_cast<lv_timer_t *>(lv_event_get_user_data(e))); }
} // namespace

void ajouter_timer(lv_obj_t *ecran, void (*cb)(lv_timer_t *), uint32_t periode_ms) {
    lv_timer_t *t = lv_timer_create(cb, periode_ms, ecran);
    lv_obj_add_event_cb(ecran, sur_suppression_minuteur, LV_EVENT_DELETE, t);
}

namespace {

// Vrai si le minuteur appartient à l'écran du dessus (un minuteur d'un écran en cours de suppression ne fait rien).
bool est_le_dessus(lv_timer_t *t) {
    const Entree *haut = nav_haut();
    return haut != nullptr && haut->obj == static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
}

// Une rangée-bouton à deux textes : le titre à gauche, la valeur à droite.
lv_obj_t *creer_rangee_bouton(lv_obj_t *parent, const char *titre, const char *valeur, lv_event_cb_t cb, void *donnee) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_height(b, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(b, S(16), 0);
    lv_obj_set_style_bg_color(b, couleur(p.surface), 0);
    lv_obj_set_style_bg_color(b, couleur(p.surface_haute), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_hor(b, S(16), 0);
    lv_obj_set_style_pad_ver(b, S(22), 0); // la hauteur d'une rangée vient du remplissage : le texte y est centré
    lv_obj_set_style_pad_column(b, S(14), 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, titre);
    lv_obj_set_style_text_font(l, env().f.texte, 0);
    lv_obj_set_style_text_color(l, couleur(p.texte), 0);
    lv_obj_set_flex_grow(l, 1);
    if (valeur != nullptr && valeur[0] != '\0') {
        lv_obj_t *v = lv_label_create(b);
        lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
        lv_label_set_text(v, valeur);
        lv_obj_set_style_text_font(v, env().f.petite, 0);
        lv_obj_set_style_text_color(v, couleur(p.texte_discret), 0);
        lv_obj_set_style_max_width(v, LV_PCT(50), 0);
    }
    if (cb != nullptr) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, donnee);
    }
    return b;
}

// Une carte : fond de surface, colonne.
lv_obj_t *creer_carte(lv_obj_t *parent) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, LV_PCT(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c, couleur(p.surface), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, S(16), 0);
    lv_obj_set_style_pad_all(c, S(16), 0);
    lv_obj_set_style_pad_row(c, S(10), 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

int index_reglage(const registre::Reglage &r) { return static_cast<int>(&r - registre::REGISTRE); }

} // namespace

// ---- résumés ----------------------------------------------------------------------------------------------------------------

const char *libelle_option(const registre::Option &o) { return o.traduire ? T(o.libelle.data()) : o.libelle.data(); }

void resume_reglage(const registre::Reglage &r, char *sortie, std::size_t capacite) {
    sortie[0] = '\0';
    switch (r.type) {
    case registre::Type::Choix: {
        const int32_t i = hote().nombre(r.id);
        if (i >= 0 && static_cast<std::size_t>(i) < r.nb_options) {
            std::snprintf(sortie, capacite, "%s", libelle_option(r.options[i]));
        } else {
            std::snprintf(sortie, capacite, "%s", "—");
        }
        break;
    }
    case registre::Type::Nombre:
        std::snprintf(sortie, capacite, "%d%s%s", static_cast<int>(hote().nombre(r.id)), r.unite.empty() ? "" : " ",
                      r.unite.data());
        break;
    case registre::Type::Secret:
        std::snprintf(sortie, capacite, "%s", hote().secret_defini(r.id) ? "••••••••" : "—");
        break;
    case registre::Type::Texte:
    case registre::Type::Lieu: {
        char t[96];
        hote().texte(r.id, t, sizeof t);
        if (r.id == "wifi.ssid") {
            const InfoWifi w = hote().wifi_info();
            if (w.connecte && w.ssid[0] != '\0') {
                std::snprintf(t, sizeof t, "%s", w.ssid);
            }
        }
        if (r.id == "alertes.adresse" && t[0] == '\0') { // vide = le service public de Vigie : on dit lequel
            const std::string_view publique = vigie::ADRESSE_PUBLIQUE.substr(std::string_view("https://").size());
            std::snprintf(t, sizeof t, "%.*s", static_cast<int>(publique.size()), publique.data());
        }
        std::snprintf(sortie, capacite, "%s", t[0] != '\0' ? t : "—");
        break;
    }
    }
}

namespace {

void resume_section(registre::Section s, char *sortie, std::size_t capacite) {
    sortie[0] = '\0';
    switch (s) {
    case registre::Section::Reseau: {
        const InfoWifi w = hote().wifi_info();
        std::snprintf(sortie, capacite, "%s", w.connecte ? w.ssid : T("Non connecté"));
        break;
    }
    case registre::Section::LieuMeteo:
        resume_reglage(*registre::chercher("lieu.ville"), sortie, capacite);
        break;
    case registre::Section::Langue:
        resume_reglage(*registre::chercher("langue.ecran"), sortie, capacite);
        break;
    case registre::Section::Affichage:
        resume_reglage(*registre::chercher("affichage.theme"), sortie, capacite);
        break;
    case registre::Section::AssistantVocal:
        break;
    case registre::Section::Appareil: {
        const InfoAppareil i = hote().info_appareil();
        std::snprintf(sortie, capacite, "%s", i.version);
        break;
    }
    }
}

// ---- liste des sections -----------------------------------------------------------------------------------------------------

void sur_section(lv_event_t *e) {
    Entree s;
    s.type = TypeEcran::Section;
    s.a = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    nav_pousser(s);
}

lv_obj_t *construire_liste(Entree &) {
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard("Réglages", false, &ecran);
    lv_obj_set_style_pad_row(corps, S(10), 0);
    for (std::size_t s = 0; s < registre::NOMBRE_SECTIONS; ++s) {
        char valeur[64];
        resume_section(static_cast<registre::Section>(s), valeur, sizeof valeur);
        creer_rangee_bouton(corps, T(registre::titre_section(static_cast<registre::Section>(s)).data()), valeur,
                            sur_section, reinterpret_cast<void *>(static_cast<intptr_t>(s)));
    }
    return ecran;
}

} // namespace

// ---- rangées de réglages ------------------------------------------------------------------------------------------------------

namespace {

// Appliqué quand une pastille est touchée : la valeur part tout de suite à l'hôte, puis les conséquences visibles.
void appliquer_pastille(const registre::Reglage &r, int option) {
    HoteReglages &h = hote();
    if (r.id == "langue.ecran") {
        g_langue_choisie = true;
        const i18n::Langue ancienne = i18n::langue_depuis_indice(h.nombre("langue.ecran"));
        const i18n::Langue assistant = i18n::langue_depuis_indice(h.nombre("langue.assistant"));
        const i18n::Langue nouvelle = i18n::langue_depuis_indice(option);
        h.definir_nombre("langue.ecran", option);
        h.definir_nombre("langue.assistant", i18n::indice(installation::langue_assistant_apres_choix(
                                                 g_mode_installation, ancienne, assistant, nouvelle)));
        i18n::definir_langue(nouvelle);
        nav_reconstruire();
        return;
    }
    h.definir_nombre(r.id, option);
    if (r.id == "affichage.theme") {
        env_choisir_theme();
        nav_reconstruire();
        return;
    }
    const std::size_t point = r.id.find('.');
    if (point != std::string_view::npos && r.id.substr(point) == ".preset") {
        // Le préréglage choisi remplit l'adresse du service ; « Personnalisé » la laisse telle quelle.
        const registre::Preset *preset = registre::chercher_preset(r.options[option].code);
        if (preset != nullptr && !preset->url.empty()) {
            std::string id_url(r.id.substr(0, point));
            id_url += ".url";
            h.definir_texte(id_url, preset->url);
        }
    }
    nav_reconstruire_haut();
}

void sur_pastille(lv_event_t *e) {
    const intptr_t code = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    appliquer_pastille(registre::REGISTRE[code >> 8], static_cast<int>(code & 0xFF));
}

void rangee_pastilles(lv_obj_t *parent, const registre::Reglage &r) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *carte = creer_carte(parent);
    creer_texte(carte, T(r.libelle.data()), env().f.petite, p.texte_discret);
    lv_obj_t *ligne = lv_obj_create(carte);
    lv_obj_remove_style_all(ligne);
    lv_obj_set_width(ligne, LV_PCT(100));
    lv_obj_set_height(ligne, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ligne, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(ligne, S(10), 0);
    lv_obj_set_style_pad_column(ligne, S(10), 0);
    lv_obj_remove_flag(ligne, LV_OBJ_FLAG_SCROLLABLE);
    const int32_t choisie = hote().nombre(r.id);
    for (std::size_t i = 0; i < r.nb_options; ++i) {
        creer_bouton(
            ligne, libelle_option(r.options[i]),
            static_cast<int32_t>(i) == choisie ? StyleBouton::Principal : StyleBouton::Discret, sur_pastille,
            reinterpret_cast<void *>((static_cast<intptr_t>(index_reglage(r)) << 8) | static_cast<intptr_t>(i)));
    }
    if (!r.aide.empty()) {
        creer_texte(carte, T(r.aide.data()), env().f.petite, p.texte_discret);
    }
    // Le préréglage choisi : son aide, et le conseil « localhost » si c'est un service du PC.
    if (choisie >= 0 && static_cast<std::size_t>(choisie) < r.nb_options && r.id.size() > 7 &&
        r.id.substr(r.id.size() - 7) == ".preset") {
        const registre::Preset *preset = registre::chercher_preset(r.options[choisie].code);
        if (preset != nullptr) {
            if (!preset->aide.empty()) {
                creer_texte(carte, T(preset->aide.data()), env().f.petite, p.texte_discret);
            }
            if (preset->local) {
                creer_texte(carte, T("Remplacez « localhost » par l’adresse de votre PC, par exemple 192.168.1.20."),
                            env().f.petite, p.avertissement);
            }
        }
    }
}

void sur_curseur_change(lv_event_t *e) {
    lv_obj_t *curseur = lv_event_get_target_obj(e);
    lv_obj_t *valeur = static_cast<lv_obj_t *>(lv_obj_get_user_data(curseur));
    const registre::Reglage &r = registre::REGISTRE[reinterpret_cast<intptr_t>(lv_event_get_user_data(e))];
    char t[24];
    std::snprintf(t, sizeof t, "%d%s%s", static_cast<int>(lv_slider_get_value(curseur)), r.unite.empty() ? "" : " ",
                  r.unite.data());
    lv_label_set_text(valeur, t);
}

void sur_curseur_relache(lv_event_t *e) {
    lv_obj_t *curseur = lv_event_get_target_obj(e);
    const registre::Reglage &r = registre::REGISTRE[reinterpret_cast<intptr_t>(lv_event_get_user_data(e))];
    hote().definir_nombre(r.id, lv_slider_get_value(curseur));
}

void rangee_curseur(lv_obj_t *parent, const registre::Reglage &r) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *carte = creer_carte(parent);
    lv_obj_t *tete = creer_ligne(carte);
    lv_obj_t *titre = creer_texte(tete, T(r.libelle.data()), env().f.petite, p.texte_discret);
    lv_obj_set_flex_grow(titre, 1);
    lv_obj_set_width(titre, LV_SIZE_CONTENT);
    char t[24];
    resume_reglage(r, t, sizeof t);
    lv_obj_t *valeur = creer_texte(tete, t, env().f.texte, p.texte);
    lv_obj_set_width(valeur, LV_SIZE_CONTENT);
    // Zone tactile : LVGL 9.5 accepte le toucher sur toute la barre élargie de LV_DPX(8) seulement (lv_slider.c:250 pose l'élargissement,
    // lv_obj_pos.c:1195-1205 le lit ; le test « rond seul » de lv_slider.c:269-277 ne tourne que si LV_OBJ_FLAG_ADV_HITTEST est posé,
    // ce que le curseur ne fait pas). On élargit donc la barre jusqu'à kToucheMinPx de haut, dans une zone de cette hauteur : un parent
    // ne laisse passer le toucher que dans ses propres limites (lv_indev.c:636), il doit donc contenir toute la zone élargie. Le rond
    // déborde de la barre de la moitié de sa taille aux deux bouts : la zone laisse S(30) de chaque côté pour le contenir.
    lv_obj_t *zone = creer_ligne(carte);
    const int32_t haut_zone = S(kToucheMinPx);
    const int32_t marge_tactile = (haut_zone - S(22)) / 2;
    lv_obj_set_height(zone, haut_zone);
    lv_obj_set_style_pad_hor(zone, S(30), 0);
    lv_obj_t *curseur = lv_slider_create(zone);
    lv_obj_set_flex_grow(curseur, 1);
    lv_obj_set_height(curseur, haut_zone - 2 * marge_tactile);
    lv_obj_set_ext_click_area(curseur, marge_tactile);
    lv_obj_set_style_pad_all(curseur, S(16), LV_PART_KNOB);
    lv_obj_set_style_bg_color(curseur, couleur(p.separateur), LV_PART_MAIN);
    lv_obj_set_style_bg_color(curseur, couleur(p.accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(curseur, couleur(p.accent), LV_PART_KNOB);
    lv_slider_set_range(curseur, r.min, r.max);
    lv_slider_set_value(curseur, hote().nombre(r.id), LV_ANIM_OFF);
    lv_obj_set_user_data(curseur, valeur);
    const void *ref = reinterpret_cast<void *>(static_cast<intptr_t>(index_reglage(r)));
    lv_obj_add_event_cb(curseur, sur_curseur_change, LV_EVENT_VALUE_CHANGED, const_cast<void *>(ref));
    lv_obj_add_event_cb(curseur, sur_curseur_relache, LV_EVENT_RELEASED, const_cast<void *>(ref));
}

void sur_rangee_editeur(lv_event_t *e) {
    const registre::Reglage &r = registre::REGISTRE[reinterpret_cast<intptr_t>(lv_event_get_user_data(e))];
    const registre::Rangee *rangee = registre::rangee_du_reglage(r.id);
    Entree n;
    switch (rangee != nullptr ? rangee->editeur : registre::Editeur::Aucun) {
    case registre::Editeur::ListeWifi:
        n.type = TypeEcran::ListeWifi;
        break;
    case registre::Editeur::RechercheVille:
        n.type = TypeEcran::RechercheVille;
        break;
    case registre::Editeur::ListeSource:
        n.type = TypeEcran::ListeSource;
        break;
    case registre::Editeur::Clavier:
    case registre::Editeur::ClavierSecret:
        n.type = TypeEcran::EditeurTexte;
        n.a = static_cast<int>(ModeSaisie::Reglage);
        std::snprintf(n.id, sizeof n.id, "%.*s", static_cast<int>(r.id.size()), r.id.data());
        break;
    default:
        return;
    }
    nav_pousser(n);
}

} // namespace

void rangee_pour(lv_obj_t *parent, const registre::Reglage &r, const registre::Rangee &rangee) {
    switch (rangee.editeur) {
    case registre::Editeur::Pastilles:
        rangee_pastilles(parent, r);
        break;
    case registre::Editeur::Curseur:
        rangee_curseur(parent, r);
        break;
    case registre::Editeur::Clavier:
    case registre::Editeur::ClavierSecret:
    case registre::Editeur::ListeWifi:
    case registre::Editeur::RechercheVille:
    case registre::Editeur::ListeSource: {
        char valeur[96];
        resume_reglage(r, valeur, sizeof valeur);
        creer_rangee_bouton(parent, T(r.libelle.data()), valeur, sur_rangee_editeur,
                            reinterpret_cast<void *>(static_cast<intptr_t>(index_reglage(r))));
        break;
    }
    case registre::Editeur::Aucun:
        break;
    }
}

namespace {

int service_de(std::string_view id) {
    for (int i = 0; i < 3; ++i) {
        if (id.find(kServices[i]) != std::string_view::npos) {
            return i;
        }
    }
    return -1;
}

void sur_tester(lv_event_t *e) {
    const int s = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    hote().tester_service(kServices[s]);
    nav_reconstruire_haut();
}

void sur_refaire(lv_event_t *) {
    hote().definir_progression(installation::refaire(hote().progression()));
    nav_vider();
    installation_reinitialiser_etat();
    definir_mode_installation(installation::Mode::Refaire);
    Entree n;
    n.type = TypeEcran::Etape;
    n.a = static_cast<int>(registre::Etape::Langue);
    nav_pousser(n);
}

void sur_redemarrer(lv_event_t *) { hote().redemarrer(); }

// « Diagnostic » : l'écran de diagnostic existant (ecran_principal.cpp), sans rien réécrire. Les réglages restent ouverts dessous.
void sur_diagnostic(lv_event_t *) {
    if (env().ouvrir_diagnostic != nullptr) {
        env().ouvrir_diagnostic();
    }
}

void sur_nouveau_code(lv_event_t *) {
    hote().nouveau_code();
    nav_reconstruire_haut();
}

void recommencer_installation_a_zero() {
    nav_vider();
    i18n::definir_langue(i18n::langue_depuis_indice(hote().nombre("langue.ecran")));
    env_choisir_theme();
    installation_reinitialiser_etat();
    g_langue_choisie = false;
    definir_mode_installation(installation::Mode::Premiere);
    Entree n;
    n.type = TypeEcran::Etape;
    n.a = static_cast<int>(registre::Etape::Langue);
    nav_pousser(n);
}

void sur_reinitialiser(lv_event_t *) {
    const auto r = installation::confirmer(g_confirmation, installation::Geste::Toucher, hote().maintenant_ms());
    g_confirmation = r.etat;
    if (r.executer) {
        hote().reinitialiser();
        recommencer_installation_a_zero();
        return;
    }
    nav_reconstruire_haut();
}

void sur_annuler_reinitialisation(lv_event_t *) {
    g_confirmation = installation::confirmer(g_confirmation, installation::Geste::Annuler, hote().maintenant_ms()).etat;
    nav_reconstruire_haut();
}

// Appelé deux fois par seconde par une page : la confirmation retombe seule, un test de service se termine.
void minuteur_page(lv_timer_t *t) {
    if (!est_le_dessus(t)) {
        return;
    }
    bool changement = false;
    if (g_confirmation.etat == installation::Confirmation::Armee) {
        const auto r = installation::confirmer(g_confirmation, installation::Geste::Temps, hote().maintenant_ms());
        if (r.etat.etat != g_confirmation.etat) {
            g_confirmation = r.etat;
            changement = true;
        }
    }
    for (int s = 0; s < 3; ++s) {
        const EtatTest vu = hote().resultat_test(kServices[s]).etat;
        if (vu != g_tests_vus[s]) {
            g_tests_vus[s] = vu;
            changement = true;
        }
    }
    if (changement) {
        nav_reconstruire_haut();
    }
}

} // namespace

void ajouter_minuteur_page(lv_obj_t *ecran) { ajouter_timer(ecran, minuteur_page, 500); }

void rangee_action_ou_info(lv_obj_t *parent, std::string_view id) {
    const PaletteReglages &p = *env().p;
    HoteReglages &h = hote();
    char tampon[160];
    if (id == "info.wifi") {
        lv_obj_t *c = creer_carte(parent);
        creer_texte(c, T("Wi-Fi"), env().f.petite, p.texte_discret);
        const InfoWifi w = h.wifi_info();
        if (w.connecte) {
            const Param ps[] = {{"ssid", w.ssid}};
            creer_texte(c, remplir("Connecté à « {ssid} »", ps, 1, tampon, sizeof tampon), env().f.texte, p.succes);
            const Param pi[] = {{"ip", w.ip}};
            char t2[64];
            creer_texte(c, remplir("Adresse : {ip}", pi, 1, t2, sizeof t2), env().f.petite, p.texte_discret);
        } else {
            creer_texte(c, T("Non connecté"), env().f.texte, p.texte);
        }
    } else if (id == "info.meteo") {
        lv_obj_t *c = creer_carte(parent);
        const ApercuMeteo m = h.meteo_apercu();
        char ville[96];
        h.texte("lieu.ville", ville, sizeof ville);
        if (m.disponible && ville[0] != '\0') {
            char *virgule = std::strchr(ville, ',');
            if (virgule != nullptr) {
                *virgule = '\0'; // « Lyon, Auvergne-Rhône-Alpes, France » → « Lyon »
            }
            char temp[16];
            std::snprintf(temp, sizeof temp, "%.0f", static_cast<double>(m.temperature_c));
            const Param ps[] = {{"ville", ville}, {"temp", temp}, {"meteo", m.libelle}};
            creer_texte(c, remplir("À {ville} : {temp} °C, {meteo}", ps, 3, tampon, sizeof tampon), env().f.texte,
                        p.texte);
            // Le service qui a RÉELLEMENT donné cette météo, avec la phrase de sa licence : là où la météo est montrée (ADR-0014).
            // Open-Meteo : CC BY 4.0 ; MET Norway : CC BY 4.0 et NLOD 2.0, les valeurs sont converties et complétées ici (« adaptées »,
            // ce que CC BY demande d'indiquer) ; NWS : données ouvertes (formule de la page de son API).
            namespace fm = meteo::fournisseurs;
            const Param pl[] = {{"lien", fm::attribution_de(m.origine).nom.data()}};
            const char *attribution =
                m.origine == fm::Famille::MetNo
                    ? remplir("Données météo : {lien} (licences CC BY 4.0 et NLOD 2.0, données adaptées)", pl, 1,
                              tampon, sizeof tampon)
                : m.origine == fm::Famille::Nws
                    ? remplir("Données météo : {lien} (données ouvertes)", pl, 1, tampon, sizeof tampon)
                    : remplir("Données météo : {lien} (licence CC BY 4.0)", pl, 1, tampon, sizeof tampon);
            creer_texte(c, attribution, env().f.petite, p.texte_discret);
            if (m.origine == fm::Famille::Nws) {
                // Le NWS demande de ne pas laisser croire à son approbation (ses conditions d'usage, docs/mesures/fournisseurs-meteo.md).
                creer_texte(c, T("Cet appareil n’est ni approuvé ni parrainé par le National Weather Service."),
                            env().f.petite, p.texte_discret);
            }
            const std::int32_t source = h.nombre("meteo.source");
            if (source >= 0 && static_cast<std::size_t>(source) < fm::NOMBRE) {
                const fm::Fournisseur &choisie = fm::TABLE[source];
                if (choisie.famille != fm::Famille::OpenMeteo && m.origine == fm::Famille::OpenMeteo) {
                    // Le service choisi n'a pas répondu : la météo vient d'Open-Meteo en attendant, et l'écran le dit.
                    const Param ps2[] = {{"service", choisie.libelle.data()}};
                    creer_texte(c,
                                remplir("{service} est indisponible : la météo vient d’Open-Meteo en attendant.", ps2,
                                        1, tampon, sizeof tampon),
                                env().f.petite, p.avertissement);
                } else if (source > 0 && choisie.famille == fm::Famille::OpenMeteo) {
                    const Param pm[] = {{"modele", choisie.libelle.data()}};
                    creer_texte(c, remplir("Modèle : {modele}", pm, 1, tampon, sizeof tampon), env().f.petite,
                                p.texte_discret);
                }
            }
        } else {
            creer_texte(c, T("Pas encore de ville"), env().f.texte, p.texte_discret);
        }
        if (h.source_meteo_retablie()) {
            // Le changement de ville a remis « Automatique » : on le dit, jamais de retour muet (ADR-0010).
            creer_texte(c, T("Source remise sur « Automatique » : l’ancienne ne couvre pas ce pays."), env().f.petite,
                        p.avertissement);
        }
    } else if (id == "info.telephone") {
        lv_obj_t *c = creer_carte(parent);
        creer_texte(c, T("Réglages depuis un téléphone"), env().f.petite, p.texte_discret);
        const InfoAppareil i = h.info_appareil();
        if (i.adresse[0] != '\0') {
            creer_texte(c, T("Adresse de la page"), env().f.petite, p.texte_discret);
            creer_texte(c, i.adresse, env().f.texte, p.texte);
            creer_texte(c, T("Code d’appairage"), env().f.petite, p.texte_discret);
            creer_texte(c, i.code, env().f.titre, p.accent);
            creer_bouton(c, T("Nouveau code"), StyleBouton::Discret, sur_nouveau_code, nullptr);
        } else {
            creer_texte(c, T("Le Wi-Fi n’est pas connecté : la page de réglages n’est pas joignable."), env().f.petite,
                        p.avertissement);
        }
    } else if (id == "info.version") {
        lv_obj_t *c = creer_carte(parent);
        const InfoAppareil info = h.info_appareil();
        const Param ps[] = {{"version", info.version}};
        creer_texte(c, remplir("Version {version}", ps, 1, tampon, sizeof tampon), env().f.petite, p.texte_discret);
    } else if (id.substr(0, 14) == "action.tester_") {
        const int s = service_de(id);
        lv_obj_t *c = creer_carte(parent);
        lv_obj_t *b = creer_bouton(c, T("Tester"), StyleBouton::Discret, sur_tester,
                                   reinterpret_cast<void *>(static_cast<intptr_t>(s)));
        lv_obj_set_width(b, LV_PCT(100));
        const ResultatTest r = s >= 0 ? h.resultat_test(kServices[s]) : ResultatTest{};
        if (s >= 0) {
            g_tests_vus[s] = r.etat;
        }
        if (r.etat == EtatTest::EnCours) {
            creer_texte(c, T("Test en cours…"), env().f.petite, p.texte_discret);
        } else if (r.etat != EtatTest::Inactif) {
            creer_texte(c, r.message, env().f.petite, r.etat == EtatTest::Reussi ? p.succes : p.danger);
        }
    } else if (id == "action.diagnostic") {
        lv_obj_t *c = creer_carte(parent);
        lv_obj_t *b =
            creer_bouton(c, ecran::visuel::texte_c("Diagnostic"), StyleBouton::Discret, sur_diagnostic, nullptr);
        lv_obj_set_width(b, LV_PCT(100));
    } else if (id == "action.refaire_installation") {
        lv_obj_t *c = creer_carte(parent);
        lv_obj_t *b = creer_bouton(c, T("Refaire l’installation guidée"), StyleBouton::Discret, sur_refaire, nullptr);
        lv_obj_set_width(b, LV_PCT(100));
        creer_texte(c, T("L’installation guidée repart avec vos réglages actuels."), env().f.petite, p.texte_discret);
    } else if (id == "action.redemarrer") {
        lv_obj_t *c = creer_carte(parent);
        lv_obj_t *b = creer_bouton(c, T("Redémarrer"), StyleBouton::Discret, sur_redemarrer, nullptr);
        lv_obj_set_width(b, LV_PCT(100));
    } else if (id == "action.reinitialiser") {
        lv_obj_t *c = creer_carte(parent);
        const bool armee = g_confirmation.etat == installation::Confirmation::Armee;
        lv_obj_t *b = creer_bouton(c, armee ? T("Toucher encore pour tout effacer") : T("Réinitialiser l’appareil"),
                                   StyleBouton::Danger, sur_reinitialiser, nullptr);
        lv_obj_set_width(b, LV_PCT(100));
        creer_texte(c, T("Tous les réglages, les clés et le Wi-Fi seront effacés."), env().f.petite, p.texte_discret);
        if (armee) {
            lv_obj_t *a = creer_bouton(c, T("Annuler"), StyleBouton::Discret, sur_annuler_reinitialisation, nullptr);
            lv_obj_set_width(a, LV_PCT(100));
        }
    }
}

void construire_page(lv_obj_t *corps, const registre::Page &page) {
    for (std::size_t i = 0; i < page.nb; ++i) {
        const registre::Rangee &r = page.rangees[i];
        if (!r.dans_la_liste) {
            continue;
        }
        if (r.genre == registre::Genre::Reglage) {
            const registre::Reglage *reglage = registre::chercher(r.ref);
            if (reglage != nullptr) {
                rangee_pour(corps, *reglage, r);
            }
        } else {
            rangee_action_ou_info(corps, r.ref);
        }
    }
}

// ---- une section ------------------------------------------------------------------------------------------------------------------

namespace {

void sur_onglet(lv_event_t *e) {
    Entree *haut = nav_haut();
    if (haut != nullptr) {
        haut->b = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        haut->scroll_y = 0; // un autre onglet est une autre page : on repart du haut
        supprimer(haut->obj);
        haut->obj = construire(*haut);
    }
}

} // namespace

void construire_corps_section(lv_obj_t *corps, registre::Section section, int onglet_voulu) {
    const std::size_t nb = registre::pages_de_section(section);
    const std::size_t onglet =
        std::min<std::size_t>(static_cast<std::size_t>(std::max(onglet_voulu, 0)), nb > 0 ? nb - 1 : 0);
    if (nb > 1) {
        lv_obj_t *onglets = creer_ligne(corps);
        for (std::size_t i = 0; i < nb; ++i) {
            const registre::Page &page = registre::PAGES[registre::page_de_section(section, i)];
            lv_obj_t *b =
                creer_bouton(onglets, T(page.titre.data()), i == onglet ? StyleBouton::Principal : StyleBouton::Discret,
                             sur_onglet, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
            lv_obj_set_flex_grow(b, 1);
            lv_obj_set_width(b, 0);
            lv_obj_set_style_pad_hor(b, S(8), 0);
            lv_obj_t *l = lv_obj_get_child(b, 0);
            lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
            lv_obj_set_width(l, LV_PCT(100));
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(l, env().f.petite, 0);
        }
    }
    const std::size_t page = registre::page_de_section(section, onglet);
    if (page < registre::NOMBRE_PAGES) {
        construire_page(corps, registre::PAGES[page]);
    }
}

namespace {

lv_obj_t *construire_section(Entree &e) {
    const registre::Section section = static_cast<registre::Section>(e.a);
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard(T(registre::titre_section(section).data()), true, &ecran);
    construire_corps_section(corps, section, e.b);
    ajouter_minuteur_page(ecran);
    return ecran;
}

} // namespace

// ---- éditeur de texte -------------------------------------------------------------------------------------------------------------

namespace {

struct ContexteSaisie {
    lv_obj_t *champ = nullptr;
    lv_obj_t *message = nullptr;
    lv_obj_t *bouton_oeil = nullptr;
    const registre::Reglage *reglage = nullptr;
    ModeSaisie mode = ModeSaisie::Reglage;
    bool masque = false;
};
ContexteSaisie g_saisie;

void afficher_message(const char *texte, uint32_t couleur_rgb) {
    if (g_saisie.message != nullptr) {
        lv_label_set_text(g_saisie.message, texte);
        lv_obj_set_style_text_color(g_saisie.message, couleur(couleur_rgb), 0);
    }
}

void sur_validation_texte(void *) {
    if (g_saisie.champ == nullptr || g_saisie.reglage == nullptr) {
        return;
    }
    const PaletteReglages &p = *env().p;
    const std::string_view saisi = lv_textarea_get_text(g_saisie.champ);
    const registre::Reglage &r = *g_saisie.reglage;
    // Un secret laissé vide sur un réglage qui en a déjà un : inchangé (l'API fait pareil : « absent = clé inchangée »).
    const bool inchange = r.type == registre::Type::Secret && saisi.empty() && g_saisie.mode == ModeSaisie::Reglage &&
                          hote().secret_defini(r.id);
    if (!inchange) {
        char tampon[96];
        switch (registre::valider_texte(r, saisi)) {
        case registre::ErreurTexte::Aucune:
            break;
        case registre::ErreurTexte::Vide:
        case registre::ErreurTexte::CaractereInterdit:
            afficher_message(T("Ce champ ne peut pas rester vide."), p.danger);
            return;
        case registre::ErreurTexte::TropCourt: {
            char n[12];
            std::snprintf(n, sizeof n, "%d", static_cast<int>(r.min));
            const Param ps[] = {{"n", n}};
            afficher_message(remplir("Trop court : {n} caractères au moins.", ps, 1, tampon, sizeof tampon), p.danger);
            return;
        }
        case registre::ErreurTexte::TropLong: {
            char n[12];
            std::snprintf(n, sizeof n, "%d", static_cast<int>(r.max));
            const Param ps[] = {{"n", n}};
            afficher_message(remplir("Trop long : {n} caractères au plus.", ps, 1, tampon, sizeof tampon), p.danger);
            return;
        }
        }
    }
    // L'adresse de Vigie : « https:// » obligatoire, les mêmes règles que la page web (ecran/reglages_alertes.hpp). Vide : l'instance
    // publique. Sans ce contrôle, une adresse refusée par l'hôte passerait pour enregistrée.
    if (g_saisie.mode == ModeSaisie::Reglage && r.id == "alertes.adresse" &&
        reglages::appliquer_adresse(reglages::Alertes{}, saisi).erreur != reglages::ErreurAlertes::Aucune) {
        afficher_message(T("L’adresse de Vigie doit commencer par https://."), p.danger);
        return;
    }
    switch (g_saisie.mode) {
    case ModeSaisie::Reglage:
        if (!inchange) {
            hote().definir_texte(r.id, saisi);
        }
        nav_retour();
        break;
    case ModeSaisie::MotDePasseWifi:
        connexion_demarrer(g_ssid, saisi);
        break;
    case ModeSaisie::NomReseauManuel: {
        char nom[40];
        std::snprintf(nom, sizeof nom, "%.*s", static_cast<int>(saisi.size()), saisi.data());
        definir_ssid_en_cours(nom);
        Entree n;
        n.type = TypeEcran::EditeurTexte;
        n.a = static_cast<int>(ModeSaisie::MotDePasseWifi);
        std::snprintf(n.id, sizeof n.id, "%s", "wifi.mot_de_passe");
        nav_remplacer(n);
        break;
    }
    }
}

void sur_oeil(lv_event_t *) {
    g_saisie.masque = !g_saisie.masque;
    lv_textarea_set_password_mode(g_saisie.champ, g_saisie.masque);
    bouton_changer_texte(g_saisie.bouton_oeil, g_saisie.masque ? T("Afficher") : T("Masquer"));
}

void sur_effacer_secret(lv_event_t *) {
    if (g_saisie.reglage != nullptr) {
        hote().effacer_secret(g_saisie.reglage->id);
        nav_reconstruire_haut();
    }
}

} // namespace

lv_obj_t *construire_editeur_texte(Entree &e) {
    const PaletteReglages &p = *env().p;
    const ModeSaisie mode = static_cast<ModeSaisie>(e.a);
    const registre::Reglage *r = registre::chercher(mode == ModeSaisie::NomReseauManuel ? "wifi.ssid" : e.id);
    if (r == nullptr) {
        r = registre::chercher("wifi.ssid");
    }
    const bool secret = r->type == registre::Type::Secret;

    lv_obj_t *ecran = nullptr;
    const char *titre = mode == ModeSaisie::MotDePasseWifi    ? g_ssid
                        : mode == ModeSaisie::NomReseauManuel ? T("Nom du réseau")
                                                              : T(r->libelle.data());
    lv_obj_t *corps = creer_ecran_standard(titre, true, &ecran);
    lv_obj_set_scroll_dir(corps, LV_DIR_NONE);
    lv_obj_set_style_pad_all(corps, S(12), 0);

    g_saisie = ContexteSaisie{};
    g_saisie.reglage = r;
    g_saisie.mode = mode;
    g_saisie.masque = secret;

    lv_obj_t *ligne = creer_ligne(corps);
    lv_obj_t *champ = lv_textarea_create(ligne);
    lv_textarea_set_one_line(champ, true);
    lv_obj_set_height(champ, S(76));
    lv_obj_set_flex_grow(champ, 1);
    lv_obj_set_style_text_font(champ, env().f.texte, 0);
    lv_obj_set_style_text_color(champ, couleur(p.texte), 0);
    lv_obj_set_style_bg_color(champ, couleur(p.surface_haute), 0);
    lv_obj_set_style_bg_opa(champ, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(champ, couleur(p.accent), 0);
    lv_obj_set_style_border_width(champ, 2, 0);
    lv_obj_set_style_radius(champ, S(14), 0);
    lv_obj_set_style_pad_hor(champ, S(16), 0);
    lv_obj_set_style_pad_ver(champ, S(18), 0);
    lv_obj_set_style_text_color(champ, couleur(p.texte_discret), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_remove_flag(champ, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(champ, LV_STATE_FOCUSED);
    if (secret) {
        lv_textarea_set_password_bullet(champ, "•");
        lv_textarea_set_password_mode(champ, true);
        lv_textarea_set_placeholder_text(champ, T("Mot de passe"));
    } else {
        char actuel[260] = {};
        hote().texte(r->id, actuel, sizeof actuel);
        if (mode == ModeSaisie::Reglage) {
            lv_textarea_set_text(champ, actuel);
        }
    }
    g_saisie.champ = champ;
    g_champ_actuel = champ;
    lv_obj_add_event_cb(champ, sur_suppression_champ, LV_EVENT_DELETE, nullptr);
    if (secret) {
        g_saisie.bouton_oeil = creer_bouton(ligne, T("Afficher"), StyleBouton::Discret, sur_oeil, nullptr);
        lv_obj_set_height(g_saisie.bouton_oeil, S(76));
    }

    lv_obj_t *message = creer_texte(corps, "", env().f.petite, p.texte_discret);
    lv_obj_set_flex_grow(message, 1);
    g_saisie.message = message;
    if (mode == ModeSaisie::MotDePasseWifi) {
        lv_label_set_text(message, T("8 caractères au moins, ou rien pour un réseau ouvert."));
    } else if (!r->aide.empty()) {
        lv_label_set_text(message, T(r->aide.data()));
    }
    if (secret && mode == ModeSaisie::Reglage && hote().secret_defini(r->id)) {
        // Le texte et le bouton sur une seule rangée : le bouton garde sa zone tactile minimale sans prendre une rangée de plus au
        // clavier, qui occupe déjà plus de la moitié de l'écran.
        lv_obj_t *rangee = creer_ligne(corps);
        lv_obj_t *enregistree = creer_texte(rangee, T("Clé enregistrée"), env().f.petite, p.succes);
        lv_obj_set_width(enregistree, 0);
        lv_obj_set_flex_grow(enregistree, 1);
        creer_bouton(rangee, T("Effacer"), StyleBouton::Discret, sur_effacer_secret, nullptr);
    }

    const bool accents = clavier_hauteur(env().hauteur, true) > 0 && i18n::langue_courante() != i18n::Langue::En &&
                         i18n::langue_courante() != i18n::Langue::De && i18n::langue_courante() != i18n::Langue::Tr;
    clavier_creer(corps, champ, clavier_hauteur(env().hauteur, accents), sur_validation_texte, nullptr);
    return ecran;
}

// ---- réseaux Wi-Fi ------------------------------------------------------------------------------------------------------------------

namespace {

void sur_reseau(lv_event_t *e) {
    const std::size_t rang = static_cast<std::size_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    ReseauWifi r[kReseauxMax];
    const std::size_t n = hote().wifi_reseaux(r, kReseauxMax);
    if (rang >= n) {
        return;
    }
    definir_ssid_en_cours(r[rang].ssid);
    if (r[rang].securise) {
        Entree m;
        m.type = TypeEcran::EditeurTexte;
        m.a = static_cast<int>(ModeSaisie::MotDePasseWifi);
        std::snprintf(m.id, sizeof m.id, "%s", "wifi.mot_de_passe");
        nav_pousser(m);
    } else {
        connexion_demarrer(g_ssid, "");
    }
}

void sur_autre_reseau(lv_event_t *) {
    Entree m;
    m.type = TypeEcran::EditeurTexte;
    m.a = static_cast<int>(ModeSaisie::NomReseauManuel);
    std::snprintf(m.id, sizeof m.id, "%s", "wifi.ssid");
    nav_pousser(m);
}

void sur_chercher_encore(lv_event_t *) {
    hote().wifi_chercher();
    nav_reconstruire_haut();
}

// Quatre barres de signal : les premières, pleines, selon le niveau.
void creer_barres(lv_obj_t *parent, int niveau) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *barres = lv_obj_create(parent);
    lv_obj_remove_style_all(barres);
    lv_obj_set_size(barres, S(48), S(40));
    lv_obj_set_flex_flow(barres, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(barres, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_remove_flag(barres, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(barres, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *b = lv_obj_create(barres);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, S(9), S(10 + 10 * i));
        lv_obj_set_style_radius(b, S(2), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, couleur(i < niveau ? p.accent : p.separateur), 0);
    }
}

} // namespace

namespace {
bool g_recherche_wifi_vue = true; // la recherche était en cours quand la liste a été dessinée
} // namespace

lv_obj_t *construire_liste_wifi(Entree &e) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard("Choisissez votre réseau", false, &ecran);
    HoteReglages &h = hote();
    if (e.neuf) {
        h.wifi_chercher(); // une recherche à l'ouverture de la liste, pas à chaque redessin
    }
    g_recherche_wifi_vue = h.wifi_recherche_en_cours();
    ReseauWifi reseaux[kReseauxMax];
    const std::size_t n = g_recherche_wifi_vue ? 0 : h.wifi_reseaux(reseaux, kReseauxMax);
    if (g_recherche_wifi_vue) {
        creer_texte(corps, T("Recherche des réseaux…"), env().f.texte, p.texte_discret);
    } else if (n == 0) {
        creer_texte(corps, T("Aucun réseau trouvé."), env().f.texte, p.texte_discret);
    }
    for (std::size_t i = 0; i < n; ++i) {
        lv_obj_t *b = creer_rangee_bouton(corps, reseaux[i].ssid, nullptr, sur_reseau,
                                          reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        if (!reseaux[i].securise) {
            lv_obj_t *ouvert = lv_label_create(b);
            lv_label_set_text(ouvert, T("ouvert"));
            lv_obj_set_style_text_font(ouvert, env().f.petite, 0);
            lv_obj_set_style_text_color(ouvert, couleur(p.texte_discret), 0);
        }
        creer_barres(b, reseaux[i].niveau);
    }
    lv_obj_t *autre = creer_bouton(corps, T("Autre réseau…"), StyleBouton::Discret, sur_autre_reseau, nullptr);
    lv_obj_set_width(autre, LV_PCT(100));
    lv_obj_t *encore = creer_bouton(corps, T("Chercher à nouveau"), StyleBouton::Discret, sur_chercher_encore, nullptr);
    lv_obj_set_width(encore, LV_PCT(100));
    // La recherche se termine toute seule : on redessine la liste à ce moment.
    ajouter_timer(
        ecran,
        [](lv_timer_t *t) {
            if (est_le_dessus(t) && g_recherche_wifi_vue && !hote().wifi_recherche_en_cours()) {
                nav_reconstruire_haut();
            }
        },
        400);
    return ecran;
}

// ---- source de la météo ---------------------------------------------------------------------------------------------------------
// « Automatique » en tête, puis les sources du pays de la ville (hote().sources_meteo()). Un toucher enregistre le choix et revient :
// l'hôte redemande la météo à la nouvelle source, comme pour un changement de ville.

namespace {

void sur_source(lv_event_t *e) {
    const intptr_t indice = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    hote().definir_nombre("meteo.source", static_cast<int32_t>(indice));
    nav_retour();
}

} // namespace

lv_obj_t *construire_liste_source(Entree &) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard("Source météo", false, &ecran);
    const registre::Reglage &reglage = *registre::chercher("meteo.source");
    creer_texte(corps, T(reglage.aide.data()), env().f.petite, p.texte_discret);
    creer_texte(corps, T("Seules les sources qui couvrent le pays de la ville sont proposées."), env().f.petite,
                p.texte_discret);
    const int32_t choisie = hote().nombre("meteo.source");
    const meteo::fournisseurs::Liste liste = hote().sources_meteo();
    for (std::size_t k = 0; k < liste.n; ++k) {
        const std::size_t i = liste.indices[k];
        lv_obj_t *b = creer_rangee_bouton(corps, libelle_option(reglage.options[i]), nullptr, sur_source,
                                          reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        if (static_cast<int32_t>(i) == choisie) {
            // La source en service, comme la pastille choisie des autres réglages : fond d'accent, texte posé sur l'accent.
            lv_obj_set_style_bg_color(b, couleur(p.accent), 0);
            lv_obj_set_style_bg_color(b, couleur(p.accent), LV_STATE_PRESSED);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 0), couleur(p.texte_sur_accent), 0);
        }
    }
    return ecran;
}

// ---- connexion avec retour arrière ---------------------------------------------------------------------------------------------------

namespace {

wifi::Changement g_changement;
lv_obj_t *g_connexion_reste = nullptr;
bool g_abandon_signale = false;

wifi::Resultat resultat_de(const ConnexionWifi &c) {
    if (c.etat == wifi::Etat::Reussi) {
        return wifi::Resultat::Connecte;
    }
    switch (c.cause) {
    case wifi::Cause::MotDePasse:
        return wifi::Resultat::MotDePasseRefuse;
    case wifi::Cause::Introuvable:
        return wifi::Resultat::ReseauIntrouvable;
    default:
        return wifi::Resultat::Autre;
    }
}

void sur_reessayer(lv_event_t *) { nav_retour(); } // retour à l'éditeur ou à la liste d'où l'on venait

// « Retour » ou « Annuler » pendant l'essai : on n'attend pas le délai, l'hôte reprend l'ancien réseau tout de suite.
void sur_abandonner_essai(lv_event_t *) {
    if (g_changement.etat == wifi::Etat::Connexion) {
        g_abandon_signale = true;
        hote().wifi_abandonner();
    }
    nav_retour();
}

void sur_connexion_ok(lv_event_t *) { nav_retour_jusqu_a_une_base(); }

void sur_connexion_retour_liste(lv_event_t *) { nav_retour_jusqu_a_une_base(); }

void minuteur_connexion(lv_timer_t *t) {
    if (!est_le_dessus(t)) {
        return;
    }
    HoteReglages &h = hote();
    const int64_t now = h.maintenant_ms();
    const wifi::Etat avant = g_changement.etat;
    const ConnexionWifi c = h.wifi_connexion();
    if (c.etat == wifi::Etat::Reussi || c.etat == wifi::Etat::Echec) {
        g_changement = wifi::sur_resultat(g_changement, resultat_de(c));
    }
    g_changement = wifi::sur_temps(g_changement, now);
    if (g_changement.etat == wifi::Etat::Echec && g_changement.cause == wifi::Cause::Delai && !g_abandon_signale) {
        g_abandon_signale = true;
        h.wifi_abandonner(); // l'ancien réseau est repris par la couche réseau
    }
    if (g_changement.etat != avant) {
        nav_reconstruire_haut();
    } else if (g_connexion_reste != nullptr && g_changement.etat == wifi::Etat::Connexion) {
        char n[12];
        char tampon[64];
        std::snprintf(n, sizeof n, "%d", wifi::secondes_restantes(g_changement, now));
        const Param ps[] = {{"n", n}};
        lv_label_set_text(g_connexion_reste, remplir("Temps restant : {n} s", ps, 1, tampon, sizeof tampon));
    }
}

} // namespace

// Démarre l'essai : l'hôte se connecte, le suivi du délai commence, l'écran de connexion s'ouvre par-dessus l'écran du dessus.
void connexion_demarrer(std::string_view ssid, std::string_view mot_de_passe) {
    HoteReglages &h = hote();
    const bool avait_un_reseau =
        h.wifi_info().connecte; // avant l'essai : c'est ce réseau qu'on reprendra en cas d'échec
    definir_ssid_en_cours(ssid);
    h.wifi_connecter(ssid, mot_de_passe);
    g_changement = wifi::demarrer(h.maintenant_ms(), avait_un_reseau);
    g_abandon_signale = false;
    Entree c;
    c.type = TypeEcran::ConnexionWifi;
    nav_pousser(c);
}

lv_obj_t *construire_connexion_wifi(Entree &) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard(g_ssid, true, &ecran, nullptr, sur_abandonner_essai);
    HoteReglages &h = hote();
    char tampon[160];
    const Param pssid[] = {{"ssid", g_ssid}};
    g_connexion_reste = nullptr;
    lv_obj_t *carte = creer_carte(corps);
    switch (g_changement.etat) {
    case wifi::Etat::Inactif:
    case wifi::Etat::Connexion: {
        creer_texte(carte, remplir("Connexion à « {ssid} »…", pssid, 1, tampon, sizeof tampon), env().f.texte, p.texte);
        char n[12];
        char t2[64];
        std::snprintf(n, sizeof n, "%d", wifi::secondes_restantes(g_changement, h.maintenant_ms()));
        const Param pn[] = {{"n", n}};
        g_connexion_reste =
            creer_texte(carte, remplir("Temps restant : {n} s", pn, 1, t2, sizeof t2), env().f.petite, p.texte_discret);
        lv_obj_t *annuler = creer_bouton(carte, T("Annuler"), StyleBouton::Discret, sur_abandonner_essai, nullptr);
        lv_obj_set_width(annuler, LV_PCT(100));
        break;
    }
    case wifi::Etat::Reussi: {
        creer_texte(carte, remplir("Connecté à « {ssid} ».", pssid, 1, tampon, sizeof tampon), env().f.texte, p.succes);
        lv_obj_t *ok = creer_bouton(carte, T("OK"), StyleBouton::Principal, sur_connexion_ok, nullptr);
        lv_obj_set_width(ok, LV_PCT(100));
        break;
    }
    case wifi::Etat::Echec: {
        const char *message = T("La connexion a échoué. Réessayez.");
        switch (g_changement.cause) {
        case wifi::Cause::MotDePasse:
            message = remplir("Mot de passe refusé par « {ssid} ».", pssid, 1, tampon, sizeof tampon);
            break;
        case wifi::Cause::Introuvable:
            message = remplir("Le réseau « {ssid} » est introuvable.", pssid, 1, tampon, sizeof tampon);
            break;
        case wifi::Cause::Delai:
            message = remplir("« {ssid} » ne répond pas.", pssid, 1, tampon, sizeof tampon);
            break;
        default:
            break;
        }
        creer_texte(carte, message, env().f.texte, p.danger);
        if (g_changement.avait_un_reseau) {
            creer_texte(carte, T("L’ancien réseau est conservé."), env().f.petite, p.texte_discret);
        }
        lv_obj_t *encore = creer_bouton(carte, T("Réessayer"), StyleBouton::Principal, sur_reessayer, nullptr);
        lv_obj_set_width(encore, LV_PCT(100));
        lv_obj_t *fin = creer_bouton(carte, T("Annuler"), StyleBouton::Discret, sur_connexion_retour_liste, nullptr);
        lv_obj_set_width(fin, LV_PCT(100));
        break;
    }
    }
    ajouter_timer(ecran, minuteur_connexion, 300);
    return ecran;
}

// ---- recherche de ville --------------------------------------------------------------------------------------------------------------

namespace {

struct ContexteVille {
    lv_obj_t *champ = nullptr;
    lv_obj_t *resultats = nullptr;
    lv_obj_t *etat = nullptr;
    lv_obj_t *clavier = nullptr;
    lv_obj_t *bouton =
        nullptr; // « Rechercher » tant que le clavier est là, « Modifier » quand il est caché pour laisser la place
    char requete[96] = {};
    bool demandee = false;
    RechercheLieu vue = RechercheLieu::Inactive; // l'état de la recherche au dernier dessin des résultats
};
ContexteVille g_ville;

void montrer_clavier_ville(bool montrer) {
    if (g_ville.clavier == nullptr) {
        return;
    }
    if (montrer) {
        lv_obj_remove_flag(g_ville.clavier, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_ville.clavier, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_ville.bouton != nullptr) {
        bouton_changer_texte(g_ville.bouton, montrer ? T("Rechercher") : T("Modifier"));
    }
}

void remplir_resultats() {
    if (g_ville.resultats == nullptr) {
        return;
    }
    const PaletteReglages &p = *env().p;
    HoteReglages &h = hote();
    lv_obj_clean(g_ville.resultats);
    char tampon[160];
    if (!g_ville.demandee) {
        return;
    }
    switch (h.lieu_recherche()) {
    case RechercheLieu::EnCours:
        lv_label_set_text(g_ville.etat, T("Recherche…"));
        return;
    case RechercheLieu::SansConnexion:
        lv_label_set_text(g_ville.etat, T("Pas de connexion : la recherche d’une ville demande Internet."));
        lv_obj_set_style_text_color(g_ville.etat, couleur(p.avertissement), 0);
        return;
    case RechercheLieu::Inactive:
        return;
    case RechercheLieu::Terminee:
        break;
    }
    LieuTrouve lieux[kLieuxMax];
    const std::size_t n = h.lieu_resultats(lieux, kLieuxMax);
    if (n == 0) {
        const Param ps[] = {{"q", g_ville.requete}};
        lv_label_set_text(g_ville.etat, remplir("Aucun lieu trouvé pour « {q} ».", ps, 1, tampon, sizeof tampon));
        return;
    }
    lv_label_set_text(g_ville.etat, "");
    for (std::size_t i = 0; i < n; ++i) {
        creer_rangee_bouton(
            g_ville.resultats, lieux[i].libelle, nullptr,
            [](lv_event_t *e) {
                hote().lieu_choisir(static_cast<std::size_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
                nav_retour();
            },
            reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    }
}

void lancer_recherche(void *) {
    if (g_ville.champ == nullptr) {
        return;
    }
    const PaletteReglages &p = *env().p;
    const std::string_view saisi = lv_textarea_get_text(g_ville.champ);
    const registre::Reglage &r = *registre::chercher("lieu.ville");
    if (registre::nombre_de_caracteres(saisi) < static_cast<std::size_t>(r.min)) {
        lv_label_set_text(g_ville.etat, T("Tapez au moins 3 lettres."));
        lv_obj_set_style_text_color(g_ville.etat, couleur(p.avertissement), 0);
        return;
    }
    lv_obj_set_style_text_color(g_ville.etat, couleur(p.texte_discret), 0);
    std::snprintf(g_ville.requete, sizeof g_ville.requete, "%.*s", static_cast<int>(saisi.size()), saisi.data());
    hote().lieu_chercher(saisi);
    g_ville.demandee = true;
    montrer_clavier_ville(false); // les résultats prennent la place du clavier
    g_ville.vue = hote().lieu_recherche();
    remplir_resultats();
}

void sur_rechercher(lv_event_t *) {
    if (g_ville.clavier != nullptr && lv_obj_has_flag(g_ville.clavier, LV_OBJ_FLAG_HIDDEN)) {
        montrer_clavier_ville(true); // « Modifier » : on retape
    } else {
        lancer_recherche(nullptr);
    }
}

} // namespace

lv_obj_t *construire_recherche_ville(Entree &) {
    const PaletteReglages &p = *env().p;
    lv_obj_t *ecran = nullptr;
    lv_obj_t *corps = creer_ecran_standard("Choisissez votre ville", false, &ecran);
    lv_obj_set_scroll_dir(corps, LV_DIR_NONE);
    lv_obj_set_style_pad_all(corps, S(12), 0);

    g_ville = ContexteVille{};
    lv_obj_t *ligne = creer_ligne(corps);
    lv_obj_t *champ = lv_textarea_create(ligne);
    lv_textarea_set_one_line(champ, true);
    lv_textarea_set_placeholder_text(champ, T("Tapez le nom d’une ville"));
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
    g_ville.champ = champ;
    g_champ_actuel = champ;
    lv_obj_add_event_cb(champ, sur_suppression_champ, LV_EVENT_DELETE, nullptr);
    g_ville.bouton = creer_bouton(ligne, T("Rechercher"), StyleBouton::Principal, sur_rechercher, nullptr);
    lv_obj_set_height(g_ville.bouton, S(76));

    g_ville.etat = creer_texte(corps, "", env().f.petite, p.texte_discret);
    g_ville.resultats = creer_colonne(corps);
    lv_obj_set_width(g_ville.resultats, LV_PCT(100));
    lv_obj_set_height(g_ville.resultats, 0);
    lv_obj_set_flex_grow(g_ville.resultats, 1);
    lv_obj_set_style_pad_row(g_ville.resultats, S(8), 0);
    lv_obj_add_flag(g_ville.resultats, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_ville.resultats, LV_DIR_VER);
    style_defilement(g_ville.resultats);

    g_ville.clavier = clavier_creer(corps, champ, clavier_hauteur(env().hauteur, true), lancer_recherche, nullptr);
    // La réponse peut arriver après la demande : on redessine les résultats quand l'état de la recherche change.
    ajouter_timer(
        ecran,
        [](lv_timer_t *t) {
            if (!est_le_dessus(t) || !g_ville.demandee) {
                return;
            }
            const RechercheLieu maintenant = hote().lieu_recherche();
            if (maintenant != g_ville.vue) {
                g_ville.vue = maintenant;
                remplir_resultats();
            }
        },
        400);
    return ecran;
}

} // namespace ecran::ui

// ---- API publique ----------------------------------------------------------------------------------------------------------------------

namespace ecran {

void ui_reglages_liberer() { ui::nav_vider(); }

void ui_reglages_creer(const ContexteReglages &ctx) {
    ui::nav_vider();
    ui::env_initialiser(ctx.racine, ctx.ttf, ctx.ttf_taille, ctx.largeur, ctx.hauteur, ctx.hote, ctx.fermer,
                        ctx.ouvrir_diagnostic);
    i18n::definir_langue(i18n::langue_depuis_indice(ctx.hote->nombre("langue.ecran")));
    ui::Entree e;
    e.type = ui::TypeEcran::Liste;
    ui::nav_pousser(e);
}

namespace {

bool commence_par(const char *s, const char *prefixe) { return std::strncmp(s, prefixe, std::strlen(prefixe)) == 0; }

void pousser_section(int section, int onglet) {
    ui::Entree s;
    s.type = ui::TypeEcran::Section;
    s.a = section;
    s.b = onglet;
    ui::nav_pousser(s);
}

// Écran(s) ouvert(s) PAR-DESSUS l'écran du dessus : « wifi », « wifi-mdp:<ssid> », « connexion:<ssid>[:<mot de passe>] », « ville »,
// « source », « editeur:<identifiant> ».
void pousser_sous_ecran(const char *sous) {
    using namespace ui;
    if (std::strcmp(sous, "wifi") == 0 || commence_par(sous, "wifi-mdp:") || commence_par(sous, "connexion:")) {
        Entree w;
        w.type = TypeEcran::ListeWifi;
        nav_pousser(w);
        if (commence_par(sous, "wifi-mdp:")) {
            definir_ssid_en_cours(sous + 9);
            Entree e;
            e.type = TypeEcran::EditeurTexte;
            e.a = static_cast<int>(ModeSaisie::MotDePasseWifi);
            std::snprintf(e.id, sizeof e.id, "%s", "wifi.mot_de_passe");
            nav_pousser(e);
        } else if (commence_par(sous, "connexion:")) {
            // L'essai est lancé auprès de l'hôte, comme le ferait le clavier.
            const char *ssid = sous + 10;
            const char *deux_points = std::strchr(ssid, ':');
            if (deux_points != nullptr) {
                char nom[40];
                std::snprintf(nom, sizeof nom, "%.*s", static_cast<int>(deux_points - ssid), ssid);
                connexion_demarrer(nom, deux_points + 1);
            } else {
                connexion_demarrer(ssid, "");
            }
        }
    } else if (std::strcmp(sous, "ville") == 0) {
        Entree v;
        v.type = TypeEcran::RechercheVille;
        nav_pousser(v);
    } else if (std::strcmp(sous, "source") == 0) {
        Entree s;
        s.type = TypeEcran::ListeSource;
        nav_pousser(s);
    } else if (commence_par(sous, "editeur:")) {
        Entree e;
        e.type = TypeEcran::EditeurTexte;
        std::snprintf(e.id, sizeof e.id, "%s", sous + 8);
        nav_pousser(e);
    }
}

} // namespace

void ui_aller(const char *chemin) {
    using namespace ui;
    nav_vider();
    const char *barre = std::strchr(chemin, '/');
    const std::size_t longueur_base = barre != nullptr ? static_cast<std::size_t>(barre - chemin) : std::strlen(chemin);
    char base[40];
    std::snprintf(base, sizeof base, "%.*s", static_cast<int>(longueur_base), chemin);

    if (commence_par(base, "etape:")) {
        definir_mode_installation(installation::Mode::Premiere);
        Entree e;
        e.type = TypeEcran::Etape;
        e.a = std::atoi(base + 6);
        nav_pousser(e);
    } else {
        Entree liste;
        liste.type = TypeEcran::Liste;
        nav_pousser(liste);
        if (commence_par(base, "section:")) {
            const char *deux_points = std::strchr(base + 8, ':');
            pousser_section(std::atoi(base + 8), deux_points != nullptr ? std::atoi(deux_points + 1) : 0);
        }
    }
    if (barre != nullptr) {
        pousser_sous_ecran(barre + 1);
    }
}

void ui_simuler_langue_choisie() {
    ui::definir_langue_choisie(true);
    ui::nav_reconstruire_haut();
}

void ui_simuler_saisie(const char *texte, bool valider) {
    // Pour le rendu PC et les essais : pose un texte dans le champ de l'éditeur affiché, comme si on l'avait tapé, puis touche « OK ».
    if (ui::champ_actuel() != nullptr) {
        lv_textarea_set_text(ui::champ_actuel(), texte);
        if (valider) {
            ui::clavier_valider();
        }
    }
}

} // namespace ecran
