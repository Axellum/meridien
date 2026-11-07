// Outils communs de test_meteo_metno.cpp et test_meteo_nws.cpp : la météo lue en une liste de nombres, comparée à la liste attendue
// que test/host/controle_python_fournisseurs.py (lecture_fournisseurs.py) a calculée en relisant la réponse sans rien du noyau.
//
// Chaque ligne attendue est {valeur, tolérance} : 0 pour l'égalité exacte ; une tolérance non nulle seulement pour ce qui dépend du
// calcul du soleil (lever, coucher, indicateur « jour » à moins de cinq minutes d'un lever ou d'un coucher), que le noyau (NOAA) et
// le contrôle (US Naval Observatory) ne calculent pas à la seconde près.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/meteo_riche.hpp"

namespace fournisseurs_test {

using namespace ecran::meteo;
using sv = std::string_view;

struct Plat {
    long long v[600] = {};
    std::size_t n = 0;
    bool trop = false;
    constexpr void ajouter(long long x) noexcept {
        if (n < 600)
            v[n++] = x;
        else
            trop = true;
    }
};

// Dans l'ORDRE de `vers_plat` de lecture_fournisseurs.py.
constexpr Plat aplatir(const Meteo &m) noexcept {
    Plat p;
    const Releve &r = m.releve;
    p.ajouter(r.instant_s);
    p.ajouter(r.decalage_utc_s);
    p.ajouter(r.temperature_dixiemes);
    p.ajouter(r.ressentie_dixiemes);
    p.ajouter(r.code_wmo);
    p.ajouter(r.jour ? 1 : 0);
    p.ajouter(r.vent_dixiemes_kmh);
    p.ajouter(r.rafales_dixiemes_kmh);
    p.ajouter(r.direction_vent_deg);
    p.ajouter(r.pression_dixiemes_hpa);
    p.ajouter(r.humidite_pct);
    p.ajouter(r.proba_precip_pct);
    p.ajouter(r.nb_jours);
    for (std::size_t i = 0; i < r.nb_jours; ++i) {
        const Jour &j = r.jours[i];
        p.ajouter(j.date_s);
        p.ajouter(j.code_wmo);
        p.ajouter(j.min_dixiemes);
        p.ajouter(j.max_dixiemes);
        p.ajouter(j.lever_s);
        p.ajouter(j.coucher_s);
        p.ajouter(j.uv_max_dixiemes);
        p.ajouter(j.proba_precip_max);
    }
    const Riche &q = m.riche;
    p.ajouter(q.precip_courante_dixiemes);
    p.ajouter(q.nuages_pct);
    p.ajouter(q.nb_heures);
    for (std::size_t i = 0; i < q.nb_heures; ++i) {
        const HeureRiche &h = q.heures[i];
        p.ajouter(h.debut_s);
        p.ajouter(h.temperature_dixiemes);
        p.ajouter(h.ressentie_dixiemes);
        p.ajouter(h.humidite_pct);
        p.ajouter(h.proba_pct);
        p.ajouter(h.precip_dixiemes_mm);
        p.ajouter(h.code_wmo);
        p.ajouter(h.nuages_pct);
        p.ajouter(h.vent_dixiemes_kmh);
        p.ajouter(h.rafales_dixiemes_kmh);
        p.ajouter(h.direction_deg);
        p.ajouter(h.uv_dixiemes);
        p.ajouter(h.jour ? 1 : 0);
    }
    p.ajouter(q.nb_jours);
    for (std::size_t i = 0; i < q.nb_jours; ++i) {
        const JourRiche &j = q.jours[i];
        p.ajouter(j.ressentie_min_dixiemes);
        p.ajouter(j.ressentie_max_dixiemes);
        p.ajouter(j.ensoleillement_min);
        p.ajouter(j.precip_dixiemes_mm);
        p.ajouter(j.neige_dixiemes_cm);
        p.ajouter(j.heures_precip);
        p.ajouter(j.vent_max_dixiemes_kmh);
        p.ajouter(j.rafales_max_dixiemes_kmh);
        p.ajouter(j.direction_dominante_deg);
    }
    p.ajouter(q.nb_quarts);
    return p;
}

constexpr long long ecart(long long a, long long b) noexcept { return a > b ? a - b : b - a; }

// Rend 0 si tout concorde, sinon 10000 + le rang de la première valeur fausse (voir le fichier attendu) ou 1 à 6 pour une section
// refusée. La section des quarts d'heure doit être vide ET dire pourquoi (ni MET Norway ni le NWS ne la donne).
template <std::size_t N> constexpr int comparer(const AnalyseRiche &a, const long long (&attendu)[N][2]) noexcept {
    if (!a.ok())
        return 1;
    if (!a.diag_heures.ok())
        return 2;
    if (!a.diag_jours.ok())
        return 3;
    if (a.diag_quarts.erreur != Erreur::ChampAbsent)
        return 4;
    const Plat p = aplatir(a.meteo);
    if (p.trop || p.n != N)
        return 5;
    for (std::size_t i = 0; i < N; ++i) {
        if (ecart(p.v[i], attendu[i][0]) > attendu[i][1])
            return 10000 + static_cast<int>(i);
    }
    return verifier_meteo(a.meteo).ok() ? 0 : 6;
}

// Les réponses réelles (jusqu'à 240 Ko) ne tiennent pas dans un seul littéral (-Woverlength-strings : 65536 caractères) : les
// fichiers donnees/*.inc sont des listes de morceaux, recollés ici à la compilation. Usage :
//   constexpr sv P_OSLO[] = {
//   #include "donnees/metno_oslo.inc"
//   };
//   constexpr auto J_OSLO = joindre<longueur_totale(P_OSLO)>(P_OSLO);
//   constexpr sv OSLO = J_OSLO.vue();
template <std::size_t K> constexpr std::size_t longueur_totale(const sv (&morceaux)[K]) noexcept {
    std::size_t n = 0;
    for (const sv &m : morceaux)
        n += m.size();
    return n;
}

template <std::size_t N> struct Joint {
    char d[N + 1] = {};
    constexpr sv vue() const noexcept { return sv(d, N); }
};

template <std::size_t N, std::size_t K> constexpr Joint<N> joindre(const sv (&morceaux)[K]) noexcept {
    Joint<N> j;
    std::size_t n = 0;
    for (const sv &m : morceaux) {
        for (const char c : m)
            j.d[n++] = c;
    }
    return j;
}

// Un texte modifiable à la compilation : sert à fabriquer de petites réponses de toutes pièces.
template <std::size_t CAP> struct Texte {
    char d[CAP] = {};
    std::size_t n = 0;
    bool trop = false;
    constexpr Texte &operator+=(sv s) noexcept {
        for (const char c : s) {
            if (n + 1 >= CAP) {
                trop = true;
                return *this;
            }
            d[n++] = c;
        }
        return *this;
    }
    constexpr sv vue() const noexcept { return sv(d, n); }
};

} // namespace fournisseurs_test
