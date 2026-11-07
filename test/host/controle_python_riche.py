#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des tests de la météo riche (components/core/include/ecran/meteo_riche.hpp).

    python test/host/controle_python_riche.py              # (ré)génère donnees/openmeteo_riche_*.inc et attendu_riche_*.inc
    python test/host/controle_python_riche.py --verifier   # ne modifie rien : échoue si un .inc n'est pas à jour

Entrées : test/host/donnees/openmeteo_riche_<lieu>.json = réponses RÉELLES d'Open-Meteo, capturées avec l'adresse exacte du
firmware par test/host/capturer_meteo_riche.py (voir donnees/LISEZMOI.md).
Sorties (générées, ne pas modifier à la main) :
  openmeteo_riche_<lieu>.inc   la réponse en littéral brut C++ (copie octet par octet du .json)
  attendu_riche_<lieu>.inc     ce que `analyser_riche()` doit produire, une valeur entière par ligne dans l'ordre de
                               `aplatir()` de test_meteo_riche.cpp ; chaque ligne porte son rang et son nom en commentaire
                               (un test qui échoue rend 10000 + rang : retrouver la ligne dans ce fichier)

Les valeurs sont calculées ici avec json.loads(parse_float=Decimal) et le module decimal (arrondi « moitié loin de zéro »,
comme Open-Meteo les écrit à 1 ou 2 décimales) : aucune ligne de code C++ du noyau n'est reprise. Le DÉCALAGE d'une étiquette
des cumuls est écrit ici à partir de la documentation d'Open-Meteo (le cumul d'une étiquette porte sur l'intervalle qui la
PRÉCÈDE), pas à partir du code C++.
"""
import argparse
import json
import sys
from decimal import ROUND_HALF_UP, Decimal
from pathlib import Path

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
FIXTURES = ["lyon", "longyearbyen", "seattle", "singapour", "sydney", "katmandou", "terreneuve"]
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_riche.py : ne pas modifier à la main.\n"
HEURES_MAX, QUARTS_MAX, JOURS_MAX = 24, 9, 7


def fixe(d, decimales):
    """d (Decimal ou int) × 10^decimales, arrondi à l'entier le plus proche, moitié loin de zéro."""
    return int((Decimal(d) * (Decimal(10) ** decimales)).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def minutes(secondes):
    """Durée en secondes -> minutes à la plus proche (moitié vers le haut), sur la valeur exacte écrite par l'API."""
    return int((Decimal(secondes) / 60).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def charger(nom):
    brut = (DONNEES / f"openmeteo_riche_{nom}.json").read_bytes()
    return brut, json.loads(brut.decode("utf-8"), parse_float=Decimal, parse_int=int)


def inc_litteral(nom, brut):
    texte = brut.decode("utf-8")
    assert ')OM"' not in texte
    return ENTETE + f"// Réponse réelle d'Open-Meteo ({nom}, requête du firmware), voir donnees/LISEZMOI.md.\nR\"OM({texte})OM\"\n"


def valeurs_attendues(doc):
    """Liste de (nom, entier) dans l'ordre de aplatir() de test_meteo_riche.cpp."""
    v = []

    def ajouter(nom, valeur):
        v.append((nom, int(valeur)))

    cur, h, m, d = doc["current"], doc["hourly"], doc["minutely_15"], doc["daily"]
    t_now = cur["time"]
    # --- relevé de base (lu par analyser()) ---
    ajouter("instant_s", t_now)
    ajouter("decalage_utc_s", doc["utc_offset_seconds"])
    ajouter("temperature_dixiemes", fixe(cur["temperature_2m"], 1))
    ajouter("ressentie_dixiemes", fixe(cur["apparent_temperature"], 1))
    ajouter("code_wmo", cur["weather_code"])
    ajouter("jour", 1 if cur["is_day"] == 1 else 0)
    ajouter("vent_dixiemes_kmh", fixe(cur["wind_speed_10m"], 1))
    ajouter("rafales_dixiemes_kmh", fixe(cur["wind_gusts_10m"], 1))
    ajouter("direction_vent_deg", fixe(cur["wind_direction_10m"], 0))
    ajouter("pression_dixiemes_hpa", fixe(cur["pressure_msl"], 1))
    ajouter("humidite_pct", fixe(cur["relative_humidity_2m"], 0))
    proba_now = -1  # règle de analyser() : la case horaire dont l'étiquette est la plus proche AVANT l'instant courant
    for t, p in zip(h["time"], h["precipitation_probability"]):
        if t <= t_now < t + 3600:
            proba_now = int(p) if p is not None else -1
            break
    ajouter("proba_precip_pct (analyser)", proba_now)
    nj = min(len(d["time"]), JOURS_MAX)
    ajouter("nb_jours", nj)
    for i in range(nj):
        ajouter(f"jours[{i}].date_s", d["time"][i])
        ajouter(f"jours[{i}].code_wmo", d["weather_code"][i])
        ajouter(f"jours[{i}].min_dixiemes", fixe(d["temperature_2m_min"][i], 1))
        ajouter(f"jours[{i}].max_dixiemes", fixe(d["temperature_2m_max"][i], 1))
        ajouter(f"jours[{i}].lever_s", d["sunrise"][i])
        ajouter(f"jours[{i}].coucher_s", d["sunset"][i])
        ajouter(f"jours[{i}].uv_max_dixiemes", fixe(d["uv_index_max"][i], 1))
        ajouter(f"jours[{i}].proba_precip_max", fixe(d["precipitation_probability_max"][i], 0))
    # --- ajouts riches ---
    ajouter("precip_courante_dixiemes", fixe(cur["precipitation"], 1))
    ajouter("nuages_pct", fixe(cur["cloud_cover"], 0))
    # Heure i = [étiquette i, étiquette i + 1 h) : instantanés à l'étiquette i, cumuls et moyenne à l'étiquette i + 1.
    nh = min(len(h["time"]) - 1, HEURES_MAX)
    ajouter("nb_heures", nh)
    for i in range(nh):
        nom = f"heures[{i}]"
        ajouter(f"{nom}.debut_s", h["time"][i])
        ajouter(f"{nom}.temperature_dixiemes", fixe(h["temperature_2m"][i], 1))
        ajouter(f"{nom}.ressentie_dixiemes", fixe(h["apparent_temperature"][i], 1))
        ajouter(f"{nom}.humidite_pct", fixe(h["relative_humidity_2m"][i], 0))
        p = h["precipitation_probability"][i + 1]
        ajouter(f"{nom}.proba_pct", -1 if p is None else fixe(p, 0))
        ajouter(f"{nom}.precip_dixiemes_mm", fixe(h["precipitation"][i + 1], 1))
        ajouter(f"{nom}.code_wmo", h["weather_code"][i])
        ajouter(f"{nom}.nuages_pct", fixe(h["cloud_cover"][i], 0))
        ajouter(f"{nom}.vent_dixiemes_kmh", fixe(h["wind_speed_10m"][i], 1))
        ajouter(f"{nom}.rafales_dixiemes_kmh", fixe(h["wind_gusts_10m"][i], 1))
        ajouter(f"{nom}.direction_deg", fixe(h["wind_direction_10m"][i], 0))
        ajouter(f"{nom}.uv_dixiemes", fixe(h["uv_index"][i + 1], 1))
        ajouter(f"{nom}.jour", 1 if h["is_day"][i] == 1 else 0)
    ajouter("riche.nb_jours", nj)
    for i in range(nj):
        nom = f"riche.jours[{i}]"
        ajouter(f"{nom}.ressentie_min_dixiemes", fixe(d["apparent_temperature_min"][i], 1))
        ajouter(f"{nom}.ressentie_max_dixiemes", fixe(d["apparent_temperature_max"][i], 1))
        ajouter(f"{nom}.ensoleillement_min", minutes(d["sunshine_duration"][i]))
        ajouter(f"{nom}.precip_dixiemes_mm", fixe(d["precipitation_sum"][i], 1))
        ajouter(f"{nom}.neige_dixiemes_cm", fixe(d["snowfall_sum"][i], 1))
        ajouter(f"{nom}.heures_precip", fixe(d["precipitation_hours"][i], 0))
        ajouter(f"{nom}.vent_max_dixiemes_kmh", fixe(d["wind_speed_10m_max"][i], 1))
        ajouter(f"{nom}.rafales_max_dixiemes_kmh", fixe(d["wind_gusts_10m_max"][i], 1))
        ajouter(f"{nom}.direction_dominante_deg", fixe(d["wind_direction_10m_dominant"][i], 0))
    # Quart d'heure i = [étiquette 0 - 15 min + 15 min × i, + 15 min) : le cumul d'une étiquette est celui du quart PRÉCÉDENT.
    nq = min(len(m["time"]), QUARTS_MAX)
    ajouter("nb_quarts", nq)
    ajouter("quarts_debut_s", m["time"][0] - 900)
    for i in range(nq):
        ajouter(f"quarts_dixiemes[{i}]", fixe(m["precipitation"][i], 1))
    return v


def inc_attendu(nom, doc):
    valeurs = valeurs_attendues(doc)
    lignes = [ENTETE, f"// Valeurs attendues pour {nom} : json.loads(parse_float=Decimal) + decimal, voir controle_python_riche.py.\n",
              f"// {len(valeurs)} valeurs, dans l'ordre de aplatir() de test_meteo_riche.cpp. Rang, valeur, nom.\n"]
    for rang, (champ, valeur) in enumerate(valeurs):
        lignes.append(f"{valeur}LL, // {rang} {champ}\n")
    return "".join(lignes), len(valeurs)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    a = ap.parse_args()
    sorties, resume = {}, []
    for nom in FIXTURES:
        brut, doc = charger(nom)
        sorties[f"openmeteo_riche_{nom}.inc"] = inc_litteral(nom, brut)
        texte, n = inc_attendu(nom, doc)
        sorties[f"attendu_riche_{nom}.inc"] = texte
        resume.append(f"{nom}: {len(brut)} octets, {n} valeurs attendues")
    ok = True
    for nom, texte in sorties.items():
        chemin = DONNEES / nom
        if a.verifier:
            # git peut convertir les fins de ligne (core.autocrlf) : on compare sans tenir compte de CRLF/LF
            if not chemin.exists() or chemin.read_bytes().replace(b"\r\n", b"\n") != texte.encode("utf-8"):
                print("PAS À JOUR :", nom)
                ok = False
        else:
            chemin.write_bytes(texte.encode("utf-8"))
    print("\n".join(resume))
    print("vérifié : tout est à jour" if a.verifier and ok else ("écrit" if not a.verifier else "ÉCHEC"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
