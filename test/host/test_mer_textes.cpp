// Tests du noyau pur : les mots de la page mer en sept langues et la mise en forme de ce qu'elle écrit (mer_textes.hpp). Les tables sont
// vérifiées par propriétés (chaque ligne a ses six traductions, les marqueurs sont les mêmes partout, aucune paire en double) et par des
// exemples écrits en dur ; les formateurs par ce qu'ils écrivent, ce qu'ils refusent d'écrire, et un tampon trop petit.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/mer_textes.hpp"
#include "mini.hpp"

using namespace ecran::mer;
using ecran::i18n::Langue;

namespace {

constexpr std::int64_t MINUIT = 1794096000; // 2026-11-08 00:00:00 UTC
constexpr std::int64_t H(int h, int m = 0) { return MINUIT + h * 3600 + m * 60; }

// Ce que dit un formateur : rend vrai s'il a écrit exactement `attendu`, zéro final compris et octets de reste intacts.
template <typename F> constexpr bool dit(F f, std::string_view attendu) {
    char b[200] = {};
    for (char &c : b)
        c = 'X';
    const std::size_t n = f(b, sizeof b);
    return n == attendu.size() && std::string_view(b, n) == attendu && b[n] == '\0';
}

// Ce qu'un formateur refuse de dire : 0 octet et un tampon VIDÉ (jamais une valeur plausible, jamais un reste de l'appel d'avant).
template <typename F> constexpr bool se_tait(F f, std::size_t capacite = 200) {
    char b[200] = {};
    for (char &c : b)
        c = 'X';
    const std::size_t n = f(b, capacite < sizeof b ? capacite : sizeof b);
    return n == 0 && b[0] == '\0';
}

constexpr bool contient(std::string_view t, std::string_view a) { return t.find(a) != std::string_view::npos; }

constexpr int nombre_de(std::string_view t, char c) {
    int n = 0;
    for (char x : t)
        n += x == c ? 1 : 0;
    return n;
}

// ---- les tables ----------------------------------------------------------------------------------------------------------------

// Les mots sont dans la table commune (i18n/*.yaml, section « Écran : la page mer ») depuis le 11/11/2026 : les tables de
// mer_textes.hpp ne portent plus que la clé française, rangée dans l'ordre des énumérations.
constexpr bool a_ses_six_traductions(std::string_view cle) {
    if (cle.empty() || !ecran::i18n::est_une_cle(cle))
        return false;
    for (int l = 1; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l)
        if (ecran::i18n::propre(cle, static_cast<Langue>(l)).empty())
            return false;
    return true;
}

constexpr int chaque_mot_a_ses_six_traductions() {
    for (Mot m : TEXTES)
        VERIFIER(a_ses_six_traductions(m));
    for (Mot m : ACTIVITES)
        VERIFIER(a_ses_six_traductions(m));
    for (int v = 1; v < 4; ++v)
        VERIFIER(a_ses_six_traductions(VERDICTS[v]));
    for (int c = 1; c < static_cast<int>(Critere::Nombre); ++c)
        VERIFIER(a_ses_six_traductions(CRITERES[c]));
    for (const TexteEtat &e : ETATS)
        VERIFIER(a_ses_six_traductions(e.mot));
    return 0;
}

constexpr int aucune_cle_n_est_en_double() {
    for (std::size_t i = 0; i < NOMBRE_TEXTES; ++i)
        for (std::size_t j = i + 1; j < NOMBRE_TEXTES; ++j)
            VERIFIER(TEXTES[i] != TEXTES[j]);
    // Une paire (critère, état) ne se dit qu'une fois.
    for (std::size_t i = 0; i < NOMBRE_ETATS; ++i)
        for (std::size_t j = i + 1; j < NOMBRE_ETATS; ++j)
            VERIFIER(!(ETATS[i].critere == ETATS[j].critere && ETATS[i].etat == ETATS[j].etat));
    return 0;
}

constexpr int une_traduction_garde_les_marqueurs_de_la_cle_francaise() {
    constexpr std::string_view NOMS[] = {"{n}",   "{age}",      "{niveau}",  "{heure}", "{points}",
                                         "{sur}", "{activite}", "{critere}", "{etat}"};
    for (Mot m : TEXTES) {
        for (int l = 1; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
            const std::string_view t = ecran::i18n::propre(m, static_cast<Langue>(l));
            VERIFIER(nombre_de(t, '{') == nombre_de(m, '{'));
            VERIFIER(nombre_de(t, '}') == nombre_de(m, '}'));
            for (std::string_view nom : NOMS)
                VERIFIER(contient(t, nom) == contient(m, nom));
        }
    }
    return 0;
}

constexpr int la_langue_choisit_la_bonne_colonne() {
    // Trois textes lus dans les sept langues : un décalage de colonne se verrait ici.
    constexpr std::string_view SOLEIL[] = {"Soleil", "Sun", "Sonne", "Zon", "Sol", "Sole", "Güneş"};
    constexpr std::string_view PLEINE[] = {"Pleine mer", "High water", "Hochwasser",   "Hoogwater",
                                           "Pleamar",    "Alta marea", "Yüksek gelgit"};
    constexpr std::string_view BON[] = {"Bon", "Good", "Gut", "Goed", "Bueno", "Buono", "İyi"};
    for (int l = 0; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
        const Langue langue = static_cast<Langue>(l);
        VERIFIER(texte(Texte::Soleil, langue) == SOLEIL[l]);
        VERIFIER(texte(Texte::PleineMer, langue) == PLEINE[l]);
        VERIFIER(nom_verdict(Verdict::Bon, langue) == BON[l]);
    }
    return 0;
}

constexpr int le_credit_et_l_avertissement_de_navigation_sont_dans_les_sept_langues() {
    // CC BY 4.0 (Open-Meteo, Copernicus Marine, EOT20) : les trois sources et la licence, telles quelles, dans chaque langue ; et
    // l'avertissement de l'ADR-0018, jamais vide.
    constexpr std::string_view NOMS[] = {"Open-Meteo", "Copernicus Marine", "EOT20", "(CC BY 4.0)"};
    for (int l = 0; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l) {
        const Langue langue = static_cast<Langue>(l);
        for (std::string_view nom : NOMS)
            VERIFIER(contient(texte(Texte::Credit, langue), nom));
        VERIFIER(!texte(Texte::PasNavigation, langue).empty());
    }
    VERIFIER(texte(Texte::PasNavigation, Langue::En) == "Estimates, not for navigation");
    return 0;
}

constexpr int une_valeur_hors_enumeration_ne_dit_rien() {
    VERIFIER(texte(Texte::Nombre, Langue::En).empty());
    VERIFIER(nom_activite(Activite::Nombre, Langue::Fr).empty());
    VERIFIER(nom_verdict(Verdict::Inconnu, Langue::Fr).empty());
    VERIFIER(nom_verdict(static_cast<Verdict>(9), Langue::Fr).empty());
    VERIFIER(nom_critere(Critere::Inconnu, Langue::Fr).empty());
    VERIFIER(nom_critere(Critere::Nombre, Langue::Fr).empty());
    return 0;
}

constexpr int les_quatre_activites_les_trois_verdicts_et_les_criteres_ont_leur_mot() {
    VERIFIER(nom_activite(Activite::Surf, Langue::En) == "Surf");
    VERIFIER(nom_activite(Activite::Kite, Langue::En) == "Kite / wing");
    VERIFIER(nom_activite(Activite::Bateau, Langue::En) == "Boat");
    VERIFIER(nom_activite(Activite::PecheAPied, Langue::En) == "Shore fishing");
    VERIFIER(nom_activite(Activite::PecheAPied, Langue::Fr) == "Pêche à pied");
    VERIFIER(nom_verdict(Verdict::Bon, Langue::Fr) == "Bon");
    VERIFIER(nom_verdict(Verdict::Possible, Langue::Fr) == "Possible");
    VERIFIER(nom_verdict(Verdict::Deconseille, Langue::Fr) == "Déconseillé");
    VERIFIER(nom_verdict(Verdict::Deconseille, Langue::En) == "Not advised");
    // Les critères suivent l'ordre de l'énumération : un décalage donnerait « Marée » à la place de « Vent ».
    VERIFIER(nom_critere(Critere::HauteurHoule, Langue::Fr) == "Houle");
    VERIFIER(nom_critere(Critere::Vent, Langue::Fr) == "Vent");
    VERIFIER(nom_critere(Critere::Maree, Langue::Fr) == "Marée");
    VERIFIER(nom_critere(Critere::EcartRafales, Langue::Fr) == "Écart des rafales");
    VERIFIER(nom_critere(Critere::ForceRafales, Langue::Fr) == "Rafales");
    VERIFIER(nom_critere(Critere::HauteurMer, Langue::Fr) == "Mer");
    VERIFIER(nom_critere(Critere::BasseMer, Langue::Fr) == "Basse mer");
    VERIFIER(nom_critere(Critere::Coefficient, Langue::Fr) == "Coefficient (est.)");
    VERIFIER(nom_critere(Critere::Vigilance, Langue::Fr) == "Vigilance côtière");
    VERIFIER(nom_critere(Critere::Vigilance, Langue::De) == "Küstenwarnung");
    return 0;
}

constexpr int chaque_paire_du_contrat_a_son_mot_et_aucune_autre() {
    struct Paire {
        Critere c;
        Etat e;
    };
    // La table des critères du contrat (vigie-api, docs/embedded-frame.md), recopiée ici.
    constexpr Paire CONTRAT[] = {
        {Critere::HauteurHoule, Etat::TropPetite},
        {Critere::HauteurHoule, Etat::Petite},
        {Critere::HauteurHoule, Etat::Bonne},
        {Critere::HauteurHoule, Etat::Grosse},
        {Critere::HauteurHoule, Etat::TropGrosse},
        {Critere::PeriodeHoule, Etat::Courte},
        {Critere::PeriodeHoule, Etat::Moyenne},
        {Critere::PeriodeHoule, Etat::Longue},
        {Critere::DirectionHoule, Etat::DeFace},
        {Critere::DirectionHoule, Etat::Oblique},
        {Critere::DirectionHoule, Etat::HorsAxe},
        {Critere::Vent, Etat::Faible},
        {Critere::Vent, Etat::Offshore},
        {Critere::Vent, Etat::Travers},
        {Critere::Vent, Etat::Onshore},
        {Critere::Vent, Etat::Forte},
        {Critere::Vent, Etat::DansLaPlage},
        {Critere::Vent, Etat::UnPeuFaible},
        {Critere::Vent, Etat::UnPeuForte},
        {Critere::Vent, Etat::TropFaible},
        {Critere::Vent, Etat::TropFort},
        {Critere::Maree, Etat::BonMoment},
        {Critere::Maree, Etat::MauvaisMoment},
        {Critere::EcartRafales, Etat::Regulier},
        {Critere::EcartRafales, Etat::Rafaleux},
        {Critere::EcartRafales, Etat::TresRafaleux},
        {Critere::DirectionVent, Etat::Onshore},
        {Critere::DirectionVent, Etat::Offshore},
        {Critere::DirectionVent, Etat::Travers},
        {Critere::ForceVent, Etat::JusquaModere},
        {Critere::ForceVent, Etat::Frais},
        {Critere::ForceVent, Etat::FortOuPlus},
        {Critere::ForceVent, Etat::JusquaFrais},
        {Critere::ForceVent, Etat::Forte},
        {Critere::ForceVent, Etat::QuasiCoupDeVentOuPlus},
        {Critere::ForceRafales, Etat::JusquaFrais},
        {Critere::ForceRafales, Etat::Forte},
        {Critere::ForceRafales, Etat::CoupDeVentOuPlus},
        {Critere::HauteurMer, Etat::Basse},
        {Critere::HauteurMer, Etat::Modere},
        {Critere::HauteurMer, Etat::Haute},
        {Critere::BasseMer, Etat::DansLaFenetre},
        {Critere::BasseMer, Etat::HorsFenetre},
        {Critere::Coefficient, Etat::Petite},
        {Critere::Coefficient, Etat::Modere},
        {Critere::Coefficient, Etat::Grand},
        {Critere::Coefficient, Etat::TresGrand},
        {Critere::Vigilance, Etat::Orange},
        {Critere::Vigilance, Etat::Rouge},
    };
    VERIFIER(sizeof(CONTRAT) / sizeof(CONTRAT[0]) == 49);
    VERIFIER(NOMBRE_ETATS == 49);
    for (const Paire &p : CONTRAT)
        for (int l = 0; l < static_cast<int>(ecran::i18n::NOMBRE_LANGUES); ++l)
            VERIFIER(!texte_etat(p.c, p.e, static_cast<Langue>(l)).empty());
    return 0;
}

constexpr int une_paire_que_le_contrat_ne_liste_pas_ne_dit_rien() {
    VERIFIER(texte_etat(Critere::HauteurHoule, Etat::Onshore, Langue::Fr).empty());
    VERIFIER(texte_etat(Critere::Maree, Etat::Faible, Langue::En).empty());
    VERIFIER(texte_etat(Critere::Inconnu, Etat::Faible, Langue::En).empty());
    VERIFIER(texte_etat(Critere::Vent, Etat::Inconnu, Langue::En).empty());
    VERIFIER(texte_etat(Critere::Vigilance, Etat::Faible, Langue::En).empty());
    return 0;
}

constexpr int le_meme_etat_n_a_pas_le_meme_mot_selon_le_critere() {
    // « faible » en français est « light » pour le vent, « low » pour la mer, « small » pour le coefficient.
    VERIFIER(texte_etat(Critere::Vent, Etat::Faible, Langue::En) == "light");
    VERIFIER(texte_etat(Critere::HauteurMer, Etat::Basse, Langue::En) == "low");
    VERIFIER(texte_etat(Critere::Coefficient, Etat::Petite, Langue::En) == "small");
    VERIFIER(texte_etat(Critere::HauteurHoule, Etat::Petite, Langue::En) == "small");
    VERIFIER(texte_etat(Critere::Vent, Etat::Faible, Langue::Fr) == "faible");
    // Le même état « petite » : le mot français de la houle n'est pas celui du coefficient, ni l'allemand.
    VERIFIER(texte_etat(Critere::HauteurHoule, Etat::Petite, Langue::Fr) == "petite");
    VERIFIER(texte_etat(Critere::Coefficient, Etat::Petite, Langue::Fr) == "faible");
    VERIFIER(texte_etat(Critere::HauteurHoule, Etat::Petite, Langue::De) == "klein");
    VERIFIER(texte_etat(Critere::Coefficient, Etat::Petite, Langue::De) == "niedrig");
    VERIFIER(texte_etat(Critere::Vent, Etat::Forte, Langue::De) == "stark");
    VERIFIER(texte_etat(Critere::HauteurHoule, Etat::TropPetite, Langue::Es) == "demasiado pequeña");
    VERIFIER(texte_etat(Critere::Vigilance, Etat::Rouge, Langue::Tr) == "kırmızı");
    return 0;
}

// ---- les heures ---------------------------------------------------------------------------------------------------------------

constexpr int une_heure_s_ecrit_hh_mm_a_l_heure_du_lieu() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(14, 5), 0, b, c); }, "14:05"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(14, 5), 3600, b, c); }, "15:05"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(14, 5), 7200, b, c); }, "16:05"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(0, 0), 0, b, c); }, "00:00"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(9, 9), 0, b, c); }, "09:09"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(23, 59), 0, b, c); }, "23:59"));
    // Le décalage passe minuit dans les deux sens.
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(23, 30), 3600, b, c); }, "00:30"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(0, 30), -3600, b, c); }, "23:30"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(5, 45), -10 * 3600, b, c); }, "19:45"));
    // Un demi-décalage (Inde, +5 h 30).
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(10, 0), 19800, b, c); }, "15:30"));
    return 0;
}

constexpr int sans_instant_il_n_y_a_pas_d_heure() {
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_heure(0, 0, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_heure(-5, 0, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_heure(H(10), 0, b, c); },
                     5)); // 5 octets : pas de place pour le zéro
    VERIFIER(dit([](char *b, std::size_t c) { return formater_heure(H(10), 0, b, c); }, "10:00"));
    VERIFIER(formater_heure(H(10), 0, nullptr, 8) == 0);
    VERIFIER(formater_heure(H(10), 0, nullptr, 0) == 0);
    return 0;
}

constexpr int un_creneau_s_ecrit_avec_un_tiret_demi_cadratin() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_creneau(H(14), H(16), 0, b, c); }, "14:00\xE2\x80\x93"
                                                                                                 "16:00"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_creneau(H(14), H(16), 7200, b, c); }, "16:00\xE2\x80\x93"
                                                                                                    "18:00"));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_creneau(0, H(16), 0, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_creneau(H(14), 0, 0, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_creneau(H(14), H(14), 0, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_creneau(H(16), H(14), 0, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_creneau(H(14), H(16), 0, b, c); }, 8));
    return 0;
}

constexpr int un_age_s_ecrit_dans_la_langue_avec_la_bonne_unite() {
    const std::int64_t t = H(12);
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t, Langue::Fr, b, c); }, "à l'instant"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 59, Langue::Fr, b, c); }, "à l'instant"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 60, Langue::Fr, b, c); }, "il y a 1 min"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_age(t, t - 12 * 60, Langue::Fr, b, c); }, "il y a 12 min"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 3599, Langue::Fr, b, c); }, "il y a 59 min"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 3600, Langue::Fr, b, c); }, "il y a 1 h"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 3 * 3600 - 59, Langue::Fr, b, c); },
                 "il y a 3 h"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 86399, Langue::Fr, b, c); }, "il y a 23 h"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 86400, Langue::Fr, b, c); }, "il y a 1 j"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_age(t, t - 2 * 86400, Langue::Fr, b, c); }, "il y a 2 j"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 12 * 60, Langue::En, b, c); }, "12 min ago"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 3 * 3600, Langue::En, b, c); }, "3 h ago"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 2 * 86400, Langue::En, b, c); }, "2 d ago"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_age(t, t - 12 * 60, Langue::De, b, c); }, "vor 12 Min."));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t - 12 * 60, Langue::Tr, b, c); }, "12 dk önce"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_age(t, t, Langue::Es, b, c); }, "ahora mismo"));
    return 0;
}

constexpr int un_age_sans_horloge_ou_sans_evenement_ne_se_dit_pas() {
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_age(0, H(12), Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_age(H(12), 0, Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_age(H(12), H(11), Langue::Fr, b, c); }, 5));
    return 0;
}

constexpr int un_evenement_dans_le_futur_compte_pour_a_l_instant() {
    // Une horloge en retard : jamais un âge négatif ni « il y a 0 min ».
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_age(H(12), H(12) + 3600, Langue::Fr, b, c); }, "à l'instant"));
    return 0;
}

// ---- les critères --------------------------------------------------------------------------------------------------------------

constexpr Critere_ligne ligne(Unite u, std::int32_t valeur_d) {
    Critere_ligne c;
    c.unite = u;
    c.valeur_d = valeur_d;
    return c;
}

constexpr int une_valeur_s_ecrit_avec_son_unite_et_le_separateur_de_la_langue() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 26), Langue::Fr, b, c); },
                 "2,6 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 26), Langue::En, b, c); },
                 "2.6 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 26), Langue::De, b, c); },
                 "2,6 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 9), Langue::Fr, b, c); },
                 "0,9 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 0), Langue::Fr, b, c); },
                 "0,0 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 120), Langue::Fr, b, c); },
                 "12,0 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Noeuds, 119), Langue::Fr, b, c); },
                 "11,9 kn"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Noeuds, 119), Langue::En, b, c); },
                 "11.9 kn"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Secondes, 100), Langue::Fr, b, c); },
                 "10 s"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Degres, 210), Langue::Fr, b, c); },
                 "21\xC2\xB0"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::KmH, 190), Langue::Fr, b, c); },
                 "19 km/h"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Minutes, 1020), Langue::Fr, b, c); },
                 "102 min"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Aucune, 910), Langue::Fr, b, c); }, "91"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Beaufort, 30), Langue::Fr, b, c); },
                 "Force 3"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Beaufort, 30), Langue::De, b, c); },
                 "Stärke 3"));
    return 0;
}

constexpr int une_valeur_entiere_s_arrondit_au_plus_proche_moitie_loin_de_zero() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Secondes, 104), Langue::Fr, b, c); },
                 "10 s"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Secondes, 105), Langue::Fr, b, c); },
                 "11 s"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Secondes, 106), Langue::Fr, b, c); },
                 "11 s"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Degres, -105), Langue::Fr, b, c); },
                 "-11\xC2\xB0"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Degres, -104), Langue::Fr, b, c); },
                 "-10\xC2\xB0"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::KmH, 4), Langue::Fr, b, c); }, "0 km/h"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::KmH, 5), Langue::Fr, b, c); }, "1 km/h"));
    return 0;
}

constexpr int une_valeur_negative_garde_son_signe() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, -3), Langue::Fr, b, c); },
                 "-0,3 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, -26), Langue::En, b, c); },
                 "-2.6 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, -100), Langue::Fr, b, c); },
                 "-10,0 m"));
    // Un entier négatif (degrés, valeur sans unité) garde aussi son signe.
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Degres, -30), Langue::Fr, b, c); },
                 "-3\xC2\xB0"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Aucune, -120), Langue::Fr, b, c); },
                 "-12"));
    return 0;
}

constexpr int une_valeur_ou_une_unite_inconnue_ne_s_ecrit_pas() {
    VERIFIER(se_tait(
        [](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, INCONNU), Langue::Fr, b, c); }));
    VERIFIER(se_tait(
        [](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Aucune, INCONNU), Langue::Fr, b, c); }));
    VERIFIER(
        se_tait([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Inconnue, 26), Langue::Fr, b, c); }));
    VERIFIER(
        se_tait([](char *b, std::size_t c) { return formater_valeur(ligne(Unite::Metres, 26), Langue::Fr, b, c); }, 5));
    return 0;
}

constexpr int les_points_d_un_critere_s_ecrivent_n_sur_max_ou_se_taisent() {
    Critere_ligne c;
    c.points = 2;
    c.points_max = 2;
    VERIFIER(dit([&](char *b, std::size_t n) { return formater_points(c, b, n); }, "2/2"));
    c.points = 0;
    c.points_max = 3;
    VERIFIER(dit([&](char *b, std::size_t n) { return formater_points(c, b, n); }, "0/3"));
    c.points = -1; // inconnu : jamais « 0/3 »
    VERIFIER(se_tait([&](char *b, std::size_t n) { return formater_points(c, b, n); }));
    c.points = 2;
    c.points_max = -1;
    VERIFIER(se_tait([&](char *b, std::size_t n) { return formater_points(c, b, n); }));
    c.points_max = 0;
    VERIFIER(se_tait([&](char *b, std::size_t n) { return formater_points(c, b, n); }));
    c.points = 4;
    c.points_max = 3;
    VERIFIER(se_tait([&](char *b, std::size_t n) { return formater_points(c, b, n); }));
    return 0;
}

constexpr int la_note_s_ecrit_dans_la_langue_ou_se_tait() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_note(7, 8, Langue::Fr, b, c); }, "Note 7 sur 8"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_note(7, 8, Langue::En, b, c); }, "Score 7 of 8"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_note(0, 8, Langue::Fr, b, c); }, "Note 0 sur 8"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_note(8, 8, Langue::Tr, b, c); }, "8 üzerinden 8 puan"));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_note(-1, 8, Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_note(7, 0, Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_note(7, -1, Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_note(9, 8, Langue::Fr, b, c); }));
    return 0;
}

constexpr Prevision avec_raison(Critere c, Etat e) {
    Prevision p;
    p.raison_critere = c;
    p.raison_etat = e;
    return p;
}

constexpr int la_raison_dit_le_critere_et_ce_que_la_regle_en_a_fait() {
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_raison(avec_raison(Critere::HauteurHoule, Etat::TropPetite), Langue::Fr, b, c);
        },
        "Houle : trop petite"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_raison(avec_raison(Critere::HauteurHoule, Etat::TropPetite), Langue::En, b, c);
        },
        "Swell: too small"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_raison(avec_raison(Critere::Vigilance, Etat::Rouge), Langue::Fr, b, c);
        },
        "Vigilance côtière : rouge"));
    VERIFIER(
        dit([](char *b,
               std::size_t c) { return formater_raison(avec_raison(Critere::Vent, Etat::TropFort), Langue::It, b, c); },
            "Vento: troppo forte"));
    return 0;
}

constexpr int une_raison_inconnue_ou_hors_vocabulaire_ne_s_ecrit_pas() {
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_raison(Prevision{}, Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_raison(avec_raison(Critere::Vent, Etat::Inconnu), Langue::Fr, b, c);
    }));
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_raison(avec_raison(Critere::Inconnu, Etat::Faible), Langue::Fr, b, c);
    }));
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_raison(avec_raison(Critere::Maree, Etat::Faible), Langue::Fr, b, c);
    }));
    VERIFIER(se_tait(
        [](char *b, std::size_t c) {
            return formater_raison(avec_raison(Critere::HauteurHoule, Etat::TropPetite), Langue::Fr, b, c);
        },
        8));
    return 0;
}

constexpr int le_titre_du_detail_et_son_heure() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_titre_detail(Activite::Surf, Langue::Fr, b, c); },
                 "Surf · détail"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_titre_detail(Activite::Surf, Langue::En, b, c); },
                 "Surf · detail"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, c); },
                 "Pêche à pied · détail"));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_titre_detail(Activite::Nombre, Langue::Fr, b, c); }));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_heure_detail(H(11), 0, Langue::Fr, b, c); }, "pour 11:00"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_heure_detail(H(11), 3600, Langue::De, b, c); }, "für 12:00"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_heure_detail(H(11), 0, Langue::Tr, b, c); }, "11:00 için"));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_heure_detail(0, 0, Langue::Fr, b, c); }));
    return 0;
}

constexpr int un_tampon_trop_petit_ne_coupe_jamais_un_caractere() {
    // « Pêche à pied · détail » fait plus de 8 octets : le texte coupé en deux (au milieu du « ê ») n'est jamais montré.
    VERIFIER(se_tait(
        [](char *b, std::size_t c) { return formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, c); }, 4));
    VERIFIER(se_tait(
        [](char *b, std::size_t c) { return formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, c); }, 5));
    VERIFIER(se_tait(
        [](char *b, std::size_t c) { return formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, c); }, 20));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, c); },
                 "Pêche à pied · détail"));
    // Au plus juste : 21 octets de texte + le zéro = 23 dans le tampon (le texte fait 23 octets avec ses accents).
    const std::string_view plein = "Pêche à pied · détail";
    char b[64] = {};
    VERIFIER(formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, plein.size() + 1) == plein.size());
    VERIFIER(formater_titre_detail(Activite::PecheAPied, Langue::Fr, b, plein.size()) == 0);
    return 0;
}

// ---- la marée -------------------------------------------------------------------------------------------------------------------

constexpr Maree maree(bool haute, std::int64_t instant, std::int32_t cm) {
    Maree m;
    m.haute = haute;
    m.instant_s = instant;
    m.hauteur_cm = cm;
    return m;
}

constexpr int une_maree_s_ecrit_avec_son_heure_et_sa_hauteur() {
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_maree(maree(true, H(14, 12), 190), 0, Langue::Fr, b, c); },
            "Pleine mer 14:12 · 1,9 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(false, H(8, 3), 40), 0, Langue::Fr, b, c); },
                 "Basse mer 08:03 · 0,4 m"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_maree(maree(true, H(14, 12), 190), 7200, Langue::En, b, c); },
            "High water 16:12 · 1.9 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(false, H(8, 3), 40), 0, Langue::De, b, c); },
                 "Niedrigwasser 08:03 · 0,4 m"));
    return 0;
}

constexpr int la_hauteur_d_une_maree_s_arrondit_au_dixieme_de_metre() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(true, H(14), 185), 0, Langue::Fr, b, c); },
                 "Pleine mer 14:00 · 1,9 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(true, H(14), 184), 0, Langue::Fr, b, c); },
                 "Pleine mer 14:00 · 1,8 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(true, H(14), 4), 0, Langue::Fr, b, c); },
                 "Pleine mer 14:00 · 0,0 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(false, H(14), -30), 0, Langue::Fr, b, c); },
                 "Basse mer 14:00 · -0,3 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(false, H(14), -25), 0, Langue::Fr, b, c); },
                 "Basse mer 14:00 · -0,3 m"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_maree(maree(false, H(14), -24), 0, Langue::Fr, b, c); },
                 "Basse mer 14:00 · -0,2 m"));
    return 0;
}

constexpr int une_maree_sans_hauteur_ou_sans_heure() {
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_maree(maree(true, H(14, 12), INCONNU), 0, Langue::Fr, b, c); },
            "Pleine mer 14:12"));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_maree(maree(true, 0, 190), 0, Langue::Fr, b, c); }));
    VERIFIER(se_tait(
        [](char *b, std::size_t c) { return formater_maree(maree(true, H(14, 12), 190), 0, Langue::Fr, b, c); }, 16));
    return 0;
}

constexpr int le_coefficient_dit_qu_il_est_estime() {
    VERIFIER(dit([](char *b, std::size_t c) { return formater_coefficient(91, Langue::Fr, b, c); },
                 "Coefficient 91 (estimé)"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_coefficient(91, Langue::En, b, c); },
                 "Coefficient 91 (estimated)"));
    VERIFIER(dit([](char *b, std::size_t c) { return formater_coefficient(120, Langue::It, b, c); },
                 "Coefficiente 120 (stimato)"));
    VERIFIER(
        dit([](char *b, std::size_t c) { return formater_coefficient(20, Langue::Tr, b, c); }, "Katsayı 20 (tahmini)"));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_coefficient(0, Langue::Fr, b, c); }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_coefficient(91, Langue::Fr, b, c); }, 10));
    return 0;
}

// ---- la vigilance et l'âge ------------------------------------------------------------------------------------------------------

constexpr Mer mer_avec_vigilance(bool lues, bool presente, ecran::vigie::Niveau niveau, std::int64_t fin) {
    Mer m;
    m.valide = true;
    m.genere_s = H(12);
    m.vigilances_lues = lues;
    m.vigilance_presente = presente;
    m.vigilance_niveau = niveau;
    m.vigilance_fin_s = fin;
    return m;
}

constexpr int la_vigilance_dit_son_niveau_et_sa_fin() {
    using N = ecran::vigie::Niveau;
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Orange, H(14)), 0, Langue::Fr, b, c);
        },
        "Vigilance côtière orange · jusqu'à 14:00"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Orange, H(14)), 3600, Langue::En, b, c);
        },
        "Coastal warning: orange · until 15:00"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Rouge, H(14)), 0, Langue::Fr, b, c);
        },
        "Vigilance côtière rouge · jusqu'à 14:00"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Jaune, H(14)), 0, Langue::Fr, b, c);
        },
        "Vigilance côtière jaune · jusqu'à 14:00"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Orange, H(14)), 0, Langue::Tr, b, c);
        },
        "Kıyı uyarısı: turuncu · 14:00 saatine kadar"));
    // Sans heure de fin : le niveau seul.
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Orange, 0), 0, Langue::Fr, b, c);
        },
        "Vigilance côtière orange"));
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Rouge, 0), 0, Langue::De, b, c);
        },
        "Küstenwarnung: rot"));
    return 0;
}

constexpr int des_vigilances_illisibles_ne_sont_jamais_un_tout_va_bien() {
    using N = ecran::vigie::Niveau;
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(false, false, N::Inconnu, 0), 0, Langue::Fr, b, c);
        },
        "Vigilances côtières indisponibles"));
    // Même si une vigilance était lue avant : illisible reste illisible.
    VERIFIER(dit(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(false, true, N::Rouge, H(14)), 0, Langue::En, b, c);
        },
        "Coastal warnings unavailable"));
    return 0;
}

constexpr int pas_de_vigilance_ou_verte_ou_mer_absente_rien_a_dire() {
    using N = ecran::vigie::Niveau;
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_vigilance(mer_avec_vigilance(true, false, N::Inconnu, 0), 0, Langue::Fr, b, c);
    }));
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_vigilance(mer_avec_vigilance(true, true, N::Vert, H(14)), 0, Langue::Fr, b, c);
    }));
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_vigilance(mer_avec_vigilance(true, true, N::Inconnu, H(14)), 0, Langue::Fr, b, c);
    }));
    VERIFIER(se_tait([](char *b, std::size_t c) {
        return formater_vigilance(mer_avec_vigilance(true, false, N::Rouge, H(14)), 0, Langue::Fr, b, c);
    }));
    VERIFIER(se_tait([](char *b, std::size_t c) { return formater_vigilance(Mer{}, 0, Langue::Fr, b, c); }));
    VERIFIER(se_tait(
        [](char *b, std::size_t c) {
            return formater_vigilance(mer_avec_vigilance(true, true, N::Orange, H(14)), 0, Langue::Fr, b, c);
        },
        12));
    return 0;
}

constexpr int l_etat_de_la_mer_dit_l_absence_la_peremption_ou_l_age() {
    Mer m;
    m.valide = true;
    m.genere_s = H(12);
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Rien, H(12), Langue::Fr, b, c); },
                 "Mer : données indisponibles"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Rien, H(12), Langue::En, b, c); },
                 "Sea data unavailable"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Expiree, H(20), Langue::Fr, b, c); },
                 "Données de mer trop anciennes"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Vieillie, H(12, 12), Langue::Fr, b, c); },
            "Mer · il y a 12 min"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Vieillie, H(12, 12), Langue::En, b, c); },
            "Sea data · 12 min ago"));
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Vieillie, H(12, 12), Langue::Nl, b, c); },
            "Zeegegevens · 12 min geleden"));
    VERIFIER(se_tait(
        [&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Fraiche, H(12, 1), Langue::Fr, b, c); }));
    // Pas d'horloge : l'âge n'est pas connu, la ligne ne l'invente pas.
    VERIFIER(
        se_tait([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Vieillie, 0, Langue::Fr, b, c); }));
    m.donnees_gardees = true; // un fournisseur a servi sa dernière réponse : on le dit, quel que soit l'âge
    VERIFIER(
        dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Vieillie, H(12, 1), Langue::Fr, b, c); },
            "Certaines données de mer datent d'une lecture précédente"));
    VERIFIER(dit([&](char *b, std::size_t c) { return formater_etat(m, Fraicheur::Vieillie, 0, Langue::En, b, c); },
                 "Some sea data is from an earlier reading"));
    return 0;
}

constexpr int le_bandeau_choisit_le_plus_important() {
    using N = ecran::vigie::Niveau;
    char b[200] = {};
    const std::size_t n = sizeof b;
    Mer orange = mer_avec_vigilance(true, true, N::Orange, H(14));
    Mer illisible = mer_avec_vigilance(false, false, N::Inconnu, 0);
    Mer calme = mer_avec_vigilance(true, false, N::Inconnu, 0);
    // Une vigilance fraîche passe devant tout, sauf une mer absente ou trop vieille.
    VERIFIER(composer_bandeau(orange, Fraicheur::Fraiche, H(12, 1), 0, Langue::Fr, b, n) == Bandeau::Vigilance);
    VERIFIER(std::string_view(b) == "Vigilance côtière orange · jusqu'à 14:00");
    VERIFIER(composer_bandeau(orange, Fraicheur::Vieillie, H(12, 20), 0, Langue::Fr, b, n) == Bandeau::Vigilance);
    VERIFIER(std::string_view(b) == "Vigilance côtière orange · jusqu'à 14:00");
    VERIFIER(composer_bandeau(orange, Fraicheur::Expiree, H(20), 0, Langue::Fr, b, n) == Bandeau::Etat);
    VERIFIER(std::string_view(b) == "Données de mer trop anciennes");
    VERIFIER(composer_bandeau(orange, Fraicheur::Rien, H(12), 0, Langue::Fr, b, n) == Bandeau::Etat);
    VERIFIER(std::string_view(b) == "Mer : données indisponibles");
    // Des vigilances illisibles sont à remarquer (jamais un « tout va bien »).
    VERIFIER(composer_bandeau(illisible, Fraicheur::Fraiche, H(12, 1), 0, Langue::Fr, b, n) ==
             Bandeau::VigilanceIllisible);
    VERIFIER(std::string_view(b) == "Vigilances côtières indisponibles");
    // Rien à craindre : le bandeau est calme si les données sont fraîches, il dit l'âge sinon.
    VERIFIER(composer_bandeau(calme, Fraicheur::Fraiche, H(12, 1), 0, Langue::Fr, b, n) == Bandeau::Rien);
    VERIFIER(b[0] == '\0');
    VERIFIER(composer_bandeau(calme, Fraicheur::Vieillie, H(12, 12), 0, Langue::Fr, b, n) == Bandeau::Etat);
    VERIFIER(std::string_view(b) == "Mer · il y a 12 min");
    VERIFIER(composer_bandeau(calme, Fraicheur::Fraiche, H(12, 1), 0, Langue::Fr, nullptr, n) == Bandeau::Rien);
    VERIFIER(composer_bandeau(calme, Fraicheur::Fraiche, H(12, 1), 0, Langue::Fr, b, 0) == Bandeau::Rien);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(chaque_mot_a_ses_six_traductions)                                                                                \
    X(aucune_cle_n_est_en_double)                                                                                      \
    X(une_traduction_garde_les_marqueurs_de_la_cle_francaise)                                                          \
    X(la_langue_choisit_la_bonne_colonne)                                                                              \
    X(le_credit_et_l_avertissement_de_navigation_sont_dans_les_sept_langues)                                           \
    X(une_valeur_hors_enumeration_ne_dit_rien)                                                                         \
    X(les_quatre_activites_les_trois_verdicts_et_les_criteres_ont_leur_mot)                                            \
    X(chaque_paire_du_contrat_a_son_mot_et_aucune_autre)                                                               \
    X(une_paire_que_le_contrat_ne_liste_pas_ne_dit_rien)                                                               \
    X(le_meme_etat_n_a_pas_le_meme_mot_selon_le_critere)                                                               \
    X(une_heure_s_ecrit_hh_mm_a_l_heure_du_lieu)                                                                       \
    X(sans_instant_il_n_y_a_pas_d_heure)                                                                               \
    X(un_creneau_s_ecrit_avec_un_tiret_demi_cadratin)                                                                  \
    X(un_age_s_ecrit_dans_la_langue_avec_la_bonne_unite)                                                               \
    X(un_age_sans_horloge_ou_sans_evenement_ne_se_dit_pas)                                                             \
    X(un_evenement_dans_le_futur_compte_pour_a_l_instant)                                                              \
    X(une_valeur_s_ecrit_avec_son_unite_et_le_separateur_de_la_langue)                                                 \
    X(une_valeur_entiere_s_arrondit_au_plus_proche_moitie_loin_de_zero)                                                \
    X(une_valeur_negative_garde_son_signe)                                                                             \
    X(une_valeur_ou_une_unite_inconnue_ne_s_ecrit_pas)                                                                 \
    X(les_points_d_un_critere_s_ecrivent_n_sur_max_ou_se_taisent)                                                      \
    X(la_note_s_ecrit_dans_la_langue_ou_se_tait)                                                                       \
    X(la_raison_dit_le_critere_et_ce_que_la_regle_en_a_fait)                                                           \
    X(une_raison_inconnue_ou_hors_vocabulaire_ne_s_ecrit_pas)                                                          \
    X(le_titre_du_detail_et_son_heure)                                                                                 \
    X(un_tampon_trop_petit_ne_coupe_jamais_un_caractere)                                                               \
    X(une_maree_s_ecrit_avec_son_heure_et_sa_hauteur)                                                                  \
    X(la_hauteur_d_une_maree_s_arrondit_au_dixieme_de_metre)                                                           \
    X(une_maree_sans_hauteur_ou_sans_heure)                                                                            \
    X(le_coefficient_dit_qu_il_est_estime)                                                                             \
    X(la_vigilance_dit_son_niveau_et_sa_fin)                                                                           \
    X(des_vigilances_illisibles_ne_sont_jamais_un_tout_va_bien)                                                        \
    X(pas_de_vigilance_ou_verte_ou_mer_absente_rien_a_dire)                                                            \
    X(l_etat_de_la_mer_dit_l_absence_la_peremption_ou_l_age)                                                           \
    X(le_bandeau_choisit_le_plus_important)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
