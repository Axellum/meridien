// Noyau pur : tout ce que l'écran principal calcule avant de dessiner. Échelles des barres (températures de la semaine, pluie),
// couleur d'une température, catégories (UV, vent de Beaufort, tendance de la pression), alertes visuelles (gel, forte chaleur,
// pluie forte, orage, vent), phrase « pluie dans N minutes », ambiance du ciel, formats de nombres. C++17 standard, aucun
// include ESP-IDF ni LVGL, entiers seulement : évaluable à la compilation (test/host/test_visuel_echelles.cpp).
//
// Convention des valeurs inconnues (celle de main/ui_principal.h) : entier -1 ; la température, elle, est en dixièmes de
// degré et son inconnue est INCONNU_DIX (valeur impossible), jamais 0. L'écran cache ce qu'il ne connaît pas.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/meteo_wmo.hpp"

namespace ecran::visuel {

inline constexpr int INCONNU = -1;
inline constexpr int INCONNU_DIX = -32768; // dixièmes (°C, hPa, km/h, mm...) : hors de toute plage vraisemblable

constexpr bool connu(int dixiemes) noexcept { return dixiemes != INCONNU_DIX; }

constexpr int borner(int v, int bas, int haut) noexcept { return v < bas ? bas : (v > haut ? haut : v); }

// Arrondi à l'entier le plus proche (moitié loin de zéro) d'une valeur en dixièmes.
constexpr int arrondi_dix(int dixiemes) noexcept {
    return dixiemes >= 0 ? (dixiemes + 5) / 10 : -((-dixiemes + 5) / 10);
}

// ---- ambiance du ciel ----

// Sert à choisir les couleurs du fond (thème « ciel ») et l'alerte principale. Une valeur par grande famille de temps.
enum class Ambiance : std::uint8_t { CLAIR, NUAGEUX, COUVERT, BRUME, PLUIE, NEIGE, ORAGE, NOMBRE };

constexpr Ambiance ambiance_pour(meteo::Icone icone) noexcept {
    switch (icone) {
    case meteo::Icone::Soleil:
    case meteo::Icone::Lune:
    case meteo::Icone::PeuNuageuxJour:
    case meteo::Icone::PeuNuageuxNuit:
        return Ambiance::CLAIR;
    case meteo::Icone::NuageuxJour:
    case meteo::Icone::NuageuxNuit:
        return Ambiance::NUAGEUX;
    case meteo::Icone::Couvert:
    case meteo::Icone::Inconnue:
        return Ambiance::COUVERT;
    case meteo::Icone::Brouillard:
        return Ambiance::BRUME;
    case meteo::Icone::Bruine:
    case meteo::Icone::PluieVerglacante:
    case meteo::Icone::Pluie:
    case meteo::Icone::PluieForte:
    case meteo::Icone::Averses:
    case meteo::Icone::AversesViolentes:
        return Ambiance::PLUIE;
    case meteo::Icone::Neige:
    case meteo::Icone::NeigeForte:
    case meteo::Icone::GrainsDeNeige:
    case meteo::Icone::AversesDeNeige:
        return Ambiance::NEIGE;
    case meteo::Icone::Orage:
    case meteo::Icone::OrageGrele:
        return Ambiance::ORAGE;
    case meteo::Icone::Nombre:
        break;
    }
    return Ambiance::COUVERT;
}

constexpr Ambiance ambiance_pour_code(int code_wmo, bool jour) noexcept {
    return ambiance_pour(meteo::decrire(code_wmo, jour).icone);
}

// ---- catégories ----

enum class CategorieUv : std::uint8_t { INCONNU, FAIBLE, MODERE, ELEVE, TRES_ELEVE, EXTREME };

// Échelle de l'OMS : 0-2 faible, 3-5 modéré, 6-7 élevé, 8-10 très élevé, 11 et plus extrême. `uv_dix` en dixièmes d'indice.
constexpr CategorieUv categorie_uv(int uv_dix) noexcept {
    if (!connu(uv_dix) || uv_dix < 0)
        return CategorieUv::INCONNU;
    if (uv_dix < 30)
        return CategorieUv::FAIBLE;
    if (uv_dix < 60)
        return CategorieUv::MODERE;
    if (uv_dix < 80)
        return CategorieUv::ELEVE;
    if (uv_dix < 110)
        return CategorieUv::TRES_ELEVE;
    return CategorieUv::EXTREME;
}

// Degré de l'échelle de Beaufort (0 à 12) d'une vitesse en km/h, bornes basses 1, 6, 12, 20, 29, 39, 50, 62, 75, 89, 103, 118.
constexpr int beaufort(int vitesse_dix_kmh) noexcept {
    constexpr int BORNES[] = {10, 60, 120, 200, 290, 390, 500, 620, 750, 890, 1030, 1180}; // dixièmes de km/h
    if (!connu(vitesse_dix_kmh) || vitesse_dix_kmh < 0)
        return INCONNU;
    int degre = 0;
    for (int b : BORNES) {
        if (vitesse_dix_kmh >= b)
            ++degre;
    }
    return degre;
}

enum class TendancePression : std::uint8_t { INCONNUE, BAISSE, STABLE, HAUSSE };

// Variation sur 3 heures, en dixièmes d'hectopascal : au moins 1,0 hPa = tendance (la météo parle de « rapide » à 1,6 hPa).
constexpr TendancePression tendance_pression(int variation_dix_hpa_3h) noexcept {
    if (!connu(variation_dix_hpa_3h))
        return TendancePression::INCONNUE;
    if (variation_dix_hpa_3h >= 10)
        return TendancePression::HAUSSE;
    if (variation_dix_hpa_3h <= -10)
        return TendancePression::BAISSE;
    return TendancePression::STABLE;
}

// Direction du vent (d'où il vient, degrés) -> secteur de 45 degrés : 0 nord, 1 nord-est, 2 est, 3 sud-est, 4 sud, 5 sud-ouest,
// 6 ouest, 7 nord-ouest ; -1 si inconnue. Le secteur sert à une phrase, la flèche est dessinée avec l'angle exact.
constexpr int secteur_vent(int degres) noexcept {
    if (degres < 0 || degres > 360)
        return INCONNU;
    return ((degres * 2 + 45) / 90) % 8;
}

// Angle vers lequel la flèche pointe : le vent souffle VERS le côté opposé à celui d'où il vient.
constexpr int angle_fleche_vent(int degres_d_ou_il_vient) noexcept {
    return degres_d_ou_il_vient < 0 ? INCONNU : (degres_d_ou_il_vient + 180) % 360;
}

// ---- alertes visuelles ----

enum class Alerte : std::uint8_t { AUCUNE, GEL, CHALEUR, VENT_FORT, PLUIE_FORTE, ORAGE };

struct EntreesAlerte {
    int min_dix = INCONNU_DIX;           // minimum du jour
    int max_dix = INCONNU_DIX;           // maximum du jour
    int code_wmo = INCONNU;              // code de l'instant ou du jour
    int proba_pluie_pct = INCONNU;       // probabilité de pluie
    int pluie_jour_dix_mm = INCONNU_DIX; // cumul de pluie du jour (dixièmes de mm)
    int rafales_dix_kmh = INCONNU_DIX;   // rafales maximales
};

inline constexpr int SEUIL_GEL_DIX = 0;          // minimum à 0 °C ou moins
inline constexpr int SEUIL_CHALEUR_DIX = 320;    // maximum à 32 °C ou plus
inline constexpr int SEUIL_RAFALES_DIX = 600;    // rafales à 60 km/h ou plus (tempête : Beaufort 8 à partir de 62)
inline constexpr int SEUIL_PLUIE_JOUR_DIX = 200; // 20 mm dans la journée

// Une seule alerte, la plus grave : orage, pluie forte, vent fort, forte chaleur, gel. Rien d'inventé : une donnée inconnue
// ne déclenche rien.
constexpr Alerte alerte_principale(const EntreesAlerte &e) noexcept {
    const bool orage = e.code_wmo == 95 || e.code_wmo == 96 || e.code_wmo == 97 || e.code_wmo == 99;
    if (orage)
        return Alerte::ORAGE;
    const bool pluie_forte =
        e.code_wmo == 65 || e.code_wmo == 67 || e.code_wmo == 82 || e.code_wmo == 86 ||
        (connu(e.pluie_jour_dix_mm) && e.pluie_jour_dix_mm >= SEUIL_PLUIE_JOUR_DIX && e.proba_pluie_pct >= 60);
    if (pluie_forte)
        return Alerte::PLUIE_FORTE;
    if (connu(e.rafales_dix_kmh) && e.rafales_dix_kmh >= SEUIL_RAFALES_DIX)
        return Alerte::VENT_FORT;
    if (connu(e.max_dix) && e.max_dix >= SEUIL_CHALEUR_DIX)
        return Alerte::CHALEUR;
    if (connu(e.min_dix) && e.min_dix <= SEUIL_GEL_DIX)
        return Alerte::GEL;
    return Alerte::AUCUNE;
}

// ---- pluie dans N minutes ----

enum class PluieProche : std::uint8_t { INCONNUE, AUCUNE, EN_COURS, BIENTOT };

// `minutes` : minutes avant la prochaine pluie (contrat de la demande à meteo-appareil : 0 il pleut, -1 rien dans les 2 h,
// -2 donnée absente pour ce lieu). Toute autre valeur négative est traitée comme inconnue.
constexpr PluieProche pluie_proche(int minutes) noexcept {
    if (minutes == 0)
        return PluieProche::EN_COURS;
    if (minutes > 0)
        return PluieProche::BIENTOT;
    if (minutes == -1)
        return PluieProche::AUCUNE;
    return PluieProche::INCONNUE;
}

// ---- échelles ----

// Échelle de température commune aux sept jours : chaque barre min-max est placée sur la même règle, sinon deux jours ne se
// comparent pas. Au moins `ETENDUE_MIN_DIX` de large (une semaine à 12-13 °C partout ne doit pas donner des barres pleines),
// aux dizaines de degrés entiers pour que les repères ne bougent pas à chaque mise à jour.
struct Echelle {
    int mini_dix;
    int maxi_dix;
};

inline constexpr int ETENDUE_MIN_DIX = 100; // 10 °C

constexpr Echelle echelle_semaine(const int *mins_dix, const int *maxs_dix, std::size_t n) noexcept {
    int bas = INCONNU_DIX;
    int haut = INCONNU_DIX;
    for (std::size_t i = 0; i < n; ++i) {
        if (connu(mins_dix[i]) && (!connu(bas) || mins_dix[i] < bas))
            bas = mins_dix[i];
        if (connu(maxs_dix[i]) && (!connu(haut) || maxs_dix[i] > haut))
            haut = maxs_dix[i];
    }
    if (!connu(bas) && !connu(haut))
        return {0, ETENDUE_MIN_DIX};
    if (!connu(bas))
        bas = haut;
    if (!connu(haut))
        haut = bas;
    // arrondi vers l'extérieur au degré entier
    int b = (bas >= 0 ? bas / 10 : -((-bas + 9) / 10)) * 10;
    int h = (haut >= 0 ? (haut + 9) / 10 : -((-haut) / 10)) * 10;
    if (h - b < ETENDUE_MIN_DIX) {
        const int manque = ETENDUE_MIN_DIX - (h - b);
        b -= manque / 2;
        h += manque - manque / 2;
    }
    return {b, h};
}

// Position (0 à `largeur`) d'une valeur sur l'échelle, bornée.
constexpr int position_sur_echelle(int valeur_dix, const Echelle &e, int largeur) noexcept {
    if (!connu(valeur_dix) || e.maxi_dix <= e.mini_dix)
        return 0;
    const long long v = static_cast<long long>(borner(valeur_dix, e.mini_dix, e.maxi_dix) - e.mini_dix) * largeur;
    return static_cast<int>(v / (e.maxi_dix - e.mini_dix));
}

// Hauteur d'une barre de pluie : `pct` (0 à 100) de `hauteur_max`, au moins 2 pixels dès que `pct` > 0 (une pluie à 5 % doit
// se voir), 0 si inconnu.
constexpr int hauteur_barre_pct(int pct, int hauteur_max) noexcept {
    if (pct <= 0)
        return 0;
    const int h = borner(pct, 0, 100) * hauteur_max / 100;
    return h < 2 ? 2 : h;
}

// ---- couleur d'une température ----

struct Rgb {
    std::uint8_t r, g, b;
};

constexpr Rgb melanger(Rgb a, Rgb b, int pour_mille) noexcept {
    const int t = borner(pour_mille, 0, 1000);
    const auto mix = [t](int x, int y) { return static_cast<std::uint8_t>((x * (1000 - t) + y * t + 500) / 1000); };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

constexpr std::uint32_t en_hex(Rgb c) noexcept {
    return (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) | c.b;
}

constexpr Rgb depuis_hex(std::uint32_t h) noexcept {
    return {static_cast<std::uint8_t>(h >> 16), static_cast<std::uint8_t>(h >> 8), static_cast<std::uint8_t>(h)};
}

// Rampe de 5 teintes (froid -> chaud) aux repères -10, 0, 12, 24 et 36 °C ; entre deux repères, mélange linéaire.
struct Rampe {
    std::uint32_t teinte[5];
};

inline constexpr int REPERES_RAMPE_DIX[5] = {-100, 0, 120, 240, 360};

constexpr std::uint32_t couleur_temperature(int temperature_dix, const Rampe &r) noexcept {
    if (!connu(temperature_dix))
        return r.teinte[2];
    if (temperature_dix <= REPERES_RAMPE_DIX[0])
        return r.teinte[0];
    for (int i = 1; i < 5; ++i) {
        if (temperature_dix <= REPERES_RAMPE_DIX[i]) {
            const int etendue = REPERES_RAMPE_DIX[i] - REPERES_RAMPE_DIX[i - 1];
            const int pm = (temperature_dix - REPERES_RAMPE_DIX[i - 1]) * 1000 / etendue;
            return en_hex(melanger(depuis_hex(r.teinte[i - 1]), depuis_hex(r.teinte[i]), pm));
        }
    }
    return r.teinte[4];
}

// ---- heure locale d'un instant de prévision ----

constexpr std::int64_t plancher_div(std::int64_t a, std::int64_t b) noexcept {
    return (a >= 0 ? a / b : -((-a + b - 1) / b));
}

// Heure (0 à 23) d'un instant UTC pour un décalage local en secondes (Open-Meteo : utc_offset_seconds).
constexpr int heure_locale(std::int64_t epoch_s, int decalage_s) noexcept {
    const std::int64_t local = epoch_s + decalage_s;
    const std::int64_t jours = plancher_div(local, 86400);
    return static_cast<int>((local - jours * 86400) / 3600);
}

// Jour de la semaine (0 dimanche ... 6 samedi) d'un instant UTC pour un décalage local ; le 1er janvier 1970 était un jeudi.
constexpr int jour_semaine_locale(std::int64_t epoch_s, int decalage_s) noexcept {
    const std::int64_t jours = plancher_div(epoch_s + decalage_s, 86400);
    return static_cast<int>(((jours % 7) + 4 + 7) % 7);
}

// ---- formats ----

// Température entière avec le vrai signe moins (U+2212, présent dans Roboto Bold) : « 12 », « −3 ». Écrit un zéro final ;
// rend le nombre d'octets écrits (sans le zéro), 0 si le tampon est trop petit ou la valeur inconnue.
constexpr std::size_t formater_entier_signe(int valeur, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite < 2)
        return 0;
    char chiffres[12] = {};
    int nb = 0;
    unsigned int v =
        valeur < 0 ? static_cast<unsigned int>(-static_cast<long long>(valeur)) : static_cast<unsigned int>(valeur);
    do {
        chiffres[nb++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v > 0 && nb < 11);
    std::size_t n = 0;
    const std::size_t besoin = static_cast<std::size_t>(nb) + (valeur < 0 ? 3 : 0) + 1;
    if (besoin > capacite) {
        sortie[0] = '\0';
        return 0;
    }
    if (valeur < 0) { // U+2212 = E2 88 92
        sortie[n++] = static_cast<char>(0xE2);
        sortie[n++] = static_cast<char>(0x88);
        sortie[n++] = static_cast<char>(0x92);
    }
    while (nb > 0)
        sortie[n++] = chiffres[--nb];
    sortie[n] = '\0';
    return n;
}

// Température en dixièmes -> « 12 », « −3 » (arrondie) ; rend 0 si inconnue.
constexpr std::size_t formater_temperature(int temperature_dix, char *sortie, std::size_t capacite) noexcept {
    if (!connu(temperature_dix)) {
        if (sortie != nullptr && capacite > 0)
            sortie[0] = '\0';
        return 0;
    }
    return formater_entier_signe(arrondi_dix(temperature_dix), sortie, capacite);
}

} // namespace ecran::visuel
