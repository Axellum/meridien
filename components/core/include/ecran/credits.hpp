// Noyau pur : la page « Sources » (ADR-0029) — QUELLES mentions de source et de licence l'écran montre, dans quel ordre, et avec quelle
// phrase. UNE seule table : la page de réglages (météo, lieux), la page des spots et la page « Sources » ne recopient plus chacune leur
// phrase de licence, elles lisent celles d'ici. C++17 standard, aucun include ESP-IDF ni LVGL, aucune allocation, évaluable à la compilation.
//
// Règle : la page ne dit que ce que l'écran montre VRAIMENT à cet instant (`Presences`). La météo est créditée au service qui l'a
// réellement donnée (Open-Meteo, MET Norway ou NWS, pas la source choisie : ADR-0014) ; Vigie n'est citée que si les alertes sont
// actives ; les normales climatiques de Météo-France que si une réponse du mois est en mémoire ; la mer et les spots que si un spot est
// choisi. Les lieux (GeoNames) et les polices sont toujours montrés.
//
// Les phrases sont des CLÉS de la table de traduction (le français est la clé, ADR-0006) avec leurs marqueurs `{lien}` et `{date}` ; les
// noms propres (Open-Meteo, GeoNames, Météo-France, Vigie) et les licences ne se traduisent pas. Le crédit de la mer est celui de
// mer_textes.hpp (`mer::Texte::Credit`), qui a sa propre table : la ligne `Rubrique::Mer` le désigne sans le recopier.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/fournisseurs_meteo.hpp"

namespace ecran::credits {

// ---- les phrases (clés françaises de la table de traduction) ---------------------------------------------------------------

// Météo : une phrase par service. CC BY 4.0 (Open-Meteo) ; CC BY 4.0 et NLOD 2.0 pour MET Norway, dont les valeurs sont converties et
// complétées ici (« adaptées », ce que CC BY demande d'indiquer) ; NWS : données ouvertes.
inline constexpr const char *METEO_OPEN_METEO = "Données météo : {lien} (licence CC BY 4.0)";
inline constexpr const char *METEO_MET_NO = "Données météo : {lien} (licences CC BY 4.0 et NLOD 2.0, données adaptées)";
inline constexpr const char *METEO_NWS = "Données météo : {lien} (données ouvertes)";
// Le NWS demande de ne pas laisser croire à son approbation (ses conditions d'usage, docs/mesures/fournisseurs-meteo.md).
inline constexpr const char *NWS_SANS_APPROBATION =
    "Cet appareil n’est ni approuvé ni parrainé par le National Weather Service.";

inline constexpr const char *LIEUX = "Lieux : {lien} (licence CC BY 4.0)";
inline constexpr const char *LIEUX_NOM = "GeoNames";

inline constexpr const char *ALERTES =
    "Alertes officielles : {lien}, chaque alerte garde la licence de son émetteur (voir la page Alertes)";
inline constexpr const char *ALERTES_NOM = "Vigie";

// Normales et records : les fiches climatologiques de Météo-France, sous Licence Ouverte 2.0, qui demande de nommer la source et la date
// d'édition. Lues chez Vigie, qui choisit la station et calcule la comparaison.
inline constexpr const char *NORMALES =
    "Normales et records : {lien}, fiches climatologiques éditées le {date} (Licence Ouverte 2.0), via Vigie";
inline constexpr const char *NORMALES_SANS_DATE =
    "Normales et records : {lien}, fiches climatologiques (Licence Ouverte 2.0), via Vigie";
inline constexpr const char *NORMALES_NOM = "Météo-France";

inline constexpr const char *SPOTS = "Spots : © contributeurs d’OpenStreetMap, ODbL.";

inline constexpr const char *POLICES =
    "Polices et icônes : Roboto (licence OFL 1.1), Material Design Icons (licence Apache 2.0)";

// Le titre de la page et du bouton de la roue : une clé qui existait déjà (page des alertes).
inline constexpr const char *TITRE = "Sources";

// ---- ce que l'écran montre -------------------------------------------------------------------------------------------------

enum class Rubrique : std::uint8_t {
    Meteo,
    MeteoNws, // l'avertissement « ni approuvé ni parrainé » (seulement quand la météo vient du NWS)
    Lieux,
    Alertes,
    Normales,
    Mer, // le crédit de mer_textes.hpp : `cle` est nul
    Spots,
    Polices,
};

struct Presences {
    bool meteo = false; // une météo est montrée
    meteo::fournisseurs::Famille origine =
        meteo::fournisseurs::Famille::OpenMeteo; // le service qui l'a RÉELLEMENT donnée
    bool alertes = false;                        // les alertes de Vigie sont activées
    bool normales = false;                       // une réponse de normales du mois local est en mémoire
    const char *edition = "";                    // sa date d'édition (« 2026-11-06 »), ou vide
    bool mer = false;                            // un spot de mer est choisi
};

// Une ligne de la page : la phrase (clé), et ce qui remplace `{lien}` et `{date}` (nuls : pas de marqueur à remplir).
struct Ligne {
    Rubrique rubrique = Rubrique::Polices;
    const char *cle = nullptr;
    const char *lien = nullptr;
    const char *date = nullptr;
};

inline constexpr std::size_t LIGNES_MAX = 8; // une de chaque rubrique

struct Liste {
    Ligne ligne[LIGNES_MAX] = {};
    std::size_t n = 0;
};

constexpr const char *phrase_meteo(meteo::fournisseurs::Famille f) noexcept {
    switch (f) {
    case meteo::fournisseurs::Famille::MetNo:
        return METEO_MET_NO;
    case meteo::fournisseurs::Famille::Nws:
        return METEO_NWS;
    case meteo::fournisseurs::Famille::OpenMeteo:
        break;
    }
    return METEO_OPEN_METEO;
}

// Les lignes de la page, dans l'ordre : météo (puis l'avertissement du NWS), lieux, alertes, normales, mer, spots, polices.
constexpr Liste composer(const Presences &p) noexcept {
    Liste l;
    const auto ajouter = [&l](Rubrique r, const char *cle, const char *lien, const char *date) {
        if (l.n < LIGNES_MAX)
            l.ligne[l.n++] = Ligne{r, cle, lien, date};
    };
    if (p.meteo) {
        ajouter(Rubrique::Meteo, phrase_meteo(p.origine), meteo::fournisseurs::attribution_de(p.origine).nom.data(),
                nullptr);
        if (p.origine == meteo::fournisseurs::Famille::Nws)
            ajouter(Rubrique::MeteoNws, NWS_SANS_APPROBATION, nullptr, nullptr);
    }
    ajouter(Rubrique::Lieux, LIEUX, LIEUX_NOM, nullptr);
    if (p.alertes)
        ajouter(Rubrique::Alertes, ALERTES, ALERTES_NOM, nullptr);
    if (p.normales) {
        const bool avec_date = p.edition != nullptr && p.edition[0] != '\0';
        ajouter(Rubrique::Normales, avec_date ? NORMALES : NORMALES_SANS_DATE, NORMALES_NOM,
                avec_date ? p.edition : nullptr);
    }
    if (p.mer) {
        ajouter(Rubrique::Mer, nullptr, nullptr, nullptr);
        ajouter(Rubrique::Spots, SPOTS, nullptr, nullptr);
    }
    ajouter(Rubrique::Polices, POLICES, nullptr, nullptr);
    return l;
}

// Toutes les clés de ce fichier, pour que les tests prouvent qu'aucune n'est absente de la table de traduction.
inline constexpr const char *CLES[] = {METEO_OPEN_METEO,
                                       METEO_MET_NO,
                                       METEO_NWS,
                                       NWS_SANS_APPROBATION,
                                       LIEUX,
                                       ALERTES,
                                       NORMALES,
                                       NORMALES_SANS_DATE,
                                       SPOTS,
                                       POLICES,
                                       TITRE};

} // namespace ecran::credits
