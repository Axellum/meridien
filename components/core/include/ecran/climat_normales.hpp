// Noyau pur : « est-ce normal ? » — lecture de la réponse de Vigie (GET /v1/climate/normals, ADR-0027 de vigie-api) et jugement du
// maximum prévu du jour contre la normale du mois (ADR-0029). C++17 standard, aucun include ESP-IDF ni LVGL, aucune allocation, aucune
// exception, évaluable à la compilation.
//
// Ce que la route donne (réponse réelle du 21/11/2026, Lorient) : pour la station Météo-France la plus proche (50 km au plus), les
// NORMALES 1991-2020 du MOIS (moyenne du mois, pas d'un jour) et les RECORDS du mois avec leur date. Hors couverture (hors de France et
// de ses territoires d'outre-mer) : `status` = `outside_coverage` et rien n'est estimé. Ce fichier ne garde que ce que l'écran montre :
// la normale du maximum du mois, le record de chaleur et le record de froid du mois, le mois de la réponse, la date d'édition de la
// fiche (que la Licence Ouverte 2.0 demande de nommer).
//
// Règles (ADR-0029) :
//   - la racine doit être un objet qui porte `status` (« available » ou « outside_coverage ») et `month` (1 à 12) ; sinon `Illisible`
//     ou `Incoherent` et rien n'est gardé : on ne devine rien d'une réponse qu'on ne comprend pas ;
//   - une valeur absente, `null` ou hors de -60 à +60 °C est INCONNUE (`std::optional` vide), jamais 0 ;
//   - les valeurs sont gardées en DIXIÈMES de degré, arrondies sur le texte (json::Valeur::en_fixe), sans flottant ;
//   - le jugement ne vaut que pour le MOIS de la réponse : un autre mois local, ou une réponse de plus de 48 h, ne dit rien ;
//   - un record n'est « battu » que par une station qui mesure encore (`station.active`) : les records d'une station arrêtée ne bougent plus.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ecran/json_mini.hpp"

namespace ecran::climat {

inline constexpr std::size_t EDITION_CAP = 11; // « 2026-11-06 » et le zéro final

// À partir de quel écart (en dixièmes de degré) le maximum du jour n'est plus « dans la normale ». 10 = 1,0° : à 0,9° d'écart l'écran
// dit « Dans la normale », à 1,0° « +1,0° sur la normale ».
inline constexpr int SEUIL_NORMAL_DIX = 10;
// Au-delà, la réponse est trop vieille pour qu'on en parle (elle est redemandée une fois par jour : 48 h = un jour de retard permis).
inline constexpr std::int64_t VALIDITE_S = 48 * 3600;

enum class Statut : std::uint8_t {
    Illisible = 0, // pas du JSON, ou texte coupé
    Incoherent,    // du JSON, mais pas la réponse attendue (pas d'objet, `status` ou `month` absents ou hors table)
    HorsCouverture, // aucune fiche Météo-France à moins de 50 km : rien à montrer, et rien à redemander avant un changement de lieu
    Ok,
};

struct Normales {
    int mois = 0;                        // 1 à 12 : le mois que la réponse décrit (0 : rien de lu)
    std::optional<int> tmax_normale_dix; // normale du maximum du mois, dixièmes de °C
    std::optional<int> tmax_record_dix;  // record de chaleur du mois
    std::optional<int> tmin_record_dix;  // record de froid du mois
    bool station_active = false;         // la station mesure encore : ses records peuvent être battus
    char edition[EDITION_CAP] = {}; // date d'édition de la fiche (« 2026-11-06 »), ou vide si Vigie ne l'a pas donnée
};

namespace detail {

inline constexpr int DIX_MIN = -600;
inline constexpr int DIX_MAX = 600;

// Un nombre de la réponse en dixièmes de degré, ou rien s'il manque, est `null` ou sort de -60..+60 °C.
constexpr std::optional<int> en_dixiemes(const json::Valeur &v) noexcept {
    std::int64_t x = 0;
    if (!v.existe() || !v.en_fixe(1, x) || x < DIX_MIN || x > DIX_MAX)
        return std::nullopt;
    return static_cast<int>(x);
}

constexpr bool est_chiffre(char c) noexcept { return c >= '0' && c <= '9'; }

// « 2026-11-06 » exactement : dix caractères, chiffres et deux tirets. Sinon rien (une date mal formée ne se montre pas).
constexpr bool copier_edition(const json::Valeur &v, char (&sortie)[EDITION_CAP]) noexcept {
    sortie[0] = '\0';
    char tampon[EDITION_CAP + 4] = {};
    if (v.copier_texte(tampon, sizeof tampon) != EDITION_CAP - 1)
        return false;
    for (std::size_t i = 0; i < EDITION_CAP - 1; ++i) {
        const bool tiret = i == 4 || i == 7;
        if (tiret ? tampon[i] != '-' : !est_chiffre(tampon[i]))
            return false;
    }
    for (std::size_t i = 0; i < EDITION_CAP; ++i)
        sortie[i] = tampon[i];
    return true;
}

} // namespace detail

// Lit la réponse. `sortie` est remise à zéro d'abord ; elle n'est garnie que si le statut est `Ok` ; pour `HorsCouverture` seul `mois`
// est écrit.
constexpr Statut analyser(std::string_view json, Normales &sortie) noexcept {
    sortie = Normales{};
    const json::Resultat r = json::lire(json);
    if (!r.ok())
        return Statut::Illisible;
    const json::Valeur &racine = r.valeur;
    if (racine.genre() != json::Genre::Objet)
        return Statut::Incoherent;
    std::int64_t mois = 0;
    if (!racine.champ("month").en_entier(mois) || mois < 1 || mois > 12)
        return Statut::Incoherent;
    const json::Valeur etat = racine.champ("status");
    if (etat.texte_egal("outside_coverage")) {
        sortie.mois = static_cast<int>(mois);
        return Statut::HorsCouverture;
    }
    if (!etat.texte_egal("available"))
        return Statut::Incoherent;
    Normales n;
    n.mois = static_cast<int>(mois);
    n.tmax_normale_dix = detail::en_dixiemes(racine.trouver("normals.tmax_mean_c.value"));
    n.tmax_record_dix = detail::en_dixiemes(racine.trouver("records.tmax_c.value"));
    n.tmin_record_dix = detail::en_dixiemes(racine.trouver("records.tmin_c.value"));
    bool active = false;
    n.station_active = racine.trouver("station.active").en_booleen(active) && active;
    detail::copier_edition(racine.trouver("station.sheet_edited"), n.edition);
    sortie = n;
    return Statut::Ok;
}

// Ce que la pastille dit.
enum class Etat : std::uint8_t {
    Aucun = 0,   // rien à dire : pas de pastille
    AuDessus,    // le maximum du jour est au moins 1,0° au-dessus de la normale
    EnDessous,   // … au moins 1,0° au-dessous
    Normal,      // à moins de 1,0° de la normale
    RecordChaud, // le maximum prévu dépasse le record de chaleur du mois
    RecordFroid, // le minimum prévu passe sous le record de froid du mois
};

struct Verdict {
    Etat etat = Etat::Aucun;
    int ecart_dix = 0; // maximum du jour moins normale, dixièmes de degré (utile pour AuDessus et EnDessous)
};

// La réponse décrit-elle le mois local ? Sinon elle ne vaut pas pour aujourd'hui (le jour de la bascule) : le client en demande une autre.
constexpr bool pour_ce_mois(const Normales &n, int mois_local) noexcept { return n.mois >= 1 && n.mois == mois_local; }

// Juge le jour. `max_dix` et `min_dix` : le maximum et le minimum PRÉVUS du jour (dixièmes de °C, vides s'ils sont inconnus).
// `mois_local` : le mois du lieu à cet instant (1 à 12). `age_s` : depuis combien de secondes la réponse est arrivée.
// Aucun verdict (`Aucun`) si la réponse n'est pas du mois local, trop vieille (ou d'un âge négatif : l'horloge a reculé), ou si les
// valeurs manquent. Les records passent avant l'écart : un record battu est plus rare que « +4° ».
constexpr Verdict juger(const Normales &n, std::optional<int> max_dix, std::optional<int> min_dix, int mois_local,
                        std::int64_t age_s) noexcept {
    if (!pour_ce_mois(n, mois_local) || age_s < 0 || age_s > VALIDITE_S)
        return {};
    if (n.station_active) {
        if (max_dix && n.tmax_record_dix && *max_dix > *n.tmax_record_dix)
            return {Etat::RecordChaud, 0};
        if (min_dix && n.tmin_record_dix && *min_dix < *n.tmin_record_dix)
            return {Etat::RecordFroid, 0};
    }
    if (!max_dix || !n.tmax_normale_dix)
        return {};
    const int ecart = *max_dix - *n.tmax_normale_dix;
    const int absolu = ecart < 0 ? -ecart : ecart;
    if (absolu < SEUIL_NORMAL_DIX)
        return {Etat::Normal, ecart};
    return {ecart > 0 ? Etat::AuDessus : Etat::EnDessous, ecart};
}

// « +1,0 » ou « −4,2 » (le moins est le vrai signe moins U+2212, que la police embarquée dessine) : l'écart en dixièmes de degré avec son
// signe, `separateur` entre les unités et les dixièmes (« , » en français, « . » en anglais). Un écart nul s'écrit « +0,0 ». Rend le
// nombre d'octets écrits, zéro final exclu ; 0 (et une chaîne vide) si `capacite` est trop petite.
constexpr std::size_t ecrire_ecart(int ecart_dix, char separateur, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    sortie[0] = '\0';
    const bool negatif = ecart_dix < 0;
    unsigned a = static_cast<unsigned>(negatif ? -ecart_dix : ecart_dix);
    if (a > 999U)
        a = 999U; // un écart de plus de 99,9° n'existe pas : jamais de dépassement de tampon
    char chiffres[8] = {};
    std::size_t nb = 0;
    chiffres[nb++] = static_cast<char>('0' + a % 10U); // dixièmes
    chiffres[nb++] = separateur;
    unsigned entier = a / 10U;
    do {
        chiffres[nb++] = static_cast<char>('0' + entier % 10U);
        entier /= 10U;
    } while (entier > 0U);
    std::size_t p = 0;
    if (negatif) {
        if (capacite < 3 + nb + 1)
            return 0;
        sortie[p++] = static_cast<char>(0xE2);
        sortie[p++] = static_cast<char>(0x88);
        sortie[p++] = static_cast<char>(0x92);
    } else {
        if (capacite < 1 + nb + 1)
            return 0;
        sortie[p++] = '+';
    }
    while (nb > 0)
        sortie[p++] = chiffres[--nb];
    sortie[p] = '\0';
    return p;
}

} // namespace ecran::climat
