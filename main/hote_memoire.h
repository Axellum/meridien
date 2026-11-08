// HoteReglages en mémoire vive (ecran/hote_reglages.hpp) : les valeurs partent des défauts du registre, rien n'est gardé après un
// redémarrage, le Wi-Fi, la recherche de lieu et les tests de service sont simulés par des données que l'appelant fournit.
// Sert au rendu PC (scènes de tools/rendu) et à l'appareil tant que l'adaptateur de mémoire permanente n'existe pas.
// Aucun include ESP-IDF ni LVGL.
#pragma once

#include <string>
#include <vector>

#include "ecran/hote_reglages.hpp"
#include "ecran/registre_reglages.hpp"

namespace ecran {

class HoteMemoire final : public HoteReglages {
  public:
    HoteMemoire();

    // ---- données de démonstration, à fournir avant d'afficher
    std::vector<ReseauWifi> reseaux_vus;   // ce que la recherche Wi-Fi trouve
    std::vector<std::string> lieux_connus; // libellés complets ; la recherche garde ceux qui contiennent la saisie
    bool internet = true;                  // faux : la recherche de lieu répond « sans connexion »
    bool recherche_instantanee = true;     // faux : la recherche Wi-Fi dure 0,8 s (on voit « Recherche des réseaux… »)
    ApercuMeteo meteo;                     // aperçu de la zone choisie
    std::string code_pays = "FR";          // pays de la ville montrée : dicte la liste des sources météo proposées
    bool source_retablie = false;          // vrai : l'écran dit que la source est repassée sur « Automatique »
    InfoAppareil info;                     // version, adresse de la page, code d'appairage
    int64_t delai_connexion_ms = 1200;     // temps avant la réponse simulée du Wi-Fi (-1 : jamais, pour voir le délai)
    int64_t delai_test_ms = 1500;
    std::string mot_de_passe_attendu; // « Maison » est protégé par ce mot de passe ; autre : MotDePasse refusé
    std::string ssid_introuvable;     // un réseau qui disparaît : Introuvable
    // Le Wi-Fi est déjà associé à ce réseau (rendu PC : l'écran d'un appareil déjà en service).
    void simuler_connecte(std::string_view ssid, std::string_view ip);
    void fixer_horloge(int64_t ms) { horloge_fixee_ = ms; } // pour des images identiques d'un rendu à l'autre
    void avancer_horloge(int64_t ms) { horloge_fixee_ += ms; }
    int redemarrages = 0; // combien de fois l'écran a demandé un redémarrage / une réinitialisation (essais)
    int reinitialisations = 0;

    // ---- la mer (ADR-0026) : le registre des spots et la liste de Vigie, inventés pour le rendu et les essais
    mer::Registre registre_mer;    // les spots gardés (vide au départ) et la plage de vent
    mer::liste::Liste liste_vigie; // ce que « Ajouter un spot » trouve chez Vigie
    ListeMer issue_liste_mer =
        ListeMer::Terminee;            // comment finit la prochaine lecture : Terminee, Vide, SansConnexion ou Echec
    bool liste_mer_instantanee = true; // faux : la lecture dure 1,2 s (on voit « Lecture de la liste… »)
    int listes_mer_demandees = 0;      // combien de lectures l'écran a demandées (essais)
    int registres_mer_enregistres = 0; // combien de fois l'écran a enregistré le registre (essais)

    // ---- HoteReglages
    int32_t nombre(std::string_view id) override;
    void definir_nombre(std::string_view id, int32_t valeur) override;
    std::size_t texte(std::string_view id, char *sortie, std::size_t capacite) override;
    void definir_texte(std::string_view id, std::string_view valeur) override;
    bool secret_defini(std::string_view id) override;
    void effacer_secret(std::string_view id) override;

    void wifi_chercher() override;
    bool wifi_recherche_en_cours() override;
    std::size_t wifi_reseaux(ReseauWifi *sortie, std::size_t max) override;
    void wifi_connecter(std::string_view ssid, std::string_view mot_de_passe) override;
    void wifi_abandonner() override;
    ConnexionWifi wifi_connexion() override;
    InfoWifi wifi_info() override;

    void lieu_chercher(std::string_view requete) override;
    RechercheLieu lieu_recherche() override;
    std::size_t lieu_resultats(LieuTrouve *sortie, std::size_t max) override;
    void lieu_choisir(std::size_t rang) override;
    ApercuMeteo meteo_apercu() override;
    meteo::fournisseurs::Liste sources_meteo() override;
    bool source_meteo_retablie() override { return source_retablie; }

    bool mer_registre(mer::Registre &sortie) override;
    bool mer_enregistrer(const mer::Registre &nouveau) override;
    void mer_liste_demander() override;
    ListeMer mer_liste_etat() override;
    bool mer_liste_copier(mer::liste::Liste &sortie) override;

    void tester_service(std::string_view service) override;
    ResultatTest resultat_test(std::string_view service) override;
    InfoAppareil info_appareil() override;
    void nouveau_code() override;
    void redemarrer() override;
    void reinitialiser() override;

    installation::Progression progression() override { return progression_; }
    void definir_progression(const installation::Progression &p) override { progression_ = p; }
    int64_t maintenant_ms() override;

  private:
    struct Valeur {
        int32_t nombre = 0;
        std::string texte;
    };
    std::vector<Valeur> valeurs_; // une par réglage du registre, même ordre
    int index(std::string_view id) const;
    void remettre_par_defaut();

    int64_t horloge_fixee_ = -1;
    int64_t recherche_debut_ms_ = -1;
    int64_t connexion_debut_ms_ = -1;
    std::string connexion_ssid_;
    std::string connexion_mdp_;
    ConnexionWifi connexion_;
    InfoWifi wifi_;
    RechercheLieu recherche_lieu_ = RechercheLieu::Inactive;
    std::vector<std::string> trouves_;
    int64_t liste_mer_debut_ms_ = -1;
    ListeMer liste_mer_etat_ = ListeMer::Inactive;
    int64_t test_debut_ms_[3] = {-1, -1, -1};
    EtatTest test_etat_[3] = {EtatTest::Inactif, EtatTest::Inactif, EtatTest::Inactif};
    installation::Progression progression_;
};

} // namespace ecran
