// Noyau pur : instant UNIX (secondes UTC) + chaîne POSIX TZ -> date et heure locales, et formats français.
//
// La chaîne POSIX est celle de fuseaux.hpp (« CET-1CEST,M3.5.0,M10.5.0/3 ») ; l'appareil n'a pas besoin de tzset()
// pour calculer : ce fichier refait le calcul (testable sur PC, comparé à zoneinfo de Python dans test/host/).
// Formes comprises : « std offset [dst [offset] ,début[/heure],fin[/heure]] », noms ABC ou <+0530>, décalages
// [+-]h[:mm[:ss]] (jusqu'à 167 h), règles « Mm.w.d » avec heure signée (« M3.5.0/-1 », « M9.5.0/2:45 »).
// NON gérées (rendues en erreur, jamais ignorées) : les règles « Jn » et « n » (aucune des 468 zones de la table
// n'en utilise), et un nom d'heure d'été SANS règle.
// Heure d'été : `heure_ete` dit « la période dst de la chaîne est en vigueur ». Cas Europe/Dublin : sa chaîne
// (IST-1GMT0,M10.5.0,M3.5.0/1) place le « dst » en HIVER : heure_ete est vrai en hiver, et décalage_s est juste.
// Le calcul vaut pour toute date (calendrier grégorien), en supposant que la règle de la chaîne s'applique à l'année
// demandée (vrai pour 2008-2040 sur les zones testées ; les règles passées ou futures de l'IANA ne sont pas connues).
//
// Formats français (accents compris), tous en UTF-8, espaces ordinaires (pas d'insécable : police non vérifiée) :
//   formater_date_longue : « jeudi 6 novembre » (avec année : « jeudi 6 novembre 2026 ») ; le 1er : « dimanche 1er novembre »
//   formater_heure       : « 11 h 40 », « 9 h 05 », « 0 h 00 » (24 h, minutes sur 2 chiffres)
//   formater_temperature : « 12 °C », « -3 °C », avec décimale « 11,6 °C » ; formater_degre : « 12° »
// Le signe moins est le « - » ASCII (le vrai « − » U+2212 n'est pas sûr d'exister dans la police : non vérifié).
// Une température arrondie à 0 s'écrit « 0 » (jamais « -0 »). Arrondi : moitié loin de zéro. Toutes les fonctions
// de format terminent la sortie par un zéro et rendent le nombre d'octets écrits ; 0 (et sortie vide) si la place
// manque ou si l'entrée est hors limites.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ecran::heure {

struct HeureLocale {
    int an = 1970;
    int mois = 1;                // 1..12
    int jour = 1;                // 1..31
    int heure = 0;               // 0..23
    int minute = 0;              // 0..59
    int seconde = 0;             // 0..59 (pas de seconde intercalaire)
    int jour_semaine = 4;        // 0 = dimanche … 6 = samedi
    std::int32_t decalage_s = 0; // décalage à l'UTC en vigueur, positif à l'est (Paris l'été : 7200)
    bool heure_ete = false;
};

enum class ErreurTz : std::uint8_t { Aucune, Vide, NomInvalide, DecalageInvalide, RegleInvalide, TexteEnTrop };

struct Regle {
    int mois = 0;       // 1..12
    int semaine = 0;    // 1..5 (5 = la dernière)
    int jour = 0;       // 0 = dimanche … 6
    int heure_s = 7200; // heure locale du changement, en secondes depuis minuit (peut être négative ou > 24 h)
};

struct Fuseau {
    std::string_view nom_std{}; // « CET » (vue sur la chaîne analysée : elle doit survivre)
    std::string_view nom_ete{};
    std::int32_t decalage_std_s = 0; // positif à l'est (l'inverse de la convention POSIX)
    std::int32_t decalage_ete_s = 0;
    bool a_heure_ete = false;
    Regle debut{}; // passage à la période « ete » ; l'heure est en heure STANDARD
    Regle fin{};   // retour à la période standard ; l'heure est en heure « ete »
};

struct AnalyseTz {
    Fuseau fuseau{};
    ErreurTz erreur = ErreurTz::Aucune;
    std::size_t position = 0; // index de l'erreur dans la chaîne
    constexpr bool ok() const noexcept { return erreur == ErreurTz::Aucune; }
};

namespace detail {

struct Curseur {
    std::string_view t;
    std::size_t p = 0;
    constexpr bool fin() const noexcept { return p >= t.size(); }
    constexpr char c() const noexcept { return p < t.size() ? t[p] : '\0'; }
};

constexpr bool est_chiffre(char c) noexcept { return c >= '0' && c <= '9'; }
constexpr bool est_lettre(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// Nom : 3 lettres ou plus, ou <...> (lettres, chiffres, + et -).
constexpr bool lire_nom(Curseur &k, std::string_view &nom) noexcept {
    if (k.c() == '<') {
        ++k.p;
        const std::size_t debut = k.p;
        while (!k.fin() && k.c() != '>') {
            const char c = k.c();
            if (!(est_lettre(c) || est_chiffre(c) || c == '+' || c == '-'))
                return false;
            ++k.p;
        }
        if (k.fin() || k.p == debut)
            return false;
        nom = k.t.substr(debut, k.p - debut);
        ++k.p;
        return true;
    }
    const std::size_t debut = k.p;
    while (est_lettre(k.c()))
        ++k.p;
    if (k.p - debut < 3)
        return false;
    nom = k.t.substr(debut, k.p - debut);
    return true;
}

// [+-]h[:mm[:ss]] -> secondes (signées). h : 1 à 3 chiffres, <= 167.
constexpr bool lire_heure(Curseur &k, std::int32_t &secondes) noexcept {
    int signe = 1;
    if (k.c() == '+' || k.c() == '-') {
        if (k.c() == '-')
            signe = -1;
        ++k.p;
    }
    int h = 0, n = 0;
    while (est_chiffre(k.c()) && n < 4) {
        h = h * 10 + (k.c() - '0');
        ++k.p;
        ++n;
    }
    if (n == 0 || n > 3 || h > 167)
        return false;
    int m = 0, s = 0;
    if (k.c() == ':') {
        ++k.p;
        if (!(est_chiffre(k.c()) && est_chiffre(k.p + 1 < k.t.size() ? k.t[k.p + 1] : '\0')))
            return false;
        m = (k.c() - '0') * 10 + (k.t[k.p + 1] - '0');
        k.p += 2;
        if (m > 59)
            return false;
        if (k.c() == ':') {
            ++k.p;
            if (!(est_chiffre(k.c()) && est_chiffre(k.p + 1 < k.t.size() ? k.t[k.p + 1] : '\0')))
                return false;
            s = (k.c() - '0') * 10 + (k.t[k.p + 1] - '0');
            k.p += 2;
            if (s > 59)
                return false;
        }
    }
    secondes = signe * (h * 3600 + m * 60 + s);
    return true;
}

// Mm.w.d[/heure]
constexpr bool lire_regle(Curseur &k, Regle &r) noexcept {
    if (k.c() != 'M')
        return false;
    ++k.p;
    auto nombre = [&k](int maximum, int &v) {
        int n = 0, x = 0;
        while (est_chiffre(k.c()) && n < 3) {
            x = x * 10 + (k.c() - '0');
            ++k.p;
            ++n;
        }
        v = x;
        return n > 0 && x <= maximum;
    };
    if (!nombre(12, r.mois) || r.mois < 1 || k.c() != '.')
        return false;
    ++k.p;
    if (!nombre(5, r.semaine) || r.semaine < 1 || k.c() != '.')
        return false;
    ++k.p;
    if (!nombre(6, r.jour))
        return false;
    r.heure_s = 7200;
    if (k.c() == '/') {
        ++k.p;
        std::int32_t s = 0;
        if (!lire_heure(k, s))
            return false;
        r.heure_s = s;
    }
    return true;
}

constexpr AnalyseTz echec_tz(ErreurTz e, const Curseur &k) noexcept {
    AnalyseTz a;
    a.erreur = e;
    a.position = k.p;
    return a;
}

// ---- calendrier grégorien (algorithmes de H. Hinnant, domaine public) ------------------------------------------------

constexpr std::int64_t division_plancher(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

// Jours depuis le 1970-01-01 pour une date civile (mois 1..12, jour 1..31).
constexpr std::int64_t jours_depuis_epoch(std::int64_t an, int mois, int jour) noexcept {
    an -= mois <= 2 ? 1 : 0;
    const std::int64_t ere = division_plancher(an, 400);
    const std::int64_t annee_ere = an - ere * 400;                                                // [0, 399]
    const std::int64_t jour_annee = (153 * (mois + (mois > 2 ? -3 : 9)) + 2) / 5 + jour - 1;      // [0, 365]
    const std::int64_t jour_ere = annee_ere * 365 + annee_ere / 4 - annee_ere / 100 + jour_annee; // [0, 146096]
    return ere * 146097 + jour_ere - 719468;
}

constexpr void date_depuis_jours(std::int64_t z, std::int64_t &an, int &mois, int &jour) noexcept {
    z += 719468;
    const std::int64_t ere = division_plancher(z, 146097);
    const std::int64_t jour_ere = z - ere * 146097; // [0, 146096]
    const std::int64_t annee_ere =
        (jour_ere - jour_ere / 1460 + jour_ere / 36524 - jour_ere / 146096) / 365;                  // [0, 399]
    const std::int64_t jour_annee = jour_ere - (365 * annee_ere + annee_ere / 4 - annee_ere / 100); // [0, 365]
    const std::int64_t mp = (5 * jour_annee + 2) / 153;                                             // [0, 11]
    jour = static_cast<int>(jour_annee - (153 * mp + 2) / 5 + 1);
    mois = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    an = annee_ere + ere * 400 + (mois <= 2 ? 1 : 0);
}

constexpr bool bissextile(std::int64_t an) noexcept { return (an % 4 == 0 && an % 100 != 0) || an % 400 == 0; }

constexpr int jours_du_mois(std::int64_t an, int mois) noexcept {
    constexpr int J[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (mois == 2 && bissextile(an)) ? 29 : J[mois - 1];
}

constexpr int jour_semaine_de(std::int64_t jours) noexcept { // 0 = dimanche ; le 1970-01-01 était un jeudi
    return static_cast<int>(((jours + 4) % 7 + 7) % 7);
}

// Jour du mois (1..31) désigné par la règle Mm.w.d pour l'année `an`.
constexpr int jour_de_la_regle(std::int64_t an, const Regle &r) noexcept {
    const int premier = jour_semaine_de(jours_depuis_epoch(an, r.mois, 1));
    int d = 1 + ((r.jour - premier + 7) % 7) + (r.semaine - 1) * 7;
    if (d > jours_du_mois(an, r.mois))
        d -= 7; // « 5 » veut dire : la dernière
    return d;
}

// Instant « mural » (heure locale lue comme si c'était de l'UTC) du changement de l'année `an`.
constexpr std::int64_t instant_mural(std::int64_t an, const Regle &r) noexcept {
    return jours_depuis_epoch(an, r.mois, jour_de_la_regle(an, r)) * 86400 + r.heure_s;
}

} // namespace detail

constexpr std::string_view message(ErreurTz e) noexcept {
    switch (e) {
    case ErreurTz::Aucune:
        return "aucune erreur";
    case ErreurTz::Vide:
        return "fuseau vide";
    case ErreurTz::NomInvalide:
        return "nom de fuseau invalide";
    case ErreurTz::DecalageInvalide:
        return "décalage invalide";
    case ErreurTz::RegleInvalide:
        return "règle d'heure d'été invalide ou non gérée";
    case ErreurTz::TexteEnTrop:
        return "texte après la règle";
    }
    return "erreur inconnue";
}

// Analyse une chaîne POSIX TZ (« CET-1CEST,M3.5.0,M10.5.0/3 »).
constexpr AnalyseTz analyser_tz(std::string_view posix) noexcept {
    detail::Curseur k{posix};
    if (posix.empty())
        return detail::echec_tz(ErreurTz::Vide, k);
    AnalyseTz a;
    Fuseau &f = a.fuseau;
    if (!detail::lire_nom(k, f.nom_std))
        return detail::echec_tz(ErreurTz::NomInvalide, k);
    std::int32_t ouest = 0;
    if (!detail::lire_heure(k, ouest))
        return detail::echec_tz(ErreurTz::DecalageInvalide, k);
    f.decalage_std_s = -ouest; // POSIX compte positif vers l'OUEST
    if (k.fin()) {
        f.decalage_ete_s = f.decalage_std_s;
        return a;
    }
    if (!detail::lire_nom(k, f.nom_ete))
        return detail::echec_tz(ErreurTz::NomInvalide, k);
    f.a_heure_ete = true;
    if (k.c() != ',' && !k.fin()) {
        std::int32_t ouest_ete = 0;
        if (!detail::lire_heure(k, ouest_ete))
            return detail::echec_tz(ErreurTz::DecalageInvalide, k);
        f.decalage_ete_s = -ouest_ete;
    } else {
        f.decalage_ete_s = f.decalage_std_s + 3600; // sans précision : une heure d'avance
    }
    if (k.c() != ',')
        return detail::echec_tz(ErreurTz::RegleInvalide, k); // nom d'été sans règle : non géré
    ++k.p;
    if (!detail::lire_regle(k, f.debut))
        return detail::echec_tz(ErreurTz::RegleInvalide, k);
    if (k.c() != ',')
        return detail::echec_tz(ErreurTz::RegleInvalide, k);
    ++k.p;
    if (!detail::lire_regle(k, f.fin))
        return detail::echec_tz(ErreurTz::RegleInvalide, k);
    if (!k.fin())
        return detail::echec_tz(ErreurTz::TexteEnTrop, k);
    return a;
}

// La période « ete » de `f` est-elle en vigueur à l'instant UTC `instant_s` ?
constexpr bool est_heure_ete(std::int64_t instant_s, const Fuseau &f) noexcept {
    if (!f.a_heure_ete)
        return false;
    std::int64_t an = 0;
    int mois = 0, jour = 0;
    detail::date_depuis_jours(detail::division_plancher(instant_s + f.decalage_std_s, 86400), an, mois, jour);
    const std::int64_t debut = detail::instant_mural(an, f.debut) - f.decalage_std_s; // heure standard avant le passage
    const std::int64_t fin = detail::instant_mural(an, f.fin) - f.decalage_ete_s;     // heure d'été avant le retour
    return debut < fin ? (instant_s >= debut && instant_s < fin) : (instant_s >= debut || instant_s < fin);
}

constexpr HeureLocale en_heure_locale(std::int64_t instant_s, const Fuseau &f) noexcept {
    HeureLocale h;
    h.heure_ete = est_heure_ete(instant_s, f);
    h.decalage_s = h.heure_ete ? f.decalage_ete_s : f.decalage_std_s;
    const std::int64_t local = instant_s + h.decalage_s;
    const std::int64_t jours = detail::division_plancher(local, 86400);
    const std::int64_t reste = local - jours * 86400;
    std::int64_t an = 0;
    detail::date_depuis_jours(jours, an, h.mois, h.jour);
    h.an = static_cast<int>(an);
    h.heure = static_cast<int>(reste / 3600);
    h.minute = static_cast<int>(reste % 3600 / 60);
    h.seconde = static_cast<int>(reste % 60);
    h.jour_semaine = detail::jour_semaine_de(jours);
    return h;
}

struct Local {
    HeureLocale heure{};
    ErreurTz erreur = ErreurTz::Aucune;
    constexpr bool ok() const noexcept { return erreur == ErreurTz::Aucune; }
};

// Raccourci : analyse la chaîne POSIX puis convertit. Pour un appel par seconde, analyser une fois (analyser_tz).
constexpr Local convertir(std::int64_t instant_s, std::string_view posix) noexcept {
    const AnalyseTz a = analyser_tz(posix);
    Local l;
    if (!a.ok()) {
        l.erreur = a.erreur;
        return l;
    }
    l.heure = en_heure_locale(instant_s, a.fuseau);
    return l;
}

// ---- formats français -------------------------------------------------------------------------------------------------

constexpr std::string_view nom_jour(int jour_semaine) noexcept {
    constexpr std::string_view N[] = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"};
    return (jour_semaine >= 0 && jour_semaine <= 6) ? N[jour_semaine] : std::string_view{};
}

constexpr std::string_view nom_mois(int mois) noexcept {
    constexpr std::string_view N[] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                                      "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};
    return (mois >= 1 && mois <= 12) ? N[mois - 1] : std::string_view{};
}

namespace detail {

struct Sortie {
    char *p;
    std::size_t capacite;
    std::size_t n = 0;
    bool trop = false;

    constexpr void car(char c) noexcept {
        if (p == nullptr || n + 1 >= capacite) { // garde la place du zéro final
            trop = true;
            return;
        }
        p[n++] = c;
    }
    constexpr void texte(std::string_view s) noexcept {
        for (char c : s)
            car(c);
    }
    constexpr void entier(std::int64_t v) noexcept { // v >= 0
        char chiffres[24] = {};
        int k = 0;
        do {
            chiffres[k++] = static_cast<char>('0' + v % 10);
            v /= 10;
        } while (v > 0);
        while (k > 0)
            car(chiffres[--k]);
    }
    constexpr void deux_chiffres(int v) noexcept {
        car(static_cast<char>('0' + v / 10 % 10));
        car(static_cast<char>('0' + v % 10));
    }
    constexpr std::size_t terminer() noexcept {
        if (p == nullptr || capacite == 0)
            return 0;
        if (trop) {
            p[0] = '\0';
            return 0;
        }
        p[n] = '\0';
        return n;
    }
};

// Température en dixièmes -> degrés entiers, arrondi moitié loin de zéro.
constexpr std::int64_t degres_arrondis(std::int64_t dixiemes) noexcept {
    return dixiemes >= 0 ? (dixiemes + 5) / 10 : -((-dixiemes + 5) / 10);
}

constexpr bool date_valide(const HeureLocale &h) noexcept {
    return h.mois >= 1 && h.mois <= 12 && h.jour >= 1 && h.jour <= jours_du_mois(h.an, h.mois) && h.jour_semaine >= 0 &&
           h.jour_semaine <= 6;
}

} // namespace detail

// « jeudi 6 novembre » ; « dimanche 1er novembre 2026 » avec l'année.
constexpr std::size_t formater_date_longue(const HeureLocale &h, char *sortie, std::size_t capacite,
                                           bool avec_annee = false) noexcept {
    detail::Sortie s{sortie, capacite};
    if (!detail::date_valide(h) || (avec_annee && h.an < 0)) {
        s.trop = true;
        return s.terminer();
    }
    s.texte(nom_jour(h.jour_semaine));
    s.car(' ');
    if (h.jour == 1) {
        s.texte("1er");
    } else {
        s.entier(h.jour);
    }
    s.car(' ');
    s.texte(nom_mois(h.mois));
    if (avec_annee) {
        s.car(' ');
        s.entier(h.an);
    }
    return s.terminer();
}

// « 11 h 40 », « 9 h 05 », « 0 h 00 ».
constexpr std::size_t formater_heure(const HeureLocale &h, char *sortie, std::size_t capacite) noexcept {
    detail::Sortie s{sortie, capacite};
    if (h.heure < 0 || h.heure > 23 || h.minute < 0 || h.minute > 59) {
        s.trop = true;
        return s.terminer();
    }
    s.entier(h.heure);
    s.texte(" h ");
    s.deux_chiffres(h.minute);
    return s.terminer();
}

// « 12 °C », « -3 °C » ; avec `decimale` : « 11,6 °C » (virgule française).
constexpr std::size_t formater_temperature(int dixiemes, char *sortie, std::size_t capacite,
                                           bool decimale = false) noexcept {
    detail::Sortie s{sortie, capacite};
    if (decimale) {
        const std::int64_t a = dixiemes < 0 ? -static_cast<std::int64_t>(dixiemes) : dixiemes;
        if (dixiemes < 0)
            s.car('-');
        s.entier(a / 10);
        s.car(',');
        s.entier(a % 10);
    } else {
        const std::int64_t d = detail::degres_arrondis(dixiemes);
        if (d < 0)
            s.car('-');
        s.entier(d < 0 ? -d : d);
    }
    s.texte(" °C");
    return s.terminer();
}

// « 12° » (affichage compact).
constexpr std::size_t formater_degre(int dixiemes, char *sortie, std::size_t capacite) noexcept {
    detail::Sortie s{sortie, capacite};
    const std::int64_t d = detail::degres_arrondis(dixiemes);
    if (d < 0)
        s.car('-');
    s.entier(d < 0 ? -d : d);
    s.texte("°");
    return s.terminer();
}

} // namespace ecran::heure
