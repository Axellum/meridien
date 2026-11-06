// Scène « principal » : l'écran principal de la phase 2 (main/ui_principal.cpp), compilé tel quel. Grande horloge, date, bloc
// météo, en paysage. Données fixées à la place du temps réel et de la météo.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/horloge.cpp
// rendu-langues: fr
// rendu-tailles: 1280x720 1024x600
// rendu-sequence: principal
//
// L'écran est dessiné pour le paysage 1280x720 (le Tab5 tourné par le PPA) et mis à l'échelle sur les autres tailles (1024x600 de la
// 7B) : la 7B n'a pas encore été essayée, ce rendu n'est qu'une simulation de sa taille.
//
// Fixtures (inventées, pas des mesures) : la météo de chaque variante. Les libellés viennent du vrai noyau pur
// (meteo_wmo.hpp puis tr()), comme ceux que la session météo met dans MeteoEtat::libelle ; l'heure et la date viennent du vrai
// fuseau Europe/Paris (horloge.cpp) à partir de l'instant fixé par le banc. Les textes propres à l'écran (« Données périmées »,
// « Météo indisponible »...) sont ceux de l'écran : on ne les recopie pas ici. L'écran n'est pas encore traduit (français).
//
// Les cas pires et la mention « Exemple » reprennent les variantes écrites par la session ecran-principal pour son écran ; son
// essai d'un libellé sans zéro final (hors contrat) n'est pas rendu ici : ce n'est pas un état de l'écran à valider.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "banc.h"
#include "ecran/i18n.hpp"
#include "ecran/meteo_wmo.hpp"
#include "horloge.h"
#include "lvgl.h"
#include "ui_principal.h"

// La police du firmware (symboles posés par le banc, comme EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

constexpr int64_t kDouzeHeures = 12 * 3600;

// Ce que la session météo écrirait dans MeteoEtat pour un code WMO : libellé court, déjà traduit, jamais un code.
MeteoEtat meteo(int code_wmo, bool jour, float temperature_c, float min_c, float max_c, bool perime) {
    MeteoEtat m = {};
    m.valide = true;
    m.perime = perime;
    m.maj_epoch_s = 0;
    m.temperature_c = temperature_c;
    m.code_wmo = code_wmo;
    m.jour = jour;
    m.min_c = min_c;
    m.max_c = max_c;
    const std::string_view libelle = ecran::i18n::tr(ecran::meteo::decrire(code_wmo, jour).libelle);
    const std::size_t n = std::min(libelle.size(), sizeof m.libelle - 1);
    std::memcpy(m.libelle, libelle.data(), n);
    m.libelle[n] = '\0';
    return m;
}

// Ce que ecran_principal.cpp (le branchement sur l'appareil) prépare avant d'appeler l'écran, avec l'heure du réseau connue
// (ou pas). `date` pointe sur un tampon statique : l'écran copie les textes pendant l'appel, le tampon n'a qu'à survivre jusque-là.
ecran::EntreesPrincipal entrees(const banc::Contexte &contexte, int64_t decalage_s, const MeteoEtat &etat_meteo,
                                bool heure_connue) {
    static ecran::TexteHorloge horloge;
    horloge = {};
    const int64_t instant = contexte.instant_utc_s + decalage_s;
    ecran::EntreesPrincipal e = {};
    e.heure_connue = heure_connue && ecran::horloge_formater(instant, horloge) &&
                     ecran::horloge_hhmm(instant, e.heure, sizeof e.heure);
    e.date = horloge.date;
    e.date_attente = nullptr; // la phrase d'attente de l'écran lui-même
    e.meteo = etat_meteo;
    e.meteo_exemple = false;
    return e;
}

void afficher(const banc::Contexte &contexte, const ecran::EntreesPrincipal &e) {
    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const std::size_t taille_police = static_cast<std::size_t>(roboto_700_end - roboto_700_start);
    if (!ecran::ui_principal_creer(ecran_lvgl, roboto_700_start, taille_police, contexte.largeur, contexte.hauteur)) {
        return; // police non créée : l'écran reste vide, le PNG le montrera
    }
    ecran::ui_principal_mettre_a_jour(e);
    lv_screen_load(ecran_lvgl);
}

// Journée ordinaire : partiellement nuageux, 12 °C.
void jour(const banc::Contexte &contexte) {
    afficher(contexte, entrees(contexte, 0, meteo(2, true, 12.4F, 8.1F, 14.6F, false), true));
}

// Pluie modérée.
void pluie(const banc::Contexte &contexte) {
    afficher(contexte, entrees(contexte, 0, meteo(63, true, 8.2F, 6.5F, 9.8F, false), true));
}

// Le soir : l'heure locale passe à 23 h 40 (instant + 12 h), ciel dégagé, température négative pour voir le signe.
void nuit(const banc::Contexte &contexte) {
    afficher(contexte, entrees(contexte, kDouzeHeures, meteo(0, false, -2.6F, -4.0F, 3.4F, false), true));
}

// La dernière météo reçue est trop ancienne : l'écran la garde et le dit.
void perimee(const banc::Contexte &contexte) {
    afficher(contexte, entrees(contexte, 0, meteo(3, true, 11.0F, 7.0F, 13.0F, true), true));
}

// Aucune météo reçue (pas de zone choisie, ou réseau absent) : « Météo indisponible ».
void meteo_absente(const banc::Contexte &contexte) { afficher(contexte, entrees(contexte, 0, MeteoEtat{}, true)); }

// Les premières secondes : pas d'heure du réseau (« --:-- » et phrase d'attente), pas de météo.
void attente(const banc::Contexte &contexte) { afficher(contexte, entrees(contexte, 0, MeteoEtat{}, false)); }

// Ce que l'appareil montre tant que la météo vient du bouchon du contrat (valeurs d'exemple) : l'écran le dit.
void exemple(const banc::Contexte &contexte) {
    ecran::EntreesPrincipal e = entrees(contexte, 0, meteo(2, true, 12.4F, 8.1F, 14.6F, false), true);
    e.meteo_exemple = true;
    afficher(contexte, e);
}

// Pires cas de largeur : chiffres larges, date la plus longue, libellé de 39 octets, températures négatives, météo périmée.
void pires_cas(const banc::Contexte &contexte) {
    ecran::EntreesPrincipal e = entrees(contexte, 0, meteo(2, true, -12.4F, -15.2F, -3.0F, true), true);
    std::snprintf(e.heure, sizeof e.heure, "%s", "20:08");
    e.date = "mercredi 30 septembre 2026";
    std::snprintf(e.meteo.libelle, sizeof e.meteo.libelle, "%s", "Orage avec grêle et pluie verglaçante");
    afficher(contexte, e);
}

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"jour", "Jour : heure et météo à jour", jour},         {"pluie", "Pluie", pluie},
    {"nuit", "Nuit, température négative", nuit},           {"perimee", "Météo périmée", perimee},
    {"meteo-absente", "Aucune météo reçue", meteo_absente}, {"attente", "Au démarrage : heure en attente", attente},
    {"exemple", "Météo d'exemple (bouchon)", exemple},      {"pires-cas", "Pires cas de largeur", pires_cas},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
