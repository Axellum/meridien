#include "horloge.h"

#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/visuel_dates.hpp"

namespace ecran {

namespace {

constexpr const char *kFuseauIana = "Europe/Paris";

// Analysée une seule fois (la structure garde des vues sur la table des fuseaux, qui est statique).
const heure::AnalyseTz &fuseau_local() {
    static const heure::AnalyseTz analyse = heure::analyser_tz(fuseaux::posix_pour(kFuseauIana));
    return analyse;
}

} // namespace

bool horloge_formater(int64_t instant_utc_s, TexteHorloge &sortie) {
    sortie.heure[0] = '\0';
    sortie.date[0] = '\0';
    const heure::AnalyseTz &analyse = fuseau_local();
    if (!analyse.ok()) {
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
    const heure::AnalyseTz &analyse = fuseau_local();
    if (!analyse.ok() || capacite < 6) {
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
    const heure::AnalyseTz &analyse = fuseau_local();
    if (!analyse.ok()) {
        return false;
    }
    const heure::HeureLocale locale = heure::en_heure_locale(instant_utc_s, analyse.fuseau);
    return visuel::formater_date(langue, locale, sortie, capacite) > 0;
}

int32_t horloge_decalage_s(int64_t instant_utc_s) {
    const heure::AnalyseTz &analyse = fuseau_local();
    if (!analyse.ok()) {
        return 0;
    }
    return heure::en_heure_locale(instant_utc_s, analyse.fuseau).decalage_s;
}

} // namespace ecran
