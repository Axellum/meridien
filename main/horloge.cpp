// Heure locale du LIEU choisi (ADR-0012) : le fuseau est celui de la zone enregistrée (meteo_zone.h), relu à chaque appel. Aucun
// fuseau n'est écrit ici : un changement de zone, depuis l'écran ou depuis la page web (les deux passent par zone_ecrire), change
// l'heure, la date et le changement de jour dès l'appel suivant, sans notification ni état à tenir. Seule la zone d'usine (Lyon,
// Europe/Paris, meteo_zone.cpp) donne l'heure de Paris, parce que c'est alors le lieu.
//
// Sans état : la chaîne POSIX est cherchée dans la table du noyau puis analysée à chaque appel (quelques microsecondes : une
// dichotomie sur 468 noms et la lecture d'une trentaine de caractères), ce qui évite tout verrou ; la zone elle-même est copiée sous le
// verrou de meteo_zone.cpp. Ce fichier est compilé tel quel par le rendu PC (tools/rendu/), qui fournit zone_lire (banc.cpp).
#include "horloge.h"

#include <cstddef>
#include <string_view>

#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/visuel_dates.hpp"
#include "meteo_zone.h"

namespace ecran {

namespace {

// Les règles du fuseau de la zone enregistrée. Faux si la zone n'a pas pu être lue ou si son nom IANA n'est pas dans la table
// (elle ne le devrait pas : la page de réglages et l'écran refusent un fuseau inconnu, mais une NVS plus ancienne pourrait en garder
// un). On n'invente alors pas l'heure de Paris : l'appelant affiche « heure inconnue ».
bool fuseau_du_lieu(heure::AnalyseTz &analyse) {
    ZoneMeteo zone = {};
    if (!zone_lire(&zone)) {
        return false;
    }
    std::size_t n = 0;
    while (n < sizeof zone.fuseau && zone.fuseau[n] != '\0') {
        ++n;
    }
    analyse = heure::analyser_tz(fuseaux::posix_pour(std::string_view(zone.fuseau, n)));
    return analyse.ok();
}

} // namespace

bool horloge_fuseau(heure::Fuseau &sortie) {
    heure::AnalyseTz analyse;
    if (!fuseau_du_lieu(analyse)) {
        return false;
    }
    sortie = analyse.fuseau;
    return true;
}

bool horloge_formater(int64_t instant_utc_s, TexteHorloge &sortie) {
    sortie.heure[0] = '\0';
    sortie.date[0] = '\0';
    heure::AnalyseTz analyse;
    if (!fuseau_du_lieu(analyse)) {
        return false;
    }
    const heure::HeureLocale locale = heure::en_heure_locale(instant_utc_s, analyse.fuseau);
    const std::size_t n_heure = heure::formater_heure(locale, sortie.heure, sizeof(sortie.heure));
    const std::size_t n_date = heure::formater_date_longue(locale, sortie.date, sizeof(sortie.date), true);
    return n_heure > 0 && n_date > 0;
}

bool horloge_hhmm(int64_t instant_utc_s, char *sortie, std::size_t capacite) {
    if (sortie == nullptr || capacite == 0) {
        return false;
    }
    sortie[0] = '\0';
    heure::AnalyseTz analyse;
    if (capacite < 6 || !fuseau_du_lieu(analyse)) {
        return false;
    }
    const heure::HeureLocale locale = heure::en_heure_locale(instant_utc_s, analyse.fuseau);
    if (locale.heure < 0 || locale.heure > 23 || locale.minute < 0 || locale.minute > 59) {
        return false;
    }
    sortie[0] = static_cast<char>('0' + locale.heure / 10);
    sortie[1] = static_cast<char>('0' + locale.heure % 10);
    sortie[2] = ':';
    sortie[3] = static_cast<char>('0' + locale.minute / 10);
    sortie[4] = static_cast<char>('0' + locale.minute % 10);
    sortie[5] = '\0';
    return true;
}

bool horloge_date_localisee(int64_t instant_utc_s, i18n::Langue langue, char *sortie, std::size_t capacite) {
    if (sortie == nullptr || capacite == 0) {
        return false;
    }
    sortie[0] = '\0';
    heure::AnalyseTz analyse;
    if (!fuseau_du_lieu(analyse)) {
        return false;
    }
    const heure::HeureLocale locale = heure::en_heure_locale(instant_utc_s, analyse.fuseau);
    return visuel::formater_date(langue, locale, sortie, capacite, false, true) > 0;
}

bool horloge_heure_locale(int64_t instant_utc_s, heure::HeureLocale &sortie) {
    heure::AnalyseTz analyse;
    if (!fuseau_du_lieu(analyse)) {
        return false;
    }
    sortie = heure::en_heure_locale(instant_utc_s, analyse.fuseau);
    return true;
}

int32_t horloge_decalage_s(int64_t instant_utc_s) {
    heure::AnalyseTz analyse;
    if (!fuseau_du_lieu(analyse)) {
        return 0;
    }
    return heure::en_heure_locale(instant_utc_s, analyse.fuseau).decalage_s;
}

} // namespace ecran
