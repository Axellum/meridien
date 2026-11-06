#!/usr/bin/env python3
"""Parcours de la page de réglages contre le faux appareil, avec Playwright (Chromium, rendu téléphone).

    python web/tests/parcours.py

Lance web/faux-serveur/serveur.mjs (Node) sur des ports de test, déroule le parcours, écrit les captures dans
web/captures/ et rend un tableau « vérification : OK / ÉCHEC ». Code de sortie 0 seulement si tout passe.
Réseau requis : la recherche de lieu et la météo appellent vraiment Open-Meteo.
"""
import json
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

from playwright.sync_api import expect, sync_playwright

RACINE = Path(__file__).resolve().parents[2]
CAPTURES = RACINE / "web" / "captures"
PORT, PORT_SERVICE = 18100, 18101
BASE = f"http://127.0.0.1:{PORT}"
CODE = "246810"
CLE_BONNE = "sk-secret-TEST-9f3a"   # la clé que le faux service accepte (valeur de test, pas un secret réel)
CLE_FAUSSE = "sk-faux-0000-leak"    # le faux service la cite dans sa réponse d'erreur, comme certains vrais services
VERROU_S = 3
BUDGET_GZ = 30 * 1024               # budget annoncé : 30 Ko compressés (HTML + CSS + JS)

resultats = []     # (nom, ok, détail)
registre = []      # toutes les réponses /api/ vues par le navigateur : {url, statut, corps, requete}
hotes = set()      # tous les hôtes joints par le navigateur (la page ne doit parler qu'à l'appareil)
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


def attendre_serveur():
    for _ in range(60):
        try:
            urllib.request.urlopen(BASE + "/api/status", timeout=1).read()
            return
        except Exception:  # noqa: BLE001
            time.sleep(0.25)
    raise RuntimeError("le faux appareil ne démarre pas")


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
    page.on("request", lambda r: hotes.add(urllib.parse.urlsplit(r.url).netloc))
    page.on("console", lambda m: erreurs_console.append(m.text) if m.type in ("error", "warning") else None)
    page.on("pageerror", lambda e: erreurs_console.append("pageerror : " + str(e)))
    return ctx, page


def jeton(page):
    return page.evaluate("sessionStorage.getItem('jeton')")


def api(page, methode, chemin, corps=None):
    r = page.request.fetch(BASE + chemin, method=methode, headers={"Authorization": "Bearer " + jeton(page), "Content-Type": "application/json"},
                           data=json.dumps(corps) if corps is not None else None)
    return r.status, (r.json() if "json" in (r.headers.get("content-type") or "") else {})


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
    env = {**__import__("os").environ, "PORT": str(PORT), "PORT_SERVICE": str(PORT_SERVICE), "CODE_APPAIRAGE": CODE,
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
                aide = page.locator("#ville > .aide").inner_text()
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
                page.get_by_role("button", name="Enregistrer").click()
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
            etape("Export et import des réglages sans clés", t_export_import, page)

            def t_ecran():
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
            etape("Écran : thème et luminosité", t_ecran, page)

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

            ctx.close()
            nav.close()

        # ---- poids ----
        print("\n== Poids de la page (servie en gzip, comme sur l'appareil)")
        brut = {f: (RACINE / "web" / "app" / f).stat().st_size for f in ("index.html", "style.css", "app.js")}
        gz = {f: next((e[1] for e in poids.get("res", []) if e[0] == f), None) for f in ("style.css", "app.js")}
        gz["index.html"] = poids["nav"][0]
        total_gz = sum(v for v in gz.values() if v)
        for f in ("index.html", "style.css", "app.js"):
            print(f"  {f:11} {brut[f]:>7} octets bruts -> {gz[f]:>6} octets gzip")
        print(f"  TOTAL       {sum(brut.values()):>7} octets bruts -> {total_gz:>6} octets gzip (budget {BUDGET_GZ})")
        verifier(f"poids gzip total ({total_gz} octets) <= budget de {BUDGET_GZ} octets", total_gz <= BUDGET_GZ)
        (CAPTURES / "poids.json").write_text(json.dumps({"brut": brut, "gzip": gz, "total_gzip": total_gz, "budget_gzip": BUDGET_GZ}, indent=2), encoding="utf-8")
    finally:
        srv.terminate()
        try:
            srv.wait(timeout=5)
        except subprocess.TimeoutExpired:
            srv.kill()

    verifier("la page ne parle qu'à l'appareil (aucune police, image, script ou CDN externe)", hotes == {f"127.0.0.1:{PORT}"}, str(sorted(hotes)))
    graves = [m for m in erreurs_console if "Failed to load resource" not in m]
    verifier("console du navigateur : aucune erreur JavaScript", not graves, "; ".join(graves[:3]))
    echecs = [r for r in resultats if not r[1]]
    print(f"\nRÉSULTAT : {len(resultats) - len(echecs)}/{len(resultats)} vérifications OK")
    for nom, _, detail in echecs:
        print(f"  ÉCHEC : {nom}  [{detail}]")
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
