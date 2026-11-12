// Scène « fuseaux » : l'écran principal (main/ui_principal.cpp, compilé tel quel) pour d'autres lieux que Paris (ADR-0012) : Tokyo
// (+9), Kolkata (+5:30), Katmandou (+5:45), puis deux fenêtres « heure par heure » qui traversent un changement d'heure (New York le
// 1er novembre 2026, Sydney le 4 octobre 2026). L'horloge lit la zone posée par le banc (banc::definir_zone) comme l'appareil lit la
// zone enregistrée : c'est main/horloge.cpp, inchangé, qui donne l'heure, la date et le fuseau.
//
// Directives lues par tools/rendu/rendre.py (une scène = ce seul fichier ; voir docs/outils.md) :
// rendu-sources: main/ui_principal.cpp main/ui_principal_outils.cpp main/ui_principal_donnees.cpp main/ui_principal_disposition.cpp main/ui_principal_barre.cpp main/ui_principal_horloge.cpp main/ui_principal_voiles.cpp main/ui_principal_carte_heure.cpp main/ui_principal_heures.cpp main/ui_principal_bandeau.cpp main/ui_principal_mode_auto.cpp main/ui_principal_jours.cpp main/ui_principal_dessin.cpp main/ui_principal_toucher.cpp main/ui_principal_ciel.cpp main/ui_meteo_icones.cpp main/horloge.cpp
// rendu-langues: fr en
// rendu-langues-defaut: fr
// rendu-tailles: 1024x600 1280x720
// rendu-sequence: fuseaux
//
// L'instant de chaque variante est FIXÉ ICI (une date de changement d'heure ne se règle pas avec --instant) et dit dans sa légende.
// Les valeurs météo sont celles, inventées, de scene_principal_donnees.hpp ; seul ce qui dépend du lieu est recalculé : le lever et le
// coucher du soleil (noyau pur, NOAA), le décalage de la réponse, les instants des colonnes « heure par heure ».
//
// Deux familles de colonnes : « alignées sur l'heure locale » (ce que renvoie Open-Meteo : mesuré sur ses réponses, les instants
// tombent à :30 UTC à Kolkata, à :15 UTC à Katmandou, donc à l'heure pile locale) et « alignées sur l'heure UTC » (une source qui
// donnerait des instants ronds en UTC) : les colonnes tombent alors à :30 ou :45 locales et l'étiquette passe au format « 14:30 ».
// Ce rendu est une simulation sur le PC (docs/outils.md) : ce n'est pas la dalle.
#include <cstddef>
#include <cstdint>

#include "banc.h"
#include "ecran/heure_lieu.hpp"
#include "ecran/soleil.hpp"
#include "lvgl.h"
#include "scenes/scene_principal_donnees.hpp"
#include "ui_principal.h"

extern const uint8_t roboto_700_start[] asm("_binary_roboto_700_ttf_start");
extern const uint8_t roboto_700_end[] asm("_binary_roboto_700_ttf_end");

namespace {

using namespace donnees;

struct Lieu {
    const char *ville;
    float latitude;
    float longitude;
    const char *fuseau;
    int64_t instant_utc_s; // l'instant de la scène
    bool colonnes_en_heure_utc;
};

// 2026-11-06 04:40 UTC = 13 h 40 à Tokyo.
constexpr Lieu kTokyo = {"Tokyo", 35.6762F, 139.6503F, "Asia/Tokyo", 1793940000LL, false};
// 2026-11-06 08:10 UTC = 13 h 40 à Kolkata.
constexpr Lieu kKolkata = {"Kolkata", 22.5726F, 88.3639F, "Asia/Kolkata", 1793952600LL, false};
constexpr Lieu kKolkataUtc = {"Kolkata", 22.5726F, 88.3639F, "Asia/Kolkata", 1793952600LL, true};
// 2026-11-06 08:00 UTC = 13 h 45 à Katmandou.
constexpr Lieu kKatmandou = {"Katmandou", 27.7172F, 85.3240F, "Asia/Kathmandu", 1793952000LL, false};
constexpr Lieu kKatmandouUtc = {"Katmandou", 27.7172F, 85.3240F, "Asia/Kathmandu", 1793952000LL, true};
// 2026-11-01 04:40 UTC = dimanche 1er novembre 0 h 40 à New York (heure d'été) ; à 06:00 UTC les horloges reculent : 1 h EDT, 1 h EST.
constexpr Lieu kNewYork = {"New York", 40.7128F, -74.0060F, "America/New_York", 1793508000LL, false};
// 2026-10-03 14:40 UTC = dimanche 4 octobre 0 h 40 à Sydney (heure d'hiver) ; à 16:00 UTC les horloges avancent : 1 h puis 3 h.
constexpr Lieu kSydney = {"Sydney", -33.8688F, 151.2093F, "Australia/Sydney", 1791038400LL, false};

// Vrai si l'instant tombe entre le lever et le coucher du soleil du jour local du lieu (jour polaire : toujours vrai).
bool est_jour(const Lieu &l, int64_t instant, const ecran::heure::Fuseau &f) {
    const ecran::heure::HeureLocale local = ecran::heure::en_heure_locale(instant, f);
    const ecran::soleil::LeverCoucher lc =
        ecran::soleil::lever_coucher(l.latitude, l.longitude, local.an, local.mois, local.jour, local.decalage_s);
    if (lc.situation == ecran::soleil::Situation::Normal) {
        return instant >= lc.lever_s && instant < lc.coucher_s;
    }
    return lc.situation == ecran::soleil::Situation::JourPolaire;
}

void afficher_lieu(const banc::Contexte &c, const Lieu &l) {
    banc::definir_zone(l.ville, l.latitude, l.longitude, l.fuseau);
    ecran::heure::Fuseau fuseau;
    if (!ecran::horloge_fuseau(fuseau)) {
        return; // fuseau hors de la table : l'écran reste vide, le PNG le montrera
    }
    const int64_t maintenant = l.instant_utc_s;
    const ecran::heure::HeureLocale local = ecran::heure::en_heure_locale(maintenant, fuseau);

    Cas cas = SOLEIL;
    cas.ville = l.ville;
    VueMeteo v = vue(cas, maintenant);
    v.decalage_utc_s = local.decalage_s;
    const ecran::soleil::LeverCoucher lc =
        ecran::soleil::lever_coucher(l.latitude, l.longitude, local.an, local.mois, local.jour, local.decalage_s);
    v.lever_s = lc.situation == ecran::soleil::Situation::Normal ? lc.lever_s : 0;
    v.coucher_s = lc.situation == ecran::soleil::Situation::Normal ? lc.coucher_s : 0;
    v.jour = est_jour(l, maintenant, fuseau);

    // Les 24 colonnes : début de l'heure LOCALE en cours (ou de l'heure UTC), puis une heure de plus chacune.
    const int64_t debut =
        l.colonnes_en_heure_utc ? maintenant - maintenant % 3600 : maintenant - (local.minute * 60 + local.seconde);
    for (int i = 0; i < v.nb_heures; ++i) {
        v.heures[i].instant_s = debut + i * 3600LL;
        v.heures[i].jour = est_jour(l, v.heures[i].instant_s, fuseau);
    }

    lv_obj_t *ecran_lvgl = lv_obj_create(nullptr);
    const ecran::RessourcesPrincipal ressources = {roboto_700_start,
                                                   static_cast<std::size_t>(roboto_700_end - roboto_700_start)};
    if (!ecran::ui_principal_creer(ecran_lvgl, ressources, c.largeur, c.hauteur)) {
        return; // police non créée : l'écran reste vide, le PNG le montrera
    }
    ecran::ui_principal_mettre_a_jour(entrees(c, maintenant - c.instant_utc_s, v));
    lv_screen_load(ecran_lvgl);
}

void tokyo(const banc::Contexte &c) { afficher_lieu(c, kTokyo); }
void kolkata(const banc::Contexte &c) { afficher_lieu(c, kKolkata); }
void kolkata_utc(const banc::Contexte &c) { afficher_lieu(c, kKolkataUtc); }
void katmandou(const banc::Contexte &c) { afficher_lieu(c, kKatmandou); }
void katmandou_utc(const banc::Contexte &c) { afficher_lieu(c, kKatmandouUtc); }
void new_york(const banc::Contexte &c) { afficher_lieu(c, kNewYork); }
void sydney(const banc::Contexte &c) { afficher_lieu(c, kSydney); }

} // namespace

namespace banc {

const Scene kScenes[] = {
    {"tokyo", "Tokyo, +9 h, le 6 novembre à 13 h 40", tokyo},
    {"kolkata", "Kolkata, +5 h 30, colonnes à l'heure pile locale", kolkata},
    {"kolkata-instants-utc", "Kolkata, colonnes à l'heure ronde UTC (:30 locales)", kolkata_utc},
    {"katmandou", "Katmandou, +5 h 45, colonnes à l'heure pile locale", katmandou},
    {"katmandou-instants-utc", "Katmandou, colonnes à l'heure ronde UTC (:45 locales)", katmandou_utc},
    {"new-york-changement-d-heure", "New York, 1er novembre : 1 h deux fois (fin de l'heure d'été)", new_york},
    {"sydney-changement-d-heure", "Sydney, 4 octobre : 2 h n'existe pas (début de l'heure d'été)", sydney},
};
const std::size_t kNombreScenes = sizeof(kScenes) / sizeof(kScenes[0]);

} // namespace banc
