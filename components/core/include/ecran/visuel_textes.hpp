// Noyau pur : les textes de l'écran principal qui ne sont pas encore dans la table de traduction (i18n/*.yaml, ADR-0006). Le texte
// FRANÇAIS est la clé, comme dans ecran/i18n.hpp ; `texte()` cherche d'abord ici, puis dans la table commune (libellés météo, noms
// de jours et de mois), puis rend le français.
//
// Pourquoi une table à part : ajouter une clé à la table commune touche les sept fichiers de langue, la table générée, la page de
// réglages et les décomptes de test_i18n.cpp (autres sessions). Ces textes sont à migrer dans i18n/*.yaml quand l'écran est
// stabilisé (même clé, mêmes traductions), puis ce fichier disparaît. Les phrases courtes sont voulues : ce sont des étiquettes
// posées sur de petites cartes.
//
// NON RELUES par un locuteur natif (comme le reste de la table). Test : test/host/test_visuel_textes.cpp (six traductions
// présentes, mêmes marques « %d », longueurs bornées, pas de clé en double).
#pragma once

#include <cstddef>
#include <string_view>

#include "ecran/i18n.hpp"

namespace ecran::visuel {

struct Texte {
    std::string_view fr;
    std::string_view t[i18n::NOMBRE_LANGUES - 1]; // en, de, nl, es, it, tr
};

inline constexpr Texte TEXTES[] = {
    // --- grandeurs ---
    {"Ressenti", {"Feels like", "Gefühlt", "Voelt als", "Sensación", "Percepita", "Hissedilen"}},
    {"Vent", {"Wind", "Wind", "Wind", "Viento", "Vento", "Rüzgâr"}},
    {"Rafales", {"Gusts", "Böen", "Windstoten", "Ráfagas", "Raffiche", "Hamleler"}},
    {"Pluie", {"Rain", "Regen", "Regen", "Lluvia", "Pioggia", "Yağmur"}},
    {"Humidité", {"Humidity", "Feuchte", "Vochtigheid", "Humedad", "Umidità", "Nem"}},
    {"Pression", {"Pressure", "Luftdruck", "Luchtdruk", "Presión", "Pressione", "Basınç"}},
    {"UV", {"UV", "UV", "UV", "UV", "UV", "UV"}},
    {"Soleil", {"Sun", "Sonne", "Zon", "Sol", "Sole", "Güneş"}},
    {"Lune", {"Moon", "Mond", "Maan", "Luna", "Luna", "Ay"}},
    {"Lever", {"Sunrise", "Aufgang", "Opkomst", "Salida", "Alba", "Doğuş"}},
    {"Coucher", {"Sunset", "Untergang", "Ondergang", "Puesta", "Tramonto", "Batış"}},
    {"Aujourd'hui", {"Today", "Heute", "Vandaag", "Hoy", "Oggi", "Bugün"}},
    {"Maintenant", {"Now", "Jetzt", "Nu", "Ahora", "Adesso", "Şimdi"}},
    // --- pluie ---
    // Ces trois phrases se lisent sous la valeur de la tuile « pluie » (le parapluie dit de quoi on parle) : très courtes.
    {"dans %d min", {"in %d min", "in %d Min.", "over %d min", "en %d min", "tra %d min", "%d dk sonra"}},
    {"Il pleut", {"Raining", "Es regnet", "Het regent", "Llueve", "Piove", "Yağmurlu"}},
    {"Sec 2 h", {"Dry 2 h", "Trocken", "Droog 2 u", "Seco 2 h", "Asciutto 2 h", "2 sa kuru"}},
    // --- pression ---
    {"En hausse", {"Rising", "Steigend", "Stijgend", "Subiendo", "In aumento", "Yükseliyor"}},
    {"En baisse", {"Falling", "Fallend", "Dalend", "Bajando", "In calo", "Düşüyor"}},
    {"Stable", {"Steady", "Stabil", "Stabiel", "Estable", "Stabile", "Sabit"}},
    // --- états de l'écran ---
    {"Données périmées",
     {"Outdated data", "Daten veraltet", "Verouderd", "Datos antiguos", "Dati vecchi", "Eski veri"}},
    {"Météo indisponible",
     {"Weather unavailable", "Wetter nicht verfügbar", "Weer niet beschikbaar", "Tiempo no disponible",
      "Meteo non disponibile", "Hava durumu yok"}},
    {"Heure du réseau en attente",
     {"Waiting for network time", "Warte auf Netzwerkzeit", "Wachten op netwerktijd", "Esperando la hora de red",
      "In attesa dell'ora di rete", "Ağ saati bekleniyor"}},
    {"Exemple", {"Example", "Beispiel", "Voorbeeld", "Ejemplo", "Esempio", "Örnek"}},
    {"Réglages", {"Settings", "Einstellungen", "Instellingen", "Ajustes", "Impostazioni", "Ayarlar"}},
    // --- indice UV ---
    {"Faible", {"Low", "Niedrig", "Laag", "Bajo", "Basso", "Düşük"}},
    {"Modéré", {"Moderate", "Mäßig", "Matig", "Moderado", "Moderato", "Orta"}},
    {"Élevé", {"High", "Hoch", "Hoog", "Alto", "Alto", "Yüksek"}},
    {"Très élevé", {"Very high", "Sehr hoch", "Zeer hoog", "Muy alto", "Molto alto", "Çok yüksek"}},
    {"Extrême", {"Extreme", "Extrem", "Extreem", "Extremo", "Estremo", "Aşırı"}},
    // --- alertes (l'orage reprend le libellé météo « Orage » de la table commune) ---
    {"Gel", {"Frost", "Frost", "Vorst", "Helada", "Gelo", "Don"}},
    {"Forte chaleur", {"Heat", "Hitze", "Hitte", "Calor", "Caldo", "Sıcak"}},
    {"Vent fort", {"Strong wind", "Starker Wind", "Harde wind", "Viento fuerte", "Vento forte", "Kuvvetli rüzgâr"}},
    {"Pluie forte", {"Heavy rain", "Starkregen", "Zware regen", "Lluvia fuerte", "Pioggia forte", "Şiddetli yağmur"}},
    // --- directions du vent (d'où il vient), abréviations françaises en clé ---
    {"N", {"N", "N", "N", "N", "N", "K"}},
    {"NE", {"NE", "NO", "NO", "NE", "NE", "KD"}},
    {"E", {"E", "O", "O", "E", "E", "D"}},
    {"SE", {"SE", "SO", "ZO", "SE", "SE", "GD"}},
    {"S", {"S", "S", "Z", "S", "S", "G"}},
    {"SO", {"SW", "SW", "ZW", "SO", "SO", "GB"}},
    {"O", {"W", "W", "W", "O", "O", "B"}},
    {"NO", {"NW", "NW", "NW", "NO", "NO", "KB"}},
    // --- phases de la lune ---
    {"Nouvelle lune", {"New moon", "Neumond", "Nieuwe maan", "Luna nueva", "Luna nuova", "Yeni ay"}},
    {"Premier croissant",
     {"Waxing crescent", "Zunehmende Sichel", "Wassende sikkel", "Luna creciente", "Luna crescente", "Büyüyen hilal"}},
    {"Premier quartier",
     {"First quarter", "Erstes Viertel", "Eerste kwartier", "Cuarto creciente", "Primo quarto", "İlk dördün"}},
    {"Gibbeuse croissante",
     {"Waxing gibbous", "Zunehmender Mond", "Wassende maan", "Gibosa creciente", "Gibbosa crescente",
      "Büyüyen şişkin ay"}},
    {"Pleine lune", {"Full moon", "Vollmond", "Volle maan", "Luna llena", "Luna piena", "Dolunay"}},
    {"Gibbeuse décroissante",
     {"Waning gibbous", "Abnehmender Mond", "Afnemende maan", "Gibosa menguante", "Gibbosa calante",
      "Küçülen şişkin ay"}},
    {"Dernier quartier",
     {"Last quarter", "Letztes Viertel", "Laatste kwartier", "Cuarto menguante", "Ultimo quarto", "Son dördün"}},
    {"Dernier croissant",
     {"Waning crescent", "Abnehmende Sichel", "Afnemende sikkel", "Luna menguante", "Luna calante", "Küçülen hilal"}},
};

inline constexpr std::size_t NOMBRE_TEXTES = sizeof(TEXTES) / sizeof(TEXTES[0]);

// Les phrases de l'ACCUEIL (ecran/accueil.hpp, main/ui_principal.cpp) : plus longues que les étiquettes ci-dessus (des phrases posées
// dans le bandeau, pas sur de petites cartes), d'où leur table à part et leur propre limite de longueur dans le test. `%s` : une heure
// (« 13 h », « 13:00 ») ou une température déjà écrite (« 9° »), mise en forme par l'appelant.
inline constexpr Texte PHRASES[] = {
    {"Pluie dès %s",
     {"Rain from %s", "Regen ab %s", "Regen vanaf %s", "Lluvia desde %s", "Pioggia dalle %s", "%s sonrası yağmur"}},
    {"Pluie dans %d min",
     {"Rain in %d min", "Regen in %d Min.", "Regen over %d min", "Lluvia en %d min", "Pioggia tra %d min",
      "%d dk sonra yağmur"}},
    {"Pluie en cours",
     {"Raining now", "Es regnet", "Het regent", "Está lloviendo", "Sta piovendo", "Şu an yağmur yağıyor"}},
    {"ressenti %s", {"feels like %s", "gefühlt %s", "voelt als %s", "sensación %s", "percepita %s", "hissedilen %s"}},
    {"Données de %s · périmées",
     {"Data from %s · outdated", "Daten von %s · veraltet", "Gegevens van %s · verouderd", "Datos de %s · antiguos",
      "Dati delle %s · vecchi", "%s verileri · eski"}},
    {"Rien à signaler dans les prochaines heures",
     {"Nothing to report in the next hours", "Nichts zu melden in den nächsten Stunden",
      "Niets te melden de komende uren", "Nada que señalar en las próximas horas",
      "Niente da segnalare nelle prossime ore", "Önümüzdeki saatlerde bildirilecek bir şey yok"}},
};

inline constexpr std::size_t NOMBRE_PHRASES = sizeof(PHRASES) / sizeof(PHRASES[0]);

// Texte dans la langue : cette table, sinon la table commune (repli : anglais, puis français). Le résultat est terminé par un zéro
// quand `francais` est un littéral (cas normal).
constexpr std::string_view texte(std::string_view francais, i18n::Langue langue) noexcept {
    if (langue == i18n::Langue::Fr)
        return francais;
    for (const Texte &x : TEXTES) {
        if (x.fr == francais) {
            const std::string_view propre = x.t[i18n::indice(langue) - 1];
            return propre.empty() ? x.t[0] : propre;
        }
    }
    for (const Texte &x : PHRASES) {
        if (x.fr == francais) {
            const std::string_view propre = x.t[i18n::indice(langue) - 1];
            return propre.empty() ? x.t[0] : propre;
        }
    }
    return i18n::tr(francais, langue);
}

inline std::string_view texte(std::string_view francais) noexcept { return texte(francais, i18n::langue_courante()); }

inline const char *texte_c(const char *francais) noexcept { return texte(std::string_view(francais)).data(); }

} // namespace ecran::visuel
