#!/usr/bin/env python3
"""Pages NVS RÉELLES (outil officiel d'ESP-IDF) pour les tests de components/core/include/ecran/nvs_securite.hpp.

    python test/host/capturer_nvs.py --capturer   # lance nvs_partition_gen.py d'ESP-IDF (lecture seule), écrit donnees/nvs_*.inc
    python test/host/capturer_nvs.py              # sans ESP-IDF : régénère donnees/attendu_nvs.inc depuis les .inc
    python test/host/capturer_nvs.py --verifier   # sans ESP-IDF : code 1 si attendu_nvs.inc n'est pas à jour

Pourquoi : un classeur de pages testé sur des octets écrits à la main ne prouve pas qu'il lit ce que NVS écrit vraiment. Les pages
viennent donc de l'outil officiel d'ESP-IDF (`generate` = en clair, `encrypt` = XTS-AES avec une clé jetable) à partir d'un CSV de
valeurs d'essai. La clé est tirée au hasard dans un dossier temporaire, ne sert qu'une fois et n'est jamais conservée : seules les
pages (4096 octets chacune, la page 0 de la partition) sont gardées, en littéral C++. Rien n'est écrit dans ESP-IDF
(PYTHONDONTWRITEBYTECODE, python -B), aucune carte n'est touchée.

Le contrôle est INDÉPENDANT du noyau : ce script relit les pages avec struct et zlib (le CRC de NVS est zlib.crc32(octets,
0xFFFFFFFF)), et les nombres attendus sont d'abord DÉDUITS DU CSV (une entrée par espace, 1 + ceil(longueur/32) par texte, 1 par
entier, 2 + chunks par blob) avant d'être comparés à ce que l'analyse trouve dans la page.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from racine_idf import racine_idf  # noqa: E402

DONNEES = Path(__file__).resolve().parent / "donnees"
TAILLE_PAGE = 4096
TAILLE_PARTITION = 0x3000  # trois pages : la dernière reste libre, c'est le minimum de l'outil

# Valeurs d'essai, évidemment fausses (rien qui ressemble à une clé d'API réelle : le contrôle de publication les laisserait passer,
# mais un lecteur humain aussi doit voir que ce sont des exemples).
CSV = """key,type,encoding,value
wifi,namespace,,
ssid,data,string,ReseauEssai
mdp,data,string,essai-mot-de-passe-0001
services,namespace,,
llm.cle,data,string,essai-cle-0002
llm.preset,data,i32,2
installation,namespace,,
etape,data,i32,3
zone,namespace,,
pos,data,hex2bin,00112233445566778899aabbccddeeff
"""
# Ce que le CSV doit produire, déduit à la main de la disposition d'ESP-IDF (nvs_partition_generator, README) :
#   4 espaces                                  = 4 entrées
#   textes : ssid 12 octets, mdp 24, llm.cle 15 = 1 en-tête + ceil(taille/32) entrées de données chacun = 2 + 2 + 2
#   2 entiers 32 bits                          = 2 entrées
#   1 blob de 16 octets (format 2)             = 1 en-tête de morceau + 1 entrée de données + 1 index = 3
# Entrées d'en-tête (celles qui portent une somme de contrôle lisible) : 4 + 3 + 2 + 2 = 11 ; entrées écrites au total : 15.
EN_TETES_ATTENDUS = 11
ECRITES_ATTENDUES = 15
CRC_VECTEUR = b"123456789"


def crc_nvs(octets):
    return zlib.crc32(octets, 0xFFFFFFFF) & 0xFFFFFFFF


def etat_entree(page, rang):
    return (page[32 + rang // 4] >> (2 * (rang % 4))) & 3


def analyser(page):
    """(lisibles, illisibles, restes) d'une page NVS formatée ; écrit à neuf à partir de nvs_page.cpp et nvs_constants.h."""
    etat = struct.unpack_from("<I", page, 0)[0]
    assert etat in (0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8, 0xFFFFFFF0), "page non formatée : %#x" % etat
    assert struct.unpack_from("<I", page, 28)[0] == crc_nvs(page[4:28]), "somme de contrôle d'en-tête fausse"
    lisibles = illisibles = restes = 0
    rang = 0
    while rang < 126:
        e = etat_entree(page, rang)
        if e not in (0, 2):
            rang += 1
            continue
        entree = page[64 + rang * 32 : 64 + rang * 32 + 32]
        bon = struct.unpack_from("<I", entree, 4)[0] == crc_nvs(entree[0:4] + entree[8:32])
        if bon and e == 2:
            lisibles += 1
        elif bon:
            restes += 1
        elif e == 2:
            illisibles += 1
        rang += entree[2] if bon and entree[2] > 0 else 1
    return lisibles, illisibles, restes


def tout_efface(page):
    """La même page après un « effacement logique » : la table d'états passe à zéro, les 32 octets de chaque entrée restent."""
    copie = bytearray(page)
    copie[32:64] = bytes(32)
    return bytes(copie)


def chemin_inc(nom):
    return DONNEES / ("nvs_%s.inc" % nom)


def contenu_inc(nom, page):
    lignes = [
        "// FICHIER GÉNÉRÉ par test/host/capturer_nvs.py : ne pas modifier à la main.",
        "// Page 0 (4096 octets) d'une partition NVS %s, produite par nvs_partition_gen.py d'ESP-IDF 5.5.5 à partir du CSV de"
        % ("EN CLAIR (generate)" if nom == "clair" else "CHIFFRÉE (encrypt, clé jetable non conservée)"),
        "// capturer_nvs.py. Voir donnees/LISEZMOI_nvs.md.",
    ]
    for debut in range(0, len(page), 16):
        lignes.append(",".join("0x%02x" % b for b in page[debut : debut + 16]) + ",")
    return "\n".join(lignes) + "\n"


def lire_page_inc(nom):
    """Relit les octets d'un .inc (ce qui est dans le dépôt, pas ce que l'outil vient de produire)."""
    octets = bytearray()
    for ligne in chemin_inc(nom).read_text(encoding="utf-8").splitlines():
        if ligne.startswith("//") or not ligne.strip():
            continue
        octets += bytes(int(x, 16) for x in ligne.strip().rstrip(",").split(","))
    assert len(octets) == TAILLE_PAGE, "%s : %d octets au lieu de %d" % (chemin_inc(nom).name, len(octets), TAILLE_PAGE)
    return bytes(octets)


def contenu_attendu():
    clair = lire_page_inc("clair")
    chiffre = lire_page_inc("chiffre")
    lis_c, ill_c, restes_c = analyser(clair)
    lis_x, ill_x, restes_x = analyser(chiffre)
    assert (lis_c, ill_c, restes_c) == (EN_TETES_ATTENDUS, 0, 0), "page en clair : %r" % ((lis_c, ill_c, restes_c),)
    assert (lis_x, ill_x, restes_x) == (0, ECRITES_ATTENDUES, 0), "page chiffrée : %r" % ((lis_x, ill_x, restes_x),)
    lis_e, ill_e, restes_e = analyser(tout_efface(clair))
    assert (lis_e, ill_e) == (0, 0) and restes_e == EN_TETES_ATTENDUS, "page effacée : %r" % ((lis_e, ill_e, restes_e),)
    # Les valeurs d'essai restent lisibles dans la page effacée : c'est ce que le test prouve (un secret effacé n'est pas parti).
    assert b"essai-mot-de-passe-0001" in tout_efface(clair)
    return "\n".join(
        [
            "// FICHIER GÉNÉRÉ par test/host/capturer_nvs.py : ne pas modifier à la main.",
            "// Ce que l'analyse doit trouver dans les deux pages, calculé par Python (struct, zlib), pas par le noyau.",
            "inline constexpr std::uint32_t ATTENDU_CLAIR_LISIBLES = %d;" % lis_c,
            "inline constexpr std::uint32_t ATTENDU_CLAIR_ILLISIBLES = %d;" % ill_c,
            "inline constexpr std::uint32_t ATTENDU_CHIFFRE_LISIBLES = %d;" % lis_x,
            "inline constexpr std::uint32_t ATTENDU_CHIFFRE_ILLISIBLES = %d;" % ill_x,
            "inline constexpr std::uint32_t ATTENDU_EFFACE_RESTES = %d;" % restes_e,
            "inline constexpr std::uint32_t ATTENDU_CRC_123456789 = 0x%08x;  // zlib.crc32(b\"123456789\", 0xFFFFFFFF)"
            % crc_nvs(CRC_VECTEUR),
            "",
        ]
    )


def outils_idf():
    racine = racine_idf()
    if not racine:
        sys.exit("ESP-IDF introuvable (MERIDIEN_IDF_RACINE ou tools/local.ps1) : --capturer a besoin de nvs_partition_gen.py")
    generateurs = sorted(racine.glob("frameworks/*/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"))
    pythons = sorted(racine.glob("penvs/*/Scripts/python.exe")) + sorted(racine.glob("penvs/*/bin/python"))
    if not generateurs or not pythons:
        sys.exit("nvs_partition_gen.py ou le Python d'ESP-IDF est introuvable sous %s" % racine)
    return str(pythons[-1]), str(generateurs[-1])


def capturer():
    python, generateur = outils_idf()
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")  # rien n'est écrit dans ESP-IDF
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "essai.csv").write_text(CSV, encoding="utf-8", newline="\n")

        def lancer(*args):
            r = subprocess.run([python, "-B", generateur, *args], cwd=tmp, env=env, capture_output=True, text=True)
            if r.returncode != 0:
                sys.exit("nvs_partition_gen.py %s a échoué :\n%s%s" % (args[0], r.stdout, r.stderr))

        lancer("generate", "essai.csv", "clair.bin", hex(TAILLE_PARTITION), "--outdir", str(tmp))
        lancer("encrypt", "essai.csv", "chiffre.bin", hex(TAILLE_PARTITION), "--keygen", "--keyfile", "cle.bin", "--outdir", str(tmp))
        for nom in ("clair", "chiffre"):
            brut = (tmp / ("%s.bin" % nom)).read_bytes()
            assert len(brut) == TAILLE_PARTITION, "%s.bin : %d octets" % (nom, len(brut))
            chemin_inc(nom).write_text(contenu_inc(nom, brut[:TAILLE_PAGE]), encoding="utf-8", newline="\n")
            print("écrit", chemin_inc(nom).name)
        # La clé jetable disparaît avec le dossier temporaire.


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--capturer", action="store_true", help="INTERROGE ESP-IDF (lecture seule) et réécrit les pages nvs_*.inc")
    ap.add_argument("--verifier", action="store_true", help="code 1 si attendu_nvs.inc n'est pas à jour")
    a = ap.parse_args()
    if a.capturer:
        capturer()
    attendu = contenu_attendu()
    chemin = DONNEES / "attendu_nvs.inc"
    if a.verifier:
        if not chemin.exists() or chemin.read_text(encoding="utf-8") != attendu:
            print("attendu_nvs.inc n'est pas à jour : lancer python test/host/capturer_nvs.py")
            return 1
        print("nvs : tout est à jour (page en clair %d entrées lisibles, page chiffrée %d illisibles)" % (EN_TETES_ATTENDUS, ECRITES_ATTENDUES))
        return 0
    chemin.write_text(attendu, encoding="utf-8", newline="\n")
    print("écrit", chemin.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
