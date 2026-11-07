// Noyau pur : de la météo du noyau (`Meteo` : entiers en dixièmes, instants UNIX) à la structure du CONTRAT `MeteoDetail`
// (main/meteo_etat.h : `float` en unités naturelles, indices recalculés pour l'heure de l'appareil). C++17 standard, aucun
// include ESP-IDF.
//
// Pourquoi un gabarit (`template <class D>`) : le contrat vit dans main/ (c'est l'interface avec l'écran), le noyau ne doit pas
// en dépendre. `D` est `::MeteoDetail` ; si un champ du contrat change de nom ou de type, la compilation du test et du
// firmware échoue ici, pas à l'exécution. Le test (test_meteo_detail.cpp) fait le lien entre les deux.
//
// Ce que fait la conversion, et ne fait pas :
//  - aucune valeur n'est inventée : une section absente du noyau (heures, quarts d'heure, compléments des jours) donne `nb_… = 0` et
//    l'indice -1 ; les jours ne sont publiés que si TOUS ont leurs compléments (un jour sans ensoleillement ni vent ne serait pas
//    un jour sur lequel dessiner) ;
//  - les indices (heure en cours, jour en cours, quart d'heure en cours) et la pluie des prochaines minutes sont recalculés pour
//    `maintenant_s`, l'heure de l'appareil ; horloge non réglée (avant l'an 2000) : aucune plage de la réponse ne la contient, tous
//    les indices valent -1 et la pluie proche n'est pas « connue » (aucun cas particulier : les fonctions du noyau le disent) ;
//  - une valeur que la source ne donne pas (INCONNU_DIX, INCONNU_ENTIER : MET Norway, NWS) sort en NaN pour les flottants et en -1
//    pour les entiers, jamais en 0 ; l'écran les cache déjà (main/vue_depuis_detail.h) ;
//  - les `float` sont exacts au dixième : un dixième entier divisé par 10,0F est l'arrondi correct du nombre décimal (la division
//    IEEE est correctement arrondie, comme le littéral 12,3F) ; seule exception, `ensoleillement_h`, à la MINUTE près (8,55 h) ;
//  - `tendance_pression_3h_hpa` vient des pressions que l'appareil a lui-même mesurées (meteo_tendance.hpp) : NaN tant qu'il n'a
//    pas de mesure d'il y a 2 h 30 à 3 h 30.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "ecran/meteo_riche.hpp"
#include "ecran/meteo_synthese.hpp"
#include "ecran/meteo_tendance.hpp"
#include "ecran/meteo_wmo.hpp"

namespace ecran::meteo {

namespace detail {

// Dixièmes -> flottant. `INCONNU_DIX` (la source ne donne pas la valeur : MET Norway, NWS) devient NaN, que l'écran sait cacher.
constexpr float vers_float(int dixiemes) noexcept {
    return dixiemes == INCONNU_DIX ? std::numeric_limits<float>::quiet_NaN() : static_cast<float>(dixiemes) / 10.0F;
}

// Texte terminé par un zéro dans un tableau du contrat. Un texte trop long serait coupé (zéro final garanti, jamais de
// débordement) ; ça n'arrive pas : le test prouve que les 100 codes WMO donnent un libellé qui tient dans `libelle[40]`, et
// `fuseau[48]` a la taille de `Releve::fuseau`.
template <std::size_t N> constexpr void copier_texte(std::string_view source, char (&dest)[N]) noexcept {
    const std::size_t n = source.size() < N - 1 ? source.size() : N - 1;
    for (std::size_t i = 0; i < n; ++i)
        dest[i] = source[i];
    dest[n] = '\0';
}

// Durée en secondes -> heures, un chiffre après la virgule, à la plus proche (en dixièmes d'heure). Négative ou nulle : 0.
constexpr int dixiemes_d_heure_depuis_secondes(std::int64_t secondes) noexcept {
    return secondes <= 0 ? 0 : static_cast<int>((secondes * 10 + 1800) / 3600);
}
// Minutes -> heures, SANS arrondi au dixième : le noyau garde l'ensoleillement à la minute (il est déjà arrondi à la minute,
// l'arrondir encore au dixième d'heure fausserait de 0,1 h environ 4 jours sur 100). 513 min -> 8,55 h.
constexpr float heures_depuis_minutes(int minutes) noexcept {
    return minutes == INCONNU_ENTIER ? std::numeric_limits<float>::quiet_NaN() : static_cast<float>(minutes) / 60.0F;
}

} // namespace detail

// Remplit `d` (type `MeteoDetail` du contrat) depuis `m`, pour l'heure `maintenant_s` de l'appareil et `echecs` essais ratés de
// suite. `version` est le numéro de publication (que l'état de l'appareil connaît) ; `histo` les pressions déjà mesurées (pour la
// tendance sur 3 h, voir meteo_tendance.hpp) ; tout le reste de `d` est refait à neuf, rien de son contenu précédent n'a besoin
// d'être valide.
template <class D>
constexpr void remplir_detail(const Meteo &m, std::uint32_t version, std::int64_t maintenant_s, int echecs,
                              const HistoPression &histo, D &d) noexcept {
    d = D{};
    d.version = version;
    d.jour_courant = -1; // les autres indices sont calculés plus bas dans tous les cas

    const Releve &r = m.releve;
    const Riche &q = m.riche;

    d.valide = true;
    d.perime = est_perime(r, maintenant_s, echecs);
    d.maj_epoch_s = r.recu_a_s;
    d.decalage_utc_s = r.decalage_utc_s;
    {
        std::size_t n = 0;
        while (n < FUSEAU_MAX && r.fuseau[n] != '\0')
            ++n;
        detail::copier_texte(std::string_view(r.fuseau, n), d.fuseau);
    }

    // ---- courant ----
    d.courant.instant_epoch_s = r.instant_s;
    d.courant.temperature_c = detail::vers_float(r.temperature_dixiemes);
    d.courant.ressentie_c = detail::vers_float(r.ressentie_dixiemes);
    d.courant.humidite_pct = r.humidite_pct;
    d.courant.jour = r.jour;
    d.courant.precipitation_mm = detail::vers_float(q.precip_courante_dixiemes);
    d.courant.code_wmo = r.code_wmo;
    d.courant.nuages_pct = q.nuages_pct;
    d.courant.pression_hpa = detail::vers_float(r.pression_dixiemes_hpa);
    {
        // Sans pression (NWS n'en donne pas), pas de tendance : jamais la différence avec une valeur « inconnue ».
        const Tendance t = r.pression_dixiemes_hpa == INCONNU_DIX
                               ? Tendance{}
                               : tendance_3h(histo, r.instant_s, r.pression_dixiemes_hpa);
        d.courant.tendance_pression_3h_hpa =
            t.connue ? detail::vers_float(t.dixiemes_hpa) : std::numeric_limits<float>::quiet_NaN();
    }
    d.courant.vent_kmh = detail::vers_float(r.vent_dixiemes_kmh);
    d.courant.direction_vent_deg = r.direction_vent_deg;
    d.courant.rafales_kmh = detail::vers_float(r.rafales_dixiemes_kmh);
    d.courant.proba_precip_pct = static_cast<decltype(d.courant.proba_precip_pct)>(proba_courante(q, maintenant_s));
    detail::copier_texte(decrire(r.code_wmo, r.jour).libelle, d.courant.libelle);

    // ---- heure par heure ----
    d.nb_heures = q.nb_heures;
    d.heure_courante = heure_courante(q, maintenant_s);
    for (std::size_t i = 0; i < q.nb_heures; ++i) {
        const HeureRiche &h = q.heures[i];
        auto &s = d.heures[i];
        s.debut_epoch_s = h.debut_s;
        s.temperature_c = detail::vers_float(h.temperature_dixiemes);
        s.ressentie_c = detail::vers_float(h.ressentie_dixiemes);
        s.humidite_pct = h.humidite_pct;
        s.proba_precip_pct = h.proba_pct;
        s.precipitation_mm = detail::vers_float(h.precip_dixiemes_mm);
        s.code_wmo = h.code_wmo;
        s.nuages_pct = h.nuages_pct;
        s.vent_kmh = detail::vers_float(h.vent_dixiemes_kmh);
        s.rafales_kmh = detail::vers_float(h.rafales_dixiemes_kmh);
        s.direction_vent_deg = h.direction_deg;
        s.uv = detail::vers_float(h.uv_dixiemes);
        s.jour = h.jour;
    }

    // ---- jours : seulement si tous ont leurs compléments ----
    if (q.nb_jours == r.nb_jours) { // sans jour de prévision : 0 jour, `jour_courant` reste à -1
        d.nb_jours = r.nb_jours;
        d.jour_courant = index_du_jour(r, maintenant_s);
        for (std::size_t i = 0; i < r.nb_jours; ++i) {
            const Jour &j = r.jours[i];
            const JourRiche &c = q.jours[i];
            auto &s = d.jours[i];
            s.date_epoch_s = j.date_s;
            s.code_wmo = j.code_wmo;
            s.min_c = detail::vers_float(j.min_dixiemes);
            s.max_c = detail::vers_float(j.max_dixiemes);
            s.ressentie_min_c = detail::vers_float(c.ressentie_min_dixiemes);
            s.ressentie_max_c = detail::vers_float(c.ressentie_max_dixiemes);
            s.lever_epoch_s = j.lever_s;
            s.coucher_epoch_s = j.coucher_s;
            s.sans_soleil = j.sans_soleil();
            s.duree_jour_h = detail::vers_float(detail::dixiemes_d_heure_depuis_secondes(j.coucher_s - j.lever_s));
            s.ensoleillement_h = detail::heures_depuis_minutes(c.ensoleillement_min);
            s.uv_max = detail::vers_float(j.uv_max_dixiemes);
            s.proba_precip_pct = j.proba_precip_max;
            s.precipitation_mm = detail::vers_float(c.precip_dixiemes_mm);
            s.neige_cm = detail::vers_float(c.neige_dixiemes_cm);
            s.heures_precipitation = c.heures_precip;
            s.vent_max_kmh = detail::vers_float(c.vent_max_dixiemes_kmh);
            s.rafales_max_kmh = detail::vers_float(c.rafales_max_dixiemes_kmh);
            s.direction_vent_dominante_deg = c.direction_dominante_deg;
        }
    }

    // ---- pluie par quarts d'heure et pluie des prochaines minutes ----
    d.nb_quarts = q.nb_quarts;
    d.quarts_debut_epoch_s = q.nb_quarts > 0 ? q.quarts_debut_s : 0;
    d.quart_courant = quart_courant(q, maintenant_s);
    for (std::size_t i = 0; i < q.nb_quarts; ++i)
        d.quarts_precipitation_mm[i] = detail::vers_float(q.quarts_dixiemes[i]);
    const PluieProche p = pluie_proche(q, r.recu_a_s, maintenant_s);
    d.pluie.connue = p.connue;
    d.pluie.perimee = p.perimee;
    d.pluie.en_cours = p.en_cours;
    d.pluie.dans_min = p.dans_min;
    d.pluie.fin_dans_min = p.fin_dans_min;
    d.pluie.fenetre_min = p.fenetre_min;
    d.pluie.cumul_mm = detail::vers_float(p.cumul_dixiemes);
    d.pluie.max_quart_mm = detail::vers_float(p.max_quart_dixiemes);
}

} // namespace ecran::meteo
