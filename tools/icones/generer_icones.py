#!/usr/bin/env python3
"""Génère le jeu d'icônes de l'écran principal : formes dessinées par le projet (aucune licence à gérer), rendues en A8.

    python tools/icones/generer_icones.py              # écrit main/assets/icones_meteo.bin + components/core/.../visuel_icones_table.inc
    python tools/icones/generer_icones.py --verifier   # relit le binaire et la table ; échoue s'ils ne correspondent pas aux dessins
    python tools/icones/generer_icones.py --apercu DOSSIER   # planches PNG (fonds sombre et clair, plusieurs tailles)

Principe
--------
* Chaque dessin est une PIÈCE : un masque d'opacité (A8) sur une grille de 128 unités, dessiné en formes simples (disques,
  capsules, polygones) avec 8 x 8 échantillons par pixel final, puis réduit. Une pièce est rendue à plusieurs tailles.
* Une ICÔNE MÉTÉO est un empilement de 1 à 3 pièces, chacune teintée par un RÔLE de la palette (soleil, nuage, pluie...). Ainsi
  un thème change les couleurs sans toucher aux images, comme le fait le Tab5 avec sa police d'icônes. La liste des pièces de
  chaque icône est dans ICONES_METEO ci-dessous ; elle est aussi écrite dans la table C++ générée (le code ne la recopie pas).
* Une pièce n'est stockée qu'une fois, rognée à sa boîte englobante (x0, y0, largeur, hauteur dans le carré de l'icône).
* Les icônes d'information (thermomètre, goutte, vent...) sont des pièces seules, d'une seule teinte, en tailles plus petites.

Format du binaire (petit-boutiste, lu par main/ui_meteo_icones.cpp, qui contrôle l'en-tête) :
    en-tête : "MIC1" | u16 version | u16 nb_entrees | u32 taille_totale
    entrée  : u16 piece | u16 taille | u16 largeur | u16 hauteur | i16 x0 | i16 y0 | u32 décalage   (16 octets)
    données : les pixels A8 de chaque entrée, alignés sur 4 octets, ligne après ligne, sans remplissage de ligne.
Le fichier est reproductible : mêmes sources, mêmes octets.

Dépendance : Pillow (le même que les planches du banc de rendu). Le binaire et la table générée sont versionnés : la
compilation du firmware n'a pas besoin de Python.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw

RACINE = Path(__file__).resolve().parents[2]
BINAIRE = RACINE / "main" / "assets" / "icones_meteo.bin"
TABLE = RACINE / "components" / "core" / "include" / "ecran" / "visuel_icones_table.inc"

GRILLE = 128  # unités du dessin
SURECH = 8  # échantillons par pixel et par axe

# Tailles (côté du carré, en pixels) de chaque famille. L'écran choisit la plus grande qui tient dans la place voulue.
TAILLES_METEO = (44, 60, 84, 120, 168, 232)
TAILLES_INFO = (28, 40, 56, 80)
TAILLES_LUNE = (64, 112)  # phases de lune : un disque entier, donc moins de tailles


# ------------------------------------------------------------------------------------------------------------------------
# Toile : un masque et des formes en unités de la grille


class Toile:
    def __init__(self, taille: int):
        self.taille = taille
        self.k = taille * SURECH / GRILLE  # pixels du masque par unité
        n = taille * SURECH
        self.masque = Image.new("L", (n, n), 0)
        self.d = ImageDraw.Draw(self.masque)

    def _p(self, v: float) -> float:
        return v * self.k

    def disque(self, cx, cy, r, v=255):
        if r <= 0:
            return
        k = self.k
        self.d.ellipse((cx * k - r * k, cy * k - r * k, cx * k + r * k, cy * k + r * k), fill=v)

    def capsule(self, x0, y0, x1, y1, largeur, v=255):
        """Segment à bouts ronds."""
        k = self.k
        self.d.line((x0 * k, y0 * k, x1 * k, y1 * k), fill=v, width=max(1, round(largeur * k)))
        self.disque(x0, y0, largeur / 2, v)
        self.disque(x1, y1, largeur / 2, v)

    def rect_rond(self, x0, y0, x1, y1, rayon, v=255):
        k = self.k
        self.d.rounded_rectangle((x0 * k, y0 * k, x1 * k, y1 * k), radius=rayon * k, fill=v)

    def polygone(self, points, v=255):
        k = self.k
        self.d.polygon([(x * k, y * k) for x, y in points], fill=v)

    def polygone_rond(self, points, rayon, v=255):
        """Polygone aux angles arrondis : le polygone + un cercle à chaque sommet et un trait le long de chaque côté,
        réduit de `rayon` (on dessine le contour épaissi, puis le remplissage décalé n'est pas nécessaire : le contour rond
        suffit pour de petites formes pleines)."""
        self.polygone(points, v)
        n = len(points)
        for i in range(n):
            x0, y0 = points[i]
            x1, y1 = points[(i + 1) % n]
            self.capsule(x0, y0, x1, y1, 2 * rayon, v)

    def etoile4(self, cx, cy, r, v=255):
        """Étoile à quatre branches fines (le scintillement de la lune)."""
        a = r * 0.22
        self.polygone(
            [(cx, cy - r), (cx + a, cy - a), (cx + r, cy), (cx + a, cy + a), (cx, cy + r), (cx - a, cy + a), (cx - r, cy), (cx - a, cy - a)],
            v,
        )

    def rendre(self) -> Image.Image:
        return self.masque.resize((self.taille, self.taille), Image.Resampling.BOX)


# ------------------------------------------------------------------------------------------------------------------------
# Formes composées (unités de la grille)


def nuage(t: Toile, cx, cy, s=1.0, grossir=0.0, v=255):
    """Un nuage de ~96 x 60 unités (à s = 1), centré en (cx, cy), fond plat. `grossir` l'épaissit de ce nombre d'unités de
    chaque côté (sert à découper un vide autour de lui dans le soleil ou la lune derrière)."""
    g = grossir
    # Le fond : une gélule. Les bosses : trois disques de tailles différentes (la grande à droite du centre).
    t.rect_rond(cx - 46 * s - g, cy + 2 * s - g, cx + 48 * s + g, cy + 28 * s + g, 13 * s + g, v)
    t.disque(cx - 21 * s, cy + 3 * s, 19 * s + g, v)
    t.disque(cx + 6 * s, cy - 6 * s, 25 * s + g, v)
    t.disque(cx + 31 * s, cy + 11 * s, 17 * s + g, v)


def soleil_forme(t: Toile, cx, cy, r, rayon_debut, rayon_fin, epaisseur, v=255, rayons=range(8)):
    """Disque et rayons (i * 45 degrés, 0 = vers la droite, sens horaire à l'écran) ; `rayons` : ceux à dessiner."""
    t.disque(cx, cy, r, v)
    for i in rayons:
        a = math.radians(i * 45)
        t.capsule(
            cx + math.cos(a) * rayon_debut, cy + math.sin(a) * rayon_debut,
            cx + math.cos(a) * rayon_fin, cy + math.sin(a) * rayon_fin, epaisseur, v,
        )


def croissant(t: Toile, cx, cy, r, v=255, ouverture=(0.45, -0.38), decoupe=0.86):
    """Croissant : un disque auquel on retire un disque plus petit décalé vers (ouverture)."""
    t.disque(cx, cy, r, v)
    t.disque(cx + ouverture[0] * r * 1.35, cy + ouverture[1] * r * 1.35, r * decoupe, 0)


def flocon(t: Toile, cx, cy, r, epaisseur, v=255):
    for i in range(3):
        a = math.radians(i * 60 + 90)
        t.capsule(cx - math.cos(a) * r, cy - math.sin(a) * r, cx + math.cos(a) * r, cy + math.sin(a) * r, epaisseur, v)
    # une petite encoche à chaque pointe : lisible dès 44 px, sinon c'est une étoile pleine
    for i in range(6):
        a = math.radians(i * 60 + 90)
        px, py = cx + math.cos(a) * r * 0.62, cy + math.sin(a) * r * 0.62
        for d in (-1, 1):
            b = a + d * math.radians(50)
            t.capsule(px, py, px + math.cos(b) * r * 0.30, py + math.sin(b) * r * 0.30, epaisseur * 0.72, v)


def goutte_trait(t: Toile, x, y, longueur=20, inclinaison=0.30, epaisseur=7.5, v=255):
    """Trait de pluie incliné, pointe en bas à gauche."""
    t.capsule(x + longueur * inclinaison / 2, y - longueur / 2, x - longueur * inclinaison / 2, y + longueur / 2, epaisseur, v)


def eclair(t: Toile, cx, cy, s=1.0, v=255):
    pts = [(14, -30), (-14, 6), (-1, 6), (-10, 30), (18, -8), (4, -8), (14, -30)]
    pts = [(cx + x * s, cy + y * s) for x, y in pts[:-1]]
    t.polygone_rond(pts, 1.6 * s, v)


def goutte(t: Toile, cx, cy, r, v=255):
    """Goutte d'eau : un disque et une pointe vers le haut."""
    t.disque(cx, cy + r * 0.25, r, v)
    t.polygone([(cx, cy - r * 1.55), (cx - r * 0.92, cy + r * 0.05), (cx + r * 0.92, cy + r * 0.05)], v)
    t.disque(cx, cy - r * 1.40, r * 0.09, v)  # adoucit la pointe


# ------------------------------------------------------------------------------------------------------------------------
# Pièces météo : nom -> fonction(toile)


def p_soleil(t):
    soleil_forme(t, 64, 64, 24, 36, 53, 8)


def p_soleil_p(t):
    """Soleil dans le coin supérieur gauche, un vide autour du nuage bas qui le recouvre."""
    soleil_forme(t, 46, 45, 19, 29, 41, 6.5, rayons=(3, 4, 5, 6, 7))
    nuage(t, 70, 80, 0.98, grossir=5, v=0)


def p_soleil_haut_p(t):
    """Soleil derrière un nuage haut (averses : de la pluie sous le nuage)."""
    soleil_forme(t, 47, 34, 17, 25, 36, 6, rayons=(3, 4, 5, 6))
    nuage(t, 72, 52, 0.9, grossir=5, v=0)


def p_lune(t):
    croissant(t, 60, 68, 36)
    t.etoile4(97, 38, 9)
    t.etoile4(104, 66, 5.5)


def p_lune_p(t):
    croissant(t, 46, 46, 27)
    t.etoile4(76, 24, 7)
    nuage(t, 70, 80, 0.98, grossir=5, v=0)


def p_lune_haut_p(t):
    croissant(t, 46, 34, 23)
    nuage(t, 72, 52, 0.9, grossir=5, v=0)


def p_nuage(t):
    nuage(t, 64, 64, 1.0)


def p_nuage_bas(t):
    nuage(t, 70, 80, 0.98)


def p_nuage_haut(t):
    nuage(t, 64, 46, 1.0)


def p_nuage_averse(t):
    nuage(t, 72, 52, 0.9)


def p_nuage_arriere(t):
    """Second nuage, derrière : ciel couvert. Un vide autour du nuage de devant."""
    nuage(t, 86, 42, 0.80)
    nuage(t, 56, 70, 1.0, grossir=5, v=0)


def p_nuage_devant(t):
    nuage(t, 56, 70, 1.0)


def p_bruine(t):
    for x, y in ((40, 94), (64, 94), (88, 94), (52, 108), (76, 108)):
        t.disque(x, y, 4.2)


def p_pluie(t):
    for x, y in ((46, 98), (66, 106), (86, 98)):
        goutte_trait(t, x, y, 19)


def p_pluie_forte(t):
    for x, y in ((40, 92), (62, 92), (84, 92), (52, 112), (74, 112), (96, 112)):
        goutte_trait(t, x, y, 18, epaisseur=8)


def p_averse(t):
    for x, y in ((52, 99), (72, 107)):
        goutte_trait(t, x, y, 19)


def p_averse_forte(t):
    for x, y in ((44, 96), (64, 96), (84, 96), (54, 114), (74, 114)):
        goutte_trait(t, x, y, 18, epaisseur=8)


def p_verglas_pluie(t):
    for x, y in ((46, 98), (82, 98)):
        goutte_trait(t, x, y, 19)


def p_verglas_glace(t):
    """Grains de glace entre et sous les traits de pluie."""
    for x, y in ((64, 94), (56, 112), (74, 112)):
        t.disque(x, y, 5.2)


def p_neige(t):
    for x, y in ((42, 100), (64, 108), (86, 100)):
        flocon(t, x, y, 10.5, 4.8)


def p_neige_forte(t):
    for x, y in ((38, 94), (64, 94), (90, 94), (51, 114), (77, 114)):
        flocon(t, x, y, 10.5, 4.8)


def p_grains(t):
    for x, y in ((40, 96), (62, 96), (84, 96), (51, 112), (73, 112)):
        t.disque(x, y, 5.4)


def p_averse_neige(t):
    for x, y in ((52, 100), (76, 108)):
        flocon(t, x, y, 10.5, 4.8)


def p_eclair(t):
    eclair(t, 66, 94, 1.0)


def p_grele(t):
    for x, y in ((36, 100), (47, 117), (94, 100), (83, 117)):
        t.disque(x, y, 5.4)


def p_brume(t):
    for y, x0, x1 in ((86, 26, 96), (100, 40, 108), (114, 28, 84)):
        t.capsule(x0, y, x1, y, 7.5)


PIECES_METEO = {
    "soleil": p_soleil, "soleil_p": p_soleil_p, "soleil_haut_p": p_soleil_haut_p,
    "lune": p_lune, "lune_p": p_lune_p, "lune_haut_p": p_lune_haut_p,
    "nuage": p_nuage, "nuage_bas": p_nuage_bas, "nuage_haut": p_nuage_haut, "nuage_averse": p_nuage_averse,
    "nuage_arriere": p_nuage_arriere, "nuage_devant": p_nuage_devant,
    "bruine": p_bruine, "pluie": p_pluie, "pluie_forte": p_pluie_forte, "averse": p_averse, "averse_forte": p_averse_forte,
    "verglas_pluie": p_verglas_pluie, "verglas_glace": p_verglas_glace, "neige": p_neige, "neige_forte": p_neige_forte, "grains": p_grains, "averse_neige": p_averse_neige,
    "eclair": p_eclair, "grele": p_grele, "brume": p_brume,
}

# Icônes météo : nom -> [(pièce, rôle de teinte)]. L'ordre est celui du dessin (la première pièce est dessous). Les noms
# d'icônes sont ceux de ecran::meteo::nom_icone() (meteo_wmo.hpp) suivis de _jour / _nuit quand les deux diffèrent.
# Rôles : soleil lune nuage nuage_sombre pluie neige eclair brume (voir main/palette_principal.h).
ICONES_METEO: dict[str, list[tuple[str, str]]] = {
    "inconnue": [("nuage", "nuage")],
    "soleil_jour": [("soleil", "soleil")],
    "lune_nuit": [("lune", "lune")],
    "peu_nuageux_jour": [("soleil_p", "soleil"), ("nuage_bas", "nuage")],
    "peu_nuageux_nuit": [("lune_p", "lune"), ("nuage_bas", "nuage")],
    "nuageux_jour": [("soleil_p", "soleil"), ("nuage_bas", "nuage")],
    "nuageux_nuit": [("lune_p", "lune"), ("nuage_bas", "nuage")],
    "couvert": [("nuage_arriere", "nuage_sombre"), ("nuage_devant", "nuage")],
    "brouillard": [("nuage_haut", "nuage"), ("brume", "brume")],
    "bruine": [("nuage_haut", "nuage"), ("bruine", "pluie")],
    "pluie_verglacante": [("nuage_haut", "nuage_sombre"), ("verglas_pluie", "pluie"), ("verglas_glace", "neige")],
    "pluie": [("nuage_haut", "nuage_sombre"), ("pluie", "pluie")],
    "pluie_forte": [("nuage_haut", "nuage_sombre"), ("pluie_forte", "pluie")],
    "neige": [("nuage_haut", "nuage"), ("neige", "neige")],
    "neige_forte": [("nuage_haut", "nuage"), ("neige_forte", "neige")],
    "grains_de_neige": [("nuage_haut", "nuage"), ("grains", "neige")],
    "averses_jour": [("soleil_haut_p", "soleil"), ("nuage_averse", "nuage_sombre"), ("averse", "pluie")],
    "averses_nuit": [("lune_haut_p", "lune"), ("nuage_averse", "nuage_sombre"), ("averse", "pluie")],
    "averses_violentes": [("nuage_haut", "nuage_sombre"), ("averse_forte", "pluie")],
    "averses_de_neige_jour": [("soleil_haut_p", "soleil"), ("nuage_averse", "nuage"), ("averse_neige", "neige")],
    "averses_de_neige_nuit": [("lune_haut_p", "lune"), ("nuage_averse", "nuage"), ("averse_neige", "neige")],
    "orage": [("nuage_haut", "nuage_sombre"), ("eclair", "eclair")],
    "orage_grele": [("nuage_haut", "nuage_sombre"), ("eclair", "eclair"), ("grele", "neige")],
}


# ------------------------------------------------------------------------------------------------------------------------
# Pièces d'information (une teinte ; le rôle est choisi par l'écran)


def i_thermometre(t):
    t.rect_rond(54, 14, 74, 78, 10)  # tube
    t.disque(64, 92, 20)  # bulbe
    t.rect_rond(60, 26, 68, 86, 4, 0)  # creux du tube
    t.disque(64, 92, 11, 0)
    t.rect_rond(60.5, 42, 67.5, 90, 3.5)  # colonne
    t.disque(64, 92, 12)
    for y in (24, 36, 48):  # graduations
        t.capsule(82, y, 94, y, 5)


def i_ressenti(t):
    """Thermomètre + petite vague : « ressenti »."""
    t.rect_rond(34, 12, 52, 72, 9)
    t.disque(43, 86, 19)
    t.rect_rond(38.5, 24, 47.5, 78, 4, 0)
    t.disque(43, 86, 10, 0)
    t.rect_rond(39.5, 40, 46.5, 84, 3.5)
    t.disque(43, 86, 11)
    for dy in (26, 52, 78):
        t.capsule(70, dy, 112, dy, 7)
        t.capsule(70, dy, 62, dy + 10, 7)


def i_humidite(t):
    goutte(t, 64, 70, 34)
    # reflet : un trait clair à gauche
    t.capsule(47, 76, 51, 90, 6, 0)


def i_parapluie(t):
    t.disque(64, 60, 46)
    t.rect_rond(10, 60, 118, 110, 0, 0)
    t.rect_rond(18, 60, 110, 62, 0)
    # trois festons en bas de la toile
    for cx in (28, 64, 100):
        t.disque(cx, 60, 18, 0)
    for cx in (28, 64, 100):
        t.disque(cx, 56, 18, 0)
    t.disque(64, 60, 46, 255)
    t.rect_rond(0, 62, 128, 128, 0, 0)
    for cx in (28.7, 64, 99.3):
        t.disque(cx, 60, 17, 0)
    t.capsule(64, 56, 64, 102, 8)
    t.capsule(64, 102, 56, 110, 8)
    t.capsule(56, 110, 49, 104, 8)


def i_pluie_mm(t):
    """Traits de pluie sur deux rangs : cumul de pluie."""
    for x, y in ((34, 40), (64, 40), (94, 40), (49, 82), (79, 82)):
        goutte_trait(t, x, y, 30, epaisseur=11)


def i_vent(t):
    t.capsule(14, 46, 78, 46, 8)
    t.capsule(78, 46, 98, 46, 8)
    t.disque(88, 40, 16)  # volute du haut (anneau)
    t.disque(88, 40, 8.5, 0)
    t.capsule(14, 70, 100, 70, 8)
    t.disque(104, 82, 16)
    t.disque(104, 82, 8.5, 0)
    t.capsule(30, 94, 76, 94, 8)
    t.disque(76, 102, 12)
    t.disque(76, 102, 5, 0)


def i_pression(t):
    """Demi-cadran et aiguille."""
    t.disque(64, 74, 54)
    t.disque(64, 74, 42, 0)
    t.rect_rond(0, 78, 128, 128, 0, 0)
    t.capsule(64, 74, 92, 46, 8)
    t.disque(64, 74, 9)
    t.capsule(18, 74, 28, 74, 6)
    t.capsule(100, 74, 110, 74, 6)
    t.capsule(64, 18, 64, 28, 6)
    t.capsule(26, 36, 33, 43, 6)
    t.capsule(102, 36, 95, 43, 6)


def i_uv(t):
    soleil_forme(t, 64, 64, 24, 36, 54, 8)


def i_lever(t):
    """Demi-soleil sur l'horizon et flèche vers le haut."""
    t.disque(64, 86, 26)
    t.rect_rond(0, 86, 128, 128, 0, 0)
    for i in (-2, -1, 0, 1, 2):
        a = math.radians(270 + i * 45)
        t.capsule(64 + math.cos(a) * 38, 86 + math.sin(a) * 38, 64 + math.cos(a) * 52, 86 + math.sin(a) * 52, 7)
    t.capsule(16, 98, 112, 98, 7)
    t.capsule(44, 112, 84, 112, 7)
    t.polygone_rond([(64, 8), (80, 28), (48, 28)], 2.5)


def i_coucher(t):
    t.disque(64, 74, 26)
    t.rect_rond(0, 74, 128, 128, 0, 0)
    for i in (-2, -1, 0, 1, 2):
        a = math.radians(270 + i * 45)
        t.capsule(64 + math.cos(a) * 38, 74 + math.sin(a) * 38, 64 + math.cos(a) * 52, 74 + math.sin(a) * 52, 7)
    t.capsule(16, 86, 112, 86, 7)
    t.capsule(44, 100, 84, 100, 7)
    t.polygone_rond([(64, 126), (80, 108), (48, 108)], 2.5)


def i_lune_info(t):
    croissant(t, 60, 68, 40)


def i_epingle(t):
    """Repère de lieu."""
    t.disque(64, 50, 34)
    t.polygone([(34, 64), (94, 64), (64, 120)])
    t.disque(64, 50, 14, 0)


def i_alerte(t):
    t.polygone_rond([(64, 14), (118, 106), (10, 106)], 6)
    t.capsule(64, 46, 64, 74, 10, 0)
    t.disque(64, 90, 6, 0)


def i_gel(t):
    flocon(t, 64, 64, 50, 9)


def i_chaleur(t):
    """Thermomètre plein dans un soleil : forte chaleur."""
    soleil_forme(t, 64, 64, 26, 40, 56, 8)
    t.rect_rond(59, 44, 69, 70, 5, 0)
    t.disque(64, 76, 9, 0)


def i_hors_ligne(t):
    """Nuage barré : données périmées."""
    nuage(t, 62, 62, 1.0)
    t.capsule(18, 18, 110, 110, 11, 0)
    t.capsule(26, 12, 118, 104, 8)


def i_reglages(t):
    t.disque(64, 64, 42)
    for i in range(8):
        a = math.radians(i * 45 + 22.5)
        t.capsule(64 + math.cos(a) * 36, 64 + math.sin(a) * 36, 64 + math.cos(a) * 48, 64 + math.sin(a) * 48, 17)
    t.disque(64, 64, 18, 0)


def i_horloge(t):
    t.disque(64, 64, 52)
    t.disque(64, 64, 41, 0)
    t.capsule(64, 64, 64, 36, 8)
    t.capsule(64, 64, 84, 74, 8)


def i_nuage_info(t):
    nuage(t, 64, 64, 1.1)


def i_degre(t):
    pass


PIECES_INFO = {
    "thermometre": i_thermometre, "ressenti": i_ressenti, "humidite": i_humidite, "parapluie": i_parapluie,
    "pluie_mm": i_pluie_mm, "vent": i_vent, "pression": i_pression, "uv": i_uv, "lever": i_lever, "coucher": i_coucher,
    "lune_info": i_lune_info, "epingle": i_epingle, "alerte": i_alerte, "gel": i_gel, "chaleur": i_chaleur,
    "hors_ligne": i_hors_ligne, "reglages": i_reglages, "horloge": i_horloge, "nuage_info": i_nuage_info,
}


# ------------------------------------------------------------------------------------------------------------------------
# Phases de lune : 8 images du côté éclairé (le côté sombre est un disque dessiné par l'écran)

NB_PHASES = 8


def phase_lune(t: Toile, indice: int):
    """indice 0 = nouvelle lune (rien d'éclairé), 2 = premier quartier, 4 = pleine lune, 6 = dernier quartier.
    Hémisphère nord : la lune croissante est éclairée à DROITE. La limite entre ombre et lumière est une demi-ellipse
    (le terminateur) : x = r * cos(phase) en largeur, à droite de l'axe pour une croissante."""
    r = 58
    cx = cy = 64
    if indice % NB_PHASES == 0:
        return
    angle = 2 * math.pi * indice / NB_PHASES  # 0 nouvelle, pi pleine
    croissante = indice < NB_PHASES // 2
    k = math.cos(angle)  # 1 (nouvelle) .. -1 (pleine)
    # demi-disque éclairé : côté droit (croissante) ou gauche (décroissante)
    pts_bord = []
    n = 90
    for i in range(n + 1):
        a = -math.pi / 2 + math.pi * i / n
        pts_bord.append((math.cos(a) * r, math.sin(a) * r))  # arc de droite, de haut en bas
    # terminateur : demi-ellipse de largeur r*|k|
    pts_term = []
    for i in range(n + 1):
        a = math.pi / 2 - math.pi * i / n  # de bas en haut
        pts_term.append((math.cos(a) * r * k, math.sin(a) * r))
    pts = pts_bord + pts_term
    if not croissante:
        pts = [(-x, y) for x, y in pts]
    t.polygone([(cx + x, cy + y) for x, y in pts])


# ------------------------------------------------------------------------------------------------------------------------
# Rendu et assemblage


@dataclass
class Entree:
    piece: int
    taille: int
    largeur: int
    hauteur: int
    x0: int
    y0: int
    donnees: bytes


def rendre_piece(fonction, taille: int):
    toile = Toile(taille)
    fonction(toile)
    image = toile.rendre()
    boite = image.getbbox()
    if boite is None:
        return 0, 0, 0, 0, b""
    rogne = image.crop(boite)
    return boite[0], boite[1], rogne.width, rogne.height, rogne.tobytes()


def toutes_les_pieces():
    """[(nom, fonction, tailles, famille)] dans l'ordre des identifiants (jamais réordonné : c'est l'ABI du binaire)."""
    pieces = []
    for nom, f in PIECES_METEO.items():
        pieces.append((nom, f, TAILLES_METEO, "meteo"))
    for nom, f in PIECES_INFO.items():
        pieces.append((nom, f, TAILLES_INFO, "info"))
    for i in range(NB_PHASES):
        pieces.append((f"phase_{i:02d}", (lambda t, i=i: phase_lune(t, i)), TAILLES_LUNE, "lune"))
    return pieces


def fabriquer() -> tuple[bytes, list[Entree], list]:
    pieces = toutes_les_pieces()
    entrees: list[Entree] = []
    for identifiant, (_nom, f, tailles, _famille) in enumerate(pieces):
        for taille in tailles:
            x0, y0, w, h, data = rendre_piece(f, taille)
            entrees.append(Entree(identifiant, taille, w, h, x0, y0, data))
    entrees.sort(key=lambda e: (e.piece, e.taille))
    en_tete = 12
    table_octets = 16 * len(entrees)
    courant = en_tete + table_octets
    decalages = []
    for e in entrees:
        courant = (courant + 3) & ~3
        decalages.append(courant)
        courant += len(e.donnees)
    total = (courant + 3) & ~3
    blob = bytearray(total)
    struct.pack_into("<4sHHI", blob, 0, b"MIC1", 1, len(entrees), total)
    for i, (e, dec) in enumerate(zip(entrees, decalages)):
        struct.pack_into("<HHHHhhI", blob, en_tete + 16 * i, e.piece, e.taille, e.largeur, e.hauteur, e.x0, e.y0, dec)
        blob[dec : dec + len(e.donnees)] = e.donnees
    return bytes(blob), entrees, pieces


def ecrire_table(pieces) -> str:
    noms_pieces = [p[0] for p in pieces]
    L = []
    L.append("// FICHIER GÉNÉRÉ par tools/icones/generer_icones.py : ne pas modifier à la main (--verifier le détecterait).")
    L.append("// Table des pièces d'icônes et composition des icônes météo ; les pixels sont dans main/assets/icones_meteo.bin.")
    L.append("// Inclus par ecran/visuel_icones.hpp.")
    L.append("")
    L.append("// Identifiants des pièces, dans l'ordre du binaire.")
    L.append("#define ECRAN_VISUEL_PIECES(X) \\")
    for i, nom in enumerate(noms_pieces):
        L.append(f"    X({nom}, {pieces[i][3].upper()}){' \\' if i + 1 < len(noms_pieces) else ''}")
    L.append("")
    L.append("// Tailles disponibles par famille.")
    L.append("inline constexpr int TAILLES_METEO[] = {" + ", ".join(map(str, TAILLES_METEO)) + "};")
    L.append("inline constexpr int TAILLES_INFO[] = {" + ", ".join(map(str, TAILLES_INFO)) + "};")
    L.append("inline constexpr int TAILLES_LUNE[] = {" + ", ".join(map(str, TAILLES_LUNE)) + "};")
    L.append(f"inline constexpr int NB_PHASES_LUNE = {NB_PHASES};")
    L.append("")
    L.append("// Composition : nom de l'icône, puis jusqu'à 3 couples (pièce, rôle de teinte).")
    noms_icones = list(ICONES_METEO)
    L.append("#define ECRAN_VISUEL_ICONES(X) \\")
    for j, nom in enumerate(noms_icones):
        couches = ICONES_METEO[nom]
        args = ", ".join(f"{c[0]}, {c[1]}" for c in couches)
        sentinelle = ", " + ", ".join(["AUCUNE, AUCUN"] * (3 - len(couches))) if len(couches) < 3 else ""
        L.append(f"    X({nom}, {len(couches)}, {args}{sentinelle}){' \\' if j + 1 < len(noms_icones) else ''}")
    L.append("")
    return "\n".join(L)


def verifier_composition():
    """Contrôles de cohérence des dessins : chaque pièce citée existe, chaque icône a 1 à 3 couches, rôles connus."""
    roles = {"soleil", "lune", "nuage", "nuage_sombre", "pluie", "neige", "eclair", "brume"}
    for nom, couches in ICONES_METEO.items():
        assert 1 <= len(couches) <= 3, nom
        for piece, role in couches:
            assert piece in PIECES_METEO, f"{nom} : pièce inconnue {piece}"
            assert role in roles, f"{nom} : rôle inconnu {role}"


# ------------------------------------------------------------------------------------------------------------------------
# Aperçu


TEINTES_APERCU = {  # palette d'APERÇU seulement ; les vraies couleurs sont dans main/palette_principal.h
    "soleil": (255, 197, 66), "lune": (232, 226, 196), "nuage": (214, 224, 236), "nuage_sombre": (140, 154, 174),
    "pluie": (84, 170, 255), "neige": (240, 248, 255), "eclair": (255, 214, 64), "brume": (170, 184, 200),
}


def apercu(dossier: Path):
    dossier.mkdir(parents=True, exist_ok=True)
    blob, entrees, pieces = fabriquer()
    index = {(e.piece, e.taille): e for e in entrees}
    identifiant = {p[0]: i for i, p in enumerate(pieces)}

    def poser(planche, piece, taille, x, y, teinte):
        e = index[(identifiant[piece], taille)]
        if e.largeur == 0:
            return
        masque = Image.frombytes("L", (e.largeur, e.hauteur), e.donnees)
        planche.paste(Image.new("RGB", masque.size, teinte), (x + e.x0, y + e.y0), masque)

    for nom_fond, fond, texte in (("sombre", (15, 20, 26), (230, 236, 242)), ("clair", (244, 240, 232), (30, 36, 44))):
        for taille in (232, 84, 44):
            noms = list(ICONES_METEO)
            colonnes = 8 if taille >= 120 else 12
            cell = taille + 20
            rangs = math.ceil(len(noms) / colonnes)
            planche = Image.new("RGB", (colonnes * cell + 20, rangs * cell + 20), fond)
            for n, nom in enumerate(noms):
                x, y = 20 + (n % colonnes) * cell, 20 + (n // colonnes) * cell
                for piece, role in ICONES_METEO[nom]:
                    poser(planche, piece, taille, x, y, TEINTES_APERCU[role] if nom_fond == "sombre" else assombrir(TEINTES_APERCU[role], role))
            planche.save(dossier / f"meteo_{nom_fond}_{taille}.png")
        # informations
        noms_info = list(PIECES_INFO)
        taille = 80
        cell = taille + 20
        planche = Image.new("RGB", (8 * cell + 20, math.ceil(len(noms_info) / 8) * cell + 20), fond)
        for n, nom in enumerate(noms_info):
            poser(planche, nom, taille, 20 + (n % 8) * cell, 20 + (n // 8) * cell, texte)
        planche.save(dossier / f"info_{nom_fond}_80.png")
        petit = Image.new("RGB", (len(noms_info) * 48 + 20, 100), fond)
        for n, nom in enumerate(noms_info):
            poser(petit, nom, 28, 20 + n * 48, 10, texte)
            poser(petit, nom, 40, 20 + n * 48, 48, texte)
        petit.save(dossier / f"info_{nom_fond}_petit.png")
        # phases de lune
        taille = 112
        planche = Image.new("RGB", (NB_PHASES * (taille + 12) + 20, taille + 40), fond)
        for i in range(NB_PHASES):
            x, y = 20 + i * (taille + 12), 20
            d = ImageDraw.Draw(planche)
            d.ellipse((x + 4, y + 4, x + taille - 4, y + taille - 4), fill=(48, 56, 70) if nom_fond == "sombre" else (200, 196, 188))
            poser(planche, f"phase_{i:02d}", taille, x, y, TEINTES_APERCU["lune"] if nom_fond == "sombre" else (250, 220, 120))
        planche.save(dossier / f"lune_{nom_fond}.png")
    print(f"aperçus écrits dans {dossier}")


def assombrir(rgb, role):
    """Sur fond clair, les teintes pâles disparaissent : l'aperçu les fonce (le vrai thème clair a ses propres valeurs)."""
    f = {"nuage": 0.62, "neige": 0.55, "brume": 0.7, "lune": 0.95}.get(role, 1.0)
    return tuple(int(c * f) for c in rgb)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--verifier", action="store_true", help="échoue si le binaire ou la table ne sont pas ceux des dessins")
    p.add_argument("--apercu", type=Path, metavar="DOSSIER", help="écrit des planches PNG d'aperçu")
    args = p.parse_args()
    verifier_composition()
    if args.apercu:
        apercu(args.apercu)
        return
    blob, entrees, pieces = fabriquer()
    table = ecrire_table(pieces)
    if args.verifier:
        ok = BINAIRE.is_file() and BINAIRE.read_bytes() == blob and TABLE.is_file() and TABLE.read_text(encoding="utf-8") == table
        print("icônes à jour" if ok else "ECHEC : le binaire ou la table ne correspondent pas aux dessins (relancer sans --verifier)")
        sys.exit(0 if ok else 1)
    BINAIRE.write_bytes(blob)
    TABLE.write_text(table, encoding="utf-8", newline="\n")
    print(f"{BINAIRE.relative_to(RACINE)} : {len(blob)} octets, {len(entrees)} images, SHA-256 {hashlib.sha256(blob).hexdigest()[:16]}…")
    print(f"{TABLE.relative_to(RACINE)} : {len(pieces)} pièces, {len(ICONES_METEO)} icônes météo")


if __name__ == "__main__":
    main()
