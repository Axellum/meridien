// Tests de la conversion `Meteo` (noyau, dixièmes entiers) -> `MeteoDetail` (contrat main/meteo_etat.h, `float` et indices recalculés
// pour l'heure de l'appareil) : meteo_detail.hpp.
//
// Deux familles de vérifications :
//  - SEPT vraies réponses d'Open-Meteo (les mêmes que test_meteo_riche.cpp, capturées le 07/11/2026 à 00 h 12 heure de Paris) :
//    la structure du contrat est aplatie et comparée, valeur par valeur, aux nombres calculés par Python
//    (donnees/attendu_riche_*.inc), sans reprendre une ligne du noyau ;
//  - des cas écrits à la main : indices selon l'heure de l'appareil, horloge non réglée, sections absentes, texte, arrondis.
// Le test inclut le VRAI contrat (`main/meteo_etat.h`) : si un champ y change de nom ou de type, ce fichier ne compile plus.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../main/meteo_etat.h"
#include "ecran/meteo_detail.hpp"
#include "ecran/meteo_requete.hpp"
#include "ecran/meteo_riche.hpp"
#include "mini.hpp"

using namespace ecran::meteo;
using sv = std::string_view;

namespace {

constexpr sv LYON =
#include "donnees/openmeteo_riche_lyon.inc"
    ;
constexpr sv LONGYEARBYEN =
#include "donnees/openmeteo_riche_longyearbyen.inc"
    ;
constexpr sv SEATTLE =
#include "donnees/openmeteo_riche_seattle.inc"
    ;
constexpr sv SINGAPOUR =
#include "donnees/openmeteo_riche_singapour.inc"
    ;
constexpr sv SYDNEY =
#include "donnees/openmeteo_riche_sydney.inc"
    ;
constexpr sv KATMANDOU =
#include "donnees/openmeteo_riche_katmandou.inc"
    ;
constexpr sv TERRENEUVE =
#include "donnees/openmeteo_riche_terreneuve.inc"
    ;

constexpr long long ATT_LYON[] = {
#include "donnees/attendu_riche_lyon.inc"
};
constexpr long long ATT_LONGYEARBYEN[] = {
#include "donnees/attendu_riche_longyearbyen.inc"
};
constexpr long long ATT_SEATTLE[] = {
#include "donnees/attendu_riche_seattle.inc"
};
constexpr long long ATT_SINGAPOUR[] = {
#include "donnees/attendu_riche_singapour.inc"
};
constexpr long long ATT_SYDNEY[] = {
#include "donnees/attendu_riche_sydney.inc"
};
constexpr long long ATT_KATMANDOU[] = {
#include "donnees/attendu_riche_katmandou.inc"
};
constexpr long long ATT_TERRENEUVE[] = {
#include "donnees/attendu_riche_terreneuve.inc"
};

// Toutes les captures ont le même `current.time` ; l'appareil les « reçoit » 100 s plus tard.
constexpr std::int64_t T0 = 1794006000;
constexpr std::int64_t RECU = T0 + 100;

constexpr AnalyseRiche R_LYON = analyser_riche(LYON, RECU);
constexpr AnalyseRiche R_LONGYEARBYEN = analyser_riche(LONGYEARBYEN, RECU);
constexpr AnalyseRiche R_SEATTLE = analyser_riche(SEATTLE, RECU);
constexpr AnalyseRiche R_SINGAPOUR = analyser_riche(SINGAPOUR, RECU);
constexpr AnalyseRiche R_SYDNEY = analyser_riche(SYDNEY, RECU);
constexpr AnalyseRiche R_KATMANDOU = analyser_riche(KATMANDOU, RECU);
constexpr AnalyseRiche R_TERRENEUVE = analyser_riche(TERRENEUVE, RECU);

// ---- outils -------------------------------------------------------------------------------------------------------------

// Un `float` du contrat (exact au dixième) en dixièmes entiers, à la plus proche.
constexpr long long dix(float f) noexcept { return static_cast<long long>(f * 10.0F + (f >= 0.0F ? 0.5F : -0.5F)); }

struct Plat {
    long long v[600] = {};
    std::size_t n = 0;
    bool trop = false;
    constexpr void ajouter(long long x) noexcept {
        if (n < 600)
            v[n++] = x;
        else
            trop = true;
    }
};

// Le `MeteoDetail` en une liste de nombres, dans l'ORDRE du fichier attendu (le même que `aplatir` de test_meteo_riche.cpp et que
// `valeurs_attendues` de controle_python_riche.py). `proba_du_releve` occupe la place de la probabilité du relevé de base : le
// contrat n'en a pas (sa probabilité du moment vient des heures recalées, vérifiée à part).
constexpr Plat aplatir_detail(const MeteoDetail &d, long long proba_du_releve) noexcept {
    Plat p;
    const MeteoCourant &c = d.courant;
    p.ajouter(c.instant_epoch_s);
    p.ajouter(d.decalage_utc_s);
    p.ajouter(dix(c.temperature_c));
    p.ajouter(dix(c.ressentie_c));
    p.ajouter(c.code_wmo);
    p.ajouter(c.jour ? 1 : 0);
    p.ajouter(dix(c.vent_kmh));
    p.ajouter(dix(c.rafales_kmh));
    p.ajouter(c.direction_vent_deg);
    p.ajouter(dix(c.pression_hpa));
    p.ajouter(c.humidite_pct);
    p.ajouter(proba_du_releve);
    p.ajouter(d.nb_jours);
    for (int i = 0; i < d.nb_jours; ++i) {
        const MeteoJour &j = d.jours[i];
        p.ajouter(j.date_epoch_s);
        p.ajouter(j.code_wmo);
        p.ajouter(dix(j.min_c));
        p.ajouter(dix(j.max_c));
        p.ajouter(j.lever_epoch_s);
        p.ajouter(j.coucher_epoch_s);
        p.ajouter(dix(j.uv_max));
        p.ajouter(j.proba_precip_pct);
    }
    p.ajouter(dix(c.precipitation_mm));
    p.ajouter(c.nuages_pct);
    p.ajouter(d.nb_heures);
    for (int i = 0; i < d.nb_heures; ++i) {
        const MeteoHeure &h = d.heures[i];
        p.ajouter(h.debut_epoch_s);
        p.ajouter(dix(h.temperature_c));
        p.ajouter(dix(h.ressentie_c));
        p.ajouter(h.humidite_pct);
        p.ajouter(h.proba_precip_pct);
        p.ajouter(dix(h.precipitation_mm));
        p.ajouter(h.code_wmo);
        p.ajouter(h.nuages_pct);
        p.ajouter(dix(h.vent_kmh));
        p.ajouter(dix(h.rafales_kmh));
        p.ajouter(h.direction_vent_deg);
        p.ajouter(dix(h.uv));
        p.ajouter(h.jour ? 1 : 0);
    }
    p.ajouter(d.nb_jours);
    for (int i = 0; i < d.nb_jours; ++i) {
        const MeteoJour &j = d.jours[i];
        p.ajouter(dix(j.ressentie_min_c));
        p.ajouter(dix(j.ressentie_max_c));
        p.ajouter(static_cast<long long>(j.ensoleillement_h * 60.0F + 0.5F)); // à la minute près
        p.ajouter(dix(j.precipitation_mm));
        p.ajouter(dix(j.neige_cm));
        p.ajouter(j.heures_precipitation);
        p.ajouter(dix(j.vent_max_kmh));
        p.ajouter(dix(j.rafales_max_kmh));
        p.ajouter(j.direction_vent_dominante_deg);
    }
    p.ajouter(d.nb_quarts);
    p.ajouter(d.quarts_debut_epoch_s);
    for (int i = 0; i < d.nb_quarts; ++i)
        p.ajouter(dix(d.quarts_precipitation_mm[i]));
    return p;
}

constexpr std::uint32_t VERSION_PAR_DEFAUT = 3;

constexpr MeteoDetail convertir(const Meteo &m, std::int64_t maintenant_s = RECU, int echecs = 0,
                                const HistoPression &histo = HistoPression{}) noexcept {
    MeteoDetail d{};
    remplir_detail(m, VERSION_PAR_DEFAUT, maintenant_s, echecs, histo, d);
    return d;
}

// `__builtin_isnan` est évaluable à la compilation (`std::isnan` ne l'est pas en C++17).
constexpr bool est_nan(float x) noexcept { return __builtin_isnan(x); }

// Un historique où la mesure de `m` a été précédée, 3 h plus tôt, d'une pression plus basse de `ecart_dixiemes` (la pression monte).
constexpr HistoPression historique_avant(const Meteo &m, int ecart_dixiemes) noexcept {
    HistoPression h;
    tendance_noter(h, m.releve.instant_s - 3 * 3600, m.releve.pression_dixiemes_hpa - ecart_dixiemes);
    tendance_noter(h, m.releve.instant_s, m.releve.pression_dixiemes_hpa);
    return h;
}

// Rend 0 si tout concorde, sinon 10000 + le rang de la première valeur fausse (voir le fichier attendu) ou 1 à 7.
template <std::size_t N> constexpr int comparer(const AnalyseRiche &a, const long long (&attendu)[N]) noexcept {
    if (!a.ok() || !a.diag_heures.ok() || !a.diag_jours.ok() || !a.diag_quarts.ok())
        return 1;
    const MeteoDetail d = convertir(a.meteo);
    if (!d.valide)
        return 2;
    if (d.perime)
        return 3; // reçue il y a 0 s : jamais périmée
    const Plat p = aplatir_detail(d, a.meteo.releve.proba_precip_pct);
    if (p.trop || p.n != N)
        return 5;
    for (std::size_t i = 0; i < N; ++i) {
        if (p.v[i] != attendu[i])
            return 10000 + static_cast<int>(i);
    }
    // La probabilité du moment du contrat est celle de l'heure en cours des heures recalées ; si la liste ne couvre pas l'heure de
    // l'appareil (Terre-Neuve, UTC-3:30 : la série commence après), elle est inconnue, pas inventée.
    const int attendue = d.heure_courante >= 0 ? d.heures[d.heure_courante].proba_precip_pct : METEO_PROBA_INCONNUE;
    if (d.courant.proba_precip_pct != attendue)
        return 6;
    return d.maj_epoch_s == RECU ? 0 : 7;
}

// ---- le contrat lui-même ------------------------------------------------------------------------------------------------

constexpr int le_contrat_a_les_tailles_annoncees() {
    VERIFIER(METEO_HEURES_MAX == HEURES_MAX);
    VERIFIER(METEO_JOURS_MAX == JOURS_MAX);
    VERIFIER(METEO_QUARTS_MAX == QUARTS_MAX);
    VERIFIER(METEO_PROBA_INCONNUE == PROBA_INCONNUE);
    VERIFIER(sizeof(MeteoDetail) <= 3072); // « environ 2 Ko » : si ça grossit, l'écran doit le savoir
    VERIFIER(sizeof(static_cast<MeteoDetail *>(nullptr)->fuseau) == FUSEAU_MAX);
    return 0;
}

// ---- réponses réelles ---------------------------------------------------------------------------------------------------

constexpr int lyon_detail_identique_a_python() { return comparer(R_LYON, ATT_LYON); }
constexpr int nuit_polaire_de_longyearbyen_detail_identique_a_python() {
    return comparer(R_LONGYEARBYEN, ATT_LONGYEARBYEN);
}
constexpr int pluie_en_cours_de_seattle_detail_identique_a_python() { return comparer(R_SEATTLE, ATT_SEATTLE); }
constexpr int singapour_detail_identique_a_python() { return comparer(R_SINGAPOUR, ATT_SINGAPOUR); }
constexpr int sydney_detail_identique_a_python() { return comparer(R_SYDNEY, ATT_SYDNEY); }
constexpr int katmandou_detail_identique_a_python() { return comparer(R_KATMANDOU, ATT_KATMANDOU); }
constexpr int terre_neuve_detail_identique_a_python() { return comparer(R_TERRENEUVE, ATT_TERRENEUVE); }

// ---- valeurs écrites à la main, lues dans la réponse JSON brute (pas dans le noyau) ---------------------------------------

constexpr int lyon_le_courant_et_le_fuseau() {
    const MeteoDetail d = convertir(R_LYON.meteo);
    VERIFIER(d.valide && !d.perime);
    VERIFIER(sv(d.fuseau) == "Europe/Paris");
    VERIFIER(d.decalage_utc_s == 3600);
    VERIFIER(d.courant.instant_epoch_s == T0);
    VERIFIER(d.courant.temperature_c == 7.4F);
    VERIFIER(d.courant.ressentie_c == 6.4F);
    VERIFIER(d.courant.humidite_pct == 98);
    VERIFIER(!d.courant.jour);
    VERIFIER(d.courant.precipitation_mm == 0.0F);
    VERIFIER(d.courant.code_wmo == 45);
    VERIFIER(d.courant.nuages_pct == 48);
    VERIFIER(d.courant.pression_hpa == 1024.9F);
    VERIFIER(d.courant.vent_kmh == 1.5F);
    VERIFIER(d.courant.direction_vent_deg == 346);
    VERIFIER(d.courant.rafales_kmh == 1.8F);
    VERIFIER(d.courant.proba_precip_pct == 0);
    VERIFIER(sv(d.courant.libelle) == decrire(45, false).libelle);
    return 0;
}

constexpr int seattle_le_libelle_est_en_francais() {
    const MeteoDetail d = convertir(R_SEATTLE.meteo);
    VERIFIER(d.courant.code_wmo == 63);
    VERIFIER(sv(d.courant.libelle) == "Pluie modérée");
    VERIFIER(d.courant.precipitation_mm == 0.8F);
    VERIFIER(d.courant.rafales_kmh == 29.2F);
    VERIFIER(d.courant.temperature_c == 11.5F);
    VERIFIER(d.courant.pression_hpa == 1016.7F);
    VERIFIER(d.decalage_utc_s == -28800);
    VERIFIER(sv(d.fuseau) == "America/Los_Angeles");
    // Probabilité de l'heure en cours : l'étiquette SUIVANTE (Open-Meteo donne l'intervalle qui précède l'étiquette) : 48 %.
    VERIFIER(d.courant.proba_precip_pct == 48);
    VERIFIER(d.heures[0].proba_precip_pct == 48 && d.heures[0].precipitation_mm == 1.0F);
    return 0;
}

constexpr int les_durees_du_jour_et_de_soleil() {
    // Lyon, jour 0 : lever 1794032936, coucher 1794068347 -> 35 411 s = 9,84 h -> 9,8 h ; ensoleillement 30 776,6 s = 512,9 min,
    // gardé à 513 min -> 8,55 h (à la minute, pas au dixième).
    const MeteoDetail l = convertir(R_LYON.meteo);
    VERIFIER(l.jours[0].lever_epoch_s == 1794032936 && l.jours[0].coucher_epoch_s == 1794068347);
    VERIFIER(l.jours[0].duree_jour_h == 9.8F);
    VERIFIER(l.jours[0].ensoleillement_h == 513.0F / 60.0F);
    VERIFIER(!l.jours[0].sans_soleil);
    // Seattle, jour 1 : lever 1794063767, coucher 1794098562 -> 34 795 s = 9,67 h -> 9,7 h ; ensoleillement 13 560,63 s = 226 min.
    const MeteoDetail s = convertir(R_SEATTLE.meteo);
    VERIFIER(s.jours[1].duree_jour_h == 9.7F);
    VERIFIER(s.jours[1].ensoleillement_h == 226.0F / 60.0F);
    // Seattle, jour 0 : aucun ensoleillement prévu (0,0 s dans la réponse).
    VERIFIER(s.jours[0].ensoleillement_h == 0.0F);
    return 0;
}

constexpr int la_nuit_polaire_n_a_ni_duree_de_jour_ni_soleil() {
    const MeteoDetail d = convertir(R_LONGYEARBYEN.meteo);
    VERIFIER(d.nb_jours == 7);
    for (int i = 0; i < d.nb_jours; ++i) {
        VERIFIER(d.jours[i].sans_soleil);
        VERIFIER(d.jours[i].lever_epoch_s == d.jours[i].coucher_epoch_s);
        VERIFIER(d.jours[i].duree_jour_h == 0.0F);
        VERIFIER(d.jours[i].ensoleillement_h == 0.0F);
    }
    VERIFIER(d.courant.temperature_c == -3.3F);
    VERIFIER(d.courant.ressentie_c == -7.5F);
    return 0;
}

constexpr int seattle_la_pluie_proche_pendant_la_pluie() {
    // Quarts d'heure de la réponse : 0,8 0,3 0,3 0,2 0,2 0,1 0,0 0,1 0,0 mm ; le premier est [T0 - 900, T0). À T0 + 100 on est dans
    // le deuxième (0,3 mm). La pluie dure jusqu'au début du septième quart (T0 + 4500) : 4 400 s = 73,3 min -> 74 min. La fenêtre
    // finit à T0 + 7200 : 7 100 s = 118,3 min -> 119 min. Total du quart en cours à la fin : 0,3+0,3+0,2+0,2+0,1+0+0,1+0 = 1,2 mm.
    const MeteoDetail d = convertir(R_SEATTLE.meteo);
    VERIFIER(d.nb_quarts == 9);
    VERIFIER(d.quarts_debut_epoch_s == T0 - 900);
    VERIFIER(d.quarts_precipitation_mm[0] == 0.8F && d.quarts_precipitation_mm[5] == 0.1F);
    VERIFIER(d.quart_courant == 1);
    VERIFIER(d.pluie.connue && !d.pluie.perimee && d.pluie.en_cours);
    VERIFIER(d.pluie.dans_min == 0);
    VERIFIER(d.pluie.fin_dans_min == 74);
    VERIFIER(d.pluie.fenetre_min == 119);
    VERIFIER(d.pluie.cumul_mm == 1.2F);
    VERIFIER(d.pluie.max_quart_mm == 0.3F);
    return 0;
}

constexpr int la_pluie_proche_est_recalculee_a_chaque_lecture() {
    // Une heure plus tard (T0 + 3700) : quart n° 5 (0,1 mm, il pleut encore), plus de pluie dans 14 min (début du quart n° 6 à
    // T0 + 4500, 800 s), fenêtre de 59 min (3 500 s), reste 0,1 + 0 + 0,1 + 0 = 0,2 mm. Reçue depuis plus de 30 min : périmée.
    const MeteoDetail d = convertir(R_SEATTLE.meteo, T0 + 3700);
    VERIFIER(d.quart_courant == 5);
    VERIFIER(d.pluie.connue && d.pluie.perimee && d.pluie.en_cours);
    VERIFIER(d.pluie.dans_min == 0 && d.pluie.fin_dans_min == 14);
    VERIFIER(d.pluie.fenetre_min == 59);
    VERIFIER(d.pluie.cumul_mm == 0.2F);
    // Au-delà des quarts d'heure (fin à T0 + 7200) : aucune pluie « proche » n'est affirmée.
    const MeteoDetail apres = convertir(R_SEATTLE.meteo, T0 + 7300);
    VERIFIER(!apres.pluie.connue && apres.quart_courant == -1);
    VERIFIER(apres.pluie.dans_min == -1 && apres.pluie.fin_dans_min == -1 && apres.pluie.fenetre_min == 0);
    // Horloge en retard de plus de 5 min sur la réception : on ne dit rien.
    const MeteoDetail avant = convertir(R_SEATTLE.meteo, RECU - 400);
    VERIFIER(!avant.pluie.connue);
    return 0;
}

constexpr int les_indices_suivent_l_heure_de_l_appareil() {
    const Meteo &m = R_LYON.meteo;
    const MeteoDetail a = convertir(m, RECU);
    VERIFIER(a.heure_courante == 0 && a.jour_courant == 0 && a.quart_courant == 1);
    VERIFIER(a.heures[a.heure_courante].debut_epoch_s == T0);
    const MeteoDetail b = convertir(m, T0 + 3 * 3600 + 50);
    VERIFIER(b.heure_courante == 3 && b.jour_courant == 0 && b.quart_courant == -1);
    const MeteoDetail c = convertir(m, T0 + 86400 + 3600); // le lendemain à 1 h
    VERIFIER(c.jour_courant == 1);
    VERIFIER(c.heure_courante == -1); // les 24 heures s'arrêtent avant
    const MeteoDetail d = convertir(m, T0 + 10 * 86400);
    VERIFIER(d.jour_courant == -1 && d.heure_courante == -1);
    return 0;
}

constexpr int horloge_non_reglee_aucun_indice_aucune_pluie() {
    const MeteoDetail d = convertir(R_SEATTLE.meteo, 0);
    VERIFIER(d.valide);
    VERIFIER(d.perime); // « jamais à jour sur une supposition »
    VERIFIER(d.heure_courante == -1 && d.jour_courant == -1 && d.quart_courant == -1);
    VERIFIER(d.courant.proba_precip_pct == METEO_PROBA_INCONNUE);
    VERIFIER(!d.pluie.connue && d.pluie.dans_min == -1 && d.pluie.fenetre_min == 0 && d.pluie.cumul_mm == 0.0F);
    // Les valeurs de la réponse restent là.
    VERIFIER(d.nb_heures == 24 && d.nb_jours == 7 && d.nb_quarts == 9);
    VERIFIER(d.courant.temperature_c == 11.5F);
    return 0;
}

constexpr int perime_suit_la_regle_de_l_appareil() {
    const Meteo &m = R_LYON.meteo;
    VERIFIER(!convertir(m, RECU + 3600).perime);
    VERIFIER(!convertir(m, RECU + 2 * 3600).perime);
    VERIFIER(convertir(m, RECU + 2 * 3600 + 1).perime);
    // Trois essais ratés de suite et une réponse de plus de 30 min : périmée ; moins de 30 min : pas encore.
    VERIFIER(convertir(m, RECU + 1801, 3).perime);
    VERIFIER(!convertir(m, RECU + 1800, 3).perime);
    VERIFIER(!convertir(m, RECU + 1801, 2).perime);
    VERIFIER(convertir(m, RECU + 60).maj_epoch_s == RECU);
    return 0;
}

constexpr int les_fuseaux_a_decalage_fractionnaire_ne_donnent_pas_de_faux_indices() {
    // Terre-Neuve (UTC-3:30) : la série horaire d'Open-Meteo commence APRÈS l'heure courante de l'appareil -> aucune heure ne la
    // contient, la probabilité du moment est inconnue (pas celle de l'heure d'à côté).
    const MeteoDetail tn = convertir(R_TERRENEUVE.meteo);
    VERIFIER(tn.valide && tn.nb_heures == 24);
    VERIFIER(tn.heure_courante == -1);
    VERIFIER(tn.courant.proba_precip_pct == METEO_PROBA_INCONNUE);
    VERIFIER(tn.decalage_utc_s == -12600);
    // Katmandou (UTC+5:45) : la série commence AVANT l'heure courante ; l'heure en cours est quand même trouvée, et elle contient
    // l'heure de l'appareil.
    const MeteoDetail ka = convertir(R_KATMANDOU.meteo);
    VERIFIER(ka.decalage_utc_s == 20700);
    VERIFIER(ka.heure_courante >= 0);
    VERIFIER(ka.heures[ka.heure_courante].debut_epoch_s <= RECU);
    VERIFIER(RECU < ka.heures[ka.heure_courante].debut_epoch_s + 3600);
    return 0;
}

constexpr int la_version_publiee_est_celle_qu_on_donne() {
    MeteoDetail d{};
    d.version = 7; // l'ancien contenu n'a aucune importance
    d.valide = true;
    d.nb_heures = 5;
    remplir_detail(R_LYON.meteo, 42, RECU, 0, HistoPression{}, d);
    VERIFIER(d.version == 42);   // le numéro de publication appartient à l'état de l'appareil, qui le donne
    VERIFIER(d.nb_heures == 24); // et tout le reste est refait à neuf
    VERIFIER(convertir(R_LYON.meteo).version == VERSION_PAR_DEFAUT);
    return 0;
}

constexpr int une_nouvelle_conversion_ne_garde_rien_de_l_ancienne() {
    // On convertit d'abord Seattle (tout est rempli, il pleut), puis dans la MÊME structure une météo sans heures, sans jours
    // et sans quarts d'heure : rien de Seattle ne doit rester (ni valeurs, ni indices, ni pluie).
    MeteoDetail d = convertir(R_SEATTLE.meteo, RECU, 0, historique_avant(R_SEATTLE.meteo, 25));
    VERIFIER(d.nb_jours == 7 && d.pluie.en_cours && d.quart_courant == 1);
    VERIFIER(d.courant.tendance_pression_3h_hpa == 2.5F);
    Meteo m = R_LYON.meteo;
    m.riche.nb_heures = 0;
    m.riche.nb_jours = 0;
    m.riche.nb_quarts = 0;
    remplir_detail(m, VERSION_PAR_DEFAUT, RECU, 0, HistoPression{}, d);
    VERIFIER(est_nan(d.courant.tendance_pression_3h_hpa)); // la tendance de Seattle ne reste pas
    VERIFIER(d.nb_heures == 0 && d.nb_jours == 0 && d.nb_quarts == 0);
    VERIFIER(d.heure_courante == -1 && d.jour_courant == -1 && d.quart_courant == -1);
    VERIFIER(!d.pluie.connue && !d.pluie.en_cours && d.pluie.cumul_mm == 0.0F);
    VERIFIER(d.heures[0].debut_epoch_s == 0 && d.jours[0].date_epoch_s == 0 && d.quarts_precipitation_mm[0] == 0.0F);
    VERIFIER(d.courant.temperature_c == 7.4F && sv(d.fuseau) == "Europe/Paris");
    return 0;
}

// ---- sections absentes : rien n'est inventé ---------------------------------------------------------------------------

constexpr int sans_heures_la_liste_est_vide_et_la_probabilite_inconnue() {
    Meteo m = R_LYON.meteo;
    m.riche.nb_heures = 0;
    const MeteoDetail d = convertir(m);
    VERIFIER(d.valide && d.nb_heures == 0 && d.heure_courante == -1);
    VERIFIER(d.courant.proba_precip_pct == METEO_PROBA_INCONNUE);
    VERIFIER(d.nb_jours == 7 && d.nb_quarts == 9); // les autres sections restent
    return 0;
}

constexpr int sans_quarts_pas_de_pluie_proche() {
    Meteo m = R_SEATTLE.meteo;
    m.riche.nb_quarts = 0;
    const MeteoDetail d = convertir(m);
    VERIFIER(d.nb_quarts == 0 && d.quart_courant == -1 && d.quarts_debut_epoch_s == 0);
    VERIFIER(!d.pluie.connue && d.pluie.dans_min == -1);
    VERIFIER(d.nb_heures == 24);
    return 0;
}

constexpr int jours_publies_seulement_si_tous_ont_leurs_complements() {
    Meteo m = R_LYON.meteo;
    m.riche.nb_jours = 0; // compléments absents (champ refusé à la lecture)
    const MeteoDetail sans = convertir(m);
    VERIFIER(sans.valide && sans.nb_jours == 0 && sans.jour_courant == -1);
    VERIFIER(sans.nb_heures == 24); // le reste ne bouge pas
    m = R_LYON.meteo;
    m.riche.nb_jours = static_cast<std::uint8_t>(m.releve.nb_jours - 1); // incomplets : pas de jour à moitié rempli
    VERIFIER(convertir(m).nb_jours == 0);
    VERIFIER(convertir(R_LYON.meteo).nb_jours == 7);
    return 0;
}

constexpr int un_releve_sans_prevision_de_jour_donne_zero_jour() {
    Meteo m = R_LYON.meteo;
    m.releve.nb_jours = 0;
    m.riche.nb_jours = 0;
    const MeteoDetail d = convertir(m);
    VERIFIER(d.valide && d.nb_jours == 0 && d.jour_courant == -1);
    return 0;
}

// ---- texte, arrondis -------------------------------------------------------------------------------------------------------

constexpr int tous_les_libelles_tiennent_dans_le_contrat() {
    // 0..99 couvre tous les codes WMO d'Open-Meteo (29 sont définis, les autres donnent « Météo inconnue ») ; jour et nuit.
    for (int code = 0; code < 100; ++code) {
        for (int j = 0; j < 2; ++j) {
            const sv lib = decrire(code, j == 1).libelle;
            VERIFIER(!lib.empty());
            VERIFIER(lib.size() < sizeof(MeteoCourant{}.libelle));
        }
    }
    return 0;
}

constexpr int le_nom_du_fuseau_le_plus_long_tient() {
    Meteo m = R_LYON.meteo;
    for (std::size_t i = 0; i < FUSEAU_MAX - 1; ++i)
        m.releve.fuseau[i] = 'A';
    m.releve.fuseau[FUSEAU_MAX - 1] = '\0';
    const MeteoDetail d = convertir(m);
    std::size_t n = 0;
    while (d.fuseau[n] != '\0')
        ++n;
    VERIFIER(n == FUSEAU_MAX - 1);
    return 0;
}

constexpr int copier_texte_coupe_et_termine_toujours_par_zero() {
    char b[4] = {'x', 'x', 'x', 'x'};
    detail::copier_texte(sv("abc"), b);
    VERIFIER(b[0] == 'a' && b[1] == 'b' && b[2] == 'c' && b[3] == '\0');
    detail::copier_texte(sv("abcdef"), b);
    VERIFIER(b[0] == 'a' && b[1] == 'b' && b[2] == 'c' && b[3] == '\0'); // coupé, jamais sans zéro final
    detail::copier_texte(sv(""), b);
    VERIFIER(b[0] == '\0');
    return 0;
}

constexpr int les_float_sont_exacts_au_dixieme() {
    // La division IEEE est correctement arrondie : n / 10,0F est le float le plus proche du décimal, comme le littéral.
    VERIFIER(detail::vers_float(0) == 0.0F);
    VERIFIER(detail::vers_float(1) == 0.1F);
    VERIFIER(detail::vers_float(123) == 12.3F);
    VERIFIER(detail::vers_float(-45) == -4.5F);
    VERIFIER(detail::vers_float(10132) == 1013.2F);
    VERIFIER(detail::vers_float(-330) == -33.0F);
    for (int n = -2000; n <= 2000; ++n)
        VERIFIER(dix(detail::vers_float(n)) == n);
    return 0;
}

// ---- tendance de pression sur 3 h ---------------------------------------------------------------------------------------

constexpr int la_tendance_de_pression_vient_de_l_historique() {
    const Meteo &m = R_LYON.meteo;
    const MeteoDetail monte = convertir(m, RECU, 0, historique_avant(m, 25)); // +2,5 hPa en 3 h
    VERIFIER(monte.courant.tendance_pression_3h_hpa == 2.5F);
    const MeteoDetail baisse = convertir(m, RECU, 0, historique_avant(m, -34)); // -3,4 hPa
    VERIFIER(baisse.courant.tendance_pression_3h_hpa == -3.4F);
    const MeteoDetail stable = convertir(m, RECU, 0, historique_avant(m, 0));
    VERIFIER(stable.courant.tendance_pression_3h_hpa == 0.0F);
    VERIFIER(!est_nan(stable.courant.tendance_pression_3h_hpa)); // « stable » n'est pas « inconnue »
    // La pression du moment, elle, ne bouge pas.
    VERIFIER(monte.courant.pression_hpa == convertir(m).courant.pression_hpa);
    return 0;
}

constexpr int sans_historique_la_tendance_de_pression_est_inconnue() {
    VERIFIER(est_nan(convertir(R_LYON.meteo).courant.tendance_pression_3h_hpa));
    // Une seule mesure (celle du moment) : toujours inconnue.
    HistoPression h;
    tendance_noter(h, R_LYON.meteo.releve.instant_s, R_LYON.meteo.releve.pression_dixiemes_hpa);
    VERIFIER(est_nan(convertir(R_LYON.meteo, RECU, 0, h).courant.tendance_pression_3h_hpa));
    // Une mesure vieille de 2 h seulement : hors fenêtre, inconnue.
    HistoPression court;
    tendance_noter(court, R_LYON.meteo.releve.instant_s - 2 * 3600, R_LYON.meteo.releve.pression_dixiemes_hpa - 10);
    VERIFIER(est_nan(convertir(R_LYON.meteo, RECU, 0, court).courant.tendance_pression_3h_hpa));
    return 0;
}

constexpr int la_tendance_de_pression_ne_depend_pas_de_l_horloge_de_l_appareil() {
    // Elle compare des instants de MESURE donnés par le serveur : horloge non réglée (0) ou très en retard, rien ne change.
    const Meteo &m = R_SEATTLE.meteo;
    const HistoPression h = historique_avant(m, -12);
    VERIFIER(convertir(m, 0, 0, h).courant.tendance_pression_3h_hpa == -1.2F);
    VERIFIER(convertir(m, RECU + 5 * 86400, 0, h).courant.tendance_pression_3h_hpa == -1.2F);
    VERIFIER(convertir(m, RECU, 5, h).courant.tendance_pression_3h_hpa == -1.2F); // et des essais ratés non plus
    return 0;
}

constexpr int la_duree_du_jour_est_arrondie_au_dixieme_d_heure() {
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(0) == 0);
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(-5) == 0);
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(-7200) == 0); // jamais une durée de jour négative
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(3600) == 10);
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(30419) == 84); // 8,4497 h -> 8,4
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(30420) == 85); // 8,45 h : la moitié monte
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(30779) == 85); // 8,5497 h -> 8,5
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(30780) == 86); // 8,55 h : la moitié monte
    VERIFIER(detail::dixiemes_d_heure_depuis_secondes(35411) == 98); // Lyon : 9,836 h
    VERIFIER(detail::heures_depuis_minutes(0) == 0.0F);
    VERIFIER(detail::heures_depuis_minutes(60) == 1.0F);
    VERIFIER(detail::heures_depuis_minutes(90) == 1.5F);
    VERIFIER(detail::heures_depuis_minutes(513) == 8.55F);
    return 0;
}

#define LISTE(X)                                                                                                       \
    X(le_contrat_a_les_tailles_annoncees)                                                                              \
    X(lyon_detail_identique_a_python)                                                                                  \
    X(nuit_polaire_de_longyearbyen_detail_identique_a_python)                                                          \
    X(pluie_en_cours_de_seattle_detail_identique_a_python)                                                             \
    X(singapour_detail_identique_a_python)                                                                             \
    X(sydney_detail_identique_a_python)                                                                                \
    X(katmandou_detail_identique_a_python)                                                                             \
    X(terre_neuve_detail_identique_a_python)                                                                           \
    X(lyon_le_courant_et_le_fuseau)                                                                                    \
    X(seattle_le_libelle_est_en_francais)                                                                              \
    X(les_durees_du_jour_et_de_soleil)                                                                                 \
    X(la_nuit_polaire_n_a_ni_duree_de_jour_ni_soleil)                                                                  \
    X(seattle_la_pluie_proche_pendant_la_pluie)                                                                        \
    X(la_pluie_proche_est_recalculee_a_chaque_lecture)                                                                 \
    X(les_indices_suivent_l_heure_de_l_appareil)                                                                       \
    X(horloge_non_reglee_aucun_indice_aucune_pluie)                                                                    \
    X(perime_suit_la_regle_de_l_appareil)                                                                              \
    X(les_fuseaux_a_decalage_fractionnaire_ne_donnent_pas_de_faux_indices)                                             \
    X(la_version_publiee_est_celle_qu_on_donne)                                                                        \
    X(une_nouvelle_conversion_ne_garde_rien_de_l_ancienne)                                                             \
    X(sans_heures_la_liste_est_vide_et_la_probabilite_inconnue)                                                        \
    X(sans_quarts_pas_de_pluie_proche)                                                                                 \
    X(jours_publies_seulement_si_tous_ont_leurs_complements)                                                           \
    X(un_releve_sans_prevision_de_jour_donne_zero_jour)                                                                \
    X(tous_les_libelles_tiennent_dans_le_contrat)                                                                      \
    X(le_nom_du_fuseau_le_plus_long_tient)                                                                             \
    X(copier_texte_coupe_et_termine_toujours_par_zero)                                                                 \
    X(les_float_sont_exacts_au_dixieme)                                                                                \
    X(la_tendance_de_pression_vient_de_l_historique)                                                                   \
    X(sans_historique_la_tendance_de_pression_est_inconnue)                                                            \
    X(la_tendance_de_pression_ne_depend_pas_de_l_horloge_de_l_appareil)                                                \
    X(la_duree_du_jour_est_arrondie_au_dixieme_d_heure)

TESTS_A_LA_COMPILATION(LISTE)

} // namespace

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
