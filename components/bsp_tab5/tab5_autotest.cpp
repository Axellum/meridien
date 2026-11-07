// Essai sur carte du materiel du Tab5, SANS DOIGT : luminosite, veille (inactivite et plages horaires dont celle qui passe minuit),
// horloge materielle, audio (haut-parleur, microphones, lecture ET enregistrement simultanes), alimentation. Compile seulement avec
//     .\tools\idf.ps1 "-DTAB5_AUTOTEST_MATERIEL=1" build
// (revenir au binaire normal avec "-DTAB5_AUTOTEST_MATERIEL=0" : le cache de CMake garde la valeur). Sans l'option, ce fichier ne
// contient qu'une fonction vide et n'ajoute rien au binaire.
//
// Chaque controle ecrit UNE ligne au journal serie, reperable par « [AT-MAT] » :
//     [AT-MAT] OK     <identifiant> : <mesure>      le controle est passe
//     [AT-MAT] ECHEC  <identifiant> : <mesure>      le controle a echoue
//     [AT-MAT] MESURE <identifiant> : <valeurs>     un relevé a consigner dans docs/mesures/tab5-materiel.md (pas de verdict)
// et une ligne finale « [AT-MAT] BILAN : n OK, m ECHEC » (puis « FIN » quand tout est rendu). La tache attend 8 s apres « Carte prête »
// (premiere image et luminosite de l'application d'abord), dure environ une minute, puis se supprime.
//
// REVERSIBLE : chaque reglage change est remis comme il etait (luminosite, plages de veille, heure de la puce a +/- 1 s pres, bus
// audio ferme). L'ecran s'eteint de lui-meme quelques secondes pendant l'essai de la veille, puis se rallume : c'est le seul effet
// visible, avec trois bips de 2,5 s a faible niveau pour l'essai des lignes de sortie.
//
// Ce qu'il ne fait PAS : toucher l'ecran (voile de la veille et toucher de reveil : essai physique, docs/mesures/tab5-materiel.md),
// juger ce qu'on entend (un bip se mesure au microphone, pas a l'oreille), et laisser une trace entre deux demarrages.
#include "tab5_internal.h"

#if defined(TAB5_AUTOTEST_MATERIEL) && TAB5_AUTOTEST_MATERIEL

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "ecran/niveau_audio.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

namespace aud = ecran::audio;

constexpr char TAG[] = "autotest_mat";

int s_ok = 0;
int s_ko = 0;

void resultat(bool ok, const char *id, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void resultat(bool ok, const char *id, const char *fmt, ...) {
    char texte[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(texte, sizeof(texte), fmt, args);
    va_end(args);
    if (ok) {
        s_ok++;
        ESP_LOGI(TAG, "[AT-MAT] OK     %s : %s", id, texte);
    } else {
        s_ko++;
        ESP_LOGE(TAG, "[AT-MAT] ECHEC  %s : %s", id, texte);
    }
}

void mesure(const char *id, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void mesure(const char *id, const char *fmt, ...) {
    char texte[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(texte, sizeof(texte), fmt, args);
    va_end(args);
    ESP_LOGI(TAG, "[AT-MAT] MESURE %s : %s", id, texte);
}

void attendre_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void memoire(const char *quand) {
    mesure("memoire", "%s : interne libre %u o, plus gros bloc interne %u o, PSRAM libre %u o", quand,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// --- luminosite --------------------------------------------------------------------------------------------------------------

void essai_luminosite() {
    static const int niveaux[] = {0, 1, 5, 10, 25, 50, 75, 100};
    uint32_t duty[sizeof(niveaux) / sizeof(niveaux[0])] = {};
    char suite[160] = "";
    size_t n = 0;
    bool croissant = true;
    for (size_t i = 0; i < sizeof(niveaux) / sizeof(niveaux[0]); i++) {
        board_set_backlight_percent(niveaux[i]);
        attendre_ms(120);
        duty[i] = board_get_backlight_duty_permille();
        n += static_cast<size_t>(snprintf(suite + n, sizeof(suite) - n, "%d%%=%u‰ ", niveaux[i], (unsigned)duty[i]));
        if (i > 0 && duty[i] < duty[i - 1]) {
            croissant = false;
        }
    }
    mesure("luminosite.courbe", "niveau perçu -> rapport cyclique relu dans le LEDC : %s", suite);
    resultat(duty[0] == 0, "luminosite.eteint", "niveau 0 -> %u ‰", (unsigned)duty[0]);
    resultat(duty[1] >= 10 && duty[1] <= 40, "luminosite.plancher", "niveau 1 -> %u ‰ (plancher attendu ~20 ‰)",
             (unsigned)duty[1]);
    resultat(duty[7] >= 995, "luminosite.plafond", "niveau 100 -> %u ‰", (unsigned)duty[7]);
    resultat(croissant, "luminosite.croissante", "la courbe ne décroît jamais");
    // Le milieu doit etre BIEN en dessous de la moitie du rapport cyclique (loi de puissance 2,5 : 50 % -> ~17,7 %, plus le plancher).
    resultat(duty[5] > 100 && duty[5] < 300, "luminosite.milieu", "niveau 50 -> %u ‰ (attendu entre 100 et 300 ‰)",
             (unsigned)duty[5]);
}

// --- veille ------------------------------------------------------------------------------------------------------------------

int s_minute_fausse = 0;
int s_jour_faux = 0;

bool heure_fausse(int *minute, int *jour) {
    *minute = s_minute_fausse;
    *jour = s_jour_faux;
    return true;
}

// Attend (par pas de 100 ms, jusqu'a `max_ms`) que l'ecran soit dans l'etat voulu ; rend le temps ecoule, -1 s'il n'y est pas arrive.
int attendre_etat(board_screen_state_t voulu, uint32_t max_ms) {
    for (uint32_t t = 0; t <= max_ms; t += 100) {
        if (board_screen_get_state() == voulu) {
            return static_cast<int>(t);
        }
        attendre_ms(100);
    }
    return -1;
}

void essai_veille() {
    board_set_backlight_percent(60);
    attendre_ms(150);
    const uint32_t plein = board_get_backlight_duty_permille();

    // Inactivite : attenuer a 2 s, eteindre a 4 s.
    board_sleep_config_t cfg = {};
    cfg.dim_after_s = 2;
    cfg.off_after_s = 4;
    cfg.relight_after_s = 3;
    cfg.dim_level = 20;
    resultat(board_sleep_configure(&cfg) == ESP_OK, "veille.configure", "réglage 2 s / 4 s accepté");
    board_screen_wake();
    const int t_dim = attendre_etat(BOARD_SCREEN_DIMMED, 3500);
    attendre_ms(150);
    const uint32_t duty_dim = board_get_backlight_duty_permille();
    resultat(t_dim >= 1500 && t_dim <= 3300 && duty_dim > 0 && duty_dim < plein, "veille.attenue",
             "atténué après %d ms (attendu ~2000), rapport cyclique %u ‰ < %u ‰", t_dim, (unsigned)duty_dim,
             (unsigned)plein);
    const int t_off = attendre_etat(BOARD_SCREEN_OFF, 3500);
    attendre_ms(150);
    const uint32_t duty_off = board_get_backlight_duty_permille();
    resultat(t_off >= 0 && duty_off == 0, "veille.eteint",
             "éteint %d ms plus tard (attendu ~2000 de plus), rapport cyclique %u ‰", t_off, (unsigned)duty_off);
    board_screen_wake();
    const int t_on = attendre_etat(BOARD_SCREEN_ON, 1000);
    attendre_ms(150);
    const uint32_t duty_on = board_get_backlight_duty_permille();
    resultat(t_on >= 0 && t_on <= 500 && duty_on == plein, "veille.reveil",
             "rallumé en %d ms, rapport cyclique %u ‰ (avant : %u ‰)", t_on, (unsigned)duty_on, (unsigned)plein);

    // Plages horaires : plage 22:00 -> 06:00 (passe minuit) ; aucune extinction par inactivite ; un toucher recent la suspend.
    memset(&cfg, 0, sizeof(cfg));
    cfg.relight_after_s = 3;
    cfg.dim_level = 20;
    cfg.window_count = 1;
    cfg.windows[0].start_min = 22 * 60;
    cfg.windows[0].end_min = 6 * 60;
    cfg.windows[0].days = 0x7F;
    resultat(board_sleep_configure(&cfg) == ESP_OK, "veille.plage.configure",
             "plage 22:00 -> 06:00 tous les jours acceptée");
    board_sleep_set_local_time_source(heure_fausse);

    struct Cas {
        int minute;
        int jour;
        bool eteint;
        const char *nom;
    };
    // Tous les jours : 23:30 dans la plage, 02:00 aussi (la plage a commence la veille), 12:00 hors plage.
    const Cas tous_les_jours[] = {
        {23 * 60 + 30, 3, true, "23:30"}, {2 * 60, 4, true, "02:00 (après minuit)"}, {12 * 60, 3, false, "12:00"}};
    for (const Cas &c : tous_les_jours) {
        s_minute_fausse = c.minute;
        s_jour_faux = c.jour;
        attendre_ms(3800); // plus que relight_after_s : le « toucher » du cas precedent ne compte plus
        const bool eteint = board_screen_get_state() == BOARD_SCREEN_OFF;
        resultat(eteint == c.eteint, "veille.plage.minuit", "heure fausse %s -> %s (attendu %s)", c.nom,
                 eteint ? "éteint" : "allumé", c.eteint ? "éteint" : "allumé");
    }

    // La plage appartient au jour de DEPART : seulement le vendredi (bit 5) a 22:00 -> 06:00. Samedi 02:00 = suite de vendredi
    // (eteint) ; vendredi 02:00 = suite de jeudi (allume).
    cfg.windows[0].days = 1u << 5;
    board_sleep_configure(&cfg);
    s_minute_fausse = 2 * 60;
    s_jour_faux = 6; // samedi
    attendre_ms(3800);
    resultat(board_screen_get_state() == BOARD_SCREEN_OFF, "veille.plage.jour_depart.samedi",
             "samedi 02:00, plage du vendredi -> %s",
             board_screen_get_state() == BOARD_SCREEN_OFF ? "éteint" : "allumé");
    s_jour_faux = 5; // vendredi
    attendre_ms(3800);
    resultat(board_screen_get_state() != BOARD_SCREEN_OFF, "veille.plage.jour_depart.vendredi",
             "vendredi 02:00, plage du vendredi -> %s",
             board_screen_get_state() != BOARD_SCREEN_OFF ? "allumé" : "éteint");

    // Un toucher dans une plage rallume puis se reteint apres relight_after_s (le toucher est simule par board_screen_wake).
    s_jour_faux = 6;
    attendre_ms(3800);
    board_screen_wake();
    const int t_rallume = attendre_etat(BOARD_SCREEN_ON, 1000);
    const int t_reteint = attendre_etat(BOARD_SCREEN_OFF, 5000);
    resultat(t_rallume >= 0 && t_rallume <= 500 && t_reteint >= 2000 && t_reteint <= 4500, "veille.plage.rallume",
             "rallumé en %d ms puis réteint %d ms plus tard (attendu ~3000)", t_rallume, t_reteint);

    // Retour a l'etat d'avant : plus de plage, plus d'extinction, ecran allume au niveau d'avant.
    memset(&cfg, 0, sizeof(cfg));
    board_sleep_configure(&cfg);
    board_sleep_set_local_time_source(nullptr);
    board_screen_wake();
    attendre_etat(BOARD_SCREEN_ON, 1000);
}

// --- horloge materielle -----------------------------------------------------------------------------------------------------

void essai_rtc() {
    board_rtc_info_t info;
    board_rtc_get_info(&info);
    mesure("rtc.etat",
           "présente=%d perte d'alimentation=%d registre d'état au démarrage=0x%02X reprise du système=%d source=%d "
           "écritures=%u",
           (int)info.present, (int)info.power_lost, (unsigned)info.flag_at_boot, (int)info.resumed_from_rtc,
           (int)info.source, (unsigned)info.writes);
    resultat(info.present, "rtc.presente", "la puce répond en 0x%02X", TAB5_I2C_ADDR_RX8130);
    if (!info.present) {
        return;
    }
    uint8_t trame_origine[7] = {};
    const bool trame_lue = tab5_rtc_raw_read(trame_origine) == ESP_OK;
    int64_t origine = 0;
    const bool origine_lisible = board_rtc_read(&origine) == ESP_OK;
    mesure("rtc.origine", "trame brute %s, heure lue %s (%lld s)", trame_lue ? "lue" : "illisible",
           origine_lisible ? "plausible" : "non exploitable", (long long)origine);
    const int64_t t0 = esp_timer_get_time();

    // Ecriture puis relecture, et la puce doit AVANCER d'une seconde par seconde.
    const int64_t essai = 1800000000; // 2027-01-15 08:00:00 UTC
    int64_t relu = 0;
    resultat(board_rtc_write(essai) == ESP_OK && board_rtc_read(&relu) == ESP_OK && relu >= essai && relu <= essai + 2,
             "rtc.ecriture", "écrit %lld, relu %lld", (long long)essai, (long long)relu);
    attendre_ms(2100);
    int64_t apres = 0;
    resultat(board_rtc_read(&apres) == ESP_OK && apres - relu >= 1 && apres - relu <= 3, "rtc.avance",
             "2,1 s plus tard : +%lld s", (long long)(apres - relu));

    // Changement d'annee, puis 28 -> 29 fevrier d'une annee bissextile : le calendrier de la puce doit suivre.
    const int64_t fin_annee = 1830297598; // 2027-12-31 23:59:58
    int64_t rel = 0;
    board_rtc_write(fin_annee);
    attendre_ms(3200);
    resultat(board_rtc_read(&rel) == ESP_OK && rel - fin_annee >= 2 && rel - fin_annee <= 5, "rtc.nouvel_an",
             "31/12/2027 23:59:58 + 3,2 s -> %lld (attendu ~%lld)", (long long)rel, (long long)(fin_annee + 3));
    const int64_t bissextile = 1835395198; // 2028-02-28 23:59:58
    board_rtc_write(bissextile);
    attendre_ms(3200);
    resultat(board_rtc_read(&rel) == ESP_OK && rel - bissextile >= 2 && rel - bissextile <= 5, "rtc.bissextile",
             "28/02/2028 23:59:58 + 3,2 s -> %lld (attendu ~%lld : le 29 février existe)", (long long)rel,
             (long long)(bissextile + 3));

    // Restitution : l'heure d'origine plus le temps ecoule, ou la trame d'origine telle quelle si elle n'etait pas exploitable.
    const int64_t ecoule = (esp_timer_get_time() - t0 + 500000) / 1000000;
    bool rendu = false;
    if (origine_lisible) {
        int64_t verif = 0;
        rendu = board_rtc_write(origine + ecoule) == ESP_OK && board_rtc_read(&verif) == ESP_OK &&
                verif >= origine + ecoule && verif <= origine + ecoule + 2;
        mesure("rtc.restitution", "heure d'origine + %lld s écoulées -> relu %lld", (long long)ecoule,
               (long long)verif);
    } else if (trame_lue) {
        rendu = tab5_rtc_raw_write(trame_origine) == ESP_OK;
        mesure("rtc.restitution", "trame d'origine remise telle quelle");
    }
    resultat(rendu, "rtc.restituee", "la puce est remise comme elle était (à ±2 s)");
}

// --- alimentation -----------------------------------------------------------------------------------------------------------

const char *nom_batterie(board_battery_state_t e) {
    switch (e) {
    case BOARD_BATTERY_NONE:
        return "aucune";
    case BOARD_BATTERY_IDLE:
        return "au repos";
    case BOARD_BATTERY_CHARGING:
        return "en charge";
    case BOARD_BATTERY_DISCHARGING:
        return "en décharge";
    case BOARD_BATTERY_UNKNOWN:
        break;
    }
    return "inconnue";
}

void essai_alimentation() {
    board_power_info_t info;
    const esp_err_t err = board_power_get(&info);
    resultat(err == ESP_OK && info.monitor_present, "alim.moniteur", "INA226 en 0x%02X : %s, identifiant %s",
             TAB5_I2C_ADDR_INA226, esp_err_to_name(err), info.monitor_present ? "conforme" : "non conforme ou muet");
    int32_t v_min = 100000;
    int32_t v_max = -100000;
    int32_t i_min = 100000;
    int32_t i_max = -100000;
    for (int i = 0; i < 6; i++) {
        board_power_get(&info);
        v_min = info.bus_mv < v_min ? info.bus_mv : v_min;
        v_max = info.bus_mv > v_max ? info.bus_mv : v_max;
        i_min = info.current_ma < i_min ? info.current_ma : i_min;
        i_max = info.current_ma > i_max ? info.current_ma : i_max;
        attendre_ms(200);
    }
    mesure("alim.lecture",
           "connecteur de batterie : %d à %d mV, %d à %d mA, batterie : %s, CHG_STAT=%d, chargeur allumé=%d",
           (int)v_min, (int)v_max, (int)i_min, (int)i_max, nom_batterie(info.battery), (int)info.charge_status_pin,
           (int)info.charger_enabled);
    resultat(v_min >= 0 && v_max <= 20000, "alim.plausible", "tension entre %d et %d mV (0 à 20 V)", (int)v_min,
             (int)v_max);
    resultat(!info.charger_enabled, "alim.chargeur_coupe", "ce micrologiciel n'allume jamais le chargeur");
    // Prémisse d'Axel : « pas de batterie ». Ce n'est pas un controle du code mais une comparaison avec ce que la carte mesure.
    mesure("alim.premisse_sans_batterie", "état « %s » à %d mV chargeur coupé : %s", nom_batterie(info.battery),
           (int)info.bus_mv,
           info.battery == BOARD_BATTERY_NONE
               ? "conforme à « pas de batterie »"
               : "NE CORRESPOND PAS à « pas de batterie » (tension de batterie présente, chargeur coupé)");
    uint8_t r[6] = {};
    const esp_err_t e = tab5_expander_dump(TAB5_EXPANDER_ALIM, r);
    mesure("alim.expandeur_0x44",
           "%s : dir=0x%02X sortie=0x%02X haute-imp=0x%02X état=0x%02X (CHG_EN = bit 7, nCHG_QC_EN = bit 5)",
           esp_err_to_name(e), r[0], r[1], r[2], r[5]);
}

// --- audio ------------------------------------------------------------------------------------------------------------------

void essai_audio() {
    board_audio_info_t info;
    memoire("avant l'audio");
    const esp_err_t err = board_audio_init();
    board_audio_get_info(&info);
    resultat(err == ESP_OK && info.codec_ready && info.sample_rate == 16000, "audio.ouverture",
             "%s, codecs prêts=%d, cadence %u Hz", esp_err_to_name(err), (int)info.codec_ready,
             (unsigned)info.sample_rate);
    mesure("audio.ram", "RAM interne prise par le bus ouvert : %u o ; casque inséré=%d",
           (unsigned)info.internal_ram_taken, (int)info.headphones);
    memoire("bus audio ouvert");
    if (err != ESP_OK) {
        return;
    }
    resultat(!info.amplifier_on, "audio.ampli_coupe", "amplificateur éteint au repos");

    uint8_t id1 = 0;
    uint8_t id2 = 0;
    const esp_err_t e_id = tab5_adc_read_ids(&id1, &id2);
    mesure("audio.es7210", "registres d'identité 0x3D=0x%02X 0x3E=0x%02X (%s)", id1, id2, esp_err_to_name(e_id));

    // Volume : 0 muet, 100 plafond de la carte, 30 par defaut.
    static const int niveaux[] = {0, 1, 30, 100};
    char suite[120] = "";
    size_t n = 0;
    bool volume_ok = true;
    for (size_t i = 0; i < sizeof(niveaux) / sizeof(niveaux[0]); i++) {
        volume_ok = volume_ok && board_audio_set_volume(niveaux[i]) == ESP_OK;
        board_audio_get_info(&info);
        n += static_cast<size_t>(snprintf(suite + n, sizeof(suite) - n, "%d=%d ", niveaux[i], info.volume_db_x10));
    }
    mesure("audio.volume", "niveau -> dixièmes de dB : %s", suite);
    board_audio_get_info(&info);
    resultat(volume_ok && info.volume_db_x10 == -60, "audio.volume.plafond",
             "niveau 100 -> %d (plafond de la carte -60 = -6 dB)", info.volume_db_x10);
    board_audio_set_volume(25);

    // Bruit de fond (micros seuls), puis un bip seul (sans micro), puis la lecture ET l'ecoute en meme temps sur chaque ligne.
    int32_t g_rms = 0;
    int32_t d_rms = 0;
    int32_t g_ton = 0;
    int32_t d_ton = 0;
    const esp_err_t e_bruit = tab5_audio_mic_levels(2000, 1000, &g_rms, &d_rms, &g_ton, &d_ton);
    resultat(e_bruit == ESP_OK, "audio.micro.bruit",
             "%s ; bruit de fond gauche %d / droite %d (dBFS×10), 1 kHz : %d / %d", esp_err_to_name(e_bruit),
             (int)g_rms, (int)d_rms, (int)g_ton, (int)d_ton);
    // Plancher de comparaison : le bruit EFFICACE large bande du micro le plus bruyant (le niveau d'un seul bin de Goertzel sans son
    // tombe sous le plancher de mesure et ne compare rien).
    const int32_t bruit_ton = g_rms > d_rms ? g_rms : d_rms;
    (void)g_ton;
    (void)d_ton;
    resultat(g_rms > -1200 || d_rms > -1200, "audio.micro.vivant", "au moins un microphone n'est pas à zéro absolu");

    const int64_t t_bip = esp_timer_get_time();
    const esp_err_t e_bip = board_audio_play_tone(1000, 400, -200);
    board_audio_get_info(&info);
    resultat(e_bip == ESP_OK && !info.amplifier_on, "audio.bip", "%s en %lld ms, amplificateur recoupé=%d",
             esp_err_to_name(e_bip), (long long)((esp_timer_get_time() - t_bip) / 1000), (int)!info.amplifier_on);

    static const struct {
        tab5_dac_line_t ligne;
        const char *nom;
    } lignes[] = {{TAB5_DAC_BOTH, "LINE1+LINE2"}, {TAB5_DAC_LINE1, "LINE1"}, {TAB5_DAC_LINE2, "LINE2"}};
    tab5_duplex_cfg_t cfg = {};
    cfg.tone_hz = 1000;
    cfg.tone_peak_dbfs_x10 = -200;
    cfg.volume_level = 25;
    cfg.duration_ms = 2500;
    bool duplex_prouve = false;
    int32_t meilleur_ton = aud::kSilence_x10 - 1;
    const char *meilleure_ligne = "aucune";
    for (const auto &l : lignes) {
        tab5_audio_set_line(l.ligne);
        tab5_duplex_result_t r;
        const esp_err_t e = tab5_audio_duplex_probe(&cfg, &r);
        const int32_t ton = r.left_tone_dbfs_x10 > r.right_tone_dbfs_x10 ? r.left_tone_dbfs_x10 : r.right_tone_dbfs_x10;
        const int32_t gain = ton - bruit_ton;
        const bool propre =
            e == ESP_OK && r.tx_overflows == 0 && r.rx_overflows == 0 && r.frames_read + 640 >= r.frames_written;
        mesure("audio.duplex",
               "ligne %s : %s, écrit %u / lu %u trames en %u ms, dépassements tx %u rx %u, bloc le plus lent %u ms, "
               "expandeur 0x43 pendant le son : dir=0x%02X sortie=0x%02X haute-imp=0x%02X",
               l.nom, esp_err_to_name(e), (unsigned)r.frames_written, (unsigned)r.frames_read, (unsigned)r.elapsed_ms,
               (unsigned)r.tx_overflows, (unsigned)r.rx_overflows, (unsigned)r.slowest_block_ms, r.expander43_regs[0],
               r.expander43_regs[1], r.expander43_regs[2]);
        mesure("audio.duplex.niveaux",
               "ligne %s : micro gauche rms %d ton %d, droit rms %d ton %d (dBFS×10) ; ton entendu = plancher %d %+d "
               "(dixièmes de dB)",
               l.nom, (int)r.left_rms_dbfs_x10, (int)r.left_tone_dbfs_x10, (int)r.right_rms_dbfs_x10,
               (int)r.right_tone_dbfs_x10, (int)bruit_ton, (int)gain);
        resultat(propre, "audio.duplex.flux", "ligne %s : lecture et enregistrement sans dépassement ni trou", l.nom);
        // SPEAKER_EN (bit 1 de l'expandeur 0x43) est piloté à 1 si : direction = sortie, niveau de sortie = 1, pas en haute impédance.
        const uint8_t m = static_cast<uint8_t>(TAB5_EXPANDER_SPEAKER_EN);
        const bool ampli_pilote =
            r.expander43_ok && (r.expander43_regs[0] & m) && (r.expander43_regs[1] & m) && !(r.expander43_regs[2] & m);
        resultat(ampli_pilote, "audio.ampli_allume", "ligne %s : SPEAKER_EN %s pendant le son (registres relus : %s)",
                 l.nom, ampli_pilote ? "piloté à 1" : "NON piloté à 1", r.expander43_ok ? "oui" : "illisibles");
        if (propre && gain >= 120) {
            duplex_prouve =
                true; // le bip a ete ENTENDU par les micros pendant que le flux d'entree tournait : la simultaneite marche
        }
        if (l.ligne != TAB5_DAC_BOTH && ton > meilleur_ton) {
            meilleur_ton = ton;
            meilleure_ligne = l.nom;
        }
    }
    tab5_audio_set_line(TAB5_DAC_BOTH);
    mesure("audio.duplex.verdict", "%s",
           duplex_prouve ? "lecture + enregistrement simultanés PROUVÉS (flux propres ET bip entendu par les micros)"
                         : "simultanéité NON PROUVÉE par ce passage (flux propre mais bip non entendu, ou flux troué)");
    mesure("audio.ligne_haut_parleur", "ligne seule la plus entendue par les micros : %s (ton %d dBFS×10, plancher %d)",
           meilleure_ligne, (int)meilleur_ton, (int)bruit_ton);

    // Enregistrement court dans la PSRAM.
    const size_t trames = 16000; // 1 s
    int16_t *pcm = static_cast<int16_t *>(heap_caps_malloc(trames * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pcm == nullptr) {
        resultat(false, "audio.enregistrement", "tampon PSRAM de %u o non alloué", (unsigned)(trames * 4));
    } else {
        size_t lues = 0;
        const esp_err_t e_rec = board_audio_record_pcm(pcm, trames, &lues, 500);
        const aud::Niveau g = aud::mesurer(pcm + 2 * 4000, lues > 4000 ? lues - 4000 : 0, 2);
        const aud::Niveau d = aud::mesurer(pcm + 2 * 4000 + 1, lues > 4000 ? lues - 4000 : 0, 2);
        resultat(e_rec == ESP_OK && lues == trames, "audio.enregistrement",
                 "%s, %u trames lues sur %u ; rms gauche %d / droite %d, crête %d / %d (dBFS×10), écrêtés %u / %u",
                 esp_err_to_name(e_rec), (unsigned)lues, (unsigned)trames, (int)g.rms_dbfs_x10, (int)d.rms_dbfs_x10,
                 (int)g.crete_dbfs_x10, (int)d.crete_dbfs_x10, (unsigned)g.ecretes, (unsigned)d.ecretes);
        heap_caps_free(pcm);
    }

    // Fermeture : la RAM interne prise par le bus doit revenir.
    const size_t avant = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const esp_err_t e_ferme = tab5_audio_close();
    attendre_ms(100);
    const size_t apres = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    memoire("bus audio refermé");
    resultat(e_ferme == ESP_OK && apres >= avant, "audio.fermeture", "RAM interne libre %u -> %u o (rendu : %d o)",
             (unsigned)avant, (unsigned)apres, (int)(apres - avant));
}

// --- tache -------------------------------------------------------------------------------------------------------------------

void tache(void *) {
    attendre_ms(8000); // la premiere image et la luminosite de l'application d'abord
    ESP_LOGI(TAG, "[AT-MAT] DEBUT essai du matériel, binaire du %s %s", __DATE__, __TIME__);
    const int niveau_origine = board_get_backlight_percent();
    memoire("au début");
    for (int w = 0; w < 2; w++) {
        uint8_t r[6] = {};
        const esp_err_t e = tab5_expander_dump(w == 0 ? TAB5_EXPANDER_ECRAN : TAB5_EXPANDER_ALIM, r);
        mesure("expandeur.registres",
               "0x%02X (%s) : dir=0x%02X sortie=0x%02X haute-imp=0x%02X tirage-actif=0x%02X tirage-haut=0x%02X "
               "état=0x%02X",
               w == 0 ? 0x43 : 0x44, esp_err_to_name(e), r[0], r[1], r[2], r[3], r[4], r[5]);
    }

    essai_luminosite();
    essai_veille();
    essai_rtc();
    essai_alimentation();
    essai_audio();

    // Retour : ecran allume au niveau d'avant, rien d'arme.
    board_set_backlight_percent(niveau_origine);
    attendre_ms(200);
    resultat(board_screen_get_state() == BOARD_SCREEN_ON && board_get_backlight_percent() == niveau_origine,
             "fin.ecran", "écran %s, niveau %d%% (avant : %d%%)",
             board_screen_get_state() == BOARD_SCREEN_ON ? "allumé" : "PAS ALLUMÉ", board_get_backlight_percent(),
             niveau_origine);
    memoire("à la fin");
    ESP_LOGI(TAG, "[AT-MAT] BILAN : %d OK, %d ECHEC", s_ok, s_ko);
    ESP_LOGI(TAG, "[AT-MAT] FIN");
    vTaskDelete(nullptr);
}

} // namespace

extern "C" void tab5_autotest_materiel_start(void) {
    if (xTaskCreate(tache, "autotest_mat", 8192, nullptr, 3, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "tâche d'essai non créée");
    }
}

#else

extern "C" void tab5_autotest_materiel_start(void) {}

#endif // TAB5_AUTOTEST_MATERIEL
