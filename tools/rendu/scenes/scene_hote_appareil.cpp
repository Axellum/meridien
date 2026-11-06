// Scène « hôte de l'appareil » : les MÊMES écrans que scene_parcours.cpp, mais au-dessus de HoteAppareil (ecran/hote_appareil.hpp,
// la logique de l'appareil) avec une mémoire permanente et un appareil factices (test/host/hote_appareil_faux.hpp), au lieu de
// l'hôte en mémoire vive. Un doigt virtuel rejoue des gestes ; chaque variante dit ECHEC (code de sortie non nul) quand l'écran et
// l'hôte ne se comportent pas ensemble comme attendu.
// Ce que cela prouve : le branchement écran → hôte de l'appareil → magasin (rien n'est gardé en double, un secret ne revient pas à
// l'écran, un Wi-Fi n'est retenu qu'après un succès, l'installation reprend après un « redémarrage »). Ce que cela ne prouve pas :
// la NVS, le Wi-Fi du C6 ni le toucher réels (main/hote_appareil_esp.cpp, à constater sur la carte).
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_installation.cpp main/ui_reglages.cpp main/ui_clavier.cpp main/ui_commun.cpp main/hote_memoire.cpp
// rendu-langues: toutes
// rendu-langues-defaut: fr
// rendu-tailles: 1280x720 720x1280
// rendu-sequence: hote-appareil
#include <memory>
#include <string>

#include "../../../test/host/hote_appareil_faux.hpp"
#include "parcours_outils.h" // g_hote (hôte en mémoire) n'est pas utilisé ici : l'hôte de cette scène est celui de l'appareil

namespace {

using namespace parcours;
using ecran::HoteAppareil;
using ecran::faux::MagasinFaux;
using ecran::faux::PortsFaux;
using ecran::installation::Mode;
using ecran::installation::Progression;
using ecran::registre::Etape;

// Un appareil entier : sa mémoire permanente, ses ports et l'hôte par-dessus. Le « redémarrage » refait l'hôte seul, la mémoire
// permanente et les réglages de la page restent (c'est ce qu'un redémarrage garde).
struct Appareil {
    MagasinFaux magasin;
    PortsFaux ports;
    std::unique_ptr<HoteAppareil> hote;
    lv_obj_t *racine = nullptr;

    Appareil() { hote = std::make_unique<HoteAppareil>(ports, magasin); }
};
std::unique_ptr<Appareil> g_appareil;

int etape_courante() { return static_cast<int>(g_appareil->hote->progression().etape); }

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

void construire(const banc::Contexte &ctx, Interface quoi) {
    g_appareil->racine = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(g_appareil->racine);
    lv_obj_set_size(g_appareil->racine, ctx.largeur, ctx.hauteur);
    lv_obj_remove_flag(g_appareil->racine, LV_OBJ_FLAG_SCROLLABLE);

    ecran::ContexteReglages c;
    c.racine = g_appareil->racine;
    c.ttf = roboto_700_start;
    c.ttf_taille = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    c.largeur = ctx.largeur;
    c.hauteur = ctx.hauteur;
    c.hote = g_appareil->hote.get();
    c.fermer = fermer_interface;
    if (quoi == Interface::Installation) {
        ecran::ui_installation_creer(c);
    } else {
        ecran::ui_reglages_creer(c);
    }
    tourner();
}

void preparer_appareil(const banc::Contexte &ctx, const char *nom, Interface quoi) {
    g_nom = nom;
    g_fermetures = 0;
    g_appareil = std::make_unique<Appareil>();
    PortsFaux &p = g_appareil->ports;
    p.horloge = 1000000;
    p.etat_affichage.langue_ecran = ctx.langue;
    p.etat_affichage.langue_assistant = ctx.langue;
    p.reseaux = {ecran::faux::point("Maison_5G", -52, true), ecran::faux::point("Freebox-A1B2C3", -67, true),
                 ecran::faux::point("Invite", -80, false)};
    construire(ctx, quoi);
}

// Un redémarrage : on libère l'interface, on efface l'écran, un hôte NEUF reprend la même mémoire, l'interface est refaite.
void redemarrer(const banc::Contexte &ctx, Interface quoi) {
    ecran::ui_reglages_liberer();
    lv_obj_delete(g_appareil->racine);
    g_appareil->racine = nullptr;
    g_appareil->hote = std::make_unique<HoteAppareil>(g_appareil->ports, g_appareil->magasin);
    construire(ctx, quoi);
}

// ---- les parcours ---------------------------------------------------------------------------------------------------------------

// La luminosité passe par la porte de la page web, une seule fois, et l'hôte n'en garde aucune copie.
void affichage_source_unique(const banc::Contexte &ctx) {
    preparer_appareil(ctx, "affichage-source-unique", Interface::Reglages);
    ecran::ui_aller("section:3");
    tourner();
    const int avant = g_appareil->ports.enregistrements;
    glisser_curseur(40);
    exiger(g_appareil->ports.etat_affichage.luminosite == 40, "la luminosité arrive à la source de la page web");
    exiger(g_appareil->ports.enregistrements == avant + 1, "un seul enregistrement pour un geste");
    exiger(g_appareil->magasin.vide_dans("affichage") && g_appareil->magasin.vide_dans("langue"),
           "l'hôte ne garde pas de copie de l'affichage ni de la langue");
}

// Une clé de service est gardée, ne ressort jamais, et l'écran n'en montre rien.
void cle_de_service_secrete(const banc::Contexte &ctx) {
    preparer_appareil(ctx, "cle-de-service-secrete", Interface::Reglages);
    ecran::ui_aller("section:4:1/editeur:llm.cle");
    tourner();
    taper_texte("sk");
    taper("OK");
    MagasinFaux &m = g_appareil->magasin;
    exiger(m.textes.count("services/llm.cle") == 1 && m.textes["services/llm.cle"] == "sk",
           "la clé est gardée dans l'espace des services");
    char relu[64] = {};
    exiger(g_appareil->hote->texte("llm.cle", relu, sizeof relu) == 0 && relu[0] == '\0',
           "l'hôte ne rend jamais la clé");
    exiger(g_appareil->hote->secret_defini("llm.cle"), "l'hôte dit seulement qu'une clé est définie");
    exiger(!affiche("sk"), "l'écran n'affiche pas la clé");
}

// Un Wi-Fi n'est retenu qu'après une connexion réussie : un mot de passe refusé n'écrit rien.
void wifi_retenu_apres_succes(const banc::Contexte &ctx) {
    preparer_appareil(ctx, "wifi-retenu-apres-succes", Interface::Reglages);
    MagasinFaux &m = g_appareil->magasin;
    PortsFaux &p = g_appareil->ports;
    ecran::ui_aller("section:0/connexion:Maison_5G:fautedefrappe");
    tourner();
    exiger(p.connexions == 1 && p.dernier_ssid == "Maison_5G", "l'essai part vers l'appareil");
    p.repondre_wifi(ecran::wifi::Resultat::MotDePasseRefuse);
    tourner();
    exiger(m.vide_dans("wifi"), "un mot de passe refusé n'écrit rien");
    exiger(g_appareil->hote->wifi_connexion().etat == ecran::wifi::Etat::Echec, "l'hôte rapporte l'échec à l'écran");

    ecran::ui_reglages_liberer();
    lv_obj_delete(g_appareil->racine);
    construire(ctx, Interface::Reglages);
    ecran::ui_aller("section:0/connexion:Maison_5G:bonmotdepasse");
    tourner();
    exiger(p.connexions == 2, "deuxième essai");
    p.repondre_wifi(ecran::wifi::Resultat::Connecte);
    tourner();
    exiger(m.textes["wifi/ssid"] == "Maison_5G" && m.textes["wifi/mdp"] == "bonmotdepasse",
           "le réseau est retenu après le succès");
}

// L'installation écrit son étape à chaque « Continuer » ; après un redémarrage, elle reprend là où elle était.
void installation_reprise(const banc::Contexte &ctx) {
    preparer_appareil(ctx, "installation-reprise", Interface::Installation);
    exiger(etape_courante() == 1 && affiche_etape(1), "une carte neuve commence à l'étape 1");
    exiger(toucher_texte(ecran::i18n::nom(ctx.langue).data()), "toucher le nom de la langue");
    exiger(g_appareil->ports.etat_affichage.langue_ecran == ctx.langue &&
               g_appareil->ports.etat_affichage.langue_assistant == ctx.langue,
           "la langue arrive à la source de la page web, écran et assistant");
    exiger(toucher("Continuer") && etape_courante() == 2, "étape 2");
    exiger(toucher("Continuer sans Wi-Fi") && etape_courante() == 3, "étape 3");
    int32_t octet = -1;
    exiger(g_appareil->magasin.lire_nombre("installation", "etape", octet) && octet != 0,
           "l'étape est écrite en mémoire permanente");

    redemarrer(ctx, Interface::Installation);
    exiger(etape_courante() == 3 && affiche_etape(3), "après redémarrage, l'installation reprend à l'étape 3");
    exiger(toucher("Retour") && etape_courante() == 2, "« Retour » revient à l'étape 2 et l'écrit");
}

// « Réinitialiser » efface ce que l'hôte garde (services, Wi-Fi) et remet l'installation au début ; deux gestes.
void reinitialisation_complete(const banc::Contexte &ctx) {
    preparer_appareil(ctx, "reinitialisation-complete", Interface::Reglages);
    MagasinFaux &m = g_appareil->magasin;
    m.textes["wifi/ssid"] = "Maison_5G";
    m.textes["wifi/mdp"] = "bonmotdepasse";
    m.textes["services/llm.cle"] = "sk";
    g_appareil->hote->definir_progression({Etape::Lieu, true, Mode::Premiere});
    ecran::ui_aller("section:5");
    tourner();
    exiger(toucher("Réinitialiser l’appareil") && toucher("Toucher encore pour tout effacer"), "les deux gestes");
    exiger(m.vide_dans("wifi") && m.vide_dans("services"), "le Wi-Fi retenu et les services sont effacés");
    exiger(g_appareil->ports.remises_par_defaut == 1,
           "l'affichage et la ville reviennent aux défauts par la porte de la page web");
    exiger(!g_appareil->hote->progression().terminee && etape_courante() == 1, "l'installation repart de l'étape 1");
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"affichage-source-unique", "Hôte de l'appareil : luminosité, une seule source", affichage_source_unique},
    {"cle-de-service-secrete", "Hôte de l'appareil : clé de service jamais relue", cle_de_service_secrete},
    {"wifi-retenu-apres-succes", "Hôte de l'appareil : Wi-Fi retenu après succès seulement", wifi_retenu_apres_succes},
    {"installation-reprise", "Hôte de l'appareil : reprise de l'installation après redémarrage", installation_reprise},
    {"reinitialisation-complete", "Hôte de l'appareil : réinitialisation complète", reinitialisation_complete},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
