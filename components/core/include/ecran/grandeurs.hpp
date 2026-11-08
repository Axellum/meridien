// Noyau pur : la page « Détails » (main/ui_details.cpp), c'est-à-dire les SEPT GRANDEURS que l'accueil refait n'affiche plus : vent,
// pluie, humidité, pression, indice UV, soleil (lever, coucher, position) et lune. C++17 standard, aucun include ESP-IDF ni LVGL,
// aucune allocation, évaluable à la compilation (test/host/test_grandeurs.cpp, mutations_grandeurs.py). Le dessin ne fait que poser
// ce que ce fichier décide.
//
// Contenu :
//   1. les textes de la page (« Détails », « Fermer ») dans les sept langues, puis ceux de l'écran principal déjà traduits ;
//   2. la valeur de chaque grandeur : ce qui s'écrit en gros, son unité, ses lignes de détail, la flèche, la jauge, les segments ;
//   3. l'ÉTAT MANQUANT : une grandeur inconnue s'écrit « — », sans unité ni détail (jamais 0, jamais une valeur inventée) ;
//   4. la page entière (`composer`), son empreinte (pour ne redessiner que si quelque chose a changé) ;
//   5. la mise en page, en pixels de référence (1280 x 720 ; 1024 x 600 = 1280 x 750 réduit à 80 % ; 720 x 1280 en portrait).
//
// Les règles de valeur sont celles des sept tuiles de l'ancien accueil (main/ui_principal.cpp avant le commit ca84569, retirées par
// ADR-0016) : mêmes seuils (rafales : au moins 5 km/h au-dessus du vent ; tendance de pression : 1 hPa sur 3 h ; UV : échelle de l'OMS),
// mêmes mots. Ce qui change : une grandeur INCONNUE n'a plus de détail (« Sec 2 h » sous un « — » aurait été une valeur inventée), la
// tendance de la pression n'est dite que si la pression est connue, et le lever et le coucher du soleil ne sont montrés que s'ils
// sont ceux du jour LOCAL (une réponse d'hier n'est pas celle d'aujourd'hui).
//
// Une météo périmée n'est PAS une grandeur manquante : comme l'accueil (ADR-0016), la page garde la dernière valeur, la dessine avec
// le thème grisé et dit « Données de 07:52 · périmées » (`Page::mention`). La lune se calcule sur l'appareil : elle n'est jamais
// grisée. Seule une valeur que la source ne donne pas, ou que l'adaptateur a jugée trop vieille (pluie des quarts d'heure, tendance
// de la pression), est « — ».
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/accueil.hpp"
#include "ecran/heure_fr.hpp"
#include "ecran/heure_lieu.hpp"
#include "ecran/i18n.hpp"
#include "ecran/visuel_astres.hpp"
#include "ecran/visuel_echelles.hpp"
#include "ecran/visuel_textes.hpp"
#include "ecran/visuel_themes.hpp"
#include "ecran/visuel_vue.hpp"

namespace ecran::grandeurs {

using i18n::Langue;
using visuel::Metrique;
using visuel::VueMeteo;

// ================================================================================================================ 1. textes

// Les phrases propres à cette page. Les mots des grandeurs (« Vent », « Humidité », « Faible », « Il pleut », « En hausse »,
// « Pleine lune »...) sont déjà traduits dans ecran/visuel_textes.hpp : `texte()` les y trouve. À migrer dans i18n/*.yaml avec eux
// (ADR-0006, quand l'écran sera stabilisé). NON relus par un locuteur natif, comme le reste de la table.
inline constexpr visuel::Texte TEXTES[] = {
    {"Détails", {"Details", "Details", "Details", "Detalles", "Dettagli", "Ayrıntılar"}},
    {"Fermer", {"Close", "Schließen", "Sluiten", "Cerrar", "Chiudi", "Kapat"}},
};

inline constexpr std::size_t NOMBRE_TEXTES = sizeof(TEXTES) / sizeof(TEXTES[0]);

// Le texte dans la langue : cette table, puis celles de l'écran principal et la table commune (visuel::texte), enfin le français.
constexpr std::string_view texte(std::string_view francais, Langue langue) noexcept {
    for (const visuel::Texte &x : TEXTES) {
        if (x.fr == francais) {
            if (langue == Langue::Fr)
                return x.fr;
            const std::string_view propre = x.t[i18n::indice(langue) - 1];
            return propre.empty() ? x.t[0] : propre;
        }
    }
    return visuel::texte(francais, langue);
}

// ============================================================================================================ 2. les tuiles

inline constexpr int NB_TUILES = visuel::NB_METRIQUES; // sept : l'ordre est celui de visuel::Metrique

inline constexpr std::size_t CAP_VALEUR = 10;
inline constexpr std::size_t CAP_UNITE = 8;
inline constexpr std::size_t CAP_LIBELLE = 24;
inline constexpr std::size_t CAP_LIGNE = 40;
inline constexpr std::size_t CAP_MENTION = 72;

// Ce qu'écrit une tuile pour une grandeur inconnue.
inline constexpr std::string_view TIRET = "—";

// Écart de rafales qui vaut d'être dit : 5 km/h (en dixièmes) au-dessus du vent moyen. En dessous, c'est le même vent.
inline constexpr int SEUIL_RAFALES_DIX = 50;
// Le soleil : le lever et le coucher d'un autre jour que « aujourd'hui » ne sont pas montrés.

struct Tuile {
    Metrique grandeur = Metrique::VENT;
    bool connue = false; // la grandeur principale est connue ; faux : `valeur` est « — », sans unité ni détail
    bool grisee = false; // vient d'une météo périmée : dessinée avec le thème grisé
    char libelle[CAP_LIBELLE] = {}; // « Vent »
    char valeur[CAP_VALEUR] = {};   // « 14 » ; soleil : l'heure du lever ; lune : l'éclairement
    char unite[CAP_UNITE] = {};     // « km/h », « % », « hPa » ; vide sans valeur
    char valeur2[CAP_VALEUR] = {};  // soleil : l'heure du coucher ; vide ailleurs
    char ligne1[CAP_LIGNE] =
        {}; // premier détail : « SO », « dans 25 min », « En hausse », « Faible », « Lever », « Pleine lune »
    char ligne2[CAP_LIGNE] = {}; // second détail : « Rafales 26 », « Coucher » ; vide le plus souvent
    int fleche_deg = visuel::
        INCONNU; // vent : angle vers lequel pointe la flèche (0 = en haut, sens horaire) ; INCONNU : pas de flèche
    int jauge_pm = visuel::INCONNU; // humidité : remplissage du rail en millièmes (0 à 1000) ; INCONNU : rail vide
    int segments = 0;               // UV : segments allumés (0 à 5)
    visuel::CategorieUv categorie_uv = visuel::CategorieUv::INCONNU;        // UV : donne la couleur de la catégorie
    visuel::TendancePression tendance = visuel::TendancePression::INCONNUE; // pression
    int position_pm =
        visuel::INCONNU; // soleil : position sur l'arc, du lever (0) au coucher (1000) ; INCONNU : pas levé ou pas su
    int phase = visuel::INCONNU; // lune : indice de l'image, 0 à 7 ; INCONNU : pas d'heure sûre
};

namespace detail {

using accueil::detail::Tampon;

constexpr void ecrire(Tampon &t, std::string_view s) noexcept {
    for (const char c : s)
        t.car(c);
}

// Copie `s` dans `dest` ; trop long : `dest` reste vide (jamais un texte coupé au milieu d'un caractère).
template <std::size_t N> constexpr void copier(char (&dest)[N], std::string_view s) noexcept {
    Tampon t{dest, N};
    ecrire(t, s);
    t.terminer();
}

template <std::size_t N> constexpr void entier(char (&dest)[N], int v) noexcept {
    Tampon t{dest, N};
    t.entier(v);
    t.terminer();
}

// Écrit `modele` en remplaçant sa marque « %d » par `n` et sa marque « %s » par `mot`. Trop long : rien n'est écrit.
template <std::size_t N>
constexpr void remplir(char (&dest)[N], std::string_view modele, int n, std::string_view mot = {}) noexcept {
    Tampon t{dest, N};
    for (std::size_t i = 0; i < modele.size(); ++i) {
        if (modele[i] == '%' && i + 1 < modele.size() && modele[i + 1] == 'd') {
            t.entier(n);
            ++i;
        } else if (modele[i] == '%' && i + 1 < modele.size() && modele[i + 1] == 's') {
            ecrire(t, mot);
            ++i;
        } else {
            t.car(modele[i]);
        }
    }
    t.terminer();
}

// Une valeur en dixièmes (positive ou nulle) : « 0,4 », « 12 » (sans décimale quand elle est nulle). Le séparateur est celui de la langue.
constexpr void dixieme(Tampon &t, int dix, char separateur) noexcept {
    t.entier(dix / 10);
    if (dix % 10 != 0) {
        t.car(separateur);
        t.entier(dix % 10);
    }
}

constexpr char separateur_decimal(Langue langue) noexcept { return langue == Langue::En ? '.' : ','; }

// « 07:12 » de l'instant, à l'heure du lieu ; vide si l'instant est inconnu ou si la place manque.
template <std::size_t N>
constexpr void heure_du_lieu(char (&dest)[N], std::int64_t instant_s, const heure::Fuseau &fuseau) noexcept {
    dest[0] = '\0';
    if (instant_s > 0)
        heure::formater_hhmm(heure::en_heure_locale(instant_s, fuseau), dest, N);
}

// Une grandeur à son état de départ : son nom, « — » pour valeur, rien d'autre.
constexpr Tuile nouvelle(Metrique grandeur, std::string_view libelle_fr, Langue langue) noexcept {
    Tuile t{};
    t.grandeur = grandeur;
    copier(t.libelle, texte(libelle_fr, langue));
    copier(t.valeur, TIRET);
    return t;
}

constexpr bool pourcentage_valide(int pct) noexcept { return pct >= 0 && pct <= 100; }

// Les huit secteurs du vent (d'où il vient, dans l'ordre de visuel::secteur_vent) et les huit phases de la lune (dans l'ordre de
// visuel::PhaseLune) : des clés françaises de la table de traduction.
inline constexpr std::string_view SECTEURS[8] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
inline constexpr std::string_view NOMS_LUNE[visuel::LUNE_NB_PHASES] = {
    "Nouvelle lune", "Premier croissant",     "Premier quartier", "Gibbeuse croissante",
    "Pleine lune",   "Gibbeuse décroissante", "Dernier quartier", "Dernier croissant"};

} // namespace detail

// ---- vent ------------------------------------------------------------------------------------------------------------------

// Le vent moyen en km/h entier, la flèche (vers où il souffle), le secteur d'où il vient (« SO »), les rafales (« Rafales 26 ») quand
// elles valent d'être dites.
constexpr Tuile tuile_vent(const VueMeteo &m, Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::VENT, "Vent", langue);
    const bool vitesse = visuel::connu(m.vent_dix_kmh) && m.vent_dix_kmh >= 0;
    if (vitesse) {
        t.connue = true;
        detail::entier(t.valeur, visuel::arrondi_dix(m.vent_dix_kmh));
        detail::copier(t.unite, "km/h");
    }
    if (m.direction_deg >= 0 && m.direction_deg <= 360) {
        t.fleche_deg = visuel::angle_fleche_vent(m.direction_deg);
        detail::copier(t.ligne1, texte(detail::SECTEURS[visuel::secteur_vent(m.direction_deg)], langue));
    }
    if (vitesse && visuel::connu(m.rafales_dix_kmh) && m.rafales_dix_kmh > m.vent_dix_kmh + SEUIL_RAFALES_DIX) {
        detail::Tampon e{t.ligne2, CAP_LIGNE};
        detail::ecrire(e, texte("Rafales", langue));
        e.car(' ');
        e.entier(visuel::arrondi_dix(m.rafales_dix_kmh));
        e.terminer(); // pas d'unité : « km/h » est sur la tuile, juste au-dessus
    }
    return t;
}

// ---- pluie -----------------------------------------------------------------------------------------------------------------

// La probabilité de pluie en pour cent ; dessous, la pluie proche (« Il pleut », « dans 25 min », « Sec 2 h ») ou, faute de mieux, le cumul de l'heure.
constexpr Tuile tuile_pluie(const VueMeteo &m, Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::PLUIE, "Pluie", langue);
    if (detail::pourcentage_valide(m.proba_pluie_pct)) {
        t.connue = true;
        detail::entier(t.valeur, m.proba_pluie_pct);
        detail::copier(t.unite, "%");
    }
    switch (visuel::pluie_proche(m.pluie_dans_min)) {
    case visuel::PluieProche::EN_COURS:
        detail::copier(t.ligne1, texte("Il pleut", langue));
        break;
    case visuel::PluieProche::BIENTOT:
        detail::remplir(t.ligne1, texte("dans %d min", langue), m.pluie_dans_min);
        break;
    case visuel::PluieProche::AUCUNE:
        detail::copier(t.ligne1, texte("Sec 2 h", langue));
        break;
    case visuel::PluieProche::INCONNUE:
        if (visuel::connu(m.pluie_dix_mm) && m.pluie_dix_mm >= 0) {
            detail::Tampon e{t.ligne1, CAP_LIGNE};
            detail::dixieme(e, m.pluie_dix_mm, detail::separateur_decimal(langue));
            detail::ecrire(e, " mm");
            e.terminer();
        }
        break;
    }
    return t;
}

// ---- humidité --------------------------------------------------------------------------------------------------------------

constexpr Tuile tuile_humidite(const VueMeteo &m, Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::HUMIDITE, "Humidité", langue);
    if (detail::pourcentage_valide(m.humidite_pct)) {
        t.connue = true;
        detail::entier(t.valeur, m.humidite_pct);
        detail::copier(t.unite, "%");
        t.jauge_pm = m.humidite_pct * 10;
    }
    return t;
}

// ---- pression --------------------------------------------------------------------------------------------------------------

// La pression en hPa entier ; sa tendance sur trois heures (un mot) seulement si la pression est connue.
constexpr Tuile tuile_pression(const VueMeteo &m, Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::PRESSION, "Pression", langue);
    if (visuel::connu(m.pression_dix_hpa) && m.pression_dix_hpa > 0) {
        t.connue = true;
        detail::entier(t.valeur, visuel::arrondi_dix(m.pression_dix_hpa));
        detail::copier(t.unite, "hPa");
        t.tendance = visuel::tendance_pression(m.tendance_dix_hpa_3h);
        switch (t.tendance) {
        case visuel::TendancePression::HAUSSE:
            detail::copier(t.ligne1, texte("En hausse", langue));
            break;
        case visuel::TendancePression::BAISSE:
            detail::copier(t.ligne1, texte("En baisse", langue));
            break;
        case visuel::TendancePression::STABLE:
            detail::copier(t.ligne1, texte("Stable", langue));
            break;
        case visuel::TendancePression::INCONNUE:
            break;
        }
    }
    return t;
}

// ---- indice UV -------------------------------------------------------------------------------------------------------------

// L'indice entier, son niveau en mot (« Modéré »), cinq segments qui s'allument jusqu'au niveau.
constexpr Tuile tuile_uv(const VueMeteo &m, Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::UV, "UV", langue);
    const visuel::CategorieUv categorie = visuel::categorie_uv(m.uv_dix);
    if (categorie != visuel::CategorieUv::INCONNU) {
        t.connue = true;
        detail::entier(t.valeur, visuel::arrondi_dix(m.uv_dix));
        t.categorie_uv = categorie;
        t.segments = static_cast<int>(categorie);
        std::string_view mot = "Faible";
        switch (categorie) {
        case visuel::CategorieUv::MODERE:
            mot = "Modéré";
            break;
        case visuel::CategorieUv::ELEVE:
            mot = "Élevé";
            break;
        case visuel::CategorieUv::TRES_ELEVE:
            mot = "Très élevé";
            break;
        case visuel::CategorieUv::EXTREME:
            mot = "Extrême";
            break;
        case visuel::CategorieUv::FAIBLE:
        case visuel::CategorieUv::INCONNU:
            break;
        }
        detail::copier(t.ligne1, texte(mot, langue));
    }
    return t;
}

// ---- soleil ----------------------------------------------------------------------------------------------------------------

// Le lever, le coucher (heure du lieu) et la place du soleil sur son arc. Il faut un lever et un coucher dans l'ordre ; quand l'heure de
// l'appareil est sûre (`maintenant_s` > 0) ce doivent être ceux d'AUJOURD'HUI, au jour local du lieu : une réponse restée de la veille
// donnerait des heures fausses, on écrit alors « — ».
constexpr Tuile tuile_soleil(const VueMeteo &m, std::int64_t maintenant_s, const heure::Fuseau &fuseau,
                             Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::SOLEIL, "Soleil", langue);
    detail::copier(t.valeur2, TIRET);
    bool sur = m.lever_s > 0 && m.coucher_s > m.lever_s;
    if (sur && maintenant_s > 0) {
        sur = heure::numero_jour_local(m.lever_s, fuseau) == heure::numero_jour_local(maintenant_s, fuseau);
    }
    if (!sur)
        return t;
    t.connue = true;
    detail::heure_du_lieu(t.valeur, m.lever_s, fuseau);
    detail::heure_du_lieu(t.valeur2, m.coucher_s, fuseau);
    detail::copier(t.ligne1, texte("Lever", langue));
    detail::copier(t.ligne2, texte("Coucher", langue));
    if (maintenant_s > 0) {
        t.position_pm = visuel::arc_soleil(maintenant_s, m.lever_s, m.coucher_s, m.jour).position_pm;
    }
    return t;
}

// ---- lune ------------------------------------------------------------------------------------------------------------------

// La phase du jour, calculée sur l'appareil (elle ne dépend ni du réseau ni de la météo) : l'image, l'éclairement en pour cent, le nom.
// Sans heure sûre (`maintenant_s` <= 0), « — » : une phase calculée sur 1970 serait fausse.
constexpr Tuile tuile_lune(std::int64_t maintenant_s, Langue langue) noexcept {
    Tuile t = detail::nouvelle(Metrique::LUNE, "Lune", langue);
    if (maintenant_s <= 0)
        return t;
    const visuel::PhaseLune p = visuel::phase_lune(maintenant_s);
    t.connue = true;
    detail::entier(t.valeur, p.eclairement_pct);
    detail::copier(t.unite, "%");
    detail::copier(t.ligne1, texte(detail::NOMS_LUNE[p.indice], langue));
    t.phase = p.indice;
    return t;
}

// ================================================================================================================ 4. la page

// Le ciel du fond (thèmes à ciel dynamique), choisi comme sur l'accueil : le moment de la journée d'après le lever et le coucher de la
// météo (ou son drapeau « jour »), l'ambiance d'après le code du temps. SANS météo l'accueil calcule le jour d'après la position du lieu ;
// cette page n'a pas cette position : elle montre alors la nuit couverte (une page presque vide de tirets).
struct Ciel {
    visuel::MomentJour moment = visuel::MomentJour::NUIT;
    visuel::Ambiance ambiance = visuel::Ambiance::COUVERT;
};

constexpr Ciel ciel_de(const VueMeteo &m, std::int64_t maintenant_s) noexcept {
    Ciel c{};
    if (!m.connue)
        return c;
    if (m.lever_s > 0) {
        c.moment = visuel::arc_soleil(maintenant_s, m.lever_s, m.coucher_s, m.jour).moment;
    } else {
        c.moment = m.jour ? visuel::MomentJour::JOUR : visuel::MomentJour::NUIT;
    }
    c.ambiance = visuel::ambiance_pour_code(m.code, m.jour);
    return c;
}

struct Page {
    Tuile tuile[NB_TUILES] = {}; // dans l'ordre de visuel::Metrique : vent, pluie, humidité, pression, UV, soleil, lune
    bool meteo_connue = false;
    bool perimee = false;
    char titre[CAP_LIBELLE] = {};   // « Détails »
    char fermer[CAP_LIBELLE] = {};  // « Fermer »
    char mention[CAP_MENTION] = {}; // « Données de 07:52 · périmées », « Météo indisponible » ; vide quand tout va bien
    char exemple[CAP_LIBELLE] = {}; // « Exemple » quand les valeurs viennent du bouchon du contrat ; vide sinon
    char ville[visuel::VUE_VILLE_MAX] = {};
    Ciel ciel = {};
};

// La page pour une météo `m` à l'instant `maintenant_s` (UTC ; 0 : l'heure n'est pas sûre) dans le fuseau du lieu.
constexpr Page composer(const VueMeteo &m, std::int64_t maintenant_s, const heure::Fuseau &fuseau,
                        Langue langue) noexcept {
    Page p{};
    p.meteo_connue = m.connue;
    p.perimee = m.connue && m.perimee;
    p.ciel = ciel_de(m, maintenant_s);
    detail::copier(p.titre, texte("Détails", langue));
    detail::copier(p.fermer, texte("Fermer", langue));
    detail::copier(p.ville, std::string_view(m.ville));
    if (m.connue && m.exemple)
        detail::copier(p.exemple, texte("Exemple", langue));
    if (!m.connue) {
        detail::copier(p.mention, texte("Météo indisponible", langue));
    } else if (m.perimee) {
        char hhmm[CAP_VALEUR] = {};
        detail::heure_du_lieu(hhmm, m.instant_s, fuseau);
        if (hhmm[0] != '\0') {
            detail::remplir(p.mention, texte("Données de %s · périmées", langue), 0, std::string_view(hhmm));
        } else {
            detail::copier(p.mention, texte("Données périmées", langue));
        }
    }
    p.tuile[static_cast<int>(Metrique::VENT)] = tuile_vent(m, langue);
    p.tuile[static_cast<int>(Metrique::PLUIE)] = tuile_pluie(m, langue);
    p.tuile[static_cast<int>(Metrique::HUMIDITE)] = tuile_humidite(m, langue);
    p.tuile[static_cast<int>(Metrique::PRESSION)] = tuile_pression(m, langue);
    p.tuile[static_cast<int>(Metrique::UV)] = tuile_uv(m, langue);
    p.tuile[static_cast<int>(Metrique::SOLEIL)] = tuile_soleil(m, maintenant_s, fuseau, langue);
    p.tuile[static_cast<int>(Metrique::LUNE)] = tuile_lune(maintenant_s, langue);
    // Une météo périmée grise tout ce qui en vient ; la lune, calculée sur place, garde ses couleurs.
    for (int i = 0; i < NB_TUILES; ++i) {
        p.tuile[i].grisee = p.perimee && i != static_cast<int>(Metrique::LUNE);
    }
    return p;
}

namespace detail {

struct Empreinte {
    std::uint32_t h = 2166136261U;
    constexpr void u(std::uint32_t v) noexcept { h = (h ^ v) * 16777619U; }
    constexpr void i(int v) noexcept { u(static_cast<std::uint32_t>(v)); }
    template <std::size_t N> constexpr void s(const char (&t)[N]) noexcept {
        for (std::size_t k = 0; k < N && t[k] != '\0'; ++k)
            u(static_cast<unsigned char>(t[k]));
        u(0xFF);
    }
};

} // namespace detail

// Une empreinte de tout ce que la page dessine : elle ne change que si un texte, un nombre ou un état de la page change.
constexpr std::uint32_t empreinte(const Page &p) noexcept {
    detail::Empreinte e;
    e.u(p.meteo_connue);
    e.u(p.perimee);
    e.s(p.titre);
    e.s(p.fermer);
    e.s(p.mention);
    e.s(p.exemple);
    e.s(p.ville);
    e.u(static_cast<std::uint32_t>(p.ciel.moment));
    e.u(static_cast<std::uint32_t>(p.ciel.ambiance));
    for (const Tuile &t : p.tuile) {
        e.u(static_cast<std::uint32_t>(t.grandeur));
        e.u(t.connue);
        e.u(t.grisee);
        e.s(t.libelle);
        e.s(t.valeur);
        e.s(t.unite);
        e.s(t.valeur2);
        e.s(t.ligne1);
        e.s(t.ligne2);
        e.i(t.fleche_deg);
        e.i(t.jauge_pm);
        e.i(t.segments);
        e.u(static_cast<std::uint32_t>(t.categorie_uv));
        e.u(static_cast<std::uint32_t>(t.tendance));
        e.i(t.position_pm);
        e.i(t.phase);
    }
    return e.h;
}

// ============================================================================================================ 5. mise en page

using Rect = accueil::Rect;

// Tailles de police en pixels de RÉFÉRENCE (1280 x 720). Aucun texte de la page n'est plus petit que `TEXTE_MIN` pixels RÉELS, même sur la
// plus petite des deux cartes : la 7B (1024 x 600) réduit tout à l'échelle 0,8, donc 30 de référence font 24 réels. (Le plus petit texte
// de l'accueil refait n'est que de 24 de référence, 19 réels sur la 7B : cette page se lit de plus loin, à 2 m.)
inline constexpr int TEXTE_MIN = 24;
inline constexpr int ECHELLE_MIN_PM = 800; // échelle de la 7B en millièmes : 1024 / 1280
inline constexpr int POLICE_VALEUR = 76;   // la valeur de chaque grandeur
inline constexpr int POLICE_TITRE = 36;    // le titre de la page et « Fermer »
inline constexpr int POLICE_TEXTE = 30;    // libellés, unités, détails, mention

// Dans une tuile (pixels de référence).
inline constexpr int TUILE_MARGE = 20; // de chaque bord
inline constexpr int TUILE_ICONE = 48; // côté de l'icône de la grandeur
inline constexpr int TUILE_ESPACE = 8; // entre deux lignes
inline constexpr int RAYON_TUILE = 24; // arrondi du verre
inline constexpr int FERMER_LARGEUR =
    180; // « Fermer » : 180 x 80 de référence, soit 144 x 64 pixels réels sur la 7B (test : au moins 140 x 64)
inline constexpr int FERMER_HAUTEUR = 80;

struct Zones {
    Rect barre;  // le titre, le lieu, la mention d'état
    Rect fermer; // le bouton « Fermer »
    Rect tuile[NB_TUILES];
    int extra_h =
        0; // pixels de référence gagnés en hauteur au-delà de 720 (paysage) ou de 1280 (portrait) : ils vont aux tuiles
};

inline constexpr int ECART = 16;
inline constexpr int HAUT_BARRE = 24;
inline constexpr int GRILLE_DEPUIS = HAUT_BARRE + FERMER_HAUTEUR + 20; // 124 : le haut de la grille de tuiles

// Positions en pixels de RÉFÉRENCE : `w` x `h` est l'écran divisé par l'échelle (1280 x 720 sur le Tab5, 1280 x 750 sur la 7B de
// 1024 x 600, 720 x 1280 en portrait). Paysage : deux rangées, quatre tuiles puis UV, soleil (double largeur) et lune. Portrait : deux
// colonnes, trois rangées de deux (vent, pluie / humidité, pression / UV, lune) puis le soleil sur toute la largeur.
constexpr Zones disposer(int w, int h, bool portrait) noexcept {
    Zones z;
    const int marge = portrait ? 28 : 32;
    z.barre = {marge, HAUT_BARRE, w - 2 * marge - FERMER_LARGEUR - ECART, FERMER_HAUTEUR};
    z.fermer = {w - marge - FERMER_LARGEUR, HAUT_BARRE, FERMER_LARGEUR, FERMER_HAUTEUR};
    constexpr int V = static_cast<int>(Metrique::VENT);
    constexpr int P = static_cast<int>(Metrique::PLUIE);
    constexpr int H = static_cast<int>(Metrique::HUMIDITE);
    constexpr int R = static_cast<int>(Metrique::PRESSION);
    constexpr int U = static_cast<int>(Metrique::UV);
    constexpr int S = static_cast<int>(Metrique::SOLEIL);
    constexpr int L = static_cast<int>(Metrique::LUNE);
    const int bas = h - marge;
    if (!portrait) {
        const int rang_h = (bas - GRILLE_DEPUIS - ECART) / 2;
        const int col_w = (w - 2 * marge - 3 * ECART) / 4;
        const int y2 = GRILLE_DEPUIS + rang_h + ECART;
        const int haut2 = bas - y2; // la deuxième rangée prend le reste (l'arrondi, les pixels gagnés de la 7B)
        const int x[4] = {marge, marge + (col_w + ECART), marge + 2 * (col_w + ECART), marge + 3 * (col_w + ECART)};
        const int droite = w - marge;
        z.tuile[V] = {x[0], GRILLE_DEPUIS, col_w, rang_h};
        z.tuile[P] = {x[1], GRILLE_DEPUIS, col_w, rang_h};
        z.tuile[H] = {x[2], GRILLE_DEPUIS, col_w, rang_h};
        z.tuile[R] = {x[3], GRILLE_DEPUIS, droite - x[3], rang_h};
        z.tuile[U] = {x[0], y2, col_w, haut2};
        z.tuile[S] = {x[1], y2, x[3] - ECART - x[1], haut2};
        z.tuile[L] = {x[3], y2, droite - x[3], haut2};
        z.extra_h = h > 720 ? h - 720 : 0;
        return z;
    }
    const int rang_h = (bas - GRILLE_DEPUIS - 3 * ECART) / 4;
    const int col_w = (w - 2 * marge - ECART) / 2;
    const int x1 = marge + col_w + ECART;
    const int droite = w - marge;
    const int y[4] = {GRILLE_DEPUIS, GRILLE_DEPUIS + (rang_h + ECART), GRILLE_DEPUIS + 2 * (rang_h + ECART),
                      GRILLE_DEPUIS + 3 * (rang_h + ECART)};
    z.tuile[V] = {marge, y[0], col_w, rang_h};
    z.tuile[P] = {x1, y[0], droite - x1, rang_h};
    z.tuile[H] = {marge, y[1], col_w, rang_h};
    z.tuile[R] = {x1, y[1], droite - x1, rang_h};
    z.tuile[U] = {marge, y[2], col_w, rang_h};
    z.tuile[L] = {x1, y[2], droite - x1, rang_h};
    z.tuile[S] = {marge, y[3], droite - marge, bas - y[3]};
    z.extra_h = h > 1280 ? h - 1280 : 0;
    return z;
}

} // namespace ecran::grandeurs
