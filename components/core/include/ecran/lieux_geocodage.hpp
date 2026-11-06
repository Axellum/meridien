// Noyau pur : les lieux que la recherche de ville de l'ÉCRAN propose, tirés de la réponse du géocodage d'Open-Meteo.
//
// La page web a sa propre conversion (api_reglages.hpp, `convertir_geocodage`, qui écrit du JSON pour la page). L'écran n'a pas
// besoin de JSON : il lui faut une liste de lieux prêts à enregistrer (nom, région, pays, position, fuseau) et un libellé court.
// Les MÊMES règles d'écart y sont appliquées (lieu sans nom, sans fuseau, aux coordonnées invalides ou au texte non UTF-8 :
// écarté), pour que l'écran et la page proposent la même liste à la même saisie.
// C++17 standard, aucun include ESP-IDF, évaluable à la compilation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/api_json.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/reglages_valeurs.hpp"

namespace ecran::lieux {

// Place d'un libellé à l'écran (hote_reglages.hpp : LieuTrouve::libelle). Un libellé plus long est coupé sur un caractère entier.
inline constexpr std::size_t LIBELLE_OCTETS = 96;

// Au plus ce nombre de lieux proposés (hote_reglages.hpp : kLieuxMax) ; les suivants sont ignorés.
inline constexpr std::size_t CANDIDATS_MAX = 8;

struct Candidat {
    reglages::Zone zone{};
    char libelle[LIBELLE_OCTETS]{};
};

// « Lyon, Auvergne-Rhône-Alpes, France » : le nom, la région et le pays, séparés par « , », les parties vides sautées. Rend le
// nombre d'octets écrits (zéro final exclu).
constexpr std::size_t libelle(const reglages::Zone &z, char *sortie, std::size_t capacite) noexcept {
    char complet[3 * reglages::TEXTE_ZONE_OCTETS + 8] = {};
    api::Ecrivain e(complet, sizeof complet);
    bool premier = true;
    for (const char *partie :
         {static_cast<const char *>(z.nom), static_cast<const char *>(z.region), static_cast<const char *>(z.pays)}) {
        if (partie[0] == '\0') {
            continue;
        }
        if (!premier) {
            e.brut(", ");
        }
        e.brut(partie);
        premier = false;
    }
    return reglages::copier_texte(e.vue(), sortie, capacite);
}

// Lit la réponse du géocodage (`{"results":[…]}`) et remplit `sortie` (au plus `max`, jamais plus de CANDIDATS_MAX). Rend le
// nombre de lieux gardés ; zéro si le texte n'est pas du JSON, n'est pas un objet ou ne contient aucun lieu valable (une réponse
// sans « results », c'est-à-dire « aucun lieu de ce nom », donne aussi zéro).
constexpr std::size_t lire(std::string_view corps, Candidat *sortie, std::size_t max) noexcept {
    const json::Resultat lu = json::lire(corps);
    if (!lu.ok() || lu.valeur.genre() != json::Genre::Objet) {
        return 0;
    }
    const json::Valeur liste = lu.valeur.champ("results");
    const std::size_t n = liste.taille();
    const std::size_t limite = max < CANDIDATS_MAX ? max : CANDIDATS_MAX;
    std::size_t gardes = 0;
    for (std::size_t i = 0; i < n && gardes < limite; ++i) {
        const json::Valeur v = liste.element(i);
        char nom[4 * reglages::NOM_MAX_CARACTERES + 8] = {};
        char fuseau[40] = {};
        double lat = 0;
        double lon = 0;
        v.champ("name").copier_texte(nom, sizeof nom);
        v.champ("timezone").copier_texte(fuseau, sizeof fuseau);
        if (nom[0] == '\0' || fuseau[0] == '\0' || !reglages::utf8_valide(nom) || !v.champ("latitude").en_double(lat) ||
            !v.champ("longitude").en_double(lon) || !(lat >= -90.0 && lat <= 90.0) ||
            !(lon >= -180.0 && lon <= 180.0)) {
            continue;
        }
        Candidat &c = sortie[gardes];
        c = Candidat{};
        reglages::copier_texte(nom, c.zone.nom, sizeof c.zone.nom);
        reglages::copier_texte(fuseau, c.zone.fuseau, sizeof c.zone.fuseau);
        c.zone.latitude = lat;
        c.zone.longitude = lon;
        char region[4 * reglages::TEXTE_ZONE_OCTETS] = {};
        char pays[4 * reglages::TEXTE_ZONE_OCTETS] = {};
        char code_pays[8] = {};
        v.champ("admin1").copier_texte(region, sizeof region);
        v.champ("country").copier_texte(pays, sizeof pays);
        v.champ("country_code").copier_texte(code_pays, sizeof code_pays);
        if (reglages::utf8_valide(region)) {
            reglages::copier_texte(region, c.zone.region, sizeof c.zone.region);
        }
        if (reglages::utf8_valide(pays)) {
            reglages::copier_texte(pays, c.zone.pays, sizeof c.zone.pays);
        }
        reglages::copier_texte(code_pays, c.zone.code_pays, sizeof c.zone.code_pays);
        libelle(c.zone, c.libelle, sizeof c.libelle);
        ++gardes;
    }
    return gardes;
}

} // namespace ecran::lieux
