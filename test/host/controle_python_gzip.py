#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des tests de ecran/gzip.hpp (en-tête, pied, CRC32 du noyau).

    python test/host/controle_python_gzip.py              # (ré)génère donnees/gzip_cas.inc et donnees/openmeteo_gzip_*.inc
    python test/host/controle_python_gzip.py --verifier   # ne modifie rien : échoue si un .inc n'est pas à jour

Entrées :
  - donnees/openmeteo_gzip_<lieu>.gz : les octets RÉELS d'une réponse d'Open-Meteo avec `Accept-Encoding: gzip`, tels qu'ils sont
    passés sur le fil, capturés par test/host/capturer_gzip_meteo.py ;
  - des fichiers gzip fabriqués ici avec `zlib` (flux DEFLATE brut) et un en-tête écrit à la main, un cas par particularité de la
    RFC 1952 : aucun champ facultatif, FNAME, FEXTRA, FCOMMENT, FHCRC, tous ensemble, flux vide, flux de 70 000 octets, deux membres.
    Chaque fichier fabriqué est relu par le module `gzip` de Python (décompression complète) avant d'être écrit.
Sorties (générées, ne pas modifier à la main) :
  gzip_cas.inc                    un tableau d'octets par cas, avec le décalage du flux, le CRC32 et la taille attendus
  openmeteo_gzip_<lieu>.inc       la réponse réelle (octets du fil), son texte décompressé et leur CRC32 / taille

Les valeurs attendues (décalage du flux, CRC32, taille) sont calculées ici avec `zlib.crc32` et `len()` ; aucune ligne de code
C++ du noyau n'est reprise. Le CRC16 du champ FHCRC est écrit à partir de la RFC : deux octets de poids faible du CRC32 de
l'en-tête qui précède.
"""
import argparse
import gzip
import struct
import sys
import zlib
from pathlib import Path

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
LIEUX = ["lyon", "sydney"]
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_gzip.py : ne pas modifier à la main.\n"

TEXTE = "Meridien : météo, heure et réglages — 25 °C, ciel dégagé.\n".encode("utf-8") * 6


def deflate_brut(donnees: bytes) -> bytes:
    c = zlib.compressobj(9, zlib.DEFLATED, -15)
    return c.compress(donnees) + c.flush()


def membre(donnees: bytes, *, flg_extra=False, fichier=None, commentaire=None, hcrc=False, mtime=0, xfl=0, os_=3, ftext=False,
           extra=b"AB\x04\x00abcd") -> tuple:
    """Un membre gzip complet : (octets du fichier, décalage du flux). En-tête RFC 1952 § 2.3, flux DEFLATE brut, pied."""
    flg = (0x01 if ftext else 0) | (0x02 if hcrc else 0) | (0x04 if flg_extra else 0) | (0x08 if fichier is not None else 0) | (
        0x10 if commentaire is not None else 0)
    t = bytes([0x1F, 0x8B, 8, flg]) + struct.pack("<I", mtime) + bytes([xfl, os_])
    if flg_extra:
        t += struct.pack("<H", len(extra)) + extra
    if fichier is not None:
        t += fichier.encode("latin-1") + b"\0"
    if commentaire is not None:
        t += commentaire.encode("latin-1") + b"\0"
    if hcrc:
        t += struct.pack("<H", zlib.crc32(t) & 0xFFFF)
    debut = len(t)
    t += deflate_brut(donnees) + struct.pack("<II", zlib.crc32(donnees), len(donnees) & 0xFFFFFFFF)
    return t, debut


def tableau(nom: str, octets: bytes, type_="std::uint8_t") -> str:
    lignes = []
    for i in range(0, len(octets), 16):
        lignes.append("    " + ", ".join(f"0x{b:02x}" for b in octets[i:i + 16]) + ",")
    if not lignes:
        lignes = ["    0,"]  # un tableau vide n'existe pas en C++ : un zéro, que `taille` ne compte pas
    return f"inline constexpr {type_} {nom}[] = {{\n" + "\n".join(lignes) + "\n};\n"


def cas_fabriques():
    """[(nom, octets du fichier, octets décompressés, décalage du flux)]"""
    cas = []

    def ajouter(nom, donnees, **options):
        octets, debut = membre(donnees, **options)
        assert gzip.decompress(octets) == donnees, nom
        cas.append((nom, octets, donnees, debut))

    ajouter("SIMPLE", TEXTE)
    ajouter("FTEXT", TEXTE, ftext=True, mtime=1793966400, xfl=2, os_=255)
    ajouter("FNAME", TEXTE, fichier="meteo.json")
    ajouter("FEXTRA", TEXTE, flg_extra=True)
    ajouter("FEXTRA_VIDE", TEXTE, flg_extra=True, extra=b"")
    ajouter("FEXTRA_LONG", TEXTE, flg_extra=True, extra=b"x" * 300)  # XLEN = 300 : l'octet de poids fort de XLEN n'est pas nul
    ajouter("FCOMMENT", TEXTE, commentaire="capturé à la main")
    ajouter("FHCRC", TEXTE, hcrc=True)
    ajouter("TOUT", TEXTE, flg_extra=True, fichier="a.json", commentaire="tout", hcrc=True, mtime=1, xfl=4, os_=3)
    ajouter("VIDE", b"")
    ajouter("REPETE", b"a" * 70000)  # taille décompressée > 65 535 : l'ISIZE tient sur plus de deux octets
    # Deux membres collés : gzip.decompress (et tous les outils) rendent a + b ; l'appareil n'en lit qu'un seul.
    a, debut_a = membre(TEXTE)
    b, _ = membre(b"deuxieme membre")
    assert gzip.decompress(a + b) == TEXTE + b"deuxieme membre"
    cas.append(("DEUX_MEMBRES", a + b, TEXTE, debut_a))
    return cas


def inc_cas():
    texte = [ENTETE, "// Fichiers gzip fabriqués avec zlib (flux brut) + en-tête écrit à la main, relus par gzip.decompress de Python.\n",
             "// Pour chaque cas : le fichier, le décalage du flux DEFLATE, le CRC32 et la taille du pied, et les octets décompressés\n",
             "// (pour DEUX_MEMBRES : ceux du PREMIER membre seulement).\n"]
    for nom, octets, donnees, debut in cas_fabriques():
        pied_crc, pied_taille = struct.unpack("<II", octets[-8:])
        texte.append(f"\n// {nom} : {len(octets)} octets, flux à partir du décalage {debut}\n")
        texte.append(tableau(f"GZ_{nom}", octets))
        texte.append(f"inline constexpr std::size_t GZ_{nom}_TAILLE = {len(octets)};\n")
        texte.append(f"inline constexpr std::size_t GZ_{nom}_DEBUT = {debut};\n")
        texte.append(f"inline constexpr std::uint32_t GZ_{nom}_CRC = 0x{pied_crc:08x};\n")
        texte.append(f"inline constexpr std::uint32_t GZ_{nom}_ISIZE = {pied_taille};\n")
        if len(donnees) <= 4096:  # au-delà, le test refait le contenu (REPETE : des « a » à la suite) : le .inc resterait énorme
            texte.append(tableau(f"GZ_{nom}_DONNEES", donnees))
        texte.append(f"inline constexpr std::size_t GZ_{nom}_DONNEES_TAILLE = {len(donnees)};\n")
        texte.append(f"inline constexpr std::uint32_t GZ_{nom}_CRC_DONNEES = 0x{zlib.crc32(donnees):08x};\n")
    return "".join(texte)


def inc_openmeteo(lieu: str):
    fil = (DONNEES / f"openmeteo_gzip_{lieu}.gz").read_bytes()
    donnees = gzip.decompress(fil)
    crc, taille = struct.unpack("<II", fil[-8:])
    assert crc == zlib.crc32(donnees) and taille == len(donnees), "le pied de la capture ne correspond pas à sa décompression"
    assert fil[:4] == bytes([0x1F, 0x8B, 8, 0]), "en-tête avec champs facultatifs : adapter _DEBUT"
    nom = "OM_GZ_" + lieu.upper()
    texte = [ENTETE, f"// Réponse RÉELLE d'Open-Meteo pour {lieu}, requête du firmware avec Accept-Encoding: gzip : octets du fil, puis texte\n",
             "// décompressé par Python (gzip.decompress), voir donnees/LISEZMOI.md et capturer_gzip_meteo.py.\n",
             tableau(nom, fil), f"inline constexpr std::size_t {nom}_TAILLE = {len(fil)};\n",
             tableau(f"{nom}_JSON", donnees), f"inline constexpr std::size_t {nom}_JSON_TAILLE = {len(donnees)};\n",
             f"inline constexpr std::uint32_t {nom}_CRC = 0x{zlib.crc32(donnees):08x};\n",
             f"inline constexpr std::size_t {nom}_DEBUT = {10};\n"]
    return "".join(texte), len(fil), len(donnees)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    a = ap.parse_args()
    sorties, resume = {"gzip_cas.inc": inc_cas()}, [f"cas fabriqués : {len(cas_fabriques())}"]
    for lieu in LIEUX:
        texte, n_fil, n_json = inc_openmeteo(lieu)
        sorties[f"openmeteo_gzip_{lieu}.inc"] = texte
        resume.append(f"{lieu}: {n_fil} octets sur le fil, {n_json} décompressés")
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
