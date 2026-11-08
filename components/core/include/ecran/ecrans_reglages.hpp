// Noyau pur : DÉFINITION DES ÉCRANS de réglages (ADR-0007, point 3). Données seulement : quelle page montre quel réglage, avec
// quel éditeur. Le dessin (LVGL) vit dans main/ui_reglages.cpp et ne fait que parcourir ce tableau ; un test du PC échoue si un
// réglage du registre n'a aucun écran, ou si l'éditeur choisi ne convient pas au type du réglage.
//
// Profondeur : la liste des sections, puis UNE page par section (les trois services de l'assistant sont trois onglets de la même
// section). Un choix (langue, thème, préréglage) et un nombre (luminosité) se règlent sur la page elle-même ; seuls un texte, un
// secret, une ville ou un réseau Wi-Fi ouvrent un éditeur plein écran (clavier, recherche, liste de réseaux), et on en revient
// par « Retour » : jamais plus de deux niveaux sous la liste.
// C++17 standard, aucun include ESP-IDF ni LVGL.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/registre_reglages.hpp"

namespace ecran::registre {

enum class Genre : std::uint8_t {
    Reglage, // `ref` est l'identifiant d'un réglage du registre
    Action, // `ref` est l'identifiant d'une action (pas un réglage) : refaire l'installation, réinitialiser, redémarrer
    Info,   // `ref` est l'identifiant d'une information en lecture seule : état du Wi-Fi, code d'appairage, version
};

enum class Editeur : std::uint8_t {
    Aucun,          // Action et Info
    Pastilles,      // un Choix : toutes les options en boutons sur la page
    Curseur,        // un Nombre : curseur sur la page
    Clavier,        // un Texte : éditeur plein écran avec le clavier
    ClavierSecret,  // un Secret : idem, masqué, avec « Afficher / Masquer »
    ListeWifi,      // le nom du réseau : éditeur plein écran, liste des réseaux trouvés
    RechercheVille, // un Lieu : éditeur plein écran, recherche puis résultats
    ListeSource // un Choix dont les options dépendent de la ville (source de la météo) : liste plein écran, filtrée par pays
};

struct Rangee {
    Genre genre;
    std::string_view ref;
    Editeur editeur;
    bool
        dans_la_liste; // faux : atteinte seulement depuis un autre éditeur (le mot de passe, saisi après le choix du réseau)
};

struct Page {
    std::string_view id;
    Section section;
    std::string_view titre; // clé de tr() ; sur une section à plusieurs pages, c'est le libellé de l'onglet
    const Rangee *rangees;
    std::size_t nb;
};

namespace detail {

inline constexpr Rangee RANGEES_RESEAU[] = {
    {Genre::Info, "info.wifi", Editeur::Aucun, true},
    {Genre::Reglage, "wifi.ssid", Editeur::ListeWifi, true},
    {Genre::Reglage, "wifi.mot_de_passe", Editeur::ClavierSecret, false},
};
inline constexpr Rangee RANGEES_LIEU[] = {
    {Genre::Reglage, "lieu.ville", Editeur::RechercheVille, true},
    {Genre::Reglage, "meteo.source", Editeur::ListeSource, true},
    {Genre::Info, "info.meteo", Editeur::Aucun, true},
};
inline constexpr Rangee RANGEES_ALERTES[] = {
    {Genre::Reglage, "alertes.actives", Editeur::Pastilles, true},
    {Genre::Reglage, "alertes.niveau", Editeur::Pastilles, true},
    {Genre::Reglage, "alertes.periode", Editeur::Curseur, true},
    {Genre::Reglage, "alertes.adresse", Editeur::Clavier, true},
};
inline constexpr Rangee RANGEES_LANGUE[] = {
    {Genre::Reglage, "langue.ecran", Editeur::Pastilles, true},
    {Genre::Reglage, "langue.assistant", Editeur::Pastilles, true},
};
inline constexpr Rangee RANGEES_AFFICHAGE[] = {
    {Genre::Reglage, "affichage.theme", Editeur::Pastilles, true},
    {Genre::Reglage, "affichage.luminosite", Editeur::Curseur, true},
};
inline constexpr Rangee RANGEES_STT[] = {
    {Genre::Reglage, "stt.preset", Editeur::Pastilles, true},
    {Genre::Reglage, "stt.url", Editeur::Clavier, true},
    {Genre::Reglage, "stt.modele", Editeur::Clavier, true},
    {Genre::Reglage, "stt.cle", Editeur::ClavierSecret, true},
    {Genre::Action, "action.tester_stt", Editeur::Aucun, true},
};
inline constexpr Rangee RANGEES_LLM[] = {
    {Genre::Reglage, "llm.preset", Editeur::Pastilles, true},
    {Genre::Reglage, "llm.url", Editeur::Clavier, true},
    {Genre::Reglage, "llm.modele", Editeur::Clavier, true},
    {Genre::Reglage, "llm.cle", Editeur::ClavierSecret, true},
    {Genre::Action, "action.tester_llm", Editeur::Aucun, true},
};
inline constexpr Rangee RANGEES_TTS[] = {
    {Genre::Reglage, "tts.preset", Editeur::Pastilles, true},
    {Genre::Reglage, "tts.url", Editeur::Clavier, true},
    {Genre::Reglage, "tts.modele", Editeur::Clavier, true},
    {Genre::Reglage, "tts.voix", Editeur::Clavier, true},
    {Genre::Reglage, "tts.cle", Editeur::ClavierSecret, true},
    {Genre::Action, "action.tester_tts", Editeur::Aucun, true},
};
inline constexpr Rangee RANGEES_APPAREIL[] = {
    {Genre::Info, "info.telephone", Editeur::Aucun, true},
    {Genre::Info, "info.version", Editeur::Aucun, true},
    // Le diagnostic (cause du dernier redémarrage, Wi-Fi, point tactile) n'a plus de bouton ni de geste sur l'écran principal : on l'ouvre ici.
    {Genre::Action, "action.diagnostic", Editeur::Aucun, true},
    {Genre::Action, "action.refaire_installation", Editeur::Aucun, true},
    {Genre::Action, "action.redemarrer", Editeur::Aucun, true},
    {Genre::Action, "action.reinitialiser", Editeur::Aucun, true},
};

template <std::size_t N>
constexpr Page page(std::string_view id, Section section, std::string_view titre, const Rangee (&rangees)[N]) noexcept {
    return {id, section, titre, rangees, N};
}

} // namespace detail

// Les pages, dans l'ordre des sections. Une section à plusieurs pages (assistant vocal) les montre en onglets.
inline constexpr Page PAGES[] = {
    detail::page("reseau", Section::Reseau, "Wi-Fi", detail::RANGEES_RESEAU),
    detail::page("lieu", Section::LieuMeteo, "Ville", detail::RANGEES_LIEU),
    detail::page("alertes", Section::LieuMeteo, "Alertes", detail::RANGEES_ALERTES),
    detail::page("langue", Section::Langue, "Langue", detail::RANGEES_LANGUE),
    detail::page("affichage", Section::Affichage, "Affichage", detail::RANGEES_AFFICHAGE),
    detail::page("assistant_stt", Section::AssistantVocal, "Comprendre ma voix", detail::RANGEES_STT),
    detail::page("assistant_llm", Section::AssistantVocal, "Réfléchir", detail::RANGEES_LLM),
    detail::page("assistant_tts", Section::AssistantVocal, "Parler", detail::RANGEES_TTS),
    detail::page("appareil", Section::Appareil, "Appareil", detail::RANGEES_APPAREIL),
};

inline constexpr std::size_t NOMBRE_PAGES = sizeof(PAGES) / sizeof(PAGES[0]);

// Titre de chaque section de la liste (clés de tr()), dans l'ordre de l'enum Section.
inline constexpr std::string_view TITRES_SECTIONS[NOMBRE_SECTIONS] = {"Réseau",    "Lieu et météo",   "Langue",
                                                                      "Affichage", "Assistant vocal", "Appareil"};

constexpr std::string_view titre_section(Section s) noexcept { return TITRES_SECTIONS[static_cast<std::size_t>(s)]; }

constexpr std::size_t pages_de_section(Section s) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < NOMBRE_PAGES; ++i)
        n += PAGES[i].section == s ? 1 : 0;
    return n;
}

// Indice de la `rang`-ième page de la section (0 = la première), ou NOMBRE_PAGES si elle n'existe pas.
constexpr std::size_t page_de_section(Section s, std::size_t rang) noexcept {
    std::size_t vu = 0;
    for (std::size_t i = 0; i < NOMBRE_PAGES; ++i) {
        if (PAGES[i].section != s)
            continue;
        if (vu == rang)
            return i;
        ++vu;
    }
    return NOMBRE_PAGES;
}

// Éditeur compatible avec le type d'un réglage.
constexpr bool editeur_convient(Type t, Editeur e) noexcept {
    switch (t) {
    case Type::Choix:
        return e == Editeur::Pastilles || e == Editeur::ListeSource;
    case Type::Nombre:
        return e == Editeur::Curseur;
    case Type::Texte:
        return e == Editeur::Clavier || e == Editeur::ListeWifi;
    case Type::Secret:
        return e == Editeur::ClavierSecret;
    case Type::Lieu:
        return e == Editeur::RechercheVille;
    }
    return false;
}

// Combien de rangées de pages montrent ce réglage (un test exige exactement une).
constexpr std::size_t ecrans_du_reglage(std::string_view id) noexcept {
    std::size_t n = 0;
    for (std::size_t p = 0; p < NOMBRE_PAGES; ++p) {
        for (std::size_t r = 0; r < PAGES[p].nb; ++r) {
            if (PAGES[p].rangees[r].genre == Genre::Reglage && PAGES[p].rangees[r].ref == id)
                ++n;
        }
    }
    return n;
}

// La rangée qui montre ce réglage, ou nullptr.
constexpr const Rangee *rangee_du_reglage(std::string_view id) noexcept {
    for (std::size_t p = 0; p < NOMBRE_PAGES; ++p) {
        for (std::size_t r = 0; r < PAGES[p].nb; ++r) {
            if (PAGES[p].rangees[r].genre == Genre::Reglage && PAGES[p].rangees[r].ref == id)
                return &PAGES[p].rangees[r];
        }
    }
    return nullptr;
}

} // namespace ecran::registre
