// Scène « installation » : l'installation guidée en sept étapes (main/ui_installation.cpp, ui_reglages.cpp, ui_clavier.cpp), compilée
// telle quelle, avec un hôte en mémoire (main/hote_memoire.cpp) à la place de la mémoire permanente, du Wi-Fi et de la météo.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_installation.cpp main/ui_reglages.cpp main/ui_clavier.cpp main/ui_commun.cpp main/hote_memoire.cpp
// rendu-langues: toutes
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 720x1280
// rendu-sequence: installation
//
// Fixtures (inventées, pas des mesures) : cinq réseaux Wi-Fi vus, mot de passe attendu « bonmotdepasse », deux villes qui contiennent
// « Lyon », météo « nuageux, 12 °C », adresse de la page 192.168.1.52, code 482913. La langue choisie à l'écran est celle du rendu.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "banc.h"
#include "hote_memoire.h"
#include "ui_reglages.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using ecran::HoteMemoire;

HoteMemoire g_hote;

const char *meteo_dans(ecran::i18n::Langue langue) {
    switch (langue) {
    case ecran::i18n::Langue::Fr:
        return "Nuageux";
    case ecran::i18n::Langue::En:
        return "Cloudy";
    case ecran::i18n::Langue::De:
        return "Bewölkt";
    case ecran::i18n::Langue::Nl:
        return "Bewolkt";
    case ecran::i18n::Langue::Es:
        return "Nublado";
    case ecran::i18n::Langue::It:
        return "Nuvoloso";
    case ecran::i18n::Langue::Tr:
        return "Bulutlu";
    }
    return "";
}

// Les données communes à toutes les variantes ; `delai_connexion_ms` dit ce que le Wi-Fi fait de l'essai (0 : répond tout de suite).
void preparer(const banc::Contexte &ctx, int64_t delai_connexion_ms) {
    g_hote.fixer_horloge(1000000);
    g_hote.reseaux_vus = {{"Maison_5G", 4, true},
                          {"Freebox-A1B2C3", 3, true},
                          {"Invite", 2, false},
                          {"Cafe du coin", 1, true},
                          {"DIRECT-7C", 1, true}};
    g_hote.mot_de_passe_attendu = "bonmotdepasse";
    g_hote.delai_connexion_ms = delai_connexion_ms;
    g_hote.lieux_connus = {"Lyon, Auvergne-Rhône-Alpes, France", "Lyons, Illinois, États-Unis",
                           "Villeurbanne, Auvergne-Rhône-Alpes, France"};
    g_hote.meteo.disponible = true;
    g_hote.meteo.temperature_c = 12.0F;
    std::snprintf(g_hote.meteo.libelle, sizeof g_hote.meteo.libelle, "%s", meteo_dans(ctx.langue));
    std::snprintf(g_hote.info.adresse, sizeof g_hote.info.adresse, "%s", "http://192.168.1.52");
    std::snprintf(g_hote.info.code, sizeof g_hote.info.code, "%s", "482913");
    g_hote.definir_nombre("langue.ecran", ecran::i18n::indice(ctx.langue));
    g_hote.definir_nombre("langue.assistant", ecran::i18n::indice(ctx.langue));

    lv_obj_t *racine = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(racine);
    lv_obj_set_size(racine, ctx.largeur, ctx.hauteur);
    lv_obj_remove_flag(racine, LV_OBJ_FLAG_SCROLLABLE);

    ecran::ContexteReglages c;
    c.racine = racine;
    c.ttf = roboto_700_start;
    c.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    c.largeur = ctx.largeur;
    c.hauteur = ctx.hauteur;
    c.hote = &g_hote;
    ecran::ui_installation_creer(c);
}

void connecte(bool avec_ville) {
    g_hote.simuler_connecte("Maison_5G", "192.168.1.52");
    if (avec_ville) {
        g_hote.definir_texte("lieu.ville", "Lyon, Auvergne-Rhône-Alpes, France");
    }
}

// ---- les variantes, dans l'ordre de la séquence -------------------------------------------------------------------------

void etape_1(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    ecran::ui_simuler_langue_choisie(); // la personne vient de toucher sa langue
}

void etape_2(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    ecran::ui_aller("etape:2");
}

void wifi_liste(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    ecran::ui_aller("etape:2/wifi");
}

void wifi_mot_de_passe(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    ecran::ui_aller("etape:2/wifi-mdp:Maison_5G");
    ecran::ui_simuler_saisie("bonmotdepasse");
}

void wifi_connexion(const banc::Contexte &ctx) {
    preparer(ctx, -1); // le réseau ne répond pas : on voit le compte à rebours
    ecran::ui_aller("etape:2/connexion:Maison_5G:bonmotdepasse");
    g_hote.avancer_horloge(8000); // 8 s déjà écoulées : « Temps restant : 22 s »
}

void wifi_refuse(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    ecran::ui_aller("etape:2/connexion:Maison_5G:fautedefrappe");
}

void wifi_connecte(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    ecran::ui_aller("etape:2/connexion:Maison_5G:bonmotdepasse");
}

void etape_2_connecte(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(false);
    ecran::ui_aller("etape:2");
}

void etape_3(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(false);
    ecran::ui_aller("etape:3");
}

void ville_saisie(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(false);
    ecran::ui_aller("etape:3/ville");
    ecran::ui_simuler_saisie("Lyo");
}

void ville_resultats(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(false);
    ecran::ui_aller("etape:3/ville");
    ecran::ui_simuler_saisie("Lyon", true);
}

void etape_3_choisie(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(true);
    ecran::ui_aller("etape:3");
}

void etape_4(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(true);
    ecran::ui_aller("etape:4");
}

void etape_5(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(true);
    ecran::ui_aller("etape:5");
}

void etape_6(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(true);
    ecran::ui_aller("etape:6");
}

void etape_7(const banc::Contexte &ctx) {
    preparer(ctx, 0);
    connecte(true);
    ecran::ui_aller("etape:7");
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"etape-1", "Étape 1 : langue", etape_1},
    {"etape-2", "Étape 2 : Wi-Fi, pas encore connecté", etape_2},
    {"wifi-liste", "Étape 2 : choix du réseau", wifi_liste},
    {"wifi-mot-de-passe", "Étape 2 : mot de passe au clavier", wifi_mot_de_passe},
    {"wifi-connexion", "Étape 2 : connexion en cours (compte à rebours)", wifi_connexion},
    {"wifi-refuse", "Étape 2 : mot de passe refusé", wifi_refuse},
    {"wifi-connecte", "Étape 2 : connexion réussie", wifi_connecte},
    {"etape-2-connecte", "Étape 2 : Wi-Fi connecté", etape_2_connecte},
    {"etape-3", "Étape 3 : ville", etape_3},
    {"ville-saisie", "Étape 3 : recherche de ville au clavier", ville_saisie},
    {"ville-resultats", "Étape 3 : résultats de la recherche", ville_resultats},
    {"etape-3-choisie", "Étape 3 : ville choisie et météo", etape_3_choisie},
    {"etape-4", "Étape 4 : affichage", etape_4},
    {"etape-5", "Étape 5 : assistant vocal (facultatif)", etape_5},
    {"etape-6", "Étape 6 : réglages depuis un téléphone", etape_6},
    {"etape-7", "Étape 7 : récapitulatif", etape_7},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
