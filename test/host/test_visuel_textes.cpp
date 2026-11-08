// Tests du noyau pur : les textes de l'écran principal hors table commune (visuel_textes.hpp).
#include <string_view>

#include "ecran/visuel_textes.hpp"
#include "mini.hpp"

using ecran::i18n::Langue;
using namespace ecran::visuel;

namespace {

constexpr int compter_marques(std::string_view s) {
    int n = 0;
    for (std::size_t i = 0; i + 1 < s.size(); ++i) {
        if (s[i] == '%')
            ++n;
    }
    return n;
}

constexpr int chaque_texte_a_ses_six_traductions() {
    for (const Texte &x : TEXTES) {
        VERIFIER(!x.fr.empty());
        for (std::string_view t : x.t) {
            VERIFIER(!t.empty());
        }
    }
    return 0;
}

constexpr int aucune_cle_n_est_en_double() {
    for (std::size_t i = 0; i < NOMBRE_TEXTES; ++i) {
        for (std::size_t j = i + 1; j < NOMBRE_TEXTES; ++j) {
            VERIFIER(TEXTES[i].fr != TEXTES[j].fr);
        }
    }
    return 0;
}

constexpr int les_marques_de_format_sont_les_memes_dans_toutes_les_langues() {
    for (const Texte &x : TEXTES) {
        const int attendu = compter_marques(x.fr);
        for (std::string_view t : x.t) {
            VERIFIER(compter_marques(t) == attendu);
        }
    }
    return 0;
}

constexpr int une_etiquette_reste_courte() {
    // Ce sont des étiquettes de petites cartes : 28 octets (accents compris) au plus, la plus longue étant une phase de lune.
    for (const Texte &x : TEXTES) {
        VERIFIER(x.fr.size() <= 28);
        for (std::string_view t : x.t) {
            VERIFIER(t.size() <= 28);
        }
    }
    return 0;
}

constexpr int la_langue_choisit_la_traduction_et_le_francais_est_la_cle() {
    VERIFIER(texte("Vent", Langue::Fr) == "Vent");
    VERIFIER(texte("Vent", Langue::En) == "Wind");
    VERIFIER(texte("Humidité", Langue::De) == "Feuchte");
    VERIFIER(texte("Réglages", Langue::Tr) == "Ayarlar");
    VERIFIER(texte("SO", Langue::En) == "SW");
    VERIFIER(texte("N", Langue::Tr) == "K");
    return 0;
}

constexpr int une_cle_absente_passe_par_la_table_commune_puis_le_francais() {
    // « Orage » n'est pas ici : il vient de la table commune des libellés météo.
    VERIFIER(texte("Orage", Langue::En) == ecran::i18n::tr("Orage", Langue::En));
    VERIFIER(texte("Orage", Langue::De) != "Orage");
    VERIFIER(texte("Texte qui n'existe nulle part", Langue::De) == "Texte qui n'existe nulle part");
    VERIFIER(texte("", Langue::De).empty());
    return 0;
}

constexpr int les_huit_phases_de_lune_et_les_huit_directions_sont_la() {
    constexpr std::string_view PHASES[] = {"Nouvelle lune",       "Premier croissant", "Premier quartier",
                                           "Gibbeuse croissante", "Pleine lune",       "Gibbeuse décroissante",
                                           "Dernier quartier",    "Dernier croissant"};
    constexpr std::string_view VENTS[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    for (std::string_view p : PHASES) {
        VERIFIER(texte(p, Langue::De) != p ||
                 p == "Nouvelle lune"); // une vraie traduction, pas le français rendu tel quel
        VERIFIER(!texte(p, Langue::Tr).empty());
    }
    for (std::string_view v : VENTS) {
        VERIFIER(!texte(v, Langue::Nl).empty());
    }
    // les marques de format des phrases à nombre
    VERIFIER(compter_marques(texte("dans %d min", Langue::Es)) == 1);
    return 0;
}

constexpr int les_phrases_de_l_accueil_ont_six_traductions_les_memes_marques_et_une_longueur_bornee() {
    for (const Texte &x : PHRASES) {
        VERIFIER(!x.fr.empty() && x.fr.size() <= 64);
        const int attendu = compter_marques(x.fr);
        VERIFIER(attendu <= 1); // une seule marque : `%s` ou `%d`
        for (std::string_view t : x.t) {
            VERIFIER(!t.empty() && t.size() <= 64);
            VERIFIER(t != x.fr); // une vraie traduction
            VERIFIER(compter_marques(t) == attendu);
        }
    }
    return 0;
}

constexpr int aucune_phrase_n_a_la_cle_d_une_etiquette() {
    for (const Texte &p : PHRASES) {
        for (const Texte &x : TEXTES) {
            VERIFIER(p.fr != x.fr);
        }
    }
    for (std::size_t i = 0; i < NOMBRE_PHRASES; ++i) {
        for (std::size_t j = i + 1; j < NOMBRE_PHRASES; ++j) {
            VERIFIER(PHRASES[i].fr != PHRASES[j].fr);
        }
    }
    return 0;
}

constexpr int les_phrases_de_l_accueil_se_traduisent_par_texte() {
    VERIFIER(texte("Pluie dès %s", Langue::Fr) == "Pluie dès %s");
    VERIFIER(texte("Pluie dès %s", Langue::En) == "Rain from %s");
    VERIFIER(texte("Pluie dans %d min", Langue::De) == "Regen in %d Min.");
    VERIFIER(texte("Pluie en cours", Langue::De) == "Es regnet");
    VERIFIER(texte("ressenti %s", Langue::Es) == "sensación %s");
    VERIFIER(texte("Données de %s · périmées", Langue::It) == "Dati delle %s · vecchi");
    VERIFIER(texte("Rien à signaler dans les prochaines heures", Langue::Nl) == "Niets te melden de komende uren");
    VERIFIER(compter_marques(texte("Pluie dès %s", Langue::Tr)) == 1);
    return 0;
}

constexpr int les_textes_du_mode_automatique_se_traduisent() {
    VERIFIER(texte("Auto · Météo", Langue::En) == "Auto · Weather");
    VERIFIER(texte("Auto · alerte orange", Langue::De) == "Auto · Warnung Orange");
    VERIFIER(texte("Auto en pause", Langue::De) == "Auto pausiert");
    VERIFIER(texte("Alerte rouge", Langue::Tr) == "Kırmızı uyarı");
    VERIFIER(texte("Pluie modérée", Langue::Es) == "Lluvia moderada");
    VERIFIER(texte("puis plus forte vers %s", Langue::Es) == "luego más fuerte hacia las %s");
    VERIFIER(texte("environ %d h", Langue::It) == "circa %d h");
    VERIFIER(compter_marques(texte("puis plus forte vers %s", Langue::Tr)) == 1);
    VERIFIER(compter_marques(texte("%s mm en tout", Langue::Tr)) == 1);
    VERIFIER(compter_marques(texte("environ %d h", Langue::Nl)) == 1);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_texte_a_ses_six_traductions)                                                                              \
    X(aucune_cle_n_est_en_double)                                                                                      \
    X(les_marques_de_format_sont_les_memes_dans_toutes_les_langues)                                                    \
    X(une_etiquette_reste_courte)                                                                                      \
    X(la_langue_choisit_la_traduction_et_le_francais_est_la_cle)                                                       \
    X(une_cle_absente_passe_par_la_table_commune_puis_le_francais)                                                     \
    X(les_huit_phases_de_lune_et_les_huit_directions_sont_la)                                                          \
    X(les_phrases_de_l_accueil_ont_six_traductions_les_memes_marques_et_une_longueur_bornee)                           \
    X(aucune_phrase_n_a_la_cle_d_une_etiquette)                                                                        \
    X(les_phrases_de_l_accueil_se_traduisent_par_texte)                                                                \
    X(les_textes_du_mode_automatique_se_traduisent)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
