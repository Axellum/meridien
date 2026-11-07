// Outil (pas un test : le lanceur ne prend que test_*.cpp) : passe des réponses Open-Meteo réelles dans le MÊME lecteur que le
// firmware (`analyser_riche`, ecran/meteo_riche.hpp) et dit, pour chacune, ce que l'écran en garderait.
//
//   analyser_reponse_riche <fichier.json> <instant UNIX de réception> [<fichier.json> <instant> ...]
//
// Une ligne par fichier, champs séparés par « | » : nom ; verdict de base (ok ou refus + champ) ; heures ; jours ; quarts d'heure.
// Chaque section est « n » (nombre gardé) ou « vide (pourquoi : champ) » : c'est exactement ce que `analyser_riche` vide et
// masque déjà, rien n'est recalculé ici. Appelé par test/host/capturer_modeles_meteo.py.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "ecran/meteo_riche.hpp"

using namespace ecran::meteo;

namespace {

void ecrire_diag(const char *nom, std::size_t n, const Diagnostic &d) {
    if (d.ok()) {
        std::printf("%s=%zu", nom, n);
        return;
    }
    const auto m = message_court(d.erreur);
    std::printf("%s=vide(%.*s : %.*s)", nom, static_cast<int>(m.size()), m.data(), static_cast<int>(d.champ.size()),
                d.champ.data());
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3 || (argc - 1) % 2 != 0) {
        std::fprintf(stderr, "usage : %s fichier.json instant_unix [fichier.json instant_unix ...]\n", argv[0]);
        return 2;
    }
    for (int i = 1; i + 1 < argc; i += 2) {
        std::ifstream f(argv[i], std::ios::binary);
        if (!f) {
            std::printf("%s | illisible\n", argv[i]);
            continue;
        }
        const std::string texte((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const AnalyseRiche a = analyser_riche(texte, std::strtoll(argv[i + 1], nullptr, 10));
        std::printf("%s | ", argv[i]);
        if (!a.ok()) {
            const auto m = message_court(a.diag.erreur);
            std::printf("REFUSEE %.*s : %.*s\n", static_cast<int>(m.size()), m.data(),
                        static_cast<int>(a.diag.champ.size()), a.diag.champ.data());
            continue;
        }
        std::printf("ok | ");
        ecrire_diag("heures", a.meteo.riche.nb_heures, a.diag_heures);
        std::printf(" | ");
        ecrire_diag("jours", a.meteo.riche.nb_jours, a.diag_jours);
        std::printf(" | ");
        ecrire_diag("quarts", a.meteo.riche.nb_quarts, a.diag_quarts);
        std::printf(" | releve.jours=%u\n", static_cast<unsigned>(a.meteo.releve.nb_jours));
    }
    return 0;
}
