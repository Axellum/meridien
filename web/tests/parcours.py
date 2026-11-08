#!/usr/bin/env python3
"""Parcours de la page de réglages contre le faux appareil, avec Playwright (Chromium, rendu téléphone).

    python web/tests/parcours.py

Lance web/faux-serveur/serveur.mjs (Node) sur des ports de test, déroule le parcours, écrit les captures dans
web/captures/ et rend un tableau « vérification : OK / ÉCHEC ». Code de sortie 0 seulement si tout passe.
Réseau requis : la recherche de lieu et la météo appellent vraiment Open-Meteo.

Contre le VRAI appareil (le firmware qui sert la page lui-même) :

    $env:URL_APPAREIL = "http://192.168.x.y"      # l'adresse affichée sur l'écran de l'appareil
    $env:CODE_APPAIRAGE = "123456"                 # le code affiché sur l'écran de l'appareil
    python web/tests/parcours.py

Aucun faux serveur n'est lancé. Les étapes que le firmware ne fait pas encore (services STT/LLM/TTS, export / import, changement
de Wi-Fi, mise à jour) sont SAUTÉES et listées comme telles : elles ne comptent ni comme réussies ni comme ratées. Le parcours
change vraiment les réglages de l'appareil (thème, luminosité, langues, ville) ; il remet l'affichage dans son état de départ à
la fin. L'essai du verrou d'appairage bloque l'appairage 60 secondes.
"""
import gzip
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

from controle_langues import lire_yaml  # même dossier : python web/tests/parcours.py met ce dossier dans sys.path
from playwright.sync_api import expect, sync_playwright

RACINE = Path(__file__).resolve().parents[2]
CAPTURES = RACINE / "web" / "captures"
LANGUES_PAGE = RACINE / "web" / "app" / "lang"
CODES_LANGUES = ["fr", "en", "de", "nl", "es", "it", "tr"]   # ordre figé par l'ADR-0006 (indices 0 à 6)
PORT, PORT_SERVICE = 18100, 18101
URL_APPAREIL = os.environ.get("URL_APPAREIL", "").strip().rstrip("/")   # vide : faux appareil (comportement d'origine)
REEL = bool(URL_APPAREIL)
BASE = URL_APPAREIL if REEL else f"http://127.0.0.1:{PORT}"
CODE = os.environ.get("CODE_APPAIRAGE", "").strip() if REEL else "246810"
CLE_BONNE = "sk-secret-TEST-9f3a"   # la clé que le faux service accepte (valeur de test, pas un secret réel)
CLE_FAUSSE = "sk-faux-0000-leak"    # le faux service la cite dans sa réponse d'erreur, comme certains vrais services
VERROU_S = 60 if REEL else 3        # durée du verrou d'appairage : 60 s sur l'appareil (noyau, appairage.hpp), 3 s sur le faux
BUDGET_GZ = 30 * 1024               # budget annoncé : 30 Ko compressés (HTML + CSS + JS)

sautes = []        # (étape, raison) : étapes que l'appareil réel ne fait pas encore
resultats = []     # (nom, ok, détail)
registre = []      # toutes les réponses /api/ vues par le navigateur : {url, statut, corps, requete}
hotes = set()      # tous les hôtes joints par le navigateur (la page ne doit parler qu'à l'appareil)
hors_appareil = []         # les requêtes du navigateur vers un autre hôte que l'appareil : {url, methode}
vigie_interceptees = []    # les lectures chez Vigie que le parcours a interceptées : {url, methode, entetes}
hotes_vigie = set()        # les hôtes de Vigie que la page a tenté de joindre, tous interceptés (la seule exception à « ne parler qu'à l'appareil »)
erreurs_console = []


def verifier(nom, condition, detail=""):
    resultats.append((nom, bool(condition), detail))
    print(f"  {'OK   ' if condition else 'ÉCHEC'} {nom}" + (f"  [{detail}]" if detail and not condition else ""))


def etape(titre, fonction, page=None):
    print(f"\n== {titre}")
    try:
        fonction()
    except Exception as e:  # noqa: BLE001 : on veut le rapport complet, pas l'arrêt au premier raté
        verifier(titre + " (déroulement)", False, str(e).strip().splitlines()[0][:220])
        if page is not None:
            try:
                page.screenshot(path=str(CAPTURES / "_echec.png"), full_page=True)
            except Exception:  # noqa: BLE001
                pass


def sauter(titre, raison):
    """Étape que l'appareil réel ne fait pas encore : dite, jamais comptée comme réussie."""
    print(f"\n== {titre}\n  SAUTÉE : {raison}")
    sautes.append((titre, raison))


def attendre_serveur():
    for _ in range(60):
        try:
            urllib.request.urlopen(BASE + "/api/status", timeout=1).read()
            return
        except Exception:  # noqa: BLE001
            time.sleep(0.25)
    raise RuntimeError(f"{'l’appareil ne répond pas à ' + BASE if REEL else 'le faux appareil ne démarre pas'}")


def nouveau_contexte(nav, largeur=390, sombre=False):
    ctx = nav.new_context(viewport={"width": largeur, "height": 844}, device_scale_factor=1.5, is_mobile=True, has_touch=True,
                          locale="fr-FR", color_scheme="dark" if sombre else "light", accept_downloads=True)
    page = ctx.new_page()
    page.set_default_timeout(20000)

    def intercepter(route):
        try:
            rep = route.fetch()
            corps = rep.text() if "json" in (rep.headers.get("content-type") or "") else f"<{len(rep.body())} octets>"
            envoye = (route.request.post_data_buffer or b"").decode("utf-8", errors="replace")  # un firmware est binaire : pas de post_data
            registre.append({"url": route.request.url, "statut": rep.status, "corps": corps, "requete": envoye})
            route.fulfill(response=rep)
        except Exception:  # noqa: BLE001 : requête annulée par la page (anti-rebond) ou appareil qui redémarre
            try:
                route.abort()
            except Exception:  # noqa: BLE001
                pass

    page.route("**/api/**", intercepter)
    installer_vigie(page)   # toute lecture de la liste des spots est interceptée, sur toutes les pages : un « Tab » depuis le champ des alertes arrive dans le champ de la mer
    def noter_requete(r):
        hote = urllib.parse.urlsplit(r.url).netloc
        hotes.add(hote)
        if hote != urllib.parse.urlsplit(BASE).netloc:
            hors_appareil.append({"url": r.url, "methode": r.method})
    page.on("request", noter_requete)
    page.on("console", lambda m: erreurs_console.append(m.text) if m.type in ("error", "warning") else None)
    page.on("pageerror", lambda e: erreurs_console.append("pageerror : " + str(e)))
    return ctx, page


def jeton(page):
    return page.evaluate("sessionStorage.getItem('jeton')")


def api(page, methode, chemin, corps=None):
    r = page.request.fetch(BASE + chemin, method=methode, headers={"Authorization": "Bearer " + jeton(page), "Content-Type": "application/json"},
                           data=json.dumps(corps) if corps is not None else None)
    return r.status, (r.json() if "json" in (r.headers.get("content-type") or "") else {})


def catalogue(code):
    """Le petit fichier JSON d'une langue, tel que la page le charge (web/app/lang/<code>.json)."""
    return json.loads((LANGUES_PAGE / f"{code}.json").read_text(encoding="utf-8"))


def norm(texte):
    """Espaces normalisés (l'espace insécable devient une espace) : on compare ce qu'on lit, pas la typographie."""
    return " ".join(texte.split())


# ---- Carte « La mer » : la PAGE lit la liste des spots chez Vigie (GET <adresse>/v1/surf/spots) ; le parcours INTERCEPTE cette lecture avec une liste
# SYNTHÉTIQUE (spots inventés : aucune vraie donnée de spot dans le dépôt) et note chaque requête interceptée. ----
VIGIE_ESSAI = "https://vigie-essai.test"   # adresse de Vigie choisie par le parcours (domaine en .test : ne se résout jamais)
NOM_LONG = "Pointe de l’Étoile-Filante-Fict🌊 du Grand Nord"   # 46 points de code ; le 32e est un émoji (deux unités UTF-16 : une coupe naïve le casserait)
NOM_COUPE = "".join(list(NOM_LONG)[:32]).rstrip()
assert len(list(NOM_LONG)) > 32 and len(NOM_COUPE) == 32 and NOM_COUPE.endswith("🌊")
SPOTS_ESSAI = {"count": 6, "built_utc": "2026-11-08T00:00:00Z", "attribution": "liste d'essai, spots inventés", "spots": [
    {"id": "spot-essai-un", "name": "Plage de l’Écluse", "osm_name": "Plage de l'Ecluse", "place": "Port-Essai", "note": "champ ignoré"},
    {"id": "spot-essai-deux", "name": "Baie des Essais", "osm_name": "Baie des Essais", "place": "Île-Test"},
    {"id": "spot-essai-trois", "name": NOM_LONG, "place": "Anse-Fictive"},
    {"id": "spot-essai-quatre", "name": "Grande Plage", "place": "Ville-Un"},
    {"id": "spot-essai-cinq", "name": "Grande Plage", "place": "Ville-Deux"},
    {"id": "spot-essai-six", "name": "", "osm_name": "Crique sans nom", "place": "Hameau-Dernier"},
]}
# Une seconde liste, servie à l'adresse VIGIE_ESSAI : 10 spots qui répondent à « lot » (la page n'en montre que 8) et un identifiant que l'appareil refuserait.
SPOTS_LOT = {"count": 11, "spots": [{"id": f"spot-lot-{i:02d}", "name": f"Spot de lot {i:02d}", "place": "Lieu-Lot"} for i in range(1, 11)]
             + [{"id": "Mauvais Id", "name": "Spot invalide", "place": "Lieu-Lot"}]}
UN = {"id": "spot-essai-un", "nom": "Plage de l’Écluse"}
DEUX = {"id": "spot-essai-deux", "nom": "Baie des Essais"}
TROIS = {"id": "spot-essai-trois", "nom": NOM_COUPE}
QUATRE = {"id": "spot-essai-quatre", "nom": "Grande Plage"}
PHRASE_VENT = "La plage de vent doit être en nœuds entiers, de 1 à 60, le minimum sous le maximum."
SPION_FETCH = """(() => { const f = window.fetch.bind(window); window.__vigie = [];
  window.fetch = (u, i) => { const url = typeof u === 'string' ? u : u.url;
    if (url.includes('/v1/surf/spots')) window.__vigie.push({url, credentials: i && i.credentials, accept: i && i.headers && i.headers.Accept, signal: !!(i && i.signal)});
    return f(u, i); }; })()"""


def liste_par_adresse(url):
    return SPOTS_LOT if urllib.parse.urlsplit(url).netloc == urllib.parse.urlsplit(VIGIE_ESSAI).netloc else SPOTS_ESSAI


vigie = {"reponse": liste_par_adresse}   # ce que « Vigie » répond : une liste, un statut (int), "illisible", "coupure", "silence", ou une fonction(url)
vigie_en_attente = []                    # les lectures laissées sans réponse ("silence") : le test les abandonne proprement avant de fermer la page


def liberer_vigie():
    while vigie_en_attente:
        try:
            vigie_en_attente.pop().abort()
        except Exception:  # noqa: BLE001 : la page a déjà annulé la requête
            pass


def installer_vigie(page):
    """Intercepte toute lecture de la liste des spots par la page. Rien ne part vers le réseau."""
    def gestionnaire(route):
        r = route.request
        vigie_interceptees.append({"url": r.url, "methode": r.method, "entetes": dict(r.headers)})
        hotes_vigie.add(urllib.parse.urlsplit(r.url).netloc)
        rep = vigie["reponse"](r.url) if callable(vigie["reponse"]) else vigie["reponse"]
        cors = {"access-control-allow-origin": "*"}   # sans cet en-tête, le navigateur refuserait la réponse venue d'une autre origine
        if rep == "coupure":
            route.abort()
        elif rep == "silence":
            vigie_en_attente.append(route)   # Vigie ne répond jamais (le délai de 15 s de la page)
        elif rep == "illisible":
            route.fulfill(status=200, content_type="application/json", headers=cors, body="{pas du json")
        elif isinstance(rep, int):
            route.fulfill(status=rep, content_type="application/json", headers=cors, body="{}")
        else:
            route.fulfill(status=200, content_type="application/json", headers=cors, body=json.dumps(rep))
    page.route("**/v1/surf/spots", gestionnaire)


def sea(page):
    return api(page, "GET", "/api/sea")[1]


def attendre_que(page, condition, delai=8.0):
    """Interroge `condition` jusqu'à ce qu'elle soit vraie (ou `delai` secondes). Rend sa dernière valeur.
    L'attente passe par Playwright (page.wait_for_timeout) et non par time.sleep : seul un appel à Playwright laisse tourner les gestionnaires de routes
    (sans cela, les requêtes interceptées ne seraient jamais notées pendant l'attente)."""
    fin, valeur = time.time() + delai, None
    while time.time() < fin:
        valeur = condition()
        if valeur:
            return valeur
        page.wait_for_timeout(100)
    return valeur


def capturer(page, carte, nom):
    """Capture de la carte seule, à la largeur de la fenêtre (390 px), sans la bulle « Enregistré » qui la recouvrirait.
    La fenêtre est allongée le temps de la capture : une carte plus haute que 844 px serait coupée, et la barre d'onglets collée en haut la recouvrirait."""
    page.evaluate("(() => { const t = document.getElementById('toast'); if (t) t.hidden = true; })()")
    taille = page.viewport_size
    page.set_viewport_size({"width": taille["width"], "height": 1500})
    try:
        carte.screenshot(path=str(CAPTURES / nom))
    finally:
        page.set_viewport_size(taille)


def puts_sea():
    """Corps des requêtes PUT /api/sea faites PAR LA PAGE (les appels du test passent par page.request et ne sont pas notés)."""
    return [json.loads(r["requete"]) for r in registre if r["url"].endswith("/api/sea") and r["requete"]]


def lignes_registre(carte):
    """Les spots enregistrés tels que la carte les montre : [nom, coché], dans l'ordre affiché."""
    return carte.evaluate("c => [...c.querySelectorAll('.spot')].map(l => [l.querySelector('.spot-choix span').textContent, l.querySelector('input').checked])")


def tout_le_texte(carte):
    """Tout ce que la carte dit à la personne : texte, libellés accessibles et indications des champs."""
    return carte.evaluate("c => c.textContent + ' ' + [...c.querySelectorAll('[aria-label],[placeholder]')].map(e => (e.getAttribute('aria-label') || '') + ' ' + (e.getAttribute('placeholder') || '')).join(' ')")


# Le service qui a donné la météo, tel que /api/weather le nomme (`fournisseur`) : identifiant -> (phrase du catalogue, nom, lien).
FOURNISSEURS = {
    "open-meteo": ("meteo.attribution", "Open-Meteo", "https://open-meteo.com/"),
    "met-norway": ("meteo.attribution_metno", "MET Norway", "https://api.met.no/"),
    "nws": ("meteo.attribution_nws", "NWS", "https://www.weather.gov/"),
}


# Les noms de licence de la phrase sont des liens vers le texte de la licence (CC BY 4.0 : texte ou lien de la licence ; NLOD 2.0 : lien).
LICENCES = {"CC BY 4.0": "https://creativecommons.org/licenses/by/4.0/", "NLOD 2.0": "https://data.norge.no/nlod/en/2.0"}
LICENCES_DU = {"open-meteo": ["CC BY 4.0"], "met-norway": ["CC BY 4.0", "NLOD 2.0"], "nws": []}


def verifier_attribution(page, code, quand, fournisseur="open-meteo"):
    """L'attribution du service qui a donné la météo : visible, traduite, son nom est un lien vers son site.
    Open-Meteo : CC BY 4.0. MET Norway : CC BY 4.0 et NLOD 2.0. NWS : données ouvertes, et la phrase « ni approuvé ni parrainé ».
    Chaque nom de licence est un lien vers son texte, et il n'y a pas d'autre lien."""
    cle, nom, adresse = FOURNISSEURS[fournisseur]
    cat = catalogue(code)
    zone = page.locator("#attribution")
    lien = zone.locator(f'a[href="{adresse}"]')
    expect(lien).to_have_text(nom)  # la page change d'attribution quand la réponse météo arrive : on attend le bon service avant de comparer
    attendu = norm(cat[cle].replace("{lien}", nom))
    if fournisseur == "nws":
        attendu += " " + norm(cat["meteo.sans_parrainage"])
    texte = norm(zone.inner_text())
    verifier(f"attribution ({quand}) : texte de la licence, dans la langue de la page", texte == attendu, f"{texte!r} au lieu de {attendu!r}")
    if fournisseur != "nws":
        verifier(f"attribution ({quand}) : la licence est nommée", "CC BY 4.0" in texte, texte)
    if fournisseur == "met-norway":
        verifier(f"attribution ({quand}) : la seconde licence (NLOD 2.0) est nommée", "NLOD 2.0" in texte, texte)
    verifier(f"attribution ({quand}) : « {nom} » est un lien vers {adresse}",
             lien.count() == 1 and lien.inner_text() == nom and lien.get_attribute("href") == adresse, str(lien.count()))
    for licence in LICENCES_DU[fournisseur]:
        texte_licence = zone.locator(f'a[href="{LICENCES[licence]}"]')
        verifier(f"attribution ({quand}) : « {licence} » est un lien vers le texte de la licence",
                 texte_licence.count() == 1 and texte_licence.inner_text() == licence and texte_licence.get_attribute("target") == "_blank"
                 and "noopener" in (texte_licence.get_attribute("rel") or ""), str(texte_licence.count()))
    verifier(f"attribution ({quand}) : aucun autre lien que le service et ses licences",
             zone.locator("a").count() == 1 + len(LICENCES_DU[fournisseur]), str(zone.locator("a").count()))


def sans_null(page, quand):
    """append()/replaceChildren() écrivent « null » à l'écran quand on leur passe null : bug vu au premier lancement."""
    texte = page.locator("body").inner_text()
    mots = re.findall(r"\bnull\b|\bundefined\b|\bNaN\b|\[object", texte)  # mots entiers : « nulle part » est du français
    verifier(f"aucun « null / undefined / NaN » affiché ({quand})", not mots, ", ".join(mots))


def appairer(page):
    page.goto(BASE)
    page.locator("#code").fill(CODE)
    expect(page.locator("#ville")).to_be_visible()


def ouvrir(page, nom):
    page.get_by_role("button", name=re.compile("^" + nom)).click()


DEPASSEMENTS = """() => {
  const l = document.documentElement.clientWidth, sortis = [];
  const rogne = (e) => { for (let p = e.parentElement; p && p !== document.body; p = p.parentElement) {
    const o = getComputedStyle(p).overflowX, r = p.getBoundingClientRect(); if (o !== 'visible' && r.left >= -1 && r.right <= l + 1) return true; } return false; };
  for (const e of document.querySelectorAll('body *')) { const r = e.getBoundingClientRect();
    if (r.width > 0 && (r.right > l + 1 || r.left < -1) && !rogne(e)) sortis.push(e.tagName + '.' + e.className + ' ' + Math.round(r.left) + '..' + Math.round(r.right)); }
  return {largeur: l, defilement: document.documentElement.scrollWidth, sortis: sortis.slice(0, 6)};
}"""


def main():
    CAPTURES.mkdir(exist_ok=True)
    if REEL:
        if not re.fullmatch(r"\d{6}", CODE):
            print("URL_APPAREIL est défini : CODE_APPAIRAGE doit contenir les 6 chiffres affichés sur l'écran de l'appareil.")
            return 2
        print(f"Appareil réel : {BASE} (aucun faux serveur lancé)")
        srv = None
    else:
        env = {**os.environ, "PORT": str(PORT), "PORT_SERVICE": str(PORT_SERVICE), "CODE_APPAIRAGE": CODE,
               "CLE_MOCK": CLE_BONNE, "VERROU_S": str(VERROU_S), "REBOOT_MS": "2500"}
        srv = subprocess.Popen(["node", str(RACINE / "web" / "faux-serveur" / "serveur.mjs")], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        attendre_serveur()
        with sync_playwright() as p:
            nav = p.chromium.launch()
            ctx, page = nouveau_contexte(nav)
            poids = {}

            def t_appairage():
                page.goto(BASE)
                expect(page.locator("#code")).to_be_visible()
                poids["res"] = page.evaluate("performance.getEntriesByType('resource').filter(e => !e.name.includes('/api/')).map(e => [e.name.split('/').pop() || 'index', e.encodedBodySize, e.decodedBodySize])")
                poids["nav"] = page.evaluate("(() => { const n = performance.getEntriesByType('navigation')[0]; return [n.encodedBodySize, n.decodedBodySize]; })()")
                page.screenshot(path=str(CAPTURES / "01-appairage.png"))
                page.locator("#code").fill("000000")
                expect(page.get_by_text(re.compile("Ce code n.est pas le bon"))).to_be_visible()
                msg = page.locator(".appairage .res").inner_text()
                verifier("code faux : phrase française, pas de code brut", "bon" in msg and not re.search(r"\b403\b|code_faux", msg), msg)
                page.locator("#code").fill(CODE)
                expect(page.locator("#ville")).to_be_visible()
                verifier("bon code : la page de réglages s'ouvre", True)
            etape("Appairage par le code affiché à l'écran", t_appairage, page)

            def t_ville():
                if not REEL:  # l'appareil réel a toujours une ville (Lyon par défaut, contrat meteo_zone.h)
                    expect(page.locator(".hero .ville")).to_have_text("Choisissez-la")
                page.screenshot(path=str(CAPTURES / "02-accueil-sans-ville.png"))
                champ = page.locator("#ville-q")
                champ.press_sequentially("Ly", delay=80)
                page.wait_for_timeout(900)
                expect(page.get_by_text("Tapez au moins 3 lettres.")).to_be_visible()
                verifier("2 lettres : aucune recherche envoyée", not any("/api/geocode" in r["url"] for r in registre))
                champ.press_sequentially("o", delay=80)
                expect(page.locator(".lieu").first).to_be_visible(timeout=20000)
                verifier("3 lettres : la recherche réelle (Open-Meteo) répond", page.locator(".lieu").count() >= 1, f"{page.locator('.lieu').count()} lieux")
                page.screenshot(path=str(CAPTURES / "03-recherche-lyon.png"))
                lyon = page.locator(".lieu").filter(has_text="Auvergne-Rhône-Alpes").filter(has_text="France").first
                expect(lyon).to_be_visible()
                lyon.click()
                expect(page.locator(".hero .ville")).to_have_text("Lyon")
                code, zone = api(page, "GET", "/api/location")
                verifier("Lyon : la zone enregistrée est Lyon", code == 200 and zone["nom"] == "Lyon" and zone["code_pays"] == "FR", str(zone))
                verifier("Lyon : fuseau IANA retenu = Europe/Paris", zone["fuseau"] == "Europe/Paris", zone["fuseau"])
                verifier("Lyon : chaîne POSIX tirée de la table du noyau", zone["posix"] == "CET-1CEST,M3.5.0,M10.5.0/3", zone["posix"])
                verifier("Lyon : coordonnées proches de 45,75 N / 4,85 E", abs(zone["latitude"] - 45.75) < 0.3 and abs(zone["longitude"] - 4.85) < 0.3)
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                aide = page.locator("#ville > .aide:not(#attribution)").inner_text()  # l'attribution Open-Meteo est aussi une « aide »
                verifier("fuseau et heure locale affichés", re.search(r"Europe/Paris · il est \d{1,2}\sh\s\d{2}", aide) is not None, aide)
                page.screenshot(path=str(CAPTURES / "04-ville-lyon.png"))
                # la météo affichée correspond à celle d'Open-Meteo lue directement au même moment
                direct = json.load(urllib.request.urlopen(f"https://api.open-meteo.com/v1/forecast?latitude={zone['latitude']}&longitude={zone['longitude']}&current=temperature_2m,weather_code&timezone=auto", timeout=10))["current"]
                affichee = float(re.findall(r"-?\d+", page.locator(".hero .temp b").inner_text())[0])
                verifier("météo affichée = météo directe d'Open-Meteo (±1,5 °C)", abs(affichee - direct["temperature_2m"]) <= 1.5, f"affichée {affichee}, directe {direct['temperature_2m']}")
                src = (RACINE / "components/core/include/ecran/meteo_wmo.hpp").read_text(encoding="utf-8")
                libelle = re.findall(rf"case {direct['weather_code']}: return \{{\"([^\"]+)\"", src)[0]
                verifier("libellé = table WMO du noyau pour le code direct", page.locator(".hero .temp small").inner_text() == libelle, libelle)
            etape("Ville : choisir « Lyon »", t_ville, page)

            def t_source():
                cat = catalogue("fr")
                champ = page.locator("#ville-q")
                liste_js = "[...document.querySelectorAll('#source-meteo option')].map((o) => [o.value, o.textContent])"
                expect(page.locator("#source-champ")).to_be_visible()
                verifier("source météo : le titre vient du catalogue", page.locator("label[for=source-meteo]").inner_text() == cat["source.titre"])
                options = page.evaluate(liste_js)
                verifier("source météo : à Lyon, « Automatique » puis le modèle mondial seulement", options == [["auto", "Automatique"], ["noaa", "NOAA GFS"]], str(options))
                verifier("source météo : « Automatique » au départ", page.locator("#source-meteo").input_value() == "auto")
                code, zone = api(page, "GET", "/api/location")
                verifier("source météo : l'API donne la même liste que la page", [s["id"] for s in zone["sources_meteo"]] == [o[0] for o in options], str(zone.get("sources_meteo")))
                code, erreur = api(page, "PUT", "/api/location", {"source_meteo": "knmi"})
                verifier("source météo : un service qui ne couvre pas la France est refusé", code == 422 and erreur["erreur"]["id"] == "source_indisponible" and erreur["erreur"]["champ"] == "source_meteo", str(erreur))
                code, erreur = api(page, "PUT", "/api/location", {"source_meteo": "n_importe_quoi"})
                verifier("source météo : un identifiant inconnu est refusé", code == 422 and erreur["erreur"]["id"] == "source_inconnue", str(erreur))
                verifier("source météo : les refus n'ont rien changé", api(page, "GET", "/api/location")[1]["source_meteo"] == "auto")
                # choisir le modèle mondial depuis la page
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                code, zone = api(page, "GET", "/api/location")
                verifier("source météo : NOAA GFS enregistrée, la ville n'a pas bougé", zone["source_meteo"] == "noaa" and zone["nom"] == "Lyon", str(zone))
                expect(page.locator("#attribution-modele")).to_contain_text(cat["source.modele"].replace("{modele}", "NOAA GFS"))
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                verifier("source météo : la météo revient avec le modèle choisi", True)
                page.screenshot(path=str(CAPTURES / "04b-source-noaa.png"))
                # Amsterdam : le service néerlandais devient proposé
                champ.fill("")
                champ.press_sequentially("Amsterdam", delay=60)
                amsterdam = page.locator(".lieu").filter(has_text="Pays-Bas").first
                expect(amsterdam).to_be_visible(timeout=20000)
                amsterdam.click()
                expect(page.locator(".hero .ville")).to_have_text("Amsterdam")
                options = page.evaluate(liste_js)
                verifier("source météo : à Amsterdam, KNMI s'ajoute avant le modèle mondial", [o[0] for o in options] == ["auto", "knmi", "noaa"], str(options))
                verifier("source météo : NOAA GFS gardée, elle couvre aussi les Pays-Bas", page.locator("#source-meteo").input_value() == "noaa" and page.locator("#source-retablie").is_hidden())
                page.locator("#source-meteo").select_option("knmi")
                expect(page.locator("#toast")).to_contain_text("KNMI")
                expect(page.locator("#attribution-modele")).to_contain_text(cat["source.modele"].replace("{modele}", "KNMI"))
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                page.screenshot(path=str(CAPTURES / "04c-source-knmi.png"))
                # retour à Lyon : KNMI ne couvre pas la France, la page revient sur « Automatique » ET le dit
                champ.fill("")
                champ.press_sequentially("Lyon", delay=60)
                page.locator(".lieu").filter(has_text="Auvergne-Rhône-Alpes").filter(has_text="France").first.click()
                expect(page.locator(".hero .ville")).to_have_text("Lyon")
                expect(page.locator("#source-retablie")).to_be_visible()
                verifier("source météo : le retour sur « Automatique » est dit, pas muet", cat["source.retablie"] in norm(page.locator("#source-retablie").inner_text()), page.locator("#source-retablie").inner_text())
                verifier("source météo : la liste montre « Automatique » et l'API aussi", page.locator("#source-meteo").input_value() == "auto" and api(page, "GET", "/api/location")[1]["source_meteo_retablie"] is True)
                page.screenshot(path=str(CAPTURES / "04d-source-retablie.png"))
                # choisir une source efface le message ; on remet « Automatique » pour la suite du parcours
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#source-retablie")).to_be_hidden()
                page.locator("#source-meteo").select_option("auto")
                expect(page.locator("#toast")).to_contain_text("Automatique")
                code, zone = api(page, "GET", "/api/location")
                verifier("source météo : remise sur « Automatique », message effacé", zone["source_meteo"] == "auto" and zone["source_meteo_retablie"] is False and page.locator("#attribution-modele").inner_text() == "", str(zone))
                verifier_attribution(page, "fr", "après les changements de source")
                sans_null(page, "source météo")
            etape("Source météo : liste par pays, choix, refus, retour automatique", t_source, page)

            def t_source_directe():
                # MET Norway : un service lu directement par l'appareil (ADR-0014), proposé pour la Norvège seulement. Le faux serveur ne sait lire
                # qu'Open-Meteo : il fait ce que l'appareil fait quand le service ne répond pas (météo d'Open-Meteo, `repli` vrai). Le cas où le
                # service répond est joué en remplaçant la réponse de /api/weather : on ne teste ici que ce que la PAGE affiche.
                cat = catalogue("fr")
                champ = page.locator("#ville-q")
                liste_js = "[...document.querySelectorAll('#source-meteo option')].map((o) => [o.value, o.textContent])"
                champ.fill("")
                champ.press_sequentially("Oslo", delay=60)
                oslo = page.locator(".lieu").filter(has_text="Norvège").first
                expect(oslo).to_be_visible(timeout=20000)
                oslo.click()
                expect(page.locator(".hero .ville")).to_have_text("Oslo")
                options = page.evaluate(liste_js)
                verifier("source directe : à Oslo, les deux « MET Norway » sont proposés (le modèle d'Open-Meteo et le service lu directement)",
                         [o[0] for o in options] == ["auto", "metno", "metnorway", "noaa"], str(options))
                code, zone = api(page, "GET", "/api/location")
                directs = {s["id"]: s["direct"] for s in zone["sources_meteo"]}
                verifier("source directe : l'API dit quelles sources sont lues directement", directs == {"auto": False, "metno": False, "metnorway": True, "noaa": False}, str(directs))
                page.locator("#source-meteo").select_option("metno")
                expect(page.locator("#attribution-modele")).to_contain_text(cat["source.modele"].replace("{modele}", "MET Norway"))
                verifier("source directe : le modèle d'Open-Meteo est nommé « Modèle : … »", page.locator("#source-repli").is_hidden())
                # 1. le service direct ne répond pas : la météo vient d'Open-Meteo, la page le dit et cite Open-Meteo
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                expect(page.locator("#source-repli")).to_be_visible()
                phrase = norm(cat["source.repli"].replace("{service}", "MET Norway (api.met.no)"))
                verifier("source directe : le repli est dit, avec le nom du service choisi", norm(page.locator("#source-repli").inner_text()) == phrase, page.locator("#source-repli").inner_text())
                verifier("source directe : pas de « Modèle : … » pour un service lu directement", page.locator("#attribution-modele").inner_text() == "", page.locator("#attribution-modele").inner_text())
                verifier_attribution(page, "fr", "repli : la météo vient d'Open-Meteo")
                code, meteo = api(page, "GET", "/api/weather")
                verifier("source directe : /api/weather nomme Open-Meteo et dit « repli »", meteo.get("repli") is True and meteo["fournisseur"]["id"] == "open-meteo", str(meteo.get("fournisseur")))
                page.screenshot(path=str(CAPTURES / "04e-source-repli.png"))

                # 2. le service répond : on joue la réponse de l'appareil (même forme que /api/weather, `fournisseur` de MET Norway)
                def reponse(fournisseur, repli=False):
                    return {"disponible": True, "perimee": False, "mise_a_jour": 1794000000, "fournisseur": fournisseur, "repli": repli,
                            "actuelle": {"temperature": 4.0, "ressentie": 2.0, "code": 3, "libelle": "Couvert", "jour": True}}

                met = {"id": "met-norway", "nom": "MET Norway", "lien": "https://api.met.no/"}
                nws = {"id": "nws", "nom": "NWS", "lien": "https://www.weather.gov/"}
                page.route("**/api/weather", lambda route: route.fulfill(status=200, content_type="application/json", body=json.dumps(reponse(met))))
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"4\s°C"), timeout=20000)
                expect(page.locator("#source-repli")).to_be_hidden()
                verifier_attribution(page, "fr", "MET Norway a répondu", "met-norway")
                verifier("source directe : pas de repli ni de « Modèle : … » quand le service a répondu",
                         page.locator("#source-repli").is_hidden() and page.locator("#attribution-modele").inner_text() == "")
                page.locator("#attribution").scroll_into_view_if_needed()
                page.screenshot(path=str(CAPTURES / "04f-source-metno.png"))
                # 3. la phrase du NWS (données ouvertes, sans parrainage) : la page n'a qu'à suivre ce que l'appareil annonce
                page.unroute("**/api/weather")
                page.route("**/api/weather", lambda route: route.fulfill(status=200, content_type="application/json", body=json.dumps(reponse(nws))))
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"4\s°C"), timeout=20000)
                verifier_attribution(page, "fr", "NWS a répondu", "nws")
                # 4. un fournisseur que la page ne connaît pas, ou un lien qui n'est pas https : retour à Open-Meteo, jamais un lien douteux
                page.unroute("**/api/weather")
                inconnu = {"id": "inconnu", "nom": "Inconnu", "lien": "javascript:alert(1)"}
                page.route("**/api/weather", lambda route: route.fulfill(status=200, content_type="application/json", body=json.dumps(reponse(inconnu))))
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"4\s°C"), timeout=20000)
                verifier_attribution(page, "fr", "fournisseur inconnu : Open-Meteo par défaut")
                page.unroute("**/api/weather")
                # retour à Lyon et à « Automatique » pour la suite du parcours
                page.locator("#source-meteo").select_option("auto")
                expect(page.locator("#toast")).to_contain_text("Automatique")
                champ.fill("")
                champ.press_sequentially("Lyon", delay=60)
                page.locator(".lieu").filter(has_text="Auvergne-Rhône-Alpes").filter(has_text="France").first.click()
                expect(page.locator(".hero .ville")).to_have_text("Lyon")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                verifier("source directe : retour à Lyon, « Automatique », aucun message de repli",
                         page.locator("#source-meteo").input_value() == "auto" and page.locator("#source-repli").is_hidden())
                verifier_attribution(page, "fr", "retour à Lyon")
                sans_null(page, "source directe")
            if REEL:
                sauter("Source directe : MET Norway (repli, réponse), NWS, fournisseur inconnu", "le test joue des réponses de l'appareil et change de ville : il ne se joue que contre le faux appareil")
            else:
                etape("Source directe : MET Norway (repli, réponse), NWS, fournisseur inconnu", t_source_directe, page)

            def t_services():
                ouvrir(page, "Réfléchir")
                page.get_by_role("radio", name="Autre").click()
                page.locator("#llm-url").fill(f"http://127.0.0.1:{PORT_SERVICE}/v1")
                page.locator("#llm-cle").fill(CLE_FAUSSE)
                page.get_by_role("button", name="Tester").click()
                expect(page.locator("#det-llm .res.err")).to_be_visible()
                msg = page.locator("#det-llm .res.err").inner_text()
                verifier("clé refusée : phrase lisible", "refuse la clé" in msg, msg)
                verifier("clé refusée : le texte du service (qui cite la clé) n'est pas recopié", CLE_FAUSSE not in msg and "Incorrect" not in msg)
                page.screenshot(path=str(CAPTURES / "05-test-cle-refusee.png"))
                page.locator("#llm-cle").fill(CLE_BONNE)
                page.get_by_role("button", name="Tester").click()
                expect(page.locator("#det-llm .res.ok")).to_contain_text("Le service répond en", timeout=20000)
                verifier("test réussi : les modèles du service sont proposés", page.evaluate("document.querySelectorAll('#mod-llm option').length") == 3)
                page.locator("#llm-modele").fill("modele-essai-b")
                sans_null(page, "service testé")
                page.screenshot(path=str(CAPTURES / "06-test-ok.png"))
                page.locator("#det-llm").get_by_role("button", name="Enregistrer").click()   # la carte « La mer » a le sien aussi
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                expect(page.get_by_role("button", name=re.compile("^Réfléchir"))).to_contain_text("clé enregistrée")
                page.reload()
                expect(page.locator("#ville")).to_be_visible()
                ouvrir(page, "Réfléchir")
                expect(page.locator("#det-llm")).to_contain_text("Clé enregistrée")
                verifier("après rechargement : l'adresse est gardée, la clé n'est plus affichée",
                         page.locator("#llm-url").input_value().endswith(f":{PORT_SERVICE}/v1") and page.locator("#llm-cle").count() == 0)
                dom = page.content()
                verifier("la clé n'est nulle part dans la page (DOM)", CLE_BONNE not in dom and CLE_FAUSSE not in dom)
                verifier("aucun champ de la page ne contient la clé", not page.evaluate(f"[...document.querySelectorAll('input')].some(i => i.value.includes('{CLE_BONNE}'))"))
                sans_null(page, "service enregistré")
                page.screenshot(path=str(CAPTURES / "07-assistant-enregistre.png"), full_page=True)
                # service injoignable : phrase du tableau §8 de l'API
                ouvrir(page, "Réfléchir")
                ouvrir(page, "Parler")
                page.get_by_role("radio", name="Autre").click()
                page.locator("#tts-url").fill("http://127.0.0.1:1/v1")
                page.get_by_role("button", name="Tester").click()
                expect(page.locator("#det-tts .res.err")).to_contain_text("Le service ne répond pas", timeout=20000)
                verifier("service injoignable : « Le service ne répond pas. Est-il lancé… »", "Est-il lancé" in page.locator("#det-tts .res.err").inner_text())
                page.screenshot(path=str(CAPTURES / "08-test-injoignable.png"))
            SANS_SERVICES = "l'appareil répond 501 « non_disponible » (services STT/LLM/TTS : pas dans ce firmware)"
            if REEL:
                sauter("Assistant : saisir un service LLM et « Tester »", SANS_SERVICES)
            else:
                etape("Assistant : saisir un service LLM et « Tester »", t_services, page)

            def t_secret():
                code, svc = api(page, "GET", "/api/services")
                verifier("GET /api/services : cle_definie vrai, aucun champ de clé", code == 200 and svc["llm"]["cle_definie"] is True and "cle" not in svc["llm"], json.dumps(svc["llm"]))
                avec_requete = [r for r in registre if CLE_BONNE in r["requete"] or CLE_FAUSSE in r["requete"]]
                verifier("contrôle du contrôle : la clé a bien voyagé dans des requêtes", len(avec_requete) >= 3, f"{len(avec_requete)} requêtes")
                fuites = [r["url"] for r in registre if CLE_BONNE in r["corps"] or CLE_FAUSSE in r["corps"] or "Incorrect API key" in r["corps"]]
                verifier(f"la clé n'apparaît dans AUCUNE des {len(registre)} réponses de l'API", not fuites, str(fuites[:3]))
                code, exp = api(page, "GET", "/api/settings/export")
                verifier("export : sans clé ni mot de passe", CLE_BONNE not in json.dumps(exp) and "mot_de_passe" not in json.dumps(exp) and "sk-" not in json.dumps(exp))
            if REEL:
                sauter("Secrets : la clé n'est jamais lisible", "aucune clé n'existe dans ce firmware (le contrôle sur les réponses reste couvert par les tests du noyau)")
            else:
                etape("Secrets : la clé n'est jamais lisible", t_secret, page)

            def t_export_import():
                with page.expect_download() as dl:
                    page.get_by_role("button", name="Sauvegarder mes réglages").click()
                chemin = CAPTURES / "reglages-exporte.json"
                dl.value.save_as(str(chemin))
                texte = chemin.read_text(encoding="utf-8")
                f = json.loads(texte)
                verifier("export téléchargé : format, ville et service LLM présents", f["format"] == "ecran-autonome-reglages" and f["zone"]["nom"] == "Lyon" and f["services"]["llm"]["url"].endswith("/v1"))
                verifier("export téléchargé : aucune clé (ni la bonne, ni la fausse, ni « sk- »)", not re.search(r"sk-|cle-de-test|mot_de_passe", texte) and "cle" not in f["services"]["llm"])
                verifier("export : indique les clés à ressaisir", f["cles_a_ressaisir"] == ["llm"], str(f["cles_a_ressaisir"]))
                # import : une luminosité modifiée + une clé glissée dans le fichier (doit être ignorée et signalée)
                f["affichage"]["luminosite"] = 30
                f["services"]["llm"]["cle"] = "sk-injectee-par-le-fichier"
                entree = page.locator('input[aria-label="Fichier de réglages"]')
                entree.set_input_files(files=[{"name": "r.json", "mimeType": "application/json", "buffer": json.dumps(f).encode()}])
                expect(page.locator("#appareil")).to_contain_text("Ce fichier changerait")
                expect(page.locator("#appareil")).to_contain_text("Les clés ne s’importent pas.")
                sans_null(page, "simulation d'import")
                page.screenshot(path=str(CAPTURES / "09-import-simulation.png"), full_page=False)
                page.get_by_role("button", name="Appliquer").click()
                expect(page.locator("#luminosite")).to_have_value("30", timeout=15000)
                _, svc = api(page, "GET", "/api/services")
                verifier("import : la luminosité du fichier est appliquée", True)
                verifier("import : la clé glissée dans le fichier n'est pas enregistrée ni renvoyée",
                         "sk-injectee" not in json.dumps(svc) and not any("sk-injectee" in r["corps"] for r in registre if "/api/settings/import" in r["url"]) and svc["llm"]["cle_definie"] is True)
            if REEL:
                sauter("Export et import des réglages sans clés", "l'appareil répond 501 « non_disponible »")
            else:
                etape("Export et import des réglages sans clés", t_export_import, page)

            depart_affichage = {}  # état de l'appareil réel avant le parcours, remis à la fin

            def t_ecran():
                if REEL:
                    depart_affichage.update({k: v for k, v in api(page, "GET", "/api/display")[1].items() if k in ("langue_ecran", "langue_assistant", "theme", "luminosite")})
                else:
                    ouvrir(page, "Réfléchir")  # referme
                page.get_by_role("radio", name="Sombre").click()
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, d = api(page, "GET", "/api/display")
                verifier("thème Sombre enregistré sur l'appareil", d["theme"] == "sombre", d["theme"])
                page.locator("#luminosite").fill("55")
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, d = api(page, "GET", "/api/display")
                verifier("luminosité 55 % enregistrée", d["luminosite"] == 55, str(d["luminosite"]))
                page.locator("#ecran").scroll_into_view_if_needed()
                page.screenshot(path=str(CAPTURES / "10-ecran.png"))
                if REEL:  # remettre l'écran de l'appareil comme avant le parcours
                    api(page, "PUT", "/api/display", {k: depart_affichage[k] for k in ("theme", "luminosite") if k in depart_affichage})
            etape("Écran : thème et luminosité", t_ecran, page)

            def t_alertes():
                carte = page.locator("#alertes")
                _, depart = api(page, "GET", "/api/alerts")
                verifier("alertes : lecture (activées, période, niveau, adresse du service public pour le champ vide)",
                         isinstance(depart.get("actives"), bool) and 10 <= depart["periode_min"] <= 180 and depart["niveau_min"] in ("yellow", "orange", "red")
                         and depart["adresse_publique"].startswith("https://") and depart["niveaux"] == ["yellow", "orange", "red"], str(depart))
                carte.scroll_into_view_if_needed()
                carte.get_by_role("radio", name="Orange", exact=True).click()
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : niveau minimal Orange enregistré sur l'appareil", a["niveau_min"] == "orange", a["niveau_min"])
                carte.get_by_role("radio", name="Non", exact=True).click()
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : « Non » éteint les alertes sur l'appareil", a["actives"] is False, str(a["actives"]))
                carte.locator("#alertes-periode").fill("45")
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : période de 45 min enregistrée", a["periode_min"] == 45, str(a["periode_min"]))
                champ = carte.locator("#alertes-adresse")
                champ.fill("http://vigie.exemple.fr")
                champ.press("Tab")
                expect(champ).to_have_attribute("aria-invalid", "true")
                expect(carte.locator("div[aria-live] .res")).to_contain_text("https://")
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : une adresse en http:// est refusée avec une phrase, rien n'est enregistré", a["adresse"] == depart["adresse"], a["adresse"])
                champ.fill("  HTTPS://Vigie.Exemple.fr//  ")
                champ.press("Tab")
                expect(champ).to_have_value("https://Vigie.Exemple.fr")
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : l'adresse est enregistrée sous sa forme normalisée (schéma en minuscules, sans « / » final)", a["adresse"] == "https://Vigie.Exemple.fr", a["adresse"])
                verifier("alertes : l'adresse refusée ne laisse pas la mention d'erreur sur une adresse valide", champ.get_attribute("aria-invalid") is None)
                champ.fill("")
                champ.press("Tab")
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : une adresse vide remet le service public", a["adresse"] == "", a["adresse"])
                r = page.evaluate(DEPASSEMENTS)
                verifier("alertes : pas de défilement horizontal avec la carte ouverte", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                carte.screenshot(path=str(CAPTURES / "10b-alertes.png"))
                # remettre l'appareil comme avant le pas (l'appareil réel garde ses réglages)
                api(page, "PUT", "/api/alerts", {k: depart[k] for k in ("actives", "adresse", "periode_min", "niveau_min")})
                _, a = api(page, "GET", "/api/alerts")
                verifier("alertes : réglages remis comme avant le pas", all(a[k] == depart[k] for k in ("actives", "adresse", "periode_min", "niveau_min")), str(a))
            etape("Alertes officielles : niveau, période, adresse", t_alertes, page)

            def t_mer():
                cat = catalogue("fr")
                carte = page.locator("#mer")
                champ = carte.locator("#mer-q")
                resultats = carte.locator(".resultats button")
                retours = carte.locator("#mer-retours")
                vmin, vmax = carte.locator("#mer-vent-min"), carte.locator("#mer-vent-max")
                enregistrer_vent = carte.get_by_role("button", name=cat["action.enregistrer"], exact=True)
                _, alertes_depart = api(page, "GET", "/api/alerts")
                pub = alertes_depart["adresse_publique"]
                n0 = len(vigie_interceptees)
                page.add_init_script(SPION_FETCH)   # note les appels fetch vers Vigie (options passées par la page) à partir du prochain chargement
                page.reload()
                expect(carte).to_be_visible()
                try:
                    # ---- départ : registre vide, onglet, carte ----
                    code, mer0 = api(page, "GET", "/api/sea")
                    verifier("mer : le faux appareil démarre avec un registre vide, la plage 12 à 25 et les limites du contrat",
                             code == 200 and mer0 == {"spots": [], "choisi": "", "vent_min_kn": 12, "vent_max_kn": 25, "spots_max": 12, "vent_plafond_kn": 60}, str(mer0))
                    liens = [norm(x) for x in page.locator("nav.ancres a").all_inner_texts()]
                    i_mer = liens.index(norm(cat["nav.mer"])) if norm(cat["nav.mer"]) in liens else -99
                    verifier("mer : l'onglet « La mer » est juste après « Alertes » et juste avant « Assistant »",
                             i_mer == liens.index(norm(cat["nav.alertes"])) + 1 and liens[i_mer + 1] == norm(cat["nav.assistant"]), str(liens))
                    sections = page.evaluate("[...document.querySelectorAll('main > section')].map(s => s.id)")
                    verifier("mer : la carte est placée entre « alertes » et « assistant »", sections[:4] == ["ville", "alertes", "mer", "assistant"], str(sections))
                    carte.scroll_into_view_if_needed()
                    vide = carte.locator("#mer-registre .res.info")
                    verifier("mer : sans spot, la carte dit que l'écran n'affiche pas la mer",
                             carte.locator("#t-mer").inner_text() == cat["mer.titre"] and vide.count() == 1 and norm(vide.inner_text()) == norm(cat["mer.vide"]), vide.all_inner_texts().__str__())
                    verifier("mer : champ de recherche de type search, saisie automatique coupée",
                             champ.get_attribute("type") == "search" and champ.get_attribute("autocomplete") == "off")
                    verifier("mer : la plage de vent affiche 12 et 25, en nœuds entiers au clavier numérique, avec le texte d'aide",
                             vmin.input_value() == "12" and vmax.input_value() == "25" and vmin.get_attribute("inputmode") == "numeric"
                             and vmax.get_attribute("inputmode") == "numeric" and norm(carte.locator("#mer-vent-aide").inner_text()) == norm(cat["mer.vent_aide"]))
                    credit = carte.locator("p.aide", has_text="OpenStreetMap")
                    verifier("mer : le crédit des spots (OpenStreetMap, ODbL) est affiché", credit.count() == 1 and norm(credit.inner_text()) == norm(cat["mer.credit"]))
                    page.wait_for_timeout(500)
                    verifier("mer : rien n'est demandé à Vigie tant que la personne ne touche pas le champ (page chargée, 0,5 s d'attente)", len(vigie_interceptees) == n0)
                    capturer(page, carte, "mer-1-vide.png")

                    # ---- la lecture chez Vigie : au premier appui dans le champ, une seule fois ----
                    champ.click()
                    attendre_que(page, lambda: len(vigie_interceptees) > n0)
                    lectures = vigie_interceptees[n0:]
                    verifier(f"mer : au premier appui dans le champ, la page lit chez Vigie : un seul GET vers {pub}/v1/surf/spots (adresse du service public, le champ des alertes étant vide)",
                             len(lectures) == 1 and lectures[0]["methode"] == "GET" and lectures[0]["url"] == pub + "/v1/surf/spots", str(lectures))
                    verifier("mer : la lecture chez Vigie ne porte pas le jeton de l'appareil", len(lectures) == 1 and "authorization" not in lectures[0]["entetes"], str(lectures))
                    spion = page.evaluate("window.__vigie")
                    verifier("mer : la lecture chez Vigie se fait sans cookies (credentials: omit), en demandant du JSON, avec un délai (AbortController)",
                             len(spion) == 1 and spion[0]["credentials"] == "omit" and spion[0]["accept"] == "application/json" and spion[0]["signal"] is True, str(spion))

                    # ---- recherche ----
                    def textes():
                        return [norm(x) for x in resultats.all_inner_texts()]

                    champ.fill("e")
                    page.wait_for_timeout(300)
                    verifier("mer : un seul caractère ne lance pas de recherche (2 au moins)", resultats.count() == 0 and retours.locator(".res").count() == 0)
                    champ.fill("ecluse")
                    expect(resultats).to_have_count(1)
                    verifier("mer : « ecluse » (sans accent) trouve « Plage de l’Écluse », écrit « Nom — Lieu »", textes() == ["Plage de l’Écluse — Port-Essai"], str(textes()))
                    champ.fill("ÉCLUSE")
                    expect(resultats).to_have_count(1)
                    champ.fill("l'ecluse")
                    expect(resultats).to_have_count(1)
                    champ.fill("port-essai")
                    expect(resultats).to_have_count(1)
                    champ.fill("spot-essai-un")
                    expect(resultats).to_have_count(1)
                    verifier("mer : la recherche ignore la casse et les accents, et porte aussi sur la ponctuation de l'apostrophe, le lieu et l'identifiant", True)
                    champ.fill("grande")
                    expect(resultats).to_have_count(2)
                    verifier("mer : deux spots de même nom se distinguent par leur lieu", textes() == ["Grande Plage — Ville-Un", "Grande Plage — Ville-Deux"], str(textes()))
                    capturer(page, carte, "mer-2-recherche.png")
                    champ.fill("crique")
                    expect(resultats).to_have_count(1)
                    verifier("mer : un spot sans « name » prend « osm_name »", textes() == ["Crique sans nom — Hameau-Dernier"], str(textes()))
                    champ.fill("essai")
                    expect(resultats).to_have_count(6)
                    champ.fill("zzzz")
                    expect(retours.locator(".res.info")).to_be_visible()
                    verifier("mer : aucun résultat → la phrase le dit, avec la recherche entre guillemets",
                             resultats.count() == 0 and norm(retours.locator(".res.info").inner_text()) == norm(cat["mer.aucun"].replace("{q}", "zzzz")), retours.inner_text())
                    verifier("mer : la liste de Vigie n'a été lue qu'une seule fois pour une dizaine de recherches", len(vigie_interceptees) - n0 == 1, str(len(vigie_interceptees) - n0))

                    # ---- ajouter : le premier devient le spot de l'écran, le suivant ne le change pas ----
                    champ.fill("ecluse")
                    expect(resultats).to_have_count(1)
                    resultats.first.click()
                    expect(carte.locator(".spot")).to_have_count(1)
                    m = sea(page)
                    verifier("mer : premier spot ajouté → il est enregistré et il devient le spot choisi", m["spots"] == [UN] and m["choisi"] == UN["id"], str(m))
                    verifier("mer : la page envoie la liste entière et le choix, rien d'autre (le lieu n'est pas dans le nom)",
                             puts_sea()[-1] == {"spots": [UN], "choisi": UN["id"]}, str(puts_sea()[-1:]))
                    verifier("mer : après l'ajout, le champ est vidé, les résultats cachés, et la carte montre le spot coché",
                             champ.input_value() == "" and carte.locator(".resultats").is_hidden() and lignes_registre(carte) == [[UN["nom"], True]], str(lignes_registre(carte)))
                    champ.fill("baie")
                    expect(resultats).to_have_count(1)
                    resultats.first.click()
                    expect(carte.locator(".spot")).to_have_count(2)
                    m = sea(page)
                    verifier("mer : second spot ajouté en fin de liste → le choix ne bouge pas", m["spots"] == [UN, DEUX] and m["choisi"] == UN["id"], str(m))
                    verifier("mer : le second ajout envoie la liste entière dans l'ordre et le choix inchangé", puts_sea()[-1] == {"spots": [UN, DEUX], "choisi": UN["id"]}, str(puts_sea()[-1:]))
                    verifier("mer : la liste enregistrée est un groupe de boutons radio nommé « Spot affiché sur l’écran », un seul coché",
                             carte.get_by_role("radiogroup", name=cat["mer.affiche"]).count() == 1 and carte.get_by_role("radio").count() == 2
                             and lignes_registre(carte) == [[UN["nom"], True], [DEUX["nom"], False]], str(lignes_registre(carte)))
                    verifier("mer : chaque ligne a un bouton « Retirer » dont le nom accessible cite le spot",
                             carte.locator(".spot").first.locator("button").get_attribute("aria-label") == cat["mer.retirer_nom"].replace("{nom}", UN["nom"])
                             and carte.locator(".spot").first.locator("button").inner_text() == cat["mer.retirer"])
                    capturer(page, carte, "mer-3-liste.png")
                    # choisir l'autre
                    carte.locator(".spot").nth(1).locator("input").click()
                    verifier("mer : choisir l'autre spot → PUT avec la liste entière et le nouveau choix",
                             attendre_que(page, lambda: sea(page)["choisi"] == DEUX["id"]) and puts_sea()[-1] == {"spots": [UN, DEUX], "choisi": DEUX["id"]}, str(puts_sea()[-1:]))
                    verifier("mer : la carte montre le nouveau choix", attendre_que(page, lambda: lignes_registre(carte) == [[UN["nom"], False], [DEUX["nom"], True]]), str(lignes_registre(carte)))
                    # un doublon
                    nb_puts = len(puts_sea())
                    champ.fill("ecluse")
                    expect(resultats).to_have_count(1)
                    resultats.first.click()
                    expect(retours.locator(".res")).to_be_visible()
                    verifier("mer : ajouter un spot déjà dans la liste → la phrase le dit, aucune requête, rien ne change",
                             norm(retours.locator(".res").inner_text()) == norm(cat["mer.deja"]) and len(puts_sea()) == nb_puts and sea(page)["spots"] == [UN, DEUX], retours.inner_text())
                    # un nom de plus de 32 caractères
                    champ.fill("pointe")
                    expect(resultats).to_have_count(1)
                    resultats.first.click()
                    expect(carte.locator(".spot")).to_have_count(3)
                    m = sea(page)
                    nom = m["spots"][2]["nom"] if len(m["spots"]) == 3 else ""
                    verifier("mer : un nom de plus de 32 caractères est coupé à 32 caractères Unicode (l'émoji du 32e rang reste entier), sans le lieu",
                             m["spots"] == [UN, DEUX, TROIS] and len(nom) <= 32 and nom == NOM_COUPE and "Anse-Fictive" not in nom, repr(nom))
                    verifier("mer : le nom envoyé est du Unicode bien formé (aucune moitié d'émoji)", nom.encode("utf-8") is not None and m["choisi"] == DEUX["id"])
                    verifier("mer : la carte montre le nom coupé", lignes_registre(carte)[2] == [NOM_COUPE, False], str(lignes_registre(carte)))
                    # retirer un spot qui n'est pas le choisi
                    carte.locator(".spot").nth(2).locator("button").click()
                    expect(carte.locator(".spot")).to_have_count(2)
                    m = sea(page)
                    verifier("mer : retirer un spot qui n'est pas le choisi ne change pas le choix", m["spots"] == [UN, DEUX] and m["choisi"] == DEUX["id"]
                             and puts_sea()[-1] == {"spots": [UN, DEUX], "choisi": DEUX["id"]}, str(m))
                    champ.fill("grande")
                    expect(resultats).to_have_count(2)
                    resultats.first.click()
                    expect(carte.locator(".spot")).to_have_count(3)
                    verifier("mer : un homonyme (autre lieu) s'ajoute comme un autre spot", sea(page)["spots"] == [UN, DEUX, QUATRE], str(sea(page)))

                    # ---- Vigie injoignable : phrase dédiée, le registre reste modifiable ----
                    vigie["reponse"] = "coupure"
                    page.reload()
                    expect(carte).to_be_visible()
                    n1 = len(vigie_interceptees)
                    page.wait_for_timeout(500)
                    verifier("mer : après rechargement, la carte montre la liste enregistrée, sans rien demander à Vigie",
                             lignes_registre(carte) == [[UN["nom"], False], [DEUX["nom"], True], [QUATRE["nom"], False]] and len(vigie_interceptees) == n1, str(lignes_registre(carte)))
                    champ.click()
                    err = retours.locator(".res.err")
                    expect(err).to_be_visible()
                    phrase = norm(cat["mer.injoignable"].replace("{carte}", cat["alertes.titre"]))
                    verifier("mer : connexion coupée → la phrase d'erreur dédiée, qui nomme la carte « Alertes officielles »", norm(err.inner_text()) == phrase, err.inner_text())
                    reessayer = retours.get_by_role("button", name=cat["action.reessayer"])
                    verifier("mer : l'erreur propose « Réessayer »", reessayer.count() == 1)
                    capturer(page, carte, "mer-4-erreur-vigie.png")
                    carte.locator(".spot").nth(0).locator("input").click()
                    verifier("mer : pendant l'erreur de Vigie, la liste enregistrée reste modifiable (choisir un autre spot)",
                             attendre_que(page, lambda: sea(page)["choisi"] == UN["id"]), str(sea(page)))
                    verifier("mer : l'erreur de Vigie reste affichée après cette modification", err.count() == 1)
                    n2 = len(vigie_interceptees)
                    champ.click()
                    attendre_que(page, lambda: len(vigie_interceptees) > n2)
                    expect(err).to_be_visible()
                    verifier("mer : après un échec, un nouvel appui dans le champ refait la lecture, une seule fois", len(vigie_interceptees) == n2 + 1, str(len(vigie_interceptees) - n2))
                    n2 = len(vigie_interceptees)
                    champ.press_sequentially("abc")
                    page.wait_for_timeout(400)
                    verifier("mer : après un échec, taper ne relance pas la lecture à chaque frappe", len(vigie_interceptees) == n2 and err.count() == 1, str(len(vigie_interceptees) - n2))
                    for repond, quoi in ((500, "statut 500"), ("illisible", "réponse qui n'est pas du JSON"), ({"count": 0}, "JSON sans liste de spots")):
                        vigie["reponse"] = repond
                        n3 = len(vigie_interceptees)
                        reessayer.click()
                        attendre_que(page, lambda n3=n3: len(vigie_interceptees) > n3)
                        expect(err).to_be_visible()
                        verifier(f"mer : {quoi} → la même phrase d'erreur, une seule requête par « Réessayer »", norm(err.inner_text()) == phrase and len(vigie_interceptees) == n3 + 1, err.inner_text())
                    vigie["reponse"] = liste_par_adresse
                    n4 = len(vigie_interceptees)
                    reessayer.click()
                    expect(err).to_have_count(0)
                    champ.fill("baie")
                    expect(resultats).to_have_count(1)
                    verifier("mer : « Réessayer » avec une Vigie rétablie → l'erreur disparaît et la recherche répond", len(vigie_interceptees) == n4 + 1 and textes() == ["Baie des Essais — Île-Test"], str(textes()))
                    champ.fill("")

                    # ---- retirer : le choix passe au spot qui prend la place ----
                    carte.locator(".spot").nth(0).locator("button").click()
                    expect(carte.locator(".spot")).to_have_count(2)
                    m = sea(page)
                    verifier("mer : retirer le spot choisi (le premier) → le choix passe au spot qui prend sa place",
                             m["spots"] == [DEUX, QUATRE] and m["choisi"] == DEUX["id"] and puts_sea()[-1] == {"spots": [DEUX, QUATRE], "choisi": DEUX["id"]}, str(m))
                    carte.locator(".spot").nth(1).locator("input").click()
                    attendre_que(page, lambda: sea(page)["choisi"] == QUATRE["id"])
                    attendre_que(page, lambda: lignes_registre(carte) == [[DEUX["nom"], False], [QUATRE["nom"], True]])
                    carte.locator(".spot").nth(1).locator("button").click()
                    expect(carte.locator(".spot")).to_have_count(1)
                    m = sea(page)
                    verifier("mer : retirer le spot choisi quand il est le dernier → le choix passe au précédent", m["spots"] == [DEUX] and m["choisi"] == DEUX["id"], str(m))
                    carte.locator(".spot").nth(0).locator("button").click()
                    expect(carte.locator("#mer-registre .res.info")).to_be_visible()
                    m = sea(page)
                    verifier("mer : retirer le dernier spot → registre vide, choix vide, et la phrase « aucun spot » revient",
                             m["spots"] == [] and m["choisi"] == "" and puts_sea()[-1] == {"spots": [], "choisi": ""}
                             and norm(carte.locator("#mer-registre .res.info").inner_text()) == norm(cat["mer.vide"]), str(m))

                    # ---- plage de vent ----
                    nb_puts = len(puts_sea())
                    vmin.fill("20")
                    vmax.fill("10")
                    enregistrer_vent.click()
                    err_vent = carte.locator("#mer-vent-retours .res.err")
                    expect(err_vent).to_be_visible()
                    verifier("mer : minimum au-dessus du maximum → la phrase de l'appareil, dans la langue de l'écran, sous les champs",
                             norm(err_vent.inner_text()) == norm(PHRASE_VENT), err_vent.inner_text())
                    verifier("mer : le refus n'a rien enregistré (12 et 25 sur l'appareil)", (sea(page)["vent_min_kn"], sea(page)["vent_max_kn"]) == (12, 25))
                    verifier("mer : la page a envoyé les deux nombres entiers tels quels ; le champ minimum est signalé (le corps contenait vent_min_kn)",
                             puts_sea()[-1] == {"vent_min_kn": 20, "vent_max_kn": 10} and vmin.get_attribute("aria-invalid") == "true" and vmax.get_attribute("aria-invalid") is None,
                             str(puts_sea()[-1:]))
                    capturer(page, carte, "mer-5-vent-refuse.png")
                    nb_puts = len(puts_sea())
                    vmin.fill("1.5")
                    enregistrer_vent.click()
                    expect(err_vent).to_be_visible()
                    verifier("mer : un nombre non entier est arrêté par la page (aucune requête), avec sa phrase",
                             len(puts_sea()) == nb_puts and vmin.get_attribute("aria-invalid") == "true" and norm(err_vent.inner_text()) == norm(cat["mer.vent_entiers"]), err_vent.inner_text())
                    vmin.fill("")
                    enregistrer_vent.click()
                    expect(err_vent).to_be_visible()
                    verifier("mer : un champ vide est arrêté de la même façon", len(puts_sea()) == nb_puts and norm(err_vent.inner_text()) == norm(cat["mer.vent_entiers"]), err_vent.inner_text())
                    vmin.fill("14")
                    vmax.fill("28")
                    enregistrer_vent.click()
                    verifier("mer : 14 à 28 nœuds → enregistré sur l'appareil, mention d'erreur effacée, seuls les deux nombres sont envoyés",
                             attendre_que(page, lambda: (sea(page)["vent_min_kn"], sea(page)["vent_max_kn"]) == (14, 28)) and puts_sea()[-1] == {"vent_min_kn": 14, "vent_max_kn": 28}
                             and err_vent.count() == 0 and vmin.get_attribute("aria-invalid") is None, str(puts_sea()[-1:]))
                    page.reload()
                    expect(carte).to_be_visible()
                    verifier("mer : après rechargement de la page, la plage de vent relue est 14 à 28", vmin.input_value() == "14" and vmax.input_value() == "28", f"{vmin.input_value()} / {vmax.input_value()}")

                    # ---- adresse de Vigie du champ des alertes (avec des « / » en trop, que la page retire) ----
                    api(page, "PUT", "/api/alerts", {"adresse": VIGIE_ESSAI})

                    def alertes_barres(route):
                        if route.request.method != "GET":
                            return route.fallback()
                        rep = route.fetch()
                        j = rep.json()
                        j["adresse"] = VIGIE_ESSAI + "///"
                        route.fulfill(status=rep.status, content_type="application/json", body=json.dumps(j))
                    page.route("**/api/alerts", alertes_barres)
                    page.reload()
                    expect(carte).to_be_visible()
                    n5 = len(vigie_interceptees)
                    champ.click()
                    attendre_que(page, lambda: len(vigie_interceptees) > n5)
                    lectures = vigie_interceptees[n5:]
                    verifier(f"mer : avec une adresse dans « Alertes officielles », la page lit chez elle ({VIGIE_ESSAI}), sans « / » final, et plus chez le service public",
                             len(lectures) == 1 and lectures[0]["url"] == VIGIE_ESSAI + "/v1/surf/spots", str(lectures))
                    page.unroute("**/api/alerts")
                    champ.fill("lot")
                    expect(resultats).to_have_count(8)
                    verifier("mer : dix spots répondent à « lot » → la page en montre 8 au plus", resultats.count() == 8)
                    champ.fill("invalide")
                    expect(retours.locator(".res.info")).to_be_visible()
                    verifier("mer : un spot de Vigie dont l'identifiant serait refusé par l'appareil n'est pas proposé", resultats.count() == 0 and norm(retours.locator(".res.info").inner_text()) == norm(cat["mer.aucun"].replace("{q}", "invalide")))
                    champ.fill("")

                    # ---- liste pleine : 12 spots ----
                    pleins = [{"id": f"spot-plein-{i:02d}", "nom": f"Spot plein {i:02d} ".ljust(32, "x")} for i in range(1, 13)]
                    pleins[11]["nom"] = "W" * 32   # 32 lettres larges, sans espace : la ligne doit se couper, pas déborder
                    code, m = api(page, "PUT", "/api/sea", {"spots": pleins, "choisi": pleins[0]["id"]})
                    verifier("mer : 12 spots (le maximum) sont acceptés", code == 200 and len(m["spots"]) == 12, str(code))
                    page.reload()
                    expect(carte).to_be_visible()
                    n6 = len(vigie_interceptees)
                    verifier("mer : liste pleine → le champ de recherche est désactivé et une phrase le dit",
                             champ.is_disabled() and carte.locator("#mer-pleine").is_visible() and norm(carte.locator("#mer-pleine").inner_text()) == norm(cat["mer.pleine"].replace("{n}", "12"))
                             and carte.locator(".spot").count() == 12, carte.locator("#mer-pleine").inner_text())
                    r = page.evaluate(DEPASSEMENTS)
                    verifier("mer : 12 spots aux noms de 32 caractères, 390 px : pas de défilement horizontal", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                    verifier("mer : champ désactivé → aucune lecture chez Vigie", len(vigie_interceptees) == n6)
                    carte.locator(".spot").nth(11).locator("button").click()
                    expect(carte.locator(".spot")).to_have_count(11)
                    verifier("mer : un spot retiré de la liste pleine → le champ redevient utilisable, la phrase disparaît",
                             champ.is_enabled() and carte.locator("#mer-pleine").is_hidden())
                    sans_null(page, "carte Mer")

                    # ---- le contrat côté faux appareil (la page en dépend) ----
                    api(page, "PUT", "/api/sea", {"spots": [], "choisi": ""})
                    A, B = {"id": "a-1", "nom": "Alpha"}, {"id": "b_2", "nom": "Bêta"}

                    def refus(corps, ident, champ_attendu):
                        code, r = api(page, "PUT", "/api/sea", corps)
                        e = r.get("erreur", {}) if isinstance(r, dict) else {}
                        return code == 422 and e.get("id") == ident and e.get("champ") == champ_attendu and bool(e.get("message"))
                    cas = [({"spots": "x"}, "spots_invalide", "spots"), ({"spots": [3]}, "spots_invalide", "spots"), ({"spots": [{"id": "a"}]}, "spots_invalide", "spots"),
                           ({"spots": [{"id": "A-1", "nom": "x"}]}, "spot_invalide", "spots"), ({"spots": [{"id": "a\u0000b", "nom": "x"}]}, "spot_invalide", "spots"),
                           ({"spots": [{"id": "a" * 65, "nom": "x"}]}, "spot_invalide", "spots"), ({"spots": [{"id": "a", "nom": ""}]}, "nom_invalide", "spots"),
                           ({"spots": [{"id": "a", "nom": "x" * 33}]}, "nom_invalide", "spots"), ({"spots": [{"id": "a", "nom": "   "}]}, "nom_invalide", "spots"),
                           ({"spots": [{"id": "a", "nom": "a\u0007b"}]}, "nom_invalide", "spots"), ({"spots": [A, A]}, "spot_double", "spots"),
                           ({"spots": [{"id": f"s{i}", "nom": "n"} for i in range(13)]}, "trop_de_spots", "spots"),
                           ({"spots": [A], "choisi": "b_2"}, "choix_inconnu", "choisi"), ({"spots": [A], "choisi": ""}, "choix_inconnu", "choisi"),
                           ({"spots": [], "choisi": "a-1"}, "choix_inconnu", "choisi"), ({"choisi": 3}, "choix_inconnu", "choisi"),
                           ({"vent_min_kn": 12.5}, "vent_invalide", "vent_min_kn"), ({"vent_max_kn": 25.5}, "vent_invalide", "vent_max_kn"),
                           ({"vent_min_kn": "12"}, "vent_invalide", "vent_min_kn"), ({"vent_min_kn": 0, "vent_max_kn": 25}, "vent_invalide", "vent_min_kn"),
                           ({"vent_min_kn": 30}, "vent_invalide", "vent_min_kn"), ({"vent_max_kn": 61}, "vent_invalide", "vent_max_kn"),
                           ({"vent_max_kn": 10}, "vent_invalide", "vent_max_kn"), ({"vent_min_kn": 12, "vent_max_kn": 12}, "vent_invalide", "vent_min_kn")]
                    ratees = [c[0] for c in cas if not refus(*c)]
                    verifier(f"mer : le faux appareil refuse comme le contrat ({len(cas)} cas : id, nom, doublon, liste pleine, choix, vent ; id stable et champ)", not ratees, str(ratees[:3]))
                    code, rien = api(page, "PUT", "/api/sea", {"spots": [A], "vent_min_kn": 5, "vent_max_kn": 5})
                    verifier("mer : tout ou rien → une liste valide avec un vent refusé n'enregistre aucun des deux", code == 422 and sea(page)["spots"] == [] and sea(page)["vent_min_kn"] == 14, str(sea(page)))
                    code, m = api(page, "PUT", "/api/sea", {"spots": [{**A, "lat": 1}, B]})
                    verifier("mer : spots sans choisi sur un registre vide → le premier ; les champs en trop d'un objet sont ignorés",
                             code == 200 and m["spots"] == [A, B] and m["choisi"] == "a-1" and set(m) == {"spots", "choisi", "vent_min_kn", "vent_max_kn", "spots_max", "vent_plafond_kn"}, str(m))
                    api(page, "PUT", "/api/sea", {"choisi": "b_2"})
                    code, m = api(page, "PUT", "/api/sea", {"spots": [A, B, {"id": "c", "nom": "Ç"}]})
                    verifier("mer : spots sans choisi → l'appareil garde le même spot choisi s'il est encore dans la liste", m["choisi"] == "b_2", str(m))
                    code, m = api(page, "PUT", "/api/sea", {"spots": [A]})
                    verifier("mer : spots sans choisi, le spot choisi n'y est plus → le premier", m["choisi"] == "a-1", str(m))
                    code, m = api(page, "PUT", "/api/sea", {})
                    verifier("mer : un PUT qui ne change rien répond 200 avec l'état enregistré", code == 200 and m["spots"] == [A] and m["choisi"] == "a-1", str(m))
                    code, m = api(page, "PUT", "/api/sea", {"spots": []})
                    verifier("mer : spots vide sans choisi → choisi vide", code == 200 and m["spots"] == [] and m["choisi"] == "", str(m))
                finally:
                    page.unroute("**/api/alerts")
                    api(page, "PUT", "/api/alerts", {"adresse": alertes_depart["adresse"]})
                    api(page, "PUT", "/api/sea", {"spots": [], "choisi": "", "vent_min_kn": 12, "vent_max_kn": 25})
                    vigie["reponse"] = liste_par_adresse
                _, a = api(page, "GET", "/api/alerts")
                verifier("mer : adresse des alertes et registre remis comme avant le pas",
                         a["adresse"] == alertes_depart["adresse"] and sea(page) == {"spots": [], "choisi": "", "vent_min_kn": 12, "vent_max_kn": 25, "spots_max": 12, "vent_plafond_kn": 60}, str(sea(page)))
            if REEL:
                sauter("Mer : registre des spots, recherche chez Vigie (interceptée), plage de vent", "le parcours remplit le registre de l'appareil et change l'adresse des alertes : il ne se joue que contre le faux appareil")
            else:
                etape("Mer : registre des spots, recherche chez Vigie (interceptée), plage de vent", t_mer, page)

            def t_mer_delai():
                # Le délai de 15 s de la lecture chez Vigie : horloge simulée du navigateur (aucune attente réelle)
                c8, p8 = nouveau_contexte(nav)
                try:
                    p8.clock.install()
                    vigie["reponse"] = "silence"
                    appairer(p8)
                    carte = p8.locator("#mer")
                    retours = carte.locator("#mer-retours")
                    n = len(vigie_interceptees)
                    carte.locator("#mer-q").click()
                    attendre_que(p8, lambda: len(vigie_interceptees) > n)
                    p8.clock.fast_forward(13000)
                    verifier("mer : Vigie qui ne répond pas, 13 s après → la page attend encore (« Chargement… », pas d'erreur)",
                             len(vigie_interceptees) == n + 1 and retours.locator(".res.err").count() == 0 and retours.locator(".res.info").count() == 1, retours.inner_text())
                    p8.clock.fast_forward(3000)
                    expect(retours.locator(".res.err")).to_be_visible()
                    verifier("mer : au-delà de 15 s sans réponse, la page abandonne et dit que Vigie est injoignable",
                             norm(retours.locator(".res.err").inner_text()) == norm(catalogue("fr")["mer.injoignable"].replace("{carte}", catalogue("fr")["alertes.titre"])), retours.inner_text())
                finally:
                    vigie["reponse"] = liste_par_adresse
                    liberer_vigie()
                    c8.close()
            if REEL:
                sauter("Mer : délai de 15 s de la lecture chez Vigie", "joué seulement contre le faux appareil (comme l'étape précédente)")
            else:
                etape("Mer : délai de 15 s de la lecture chez Vigie", t_mer_delai)

            def t_mer_langues():
                # Allemand et turc : tous les états de la carte « La mer », sans mot français, phrases de l'appareil dans la langue de l'écran
                fr = catalogue("fr")
                ids_page = list(lire_yaml(RACINE / "i18n" / "page.yaml"))
                c9, p9 = nouveau_contexte(nav, largeur=360)
                langue_depart = None
                try:
                    appairer(p9)
                    langue_depart = api(p9, "GET", "/api/display")[1]["langue_ecran"]
                    for code, capture in (("de", "mer-6-allemand.png"), ("tr", "mer-7-turc.png")):
                        cat = catalogue(code)
                        traduits = lire_yaml(RACINE / "i18n" / f"{code}.yaml")
                        vigie["reponse"] = liste_par_adresse
                        api(p9, "PUT", "/api/display", {"langue_ecran": code})
                        api(p9, "PUT", "/api/sea", {"spots": [], "choisi": "", "vent_min_kn": 12, "vent_max_kn": 25})
                        p9.reload()
                        expect(p9.locator("#ville")).to_be_visible()
                        carte = p9.locator("#mer")
                        champ, retours, resultats = carte.locator("#mer-q"), carte.locator("#mer-retours"), carte.locator(".resultats button")
                        err_vent = carte.locator("#mer-vent-retours .res.err")
                        enregistrer = carte.get_by_role("button", name=cat["action.enregistrer"], exact=True)
                        vus = []   # tout ce que la carte a dit, état après état

                        def noter(vus=vus, carte=carte):
                            vus.append(tout_le_texte(carte))

                        # 1. registre vide
                        liens = [norm(x) for x in p9.locator("nav.ancres a").all_inner_texts()]
                        verifier(f"{code} : l'onglet « {cat['nav.mer']} » est après « {cat['nav.alertes']} » et avant « {cat['nav.assistant']} »",
                                 norm(cat["nav.mer"]) in liens and liens.index(norm(cat["nav.mer"])) == liens.index(norm(cat["nav.alertes"])) + 1
                                 and liens[liens.index(norm(cat["nav.mer"])) + 1] == norm(cat["nav.assistant"]), str(liens))
                        verifier(f"{code} : état vide — titre, phrase, champ, crédit, plage de vent, aide, bouton, dans la langue",
                                 carte.locator("#t-mer").inner_text() == cat["mer.titre"] and norm(carte.locator("#mer-registre .res.info").inner_text()) == norm(cat["mer.vide"])
                                 and norm(carte.locator("label[for=mer-q]").inner_text()) == norm(cat["mer.ajouter"]) and champ.get_attribute("placeholder") == cat["mer.recherche_ph"]
                                 and norm(carte.locator("p.aide", has_text="OpenStreetMap").inner_text()) == norm(cat["mer.credit"])
                                 and norm(carte.locator("#mer-vent-l").inner_text()) == norm(cat["mer.vent"]) and norm(carte.locator("label[for=mer-vent-min]").inner_text()) == norm(cat["mer.vent_min"])
                                 and norm(carte.locator("label[for=mer-vent-max]").inner_text()) == norm(cat["mer.vent_max"]) and norm(carte.locator("#mer-vent-aide").inner_text()) == norm(cat["mer.vent_aide"])
                                 and norm(enregistrer.inner_text()) == norm(cat["action.enregistrer"]))
                        noter()
                        # 2. recherche, aucun résultat, ajout, doublon
                        champ.click()
                        champ.fill("ecluse")
                        expect(resultats).to_have_count(1)
                        champ.fill("zzzz")
                        expect(retours.locator(".res.info")).to_be_visible()
                        verifier(f"{code} : aucun résultat — phrase dans la langue, recherche citée", norm(retours.locator(".res.info").inner_text()) == norm(cat["mer.aucun"].replace("{q}", "zzzz")), retours.inner_text())
                        noter()
                        for requete, attendu in (("ecluse", UN["nom"]), ("baie", DEUX["nom"])):
                            champ.fill(requete)
                            expect(resultats).to_have_count(1)
                            resultats.first.click()
                            attendre_que(p9, lambda attendu=attendu, carte=carte: any(attendu == ligne[0] for ligne in lignes_registre(carte)))
                        verifier(f"{code} : deux spots ajoutés, le premier est coché", lignes_registre(carte) == [[UN["nom"], True], [DEUX["nom"], False]], str(lignes_registre(carte)))
                        verifier(f"{code} : liste — groupe « {cat['mer.affiche']} », bouton « {cat['mer.retirer']} » et son nom accessible dans la langue",
                                 carte.get_by_role("radiogroup", name=cat["mer.affiche"]).count() == 1 and carte.locator(".spot").first.locator("button").inner_text() == cat["mer.retirer"]
                                 and carte.locator(".spot").first.locator("button").get_attribute("aria-label") == cat["mer.retirer_nom"].replace("{nom}", UN["nom"]))
                        noter()
                        champ.fill("ecluse")
                        expect(resultats).to_have_count(1)
                        resultats.first.click()
                        expect(retours.locator(".res")).to_be_visible()
                        verifier(f"{code} : doublon — phrase dans la langue", norm(retours.locator(".res").inner_text()) == norm(cat["mer.deja"]), retours.inner_text())
                        noter()
                        # 3. plage de vent : la phrase de l'appareil arrive dans la langue de l'écran ; la phrase de la page aussi
                        carte.locator("#mer-vent-min").fill("20")
                        carte.locator("#mer-vent-max").fill("10")
                        enregistrer.click()
                        expect(err_vent).to_be_visible()
                        phrase = traduits[PHRASE_VENT]
                        verifier(f"{code} : vent refusé — la phrase de l'appareil est traduite (et n'est pas le français)", norm(err_vent.inner_text()) == norm(phrase) and phrase != PHRASE_VENT, err_vent.inner_text())
                        noter()
                        capturer(p9, carte, capture)
                        carte.locator("#mer-vent-min").fill("1.5")
                        enregistrer.click()
                        expect(err_vent).to_be_visible()
                        verifier(f"{code} : vent non entier — phrase de la page dans la langue", norm(err_vent.inner_text()) == norm(cat["mer.vent_entiers"]), err_vent.inner_text())
                        noter()
                        # 4. lecture chez Vigie en cours (Vigie ne répond pas), puis Vigie injoignable
                        vigie["reponse"] = "silence"
                        p9.reload()
                        expect(carte).to_be_visible()
                        champ.click()
                        expect(retours.locator(".res.info")).to_be_visible()
                        verifier(f"{code} : lecture chez Vigie en cours — « {cat['mer.chargement']} »", norm(retours.locator(".res.info").inner_text()) == norm(cat["mer.chargement"]), retours.inner_text())
                        noter()
                        liberer_vigie()
                        vigie["reponse"] = "coupure"
                        p9.reload()
                        expect(carte).to_be_visible()
                        champ.click()
                        expect(retours.locator(".res.err")).to_be_visible()
                        verifier(f"{code} : Vigie injoignable — phrase et « {cat['action.reessayer']} » dans la langue, la carte « {cat['alertes.titre']} » nommée",
                                 norm(retours.locator(".res.err").inner_text()) == norm(cat["mer.injoignable"].replace("{carte}", cat["alertes.titre"]))
                                 and retours.get_by_role("button", name=cat["action.reessayer"]).count() == 1, retours.inner_text())
                        noter()
                        # 5. liste pleine, 360 px
                        pleins = [{"id": f"spot-plein-{i:02d}", "nom": f"Spot plein {i:02d}"} for i in range(1, 13)]
                        api(p9, "PUT", "/api/sea", {"spots": pleins, "choisi": pleins[0]["id"]})
                        p9.reload()
                        expect(carte).to_be_visible()
                        verifier(f"{code} : liste pleine — phrase dans la langue, champ désactivé",
                                 champ.is_disabled() and norm(carte.locator("#mer-pleine").inner_text()) == norm(cat["mer.pleine"].replace("{n}", "12")), carte.locator("#mer-pleine").inner_text())
                        noter()
                        r = p9.evaluate(DEPASSEMENTS)
                        verifier(f"{code} : 360 px, 12 spots, pas de défilement horizontal", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                        # bilan : aucun français, aucun identifiant brut dans tout ce que la carte a dit
                        dit = norm(" ".join(vus))
                        bruts = [i for i in ids_page if i in dit]
                        verifier(f"{code} : aucun identifiant brut (« mer.vide ») dans la carte", not bruts, str(bruts[:4]))
                        propres = [v for k, v in fr.items() if "{" not in v and len(v) >= 6 and v != cat.get(k)]
                        restes = sorted({norm(v) for v in propres if norm(v) in dit})
                        verifier(f"{code} : aucun texte français dans la carte ({len(vus)} états, libellés accessibles compris)", not restes, str(restes[:4]))
                        cles = [k for k in fr if k.startswith("mer.")] + ["nav.mer", "alertes.titre", "action.reessayer", "action.enregistrer"]
                        morceaux = sorted({norm(m) for k in cles if fr[k] != cat[k] for m in re.split(r"\{[^}]*\}", fr[k]) if len(norm(m)) >= 4})
                        restes = [m for m in morceaux if m in dit]
                        verifier(f"{code} : aucun morceau français des phrases à trous dans la carte", not restes, str(restes[:4]))
                finally:
                    vigie["reponse"] = liste_par_adresse
                    try:
                        api(p9, "PUT", "/api/sea", {"spots": [], "choisi": "", "vent_min_kn": 12, "vent_max_kn": 25})
                        if langue_depart:
                            api(p9, "PUT", "/api/display", {"langue_ecran": langue_depart})
                    except Exception:  # noqa: BLE001
                        pass
                    liberer_vigie()
                    c9.close()
            if REEL:
                sauter("Mer : allemand et turc, tous les états de la carte", "joué seulement contre le faux appareil (comme l'étape « Mer »)")
            else:
                etape("Mer : allemand et turc, tous les états de la carte", t_mer_langues)

            def t_wifi():
                page.get_by_role("button", name="Changer de réseau").click()
                page.get_by_role("button", name=re.compile("^Voisin_5G")).click()
                page.locator("#wifi-mdp").fill("mauvais")
                page.get_by_role("button", name="Se connecter").click()
                expect(page.locator("#wifi .res.err")).to_contain_text("Mot de passe refusé", timeout=20000)
                page.screenshot(path=str(CAPTURES / "11-wifi-refuse.png"))
                _, w = api(page, "GET", "/api/wifi")
                verifier("Wi-Fi refusé : l'ancien réseau est conservé", w["ssid"] == "Maison", json.dumps(w))
                page.get_by_role("button", name="Changer de réseau").click()
                page.get_by_role("button", name=re.compile("^Café Libre")).click()
                page.get_by_role("button", name="Se connecter").click()
                expect(page.locator("#toast")).to_contain_text("Connecté à", timeout=20000)
                verifier("réseau ouvert : connexion réussie", api(page, "GET", "/api/wifi")[1]["ssid"] == "Café Libre")
            if REEL:
                sauter("Wi-Fi : mot de passe refusé puis réseau ouvert", "l'appareil répond 501 « non_disponible » (changer de réseau : pas dans ce firmware ; mode station seulement)")
            else:
                etape("Wi-Fi : mot de passe refusé puis réseau ouvert", t_wifi, page)

            def depot(contenu, nom="ecran.bin"):
                page.locator('input[aria-label="Fichier de mise à jour"]').set_input_files(files=[{"name": nom, "mimeType": "application/octet-stream", "buffer": contenu}])
                page.get_by_role("button", name="Envoyer à l’écran").click()

            def reappairer():
                expect(page.get_by_text(re.compile("a redémarré"))).to_be_visible(timeout=40000)
                page.locator("#code").fill(CODE)
                expect(page.locator("#appareil")).to_be_visible()

            def t_maj():
                page.locator("#appareil").scroll_into_view_if_needed()
                depot(b"MZ" + bytes(2000), "pas-un-firmware.bin")
                expect(page.locator("#appareil .res.err")).to_contain_text("n’est pas un firmware", timeout=15000)
                verifier("fichier qui n'est pas un firmware : refusé avec une phrase", True)
                depot(b"\xe9" + b"CASSEE" + bytes(4000), "cassee.bin")
                reappairer()
                expect(page.locator("#appareil .res.err")).to_contain_text("n’a pas démarré correctement", timeout=15000)
                verifier("image cassée : retour arrière automatique, version inchangée (0.1.0)", "Version 0.1.0" in page.locator("#appareil").inner_text())
                page.screenshot(path=str(CAPTURES / "12-retour-arriere-auto.png"))
                depot(b"\xe9" + bytes(4000), "bonne.bin")
                reappairer()
                expect(page.locator("#appareil .res.ok")).to_contain_text("Mise à jour réussie", timeout=15000)
                verifier("bonne image : nouvelle version 0.1.1", "Version 0.1.1" in page.locator("#appareil").inner_text())
                page.get_by_role("button", name="Revenir à la version 0.1.0").click()
                page.get_by_role("button", name="Revenir", exact=True).click()
                reappairer()
                expect(page.locator("#appareil")).to_contain_text("Version 0.1.0", timeout=15000)
                verifier("retour arrière demandé : version 0.1.0 remise", "Retour à la version 0.1.0" in page.locator("#appareil").inner_text())
                verifier("le jeton a disparu au redémarrage (la page a redemandé le code)", True)
            if REEL:
                sauter("Mise à jour par la page, avec retour arrière", "l'appareil répond 501 « non_disponible » (mise à jour par la page : pas dans ce firmware)")
            else:
                etape("Mise à jour par la page, avec retour arrière", t_maj, page)

            def t_verrou():
                brut = lambda c: page.request.post(BASE + "/api/pair", data=json.dumps({"code": c}), headers={"Content-Type": "application/json"})  # noqa: E731
                statuts = [brut("111111").status for _ in range(5)]
                verifier("appairage : 4 mauvais codes en 403, le 5e verrouille (429)", statuts == [403, 403, 403, 403, 429], str(statuts))
                r = brut(CODE)
                verifier("appairage verrouillé : même le bon code est refusé, avec une phrase", r.status == 429 and "Trop d" in r.json()["erreur"]["message"], r.text()[:120])
                time.sleep(VERROU_S + 0.7)
                verifier("appairage : le verrou se lève", brut(CODE).status == 200)
            etape("Appairage : verrou après 5 essais ratés", t_verrou, page)

            # ---- captures clair / sombre, plein cadre, et contrôle du débordement ----
            def t_themes():
                ctx2, p2 = nouveau_contexte(nav, sombre=True)
                appairer(p2)
                ouvrir(p2, "Réfléchir")
                p2.screenshot(path=str(CAPTURES / "13-sombre-assistant.png"), full_page=True)
                p2.locator("#ville").scroll_into_view_if_needed()
                ctx2.close()
                ctx3, p3 = nouveau_contexte(nav, sombre=False)
                appairer(p3)
                p3.screenshot(path=str(CAPTURES / "14-clair-complet.png"), full_page=True)
                ctx3.close()
                verifier("captures clair et sombre enregistrées", True)
            etape("Rendu clair et sombre", t_themes, page)

            def t_largeurs():
                for largeur in (360, 390, 430):
                    c, pg = nouveau_contexte(nav, largeur=largeur)
                    appairer(pg)
                    ouvrir(pg, "Réfléchir")
                    r = pg.evaluate(DEPASSEMENTS)
                    verifier(f"{largeur} px : pas de défilement horizontal (carte « Réfléchir » ouverte)", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                    c.close()
            etape("Largeurs de téléphone", t_largeurs)

            # ---- sept langues : sélecteurs distincts, fichiers chargés à la demande, repli, attribution ----
            def t_langues():
                src = (RACINE / "components/core/include/ecran/meteo_wmo.hpp").read_text(encoding="utf-8")
                libelles_fr = re.findall(r'return \{"([^"]+)", ', src)
                ids = list(lire_yaml(RACINE / "i18n" / "page.yaml"))
                fr = catalogue("fr")
                cle_code_faux = "Ce code n’est pas le bon. Regardez l’écran de l’appareil."
                c4, p4 = nouveau_contexte(nav, largeur=360)
                depart_langues = {}  # langues de l'appareil avant le parcours (remises à la fin sur l'appareil réel)
                fichiers_vus = []
                p4.on("request", lambda r: fichiers_vus.append(r.url.rsplit("/", 1)[-1]) if "/lang/" in r.url else None)
                try:
                    appairer(p4)
                    d = api(p4, "GET", "/api/display")[1]
                    noms = [x["nom"] for x in d["langues"]]
                    verifier("langues : l'appareil propose sept langues dans l'ordre figé (fr en de nl es it tr)", [x["code"] for x in d["langues"]] == CODES_LANGUES, str(d["langues"]))
                    verifier("langues : chaque langue porte son nom dans sa propre langue", noms == ["Français", "English", "Deutsch", "Nederlands", "Español", "Italiano", "Türkçe"], str(noms))
                    depart_langues = {k: d[k] for k in ("langue_ecran", "langue_assistant")}
                    if not REEL:  # sur l'appareil réel, les langues sont celles que la personne a choisies
                        verifier("langues : français par défaut, pour l'écran comme pour l'assistant", d["langue_ecran"] == "fr" and d["langue_assistant"] == "fr", f"{d['langue_ecran']} / {d['langue_assistant']}")
                    opt_ecran = p4.evaluate("[...document.querySelectorAll('#langue-ecran option')].map(o => [o.value, o.textContent])")
                    opt_assistant = p4.evaluate("[...document.querySelectorAll('#langue-assistant option')].map(o => o.value)")
                    verifier("page : deux sélecteurs distincts (écran, assistant), sept options chacun", opt_ecran == [[x["code"], x["nom"]] for x in d["langues"]] and opt_assistant == CODES_LANGUES, f"{opt_ecran} / {opt_assistant}")
                    verifier("chargement à la demande : au départ, seul fr.json est demandé", set(fichiers_vus) == {"fr.json"}, str(fichiers_vus))
                    verifier_attribution(p4, "fr", "fr")
                    p4.locator("#attribution").scroll_into_view_if_needed()
                    p4.screenshot(path=str(CAPTURES / "15-attribution-fr.png"))

                    # deux réglages distincts : changer l'un ne change pas l'autre
                    p4.locator("#langue-assistant").select_option("tr")
                    expect(p4.locator("#toast")).to_contain_text("Enregistré")
                    d = api(p4, "GET", "/api/display")[1]
                    verifier("langue de l'assistant = turc ; langue de l'écran inchangée (fr)", d["langue_assistant"] == "tr" and d["langue_ecran"] == "fr", f"{d['langue_assistant']} / {d['langue_ecran']}")
                    verifier("la page reste en français quand on change la langue de l'assistant", p4.locator("html").get_attribute("lang") == "fr")
                    p4.locator("#langue-ecran").select_option("de")
                    expect(p4.locator("html")).to_have_attribute("lang", "de")
                    de = catalogue("de")
                    expect(p4.locator("#toast")).to_contain_text(de["toast.enregistre"])
                    d = api(p4, "GET", "/api/display")[1]
                    verifier("langue de l'écran = allemand ; langue de l'assistant (turc) inchangée", d["langue_ecran"] == "de" and d["langue_assistant"] == "tr", f"{d['langue_ecran']} / {d['langue_assistant']}")
                    verifier("la page passe à l'allemand tout de suite : titre de carte, étiquette du sélecteur, titre de l'onglet",
                             p4.locator("#t-ville").inner_text() == de["ville.titre"] and norm(p4.locator("label[for=langue-ecran]").inner_text()) == norm(de["ecran.langue_ecran"]) and p4.title() == de["app.titre"],
                             f"{p4.locator('#t-ville').inner_text()!r} / {p4.title()!r}")
                    verifier("chargement à la demande : fr.json puis de.json seulement (pas les cinq autres)", set(fichiers_vus) == {"fr.json", "de.json"}, str(fichiers_vus))
                    verifier("le sélecteur de la langue de l'assistant a gardé le turc", p4.locator("#langue-assistant").input_value() == "tr")
                    p4.locator("#ecran").scroll_into_view_if_needed()
                    p4.screenshot(path=str(CAPTURES / "16-ecran-allemand.png"))

                    # les sept langues, une par une : page, identifiants, français résiduel, attribution, météo, largeur
                    for code in CODES_LANGUES:
                        api(p4, "PUT", "/api/display", {"langue_ecran": code})
                        p4.reload()
                        expect(p4.locator("#ville")).to_be_visible()
                        expect(p4.locator("html")).to_have_attribute("lang", code)
                        cat = catalogue(code)
                        sections = ["ville", "alertes", "mer", "assistant", "ecran", "wifi", "appareil"]
                        liens = [norm(x) for x in p4.locator("nav.ancres a").all_inner_texts()]
                        verifier(f"{code} : onglets, titre de carte et titre de l'onglet du navigateur dans la langue",
                                 liens == [norm(cat["nav." + s]) for s in sections] and p4.locator("#t-ville").inner_text() == cat["ville.titre"] and p4.title() == cat["app.titre"], str(liens))
                        texte = p4.evaluate("document.getElementById('racine').textContent")  # y compris les cartes repliées
                        bruts = [i for i in ids if i in texte]
                        verifier(f"{code} : aucun identifiant brut (« ville.titre ») affiché", not bruts, str(bruts[:4]))
                        if code != "fr":
                            propres_au_francais = [v for k, v in fr.items() if "{" not in v and len(v) >= 6 and v != cat.get(k)]
                            restes = sorted({v for v in propres_au_francais if v in texte})
                            verifier(f"{code} : aucun texte français visible dans la page", not restes, str(restes[:4]))
                        verifier_attribution(p4, code, code)
                        expect(p4.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                        etiquette = p4.locator(".hero .temp small").inner_text()
                        traduits = lire_yaml(RACINE / "i18n" / f"{code}.yaml")
                        verifier(f"{code} : le libellé météo est un libellé de la table WMO du noyau, dans la langue", etiquette in {traduits.get(fr_l, fr_l) for fr_l in libelles_fr}, etiquette)
                        p4.locator("button[aria-expanded]").nth(1).click()
                        r = p4.evaluate(DEPASSEMENTS)
                        verifier(f"{code} : 360 px, pas de défilement horizontal (carte « LLM » ouverte)", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                        if code in ("nl", "tr"):
                            p4.screenshot(path=str(CAPTURES / f"17-page-{code}.png"), full_page=True)

                    # une phrase qui vient de l'appareil (pas de la page) arrive dans la langue de l'écran
                    api(p4, "PUT", "/api/display", {"langue_ecran": "de"})
                    c5, p5 = nouveau_contexte(nav)
                    p5.goto(BASE)
                    de_yaml = lire_yaml(RACINE / "i18n" / "de.yaml")
                    expect(p5.locator("#code")).to_be_visible()
                    verifier("appairage (page non connectée) : l'invite est en allemand, d'après /api/status", norm(p5.locator(".appairage > p").inner_text()) == norm(de["appairage.invite"]), p5.locator(".appairage > p").inner_text())
                    p5.locator("#code").fill("000000")
                    expect(p5.locator(".appairage .res")).to_be_visible()
                    verifier("code faux : la phrase envoyée par l'appareil est affichée en allemand, telle quelle",
                             norm(p5.locator(".appairage .res").inner_text()) == norm(de_yaml[cle_code_faux]), p5.locator(".appairage .res").inner_text())
                    p5.locator("#code").fill(CODE)  # le bon code remet le compteur d'essais à zéro
                    expect(p5.locator("#ville")).to_be_visible()
                    c5.close()

                    # repli : fichier de la langue absent -> anglais ; anglais absent aussi -> français
                    for absentes, attendu in ((["de"], "en"), (["de", "en"], "fr")):
                        c6, p6 = nouveau_contexte(nav)
                        for absente in absentes:
                            p6.route(f"**/lang/{absente}.json", lambda r: r.fulfill(status=404, content_type="text/plain", body="absent"))
                        appairer(p6)
                        expect(p6.locator("html")).to_have_attribute("lang", attendu)
                        cat = catalogue(attendu)
                        verifier(f"repli : {' et '.join(a + '.json' for a in absentes)} introuvable (écran en allemand) -> la page s'affiche en {attendu}",
                                 p6.locator("#t-ville").inner_text() == cat["ville.titre"] and p6.title() == cat["app.titre"], p6.locator("#t-ville").inner_text())
                        verifier_attribution(p6, attendu, f"repli {attendu}")
                        c6.close()
                finally:
                    try:  # remettre l'appareil dans son état de départ (français partout pour le faux appareil)
                        api(p4, "PUT", "/api/display", depart_langues if REEL else {"langue_ecran": "fr", "langue_assistant": "fr"})
                    except Exception:  # noqa: BLE001
                        pass
                    c4.close()
            etape("Sept langues : sélecteurs, chargement à la demande, repli, attribution", t_langues)

            ctx.close()
            nav.close()

        # ---- poids ----
        print("\n== Poids de la page + un fichier de langue (servis en gzip, comme sur l'appareil)")
        fichiers = ("index.html", "style.css", "app.js", "fr.json")
        brut = {f: (RACINE / "web" / "app" / ("lang" if f.endswith(".json") else ".") / f).stat().st_size for f in fichiers}
        gz = {f: next((e[1] for e in poids.get("res", []) if e[0] == f), None) for f in fichiers[1:]}
        gz["index.html"] = poids["nav"][0]
        total_gz = sum(v for v in gz.values() if v)
        for f in fichiers:
            print(f"  {f:11} {brut[f]:>7} octets bruts -> {gz[f]:>6} octets gzip")
        print(f"  TOTAL       {sum(brut.values()):>7} octets bruts -> {total_gz:>6} octets gzip (budget {BUDGET_GZ})")
        verifier(f"poids gzip : page + fr.json ({total_gz} octets) <= budget de {BUDGET_GZ} octets", all(gz.values()) and total_gz <= BUDGET_GZ, str(gz))
        # les six autres langues, compressées ici avec le même niveau 9 : la page + la plus lourde doit tenir aussi
        langues_gz = {c: len(gzip.compress((LANGUES_PAGE / f"{c}.json").read_bytes(), 9)) for c in CODES_LANGUES}
        page_gz = total_gz - (gz["fr.json"] or 0)
        lourde = max(langues_gz, key=lambda c: langues_gz[c])
        print("  fichiers de langue (gzip niveau 9) : " + ", ".join(f"{c} {n}" for c, n in langues_gz.items()))
        verifier(f"poids gzip : page + la langue la plus lourde ({lourde}, {page_gz + langues_gz[lourde]} octets) <= budget de {BUDGET_GZ} octets", page_gz + langues_gz[lourde] <= BUDGET_GZ)
        (CAPTURES / "poids.json").write_text(json.dumps({"brut": brut, "gzip": gz, "total_gzip": total_gz, "budget_gzip": BUDGET_GZ, "langues_gzip": langues_gz}, indent=2), encoding="utf-8")
    finally:
        if srv is not None:
            srv.terminate()
            try:
                srv.wait(timeout=5)
            except subprocess.TimeoutExpired:
                srv.kill()

    appareil = urllib.parse.urlsplit(BASE).netloc
    verifier("la page ne parle qu'à l'appareil (aucune police, image, script ou CDN externe) ; seule exception : la lecture GET /v1/surf/spots chez l'adresse de Vigie "
             "(celle du test, ou le service public par défaut), interceptée par le parcours : aucune de ces requêtes n'a atteint le réseau",
             hotes - {appareil} <= hotes_vigie
             and all(r["methode"] == "GET" and urllib.parse.urlsplit(r["url"]).path == "/v1/surf/spots" and urllib.parse.urlsplit(r["url"]).netloc in hotes_vigie for r in hors_appareil)
             and len(hors_appareil) == len(vigie_interceptees),
             f"hôtes {sorted(hotes)} ; {len(hors_appareil)} requête(s) hors appareil, {len(vigie_interceptees)} interceptée(s) ; {hors_appareil[:2]}")
    graves = [m for m in erreurs_console if "Failed to load resource" not in m]
    verifier("console du navigateur : aucune erreur JavaScript", not graves, "; ".join(graves[:3]))
    echecs = [r for r in resultats if not r[1]]
    print(f"\nRÉSULTAT : {len(resultats) - len(echecs)}/{len(resultats)} vérifications OK" + (f", {len(sautes)} étape(s) sautée(s)" if sautes else ""))
    for titre, raison in sautes:
        print(f"  SAUTÉE : {titre}  [{raison}]")
    for nom, _, detail in echecs:
        print(f"  ÉCHEC : {nom}  [{detail}]")
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
