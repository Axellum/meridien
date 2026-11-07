"""Mutations de la partie pure de la couche carte de la Waveshare 7B (components/bsp_waveshare_7b/include/ws7b_pur.h), lues par lancer.py.

Même principe que les mutations du noyau (lancer.py --mutations) : une copie du dossier include/ de la couche carte, UNE modification,
et le test visé (test_carte_7b.cpp) DOIT échouer à la compilation, en citant la fonction de test attendue. Une mutation « non
détectée » ou « détectée par autre chose » fait échouer le lanceur.

Chaque mutation simule une erreur qui aurait des conséquences sur la carte : une broche prise pour une autre (ampli sur la broche
du bouton BOOT), un écran qui ne se rallume pas (plus de « sleep out »), un haut-parleur poussé à +32 dB (saturation), un point
tactile qui sort de la dalle, une rotation décalée d'un pixel.
Non couvert : tout ce qui se passe dans les fichiers ws7b_*.c (appels ESP-IDF) et sur la carte elle-même.
"""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

PUR = "ws7b_pur.h"
TEST = "test_carte_7b.cpp"

# (fichier de include/, texte exact à remplacer — il doit apparaître UNE fois —, remplacement, fonction de test qui doit être citée, ce que la mutation simule)
MUTATIONS_CARTE_7B = [
    # --- broches ---
    (PUR, "#define WS7B_PIN_C6_SDIO_CLK 18", "#define WS7B_PIN_C6_SDIO_CLK 19", "broches_valides_et_distinctes", "CLK et CMD du C6 sur la meme broche"),
    (PUR, "#define WS7B_PIN_AMP_CTRL 53", "#define WS7B_PIN_AMP_CTRL 35", "aucune_broche_reservee_n_est_prise", "l'ampli sur la broche du bouton BOOT"),
    (PUR, "#define WS7B_PIN_BACKLIGHT 32", "#define WS7B_PIN_BACKLIGHT 99", "broches_valides_et_distinctes", "retroeclairage sur une broche qui n'existe pas"),
    (PUR, "#define WS7B_PIN_C6_RESET 54", "#define WS7B_PIN_C6_RESET 53", "sdio_du_c6_sur_la_fente_1", "reset du C6 sur la broche de l'ampli"),
    (PUR, "#define WS7B_TOUCH_INT_CONNECTED 0", "#define WS7B_TOUCH_INT_CONNECTED 1", "i2c_et_i2s_valeurs_connues", "INT du tactile declare relie au P4"),
    # --- dalle ---
    (PUR, "#define WS7B_DPI_CLOCK_MHZ 52", "#define WS7B_DPI_CLOCK_MHZ 26", "cadence_de_rafraichissement_proche_de_60_hz", "horloge de pixels divisee par deux"),
    (PUR, "#define WS7B_DSI_LANE_MBPS 900", "#define WS7B_DSI_LANE_MBPS 300", "les_voies_dsi_ont_de_la_marge", "debit de ligne DSI trop faible pour les pixels"),
    (PUR, "{0x84, 1, 0xA8, 0}", "{0x84, 1, 0xA9, 0}", "table_d_initialisation_de_l_ek79007", "un octet de l'initialisation de la dalle change"),
    (PUR, "{WS7B_DCS_SLEEP_OUT, 0, 0, 120}", "{WS7B_DCS_SLEEP_OUT, 0, 0, 0}", "table_d_initialisation_de_l_ek79007", "plus d'attente apres « sleep out »"),
    (PUR, "{0x80, 1, 0x8B, 0},", "{WS7B_DCS_LANE_CONTROL, 1, 0x10, 0},", "la_table_n_emet_ni_b2_ni_extinction", "B2 emise deux fois"),
    # --- rotation et tactile ---
    (PUR, "return degrees == WS7B_ROTATION_AS_SCANNED || degrees == WS7B_ROTATION_FLIPPED;",
     "return degrees == WS7B_ROTATION_AS_SCANNED || degrees == WS7B_ROTATION_FLIPPED || degrees == 90;", "rotations_prises_en_charge",
     "une rotation de 90 degres acceptee sur une dalle deja en paysage"),
    (PUR, "*width = tourne ? WS7B_LCD_V_RES : WS7B_LCD_H_RES;", "*width = WS7B_LCD_H_RES;", "taille_logique_de_l_interface",
     "la largeur logique ne suit pas la rotation"),
    (PUR, "        *ox = panel_w - 1 - x;\n        *oy = panel_h - 1 - y;\n        break;\n    case 270:",
     "        *ox = panel_w - x;\n        *oy = panel_h - 1 - y;\n        break;\n    case 270:", "rotation_de_180_degres_des_coins_et_du_centre",
     "rotation de 180 degres decalee d'un pixel"),
    (PUR, "#define WS7B_GT911_MIRROR_X 1", "#define WS7B_GT911_MIRROR_X 0", "miroirs_du_gt911_gardes_et_pas_d_echange", "miroir X du tactile perdu"),
    (PUR, "#define WS7B_GT911_SWAP_XY 0", "#define WS7B_GT911_SWAP_XY 1", "miroirs_du_gt911_gardes_et_pas_d_echange", "echange X/Y du tactile active"),
    (PUR, "id[2] == '1' && id[3] == 0", "id[2] == '2' && id[3] == 0", "identifiant_produit_du_gt911", "identifiant produit mal lu"),
    (PUR, "return v < lo ? lo : (v > hi ? hi : v);", "return v < lo ? lo : v;", "le_point_tactile_est_ramene_dans_la_dalle", "un point tactile peut sortir de la dalle"),
    (PUR, "WS7B_LCD_H_RES / 2};", "WS7B_LCD_H_RES};",
     "points_de_l_autotest_dans_l_interface_et_retrouves_apres_rotation", "un point d'auto-test hors de la dalle"),
    # --- retroeclairage ---
    (PUR, "const uint32_t max_duty = (1U << bits) - 1U;", "const uint32_t max_duty = (1U << bits);", "rapport_cyclique_du_retroeclairage",
     "rapport cyclique maximal hors de la resolution"),
    (PUR, "#define WS7B_BACKLIGHT_HZ 5000", "#define WS7B_BACKLIGHT_HZ 500", "rapport_cyclique_du_retroeclairage", "frequence du retroeclairage divisee par dix"),
    # --- extinction et reveil ---
    (PUR, "        plan[n].kind = WS7B_STEP_VEIL_ON, plan[n].dcs = 0, plan[n].ms = 0, n++;\n", "", "plan_d_extinction_voile_puis_0x28_puis_0x10",
     "plus de voile noir avant d'eteindre la dalle"),
    (PUR, "plan[n].dcs = WS7B_DCS_DISPLAY_OFF", "plan[n].dcs = WS7B_DCS_DISPLAY_ON", "plan_d_extinction_voile_puis_0x28_puis_0x10",
     "« display on » envoye a la place de « display off »"),
    (PUR, "plan[n].kind = WS7B_STEP_SPACE, plan[n].dcs = 0, plan[n].ms = 0, n++;\n        plan[n].kind = WS7B_STEP_DCS, plan[n].dcs = WS7B_DCS_SLEEP_IN",
     "plan[n].kind = WS7B_STEP_DCS, plan[n].dcs = WS7B_DCS_SLEEP_IN", "deux_changements_de_veille_sont_toujours_espaces",
     "« sleep in » sans attendre 120 ms apres le dernier changement de veille"),
    (PUR, "plan[n].kind = WS7B_STEP_WAIT, plan[n].dcs = 0, plan[n].ms = WS7B_PANEL_SLEEP_SPACING_MS, n++;\n", "",
     "plan_de_reveil_0x11_120_ms_0x29_puis_voile_retire", "pas d'attente de 120 ms entre « sleep out » et « display on »"),
    (PUR, "plan[n].kind = WS7B_STEP_VEIL_OFF, plan[n].dcs = 0, plan[n].ms = 0, n++;\n", "", "le_voile_couvre_toute_la_veille",
     "le voile noir n'est jamais retire : l'ecran reste noir au reveil"),
    (PUR, "if (!!is_on == !!want_on) {\n        return 0;\n    }", "if (false) {\n        return 0;\n    }", "rien_a_faire_si_l_etat_est_deja_le_bon",
     "le plan d'extinction rejoue quand la dalle est deja eteinte"),
    (PUR, "return elapsed >= spacing_ms ? 0U : spacing_ms - elapsed;", "return spacing_ms - elapsed;", "attente_restante_de_l_espacement",
     "attente restante negative (modulo) apres l'espacement"),
    (PUR, "const uint32_t elapsed = now_ms - last_ms; // modulo 2^32 voulu", "const uint32_t elapsed = now_ms > last_ms ? now_ms - last_ms : 0;",
     "attente_restante_sur_le_passage_a_zero_du_compteur", "espacement faux au passage a zero du compteur de millisecondes"),
    # --- audio ---
    (PUR, "#define WS7B_ES8311_REG_0DB 0xBF", "#define WS7B_ES8311_REG_0DB 0xFF", "le_volume_ne_depasse_jamais_zero_db",
     "plafond du volume a +32 dB : le haut-parleur sature"),
    (PUR, "    if (percent <= 0) {\n        return 0;\n    }\n    if (percent > 100) {\n        percent = 100;\n    }\n    const int span",
     "    if (percent > 100) {\n        percent = 100;\n    }\n    const int span", "volume_zero_coupe_et_cent_vaut_zero_db",
     "un volume de zero ne coupe plus le son"),
    (PUR, "#define WS7B_ES8311_REG_FLOOR 0x66", "#define WS7B_ES8311_REG_FLOOR 0x30", "plancher_audible_a_40_pour_cent_de_l_echelle",
     "plancher du volume trop bas : plus rien d'audible"),
    (PUR, "return ((int)reg - WS7B_ES8311_REG_0DB) * 5;", "return ((int)reg - WS7B_ES8311_REG_0DB) * 4;", "echelle_du_registre_de_l_es8311",
     "pas de 0,5 dB mal converti"),
    # --- memoire ---
    (PUR, "#define WS7B_DRAW_BUFFER_LINES_DEFAULT 25", "#define WS7B_DRAW_BUFFER_LINES_DEFAULT 50", "tampon_de_dessin_dans_le_budget_du_tab5",
     "tampon de dessin plus gros que le budget de memoire interne"),
]


def _commande(cxx: str, mode: str, drapeaux: list[str], source: Path, racine: Path, include_noyau: Path, include_carte: Path, sortie: Path):
    cmd = [cxx, *drapeaux, "-I", str(include_noyau), "-I", str(racine / "test" / "host"), "-I", str(include_carte)]
    if mode == "syntaxe":
        return cmd + ["-fsyntax-only", str(source)]
    return cmd + [str(source), "-o", str(sortie)]


def mutations_carte_7b(cxx: str, mode: str, racine: Path, drapeaux: list[str], include_noyau: Path) -> int:
    """Rend 0 si chaque mutation est détectée par le bon test, 1 sinon (avec le détail)."""
    source = racine / "test" / "host" / TEST
    include_carte = racine / "components" / "bsp_waveshare_7b" / "include"
    mauvais = 0
    with tempfile.TemporaryDirectory() as tmp:
        base = subprocess.run(_commande(cxx, mode, drapeaux, source, racine, include_noyau, include_carte, Path(tmp) / "base.exe"),
                              capture_output=True, text=True, encoding="utf-8", errors="replace")
        if base.returncode != 0:
            print(f"BASE NON SAINE : {TEST} ne compile pas sans mutation\n{base.stderr[:800]}")
            return 1
        for i, (fichier, ancien, nouveau, attendu, quoi) in enumerate(MUTATIONS_CARTE_7B, 1):
            copie = Path(tmp) / f"carte_{i}"
            shutil.copytree(include_carte, copie)
            cible = copie / fichier
            texte = cible.read_text(encoding="utf-8")
            if texte.count(ancien) != 1:
                print(f"[7B {i:2}] MUTATION INAPPLICABLE ({texte.count(ancien)} occurrence(s) de son texte) : {quoi}")
                mauvais += 1
                continue
            cible.write_text(texte.replace(ancien, nouveau), encoding="utf-8", newline="\n")
            r = subprocess.run(_commande(cxx, mode, drapeaux, source, racine, include_noyau, copie, Path(tmp) / f"m{i}.exe"),
                               capture_output=True, text=True, encoding="utf-8", errors="replace")
            sortie = r.stdout + r.stderr
            if r.returncode == 0:
                print(f"[7B {i:2}] NON DÉTECTÉE : {quoi}  ({TEST})")
                mauvais += 1
            elif attendu not in sortie:
                print(f"[7B {i:2}] DÉTECTÉE PAR AUTRE CHOSE (le test {attendu} n'est pas en cause) : {quoi}")
                mauvais += 1
            else:
                print(f"[7B {i:2}] détectée par {attendu} : {quoi}")
    print(f"\n{len(MUTATIONS_CARTE_7B)} mutations de la couche carte 7B, {mauvais} problème(s)")
    return 1 if mauvais else 0
