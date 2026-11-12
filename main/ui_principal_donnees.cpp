// Écran principal : les données dérivées (soleil du lieu, moment du jour, alerte déduite, premières heure et jour).
#include "ui_principal_interne.h"

namespace ecran {
namespace principal {

// ---- données dérivées ---------------------------------------------------------------------------------------------------

namespace {

// Lever et coucher du soleil du jour LOCAL du lieu, calculés (NOAA, noyau pur). Le calcul coûte quelques dizaines de microsecondes et
// l'empreinte du ciel le demande à chaque passage : le dernier résultat est gardé tant que le jour local, le lieu et le décalage
// ne changent pas.
soleil::LeverCoucher lever_coucher_du_lieu(const ecran::heure::HeureLocale &local) {
    static struct {
        bool valide = false;
        std::int64_t numero_jour = 0;
        double latitude = 0;
        double longitude = 0;
        std::int32_t decalage_s = 0;
        soleil::LeverCoucher resultat;
    } cache;
    const std::int64_t numero = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());
    if (!cache.valide || cache.numero_jour != numero || cache.latitude != g.e.latitude_deg ||
        cache.longitude != g.e.longitude_deg || cache.decalage_s != local.decalage_s) {
        cache.resultat = soleil::lever_coucher(g.e.latitude_deg, g.e.longitude_deg, local.an, local.mois, local.jour,
                                               local.decalage_s);
        cache.numero_jour = numero;
        cache.latitude = g.e.latitude_deg;
        cache.longitude = g.e.longitude_deg;
        cache.decalage_s = local.decalage_s;
        cache.valide = true;
    }
    return cache.resultat;
}

// Jour, aube, crépuscule ou nuit SANS météo (démarrage, réseau absent). Avec la position du lieu : le lever et le coucher du soleil du
// jour local, calculés. Sans position : la plage fixe 8 h à 18 h de l'heure locale (limite connue, docs/mesures/). Sans fuseau
// connu on ne devine rien : nuit, comme quand l'heure n'est pas encore sûre.
MomentJour moment_sans_meteo() {
    if (g.e.maintenant_s <= 0 || !g.e.fuseau_connu) {
        return MomentJour::NUIT;
    }
    const ecran::heure::HeureLocale local = heure_du(g.e.maintenant_s);
    if (g.e.position_connue) {
        const soleil::LeverCoucher lc = lever_coucher_du_lieu(local);
        switch (lc.situation) {
        case soleil::Situation::Normal:
            return arc_soleil(g.e.maintenant_s, lc.lever_s, lc.coucher_s, true).moment;
        case soleil::Situation::JourPolaire:
            return MomentJour::JOUR;
        case soleil::Situation::NuitPolaire:
            return MomentJour::NUIT;
        }
    }
    return (local.heure >= 8 && local.heure < 18) ? MomentJour::JOUR : MomentJour::NUIT;
}
} // namespace

MomentJour moment_du_jour() {
    const VueMeteo &m = g.e.meteo;
    if (m.connue && m.lever_s > 0) {
        return arc_soleil(g.e.maintenant_s, m.lever_s, m.coucher_s, m.jour).moment;
    }
    if (m.connue) {
        return m.jour ? MomentJour::JOUR : MomentJour::NUIT;
    }
    return moment_sans_meteo();
}

Ambiance ambiance_du_moment() {
    const VueMeteo &m = g.e.meteo;
    return m.connue ? ambiance_pour_code(m.code, m.jour) : Ambiance::COUVERT;
}

Alerte alerte_du_jour() {
    const VueMeteo &m = g.e.meteo;
    if (!m.connue) {
        return Alerte::AUCUNE;
    }
    EntreesAlerte a;
    a.min_dix = m.min_dix;
    a.max_dix = m.max_dix;
    a.code_wmo = m.code;
    a.proba_pluie_pct = m.nb_jours > 0 ? m.jours[0].proba_pct : m.proba_pluie_pct;
    a.pluie_jour_dix_mm = m.nb_jours > 0 ? m.jours[0].pluie_dix_mm : INCONNU_DIX;
    a.rafales_dix_kmh = m.rafales_dix_kmh;
    return alerte_principale(a);
}

const char *texte_alerte(Alerte a) {
    switch (a) {
    case Alerte::GEL:
        return texte_principal::t("Gel");
    case Alerte::CHALEUR:
        return texte_principal::t("Forte chaleur");
    case Alerte::VENT_FORT:
        return texte_principal::t("Vent fort");
    case Alerte::PLUIE_FORTE:
        return texte_principal::t("Pluie forte");
    case Alerte::ORAGE:
        return texte_principal::t("Orage");
    case Alerte::AUCUNE:
        break;
    }
    return "";
}

Piece piece_alerte(Alerte a) {
    switch (a) {
    case Alerte::GEL:
        return Piece::gel;
    case Alerte::CHALEUR:
        return Piece::chaleur;
    case Alerte::VENT_FORT:
        return Piece::vent;
    case Alerte::PLUIE_FORTE:
        return Piece::parapluie;
    default:
        return Piece::alerte;
    }
}

// L'heure (indice dans `heures`) à partir de laquelle on montre les colonnes : la première dont l'heure n'est pas écoulée.
int premiere_heure() {
    const VueMeteo &m = g.e.meteo;
    for (int i = 0; i < m.nb_heures; ++i) {
        if (m.heures[i].instant_s + 3600 > g.e.maintenant_s) {
            return i;
        }
    }
    return m.nb_heures;
}

// Le jour LOCAL d'un jour de prévision (`date_s` est son minuit local, calculé par la météo avec UN décalage) : on lit son midi, qu'un
// changement d'heure d'une heure ne sort jamais du jour.
std::int64_t numero_du_jour_prevu(const JourPrevu &j) {
    return ecran::heure::numero_jour_local(j.date_s + 12 * 3600, fuseau());
}

// Le premier jour à montrer : on saute ceux qui sont finis (relevé gardé en cache depuis la veille). « Fini » se juge sur le jour
// civil du lieu, pas sur 86 400 s : un jour de changement d'heure dure 23 ou 25 heures.
int premier_jour() {
    const VueMeteo &m = g.e.meteo;
    const std::int64_t aujourdhui = ecran::heure::numero_jour_local(g.e.maintenant_s, fuseau());
    for (int i = 0; i < m.nb_jours; ++i) {
        if (numero_du_jour_prevu(m.jours[i]) >= aujourdhui) {
            return i;
        }
    }
    return m.nb_jours;
}

} // namespace principal
} // namespace ecran
