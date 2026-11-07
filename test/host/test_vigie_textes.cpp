// Tests du noyau pur : les mots propres à l'écran des alertes en sept langues et la date d'un instant (vigie_textes.hpp).
// Les mots de l'ÉMETTEUR ne passent jamais par ce fichier : ils sont testés dans test_vigie_alertes.cpp.
#include <cstddef>
#include <string_view>

#include "ecran/heure_fr.hpp"
#include "ecran/vigie_textes.hpp"
#include "mini.hpp"

using ecran::heure::HeureLocale;
using ecran::i18n::Langue;
using namespace ecran::vigie;

namespace {

constexpr bool egal(const char *a, std::string_view b) { return std::string_view(a) == b; }

constexpr bool est_une_cle(std::string_view cle) {
    for (const Mot &m : MOTS) {
        if (m.fr == cle)
            return true;
    }
    return false;
}

constexpr int chaque_mot_a_ses_six_traductions() {
    for (const Mot &m : MOTS) {
        VERIFIER(!m.fr.empty());
        for (std::string_view t : m.t)
            VERIFIER(!t.empty());
    }
    return 0;
}

constexpr int aucune_cle_n_est_en_double() {
    for (std::size_t i = 0; i < NOMBRE_MOTS; ++i) {
        for (std::size_t j = i + 1; j < NOMBRE_MOTS; ++j)
            VERIFIER(MOTS[i].fr != MOTS[j].fr);
    }
    return 0;
}

constexpr int la_langue_choisit_la_bonne_colonne() {
    // Trois entrées lues sur les sept langues : un décalage de colonne se verrait ici.
    constexpr std::string_view VENT[] = {"Vent", "Wind", "Wind", "Wind", "Viento", "Vento", "Rüzgâr"};
    constexpr std::string_view DEBUT[] = {"Début", "Start", "Beginn", "Begin", "Inicio", "Inizio", "Başlangıç"};
    constexpr std::string_view ROUGE[] = {"Rouge", "Red", "Rot", "Rood", "Rojo", "Rosso", "Kırmızı"};
    for (int l = 0; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
        const Langue langue = static_cast<Langue>(l);
        VERIFIER(mot("Vent", langue) == VENT[l]);
        VERIFIER(mot("Début", langue) == DEBUT[l]);
        VERIFIER(mot("Rouge", langue) == ROUGE[l]);
    }
    return 0;
}

constexpr int une_cle_inconnue_est_rendue_telle_quelle() {
    VERIFIER(mot("Texte qui n'existe nulle part", Langue::Tr) == "Texte qui n'existe nulle part");
    VERIFIER(mot("", Langue::De).empty());
    VERIFIER(mot("vent", Langue::En) == "vent"); // la casse compte : « Vent » est la clé
    return 0;
}

constexpr int une_traduction_vide_retombe_sur_l_anglais_puis_sur_le_francais() {
    constexpr Mot sans_turc{"Brume", {"Mist", "Dunst", "Nevel", "Neblina", "Foschia", ""}};
    VERIFIER(traduction(sans_turc, Langue::Tr) == "Mist");
    VERIFIER(traduction(sans_turc, Langue::De) == "Dunst");
    VERIFIER(traduction(sans_turc, Langue::Fr) == "Brume");
    constexpr Mot sans_anglais{"Brume", {"", "Dunst", "Nevel", "Neblina", "Foschia", ""}};
    VERIFIER(traduction(sans_anglais, Langue::Tr) == "Brume");
    VERIFIER(traduction(sans_anglais, Langue::En) == "Brume");
    VERIFIER(traduction(sans_anglais, Langue::Es) == "Neblina");
    return 0;
}

constexpr int chaque_phenomene_et_chaque_niveau_a_son_mot_dans_la_table() {
    for (int p = 0; p <= 12; ++p) {
        const std::string_view cle = cle_phenomene(static_cast<Phenomene>(p));
        VERIFIER(!cle.empty() && est_une_cle(cle));
        for (int q = p + 1; q <= 12; ++q)
            VERIFIER(cle != cle_phenomene(static_cast<Phenomene>(q))); // deux phénomènes ne partagent pas un mot
    }
    VERIFIER(cle_phenomene(Phenomene::Inconnu) == "Alerte météo");
    VERIFIER(cle_phenomene(Phenomene::Autre) == "Alerte");
    VERIFIER(cle_phenomene(Phenomene::Vent) == "Vent");
    VERIFIER(cle_phenomene(Phenomene::NeigeVerglas) == "Neige et verglas");
    VERIFIER(cle_niveau(Niveau::Jaune) == "Jaune" && est_une_cle("Jaune"));
    VERIFIER(cle_niveau(Niveau::Orange) == "Orange" && est_une_cle("Orange"));
    VERIFIER(cle_niveau(Niveau::Rouge) == "Rouge" && est_une_cle("Rouge"));
    VERIFIER(cle_niveau(Niveau::Vert).empty() &&
             cle_niveau(Niveau::Inconnu).empty()); // pas de mot pour ce qui ne se montre pas
    return 0;
}

constexpr int le_libelle_derive_dit_le_phenomene_puis_le_niveau_s_il_est_connu() {
    char t[64] = {};
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Fr, t, sizeof t) > 0 &&
             egal(t, "Pluie \xC2\xB7 Orange"));
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::En, t, sizeof t) > 0 &&
             egal(t, "Rain \xC2\xB7 Orange"));
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Tr, t, sizeof t) > 0 &&
             egal(t, "Yağmur \xC2\xB7 Turuncu"));
    VERIFIER(libelle_derive(Phenomene::Inconnu, Niveau::Rouge, Langue::De, t, sizeof t) > 0 &&
             egal(t, "Wetterwarnung \xC2\xB7 Rot"));
    VERIFIER(libelle_derive(Phenomene::Vent, Niveau::Inconnu, Langue::Es, t, sizeof t) == 6 &&
             egal(t, "Viento")); // niveau inconnu : pas de niveau
    VERIFIER(libelle_derive(Phenomene::Vent, Niveau::Vert, Langue::Fr, t, sizeof t) == 4 && egal(t, "Vent"));
    return 0;
}

constexpr int le_libelle_derive_tient_ou_donne_un_tampon_vide() {
    char t[64] = {};
    // « Pluie · Orange » : 5 + 4 (espace, « · » sur deux octets, espace) + 6 = 15 octets, 16 avec le zéro.
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Fr, t, 16) == 15 &&
             egal(t, "Pluie \xC2\xB7 Orange"));
    t[0] = 'x';
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Fr, t, 15) == 0 && t[0] == '\0');
    t[0] = 'x';
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Fr, t, 1) == 0 && t[0] == '\0');
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Fr, nullptr, 64) == 0);
    VERIFIER(libelle_derive(Phenomene::Pluie, Niveau::Orange, Langue::Fr, t, 0) == 0);
    return 0;
}

constexpr int le_libelle_le_plus_long_tient_dans_quarante_octets() {
    // La bande réserve la place d'une ligne : le libellé de repli le plus bavard, dans la langue la plus bavarde, reste court.
    char t[64] = {};
    for (int l = 0; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
        for (int p = 0; p <= 12; ++p) {
            for (const Niveau n : {Niveau::Inconnu, Niveau::Jaune, Niveau::Orange, Niveau::Rouge}) {
                const std::size_t k = libelle_derive(static_cast<Phenomene>(p), n, static_cast<Langue>(l), t, sizeof t);
                VERIFIER(k > 0 && k <= 40);
            }
        }
    }
    return 0;
}

// Samedi 7 novembre 2026, 10:00 à Paris (CET, UTC+1) : l'instant 1794042000, lu avec Python (datetime), pas avec le noyau.
constexpr HeureLocale SAMEDI_DIX_HEURES = {2026, 11, 7, 10, 0, 0, 6, 3600, false};

constexpr int un_instant_s_ecrit_jour_mois_et_heure_dans_la_langue_de_l_ecran() {
    char t[64] = {};
    VERIFIER(formater_instant(Langue::Fr, SAMEDI_DIX_HEURES, t, sizeof t) > 0 &&
             egal(t, "10:00 \xC2\xB7 samedi 7 novembre"));
    VERIFIER(formater_instant(Langue::En, SAMEDI_DIX_HEURES, t, sizeof t) > 0 &&
             egal(t, "10:00 \xC2\xB7 Saturday, 7 November"));
    VERIFIER(formater_instant(Langue::De, SAMEDI_DIX_HEURES, t, sizeof t) > 0 &&
             egal(t, "10:00 \xC2\xB7 Samstag, 7. November"));
    VERIFIER(formater_instant(Langue::Es, SAMEDI_DIX_HEURES, t, sizeof t) > 0 && egal(t, "10:00 \xC2\xB7 s\xC3\xA1"
                                                                                         "bado, 7 de noviembre"));
    VERIFIER(formater_instant(Langue::Tr, SAMEDI_DIX_HEURES, t, sizeof t) > 0 &&
             egal(t, "10:00 \xC2\xB7 7 Kas\xC4\xB1m Cumartesi"));
    return 0;
}

constexpr int les_minutes_et_les_heures_ont_toujours_deux_chiffres() {
    char t[64] = {};
    HeureLocale h = SAMEDI_DIX_HEURES;
    h.heure = 0;
    h.minute = 7;
    VERIFIER(formater_instant(Langue::Fr, h, t, sizeof t) > 0 && egal(t, "00:07 \xC2\xB7 samedi 7 novembre"));
    h.heure = 9;
    h.minute = 59;
    VERIFIER(formater_instant(Langue::Fr, h, t, sizeof t) > 0 && egal(t, "09:59 \xC2\xB7 samedi 7 novembre"));
    h.heure = 23;
    h.minute = 0;
    VERIFIER(formater_instant(Langue::Fr, h, t, sizeof t) > 0 && egal(t, "23:00 \xC2\xB7 samedi 7 novembre"));
    return 0;
}

constexpr int une_date_invalide_ou_un_tampon_trop_petit_donne_un_texte_vide() {
    char t[64] = {'x'};
    HeureLocale h = SAMEDI_DIX_HEURES;
    h.mois = 13;
    VERIFIER(formater_instant(Langue::Fr, h, t, sizeof t) == 0 && t[0] == '\0');
    // Une heure ou une minute hors plage n'est pas écrite comme « 25:61 » : rien.
    const int heures_minutes[][2] = {{24, 0}, {-1, 0}, {10, 60}, {10, -1}};
    for (const auto &hm : heures_minutes) {
        h = SAMEDI_DIX_HEURES;
        h.heure = hm[0];
        h.minute = hm[1];
        t[0] = 'x';
        VERIFIER(formater_instant(Langue::Fr, h, t, sizeof t) == 0 && t[0] == '\0');
    }
    h = SAMEDI_DIX_HEURES;
    t[0] = 'x';
    // « 10:00 · samedi 7 novembre » : 5 + 4 (espace, « · » sur deux octets, espace) + 5 = 26 octets, 27 avec le zéro.
    VERIFIER(formater_instant(Langue::Fr, h, t, 27) == 26 && egal(t, "10:00 \xC2\xB7 samedi 7 novembre"));
    t[0] = 'x';
    VERIFIER(formater_instant(Langue::Fr, h, t, 26) == 0 && t[0] == '\0');
    t[0] = 'x';
    VERIFIER(formater_instant(Langue::Fr, h, t, 20) == 0 &&
             t[0] == '\0'); // la date tient, pas l'heure : rien plutôt que la moitié
    VERIFIER(formater_instant(Langue::Fr, h, nullptr, 64) == 0);
    // Pas de sortie, ou une sortie de capacité zéro, ET une heure invalide : on ne touche à rien (écrire un zéro final planterait).
    h.heure = 24;
    VERIFIER(formater_instant(Langue::Fr, h, nullptr, 64) == 0);
    t[0] = 'x';
    VERIFIER(formater_instant(Langue::Fr, h, t, 0) == 0 && t[0] == 'x');
    return 0;
}

constexpr int la_date_la_plus_longue_avec_l_heure_tient_dans_cinquante_octets() {
    char t[80] = {};
    HeureLocale h = SAMEDI_DIX_HEURES;
    for (int l = 0; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
        for (int mois = 1; mois <= 12; ++mois) {
            for (int jour_sem = 0; jour_sem < 7; ++jour_sem) {
                h.mois = mois;
                h.jour_semaine = jour_sem;
                h.jour = 28;
                const std::size_t n = formater_instant(static_cast<Langue>(l), h, t, sizeof t);
                VERIFIER(n > 0 && n <= 50);
            }
        }
    }
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_mot_a_ses_six_traductions)                                                                                \
    X(aucune_cle_n_est_en_double)                                                                                      \
    X(la_langue_choisit_la_bonne_colonne)                                                                              \
    X(une_cle_inconnue_est_rendue_telle_quelle)                                                                        \
    X(une_traduction_vide_retombe_sur_l_anglais_puis_sur_le_francais)                                                  \
    X(chaque_phenomene_et_chaque_niveau_a_son_mot_dans_la_table)                                                       \
    X(le_libelle_derive_dit_le_phenomene_puis_le_niveau_s_il_est_connu)                                                \
    X(le_libelle_derive_tient_ou_donne_un_tampon_vide)                                                                 \
    X(le_libelle_le_plus_long_tient_dans_quarante_octets)                                                              \
    X(un_instant_s_ecrit_jour_mois_et_heure_dans_la_langue_de_l_ecran)                                                 \
    X(les_minutes_et_les_heures_ont_toujours_deux_chiffres)                                                            \
    X(une_date_invalide_ou_un_tampon_trop_petit_donne_un_texte_vide)                                                   \
    X(la_date_la_plus_longue_avec_l_heure_tient_dans_cinquante_octets)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
