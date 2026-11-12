#include "ui_principal.h"

#include "ui_principal_interne.h"

// Écran principal. Chaque ZONE est un seul objet LVGL qui dessine tout son contenu dans l'événement LV_EVENT_DRAW_MAIN : six zones, la
// pastille de l'alerte officielle et l'anneau tactile, au lieu d'une centaine d'étiquettes. Un objet LVGL coûte de la RAM interne (le
// point étroit de cette carte), une tâche de dessin non. Les réglages ne sont plus un bouton de cet écran : un appui long ouvre la roue
// (ui_roue.cpp, ADR-0021), dont « Réglages » est une entrée.
//
//   accueil (thèmes « ciel », « tableau sombre », « clair »), paysage 1280 x 720
//   +-- lieu -------------------+--------------------------+
//   | heure                  |  courbe des 12 prochaines |
//   | date                   |  heures : colonnes en     |
//   | météo du moment        |  couleur, icônes, extrêmes|
//   +-- bandeau : la pluie ---+--- l'alerte -------------+
//   | cinq jours : nom et icône, 14° / 8°, ruban des températures, vent, pluie |
//
// Le thème « horloge » garde sa composition d'avant (l'heure géante, la météo du moment, les sept jours).
//
// Une zone n'est redessinée que si ses données ont changé (empreinte par zone) : l'horloge change à la minute, le reste à la mise à
// jour de la météo. Ce qui est inconnu est caché ou remplacé par « — », jamais inventé. Une météo périmée est dessinée avec le thème
// grisé (ecran/accueil.hpp).
//
// Couleurs, opacités, dispositions : visuel_themes.hpp et accueil.hpp (aucune couleur écrite ici). Textes : visuel_textes.hpp (sept
// langues). Tout ce qui se calcule sans LVGL est dans le noyau pur (visuel_*.hpp, accueil.hpp, testé sur le PC).

namespace ecran {
namespace principal {

// Cuisson du verre différée (démarrage) : tant que c'est vrai, cuire_verre() ne fait rien et le verre se dessine en direct, au même
// pixel. Hors de Vue : ui_principal_creer() remet Vue à zéro, ce réglage doit survivre à la construction.
bool s_verre_differe = false;

Vue g;
int images_voile = 0; // images de voile allouées et pas encore rendues (ui_principal_images_voile) : 2 au plus

namespace {

void rendre_voiles() {
    for (VoileCuit &v : g.voile) {
        if (v.image != nullptr) {
            lv_draw_buf_destroy(v.image);
            --images_voile;
        }
        v = VoileCuit{};
    }
}

// Une police de l'accueil : celle du registre partagé (ui_ressources.h), la même instance que les pages de même taille (Détails,
// Alertes, Réglages), créée une seule fois pour tout l'appareil ; une reconstruction de l'accueil ne la recrée pas.
Police creer_police(const std::uint8_t *ttf, std::size_t taille, int reference) {
    Police p;
    p.px = px(reference);
    p.f = ressources::police(ttf, taille, p.px);
    return p;
}
} // namespace

} // namespace principal

using namespace principal;

bool ui_principal_creer(lv_obj_t *ecran, const RessourcesPrincipal &ressources, int32_t largeur, int32_t hauteur,
                        const ThemePrincipal &theme) {
    if (largeur <= 0 || hauteur <= 0 || ressources.ttf == nullptr) {
        return false;
    }
    if (g.ciel_image != nullptr) { // une construction précédente (l'écran qui la montrait est supprimé par l'appelant)
        lv_draw_buf_destroy(g.ciel_image);
    }
    rendre_voiles();
    g = Vue{};
    g.theme = &theme;
    g.largeur = largeur;
    g.hauteur = hauteur;
    g.portrait = hauteur > largeur;
    const float ref_l = g.portrait ? 720.0F : 1280.0F;
    const float ref_h = g.portrait ? 1280.0F : 720.0F;
    g.echelle = std::min(static_cast<float>(largeur) / ref_l, static_cast<float>(hauteur) / ref_h);
    g.diametre_anneau = px(88);
    g.seuil_glissement = roue::glissement(roue::ecran_de(largeur, hauteur));
    g.ecran = ecran;

    // Tailles de référence. L'accueil : l'heure 168, la température du moment 54, la date 36, le texte des jours 32 et 28, le plus petit
    // texte 24 (19 px réels sur la 7B). L'horloge en vedette : l'heure 270 px (250 en portrait, 664 px de large) et la température 124.
    g.est_accueil = theme.disposition != Disposition::HORLOGE;
    g.theme_gris = accueil::theme_grise(theme);
    if (g.est_accueil) {
        g.heure = creer_police(ressources.ttf, ressources.ttf_taille, 168);
        g.temperature = creer_police(ressources.ttf, ressources.ttf_taille, 54);
        g.titre = creer_police(ressources.ttf, ressources.ttf_taille, 36);
        g.corps = creer_police(ressources.ttf, ressources.ttf_taille, 32);
        g.petit = creer_police(ressources.ttf, ressources.ttf_taille, 28);
        g.mini = creer_police(ressources.ttf, ressources.ttf_taille, 24);
    } else {
        g.heure = creer_police(ressources.ttf, ressources.ttf_taille, g.portrait ? 250 : 270);
        g.temperature = creer_police(ressources.ttf, ressources.ttf_taille, g.portrait ? 128 : 124);
        g.titre = creer_police(ressources.ttf, ressources.ttf_taille, 42);
        g.corps = creer_police(ressources.ttf, ressources.ttf_taille, 34);
        g.petit = creer_police(ressources.ttf, ressources.ttf_taille, 28);
        g.mini = creer_police(ressources.ttf, ressources.ttf_taille, 23);
    }
    if (g.heure.f == nullptr || g.temperature.f == nullptr || g.titre.f == nullptr || g.corps.f == nullptr ||
        g.petit.f == nullptr || g.mini.f == nullptr) {
        return false;
    }

    lv_obj_set_style_bg_opa(ecran, LV_OPA_COVER, 0);
    lv_obj_add_flag(ecran, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ecran, LV_OBJ_FLAG_SCROLLABLE); // un glissement ne doit pas faire défiler l'écran
    lv_obj_add_event_cb(ecran, sur_toucher, LV_EVENT_ALL, nullptr);
    g.objets = 1;

    for (int z = 0; z < Z_NOMBRE; ++z) {
        g.zone[z] = creer_zone(ecran);
        lv_obj_add_event_cb(g.zone[z], sur_dessin, LV_EVENT_DRAW_MAIN,
                            reinterpret_cast<void *>(static_cast<std::intptr_t>(z)));
    }

    // Pastille de l'alerte officielle (ADR-0009) : cachée tant qu'aucune alerte n'est en vigueur (pas de bandeau permanent), placée par
    // positionner_pastille_officielle. Ses événements ne remontent pas à l'écran : pas d'appui long dessus.
    g.pastille = creer_zone(ecran);
    lv_obj_add_flag(g.pastille, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g.pastille, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_ext_click_area(g.pastille, px(12));
    lv_obj_add_event_cb(g.pastille, sur_pastille, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(g.pastille, sur_dessin_pastille, LV_EVENT_DRAW_MAIN, nullptr);

    // Retour tactile : un anneau sous le doigt, sur la couche supérieure (coordonnées d'écran exactes).
    lv_obj_t *couche = lv_display_get_layer_top(lv_obj_get_display(ecran));
    g.anneau = lv_obj_create(couche);
    lv_obj_remove_style_all(g.anneau);
    lv_obj_remove_flag(g.anneau, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g.anneau, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g.anneau, g.diametre_anneau, g.diametre_anneau);
    lv_obj_set_style_radius(g.anneau, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g.anneau, px(6), 0);
    lv_obj_set_style_border_color(g.anneau, dz::couleur(theme.accent), 0);
    lv_obj_add_flag(g.anneau, LV_OBJ_FLAG_HIDDEN);
    ++g.objets;

    appliquer_disposition();
    appliquer_ciel();
    return true;
}

void ui_principal_mettre_a_jour(const EntreesPrincipal &entrees) {
    if (g.ecran == nullptr) {
        return;
    }
    g.e = entrees;
    std::snprintf(g.date, sizeof g.date, "%s", entrees.date != nullptr ? entrees.date : "");
    std::snprintf(g.date_attente, sizeof g.date_attente, "%s",
                  entrees.date_attente != nullptr ? entrees.date_attente : "");
    g.e.date = g.date;
    g.e.date_attente = g.date_attente;

    const VueMeteo &m = g.e.meteo;
    // La pluie à annoncer : les cumuls horaires, recalés sur la pluie proche au quart d'heure, bornés aux 12 prochaines heures.
    g.pluie = accueil::dans_l_horizon(
        accueil::episode_de_pluie(m.heures, m.nb_heures, g.e.maintenant_s, m.pluie_dans_min), g.e.maintenant_s);

    // Le mode automatique (ADR-0023) : le noyau a décidé (accueil_pilote.hpp), l'écran dessine. Une composition que les données de cet
    // écran ne soutiennent pas retombe sur la composition d'avant (jamais une carte vide) : le bandeau agrandi sans alerte officielle,
    // la carte pluie sans la pluie qu'elle devait dire.
    g.mode = g.est_accueil ? g.e.mode_auto : accueil::Presentation{};
    const Alerte al_jour = alerte_du_jour();
    const accueil::ContenuBandeau cb =
        accueil::composer_bandeau(m.connue, m.perimee, g.pluie.genre, g.e.bandeau.present, al_jour != Alerte::AUCUNE);
    g.carte_pluie = accueil::composer_carte_pluie(g.pluie, m.heures, m.nb_heures, g.e.maintenant_s, g.portrait ? 8 : 9);
    // La bande de pluie de la ligne centrale (ADR-0027) : les quarts d'heure ne couvrent le moment que si la météo n'est pas périmée.
    g.bande = (m.connue && !m.perimee)
                  ? accueil::composer_bande_pluie(m.quarts_debut_s, m.quarts_dix_mm, m.nb_quarts, g.e.maintenant_s)
                  : accueil::BandePluie{};
    accueil::Composition composition = accueil::composition_de(g.mode);
    if (composition == accueil::Composition::AlerteAgrandie && !g.e.bandeau.present) {
        composition = g.mode.carte_pluie ? accueil::Composition::CartePluie : accueil::Composition::Normale;
    }
    // La grande carte qui recouvrait les cinq jours n'est plus choisie (ADR-0027) : la pluie est dite par la bande de la ligne
    // centrale, les cinq jours restent visibles. (La carte et son dessin ne sont pas encore retirés du code.)
    if (composition == accueil::Composition::CartePluie) {
        composition = accueil::Composition::Normale;
    }
    const bool alerte_bandeau = g.e.bandeau.present || cb.droite == accueil::Puce::ALERTE_METEO;
    const bool tuile = cb.gauche == accueil::Puce::PLUIE || cb.gauche == accueil::Puce::PERIMEE;
    const std::uint32_t cle_disposition =
        static_cast<std::uint32_t>(composition) | (alerte_bandeau ? 4U : 0U) | (tuile ? 8U : 0U);
    if (cle_disposition != g.cle_disposition) {
        g.cle_disposition = cle_disposition;
        g.composition = composition;
        g.alerte_dans_bandeau = alerte_bandeau;
        g.tuile_dit_quelque_chose = tuile;
        appliquer_disposition(); // les rectangles du bandeau et de la zone du bas changent (la pastille de l'alerte suit)
    }

    bool tout = false;
    const int langue = i18n::indice(i18n::langue_courante());
    const auto base = [&](Empreinte &e) {
        e.u(static_cast<std::uint32_t>(langue));
        e.u(m.connue);
        e.u(m.perimee); // une météo périmée est dessinée grisée
    };
    // Le lieu change de fuseau (autre ville choisie sur l'écran ou sur la page web) : les heures, les jours et le soleil se redessinent.
    const std::uint32_t cle_fuseau = ecran::heure::cle_du_fuseau(fuseau());
    std::uint32_t neuf[Z_NOMBRE];
    // La carte pluie et la tuile : tout ce qu'elles dessinent (le genre de pluie, les minutes, les barres, la durée, le cumul).
    const auto carte = [&](Empreinte &e) {
        const accueil::CartePluie &cp = g.carte_pluie;
        e.u(g.cle_disposition);
        e.u(cp.presente);
        e.u(static_cast<std::uint32_t>(cp.genre));
        e.u(cp.dans_min);
        e.u(cp.duree_h);
        e.u(cp.cumul_dix_mm);
        e.u(cp.force_connue);
        e.u(static_cast<std::uint32_t>(cp.force));
        e.u(cp.plus_forte_plus_tard);
        e.i(cp.pic_s);
        e.u(cp.echelle_dix);
        for (int i = 0; i < cp.n; ++i) {
            e.i(cp.barre[i].instant_s);
            e.u(cp.barre[i].dix_mm);
        }
        e.u(g.pluie.cumul_dix_mm); // le cumul d'une pluie plus tardive, dit par la tuile
    };
    {
        Empreinte e;
        base(e);
        e.s(m.ville);
        e.u(m.exemple);
        e.u(static_cast<std::uint32_t>(g.mode.raison)); // la pastille du mode : sa phrase, sa teinte
        e.u(static_cast<std::uint32_t>(g.mode.profil));
        e.u(g.mode.epingle);
        e.u(g.e.bandeau.present ? static_cast<std::uint32_t>(g.e.bandeau.niveau)
                                : 0U); // la teinte d'une pastille d'alerte
        if (!g.est_accueil) {          // la composition de l'horloge dit encore l'alerte et la fraîcheur dans sa barre
            e.u(static_cast<std::uint32_t>(alerte_du_jour()));
            e.u(g.e.bandeau.present);
            e.u(g.e.bandeau.empreinte); // texte, niveau et nombre d'autres alertes (noyau : vigie::composer_bandeau)
        }
        neuf[Z_BARRE] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(g.e.heure_connue);
        e.s(g.e.heure);
        e.s(g.e.date);
        e.s(g.e.date_attente);
        neuf[Z_HORLOGE] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(m.code);
        e.u(m.jour);
        e.u(m.temp_dix);
        e.u(m.ressenti_dix);
        e.u(m.min_dix);
        e.u(m.max_dix);
        neuf[Z_MAINTENANT] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(cle_fuseau); // l'étiquette de chaque colonne est l'heure locale de son instant
        e.u(premiere_heure());
        for (int i = 0; i < m.nb_heures; ++i) {
            e.i(m.heures[i].instant_s);
            e.u(m.heures[i].temp_dix);
            e.u(m.heures[i].code);
            e.u(m.heures[i].jour);
            e.u(m.heures[i].pluie_dix_mm);
        }
        neuf[Z_HEURES] = e.h;
    }
    {
        // Le bandeau : ce que dit la pluie (genre, minutes, heure de début, cumul), l'heure du relevé périmé, l'alerte déduite, l'alerte
        // officielle (dessinée par la pastille, que le changement de ce bandeau repositionne).
        Empreinte e;
        base(e);
        e.u(cle_fuseau);
        e.u(static_cast<std::uint32_t>(g.pluie.genre));
        e.u(g.pluie.genre == accueil::GenrePluie::BIENTOT ? g.pluie.dans_min : 0); // « dans N min » change à la minute
        e.i(g.pluie.genre == accueil::GenrePluie::PLUS_TARD ? g.pluie.debut_s / 60 : 0);
        e.u(g.pluie.cumul_dix_mm);
        // La bande de pluie : les barres, et la minute (« fin dans N min », « Pluie dans N min » et le repère « maintenant » avancent).
        e.u(g.bande.presente);
        if (g.bande.presente) {
            e.i(g.e.maintenant_s / 60);
            e.u(static_cast<std::uint32_t>(g.bande.n));
            e.u(static_cast<std::uint32_t>(g.bande.echelle_dix_h));
            for (int i = 0; i < g.bande.n; ++i) {
                e.i(g.bande.barre[i].debut_s);
                e.u(static_cast<std::uint32_t>(g.bande.barre[i].dix_mm));
            }
        }
        e.i(m.perimee ? m.instant_s / 60 : 0); // « Données de 07:52 »
        e.u(static_cast<std::uint32_t>(alerte_du_jour()));
        e.u(g.e.bandeau.present);
        e.u(g.e.bandeau.empreinte);
        carte(e); // la tuile de pluie de la composition d'alerte
        neuf[Z_BANDEAU] = e.h;
    }
    {
        Empreinte e;
        base(e);
        e.u(cle_fuseau); // les noms des jours sont ceux du lieu
        e.u(premier_jour());
        // « aujourd'hui » change à minuit LOCAL, pas à une heure ronde de l'UTC (Kolkata : 18 h 30 UTC) : on range le numéro du jour local
        e.i(ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau()));
        for (int i = 0; i < m.nb_jours; ++i) {
            e.i(m.jours[i].date_s);
            e.u(m.jours[i].code);
            e.u(m.jours[i].min_dix);
            e.u(m.jours[i].max_dix);
            e.u(m.jours[i].proba_pct);
            e.u(m.jours[i].pluie_dix_mm);
            e.u(m.jours[i].vent_dix_kmh);
            e.u(m.jours[i].direction_deg);
        }
        carte(e); // la carte pluie recouvre cette zone ; la composition d'alerte la réduit à une bande
        neuf[Z_SEMAINE] = e.h;
    }
    Empreinte ciel_e;
    ciel_e.u(static_cast<std::uint32_t>(moment_du_jour()));
    ciel_e.u(static_cast<std::uint32_t>(ambiance_du_moment()));
    if (ciel_e.h != g.empreinte_ciel) {
        g.empreinte_ciel = ciel_e.h;
        appliquer_ciel();
        tout = true;
    }
    for (int z = 0; z < Z_NOMBRE; ++z) {
        if (tout || neuf[z] != g.empreinte[z]) {
            g.empreinte[z] = neuf[z];
            if (g.zone[z] != nullptr) {
                lv_obj_invalidate(g.zone[z]);
            }
            if (z == (g.est_accueil ? Z_BANDEAU : Z_BARRE)) {
                positionner_pastille_officielle(); // l'alerte, la langue ou une indication d'état a changé
            }
        }
    }
    if (tout) {
        lv_obj_invalidate(g.ecran);
    }
}

bool ui_principal_changer_theme(const ThemePrincipal &theme) {
    if (g.ecran == nullptr || g.theme == nullptr) {
        return false;
    }
    if (&theme == g.theme) {
        return true;
    }
    // L'horloge en vedette a ses propres polices et sa propre disposition (créées à la construction) : pas de bascule à chaud.
    if ((theme.disposition == Disposition::HORLOGE) != (g.theme->disposition == Disposition::HORLOGE)) {
        return false;
    }
    g.theme = &theme;
    g.theme_gris = accueil::theme_grise(theme);
    lv_obj_set_style_border_color(g.anneau, dz::couleur(theme.accent), 0);
    appliquer_ciel();
    for (int z = 0; z < Z_NOMBRE; ++z) {
        lv_obj_invalidate(g.zone[z]);
    }
    lv_obj_invalidate(g.pastille);
    lv_obj_invalidate(g.ecran);
    return true;
}

void ui_principal_sur_appui_long(void (*rappel)(int32_t x, int32_t y)) { g.appui_long = rappel; }

void ui_principal_sur_alertes(void (*rappel)()) { g.ouvrir_alertes = rappel; }

CoutEcran ui_principal_cout() { return {g.objets, 6}; }

int ui_principal_images_voile() { return images_voile; }

void ui_principal_differer_verre(bool differer) { s_verre_differe = differer; }

void ui_principal_cuire_verre_differe() {
    if (!s_verre_differe) {
        return;
    }
    s_verre_differe = false;
    if (g.ecran != nullptr) {
        cuire_verre(); // invalide l'écran s'il cuit quelque chose : un repeint, au même pixel que l'image affichée
    }
}

} // namespace ecran
