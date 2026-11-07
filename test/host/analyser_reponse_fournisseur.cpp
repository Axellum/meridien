// Outil (pas un test : le lanceur ne prend que test_*.cpp) : passe une réponse RÉELLE de MET Norway ou du NWS dans le MÊME lecteur
// que le firmware et écrit TOUT ce qu'il en garde, en JSON (une valeur par champ, entiers du noyau : dixièmes, secondes UNIX,
// -32768 pour « inconnu » en dixièmes, -1 pour « inconnu » en entier).
//
//   analyser_reponse_fournisseur metno <reponse.json> <reçu_à_s> <lat_e5> <lon_e5> <fuseau IANA>
//   analyser_reponse_fournisseur nws <points.json> <grille.json> <reçu_à_s> <lat_e5> <lon_e5>
//
// Sert à test/host/capturer_fournisseurs_meteo.py (ce que l'écran garderait) et à test/host/controle_python_fournisseurs.py
// (qui compare chaque champ à sa propre lecture de la réponse, faite sans ce code).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include "ecran/meteo_metno.hpp"
#include "ecran/meteo_nws.hpp"
#include "ecran/meteo_riche.hpp"

using namespace ecran::meteo;

namespace {

bool lire_fichier(const char *chemin, std::string &texte) {
    std::ifstream f(chemin, std::ios::binary);
    if (!f)
        return false;
    texte.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

void sec(const char *nom, const Diagnostic &d) {
    if (d.ok()) {
        std::printf("\"%s\":null,", nom);
        return;
    }
    std::printf("\"%s\":\"%d %.*s\",", nom, static_cast<int>(d.erreur), static_cast<int>(d.champ.size()),
                d.champ.data());
}

void ecrire(const AnalyseRiche &a) {
    std::printf("{");
    sec("diag", a.diag);
    sec("diag_heures", a.diag_heures);
    sec("diag_jours", a.diag_jours);
    sec("diag_quarts", a.diag_quarts);
    const Releve &r = a.meteo.releve;
    const Riche &q = a.meteo.riche;
    std::printf(
        "\"releve\":{\"recu_a_s\":%lld,\"instant_s\":%lld,\"decalage_utc_s\":%d,\"fuseau\":\"%s\","
        "\"temperature\":%d,\"ressentie\":%d,\"code\":%d,\"jour\":%d,\"vent\":%d,\"rafales\":%d,\"direction\":%d,"
        "\"pression\":%d,\"humidite\":%d,\"proba\":%d,\"nb_jours\":%d,\"jours\":[",
        static_cast<long long>(r.recu_a_s), static_cast<long long>(r.instant_s), static_cast<int>(r.decalage_utc_s),
        r.fuseau, r.temperature_dixiemes, r.ressentie_dixiemes, r.code_wmo, r.jour ? 1 : 0, r.vent_dixiemes_kmh,
        r.rafales_dixiemes_kmh, r.direction_vent_deg, r.pression_dixiemes_hpa, r.humidite_pct, r.proba_precip_pct,
        r.nb_jours);
    for (int i = 0; i < r.nb_jours; ++i) {
        const Jour &j = r.jours[i];
        std::printf(
            "%s{\"date_s\":%lld,\"code\":%d,\"min\":%d,\"max\":%d,\"lever_s\":%lld,\"coucher_s\":%lld,\"uv_max\":%d,"
            "\"proba\":%d}",
            i ? "," : "", static_cast<long long>(j.date_s), j.code_wmo, j.min_dixiemes, j.max_dixiemes,
            static_cast<long long>(j.lever_s), static_cast<long long>(j.coucher_s), j.uv_max_dixiemes,
            j.proba_precip_max);
    }
    std::printf("]},\"riche\":{\"precip_courante\":%d,\"nuages\":%d,\"nb_heures\":%d,\"heures\":[",
                q.precip_courante_dixiemes, q.nuages_pct, q.nb_heures);
    for (int i = 0; i < q.nb_heures; ++i) {
        const HeureRiche &h = q.heures[i];
        std::printf(
            "%s{\"debut_s\":%lld,\"temperature\":%d,\"ressentie\":%d,\"humidite\":%d,\"proba\":%d,\"precip\":%d,"
            "\"code\":%d,\"nuages\":%d,\"vent\":%d,\"rafales\":%d,\"direction\":%d,\"uv\":%d,\"jour\":%d}",
            i ? "," : "", static_cast<long long>(h.debut_s), h.temperature_dixiemes, h.ressentie_dixiemes,
            h.humidite_pct, h.proba_pct, h.precip_dixiemes_mm, h.code_wmo, h.nuages_pct, h.vent_dixiemes_kmh,
            h.rafales_dixiemes_kmh, h.direction_deg, h.uv_dixiemes, h.jour ? 1 : 0);
    }
    std::printf("],\"nb_jours\":%d,\"jours\":[", q.nb_jours);
    for (int i = 0; i < q.nb_jours; ++i) {
        const JourRiche &c = q.jours[i];
        std::printf("%s{\"ressentie_min\":%d,\"ressentie_max\":%d,\"ensoleillement\":%d,\"precip\":%d,\"neige\":%d,"
                    "\"heures_precip\":%d,\"vent_max\":%d,\"rafales_max\":%d,\"direction\":%d}",
                    i ? "," : "", c.ressentie_min_dixiemes, c.ressentie_max_dixiemes, c.ensoleillement_min,
                    c.precip_dixiemes_mm, c.neige_dixiemes_cm, c.heures_precip, c.vent_max_dixiemes_kmh,
                    c.rafales_max_dixiemes_kmh, c.direction_dominante_deg);
    }
    std::printf("],\"nb_quarts\":%d}}\n", q.nb_quarts);
}

} // namespace

int main(int argc, char **argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "metno" && argc == 7) {
        std::string texte;
        if (!lire_fichier(argv[2], texte)) {
            std::fprintf(stderr, "illisible : %s\n", argv[2]);
            return 2;
        }
        auto serie = std::make_unique<metno::Serie>();
        ecrire(metno::analyser_metno(texte, std::strtoll(argv[3], nullptr, 10),
                                     static_cast<std::int32_t>(std::strtol(argv[4], nullptr, 10)),
                                     static_cast<std::int32_t>(std::strtol(argv[5], nullptr, 10)), argv[6], *serie));
        return 0;
    }
    if (mode == "nws" && argc == 7) {
        std::string points, grille;
        if (!lire_fichier(argv[2], points) || !lire_fichier(argv[3], grille)) {
            std::fprintf(stderr, "illisible : %s ou %s\n", argv[2], argv[3]);
            return 2;
        }
        nws::Maille maille;
        const Diagnostic dp = nws::analyser_points(points, maille);
        if (!dp.ok()) {
            std::printf("{\"points\":\"%d %.*s\"}\n", static_cast<int>(dp.erreur), static_cast<int>(dp.champ.size()),
                        dp.champ.data());
            return 0;
        }
        auto serie = std::make_unique<nws::Serie>();
        std::printf("{\"maille\":{\"bureau\":\"%s\",\"x\":%d,\"y\":%d,\"fuseau\":\"%s\"},\"analyse\":", maille.bureau,
                    maille.x, maille.y, maille.fuseau);
        ecrire(nws::analyser_nws(grille, maille, std::strtoll(argv[4], nullptr, 10),
                                 static_cast<std::int32_t>(std::strtol(argv[5], nullptr, 10)),
                                 static_cast<std::int32_t>(std::strtol(argv[6], nullptr, 10)), *serie));
        std::printf("}\n");
        return 0;
    }
    std::fprintf(stderr,
                 "usage : %s metno reponse.json recu_a_s lat_e5 lon_e5 fuseau_iana\n"
                 "        %s nws points.json grille.json recu_a_s lat_e5 lon_e5\n",
                 argv[0], argv[0]);
    return 2;
}
