// Noyau pur : le MODE AUTOMATIQUE de l'accueil — quel profil montrer (Météo, Surf, Vent·Bateau, Côte), pourquoi, et deux ajouts
// d'écran (bandeau d'alerte agrandi, carte pluie avec compte à rebours). C++17 standard, aucun include ESP-IDF ni LVGL, aucune
// allocation, aucune exception, évaluable à la compilation. Voir ADR-0015.
//
// Ce que ce fichier est : UNE fonction, `decider()`, sans horloge ni état global. Le temps, les réglages, les données reçues et
// l'état de l'écran arrivent dans `Entrees` ; la réponse est une `Decision` (profil, RAISONS, deux drapeaux, état à rendre au
// prochain appel). Deux appels avec les mêmes entrées rendent la même décision.
// Ce qu'il n'est pas : aucun texte (les mots de la pastille « Auto · Surf » et du panneau « Pourquoi cet écran ? » viendront de la
// table des sept langues, une phrase par `Raison`), aucune couleur, aucun réseau, aucun réglage NVS. Il ne calcule ni la houle, ni
// la marée, ni le vent d'une activité : ces notions arrivent déjà calculées (booléens, minutes, dixièmes de km/h). Il calcule
// seulement le créneau de sortie, à partir du lever et du coucher du soleil (soleil.hpp).
//
// LES RÈGLES SONT UNE PROPOSITION : « règle de Vigie, réglable, à valider par Axel ». Aucun seuil officiel n'existe pour le surf, le
// kite, le bateau ou la pêche à pied ; tous les nombres ci-dessous sont des constantes nommées, au même endroit, et l'écran ne dit
// jamais « sûr ». Par priorité, de la plus forte à la plus faible :
//   1. alerte officielle ORANGE ou ROUGE en vigueur -> `bandeau_agrandi`, quel que soit le profil, l'épinglage, le délai ou un
//      toucher : une alerte n'est pas un changement de profil ;
//   2. pluie dans MOINS de PLUIE_PROCHE_MAX_MIN minutes (en cours comprise : 0) -> `carte_pluie`, sauf profil épinglé ; elle
//      recouvre la zone du bas mais ne change pas le profil, qui continue dessous ;
//   3. SURF : un spot de surf enregistré ET l'activité Surf cochée ET les données de mer (houle, marée) fraîches ET un créneau de
//      sortie (du lever du soleil à 10 h, ou de 16 h au coucher du soleil, heures locales) ;
//   4. VENT·BATEAU : l'activité cochée ET un vent STRICTEMENT au-dessus du seuil réglé (données météo fraîches) ;
//   5. CÔTE : un spot de pêche à pied enregistré ET une basse mer dans les BASSE_MER_MAX_MIN prochaines minutes (3 h, bornes
//      comprises ; données de mer fraîches) ;
//   6. sinon MÉTÉO.
// 1 et 2 s'ajoutent à l'écran ; 3 à 6 choisissent la zone du bas : le premier qui convient gagne.
//
// GARDE-FOUS :
//   - UN changement de profil au plus toutes les CHANGEMENT_ECART_MIN_S (30 min) : 29 min 59 s plus tard, rien ; 30 min, oui.
//     Un profil que les règles voudraient changer est GARDÉ, avec la raison `ChangementDiffere`. Le premier choix (aucun
//     changement noté) est libre ;
//   - jamais pendant un toucher en cours (`toucher_en_cours`) : le profil attend la fin du toucher ;
//   - donnée périmée : si les règles 3 à 5 auraient choisi un profil dont la donnée est PÉRIMÉE, retour à Météo avec la raison
//     `DonneesPerimees`, tout de suite (le délai de 30 min ne s'applique pas : montrer du périmé pour du frais est pire qu'un
//     va-et-vient ; le toucher, lui, attend). Une donnée INCONNUE (jamais reçue) n'est pas périmée : aucune bascule, raison
//     `Defaut` ;
//   - un profil ÉPINGLÉ gagne sur 2 à 6 jusqu'à ce qu'on le désépingle (le bandeau agrandi, lui, reste) ; le désépinglage reprend
//     les règles tout de suite, sans attendre les 30 min ;
//   - une maison sans spot enregistré ne bascule JAMAIS vers Surf ni Côte (Vent·Bateau n'a pas besoin de spot : il lit le vent du
//     lieu de l'écran).
//
// ENTRÉES MANQUANTES : une valeur inconnue (niveau d'alerte `Inconnu`, minutes -1, fraîcheur `Inconnue`, lever ou coucher 0, heure
// 0, seuil de vent non réglé) ne déclenche JAMAIS une bascule ; un `Entrees{}` vide donne Météo sans aucun drapeau. Une valeur
// hors de sa table (un `Profil` de 9) est ignorée : un épinglage invalide vaut « pas d'épinglage », un état invalide vaut Météo.
//
// CE QUE LES DEUX DRAPEAUX ET LE TOUCHER NE FONT PAS : le bandeau agrandi et la carte pluie sont des AJOUTS d'écran, pas des
// changements de profil : ni la limite de 30 min ni le gel pendant un toucher ne s'y appliquent. C'est à l'interface de ne pas
// redessiner sous le doigt (non décidé ici : voir ADR-0015, questions ouvertes).
//
// LIMITES ÉCRITES :
//   - « aube » = LEVER du soleil (soleil.hpp ne calcule pas le crépuscule) : une sortie de pointe du jour commence souvent avant ;
//   - le créneau n'existe que si le lever ET le coucher donnés tombent dans le jour local de `maintenant_s` ; en deçà de ~60° de
//     latitude et hors jour/nuit polaire seulement. Un lever après 10 h ou un coucher avant 16 h (hiver très au nord) vide la
//     fenêtre concernée ; un coucher après minuit local n'est pas du « jour » : pas de créneau du soir ;
//   - 10 h et 16 h sont lues avec le décalage local EN VIGUEUR à `maintenant_s` : sur le jour d'un changement d'heure, avant le
//     changement (2 h ou 3 h du matin), la lecture est décalée d'une heure ; aucun créneau n'existe à cette heure-là ;
//   - Côte ne regarde que la PROCHAINE basse mer : une basse mer passée depuis cinq minutes ne compte plus (la pêche à pied dure
//     pourtant des deux côtés de l'étale). Proposition à valider, pas un oubli ;
//   - la carte pluie ne connaît pas d'hystérésis : une prévision qui oscille entre 59 et 61 minutes fait apparaître et disparaître
//     la carte.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecran/vigie_alertes.hpp"

namespace ecran::accueil {

// ---- les seuils (toutes les constantes de la décision, au même endroit) -------------------------------------------------------

// Règle 1 : niveau minimal d'une alerte officielle pour agrandir le bandeau. Le niveau reçu est le plus grave des alertes EN
// VIGUEUR et déjà filtrées par le réglage « niveau minimal » de la personne (vigie_vue.hpp : `Vue::pire`).
inline constexpr vigie::Niveau ALERTE_NIVEAU_MIN = vigie::Niveau::Orange;

// Règle 2 : la carte pluie apparaît quand la pluie commence dans MOINS de ce nombre de minutes (59 oui, 60 non).
inline constexpr int PLUIE_PROCHE_MAX_MIN = 60;

// Règle 3 : le créneau de sortie, en secondes depuis minuit LOCAL. Matin : du lever du soleil jusqu'à 10 h (exclu). Soir : de 16 h
// (incluse) jusqu'au coucher du soleil (exclu).
inline constexpr std::int64_t CRENEAU_MATIN_FIN_S = 10 * 3600;
inline constexpr std::int64_t CRENEAU_SOIR_DEBUT_S = 16 * 3600;

// Règle 5 : la basse mer est « proche » jusqu'à ce nombre de minutes (180 : 3 h pile compte, 181 non).
inline constexpr int BASSE_MER_MAX_MIN = 180;

// Garde-fou : l'écart minimal entre deux changements de profil (1 800 s ; 1 799 s : non, 1 800 s : oui).
inline constexpr std::int64_t CHANGEMENT_ECART_MIN_S = 30 * 60;

inline constexpr std::int64_t JOUR_S = 24 * 3600;

// Valeur de « je ne sais pas » pour les minutes (pluie, basse mer) et le vent : jamais 0, qui voudrait dire « maintenant » ou
// « calme ».
inline constexpr int VALEUR_INCONNUE = -1;

// ---- les types ----------------------------------------------------------------------------------------------------------------

// Les quatre profils de la zone du bas. Les valeurs seront celles enregistrées en NVS pour le profil épinglé : ne jamais les
// renuméroter.
enum class Profil : std::uint8_t { Meteo = 0, Surf = 1, VentBateau = 2, Cote = 3 };
inline constexpr std::size_t NOMBRE_PROFILS = 4;

// Fraîcheur d'une famille de données. `Inconnue` (jamais reçue, ou on ne sait pas) est la valeur par défaut : elle ne déclenche
// rien et ne s'appelle pas « périmée ».
enum class Fraicheur : std::uint8_t { Inconnue = 0, Fraiche = 1, Perimee = 2 };

// Pourquoi l'écran est ce qu'il est : une phrase par valeur dans la table des sept langues (plus tard). Les valeurs ne sont pas
// enregistrées ; l'ordre est celui des priorités.
enum class Raison : std::uint8_t {
    Defaut = 0,        // aucune règle ne demande autre chose : Météo
    AlerteOrange,      // règle 1, alerte orange en vigueur
    AlerteRouge,       // règle 1, alerte rouge en vigueur
    PluieProche,       // règle 2
    Surf,              // règle 3
    VentBateau,        // règle 4
    Cote,              // règle 5
    DonneesPerimees,   // 3 à 5 auraient choisi un profil, mais sa donnée est périmée : Météo
    Epingle,           // la personne a épinglé ce profil
    ChangementDiffere, // les règles voudraient un autre profil : gardé pour 30 min ou jusqu'à la fin du toucher
};
inline constexpr std::size_t NOMBRE_RAISONS = 10;

// Ce que la fonction rend à chaque appel pour qu'on le lui redonne au suivant (aucun état caché).
struct EtatAuto {
    Profil profil = Profil::Meteo; // le profil montré à l'appel précédent
    bool changement_connu = false; // `change_a_s` est valable ; faux au départ, et après un épinglage
    std::int64_t change_a_s = 0;   // instant UNIX (UTC) du dernier changement décidé par les règles
};

struct Entrees {
    // -- le temps : rien n'est lu ici, l'appelant le passe --
    std::int64_t maintenant_s = 0; // instant UNIX (UTC) ; 0 ou moins : heure inconnue (aucun changement, aucun créneau)
    std::int32_t decalage_local_s = 0; // décalage du lieu à l'UTC, positif à l'est (Paris l'hiver : 3600)
    std::int64_t lever_s = 0;          // lever du soleil du jour LOCAL de `maintenant_s` (soleil.hpp) ; 0 : inconnu
    std::int64_t coucher_s = 0;        // coucher du soleil du même jour ; 0 : inconnu

    // -- ce que la personne a réglé --
    bool epingle = false;                     // un profil est épinglé
    Profil profil_epingle = Profil::Meteo;    // lequel
    bool activite_surf = false;               // activité Surf cochée
    bool activite_vent_bateau = false;        // activité Vent·Bateau cochée
    bool spot_surf = false;                   // au moins un spot de surf enregistré
    bool spot_peche_a_pied = false;           // au moins un spot de pêche à pied enregistré
    std::int32_t seuil_vent_dixiemes_kmh = 0; // seuil de vent réglé (dixièmes de km/h) ; 0 ou moins : non réglé

    // -- l'écran --
    bool toucher_en_cours = false; // un doigt est posé
    EtatAuto etat{};               // celui que la décision précédente a rendu

    // -- les données reçues, déjà calculées ailleurs --
    vigie::Niveau niveau_alerte = vigie::Niveau::Inconnu; // le plus grave des alertes EN VIGUEUR
    int pluie_dans_min = VALEUR_INCONNUE;             // minutes avant la pluie (0 : en cours) ; -1 : aucune ou inconnue
    Fraicheur fraicheur_meteo = Fraicheur::Inconnue;  // prévisions (pluie proche, vent) : règles 2 et 4
    std::int32_t vent_dixiemes_kmh = VALEUR_INCONNUE; // le vent à comparer au seuil (dixièmes de km/h) ; -1 : inconnu
    Fraicheur fraicheur_mer = Fraicheur::Inconnue;    // houle et marée : règles 3 et 5
    int basse_mer_dans_min = VALEUR_INCONNUE;         // minutes avant la PROCHAINE basse mer ; -1 : inconnue
};

struct Decision {
    Profil profil = Profil::Meteo;  // le profil de la zone du bas (la carte pluie la recouvre mais ne le change pas)
    Raison raison = Raison::Defaut; // la pastille : l'alerte, sinon la pluie, sinon le profil (épinglé : lui)
    Raison raison_profil = Raison::Defaut; // le panneau « Pourquoi cet écran ? » : la raison du seul profil
    bool bandeau_agrandi = false;          // règle 1
    bool carte_pluie = false;              // règle 2
    EtatAuto etat{};                       // à redonner dans `Entrees::etat` au prochain appel
};

// ---- les règles, une par une (publiques pour être testées une par une) --------------------------------------------------------

constexpr bool profil_valide(Profil p) noexcept { return static_cast<std::size_t>(p) < NOMBRE_PROFILS; }

// Règle 1 : le niveau le plus grave des alertes en vigueur agrandit-il le bandeau ? (Inconnu, vert, jaune : non.)
constexpr bool alerte_agrandit_le_bandeau(vigie::Niveau n) noexcept { return n >= ALERTE_NIVEAU_MIN; }

// Règle 2 : la carte pluie, hors épinglage. Des prévisions fraîches ET une pluie qui commence dans moins de 60 minutes (en cours :
// 0). Négatif : aucune pluie ou inconnue.
constexpr bool pluie_proche(Fraicheur meteo, int pluie_dans_min) noexcept {
    return meteo == Fraicheur::Fraiche && pluie_dans_min >= 0 && pluie_dans_min < PLUIE_PROCHE_MAX_MIN;
}

// Règle 3 : l'instant est-il dans le créneau de sortie du jour local ? Le lever et le coucher doivent tomber dans le jour local de
// `maintenant_s` (sinon ils sont d'un autre jour, donc inconnus) ; 0 ou moins : inconnu.
constexpr bool creneau_de_sortie(std::int64_t maintenant_s, std::int32_t decalage_local_s, std::int64_t lever_s,
                                 std::int64_t coucher_s) noexcept {
    if (maintenant_s <= 0 || lever_s <= 0 || coucher_s <= 0)
        return false;
    // Seconde du jour LOCAL (0 à 86 399), y compris pour un décalage négatif (Amériques) : le reste de la division doit être
    // ramené dans [0, 86 400).
    std::int64_t seconde_du_jour = (maintenant_s + decalage_local_s) % JOUR_S;
    if (seconde_du_jour < 0)
        seconde_du_jour += JOUR_S;
    const std::int64_t minuit_local_s = maintenant_s - seconde_du_jour;
    if (lever_s < minuit_local_s || lever_s >= minuit_local_s + JOUR_S)
        return false;
    if (coucher_s < minuit_local_s || coucher_s >= minuit_local_s + JOUR_S)
        return false;
    const bool matin = maintenant_s >= lever_s && seconde_du_jour < CRENEAU_MATIN_FIN_S;
    const bool soir = seconde_du_jour >= CRENEAU_SOIR_DEBUT_S && maintenant_s < coucher_s;
    return matin || soir;
}

// Règle 4 : le vent est-il STRICTEMENT au-dessus du seuil réglé ? Seuil non réglé (0 ou moins) : non. Un vent inconnu (-1) ne dépasse
// jamais un seuil positif.
constexpr bool vent_au_dessus_du_seuil(std::int32_t vent_dixiemes_kmh, std::int32_t seuil_dixiemes_kmh) noexcept {
    return seuil_dixiemes_kmh > 0 && vent_dixiemes_kmh > seuil_dixiemes_kmh;
}

// Règle 5 : la prochaine basse mer est-elle dans les 3 prochaines heures (180 minutes comprises, maintenant compris) ?
constexpr bool basse_mer_proche(int basse_mer_dans_min) noexcept {
    return basse_mer_dans_min >= 0 && basse_mer_dans_min <= BASSE_MER_MAX_MIN;
}

// Garde-fou : le dernier changement date-t-il d'assez longtemps ? Aucun changement noté : oui. Une horloge revenue en arrière (le
// dernier changement serait dans le futur) ne bloque pas : l'horodatage ne veut plus rien dire.
constexpr bool delai_ecoule(const EtatAuto &etat, std::int64_t maintenant_s) noexcept {
    if (!etat.changement_connu)
        return true;
    const std::int64_t ecoule = maintenant_s - etat.change_a_s;
    return ecoule < 0 || ecoule >= CHANGEMENT_ECART_MIN_S;
}

namespace detail {

struct Choix {
    Profil profil = Profil::Meteo;
    Raison raison = Raison::Defaut;
};

// Les règles 3 à 5 : le premier profil dont TOUT convient, données fraîches comprises. Si une règle conviendrait sans la
// péremption de sa donnée, on le retient : sans autre profil, c'est la raison « données périmées » et non « par défaut ».
constexpr Choix choisir_profil(const Entrees &e) noexcept {
    bool perimee = false;
    if (e.spot_surf && e.activite_surf &&
        creneau_de_sortie(e.maintenant_s, e.decalage_local_s, e.lever_s, e.coucher_s)) {
        if (e.fraicheur_mer == Fraicheur::Fraiche)
            return {Profil::Surf, Raison::Surf};
        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;
    }
    if (e.activite_vent_bateau && vent_au_dessus_du_seuil(e.vent_dixiemes_kmh, e.seuil_vent_dixiemes_kmh)) {
        if (e.fraicheur_meteo == Fraicheur::Fraiche)
            return {Profil::VentBateau, Raison::VentBateau};
        perimee = perimee || e.fraicheur_meteo == Fraicheur::Perimee;
    }
    if (e.spot_peche_a_pied && basse_mer_proche(e.basse_mer_dans_min)) {
        if (e.fraicheur_mer == Fraicheur::Fraiche)
            return {Profil::Cote, Raison::Cote};
        perimee = perimee || e.fraicheur_mer == Fraicheur::Perimee;
    }
    return {Profil::Meteo, perimee ? Raison::DonneesPerimees : Raison::Defaut};
}

} // namespace detail

// ---- la décision ----------------------------------------------------------------------------------------------------------------

constexpr Decision decider(const Entrees &e) noexcept {
    Decision d;
    d.bandeau_agrandi = alerte_agrandit_le_bandeau(e.niveau_alerte);

    // Un profil épinglé gagne sur 2 à 6, sans délai ni attente de la fin d'un toucher (c'est la personne qui l'a demandé). L'état
    // rendu n'a plus de dernier changement : le désépinglage reprend les règles tout de suite.
    if (e.epingle && profil_valide(e.profil_epingle)) {
        d.profil = e.profil_epingle;
        d.raison = Raison::Epingle;
        d.raison_profil = Raison::Epingle;
        d.etat.profil = e.profil_epingle;
        return d;
    }

    EtatAuto avant = e.etat;
    if (!profil_valide(avant.profil))
        avant = EtatAuto{};

    d.carte_pluie = pluie_proche(e.fraicheur_meteo, e.pluie_dans_min);

    const detail::Choix voulu = detail::choisir_profil(e);
    d.profil = avant.profil;
    d.etat = avant;
    if (voulu.profil == avant.profil) {
        d.raison_profil = voulu.raison;
    } else {
        // Un changement : jamais pendant un toucher, jamais sans heure ; sinon seulement après 30 min, sauf pour une donnée
        // périmée (retour à Météo tout de suite).
        const bool permis = !e.toucher_en_cours && e.maintenant_s > 0 &&
                            (voulu.raison == Raison::DonneesPerimees || delai_ecoule(avant, e.maintenant_s));
        if (permis) {
            d.profil = voulu.profil;
            d.raison_profil = voulu.raison;
            d.etat = EtatAuto{voulu.profil, true, e.maintenant_s};
        } else {
            d.raison_profil = Raison::ChangementDiffere;
        }
    }

    // La pastille : l'alerte d'abord, la pluie ensuite, sinon la raison du profil.
    if (d.bandeau_agrandi)
        d.raison = e.niveau_alerte >= vigie::Niveau::Rouge ? Raison::AlerteRouge : Raison::AlerteOrange;
    else if (d.carte_pluie)
        d.raison = Raison::PluieProche;
    else
        d.raison = d.raison_profil;
    return d;
}

} // namespace ecran::accueil
