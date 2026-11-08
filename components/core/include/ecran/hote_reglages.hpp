// Ce que l'interface des réglages et de l'installation guidée demande à l'appareil (ADR-0007). Les écrans (main/ui_reglages.cpp,
// ui_installation.cpp, ui_clavier.cpp) ne touchent ni la mémoire permanente, ni le Wi-Fi, ni le réseau : ils parlent à cette
// interface. Même principe que board.h pour les cartes : l'interface ne sait pas qui répond.
//
//  - HoteMemoire (main/hote_memoire.cpp) répond en mémoire vive, sans ESP-IDF : c'est ce que le rendu PC et les essais utilisent.
//  - HoteAppareil (ecran/hote_appareil.hpp) répond sur l'appareil : mêmes réglages que l'API web (affichage, ville), mémoire
//    permanente pour le reste, Wi-Fi de main/reseau.*, recherche de lieu. Sa logique est pure et testée sur le PC ; seuls les
//    « ports » (main/hote_appareil_esp.cpp) touchent ESP-IDF.
//
// Aucune méthode ne rend ni ne reçoit un secret en clair, sauf `definir_texte` pour l'enregistrer (la valeur vient du clavier) :
// l'écran ne relit jamais une clé ou un mot de passe (docs/api-reglages.md, « écriture seule »).
// C++17 standard, aucun include ESP-IDF ni LVGL.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/changement_wifi.hpp"
#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/installation.hpp"
#include "ecran/mer_liste.hpp"
#include "ecran/mer_spots.hpp"

namespace ecran {

// ---- Wi-Fi ----------------------------------------------------------------------------------------------------------------

struct ReseauWifi {
    char ssid[33];
    int8_t niveau; // 0 à 4 (barres affichées)
    bool securise;
};
inline constexpr std::size_t kReseauxMax = 16;

// La connexion en cours ou la dernière. `etat` suit ecran::wifi : Inactif (aucun essai), Connexion, Reussi, Echec (+ cause).
struct ConnexionWifi {
    wifi::Etat etat = wifi::Etat::Inactif;
    wifi::Cause cause = wifi::Cause::Aucune;
};

struct InfoWifi {
    bool connecte = false;
    char ssid[33] = {};
    char ip[16] = {};
};

// ---- lieu et météo --------------------------------------------------------------------------------------------------------

struct LieuTrouve {
    char libelle[96]; // « Lyon, Auvergne-Rhône-Alpes, France »
};
inline constexpr std::size_t kLieuxMax = 8;

enum class RechercheLieu : uint8_t { Inactive, EnCours, Terminee, SansConnexion };

struct ApercuMeteo {
    bool disponible = false;
    float temperature_c = 0;
    char libelle[48] = {}; // libellé court dans la langue de l'écran
    // Le service qui a RÉELLEMENT donné cette météo : à nommer sous la météo (licence). Peut différer de la source choisie quand un
    // service lu directement n'a pas répondu et que la météo vient d'Open-Meteo en attendant (ADR-0014).
    meteo::fournisseurs::Famille origine = meteo::fournisseurs::Famille::OpenMeteo;
};

// ---- la mer ----------------------------------------------------------------------------------------------------------------

// La lecture de la liste des spots de Vigie par l'appareil (ADR-0026). `Terminee` : la liste est prête (`mer_liste_copier`) ; `Vide` :
// Vigie a répondu mais sans aucun spot utilisable ; `SansConnexion` : pas de réseau, ou Vigie injoignable ; `Echec` : Vigie a répondu
// autre chose qu'une liste (refus, réponse trop grande ou illisible).
enum class ListeMer : uint8_t { Inactive, EnCours, Terminee, Vide, SansConnexion, Echec };

// ---- services et appareil -------------------------------------------------------------------------------------------------

enum class EtatTest : uint8_t { Inactif, EnCours, Reussi, Echec };

struct ResultatTest {
    EtatTest etat = EtatTest::Inactif;
    char message[96] = {}; // phrase de la cause ou « Le service répond en 0,4 s. » (déjà dans la langue de l'écran)
};

struct InfoAppareil {
    char version[32] = {};
    char adresse[48] = {}; // adresse de la page de réglages (vide si le Wi-Fi n'est pas connecté)
    char code[8] = {};     // code d'appairage à 6 chiffres
};

// Les trois services de l'assistant : le même identifiant que dans le registre (« stt », « llm », « tts »).
inline constexpr std::string_view kServices[3] = {"stt", "llm", "tts"};

class HoteReglages {
  public:
    virtual ~HoteReglages() = default;

    // ---- réglages du registre, par identifiant (« affichage.luminosite »). Choix : l'indice de l'option ; Nombre : la valeur.
    virtual int32_t nombre(std::string_view id) = 0;
    virtual void definir_nombre(std::string_view id, int32_t valeur) = 0; // applique tout de suite et garde
    // Texte lisible d'un réglage Texte ou Lieu ; jamais un secret. Rend le nombre d'octets écrits (zéro final exclu).
    virtual std::size_t texte(std::string_view id, char *sortie, std::size_t capacite) = 0;
    virtual void definir_texte(std::string_view id, std::string_view valeur) = 0; // texte déjà validé par le registre
    virtual bool secret_defini(std::string_view id) = 0;
    virtual void effacer_secret(std::string_view id) = 0;

    // ---- Wi-Fi (le retour arrière sur l'ancien réseau est fait par main/reseau.*, pas par l'écran)
    virtual void wifi_chercher() = 0;
    virtual bool wifi_recherche_en_cours() = 0;
    virtual std::size_t wifi_reseaux(ReseauWifi *sortie, std::size_t max) = 0;
    virtual void wifi_connecter(std::string_view ssid, std::string_view mot_de_passe) = 0; // asynchrone
    virtual void wifi_abandonner() = 0; // plus de réponse dans le délai : reprendre l'ancien réseau
    virtual ConnexionWifi wifi_connexion() = 0;
    virtual InfoWifi wifi_info() = 0;

    // ---- lieu
    virtual void lieu_chercher(std::string_view requete) = 0;
    virtual RechercheLieu lieu_recherche() = 0;
    virtual std::size_t lieu_resultats(LieuTrouve *sortie, std::size_t max) = 0;
    virtual void lieu_choisir(std::size_t rang) = 0;
    virtual ApercuMeteo meteo_apercu() = 0;

    // ---- source de la météo (réglage « meteo.source » : `nombre` rend la position de la source dans la table des sources, `definir_nombre`
    // la change). La liste PROPOSÉE dépend du pays de la ville : « Automatique » en tête, puis les sources de ce pays.
    virtual meteo::fournisseurs::Liste sources_meteo() = 0;
    // Vrai quand le dernier changement de ville a remis « Automatique » parce que l'ancienne source ne couvre pas le nouveau pays :
    // l'écran le dit en une phrase (jamais de retour muet). Retombe à faux dès que la source est choisie ou que la ville change encore.
    virtual bool source_meteo_retablie() = 0;

    // ---- la mer (ADR-0026) : le registre des spots de l'écran « Mer », le même que celui de la page web (ADR-0019, ADR-0025)
    // Copie le registre courant dans `sortie` (≈ 2,3 Ko : à la charge de l'appelant). Faux, `sortie` inchangée, s'il n'est pas lisible.
    virtual bool mer_registre(mer::Registre &sortie) = 0;
    // Valide, garde puis réveille le client réseau. Faux, rien de changé, si le registre est refusé (mer::registre_valide) ou si
    // l'écriture échoue.
    virtual bool mer_enregistrer(const mer::Registre &nouveau) = 0;
    // Lance la lecture de la liste de Vigie (asynchrone, hors de la tâche de l'interface). Sans effet si une lecture est en cours.
    virtual void mer_liste_demander() = 0;
    virtual ListeMer mer_liste_etat() = 0;
    // Copie la liste lue dans `sortie` (≈ 16,6 Ko : à la charge de l'appelant, en PSRAM). Faux tant que l'état n'est pas `Terminee`.
    virtual bool mer_liste_copier(mer::liste::Liste &sortie) = 0;

    // ---- appareil
    virtual void tester_service(std::string_view service) = 0; // « stt », « llm » ou « tts »
    virtual ResultatTest resultat_test(std::string_view service) = 0;
    virtual InfoAppareil info_appareil() = 0;
    virtual void nouveau_code() = 0;
    virtual void redemarrer() = 0;
    virtual void reinitialiser() = 0; // toutes les valeurs reviennent au défaut du registre ; la progression aussi

    // ---- installation guidée : la progression tient en UN octet de mémoire permanente (installation.hpp)
    virtual installation::Progression progression() = 0;
    virtual void definir_progression(const installation::Progression &p) = 0;

    // Horloge monotone en millisecondes (délais de connexion et de confirmation).
    virtual int64_t maintenant_ms() = 0;
};

} // namespace ecran
