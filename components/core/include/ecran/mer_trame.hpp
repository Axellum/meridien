// Noyau pur : la MER d'un spot telle que la trame de Vigie la donne. C++17 standard, aucun include ESP-IDF ni LVGL, aucune allocation, aucun
// flottant, évaluable à la compilation (test/host/test_mer_trame.cpp).
//
// Lit les lignes `SEA`, `ACT` et `CRIT` que vigie-api ajoute à la trame embarquée quand on demande un spot
// (`GET /v1/embedded/frame?spot=<id>`, vigie_api/docs/embedded-frame.md, « Activities of a surf spot »). Elles répondent à « est-ce que j'y
// vais aujourd'hui ? » pour le surf, le kite (kite, wing, planche), le bateau et la pêche à pied : un verdict par heure sur 24 heures, le
// meilleur créneau, les critères derrière le verdict, la marée (pleines et basses mers, coefficients ESTIMÉS), le soleil et la vigilance
// côtière la plus forte.
//
// Règles tenues (elles viennent du contrat de la trame et de la page mer de vigie-display, dont ce fichier reprend la logique) :
//   - Un champ vide est INCONNU et reste inconnu : jamais 0, jamais une valeur plausible (`INCONNU` pour un nombre, `Verdict::Inconnu`…).
//   - Une trame SANS le bloc n'est pas « rien à craindre » : `mettre_a_jour` garde alors la mer précédente (que `fraicheur` vieillit).
//   - Des vigilances illisibles (`vigilance_status` autre que `ok`, ou niveau inconnu de ce firmware) ne sont JAMAIS « aucune vigilance » :
//     `vigilances_lues` reste faux.
//   - Un critère dont le nom est inconnu est ignoré (pas compté) ; une ligne de balise inconnue est sautée (le CRC la couvre quand même).
//   - Chaque verdict est une règle de Vigie, pas une règle officielle, et pas pour la sécurité ; le coefficient est une estimation, pas celui
//     du SHOM. Les mots qui le disent sont dans la page, pas ici.
//   - Les instants sont des secondes UNIX UTC. La trame n'a pas le décalage horaire du spot : l'écran utilise celui du lieu qu'il montre.
//
// Nombres : hauteurs de marée en CENTIMÈTRES (la trame donne 2 décimales), valeurs des critères et vents en DIXIÈMES (1 décimale), lus sur
// le texte sans passer par un flottant (arrondi à l'entier le plus proche, moitié loin de zéro, si la trame en donnait plus).
//
// Pas de limite de longueur de ligne ici (le contrat en promet 512) : la taille de la réponse est bornée par le client qui la reçoit.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/vigie_alertes.hpp"

namespace ecran::mer {

// ---- capacités -----------------------------------------------------------------------------------------------------------------
inline constexpr int HEURES = 24;    // heures des bandes d'une activité
inline constexpr int MAREES_MAX = 8; // comme l'API les plafonne
inline constexpr int COEFFICIENTS_MAX = 4;
inline constexpr int CRITERES_MAX = 6;             // le surf en a cinq
inline constexpr int CHAMPS_MAX = 32;              // champs d'une ligne (le décodeur d'écran de Vigie en lit autant)
inline constexpr std::size_t LIEU_CAP = 100;       // octets, zéro final compris : 32 caractères de 3 octets au plus
inline constexpr std::size_t SOURCE_CAP = 28;      // octets, zéro final compris : 24 caractères au plus
inline constexpr std::int32_t INCONNU = INT32_MIN; // nombre inconnu (champ vide ou illisible)

// ---- énumérations (dans l'ordre de la trame) -------------------------------------------------------------------------------------
enum class Activite : std::uint8_t { Surf = 0, Kite = 1, Bateau = 2, PecheAPied = 3, Nombre };
inline constexpr int NB_ACTIVITES = static_cast<int>(Activite::Nombre);

enum class Verdict : std::uint8_t {
    Inconnu = 0, // pas de verdict (champ vide, ou « - » dans une bande)
    Bon = 1,
    Possible = 2,
    Deconseille = 3, // « poor » dans la trame : l'écran dit « déconseillé »
};

enum class Lumiere : std::uint8_t { Inconnue = 0, Nuit = 1, Jour = 2 };

enum class Unite : std::uint8_t {
    Aucune = 0, // pas d'unité (le coefficient)
    Metres,
    Secondes,
    Degres,
    KmH,
    Noeuds,
    Beaufort,
    Minutes,
    Inconnue = 255, // une unité que ce firmware ne connaît pas : la valeur ne s'affiche pas
};

// Les critères du vocabulaire du contrat, plus la vigilance côtière (une raison seulement).
enum class Critere : std::uint8_t {
    Inconnu = 0,
    HauteurHoule,
    PeriodeHoule,
    DirectionHoule,
    Vent,
    Maree,
    EcartRafales,
    DirectionVent,
    ForceVent,
    ForceRafales,
    HauteurMer,
    BasseMer,
    Coefficient,
    Vigilance,
    Nombre
};

// Ce que la règle fait d'une valeur. Une seule liste pour tous les critères : quels états vont avec quel critère, c'est le tableau du
// contrat (et, plus tard, celui des textes de la page).
enum class Etat : std::uint8_t {
    Inconnu = 0,
    TropPetite,
    Petite,
    Bonne,
    Grosse,
    TropGrosse,
    Courte,
    Moyenne,
    Longue,
    DeFace,
    Oblique,
    HorsAxe,
    Faible,
    Offshore,
    Travers,
    Onshore,
    Forte,
    BonMoment,
    MauvaisMoment,
    DansLaPlage,
    UnPeuFaible,
    UnPeuForte,
    TropFaible,
    TropFort,
    Regulier,
    Rafaleux,
    TresRafaleux,
    JusquaModere,
    Frais,
    FortOuPlus,
    JusquaFrais,
    CoupDeVentOuPlus,
    QuasiCoupDeVentOuPlus,
    Basse,
    Modere,
    Haute,
    DansLaFenetre,
    HorsFenetre,
    Grand,
    TresGrand,
    Orange,
    Rouge, // la vigilance côtière
    Nombre
};

enum class Fraicheur : std::uint8_t {
    Rien = 0, // rien reçu
    Fraiche = 1,
    Vieillie = 2, // l'API dit qu'elle sert sa dernière réponse, ou les données ont quelques minutes : montrer leur âge
    Expiree = 3,  // trop ancienne, ou les 24 heures sont passées : ne pas montrer les verdicts
};

enum class MiseAJour : std::uint8_t {
    Remplacee = 0, // la mer a été remplacée
    SansBloc =
        1, // trame bonne sans le bloc (pas de spot demandé, ou la houle n'a pas pu être lue) : l'ancienne mer est gardée
    Refusee = 2, // pas une trame valide (CRC) ou une ligne `SEA` illisible
};

// ---- état ---------------------------------------------------------------------------------------------------------------------
struct Critere_ligne {
    Critere id = Critere::Inconnu;
    Etat etat = Etat::Inconnu;       // inconnu : la valeur ou ce à quoi on la compare est inconnu
    std::int32_t valeur_d = INCONNU; // dixièmes, dans l'unité ci-dessous
    Unite unite = Unite::Aucune;
    Verdict verdict = Verdict::Inconnu; // inconnu : le critère ne compte pour rien
    std::int8_t points = -1;            // surf seulement ; -1 inconnu
    std::int8_t points_max = -1;        // surf seulement ; -1 inconnu
};

struct Prevision {
    bool presente = false;              // une ligne ACT a été reçue pour cette activité
    bool disponible = false;            // faux : on ne peut pas la juger ici (pêche à pied sans modèle de marée)
    Verdict verdict = Verdict::Inconnu; // le meilleur des heures de jour des 24 heures
    // Pourquoi ce n'est pas bon : un critère et son état, ou la vigilance côtière (orange / rouge).
    Critere raison_critere = Critere::Inconnu;
    Etat raison_etat = Etat::Inconnu;
    std::int64_t creneau_debut_s = 0; // meilleur créneau ; 0 = aucun (déconseillé, ou inconnu)
    std::int64_t creneau_fin_s = 0;   // exclusif
    Verdict heures[HEURES] = {};      // depuis Mer::premiere_heure_s, une par heure
    Lumiere lumiere[HEURES] = {};
    std::int64_t detail_s = 0; // l'heure sur laquelle repose le verdict : les critères ci-dessous sont les siens
    std::int8_t points = -1;   // surf seulement : points de cette heure ; -1 inconnu
    std::int8_t sur = -1;      // surf seulement : le plus que les critères notables peuvent donner
    std::uint8_t nb_criteres = 0;
    Critere_ligne criteres[CRITERES_MAX] = {};
};

struct Maree {
    bool haute = false;
    std::int64_t instant_s = 0;
    std::int32_t hauteur_cm = INCONNU; // par rapport au niveau moyen de la mer du modèle
};

struct Coefficient {
    std::int64_t pleine_mer_s = 0; // la pleine mer de Brest à laquelle il appartient
    std::uint16_t valeur = 0;      // ESTIMÉ par Vigie, pas celui du SHOM
};

struct Mer {
    bool valide = false; // une ligne SEA a été lue
    char nom[LIEU_CAP] = {};
    char lieu[LIEU_CAP] = {}; // la commune la plus proche ; peut être vide
    std::int64_t genere_s = 0;
    std::int64_t premiere_heure_s = 0; // début de l'heure 0 des bandes ; 0 = aucune activité n'a d'heure
    bool donnees_gardees = false;      // un fournisseur n'a pas répondu : sa dernière réponse est servie
    std::int32_t vent_min_d = INCONNU; // la plage du kite, en dixièmes de nœud
    std::int32_t vent_max_d = INCONNU;
    std::int64_t lever_s = 0; // 0 inconnu ; le premier coucher à venir et le lever de ce jour
    std::int64_t coucher_s = 0;
    std::int64_t lever2_s = 0; // le jour d'après
    std::int64_t coucher2_s = 0;
    std::uint8_t nb_marees = 0; // 0 là où aucun modèle de marée n'est proche du spot
    Maree marees[MAREES_MAX] = {};
    std::uint8_t nb_coefficients = 0;
    Coefficient coefficients[COEFFICIENTS_MAX] = {};
    // Vigilances côtières officielles (en France, vague-submersion). Faux : elles n'ont pas pu être lues, ce qui n'est PAS « rien à craindre ».
    bool vigilances_lues = false;
    bool vigilance_presente = false; // une vigilance tient à un moment des 24 heures
    vigie::Niveau vigilance_niveau = vigie::Niveau::Inconnu;
    std::int64_t vigilance_fin_s = 0; // 0 : fin inconnue
    char vigilance_source[SOURCE_CAP] = {};
    Prevision activites[NB_ACTIVITES] = {};
};

// ---- noms du contrat -----------------------------------------------------------------------------------------------------------
namespace detail {

inline constexpr std::string_view NOMS_CRITERES[] = {
    "",           "swell_height", "swell_period",   "swell_direction", "wind",
    "tide",       "gust_gap",     "wind_direction", "wind_force",      "gust_force",
    "sea_height", "low_water",    "coefficient",    "vigilance",
};
static_assert(sizeof(NOMS_CRITERES) / sizeof(NOMS_CRITERES[0]) == static_cast<std::size_t>(Critere::Nombre),
              "un nom du contrat par critere");

inline constexpr std::string_view NOMS_ETATS[] = {
    "",
    "too_small",
    "small",
    "right",
    "big",
    "too_big",
    "short",
    "medium",
    "long",
    "facing",
    "oblique",
    "off_axis",
    "light",
    "offshore",
    "cross_shore",
    "onshore",
    "strong",
    "right_stage",
    "wrong_stage",
    "in_range",
    "a_bit_light",
    "a_bit_strong",
    "too_light",
    "too_strong",
    "steady",
    "gusty",
    "very_gusty",
    "up_to_moderate",
    "fresh",
    "strong_or_more",
    "up_to_fresh",
    "gale_or_more",
    "near_gale_or_more",
    "low",
    "moderate",
    "high",
    "in_window",
    "outside_window",
    "large",
    "very_large",
    "orange",
    "red",
};
static_assert(sizeof(NOMS_ETATS) / sizeof(NOMS_ETATS[0]) == static_cast<std::size_t>(Etat::Nombre),
              "un nom du contrat par etat");

inline constexpr std::string_view NOMS_ACTIVITES[] = {"surf", "kite", "boat", "shore_fishing"};
static_assert(sizeof(NOMS_ACTIVITES) / sizeof(NOMS_ACTIVITES[0]) == static_cast<std::size_t>(Activite::Nombre),
              "un nom du contrat par activite");

} // namespace detail

constexpr Activite activite_de_nom(std::string_view nom) noexcept {
    for (int i = 0; i < NB_ACTIVITES; ++i)
        if (nom == detail::NOMS_ACTIVITES[i])
            return static_cast<Activite>(i);
    return Activite::Nombre;
}

constexpr Verdict verdict_de_nom(std::string_view nom) noexcept {
    if (nom == "good")
        return Verdict::Bon;
    if (nom == "possible")
        return Verdict::Possible;
    if (nom == "poor")
        return Verdict::Deconseille;
    return Verdict::Inconnu;
}

constexpr Critere critere_de_nom(std::string_view nom) noexcept {
    if (nom.empty())
        return Critere::Inconnu;
    for (int i = 1; i < static_cast<int>(Critere::Nombre); ++i)
        if (nom == detail::NOMS_CRITERES[i])
            return static_cast<Critere>(i);
    return Critere::Inconnu;
}

constexpr Etat etat_de_nom(std::string_view nom) noexcept {
    if (nom.empty())
        return Etat::Inconnu;
    for (int i = 1; i < static_cast<int>(Etat::Nombre); ++i)
        if (nom == detail::NOMS_ETATS[i])
            return static_cast<Etat>(i);
    return Etat::Inconnu;
}

constexpr Unite unite_de_nom(std::string_view nom) noexcept {
    if (nom.empty())
        return Unite::Aucune;
    if (nom == "m")
        return Unite::Metres;
    if (nom == "s")
        return Unite::Secondes;
    if (nom == "deg")
        return Unite::Degres;
    if (nom == "km/h")
        return Unite::KmH;
    if (nom == "kn")
        return Unite::Noeuds;
    if (nom == "Beaufort")
        return Unite::Beaufort;
    if (nom == "min")
        return Unite::Minutes;
    return Unite::Inconnue;
}

// ---- CRC et lecture des champs -------------------------------------------------------------------------------------------------
namespace detail {

// CRC32 IEEE 802.3 (zlib, PNG) : polynôme 0xEDB88320 réfléchi, départ et sortie XOR 0xFFFFFFFF. Le contrat le calcule sur tous les octets
// AVANT la ligne `END|`.
constexpr std::uint32_t crc32(std::string_view s) noexcept {
    std::uint32_t c = 0xFFFFFFFFu;
    for (const char ch : s) {
        c ^= static_cast<std::uint8_t>(ch);
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);
    }
    return ~c;
}

constexpr int chiffre_hexa(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

// Une ligne `END|` suivie de 8 chiffres hexadécimaux doit exister et valoir le CRC32 de tout ce qui la précède ; ce qui suit est ignoré
// (comme le fait le décodeur d'écran de Vigie). DIFFÉRENCE voulue avec lui : sans ligne `END`, la trame est REFUSÉE (lui l'accepte « non
// signée » pour ses tests) : une réponse coupée en route n'a justement pas sa dernière ligne, et la prendre pour bonne montrerait des
// verdicts à moitié lus.
constexpr bool crc_valide(std::string_view t) noexcept {
    std::size_t pos = 0;
    while (pos < t.size()) {
        const std::string_view reste = t.substr(pos);
        if (reste.size() >= 4 && reste.substr(0, 4) == "END|") {
            if (reste.size() < 12)
                return false;
            std::uint32_t annonce = 0;
            for (std::size_t i = 4; i < 12; ++i) {
                const int v = chiffre_hexa(reste[i]);
                if (v < 0)
                    return false;
                annonce = (annonce << 4) | static_cast<std::uint32_t>(v);
            }
            if (reste.size() > 12 && reste[12] != '\r' && reste[12] != '\n' && reste[12] != ' ' && reste[12] != '\t')
                return false; // plus de 8 chiffres : ce n'est pas un CRC
            return crc32(t.substr(0, pos)) == annonce;
        }
        const std::size_t nl = reste.find('\n');
        if (nl == std::string_view::npos)
            return false; // pas de ligne END
        pos += nl + 1;
    }
    return false;
}

struct Champs {
    std::string_view v[CHAMPS_MAX] = {};
    int n = 0;
};

// Coupe sur `sep` en gardant les champs vides (« un champ vide = inconnu »). Au-delà de CHAMPS_MAX, le reste est laissé.
constexpr void decouper(std::string_view s, char sep, Champs &c) noexcept {
    c.n = 0;
    std::size_t debut = 0;
    for (;;) {
        if (c.n >= CHAMPS_MAX)
            return;
        const std::size_t p = s.find(sep, debut);
        if (p == std::string_view::npos) {
            c.v[c.n++] = s.substr(debut);
            return;
        }
        c.v[c.n++] = s.substr(debut, p - debut);
        debut = p + 1;
    }
}

// Chiffres seulement, au plus 4 294 967 295 (un instant UNIX sur 32 bits) ; vide ou autre chose : 0 (inconnu).
constexpr std::int64_t lire_instant(std::string_view t) noexcept {
    if (t.empty())
        return 0;
    std::int64_t v = 0;
    for (const char c : t) {
        if (c < '0' || c > '9')
            return 0;
        v = v * 10 + (c - '0');
        if (v > 4294967295LL)
            return 0;
    }
    return v;
}

// Nombre décimal « -1.66 » -> entier multiplié par 10^decimales, lu sur le texte (arrondi à l'entier le plus proche, moitié loin de zéro).
// Faux (sortie intacte) : vide, signe seul, autre caractère, plus de 6 chiffres avant la virgule.
constexpr bool lire_fixe(std::string_view t, int decimales, std::int32_t &sortie) noexcept {
    if (t.empty() || decimales < 0 || decimales > 3)
        return false;
    std::size_t i = 0;
    bool neg = false;
    if (t[0] == '-') {
        neg = true;
        i = 1;
    }
    std::int64_t entier = 0;
    int nb_entier = 0;
    for (; i < t.size() && t[i] != '.'; ++i) {
        if (t[i] < '0' || t[i] > '9')
            return false;
        entier = entier * 10 + (t[i] - '0');
        if (++nb_entier > 6)
            return false; // 999 999 x 10^3 tient dans 32 bits
    }
    if (nb_entier == 0)
        return false;
    std::int64_t frac = 0;
    int lus = 0;           // chiffres après la virgule
    bool arrondir = false; // le premier chiffre au-delà des décimales gardées vaut 5 ou plus
    if (i < t.size()) {    // t[i] == '.'
        ++i;
        if (i >= t.size())
            return false; // « 3. »
        for (; i < t.size(); ++i) {
            if (t[i] < '0' || t[i] > '9')
                return false;
            if (lus < decimales)
                frac = frac * 10 + (t[i] - '0');
            else if (lus == decimales)
                arrondir = t[i] >= '5';
            ++lus; // les chiffres suivants sont lus (pour refuser un texte cassé) mais ne comptent pas
        }
    }
    for (int k = lus; k < decimales; ++k)
        frac *= 10; // « 1.6 » avec 2 décimales vaut 160
    std::int64_t echelle = 1;
    for (int k = 0; k < decimales; ++k)
        echelle *= 10;
    std::int64_t v = entier * echelle + frac + (arrondir ? 1 : 0);
    if (neg)
        v = -v;
    sortie = static_cast<std::int32_t>(v);
    return true;
}

// Copie `src` dans `dst` sans couper un caractère UTF-8 en deux (un accent coupé s'afficherait en caractère cassé).
template <std::size_t N> constexpr void copier_texte(std::string_view src, char (&dst)[N]) noexcept {
    for (std::size_t i = 0; i < N; ++i)
        dst[i] = '\0';
    std::size_t n = src.size() < N - 1 ? src.size() : N - 1;
    if (n < src.size()) {
        while (n > 0 && (static_cast<std::uint8_t>(src[n]) & 0xC0u) == 0x80u)
            --n;
    }
    for (std::size_t i = 0; i < n; ++i)
        dst[i] = src[i];
}

// Points : 0 à 100, sinon -1 (inconnu).
constexpr std::int8_t lire_points(std::string_view t) noexcept {
    if (t.empty())
        return -1;
    const std::int64_t v = lire_instant(t);
    if (v > 100 || (v == 0 && t != "0"))
        return -1;
    return static_cast<std::int8_t>(v);
}

constexpr void lire_heures(std::string_view t, Verdict (&sortie)[HEURES]) noexcept {
    const bool entier = t.size() == static_cast<std::size_t>(HEURES);
    for (int i = 0; i < HEURES; ++i) {
        sortie[i] = Verdict::Inconnu;
        if (!entier)
            continue;
        if (t[i] == 'G')
            sortie[i] = Verdict::Bon;
        else if (t[i] == 'P')
            sortie[i] = Verdict::Possible;
        else if (t[i] == 'X')
            sortie[i] = Verdict::Deconseille;
    }
}

constexpr void lire_lumiere(std::string_view t, Lumiere (&sortie)[HEURES]) noexcept {
    const bool entier = t.size() == static_cast<std::size_t>(HEURES);
    for (int i = 0; i < HEURES; ++i) {
        sortie[i] = Lumiere::Inconnue;
        if (!entier)
            continue;
        if (t[i] == '1')
            sortie[i] = Lumiere::Jour;
        else if (t[i] == '0')
            sortie[i] = Lumiere::Nuit;
    }
}

// « H:1794149460:1.71,L:1794171060:-1.67 » -> pleines et basses mers (au plus MAREES_MAX). Un élément mal formé est sauté.
constexpr void lire_marees(std::string_view t, Mer &m) noexcept {
    m.nb_marees = 0;
    std::size_t debut = 0;
    while (!t.empty() && debut <= t.size() && m.nb_marees < MAREES_MAX) {
        const std::size_t virgule = t.find(',', debut);
        const std::string_view element =
            virgule == std::string_view::npos ? t.substr(debut) : t.substr(debut, virgule - debut);
        debut = virgule == std::string_view::npos ? t.size() + 1 : virgule + 1;
        Champs p;
        decouper(element, ':', p);
        if (p.n != 3 || (p.v[0] != "H" && p.v[0] != "L"))
            continue;
        const std::int64_t instant = lire_instant(p.v[1]);
        if (instant == 0)
            continue;
        Maree &e = m.marees[m.nb_marees++];
        e.haute = p.v[0] == "H";
        e.instant_s = instant;
        std::int32_t cm = INCONNU;
        if (lire_fixe(p.v[2], 2, cm))
            e.hauteur_cm = cm;
    }
}

// « 1794162580:97,1794207100:93 » -> coefficients estimés (au plus COEFFICIENTS_MAX). Valeur 1 à 200 ; autre chose : sauté.
constexpr void lire_coefficients(std::string_view t, Mer &m) noexcept {
    m.nb_coefficients = 0;
    std::size_t debut = 0;
    while (!t.empty() && debut <= t.size() && m.nb_coefficients < COEFFICIENTS_MAX) {
        const std::size_t virgule = t.find(',', debut);
        const std::string_view element =
            virgule == std::string_view::npos ? t.substr(debut) : t.substr(debut, virgule - debut);
        debut = virgule == std::string_view::npos ? t.size() + 1 : virgule + 1;
        Champs p;
        decouper(element, ':', p);
        if (p.n != 2)
            continue;
        const std::int64_t instant = lire_instant(p.v[0]);
        const std::int64_t valeur = lire_instant(p.v[1]);
        if (instant == 0 || valeur == 0 || valeur > 200)
            continue;
        Coefficient &c = m.coefficients[m.nb_coefficients++];
        c.pleine_mer_s = instant;
        c.valeur = static_cast<std::uint16_t>(valeur);
    }
}

// « swell_height.too_small », « vigilance.orange » -> critère et état ; un nom inconnu laisse l'inconnu.
constexpr void lire_raison(std::string_view t, Critere &critere, Etat &etat) noexcept {
    critere = Critere::Inconnu;
    etat = Etat::Inconnu;
    const std::size_t point = t.find('.');
    if (point == std::string_view::npos)
        return;
    const Critere c = critere_de_nom(t.substr(0, point));
    const Etat e = etat_de_nom(t.substr(point + 1));
    if (c == Critere::Inconnu || e == Etat::Inconnu)
        return;
    critere = c;
    etat = e;
}

// SEA|… : faux quand la ligne est inutilisable.
constexpr bool lire_ligne_sea(const Champs &f, Mer &m) noexcept {
    if (f.n < 18 || f.v[1].empty())
        return false;
    const std::int64_t genere = lire_instant(f.v[3]);
    if (genere == 0)
        return false;
    m.valide = true;
    copier_texte(f.v[1], m.nom);
    copier_texte(f.v[2], m.lieu);
    m.genere_s = genere;
    m.premiere_heure_s = lire_instant(f.v[4]);
    m.donnees_gardees = f.v[5] == "1";
    std::int32_t d = INCONNU;
    if (lire_fixe(f.v[6], 1, d))
        m.vent_min_d = d;
    d = INCONNU;
    if (lire_fixe(f.v[7], 1, d))
        m.vent_max_d = d;
    m.lever_s = lire_instant(f.v[8]);
    m.coucher_s = lire_instant(f.v[9]);
    m.lever2_s = lire_instant(f.v[10]);
    m.coucher2_s = lire_instant(f.v[11]);
    lire_marees(f.v[12], m);
    lire_coefficients(f.v[13], m);
    // Les vigilances : « ok » veut dire qu'elles ont été lues, et un niveau vide veut alors dire qu'aucune ne tient. Tout autre état, ou un
    // niveau que ce firmware ne connaît pas, veut dire « on n'a pas pu savoir » (jamais « rien à craindre »).
    m.vigilances_lues = f.v[14] == "ok";
    m.vigilance_presente = false;
    if (m.vigilances_lues && !f.v[15].empty()) {
        if (f.v[15] == "GREEN")
            m.vigilance_niveau = vigie::Niveau::Vert;
        else if (f.v[15] == "YELLOW")
            m.vigilance_niveau = vigie::Niveau::Jaune;
        else if (f.v[15] == "ORANGE")
            m.vigilance_niveau = vigie::Niveau::Orange;
        else if (f.v[15] == "RED")
            m.vigilance_niveau = vigie::Niveau::Rouge;
        else
            m.vigilances_lues = false;
        m.vigilance_presente = m.vigilances_lues;
    }
    if (m.vigilance_presente) {
        m.vigilance_fin_s = lire_instant(f.v[16]);
        copier_texte(f.v[17], m.vigilance_source);
    }
    return true;
}

// ACT|… : l'indice de l'activité, ou -1 quand la ligne est inutilisable.
constexpr int lire_ligne_act(const Champs &f, Mer &m) noexcept {
    if (f.n < 12)
        return -1;
    const Activite activite = activite_de_nom(f.v[1]);
    if (activite == Activite::Nombre)
        return -1;
    const int indice = static_cast<int>(activite);
    Prevision &a = m.activites[indice];
    a = Prevision{};
    a.presente = true;
    a.disponible = f.v[2] == "1";
    if (!a.disponible)
        return indice; // les champs 3 à 11 sont vides alors
    a.verdict = verdict_de_nom(f.v[3]);
    if (a.verdict != Verdict::Inconnu)
        lire_raison(f.v[4], a.raison_critere, a.raison_etat);
    a.creneau_debut_s = lire_instant(f.v[5]);
    a.creneau_fin_s = lire_instant(f.v[6]);
    if (a.creneau_debut_s == 0 || a.creneau_fin_s <= a.creneau_debut_s)
        a.creneau_debut_s = a.creneau_fin_s = 0;
    lire_heures(f.v[7], a.heures);
    lire_lumiere(f.v[8], a.lumiere);
    a.detail_s = lire_instant(f.v[9]);
    a.points = lire_points(f.v[10]);
    a.sur = lire_points(f.v[11]);
    return indice;
}

// CRIT|… de la ligne ACT `courant`.
constexpr void lire_ligne_crit(const Champs &f, Mer &m, int courant) noexcept {
    if (f.n < 9 || courant < 0)
        return;
    if (activite_de_nom(f.v[1]) != static_cast<Activite>(courant))
        return;
    Prevision &a = m.activites[courant];
    if (a.nb_criteres >= CRITERES_MAX)
        return;
    Critere_ligne c;
    c.id = critere_de_nom(f.v[2]);
    if (c.id == Critere::Inconnu)
        return; // un critère que ce firmware ne sait pas nommer n'est pas compté
    std::int32_t d = INCONNU;
    if (lire_fixe(f.v[3], 1, d))
        c.valeur_d = d;
    c.unite = unite_de_nom(f.v[4]);
    c.etat = etat_de_nom(f.v[5]);
    c.verdict = verdict_de_nom(f.v[6]);
    c.points = lire_points(f.v[7]);
    c.points_max = lire_points(f.v[8]);
    a.criteres[a.nb_criteres++] = c;
}

// Vrai quand une ligne de `t` commence par `balise` suivie de `|`.
constexpr bool a_une_ligne(std::string_view t, std::string_view balise) noexcept {
    std::size_t pos = 0;
    while (pos < t.size()) {
        const std::string_view reste = t.substr(pos);
        if (reste.size() > balise.size() && reste.substr(0, balise.size()) == balise && reste[balise.size()] == '|')
            return true;
        const std::size_t nl = reste.find('\n');
        if (nl == std::string_view::npos)
            return false;
        pos += nl + 1;
    }
    return false;
}

} // namespace detail

// ---- décodage --------------------------------------------------------------------------------------------------------------------
// Décode les lignes SEA / ACT / CRIT de `trame` dans `sortie` (remplacée). Faux, `sortie` intacte, quand le CRC est faux ou qu'aucune ligne SEA
// n'est utilisable. Une balise inconnue est sautée. Une ligne CRIT appartient à la ligne ACT juste au-dessus.
constexpr bool decoder(std::string_view trame, Mer &sortie) noexcept {
    if (trame.empty() || !detail::crc_valide(trame))
        return false;
    Mer resultat{};
    bool a_sea = false;
    int courant = -1; // l'activité de la dernière ligne ACT, pour les lignes CRIT qui suivent
    std::size_t pos = 0;
    while (pos < trame.size()) {
        const std::size_t nl = trame.find('\n', pos);
        std::string_view ligne = nl == std::string_view::npos ? trame.substr(pos) : trame.substr(pos, nl - pos);
        pos = nl == std::string_view::npos ? trame.size() : nl + 1;
        if (!ligne.empty() && ligne.back() == '\r')
            ligne.remove_suffix(1);
        if (ligne.empty())
            continue;
        detail::Champs f;
        detail::decouper(ligne, '|', f);
        if (f.n < 2)
            continue;
        if (f.v[0] == "END")
            break;
        if (f.v[0] == "SEA") {
            if (a_sea)
                continue; // une seule ligne SEA par trame : la première gagne
            a_sea = detail::lire_ligne_sea(f, resultat);
            if (!a_sea)
                resultat = Mer{};
            continue;
        }
        if (!a_sea)
            continue; // ACT et CRIT n'ont de sens qu'après leur ligne SEA
        if (f.v[0] == "ACT") {
            courant = detail::lire_ligne_act(f, resultat);
            continue;
        }
        if (f.v[0] == "CRIT")
            detail::lire_ligne_crit(f, resultat, courant);
    }
    if (!a_sea)
        return false;
    sortie = resultat;
    return true;
}

// Ce que le client appelle avec chaque bonne trame : ne remplace `mer` que si la trame porte le bloc.
constexpr MiseAJour mettre_a_jour(std::string_view trame, Mer &mer) noexcept {
    if (trame.empty() || !detail::crc_valide(trame))
        return MiseAJour::Refusee;
    if (!detail::a_une_ligne(trame, "SEA"))
        return MiseAJour::SansBloc;
    return decoder(trame, mer) ? MiseAJour::Remplacee : MiseAJour::Refusee;
}

// Un identifiant de spot est-il utilisable dans la requête ? Un « slug » : a à z, 0 à 9, « - » et « _ », 64 caractères au plus.
constexpr bool spot_valide(std::string_view spot) noexcept {
    if (spot.empty() || spot.size() > 64)
        return false;
    for (const char c : spot) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok)
            return false;
    }
    return true;
}

} // namespace ecran::mer
