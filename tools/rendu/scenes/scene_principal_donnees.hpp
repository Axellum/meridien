// Données fixées des scènes de l'écran principal (scene_principal.cpp, scene_principal_themes.cpp) : des temps inventés, pas des
// mesures, assez variés pour montrer chaque état que l'écran sait dessiner (soleil, pluie, neige, orage, brouillard, nuit, chaleur,
// froid, textes longs, données absentes...). Le libellé du temps, le nom des jours et la date viennent des vraies tables du noyau
// pur dans la langue demandée au banc ; l'heure locale, du vrai fuseau (main/horloge.cpp).
//
// Les prévisions heure par heure, la tendance de la pression, le cumul de pluie et la « pluie dans N minutes » n'existent pas encore
// dans le client météo de l'appareil (docs/design/ : demande faite à la session météo) : la variante « donnees-minimales » montre
// l'écran avec ce que l'appareil reçoit AUJOURD'HUI, les autres avec les données complètes qu'il recevra.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "banc.h"
#include "ecran/visuel_vue.hpp"
#include "horloge.h"
#include "meteo_zone.h"
#include "ui_principal.h"

namespace donnees {

using ecran::visuel::INCONNU;
using ecran::visuel::INCONNU_DIX;
using ecran::visuel::VueMeteo;

// Un temps, en dixièmes (températures, vitesses, pression, UV) comme le noyau pur.
struct Cas {
    const char *ville;
    int code;
    int temp, ressenti, mini, maxi;
    int vent, rafales, direction;
    int humidite;
    int pression, tendance;
    int uv;
    int proba, pluie_mm, pluie_dans_min;
    int temp_moyenne_h, amplitude_h; // courbe des 24 heures : moyenne et demi-amplitude, pic à 15 h
    int codes_h[8];                  // un code par tranche de 3 heures
    int proba_h[8];
    int codes_j[7];
    int mini_j[7], maxi_j[7];
    int proba_j[7];
};

inline constexpr Cas SOLEIL = {"Rennes",
                               1,
                               142,
                               131,
                               62,
                               156,
                               142,
                               261,
                               225,
                               58,
                               10243,
                               12,
                               24,
                               5,
                               0,
                               -1,
                               100,
                               45,
                               {1, 1, 2, 2, 3, 3, 2, 1},
                               {0, 0, 0, 5, 5, 10, 5, 0},
                               {1, 2, 3, 61, 63, 3, 2},
                               {62, 70, 81, 92, 85, 60, 55},
                               {156, 148, 132, 120, 110, 125, 138},
                               {5, 15, 35, 70, 85, 30, 10}};
inline constexpr Cas PLUIE = {"Lorient",
                              63,
                              82,
                              61,
                              65,
                              98,
                              322,
                              553,
                              260,
                              91,
                              10081,
                              -38,
                              5,
                              85,
                              42,
                              0,
                              80,
                              20,
                              {63, 63, 61, 80, 63, 65, 61, 3},
                              {85, 90, 80, 70, 90, 95, 80, 40},
                              {63, 61, 80, 3, 61, 63, 2},
                              {65, 60, 55, 50, 62, 70, 58},
                              {98, 90, 85, 92, 100, 105, 96},
                              {90, 80, 60, 25, 70, 90, 15}};
inline constexpr Cas NEIGE = {"Grenoble",
                              73,
                              -26,
                              -70,
                              -52,
                              12,
                              121,
                              205,
                              45,
                              86,
                              10112,
                              -15,
                              3,
                              90,
                              18,
                              0,
                              -20,
                              40,
                              {73, 71, 71, 75, 73, 3, 2, 0},
                              {90, 85, 80, 95, 90, 20, 5, 0},
                              {73, 71, 3, 2, 0, 2, 3},
                              {-52, -60, -48, -35, -62, -40, -30},
                              {12, 5, 18, 40, 52, 38, 30},
                              {90, 70, 20, 5, 0, 10, 25}};
inline constexpr Cas ORAGE = {"Toulouse",
                              95,
                              241,
                              266,
                              188,
                              292,
                              242,
                              721,
                              135,
                              82,
                              10005,
                              -52,
                              40,
                              90,
                              86,
                              0,
                              240,
                              40,
                              {95, 96, 80, 81, 3, 2, 2, 1},
                              {90, 95, 80, 60, 30, 10, 5, 0},
                              {95, 80, 3, 2, 1, 61, 2},
                              {188, 180, 175, 168, 172, 185, 190},
                              {292, 270, 262, 275, 280, 268, 285},
                              {90, 65, 30, 10, 5, 40, 10}};
inline constexpr Cas BROUILLARD = {"Lille",
                                   45,
                                   31,
                                   22,
                                   18,
                                   62,
                                   54,
                                   90,
                                   90,
                                   98,
                                   10201,
                                   5,
                                   10,
                                   5,
                                   0,
                                   -1,
                                   40,
                                   25,
                                   {45, 45, 45, 3, 3, 3, 2, 2},
                                   {5, 5, 5, 0, 0, 5, 5, 0},
                                   {45, 3, 2, 3, 61, 63, 3},
                                   {18, 25, 31, 36, 42, 50, 38},
                                   {62, 70, 78, 85, 90, 86, 72},
                                   {5, 10, 5, 20, 60, 80, 30}};
inline constexpr Cas NUIT = {"Rennes",
                             0,
                             -26,
                             -52,
                             -40,
                             34,
                             31,
                             62,
                             0,
                             80,
                             10310,
                             25,
                             0,
                             0,
                             0,
                             -1,
                             0,
                             35,
                             {0, 0, 0, 1, 1, 2, 3, 1},
                             {0, 0, 0, 0, 0, 5, 10, 0},
                             {0, 1, 2, 3, 3, 2, 1},
                             {-40, -22, -10, 0, 12, 5, -3},
                             {34, 52, 64, 78, 85, 70, 58},
                             {0, 5, 10, 20, 25, 10, 5}};
inline constexpr Cas CHALEUR = {"Marseille",
                                0,
                                364,
                                391,
                                241,
                                389,
                                232,
                                411,
                                315,
                                28,
                                10095,
                                -8,
                                96,
                                0,
                                0,
                                -1,
                                320,
                                55,
                                {0, 0, 0, 0, 1, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 1, 0, 0, 2, 61},
                                {241, 250, 246, 238, 230, 226, 214},
                                {389, 402, 395, 380, 371, 340, 305},
                                {0, 0, 5, 0, 0, 20, 60}};
// Pires cas de largeur : ville très longue, température négative à deux chiffres, orage avec grêle (libellé le plus long).
inline constexpr Cas PIRES_CAS = {"Villeneuve-d'Ascq-Lez-Marcq-en-Bar\xC5\x93l",
                                  99,
                                  -124,
                                  -182,
                                  -152,
                                  -30,
                                  1034,
                                  1387,
                                  315,
                                  100,
                                  9787,
                                  -61,
                                  112,
                                  100,
                                  1234,
                                  0,
                                  -80,
                                  60,
                                  {99, 96, 95, 82, 86, 75, 67, 65},
                                  {100, 100, 95, 90, 80, 100, 100, 100},
                                  {99, 96, 86, 75, 67, 65, 82},
                                  {-152, -148, -160, -171, -143, -155, -166},
                                  {-30, -22, -28, -35, -20, -25, -31},
                                  {100, 95, 90, 100, 100, 100, 100}};

inline int64_t minuit_local(int64_t maintenant_s, int decalage_s) {
    const int64_t local = maintenant_s + decalage_s;
    return (local - ((local % 86400) + 86400) % 86400) - decalage_s;
}

// La vue complète d'un cas à l'instant `maintenant_s`. `avec_previsions` faux : ce que le client de l'appareil fournit aujourd'hui
// (pas d'heure par heure, pas de tendance, pas de cumul, pas de « pluie dans N minutes »).
inline VueMeteo vue(const Cas &c, int64_t maintenant_s, bool avec_previsions = true) {
    VueMeteo v;
    const int dec = ecran::horloge_decalage_s(maintenant_s);
    const int64_t minuit = minuit_local(maintenant_s, dec);
    const int heure_loc = static_cast<int>((maintenant_s - minuit) / 3600) % 24; // `minuit` est un instant UTC
    v.connue = true;
    v.instant_s = maintenant_s - 600;
    v.decalage_utc_s = dec;
    std::snprintf(v.ville, sizeof v.ville, "%s", c.ville);
    v.code = c.code;
    v.jour = heure_loc >= 7 && heure_loc < 17;
    v.temp_dix = c.temp;
    v.ressenti_dix = c.ressenti;
    v.min_dix = c.mini;
    v.max_dix = c.maxi;
    v.vent_dix_kmh = c.vent;
    v.rafales_dix_kmh = c.rafales;
    v.direction_deg = c.direction;
    v.humidite_pct = c.humidite;
    v.pression_dix_hpa = c.pression;
    v.uv_dix = c.uv;
    v.uv_est_max = true;
    v.proba_pluie_pct = c.proba;
    v.lever_s = minuit + 7 * 3600 + 12 * 60;
    v.coucher_s = minuit + 17 * 3600 + 10 * 60;
    if (avec_previsions) {
        v.tendance_dix_hpa_3h = c.tendance;
        v.pluie_dix_mm = c.pluie_mm;
        v.pluie_dans_min = c.pluie_dans_min;
        const int64_t heure_courante = maintenant_s - (maintenant_s % 3600);
        v.nb_heures = ecran::visuel::VUE_HEURES_MAX;
        for (int i = 0; i < v.nb_heures; ++i) {
            ecran::visuel::HeurePrevue &h = v.heures[i];
            h.instant_s = heure_courante + i * 3600LL;
            const int hl = static_cast<int>(((h.instant_s + dec) % 86400) / 3600);
            const double phase = (hl - 15) * 3.14159265358979 / 12.0;
            h.temp_dix = c.temp_moyenne_h + static_cast<int>(std::lround(c.amplitude_h * std::cos(phase)));
            h.code = c.codes_h[(i / 3) % 8];
            h.jour = hl >= 7 && hl < 17;
            h.proba_pct = c.proba_h[(i / 3) % 8];
            h.pluie_dix_mm = h.proba_pct / 10;
        }
        // la température de la première colonne est celle du moment
        v.heures[0].temp_dix = c.temp;
    }
    v.nb_jours = ecran::visuel::VUE_JOURS_MAX;
    for (int i = 0; i < v.nb_jours; ++i) {
        ecran::visuel::JourPrevu &j = v.jours[i];
        j.date_s = minuit + i * 86400LL;
        j.code = c.codes_j[i];
        j.min_dix = c.mini_j[i];
        j.max_dix = c.maxi_j[i];
        j.proba_pct = c.proba_j[i];
        j.pluie_dix_mm = j.proba_pct / 4;
        j.uv_dix = c.uv;
        // Le vent maximal et sa direction dominante, inventés à partir du temps du jour : varient d'un jour à l'autre.
        j.vent_dix_kmh = c.rafales * (7 + (i * 5) % 6) / 10;
        j.direction_deg = (c.direction + 20 * i) % 360;
    }
    v.jours[0].min_dix = c.mini;
    v.jours[0].max_dix = c.maxi;
    return v;
}

// La scène de l'esquisse « croquis aéré » (00ProjetTab/.claude/esquisses-accueil-meteo, lib.js DONNEES) : Lorient, un dimanche
// 08:45 en novembre, éclaircies, pluie dès 13 h (6,5 mm), vent violent annoncé de 14 h à 22 h. Les heures sont indexées depuis
// l'heure courante, les valeurs sont celles de l'esquisse. À rendre avec -Instant 2026-11-08T07:45:00Z (08 h 45 en France).
inline VueMeteo vue_esquisse(int64_t maintenant_s) {
    static const int temp[24] = {10, 11, 12, 13, 14, 14, 13, 12, 12, 11, 10, 10, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 7, 8};
    static const int pluie_dix[24] = {0, 0, 0, 0, 0, 4, 16, 31, 12, 2, 0, 0, 0, 0, 5, 14, 9, 0, 0, 0, 0, 0, 0, 0};
    // peuNuageux 2, nuage 3, pluie 61, averse 81 (nuitNuageuse : 2 ou 3 avec jour faux)
    static const int code[24] = {2, 2, 3, 3, 3, 61, 61, 81, 61, 3, 2, 2, 3, 3, 61, 61, 61, 3, 2, 2, 2, 3, 2, 2};
    static const int jours_code[7] = {61, 81, 2, 0, 3, 61, 2};
    static const int jours_max[7] = {14, 12, 13, 14, 13, 11, 12};
    static const int jours_min[7] = {8, 7, 6, 5, 7, 8, 6};
    static const int jours_pct[7] = {70, 80, 20, 5, 30, 75, 15};
    static const int jours_mm_dix[7] = {93, 51, 4, 0, 8, 42, 2};
    static const int jours_vent[7] = {38, 40, 26, 18, 22, 36, 20};
    static const int jours_dir[7] = {270, 285, 300, 330, 350, 20, 40};
    VueMeteo v;
    const int dec = ecran::horloge_decalage_s(maintenant_s);
    const int64_t minuit = minuit_local(maintenant_s, dec);
    v.connue = true;
    v.exemple = false;
    v.instant_s = maintenant_s - 600;
    v.decalage_utc_s = dec;
    std::snprintf(v.ville, sizeof v.ville, "Lorient");
    v.code = 2;
    v.jour = true;
    v.temp_dix = 110;
    v.ressenti_dix = 90;
    v.min_dix = 80;
    v.max_dix = 140;
    v.vent_dix_kmh = 220;
    v.rafales_dix_kmh = 380;
    v.direction_deg = 240;
    v.humidite_pct = 78;
    v.pression_dix_hpa = 10110;
    v.tendance_dix_hpa_3h = -20;
    v.uv_dix = 10;
    v.uv_est_max = true;
    v.proba_pluie_pct = 70;
    v.pluie_dix_mm = 65;
    v.pluie_dans_min = -1; // rien dans les deux heures qui viennent : la pluie de 13 h vient des cumuls horaires
    v.lever_s = minuit + 7 * 3600 + 58 * 60;
    v.coucher_s = minuit + 17 * 3600 + 35 * 60;
    const int64_t heure_courante = maintenant_s - (maintenant_s % 3600);
    v.nb_heures = ecran::visuel::VUE_HEURES_MAX;
    for (int i = 0; i < v.nb_heures; ++i) {
        ecran::visuel::HeurePrevue &h = v.heures[i];
        h.instant_s = heure_courante + i * 3600LL;
        const int hl = static_cast<int>(((h.instant_s + dec) % 86400) / 3600);
        h.temp_dix = temp[i] * 10;
        h.code = code[i];
        h.jour = hl >= 8 && hl < 18;
        h.proba_pct = pluie_dix[i] > 0 ? 80 : 5;
        h.pluie_dix_mm = pluie_dix[i];
    }
    v.nb_jours = 7;
    for (int i = 0; i < v.nb_jours; ++i) {
        ecran::visuel::JourPrevu &j = v.jours[i];
        j.date_s = minuit + i * 86400LL;
        j.code = jours_code[i];
        j.min_dix = jours_min[i] * 10;
        j.max_dix = jours_max[i] * 10;
        j.proba_pct = jours_pct[i];
        j.pluie_dix_mm = jours_mm_dix[i];
        j.uv_dix = 10;
        j.vent_dix_kmh = jours_vent[i] * 10;
        j.direction_deg = jours_dir[i];
    }
    return v;
}

// Plus une goutte à l'horizon : « Rien à signaler dans les prochaines heures ».
inline VueMeteo sans_pluie(VueMeteo v) {
    v.pluie_dix_mm = 0;
    v.pluie_dans_min = -1;
    v.proba_pluie_pct = 0;
    for (int i = 0; i < v.nb_heures; ++i) {
        v.heures[i].pluie_dix_mm = 0;
        v.heures[i].proba_pct = 0;
        if (v.heures[i].code >= 51) {
            v.heures[i].code = 3; // couvert plutôt que pluvieux
        }
    }
    // Aujourd'hui et demain s'adoucissent (comme l'esquisse « calme »).
    for (int i = 0; i < 2 && i < v.nb_jours; ++i) {
        v.jours[i].code = i == 0 ? 2 : 3;
        v.jours[i].pluie_dix_mm = i == 0 ? 0 : 6;
        v.jours[i].proba_pct = i == 0 ? 5 : 20;
        v.jours[i].vent_dix_kmh = i == 0 ? 200 : 240;
    }
    return v;
}

// L'alerte officielle de Vigie, jaune, « Vent violent » de 14 h à 22 h le jour de `maintenant_s` (heures de France, hiver : UTC + 1).
inline ecran::vigie::Bandeau alerte_vent(int64_t maintenant_s, int autres = 0) {
    ecran::vigie::Bandeau b;
    b.present = true;
    b.niveau = ecran::vigie::Niveau::Jaune;
    b.autres = static_cast<std::uint8_t>(autres);
    std::snprintf(b.texte, sizeof b.texte, "Vent violent");
    const int64_t minuit = minuit_local(maintenant_s, ecran::horloge_decalage_s(maintenant_s));
    b.debut_s = minuit + 14 * 3600;
    b.fin_s = minuit + 22 * 3600;
    b.empreinte = 0x5eed0001u + static_cast<std::uint32_t>(autres);
    return b;
}

inline ecran::EntreesPrincipal entrees(const banc::Contexte &contexte, int64_t decalage_instant_s,
                                       const VueMeteo &meteo, bool heure_connue = true) {
    static char date[64];
    const int64_t maintenant = contexte.instant_utc_s + decalage_instant_s;
    ecran::EntreesPrincipal e = {};
    date[0] = '\0';
    e.heure_connue = heure_connue && ecran::horloge_hhmm(maintenant, e.heure, sizeof e.heure) &&
                     ecran::horloge_date_localisee(maintenant, contexte.langue, date, sizeof date);
    e.date = date;
    e.date_attente = nullptr; // la phrase d'attente de l'écran lui-même
    e.maintenant_s = maintenant;
    // Le lieu, comme le branchement de l'appareil le fait (main/ecran_principal.cpp) : la zone posée par le banc (Lyon par défaut).
    ZoneMeteo zone = {};
    e.fuseau_connu = ecran::horloge_fuseau(e.fuseau);
    e.position_connue = zone_lire(&zone);
    e.latitude_deg = zone.latitude;
    e.longitude_deg = zone.longitude;
    e.meteo = meteo;
    return e;
}

} // namespace donnees
