// Contrat « météo » entre le client Open-Meteo (session meteo-appareil, propriétaire) et l'interface (écran principal, page de
// réglages). Ce fichier est PUR : seul <cstdint>, aucun en-tête ESP-IDF, pour que le code d'interface se compile sur le PC
// (rendu-pc). On peut AJOUTER des champs ; on n'en retire pas et on ne change pas leur sens.
//
// Les noms sont au niveau global, comme dans le contrat convenu entre les sessions ; le code d'un `namespace ecran` les trouve
// sans qualification.
//
// Deux niveaux, indépendants et compatibles :
//   - v1 `MeteoEtat` / `meteo_etat_lire()` : la météo actuelle en un coup d'œil (température, code, min/max du jour, libellé).
//     Inchangé depuis le premier commit, plus un champ (`minmax_connus`). L'écran principal actuel n'a rien à changer.
//   - v2 `MeteoDetail` / `meteo_detail_lire()` : tout ce que la requête Open-Meteo ramène — courant complet, 24 heures, 7 jours,
//     pluie des deux prochaines heures par quarts d'heure. Pour un écran « très visuel » (icônes, courbes, barres de pluie).
//
// Libellés et icônes : le contrat ne porte que des CODES. Pour le libellé français et l'icône d'un code WMO, l'interface
// appelle le noyau pur (aucun en-tête ESP-IDF, utilisable sur le PC) :
//     #include "ecran/meteo_wmo.hpp"
//     const ecran::meteo::Condition c = ecran::meteo::decrire(code_wmo, jour);   // c.libelle (UTF-8), c.icone (enum Icone)
//     ecran::meteo::nom_icone(c.icone);                                         // identifiant ASCII stable (« soleil », « lune »…)
//
// Unités : °C, km/h, hPa, mm (pluie, tout état confondu), cm (neige), % , degrés, codes WMO d'Open-Meteo. Tous les instants
// sont en secondes UNIX UTC (`int64_t`) : l'interface les convertit en heure locale avec `decalage_utc_s` (valable à la
// réception de la réponse) ou avec son propre fuseau. Les `float` sont exacts au dixième (le noyau compte en dixièmes entiers).
// Valeur absente de la réponse Open-Meteo : `METEO_PROBA_INCONNUE` pour une probabilité, sinon le champ est tel que le
// commentaire le dit ; aucune valeur n'est inventée.
#pragma once

#include <cstdint>

// =====================================================================================================================
// v1 — météo actuelle en un coup d'œil
// =====================================================================================================================

struct MeteoEtat {
    bool valide; // une météo exploitable existe (reçue ou relue du cache) ; faux : ne rien afficher d'autre que « — »
    bool perime; // la dernière réponse date de plus de 2 h, ou les essais échouent depuis longtemps
    int64_t maj_epoch_s; // instant UNIX (UTC) de la dernière réponse reçue ; 0 si aucune
    float temperature_c; // °C, un chiffre après la virgule
    int code_wmo;        // code météo WMO d'Open-Meteo
    bool jour;           // is_day d'Open-Meteo (soleil ou lune pour les codes 0 à 2)
    float min_c;         // minimum du jour, °C ; seulement si valide
    float max_c;         // maximum du jour, °C ; seulement si valide
    char libelle[40];    // libellé français court (« Partiellement nuageux »), terminé par un zéro ; vide si !valide
    // AJOUTÉ après le premier commit des contrats (on peut ajouter, jamais retirer) :
    bool minmax_connus; // faux : aucune prévision ne couvre ce jour ; min_c et max_c valent alors temperature_c
};

// Copie atomique de l'état courant, sûre depuis n'importe quelle tâche. Rend `valide` (donc faux tant qu'aucune météo n'est
// connue, et alors `*sortie` est quand même remise à zéro). `sortie` nul : rend faux sans rien écrire.
bool meteo_etat_lire(MeteoEtat *sortie);

// =====================================================================================================================
// v2 — météo détaillée (courant, heure par heure, 7 jours, pluie proche)
// =====================================================================================================================

inline constexpr int METEO_HEURES_MAX =
    24; // heures de prévision gardées (la première est l'heure en cours à la réception)
inline constexpr int METEO_JOURS_MAX = 7;  // jours de prévision (le premier est le jour local de la réception)
inline constexpr int METEO_QUARTS_MAX = 9; // quarts d'heure de pluie (le dernier quart écoulé + les 2 h qui viennent)
inline constexpr int METEO_PROBA_INCONNUE = -1; // probabilité de précipitations absente de la réponse

// Mesure du moment (`current` d'Open-Meteo : valeur de modèle mise à jour toutes les 15 min, pas une mesure de station).
struct MeteoCourant {
    int64_t instant_epoch_s; // heure de la valeur (current.time), UTC
    float temperature_c;
    float ressentie_c;          // température ressentie
    int16_t humidite_pct;       // 0..100
    bool jour;                  // is_day : vrai le jour, faux la nuit (soleil ou lune pour les codes 0 à 2)
    float precipitation_mm;     // précipitations du moment (pluie, averses, neige fondue) ; 0 s'il ne pleut pas
    int16_t code_wmo;           // état du ciel, code WMO
    int16_t nuages_pct;         // couverture nuageuse 0..100
    float pression_hpa;         // pression ramenée au niveau de la mer
    float vent_kmh;             // vitesse moyenne du vent à 10 m
    int16_t direction_vent_deg; // d'où vient le vent : 0 = nord, 90 = est, 180 = sud, 270 = ouest
    float rafales_kmh;
    int16_t proba_precip_pct; // probabilité de précipitations de l'heure en cours, ou METEO_PROBA_INCONNUE
    char libelle[40];         // libellé français court de code_wmo/jour, terminé par un zéro
};

// Une heure de prévision = l'intervalle [debut_epoch_s, debut_epoch_s + 3600) ; les heures se suivent de 3600 s.
// Ce qui est « instantané » (température, ressentie, humidité, vent, rafales, direction, nuages, code, jour) est la valeur AU
// DÉBUT de l'heure. Ce qui est un cumul ou une moyenne (précipitations, probabilité, UV) porte sur l'heure ENTIÈRE : Open-Meteo
// donne ces trois-là pour « l'heure qui précède » son étiquette (documenté ; recoupé le 07/11/2026 sur une vraie réponse, voir
// docs/mesures/meteo.md), le noyau les décale donc d'une étiquette pour que `heures[i]` soit cohérente. L'interface n'a pas à
// s'en soucier : elle dessine `heures[i]` sous l'heure `debut_epoch_s`.
struct MeteoHeure {
    int64_t debut_epoch_s;
    float temperature_c;
    float ressentie_c;
    int16_t humidite_pct;
    int16_t proba_precip_pct; // probabilité de plus de 0,1 mm dans l'heure, ou METEO_PROBA_INCONNUE
    float precipitation_mm;   // cumul prévu pour l'heure entière
    int16_t code_wmo;
    int16_t nuages_pct;
    float vent_kmh;
    float rafales_kmh;
    int16_t direction_vent_deg;
    float uv;  // indice UV moyen de l'heure (0 la nuit), un chiffre après la virgule
    bool jour; // is_day au début de l'heure
};

// Un jour de prévision. `jours[i].date_epoch_s` = minuit LOCAL du jour (en UTC) ; un jour dure 23, 24 ou 25 h.
struct MeteoJour {
    int64_t date_epoch_s;
    int16_t code_wmo; // état du ciel dominant du jour
    float min_c;
    float max_c;
    float ressentie_min_c;
    float ressentie_max_c;
    int64_t lever_epoch_s;        // lever du soleil, UTC
    int64_t coucher_epoch_s;      // coucher du soleil, UTC
    bool sans_soleil;             // lever == coucher : nuit polaire (jour polaire : comportement non vérifié)
    float duree_jour_h;           // durée entre lever et coucher, en heures (un chiffre après la virgule)
    float ensoleillement_h;       // ensoleillement prévu, en heures, à la minute près (8,55 = 8 h 33)
    float uv_max;                 // indice UV maximal du jour
    int16_t proba_precip_pct;     // probabilité maximale de précipitations du jour, ou METEO_PROBA_INCONNUE
    float precipitation_mm;       // cumul du jour (pluie, averses, neige fondue)
    float neige_cm;               // chute de neige du jour
    int16_t heures_precipitation; // nombre d'heures avec précipitations
    float vent_max_kmh;
    float rafales_max_kmh;
    int16_t direction_vent_dominante_deg;
};

// Pluie des deux prochaines heures, déduite des quarts d'heure d'Open-Meteo et RECALCULÉE à chaque lecture par rapport à
// l'heure de l'appareil. « Pluie » = au moins 0,1 mm sur un quart d'heure (bruine comprise).
struct MeteoPluieProche {
    bool
        connue; // des quarts d'heure existent ET l'heure de l'appareil les couvre ; faux : rien à afficher sur la pluie proche
    bool
        perimee; // les quarts d'heure ont plus de 30 min : ils restent lisibles mais ne sont plus un « dans N minutes » sûr
    bool en_cours; // il pleut pendant le quart d'heure en cours
    int dans_min; // minutes avant le début de la prochaine pluie ; 0 si elle est en cours ; -1 si aucune dans la fenêtre
    int fin_dans_min; // minutes avant la fin de cette pluie (en cours ou à venir) ; -1 si aucune pluie ou si elle dure au-delà de la fenêtre
    int fenetre_min; // minutes encore couvertes devant l'heure de l'appareil (de maintenant à la fin du dernier quart)
    float cumul_mm;  // total prévu entre maintenant et la fin de la fenêtre (le quart en cours compte en entier)
    float max_quart_mm; // plus forte quantité sur un quart d'heure
};

struct MeteoDetail {
    uint32_t version; // augmente à chaque nouvelle réponse reçue ou relue : l'interface ne redessine que si elle change
    bool valide;      // une météo détaillée existe ; faux : tout le reste est à zéro (indices à -1)
    bool perime;      // même règle que MeteoEtat::perime
    int64_t maj_epoch_s;    // instant UNIX (UTC) de la réponse
    int32_t decalage_utc_s; // décalage du fuseau du lieu à la réception (3600 en hiver à Paris)
    char fuseau[48];        // nom IANA du lieu (« Europe/Paris »), terminé par un zéro
    MeteoCourant courant;

    int nb_heures; // 0..METEO_HEURES_MAX
    int heure_courante; // indice dans heures[] de l'heure qui contient l'heure de l'appareil ; -1 si la liste ne la couvre pas
    MeteoHeure heures[METEO_HEURES_MAX];

    int nb_jours;     // 0..METEO_JOURS_MAX
    int jour_courant; // indice dans jours[] du jour qui contient l'heure de l'appareil ; -1 si aucun
    MeteoJour jours[METEO_JOURS_MAX];

    // Pluie par quart d'heure : quarts_precipitation_mm[i] = précipitations de l'intervalle
    // [quarts_debut_epoch_s + 900 * i, quarts_debut_epoch_s + 900 * (i + 1)). Le premier est en général le quart écoulé (Open-Meteo
    // donne le cumul du quart d'heure qui PRÉCÈDE l'étiquette : recoupé le 07/11/2026 sur une vraie réponse, voir docs/mesures/meteo.md).
    int nb_quarts;     // 0..METEO_QUARTS_MAX
    int quart_courant; // indice du quart d'heure qui contient l'heure de l'appareil ; -1 si aucun
    int64_t quarts_debut_epoch_s;
    float quarts_precipitation_mm[METEO_QUARTS_MAX];
    MeteoPluieProche pluie;
};

// Copie atomique de la météo détaillée (environ 2 Ko : 2 080 octets sur le PC ; la mettre en statique ou en PSRAM, pas sur une petite pile). Rend `valide`
// (faux tant qu'aucune météo n'est connue ; `*sortie` est alors remise à zéro, `heure_courante`, `jour_courant` et `quart_courant`
// valent -1). `sortie` nul : rend faux sans rien écrire. Sûre depuis n'importe quelle tâche (jamais depuis une interruption : elle
// prend un verrou, pour quelques dizaines de microsecondes).
//
// Rien n'est inventé. Une section que la réponse d'Open-Meteo n'a pas donnée ou qui était invraisemblable est VIDE (`nb_heures`,
// `nb_jours` ou `nb_quarts` à 0) et son indice vaut -1 ; en particulier `jours` n'est publié que si les sept jours ont tous leurs
// compléments (ensoleillement, vent, pluie), sinon `nb_jours` vaut 0. L'indice `heure_courante` vaut aussi -1 quand la liste ne
// couvre pas l'heure de l'appareil : c'est ce qu'on a mesuré à Terre-Neuve (UTC-3:30), où la série d'Open-Meteo commence après
// l'heure courante ; `courant.proba_precip_pct` vaut alors METEO_PROBA_INCONNUE plutôt que la probabilité de l'heure voisine.
// (Au Népal, UTC+5:45, la série commence avant : l'heure en cours est trouvée.) Les indices et la pluie proche sont recalculés
// à chaque lecture avec l'heure de l'appareil ; horloge non réglée : tous les indices valent -1 et `perime` est vrai.
bool meteo_detail_lire(MeteoDetail *sortie);

// Numéro de la dernière météo détaillée publiée (le même que `MeteoDetail::version`), sans copier la structure ni prendre de
// verrou. 0 tant qu'aucune météo n'est connue, et de nouveau 0 quand la zone change (la météo de l'ancien lieu est oubliée) : un
// changement de numéro, y compris vers 0, veut dire « relire ». Une nouvelle réponse prend toujours un numéro encore jamais
// utilisé. Permet à l'interface de ne lire et ne redessiner que quand la météo a changé.
uint32_t meteo_detail_version();
