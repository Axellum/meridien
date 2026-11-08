// Interne aux écrans de réglages, d'installation guidée et de clavier (ui_reglages.cpp, ui_installation.cpp, ui_clavier.cpp,
// ui_commun.cpp) : ce que ces fichiers se partagent. Rien ici n'est public ; l'extérieur utilise ui_reglages.h.
// LVGL pur : aucun include ESP-IDF (le rendu PC compile ces fichiers tels quels, tools/rendu).
#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>

#include "lvgl.h"

#include "ecran/ecrans_reglages.hpp"
#include "ecran/hote_reglages.hpp"
#include "ecran/i18n.hpp"
#include "ecran/installation.hpp"
#include "ecran/registre_reglages.hpp"
#include "ecran/texte_modele.hpp"
#include "palette_reglages.h"

namespace ecran::ui {

struct Polices {
    lv_font_t *titre = nullptr;
    lv_font_t *texte = nullptr;
    lv_font_t *petite = nullptr;
    lv_font_t *touche = nullptr;
};

// L'environnement de dessin : une seule interface de réglages existe à la fois.
struct Env {
    HoteReglages *hote = nullptr;
    const PaletteReglages *p = &kReglagesSombre;
    Polices f;
    int32_t largeur = 0; // taille LOGIQUE de l'écran (rotation comprise), jamais écrite en dur
    int32_t hauteur = 0;
    float echelle = 1.0F;                  // 1.0 = 720 px sur le petit côté
    lv_obj_t *racine = nullptr;            // conteneur plein écran qui porte les écrans empilés
    void (*fermer)() = nullptr;            // « Retour » sur le premier écran
    void (*ouvrir_diagnostic)() = nullptr; // ligne « Diagnostic » de la page Appareil ; nul : sans effet
    const uint8_t *ttf = nullptr;
    std::size_t ttf_taille = 0;
};

Env &env();
void env_initialiser(lv_obj_t *racine, const uint8_t *ttf, std::size_t ttf_taille, int32_t largeur, int32_t hauteur,
                     HoteReglages *hote, void (*fermer)(), void (*ouvrir_diagnostic)() = nullptr);
void env_choisir_theme(); // relit « affichage.theme » auprès de l'hôte

// Un nombre de pixels à l'échelle de l'écran (720 px de petit côté = 1.0).
int32_t S(int32_t px);

// Plus petite zone tactile (côté le plus court, à l'échelle de 720 px) : tout bouton, toute rangée-bouton et le curseur doivent la
// respecter, zone d'accroche comprise. Le rendu le vérifie (scene_parcours : « zones-tactiles-… »). Exception connue : les touches
// du clavier à six rangées (fr, nl, es, it) font 59 px à 1280×720, plancher 56 px contrôlé par le même parcours.
constexpr int32_t kToucheMinPx = 72;

// Un conteneur qui défile montre une barre de défilement tant que son contenu dépasse (sinon un bouton coupé net n'est pas lisible
// comme « il y a une suite »). À appeler sur chaque conteneur qui défile.
void style_defilement(lv_obj_t *conteneur);
lv_color_t couleur(uint32_t rgb);
const char *T(const char *francais); // texte traduit dans la langue courante, terminé par un zéro

enum class StyleBouton : uint8_t { Normal, Principal, Danger, Discret };

lv_obj_t *creer_texte(lv_obj_t *parent, const char *texte, const lv_font_t *police, uint32_t couleur_rgb);
lv_obj_t *creer_bouton(lv_obj_t *parent, const char *texte, StyleBouton style, lv_event_cb_t cb, void *donnee);
void bouton_activer(lv_obj_t *bouton, bool actif);
void bouton_changer_texte(lv_obj_t *bouton, const char *texte);
lv_obj_t *creer_colonne(lv_obj_t *parent); // conteneur sans style : colonne, pleine largeur, hauteur du contenu
lv_obj_t *creer_ligne(lv_obj_t *parent);   // conteneur sans style : rangée, pleine largeur, hauteur du contenu

// Texte d'un modèle traduit avec des marqueurs {nom} remplis. `tampon` doit rester vivant tant que LVGL garde le texte (LVGL copie).
const char *remplir(const char *modele_fr, const ecran::texte::Parametre *p, std::size_t n, char *tampon,
                    std::size_t capacite);

// ---- pile d'écrans ----------------------------------------------------------------------------------------------------------

enum class TypeEcran : uint8_t {
    Liste,          // la liste des sections
    Section,        // une section ; a = indice de la section, b = onglet
    EditeurTexte,   // un clavier ; a = ModeSaisie, id = identifiant du réglage
    ListeWifi,      // les réseaux trouvés
    ConnexionWifi,  // essai de connexion avec compte à rebours
    RechercheVille, // saisie + résultats
    ListeSource,    // les sources de la météo proposées pour le pays de la ville (ADR-0010)
    RechercheSpot,  // « Ajouter un spot » : la liste de Vigie, lue par l'appareil, et une recherche dedans (ADR-0026)
    Etape,          // une étape de l'installation guidée ; a = numéro de l'étape
};

enum class ModeSaisie : int {
    Reglage = 0,         // enregistre le réglage `id`
    MotDePasseWifi = 1,  // mot de passe du réseau choisi : lance la connexion
    NomReseauManuel = 2, // nom saisi à la main (réseau caché), suivi du mot de passe
};

struct Entree {
    TypeEcran type = TypeEcran::Liste;
    int a = 0;
    int b = 0;
    char id[48] = {};
    lv_obj_t *obj = nullptr;
    int32_t scroll_y = 0; // position de défilement gardée quand l'écran est redessiné (une valeur a changé)
    bool neuf = true;     // vrai pendant la toute première construction de l'écran (lancer une recherche, par exemple)
};

void nav_pousser(const Entree &e);
void nav_remplacer(const Entree &e);
void nav_retour();
void nav_retour_jusqu_a_une_base(); // dépile les éditeurs jusqu'à la section ou l'étape qui les a ouverts
void nav_reconstruire();            // redessine tous les écrans de la pile (langue ou thème changés)
void nav_reconstruire_haut();       // redessine l'écran du dessus (une valeur affichée a changé)
Entree *nav_haut();
int nav_profondeur();
void nav_vider();

// Un écran standard : fond, barre du haut avec « Retour » et un titre, corps défilant. Rend le corps ; `ecran_sortie` reçoit l'écran.
lv_obj_t *creer_ecran_standard(const char *titre_fr_ou_deja_traduit, bool titre_deja_traduit, lv_obj_t **ecran_sortie,
                               lv_obj_t **barre_sortie = nullptr, lv_event_cb_t action_retour = nullptr);
void bouton_retour_action(lv_event_t *e);                                           // « Retour » : nav_retour()
void ajouter_timer(lv_obj_t *ecran, void (*cb)(lv_timer_t *), uint32_t periode_ms); // supprimé avec l'écran

// ---- rangées de réglages (utilisées aussi par l'installation) ---------------------------------------------------------------
void rangee_pour(lv_obj_t *parent, const registre::Reglage &r, const registre::Rangee &rangee);
void rangee_action_ou_info(lv_obj_t *parent, std::string_view id);
void construire_page(lv_obj_t *corps, const registre::Page &page);

// Briques de page partagées avec ui_reglages_mer.cpp (définies dans ui_reglages.cpp).
lv_obj_t *creer_carte(lv_obj_t *parent); // fond de surface, colonne
// Une rangée-bouton : le titre à gauche, la valeur (facultative) à droite. `cb` nul : la rangée ne répond pas au toucher.
lv_obj_t *creer_rangee_bouton(lv_obj_t *parent, const char *titre, const char *valeur, lv_event_cb_t cb, void *donnee);
bool est_le_dessus(lv_timer_t *t); // le minuteur appartient à l'écran du dessus de la pile
// Fait de `champ` le champ de saisie de l'éditeur affiché (rendu PC et essais : ui_simuler_saisie) ; l'oublie à sa suppression.
void champ_actuel_definir(lv_obj_t *champ);

// ---- la mer (ui_reglages_mer.cpp, ADR-0026) ----------------------------------------------------------------------------------
// Les trois rangées de la page « La mer » : info.mer_spots, action.mer_ajouter, info.mer_vent.
void rangee_mer_spots(lv_obj_t *parent);
void rangee_mer_ajouter(lv_obj_t *parent);
void rangee_mer_vent(lv_obj_t *parent);
lv_obj_t *construire_recherche_spot(Entree &e);

// Un gros objet (le registre des spots : 2,3 Ko ; la liste de Vigie : 16,6 Ko) hors de la pile de LVGL. lv_malloc range tout en PSRAM
// d'abord (main/lvgl_memoire.cpp) : la mémoire interne, trop juste, n'est pas touchée. `ok()` faux : la place a manqué.
template <typename T> class Gros {
  public:
    Gros() {
        void *m = lv_malloc(sizeof(T));
        p_ = m != nullptr ? new (m) T() : nullptr;
    }
    ~Gros() {
        if (p_ != nullptr) {
            p_->~T();
            lv_free(p_);
        }
    }
    Gros(const Gros &) = delete;
    Gros &operator=(const Gros &) = delete;
    bool ok() const { return p_ != nullptr; }
    T &operator*() const { return *p_; }
    T *operator->() const { return p_; }
    T *get() const { return p_; }

  private:
    T *p_;
};

// Résumé d'une valeur pour la rangée d'un réglage (texte de l'option choisie, nombre + unité, texte, « •••••»).
void resume_reglage(const registre::Reglage &r, char *sortie, std::size_t capacite);
const char *libelle_option(const registre::Option &o); // traduit si besoin

// ---- éditeurs ---------------------------------------------------------------------------------------------------------------
lv_obj_t *construire_editeur_texte(Entree &e);
lv_obj_t *construire_liste_wifi(Entree &e);
lv_obj_t *construire_connexion_wifi(Entree &e);
lv_obj_t *construire_recherche_ville(Entree &e);
lv_obj_t *construire_liste_source(Entree &e);
const char *ssid_en_cours();
void definir_ssid_en_cours(std::string_view ssid);
// Lance la connexion au réseau choisi (l'hôte, puis le suivi du délai) et ouvre l'écran de connexion par-dessus l'écran du dessus.
void connexion_demarrer(std::string_view ssid, std::string_view mot_de_passe);
// Le champ de saisie de l'éditeur affiché (pour poser un texte dans le rendu PC), ou nullptr.
lv_obj_t *champ_actuel();
// Corps d'une section (onglets + page), partagé par l'écran de la section et par l'étape « Assistant » de l'installation.
void construire_corps_section(lv_obj_t *corps, registre::Section section, int onglet);

// ---- installation guidée (ui_installation.cpp) -----------------------------------------------------------------------------------
lv_obj_t *construire_etape(Entree &e);
installation::Mode mode_installation();
void definir_mode_installation(installation::Mode m);
bool langue_choisie(); // la personne a touché une langue pendant cette installation (bloque « Continuer » à l'étape 1)
void definir_langue_choisie(bool vrai);
void installation_reinitialiser_etat(); // oublie « Continuer sans Wi-Fi » d'un essai précédent

// ---- clavier (ui_clavier.cpp) ------------------------------------------------------------------------------------------------------
// Crée le clavier de l'écran dans `parent`, lié à `textarea`. `sur_valider` est appelé quand la touche « OK » est touchée.
lv_obj_t *clavier_creer(lv_obj_t *parent, lv_obj_t *textarea, int32_t hauteur_voulue, void (*sur_valider)(void *),
                        void *donnee);
int32_t clavier_hauteur(int32_t hauteur_ecran, bool avec_rangee_accents);
void clavier_valider(); // comme si « OK » était touchée (essais et rendu PC)
void ajouter_minuteur_page(
    lv_obj_t *ecran); // rafraîchit une page de réglages : confirmation qui retombe, test de service terminé

} // namespace ecran::ui
