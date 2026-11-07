// Noyau pur : les mots PROPRES à l'écran des alertes, en sept langues (fr, en, de, nl, es, it, tr), et la mise en forme des instants.
// C++17 standard, aucun include ESP-IDF ni LVGL, évaluable à la compilation (test/host/test_vigie_textes.cpp).
//
// À ne pas confondre avec les mots de l'ÉMETTEUR : titre, description, consigne, niveau et phénomène d'origine arrivent de Vigie dans les
// langues de l'émetteur et ne passent jamais par ce fichier. Ici : les étiquettes de la page (« Début », « Fin »…), les mots que l'écran
// dérive du niveau et du phénomène de Vigie quand l'émetteur n'a rien dit de dessinable, et les phrases fixes (avertissement, couverture).
//
// Comme visuel_textes.hpp : le texte FRANÇAIS est la clé, repli sur l'anglais puis sur le français, table propre à ce sujet tant que
// l'écran n'est pas stabilisé (migration possible dans i18n/*.yaml, même clé, mêmes traductions). NON RELUS par un locuteur natif.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ecran/heure_fr.hpp"
#include "ecran/i18n.hpp"
#include "ecran/vigie_alertes.hpp"
#include "ecran/visuel_dates.hpp"

namespace ecran::vigie {

struct Mot {
    std::string_view fr;
    std::string_view t[i18n::NOMBRE_LANGUES - 1]; // en, de, nl, es, it, tr
};

inline constexpr Mot MOTS[] = {
    // --- phénomènes (mots de Vigie, pas de l'émetteur) ---
    {"Alerte météo",
     {"Weather alert", "Wetterwarnung", "Weerwaarschuwing", "Aviso meteorológico", "Allerta meteo", "Hava uyarısı"}},
    {"Vent", {"Wind", "Wind", "Wind", "Viento", "Vento", "Rüzgâr"}},
    {"Pluie", {"Rain", "Regen", "Regen", "Lluvia", "Pioggia", "Yağmur"}},
    {"Orage", {"Thunderstorm", "Gewitter", "Onweer", "Tormenta", "Temporale", "Fırtına"}},
    {"Neige et verglas",
     {"Snow and ice", "Schnee und Eis", "Sneeuw en ijs", "Nieve y hielo", "Neve e ghiaccio", "Kar ve buz"}},
    {"Chaleur", {"Heat", "Hitze", "Hitte", "Calor", "Caldo", "Sıcak"}},
    {"Froid", {"Cold", "Kälte", "Kou", "Frío", "Freddo", "Soğuk"}},
    {"Crue", {"Flood", "Hochwasser", "Hoogwater", "Crecida", "Piena", "Sel"}},
    {"Avalanche", {"Avalanche", "Lawine", "Lawine", "Avalancha", "Valanga", "Çığ"}},
    {"Littoral", {"Coast", "Küste", "Kust", "Litoral", "Costa", "Kıyı"}},
    {"Incendie", {"Fire", "Brand", "Brand", "Incendio", "Incendio", "Yangın"}},
    {"Brouillard", {"Fog", "Nebel", "Mist", "Niebla", "Nebbia", "Sis"}},
    {"Alerte", {"Alert", "Warnung", "Waarschuwing", "Aviso", "Allerta", "Uyarı"}},
    // --- niveaux (mots de Vigie) ---
    {"Jaune", {"Yellow", "Gelb", "Geel", "Amarillo", "Giallo", "Sarı"}},
    {"Orange", {"Orange", "Orange", "Oranje", "Naranja", "Arancione", "Turuncu"}},
    {"Rouge", {"Red", "Rot", "Rood", "Rojo", "Rosso", "Kırmızı"}},
    // --- étiquettes de la page ---
    {"Alertes officielles",
     {"Official warnings", "Amtliche Warnungen", "Officiële waarschuwingen", "Avisos oficiales", "Allerte ufficiali",
      "Resmî uyarılar"}},
    {"Niveau", {"Level", "Stufe", "Niveau", "Nivel", "Livello", "Seviye"}},
    {"Phénomène", {"Phenomenon", "Phänomen", "Verschijnsel", "Fenómeno", "Fenomeno", "Olay"}},
    {"Début", {"Start", "Beginn", "Begin", "Inicio", "Inizio", "Başlangıç"}},
    {"Fin", {"End", "Ende", "Einde", "Fin", "Fine", "Bitiş"}},
    {"Zone", {"Area", "Gebiet", "Gebied", "Zona", "Zona", "Bölge"}},
    {"Émetteur", {"Issued by", "Herausgeber", "Uitgever", "Emisor", "Emittente", "Yayıncı"}},
    {"Que faire", {"What to do", "Was tun", "Wat te doen", "Qué hacer", "Cosa fare", "Ne yapmalı"}},
    {"Sources", {"Sources", "Quellen", "Bronnen", "Fuentes", "Fonti", "Kaynaklar"}},
    {"Non communiquée",
     {"Not given", "Nicht angegeben", "Niet opgegeven", "No indicada", "Non indicata", "Belirtilmedi"}},
    {"Reçu à", {"Received at", "Empfangen um", "Ontvangen om", "Recibido a las", "Ricevuto alle", "Alındığı saat"}},
    {"Fermer", {"Close", "Schließen", "Sluiten", "Cerrar", "Chiudi", "Kapat"}},
    // --- phrases fixes ---
    {"Ceci n'est pas un service d'alerte officiel. Suivez les avis de vos autorités nationales.",
     {"This is not an official warning service. Follow the warnings of your national authorities.",
      "Dies ist kein amtlicher Warndienst. Beachten Sie die Warnungen Ihrer nationalen Behörden.",
      "Dit is geen officiële waarschuwingsdienst. Volg de waarschuwingen van uw nationale autoriteiten.",
      "Este no es un servicio oficial de avisos. Siga los avisos de sus autoridades nacionales.",
      "Questo non è un servizio ufficiale di allerta. Segui le allerte delle autorità nazionali.",
      "Bu resmî bir uyarı hizmeti değildir. Ulusal makamlarınızın uyarılarını izleyin."}},
    {"Certains avertissements de ce lieu ne peuvent pas être rattachés à un point : la liste peut être incomplète.",
     {"Some warnings for this place can't be matched to a point: the list may be incomplete.",
      "Einige Warnungen für diesen Ort lassen sich keinem Punkt zuordnen: Die Liste ist womöglich unvollständig.",
      "Sommige waarschuwingen voor deze plaats kunnen niet aan een punt worden gekoppeld: de lijst is mogelijk "
      "onvolledig.",
      "Algunos avisos de este lugar no se pueden asignar a un punto: la lista puede estar incompleta.",
      "Alcune allerte per questo luogo non si possono associare a un punto: l'elenco potrebbe essere incompleto.",
      "Bu yerdeki bazı uyarılar bir noktayla eşleştirilemiyor: liste eksik olabilir."}},
    {"Des données de cette source ne sont pas à jour.",
     {"Data from this source is not up to date.", "Daten dieser Quelle sind nicht aktuell.",
      "Gegevens van deze bron zijn niet actueel.", "Los datos de esta fuente no están al día.",
      "I dati di questa fonte non sono aggiornati.", "Bu kaynağın verileri güncel değil."}},
};
inline constexpr std::size_t NOMBRE_MOTS = sizeof(MOTS) / sizeof(MOTS[0]);

// Le mot `m` dans `langue` ; repli : anglais, puis la clé française (une traduction vide ne doit pas donner un bandeau vide).
constexpr std::string_view traduction(const Mot &m, i18n::Langue langue) noexcept {
    if (langue == i18n::Langue::Fr)
        return m.fr;
    const std::string_view propre = m.t[i18n::indice(langue) - 1];
    if (!propre.empty())
        return propre;
    const std::string_view anglais = m.t[i18n::indice(i18n::Langue::En) - 1];
    return anglais.empty() ? m.fr : anglais;
}

// Le mot de clé `francais` dans `langue`. Une clé inconnue est rendue telle quelle.
constexpr std::string_view mot(std::string_view francais, i18n::Langue langue) noexcept {
    for (const Mot &m : MOTS) {
        if (m.fr == francais)
            return traduction(m, langue);
    }
    return francais;
}

// Pour printf et LVGL : `mot` rend un `string_view` sur un littéral terminé par un zéro.
inline const char *mot_c(const char *francais) noexcept {
    return mot(std::string_view(francais), i18n::langue_courante()).data();
}

constexpr std::string_view cle_phenomene(Phenomene p) noexcept {
    switch (p) {
    case Phenomene::Vent:
        return "Vent";
    case Phenomene::Pluie:
        return "Pluie";
    case Phenomene::Orage:
        return "Orage";
    case Phenomene::NeigeVerglas:
        return "Neige et verglas";
    case Phenomene::Chaleur:
        return "Chaleur";
    case Phenomene::Froid:
        return "Froid";
    case Phenomene::Crue:
        return "Crue";
    case Phenomene::Avalanche:
        return "Avalanche";
    case Phenomene::Cotier:
        return "Littoral";
    case Phenomene::Incendie:
        return "Incendie";
    case Phenomene::Brouillard:
        return "Brouillard";
    case Phenomene::Autre:
        return "Alerte";
    case Phenomene::Inconnu:
        break;
    }
    return "Alerte météo";
}

constexpr std::string_view cle_niveau(Niveau n) noexcept {
    switch (n) {
    case Niveau::Jaune:
        return "Jaune";
    case Niveau::Orange:
        return "Orange";
    case Niveau::Rouge:
        return "Rouge";
    case Niveau::Vert:
    case Niveau::Inconnu:
        break;
    }
    return {};
}

// Les mots de Vigie pour une alerte, quand l'émetteur n'a rien dit de dessinable : « Pluie · Orange » (le niveau n'est écrit que s'il est
// connu). Rend le nombre d'octets écrits (sans le zéro) ; 0 et un tampon vidé si le tampon est trop petit.
constexpr std::size_t libelle_derive(Phenomene phenomene, Niveau niveau_alerte, i18n::Langue langue, char *sortie,
                                     std::size_t capacite) noexcept {
    visuel::detail::Plume p{sortie, capacite};
    p.texte(mot(cle_phenomene(phenomene), langue));
    const std::string_view niveau = cle_niveau(niveau_alerte);
    if (!niveau.empty()) {
        p.texte(" \xC2\xB7 "); // « · »
        p.texte(mot(niveau, langue));
    }
    return p.finir();
}

// « samedi 8 novembre · 05:59 » (sans l'année) dans la langue de l'écran, d'après l'heure locale déjà calculée. Rend 0 et un tampon vidé
// si la date est invalide ou le tampon trop petit.
constexpr std::size_t formater_instant(i18n::Langue langue, const heure::HeureLocale &h, char *sortie,
                                       std::size_t capacite) noexcept {
    if (sortie == nullptr || capacite == 0)
        return 0;
    if (h.heure < 0 || h.heure > 23 || h.minute < 0 || h.minute > 59) {
        sortie[0] = '\0';
        return 0;
    }
    const std::size_t n = visuel::formater_date(langue, h, sortie, capacite, false);
    if (n == 0)
        return 0;
    visuel::detail::Plume p{sortie, capacite};
    p.n = n;
    p.texte(" \xC2\xB7 ");
    p.car(static_cast<char>('0' + h.heure / 10));
    p.car(static_cast<char>('0' + h.heure % 10));
    p.car(':');
    p.car(static_cast<char>('0' + h.minute / 10));
    p.car(static_cast<char>('0' + h.minute % 10));
    return p.finir();
}

} // namespace ecran::vigie
