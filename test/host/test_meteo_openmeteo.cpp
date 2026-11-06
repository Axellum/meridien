// Tests du noyau pur : réponse Open-Meteo -> Releve (meteo_openmeteo.hpp) et lecteur JSON (json_mini.hpp) sur deux
// VRAIES réponses (donnees/openmeteo_*.inc, capturées le 06/11/2026). Les valeurs attendues viennent de Python
// (json.loads, decimal) : voir controle_python.py et donnees/LISEZMOI.md. Elles ne sont jamais écrites à la main ici.
#include "ecran/fuseaux.hpp"
#include "ecran/json_mini.hpp"
#include "ecran/meteo_openmeteo.hpp"
#include "ecran/meteo_wmo.hpp"
#include "mini.hpp"

using namespace ecran;
using namespace ecran::meteo;
using sv = std::string_view;

namespace {

constexpr sv PARIS =
#include "donnees/openmeteo_paris.inc"
    ;
constexpr sv LONGYEARBYEN =
#include "donnees/openmeteo_longyearbyen.inc"
    ;

constexpr std::int64_t RECU = 1793966500; // 100 s après current.time de la réponse de Paris

// ---- tables générées par Python -------------------------------------------------------------------------------------

struct Nombre {
    const char *chemin;
    double valeur;
    bool entier;
    long long entier_valeur;
    long long fixe1;
};
struct Texte {
    const char *chemin;
    const char *texte;
};
struct Taille {
    const char *chemin;
    int n;
};

constexpr Nombre NOMBRES_PARIS[] = {
#define ATTENDU_NOMBRES_ICI
#include "donnees/attendu_json_paris.inc"
#undef ATTENDU_NOMBRES_ICI
};
constexpr Texte TEXTES_PARIS[] = {
#define ATTENDU_TEXTES_ICI
#include "donnees/attendu_json_paris.inc"
#undef ATTENDU_TEXTES_ICI
};
constexpr Taille TAILLES_PARIS[] = {
#define ATTENDU_TAILLES_ICI
#include "donnees/attendu_json_paris.inc"
#undef ATTENDU_TAILLES_ICI
};
constexpr Nombre NOMBRES_LYB[] = {
#define ATTENDU_NOMBRES_ICI
#include "donnees/attendu_json_longyearbyen.inc"
#undef ATTENDU_NOMBRES_ICI
};
constexpr Texte TEXTES_LYB[] = {
#define ATTENDU_TEXTES_ICI
#include "donnees/attendu_json_longyearbyen.inc"
#undef ATTENDU_TEXTES_ICI
};
constexpr Taille TAILLES_LYB[] = {
#define ATTENDU_TAILLES_ICI
#include "donnees/attendu_json_longyearbyen.inc"
#undef ATTENDU_TAILLES_ICI
};

struct AttJour {
    long long date;
    int code, min, max;
    long long lever, coucher;
    int uv, proba;
};
struct AttReleve {
    long long instant;
    int decalage;
    const char *fuseau;
    int temp, ress, code;
    bool jour;
    int vent, rafales, direction, pression, humidite, proba_now, nb_jours;
    AttJour jours[7];
};
constexpr AttReleve ATTENDU_PARIS = {
#include "donnees/attendu_meteo_paris.inc"
};
constexpr AttReleve ATTENDU_LYB = {
#include "donnees/attendu_meteo_longyearbyen.inc"
};

struct AttProba {
    long long instant;
    int proba;
};
constexpr AttProba PROBAS_PARIS[] = {
#include "donnees/attendu_proba_paris.inc"
};
constexpr AttProba PROBAS_LYB[] = {
#include "donnees/attendu_proba_longyearbyen.inc"
};

// ---- 1. le lecteur JSON contre json.loads --------------------------------------------------------------------------

template <std::size_t N> constexpr int nombres_identiques(sv doc, const Nombre (&attendus)[N]) {
    const json::Resultat r = json::lire(doc);
    VERIFIER(r.ok());
    for (const Nombre &a : attendus) {
        const json::Valeur v = r.valeur.trouver(a.chemin);
        VERIFIER(v.existe() && v.genre() == json::Genre::Nombre);
        double d = 0;
        VERIFIER(v.en_double(d));
        VERIFIER(d == a.valeur); // même double, au bit près, que json.loads
        std::int64_t n = 0;
        VERIFIER(v.en_entier(n) == a.entier); // « 3 » est un entier, « 3.0 » non
        if (a.entier)
            VERIFIER(n == a.entier_valeur);
        std::int64_t f = 0;
        VERIFIER(v.en_fixe(1, f) && f == a.fixe1); // ×10 arrondi comme Decimal(ROUND_HALF_UP)
    }
    return 0;
}

template <std::size_t N, std::size_t M>
constexpr int textes_et_tailles_identiques(sv doc, const Texte (&textes)[N], const Taille (&tailles)[M]) {
    const json::Resultat r = json::lire(doc);
    VERIFIER(r.ok());
    for (const Texte &t : textes)
        VERIFIER(r.valeur.trouver(t.chemin).texte_egal(t.texte));
    for (const Taille &t : tailles) {
        const json::Valeur v = r.valeur.trouver(t.chemin);
        VERIFIER(v.existe());
        VERIFIER(v.genre() == json::Genre::Objet || v.genre() == json::Genre::Tableau);
        VERIFIER(v.taille() == static_cast<std::size_t>(t.n));
    }
    return 0;
}

constexpr int les_tables_python_ne_sont_pas_vides() {
    VERIFIER(sizeof(NOMBRES_PARIS) / sizeof(Nombre) == 312);
    VERIFIER(sizeof(TEXTES_PARIS) / sizeof(Texte) == 26);
    VERIFIER(sizeof(TAILLES_PARIS) / sizeof(Taille) == 19);
    VERIFIER(sizeof(NOMBRES_LYB) / sizeof(Nombre) == 312);
    VERIFIER(sizeof(TEXTES_LYB) / sizeof(Texte) == 26);
    VERIFIER(sizeof(TAILLES_LYB) / sizeof(Taille) == 19);
    VERIFIER(PARIS.size() == 2826 && LONGYEARBYEN.size() == 2960); // octets réellement reçus
    return 0;
}
constexpr int json_paris_nombres() { return nombres_identiques(PARIS, NOMBRES_PARIS); }
constexpr int json_paris_textes_et_tailles() {
    return textes_et_tailles_identiques(PARIS, TEXTES_PARIS, TAILLES_PARIS);
}
constexpr int json_longyearbyen_nombres() { return nombres_identiques(LONGYEARBYEN, NOMBRES_LYB); }
constexpr int json_longyearbyen_textes_et_tailles() {
    return textes_et_tailles_identiques(LONGYEARBYEN, TEXTES_LYB, TAILLES_LYB);
}

// ---- 2. le relevé contre json.loads ----------------------------------------------------------------------------------

constexpr int meme_releve(const Releve &r, const AttReleve &a, std::int64_t recu) {
    VERIFIER(r.recu_a_s == recu);
    VERIFIER(r.instant_s == a.instant);
    VERIFIER(r.decalage_utc_s == a.decalage);
    VERIFIER(sv(r.fuseau) == sv(a.fuseau));
    VERIFIER(r.temperature_dixiemes == a.temp);
    VERIFIER(r.ressentie_dixiemes == a.ress);
    VERIFIER(r.code_wmo == a.code);
    VERIFIER(r.jour == a.jour);
    VERIFIER(r.vent_dixiemes_kmh == a.vent);
    VERIFIER(r.rafales_dixiemes_kmh == a.rafales);
    VERIFIER(r.direction_vent_deg == a.direction);
    VERIFIER(r.pression_dixiemes_hpa == a.pression);
    VERIFIER(r.humidite_pct == a.humidite);
    VERIFIER(r.proba_precip_pct == a.proba_now);
    VERIFIER(r.nb_jours == a.nb_jours);
    for (int i = 0; i < a.nb_jours; ++i) {
        const Jour &j = r.jours[i];
        const AttJour &e = a.jours[i];
        VERIFIER(j.date_s == e.date);
        VERIFIER(j.code_wmo == e.code);
        VERIFIER(j.min_dixiemes == e.min);
        VERIFIER(j.max_dixiemes == e.max);
        VERIFIER(j.lever_s == e.lever);
        VERIFIER(j.coucher_s == e.coucher);
        VERIFIER(j.uv_max_dixiemes == e.uv);
        VERIFIER(j.proba_precip_max == e.proba);
    }
    return 0;
}

constexpr int releve_de_paris_identique_a_python() {
    const Analyse a = analyser(PARIS, RECU);
    VERIFIER(a.ok());
    VERIFIER(a.diag.champ.empty());
    return meme_releve(a.releve, ATTENDU_PARIS, RECU);
}

constexpr int releve_de_longyearbyen_identique_a_python() {
    const Analyse a = analyser(LONGYEARBYEN, RECU);
    VERIFIER(a.ok());
    return meme_releve(a.releve, ATTENDU_LYB, RECU);
}

constexpr int valeurs_lues_a_la_main_dans_la_reponse_de_paris() {
    // Contrôle humain de la première ligne de la réponse réelle : "temperature_2m":11.6, "apparent_temperature":9.7,
    // "is_day":1, "weather_code":3, "wind_speed_10m":4.4, "wind_direction_10m":325, "wind_gusts_10m":11.5,
    // "pressure_msl":1022.6, "relative_humidity_2m":64, utc_offset_seconds 3600, Europe/Paris.
    const Releve r = analyser(PARIS, RECU).releve;
    VERIFIER(r.temperature_dixiemes == 116 && r.ressentie_dixiemes == 97 && r.jour && r.code_wmo == 3);
    VERIFIER(r.vent_dixiemes_kmh == 44 && r.direction_vent_deg == 325 && r.rafales_dixiemes_kmh == 115);
    VERIFIER(r.pression_dixiemes_hpa == 10226 && r.humidite_pct == 64);
    VERIFIER(r.decalage_utc_s == 3600 && sv(r.fuseau) == "Europe/Paris" && r.instant_s == 1793966400);
    VERIFIER(r.nb_jours == 7 && r.jours[0].max_dixiemes == 128 && r.jours[0].min_dixiemes == 73);
    VERIFIER(r.jours[0].lever_s == 1793947517 && r.jours[0].coucher_s == 1793982156);
    VERIFIER(r.jours[0].uv_max_dixiemes == 14); // 1,35 arrondi loin de zéro
    return 0;
}

constexpr int nuit_polaire_lever_egal_coucher() {
    const Releve r = analyser(LONGYEARBYEN, RECU).releve;
    VERIFIER(!r.jour); // is_day = 0 à Longyearbyen le 06/11/2026
    VERIFIER(r.nb_jours == 7);
    for (int i = 0; i < 7; ++i) {
        VERIFIER(r.jours[i].sans_soleil()); // lever == coucher == minuit local
        VERIFIER(r.jours[i].lever_s == r.jours[i].date_s);
    }
    VERIFIER(!analyser(PARIS, RECU).releve.jours[0].sans_soleil());
    return 0;
}

constexpr int fuseaux_des_reponses_connus_de_la_table() {
    VERIFIER(fuseaux::trouver(analyser(PARIS, RECU).releve.fuseau) != nullptr);
    VERIFIER(fuseaux::trouver(analyser(LONGYEARBYEN, RECU).releve.fuseau) != nullptr);
    return 0;
}

constexpr int codes_des_reponses_connus_de_la_table_wmo() {
    const Releve r = analyser(PARIS, RECU).releve;
    VERIFIER(decrire(r.code_wmo, r.jour).connu);
    for (int i = 0; i < r.nb_jours; ++i)
        VERIFIER(decrire(r.jours[i].code_wmo, true).connu);
    const Releve l = analyser(LONGYEARBYEN, RECU).releve;
    VERIFIER(decrire(l.code_wmo, l.jour).connu);
    return 0;
}

// ---- 3. probabilité de précipitations de l'heure en cours ---------------------------------------------------------------

template <std::size_t N> constexpr int probas_identiques(sv doc, const AttProba (&attendus)[N]) {
    const json::Valeur racine = json::lire(doc).valeur;
    for (const AttProba &a : attendus) {
        detail::Lecteur l;
        const std::int8_t p = detail::proba_a(racine, a.instant, l);
        VERIFIER(l.diag.ok());
        VERIFIER(p == a.proba);
    }
    return 0;
}
constexpr int proba_paris_par_heure() { return probas_identiques(PARIS, PROBAS_PARIS); }
constexpr int proba_longyearbyen_par_heure() { return probas_identiques(LONGYEARBYEN, PROBAS_LYB); }

// ---- 4. entrées abîmées : chaque champ -> une erreur lisible ---------------------------------------------------------------

struct Tampon {
    char c[4096] = {};
    std::size_t n = 0; // 0 = la modification n'a pas pu s'appliquer (texte absent ou en double)
    constexpr sv vue() const noexcept { return sv(c, n); }
};

// Remplace l'UNIQUE occurrence de `ancien` dans `source` par `nouveau`.
constexpr Tampon remplace(sv source, sv ancien, sv nouveau) {
    Tampon t;
    const std::size_t pos = source.find(ancien);
    if (pos == sv::npos || source.find(ancien, pos + 1) != sv::npos)
        return t;
    if (source.size() - ancien.size() + nouveau.size() > sizeof(t.c))
        return t;
    std::size_t n = 0;
    for (std::size_t i = 0; i < pos; ++i)
        t.c[n++] = source[i];
    for (char ch : nouveau)
        t.c[n++] = ch;
    for (std::size_t i = pos + ancien.size(); i < source.size(); ++i)
        t.c[n++] = source[i];
    t.n = n;
    return t;
}

constexpr Tampon coupe(sv source, std::size_t n) {
    Tampon t;
    for (std::size_t i = 0; i < n && i < source.size(); ++i)
        t.c[t.n++] = source[i];
    return t;
}

// "aaa…" entre guillemets
constexpr Tampon chaine_de(std::size_t longueur) {
    Tampon t;
    t.c[t.n++] = '"';
    for (std::size_t i = 0; i < longueur; ++i)
        t.c[t.n++] = 'a';
    t.c[t.n++] = '"';
    return t;
}

constexpr bool refuse(sv ancien, sv nouveau, Erreur erreur, sv champ) {
    const Tampon t = remplace(PARIS, ancien, nouveau);
    if (t.n == 0)
        return false;
    const Analyse a = analyser(t.vue(), RECU);
    return !a.ok() && a.diag.erreur == erreur && a.diag.champ == champ;
}
constexpr bool accepte(sv ancien, sv nouveau) {
    const Tampon t = remplace(PARIS, ancien, nouveau);
    return t.n != 0 && analyser(t.vue(), RECU).ok();
}

constexpr int champs_de_current_absents_nuls_ou_mal_types() {
    VERIFIER(refuse(R"("time":1793966400,)", R"("timx":1793966400,)", Erreur::ChampAbsent, "current.time"));
    VERIFIER(refuse(R"("temperature_2m":11.6,)", R"("temperature_2x":11.6,)", Erreur::ChampAbsent,
                    "current.temperature_2m"));
    VERIFIER(refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":null,)", Erreur::ChampAbsent,
                    "current.temperature_2m"));
    VERIFIER(refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":"11.6",)", Erreur::MauvaisType,
                    "current.temperature_2m"));
    VERIFIER(refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":[11.6],)", Erreur::MauvaisType,
                    "current.temperature_2m"));
    VERIFIER(refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":true,)", Erreur::MauvaisType,
                    "current.temperature_2m"));
    VERIFIER(refuse(R"("apparent_temperature":9.7,)", R"("apparent_temperaturx":9.7,)", Erreur::ChampAbsent,
                    "current.apparent_temperature"));
    VERIFIER(refuse(R"("is_day":1,)", R"("is_dax":1,)", Erreur::ChampAbsent, "current.is_day"));
    VERIFIER(refuse(R"("is_day":1,)", R"("is_day":1.0,)", Erreur::MauvaisType, "current.is_day"));
    VERIFIER(refuse(R"("weather_code":3,)", R"("weather_cod":3,)", Erreur::ChampAbsent, "current.weather_code"));
    VERIFIER(refuse(R"("weather_code":3,)", R"("weather_code":3.0,)", Erreur::MauvaisType, "current.weather_code"));
    VERIFIER(
        refuse(R"("wind_speed_10m":4.4,)", R"("wind_speed_10x":4.4,)", Erreur::ChampAbsent, "current.wind_speed_10m"));
    VERIFIER(refuse(R"("wind_direction_10m":325,)", R"("wind_direction_10m":null,)", Erreur::ChampAbsent,
                    "current.wind_direction_10m"));
    VERIFIER(refuse(R"("wind_gusts_10m":11.5,)", R"("wind_gusts_10x":11.5,)", Erreur::ChampAbsent,
                    "current.wind_gusts_10m"));
    VERIFIER(
        refuse(R"("pressure_msl":1022.6,)", R"("pressure_msx":1022.6,)", Erreur::ChampAbsent, "current.pressure_msl"));
    VERIFIER(refuse(R"("relative_humidity_2m":64})", R"("relative_humidity_2x":64})", Erreur::ChampAbsent,
                    "current.relative_humidity_2m"));
    VERIFIER(refuse(R"("current":{"time")", R"("curren_":{"time")", Erreur::ChampAbsent, "current"));
    VERIFIER(refuse(R"("utc_offset_seconds":3600,)", R"("utc_offset_secondz":3600,)", Erreur::ChampAbsent,
                    "utc_offset_seconds"));
    VERIFIER(refuse(R"("utc_offset_seconds":3600,)", R"("utc_offset_seconds":3600.5,)", Erreur::MauvaisType,
                    "utc_offset_seconds"));
    return 0;
}

constexpr int valeurs_invraisemblables_et_bornes() {
    // refus juste au-delà de chaque borne, acceptation sur la borne
    VERIFIER(accepte(R"("temperature_2m":11.6,)", R"("temperature_2m":60.0,)"));
    VERIFIER(accepte(R"("temperature_2m":11.6,)", R"("temperature_2m":-90.0,)"));
    VERIFIER(
        refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":60.1,)", Erreur::HorsPlage, "current.temperature_2m"));
    VERIFIER(
        refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":-90.1,)", Erreur::HorsPlage, "current.temperature_2m"));
    VERIFIER(
        refuse(R"("temperature_2m":11.6,)", R"("temperature_2m":99999,)", Erreur::HorsPlage, "current.temperature_2m"));
    VERIFIER(accepte(R"("apparent_temperature":9.7,)", R"("apparent_temperature":-120.0,)"));
    VERIFIER(refuse(R"("apparent_temperature":9.7,)", R"("apparent_temperature":-120.1,)", Erreur::HorsPlage,
                    "current.apparent_temperature"));
    VERIFIER(refuse(R"("apparent_temperature":9.7,)", R"("apparent_temperature":80.1,)", Erreur::HorsPlage,
                    "current.apparent_temperature"));
    VERIFIER(refuse(R"("is_day":1,)", R"("is_day":2,)", Erreur::HorsPlage, "current.is_day"));
    VERIFIER(refuse(R"("is_day":1,)", R"("is_day":-1,)", Erreur::HorsPlage, "current.is_day"));
    VERIFIER(accepte(R"("is_day":1,)", R"("is_day":0,)"));
    VERIFIER(accepte(R"("weather_code":3,)", R"("weather_code":99,)"));
    VERIFIER(refuse(R"("weather_code":3,)", R"("weather_code":100,)", Erreur::HorsPlage, "current.weather_code"));
    VERIFIER(refuse(R"("weather_code":3,)", R"("weather_code":-1,)", Erreur::HorsPlage, "current.weather_code"));
    VERIFIER(refuse(R"("weather_code":3,)", R"("weather_code":99999,)", Erreur::HorsPlage, "current.weather_code"));
    VERIFIER(accepte(R"("wind_speed_10m":4.4,)", R"("wind_speed_10m":500.0,)"));
    VERIFIER(
        refuse(R"("wind_speed_10m":4.4,)", R"("wind_speed_10m":500.1,)", Erreur::HorsPlage, "current.wind_speed_10m"));
    VERIFIER(
        refuse(R"("wind_speed_10m":4.4,)", R"("wind_speed_10m":-0.1,)", Erreur::HorsPlage, "current.wind_speed_10m"));
    VERIFIER(refuse(R"("wind_gusts_10m":11.5,)", R"("wind_gusts_10m":999999,)", Erreur::HorsPlage,
                    "current.wind_gusts_10m"));
    VERIFIER(accepte(R"("wind_direction_10m":325,)", R"("wind_direction_10m":360,)"));
    VERIFIER(refuse(R"("wind_direction_10m":325,)", R"("wind_direction_10m":361,)", Erreur::HorsPlage,
                    "current.wind_direction_10m"));
    VERIFIER(refuse(R"("wind_direction_10m":325,)", R"("wind_direction_10m":-1,)", Erreur::HorsPlage,
                    "current.wind_direction_10m"));
    VERIFIER(accepte(R"("pressure_msl":1022.6,)", R"("pressure_msl":800.0,)"));
    VERIFIER(accepte(R"("pressure_msl":1022.6,)", R"("pressure_msl":1100.0,)"));
    VERIFIER(
        refuse(R"("pressure_msl":1022.6,)", R"("pressure_msl":799.9,)", Erreur::HorsPlage, "current.pressure_msl"));
    VERIFIER(
        refuse(R"("pressure_msl":1022.6,)", R"("pressure_msl":1100.1,)", Erreur::HorsPlage, "current.pressure_msl"));
    VERIFIER(refuse(R"("pressure_msl":1022.6,)", R"("pressure_msl":0,)", Erreur::HorsPlage, "current.pressure_msl"));
    VERIFIER(accepte(R"("relative_humidity_2m":64})", R"("relative_humidity_2m":100})"));
    VERIFIER(accepte(R"("relative_humidity_2m":64})", R"("relative_humidity_2m":0})"));
    VERIFIER(refuse(R"("relative_humidity_2m":64})", R"("relative_humidity_2m":101})", Erreur::HorsPlage,
                    "current.relative_humidity_2m"));
    VERIFIER(refuse(R"("relative_humidity_2m":64})", R"("relative_humidity_2m":-1})", Erreur::HorsPlage,
                    "current.relative_humidity_2m"));
    VERIFIER(accepte(R"("utc_offset_seconds":3600,)", R"("utc_offset_seconds":50400,)"));
    VERIFIER(accepte(R"("utc_offset_seconds":3600,)", R"("utc_offset_seconds":-43200,)"));
    VERIFIER(refuse(R"("utc_offset_seconds":3600,)", R"("utc_offset_seconds":50401,)", Erreur::HorsPlage,
                    "utc_offset_seconds"));
    VERIFIER(refuse(R"("utc_offset_seconds":3600,)", R"("utc_offset_seconds":-43201,)", Erreur::HorsPlage,
                    "utc_offset_seconds"));
    VERIFIER(refuse(R"("utc_offset_seconds":3600,)", R"("utc_offset_seconds":4294970896,)", Erreur::HorsPlage,
                    "utc_offset_seconds")); // 2^32 + 3600
    VERIFIER(refuse(R"("time":1793966400,)", R"("time":946684799,)", Erreur::HorsPlage, "current.time"));
    VERIFIER(refuse(R"("time":1793966400,)", R"("time":4102444801,)", Erreur::HorsPlage, "current.time"));
    return 0;
}

constexpr int fuseau_facultatif_mais_controle() {
    const Tampon sans = remplace(PARIS, R"("timezone":"Europe/Paris",)", R"("timezonx":"Europe/Paris",)");
    VERIFIER(sans.n != 0);
    const Analyse a = analyser(sans.vue(), RECU);
    VERIFIER(a.ok() && a.releve.fuseau[0] == '\0'); // absent : vide, rien d'inventé
    const Tampon l47 = chaine_de(FUSEAU_MAX - 1);   // 47 caractères : la plus grande taille
    VERIFIER(accepte(R"("Europe/Paris")", l47.vue()));
    const Tampon l48 = chaine_de(FUSEAU_MAX); // 48 : ne tient pas
    VERIFIER(refuse(R"("Europe/Paris")", l48.vue(), Erreur::HorsPlage, "timezone"));
    VERIFIER(refuse(R"("Europe/Paris")", R"("Europe Paris")", Erreur::HorsPlage, "timezone"));
    VERIFIER(refuse(R"("Europe/Paris")", R"("Europe/Pàris")", Erreur::HorsPlage, "timezone"));
    VERIFIER(accepte(R"("Europe/Paris")", R"("America/Port-au-Prince")"));
    VERIFIER(accepte(R"("Europe/Paris")", R"("Etc/GMT+5")"));
    VERIFIER(refuse(R"("timezone":"Europe/Paris",)", R"("timezone":5,)", Erreur::MauvaisType, "timezone"));
    return 0;
}

constexpr int json_invalide_ou_qui_n_est_pas_un_objet() {
    for (sv t : {"", "   ", "[]", "[1,2]", "12", "\"x\"", "null", "{", "{\"a\":", "}{", "{} {}"}) {
        const Analyse a = analyser(t, RECU);
        VERIFIER(!a.ok() && a.diag.erreur == Erreur::JsonInvalide && a.diag.champ == "json");
    }
    for (std::size_t n : {std::size_t{10}, std::size_t{500}, PARIS.size() - 1}) { // réponse coupée en route
        const Tampon t = coupe(PARIS, n);
        const Analyse a = analyser(t.vue(), RECU);
        VERIFIER(!a.ok() && a.diag.erreur == Erreur::JsonInvalide);
    }
    VERIFIER(analyser(PARIS, RECU).ok());
    VERIFIER(analyser("{}", RECU).diag.erreur == Erreur::ChampAbsent); // un objet vide : champ manquant, pas valeur 0
    return 0;
}

constexpr int horloge_de_reception_invraisemblable() {
    VERIFIER(analyser(PARIS, 0).diag.erreur == Erreur::HorsPlage && analyser(PARIS, 0).diag.champ == "recu_a_s");
    VERIFIER(analyser(PARIS, 946684799).diag.champ == "recu_a_s");
    VERIFIER(analyser(PARIS, 946684800).ok());
    VERIFIER(analyser(PARIS, 4102444800).ok());
    VERIFIER(analyser(PARIS, 4102444801).diag.champ == "recu_a_s");
    return 0;
}

constexpr int prevision_par_jour_facultative_mais_complete() {
    { // pas de bloc daily : accepté, aucun jour
        const Tampon t = remplace(PARIS, R"("daily":{"time")", R"("dailx":{"time")");
        VERIFIER(t.n != 0);
        const Analyse a = analyser(t.vue(), RECU);
        VERIFIER(a.ok() && a.releve.nb_jours == 0);
        VERIFIER(a.releve.temperature_dixiemes == 116); // le reste est intact
    }
    // chaque colonne manquante : le nom de la colonne est rendu
    VERIFIER(refuse(R"("time":[1793919600,)", R"("timx":[1793919600,)", Erreur::ChampAbsent, "daily.time"));
    VERIFIER(refuse(R"("weather_code":[3,45)", R"("weather_cod":[3,45)", Erreur::ChampAbsent, "daily.weather_code"));
    VERIFIER(refuse(R"("temperature_2m_max":[)", R"("temperature_2m_mxx":[)", Erreur::ChampAbsent,
                    "daily.temperature_2m_max"));
    VERIFIER(refuse(R"("temperature_2m_min":[)", R"("temperature_2m_mnn":[)", Erreur::ChampAbsent,
                    "daily.temperature_2m_min"));
    VERIFIER(refuse(R"("sunrise":[)", R"("sunrisx":[)", Erreur::ChampAbsent, "daily.sunrise"));
    VERIFIER(refuse(R"("sunset":[)", R"("sunsex":[)", Erreur::ChampAbsent, "daily.sunset"));
    VERIFIER(refuse(R"("uv_index_max":[)", R"("uv_index_mxx":[)", Erreur::ChampAbsent, "daily.uv_index_max"));
    VERIFIER(refuse(R"("precipitation_probability_max":[)", R"("precipitation_probability_mxx":[)", Erreur::ChampAbsent,
                    "daily.precipitation_probability_max"));
    // tableaux de longueurs différentes
    VERIFIER(refuse(R"("temperature_2m_max":[12.8,)", R"("temperature_2m_max":[)", Erreur::TableauxIncoherents,
                    "daily.temperature_2m_max"));
    VERIFIER(refuse(R"("precipitation_probability_max":[15,)", R"("precipitation_probability_max":[)",
                    Erreur::TableauxIncoherents, "daily.precipitation_probability_max"));
    // valeurs nulles dans une colonne
    VERIFIER(refuse(R"("temperature_2m_min":[7.3,)", R"("temperature_2m_min":[null,)", Erreur::ChampAbsent,
                    "daily.temperature_2m_min"));
    VERIFIER(refuse(R"("precipitation_probability_max":[15,)", R"("precipitation_probability_max":[null,)",
                    Erreur::ChampAbsent, "daily.precipitation_probability_max"));
    VERIFIER(refuse(R"("weather_code":[3,45)", R"("weather_code":[3.5,45)", Erreur::MauvaisType, "daily.weather_code"));
    // incohérences
    VERIFIER(refuse(R"("temperature_2m_min":[7.3,)", R"("temperature_2m_min":[20.0,)", Erreur::HorsPlage,
                    "daily.temperature_2m_min"));
    VERIFIER(accepte(R"("temperature_2m_min":[7.3,)", R"("temperature_2m_min":[12.8,)")); // min == max : possible
    VERIFIER(refuse(R"("sunrise":[1793947517,)", R"("sunrise":[1793999999,)", Erreur::HorsPlage, "daily.sunset"));
    VERIFIER(accepte(R"("sunrise":[1793947517,)", R"("sunrise":[1793982156,)")); // lever == coucher : nuit polaire
    VERIFIER(refuse(R"("uv_index_max":[1.35,)", R"("uv_index_max":[25.1,)", Erreur::HorsPlage, "daily.uv_index_max"));
    VERIFIER(accepte(R"("uv_index_max":[1.35,)", R"("uv_index_max":[25.0,)"));
    VERIFIER(refuse(R"("precipitation_probability_max":[15,)", R"("precipitation_probability_max":[101,)",
                    Erreur::HorsPlage, "daily.precipitation_probability_max"));
    VERIFIER(refuse(R"("precipitation_probability_max":[15,)", R"("precipitation_probability_max":[-1,)",
                    Erreur::HorsPlage, "daily.precipitation_probability_max"));
    // dates : un jour dure de 23 h à 25 h
    VERIFIER(accepte(R"("time":[1793919600,1794006000,)", R"("time":[1793919600,1794002400,)")); // +23 h puis +25 h
    VERIFIER(refuse(R"("time":[1793919600,1794006000,)", R"("time":[1793919600,1794002399,)",
                    Erreur::TableauxIncoherents, "daily.time"));
    VERIFIER(refuse(R"("time":[1793919600,1794006000,)", R"("time":[1793919600,1794013200,)",
                    Erreur::TableauxIncoherents, "daily.time"));
    return 0;
}

constexpr int plus_de_sept_jours_garde_les_sept_premiers() {
    // huit jours : on ajoute un jour AVANT le premier dans chacune des huit colonnes
    Tampon t = remplace(PARIS, R"("time":[1793919600,)", R"("time":[1793833200,1793919600,)");
    VERIFIER(t.n != 0);
    const sv colonnes[][2] = {{R"("weather_code":[3,45)", R"("weather_code":[0,3,45)"},
                              {R"("temperature_2m_max":[12.8,)", R"("temperature_2m_max":[1.0,12.8,)"},
                              {R"("temperature_2m_min":[7.3,)", R"("temperature_2m_min":[-2.0,7.3,)"},
                              {R"("sunrise":[1793947517,)", R"("sunrise":[1793861000,1793947517,)"},
                              {R"("sunset":[1793982156,)", R"("sunset":[1793895000,1793982156,)"},
                              {R"("uv_index_max":[1.35,)", R"("uv_index_max":[0.5,1.35,)"},
                              {R"("precipitation_probability_max":[15,)", R"("precipitation_probability_max":[7,15,)"}};
    for (const auto &c : colonnes) {
        t = remplace(t.vue(), c[0], c[1]);
        VERIFIER(t.n != 0);
    }
    VERIFIER(json::lire(t.vue()).valeur.trouver("daily.time").taille() == 8);
    const Analyse a = analyser(t.vue(), RECU);
    VERIFIER(a.ok());
    VERIFIER(a.releve.nb_jours == JOURS_MAX);
    VERIFIER(a.releve.jours[0].date_s == 1793833200 && a.releve.jours[0].max_dixiemes == 10);
    VERIFIER(a.releve.jours[1].date_s == 1793919600 && a.releve.jours[1].max_dixiemes == 128);
    VERIFIER(a.releve.jours[6].date_s == 1794351600); // le 7e : l'avant-dernier de la réponse d'origine
    return 0;
}

constexpr int hourly_facultatif() {
    { // pas de bloc hourly : la probabilité est INCONNUE (-1), pas 0
        const Tampon t = remplace(PARIS, R"("hourly":{"time")", R"("hourlx":{"time")");
        VERIFIER(t.n != 0);
        const Analyse a = analyser(t.vue(), RECU);
        VERIFIER(a.ok() && a.releve.proba_precip_pct == PROBA_INCONNUE);
    }
    { // pas de précipitations dans hourly : idem
        const Tampon t = remplace(PARIS, R"("precipitation_probability":[)", R"("precipitation_probabilitx":[)");
        VERIFIER(t.n != 0);
        const Analyse a = analyser(t.vue(), RECU);
        VERIFIER(a.ok() && a.releve.proba_precip_pct == PROBA_INCONNUE);
    }
    VERIFIER(analyser(PARIS, RECU).releve.proba_precip_pct == 0);
    // durées incohérentes : tableaux de longueurs différentes
    VERIFIER(refuse(R"("precipitation_probability":[0,)", R"("precipitation_probability":[)",
                    Erreur::TableauxIncoherents, "hourly.precipitation_probability"));
    VERIFIER(refuse(R"("hourly":{"time")", R"("hourly":5,"x":{"time")", Erreur::MauvaisType, "hourly"));
    return 0;
}

// ---- 5. fraîcheur ---------------------------------------------------------------------------------------------------------------

constexpr int fraicheur_aux_bornes() {
    Releve r;
    r.recu_a_s = 1793966500;
    constexpr std::int64_t T = 1793966500;
    VERIFIER(DELAI_PERIME_S == 3600 && DELAI_ABANDON_S == 21600 && TOLERANCE_HORLOGE_S == 300);
    VERIFIER(fraicheur(r, T) == Fraicheur::Frais);
    VERIFIER(fraicheur(r, T + 3600) == Fraicheur::Frais); // exactement 1 h : encore frais
    VERIFIER(fraicheur(r, T + 3601) == Fraicheur::Perime);
    VERIFIER(fraicheur(r, T + 21600) == Fraicheur::Perime); // exactement 6 h : encore périmé
    VERIFIER(fraicheur(r, T + 21601) == Fraicheur::TresPerime);
    VERIFIER(fraicheur(r, T + 10 * 86400) == Fraicheur::TresPerime);
    VERIFIER(fraicheur(r, T - 300) == Fraicheur::Frais);     // horloge en retard de 5 min : toléré
    VERIFIER(fraicheur(r, T - 301) == Fraicheur::Incertain); // plus que la tolérance
    VERIFIER(fraicheur(r, 0) == Fraicheur::Incertain);       // SNTP pas encore passé : 1970
    VERIFIER(fraicheur(r, 946684799) == Fraicheur::Incertain);
    VERIFIER(fraicheur(r, 946684800 + 900) != Fraicheur::Frais); // un cache de 2026 lu avec une horloge de 2000
    Releve sans_date;                                            // recu_a_s = 0 : jamais reçu
    VERIFIER(fraicheur(sans_date, T) == Fraicheur::Incertain);
    VERIFIER(fraicheur(r, T + 100, 60, 120) == Fraicheur::Perime); // délais personnalisés
    VERIFIER(fraicheur(r, T + 59, 60, 120) == Fraicheur::Frais);
    VERIFIER(fraicheur(r, T + 121, 60, 120) == Fraicheur::TresPerime);
    return 0;
}

constexpr int index_du_jour_aux_bornes() {
    const Releve r = analyser(PARIS, RECU).releve;
    VERIFIER(index_du_jour(r, r.jours[0].date_s) == 0);
    VERIFIER(index_du_jour(r, r.jours[1].date_s - 1) == 0);
    VERIFIER(index_du_jour(r, r.jours[1].date_s) == 1);
    VERIFIER(index_du_jour(r, r.jours[6].date_s) == 6);
    VERIFIER(index_du_jour(r, r.jours[6].date_s + 86399) == 6);
    VERIFIER(index_du_jour(r, r.jours[6].date_s + 86400) == -1); // le relevé ne couvre plus ce jour
    VERIFIER(index_du_jour(r, r.jours[0].date_s - 1) == -1);
    VERIFIER(index_du_jour(r, RECU) == 0);
    const Releve vide;
    VERIFIER(index_du_jour(vide, RECU) == -1);
    return 0;
}

constexpr int messages_courts_en_francais_sans_code() {
    VERIFIER(message_court(Erreur::JsonInvalide) == "Réponse météo illisible");
    VERIFIER(message_court(Erreur::ChampAbsent) == "Donnée météo manquante");
    VERIFIER(message_court(Erreur::MauvaisType) == "Donnée météo inattendue");
    VERIFIER(message_court(Erreur::HorsPlage) == "Donnée météo invraisemblable");
    VERIFIER(message_court(Erreur::TableauxIncoherents) == "Prévisions incohérentes");
    VERIFIER(message_court(Erreur::Corrompu) == "Cache météo corrompu");
    VERIFIER(message_court(Erreur::VersionInconnue) == "Cache météo trop ancien");
    VERIFIER(message_court(Erreur::Aucune).empty()); // sans erreur : rien à dire (surtout pas « à jour »)
    return 0;
}

// ---- 6. cache binaire (NVS) -----------------------------------------------------------------------------------------------------

constexpr bool meme_contenu(const Releve &a, const Releve &b) {
    if (a.recu_a_s != b.recu_a_s || a.instant_s != b.instant_s || a.decalage_utc_s != b.decalage_utc_s)
        return false;
    if (sv(a.fuseau) != sv(b.fuseau))
        return false;
    if (a.temperature_dixiemes != b.temperature_dixiemes || a.ressentie_dixiemes != b.ressentie_dixiemes)
        return false;
    if (a.code_wmo != b.code_wmo || a.jour != b.jour)
        return false;
    if (a.vent_dixiemes_kmh != b.vent_dixiemes_kmh || a.rafales_dixiemes_kmh != b.rafales_dixiemes_kmh)
        return false;
    if (a.direction_vent_deg != b.direction_vent_deg || a.pression_dixiemes_hpa != b.pression_dixiemes_hpa)
        return false;
    if (a.humidite_pct != b.humidite_pct || a.proba_precip_pct != b.proba_precip_pct || a.nb_jours != b.nb_jours)
        return false;
    for (int i = 0; i < a.nb_jours; ++i) {
        const Jour &x = a.jours[i], &y = b.jours[i];
        if (x.date_s != y.date_s || x.code_wmo != y.code_wmo || x.min_dixiemes != y.min_dixiemes ||
            x.max_dixiemes != y.max_dixiemes || x.lever_s != y.lever_s || x.coucher_s != y.coucher_s ||
            x.uv_max_dixiemes != y.uv_max_dixiemes || x.proba_precip_max != y.proba_precip_max) {
            return false;
        }
    }
    return true;
}

constexpr int crc32_vecteur_connu() {
    constexpr std::uint8_t NEUF[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    VERIFIER(detail::crc32(NEUF, 9) == 0xCBF43926U); // valeur de référence publiée du CRC-32/ISO-HDLC
    VERIFIER(detail::crc32(NEUF, 0) == 0U);
    return 0;
}

constexpr int cache_aller_retour_identique() {
    for (sv doc : {PARIS, LONGYEARBYEN}) {
        const Releve r = analyser(doc, RECU).releve;
        std::uint8_t tampon[TAILLE_CACHE_MAX] = {};
        const std::size_t n = serialiser(r, tampon, sizeof tampon);
        VERIFIER(n > 0 && n <= TAILLE_CACHE_MAX);
        const Lecture l = deserialiser(tampon, n);
        VERIFIER(l.ok());
        VERIFIER(meme_contenu(l.releve, r));
        VERIFIER(fraicheur(l.releve, RECU + 1800) == Fraicheur::Frais); // l'horodatage de réception survit au cache
    }
    return 0;
}

constexpr int cache_taille_exacte() {
    std::uint8_t tampon[TAILLE_CACHE_MAX] = {};
    // 3 (marque+version) + 12 (3 instants/décalage) + 1+12 (fuseau « Europe/Paris ») + 17 (mesures) + 7 × 20 (jours) + 4 (CRC)
    VERIFIER(serialiser(analyser(PARIS, RECU).releve, tampon, sizeof tampon) == 3 + 12 + 1 + 12 + 17 + 140 + 4);
    VERIFIER(serialiser(analyser(PARIS, RECU).releve, tampon, sizeof tampon) == 189);
    VERIFIER(TAILLE_CACHE_MAX >= 189 && TAILLE_CACHE_MAX < 256); // tient dans un petit blob NVS
    Releve sans_jours = analyser(PARIS, RECU).releve;
    sans_jours.nb_jours = 0;
    VERIFIER(serialiser(sans_jours, tampon, sizeof tampon) == 189 - 140);
    return 0;
}

constexpr int cache_trop_petit_ou_releve_invraisemblable_refuses() {
    const Releve r = analyser(PARIS, RECU).releve;
    std::uint8_t tampon[TAILLE_CACHE_MAX] = {};
    for (std::size_t capacite = 0; capacite < 189; ++capacite)
        VERIFIER(serialiser(r, tampon, capacite) == 0);
    VERIFIER(serialiser(r, tampon, 189) == 189);
    VERIFIER(serialiser(r, nullptr, 500) == 0);
    Releve fou = r;
    fou.temperature_dixiemes = 5000;
    VERIFIER(serialiser(fou, tampon, sizeof tampon) == 0); // on n'écrit pas une mesure invraisemblable
    Releve vierge;                                         // rien reçu : recu_a_s = 0
    VERIFIER(serialiser(vierge, tampon, sizeof tampon) == 0);
    return 0;
}

constexpr int cache_corrompu_est_toujours_refuse() {
    const Releve r = analyser(PARIS, RECU).releve;
    std::uint8_t tampon[TAILLE_CACHE_MAX] = {};
    const std::size_t n = serialiser(r, tampon, sizeof tampon);
    VERIFIER(n == 189);
    for (std::size_t i = 0; i < n; ++i) { // un bit retourné, partout
        for (int bit : {0, 7}) {
            std::uint8_t copie[TAILLE_CACHE_MAX] = {};
            for (std::size_t k = 0; k < n; ++k)
                copie[k] = tampon[k];
            copie[i] = static_cast<std::uint8_t>(copie[i] ^ (1U << bit));
            const Lecture l = deserialiser(copie, n);
            VERIFIER(!l.ok());
        }
    }
    for (std::size_t m = 0; m < n; ++m)
        VERIFIER(!deserialiser(tampon, m).ok()); // tronqué
    VERIFIER(!deserialiser(nullptr, 100).ok());
    VERIFIER(deserialiser(tampon, n - 1).diag.erreur == Erreur::Corrompu);
    return 0;
}

constexpr void recalculer_crc(std::uint8_t *t, std::size_t n) {
    const std::uint32_t c = detail::crc32(t, n - 4);
    for (int i = 0; i < 4; ++i)
        t[n - 4 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((c >> (8 * i)) & 0xFFU);
}

constexpr int cache_de_bonne_somme_mais_autre_version_ou_valeurs_folles() {
    const Releve r = analyser(PARIS, RECU).releve;
    std::uint8_t ref[TAILLE_CACHE_MAX] = {};
    const std::size_t n = serialiser(r, ref, sizeof ref);
    VERIFIER(n == 189);
    { // version 2 avec une somme juste : « version inconnue », pas « corrompu »
        std::uint8_t t[TAILLE_CACHE_MAX] = {};
        for (std::size_t k = 0; k < n; ++k)
            t[k] = ref[k];
        t[2] = 2;
        recalculer_crc(t, n);
        const Lecture l = deserialiser(t, n);
        VERIFIER(!l.ok() && l.diag.erreur == Erreur::VersionInconnue);
    }
    { // mauvaise marque
        std::uint8_t t[TAILLE_CACHE_MAX] = {};
        for (std::size_t k = 0; k < n; ++k)
            t[k] = ref[k];
        t[0] = 'X';
        recalculer_crc(t, n);
        VERIFIER(deserialiser(t, n).diag.erreur == Erreur::Corrompu);
    }
    { // température 0x7FFF dixièmes (3276,7 °C) avec une somme juste : refusée comme invraisemblable
        std::uint8_t t[TAILLE_CACHE_MAX] = {};
        for (std::size_t k = 0; k < n; ++k)
            t[k] = ref[k];
        t[3 + 12 + 1 + 12] = 0xFF;
        t[3 + 12 + 1 + 12 + 1] = 0x7F;
        recalculer_crc(t, n);
        const Lecture l = deserialiser(t, n);
        VERIFIER(!l.ok() && l.diag.erreur == Erreur::HorsPlage && l.diag.champ == "current.temperature_2m");
    }
    { // nombre de jours = 8 avec une somme juste
        std::uint8_t t[TAILLE_CACHE_MAX] = {};
        for (std::size_t k = 0; k < n; ++k)
            t[k] = ref[k];
        t[3 + 12 + 1 + 12 + 16] = 8;
        recalculer_crc(t, n);
        VERIFIER(deserialiser(t, n).diag.erreur == Erreur::Corrompu);
    }
    for (std::size_t longueur : {std::size_t{47}, std::size_t{48}}) {
        // nom de fuseau rallongé avec des « a » (le reste du cache est décalé, la somme est refaite) :
        // 47 octets = le maximum, accepté ; 48 = pas de place pour le zéro, refusé comme corrompu
        std::uint8_t t[TAILLE_CACHE_MAX] = {};
        constexpr std::size_t FIN_NOM = 3 + 12 + 1 + 12; // « Europe/Paris » finit ici
        const std::size_t en_plus = longueur - 12;
        for (std::size_t k = 0; k < FIN_NOM; ++k)
            t[k] = ref[k];
        for (std::size_t k = 0; k < en_plus; ++k)
            t[FIN_NOM + k] = 'a';
        for (std::size_t k = FIN_NOM; k < n; ++k)
            t[k + en_plus] = ref[k];
        t[3 + 12] = static_cast<std::uint8_t>(longueur);
        recalculer_crc(t, n + en_plus);
        const Lecture l = deserialiser(t, n + en_plus);
        if (longueur == 47) {
            VERIFIER(l.ok() && sv(l.releve.fuseau).size() == 47 && l.releve.temperature_dixiemes == 116);
        } else {
            VERIFIER(!l.ok() && l.diag.erreur == Erreur::Corrompu && l.diag.champ == "cache.fuseau");
        }
    }
    return 0;
}

constexpr int cache_sans_jours_et_sans_proba_reste_valide() {
    Releve r = analyser(PARIS, RECU).releve;
    r.nb_jours = 0;
    r.proba_precip_pct = PROBA_INCONNUE;
    for (char &c : r.fuseau)
        c = '\0'; // rien après le zéro final : le contrôle l'exige
    std::uint8_t t[TAILLE_CACHE_MAX] = {};
    const std::size_t n = serialiser(r, t, sizeof t);
    VERIFIER(n > 0);
    const Lecture l = deserialiser(t, n);
    VERIFIER(l.ok() && l.releve.nb_jours == 0 && l.releve.proba_precip_pct == PROBA_INCONNUE &&
             l.releve.fuseau[0] == '\0');
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(les_tables_python_ne_sont_pas_vides)                                                                             \
    X(json_paris_nombres)                                                                                              \
    X(json_paris_textes_et_tailles)                                                                                    \
    X(json_longyearbyen_nombres)                                                                                       \
    X(json_longyearbyen_textes_et_tailles)                                                                             \
    X(releve_de_paris_identique_a_python)                                                                              \
    X(releve_de_longyearbyen_identique_a_python)                                                                       \
    X(valeurs_lues_a_la_main_dans_la_reponse_de_paris)                                                                 \
    X(nuit_polaire_lever_egal_coucher)                                                                                 \
    X(fuseaux_des_reponses_connus_de_la_table)                                                                         \
    X(codes_des_reponses_connus_de_la_table_wmo)                                                                       \
    X(proba_paris_par_heure)                                                                                           \
    X(proba_longyearbyen_par_heure)                                                                                    \
    X(champs_de_current_absents_nuls_ou_mal_types)                                                                     \
    X(valeurs_invraisemblables_et_bornes)                                                                              \
    X(fuseau_facultatif_mais_controle)                                                                                 \
    X(json_invalide_ou_qui_n_est_pas_un_objet)                                                                         \
    X(horloge_de_reception_invraisemblable)                                                                            \
    X(prevision_par_jour_facultative_mais_complete)                                                                    \
    X(plus_de_sept_jours_garde_les_sept_premiers)                                                                      \
    X(hourly_facultatif)                                                                                               \
    X(fraicheur_aux_bornes)                                                                                            \
    X(index_du_jour_aux_bornes)                                                                                        \
    X(messages_courts_en_francais_sans_code)                                                                           \
    X(crc32_vecteur_connu)                                                                                             \
    X(cache_aller_retour_identique)                                                                                    \
    X(cache_taille_exacte)                                                                                             \
    X(cache_trop_petit_ou_releve_invraisemblable_refuses)                                                              \
    X(cache_corrompu_est_toujours_refuse)                                                                              \
    X(cache_de_bonne_somme_mais_autre_version_ou_valeurs_folles)                                                       \
    X(cache_sans_jours_et_sans_proba_reste_valide)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
