#!/usr/bin/env python3
"""Contrôle et captures de la planche d'esquisses avec Playwright (Chromium).

- vérifie qu'à 360, 390 et 430 px de large rien ne dépasse (pas de défilement horizontal) ;
- vérifie qu'aucune erreur de console n'est levée ;
- enregistre : esquisse-a.png, esquisse-b.png, esquisse-c.png (clair + sombre côte à côte)
  et planche-mobile-390.png (la planche entière telle qu'un téléphone la voit).
"""
import sys
from pathlib import Path

from playwright.sync_api import sync_playwright

DOSSIER = Path(__file__).parent
URL = (DOSSIER / "index.html").resolve().as_uri()
DEPASSEMENTS = """() => {
  const l = document.documentElement.clientWidth, sortis = [];
  const rogne = (e) => {  // un parent qui défile ou rogne, lui-même dans l'écran, contient le dépassement
    for (let p = e.parentElement; p && p !== document.body; p = p.parentElement) {
      const o = getComputedStyle(p).overflowX, r = p.getBoundingClientRect();
      if (o !== 'visible' && r.left >= -1 && r.right <= l + 1) return true;
    }
    return false;
  };
  for (const e of document.querySelectorAll('body *')) {
    const r = e.getBoundingClientRect();
    if (r.width > 0 && (r.right > l + 1 || r.left < -1) && !rogne(e)) sortis.push(e.tagName + '.' + e.className + ' ' + Math.round(r.left) + '..' + Math.round(r.right));
  }
  return {largeur: l, defilement: document.documentElement.scrollWidth, sortis: sortis.slice(0, 8)};
}"""


def main() -> int:
    erreurs_console, echec = [], False
    with sync_playwright() as p:
        nav = p.chromium.launch()
        for largeur in (360, 390, 430):
            ctx = nav.new_context(viewport={"width": largeur, "height": 800}, device_scale_factor=1.5, is_mobile=True, has_touch=True)
            page = ctx.new_page()
            page.on("console", lambda m: erreurs_console.append(m.text) if m.type in ("error", "warning") else None)
            page.on("pageerror", lambda e: erreurs_console.append(str(e)))
            page.goto(URL)
            r = page.evaluate(DEPASSEMENTS)
            ok = r["defilement"] <= r["largeur"] and not r["sortis"]
            echec |= not ok
            print(f"{largeur:>4} px : défilement {r['defilement']} / largeur {r['largeur']} -> {'OK' if ok else 'DÉPASSE'} {r['sortis'] if not ok else ''}")
            if largeur == 390:
                page.screenshot(path=str(DOSSIER / "planche-mobile-390.png"), full_page=True)
            ctx.close()
        ctx = nav.new_context(viewport={"width": 940, "height": 900}, device_scale_factor=1.5)
        page = ctx.new_page()
        page.goto(URL)
        for k in "abc":
            page.locator(f"#esquisse-{k} .duo").screenshot(path=str(DOSSIER / f"esquisse-{k}.png"))
        ctx.close()
        nav.close()
    print("console :", erreurs_console or "aucune erreur")
    return 1 if (echec or erreurs_console) else 0


if __name__ == "__main__":
    sys.exit(main())
