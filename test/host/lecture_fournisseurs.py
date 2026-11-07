"""Lecture INDÉPENDANTE des réponses de MET Norway et du NWS, pour controle_python_fournisseurs.py.

Ce module relit les réponses réelles (json.loads(parse_float=Decimal), decimal, datetime, zoneinfo) et en tire les valeurs que
les lecteurs C++ (meteo_metno.hpp, meteo_nws.hpp) doivent produire, sans reprendre une ligne de C++. Les règles viennent de la
documentation de chaque service (voir docs/mesures/fournisseurs-meteo.md) et des commentaires d'en-tête des deux lecteurs
(ce qui est inconnu, comment un jour se découpe) ; elles sont écrites ici dans un autre ordre, avec d'autres structures.

Chaque valeur attendue est un couple (valeur, tolérance). La tolérance est 0 (égalité exacte) sauf pour ce qui dépend du calcul
du soleil, dont les deux algorithmes (NOAA dans le noyau, US Naval Observatory ici) ne s'accordent qu'à quelques minutes :
lever et coucher (120 s jusqu'à 60 ° de latitude, 300 s au-delà), et l'indicateur « jour » d'un instant à moins de 5 minutes d'un
lever ou d'un coucher (1 : les deux réponses sont acceptées).
"""
import re
from datetime import datetime, timedelta
from decimal import ROUND_HALF_UP, Decimal
from zoneinfo import ZoneInfo

DIX_INCONNU = -32768
ENT_INCONNU = -1
HEURES_MAX = 24
JOURS_MAX = 7
SEUIL_PLUIE = 1  # dixièmes de mm : une heure « de pluie » compte au moins 0,1 mm


def fixe(d, decimales):
    """d × 10^decimales, arrondi à l'entier le plus proche, moitié loin de zéro."""
    return int((Decimal(d) * (Decimal(10) ** decimales)).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def dix(d):
    return DIX_INCONNU if d is None else fixe(d, 1)


def entier(d):
    return ENT_INCONNU if d is None else fixe(d, 0)


def unix(texte):
    """« 2026-11-07T12:00:00Z » ou « …+00:00 » -> secondes UNIX."""
    return int(datetime.fromisoformat(texte.replace("Z", "+00:00")).timestamp())


def duree_s(texte):
    m = re.fullmatch(r"P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?", texte)
    if not m:
        raise ValueError(texte)
    j, h, mi, s = (int(x) if x else 0 for x in m.groups())
    return j * 86400 + h * 3600 + mi * 60 + s


class Plat:
    """La liste ordonnée des valeurs attendues : (nom, valeur, tolérance)."""

    def __init__(self):
        self.v = []

    def ajouter(self, nom, valeur, tol=0):
        self.v.append((nom, int(valeur), int(tol)))


def decalage_s(t, zone):
    """Décalage UTC (secondes) de la zone à l'instant t."""
    o = datetime.fromtimestamp(t, zone).utcoffset()
    assert o is not None
    return int(o.total_seconds())


def jours_locaux(recu, iana):
    """Débuts (UNIX) de 7 jours locaux à partir du jour local de `recu`, puis le début du suivant."""
    zone = ZoneInfo(iana)
    d0 = datetime.fromtimestamp(recu, zone).date()
    debuts = []
    for k in range(JOURS_MAX + 1):
        d = d0 + timedelta(days=k)
        debuts.append(int(datetime(d.year, d.month, d.day, tzinfo=zone).timestamp()))
    return debuts


def tolerance_soleil(lat_e5):
    return 120 if abs(lat_e5) <= 6000000 else 300


class Soleil:
    """Lever, coucher et « jour » d'après le calcul indépendant `soleil(debut, lat, lon)` de controle_python_fournisseurs.py."""

    def __init__(self, fonction, lat_e5, lon_e5, iana):
        self.f, self.lat_e5, self.lon_e5, self.iana = fonction, lat_e5, lon_e5, iana
        self.cache = {}

    def jour(self, debut):
        """(situation, lever, coucher) du jour local [debut, fin)."""
        if debut not in self.cache:
            cas, lever, coucher, marge = self.f(debut, self.lat_e5 / 100000.0, self.lon_e5 / 100000.0)
            if marge < 0.02:
                raise SystemExit(f"jour trop proche de la limite polaire pour {self.iana} {debut}")
            self.cache[debut] = (cas, lever, coucher)
        return self.cache[debut]

    def est_jour(self, t):
        """(booleen, tolérance) pour l'instant t."""
        zone = ZoneInfo(self.iana)
        d = datetime.fromtimestamp(t, zone).date()
        debut = int(datetime(d.year, d.month, d.day, tzinfo=zone).timestamp())
        cas, lever, coucher = self.jour(debut)
        if cas == 1:
            return False, 0
        if cas == 2:
            return True, 0
        tol = 1 if min(abs(t - lever), abs(t - coucher)) <= 300 else 0
        return lever <= t < coucher, tol

    def lever_coucher(self, debut, fin):
        """(lever, coucher, tolérance) tels que le noyau les écrit (nuit polaire : minuit, minuit ; jour polaire : minuit, fin)."""
        cas, lever, coucher = self.jour(debut)
        if cas == 1:
            return debut, debut, 0
        if cas == 2:
            return debut, fin, 0
        return lever, coucher, tolerance_soleil(self.lat_e5)


def vers_plat(plat, releve, jours, riche, heures, jours_riche):
    """Ajoute à `plat` les valeurs dans l'ordre de `aplatir()` des tests C++ (test_meteo_metno.cpp, test_meteo_nws.cpp)."""
    r = releve
    plat.ajouter("instant_s", r["instant_s"])
    plat.ajouter("decalage_utc_s", r["decalage_utc_s"])
    plat.ajouter("temperature_dixiemes", r["temperature"])
    plat.ajouter("ressentie_dixiemes", r["ressentie"])
    plat.ajouter("code_wmo", r["code"])
    plat.ajouter("jour", 1 if r["jour"][0] else 0, r["jour"][1])
    plat.ajouter("vent_dixiemes_kmh", r["vent"])
    plat.ajouter("rafales_dixiemes_kmh", r["rafales"])
    plat.ajouter("direction_vent_deg", r["direction"])
    plat.ajouter("pression_dixiemes_hpa", r["pression"])
    plat.ajouter("humidite_pct", r["humidite"])
    plat.ajouter("proba_precip_pct", r["proba"])
    plat.ajouter("nb_jours", len(jours))
    for i, j in enumerate(jours):
        plat.ajouter(f"jours[{i}].date_s", j["date_s"])
        plat.ajouter(f"jours[{i}].code_wmo", j["code"])
        plat.ajouter(f"jours[{i}].min_dixiemes", j["min"])
        plat.ajouter(f"jours[{i}].max_dixiemes", j["max"])
        plat.ajouter(f"jours[{i}].lever_s", j["lever"], j["tol_soleil"])
        plat.ajouter(f"jours[{i}].coucher_s", j["coucher"], j["tol_soleil"])
        plat.ajouter(f"jours[{i}].uv_max_dixiemes", DIX_INCONNU)
        plat.ajouter(f"jours[{i}].proba_precip_max", j["proba"])
    plat.ajouter("precip_courante_dixiemes", DIX_INCONNU)
    plat.ajouter("nuages_pct", riche["nuages"])
    plat.ajouter("nb_heures", len(heures))
    for i, h in enumerate(heures):
        n = f"heures[{i}]"
        plat.ajouter(f"{n}.debut_s", h["debut_s"])
        plat.ajouter(f"{n}.temperature_dixiemes", h["temperature"])
        plat.ajouter(f"{n}.ressentie_dixiemes", h["ressentie"])
        plat.ajouter(f"{n}.humidite_pct", h["humidite"])
        plat.ajouter(f"{n}.proba_pct", h["proba"])
        plat.ajouter(f"{n}.precip_dixiemes_mm", h["precip"])
        plat.ajouter(f"{n}.code_wmo", h["code"])
        plat.ajouter(f"{n}.nuages_pct", h["nuages"])
        plat.ajouter(f"{n}.vent_dixiemes_kmh", h["vent"])
        plat.ajouter(f"{n}.rafales_dixiemes_kmh", h["rafales"])
        plat.ajouter(f"{n}.direction_deg", h["direction"])
        plat.ajouter(f"{n}.uv_dixiemes", DIX_INCONNU)
        plat.ajouter(f"{n}.jour", 1 if h["jour"][0] else 0, h["jour"][1])
    plat.ajouter("riche.nb_jours", len(jours_riche))
    for i, c in enumerate(jours_riche):
        n = f"riche.jours[{i}]"
        plat.ajouter(f"{n}.ressentie_min_dixiemes", c["ressentie_min"])
        plat.ajouter(f"{n}.ressentie_max_dixiemes", c["ressentie_max"])
        plat.ajouter(f"{n}.ensoleillement_min", ENT_INCONNU)
        plat.ajouter(f"{n}.precip_dixiemes_mm", c["precip"])
        plat.ajouter(f"{n}.neige_dixiemes_cm", c["neige"])
        plat.ajouter(f"{n}.heures_precip", c["heures_precip"])
        plat.ajouter(f"{n}.vent_max_dixiemes_kmh", c["vent_max"])
        plat.ajouter(f"{n}.rafales_max_dixiemes_kmh", c["rafales_max"])
        plat.ajouter(f"{n}.direction_dominante_deg", ENT_INCONNU)
    plat.ajouter("nb_quarts", 0)
    return plat


def extremes(valeurs):
    """(min, max) des valeurs connues (DIX_INCONNU écarté), ou (DIX_INCONNU, DIX_INCONNU)."""
    connues = [v for v in valeurs if v != DIX_INCONNU]
    return (min(connues), max(connues)) if connues else (DIX_INCONNU, DIX_INCONNU)


# ----------------------------------------------------------------------------------------------------------------------------
# MET Norway Locationforecast 2.0 « complete »
# ----------------------------------------------------------------------------------------------------------------------------

# Les 41 identifiants du dépôt officiel metno/weathericons (weather/legend.csv, lu le 07/11/2026), sans _day/_night/_polartwilight.
SYMBOLES_METNO = [
    "clearsky", "fair", "partlycloudy", "cloudy", "lightrainshowers", "rainshowers", "heavyrainshowers",
    "lightrainshowersandthunder", "rainshowersandthunder", "heavyrainshowersandthunder", "lightsleetshowers", "sleetshowers",
    "heavysleetshowers", "lightssleetshowersandthunder", "sleetshowersandthunder", "heavysleetshowersandthunder",
    "lightsnowshowers", "snowshowers", "heavysnowshowers", "lightssnowshowersandthunder", "snowshowersandthunder",
    "heavysnowshowersandthunder", "lightrain", "rain", "heavyrain", "lightrainandthunder", "rainandthunder",
    "heavyrainandthunder", "lightsleet", "sleet", "heavysleet", "lightsleetandthunder", "sleetandthunder",
    "heavysleetandthunder", "lightsnow", "snow", "heavysnow", "lightsnowandthunder", "snowandthunder",
    "heavysnowandthunder", "fog",
]


def code_symbole_metno(symbole):
    """Code WMO du projet pour un symbole MET Norway, par la règle écrite dans ADR-0014 ; -1 s'il n'en a pas."""
    base = re.sub(r"_(day|night|polartwilight)$", "", symbole)
    if base == "lightssleetshowersandthunder":  # orthographe corrigée par le dépôt officiel
        base = "lightsleetshowersandthunder"
    if base == "lightssnowshowersandthunder":
        base = "lightsnowshowersandthunder"
    if base in ("clearsky", "fair", "partlycloudy", "cloudy"):
        return {"clearsky": 0, "fair": 1, "partlycloudy": 2, "cloudy": 3}[base]
    if base == "fog":
        return 45
    if "thunder" in base:
        return 97 if base.startswith("heavy") else 95
    force = 0 if base.startswith("light") else (2 if base.startswith("heavy") else 1)
    nature = re.sub(r"^(light|heavy)", "", base)
    familles = {
        "rain": (61, 63, 65),
        "rainshowers": (80, 81, 82),
        "sleet": (71, 73, 75),
        "sleetshowers": (85, 85, 86),
        "snow": (71, 73, 75),
        "snowshowers": (85, 85, 86),
    }
    return familles[nature][force] if nature in familles else -1


def _periode_metno(donnees, cle):
    bloc = donnees.get(cle)
    if bloc is None:
        return {"presente": False, "code": -1, "precip": DIX_INCONNU, "proba": ENT_INCONNU, "tmin": DIX_INCONNU,
                "tmax": DIX_INCONNU}
    det = bloc.get("details", {})
    symbole = bloc.get("summary", {}).get("symbol_code")
    return {
        "presente": True,
        "code": code_symbole_metno(symbole) if symbole is not None else -1,
        "precip": dix(det.get("precipitation_amount")),
        "proba": entier(det.get("probability_of_precipitation")),
        "tmin": dix(det.get("air_temperature_min")),
        "tmax": dix(det.get("air_temperature_max")),
    }


def _kmh_dix(ms):
    """m/s -> dixièmes de km/h : arrondi au dixième de m/s (écrit par la source), × 3,6, arrondi au dixième de km/h."""
    if ms is None:
        return DIX_INCONNU
    ms_dix = fixe(ms, 1)
    return (ms_dix * 36 + 5) // 10


def lire_metno(doc, recu, lat_e5, lon_e5, iana, soleil_f):
    """Valeurs attendues de analyser_metno() : un `Plat`."""
    sol = Soleil(soleil_f, lat_e5, lon_e5, iana)
    serie = []
    for el in doc["properties"]["timeseries"]:
        d = el["data"]
        det = d["instant"]["details"]
        serie.append({
            "t": unix(el["time"]),
            "temp": dix(det.get("air_temperature")),
            "ress": dix(det.get("apparent_air_temperature")),
            "hum": entier(det.get("relative_humidity")),
            "pres": dix(det.get("air_pressure_at_sea_level")),
            "nuages": entier(det.get("cloud_area_fraction")),
            "vent": _kmh_dix(det.get("wind_speed")),
            "raf": _kmh_dix(det.get("wind_speed_of_gust")),
            "dir": entier(det.get("wind_from_direction")),
            "h1": _periode_metno(d, "next_1_hours"),
            "h6": _periode_metno(d, "next_6_hours"),
            "h12": _periode_metno(d, "next_12_hours"),
        })

    def code_de(e):
        for p in ("h1", "h6", "h12"):
            if e[p]["code"] >= 0:
                return e[p]["code"]
        return -1

    i0 = max(i for i, e in enumerate(serie) if e["t"] <= recu)
    c = serie[i0]
    zone = ZoneInfo(iana)
    releve = {
        "instant_s": c["t"],
        "decalage_utc_s": decalage_s(recu, zone),
        "temperature": c["temp"], "ressentie": c["ress"], "code": code_de(c), "jour": sol.est_jour(c["t"]),
        "vent": c["vent"], "rafales": c["raf"], "direction": c["dir"], "pression": c["pres"], "humidite": c["hum"],
        "proba": c["h1"]["proba"],
    }
    riche = {"nuages": c["nuages"]}

    # 24 heures
    heures = []
    consecutif = all(
        i0 + k < len(serie) and serie[i0 + k]["t"] == c["t"] + 3600 * k and serie[i0 + k]["temp"] != DIX_INCONNU
        and serie[i0 + k]["h1"]["presente"] and serie[i0 + k]["h1"]["code"] >= 0 for k in range(HEURES_MAX))
    if consecutif:
        for k in range(HEURES_MAX):
            e = serie[i0 + k]
            heures.append({"debut_s": e["t"], "temperature": e["temp"], "ressentie": e["ress"], "humidite": e["hum"],
                           "proba": e["h1"]["proba"], "precip": e["h1"]["precip"], "code": e["h1"]["code"],
                           "nuages": e["nuages"], "vent": e["vent"], "rafales": e["raf"], "direction": e["dir"],
                           "jour": sol.est_jour(e["t"])})

    # jours locaux
    debuts = jours_locaux(recu, iana)
    par_t = {e["t"]: e for e in serie}
    jours, jours_riche = [], []
    for k in range(JOURS_MAX):
        d0, d1 = debuts[k], debuts[k + 1]
        dedans = [e for e in serie if d0 <= e["t"] < d1]
        temps = [e["temp"] for e in dedans]
        ress = [e["ress"] for e in dedans]
        vents = [e["vent"] for e in dedans]
        rafs = [e["raf"] for e in dedans]
        proba = ENT_INCONNU
        for e in dedans:
            proba = max(proba, e["h1"]["proba"])
            if e["h6"]["presente"] and e["t"] + 6 * 3600 <= d1:
                temps += [e["h6"]["tmin"], e["h6"]["tmax"]]
                proba = max(proba, e["h6"]["proba"])
        # le symbole du jour : l'élément le plus proche de midi local (le premier en cas d'égalité)
        code, meilleur = -1, None
        for e in dedans:
            cand = e["h6"]["code"] if e["h6"]["code"] >= 0 else (e["h12"]["code"] if e["h12"]["code"] >= 0 else e["h1"]["code"])
            ecart = abs(e["t"] - (d0 + 43200))
            if cand >= 0 and (meilleur is None or ecart < meilleur):
                meilleur, code = ecart, cand
        tmin, tmax = extremes(temps)
        if tmin == DIX_INCONNU or code < 0:
            break
        # cumul : des éléments qui se suivent de d0 à d1
        curseur, total, heures_pluie, complet, que_heures = d0, 0, 0, True, True
        while curseur < d1 and complet:
            e = par_t.get(curseur)
            if e is None:
                complet = False
            elif e["h1"]["presente"] and e["h1"]["precip"] != DIX_INCONNU:
                total += e["h1"]["precip"]
                heures_pluie += 1 if e["h1"]["precip"] >= SEUIL_PLUIE else 0
                curseur += 3600
            elif e["h6"]["presente"] and e["h6"]["precip"] != DIX_INCONNU and curseur + 6 * 3600 <= d1:
                total += e["h6"]["precip"]
                que_heures = False
                curseur += 6 * 3600
            else:
                complet = False
        lever, coucher, tol = sol.lever_coucher(d0, d1)
        rmin, rmax = extremes(ress)
        jours.append({"date_s": d0, "code": code, "min": tmin, "max": tmax, "lever": lever, "coucher": coucher,
                      "tol_soleil": tol, "proba": proba})
        jours_riche.append({
            "ressentie_min": rmin, "ressentie_max": rmax,
            "precip": total if complet else DIX_INCONNU,
            "neige": DIX_INCONNU,
            "heures_precip": heures_pluie if (complet and que_heures) else ENT_INCONNU,
            "vent_max": extremes(vents)[1], "rafales_max": extremes(rafs)[1],
        })
    return vers_plat(Plat(), releve, jours, riche, heures, jours_riche)


# ----------------------------------------------------------------------------------------------------------------------------
# NWS api.weather.gov : /points puis /gridpoints
# ----------------------------------------------------------------------------------------------------------------------------

def _force(intensite):
    if intensite in ("very_light", "light"):
        return 0
    if intensite == "heavy":
        return 2
    return 1  # « moderate » ou absente : le NWS écrit « Rain » pour une pluie ordinaire


IGNORES_NWS = {"blowing_dust", "blowing_sand", "blowing_snow", "frost", "haze", "ice_crystals", "smoke", "volcanic_ash",
               "water_spouts", "freezing_spray"}
POSSIBILITES_NWS = {"slight_chance", "isolated", "few", "brief"}


def code_phenomene_nws(entree):
    """Code WMO du projet d'un phénomène de la couche `weather`, -1 : aucun (rien, simple possibilité, hors de nos codes)."""
    nom = entree["weather"]
    if nom is None or entree["coverage"] in POSSIBILITES_NWS or nom in IGNORES_NWS:
        return -1
    f = _force(entree["intensity"])
    attrs = entree.get("attributes", [])
    table = {
        "drizzle": (51, 53, 55), "freezing_drizzle": (56, 57, 57), "rain": (61, 63, 65), "rain_showers": (80, 81, 82),
        "freezing_rain": (66, 67, 67), "snow": (71, 73, 75), "snow_showers": (85, 85, 86),
    }
    if nom in table:
        return table[nom][f]
    if nom == "sleet":
        return 77
    if nom == "hail":
        return 99 if "large_hail" in attrs else 96
    if nom == "thunderstorms":
        if "large_hail" in attrs:
            return 99
        if "small_hail" in attrs:
            return 96
        return 97 if f == 2 else 95
    if nom == "fog":
        return 45
    if nom in ("freezing_fog", "ice_fog"):
        return 48
    raise ValueError(f"phénomène inconnu : {nom}")


def code_ciel_nws(pourcent):
    if pourcent <= 12:
        return 0
    if pourcent <= 37:
        return 1
    if pourcent <= 87:
        return 2
    return 3


class CoucheNws:
    """Une couche du NWS : des intervalles (début, fin, valeur) en secondes UNIX absolues."""

    def __init__(self, grille, nom, convertir):
        self.nom = nom
        couche = grille.get(nom) or {}
        self.iv = []
        for x in couche.get("values") or []:
            debut_s, duree = x["validTime"].split("/")
            deb = unix(debut_s)
            self.iv.append((deb, deb + duree_s(duree), convertir(x["value"])))

    def a(self, t, inconnu):
        for deb, fin, v in self.iv:
            if deb <= t < fin:
                return v
        return inconnu

    def intervalle(self, t):
        for iv in self.iv:
            if iv[0] <= t < iv[1]:
                return iv
        return None

    def sur(self, a, b):
        return [v for deb, fin, v in self.iv if deb < b and fin > a]

    def cumul_exact(self, a, b):
        """Total des intervalles ENTIERS qui se suivent de a à b, ou None."""
        curseur, total = a, 0
        while curseur < b:
            suite = [iv for iv in self.iv if iv[0] == curseur]
            if not suite or suite[0][2] == DIX_INCONNU or suite[0][1] > b:
                return None
            total += suite[0][2]
            curseur = suite[0][1]
        return total


def lire_nws(points, grille, recu, lat_e5, lon_e5, soleil_f):
    """Valeurs attendues de analyser_nws() : un `Plat`."""
    iana = points["properties"]["timeZone"]
    g = grille["properties"]
    sol = Soleil(soleil_f, lat_e5, lon_e5, iana)
    temp = CoucheNws(g, "temperature", dix)
    ress = CoucheNws(g, "apparentTemperature", dix)
    hum = CoucheNws(g, "relativeHumidity", entier)
    nuages = CoucheNws(g, "skyCover", entier)
    vent = CoucheNws(g, "windSpeed", dix)
    raf = CoucheNws(g, "windGust", dix)
    direction = CoucheNws(g, "windDirection", entier)
    proba = CoucheNws(g, "probabilityOfPrecipitation", entier)
    pluie = CoucheNws(g, "quantitativePrecipitation", dix)
    # millimètres de neige -> dixièmes de centimètre : le même nombre, arrondi à l'entier
    neige = CoucheNws(g, "snowfallAmount", lambda v: DIX_INCONNU if v is None else fixe(v, 0))
    temps = CoucheNws(g, "weather", lambda v: max([code_phenomene_nws(e) for e in v] + [-1]))

    def proba_a(t):
        iv = proba.intervalle(t)
        return ENT_INCONNU if iv is None or iv[1] - iv[0] > 6 * 3600 else iv[2]

    def code_a(t):
        c, w = nuages.intervalle(t), temps.intervalle(t)
        if c is None or c[2] == ENT_INCONNU or w is None:
            return -1
        return max(code_ciel_nws(c[2]), w[2])

    zone = ZoneInfo(iana)
    releve = {
        "instant_s": recu,
        "decalage_utc_s": decalage_s(recu, zone),
        "temperature": temp.a(recu, DIX_INCONNU), "ressentie": ress.a(recu, DIX_INCONNU), "code": code_a(recu),
        "jour": sol.est_jour(recu), "vent": vent.a(recu, DIX_INCONNU), "rafales": raf.a(recu, DIX_INCONNU),
        "direction": direction.a(recu, ENT_INCONNU), "pression": DIX_INCONNU, "humidite": hum.a(recu, ENT_INCONNU),
        "proba": proba_a(recu),
    }
    riche = {"nuages": nuages.a(recu, ENT_INCONNU)}

    heures = []
    h0 = recu - recu % 3600
    for k in range(HEURES_MAX):
        t = h0 + 3600 * k
        if code_a(t) < 0 or temp.a(t, DIX_INCONNU) == DIX_INCONNU:
            heures = []
            break
        cumul = pluie.cumul_exact(t, t + 3600)
        heures.append({"debut_s": t, "temperature": temp.a(t, DIX_INCONNU), "ressentie": ress.a(t, DIX_INCONNU),
                       "humidite": hum.a(t, ENT_INCONNU), "proba": proba_a(t),
                       "precip": cumul if cumul is not None else DIX_INCONNU, "code": code_a(t),
                       "nuages": nuages.a(t, ENT_INCONNU), "vent": vent.a(t, DIX_INCONNU),
                       "rafales": raf.a(t, DIX_INCONNU), "direction": direction.a(t, ENT_INCONNU),
                       "jour": sol.est_jour(t)})

    debuts = jours_locaux(recu, iana)
    jours, jours_riche = [], []
    for k in range(JOURS_MAX):
        d0, d1 = debuts[k], debuts[k + 1]
        tmin, tmax = extremes(temp.sur(d0, d1))
        codes = [code_a(t) for t in range(d0, d1, 3600)]
        code = max(codes + [-1])
        if tmin == DIX_INCONNU or code < 0:
            break
        rmin, rmax = extremes(ress.sur(d0, d1))
        p = pluie.cumul_exact(d0, d1)
        n = neige.cumul_exact(d0, d1) if neige.iv else None
        lever, coucher, tol = sol.lever_coucher(d0, d1)
        pmax = max(proba.sur(d0, d1) + [ENT_INCONNU])
        jours.append({"date_s": d0, "code": code, "min": tmin, "max": tmax, "lever": lever, "coucher": coucher,
                      "tol_soleil": tol, "proba": pmax})
        jours_riche.append({
            "ressentie_min": rmin, "ressentie_max": rmax,
            "precip": p if p is not None and p <= 32767 else DIX_INCONNU,
            "neige": n if n is not None and n <= 32767 else DIX_INCONNU,
            "heures_precip": ENT_INCONNU,
            "vent_max": extremes(vent.sur(d0, d1))[1], "rafales_max": extremes(raf.sur(d0, d1))[1],
        })
    return vers_plat(Plat(), releve, jours, riche, heures, jours_riche)
