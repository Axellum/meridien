// Outils communs aux tests du noyau « surf » (test_surf_*.cpp) : un texte modifiable à la compilation pour casser UNE chose dans une
// vraie réponse, et des fabriques de données synthétiques (une mer et un vent dont chaque heure est choisie). C++17 standard.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/surf_note.hpp"

namespace surf_test {

using namespace ecran::surf;
using sv = std::string_view;

// Minuit UTC d'un jour quelconque (2026-11-05 00:00:00 UTC) : tous les créneaux synthétiques partent de là, pour raisonner en
// « heure locale = heure UTC + décalage » sans calendrier.
inline constexpr std::int64_t MINUIT = 1793836800;
static_assert(MINUIT % 86400 == 0, "MINUIT doit être un minuit UTC");
inline constexpr std::int64_t HEURE = 3600;

// ---- un texte modifiable ------------------------------------------------------------------------------------------------

struct Texte {
    char d[6000] = {};
    std::size_t n = 0;
    constexpr sv vue() const noexcept { return sv(d, n); }
};

// Copie `source` en remplaçant la `rang`-ième occurrence (de 0) de `ancien` par `nouveau`. n == 0 : occurrence introuvable ou
// résultat trop grand (le test qui s'en sert doit le refuser, jamais le laisser passer en silence).
constexpr Texte remplacer(sv source, sv ancien, sv nouveau, int rang = 0) noexcept {
    Texte t;
    std::size_t pos = 0;
    for (int i = 0; i <= rang; ++i) {
        pos = source.find(ancien, i == 0 ? 0 : pos + 1);
        if (pos == sv::npos)
            return t;
    }
    if (source.size() - ancien.size() + nouveau.size() > sizeof(t.d))
        return t;
    std::size_t n = 0;
    for (std::size_t i = 0; i < pos; ++i)
        t.d[n++] = source[i];
    for (const char c : nouveau)
        t.d[n++] = c;
    for (std::size_t i = pos + ancien.size(); i < source.size(); ++i)
        t.d[n++] = source[i];
    t.n = n;
    return t;
}

// ---- données synthétiques -----------------------------------------------------------------------------------------------

// `n` heures consécutives à partir de `debut_s`, toutes les grandeurs ABSENT (aucune mer).
constexpr Marine mer_vide(std::size_t n, std::int64_t debut_s = MINUIT, std::int32_t decalage = 0) noexcept {
    Marine m{};
    m.decalage_utc_s = decalage;
    m.nb = static_cast<std::uint8_t>(n);
    for (std::size_t i = 0; i < n; ++i)
        m.heures[i].debut_s = debut_s + static_cast<std::int64_t>(i) * HEURE;
    return m;
}

// Règle la houle de l'heure `i` (hauteur, période, direction) et marque les drapeaux de la réponse.
constexpr void regler_houle(Marine &m, std::size_t i, int hauteur_cm, int periode_ds, int direction = 270) noexcept {
    m.heures[i].houle_cm = static_cast<std::int16_t>(hauteur_cm);
    m.heures[i].houle_periode_ds = static_cast<std::int16_t>(periode_ds);
    m.heures[i].houle_direction_deg = static_cast<std::int16_t>(direction);
    m.heures[i].vagues_cm = static_cast<std::int16_t>(hauteur_cm);
    m.heures[i].vagues_periode_ds = static_cast<std::int16_t>(periode_ds);
    m.a_houle = true;
    m.a_vagues = true;
}

// Mer avec la même houle toutes les heures.
constexpr Marine mer_uniforme(std::size_t n, int hauteur_cm, int periode_ds, std::int64_t debut_s = MINUIT,
                              std::int32_t decalage = 0) noexcept {
    Marine m = mer_vide(n, debut_s, decalage);
    for (std::size_t i = 0; i < n; ++i)
        regler_houle(m, i, hauteur_cm, periode_ds);
    return m;
}

// Vent identique toutes les heures.
constexpr Vent vent_uniforme(std::size_t n, int vitesse, int rafale, int direction, std::int64_t debut_s = MINUIT,
                             std::int32_t decalage = 0) noexcept {
    Vent v{};
    v.decalage_utc_s = decalage;
    v.nb = static_cast<std::uint8_t>(n);
    for (std::size_t i = 0; i < n; ++i) {
        v.heures[i].debut_s = debut_s + static_cast<std::int64_t>(i) * HEURE;
        v.heures[i].vitesse_dixiemes_kmh = static_cast<std::int16_t>(vitesse);
        v.heures[i].rafale_dixiemes_kmh = static_cast<std::int16_t>(rafale);
        v.heures[i].direction_deg = static_cast<std::int16_t>(direction);
    }
    return v;
}

// ---- réponses JSON fabriquées et refus attendus -------------------------------------------------------------------------------

struct Json {
    char d[4000] = {};
    std::size_t n = 0;
    constexpr void texte(sv t) noexcept {
        for (const char c : t)
            d[n++] = c;
    }
    constexpr void nombre(long long v) noexcept {
        if (v < 0) {
            d[n++] = '-';
            v = -v;
        }
        char chiffres[24] = {};
        int k = 0;
        do {
            chiffres[k++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v > 0);
        while (k > 0)
            d[n++] = chiffres[--k];
    }
    constexpr sv vue() const noexcept { return sv(d, n); }
};

// {"utc_offset_seconds":<decalage>,"hourly":{"time":[<nb heures espacées de pas>]}} ; avec_vagues ajoute wave_height = 0,50 m partout.
constexpr Json reponse_minimale(int nb, long long pas = 3600, long long decalage = 0,
                                bool avec_vagues = false) noexcept {
    Json j;
    j.texte("{\"utc_offset_seconds\":");
    j.nombre(decalage);
    j.texte(",\"hourly\":{\"time\":[");
    for (int i = 0; i < nb; ++i) {
        if (i > 0)
            j.texte(",");
        j.nombre(1794031200LL + i * pas);
    }
    j.texte("]");
    if (avec_vagues) {
        j.texte(",\"wave_height\":[");
        for (int i = 0; i < nb; ++i)
            j.texte(i > 0 ? ",0.5" : "0.5");
        j.texte("]");
    }
    j.texte("}}");
    return j;
}

// Une réponse de `nb` heures dont les séries (`séries` : "\"wave_height\":[...],\"wave_period\":[...]") et les unités (`unites` :
// "\"wave_height\":\"m\",...", vide si on n'en veut pas) sont écrites à la main.
constexpr Json reponse_avec(int nb, sv series, sv unites = {}) noexcept {
    Json j;
    j.texte("{\"utc_offset_seconds\":0,");
    if (!unites.empty()) {
        j.texte("\"hourly_units\":{\"time\":\"unixtime\",");
        j.texte(unites);
        j.texte("},");
    }
    j.texte("\"hourly\":{\"time\":[");
    for (int i = 0; i < nb; ++i) {
        if (i > 0)
            j.texte(",");
        j.nombre(1794031200LL + i * 3600LL);
    }
    j.texte("],");
    j.texte(series);
    j.texte("}}");
    return j;
}

constexpr bool refuse(sv texte, Erreur attendue, sv champ) noexcept {
    const AnalyseMarine a = analyser_marine(texte);
    return !a.ok() && a.diag.erreur == attendue && a.diag.champ == champ;
}
constexpr bool refuse_vent(sv texte, Erreur attendue, sv champ) noexcept {
    const AnalyseVent a = analyser_vent(texte);
    return !a.ok() && a.diag.erreur == attendue && a.diag.champ == champ;
}

// Un spot valide de test : plage tournée vers l'ouest, fourchette par défaut (1,00 m à 2,50 m).
constexpr Spot spot_ouest() noexcept {
    Spot s{};
    const char nom[] = "Spot de test";
    for (std::size_t i = 0; i < sizeof(nom); ++i)
        s.nom[i] = nom[i];
    s.latitude_e5 = 4366760;
    s.longitude_e5 = -144500;
    s.orientation_deg = 270;
    return s;
}

} // namespace surf_test
