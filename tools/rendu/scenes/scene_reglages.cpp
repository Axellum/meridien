// Scène « réglages » : la liste des six sections et chaque page de réglages (main/ui_reglages.cpp, ui_clavier.cpp, ui_commun.cpp),
// compilées telles quelles, avec un hôte en mémoire vive (main/hote_memoire.cpp) à la place de la mémoire permanente, du Wi-Fi et
// de la météo. L'installation guidée a sa propre scène (scene_installation.cpp) ; les gestes rejoués sont dans scene_parcours.cpp.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_installation.cpp main/ui_reglages.cpp main/ui_reglages_mer.cpp main/ui_clavier.cpp main/ui_commun.cpp main/hote_memoire.cpp
// rendu-langues: toutes
// rendu-langues-defaut: fr de tr
// rendu-tailles: 1280x720 720x1280 1024x600
// rendu-sequence: reglages
//
// Fixtures (inventées, pas des mesures) : appareil déjà en service sur « Maison_5G » (192.168.1.52), ville Lyon, météo « nuageux,
// 12 °C », services de l'assistant réglés à la main (Ollama sur un PC du réseau, clé factice déjà enregistrée), version « banc de
// rendu PC ». Les clés ne s'affichent jamais : la page « clé » montre un champ masqué rempli d'un texte factice.
#include "parcours_outils.h"

namespace {

using namespace parcours;

void en_service(const banc::Contexte &ctx) {
    preparer(ctx, "reglages", Interface::Reglages);
    g_hote.simuler_connecte("Maison_5G", "192.168.1.52");
    g_hote.definir_texte("wifi.mot_de_passe", "bonmotdepasse");
    g_hote.definir_texte("lieu.ville", "Lyon, Auvergne-Rhône-Alpes, France");
    std::snprintf(g_hote.info.version, sizeof g_hote.info.version, "%s", "banc de rendu PC");
}

void assistant_regle() {
    g_hote.definir_nombre("stt.preset", 1); // Ollama (mon PC)
    g_hote.definir_texte("stt.url", "http://192.168.1.20:11434/v1");
    g_hote.definir_texte("stt.modele", "whisper-small");
    g_hote.definir_nombre("llm.preset", 3); // Ollama (mon PC)
    g_hote.definir_texte("llm.url", "http://192.168.1.20:11434/v1");
    g_hote.definir_texte("llm.modele", "mistral-small");
    g_hote.definir_texte("llm.cle", "cle-factice-pour-le-rendu");
    g_hote.definir_nombre("tts.preset", 2); // Kokoro (mon PC)
    g_hote.definir_texte("tts.url", "http://192.168.1.20:8880/v1");
    g_hote.definir_texte("tts.voix", "ff_siwis");
}

void aller(const char *chemin) {
    ecran::ui_aller(chemin);
    tourner();
}

// ---- les variantes, dans l'ordre de la séquence -------------------------------------------------------------------------

void liste(const banc::Contexte &ctx) {
    en_service(ctx);
    assistant_regle();
    aller("liste");
}

void liste_neuve(const banc::Contexte &ctx) {
    preparer(ctx, "liste-neuve", Interface::Reglages);
    aller("liste");
}

void reseau(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:0");
}

void reseau_hors_ligne(const banc::Contexte &ctx) {
    preparer(ctx, "reseau-hors-ligne", Interface::Reglages);
    aller("section:0");
}

void wifi_liste(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:0/wifi");
}

void lieu(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:1");
}

void lieu_vide(const banc::Contexte &ctx) {
    preparer(ctx, "lieu-vide", Interface::Reglages);
    aller("section:1");
}

// La recherche de ville a trouvé des lieux : la liste, et au-dessus le crédit des données de lieux (GeoNames, CC BY 4.0).
void ville_resultats(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.lieux_connus = {"Saint-Malo, Bretagne, France", "Saint-Malo-de-Guersac, Pays de la Loire, France",
                           "Saint-Malo-de-Phily, Bretagne, France", "Saint-Malo-des-Trois-Fontaines, Bretagne, France"};
    aller("section:1/ville");
    ecran::ui_simuler_saisie("Saint-Malo", true);
    tourner();
}

void lieu_alertes(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:1:1");
}

void alertes_adresse_saisie(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:1:1/editeur:alertes.adresse");
}

// La source de la météo (ADR-0010). Fixtures inventées : Lyon (la liste ne propose que « Automatique » et le modèle mondial), puis
// Amsterdam (le service néerlandais s'y ajoute).
void source_liste(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:1/source");
}

void source_liste_pays_bas(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.definir_texte("lieu.ville", "Amsterdam, Hollande-Septentrionale, Pays-Bas");
    g_hote.code_pays = "NL";
    g_hote.definir_nombre("meteo.source", static_cast<int32_t>(ecran::meteo::fournisseurs::indice_de("knmi")));
    aller("section:1/source");
}

void lieu_source_choisie(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.definir_texte("lieu.ville", "Amsterdam, Hollande-Septentrionale, Pays-Bas");
    g_hote.code_pays = "NL";
    g_hote.definir_nombre("meteo.source", static_cast<int32_t>(ecran::meteo::fournisseurs::indice_de("knmi")));
    aller("section:1");
}

void lieu_source_retablie(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.definir_nombre("meteo.source", 0);
    g_hote.source_retablie = true; // la ville vient de changer vers un pays que l'ancienne source ne couvre pas
    aller("section:1");
}

// Les services lus directement (ADR-0014). Fixtures inventées : Oslo (MET Norway, deux fois dans la liste : le modèle d'Open-Meteo et le
// service lu directement) et Denver (NWS). `origine` est le service qui a RÉELLEMENT donné la météo montrée.
void oslo(const char *source, ecran::meteo::fournisseurs::Famille origine) {
    g_hote.definir_texte("lieu.ville", "Oslo, Oslo, Norvège");
    g_hote.code_pays = "NO";
    g_hote.definir_nombre("meteo.source", static_cast<int32_t>(ecran::meteo::fournisseurs::indice_de(source)));
    g_hote.meteo.origine = origine;
}

void source_liste_norvege(const banc::Contexte &ctx) {
    en_service(ctx);
    oslo("metnorway", ecran::meteo::fournisseurs::Famille::MetNo);
    aller("section:1/source");
}

void lieu_source_metno(const banc::Contexte &ctx) {
    en_service(ctx);
    oslo("metnorway", ecran::meteo::fournisseurs::Famille::MetNo);
    aller("section:1");
}

void lieu_source_repli(const banc::Contexte &ctx) {
    en_service(ctx);
    oslo("metnorway",
         ecran::meteo::fournisseurs::Famille::OpenMeteo); // MET Norway n'a pas répondu : la météo vient d'Open-Meteo
    aller("section:1");
}

void lieu_source_nws(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.definir_texte("lieu.ville", "Denver, Colorado, États-Unis");
    g_hote.code_pays = "US";
    g_hote.definir_nombre("meteo.source", static_cast<int32_t>(ecran::meteo::fournisseurs::indice_de("nws")));
    g_hote.meteo.origine = ecran::meteo::fournisseurs::Famille::Nws;
    aller("section:1");
}

void langue(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:2");
}

void affichage(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:3");
}

void assistant_stt(const banc::Contexte &ctx) {
    en_service(ctx);
    assistant_regle();
    aller("section:4:0");
}

void assistant_llm(const banc::Contexte &ctx) {
    en_service(ctx);
    assistant_regle();
    aller("section:4:1");
}

void assistant_tts(const banc::Contexte &ctx) {
    en_service(ctx);
    assistant_regle();
    aller("section:4:2");
}

void assistant_neuf(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:4:1");
}

void cle_saisie(const banc::Contexte &ctx) {
    en_service(ctx);
    assistant_regle();
    aller("section:4:1/editeur:llm.cle");
    ecran::ui_simuler_saisie("cle-factice-pour-le-rendu");
}

void adresse_saisie(const banc::Contexte &ctx) {
    en_service(ctx);
    assistant_regle();
    aller("section:4:1/editeur:llm.url");
}

void appareil(const banc::Contexte &ctx) {
    en_service(ctx);
    aller("section:5");
}

// La mer (ADR-0026). Fixtures INVENTÉES (parcours_outils.h) : seize spots chez « Vigie », trois gardés par l'appareil, le deuxième affiché.
void mer_vide(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    aller("section:1:2");
}

void mer_spots(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    mer_spots_gardes(3, 1);
    aller("section:1:2");
}

void mer_pleine(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    mer_spots_gardes(12, 0); // le registre est plein : « Ajouter un spot » est grisé et dit pourquoi
    aller("section:1:2");
}

void mer_vent_bornes(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    mer_spots_gardes(1, 0);
    g_hote.registre_mer.vent_min_kn = 1; // « − » du minimum grisé
    g_hote.registre_mer.vent_max_kn = 2; // « + » du minimum grisé (il toucherait le maximum)
    aller("section:1:2");
}

void mer_ajouter(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    mer_spots_gardes(3, 1); // les trois premiers sont déjà gardés : grisés, « Déjà ajouté »
    aller("section:1:2/spot");
}

void mer_ajouter_recherche(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    mer_spots_gardes(3, 1);
    aller("section:1:2/spot");
    ecran::ui_simuler_saisie("etoile", true); // sans accent : « Anse de l’Étoile » doit sortir
    tourner();
}

void mer_ajouter_aucun(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    mer_spots_gardes(3, 1);
    aller("section:1:2/spot");
    ecran::ui_simuler_saisie("zzzz", true);
    tourner();
}

void mer_ajouter_chargement(const banc::Contexte &ctx) {
    en_service(ctx);
    mer_liste_fictive();
    g_hote.liste_mer_instantanee = false; // la lecture dure 1,2 s : on voit « Chargement de la liste des spots… »
    aller("section:1:2/spot");
}

void mer_ajouter_sans_connexion(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.issue_liste_mer = ecran::ListeMer::SansConnexion;
    aller("section:1:2/spot");
}

void mer_ajouter_echec(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.issue_liste_mer = ecran::ListeMer::Echec;
    aller("section:1:2/spot");
}

void mer_ajouter_vide(const banc::Contexte &ctx) {
    en_service(ctx);
    g_hote.issue_liste_mer = ecran::ListeMer::Vide;
    aller("section:1:2/spot");
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"liste", "Liste des réglages (appareil en service)", liste},
    {"liste-neuve", "Liste des réglages (appareil neuf : rien de réglé)", liste_neuve},
    {"reseau", "Réseau : connecté", reseau},
    {"reseau-hors-ligne", "Réseau : pas de réseau enregistré", reseau_hors_ligne},
    {"wifi-liste", "Réseau : choix d'un réseau", wifi_liste},
    {"lieu", "Lieu et météo : ville choisie", lieu},
    {"lieu-vide", "Lieu et météo : pas de ville", lieu_vide},
    {"ville-resultats", "Recherche de ville : quatre lieux trouvés, crédit GeoNames", ville_resultats},
    {"lieu-alertes", "Lieu et météo : alertes officielles (Vigie)", lieu_alertes},
    {"alertes-adresse-saisie", "Alertes officielles : saisie de l'adresse de Vigie", alertes_adresse_saisie},
    {"mer-vide", "La mer : aucun spot gardé", mer_vide},
    {"mer-spots", "La mer : trois spots gardés, le deuxième affiché, plage de vent", mer_spots},
    {"mer-pleine", "La mer : douze spots gardés, « Ajouter un spot » grisé", mer_pleine},
    {"mer-vent-bornes", "La mer : plage de vent aux limites, boutons grisés", mer_vent_bornes},
    {"mer-ajouter", "Ajouter un spot : la liste de Vigie, les spots gardés grisés", mer_ajouter},
    {"mer-ajouter-recherche", "Ajouter un spot : « etoile » trouve « Anse de l’Étoile »", mer_ajouter_recherche},
    {"mer-ajouter-aucun", "Ajouter un spot : aucun spot ne correspond", mer_ajouter_aucun},
    {"mer-ajouter-chargement", "Ajouter un spot : lecture de la liste en cours", mer_ajouter_chargement},
    {"mer-ajouter-sans-connexion", "Ajouter un spot : pas de connexion, « Réessayer »", mer_ajouter_sans_connexion},
    {"mer-ajouter-echec", "Ajouter un spot : Vigie n'a pas donné de liste", mer_ajouter_echec},
    {"mer-ajouter-vide", "Ajouter un spot : Vigie ne propose aucun spot", mer_ajouter_vide},
    {"source-liste", "Source météo : liste proposée à Lyon", source_liste},
    {"source-liste-pays-bas", "Source météo : liste proposée à Amsterdam, KNMI choisi", source_liste_pays_bas},
    {"lieu-source-choisie", "Lieu et météo : un modèle national choisi, attribution", lieu_source_choisie},
    {"lieu-source-retablie", "Lieu et météo : source remise sur « Automatique » après un changement de ville",
     lieu_source_retablie},
    {"source-liste-norvege", "Source météo : liste proposée à Oslo, MET Norway (service direct) choisi",
     source_liste_norvege},
    {"lieu-source-metno", "Lieu et météo : MET Norway a répondu, attribution CC BY 4.0 et NLOD 2.0", lieu_source_metno},
    {"lieu-source-repli", "Lieu et météo : MET Norway n'a pas répondu, la météo vient d'Open-Meteo", lieu_source_repli},
    {"lieu-source-nws", "Lieu et météo : NWS a répondu, données ouvertes", lieu_source_nws},
    {"langue", "Langue de l'écran et de l'assistant", langue},
    {"affichage", "Affichage : thème et luminosité", affichage},
    {"assistant-stt", "Assistant : reconnaissance de la voix", assistant_stt},
    {"assistant-llm", "Assistant : modèle de langage", assistant_llm},
    {"assistant-tts", "Assistant : synthèse vocale", assistant_tts},
    {"assistant-neuf", "Assistant : rien de réglé", assistant_neuf},
    {"cle-saisie", "Assistant : saisie d'une clé (masquée)", cle_saisie},
    {"adresse-saisie", "Assistant : saisie d'une adresse", adresse_saisie},
    {"appareil", "Appareil : version, page sur téléphone, réinitialisation", appareil},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
