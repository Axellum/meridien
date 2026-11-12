// Écran principal : petits formats, l'heure du lieu, le texte ajusté à une largeur.
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- petits formats ----------------------------------------------------------------------------------------------------

namespace {

constexpr const char *DEGRE = "\xC2\xB0"; // °

char virgule_decimale() { return i18n::langue_courante() == i18n::Langue::En ? '.' : ','; }
} // namespace

// Température en dixièmes -> « 12° », « −3° » ; « — » si inconnue.
void fmt_temp(char *sortie, std::size_t n, int dix) {
    char nombre[16];
    if (!connu(dix) || formater_temperature(dix, nombre, sizeof nombre) == 0) {
        std::snprintf(sortie, n, "%s", TIRET);
        return;
    }
    std::snprintf(sortie, n, "%s%s", nombre, DEGRE);
}

// Valeur en dixièmes -> « 0,4 » ou « 12 » (sans décimale quand elle est nulle).
void fmt_dixieme(char *sortie, std::size_t n, int dix) {
    if (dix % 10 == 0) {
        std::snprintf(sortie, n, "%d", dix / 10);
    } else {
        std::snprintf(sortie, n, "%d%c%d", dix / 10, virgule_decimale(), dix % 10 < 0 ? -(dix % 10) : dix % 10);
    }
}

// ---- l'heure du lieu (ADR-0012) -----------------------------------------------------------------------------------------
//
// Chaque instant affiché est converti avec les règles du fuseau du lieu, au décalage en vigueur à CET instant (ecran/heure_lieu.hpp,
// ecran/heure_fr.hpp) : pas un décalage unique pour toute la fenêtre de prévision.

ecran::heure::Fuseau fuseau() {
    if (g.e.fuseau_connu) {
        return g.e.fuseau;
    }
    // Nom de fuseau absent de la table (enregistrement ancien) : on garde le décalage de la réponse météo, fixe, faute de mieux.
    return ecran::heure::fuseau_fixe(g.e.meteo.connue ? g.e.meteo.decalage_utc_s : 0);
}

ecran::heure::HeureLocale heure_du(std::int64_t instant_s) {
    return ecran::heure::en_heure_locale(instant_s, fuseau());
}

// « 07:12 » ; « — » quand l'instant est inconnu.
void fmt_hhmm(char *sortie, std::size_t n, std::int64_t epoch_s) {
    if (epoch_s <= 0 || ecran::heure::formater_hhmm(heure_du(epoch_s), sortie, n) == 0) {
        std::snprintf(sortie, n, "%s", TIRET);
    }
}

// Libellé météo de l'instant, dans la langue de l'écran (le code WMO est la source, jamais un libellé déjà traduit).
void libelle_meteo(char *sortie, std::size_t n, int code, bool jour) {
    const meteo::Condition d = meteo::decrire(code, jour);
    const std::string_view texte = i18n::tr(d.libelle);
    std::snprintf(sortie, n, "%.*s", static_cast<int>(texte.size()), texte.data());
}

Ajuste ajuster(std::initializer_list<const Police *> polices, const char *s, int largeur) {
    Ajuste a = {};
    std::snprintf(a.texte, sizeof a.texte, "%s", s != nullptr ? s : "");
    for (const Police *p : polices) {
        a.police = p;
        if (dz::largeur_texte(*p, a.texte) <= largeur) {
            return a;
        }
    }
    // Aucune ne convient : on coupe la dernière à la limite d'un caractère UTF-8 complet, on pose « … ».
    std::size_t n = std::strlen(a.texte);
    while (n > 0 && dz::largeur_texte(*a.police, a.texte) + dz::largeur_texte(*a.police, "\xE2\x80\xA6") > largeur) {
        --n;
        while (n > 0 && (static_cast<unsigned char>(a.texte[n]) & 0xC0) == 0x80) {
            --n;
        }
        a.texte[n] = '\0';
    }
    if (n + 4 < sizeof a.texte) {
        std::memcpy(a.texte + n, "\xE2\x80\xA6", 4);
    }
    return a;
}

// Pose un texte ajusté, centré verticalement sur `y_centre`.
void texte_ajuste(const dz::Pinceau &pc, std::initializer_list<const Police *> polices, const char *s, int x,
                  int y_centre, int w, lv_text_align_t align, std::uint32_t rgb) {
    const Ajuste a = ajuster(polices, s, w);
    dz::texte(pc, *a.police, a.texte, x, y_centre - dz::hauteur_ligne(*a.police) / 2, w, align, rgb);
}

// ---- empreintes (pour ne redessiner que ce qui change) -------------------------------------------------------------------

} // namespace principal
} // namespace ecran
