// Noyau pur : les MOTS de la page mer en sept langues (fr, en, de, nl, es, it, tr) et la mise en forme de ce que la page écrit (heures,
// créneau, valeur d'un critère, marée, vigilance, âge des données). C++17 standard, aucun include ESP-IDF ni LVGL, aucun flottant,
// évaluable à la compilation (test/host/test_mer_textes.cpp).
//
// Ce que ce fichier reprend : les phrases et les tables de vigie-display (firmware/core/vigie_text.cpp : activités, verdicts, critères, le
// vocabulaire des états du contrat, les phrases de vigilance et d'âge), traduites dans les sept langues de l'écran (ADR-0006). Le texte
// FRANÇAIS est la clé (comme ecran/i18n.hpp) ; repli sur l'anglais puis sur le français (vigie::traduction). Les marqueurs sont NOMMÉS
// (« {heure} ») et non en %s : une traduction peut changer l'ordre des mots (« {heure} saatine kadar » en turc). Un marqueur inconnu reste tel
// quel dans le texte, donc visible.
//
// Table propre à la page tant que l'écran n'est pas stabilisé, comme vigie_textes.hpp et visuel_textes.hpp : ajouter une clé à la table
// commune (i18n_donnees.inc) touche les sept fichiers de langue, la table générée et les décomptes de test_i18n.cpp, que d'autres sessions
// modifient. À migrer dans i18n/*.yaml (même clé, mêmes traductions) quand la page sera stable.
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
    {"Mer", {"Sea", "Meer", "Zee", "Mar", "Mare", "Deniz"}},
    {"Meilleur créneau",
     {"Best slot", "Beste Zeit", "Beste moment", "Mejor franja", "Fascia migliore", "En iyi aralık"}},
    {"aucun", {"none", "keiner", "geen", "ninguno", "nessuno", "yok"}},
    {"Marée", {"Tide", "Gezeiten", "Getij", "Marea", "Marea", "Gelgit"}},
    {"Pas de marée connue pour ce lieu",
     {"No tide known for this place", "Keine Gezeiten für diesen Ort bekannt", "Geen getij bekend voor deze plaats",
      "Sin mareas conocidas para este lugar", "Nessuna marea nota per questo luogo", "Bu yer için bilinen gelgit yok"}},
    {"Soleil", {"Sun", "Sonne", "Zon", "Sol", "Sole", "Güneş"}},
    {"Lever", {"Sunrise", "Aufgang", "Opkomst", "Salida", "Alba", "Doğuş"}},
    {"Coucher", {"Sunset", "Untergang", "Ondergang", "Puesta", "Tramonto", "Batış"}},
    {"nuit", {"night", "Nacht", "nacht", "noche", "notte", "gece"}},
    {"une barre = 1 heure · trait blanc = maintenant",
     {"one bar = 1 hour · white line = now", "ein Balken = 1 Stunde · weißer Strich = jetzt",
      "één balk = 1 uur · witte streep = nu", "una barra = 1 hora · línea blanca = ahora",
      "una barra = 1 ora · linea bianca = adesso", "bir çubuk = 1 saat · beyaz çizgi = şimdi"}},
    {"Règles de Vigie, pas pour la sécurité",
     {"Vigie's own rules, not for safety", "Eigene Regeln von Vigie, nicht für die Sicherheit",
      "Eigen regels van Vigie, niet voor de veiligheid", "Reglas propias de Vigie, no para la seguridad",
      "Regole proprie di Vigie, non per la sicurezza", "Vigie'nin kendi kuralları, güvenlik için değil"}},
    {"Indisponible ici",
     {"Not available here", "Hier nicht verfügbar", "Hier niet beschikbaar", "No disponible aquí",
      "Non disponibile qui", "Burada kullanılamıyor"}},
    {"Pleine mer", {"High water", "Hochwasser", "Hoogwater", "Pleamar", "Alta marea", "Yüksek gelgit"}},
    {"Basse mer", {"Low water", "Niedrigwasser", "Laagwater", "Bajamar", "Bassa marea", "Alçak gelgit"}},
    {"Coefficient {n} (estimé)",
     {"Coefficient {n} (estimated)", "Koeffizient {n} (geschätzt)", "Coëfficiënt {n} (geschat)",
      "Coeficiente {n} (estimado)", "Coefficiente {n} (stimato)", "Katsayı {n} (tahmini)"}},
    {"Mer : données indisponibles",
     {"Sea data unavailable", "Meeresdaten nicht verfügbar", "Zeegegevens niet beschikbaar",
      "Datos del mar no disponibles", "Dati del mare non disponibili", "Deniz verileri yok"}},
    {"Mer · {age}",
     {"Sea data · {age}", "Meeresdaten · {age}", "Zeegegevens · {age}", "Datos del mar · {age}",
      "Dati del mare · {age}", "Deniz verileri · {age}"}},
    {"Certaines données de mer datent d'une lecture précédente",
     {"Some sea data is from an earlier reading", "Einige Meeresdaten stammen von einer früheren Abfrage",
      "Sommige zeegegevens komen van een eerdere meting", "Algunos datos del mar son de una lectura anterior",
      "Alcuni dati del mare risalgono a una lettura precedente", "Bazı deniz verileri önceki bir okumadan"}},
    {"Données de mer trop anciennes",
     {"Sea data too old", "Meeresdaten zu alt", "Zeegegevens te oud", "Datos del mar demasiado antiguos",
      "Dati del mare troppo vecchi", "Deniz verileri çok eski"}},
    {"Vigilances côtières indisponibles",
     {"Coastal warnings unavailable", "Küstenwarnungen nicht verfügbar", "Kustwaarschuwingen niet beschikbaar",
      "Avisos costeros no disponibles", "Allerte costiere non disponibili", "Kıyı uyarıları yok"}},
    {"Vigilance côtière {niveau}",
     {"Coastal warning: {niveau}", "Küstenwarnung: {niveau}", "Kustwaarschuwing: {niveau}", "Aviso costero: {niveau}",
      "Allerta costiera: {niveau}", "Kıyı uyarısı: {niveau}"}},
    {"Vigilance côtière {niveau} · jusqu'à {heure}",
     {"Coastal warning: {niveau} · until {heure}", "Küstenwarnung: {niveau} · bis {heure}",
      "Kustwaarschuwing: {niveau} · tot {heure}", "Aviso costero: {niveau} · hasta las {heure}",
      "Allerta costiera: {niveau} · fino alle {heure}", "Kıyı uyarısı: {niveau} · {heure} saatine kadar"}},
    {"jaune", {"yellow", "gelb", "geel", "amarillo", "giallo", "sarı"}},
    {"orange", {"orange", "orange", "oranje", "naranja", "arancione", "turuncu"}},
    {"rouge", {"red", "rot", "rood", "rojo", "rosso", "kırmızı"}},
    {"Note {points} sur {sur}",
     {"Score {points} of {sur}", "Punkte {points} von {sur}", "Score {points} van {sur}", "Puntos {points} de {sur}",
      "Punteggio {points} su {sur}", "{sur} üzerinden {points} puan"}},
    {"{activite} · détail",
     {"{activite} · detail", "{activite} · Details", "{activite} · details", "{activite} · detalle",
      "{activite} · dettaglio", "{activite} · ayrıntı"}},
    {"pour {heure}",
     {"for {heure}", "für {heure}", "voor {heure}", "para las {heure}", "per le {heure}", "{heure} için"}},
    {"Critère", {"Criterion", "Kriterium", "Criterium", "Criterio", "Criterio", "Ölçüt"}},
    {"Valeur", {"Value", "Wert", "Waarde", "Valor", "Valore", "Değer"}},
    {"Note", {"Score", "Punkte", "Score", "Puntos", "Punti", "Puan"}},
    {"Lecture", {"Reading", "Einordnung", "Beoordeling", "Valoración", "Valutazione", "Değerlendirme"}},
    {"{critere} : {etat}",
     {"{critere}: {etat}", "{critere}: {etat}", "{critere}: {etat}", "{critere}: {etat}", "{critere}: {etat}",
      "{critere}: {etat}"}},
    {"à l'instant", {"just now", "gerade eben", "zojuist", "ahora mismo", "proprio ora", "az önce"}},
    {"il y a {n} min", {"{n} min ago", "vor {n} Min.", "{n} min geleden", "hace {n} min", "{n} min fa", "{n} dk önce"}},
    {"il y a {n} h", {"{n} h ago", "vor {n} Std.", "{n} u geleden", "hace {n} h", "{n} h fa", "{n} sa önce"}},
    {"il y a {n} j", {"{n} d ago", "vor {n} T.", "{n} d geleden", "hace {n} d", "{n} g fa", "{n} g önce"}},
    {"Force {n}", {"Force {n}", "Stärke {n}", "Kracht {n}", "Fuerza {n}", "Forza {n}", "Kuvvet {n}"}},
    {"Fermer", {"Close", "Schließen", "Sluiten", "Cerrar", "Chiudi", "Kapat"}},
    // Les noms propres (Open-Meteo, Copernicus Marine, EOT20) et « CC BY 4.0 » ne se traduisent pas.
    {"Données mer : Open-Meteo, Copernicus Marine, marée EOT20 (CC BY 4.0)",
     {"Sea data: Open-Meteo, Copernicus Marine, EOT20 tide (CC BY 4.0)",
      "Meeresdaten: Open-Meteo, Copernicus Marine, Gezeiten EOT20 (CC BY 4.0)",
      "Zeegegevens: Open-Meteo, Copernicus Marine, getij EOT20 (CC BY 4.0)",
      "Datos del mar: Open-Meteo, Copernicus Marine, marea EOT20 (CC BY 4.0)",
      "Dati del mare: Open-Meteo, Copernicus Marine, marea EOT20 (CC BY 4.0)",
      "Deniz verileri: Open-Meteo, Copernicus Marine, EOT20 gelgiti (CC BY 4.0)"}},
    {"Estimations, pas pour la navigation",
     {"Estimates, not for navigation", "Schätzungen, nicht für die Navigation", "Schattingen, niet voor navigatie",
      "Estimaciones, no para la navegación", "Stime, non per la navigazione", "Tahmini değerler, seyir için değil"}},
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
    {"Surf", {"Surf", "Surfen", "Surfen", "Surf", "Surf", "Sörf"}},
    {"Kite / wing", {"Kite / wing", "Kite / Wing", "Kite / wing", "Kite / wing", "Kite / wing", "Kite / wing"}},
    {"Bateau", {"Boat", "Boot", "Boot", "Barco", "Barca", "Tekne"}},
    {"Pêche à pied", {"Shore fishing", "Küstenangeln", "Kustvissen", "Pesca a pie", "Pesca da riva", "Kıyıdan balık"}},
};

constexpr std::string_view nom_activite(Activite a, Langue langue) noexcept {
    const int i = static_cast<int>(a);
    return (i >= 0 && i < NB_ACTIVITES) ? vigie::traduction(ACTIVITES[i], langue) : std::string_view{};
}

// Indexé par `Verdict` ; « inconnu » n'a pas de mot (la page écrit un tiret).
inline constexpr Mot VERDICTS[4] = {
    {"", {"", "", "", "", "", ""}},
    {"Bon", {"Good", "Gut", "Goed", "Bueno", "Buono", "İyi"}},
    {"Possible", {"Possible", "Möglich", "Mogelijk", "Posible", "Possibile", "Mümkün"}},
    {"Déconseillé", {"Not advised", "Nicht ratsam", "Afgeraden", "No aconsejado", "Sconsigliato", "Önerilmez"}},
};

constexpr std::string_view nom_verdict(Verdict v, Langue langue) noexcept {
    const int i = static_cast<int>(v);
    return (i >= 0 && i < 4) ? vigie::traduction(VERDICTS[i], langue) : std::string_view{};
}

// Indexé par `Critere` (le même ordre que l'énumération).
inline constexpr Mot CRITERES[static_cast<int>(Critere::Nombre)] = {
    {"", {"", "", "", "", "", ""}},
    {"Houle", {"Swell", "Dünung", "Deining", "Oleaje", "Onda", "Dalga"}},
    {"Période de houle",
     {"Swell period", "Dünungsperiode", "Deiningsperiode", "Periodo del oleaje", "Periodo dell'onda",
      "Dalga periyodu"}},
    {"Direction de la houle",
     {"Swell direction", "Dünungsrichtung", "Deiningsrichting", "Dirección del oleaje", "Direzione dell'onda",
      "Dalga yönü"}},
    {"Vent", {"Wind", "Wind", "Wind", "Viento", "Vento", "Rüzgâr"}},
    {"Marée", {"Tide", "Gezeiten", "Getij", "Marea", "Marea", "Gelgit"}},
    {"Écart des rafales",
     {"Gust gap", "Böenabstand", "Rukverschil", "Diferencia de rachas", "Differenza delle raffiche", "Hamle farkı"}},
    {"Direction du vent",
     {"Wind direction", "Windrichtung", "Windrichting", "Dirección del viento", "Direzione del vento", "Rüzgâr yönü"}},
    {"Vent", {"Wind", "Wind", "Wind", "Viento", "Vento", "Rüzgâr"}},
    {"Rafales", {"Gusts", "Böen", "Windstoten", "Rachas", "Raffiche", "Hamleler"}},
    {"Mer", {"Sea", "Meer", "Zee", "Mar", "Mare", "Deniz"}},
    {"Basse mer", {"Low water", "Niedrigwasser", "Laagwater", "Bajamar", "Bassa marea", "Alçak gelgit"}},
    {"Coefficient (est.)",
     {"Coefficient (est.)", "Koeffizient (gesch.)", "Coëfficiënt (gesch.)", "Coeficiente (est.)",
      "Coefficiente (stim.)", "Katsayı (tah.)"}},
    {"Vigilance côtière",
     {"Coastal warning", "Küstenwarnung", "Kustwaarschuwing", "Aviso costero", "Allerta costiera", "Kıyı uyarısı"}},
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
    {Critere::HauteurHoule,
     Etat::TropPetite,
     {"trop petite", {"too small", "zu klein", "te klein", "demasiado pequeña", "troppo piccola", "çok küçük"}}},
    {Critere::HauteurHoule, Etat::Petite, {"petite", {"small", "klein", "klein", "pequeña", "piccola", "küçük"}}},
    {Critere::HauteurHoule,
     Etat::Bonne,
     {"bonne taille",
      {"right size", "passende Größe", "goede maat", "buen tamaño", "buona dimensione", "uygun boyut"}}},
    {Critere::HauteurHoule, Etat::Grosse, {"grosse", {"big", "groß", "groot", "grande", "grande", "büyük"}}},
    {Critere::HauteurHoule,
     Etat::TropGrosse,
     {"trop grosse", {"too big", "zu groß", "te groot", "demasiado grande", "troppo grande", "çok büyük"}}},
    {Critere::PeriodeHoule, Etat::Courte, {"courte", {"short", "kurz", "kort", "corto", "corto", "kısa"}}},
    {Critere::PeriodeHoule, Etat::Moyenne, {"moyenne", {"medium", "mittel", "gemiddeld", "media", "media", "orta"}}},
    {Critere::PeriodeHoule, Etat::Longue, {"longue", {"long", "lang", "lang", "largo", "lungo", "uzun"}}},
    {Critere::DirectionHoule,
     Etat::DeFace,
     {"face à la plage",
      {"facing the beach", "zum Strand hin", "naar het strand gericht", "de cara a la playa", "verso la spiaggia",
       "kıyıya dönük"}}},
    {Critere::DirectionHoule,
     Etat::Oblique,
     {"de biais", {"oblique", "schräg", "schuin", "oblicua", "obliqua", "çapraz"}}},
    {Critere::DirectionHoule,
     Etat::HorsAxe,
     {"hors axe", {"off axis", "außerhalb der Achse", "buiten de as", "fuera de eje", "fuori asse", "eksen dışı"}}},
    {Critere::Vent, Etat::Faible, {"faible", {"light", "schwach", "zwak", "flojo", "debole", "zayıf"}}},
    {Critere::Vent,
     Etat::Offshore,
     {"de terre", {"offshore", "ablandig", "aflandig", "terral", "di terra", "karadan"}}},
    {Critere::Vent,
     Etat::Travers,
     {"de travers", {"cross-shore", "seitlich", "zijwaarts", "lateral", "laterale", "yandan"}}},
    {Critere::Vent, Etat::Onshore, {"de mer", {"onshore", "auflandig", "aanlandig", "de mar", "di mare", "denizden"}}},
    {Critere::Vent, Etat::Forte, {"fort", {"strong", "stark", "sterk", "fuerte", "forte", "güçlü"}}},
    {Critere::Vent,
     Etat::DansLaPlage,
     {"dans votre plage",
      {"in your range", "in Ihrem Bereich", "binnen uw bereik", "en su rango", "nel tuo intervallo", "aralığınızda"}}},
    {Critere::Vent,
     Etat::UnPeuFaible,
     {"un peu faible",
      {"a bit light", "etwas schwach", "een beetje zwak", "algo flojo", "un po' debole", "biraz zayıf"}}},
    {Critere::Vent,
     Etat::UnPeuForte,
     {"un peu fort",
      {"a bit strong", "etwas stark", "een beetje sterk", "algo fuerte", "un po' forte", "biraz güçlü"}}},
    {Critere::Vent,
     Etat::TropFaible,
     {"trop faible", {"too light", "zu schwach", "te zwak", "demasiado flojo", "troppo debole", "çok zayıf"}}},
    {Critere::Vent,
     Etat::TropFort,
     {"trop fort", {"too strong", "zu stark", "te sterk", "demasiado fuerte", "troppo forte", "çok güçlü"}}},
    {Critere::Maree,
     Etat::BonMoment,
     {"bon moment", {"right stage", "guter Zeitpunkt", "goed moment", "buen momento", "buon momento", "uygun zaman"}}},
    {Critere::Maree,
     Etat::MauvaisMoment,
     {"mauvais moment",
      {"wrong stage", "schlechter Zeitpunkt", "slecht moment", "mal momento", "momento sbagliato",
       "uygun olmayan zaman"}}},
    {Critere::EcartRafales,
     Etat::Regulier,
     {"régulier", {"steady", "gleichmäßig", "gelijkmatig", "constante", "costante", "sabit"}}},
    {Critere::EcartRafales,
     Etat::Rafaleux,
     {"rafaleux", {"gusty", "böig", "vlagerig", "racheado", "rafficoso", "hamleli"}}},
    {Critere::EcartRafales,
     Etat::TresRafaleux,
     {"très rafaleux", {"very gusty", "sehr böig", "zeer vlagerig", "muy racheado", "molto rafficoso", "çok hamleli"}}},
    {Critere::DirectionVent,
     Etat::Onshore,
     {"de mer", {"onshore", "auflandig", "aanlandig", "de mar", "di mare", "denizden"}}},
    {Critere::DirectionVent,
     Etat::Offshore,
     {"de terre", {"offshore", "ablandig", "aflandig", "terral", "di terra", "karadan"}}},
    {Critere::DirectionVent,
     Etat::Travers,
     {"de travers", {"cross-shore", "seitlich", "zijwaarts", "lateral", "laterale", "yandan"}}},
    {Critere::ForceVent,
     Etat::JusquaModere,
     {"modéré ou moins",
      {"moderate or less", "mäßig oder weniger", "matig of minder", "moderado o menos", "moderato o meno",
       "orta ya da daha az"}}},
    {Critere::ForceVent, Etat::Frais, {"bon frais", {"fresh", "frisch", "fris", "fresco", "teso", "taze"}}},
    {Critere::ForceVent,
     Etat::FortOuPlus,
     {"fort ou plus",
      {"strong or more", "stark oder mehr", "sterk of meer", "fuerte o más", "forte o più", "güçlü ya da daha fazla"}}},
    {Critere::ForceVent,
     Etat::JusquaFrais,
     {"bon frais ou moins",
      {"fresh or less", "frisch oder weniger", "fris of minder", "fresco o menos", "teso o meno",
       "taze ya da daha az"}}},
    {Critere::ForceVent, Etat::Forte, {"fort", {"strong", "stark", "sterk", "fuerte", "forte", "güçlü"}}},
    {Critere::ForceVent,
     Etat::QuasiCoupDeVentOuPlus,
     {"grand frais ou plus",
      {"near gale or more", "steif oder mehr", "stijf of meer", "muy fuerte o más", "molto teso o più",
       "çok sert ya da daha fazla"}}},
    {Critere::ForceRafales,
     Etat::JusquaFrais,
     {"bon frais ou moins",
      {"fresh or less", "frisch oder weniger", "fris of minder", "fresco o menos", "teso o meno",
       "taze ya da daha az"}}},
    {Critere::ForceRafales, Etat::Forte, {"fort", {"strong", "stark", "sterk", "fuerte", "forte", "güçlü"}}},
    {Critere::ForceRafales,
     Etat::CoupDeVentOuPlus,
     {"coup de vent ou plus",
      {"gale or more", "Sturm oder mehr", "storm of meer", "temporal o más", "burrasca o più",
       "fırtına ya da daha fazla"}}},
    {Critere::HauteurMer, Etat::Basse, {"faible", {"low", "niedrig", "laag", "baja", "bassa", "alçak"}}},
    {Critere::HauteurMer, Etat::Modere, {"modérée", {"moderate", "mäßig", "matig", "moderada", "moderata", "orta"}}},
    {Critere::HauteurMer, Etat::Haute, {"forte", {"high", "hoch", "hoog", "alta", "alta", "yüksek"}}},
    {Critere::BasseMer,
     Etat::DansLaFenetre,
     {"dans la fenêtre",
      {"in the window", "im Zeitfenster", "in het tijdvenster", "en la ventana", "nella finestra", "pencerede"}}},
    {Critere::BasseMer,
     Etat::HorsFenetre,
     {"hors fenêtre",
      {"outside the window", "außerhalb des Zeitfensters", "buiten het tijdvenster", "fuera de la ventana",
       "fuori dalla finestra", "pencere dışı"}}},
    {Critere::Coefficient, Etat::Petite, {"faible", {"small", "niedrig", "laag", "bajo", "basso", "düşük"}}},
    {Critere::Coefficient, Etat::Modere, {"moyen", {"moderate", "mittel", "gemiddeld", "medio", "medio", "orta"}}},
    {Critere::Coefficient, Etat::Grand, {"fort", {"large", "hoch", "hoog", "alto", "alto", "yüksek"}}},
    {Critere::Coefficient,
     Etat::TresGrand,
     {"très fort", {"very large", "sehr hoch", "zeer hoog", "muy alto", "molto alto", "çok yüksek"}}},
    {Critere::Vigilance, Etat::Orange, {"orange", {"orange", "orange", "oranje", "naranja", "arancione", "turuncu"}}},
    {Critere::Vigilance, Etat::Rouge, {"rouge", {"red", "rot", "rood", "rojo", "rosso", "kırmızı"}}},
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
