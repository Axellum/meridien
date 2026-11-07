// Tests du noyau pur : dates, heures et durées ISO 8601 des réponses de MET Norway et du NWS (iso8601.hpp).
//
// Les valeurs UNIX attendues ont été calculées par Python (datetime), pas par le noyau. Les formes valides sont celles qu'emploient
// les deux services (relevées sur leurs réponses réelles du 07/11/2026) ; tout le reste est refusé et ne touche pas aux sorties.
#include <cstdint>
#include <string_view>

#include "ecran/iso8601.hpp"
#include "mini.hpp"

using namespace ecran::iso;

namespace {

constexpr std::int64_t INTACT = 123456789; // valeur de départ des sorties : un refus ne doit pas la changer

constexpr bool lire_instant(std::string_view t, std::int64_t &v) { return instant(t, v); }

constexpr int des_instants_valides() {
    std::int64_t v = INTACT;
    VERIFIER(instant("1970-01-01T00:00:00Z", v) && v == 0);
    VERIFIER(instant("1999-12-31T23:59:59Z", v) && v == 946684799);
    VERIFIER(instant("2000-02-29T00:00:00Z", v) && v == 951782400);
    VERIFIER(instant("2024-02-29T12:00:00Z", v) && v == 1709208000);
    VERIFIER(instant("2026-11-07T12:00:00Z", v) && v == 1794052800);      // la forme de MET Norway
    VERIFIER(instant("2026-11-07T01:00:00+00:00", v) && v == 1794013200); // la forme du NWS
    VERIFIER(instant("2026-12-31T23:59:59Z", v) && v == 1798761599);
    VERIFIER(instant("2038-01-19T03:14:08Z", v) && v == 2147483648LL); // au-delà de 2^31 : pas de dépassement
    VERIFIER(instant("2100-03-01T00:00:00Z", v) && v == 4107542400LL);
    return 0;
}

constexpr int un_decalage_est_retranche() {
    std::int64_t v = 0;
    VERIFIER(instant("2026-11-07T13:00:00+01:00", v) && v == 1794052800);
    VERIFIER(instant("2026-11-07T04:00:00-08:00", v) && v == 1794052800);
    VERIFIER(instant("2026-11-07T17:30:00+05:30", v) && v == 1794052800);
    VERIFIER(instant("2026-11-08T02:00:00+14:00", v) && v == 1794052800); // +14:00 (Kiritimati) est permis
    VERIFIER(instant("2026-11-06T22:00:00-14:00", v) && v == 1794052800); // 22:00 la veille à -14:00 = 12:00 UTC
    return 0;
}

constexpr int des_instants_invalides_sont_refuses_sans_toucher_la_sortie() {
    for (const std::string_view t :
         {std::string_view(""),
          std::string_view("demain"),
          std::string_view("2026-11-07"),
          std::string_view("2026-11-07T12:00"),
          std::string_view("2026-11-07T12:00:00"),       // pas de fuseau
          std::string_view("2026-11-07 12:00:00Z"),      // espace au lieu de T
          std::string_view("2026-11-07T12:00:00.000Z"),  // fraction de seconde : non lue
          std::string_view("2026-11-07T12:00:00z"),      // z minuscule
          std::string_view("2026-11-07T12:00:00Zx"),     // reste en queue
          std::string_view("2026-11-07T12:00:00+0100"),  // décalage sans deux-points
          std::string_view("2026-11-07T12:00:00+01"),    //
          std::string_view("2026-11-07T12:00:00+15:00"), // au-delà de 14 h
          std::string_view("2026-11-07T12:00:00+14:01"), //
          std::string_view("2026-11-07T12:00:00+05:60"), // minutes du décalage
          std::string_view("2026-11-07T24:00:00Z"),      // heure 24
          std::string_view("2026-11-07T12:60:00Z"),      //
          std::string_view("2026-11-07T12:00:60Z"),      // seconde 60 : pas de seconde intercalaire
          std::string_view("2026-00-07T12:00:00Z"),      // mois 0
          std::string_view("2026-13-07T12:00:00Z"),      // mois 13
          std::string_view("2026-11-00T12:00:00Z"),      // jour 0
          std::string_view("2026-11-31T12:00:00Z"),      // novembre n'a que 30 jours
          std::string_view("2026-02-29T12:00:00Z"),      // 2026 n'est pas bissextile
          std::string_view("2100-02-29T12:00:00Z"),      // 2100 non plus (siècle)
          std::string_view("20261107T120000Z"),          // forme condensée
          std::string_view("2026/11/07T12:00:00Z"),      //
          std::string_view("2o26-11-07T12:00:00Z"),      // une lettre dans l'année
          std::string_view("2026-11-07T12:00:00-"),      //
          std::string_view("+2026-11-07T12:00:00Z")}) {
        std::int64_t v = INTACT;
        if (lire_instant(t, v) || v != INTACT)
            return __LINE__;
    }
    return 0;
}

constexpr int des_durees_valides() {
    std::int64_t v = INTACT;
    VERIFIER(duree("PT3H", v) && v == 10800);
    VERIFIER(duree("PT1H", v) && v == 3600);
    VERIFIER(duree("PT30M", v) && v == 1800);
    VERIFIER(duree("PT90S", v) && v == 90);
    VERIFIER(duree("PT1H30M", v) && v == 5400);
    VERIFIER(duree("PT1H30M15S", v) && v == 5415);
    VERIFIER(duree("P1D", v) && v == 86400);
    VERIFIER(duree("P8D", v) && v == 691200);
    VERIFIER(duree("P1DT3H", v) && v == 97200);
    VERIFIER(duree("P7DT23H", v) && v == 687600); // la forme de validTimes du NWS
    VERIFIER(duree("P366D", v) && v == 31622400);
    VERIFIER(duree("PT0S", v) && v == 0); // valide en soi ; intervalle() la refuse
    return 0;
}

constexpr int des_durees_invalides_sont_refusees_sans_toucher_la_sortie() {
    for (const std::string_view t : {std::string_view(""),
                                     std::string_view("P"),
                                     std::string_view("PT"),
                                     std::string_view("P1"),
                                     std::string_view("PT1"),
                                     std::string_view("3H"),
                                     std::string_view("1H"),
                                     std::string_view("P1H"),    // heures sans T
                                     std::string_view("PT1D"),   // jours après T
                                     std::string_view("PT1H1H"), // composante en double
                                     std::string_view("PT1M1H"), // ordre
                                     std::string_view("P1DT"),   // T en queue
                                     std::string_view("PT-1H"),
                                     std::string_view("PT1.5H"),
                                     std::string_view("P1,5D"), // pas de signe ni de fraction
                                     std::string_view("P1Y"),
                                     std::string_view("P1M"),
                                     std::string_view("P1W"),  // ni années, ni mois, ni semaines
                                     std::string_view("PT1h"), // minuscule
                                     std::string_view("PT1H "),
                                     std::string_view(" PT1H"),             //
                                     std::string_view("P367D"),             // plus d'un an
                                     std::string_view("PT99999999999H")}) { // dépassement
        std::int64_t v = INTACT;
        if (duree(t, v) || v != INTACT)
            return __LINE__;
    }
    return 0;
}

constexpr int un_intervalle_du_nws() {
    std::int64_t debut = INTACT, d = INTACT;
    VERIFIER(intervalle("2026-11-07T01:00:00+00:00/PT3H", debut, d) && debut == 1794013200 && d == 10800);
    VERIFIER(intervalle("2026-11-07T06:00:00+00:00/P7DT19H", debut, d) && debut == 1794009600 + 6 * 3600 &&
             d == 7 * 86400 + 19 * 3600);
    VERIFIER(intervalle("2026-11-07T12:00:00Z/PT1H", debut, d) && debut == 1794052800 && d == 3600);
    return 0;
}

constexpr int un_intervalle_invalide_est_refuse_sans_toucher_les_sorties() {
    for (const std::string_view t :
         {std::string_view(""), std::string_view("/"), std::string_view("PT3H"),
          std::string_view("2026-11-07T01:00:00+00:00"), std::string_view("2026-11-07T01:00:00+00:00/"),
          std::string_view("/PT3H"),
          std::string_view("2026-11-07T01:00:00+00:00/PT0S"),      // durée nulle : pas un intervalle
          std::string_view("2026-11-07T01:00:00+00:00/PT3H/PT1H"), // deux barres
          std::string_view("2026-11-07T01:00:00+00:00/3H"), std::string_view("2026-13-07T01:00:00+00:00/PT3H"),
          std::string_view("2026-11-07T01:00:00+00:00 /PT3H")}) {
        std::int64_t debut = INTACT, d = INTACT;
        if (intervalle(t, debut, d) || debut != INTACT || d != INTACT)
            return __LINE__;
    }
    return 0;
}

constexpr int des_dates_http_valides() {
    std::int64_t v = INTACT;
    VERIFIER(date_http("Thu, 01 Jan 1970 00:00:00 GMT", v) && v == 0);
    VERIFIER(date_http("Sat, 07 Nov 2026 13:27:13 GMT", v) && v == 1794058033); // un Expires de MET Norway
    VERIFIER(date_http("Sun, 08 Nov 2026 12:57:18 GMT", v) && v == 1794142638); // un Expires de /points du NWS
    VERIFIER(date_http("Sat, 07 Nov 2026 12:56:41 GMT", v) && v == 1794056201); // un Last-Modified
    VERIFIER(date_http("Thu, 29 Feb 2024 12:00:00 GMT", v) && v == 1709208000);
    VERIFIER(date_http("Tue, 19 Jan 2038 03:14:08 GMT", v) && v == 2147483648LL); // au-delà de 2^31
    return 0;
}

constexpr int des_dates_http_invalides_sont_refusees_sans_toucher_la_sortie() {
    for (const std::string_view t :
         {std::string_view(""),
          std::string_view("0"),
          std::string_view("-1"),
          std::string_view("Sat, 07 Nov 2026 13:27:13 UTC"),    // seul GMT
          std::string_view("Sat, 07 Nov 2026 13:27:13 gmt"),    //
          std::string_view("Sat, 07 Nov 2026 13:27:13 GMT "),   // reste en queue
          std::string_view(" Sat, 07 Nov 2026 13:27:13 GMT"),   //
          std::string_view("Sat, 07 Nov 2026 13:27:13"),        // pas de fuseau
          std::string_view("Saturday, 07-Nov-26 13:27:13 GMT"), // forme obsolète (RFC 850)
          std::string_view("Sat Nov  7 13:27:13 2026"),         // forme obsolète (asctime)
          std::string_view("Sun, 07 Nov 2026 13:27:13 GMT"),    // 7 novembre 2026 est un samedi
          std::string_view("Fri, 07 Nov 2026 13:27:13 GMT"),    //
          std::string_view("Sat, 7 Nov 2026 13:27:13 GMT"),     // jour sur un chiffre
          std::string_view("Sat, 00 Nov 2026 13:27:13 GMT"),    // jour 0
          std::string_view("Mon, 31 Nov 2026 13:27:13 GMT"),    // novembre n'a que 30 jours
          std::string_view("Sat, 29 Feb 2026 13:27:13 GMT"),    // 2026 n'est pas bissextile
          std::string_view("Sat, 07 Nov 2026 24:00:00 GMT"),    // heure 24
          std::string_view("Sat, 07 Nov 2026 13:60:13 GMT"),    //
          std::string_view("Sat, 07 Nov 2026 13:27:60 GMT"),    //
          std::string_view("Sat, 07 Nov 2026 13:27:1x GMT"),    // un caractère qui n'est pas un chiffre
          std::string_view("Sat, 07 Nox 2026 13:27:13 GMT"),    // mois inconnu
          std::string_view("Sat, 07 nov 2026 13:27:13 GMT"),    // minuscule
          std::string_view("Sax, 07 Nov 2026 13:27:13 GMT"),    // jour de la semaine inconnu
          std::string_view("Sat; 07 Nov 2026 13:27:13 GMT"),    // séparateur
          std::string_view("Sat,07 Nov 2026 13:27:13 GMT")}) {  // espace manquante
        std::int64_t v = INTACT;
        if (date_http(t, v) || v != INTACT)
            return __LINE__;
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(des_instants_valides)                                                                                            \
    X(un_decalage_est_retranche)                                                                                       \
    X(des_instants_invalides_sont_refuses_sans_toucher_la_sortie)                                                      \
    X(des_durees_valides)                                                                                              \
    X(des_durees_invalides_sont_refusees_sans_toucher_la_sortie)                                                       \
    X(un_intervalle_du_nws)                                                                                            \
    X(un_intervalle_invalide_est_refuse_sans_toucher_les_sorties)                                                      \
    X(des_dates_http_valides)                                                                                          \
    X(des_dates_http_invalides_sont_refusees_sans_toucher_la_sortie)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
