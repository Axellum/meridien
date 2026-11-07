// Noyau pur : l'heure LOCALE DU LIEU, instant par instant, pour l'écran principal (ADR-0012).
//
// Pourquoi un fichier à part : avant, l'écran convertissait toute la fenêtre de prévision avec UN décalage (celui de la réponse
// météo, pris à la réception). C'est faux dès que la fenêtre contient un changement d'heure (New York le 1er novembre, Sydney le
// 4 octobre) et pour les fuseaux dont les minutes ne sont pas nulles (Kolkata +5:30, Katmandou +5:45, Lord Howe +10:30, Chatham
// +12:45), dont les colonnes tombent à :30 ou :45. Ici, chaque instant est converti avec les règles du fuseau (heure_fr.hpp,
// fuseaux.hpp) : son décalage est celui en vigueur à CET instant.
//
// Contenu : le fuseau à décalage fixe (repli quand le nom du fuseau n'est pas connu), le jour local d'un instant, le jour de la
// semaine d'un jour de prévision, la clé d'un fuseau (pour ne redessiner que ce qui change) et les deux formats d'heure de l'écran
// (étiquette d'une colonne « heure par heure », « hh:mm » du lever et du coucher du soleil).
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation (testé sur le PC, test_heure_lieu.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/heure_fr.hpp"

namespace ecran::heure {

// Fuseau à décalage fixe, sans heure d'été : le repli quand le fuseau du lieu n'a pas pu être lu (on garde alors le décalage de la
// réponse météo ou, à défaut, celui de l'horloge). `decalage_s` est positif à l'est.
constexpr Fuseau fuseau_fixe(std::int32_t decalage_s) noexcept {
    Fuseau f;
    f.decalage_std_s = decalage_s;
    f.decalage_ete_s = decalage_s;
    f.a_heure_ete = false;
    return f;
}

// Numéro du jour LOCAL d'un instant (jours écoulés depuis le 1970-01-01 local). Deux instants ont le même numéro s'ils tombent
// le même jour civil sur place, même quand ce jour dure 23 ou 25 heures. Sert à « aujourd'hui » et au changement de jour.
constexpr std::int64_t numero_jour_local(std::int64_t instant_s, const Fuseau &f) noexcept {
    const HeureLocale h = en_heure_locale(instant_s, f);
    return detail::jours_depuis_epoch(h.an, h.mois, h.jour);
}

// Jour de la semaine (0 dimanche … 6 samedi) d'un jour de prévision dont `date_s` est le minuit local. On lit le MIDI local du jour
// (minuit + 12 h) : un changement d'heure d'une heure ou d'une demi-heure ne le sort jamais du jour, à la différence du minuit.
constexpr int jour_semaine_du_jour(std::int64_t date_s, const Fuseau &f) noexcept {
    return en_heure_locale(date_s + 12 * 3600, f).jour_semaine;
}

// Clé d'un fuseau : change dès qu'un décalage ou une règle de changement d'heure change. L'écran la range dans l'empreinte de ses
// zones pour redessiner les heures, les jours et le soleil quand le lieu change de fuseau (FNV-1a sur les champs du fuseau).
constexpr std::uint32_t cle_du_fuseau(const Fuseau &f) noexcept {
    std::uint32_t h = 2166136261U;
    const auto mele = [&h](std::int64_t v) {
        const std::uint64_t u = static_cast<std::uint64_t>(v);
        for (int i = 0; i < 8; ++i) {
            h = (h ^ static_cast<std::uint32_t>((u >> (8 * i)) & 0xFFU)) * 16777619U;
        }
    };
    mele(f.decalage_std_s);
    mele(f.decalage_ete_s);
    mele(f.a_heure_ete ? 1 : 0);
    if (f.a_heure_ete) { // les règles ne comptent que s'il y a un changement d'heure
        const auto regle = [&mele](const Regle &r) {
            mele(r.mois);
            mele(r.semaine);
            mele(r.jour);
            mele(r.heure_s);
        };
        regle(f.debut);
        regle(f.fin);
    }
    return h;
}

// ---- formats d'heure de l'écran principal -------------------------------------------------------------------------------------

// Étiquette d'une colonne « heure par heure », à partir de l'heure LOCALE de l'instant :
//   minute nulle : « 7h » en français, « 7:00 » dans les autres langues (le format de l'écran depuis le début) ;
//   sinon        : « 05:30 », « 17:45 » (le même « hh:mm » que le lever du soleil et la grande horloge), parce que « 5h » serait faux.
// Rend le nombre d'octets écrits (zéro final non compté) ; 0 et sortie vide si la place manque ou si l'heure est hors limites.
constexpr std::size_t formater_etiquette_heure(const HeureLocale &h, bool francais, char *sortie,
                                               std::size_t capacite) noexcept {
    detail::Sortie s{sortie, capacite};
    if (h.heure < 0 || h.heure > 23 || h.minute < 0 || h.minute > 59) {
        s.trop = true;
        return s.terminer();
    }
    if (h.minute == 0) {
        s.entier(h.heure);
        if (francais) {
            s.car('h');
        } else {
            s.texte(":00");
        }
    } else {
        s.deux_chiffres(h.heure);
        s.car(':');
        s.deux_chiffres(h.minute);
    }
    return s.terminer();
}

// « 07:12 », « 17:10 » : heure locale en chiffres, deux chiffres partout (lever et coucher du soleil).
constexpr std::size_t formater_hhmm(const HeureLocale &h, char *sortie, std::size_t capacite) noexcept {
    detail::Sortie s{sortie, capacite};
    if (h.heure < 0 || h.heure > 23 || h.minute < 0 || h.minute > 59) {
        s.trop = true;
        return s.terminer();
    }
    s.deux_chiffres(h.heure);
    s.car(':');
    s.deux_chiffres(h.minute);
    return s.terminer();
}

} // namespace ecran::heure
