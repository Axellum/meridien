#include "horloge.h"

#include "ecran/fuseaux.hpp"
#include "ecran/heure_fr.hpp"

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

} // namespace ecran
