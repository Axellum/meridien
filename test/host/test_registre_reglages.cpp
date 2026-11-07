// Tests du noyau pur : registre des réglages, définition des écrans, préréglages des services (ecran/registre_reglages.hpp,
// ecrans_reglages.hpp, presets_services.hpp). Chaque test est évalué à la compilation (static_assert) ET à l'exécution (mini.hpp).
//
// Ce que ces tests prouvent : chaque réglage du registre a un chemin dans l'API (CONTRAT_API, copie du contrat de
// docs/api-reglages.md que test/host/controle_reglages.py compare au document) ET une rangée dans une page d'écran avec un éditeur
// qui convient à son type ; chaque champ réglable de l'API a un réglage du registre ; les libellés sont des clés de la table de
// traduction ; les bornes sont cohérentes. Ce qu'ils NE prouvent PAS : que l'écran dessine bien chaque éditeur (le contrôle
// croisé controle_reglages.py cherche un `case Editeur::…` par éditeur dans main/ui_*.cpp, c'est tout), ni que le serveur de
// l'appareil implémente la route (le contrat est écrit, le serveur est une autre session).
#include <cstdio>
#include <initializer_list>

#include "ecran/ecrans_reglages.hpp"
#include "ecran/i18n.hpp"
#include "ecran/presets_services.hpp"
#include "ecran/registre_reglages.hpp"
#include "ecran/reglages_alertes.hpp"
#include "mini.hpp"

using namespace ecran::registre;
using ecran::i18n::est_une_cle;
namespace vigie = ecran::vigie;
namespace regl = ecran::reglages;

namespace {

// ---- le contrat de l'API, recopié de docs/api-reglages.md (§4 à §7) --------------------------------------------------------
// Une ligne = un champ du corps JSON qu'un client PEUT envoyer à l'appareil pour régler quelque chose. `genre` dit comment le
// registre le couvre : un réglage de même route et de même champ (Direct), la ville dont les champs forment UN seul réglage de
// type Lieu (Compose), l'effacement d'une clé qu'on fait depuis l'éditeur du réglage Secret (Effacement).
enum class Couverture { Direct, Compose, Effacement };

struct ChampApi {
    std::string_view route;
    std::string_view champ;
    Couverture couverture;
};

constexpr ChampApi CONTRAT_API[] = {
    {"/api/wifi", "ssid", Couverture::Direct},
    {"/api/wifi", "mot_de_passe", Couverture::Direct},
    {"/api/location", "nom", Couverture::Direct},
    {"/api/location", "source_meteo", Couverture::Direct},
    {"/api/location", "region", Couverture::Compose},
    {"/api/location", "pays", Couverture::Compose},
    {"/api/location", "code_pays", Couverture::Compose},
    {"/api/location", "latitude", Couverture::Compose},
    {"/api/location", "longitude", Couverture::Compose},
    {"/api/location", "fuseau", Couverture::Compose},
    {"/api/location", "altitude", Couverture::Compose}, // facultatif dans le document
    {"/api/display", "langue_ecran", Couverture::Direct},
    {"/api/display", "langue_assistant", Couverture::Direct},
    {"/api/display", "theme", Couverture::Direct},
    {"/api/display", "luminosite", Couverture::Direct},
    {"/api/alerts", "actives", Couverture::Direct},
    {"/api/alerts", "adresse", Couverture::Direct},
    {"/api/alerts", "periode_min", Couverture::Direct},
    {"/api/alerts", "niveau_min", Couverture::Direct},
    {"/api/services/{id}", "preset", Couverture::Direct},
    {"/api/services/{id}", "url", Couverture::Direct},
    {"/api/services/{id}", "modele", Couverture::Direct},
    {"/api/services/{id}", "voix", Couverture::Direct},
    {"/api/services/{id}", "cle", Couverture::Direct},
    {"/api/services/{id}", "effacer_cle", Couverture::Effacement},
};

// Rangées d'écran sans réglage (actions et informations) que l'écran sait dessiner : copie de la liste de ui_reglages.cpp
// (controle_reglages.py compare les deux).
constexpr std::string_view ACTIONS_ET_INFOS[] = {
    "info.wifi",         "info.meteo",           "info.telephone",    "info.version",
    "action.tester_stt", "action.tester_llm",    "action.tester_tts", "action.refaire_installation",
    "action.redemarrer", "action.reinitialiser",
};

constexpr bool est_une_action_connue(std::string_view id) {
    for (std::string_view a : ACTIONS_ET_INFOS) {
        if (a == id)
            return true;
    }
    return false;
}

constexpr bool lettre_minuscule_ou_chiffre(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

// « groupe.nom » : minuscules, chiffres et soulignés, un seul point, aucune partie vide.
constexpr bool identifiant_bien_forme(std::string_view id) {
    std::size_t points = 0;
    std::size_t longueur_partie = 0;
    for (char c : id) {
        if (c == '.') {
            ++points;
            if (longueur_partie == 0)
                return false;
            longueur_partie = 0;
        } else if (lettre_minuscule_ou_chiffre(c)) {
            ++longueur_partie;
        } else {
            return false;
        }
    }
    return points == 1 && longueur_partie > 0;
}

// ---- le registre --------------------------------------------------------------------------------------------------------

constexpr int identifiants_uniques_et_bien_formes() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        VERIFIER(identifiant_bien_forme(REGISTRE[i].id));
        for (std::size_t j = i + 1; j < NOMBRE_REGLAGES; ++j)
            VERIFIER(REGISTRE[i].id != REGISTRE[j].id);
    }
    VERIFIER(NOMBRE_REGLAGES == 25);
    return 0;
}

constexpr int la_recherche_par_identifiant_retrouve_chaque_reglage() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i)
        VERIFIER(chercher(REGISTRE[i].id) == &REGISTRE[i]);
    VERIFIER(chercher("affichage.inconnu") == nullptr);
    VERIFIER(chercher("") == nullptr);
    VERIFIER(premier_du_groupe("stt") == chercher("stt.preset"));
    VERIFIER(premier_du_groupe("sttt") == nullptr);
    VERIFIER(premier_du_groupe("wifi")->id == "wifi.ssid");
    return 0;
}

constexpr int chaque_choix_a_des_options_coherentes() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        const Reglage &r = REGISTRE[i];
        if (r.type != Type::Choix) {
            VERIFIER(r.nb_options == 0 && r.options == nullptr);
            continue;
        }
        VERIFIER(r.nb_options >= 2 && r.options != nullptr);
        VERIFIER(r.max == static_cast<std::int32_t>(r.nb_options) - 1);
        VERIFIER(r.min == 0);
        VERIFIER(r.defaut >= -1 && r.defaut < static_cast<std::int32_t>(r.nb_options));
        VERIFIER(r.defaut != -1 || r.facultatif); // « pas encore choisi » seulement pour un choix facultatif
        for (std::size_t a = 0; a < r.nb_options; ++a) {
            VERIFIER(!r.options[a].code.empty() && !r.options[a].libelle.empty());
            for (std::size_t b = a + 1; b < r.nb_options; ++b)
                VERIFIER(r.options[a].code != r.options[b].code);
            if (r.options[a].traduire)
                VERIFIER(est_une_cle(r.options[a].libelle));
        }
    }
    return 0;
}

constexpr int les_sept_langues_du_registre_sont_celles_du_noyau() {
    for (const char *id : {"langue.ecran", "langue.assistant"}) {
        const Reglage *r = chercher(id);
        VERIFIER(r != nullptr && r->nb_options == ecran::i18n::NOMBRE_LANGUES);
        for (std::size_t i = 0; i < ecran::i18n::NOMBRE_LANGUES; ++i) {
            VERIFIER(r->options[i].code == ecran::i18n::CODES[i]);
            VERIFIER(r->options[i].libelle == ecran::i18n::NOMS[i]); // chaque langue dans sa propre langue
            VERIFIER(!r->options[i].traduire);
        }
        VERIFIER(r->defaut == 0); // le français
    }
    return 0;
}

constexpr int les_themes_sont_ceux_de_l_api() {
    const Reglage *r = chercher("affichage.theme");
    VERIFIER(r != nullptr && r->nb_options == 3);
    VERIFIER(r->options[0].code == "auto" && r->options[1].code == "clair" && r->options[2].code == "sombre");
    VERIFIER(r->defaut == 0);
    return 0;
}

constexpr int chaque_nombre_a_une_plage_et_un_defaut_sur() {
    int nombres = 0;
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        const Reglage &r = REGISTRE[i];
        if (r.type != Type::Nombre)
            continue;
        ++nombres;
        VERIFIER(r.min < r.max);
        VERIFIER(r.defaut >= r.min && r.defaut <= r.max);
        VERIFIER(!r.unite.empty());
    }
    VERIFIER(nombres == 2); // la luminosité et la période des alertes
    const Reglage *lum = chercher("affichage.luminosite");
    VERIFIER(lum->min == 5 && lum->max == 100); // docs/api-reglages.md §6
    VERIFIER(lum->defaut == LUMINOSITE_DEFAUT && LUMINOSITE_DEFAUT == 75);
    return 0;
}

constexpr int la_luminosite_ne_peut_jamais_eteindre_l_ecran() {
    const Reglage &lum = *chercher("affichage.luminosite");
    VERIFIER(!nombre_valide(lum, 0) && !nombre_valide(lum, 4) && !nombre_valide(lum, -1));
    VERIFIER(nombre_valide(lum, 5) && nombre_valide(lum, 100));
    VERIFIER(!nombre_valide(lum, 101) && !nombre_valide(lum, 100000));
    // un stockage abîmé : retombe sur le défaut, jamais sur 0
    VERIFIER(nombre_depuis_nvs(lum, 0) == LUMINOSITE_DEFAUT);
    VERIFIER(nombre_depuis_nvs(lum, -1) == LUMINOSITE_DEFAUT);
    VERIFIER(nombre_depuis_nvs(lum, 255) == LUMINOSITE_DEFAUT);
    VERIFIER(nombre_depuis_nvs(lum, 60) == 60);
    VERIFIER(!nombre_valide(*chercher("langue.ecran"), 3)); // un choix n'est pas un nombre
    return 0;
}

constexpr int un_choix_lu_en_nvs_hors_liste_donne_le_defaut() {
    const Reglage &langue = *chercher("langue.ecran");
    VERIFIER(choix_depuis_nvs(langue, 2) == 2);
    VERIFIER(choix_depuis_nvs(langue, 6) == 6);
    VERIFIER(choix_depuis_nvs(langue, 7) == 0);
    VERIFIER(choix_depuis_nvs(langue, -1) == 0);
    const Reglage &theme = *chercher("affichage.theme");
    VERIFIER(choix_depuis_nvs(theme, 3) == 0);
    const Reglage &stt = *chercher("stt.preset");
    VERIFIER(choix_depuis_nvs(stt, 9) == -1);                               // « pas encore réglé »
    VERIFIER(choix_depuis_nvs(*chercher("affichage.luminosite"), 1) == 75); // pas un choix : défaut
    VERIFIER(indice_option(langue, "tr") == 6 && indice_option(langue, "xx") == -1 &&
             indice_option(theme, "sombre") == 2);
    return 0;
}

constexpr int les_textes_ont_des_bornes_et_aucun_defaut_secret() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        const Reglage &r = REGISTRE[i];
        if (r.type != Type::Texte && r.type != Type::Secret)
            continue;
        VERIFIER(r.max > 0 && r.min >= 0 && r.min <= r.max);
        VERIFIER(r.defaut_texte.empty()); // aucune valeur par défaut écrite dans le code, surtout pas un secret
        VERIFIER(r.options == nullptr);
    }
    return 0;
}

constexpr int les_bornes_du_wifi_sont_celles_du_standard() {
    const Reglage &ssid = *chercher("wifi.ssid");
    VERIFIER(ssid.max == 32 && ssid.longueur == Longueur::Octets && !ssid.facultatif);
    const Reglage &mdp = *chercher("wifi.mot_de_passe");
    VERIFIER(mdp.type == Type::Secret && mdp.min == 8 && mdp.max == 63 && mdp.longueur == Longueur::Octets &&
             mdp.facultatif);
    // validation d'un nom de réseau : 1 à 32 OCTETS (« é » en pèse 2)
    VERIFIER(valider_texte(ssid, "") == ErreurTexte::Vide);
    VERIFIER(valider_texte(ssid, "Maison") == ErreurTexte::Aucune);
    VERIFIER(valider_texte(ssid, "12345678901234567890123456789012") == ErreurTexte::Aucune);    // 32
    VERIFIER(valider_texte(ssid, "123456789012345678901234567890123") == ErreurTexte::TropLong); // 33
    VERIFIER(valider_texte(ssid, "éééééééééééééééé") == ErreurTexte::Aucune);                    // 16 × 2 = 32 octets
    VERIFIER(valider_texte(ssid, "ééééééééééééééééé") == ErreurTexte::TropLong);                 // 17 × 2 = 34 octets
    // mot de passe : vide (réseau ouvert) ou 8 à 63 octets
    VERIFIER(valider_texte(mdp, "") == ErreurTexte::Aucune);
    VERIFIER(valider_texte(mdp, "1234567") == ErreurTexte::TropCourt);
    VERIFIER(valider_texte(mdp, "12345678") == ErreurTexte::Aucune);
    VERIFIER(valider_texte(mdp, "1234567890123456789012345678901234567890123456789012345678901234") ==
             ErreurTexte::TropLong); // 64
    VERIFIER(valider_texte(mdp, "motdepasse\n") == ErreurTexte::CaractereInterdit);
    VERIFIER(valider_texte(*chercher("affichage.luminosite"), "5") == ErreurTexte::CaractereInterdit); // pas un texte
    return 0;
}

constexpr int les_bornes_des_services_suivent_le_contrat() {
    const Reglage &url = *chercher("llm.url");
    VERIFIER(url.max == 200 && url.longueur == Longueur::Caracteres); // §7 : 200 caractères
    VERIFIER(chercher("stt.modele")->max == 80 && chercher("tts.voix")->max == 80);
    // en caractères et non en octets : 80 « é » (160 octets) passent
    const Reglage &modele = *chercher("llm.modele");
    VERIFIER(
        valider_texte(modele, "éééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééé") ==
        ErreurTexte::Aucune);
    VERIFIER(valider_texte(modele, "") == ErreurTexte::Aucune); // facultatif
    VERIFIER(chercher("stt.cle")->type == Type::Secret && chercher("llm.cle")->type == Type::Secret &&
             chercher("tts.cle")->type == Type::Secret);
    VERIFIER(chercher("stt.voix") == nullptr && chercher("llm.voix") == nullptr); // la voix n'existe que pour TTS
    return 0;
}

constexpr int tous_les_secrets_sont_des_secrets() {
    // Les seuls réglages dont la valeur ne doit JAMAIS s'afficher en clair par défaut ni sortir de l'appareil (api-reglages.md §1).
    int secrets = 0;
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        const Reglage &r = REGISTRE[i];
        const bool nom_de_secret =
            r.id == "wifi.mot_de_passe" || r.id == "stt.cle" || r.id == "llm.cle" || r.id == "tts.cle";
        VERIFIER((r.type == Type::Secret) == nom_de_secret);
        secrets += r.type == Type::Secret ? 1 : 0;
    }
    VERIFIER(secrets == 4);
    return 0;
}

constexpr int les_libelles_et_les_aides_sont_des_cles_de_traduction() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        VERIFIER(!REGISTRE[i].libelle.empty());
        VERIFIER(est_une_cle(REGISTRE[i].libelle));
        VERIFIER(REGISTRE[i].aide.empty() || est_une_cle(REGISTRE[i].aide));
    }
    return 0;
}

constexpr int les_sections_et_les_etapes_sont_coherentes() {
    // Réseau 2, Lieu et météo 6 (la ville, sa source météo et les 4 alertes), Langue 2, Affichage 2, Assistant 13,
    // Appareil 0 : l'appareil n'a que des actions et des informations
    VERIFIER(compter_section(Section::Reseau) == 2 && compter_section(Section::LieuMeteo) == 6);
    VERIFIER(compter_section(Section::Langue) == 2 && compter_section(Section::Affichage) == 2);
    VERIFIER(compter_section(Section::AssistantVocal) == 13 && compter_section(Section::Appareil) == 0);
    // L'installation : langue, Wi-Fi, lieu, affichage, assistant ont des réglages ; téléphone et récapitulatif n'en ont pas.
    VERIFIER(compter_etape(Etape::Langue) == 1 && compter_etape(Etape::Wifi) == 2 && compter_etape(Etape::Lieu) == 1);
    VERIFIER(compter_etape(Etape::Affichage) == 2 && compter_etape(Etape::Assistant) == 14);
    // Ni la source météo (« Automatique » convient à tout le monde, on la change plus tard) ni les quatre alertes
    // (leurs défauts conviennent) ne sont demandées à l'installation.
    VERIFIER(compter_etape(Etape::Telephone) == 0 && compter_etape(Etape::Recapitulatif) == 0 &&
             compter_etape(Etape::Aucune) == 5);
    VERIFIER(chercher("meteo.source")->etape == Etape::Aucune);
    VERIFIER(NOMBRE_ETAPES == 7);
    return 0;
}

// ---- le chemin dans l'API -----------------------------------------------------------------------------------------------

constexpr int chaque_reglage_a_un_chemin_dans_l_api() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        bool trouve = false;
        for (const ChampApi &c : CONTRAT_API) {
            if (c.route == REGISTRE[i].route && c.champ == REGISTRE[i].champ && c.couverture == Couverture::Direct)
                trouve = true;
        }
        VERIFIER(trouve);
    }
    return 0;
}

constexpr int chaque_champ_reglable_de_l_api_a_un_reglage() {
    for (const ChampApi &c : CONTRAT_API) {
        bool couvert = false;
        for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
            const Reglage &r = REGISTRE[i];
            switch (c.couverture) {
            case Couverture::Direct:
                couvert = couvert || (r.route == c.route && r.champ == c.champ);
                break;
            case Couverture::Compose:
                couvert = couvert || (r.route == c.route && r.type == Type::Lieu);
                break;
            case Couverture::Effacement:
                couvert = couvert || (r.route == c.route && r.type == Type::Secret);
                break;
            }
        }
        VERIFIER(couvert);
    }
    return 0;
}

constexpr int deux_reglages_ne_partagent_pas_le_meme_champ_de_la_meme_route_sauf_les_services() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        for (std::size_t j = i + 1; j < NOMBRE_REGLAGES; ++j) {
            const bool meme_chemin = REGISTRE[i].route == REGISTRE[j].route && REGISTRE[i].champ == REGISTRE[j].champ;
            if (!meme_chemin)
                continue;
            VERIFIER(REGISTRE[i].route == "/api/services/{id}"); // les trois services partagent la forme
            VERIFIER(REGISTRE[i].id.substr(0, 3) != REGISTRE[j].id.substr(0, 3));
        }
    }
    return 0;
}

// ---- les écrans ---------------------------------------------------------------------------------------------------------

constexpr int chaque_reglage_a_exactement_un_ecran() {
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i)
        VERIFIER(ecrans_du_reglage(REGISTRE[i].id) == 1);
    VERIFIER(ecrans_du_reglage("inconnu.inconnu") == 0);
    return 0;
}

constexpr int chaque_rangee_reglage_designe_un_reglage_du_registre_et_un_editeur_qui_convient() {
    for (std::size_t p = 0; p < NOMBRE_PAGES; ++p) {
        for (std::size_t k = 0; k < PAGES[p].nb; ++k) {
            const Rangee &rg = PAGES[p].rangees[k];
            if (rg.genre == Genre::Reglage) {
                const Reglage *r = chercher(rg.ref);
                VERIFIER(r != nullptr);
                VERIFIER(editeur_convient(r->type, rg.editeur));
                VERIFIER(r->section == PAGES[p].section); // la page est dans la section du réglage
            } else {
                VERIFIER(rg.editeur == Editeur::Aucun);
                VERIFIER(est_une_action_connue(rg.ref));
                VERIFIER(rg.ref.substr(0, rg.genre == Genre::Action ? 7 : 5) ==
                         (rg.genre == Genre::Action ? "action." : "info."));
            }
        }
    }
    return 0;
}

constexpr int les_actions_et_les_informations_sont_toutes_sur_une_page() {
    for (std::string_view a : ACTIONS_ET_INFOS) {
        int vues = 0;
        for (std::size_t p = 0; p < NOMBRE_PAGES; ++p) {
            for (std::size_t k = 0; k < PAGES[p].nb; ++k)
                vues += PAGES[p].rangees[k].ref == a ? 1 : 0;
        }
        VERIFIER(vues == 1);
    }
    return 0;
}

constexpr int chaque_section_a_une_page_et_les_pages_suivent_l_ordre_des_sections() {
    for (std::size_t s = 0; s < NOMBRE_SECTIONS; ++s)
        VERIFIER(pages_de_section(static_cast<Section>(s)) >= 1);
    VERIFIER(pages_de_section(Section::AssistantVocal) == 3); // trois onglets : comprendre, réfléchir, parler
    VERIFIER(page_de_section(Section::AssistantVocal, 3) == NOMBRE_PAGES);
    // la n-ième page d'une section est la bonne, dans l'ordre des onglets
    VERIFIER(PAGES[page_de_section(Section::AssistantVocal, 0)].id == "assistant_stt");
    VERIFIER(PAGES[page_de_section(Section::AssistantVocal, 1)].id == "assistant_llm");
    VERIFIER(PAGES[page_de_section(Section::AssistantVocal, 2)].id == "assistant_tts");
    VERIFIER(PAGES[page_de_section(Section::Reseau, 0)].id == "reseau" &&
             page_de_section(Section::Reseau, 1) == NOMBRE_PAGES);
    VERIFIER(PAGES[page_de_section(Section::Appareil, 0)].id == "appareil");
    VERIFIER(pages_de_section(Section::LieuMeteo) == 2); // la ville, puis les alertes
    VERIFIER(PAGES[page_de_section(Section::LieuMeteo, 0)].id == "lieu" &&
             PAGES[page_de_section(Section::LieuMeteo, 1)].id == "alertes");
    for (std::size_t p = 1; p < NOMBRE_PAGES; ++p)
        VERIFIER(static_cast<int>(PAGES[p - 1].section) <= static_cast<int>(PAGES[p].section));
    for (std::size_t p = 0; p < NOMBRE_PAGES; ++p) {
        VERIFIER(est_une_cle(PAGES[p].titre));
        for (std::size_t q = p + 1; q < NOMBRE_PAGES; ++q)
            VERIFIER(PAGES[p].id != PAGES[q].id);
    }
    return 0;
}

constexpr int les_titres_de_section_sont_des_cles_de_traduction() {
    for (std::size_t s = 0; s < NOMBRE_SECTIONS; ++s) {
        VERIFIER(est_une_cle(TITRES_SECTIONS[s]));
        VERIFIER(titre_section(static_cast<Section>(s)) == TITRES_SECTIONS[s]);
    }
    // les titres, écrits à la main dans l'ordre de l'enum Section (un titre au mauvais rang afficherait « Langue » sur le Wi-Fi)
    VERIFIER(titre_section(Section::Reseau) == "Réseau" && titre_section(Section::LieuMeteo) == "Lieu et météo");
    VERIFIER(titre_section(Section::Langue) == "Langue" && titre_section(Section::Affichage) == "Affichage");
    VERIFIER(titre_section(Section::AssistantVocal) == "Assistant vocal" &&
             titre_section(Section::Appareil) == "Appareil");
    return 0;
}

constexpr int une_page_ne_montre_pas_plus_de_sept_rangees_visibles() {
    // Au-delà, il faut défiler sur une dalle de 5 pouces : à éviter. Les services (6 rangées avec le bouton « Tester ») sont le maximum.
    for (std::size_t p = 0; p < NOMBRE_PAGES; ++p) {
        std::size_t visibles = 0;
        for (std::size_t k = 0; k < PAGES[p].nb; ++k)
            visibles += PAGES[p].rangees[k].dans_la_liste ? 1 : 0;
        VERIFIER(visibles >= 1 && visibles <= 7);
    }
    return 0;
}

constexpr int le_mot_de_passe_wifi_n_est_atteint_qu_apres_le_choix_du_reseau() {
    const Rangee *ssid = rangee_du_reglage("wifi.ssid");
    const Rangee *mdp = rangee_du_reglage("wifi.mot_de_passe");
    VERIFIER(ssid != nullptr && mdp != nullptr);
    VERIFIER(ssid->editeur == Editeur::ListeWifi && ssid->dans_la_liste);
    VERIFIER(mdp->editeur == Editeur::ClavierSecret && !mdp->dans_la_liste);
    VERIFIER(ssid < mdp); // le nom avant le mot de passe, dans la même page
    // tout autre réglage est directement atteignable depuis sa page
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        if (REGISTRE[i].id != "wifi.mot_de_passe")
            VERIFIER(rangee_du_reglage(REGISTRE[i].id)->dans_la_liste);
    }
    return 0;
}

constexpr int choix_et_nombres_se_reglent_sur_la_page_les_autres_ouvrent_un_editeur() {
    // « Jamais plus de deux niveaux » : un choix ou un nombre n'ouvre pas d'éditeur plein écran.
    for (std::size_t i = 0; i < NOMBRE_REGLAGES; ++i) {
        const Reglage &r = REGISTRE[i];
        const Rangee &rg = *rangee_du_reglage(r.id);
        // Une seule exception : la source de la météo. Sa liste dépend du pays de la ville et compte jusqu'à quatre entrées, avec
        // des noms de service : des pastilles ne la tiennent pas ; elle ouvre une liste plein écran (un niveau, comme la ville).
        if (r.type == Type::Choix)
            VERIFIER(rg.editeur == (r.id == "meteo.source" ? Editeur::ListeSource : Editeur::Pastilles));
        if (r.type == Type::Nombre)
            VERIFIER(rg.editeur == Editeur::Curseur);
    }
    return 0;
}

// Le réglage « meteo.source » est dérivé de la table des sources (ecran/fournisseurs_meteo.hpp) : une option par entrée, dans le
// même ordre (la position est l'indice du choix, gardé et servi sous forme d'identifiant). Les valeurs attendues sont écrites ici à
// la main ; seule la comparaison avec la table est calculée.
constexpr int les_options_de_la_source_meteo_suivent_la_table_des_sources() {
    namespace fm = ecran::meteo::fournisseurs;
    const Reglage *r = chercher("meteo.source");
    VERIFIER(r != nullptr && r->type == Type::Choix && r->section == Section::LieuMeteo);
    VERIFIER(r->nb_options == fm::NOMBRE && r->min == 0 && r->max == static_cast<std::int32_t>(fm::NOMBRE) - 1);
    VERIFIER(r->defaut == 0 && r->options[0].code == "auto" &&
             !r->facultatif); // « Automatique » : le défaut, jamais « pas choisi »
    VERIFIER(r->options[0].libelle == "Automatique" && r->options[0].traduire);
    for (std::size_t i = 0; i < fm::NOMBRE; ++i) {
        VERIFIER(r->options[i].code == fm::TABLE[i].id && r->options[i].libelle == fm::TABLE[i].libelle);
        VERIFIER(r->options[i].traduire == fm::TABLE[i].libelle_traduit);
        VERIFIER(i == 0 || !r->options[i].traduire); // un nom de service est un nom propre
    }
    VERIFIER(r->route == "/api/location" &&
             r->champ == "source_meteo"); // la même route que la ville : un seul appel pour les deux
    return 0;
}

constexpr int l_editeur_ne_convient_qu_au_type_prevu() {
    // La matrice complète (5 types × 8 éditeurs), écrite à la main : « 1 » = l'éditeur convient. Rien n'est déduit du code testé.
    //                                Aucun Pastilles Curseur Clavier ClavierSecret ListeWifi RechercheVille ListeSource
    constexpr int ATTENDU[5][8] = {
        {0, 1, 0, 0, 0, 0, 0, 1},  // Choix : des pastilles, ou la liste filtrée par pays de la source météo
        {0, 0, 1, 0, 0, 0, 0, 0},  // Nombre
        {0, 0, 0, 1, 0, 1, 0, 0},  // Texte : le clavier, ou la liste des réseaux pour le nom du Wi-Fi
        {0, 0, 0, 0, 1, 0, 0, 0},  // Secret : jamais un clavier qui affiche
        {0, 0, 0, 0, 0, 0, 1, 0}}; // Lieu
    for (int t = 0; t < 5; ++t) {
        for (int e = 0; e < 8; ++e)
            VERIFIER(editeur_convient(static_cast<Type>(t), static_cast<Editeur>(e)) == (ATTENDU[t][e] == 1));
    }
    return 0;
}

constexpr int la_saisie_d_un_texte_est_controlee() {
    const Reglage &ssid = *chercher("wifi.ssid");
    const Reglage &modele = *chercher("stt.modele"); // 80 CARACTÈRES, pas 80 octets
    // aucun octet de contrôle : ni retour à la ligne, ni tabulation, ni effacement (0x7F)
    VERIFIER(valider_texte(ssid, "ab\x7f") == ErreurTexte::CaractereInterdit);
    VERIFIER(valider_texte(ssid, "a\nb") == ErreurTexte::CaractereInterdit);
    VERIFIER(valider_texte(ssid, "a\tb") == ErreurTexte::CaractereInterdit);
    VERIFIER(valider_texte(ssid, "a\x1f") == ErreurTexte::CaractereInterdit);
    VERIFIER(valider_texte(ssid, " ") == ErreurTexte::Aucune); // l'espace est permise (nom de réseau « Chez moi »)
    // un réglage qui n'est pas un texte ne se valide pas comme un texte
    VERIFIER(valider_texte(*chercher("langue.ecran"), "x") == ErreurTexte::CaractereInterdit);
    VERIFIER(valider_texte(*chercher("affichage.luminosite"), "50") == ErreurTexte::CaractereInterdit);
    // les accents comptent pour UN caractère quand le réglage se mesure en caractères
    char accents[2 * 81 + 1] = {}; // « é » = 0xC3 0xA9 (2 octets)
    for (int i = 0; i < 81; ++i) {
        accents[2 * i] = static_cast<char>(0xC3);
        accents[2 * i + 1] = static_cast<char>(0xA9);
    }
    VERIFIER(valider_texte(modele, std::string_view(accents, 2 * 80)) ==
             ErreurTexte::Aucune); // 80 caractères = 160 octets
    VERIFIER(valider_texte(modele, std::string_view(accents, 2 * 81)) == ErreurTexte::TropLong); // 81 caractères
    VERIFIER(valider_texte(modele, "") == ErreurTexte::Aucune);                                  // facultatif
    // le décompte lui-même : une lettre = un caractère, quel que soit son poids
    VERIFIER(nombre_de_caracteres("") == 0 && nombre_de_caracteres("a") == 1 && nombre_de_caracteres("é") == 1);
    VERIFIER(nombre_de_caracteres("İ") == 1 && nombre_de_caracteres("ẞ") == 1 && nombre_de_caracteres("€") == 1);
    VERIFIER(nombre_de_caracteres("aéẞ") == 3 && nombre_de_caracteres("日本語") == 3 &&
             nombre_de_caracteres("😀") == 1);
    return 0;
}

// ---- les préréglages des services ---------------------------------------------------------------------------------------

constexpr int les_options_de_chaque_service_sont_les_presets_qui_le_documentent() {
    const char *prefixes[NOMBRE_SERVICES] = {"stt.preset", "llm.preset", "tts.preset"};
    for (std::size_t s = 0; s < NOMBRE_SERVICES; ++s) {
        const Reglage &r = *chercher(prefixes[s]);
        VERIFIER(r.nb_options == compter_presets(static_cast<Service>(s)));
        std::size_t rang = 0;
        for (std::size_t p = 0; p < NOMBRE_PRESETS; ++p) {
            if (!preset_pour(PRESETS[p], static_cast<Service>(s)))
                continue;
            VERIFIER(r.options[rang].code == PRESETS[p].id);
            VERIFIER(r.options[rang].libelle == PRESETS[p].nom);
            VERIFIER(r.options[rang].traduire == PRESETS[p].nom_traduit);
            ++rang;
        }
        VERIFIER(rang == r.nb_options);
    }
    return 0;
}

constexpr int google_n_a_pas_d_audio_et_lm_studio_non_plus() {
    // docs/api-reglages.md §7 : Google ne documente que /chat/completions ; LM Studio aucune route audio.
    VERIFIER(chercher_preset("google") != nullptr && !preset_pour(*chercher_preset("google"), Service::Stt));
    VERIFIER(!preset_pour(*chercher_preset("google"), Service::Tts) &&
             preset_pour(*chercher_preset("google"), Service::Llm));
    VERIFIER(!preset_pour(*chercher_preset("lmstudio"), Service::Stt) &&
             !preset_pour(*chercher_preset("lmstudio"), Service::Tts));
    VERIFIER(preset_pour(*chercher_preset("kokoro"), Service::Tts) &&
             !preset_pour(*chercher_preset("kokoro"), Service::Llm));
    VERIFIER(preset_pour(*chercher_preset("openai"), Service::Stt) &&
             preset_pour(*chercher_preset("ollama"), Service::Tts));
    return 0;
}

constexpr int le_catalogue_des_presets_est_coherent() {
    VERIFIER(NOMBRE_PRESETS == 6);
    for (std::size_t p = 0; p < NOMBRE_PRESETS; ++p) {
        const Preset &x = PRESETS[p];
        VERIFIER(!x.id.empty() && !x.nom.empty() && x.services != 0 && (x.services & ~TOUS_LES_SERVICES) == 0);
        VERIFIER(!x.nom_traduit || est_une_cle(x.nom));
        VERIFIER(x.aide.empty() || est_une_cle(x.aide));
        VERIFIER(x.url.empty() || x.url.back() != '/'); // l'API retire le « / » final
        VERIFIER(x.url.empty() || x.url.substr(0, 7) == "http://" || x.url.substr(0, 8) == "https://");
        VERIFIER(x.local == contient_localhost(x.url));      // « mon PC » ⇔ localhost
        VERIFIER(x.url.empty() || x.cle_requise != x.local); // avec une adresse : local sans clé, en ligne avec clé
        for (std::size_t q = p + 1; q < NOMBRE_PRESETS; ++q)
            VERIFIER(x.id != PRESETS[q].id);
    }
    VERIFIER(chercher_preset("personnalise")->url.empty() && !chercher_preset("personnalise")->cle_requise);
    VERIFIER(chercher_preset("inconnu") == nullptr);
    VERIFIER(contient_localhost("http://localhost:1234/v1") && !contient_localhost("http://192.168.1.20:1234/v1"));
    VERIFIER(!contient_localhost("local") && !contient_localhost(""));
    return 0;
}

// ---- les alertes officielles (ADR-0009) ---------------------------------------------------------------------------------

constexpr int les_reglages_d_alertes_suivent_le_noyau_des_alertes() {
    // Les mêmes nombres que le noyau des alertes : le registre n'inclut pas ses en-têtes, ce test est le seul lien.
    VERIFIER(ALERTES_PERIODE_MIN == vigie::PERIODE_MIN_MIN && ALERTES_PERIODE_MAX == vigie::PERIODE_MAX_MIN);
    VERIFIER(ALERTES_PERIODE_DEFAUT == vigie::PERIODE_DEFAUT_MIN);
    VERIFIER(ALERTES_ADRESSE_MAX == static_cast<std::int32_t>(vigie::ADRESSE_MAX));
    const Reglage &periode = *chercher("alertes.periode");
    VERIFIER(periode.min == 10 && periode.max == 180 && periode.defaut == 15 && periode.unite == "min");
    VERIFIER(!nombre_valide(periode, 9) && nombre_valide(periode, 10) && nombre_valide(periode, 180) &&
             !nombre_valide(periode, 181));
    VERIFIER(nombre_depuis_nvs(periode, 1) == 15 && nombre_depuis_nvs(periode, 60) == 60); // un NVS abîmé : le défaut
    // Le niveau minimal : l'indice de l'option est celui de vigie::NiveauMin, son code celui de l'API.
    const Reglage &niveau = *chercher("alertes.niveau");
    VERIFIER(niveau.nb_options == vigie::NOMBRE_NIVEAUX_MIN);
    VERIFIER(niveau.defaut == static_cast<std::int32_t>(vigie::NiveauMin::Jaune));
    for (std::size_t i = 0; i < niveau.nb_options; ++i) {
        VERIFIER(niveau.options[i].code == regl::code(static_cast<vigie::NiveauMin>(i)));
        VERIFIER(niveau.options[i].traduire);
    }
    // « Oui » vient en premier : défaut = indice 0 = « true » dans l'API.
    const Reglage &actives = *chercher("alertes.actives");
    VERIFIER(actives.nb_options == 2 && actives.defaut == 0);
    VERIFIER(actives.options[0].code == "true" && actives.options[1].code == "false");
    // L'adresse : facultative (vide = instance publique), comptée en octets, au plus vigie::ADRESSE_MAX.
    const Reglage &adresse = *chercher("alertes.adresse");
    VERIFIER(adresse.type == Type::Texte && adresse.facultatif && adresse.longueur == Longueur::Octets);
    VERIFIER(adresse.max == 80 && adresse.min == 0);
    char texte[82] = {};
    for (int i = 0; i < 81; ++i)
        texte[i] = 'a';
    VERIFIER(valider_texte(adresse, "") == ErreurTexte::Aucune);
    VERIFIER(valider_texte(adresse, std::string_view(texte, 80)) == ErreurTexte::Aucune);
    VERIFIER(valider_texte(adresse, std::string_view(texte, 81)) == ErreurTexte::TropLong);
    // Les quatre : dans « Lieu et météo », hors de l'installation guidée (les défauts conviennent), sur la route /api/alerts.
    for (const char *id : {"alertes.actives", "alertes.niveau", "alertes.periode", "alertes.adresse"}) {
        const Reglage *r = chercher(id);
        VERIFIER(r != nullptr && r->section == Section::LieuMeteo && r->etape == Etape::Aucune);
        VERIFIER(r->route == "/api/alerts");
    }
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(identifiants_uniques_et_bien_formes)                                                                             \
    X(la_recherche_par_identifiant_retrouve_chaque_reglage)                                                            \
    X(chaque_choix_a_des_options_coherentes)                                                                           \
    X(les_sept_langues_du_registre_sont_celles_du_noyau)                                                               \
    X(les_themes_sont_ceux_de_l_api)                                                                                   \
    X(chaque_nombre_a_une_plage_et_un_defaut_sur)                                                                      \
    X(la_luminosite_ne_peut_jamais_eteindre_l_ecran)                                                                   \
    X(un_choix_lu_en_nvs_hors_liste_donne_le_defaut)                                                                   \
    X(les_textes_ont_des_bornes_et_aucun_defaut_secret)                                                                \
    X(les_bornes_du_wifi_sont_celles_du_standard)                                                                      \
    X(les_bornes_des_services_suivent_le_contrat)                                                                      \
    X(tous_les_secrets_sont_des_secrets)                                                                               \
    X(les_libelles_et_les_aides_sont_des_cles_de_traduction)                                                           \
    X(les_sections_et_les_etapes_sont_coherentes)                                                                      \
    X(chaque_reglage_a_un_chemin_dans_l_api)                                                                           \
    X(chaque_champ_reglable_de_l_api_a_un_reglage)                                                                     \
    X(deux_reglages_ne_partagent_pas_le_meme_champ_de_la_meme_route_sauf_les_services)                                 \
    X(chaque_reglage_a_exactement_un_ecran)                                                                            \
    X(chaque_rangee_reglage_designe_un_reglage_du_registre_et_un_editeur_qui_convient)                                 \
    X(les_actions_et_les_informations_sont_toutes_sur_une_page)                                                        \
    X(chaque_section_a_une_page_et_les_pages_suivent_l_ordre_des_sections)                                             \
    X(les_titres_de_section_sont_des_cles_de_traduction)                                                               \
    X(une_page_ne_montre_pas_plus_de_sept_rangees_visibles)                                                            \
    X(le_mot_de_passe_wifi_n_est_atteint_qu_apres_le_choix_du_reseau)                                                  \
    X(choix_et_nombres_se_reglent_sur_la_page_les_autres_ouvrent_un_editeur)                                           \
    X(l_editeur_ne_convient_qu_au_type_prevu)                                                                          \
    X(les_options_de_la_source_meteo_suivent_la_table_des_sources)                                                     \
    X(la_saisie_d_un_texte_est_controlee)                                                                              \
    X(les_options_de_chaque_service_sont_les_presets_qui_le_documentent)                                               \
    X(google_n_a_pas_d_audio_et_lm_studio_non_plus)                                                                    \
    X(le_catalogue_des_presets_est_coherent)                                                                           \
    X(les_reglages_d_alertes_suivent_le_noyau_des_alertes)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
