// Tests du noyau pur : le lecteur du NWS (meteo_nws.hpp), en deux réponses : /points puis /gridpoints.
//
// Six GRILLES RÉELLES, capturées le 07/11/2026 avec l'adresse exacte du firmware (test/host/capturer_fournisseurs_meteo.py) :
// New York, Miami, Seattle, Anchorage, Honolulu, San Juan (Porto Rico), plus la réponse 404 réelle de /points pour Lyon (hors de
// la couverture du NWS). Les valeurs attendues sont recalculées par Python, qui relit les réponses sans rien du noyau
// (test/host/controle_python_fournisseurs.py, lecture_fournisseurs.py) et qui écrit donnees/attendu_nws_*.inc : lever, coucher et
// « jour » sont comparés à quelques minutes près (autre algorithme du soleil), tout le reste à l'unité. Les cas limites sont de
// petites grilles écrites à la main.
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../main/meteo_etat.h"
#include "aplatir_fournisseurs.hpp"
#include "ecran/fournisseurs_meteo.hpp"
#include "ecran/meteo_detail.hpp"
#include "ecran/meteo_nws.hpp"
#include "ecran/meteo_riche.hpp"
#include "mini.hpp"

using namespace ecran::meteo;
using namespace fournisseurs_test;

namespace {

// ---- les réponses réelles -------------------------------------------------------------------------------------------------

constexpr sv P_PTS_NEW_YORK[] = {
#include "donnees/nws_new_york_points.inc"
};
constexpr auto J_PTS_NEW_YORK = joindre<longueur_totale(P_PTS_NEW_YORK)>(P_PTS_NEW_YORK);
constexpr sv PTS_NEW_YORK = J_PTS_NEW_YORK.vue();
constexpr sv P_GRL_NEW_YORK[] = {
#include "donnees/nws_new_york_grille.inc"
};
constexpr auto J_GRL_NEW_YORK = joindre<longueur_totale(P_GRL_NEW_YORK)>(P_GRL_NEW_YORK);
constexpr sv GRL_NEW_YORK = J_GRL_NEW_YORK.vue();

constexpr sv P_PTS_MIAMI[] = {
#include "donnees/nws_miami_points.inc"
};
constexpr auto J_PTS_MIAMI = joindre<longueur_totale(P_PTS_MIAMI)>(P_PTS_MIAMI);
constexpr sv PTS_MIAMI = J_PTS_MIAMI.vue();
constexpr sv P_GRL_MIAMI[] = {
#include "donnees/nws_miami_grille.inc"
};
constexpr auto J_GRL_MIAMI = joindre<longueur_totale(P_GRL_MIAMI)>(P_GRL_MIAMI);
constexpr sv GRL_MIAMI = J_GRL_MIAMI.vue();

constexpr sv P_PTS_SEATTLE[] = {
#include "donnees/nws_seattle_points.inc"
};
constexpr auto J_PTS_SEATTLE = joindre<longueur_totale(P_PTS_SEATTLE)>(P_PTS_SEATTLE);
constexpr sv PTS_SEATTLE = J_PTS_SEATTLE.vue();
constexpr sv P_GRL_SEATTLE[] = {
#include "donnees/nws_seattle_grille.inc"
};
constexpr auto J_GRL_SEATTLE = joindre<longueur_totale(P_GRL_SEATTLE)>(P_GRL_SEATTLE);
constexpr sv GRL_SEATTLE = J_GRL_SEATTLE.vue();

constexpr sv P_PTS_ANCHORAGE[] = {
#include "donnees/nws_anchorage_points.inc"
};
constexpr auto J_PTS_ANCHORAGE = joindre<longueur_totale(P_PTS_ANCHORAGE)>(P_PTS_ANCHORAGE);
constexpr sv PTS_ANCHORAGE = J_PTS_ANCHORAGE.vue();
constexpr sv P_GRL_ANCHORAGE[] = {
#include "donnees/nws_anchorage_grille.inc"
};
constexpr auto J_GRL_ANCHORAGE = joindre<longueur_totale(P_GRL_ANCHORAGE)>(P_GRL_ANCHORAGE);
constexpr sv GRL_ANCHORAGE = J_GRL_ANCHORAGE.vue();

constexpr sv P_PTS_HONOLULU[] = {
#include "donnees/nws_honolulu_points.inc"
};
constexpr auto J_PTS_HONOLULU = joindre<longueur_totale(P_PTS_HONOLULU)>(P_PTS_HONOLULU);
constexpr sv PTS_HONOLULU = J_PTS_HONOLULU.vue();
constexpr sv P_GRL_HONOLULU[] = {
#include "donnees/nws_honolulu_grille.inc"
};
constexpr auto J_GRL_HONOLULU = joindre<longueur_totale(P_GRL_HONOLULU)>(P_GRL_HONOLULU);
constexpr sv GRL_HONOLULU = J_GRL_HONOLULU.vue();

constexpr sv P_PTS_SAN_JUAN[] = {
#include "donnees/nws_san_juan_points.inc"
};
constexpr auto J_PTS_SAN_JUAN = joindre<longueur_totale(P_PTS_SAN_JUAN)>(P_PTS_SAN_JUAN);
constexpr sv PTS_SAN_JUAN = J_PTS_SAN_JUAN.vue();
constexpr sv P_GRL_SAN_JUAN[] = {
#include "donnees/nws_san_juan_grille.inc"
};
constexpr auto J_GRL_SAN_JUAN = joindre<longueur_totale(P_GRL_SAN_JUAN)>(P_GRL_SAN_JUAN);
constexpr sv GRL_SAN_JUAN = J_GRL_SAN_JUAN.vue();

constexpr sv P_PTS_LYON[] = {
#include "donnees/nws_lyon_points.inc"
};
constexpr auto J_PTS_LYON = joindre<longueur_totale(P_PTS_LYON)>(P_PTS_LYON);
constexpr sv PTS_LYON = J_PTS_LYON.vue(); // HTTP 404 « Data Unavailable For Requested Point »

constexpr long long ATT_NEW_YORK[][2] = {
#include "donnees/attendu_nws_new_york.inc"
};
constexpr long long ATT_MIAMI[][2] = {
#include "donnees/attendu_nws_miami.inc"
};
constexpr long long ATT_SEATTLE[][2] = {
#include "donnees/attendu_nws_seattle.inc"
};
constexpr long long ATT_ANCHORAGE[][2] = {
#include "donnees/attendu_nws_anchorage.inc"
};
constexpr long long ATT_HONOLULU[][2] = {
#include "donnees/attendu_nws_honolulu.inc"
};
constexpr long long ATT_SAN_JUAN[][2] = {
#include "donnees/attendu_nws_san_juan.inc"
};

// {nom, reçu à, latitude e5, longitude e5, bureau, x, y, fuseau}, dans l'ordre de LIEUX_NWS (controle_python_fournisseurs.py).
struct Lieu {
    sv nom;
    long long recu;
    int lat;
    int lon;
    sv bureau;
    int x;
    int y;
    sv fuseau;
};
constexpr Lieu LIEUX[] = {
#include "donnees/lieux_nws.inc"
};
constexpr std::size_t I_NEW_YORK = 0, I_MIAMI = 1, I_SEATTLE = 2, I_ANCHORAGE = 3, I_HONOLULU = 4, I_SAN_JUAN = 5;
static_assert(sizeof LIEUX / sizeof LIEUX[0] == 6, "lieux_nws.inc : six lieux");

constexpr bool meme_texte(const char *a, sv b) noexcept { return sv(a) == b; }

// Maille lue dans la réponse /points RÉELLE, puis grille lue dans la réponse /gridpoints RÉELLE.
constexpr AnalyseRiche lire(sv points, sv grille, const Lieu &l) {
    nws::Maille maille;
    if (!nws::analyser_points(points, maille).ok())
        return AnalyseRiche{};
    nws::Serie travail;
    return nws::analyser_nws(grille, maille, l.recu, l.lat, l.lon, travail);
}

constexpr AnalyseRiche R_NEW_YORK = lire(PTS_NEW_YORK, GRL_NEW_YORK, LIEUX[I_NEW_YORK]);
constexpr AnalyseRiche R_MIAMI = lire(PTS_MIAMI, GRL_MIAMI, LIEUX[I_MIAMI]);
constexpr AnalyseRiche R_SEATTLE = lire(PTS_SEATTLE, GRL_SEATTLE, LIEUX[I_SEATTLE]);
constexpr AnalyseRiche R_ANCHORAGE = lire(PTS_ANCHORAGE, GRL_ANCHORAGE, LIEUX[I_ANCHORAGE]);
constexpr AnalyseRiche R_HONOLULU = lire(PTS_HONOLULU, GRL_HONOLULU, LIEUX[I_HONOLULU]);
constexpr AnalyseRiche R_SAN_JUAN = lire(PTS_SAN_JUAN, GRL_SAN_JUAN, LIEUX[I_SAN_JUAN]);

constexpr int new_york_identique_a_python() { return comparer(R_NEW_YORK, ATT_NEW_YORK); }
constexpr int miami_identique_a_python() { return comparer(R_MIAMI, ATT_MIAMI); }
constexpr int seattle_identique_a_python() { return comparer(R_SEATTLE, ATT_SEATTLE); }
constexpr int anchorage_identique_a_python() { return comparer(R_ANCHORAGE, ATT_ANCHORAGE); }
constexpr int honolulu_identique_a_python() { return comparer(R_HONOLULU, ATT_HONOLULU); }
constexpr int san_juan_identique_a_python() { return comparer(R_SAN_JUAN, ATT_SAN_JUAN); }

constexpr int les_six_mailles_reelles_sont_lues() {
    constexpr sv POINTS[] = {PTS_NEW_YORK, PTS_MIAMI, PTS_SEATTLE, PTS_ANCHORAGE, PTS_HONOLULU, PTS_SAN_JUAN};
    for (std::size_t i = 0; i < 6; ++i) {
        nws::Maille m;
        VERIFIER(nws::analyser_points(POINTS[i], m).ok());
        VERIFIER(meme_texte(m.bureau, LIEUX[i].bureau));
        VERIFIER(m.x == LIEUX[i].x && m.y == LIEUX[i].y);
        VERIFIER(meme_texte(m.fuseau, LIEUX[i].fuseau));
    }
    return 0;
}

constexpr int hors_de_la_couverture_le_404_est_un_refus_qui_laisse_la_maille_vide() {
    nws::Maille m;
    m.x = 7; // doit être remise à zéro
    const Diagnostic d = nws::analyser_points(PTS_LYON, m);
    VERIFIER(!d.ok() && d.erreur == Erreur::ChampAbsent && d.champ == "properties");
    VERIFIER(m.x == 0 && m.y == 0 && m.bureau[0] == '\0' && m.fuseau[0] == '\0');
    return 0;
}

constexpr int ce_que_la_source_ne_donne_pas_reste_inconnu() {
    for (const AnalyseRiche *a : {&R_NEW_YORK, &R_MIAMI, &R_SEATTLE, &R_ANCHORAGE, &R_HONOLULU, &R_SAN_JUAN}) {
        const Meteo &m = a->meteo;
        VERIFIER(a->ok() && a->diag_heures.ok() && a->diag_jours.ok());
        VERIFIER(m.releve.nb_jours >= 6);
        VERIFIER(m.releve.pression_dixiemes_hpa == INCONNU_DIX); // la couche « pressure » du NWS est vide
        VERIFIER(m.riche.precip_courante_dixiemes == INCONNU_DIX);
        VERIFIER(m.riche.nb_quarts == 0 && a->diag_quarts.erreur == Erreur::ChampAbsent);
        for (std::size_t i = 0; i < m.releve.nb_jours; ++i) {
            VERIFIER(m.releve.jours[i].uv_max_dixiemes == INCONNU_DIX);
            VERIFIER(m.riche.jours[i].ensoleillement_min == INCONNU_ENTIER);
            VERIFIER(m.riche.jours[i].heures_precip == INCONNU_ENTIER);
            VERIFIER(m.riche.jours[i].direction_dominante_deg == INCONNU_ENTIER);
        }
        for (std::size_t i = 0; i < m.riche.nb_heures; ++i)
            VERIFIER(m.riche.heures[i].uv_dixiemes == INCONNU_DIX);
    }
    return 0;
}

constexpr int le_fuseau_et_le_decalage_viennent_de_la_maille() {
    // 07/11/2026, après le retour à l'heure d'hiver américaine (01/11) : New York UTC-5, Seattle UTC-8, Anchorage UTC-9, Honolulu
    // UTC-10, San Juan UTC-4.
    VERIFIER(R_NEW_YORK.meteo.releve.decalage_utc_s == -5 * 3600);
    VERIFIER(R_SEATTLE.meteo.releve.decalage_utc_s == -8 * 3600);
    VERIFIER(R_ANCHORAGE.meteo.releve.decalage_utc_s == -9 * 3600);
    VERIFIER(R_HONOLULU.meteo.releve.decalage_utc_s == -10 * 3600);
    VERIFIER(R_SAN_JUAN.meteo.releve.decalage_utc_s == -4 * 3600);
    VERIFIER(meme_texte(R_HONOLULU.meteo.releve.fuseau, "Pacific/Honolulu"));
    return 0;
}

constexpr int les_jours_commencent_a_minuit_local() {
    // Seattle : minuit local = 08:00 UTC.
    const Releve &r = R_SEATTLE.meteo.releve;
    for (std::size_t i = 0; i < r.nb_jours; ++i)
        VERIFIER(r.jours[i].date_s % 86400 == 8 * 3600 &&
                 (i == 0 || r.jours[i].date_s - r.jours[i - 1].date_s == 86400));
    return 0;
}

// ---- ce que la source ne donne pas traverse le cache et le contrat de l'écran sans devenir un zéro ----------------------------

constexpr bool est_nan(float x) noexcept { return __builtin_isnan(x); }

constexpr int le_cache_garde_les_valeurs_inconnues() {
    for (const AnalyseRiche *a : {&R_NEW_YORK, &R_SEATTLE, &R_HONOLULU, &R_SAN_JUAN}) {
        std::uint8_t tampon[TAILLE_CACHE_RICHE_MAX] = {};
        const std::size_t n = serialiser_cache_meteo(a->meteo, 4071280, -7400600, tampon, sizeof tampon);
        VERIFIER(n != 0 && n <= TAILLE_CACHE_RICHE_MAX);
        const LectureCacheMeteo l = deserialiser_cache_meteo(tampon, n);
        VERIFIER(l.ok());
        const Plat avant = aplatir(a->meteo);
        const Plat apres = aplatir(l.meteo);
        VERIFIER(!avant.trop && avant.n == apres.n);
        for (std::size_t i = 0; i < avant.n; ++i)
            VERIFIER(avant.v[i] == apres.v[i]);
    }
    return 0;
}

constexpr MeteoDetail convertir(const Meteo &m, const HistoPression &histo = HistoPression{}) {
    MeteoDetail d{};
    remplir_detail(m, 1, m.releve.recu_a_s, 0, histo, d);
    return d;
}

constexpr int le_contrat_de_l_ecran_recoit_nan_et_moins_un_pour_ce_que_la_source_ne_donne_pas() {
    for (const AnalyseRiche *a : {&R_NEW_YORK, &R_MIAMI, &R_SEATTLE, &R_ANCHORAGE, &R_HONOLULU, &R_SAN_JUAN}) {
        const MeteoDetail d = convertir(a->meteo);
        VERIFIER(d.valide && !d.perime);
        VERIFIER(est_nan(d.courant.pression_hpa) && est_nan(d.courant.precipitation_mm));
        VERIFIER(!est_nan(d.courant.temperature_c) && !est_nan(d.courant.vent_kmh));
        VERIFIER(d.nb_heures == 24 && d.nb_jours >= 6);
        for (int i = 0; i < d.nb_heures; ++i)
            VERIFIER(est_nan(d.heures[i].uv) && !est_nan(d.heures[i].temperature_c));
        for (int i = 0; i < d.nb_jours; ++i) {
            const MeteoJour &j = d.jours[i];
            VERIFIER(est_nan(j.uv_max) && est_nan(j.ensoleillement_h));
            VERIFIER(j.heures_precipitation == -1 && j.direction_vent_dominante_deg == -1);
            VERIFIER(!est_nan(j.min_c) && !est_nan(j.max_c) && !est_nan(j.duree_jour_h));
        }
        VERIFIER(!d.pluie.connue && d.nb_quarts == 0);
    }
    return 0;
}

constexpr int sans_pression_il_n_y_a_pas_de_tendance_meme_avec_un_historique() {
    // Un historique rempli par une AUTRE source (Open-Meteo, en repli) ne fait pas une tendance avec une pression inconnue
    HistoPression h;
    tendance_noter(h, R_NEW_YORK.meteo.releve.instant_s - 3 * 3600, 10132);
    tendance_noter(h, R_NEW_YORK.meteo.releve.instant_s, 10150);
    VERIFIER(R_NEW_YORK.meteo.releve.pression_dixiemes_hpa == INCONNU_DIX);
    VERIFIER(est_nan(convertir(R_NEW_YORK.meteo, h).courant.tendance_pression_3h_hpa));
    return 0;
}

// ---- les pays que la table des sources annonce sont ceux dont une réponse RÉELLE est rejouée ici ---------------------------

struct PaysVu {
    const AnalyseRiche *reponse;
    sv pays;
};

constexpr int les_pays_annonces_par_la_table_sont_ceux_des_captures() {
    const fournisseurs::Fournisseur *f = fournisseurs::trouver("nws");
    VERIFIER(f != nullptr && f->famille == fournisseurs::Famille::Nws);
    const PaysVu vus[] = {{&R_NEW_YORK, "US"},  {&R_MIAMI, "US"},    {&R_SEATTLE, "US"},
                          {&R_ANCHORAGE, "US"}, {&R_HONOLULU, "US"}, {&R_SAN_JUAN, "PR"}};
    for (const PaysVu &v : vus) {
        VERIFIER(v.reponse->ok());
        VERIFIER(fournisseurs::concerne_le_pays(*f, v.pays));
    }
    for (std::size_t i = 0; i < f->pays.size();
         i += 3) { // chaque pays de la table a sa capture : jamais un code deviné
        bool vu = false;
        for (const PaysVu &v : vus)
            vu = vu || v.pays == f->pays.substr(i, 2);
        VERIFIER(vu);
    }
    // Lyon : le NWS répond 404 (hors couverture) ; la table ne propose pas le NWS en France ni au Canada
    nws::Maille m;
    VERIFIER(!nws::analyser_points(PTS_LYON, m).ok());
    for (const sv hors : {sv("FR"), sv("CA"), sv("MX"), sv("GB")})
        VERIFIER(!fournisseurs::concerne_le_pays(*f, hors));
    return 0;
}

// ---- /points : cas limites écrits à la main --------------------------------------------------------------------------------

constexpr sv POINTS_BONS = R"({"properties":{"gridId":"SEW","gridX":125,"gridY":68,"timeZone":"America/Los_Angeles"}})";

constexpr Diagnostic points(sv texte) {
    nws::Maille m;
    return nws::analyser_points(texte, m);
}

constexpr int une_maille_correcte_est_lue() {
    nws::Maille m;
    VERIFIER(nws::analyser_points(POINTS_BONS, m).ok());
    VERIFIER(meme_texte(m.bureau, "SEW") && m.x == 125 && m.y == 68 && meme_texte(m.fuseau, "America/Los_Angeles"));
    VERIFIER(nws::analyser_points(
                 R"({"properties":{"gridId":"AER","gridX":0,"gridY":9999,"timeZone":"America/Anchorage"}})", m)
                 .ok());
    VERIFIER(m.x == 0 && m.y == 9999);
    return 0;
}

constexpr int les_samoa_americaines_sans_maille_sont_un_refus() {
    // Forme relevée le 07/11/2026 pour Pago Pago : HTTP 200, mais gridId, gridX et gridY nuls. Le NWS n'a pas de prévision.
    nws::Maille m;
    const Diagnostic d = nws::analyser_points(
        R"({"properties":{"gridId":null,"gridX":null,"gridY":null,"timeZone":"Pacific/Pago_Pago"}})", m);
    VERIFIER(!d.ok() && d.erreur == Erreur::ChampAbsent && d.champ == "gridId");
    VERIFIER(m.bureau[0] == '\0');
    return 0;
}

constexpr int un_bureau_mal_forme_est_refuse() {
    // trois lettres capitales exactement : le bureau entre dans l'adresse demandée ensuite
    for (const sv b :
         {sv("sew"), sv("SE"), sv("SEWA"), sv("S3W"), sv(""), sv("SE/"), sv("S W"), sv("../"), sv("SEW\\u0000")}) {
        Texte<256> d;
        d += R"({"properties":{"gridId":")";
        d += b;
        d += R"(","gridX":1,"gridY":2,"timeZone":"America/Los_Angeles"}})";
        const Diagnostic r = points(d.vue());
        VERIFIER(!r.ok());
        VERIFIER(r.champ == "gridId");
    }
    VERIFIER(points(R"({"properties":{"gridId":12,"gridX":1,"gridY":2,"timeZone":"America/Los_Angeles"}})").erreur ==
             Erreur::MauvaisType);
    return 0;
}

constexpr int une_maille_hors_plage_ou_mal_typee_est_refusee() {
    constexpr sv DEBUT = R"({"properties":{"gridId":"SEW",)";
    constexpr sv FIN = R"("timeZone":"America/Los_Angeles"}})";
    const auto avec = [&](sv xy) {
        Texte<256> d;
        d += DEBUT;
        d += xy;
        d += FIN;
        return points(d.vue());
    };
    VERIFIER(avec(R"("gridX":10000,"gridY":1,)").erreur == Erreur::HorsPlage);
    VERIFIER(avec(R"("gridX":1,"gridY":10000,)").erreur == Erreur::HorsPlage);
    VERIFIER(avec(R"("gridX":-1,"gridY":1,)").erreur == Erreur::HorsPlage);
    VERIFIER(avec(R"("gridX":1,"gridY":-3,)").erreur == Erreur::HorsPlage);
    VERIFIER(avec(R"("gridX":"125","gridY":1,)").erreur == Erreur::MauvaisType);
    VERIFIER(avec(R"("gridX":1,"gridY":"68",)").erreur == Erreur::MauvaisType);
    VERIFIER(avec(R"("gridY":1,)").erreur == Erreur::ChampAbsent);
    VERIFIER(avec(R"("gridX":1,)").erreur == Erreur::ChampAbsent);
    VERIFIER(avec(R"("gridX":1,"gridY":2,)").ok());
    return 0;
}

constexpr int un_fuseau_inconnu_ou_absent_dans_points_est_refuse() {
    const Diagnostic a = points(R"({"properties":{"gridId":"SEW","gridX":1,"gridY":2,"timeZone":"Mars/Olympus"}})");
    VERIFIER(!a.ok() && a.erreur == Erreur::HorsPlage && a.champ == "timeZone");
    const Diagnostic b = points(R"({"properties":{"gridId":"SEW","gridX":1,"gridY":2}})");
    VERIFIER(!b.ok() && b.erreur == Erreur::ChampAbsent && b.champ == "timeZone");
    const Diagnostic c = points(R"({"properties":{"gridId":"SEW","gridX":1,"gridY":2,"timeZone":""}})");
    VERIFIER(!c.ok() && c.champ == "timeZone");
    const Diagnostic d = points(R"({"properties":{"gridId":"SEW","gridX":1,"gridY":2,"timeZone":7}})");
    VERIFIER(!d.ok() && d.erreur == Erreur::MauvaisType);
    return 0;
}

constexpr int points_qui_n_est_pas_du_json_ou_pas_un_objet_est_refuse() {
    VERIFIER(points("").erreur == Erreur::JsonInvalide);
    VERIFIER(points("{\"properties\":").erreur == Erreur::JsonInvalide);
    VERIFIER(points("[]").erreur == Erreur::JsonInvalide);
    VERIFIER(points("{}").erreur == Erreur::ChampAbsent);
    VERIFIER(points("{\"properties\":[]}").erreur == Erreur::ChampAbsent ||
             points("{\"properties\":[]}").erreur == Erreur::MauvaisType);
    VERIFIER(!points("{\"properties\":[]}").ok());
    return 0;
}

// ---- /gridpoints : petites grilles écrites à la main ------------------------------------------------------------------------

using Doc = Texte<32768>;
using Petit = Texte<640>;

constexpr long long RECU =
    1794052800 + 3420;       // 2026-11-07 12:57 UTC ; les couches de base commencent à 12:00 et durent 72 h
constexpr int LAT = 5150720; // Londres : le fuseau est UTC en novembre, les jours locaux sont donc les jours UTC
constexpr int LON = -12760;

constexpr nws::Maille maille(sv bureau, int x, int y, sv fuseau) {
    nws::Maille m;
    for (std::size_t i = 0; i < bureau.size() && i < 3; ++i)
        m.bureau[i] = bureau[i];
    m.x = static_cast<std::int16_t>(x);
    m.y = static_cast<std::int16_t>(y);
    for (std::size_t i = 0; i < fuseau.size() && i + 1 < sizeof m.fuseau; ++i)
        m.fuseau[i] = fuseau[i];
    return m;
}
constexpr nws::Maille MAILLE = maille("SEW", 125, 68, "Europe/London");

constexpr sv TEMPERATURE_72H =
    R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":10.0}]})";
constexpr sv RIEN =
    R"({"coverage":null,"weather":null,"intensity":null,"visibility":{"unitCode":"wmoUnit:km","value":null},"attributes":[]})";

constexpr Petit ciel(sv pourcent) {
    Petit p;
    p += R"("skyCover":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":)";
    p += pourcent;
    p += "}]}";
    return p;
}

constexpr Petit temps(sv elements) {
    Petit p;
    p += R"("weather":{"values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":[)";
    p += elements;
    p += "]}]}";
    return p;
}

// Un élément de la couche weather. `couverture` et `intensite` : le mot, ou "" pour null ; `attributs` : le contenu JSON du tableau.
constexpr Petit element(sv nom, sv intensite = "", sv couverture = "definite", sv attributs = "") {
    Petit p;
    p += R"({"coverage":)";
    if (couverture.empty()) {
        p += "null";
    } else {
        p += "\"";
        p += couverture;
        p += "\"";
    }
    p += R"(,"weather":")";
    p += nom;
    p += R"(","intensity":)";
    if (intensite.empty()) {
        p += "null";
    } else {
        p += "\"";
        p += intensite;
        p += "\"";
    }
    p += R"(,"visibility":{"unitCode":"wmoUnit:km","value":null},"attributes":[)";
    p += attributs;
    p += "]}";
    return p;
}

constexpr Doc grille(sv temperature, sv ciel_json, sv temps_json, sv autres = "",
                     sv maille_id = R"("gridId":"SEW","gridX":125,"gridY":68,)") {
    Doc d;
    d += R"({"properties":{)";
    d += maille_id;
    d += R"("validTimes":"2026-11-07T12:00:00+00:00/P3D",)";
    d += temperature;
    d += ",";
    d += ciel_json;
    d += ",";
    d += temps_json;
    if (!autres.empty()) {
        d += ",";
        d += autres;
    }
    d += "}}";
    return d;
}

constexpr Doc grille_de_base(sv autres = "") {
    return grille(TEMPERATURE_72H, ciel("50").vue(), temps(RIEN).vue(), autres);
}

constexpr AnalyseRiche lire_grille(const Doc &d, const nws::Maille &m = MAILLE) {
    nws::Serie travail;
    return nws::analyser_nws(d.vue(), m, RECU, LAT, LON, travail);
}

constexpr int une_grille_minimale_donne_le_courant_et_dit_pourquoi_le_reste_manque() {
    const AnalyseRiche a = lire_grille(grille_de_base());
    VERIFIER(a.ok());
    const Releve &r = a.meteo.releve;
    VERIFIER(r.instant_s == RECU && r.temperature_dixiemes == 100 && r.code_wmo == 2);
    VERIFIER(r.ressentie_dixiemes == INCONNU_DIX); // jamais la température à la place
    VERIFIER(r.humidite_pct == INCONNU_ENTIER && r.direction_vent_deg == INCONNU_ENTIER &&
             r.proba_precip_pct == INCONNU_ENTIER);
    VERIFIER(r.vent_dixiemes_kmh == INCONNU_DIX && r.rafales_dixiemes_kmh == INCONNU_DIX &&
             r.pression_dixiemes_hpa == INCONNU_DIX);
    VERIFIER(a.meteo.riche.nuages_pct == 50);
    VERIFIER(a.diag_heures.ok() && a.meteo.riche.nb_heures == 24);
    VERIFIER(a.meteo.riche.heures[0].debut_s == 1794052800 &&
             a.meteo.riche.heures[23].debut_s == 1794052800 + 23 * 3600);
    VERIFIER(a.meteo.riche.heures[0].precip_dixiemes_mm == INCONNU_DIX);
    VERIFIER(a.diag_jours.ok() && r.nb_jours == 4); // 07, 08, 09 et la moitié du 10 novembre
    VERIFIER(r.jours[0].date_s == 1794009600);
    VERIFIER(a.diag_quarts.erreur == Erreur::ChampAbsent && a.meteo.riche.nb_quarts == 0);
    return 0;
}

constexpr int le_courant_les_heures_et_les_jours_lisent_chaque_couche() {
    const Doc d = grille_de_base(
        R"("apparentTemperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":7.24}]},)"
        R"("relativeHumidity":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":80}]},)"
        R"("windSpeed":{"uom":"wmoUnit:km_h-1","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":12.5}]},)"
        R"("windGust":{"uom":"wmoUnit:km_h-1","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":30}]},)"
        R"J("windDirection":{"uom":"wmoUnit:degree_(angle)","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":270}]})J");
    const AnalyseRiche a = lire_grille(d);
    VERIFIER(a.ok() && a.diag_heures.ok() && a.diag_jours.ok());
    const Releve &r = a.meteo.releve;
    VERIFIER(r.ressentie_dixiemes == 72 && r.humidite_pct == 80 && r.vent_dixiemes_kmh == 125 &&
             r.rafales_dixiemes_kmh == 300);
    VERIFIER(r.direction_vent_deg == 270);
    const HeureRiche &h = a.meteo.riche.heures[5];
    VERIFIER(h.ressentie_dixiemes == 72 && h.humidite_pct == 80 && h.vent_dixiemes_kmh == 125 &&
             h.rafales_dixiemes_kmh == 300);
    VERIFIER(h.direction_deg == 270 && h.uv_dixiemes == INCONNU_DIX && h.nuages_pct == 50);
    VERIFIER(a.meteo.riche.jours[1].ressentie_min_dixiemes == 72 &&
             a.meteo.riche.jours[1].vent_max_dixiemes_kmh == 125);
    VERIFIER(a.meteo.riche.jours[1].rafales_max_dixiemes_kmh == 300);
    VERIFIER(a.meteo.riche.jours[1].direction_dominante_deg == INCONNU_ENTIER);
    return 0;
}

constexpr int une_valeur_nulle_reste_inconnue_et_ne_vaut_jamais_zero() {
    const Doc d = grille_de_base(
        R"("relativeHumidity":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":null}]},)"
        R"("windSpeed":{"uom":"wmoUnit:km_h-1","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":null}]})");
    const AnalyseRiche a = lire_grille(d);
    VERIFIER(a.ok());
    VERIFIER(a.meteo.releve.humidite_pct == INCONNU_ENTIER && a.meteo.releve.vent_dixiemes_kmh == INCONNU_DIX);
    VERIFIER(a.meteo.riche.heures[3].humidite_pct == INCONNU_ENTIER &&
             a.meteo.riche.heures[3].vent_dixiemes_kmh == INCONNU_DIX);
    return 0;
}

constexpr int le_minimum_et_le_maximum_du_jour_viennent_des_intervalles_qui_le_touchent() {
    const Doc d = grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT12H","value":5.0},)"
        R"({"validTime":"2026-11-08T00:00:00+00:00/PT12H","value":12.0},{"validTime":"2026-11-08T12:00:00+00:00/PT60H","value":8.0}]})",
        ciel("50").vue(), temps(RIEN).vue());
    const AnalyseRiche a = lire_grille(d);
    VERIFIER(a.ok() && a.diag_jours.ok() && a.meteo.releve.nb_jours == 4);
    const Releve &r = a.meteo.releve;
    VERIFIER(r.jours[0].min_dixiemes == 50 &&
             r.jours[0].max_dixiemes == 50); // le jour 0 ne touche que l'intervalle 12:00-24:00
    VERIFIER(r.jours[1].min_dixiemes == 80 &&
             r.jours[1].max_dixiemes == 120); // 08/11 : 00:00-12:00 (12,0), puis 12:00 (8,0)
    VERIFIER(r.jours[2].min_dixiemes == 80 && r.jours[2].max_dixiemes == 80);
    return 0;
}

constexpr int la_grille_doit_couvrir_l_instant_present() {
    const Doc d = grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T13:00:00+00:00/PT71H","value":10.0}]})",
        ciel("50").vue(), temps(RIEN).vue());
    const AnalyseRiche a = lire_grille(d);
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::HorsPlage && a.diag.champ == "temperature");
    return 0;
}

constexpr int une_temperature_manquante_dans_les_24_heures_vide_les_heures_seulement() {
    const Doc d = grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT10H","value":10.0},)"
        R"({"validTime":"2026-11-07T22:00:00+00:00/PT2H","value":null},{"validTime":"2026-11-08T00:00:00+00:00/PT60H","value":8.0}]})",
        ciel("50").vue(), temps(RIEN).vue());
    const AnalyseRiche a = lire_grille(d);
    VERIFIER(a.ok());
    VERIFIER(!a.diag_heures.ok() && a.diag_heures.erreur == Erreur::TableauxIncoherents &&
             a.meteo.riche.nb_heures == 0);
    VERIFIER(a.diag_jours.ok() && a.meteo.releve.nb_jours >= 2);
    VERIFIER(a.meteo.releve.jours[0].min_dixiemes == 100 &&
             a.meteo.releve.jours[0].max_dixiemes == 100); // l'inconnu n'est pas un zéro
    return 0;
}

constexpr int le_ciel_et_le_temps_sont_obligatoires_pour_le_courant() {
    // sans skyCover : pas de code WMO du courant
    Doc sans_ciel;
    sans_ciel +=
        R"({"properties":{"gridId":"SEW","gridX":125,"gridY":68,"validTimes":"2026-11-07T12:00:00+00:00/P3D",)";
    sans_ciel += TEMPERATURE_72H;
    sans_ciel += ",";
    sans_ciel += temps(RIEN).vue();
    sans_ciel += "}}";
    const AnalyseRiche a = lire_grille(sans_ciel);
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::ChampAbsent && a.diag.champ == "skyCover");
    // sans weather : idem (le ciel seul ne dit pas qu'il ne pleut pas)
    Doc sans_temps;
    sans_temps +=
        R"({"properties":{"gridId":"SEW","gridX":125,"gridY":68,"validTimes":"2026-11-07T12:00:00+00:00/P3D",)";
    sans_temps += TEMPERATURE_72H;
    sans_temps += ",";
    sans_temps += ciel("50").vue();
    sans_temps += "}}";
    const AnalyseRiche b = lire_grille(sans_temps);
    VERIFIER(!b.ok() && b.diag.champ == "skyCover");
    return 0;
}

// ---- le temps (couche weather) et le ciel (skyCover) -> code WMO du projet -------------------------------------------------------

constexpr int code_pour(sv pourcent_ciel, sv elements) {
    const AnalyseRiche a = lire_grille(grille(TEMPERATURE_72H, ciel(pourcent_ciel).vue(), temps(elements).vue()));
    return a.ok() ? a.meteo.releve.code_wmo : -100;
}

struct Phenomene {
    sv nom;
    sv intensite;
    sv couverture;
    sv attributs;
    int attendu;
};
constexpr Phenomene PHENOMENES[] = {
    {"rain", "light", "definite", "", 61},
    {"rain", "very_light", "definite", "", 61},
    {"rain", "moderate", "definite", "", 63},
    {"rain", "", "definite", "", 63},
    {"rain", "heavy", "definite", "", 65},
    {"rain", "light", "chance", "", 61}, // « chance » est une vraie probabilité, pas une simple possibilité
    {"rain", "light", "likely", "", 61},
    {"rain", "light", "", "", 61},
    {"drizzle", "light", "definite", "", 51},
    {"drizzle", "moderate", "definite", "", 53},
    {"drizzle", "heavy", "definite", "", 55},
    {"freezing_drizzle", "light", "definite", "", 56},
    {"freezing_drizzle", "moderate", "definite", "", 57},
    {"freezing_drizzle", "heavy", "definite", "", 57},
    {"rain_showers", "light", "definite", "", 80},
    {"rain_showers", "moderate", "definite", "", 81},
    {"rain_showers", "heavy", "definite", "", 82},
    {"freezing_rain", "light", "definite", "", 66},
    {"freezing_rain", "heavy", "definite", "", 67},
    {"snow", "light", "definite", "", 71},
    {"snow", "moderate", "definite", "", 73},
    {"snow", "heavy", "definite", "", 75},
    {"snow_showers", "light", "definite", "", 85},
    {"snow_showers", "moderate", "definite", "", 85},
    {"snow_showers", "heavy", "definite", "", 86},
    {"sleet", "light", "definite", "", 77},
    {"sleet", "heavy", "definite", "", 77},
    {"hail", "", "definite", "", 96},
    {"hail", "", "definite", R"("large_hail")", 99},
    {"thunderstorms", "", "definite", "", 95},
    {"thunderstorms", "heavy", "definite", "", 97},
    {"thunderstorms", "", "definite", R"("small_hail")", 96},
    {"thunderstorms", "", "definite", R"("large_hail")", 99},
    {"thunderstorms", "heavy", "definite", R"("damaging_wind","large_hail")", 99},
    {"fog", "", "patchy", "", 45},
    {"freezing_fog", "", "definite", "", 48},
    {"ice_fog", "", "definite", "", 48},
    // sans équivalent parmi nos codes : ignoré, le ciel garde sa place
    {"haze", "", "definite", "", 2},
    {"smoke", "", "definite", "", 2},
    {"frost", "", "definite", "", 2},
    {"blowing_snow", "", "definite", "", 2},
    {"blowing_dust", "", "definite", "", 2},
    {"blowing_sand", "", "definite", "", 2},
    {"ice_crystals", "", "definite", "", 2},
    {"volcanic_ash", "", "definite", "", 2},
    {"water_spouts", "", "definite", "", 2},
    {"freezing_spray", "", "definite", "", 2},
    // une simple possibilité n'est pas un phénomène prévu
    {"rain", "light", "slight_chance", "", 2},
    {"thunderstorms", "heavy", "isolated", "", 2},
    {"snow", "light", "few", "", 2},
    {"rain_showers", "light", "brief", "", 2},
};

constexpr int chaque_phenomene_donne_son_code() {
    for (const Phenomene &p : PHENOMENES) {
        if (code_pour("50", element(p.nom, p.intensite, p.couverture, p.attributs).vue()) != p.attendu)
            return __LINE__;
    }
    return 0;
}

constexpr int le_plus_fort_du_ciel_et_du_phenomene_l_emporte() {
    VERIFIER(code_pour("100", element("haze").vue()) == 3); // ciel couvert, brume sèche ignorée
    VERIFIER(code_pour("100", element("fog").vue()) == 45); // le brouillard est plus fort que « couvert »
    VERIFIER(code_pour("0", element("rain", "light").vue()) == 61);
    // deux phénomènes dans le même intervalle : le plus fort
    Petit deux;
    deux += element("rain", "light").vue();
    deux += ",";
    deux += element("thunderstorms").vue();
    VERIFIER(code_pour("50", deux.vue()) == 95);
    return 0;
}

constexpr int les_seuils_du_ciel() {
    // < 12,5 % : 0 ; < 37,5 % : 1 ; < 87,5 % : 2 ; sinon 3 (pourcents entiers : 12 | 13, 37 | 38, 87 | 88)
    VERIFIER(code_pour("0", RIEN) == 0 && code_pour("12", RIEN) == 0);
    VERIFIER(code_pour("13", RIEN) == 1 && code_pour("37", RIEN) == 1);
    VERIFIER(code_pour("38", RIEN) == 2 && code_pour("87", RIEN) == 2);
    VERIFIER(code_pour("88", RIEN) == 3 && code_pour("100", RIEN) == 3);
    return 0;
}

constexpr int un_phenomene_ou_une_intensite_inconnus_font_refuser_la_reponse() {
    VERIFIER(code_pour("50", element("tornado").vue()) == -100);
    VERIFIER(code_pour("50", element("rain", "extreme").vue()) == -100);
    VERIFIER(code_pour("50", "7") == -100); // un élément qui n'est pas un objet
    VERIFIER(code_pour("50", R"({"weather":12})") == -100);
    const AnalyseRiche a =
        lire_grille(grille(TEMPERATURE_72H, ciel("50").vue(), temps(element("tornado").vue()).vue()));
    VERIFIER(!a.ok() && a.diag.champ == "weather");
    return 0;
}

// ---- unités, intervalles, maille ------------------------------------------------------------------------------------------

constexpr int une_unite_inattendue_fait_refuser_la_reponse() {
    const AnalyseRiche a = lire_grille(grille(
        R"("temperature":{"uom":"wmoUnit:degF","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":50.0}]})",
        ciel("50").vue(), temps(RIEN).vue()));
    VERIFIER(!a.ok() && a.diag.champ == "temperature");
    const AnalyseRiche b = lire_grille(grille_de_base(
        R"("windSpeed":{"uom":"wmoUnit:m_s-1","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":5}]})"));
    VERIFIER(!b.ok() && b.diag.champ == "windSpeed");
    const AnalyseRiche c = lire_grille(
        grille_de_base(R"("windDirection":{"values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":5}]})"));
    VERIFIER(!c.ok() && c.diag.champ == "windDirection"); // pas d'unité du tout
    return 0;
}

constexpr int des_intervalles_qui_se_chevauchent_ou_a_l_envers_font_refuser_la_reponse() {
    const AnalyseRiche a = lire_grille(grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT10H","value":10.0},)"
        R"({"validTime":"2026-11-07T20:00:00+00:00/PT10H","value":9.0}]})",
        ciel("50").vue(), temps(RIEN).vue()));
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::TableauxIncoherents && a.diag.champ == "temperature");
    const AnalyseRiche b = lire_grille(grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-08T12:00:00+00:00/PT10H","value":10.0},)"
        R"({"validTime":"2026-11-07T12:00:00+00:00/PT10H","value":9.0}]})",
        ciel("50").vue(), temps(RIEN).vue()));
    VERIFIER(!b.ok() && b.diag.erreur == Erreur::TableauxIncoherents);
    // avant le début de validTimes de la grille
    const AnalyseRiche c = lire_grille(grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T10:00:00+00:00/PT72H","value":10.0}]})",
        ciel("50").vue(), temps(RIEN).vue()));
    VERIFIER(!c.ok() && c.diag.erreur == Erreur::TableauxIncoherents);
    // un trou entre deux intervalles est permis : la valeur y est inconnue, et les 24 heures le disent
    const AnalyseRiche d = lire_grille(grille(
        R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT3H","value":10.0},)"
        R"({"validTime":"2026-11-07T16:00:00+00:00/PT68H","value":9.0}]})",
        ciel("50").vue(), temps(RIEN).vue()));
    VERIFIER(d.ok() && !d.diag_heures.ok() && d.meteo.riche.nb_heures == 0);
    return 0;
}

constexpr int un_intervalle_illisible_fait_refuser_la_reponse() {
    for (const sv vt : {sv("demain"), sv(""), sv("2026-11-07T12:00:00+00:00"), sv("2026-11-07T12:00:00+00:00/"),
                        sv("2026-11-07T12:00:00+00:00/PT"), sv("2026-11-07T12:00:00+00:00/P1M"),
                        sv("2026-13-07T12:00:00+00:00/PT1H")}) {
        Doc t;
        t += R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":")";
        t += vt;
        t += R"(","value":10.0}]})";
        const AnalyseRiche a = lire_grille(grille(t.vue(), ciel("50").vue(), temps(RIEN).vue()));
        VERIFIER(!a.ok());
    }
    return 0;
}

constexpr int la_grille_doit_etre_celle_de_la_maille_demandee() {
    const AnalyseRiche autre_bureau = lire_grille(grille_de_base(), maille("OKX", 125, 68, "Europe/London"));
    VERIFIER(!autre_bureau.ok() && autre_bureau.diag.erreur == Erreur::TableauxIncoherents &&
             autre_bureau.diag.champ == "gridId");
    VERIFIER(!lire_grille(grille_de_base(), maille("SEW", 126, 68, "Europe/London")).ok());
    VERIFIER(!lire_grille(grille_de_base(), maille("SEW", 125, 69, "Europe/London")).ok());
    VERIFIER(lire_grille(grille_de_base(), maille("SEW", 125, 68, "Europe/London")).ok());
    // gridId absent de la grille
    const AnalyseRiche sans_id = lire_grille(grille(TEMPERATURE_72H, ciel("50").vue(), temps(RIEN).vue(), "", ""));
    VERIFIER(!sans_id.ok());
    return 0;
}

constexpr int un_fuseau_inconnu_de_la_maille_fait_refuser_la_grille() {
    const AnalyseRiche a = lire_grille(grille_de_base(), maille("SEW", 125, 68, "Mars/Olympus"));
    VERIFIER(!a.ok() && a.diag.champ == "fuseau");
    VERIFIER(!lire_grille(grille_de_base(), maille("SEW", 125, 68, "")).ok());
    return 0;
}

constexpr int une_grille_sans_validTimes_ou_pas_du_json_est_refusee() {
    Doc d;
    d += R"({"properties":{"gridId":"SEW","gridX":125,"gridY":68,)";
    d += TEMPERATURE_72H;
    d += "}}";
    const AnalyseRiche a = lire_grille(d);
    VERIFIER(!a.ok() && a.diag.erreur == Erreur::ChampAbsent && a.diag.champ == "validTimes");
    nws::Serie travail;
    VERIFIER(nws::analyser_nws("{\"properties\":", MAILLE, RECU, LAT, LON, travail).diag.erreur ==
             Erreur::JsonInvalide);
    VERIFIER(nws::analyser_nws("[]", MAILLE, RECU, LAT, LON, travail).diag.erreur == Erreur::JsonInvalide);
    VERIFIER(!nws::analyser_nws("{}", MAILLE, RECU, LAT, LON, travail).ok());
    VERIFIER(!nws::analyser_nws("{\"properties\":[]}", MAILLE, RECU, LAT, LON, travail).ok());
    return 0;
}

constexpr int une_valeur_invraisemblable_fait_refuser_la_reponse() {
    VERIFIER(
        !lire_grille(
             grille(
                 R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":500.0}]})",
                 ciel("50").vue(), temps(RIEN).vue()))
             .ok());
    VERIFIER(
        !lire_grille(
             grille(
                 R"("temperature":{"uom":"wmoUnit:degC","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":"chaud"}]})",
                 ciel("50").vue(), temps(RIEN).vue()))
             .ok());
    VERIFIER(
        !lire_grille(
             grille_de_base(
                 R"("relativeHumidity":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":140}]})"))
             .ok());
    VERIFIER(
        !lire_grille(
             grille_de_base(
                 R"("relativeHumidity":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":-1}]})"))
             .ok());
    // 306 % ne doit pas se retrouver à 50 % en tenant dans un octet (306 - 256) : refusé, comme 140
    const AnalyseRiche trop = lire_grille(grille_de_base(
        R"("relativeHumidity":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT72H","value":306}]})"));
    VERIFIER(!trop.ok() && trop.diag.erreur == Erreur::HorsPlage && trop.diag.champ == "relativeHumidity");
    const AnalyseRiche proba = lire_grille(grille_de_base(
        R"("probabilityOfPrecipitation":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT3H","value":356}]})"));
    VERIFIER(!proba.ok() && proba.diag.champ == "probabilityOfPrecipitation");
    const AnalyseRiche ciel_fou = lire_grille(grille(TEMPERATURE_72H, ciel("300").vue(), temps(RIEN).vue()));
    VERIFIER(!ciel_fou.ok() && ciel_fou.diag.champ == "skyCover");
    return 0;
}

// ---- pluie, neige, probabilité : jamais répartis sur des heures qu'ils ne couvrent pas -------------------------------------------

constexpr sv PLUIE =
    R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-07T13:00:00+00:00/PT1H","value":2.3},)"
    R"({"validTime":"2026-11-07T14:00:00+00:00/PT2H","value":5.0},{"validTime":"2026-11-08T00:00:00+00:00/PT6H","value":1.0},)"
    R"({"validTime":"2026-11-08T06:00:00+00:00/PT6H","value":0.5},{"validTime":"2026-11-08T12:00:00+00:00/PT6H","value":0.0},)"
    R"({"validTime":"2026-11-08T18:00:00+00:00/PT6H","value":2.0}]})";

constexpr int le_cumul_d_une_heure_n_existe_que_pour_un_intervalle_d_exactement_cette_heure() {
    const AnalyseRiche a = lire_grille(grille_de_base(PLUIE));
    VERIFIER(a.ok() && a.diag_heures.ok());
    const Riche &q = a.meteo.riche;
    VERIFIER(q.heures[0].precip_dixiemes_mm == INCONNU_DIX); // 12:00 : aucun intervalle
    VERIFIER(q.heures[1].precip_dixiemes_mm == 23);          // 13:00 : un intervalle d'une heure, 2,3 mm
    VERIFIER(q.heures[2].precip_dixiemes_mm == INCONNU_DIX); // 14:00 : l'intervalle de 2 h n'est PAS réparti
    VERIFIER(q.heures[3].precip_dixiemes_mm == INCONNU_DIX);
    VERIFIER(q.heures[4].precip_dixiemes_mm == INCONNU_DIX);
    return 0;
}

constexpr int le_cumul_d_un_jour_n_existe_que_si_des_intervalles_entiers_couvrent_ses_24_heures() {
    const AnalyseRiche a = lire_grille(grille_de_base(PLUIE));
    VERIFIER(a.ok() && a.diag_jours.ok());
    const Riche &q = a.meteo.riche;
    VERIFIER(q.jours[0].precip_dixiemes_mm == INCONNU_DIX); // 07/11 : la grille commence à 12:00
    VERIFIER(q.jours[1].precip_dixiemes_mm ==
             35); // 08/11 : 1,0 + 0,5 + 0,0 + 2,0 mm, intervalles entiers de 00:00 à 24:00
    VERIFIER(q.jours[2].precip_dixiemes_mm == INCONNU_DIX); // 09/11 : aucun intervalle
    return 0;
}

constexpr int un_intervalle_qui_deborde_le_jour_ou_un_trou_rend_le_cumul_inconnu() {
    // 18:00 -> 06:00 chevauche minuit : ni le 08 ni le 07 n'ont de cumul
    const AnalyseRiche a = lire_grille(grille_de_base(
        R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-07T18:00:00+00:00/PT12H","value":4.0},)"
        R"({"validTime":"2026-11-08T06:00:00+00:00/PT18H","value":3.0}]})"));
    VERIFIER(a.ok());
    VERIFIER(a.meteo.riche.jours[0].precip_dixiemes_mm == INCONNU_DIX &&
             a.meteo.riche.jours[1].precip_dixiemes_mm == INCONNU_DIX);
    // un intervalle de 24 h exactement sur le jour : son cumul est celui du jour
    const AnalyseRiche b = lire_grille(grille_de_base(
        R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-08T00:00:00+00:00/P1D","value":6.4}]})"));
    VERIFIER(b.ok() && b.meteo.riche.jours[1].precip_dixiemes_mm == 64);
    // un intervalle de valeur nulle dans la chaîne rend le jour inconnu, pas plus faible
    const AnalyseRiche c = lire_grille(grille_de_base(
        R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-08T00:00:00+00:00/PT12H","value":null},)"
        R"({"validTime":"2026-11-08T12:00:00+00:00/PT12H","value":2.0}]})"));
    VERIFIER(c.ok() && c.meteo.riche.jours[1].precip_dixiemes_mm == INCONNU_DIX);
    // un trou de six heures
    const AnalyseRiche d = lire_grille(grille_de_base(
        R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-08T00:00:00+00:00/PT6H","value":1.0},)"
        R"({"validTime":"2026-11-08T12:00:00+00:00/PT12H","value":2.0}]})"));
    VERIFIER(d.ok() && d.meteo.riche.jours[1].precip_dixiemes_mm == INCONNU_DIX);
    // un intervalle qui commence dans le jour et finit le lendemain : le cumul du jour est inconnu, l'intervalle n'est pas compté en entier
    const AnalyseRiche e = lire_grille(grille_de_base(
        R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-08T00:00:00+00:00/PT12H","value":2.0},)"
        R"({"validTime":"2026-11-08T12:00:00+00:00/PT24H","value":5.0}]})"));
    VERIFIER(e.ok() && e.meteo.riche.jours[1].precip_dixiemes_mm == INCONNU_DIX);
    // un cumul négatif est refusé
    VERIFIER(
        !lire_grille(
             grille_de_base(
                 R"("quantitativePrecipitation":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-08T00:00:00+00:00/P1D","value":-1.0}]})"))
             .ok());
    return 0;
}

constexpr int la_neige_est_en_millimetres_dans_la_source_et_en_centimetres_dans_le_noyau() {
    const AnalyseRiche a = lire_grille(grille_de_base(
        R"("snowfallAmount":{"uom":"wmoUnit:mm","values":[{"validTime":"2026-11-08T00:00:00+00:00/PT12H","value":20},)"
        R"({"validTime":"2026-11-08T12:00:00+00:00/PT12H","value":5}]})"));
    VERIFIER(a.ok());
    VERIFIER(a.meteo.riche.jours[1].neige_dixiemes_cm == 25); // 25 mm = 2,5 cm
    VERIFIER(a.meteo.riche.jours[0].neige_dixiemes_cm == INCONNU_DIX);
    return 0;
}

constexpr int la_probabilite_d_une_heure_est_celle_d_un_intervalle_de_six_heures_au_plus() {
    const AnalyseRiche a = lire_grille(grille_de_base(
        R"("probabilityOfPrecipitation":{"uom":"wmoUnit:percent","values":[{"validTime":"2026-11-07T12:00:00+00:00/PT3H","value":20},)"
        R"({"validTime":"2026-11-07T15:00:00+00:00/PT6H","value":40},{"validTime":"2026-11-07T21:00:00+00:00/PT12H","value":70}]})"));
    VERIFIER(a.ok() && a.diag_heures.ok() && a.diag_jours.ok());
    const Riche &q = a.meteo.riche;
    VERIFIER(a.meteo.releve.proba_precip_pct == 20); // 12:57 est dans le premier intervalle
    VERIFIER(q.heures[0].proba_pct == 20 && q.heures[2].proba_pct == 20);
    VERIFIER(q.heures[3].proba_pct == 40 && q.heures[8].proba_pct == 40);
    VERIFIER(q.heures[9].proba_pct == INCONNU_ENTIER &&
             q.heures[20].proba_pct == INCONNU_ENTIER); // 12 h : pas celle d'une heure
    // le maximum du jour est celui de tous les intervalles qui le touchent, quelle que soit leur durée
    VERIFIER(a.meteo.releve.jours[0].proba_precip_max == 70 && a.meteo.releve.jours[1].proba_precip_max == 70);
    VERIFIER(a.meteo.releve.jours[2].proba_precip_max == INCONNU_ENTIER);
    return 0;
}

constexpr int une_couche_plus_longue_que_le_tableau_est_tronquee_sans_deborder() {
    static_assert(nws::INTERVALLES_MAX == 192);
    Doc t;
    t += R"("temperature":{"uom":"wmoUnit:degC","values":[)";
    for (int i = 0; i < 200; ++i) {
        const int depuis = 12 + i; // heures depuis le 07/11 à 00:00 UTC ; 200 heures : on reste en novembre
        const int jour = 7 + depuis / 24;
        const int h = depuis % 24;
        const char c[] = {static_cast<char>('0' + jour / 10), static_cast<char>('0' + jour % 10), 'T',
                          static_cast<char>('0' + h / 10), static_cast<char>('0' + h % 10)};
        t += i == 0 ? R"({"validTime":"2026-11-)" : R"(,{"validTime":"2026-11-)";
        t += sv(c, 5);
        t += R"(:00:00+00:00/PT1H","value":10.0})";
    }
    t += "]}";
    VERIFIER(!t.trop);
    nws::Serie travail;
    Doc d = grille(t.vue(), ciel("50").vue(), temps(RIEN).vue());
    VERIFIER(!d.trop);
    const AnalyseRiche a = nws::analyser_nws(d.vue(), MAILLE, RECU, LAT, LON, travail);
    VERIFIER(a.ok());
    VERIFIER(travail.temperature.n == nws::INTERVALLES_MAX);
    return 0;
}

} // namespace

#define LISTE(X)                                                                                                       \
    X(new_york_identique_a_python)                                                                                     \
    X(miami_identique_a_python)                                                                                        \
    X(seattle_identique_a_python)                                                                                      \
    X(anchorage_identique_a_python)                                                                                    \
    X(honolulu_identique_a_python)                                                                                     \
    X(san_juan_identique_a_python)                                                                                     \
    X(les_six_mailles_reelles_sont_lues)                                                                               \
    X(hors_de_la_couverture_le_404_est_un_refus_qui_laisse_la_maille_vide)                                             \
    X(ce_que_la_source_ne_donne_pas_reste_inconnu)                                                                     \
    X(le_fuseau_et_le_decalage_viennent_de_la_maille)                                                                  \
    X(les_jours_commencent_a_minuit_local)                                                                             \
    X(les_pays_annonces_par_la_table_sont_ceux_des_captures)                                                           \
    X(le_cache_garde_les_valeurs_inconnues)                                                                            \
    X(le_contrat_de_l_ecran_recoit_nan_et_moins_un_pour_ce_que_la_source_ne_donne_pas)                                 \
    X(sans_pression_il_n_y_a_pas_de_tendance_meme_avec_un_historique)                                                  \
    X(une_maille_correcte_est_lue)                                                                                     \
    X(les_samoa_americaines_sans_maille_sont_un_refus)                                                                 \
    X(un_bureau_mal_forme_est_refuse)                                                                                  \
    X(une_maille_hors_plage_ou_mal_typee_est_refusee)                                                                  \
    X(un_fuseau_inconnu_ou_absent_dans_points_est_refuse)                                                              \
    X(points_qui_n_est_pas_du_json_ou_pas_un_objet_est_refuse)                                                         \
    X(une_grille_minimale_donne_le_courant_et_dit_pourquoi_le_reste_manque)                                            \
    X(le_courant_les_heures_et_les_jours_lisent_chaque_couche)                                                         \
    X(une_valeur_nulle_reste_inconnue_et_ne_vaut_jamais_zero)                                                          \
    X(le_minimum_et_le_maximum_du_jour_viennent_des_intervalles_qui_le_touchent)                                       \
    X(la_grille_doit_couvrir_l_instant_present)                                                                        \
    X(une_temperature_manquante_dans_les_24_heures_vide_les_heures_seulement)                                          \
    X(le_ciel_et_le_temps_sont_obligatoires_pour_le_courant)                                                           \
    X(chaque_phenomene_donne_son_code)                                                                                 \
    X(le_plus_fort_du_ciel_et_du_phenomene_l_emporte)                                                                  \
    X(les_seuils_du_ciel)                                                                                              \
    X(un_phenomene_ou_une_intensite_inconnus_font_refuser_la_reponse)                                                  \
    X(une_unite_inattendue_fait_refuser_la_reponse)                                                                    \
    X(des_intervalles_qui_se_chevauchent_ou_a_l_envers_font_refuser_la_reponse)                                        \
    X(un_intervalle_illisible_fait_refuser_la_reponse)                                                                 \
    X(la_grille_doit_etre_celle_de_la_maille_demandee)                                                                 \
    X(un_fuseau_inconnu_de_la_maille_fait_refuser_la_grille)                                                           \
    X(une_grille_sans_validTimes_ou_pas_du_json_est_refusee)                                                           \
    X(une_valeur_invraisemblable_fait_refuser_la_reponse)                                                              \
    X(le_cumul_d_une_heure_n_existe_que_pour_un_intervalle_d_exactement_cette_heure)                                   \
    X(le_cumul_d_un_jour_n_existe_que_si_des_intervalles_entiers_couvrent_ses_24_heures)                               \
    X(un_intervalle_qui_deborde_le_jour_ou_un_trou_rend_le_cumul_inconnu)                                              \
    X(la_neige_est_en_millimetres_dans_la_source_et_en_centimetres_dans_le_noyau)                                      \
    X(la_probabilite_d_une_heure_est_celle_d_un_intervalle_de_six_heures_au_plus)                                      \
    X(une_couche_plus_longue_que_le_tableau_est_tronquee_sans_deborder)

TESTS_A_LA_COMPILATION(LISTE)

int main() {
    static const Cas cas[] = {LISTE(CAS)};
    return lancer_tous(cas);
}
