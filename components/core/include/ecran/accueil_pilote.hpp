// Noyau pur : le PILOTE du mode automatique de l'accueil (ADR-0023). C'est lui que l'écran appelle, deux fois par seconde, à la place de
// `accueil::decider()` (accueil_auto.hpp, ADR-0015) : il range les VRAIES mesures de l'appareil dans les entrées de `decider()`, garde
// l'état de la décision d'un appel au suivant, le remet à zéro quand le lieu change, et rend ce que l'écran doit montrer (une
// `Presentation`). C++17 standard, aucun include ESP-IDF ni LVGL, aucune allocation, évaluable à la compilation (test_accueil_pilote.cpp,
// mutations_accueil_pilote.py). Rien dans `main/` ne décide : `ecran_principal.cpp` lit les mesures, appelle `avancer()`, passe le
// résultat à l'écran.
//
// Ce que ce fichier ajoute à ADR-0015 (et à valider par Axel, voir ADR-0023) :
//   1. les entrées qui n'existent pas encore côté appareil (activités, spots, vent d'une activité, données de mer, basse mer) sont
//      données « inconnues » : `decider()` ne bascule jamais sur elles. Le lever et le coucher du soleil, eux, sont calculés pour de
//      vrai (soleil.hpp, une fois par jour local et par lieu) ;
//   2. un profil que l'appareil ne sait pas dessiner (tout sauf Météo aujourd'hui, `PROFILS_DISPONIBLES`) n'est ni choisi ni épinglé :
//      épinglage inconnu = « pas d'épinglage », profil indisponible rendu par les règles = Météo ;
//   3. PAS DE REDESSIN SOUS LE DOIGT : ADR-0015 laisse à l'interface la charge de ne pas changer l'écran pendant un toucher (le bandeau
//      agrandi et la carte pluie ne sont gelés ni par le délai de 30 min ni par le toucher dans `decider()`). Ici, tant qu'un doigt est
//      posé, la présentation rendue est celle de l'appel d'avant, pendant `GEL_TOUCHER_MAX_S` au plus (un doigt oublié sur l'écran ne
//      cache pas une alerte). Le gel s'arrête tout de suite quand la personne change l'épinglage ou que le lieu change : ce sont ses
//      gestes, pas ceux des règles.
//   4. les phrases de la pastille : une clé française par `Raison` (la table des sept langues est `visuel_textes.hpp`).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/accueil_auto.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/soleil.hpp"
#include "ecran/vigie_alertes.hpp"

namespace ecran::accueil {

// ---- les constantes (propositions, réglables plus tard, jamais présentées comme officielles) ---------------------------------------

// Les profils que l'appareil sait DESSINER, un bit par `Profil` (bit 0 : Météo). Surf, Vent·Bateau et Côte viendront avec les lots 4 à 6
// de l'accueil : tant qu'ils n'ont pas de dessin, les règles ne doivent pas les choisir et la roue ne doit pas les offrir.
inline constexpr std::uint8_t PROFILS_DISPONIBLES = 1U << static_cast<unsigned>(Profil::Meteo);

// Plus longtemps qu'un doigt posé, la présentation ne reste pas figée (un doigt oublié, un événement de relâchement perdu).
inline constexpr std::int64_t GEL_TOUCHER_MAX_S = 10;

// ---- les entrées ---------------------------------------------------------------------------------------------------------------

constexpr bool profil_disponible(Profil p) noexcept {
    return profil_valide(p) && (PROFILS_DISPONIBLES & (1U << static_cast<unsigned>(p))) != 0;
}

// Ce que l'appareil sait à cet instant. Tout est déjà lu : rien ici ne touche l'horloge ni la mémoire.
struct Mesures {
    // -- le temps et le lieu --
    std::int64_t maintenant_s = 0; // instant UNIX (UTC) ; 0 ou moins : heure inconnue
    heure::Fuseau fuseau{};        // les règles du fuseau du lieu (heure_fr.hpp)
    bool fuseau_connu = false;
    bool position_connue = false;
    double latitude_deg = 0;
    double longitude_deg = 0;
    std::uint32_t lieu =
        0; // clé du lieu choisi (`cle_du_lieu`) ; quand elle change, l'état de la décision est remis à zéro

    // -- la météo (le contrat de visuel::VueMeteo) --
    bool meteo_connue = false;
    bool meteo_perimee = false;
    int pluie_dans_min =
        -2; // 0 : il pleut ; > 0 : minutes avant la pluie ; -1 : rien dans 2 h ; -2 : inconnu (ou quarts d'heure périmés)

    // -- l'alerte officielle (vigie::Bandeau) : le plus grave des alertes en vigueur, déjà filtré par le niveau minimal réglé --
    vigie::Niveau niveau_alerte = vigie::Niveau::Inconnu;

    // -- l'écran et la personne --
    bool toucher = false;                  // un doigt est posé sur l'écran principal
    bool epingle = false;                  // un profil est épinglé
    Profil profil_epingle = Profil::Meteo; // lequel
};

// Une clé qui change quand le lieu choisi change (le nom ou la position, arrondie à 1e-4 degré, soit une dizaine de mètres). 0 est
// réservé à « pas de lieu » : une clé calculée n'est jamais 0.
namespace detail {
constexpr std::uint32_t cle_non_nulle(std::uint32_t h) noexcept { return h == 0 ? 1U : h; }
} // namespace detail

constexpr std::uint32_t cle_du_lieu(const char *nom, double latitude_deg, double longitude_deg) noexcept {
    std::uint32_t h = 2166136261U;
    const auto mele_octet = [&h](std::uint32_t o) { h = (h ^ (o & 0xFFU)) * 16777619U; };
    const auto mele_entier = [&mele_octet](std::int64_t v) {
        const std::uint64_t u = static_cast<std::uint64_t>(v);
        for (int i = 0; i < 8; ++i)
            mele_octet(static_cast<std::uint32_t>(u >> (8 * i)));
    };
    for (const char *c = nom; c != nullptr && *c != '\0'; ++c)
        mele_octet(static_cast<unsigned char>(*c));
    const auto dix_milliemes = [](double v) {
        const double x = v * 10000.0;
        return static_cast<std::int64_t>(x < 0 ? x - 0.5 : x + 0.5);
    };
    mele_entier(dix_milliemes(latitude_deg));
    mele_entier(dix_milliemes(longitude_deg));
    return detail::cle_non_nulle(h);
}

// ---- ce que l'écran montre -----------------------------------------------------------------------------------------------------

struct Presentation {
    Profil profil = Profil::Meteo;         // le profil de la zone du bas (seul Météo se dessine aujourd'hui)
    Raison raison = Raison::Defaut;        // la pastille
    Raison raison_profil = Raison::Defaut; // la raison du seul profil (le futur panneau « Pourquoi cet écran ? »)
    bool epingle = false;                  // un profil est épinglé : la pastille le dit, avec « Auto en pause »
    bool bandeau_agrandi = false;          // règle 1 : une alerte orange ou rouge en vigueur
    bool carte_pluie = false;              // règle 2 : la pluie arrive dans moins de 60 min, hors épinglage
};

constexpr bool operator==(const Presentation &a, const Presentation &b) noexcept {
    return a.profil == b.profil && a.raison == b.raison && a.raison_profil == b.raison_profil &&
           a.epingle == b.epingle && a.bandeau_agrandi == b.bandeau_agrandi && a.carte_pluie == b.carte_pluie;
}
constexpr bool operator!=(const Presentation &a, const Presentation &b) noexcept { return !(a == b); }

// ---- les phrases de la pastille --------------------------------------------------------------------------------------------------

// La clé FRANÇAISE de la phrase de la pastille (visuel_textes.hpp : PHRASES, sept langues). Une phrase courte par `Raison` : elle tient
// dans une pastille de 40 px de haut, sur les deux cartes, dans les sept langues (contrôlé sur les rendus du PC). Un épinglage ne dit
// que le profil de l'appareil : « Météo · épinglé » ; un profil que l'appareil n'a pas ne peut pas être épinglé (`profil_disponible`),
// mais la fonction reste totale et rend alors le mot seul.
constexpr const char *phrase_pastille(Raison raison, Profil profil) noexcept {
    switch (raison) {
    case Raison::Defaut:
        return "Auto \xC2\xB7 M\xC3\xA9t\xC3\xA9o";
    case Raison::AlerteOrange:
        return "Auto \xC2\xB7 alerte orange";
    case Raison::AlerteRouge:
        return "Auto \xC2\xB7 alerte rouge";
    case Raison::PluieProche:
        return "Auto \xC2\xB7 pluie proche";
    case Raison::Surf:
        return "Auto \xC2\xB7 Surf";
    case Raison::VentBateau:
        return "Auto \xC2\xB7 Vent\xC2\xB7"
               "Bateau";
    case Raison::Cote:
        return "Auto \xC2\xB7 C\xC3\xB4te";
    case Raison::DonneesPerimees:
        return "Auto \xC2\xB7 donn\xC3\xA9"
               "es p\xC3\xA9rim\xC3\xA9"
               "es";
    case Raison::Epingle:
        return profil == Profil::Meteo ? "M\xC3\xA9t\xC3\xA9o \xC2\xB7 \xC3\xA9pingl\xC3\xA9" : "\xC3\x89pingl\xC3\xA9";
    case Raison::ChangementDiffere:
        return "Auto \xC2\xB7 changement diff\xC3\xA9r\xC3\xA9";
    }
    return "Auto \xC2\xB7 M\xC3\xA9t\xC3\xA9o"; // une `Raison` hors table (jamais rendue par decider()) se lit comme « par défaut »
}

// La ligne qui accompagne une pastille épinglée (« Auto en pause ») : la clé française.
inline constexpr const char *PHRASE_AUTO_EN_PAUSE = "Auto en pause";

// La teinte d'une pastille : l'alerte colore la pastille de son niveau, l'épinglage est la teinte d'accent (le choix fait à la main),
// la pluie proche prend la couleur de la pluie, le reste est neutre. Jamais une couleur : un RÔLE du thème, choisi par l'interface.
enum class Ton : std::uint8_t { Neutre, Pluie, Alerte, Epingle };

constexpr Ton ton_de(Raison raison) noexcept {
    switch (raison) {
    case Raison::AlerteOrange:
    case Raison::AlerteRouge:
        return Ton::Alerte;
    case Raison::PluieProche:
        return Ton::Pluie;
    case Raison::Epingle:
        return Ton::Epingle;
    default:
        return Ton::Neutre;
    }
}

// ---- le pilote -------------------------------------------------------------------------------------------------------------------

class Pilote {
  public:
    constexpr Pilote() noexcept = default;
    // Reprend un état de décision déjà connu (essais ; plus tard, un état gardé d'un démarrage à l'autre).
    constexpr explicit Pilote(const EtatAuto &etat) noexcept : etat_(etat) {}

    // Un pas : range les mesures dans les entrées de `decider()`, garde son état, rend ce que l'écran montre. À appeler à chaque
    // rafraîchissement de l'écran (deux fois par seconde) avec des mesures lues à l'instant.
    constexpr Presentation avancer(const Mesures &m) noexcept {
        const bool heure_sure = m.maintenant_s > 0;

        // Le lieu change : la décision précédente parlait d'un autre endroit (son dernier changement de profil ne compte plus).
        const bool lieu_change = lieu_connu_ && m.lieu != lieu_;
        if (lieu_change)
            etat_ = EtatAuto{};
        lieu_ = m.lieu;
        lieu_connu_ = true;

        // Un épinglage que l'appareil ne peut pas montrer vaut « pas d'épinglage » (NVS corrompue, profil de plus tard).
        const bool epingle = m.epingle && profil_disponible(m.profil_epingle);

        const Entrees e = entrees_de(m, epingle, heure_sure);
        const Decision d = decider(e);
        etat_ = d.etat;
        Presentation neuve = presentation_de(d, epingle);
        if (!profil_disponible(
                neuve.profil)) { // les règles ne devraient pas le choisir (entrées inconnues) : filet de sécurité
            neuve.profil = Profil::Meteo;
            neuve.raison_profil = Raison::Defaut;
            if (!neuve.epingle && !neuve.bandeau_agrandi && !neuve.carte_pluie)
                neuve.raison = Raison::Defaut;
            etat_.profil = Profil::Meteo;
        }

        // Pas de redessin sous le doigt.
        if (m.toucher && heure_sure) {
            if (toucher_depuis_s_ <= 0)
                toucher_depuis_s_ = m.maintenant_s;
        } else {
            toucher_depuis_s_ = 0;
        }
        const std::int64_t depuis = toucher_depuis_s_ > 0 ? m.maintenant_s - toucher_depuis_s_ : -1;
        const bool geler =
            derniere_valide_ && depuis >= 0 && depuis < GEL_TOUCHER_MAX_S && !lieu_change && epingle == epingle_vu_;
        gelee_ = geler;
        epingle_vu_ = epingle;
        if (!geler) {
            derniere_ = neuve;
            derniere_valide_ = true;
        }
        return derniere_;
    }

    // Pour les essais et le journal.
    constexpr const EtatAuto &etat() const noexcept { return etat_; }
    constexpr bool gelee() const noexcept { return gelee_; }
    constexpr const Presentation &derniere() const noexcept { return derniere_; }

    // Les entrées que le dernier appel a données à `decider()`, pour les essais : prouve ce qui est « inconnu » et ce qui est réel.
    constexpr const Entrees &dernieres_entrees() const noexcept { return entrees_; }

  private:
    constexpr Entrees entrees_de(const Mesures &m, bool epingle, bool heure_sure) noexcept {
        Entrees e;
        e.maintenant_s = m.maintenant_s;
        if (heure_sure && m.fuseau_connu) {
            const heure::HeureLocale local = heure::en_heure_locale(m.maintenant_s, m.fuseau);
            e.decalage_local_s = local.decalage_s;
            if (m.position_connue) {
                lever_coucher_du_jour(m, local);
                e.lever_s = soleil_.lever_s;
                e.coucher_s = soleil_.coucher_s;
            }
        }
        e.epingle = epingle;
        e.profil_epingle = m.profil_epingle;
        // Aucune activité, aucun spot, aucun seuil de vent : ni la mer ni le vent d'une activité n'existent côté appareil (lots 4 à 6).
        e.toucher_en_cours = m.toucher;
        e.etat = etat_;
        e.niveau_alerte = m.niveau_alerte;
        e.pluie_dans_min = m.pluie_dans_min >= 0 ? m.pluie_dans_min : VALEUR_INCONNUE;
        e.fraicheur_meteo =
            !m.meteo_connue ? Fraicheur::Inconnue : (m.meteo_perimee ? Fraicheur::Perimee : Fraicheur::Fraiche);
        entrees_ = e;
        return e;
    }

    // Le lever et le coucher du jour LOCAL, calculés une fois par jour, par lieu et par décalage.
    constexpr void lever_coucher_du_jour(const Mesures &m, const heure::HeureLocale &local) noexcept {
        if (soleil_valide_ && soleil_an_ == local.an && soleil_mois_ == local.mois && soleil_jour_ == local.jour &&
            soleil_decalage_s_ == local.decalage_s && soleil_lat_ == m.latitude_deg && soleil_lon_ == m.longitude_deg)
            return;
        const soleil::LeverCoucher lc =
            soleil::lever_coucher(m.latitude_deg, m.longitude_deg, local.an, local.mois, local.jour, local.decalage_s);
        soleil_ = {};
        if (lc.situation ==
            soleil::Situation::Normal) { // un jour ou une nuit polaire n'a ni lever ni coucher : « inconnu »
            soleil_.lever_s = lc.lever_s;
            soleil_.coucher_s = lc.coucher_s;
        }
        soleil_an_ = local.an;
        soleil_mois_ = local.mois;
        soleil_jour_ = local.jour;
        soleil_decalage_s_ = local.decalage_s;
        soleil_lat_ = m.latitude_deg;
        soleil_lon_ = m.longitude_deg;
        soleil_valide_ = true;
    }

    static constexpr Presentation presentation_de(const Decision &d, bool epingle) noexcept {
        Presentation p;
        p.profil = d.profil;
        p.raison = d.raison;
        p.raison_profil = d.raison_profil;
        p.epingle = epingle;
        p.bandeau_agrandi = d.bandeau_agrandi;
        p.carte_pluie = d.carte_pluie;
        return p;
    }

    EtatAuto etat_{};
    Entrees entrees_{};
    std::uint32_t lieu_ = 0;
    bool lieu_connu_ = false;
    Presentation derniere_{};
    bool derniere_valide_ = false;
    bool gelee_ = false;
    bool epingle_vu_ = false;
    std::int64_t toucher_depuis_s_ = 0;
    // le cache du lever et du coucher
    struct Soleil {
        std::int64_t lever_s = 0;
        std::int64_t coucher_s = 0;
    } soleil_{};
    bool soleil_valide_ = false;
    int soleil_an_ = 0;
    int soleil_mois_ = 0;
    int soleil_jour_ = 0;
    std::int32_t soleil_decalage_s_ = 0;
    double soleil_lat_ = 0;
    double soleil_lon_ = 0;
};

} // namespace ecran::accueil
