// Noyau pur : les MOTS de la page mer en sept langues (fr, en, de, nl, es, it, tr) et la mise en forme de ce que la page écrit (heures,
// créneau, valeur d'un critère, marée, vigilance, âge des données). C++17 standard, aucun include ESP-IDF ni LVGL, aucun flottant,
// évaluable à la compilation (test/host/test_mer_textes.cpp).
//
// Ce que ce fichier reprend : les phrases et les tables de vigie-display (firmware/core/vigie_text.cpp : activités, verdicts, critères, le
// vocabulaire des états du contrat, les phrases de vigilance et d'âge), traduites dans les sept langues de l'écran (ADR-0006). Le texte
// FRANÇAIS est la clé de la table commune (i18n/*.yaml, section « Écran : la page mer » ; ramenée là le 11/11/2026) ; repli sur
// l'anglais puis sur le français (i18n::tr). Un mot qui a plusieurs sens porte un contexte dans sa clé (« vent|faible » : light,
// « mer|faible » : low, « coefficient|faible » : small ; voir i18n.hpp). Les marqueurs sont NOMMÉS (« {heure} ») et non en %s : une
// traduction peut changer l'ordre des mots (« {heure} saatine kadar » en turc). Un marqueur inconnu reste tel quel dans le texte,
// donc visible. Ce fichier ne garde que l'ordre des clés (indexé par les énumérations) et la mise en forme.
//
// TRADUCTIONS ÉCRITES PAR UNE IA, NON RELUES par un locuteur natif (de, nl, es, it, tr surtout) ; le français et l'anglais viennent de
// vigie-display. Les formes courtes (« Std. », « dk ») sont celles que les sites d'information utilisent, pas vérifiées une à une.
//
// Les mots d'un verdict sont des règles de Vigie, pas des règles officielles, et pas pour la sécurité ; le coefficient est une estimation :
// la phrase qui le dit est `Texte::Regles`, le mot « estimé » est dans `Texte::Coefficient`.
//
// Chaque formateur écrit dans un tampon de l'appelant, rend le nombre d'octets écrits (zéro final exclu), et rend 0 avec un tampon VIDÉ
// quand il ne peut pas dire la chose (valeur ou unité inconnue, instant nul, tampon trop petit) : l'écran masque alors la ligne, il n'écrit
// ni « 0 » ni un texte coupé au milieu d'un caractère.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/i18n.hpp"
#include "ecran/mer_trame.hpp"
#include "ecran/vigie_textes.hpp"
#include "ecran/visuel_dates.hpp"

namespace ecran::mer {

using Langue = i18n::Langue;
using Mot = vigie::Mot;

// ---- les phrases fixes -----------------------------------------------------------------------------------------------------------
enum class Texte : std::uint8_t {
    TitreMer, // le titre quand le spot n'a pas de nom
    MeilleurCreneau,
    Aucun,
    Maree,
    PasDeMaree,
    Soleil,
    Lever,
    Coucher,
    Nuit,
    Legende,
    Regles,
    IndisponibleIci,
    PleineMer,
    BasseMer,
    Coefficient,          // {n}
    MerIndisponible,      // aucune donnée de mer
    MerAge,               // {age}
    DonneesGardees,       // un fournisseur a servi sa dernière réponse
    DonneesAnciennes,     // trop vieilles : les verdicts ne sont plus montrés
    VigilancesIllisibles, // les vigilances n'ont pas pu être lues : ce n'est PAS « rien à craindre »
    Vigilance,            // {niveau}
    VigilanceJusqua,      // {niveau} {heure}
    NiveauJaune,
    NiveauOrange,
    NiveauRouge,
    Note,      // {points} {sur}
    Detail,    // {activite}
    PourHeure, // {heure}
    ColCritere,
    ColValeur,
    ColNote,
    ColLecture,
    Raison, // {critere} {etat}
    AInstant,
    IlYAMin, // {n}
    IlYAH,   // {n}
    IlYAJ,   // {n}
    Force,   // {n}, l'échelle de Beaufort
    Fermer,
    Credit, // le crédit des données de mer (CC BY 4.0), en pied de page ; formes longues et URL dans THIRD_PARTY.md
    PasNavigation, // ADR-0018 : la marée et la houle sont des estimations, pas pour la navigation
    Nombre
};

inline constexpr Mot TEXTES[] = {
    "Mer",
    "Meilleur créneau",
    "aucun",
    "Marée",
    "Pas de marée connue pour ce lieu",
    "Soleil",
    "Lever",
    "Coucher",
    "nuit",
    "une barre = 1 heure · trait blanc = maintenant",
    "Règles de Vigie, pas pour la sécurité",
    "Indisponible ici",
    "Pleine mer",
    "Basse mer",
    "Coefficient {n} (estimé)",
    "Mer : données indisponibles",
    "Mer · {age}",
    "Certaines données de mer datent d'une lecture précédente",
    "Données de mer trop anciennes",
    "Vigilances côtières indisponibles",
    "Vigilance côtière {niveau}",
    "Vigilance côtière {niveau} · jusqu'à {heure}",
    "vigilance|jaune",
    "vigilance|orange",
    "vigilance|rouge",
    "Note {points} sur {sur}",
    "{activite} · détail",
    "pour {heure}",
    "Critère",
    "Valeur",
    "Note",
    "Lecture",
    "{critere} : {etat}",
    "à l'instant",
    "il y a {n} min",
    "il y a {n} h",
    "il y a {n} j",
    "Force {n}",
    "Fermer",
    // Les noms propres (Open-Meteo, Copernicus Marine, EOT20) et « CC BY 4.0 » ne se traduisent pas.
    "Données mer : Open-Meteo, Copernicus Marine, marée EOT20 (CC BY 4.0)",
    "Estimations, pas pour la navigation",
};
inline constexpr std::size_t NOMBRE_TEXTES = sizeof(TEXTES) / sizeof(TEXTES[0]);
static_assert(NOMBRE_TEXTES == static_cast<std::size_t>(Texte::Nombre), "une ligne de TEXTES par Texte");

// Le texte `t` dans `langue` (avec ses marqueurs). Vide pour une valeur hors énumération.
constexpr std::string_view texte(Texte t, Langue langue) noexcept {
    const std::size_t i = static_cast<std::size_t>(t);
    return i < NOMBRE_TEXTES ? vigie::traduction(TEXTES[i], langue) : std::string_view{};
}

// ---- activités, verdicts, critères ---------------------------------------------------------------------------------------------
inline constexpr Mot ACTIVITES[NB_ACTIVITES] = {
    "Surf",
    "Kite / wing",
    "Bateau",
    "Pêche à pied",
};

constexpr std::string_view nom_activite(Activite a, Langue langue) noexcept {
    const int i = static_cast<int>(a);
    return (i >= 0 && i < NB_ACTIVITES) ? vigie::traduction(ACTIVITES[i], langue) : std::string_view{};
}

// Indexé par `Verdict` ; « inconnu » n'a pas de mot (la page écrit un tiret).
inline constexpr Mot VERDICTS[4] = {
    "",
    "Bon",
    "Possible",
    "Déconseillé",
};

constexpr std::string_view nom_verdict(Verdict v, Langue langue) noexcept {
    const int i = static_cast<int>(v);
    return (i >= 0 && i < 4) ? vigie::traduction(VERDICTS[i], langue) : std::string_view{};
}

// Indexé par `Critere` (le même ordre que l'énumération).
inline constexpr Mot CRITERES[static_cast<int>(Critere::Nombre)] = {
    "",
    "Houle",
    "Période de houle",
    "Direction de la houle",
    "Vent",
    "Marée",
    "Écart des rafales",
    "Direction du vent",
    "Vent",
    "Rafales",
    "Mer",
    "Basse mer",
    "Coefficient (est.)",
    "Vigilance côtière",
};

constexpr std::string_view nom_critere(Critere c, Langue langue) noexcept {
    const int i = static_cast<int>(c);
    return (i >= 0 && i < static_cast<int>(Critere::Nombre)) ? vigie::traduction(CRITERES[i], langue)
                                                             : std::string_view{};
}

// ---- ce que fait une règle d'une valeur : le vocabulaire du contrat (vigie-api, docs/embedded-frame.md, table des critères) --------
// Une ligne par PAIRE (critère, état) : le même état n'a pas le même mot partout (« faible » est « light » pour le vent, « low » pour la
// mer, « small » pour le coefficient). Une paire que le contrat ne liste pas n'a pas de mot : la page n'écrit rien, elle ne devine pas.
struct TexteEtat {
    Critere critere;
    Etat etat;
    Mot mot;
};

inline constexpr TexteEtat ETATS[] = {
    {Critere::HauteurHoule, Etat::TropPetite, "trop petite"},
    {Critere::HauteurHoule, Etat::Petite, "petite"},
    {Critere::HauteurHoule, Etat::Bonne, "bonne taille"},
    {Critere::HauteurHoule, Etat::Grosse, "grosse"},
    {Critere::HauteurHoule, Etat::TropGrosse, "trop grosse"},
    {Critere::PeriodeHoule, Etat::Courte, "courte"},
    {Critere::PeriodeHoule, Etat::Moyenne, "moyenne"},
    {Critere::PeriodeHoule, Etat::Longue, "longue"},
    {Critere::DirectionHoule, Etat::DeFace, "face à la plage"},
    {Critere::DirectionHoule, Etat::Oblique, "de biais"},
    {Critere::DirectionHoule, Etat::HorsAxe, "hors axe"},
    {Critere::Vent, Etat::Faible, "vent|faible"},
    {Critere::Vent, Etat::Offshore, "de terre"},
    {Critere::Vent, Etat::Travers, "de travers"},
    {Critere::Vent, Etat::Onshore, "de mer"},
    {Critere::Vent, Etat::Forte, "vent|fort"},
    {Critere::Vent, Etat::DansLaPlage, "dans votre plage"},
    {Critere::Vent, Etat::UnPeuFaible, "un peu faible"},
    {Critere::Vent, Etat::UnPeuForte, "un peu fort"},
    {Critere::Vent, Etat::TropFaible, "trop faible"},
    {Critere::Vent, Etat::TropFort, "trop fort"},
    {Critere::Maree, Etat::BonMoment, "bon moment"},
    {Critere::Maree, Etat::MauvaisMoment, "mauvais moment"},
    {Critere::EcartRafales, Etat::Regulier, "régulier"},
    {Critere::EcartRafales, Etat::Rafaleux, "rafaleux"},
    {Critere::EcartRafales, Etat::TresRafaleux, "très rafaleux"},
    {Critere::DirectionVent, Etat::Onshore, "de mer"},
    {Critere::DirectionVent, Etat::Offshore, "de terre"},
    {Critere::DirectionVent, Etat::Travers, "de travers"},
    {Critere::ForceVent, Etat::JusquaModere, "modéré ou moins"},
    {Critere::ForceVent, Etat::Frais, "bon frais"},
    {Critere::ForceVent, Etat::FortOuPlus, "fort ou plus"},
    {Critere::ForceVent, Etat::JusquaFrais, "bon frais ou moins"},
    {Critere::ForceVent, Etat::Forte, "vent|fort"},
    {Critere::ForceVent, Etat::QuasiCoupDeVentOuPlus, "grand frais ou plus"},
    {Critere::ForceRafales, Etat::JusquaFrais, "bon frais ou moins"},
    {Critere::ForceRafales, Etat::Forte, "vent|fort"},
    {Critere::ForceRafales, Etat::CoupDeVentOuPlus, "coup de vent ou plus"},
    {Critere::HauteurMer, Etat::Basse, "mer|faible"},
    {Critere::HauteurMer, Etat::Modere, "modérée"},
    {Critere::HauteurMer, Etat::Haute, "forte"},
    {Critere::BasseMer, Etat::DansLaFenetre, "dans la fenêtre"},
    {Critere::BasseMer, Etat::HorsFenetre, "hors fenêtre"},
    {Critere::Coefficient, Etat::Petite, "coefficient|faible"},
    {Critere::Coefficient, Etat::Modere, "moyen"},
    {Critere::Coefficient, Etat::Grand, "coefficient|fort"},
    {Critere::Coefficient, Etat::TresGrand, "très fort"},
    {Critere::Vigilance, Etat::Orange, "vigilance|orange"},
    {Critere::Vigilance, Etat::Rouge, "vigilance|rouge"},
};
inline constexpr std::size_t NOMBRE_ETATS = sizeof(ETATS) / sizeof(ETATS[0]);

// Le mot que la règle de `critere` dit d'un `etat` ; vide pour une paire que le contrat ne liste pas.
constexpr std::string_view texte_etat(Critere critere, Etat etat, Langue langue) noexcept {
    for (const TexteEtat &t : ETATS)
        if (t.critere == critere && t.etat == etat)
            return vigie::traduction(t.mot, langue);
    return {};
}

// ---- briques de mise en forme ----------------------------------------------------------------------------------------------------
namespace detail {

using Plume = visuel::detail::Plume;

struct Param {
    std::string_view nom; // sans accolades
    std::string_view valeur;
};

// Écrit `modele` dans `p` en remplaçant chaque {nom} connu ; un marqueur inconnu est recopié tel quel (visible).
template <std::size_t N> constexpr void remplir(Plume &p, std::string_view modele, const Param (&params)[N]) noexcept {
    std::size_t i = 0;
    while (i < modele.size()) {
        if (modele[i] == '{') {
            const std::size_t fin = modele.find('}', i + 1);
            if (fin != std::string_view::npos) {
                const std::string_view nom = modele.substr(i + 1, fin - i - 1);
                bool trouve = false;
                for (std::size_t k = 0; k < N; ++k) {
                    if (params[k].nom == nom) {
                        p.texte(params[k].valeur);
                        trouve = true;
                        break;
                    }
                }
                if (trouve) {
                    i = fin + 1;
                    continue;
                }
            }
        }
        p.car(modele[i]);
        ++i;
    }
}

// Un nombre écrit en caractères, gardé dans sa propre case (un string_view dessus ne vit que tant que la case vit).
struct Nombre {
    char c[24] = {};
    std::size_t n = 0;
    constexpr std::string_view vue() const noexcept { return std::string_view(c, n); }
};

// Un entier signé : « -3 », « 12 ».
constexpr Nombre entier(std::int64_t v) noexcept {
    Nombre r;
    char chiffres[24] = {};
    int nb = 0;
    const bool negatif = v < 0;
    std::uint64_t u = negatif ? static_cast<std::uint64_t>(-(v + 1)) + 1u : static_cast<std::uint64_t>(v);
    do {
        chiffres[nb++] = static_cast<char>('0' + u % 10);
        u /= 10;
    } while (u > 0 && nb < 20);
    if (negatif)
        r.c[r.n++] = '-';
    while (nb > 0)
        r.c[r.n++] = chiffres[--nb];
    return r;
}

// Un nombre en DIXIÈMES avec une décimale : « 2,6 » (« 2.6 » en anglais). Le séparateur suit la langue.
constexpr Nombre decimal_d(std::int64_t dixiemes, Langue langue) noexcept {
    Nombre r;
    const bool negatif = dixiemes < 0;
    std::uint64_t u = negatif ? static_cast<std::uint64_t>(-(dixiemes + 1)) + 1u : static_cast<std::uint64_t>(dixiemes);
    const unsigned frac = static_cast<unsigned>(u % 10);
    const Nombre ent = entier(static_cast<std::int64_t>(u / 10));
    // « -0,3 » : le signe est celui du nombre, pas de la partie entière.
    if (negatif && u != 0)
        r.c[r.n++] = '-';
    for (std::size_t i = 0; i < ent.n; ++i)
        r.c[r.n++] = ent.c[i];
    r.c[r.n++] = langue == Langue::En ? '.' : ',';
    r.c[r.n++] = static_cast<char>('0' + frac);
    return r;
}

// Dixièmes -> entier le plus proche, moitié loin de zéro.
constexpr std::int64_t arrondi_dixiemes(std::int64_t d) noexcept { return d >= 0 ? (d + 5) / 10 : -((-d + 5) / 10); }

// Reste d'une division par 86400 qui ne dépasse jamais en négatif (heure locale avant 1970 ou décalage négatif).
constexpr std::int64_t seconde_du_jour(std::int64_t instant_s, std::int32_t decalage_s) noexcept {
    const std::int64_t t = instant_s + decalage_s;
    std::int64_t r = t % 86400;
    if (r < 0)
        r += 86400;
    return r;
}

} // namespace detail

// ---- heures ------------------------------------------------------------------------------------------------------------------------
// « 14:05 » à l'heure du lieu montré (`decalage_s` : l'UTC du lieu ; la trame n'a pas celui du spot). Faux sans instant (0 = inconnu).
constexpr std::size_t formater_heure(std::int64_t instant_s, std::int32_t decalage_s, char *sortie,
                                     std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (instant_s <= 0) {
        p.trop = true;
        return p.finir();
    }
    const std::int64_t s = detail::seconde_du_jour(instant_s, decalage_s);
    const int h = static_cast<int>(s / 3600);
    const int m = static_cast<int>(s / 60 % 60);
    p.car(static_cast<char>('0' + h / 10));
    p.car(static_cast<char>('0' + h % 10));
    p.car(':');
    p.car(static_cast<char>('0' + m / 10));
    p.car(static_cast<char>('0' + m % 10));
    return p.finir();
}

// « 14:00–16:00 » (tiret demi-cadratin). Faux sans créneau (début nul, fin pas après le début).
constexpr std::size_t formater_creneau(std::int64_t debut_s, std::int64_t fin_s, std::int32_t decalage_s, char *sortie,
                                       std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    char a[8] = {};
    char b[8] = {};
    if (debut_s <= 0 || fin_s <= debut_s || formater_heure(debut_s, decalage_s, a, sizeof a) == 0 ||
        formater_heure(fin_s, decalage_s, b, sizeof b) == 0) {
        p.trop = true;
        return p.finir();
    }
    p.texte(std::string_view(a, 5));
    p.texte("\xE2\x80\x93"); // –
    p.texte(std::string_view(b, 5));
    return p.finir();
}

// « à l'instant », « il y a 12 min », « il y a 3 h », « il y a 2 j ». Faux sans horloge ni instant. Une date dans le futur (horloge en
// retard) compte pour « à l'instant ».
constexpr std::size_t formater_age(std::int64_t maintenant_s, std::int64_t evenement_s, Langue langue, char *sortie,
                                   std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (maintenant_s <= 0 || evenement_s <= 0) {
        p.trop = true;
        return p.finir();
    }
    const std::int64_t delta = maintenant_s > evenement_s ? maintenant_s - evenement_s : 0;
    if (delta < 60) {
        p.texte(texte(Texte::AInstant, langue));
        return p.finir();
    }
    Texte modele = Texte::IlYAMin;
    std::int64_t valeur = delta / 60;
    if (delta >= 86400) {
        modele = Texte::IlYAJ;
        valeur = delta / 86400;
    } else if (delta >= 3600) {
        modele = Texte::IlYAH;
        valeur = delta / 3600;
    }
    const detail::Nombre n = detail::entier(valeur);
    const detail::Param params[] = {{"n", n.vue()}};
    detail::remplir(p, texte(modele, langue), params);
    return p.finir();
}

// ---- critères --------------------------------------------------------------------------------------------------------------------
// La valeur avec son unité : « 2,6 m », « 10 s », « 21° », « 19 km/h », « 11,9 kn », « Force 3 », « 102 min », « 91 ». Faux quand la valeur
// ou l'unité est inconnue (une unité que ce firmware ne connaît pas ne s'affiche pas).
constexpr std::size_t formater_valeur(const Critere_ligne &c, Langue langue, char *sortie,
                                      std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (c.valeur_d == INCONNU) {
        p.trop = true;
        return p.finir();
    }
    const std::int64_t entier = detail::arrondi_dixiemes(c.valeur_d);
    switch (c.unite) {
    case Unite::Metres:
        p.texte(detail::decimal_d(c.valeur_d, langue).vue());
        p.texte(" m");
        break;
    case Unite::Noeuds:
        p.texte(detail::decimal_d(c.valeur_d, langue).vue());
        p.texte(" kn");
        break;
    case Unite::Secondes:
        p.texte(detail::entier(entier).vue());
        p.texte(" s");
        break;
    case Unite::Degres:
        p.texte(detail::entier(entier).vue());
        p.texte("\xC2\xB0"); // °
        break;
    case Unite::KmH:
        p.texte(detail::entier(entier).vue());
        p.texte(" km/h");
        break;
    case Unite::Minutes:
        p.texte(detail::entier(entier).vue());
        p.texte(" min");
        break;
    case Unite::Aucune:
        p.texte(detail::entier(entier).vue());
        break;
    case Unite::Beaufort: {
        const detail::Nombre n = detail::entier(entier);
        const detail::Param params[] = {{"n", n.vue()}};
        detail::remplir(p, texte(Texte::Force, langue), params);
        break;
    }
    default: // Unite::Inconnue
        p.trop = true;
        break;
    }
    return p.finir();
}

// « 2/2 », les points qu'un critère a donnés (surf). Faux quand ils sont inconnus ou incohérents.
constexpr std::size_t formater_points(const Critere_ligne &c, char *sortie, std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (c.points < 0 || c.points_max <= 0 || c.points > c.points_max) {
        p.trop = true;
        return p.finir();
    }
    p.texte(detail::entier(c.points).vue());
    p.car('/');
    p.texte(detail::entier(c.points_max).vue());
    return p.finir();
}

// « Note 7 sur 8 ». Faux quand les points sont inconnus, négatifs ou au-delà du maximum.
constexpr std::size_t formater_note(int points, int sur, Langue langue, char *sortie, std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (points < 0 || sur <= 0 || points > sur) {
        p.trop = true;
        return p.finir();
    }
    const detail::Nombre a = detail::entier(points);
    const detail::Nombre b = detail::entier(sur);
    const detail::Param params[] = {{"points", a.vue()}, {"sur", b.vue()}};
    detail::remplir(p, texte(Texte::Note, langue), params);
    return p.finir();
}

// Pourquoi une activité n'est pas bonne : « Houle : trop petite », « Vigilance côtière : rouge ». Faux quand on ne sait pas (ni critère ni
// état connus du vocabulaire).
constexpr std::size_t formater_raison(const Prevision &a, Langue langue, char *sortie, std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    const std::string_view etat = texte_etat(a.raison_critere, a.raison_etat, langue);
    const std::string_view critere = nom_critere(a.raison_critere, langue);
    if (etat.empty() || critere.empty()) {
        p.trop = true;
        return p.finir();
    }
    const detail::Param params[] = {{"critere", critere}, {"etat", etat}};
    detail::remplir(p, texte(Texte::Raison, langue), params);
    return p.finir();
}

// « Activité · détail ». Faux pour une activité hors énumération.
constexpr std::size_t formater_titre_detail(Activite a, Langue langue, char *sortie, std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    const std::string_view nom = nom_activite(a, langue);
    if (nom.empty()) {
        p.trop = true;
        return p.finir();
    }
    const detail::Param params[] = {{"activite", nom}};
    detail::remplir(p, texte(Texte::Detail, langue), params);
    return p.finir();
}

// « pour 11:00 » : l'heure sur laquelle repose le verdict du détail. Faux sans instant.
constexpr std::size_t formater_heure_detail(std::int64_t instant_s, std::int32_t decalage_s, Langue langue,
                                            char *sortie, std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    char h[8] = {};
    if (formater_heure(instant_s, decalage_s, h, sizeof h) == 0) {
        p.trop = true;
        return p.finir();
    }
    const detail::Param params[] = {{"heure", std::string_view(h, 5)}};
    detail::remplir(p, texte(Texte::PourHeure, langue), params);
    return p.finir();
}

// ---- marée -----------------------------------------------------------------------------------------------------------------------
// « Pleine mer 14:12 · 1,9 m » (« Pleine mer 14:12 » quand la hauteur est inconnue). Faux sans instant.
constexpr std::size_t formater_maree(const Maree &m, std::int32_t decalage_s, Langue langue, char *sortie,
                                     std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    char h[8] = {};
    if (formater_heure(m.instant_s, decalage_s, h, sizeof h) == 0) {
        p.trop = true;
        return p.finir();
    }
    p.texte(texte(m.haute ? Texte::PleineMer : Texte::BasseMer, langue));
    p.car(' ');
    p.texte(std::string_view(h, 5));
    if (m.hauteur_cm != INCONNU) {
        p.texte(" \xC2\xB7 "); // ·
        // centimètres -> dixièmes de mètre, au plus proche
        const std::int64_t cm = m.hauteur_cm;
        const std::int64_t d = cm >= 0 ? (cm + 5) / 10 : -((-cm + 5) / 10);
        p.texte(detail::decimal_d(d, langue).vue());
        p.texte(" m");
    }
    return p.finir();
}

// « Coefficient 91 (estimé) » : le nombre est l'estimation de Vigie, pas celui du SHOM. Faux sans coefficient.
constexpr std::size_t formater_coefficient(std::uint16_t valeur, Langue langue, char *sortie,
                                           std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (valeur == 0) {
        p.trop = true;
        return p.finir();
    }
    const detail::Nombre n = detail::entier(valeur);
    const detail::Param params[] = {{"n", n.vue()}};
    detail::remplir(p, texte(Texte::Coefficient, langue), params);
    return p.finir();
}

// ---- vigilance côtière et âge des données ---------------------------------------------------------------------------------------
// La vigilance la plus forte : « Vigilance côtière orange · jusqu'à 14:00 ». Quand elles n'ont pas pu être lues : « Vigilances côtières
// indisponibles » (JAMAIS « rien à craindre »). Faux quand aucune vigilance ne tient (ou qu'elle est verte).
constexpr std::size_t formater_vigilance(const Mer &mer, std::int32_t decalage_s, Langue langue, char *sortie,
                                         std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    if (!mer.valide) {
        p.trop = true;
        return p.finir();
    }
    if (!mer.vigilances_lues) {
        p.texte(texte(Texte::VigilancesIllisibles, langue));
        return p.finir();
    }
    Texte niveau = Texte::Nombre;
    switch (mer.vigilance_niveau) {
    case vigie::Niveau::Jaune:
        niveau = Texte::NiveauJaune;
        break;
    case vigie::Niveau::Orange:
        niveau = Texte::NiveauOrange;
        break;
    case vigie::Niveau::Rouge:
        niveau = Texte::NiveauRouge;
        break;
    default: // verte ou inconnue : rien à dire
        break;
    }
    if (!mer.vigilance_presente || niveau == Texte::Nombre) {
        p.trop = true;
        return p.finir();
    }
    char h[8] = {};
    const std::string_view mot_niveau = texte(niveau, langue);
    if (formater_heure(mer.vigilance_fin_s, decalage_s, h, sizeof h) != 0) {
        const detail::Param params[] = {{"niveau", mot_niveau}, {"heure", std::string_view(h, 5)}};
        detail::remplir(p, texte(Texte::VigilanceJusqua, langue), params);
    } else {
        const detail::Param params[] = {{"niveau", mot_niveau}};
        detail::remplir(p, texte(Texte::Vigilance, langue), params);
    }
    return p.finir();
}

// La ligne qui dit l'âge de la mer : rien d'écrit pour des données fraîches. Faux quand il n'y a rien à dire.
constexpr std::size_t formater_etat(const Mer &mer, Fraicheur f, std::int64_t maintenant_s, Langue langue, char *sortie,
                                    std::size_t capacite) noexcept {
    detail::Plume p{sortie, capacite};
    switch (f) {
    case Fraicheur::Rien:
        p.texte(texte(Texte::MerIndisponible, langue));
        return p.finir();
    case Fraicheur::Expiree:
        p.texte(texte(Texte::DonneesAnciennes, langue));
        return p.finir();
    case Fraicheur::Vieillie: {
        // Un fournisseur a servi sa dernière réponse : on le dit, quel que soit l'âge de la trame.
        if (mer.donnees_gardees) {
            p.texte(texte(Texte::DonneesGardees, langue));
            return p.finir();
        }
        char age[40] = {};
        if (formater_age(maintenant_s, mer.genere_s, langue, age, sizeof age) == 0) {
            p.trop = true;
            return p.finir();
        }
        const detail::Param params[] = {{"age", std::string_view(age)}};
        detail::remplir(p, texte(Texte::MerAge, langue), params);
        return p.finir();
    }
    case Fraicheur::Fraiche:
        break;
    }
    p.trop = true; // fraîches : rien à dire
    return p.finir();
}

// Ce que dit le bandeau sous la barre de titre, s'il dit quelque chose.
enum class Bandeau : std::uint8_t {
    Rien = 0,
    Vigilance = 1,          // une vigilance côtière tient : son niveau est `mer.vigilance_niveau`
    VigilanceIllisible = 2, // les vigilances n'ont pas pu être lues : à remarquer, jamais un « tout va bien »
    Etat = 3,               // pas de mer, trop vieille, ou quelques minutes
};

// Le plus important à dire, dans `sortie`. Une mer absente ou trop vieille passe devant une vigilance qui n'est plus d'actualité ; sinon une
// vigilance (ou des vigilances illisibles) passe devant l'âge.
constexpr Bandeau composer_bandeau(const Mer &mer, Fraicheur f, std::int64_t maintenant_s, std::int32_t decalage_s,
                                   Langue langue, char *sortie, std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return Bandeau::Rien;
    sortie[0] = '\0';
    if (f == Fraicheur::Rien || f == Fraicheur::Expiree)
        return formater_etat(mer, f, maintenant_s, langue, sortie, capacite) != 0 ? Bandeau::Etat : Bandeau::Rien;
    if (formater_vigilance(mer, decalage_s, langue, sortie, capacite) != 0)
        return mer.vigilances_lues ? Bandeau::Vigilance : Bandeau::VigilanceIllisible;
    return formater_etat(mer, f, maintenant_s, langue, sortie, capacite) != 0 ? Bandeau::Etat : Bandeau::Rien;
}

} // namespace ecran::mer
