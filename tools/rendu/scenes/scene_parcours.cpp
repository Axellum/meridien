// Scène « parcours » : des gestes d'utilisateur rejoués sur l'interface des réglages et de l'installation (main/ui_*.cpp, compilée
// telle quelle) avec un doigt virtuel (parcours_outils.h). Chaque variante vérifie un comportement et s'arrête en ECHEC (code de
// sortie non nul) s'il n'est pas celui attendu ; l'image écrite est l'écran à la fin du parcours.
// Ce que cela prouve : la logique des écrans sur le PC. Ce que cela ne prouve pas : le toucher réel, la rapidité sur la carte, la
// mémoire permanente, le vrai Wi-Fi (l'hôte est en mémoire vive, main/hote_memoire.cpp).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_installation.cpp main/ui_reglages.cpp main/ui_clavier.cpp main/ui_commun.cpp main/hote_memoire.cpp
// rendu-langues: toutes
// rendu-langues-defaut: fr
// rendu-tailles: 1280x720 720x1280
// rendu-sequence: parcours
#include "parcours_outils.h"

#include "ecran/texte_modele.hpp"
#include "ui_reglages_interne.h"

namespace {

using namespace parcours;
using ecran::HoteMemoire;
using ecran::installation::Mode;
using ecran::installation::Progression;
using ecran::registre::Etape;

int etape_courante() { return static_cast<int>(g_hote.progression().etape); }

// « Étape 3 sur 7 » dans la langue courante.
bool affiche_etape(int n) {
    char a[8];
    char b[8];
    char texte[96];
    std::snprintf(a, sizeof a, "%d", n);
    std::snprintf(b, sizeof b, "%d", ecran::registre::NOMBRE_ETAPES);
    const ecran::texte::Parametre p[] = {{"n", a}, {"total", b}};
    ecran::texte::remplir(ecran::i18n::tr_c("Étape {n} sur {total}"), p, 2, texte, sizeof texte);
    return affiche(texte);
}

bool affiche_modele(const char *modele_fr, const char *nom, const char *valeur) {
    char texte[160];
    const ecran::texte::Parametre p[] = {{nom, valeur}};
    ecran::texte::remplir(ecran::i18n::tr_c(modele_fr), p, 1, texte, sizeof texte);
    return affiche(texte);
}

std::string texte_du_reglage(const char *id) {
    char t[260] = {};
    g_hote.texte(id, t, sizeof t);
    return t;
}

// ---- les parcours ---------------------------------------------------------------------------------------------------------------

// Première installation de bout en bout : la langue d'abord, le Wi-Fi ensuite (ou son abandon explicite), le reste au fil de l'eau,
// et la progression est écrite à chaque « Continuer ».
void installation_complete(const banc::Contexte &ctx) {
    preparer(ctx, "installation-complete", Interface::Installation);
    exiger(etape_courante() == 1 && affiche_etape(1), "un appareil neuf commence à l'étape 1");
    exiger(!toucher("Continuer"), "« Continuer » doit rester sans effet tant qu'aucune langue n'est touchée");
    exiger(etape_courante() == 1, "la progression ne doit pas avancer sans langue choisie");

    exiger(toucher_texte(ecran::i18n::nom(ctx.langue).data()), "toucher le nom de la langue");
    exiger(g_hote.nombre("langue.ecran") == ecran::i18n::indice(ctx.langue),
           "la langue de l'écran est enregistrée tout de suite");
    exiger(g_hote.nombre("langue.assistant") == ecran::i18n::indice(ctx.langue),
           "la langue de l'assistant suit en première installation");
    exiger(toucher("Continuer") && etape_courante() == 2 && affiche_etape(2), "étape 2 après « Continuer »");

    exiger(!toucher("Continuer") && etape_courante() == 2,
           "sans Wi-Fi ni abandon explicite, l'étape 2 ne se passe pas");
    exiger(toucher("Continuer sans Wi-Fi") && etape_courante() == 3 && affiche_etape(3),
           "« Continuer sans Wi-Fi » mène à l'étape 3");
    exiger(toucher("Continuer") && etape_courante() == 4, "étape 4");
    exiger(toucher("Continuer") && etape_courante() == 5, "étape 5");
    exiger(toucher("Plus tard") && etape_courante() == 6, "« Plus tard » passe l'assistant");
    exiger(toucher("Continuer") && etape_courante() == 7, "étape 7");
    exiger(!g_hote.progression().terminee && g_fermetures == 0, "rien n'est terminé avant « Terminer »");
    exiger(toucher("Terminer"), "toucher « Terminer »");
    exiger(g_hote.progression().terminee, "l'installation est marquée terminée");
    exiger(g_fermetures == 1, "l'interface rend la main à l'écran principal");
}

// Un redémarrage au milieu : l'étape écrite par le dernier « Continuer » est celle qui revient.
void reprise_apres_redemarrage(const banc::Contexte &ctx) {
    g_nom = "reprise-apres-redemarrage";
    g_hote.definir_progression({Etape::Lieu, false, Mode::Premiere});
    preparer(ctx, "reprise-apres-redemarrage", Interface::Installation);
    exiger(affiche_etape(3), "l'installation reprend à l'étape 3");
    exiger(toucher("Retour") && etape_courante() == 2, "« Retour » revient à l'étape 2 et l'écrit");
}

// Un nouveau réseau qui ne répond pas : passé le délai, l'écran le dit, l'hôte reprend l'ancien réseau et l'écran le garde.
void wifi_retour_arriere(const banc::Contexte &ctx) {
    preparer(ctx, "wifi-retour-arriere", Interface::Reglages, -1);
    g_hote.simuler_connecte("Maison_5G", "192.168.1.52");
    ecran::ui_aller("section:0/connexion:Freebox-A1B2C3:fautedefrappe");
    tourner();
    exiger(g_hote.wifi_connexion().etat == ecran::wifi::Etat::Connexion, "l'essai est en cours");
    g_hote.avancer_horloge(31000);
    tourner();
    exiger(g_hote.wifi_connexion().etat == ecran::wifi::Etat::Echec &&
               g_hote.wifi_connexion().cause == ecran::wifi::Cause::Delai,
           "après le délai, l'écran signale l'abandon à l'hôte");
    exiger(g_hote.wifi_info().connecte && std::strcmp(g_hote.wifi_info().ssid, "Maison_5G") == 0,
           "l'ancien réseau reste en place");
    exiger(affiche_modele("« {ssid} » ne répond pas.", "ssid", "Freebox-A1B2C3"), "la phrase d'échec nomme le réseau");
    exiger(existe("L’ancien réseau est conservé."), "l'écran dit que l'ancien réseau est conservé");
}

// Réinitialiser demande deux gestes, et le premier retombe tout seul.
void reinitialisation_deux_gestes(const banc::Contexte &ctx) {
    g_hote.definir_progression({Etape::Langue, true, Mode::Premiere});
    preparer(ctx, "reinitialisation-deux-gestes", Interface::Reglages);
    g_hote.definir_nombre("affichage.luminosite", 30);
    g_hote.definir_texte("lieu.ville", "Lyon, Auvergne-Rhône-Alpes, France");
    ecran::ui_aller("section:5");
    tourner();
    exiger(toucher("Réinitialiser l’appareil"), "premier toucher");
    exiger(g_hote.reinitialisations == 0 && g_hote.nombre("affichage.luminosite") == 30,
           "un seul toucher n'efface rien");
    exiger(existe("Toucher encore pour tout effacer"), "le bouton demande un second toucher");
    g_hote.avancer_horloge(6000);
    tourner();
    exiger(existe("Réinitialiser l’appareil") && !existe("Toucher encore pour tout effacer"),
           "sans second toucher, la demande retombe seule");
    exiger(toucher("Réinitialiser l’appareil"), "nouveau premier toucher");
    exiger(toucher("Toucher encore pour tout effacer"), "second toucher");
    exiger(g_hote.reinitialisations == 1, "l'effacement a eu lieu une fois");
    exiger(g_hote.nombre("affichage.luminosite") == 75, "la luminosité est revenue au défaut");
    exiger(texte_du_reglage("lieu.ville").empty(), "la ville est effacée");
    exiger(!g_hote.progression().terminee && etape_courante() == 1, "l'installation repart de l'étape 1");
    exiger(affiche_etape(1), "l'écran montre l'étape 1");
}

// Refaire l'installation : les valeurs sont gardées, on peut en sortir à tout moment, et ce n'est pas une réinitialisation.
void refaire_puis_quitter(const banc::Contexte &ctx) {
    g_hote.definir_progression({Etape::Langue, true, Mode::Premiere});
    preparer(ctx, "refaire-puis-quitter", Interface::Reglages);
    g_hote.definir_nombre("affichage.luminosite", 30);
    ecran::ui_aller("section:5");
    tourner();
    exiger(toucher("Refaire l’installation guidée"), "toucher « Refaire l'installation guidée »");
    exiger(g_hote.reinitialisations == 0, "refaire n'efface rien");
    exiger(g_hote.progression().mode == Mode::Refaire && !g_hote.progression().terminee,
           "la progression est en mode « refaire »");
    exiger(existe("Quitter l’installation"), "la sortie est proposée");
    exiger(toucher("Continuer") && etape_courante() == 2,
           "en mode « refaire », la langue déjà en place suffit pour continuer");
    exiger(g_hote.nombre("affichage.luminosite") == 30, "les valeurs actuelles sont gardées");
    exiger(toucher("Quitter l’installation"), "quitter");
    exiger(g_hote.progression().terminee, "quitter remet l'installation à « terminée »");
    exiger(existe("Réglages"), "on retrouve la liste des réglages");
}

// Un préréglage remplit l'adresse du service et dit ce qu'il faut faire pour un service du PC.
void preset_service_local(const banc::Contexte &ctx) {
    preparer(ctx, "preset-service-local", Interface::Reglages);
    ecran::ui_aller("section:4:1");
    tourner();
    exiger(toucher("Ollama (mon PC)"), "choisir le préréglage Ollama");
    exiger(texte_du_reglage("llm.url") == "http://localhost:11434/v1", "le préréglage remplit l'adresse");
    exiger(existe("Remplacez « localhost » par l’adresse de votre PC, par exemple 192.168.1.20."),
           "le conseil « localhost » est affiché");
}

// Saisie au clavier d'un texte, majuscule pour une seule lettre, puis « OK » : la valeur est enregistrée et l'éditeur se ferme.
void clavier_saisie(const banc::Contexte &ctx) {
    preparer(ctx, "clavier-saisie", Interface::Reglages);
    ecran::ui_aller("section:4:1/editeur:llm.modele");
    tourner();
    taper_texte("gpt");
    taper("Aa");
    taper("X");
    taper("y"); // la majuscule ne valait que pour une lettre
    exiger(ecran::ui::champ_actuel() != nullptr, "le champ de saisie existe");
    exiger(std::strcmp(lv_textarea_get_text(ecran::ui::champ_actuel()), "gptXy") == 0, "le texte tapé est « gptXy »");
    taper("<-");
    exiger(std::strcmp(lv_textarea_get_text(ecran::ui::champ_actuel()), "gptX") == 0,
           "« <- » efface la dernière lettre");
    taper("OK");
    exiger(texte_du_reglage("llm.modele") == "gptX", "« OK » enregistre la valeur");
    exiger(chercher(est_clavier, nullptr) == nullptr, "l'éditeur est fermé");
}

// Une recherche de ville qui aboutit : le choix est enregistré, l'aperçu météo suit.
void ville_choisie(const banc::Contexte &ctx) {
    preparer(ctx, "ville-choisie", Interface::Reglages);
    ecran::ui_aller("section:1/ville");
    tourner();
    taper("Aa");
    taper("L");
    taper_texte("yo");
    exiger(std::strcmp(lv_textarea_get_text(ecran::ui::champ_actuel()), "Lyo") == 0, "la saisie est « Lyo »");
    exiger(toucher("Rechercher"), "lancer la recherche");
    exiger(affiche("Lyon, Auvergne-Rhône-Alpes, France"), "les résultats s'affichent");
    exiger(toucher_texte("Lyon, Auvergne-Rhône-Alpes, France"), "choisir le premier résultat");
    exiger(texte_du_reglage("lieu.ville") == "Lyon, Auvergne-Rhône-Alpes, France", "la ville est enregistrée");
    exiger(chercher(est_clavier, nullptr) == nullptr, "la recherche est fermée");
}

// La source de la météo : la liste ne montre que ce que le pays de la ville couvre, « Automatique » en tête ; toucher une source
// l'enregistre et revient à la page, qui nomme le modèle et donne l'attribution ; un changement de ville vers un pays que la source
// ne couvre pas la remet sur « Automatique » ET le dit.
void source_meteo_choix(const banc::Contexte &ctx) {
    namespace fm = ecran::meteo::fournisseurs;
    preparer(ctx, "source-meteo-choix", Interface::Reglages);
    g_hote.definir_texte("lieu.ville", "Lyon, Auvergne-Rhône-Alpes, France");
    g_hote.code_pays = "FR";
    ecran::ui_aller("section:1/source");
    tourner();
    exiger(existe("Automatique") && existe("Seules les sources qui couvrent le pays de la ville sont proposées."),
           "la liste commence par « Automatique » et dit qu'elle est filtrée par pays");
    exiger(affiche("NOAA GFS"), "à Lyon, le modèle mondial est proposé");
    exiger(!affiche("KNMI") && !affiche("MET Norway"), "à Lyon, les services d'autres pays ne sont pas proposés");
    exiger(g_hote.nombre("meteo.source") == 0, "la source de départ est « Automatique »");
    exiger(toucher_texte("NOAA GFS"), "choisir NOAA GFS");
    exiger(g_hote.nombre("meteo.source") == static_cast<int32_t>(fm::indice_de("noaa")), "la source est enregistrée");
    exiger(chercher(est_clavier, nullptr) == nullptr && existe("Source météo"), "retour à la page « Lieu et météo »");
    exiger(affiche_modele("Modèle : {modele}", "modele", "NOAA GFS"), "la page nomme le modèle choisi");
    exiger(affiche_modele("Données météo : {lien} (licence CC BY 4.0)", "lien", "Open-Meteo"),
           "l'attribution d'Open-Meteo est donnée");
    // Un service qui ne couvre pas le pays est refusé par l'hôte, comme par l'API.
    g_hote.definir_nombre("meteo.source", static_cast<int32_t>(fm::indice_de("knmi")));
    exiger(g_hote.nombre("meteo.source") == static_cast<int32_t>(fm::indice_de("noaa")),
           "KNMI ne couvre pas la France : la source ne change pas");
    // Amsterdam : le service néerlandais s'ajoute, la liste le montre.
    g_hote.definir_texte("lieu.ville", "Amsterdam, Hollande-Septentrionale, Pays-Bas");
    g_hote.code_pays = "NL";
    ecran::ui_aller("section:1/source");
    tourner();
    exiger(affiche("KNMI") && affiche("NOAA GFS") && !affiche("MET Norway"), "à Amsterdam, KNMI s'ajoute");
    exiger(toucher_texte("KNMI"), "choisir KNMI");
    exiger(affiche_modele("Modèle : {modele}", "modele", "KNMI"), "la page nomme KNMI");
    // De retour en France : l'hôte remet « Automatique », l'écran le dit en une phrase.
    g_hote.code_pays = "FR";
    g_hote.definir_nombre("meteo.source", 0);
    g_hote.source_retablie = true;
    ecran::ui_aller("section:1");
    tourner();
    exiger(existe("Source remise sur « Automatique » : l’ancienne ne couvre pas ce pays."),
           "le retour sur « Automatique » est dit, pas muet");
    exiger(!affiche_modele("Modèle : {modele}", "modele", "KNMI"), "plus de modèle national affiché");
    // Choisir une source efface le message.
    ecran::ui_aller("section:1/source");
    tourner();
    exiger(toucher_texte("NOAA GFS"), "choisir NOAA GFS à nouveau");
    exiger(!existe("Source remise sur « Automatique » : l’ancienne ne couvre pas ce pays."),
           "le message disparaît dès qu'une source est choisie");
}

// Un mot de passe Wi-Fi trop court est refusé par l'écran, sans lancer de connexion.
void wifi_mot_de_passe_trop_court(const banc::Contexte &ctx) {
    preparer(ctx, "wifi-mot-de-passe-trop-court", Interface::Reglages);
    ecran::ui_aller("section:0/wifi-mdp:Maison_5G");
    tourner();
    taper_texte("abc");
    taper("OK");
    exiger(affiche_modele("Trop court : {n} caractères au moins.", "n", "8"), "la phrase d'erreur dit le minimum");
    exiger(g_hote.wifi_connexion().etat == ecran::wifi::Etat::Inactif, "aucune connexion n'est lancée");
}

// La luminosité s'applique dès que le doigt se relève, sans bouton « Enregistrer ».
void luminosite_immediate(const banc::Contexte &ctx) {
    preparer(ctx, "luminosite-immediate", Interface::Reglages);
    ecran::ui_aller("section:3");
    tourner();
    exiger(g_hote.nombre("affichage.luminosite") == 75, "la luminosité de départ est 75 %");
    glisser_curseur(40);
    exiger(g_hote.nombre("affichage.luminosite") == 40, "la valeur est appliquée au relâchement");
}

// La langue de l'écran change tout de suite, titres compris.
void langue_immediate(const banc::Contexte &ctx) {
    preparer(ctx, "langue-immediate", Interface::Reglages);
    ecran::ui_aller("section:2");
    tourner();
    exiger(toucher_texte("Deutsch"), "toucher Deutsch");
    exiger(g_hote.nombre("langue.ecran") == 2, "la langue de l'écran est enregistrée");
    exiger(affiche(ecran::i18n::tr("Langue", ecran::i18n::Langue::De).data()),
           "le titre de l'écran est passé en allemand");
}

// L'essai d'un service : « en cours », puis le résultat.
void service_essai(const banc::Contexte &ctx) {
    preparer(ctx, "service-essai", Interface::Reglages);
    ecran::ui_aller("section:4:1");
    tourner();
    exiger(toucher("Tester"), "toucher « Tester »");
    exiger(existe("Test en cours…"), "l'essai est annoncé en cours");
    g_hote.avancer_horloge(2000);
    tourner();
    // La phrase du résultat vient de l'hôte (en mémoire vive : en français seulement) ; l'écran peut la traduire ou la garder telle quelle.
    exiger(affiche("Le service répond en 0,4 s.") || existe("Le service répond en 0,4 s."),
           "le résultat apparaît tout seul");
}

void dire_zones(const char *quoi) {
    std::fprintf(
        stderr, // la sortie standard est jetée par rendre.py quand le rendu réussit ; l'erreur standard est affichée
        "zones tactiles (%s) : %d boutons ou curseurs vérifiés, le plus petit mesure %d px ; %d clavier(s), touche la "
        "plus basse "
        "%d px ; %d contenu(s) qui dépasse(nt), tous avec leur barre de défilement ; minimum voulu %d px\n",
        quoi, g_tactile.zones, static_cast<int>(g_tactile.plus_petite), g_tactile.claviers,
        static_cast<int>(g_tactile.plus_petite_touche), g_tactile.defilements,
        static_cast<int>(ecran::ui::S(ecran::ui::kToucheMinPx)));
}

// Tout ce qui répond au doigt, sur chaque page des réglages, a au moins la zone tactile minimale ; un contenu qui dépasse montre sa barre.
void zones_tactiles_reglages(const banc::Contexte &ctx) {
    preparer(ctx, "zones-tactiles-reglages", Interface::Reglages);
    g_hote.simuler_connecte("Maison_5G", "192.168.1.52");
    g_hote.definir_texte("lieu.ville", "Lyon, Auvergne-Rhône-Alpes, France");
    g_hote.definir_nombre("llm.preset", 3);
    g_hote.definir_texte("llm.url", "http://192.168.1.20:11434/v1");
    g_hote.definir_texte("llm.modele", "mistral-small");
    g_hote.definir_texte("llm.cle", "cle-factice"); // une clé enregistrée : le bouton « Effacer » apparaît
    const char *const chemins[] = {"liste",
                                   "section:0",
                                   "section:0/wifi",
                                   "section:0/wifi-mdp:Maison_5G",
                                   "section:0/connexion:Freebox-A1B2C3:fautedefrappe",
                                   "section:1",
                                   "section:1/ville",
                                   "section:1/source",
                                   "section:2",
                                   "section:3",
                                   "section:4:0",
                                   "section:4:1",
                                   "section:4:2",
                                   "section:4:1/editeur:llm.cle",
                                   "section:4:1/editeur:llm.url",
                                   "section:5"};
    for (const char *chemin : chemins) {
        ecran::ui_aller(chemin);
        tourner();
        verifier_zones_tactiles(chemin);
    }
    ecran::ui_aller("section:5");
    tourner();
    exiger(toucher("Réinitialiser l’appareil"), "armer la réinitialisation");
    verifier_zones_tactiles("section:5, réinitialisation armée"); // le bouton « Annuler » apparaît
    exiger(toucher("Annuler"), "désarmer la réinitialisation");
    exiger(toucher("Refaire l’installation guidée"), "refaire l'installation");
    exiger(existe("Quitter l’installation"), "la sortie est proposée en mode « refaire »");
    verifier_zones_tactiles(
        "installation refaite depuis les réglages"); // le bouton « Quitter l'installation » apparaît
    exiger(g_tactile.zones > 0, "aucun bouton trouvé : la vérification ne regarde rien");
    exiger(g_tactile.defilements > 0 || ctx.langue != ecran::i18n::Langue::De || ctx.largeur != 1280,
           "en allemand à 1280x720 au moins une page des réglages dépasse : la barre de défilement doit être mesurée");
    dire_zones("réglages");
}

// Idem pour les sept étapes de l'installation guidée.
void zones_tactiles_installation(const banc::Contexte &ctx) {
    preparer(ctx, "zones-tactiles-installation", Interface::Installation);
    ecran::ui_simuler_langue_choisie();
    g_hote.simuler_connecte("Maison_5G", "192.168.1.52");
    g_hote.definir_texte("lieu.ville", "Lyon, Auvergne-Rhône-Alpes, France");
    const char *const chemins[] = {"etape:1",
                                   "etape:2",
                                   "etape:2/wifi",
                                   "etape:2/wifi-mdp:Maison_5G",
                                   "etape:2/connexion:Freebox-A1B2C3:fautedefrappe",
                                   "etape:3",
                                   "etape:3/ville",
                                   "etape:4",
                                   "etape:5",
                                   "etape:6",
                                   "etape:7"};
    for (const char *chemin : chemins) {
        ecran::ui_aller(chemin);
        tourner();
        verifier_zones_tactiles(chemin);
    }
    exiger(g_tactile.zones > 0, "aucun bouton trouvé : la vérification ne regarde rien");
    dire_zones("installation");
}

// Le curseur de luminosité se saisit au doigt sur toute sa zone tactile (pas seulement sur la barre fine ni sur le rond), et un vrai
// appui à côté de la barre fixe la valeur au relâchement.
void curseur_au_doigt(const banc::Contexte &ctx) {
    preparer(ctx, "curseur-au-doigt", Interface::Reglages);
    ecran::ui_aller("section:3");
    tourner();
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *s = chercher(est_curseur, nullptr);
    exiger(s != nullptr, "pas de curseur sur la page Affichage");
    lv_area_t z;
    lv_obj_get_coords(s, &z);
    const int32_t centre_y = (z.y1 + z.y2) / 2;
    const int32_t demi = ecran::ui::S(ecran::ui::kToucheMinPx) / 2;
    const int32_t xs[] = {z.x1 + 10, (z.x1 + z.x2) / 2, z.x2 - 10};
    const int32_t dys[] = {-(demi - 2), -demi / 2, 0, demi / 2, demi - 2};
    for (const int32_t x : xs) {
        for (const int32_t dy : dys) {
            exiger(objet_sous_le_doigt(x, centre_y + dy) == s,
                   "un toucher dans la zone tactile minimale du curseur doit l'atteindre");
        }
        exiger(objet_sous_le_doigt(x, centre_y - demi - ecran::ui::S(14)) != s,
               "au-dessus de la zone tactile, le curseur ne doit plus répondre");
        exiger(objet_sous_le_doigt(x, centre_y + demi + ecran::ui::S(14)) != s,
               "au-dessous de la zone tactile, le curseur ne doit plus répondre");
    }
    exiger(g_hote.nombre("affichage.luminosite") == 75, "la luminosité de départ est 75 %");
    const int32_t x_visee = z.x1 + lv_area_get_width(&z) / 4;
    toucher_point(x_visee,
                  centre_y - demi + ecran::ui::S(6)); // près du bord haut de la zone, bien à côté de la barre fine
    const int32_t mini = lv_slider_get_min_value(s);
    const int32_t maxi = lv_slider_get_max_value(s);
    const int32_t attendu = mini + (maxi - mini) / 4;
    const int32_t obtenu = g_hote.nombre("affichage.luminosite");
    exiger(obtenu != 75 && obtenu >= attendu - (maxi - mini) / 15 && obtenu <= attendu + (maxi - mini) / 15,
           "un appui à côté de la barre, dans la zone tactile, fixe la valeur au quart de la course");
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"zones-tactiles-reglages", "Parcours : zones tactiles des réglages", zones_tactiles_reglages},
    {"zones-tactiles-installation", "Parcours : zones tactiles de l'installation", zones_tactiles_installation},
    {"curseur-au-doigt", "Parcours : curseur saisi au doigt", curseur_au_doigt},
    {"installation-complete", "Parcours : installation de bout en bout", installation_complete},
    {"reprise-apres-redemarrage", "Parcours : reprise de l'installation à l'étape 3", reprise_apres_redemarrage},
    {"wifi-retour-arriere", "Parcours : Wi-Fi qui ne répond pas, ancien réseau gardé", wifi_retour_arriere},
    {"reinitialisation-deux-gestes", "Parcours : réinitialiser demande deux gestes", reinitialisation_deux_gestes},
    {"refaire-puis-quitter", "Parcours : refaire l'installation puis la quitter", refaire_puis_quitter},
    {"preset-service-local", "Parcours : préréglage d'un service du PC", preset_service_local},
    {"clavier-saisie", "Parcours : saisie au clavier", clavier_saisie},
    {"ville-choisie", "Parcours : recherche et choix d'une ville", ville_choisie},
    {"source-meteo-choix", "Parcours : source de la météo, liste par pays, retour sur Automatique", source_meteo_choix},
    {"wifi-mdp-trop-court", "Parcours : mot de passe Wi-Fi trop court", wifi_mot_de_passe_trop_court},
    {"luminosite-immediate", "Parcours : luminosité appliquée au relâchement", luminosite_immediate},
    {"langue-immediate", "Parcours : changement de langue immédiat", langue_immediate},
    {"service-essai", "Parcours : essai d'un service", service_essai},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
