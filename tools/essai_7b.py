#!/usr/bin/env python3
"""Essai physique de la Waveshare 7B : sauvegarde de la flash, essai du firmware, RESTAURATION verifiee.

Une seule commande rejoue tout (voir `essai`), mais chaque etape existe seule. Le script ne decouvre jamais de carte : il ne parle
qu'au port qu'on lui donne, et seulement apres avoir lu la MAC de la puce et l'avoir comparee a celle de la variable
d'environnement MERIDIEN_MAC_7B (la MAC n'est ecrite NULLE PART dans le depot). Le numero de port change avec la prise USB.

Garde-fous (pourquoi ce script est long : une carte en service est en jeu, sa flash contient le firmware qu'Axel utilise) :
  - aucune ecriture sans une sauvegarde COMPLETE, lue DEUX fois avec la meme empreinte SHA-256, ET confirmee par le MD5 calcule
    par la puce elle-meme (verify-flash) : le dossier recoit alors `sauvegarde_ok.json`, que `flasher` et `restaurer` exigent ;
  - la restauration est dans un `finally` : meme si l'essai plante, on remet la sauvegarde puis on la verifie ;
  - la restauration reecrit la flash ENTIERE (jamais d'effacement de puce) et la relit : empreinte identique, sinon le script le dit
    et s'arrete sans improviser (code de sortie 3) ;
  - heure limite (`--limite`, 06:30 par defaut) : passee, on saute les essais et on restaure.

Pieges payes (memoire d'Axel `sauvegarde-flash-p4-pieges`, ESP32-P4 sous esptool 5.3.1) :
  - `read-flash` avec le « stub » echoue a des secteurs fixes : on lit par morceaux de 2 Mo (~21 s), on coupe en 8 en cas d'echec
    et, en dessous de 256 Ko, on retente SANS stub (`--no-stub`, secteurs de 4 Ko, ~9 s par secteur lu) ;
  - apres une coupure du stub, la synchronisation suivante exige `--before default-reset` : on le met partout ;
  - `verify-flash` calcule le MD5 dans la puce (rapide, aucun transfert) : c'est la preuve independante de la relecture ;
  - otadata (0x9000 et 0xa000) : on decode avant de choisir ce qu'on garde (voir `decoder_otadata`).

Ce script est teste sans carte par `python tools/essai_7b.py autotest` (decodeurs, decoupage, empreintes, plan d'ecriture). La
partie qui parle a la carte N'A PAS ete rejouee tant que docs/mesures/7b-*.md ne dit pas le contraire.

Usage (PowerShell 5.1, depuis la racine du depot) :
    $env:MERIDIEN_MAC_7B = "<MAC lue sur l'etiquette ou par esptool>"
    python tools\\essai_7b.py identifier --port COMx
    python tools\\essai_7b.py essai --port COMx --dossier <dossier de sauvegarde> --build build
Codes de sortie : 0 = tout bon ; 1 = essai en echec mais flash restauree et verifiee ; 2 = refus (garde-fou) ; 3 = restauration NON
verifiee (la carte est dans un etat inconnu : lire le message, ne rien tenter d'autre).
"""

from __future__ import annotations

import argparse
import contextlib
import datetime as dt
import hashlib
import io
import json
import os
import random
import re
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path

# --- Constantes de la flash ---------------------------------------------------------------------------------------------------
SECTEUR = 0x1000  # plus petite unite d'effacement de la flash
MORCEAU = 2 * 1024 * 1024  # lecture par morceaux de 2 Mo
MORCEAU_SANS_STUB = 256 * 1024  # en dessous, on retente sans le stub
DECALAGE_TABLE = 0x8000  # table de partitions
TAILLE_TABLE = 0xC00
DECALAGE_OTADATA = 0x9000
TAILLE_OTADATA = 0x2000
MAGIE_PARTITION = b"\xaa\x50"
MAGIE_MD5 = b"\xeb\xeb"
ECHECS_MAX = 400  # une lecture qui echoue plus souvent est abandonnee (la carte est en cause, pas le hasard)

# Ou ecrire les quatre fichiers du build (sortie de `idf.py build`, voir la ligne « To flash » de la compilation).
IMAGES = (
    ("bootloader", 0x2000, Path("bootloader") / "bootloader.bin"),
    ("table", 0x8000, Path("partition_table") / "partition-table.bin"),
    ("otadata", 0x9000, Path("ota_data_initial.bin")),
    ("application", 0x10000, Path("meridien.bin")),
)

# --- Jalons attendus dans le journal serie (texte exact des ESP_LOGx du firmware) ----------------------------------------------
JALONS = (
    ("ecran", r"EK79007 : DSI"),
    ("tactile", r"Tactile : Goodix GT911"),
    ("codecs", r"ES8311 present, ES7210 present"),
    ("carte_prete", r"Carte prête"),
    ("lien_c6", r"lien avec le coprocesseur etabli"),
    ("ip", r"adresse IP obtenue"),
    ("heure", r"horloge réglée"),
)
# Ces motifs sont des ECHECS s'ils apparaissent : reset en boucle, panique, lien du C6 perdu.
MOTIFS_ECHEC = (
    r"Guru Meditation",
    r"abort\(\) was called",
    r"panne du transport",
    r"carte non initialisée",
    r"ASSERT",
)


# --- Fonctions pures (testees par `autotest`) ---------------------------------------------------------------------------------
def crc_otadata(sequence: int) -> int:
    """CRC d'une entree otadata : crc32_le(0xFFFFFFFF, sequence sur 4 octets little-endian), comme le bootloader.

    La memoire d'Axel donne `zlib.crc32(seq_le32, 0xFFFFFFFF)`. Verifie sur l'otadata d'une ancienne sauvegarde de cette carte.
    """
    return zlib.crc32(struct.pack("<I", sequence), 0xFFFFFFFF) & 0xFFFFFFFF


def decoder_otadata(bloc: bytes) -> dict:
    """Decode la partition otadata (2 secteurs de 4 Ko ; une entree de 32 octets au debut de chacun).

    Rend {"entrees": [...], "choisie": numero d'emplacement 0/1 ou None (= application « factory » / ota_0 par defaut)}.
    Une entree compte si sa sequence n'est pas 0xFFFFFFFF ET si son CRC est bon ; l'emplacement est (sequence - 1) % 2.
    """
    entrees = []
    for i in range(2):
        debut = i * SECTEUR
        brut = bloc[debut : debut + 32]
        if len(brut) < 32:
            entrees.append({"sequence": None, "valide": False, "motif": "tronque"})
            continue
        sequence, _etiquette, etat, crc = struct.unpack("<I20sII", brut)
        if sequence == 0xFFFFFFFF:
            entrees.append({"sequence": None, "valide": False, "motif": "vide"})
            continue
        valide = crc == crc_otadata(sequence)
        entrees.append(
            {
                "sequence": sequence,
                "etat": etat,
                "valide": valide,
                "motif": "ok" if valide else "CRC faux",
                "emplacement": (sequence - 1) % 2,
            }
        )
    bonnes = [e for e in entrees if e["valide"]]
    choisie = max(bonnes, key=lambda e: e["sequence"])["emplacement"] if bonnes else None
    return {"entrees": entrees, "choisie": choisie}


def decoder_table(donnees: bytes) -> list[dict]:
    """Decode une table de partitions ESP-IDF (entrees de 32 octets, magie AA 50, fin sur l'entree MD5 EB EB ou du vide)."""
    partitions = []
    for debut in range(0, min(len(donnees), TAILLE_TABLE), 32):
        entree = donnees[debut : debut + 32]
        if len(entree) < 32 or entree[:2] == MAGIE_MD5 or entree[:2] != MAGIE_PARTITION:
            break
        _magie, type_, sous_type, decalage, taille, nom, drapeaux = struct.unpack("<2sBBII16sI", entree)
        partitions.append(
            {
                "nom": nom.split(b"\x00", 1)[0].decode("ascii", "replace"),
                "type": type_,
                "sous_type": sous_type,
                "decalage": decalage,
                "taille": taille,
                "drapeaux": drapeaux,
            }
        )
    return partitions


def plages_ecrites(dossier_build: Path) -> list[tuple[str, int, int]]:
    """(nom, debut, fin) de chaque region que `flasher` va ecrire, arrondie au secteur (fin exclue)."""
    plages = []
    for nom, decalage, relatif in IMAGES:
        taille = (dossier_build / relatif).stat().st_size
        arrondi = -(-taille // SECTEUR) * SECTEUR
        plages.append((nom, decalage, decalage + arrondi))
    return plages


def chevauchements(table: list[dict], plages: list[tuple[str, int, int]]) -> list[str]:
    """Partitions de l'ancienne table que l'ecriture va entamer (informatif : la restauration remet tout)."""
    gravees = []
    for p in table:
        p_debut, p_fin = p["decalage"], p["decalage"] + p["taille"]
        for nom, debut, fin in plages:
            if debut < p_fin and p_debut < fin:
                gravees.append(f"{p['nom']} (0x{p_debut:x}-0x{p_fin:x}) <- {nom} (0x{debut:x}-0x{fin:x})")
    return gravees


def decouper(adresse: int, longueur: int) -> list[tuple[int, int]]:
    """Coupe une lecture en 8 morceaux alignes sur 4 Ko (le dernier prend le reste) ; ne coupe pas en dessous d'un secteur."""
    if longueur <= SECTEUR:
        return [(adresse, longueur)]
    pas = max(SECTEUR, -(-(longueur // 8) // SECTEUR) * SECTEUR)
    morceaux = []
    for debut in range(0, longueur, pas):
        morceaux.append((adresse + debut, min(pas, longueur - debut)))
    return morceaux


def plan_lecture(taille: int, morceau: int = MORCEAU) -> list[tuple[int, int]]:
    """Plan de lecture de toute la flash par morceaux de `morceau` octets."""
    return [(debut, min(morceau, taille - debut)) for debut in range(0, taille, morceau)]


def empreinte(donnees: bytes) -> str:
    return hashlib.sha256(donnees).hexdigest()


def empreinte_fichier(chemin: Path) -> str:
    h = hashlib.sha256()
    with chemin.open("rb") as f:
        for bloc in iter(lambda: f.read(1024 * 1024), b""):
            h.update(bloc)
    return h.hexdigest()


def analyser_journal(texte: str) -> dict:
    """Jalons trouves, echecs trouves, et ordre d'apparition (numero de ligne) de chaque jalon."""
    trouves, lignes = {}, texte.splitlines()
    for nom, motif in JALONS:
        for numero, ligne in enumerate(lignes):
            if re.search(motif, ligne):
                trouves[nom] = numero + 1
                break
    echecs = [ligne for ligne in lignes if any(re.search(m, ligne) for m in MOTIFS_ECHEC)]
    return {"jalons": trouves, "manquants": [n for n, _ in JALONS if n not in trouves], "echecs": echecs}


def moyenne_decalage(texte: str) -> dict:
    """Decalage moyen des appuis simules de l'autotest tactile (lignes « AUTOTEST appui k/5 ... OK/ECART »)."""
    lignes = [ligne for ligne in texte.splitlines() if "AUTOTEST appui" in ligne]
    return {"appuis": len(lignes), "ok": sum(1 for ligne in lignes if ligne.rstrip().endswith("OK")), "lignes": lignes}


# --- Appels a esptool et au port serie ----------------------------------------------------------------------------------------
class Refus(Exception):
    """Un garde-fou a refuse l'operation (code de sortie 2)."""


def esptool(port: str, args: list[str], sans_stub: bool = False, delai: int = 900, avant: str = "default-reset"):
    commande = [sys.executable, "-m", "esptool", "--chip", "esp32p4", "--port", port, "--before", avant]
    if sans_stub:
        commande.append("--no-stub")
    commande += args
    return subprocess.run(commande, capture_output=True, text=True, timeout=delai, encoding="utf-8", errors="replace")


def lire_mac(port: str) -> str:
    sortie = esptool(port, ["read-mac"], delai=60)
    m = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", sortie.stdout + sortie.stderr)
    if sortie.returncode != 0 or not m:
        raise Refus(f"MAC illisible sur {port} (esptool rend {sortie.returncode}) : {(sortie.stdout + sortie.stderr)[-300:]}")
    return m.group(1).lower()


def verifier_carte(port: str) -> None:
    """Refuse tout si la puce du port n'est pas celle de la variable d'environnement MERIDIEN_MAC_7B."""
    attendue = os.environ.get("MERIDIEN_MAC_7B", "").strip().lower()
    if not re.fullmatch(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", attendue):
        raise Refus("variable d'environnement MERIDIEN_MAC_7B absente ou mal formee (aa:bb:cc:dd:ee:ff)")
    lue = lire_mac(port)
    if lue != attendue:
        raise Refus(f"{port} n'est PAS la 7B attendue (MAC differente) : on ne touche a rien")
    print(f"[carte] {port} : la MAC correspond a MERIDIEN_MAC_7B")


def taille_flash(port: str) -> int:
    sortie = esptool(port, ["flash-id"], delai=60)
    m = re.search(r"Detected flash size:\s*(\d+)\s*MB", sortie.stdout + sortie.stderr)
    if not m:
        raise Refus("taille de flash non lue par `flash-id`")
    taille = int(m.group(1)) * 1024 * 1024
    print(f"[flash] taille detectee : {taille // (1024 * 1024)} Mo")
    return taille


def lire_morceau(port: str, adresse: int, longueur: int, compteur: dict, sans_stub: bool = False) -> bytes:
    """Lit [adresse, adresse+longueur).

    Echec : on coupe en 8 ; a partir de 256 Ko les morceaux sont lus SANS stub ; un secteur de 4 Ko qui echoue est d'abord retente
    sans stub, puis, s'il echoue encore sans stub, la lecture est abandonnee (rien n'est ecrit).
    """
    with tempfile.TemporaryDirectory() as tmp:
        fichier = Path(tmp) / "morceau.bin"
        sortie = esptool(port, ["read-flash", hex(adresse), hex(longueur), str(fichier)], sans_stub=sans_stub, delai=600)
        if sortie.returncode == 0 and fichier.exists() and fichier.stat().st_size == longueur:
            return fichier.read_bytes()
    compteur["echecs"] = compteur.get("echecs", 0) + 1
    print(f"[lecture] echec en 0x{adresse:x} (+0x{longueur:x}), n° {compteur['echecs']}, sans stub : {sans_stub}")
    if compteur["echecs"] > ECHECS_MAX:
        raise Refus("trop d'echecs de lecture : la carte ou le cable est en cause, rien n'est ecrit")
    if longueur <= SECTEUR:
        if sans_stub:
            raise Refus(f"secteur 0x{adresse:x} illisible, meme sans stub")
        return lire_morceau(port, adresse, longueur, compteur, sans_stub=True)
    suite = sans_stub or longueur <= MORCEAU_SANS_STUB
    return b"".join(lire_morceau(port, a, n, compteur, suite) for a, n in decouper(adresse, longueur))


def lire_toute_la_flash(port: str, taille: int, sortie: Path) -> str:
    """Lit toute la flash dans `sortie` ; rend son SHA-256. Affiche l'avancement (la lecture prend quelques minutes)."""
    compteur: dict = {}
    h = hashlib.sha256()
    debut = time.monotonic()
    with sortie.open("wb") as f:
        for adresse, longueur in plan_lecture(taille):
            donnees = lire_morceau(port, adresse, longueur, compteur)
            f.write(donnees)
            h.update(donnees)
            print(f"[lecture] {adresse + longueur:#x} / {taille:#x} ({time.monotonic() - debut:.0f} s)")
    return h.hexdigest()


def verifier_puce(port: str, fichier: Path, decalage: int = 0) -> bool:
    """verify-flash : le MD5 de la region est calcule par la puce, sans transfert."""
    sortie = esptool(port, ["verify-flash", hex(decalage), str(fichier)], delai=1200)
    ok = sortie.returncode == 0
    print(f"[verify-flash] {fichier.name} @ {decalage:#x} : {'identique' if ok else 'DIFFERENT'}")
    if not ok:
        print((sortie.stdout + sortie.stderr)[-600:])
    return ok


# --- Etapes ------------------------------------------------------------------------------------------------------------------
def etape_sauvegarde(port: str, dossier: Path) -> dict:
    taille = taille_flash(port)
    dossier.mkdir(parents=True, exist_ok=True)
    f1, f2 = dossier / "flash_lecture1.bin", dossier / "flash_lecture2.bin"
    sha1 = lire_toute_la_flash(port, taille, f1)
    sha2 = lire_toute_la_flash(port, taille, f2)
    identiques = sha1 == sha2
    print(f"[sauvegarde] lecture 1 {sha1}\n[sauvegarde] lecture 2 {sha2}\n[sauvegarde] {'IDENTIQUES' if identiques else 'DIFFERENTES'}")
    if not identiques:
        raise Refus("les deux lectures different : la sauvegarde n'est pas fiable, rien n'est ecrit")
    if not verifier_puce(port, f1):
        raise Refus("verify-flash (MD5 de la puce) different de la lecture : sauvegarde non fiable, rien n'est ecrit")
    donnees = f1.read_bytes()
    table = decoder_table(donnees[DECALAGE_TABLE : DECALAGE_TABLE + TAILLE_TABLE])
    otadata = decoder_otadata(donnees[DECALAGE_OTADATA : DECALAGE_OTADATA + TAILLE_OTADATA])
    del donnees
    f2.unlink()  # une seule copie de 32 Mo sur le disque : la seconde lecture n'a servi qu'a la comparaison
    marque = {
        "date": dt.datetime.now().isoformat(timespec="seconds"),
        "taille": taille,
        "sha256": sha1,
        "lectures_identiques": True,
        "verify_flash_puce": True,
        "fichier": f1.name,
        "table": table,
        "otadata": otadata,
    }
    (dossier / "sauvegarde_ok.json").write_text(json.dumps(marque, indent=2, ensure_ascii=False), encoding="utf-8")
    (dossier / "sha256.txt").write_text(f"{sha1}  {f1.name}\n", encoding="ascii")
    afficher_table(table, otadata)
    return marque


def afficher_table(table: list[dict], otadata: dict) -> None:
    print("[table] partitions du firmware en service :")
    for p in table:
        print(f"    {p['nom']:<14} type {p['type']:#04x} sous-type {p['sous_type']:#04x} @ {p['decalage']:#09x} + {p['taille']:#x}")
    print(f"[otadata] entrees : {otadata['entrees']} ; emplacement choisi : {otadata['choisie']}")


def charger_marque(dossier: Path) -> dict:
    chemin = dossier / "sauvegarde_ok.json"
    if not chemin.exists():
        raise Refus(f"{chemin} absent : pas de sauvegarde confirmee, aucune ecriture")
    marque = json.loads(chemin.read_text(encoding="utf-8"))
    flash = dossier / marque["fichier"]
    if not flash.exists() or empreinte_fichier(flash) != marque["sha256"]:
        raise Refus("la sauvegarde sur disque ne correspond plus a son empreinte : aucune ecriture")
    return marque


def etape_flasher(port: str, build: Path, dossier: Path) -> None:
    marque = charger_marque(dossier)
    plages = plages_ecrites(build)
    gravees = chevauchements(marque["table"], plages)
    print("[flasher] regions ecrites : " + ", ".join(f"{n} 0x{d:x}-0x{f:x}" for n, d, f in plages))
    print("[flasher] partitions entamees : " + ("; ".join(gravees) if gravees else "aucune"))
    args = ["write-flash", "--flash-mode", "dio", "--flash-size", "32MB", "--flash-freq", "80m"]
    for _nom, decalage, relatif in IMAGES:
        args += [hex(decalage), str(build / relatif)]
    sortie = esptool(port, args, delai=900, avant="default-reset")
    print(sortie.stdout[-500:])
    if sortie.returncode != 0:
        raise RuntimeError(f"write-flash a echoue ({sortie.returncode}) : {sortie.stderr[-400:]}")
    for _nom, decalage, relatif in IMAGES:
        if not verifier_puce(port, build / relatif, decalage):
            raise RuntimeError(f"verify-flash different apres l'ecriture de {relatif}")


def etape_restaurer(port: str, dossier: Path, relire: bool = True) -> bool:
    """Reecrit la flash ENTIERE depuis la sauvegarde, la verifie dans la puce et (par defaut) la relit. Rend True si tout est bon."""
    marque = charger_marque(dossier)
    flash = dossier / marque["fichier"]
    print(f"[restauration] ecriture de {flash.name} ({flash.stat().st_size // (1024 * 1024)} Mo) depuis 0x0")
    # `keep` : esptool ne retouche pas l'en-tete du bootloader (mode, taille, frequence), il garde ceux de la sauvegarde.
    sortie = esptool(
        port,
        ["write-flash", "--flash-mode", "keep", "--flash-size", "keep", "--flash-freq", "keep", "0x0", str(flash)],
        delai=1800,
    )
    print(sortie.stdout[-400:])
    if sortie.returncode != 0:
        print(f"[restauration] write-flash a rendu {sortie.returncode} : {sortie.stderr[-400:]}")
        return False
    if not verifier_puce(port, flash):
        return False
    if relire:
        with tempfile.TemporaryDirectory() as tmp:
            relu = Path(tmp) / "relecture.bin"
            sha = lire_toute_la_flash(port, marque["taille"], relu)
        if sha != marque["sha256"]:
            print(f"[restauration] RELECTURE DIFFERENTE de la sauvegarde ({sha})")
            return False
        print("[restauration] relecture : empreinte identique a la sauvegarde")
    esptool(port, ["chip-id"], delai=60)  # simple resynchronisation puis fin de session
    return True


def ouvrir_port(port: str, delai_max: float = 20.0):
    import serial  # pyserial

    fin = time.monotonic() + delai_max
    derniere = None
    while time.monotonic() < fin:
        try:
            s = serial.Serial(port, 115200, timeout=0.2)
            s.dtr = False
            s.rts = False
            return s
        except (serial.SerialException, OSError) as e:  # le port reapparait apres un reset de la puce
            derniere = e
            time.sleep(0.3)
    raise RuntimeError(f"port {port} non reouvert en {delai_max:.0f} s : {derniere}")


def capturer(port: str, secondes: float, sortie: Path | None, reinitialiser: bool = True, arret: str | None = None) -> str:
    """Reinitialise la puce (esptool, fin « hard-reset ») puis note ce que la carte ecrit sur le port serie."""
    if reinitialiser:
        esptool(port, ["chip-id"], delai=60)  # fin de session par defaut : « hard-reset », la carte demarre son application
    texte, tampon = [], b""
    fin = time.monotonic() + secondes
    with ouvrir_port(port) as s:
        while time.monotonic() < fin:
            try:
                tampon += s.read(4096)
            except OSError:
                time.sleep(0.2)
                continue
            while b"\n" in tampon:
                ligne, tampon = tampon.split(b"\n", 1)
                texte.append(ligne.decode("utf-8", "replace").rstrip("\r"))
                if arret and re.search(arret, texte[-1]):
                    fin = min(fin, time.monotonic() + 3.0)
    contenu = "\n".join(texte) + "\n"
    if sortie is not None:
        sortie.parent.mkdir(parents=True, exist_ok=True)
        sortie.write_text(contenu, encoding="utf-8")
    return contenu


def etape_essai(port: str, build: Path, dossier: Path, journaux: Path, limite: dt.datetime, resets: int) -> int:
    """Essai complet. La restauration est dans le `finally`. Rend le code de sortie du script."""
    code = 1
    rapport: dict = {"debut": dt.datetime.now().isoformat(timespec="seconds")}
    ecrit = False
    try:
        if not (dossier / "sauvegarde_ok.json").exists():
            etape_sauvegarde(port, dossier)
        charger_marque(dossier)
        if dt.datetime.now() >= limite:
            raise Refus("heure limite deja passee : aucune ecriture")
        ecrit = True  # des l'instant ou on lance l'ecriture, la restauration est obligatoire
        etape_flasher(port, build, dossier)
        journaux.mkdir(parents=True, exist_ok=True)
        premier = capturer(port, 90, journaux / "demarrage.log", reinitialiser=True, arret=r"horloge réglée")
        rapport["demarrage"] = analyser_journal(premier)
        rapport["tactile_autotest"] = moyenne_decalage(premier)
        print(f"[essai] demarrage : jalons {rapport['demarrage']['jalons']} ; manquants {rapport['demarrage']['manquants']}")
        reussis = 0
        for n in range(1, resets + 1):
            if dt.datetime.now() >= limite:
                print(f"[essai] heure limite atteinte avant la remise a zero n° {n} : on s'arrete et on restaure")
                break
            texte = capturer(port, 25, journaux / f"reset_{n:02d}.log", reinitialiser=True, arret=r"Carte prête")
            analyse = analyser_journal(texte)
            bon = "carte_prete" in analyse["jalons"] and not analyse["echecs"]
            reussis += bon
            print(f"[essai] remise a zero {n}/{resets} : {'ok' if bon else 'ECHEC'} {analyse['echecs'][:1]}")
        rapport["resets"] = {"demandes": resets, "reussies": reussis}
        tout_bon = not rapport["demarrage"]["manquants"] and not rapport["demarrage"]["echecs"] and reussis == resets
        code = 0 if tout_bon else 1
    except Refus as e:
        print(f"[REFUS] {e}")
        code = 2
    except Exception as e:  # noqa: BLE001 - on veut restaurer quoi qu'il arrive
        print(f"[ERREUR] {type(e).__name__} : {e}")
        code = 1
    finally:
        if ecrit:
            print("[essai] RESTAURATION")
            restauree = False
            try:
                restauree = etape_restaurer(port, dossier)
            except Exception as e:  # noqa: BLE001
                print(f"[restauration] exception : {type(e).__name__} : {e}")
            rapport["restauration_verifiee"] = restauree
            if not restauree:
                print(
                    "[restauration] NON VERIFIEE. Ne rien effacer. Relancer a la main :\n"
                    f"    python tools\\essai_7b.py restaurer --port {port} --dossier {dossier}"
                )
                code = 3
        rapport["fin"] = dt.datetime.now().isoformat(timespec="seconds")
        rapport["code"] = code
        journaux.mkdir(parents=True, exist_ok=True)
        (journaux / "rapport_essai.json").write_text(json.dumps(rapport, indent=2, ensure_ascii=False), encoding="utf-8")
    return code


# --- Autotest (sans carte) ----------------------------------------------------------------------------------------------------
def _entree_table(nom: str, type_: int, sous: int, decalage: int, taille: int) -> bytes:
    return struct.pack("<2sBBII16sI", MAGIE_PARTITION, type_, sous, decalage, taille, nom.encode().ljust(16, b"\0"), 0)


# --- Carte simulee : l'essai complet, y compris la restauration, rejoue contre une flash en memoire ---------------------------
class _FausseCarte:
    """Flash simulee qui repond comme esptool (read-mac, flash-id, read-flash, verify-flash, write-flash, chip-id).

    Defauts simulables : un secteur que la lecture AVEC stub ne sait pas lire (comme sur le P4), une flash qui change entre deux
    lectures, une ecriture a l'adresse 0 qui echoue (restauration impossible), une MAC differente.
    """

    def __init__(self, mac="aa:bb:cc:dd:ee:01", taille=8 * 1024 * 1024):
        self.flash = bytearray(random.Random(7).randbytes(taille))
        self.mac = mac
        self.secteur_mauvais = None
        self.alterer_apres = None  # numero de l'appel read-flash apres lequel un octet de la flash change
        self.echec_ecriture_zero = False
        self.bruit_apres = None  # a partir de cet appel read-flash, les donnees LUES different de la flash (flash intacte)
        self.illisible = None  # un secteur que meme la lecture sans stub refuse
        self.corrompre_ecriture = False  # une ecriture a l'adresse 0 reussit mais altere un octet (la puce le verrait)
        self.appels = []
        self._lectures = 0

    def _reponse(self, args, code=0, sortie=""):
        return subprocess.CompletedProcess(args, code, sortie, "")

    def esptool(self, port, args, sans_stub=False, delai=0, avant="default-reset"):
        self.appels.append((tuple(args), sans_stub))
        cmd = args[0]
        if cmd == "read-mac":
            return self._reponse(args, 0, f"MAC: {self.mac}\n")
        if cmd == "flash-id":
            return self._reponse(args, 0, f"Detected flash size: {len(self.flash) // (1024 * 1024)}MB\n")
        if cmd == "chip-id":
            return self._reponse(args)
        if cmd == "read-flash":
            adresse, longueur, fichier = int(args[1], 0), int(args[2], 0), args[3]
            self._lectures += 1
            if self.alterer_apres is not None and self._lectures == self.alterer_apres + 1:
                self.flash[5 * 1024 * 1024] ^= 0xFF
            if not sans_stub and self.secteur_mauvais is not None and adresse <= self.secteur_mauvais < adresse + longueur:
                return self._reponse(args, 2, "Packet content transfer stopped")
            if self.illisible is not None and adresse <= self.illisible < adresse + longueur:
                return self._reponse(args, 2, "Failed to read")
            donnees = bytes(self.flash[adresse : adresse + longueur])
            if self.bruit_apres is not None and self._lectures > self.bruit_apres:
                donnees = bytes([donnees[0] ^ 0xFF]) + donnees[1:]
            Path(fichier).write_bytes(donnees)
            return self._reponse(args)
        if cmd == "verify-flash":
            adresse, donnees = int(args[1], 0), Path(args[2]).read_bytes()
            return self._reponse(args, 0 if bytes(self.flash[adresse : adresse + len(donnees)]) == donnees else 2)
        if cmd == "write-flash":
            reste, paires, i = args[1:], [], 0
            while i < len(reste):
                if reste[i].startswith("--"):
                    i += 2
                    continue
                paires.append((int(reste[i], 0), Path(reste[i + 1])))
                i += 2
            if self.echec_ecriture_zero and any(a == 0 for a, _ in paires):
                return self._reponse(args, 2, "write failed")
            for adresse, fichier in paires:
                donnees = fichier.read_bytes()
                self.flash[adresse : adresse + len(donnees)] = donnees
                if self.corrompre_ecriture and adresse == 0:
                    self.flash[len(donnees) - 1] ^= 0xFF
            return self._reponse(args)
        return self._reponse(args, 2, f"commande inconnue {cmd}")

    def ecritures(self):
        return [a for a, _ in self.appels if a[0] == "write-flash"]


JOURNAL_SIMULE = "\n".join(
    [
        "I (900) ws7b_display: EK79007 : DSI 2 voies",
        "I (950) ws7b_board: Tactile : Goodix GT911 (0x5d)",
        "I (960) ws7b_board: Codecs audio sur le bus I2C : ES8311 present, ES7210 present",
        "I (970) ws7b_board: Carte prête : Waveshare 7B",
        "I (2100) ws7b_wifi: lien avec le coprocesseur etabli a 2100 ms",
        "I (3400) reseau: adresse IP obtenue : x à 3400 ms",
        "I (3900) sntp_rapide: horloge réglée à +120 ms",
    ]
)


@contextlib.contextmanager
def _remplacer(**choses):
    """Remplace des fonctions du module le temps d'un bloc (les etapes les cherchent dans les globales a chaque appel)."""
    globales = globals()
    anciennes = {nom: globales[nom] for nom in choses}
    globales.update(choses)
    try:
        yield
    finally:
        globales.update(anciennes)


def _simulation(verifier) -> None:
    """Rejoue sauvegarde, flash, essai et restauration contre une carte simulee ; verifie surtout que la flash est REMISE."""
    silence = contextlib.redirect_stdout(io.StringIO())

    def capture_ok(port, secondes, sortie, reinitialiser=True, arret=None):
        return JOURNAL_SIMULE

    def capture_plantee(port, secondes, sortie, reinitialiser=True, arret=None):
        raise RuntimeError("port disparu")

    def dossiers(tmp):
        build = Path(tmp) / "build"
        (build / "bootloader").mkdir(parents=True)
        (build / "partition_table").mkdir()
        rng = random.Random(11)
        (build / "bootloader" / "bootloader.bin").write_bytes(rng.randbytes(20000))
        (build / "partition_table" / "partition-table.bin").write_bytes(rng.randbytes(3072))
        (build / "ota_data_initial.bin").write_bytes(rng.randbytes(8192))
        (build / "meridien.bin").write_bytes(rng.randbytes(300000))
        return build, Path(tmp) / "sauvegarde", Path(tmp) / "journaux"

    futur = dt.datetime.now() + dt.timedelta(hours=1)
    mac = "aa:bb:cc:dd:ee:01"
    os.environ["MERIDIEN_MAC_7B"] = mac

    # 1. Essai nominal ; la lecture avec stub echoue au secteur 0x300000 (comme sur le P4) -> decoupe puis sans stub
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        origine = bytes(carte.flash)
        carte.secteur_mauvais = 0x300000
        build, dossier, journaux = dossiers(tmp)
        with _remplacer(esptool=carte.esptool, capturer=capture_ok), silence:
            code = etape_essai("COMX", build, dossier, journaux, futur, 3)
        verifier(code == 0, "simulation : essai nominal, code de sortie 0")
        verifier(bytes(carte.flash) == origine, "simulation : la flash est IDENTIQUE a l'origine apres l'essai (restauration)")
        verifier(any(sans for _, sans in carte.appels), "simulation : un secteur illisible avec le stub est lu sans stub")
        verifier(len(carte.ecritures()) == 2, "simulation : deux ecritures seulement (les quatre images, puis la restauration)")
        rapport = json.loads((journaux / "rapport_essai.json").read_text(encoding="utf-8"))
        verifier(rapport["restauration_verifiee"] is True, "simulation : le rapport dit que la restauration est verifiee")
        marque = json.loads((dossier / "sauvegarde_ok.json").read_text(encoding="utf-8"))
        verifier(marque["sha256"] == empreinte(origine), "simulation : l'empreinte notee est celle de la flash d'origine")
        verifier(not (dossier / "flash_lecture2.bin").exists(), "simulation : une seule copie de la flash gardee sur le disque")
        premier_write = next(i for i, (a, _) in enumerate(carte.appels) if a[0] == "write-flash")
        dernier_read = max(i for i, (a, _) in enumerate(carte.appels) if a[0] == "read-flash")
        verifier(dernier_read > premier_write, "simulation : la restauration est relue apres l'ecriture")
        lectures_avant = [a for a, _ in carte.appels[:premier_write] if a[0] == "verify-flash"]
        verifier(len(lectures_avant) == 1, "simulation : verify-flash (MD5 de la puce) passe AVANT toute ecriture")

    # 2. Le port disparait pendant l'essai : la restauration a lieu quand meme
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        origine = bytes(carte.flash)
        build, dossier, journaux = dossiers(tmp)
        with _remplacer(esptool=carte.esptool, capturer=capture_plantee), silence:
            code = etape_essai("COMX", build, dossier, journaux, futur, 3)
        verifier(code == 1, "simulation : un plantage de l'essai rend le code 1")
        verifier(bytes(carte.flash) == origine, "simulation : le plantage de l'essai n'empeche pas la restauration")

    # 3. La restauration echoue : le script le dit, rend 3, et n'efface rien
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        build, dossier, journaux = dossiers(tmp)
        carte.echec_ecriture_zero = True
        with _remplacer(esptool=carte.esptool, capturer=capture_ok), silence:
            code = etape_essai("COMX", build, dossier, journaux, futur, 2)
        verifier(code == 3, "simulation : restauration impossible -> code 3")
        rapport = json.loads((journaux / "rapport_essai.json").read_text(encoding="utf-8"))
        verifier(rapport["restauration_verifiee"] is False, "simulation : le rapport dit que la restauration N'EST PAS verifiee")
        verifier(not any(a[0] == "erase-flash" for a, _ in carte.appels), "simulation : aucun effacement de puce, jamais")

    # 4. MAC differente : refus avant la moindre lecture de la flash
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte(mac="aa:bb:cc:dd:ee:99")
        try:
            with _remplacer(esptool=carte.esptool), silence:
                verifier_carte("COMX")
            verifier(False, "simulation : une MAC differente doit refuser")
        except Refus:
            verifier(True, "simulation : une MAC differente refuse")
        verifier([a[0] for a, _ in carte.appels] == ["read-mac"], "simulation : seule la MAC a ete lue avant le refus")

    # 5. Heure limite passee : la sauvegarde se fait (lecture seule), aucune ecriture n'a lieu
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        build, dossier, journaux = dossiers(tmp)
        passe = dt.datetime.now() - dt.timedelta(minutes=1)
        with _remplacer(esptool=carte.esptool, capturer=capture_ok), silence:
            code = etape_essai("COMX", build, dossier, journaux, passe, 2)
        verifier(code == 2 and carte.ecritures() == [], "simulation : heure limite passee -> code 2 et AUCUNE ecriture")

    # 6. La flash change entre les deux lectures : la sauvegarde est refusee, rien n'est ecrit
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        carte.alterer_apres = 5
        build, dossier, journaux = dossiers(tmp)
        with _remplacer(esptool=carte.esptool, capturer=capture_ok), silence:
            code = etape_essai("COMX", build, dossier, journaux, futur, 2)
        verifier(code == 2 and carte.ecritures() == [], "simulation : deux lectures differentes -> refus, AUCUNE ecriture")
        verifier(not (dossier / "sauvegarde_ok.json").exists(), "simulation : pas de sauvegarde_ok.json si les lectures different")

    # 6b. Les DONNEES lues different d'une lecture a l'autre alors que la flash, elle, est intacte (le MD5 de la puce ne le voit
    #     pas) : seule la comparaison des deux empreintes protege
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        carte.bruit_apres = 4
        build, dossier, journaux = dossiers(tmp)
        with _remplacer(esptool=carte.esptool, capturer=capture_ok), silence:
            code = etape_essai("COMX", build, dossier, journaux, futur, 2)
        verifier(code == 2 and carte.ecritures() == [], "simulation : lectures differentes, flash intacte -> refus sans ecriture")

    # 6c. Une ecriture de restauration qui reussit mais altere la flash est vue par verify-flash SEUL (sans relecture)
    for relire in (False, True):
        with tempfile.TemporaryDirectory() as tmp:
            carte = _FausseCarte()
            _build, dossier, _journaux = dossiers(tmp)
            with _remplacer(esptool=carte.esptool), silence:
                etape_sauvegarde("COMX", dossier)
                carte.corrompre_ecriture = True
                restauree = etape_restaurer("COMX", dossier, relire=relire)
            verifier(restauree is False, f"simulation : une restauration alteree est refusee (relecture={relire})")

    # 6d. Lecture d'un secteur : avec le stub en echec on retente sans stub ; illisible meme sans stub -> refus
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        carte.secteur_mauvais = 0x300000
        with _remplacer(esptool=carte.esptool), silence:
            lu = lire_morceau("COMX", 0x300000, SECTEUR, {})
        verifier(lu == bytes(carte.flash[0x300000 : 0x300000 + SECTEUR]), "simulation : un secteur refuse avec le stub est lu sans stub")
        carte.illisible = 0x300000
        try:
            with _remplacer(esptool=carte.esptool), silence:
                lire_morceau("COMX", 0x300000, SECTEUR, {})
            verifier(False, "simulation : un secteur illisible doit refuser")
        except Refus:
            verifier(True, "simulation : un secteur illisible meme sans stub refuse la sauvegarde")

    # 7. Flasher a la main sans sauvegarde confirmee : refus
    with tempfile.TemporaryDirectory() as tmp:
        carte = _FausseCarte()
        build, dossier, _journaux = dossiers(tmp)
        try:
            with _remplacer(esptool=carte.esptool), silence:
                etape_flasher("COMX", build, dossier)
            verifier(False, "simulation : flasher sans sauvegarde doit refuser")
        except Refus:
            verifier(carte.ecritures() == [], "simulation : flasher sans sauvegarde confirmee refuse, rien n'est ecrit")


def autotest() -> int:
    """Verifie les fonctions pures avec des donnees fabriquees. Aucune carte, aucun port."""
    ok = True

    def verifier(condition: bool, nom: str) -> None:
        nonlocal ok
        print(f"  {'ok ' if condition else 'ECHEC'} {nom}")
        ok = ok and bool(condition)

    # otadata : deux entrees, la sequence 3 (emplacement 0) l'emporte sur la 2 (emplacement 1)
    def entree(seq: int, crc: int | None = None) -> bytes:
        return struct.pack("<I20sII", seq, b"\xff" * 20, 0xFFFFFFFF, crc_otadata(seq) if crc is None else crc)

    bloc = entree(3).ljust(SECTEUR, b"\xff") + entree(2).ljust(SECTEUR, b"\xff")
    verifier(decoder_otadata(bloc)["choisie"] == 0, "otadata : la plus grande sequence valide l'emporte (3 -> emplacement 0)")
    bloc = entree(2).ljust(SECTEUR, b"\xff") + entree(3, crc=0).ljust(SECTEUR, b"\xff")
    verifier(decoder_otadata(bloc)["choisie"] == 1, "otadata : une entree au CRC faux est ignoree (2 -> emplacement 1)")
    verifier(decoder_otadata(b"\xff" * TAILLE_OTADATA)["choisie"] is None, "otadata : vide -> aucune entree (application par defaut)")
    verifier(crc_otadata(1) != crc_otadata(2), "otadata : le CRC depend de la sequence")

    # table de partitions
    table = b"".join(
        [_entree_table("nvs", 1, 2, 0x9000, 0x6000), _entree_table("ota_0", 0, 0x10, 0x10000, 0x400000), MAGIE_MD5 + b"\xff" * 30]
    ).ljust(TAILLE_TABLE, b"\xff")
    decodee = decoder_table(table)
    verifier([p["nom"] for p in decodee] == ["nvs", "ota_0"], "table : deux partitions lues, l'entree MD5 arrete la lecture")
    verifier(decoder_table(b"\xff" * TAILLE_TABLE) == [], "table : flash vide -> aucune partition")

    # chevauchements
    plages = [("application", 0x10000, 0x10000 + 0x200000)]
    verifier(len(chevauchements(decodee, plages)) == 1, "chevauchement : l'application entame ota_0 et pas nvs")
    verifier(chevauchements(decodee, [("x", 0x20000000, 0x20001000)]) == [], "chevauchement : region hors table -> aucune")

    # decoupage et plan de lecture
    verifier(plan_lecture(32 * 1024 * 1024)[-1] == (15 * MORCEAU, MORCEAU), "plan : 32 Mo = 16 morceaux de 2 Mo")
    verifier(sum(n for _, n in plan_lecture(5_000_000)) == 5_000_000, "plan : couvre exactement la taille demandee")
    morceaux = decouper(0x400000, 0x200000)
    verifier(len(morceaux) == 8 and sum(n for _, n in morceaux) == 0x200000, "decoupage : 2 Mo -> 8 morceaux de 256 Ko")
    verifier(all(a % SECTEUR == 0 for a, _ in morceaux), "decoupage : tous alignes sur 4 Ko")
    verifier(decouper(0x1000, 0x1000) == [(0x1000, 0x1000)], "decoupage : un secteur ne se coupe pas")
    contigus = decouper(0, 0x40000)
    verifier(all(contigus[i][0] + contigus[i][1] == contigus[i + 1][0] for i in range(len(contigus) - 1)), "decoupage : sans trou")

    # empreintes
    verifier(empreinte(b"") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 du vide connu")
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "x.bin"
        p.write_bytes(b"abc" * 100000)
        verifier(empreinte_fichier(p) == empreinte(b"abc" * 100000), "empreinte d'un fichier = empreinte de son contenu")
        # plages_ecrites : arrondi au secteur
        (Path(tmp) / "bootloader").mkdir()
        (Path(tmp) / "partition_table").mkdir()
        (Path(tmp) / "bootloader" / "bootloader.bin").write_bytes(b"\0" * 5000)
        (Path(tmp) / "partition_table" / "partition-table.bin").write_bytes(b"\0" * 3072)
        (Path(tmp) / "ota_data_initial.bin").write_bytes(b"\0" * 8192)
        (Path(tmp) / "meridien.bin").write_bytes(b"\0" * 100000)
        pl = plages_ecrites(Path(tmp))
        verifier(pl[0] == ("bootloader", 0x2000, 0x2000 + 0x2000), "plages : 5000 octets -> 2 secteurs")
        verifier(pl[3][2] - pl[3][1] == 0x19000, "plages : 100000 octets -> 0x19000")
        verifier(all(a[2] <= b[1] for a, b in zip(pl, pl[1:])), "plages : les quatre regions ne se chevauchent pas")

    # journal
    journal = "\n".join(
        [
            "I (900) ws7b_display: EK79007 : DSI 2 voies",
            "I (950) ws7b_board: Tactile : Goodix GT911 (0x5d)",
            "I (960) ws7b_board: Codecs audio sur le bus I2C : ES8311 present, ES7210 present",
            "I (970) ws7b_board: Carte prête : Waveshare 7B",
            "I (2100) ws7b_wifi: lien avec le coprocesseur etabli a 2100 ms",
            "I (3400) reseau: adresse IP obtenue : x à 3400 ms",
            "I (3900) sntp_rapide: horloge réglée à +120 ms",
            "I (1) ws7b_touch: AUTOTEST appui 1/5 -> OK",
            "I (2) ws7b_touch: AUTOTEST appui 2/5 -> ECART",
        ]
    )
    a = analyser_journal(journal)
    verifier(a["manquants"] == [] and a["echecs"] == [], "journal : les sept jalons sont reconnus, aucun echec")
    verifier(analyser_journal("Guru Meditation Error")["echecs"] != [], "journal : une panique est un echec")
    verifier("carte_prete" in analyser_journal("rien")["manquants"], "journal : jalon absent signale")
    t = moyenne_decalage(journal)
    verifier((t["appuis"], t["ok"]) == (2, 1), "journal : appuis de l'autotest tactile comptes (2 dont 1 OK)")

    # garde-fou MAC : variable absente ou mal formee -> refus avant tout appel a la carte
    ancien = os.environ.pop("MERIDIEN_MAC_7B", None)
    try:
        verifier_carte("COM0")
        verifier(False, "garde-fou : MAC absente doit refuser")
    except Refus:
        verifier(True, "garde-fou : sans MERIDIEN_MAC_7B on refuse avant de parler a la carte")
    finally:
        if ancien is not None:
            os.environ["MERIDIEN_MAC_7B"] = ancien

    # garde-fou sauvegarde : ni marque ni empreinte juste -> refus d'ecrire
    with tempfile.TemporaryDirectory() as tmp:
        try:
            charger_marque(Path(tmp))
            verifier(False, "garde-fou : sans sauvegarde_ok.json on refuse")
        except Refus:
            verifier(True, "garde-fou : sans sauvegarde_ok.json on refuse d'ecrire")
        (Path(tmp) / "flash.bin").write_bytes(b"\xff" * 16)
        (Path(tmp) / "sauvegarde_ok.json").write_text(json.dumps({"fichier": "flash.bin", "sha256": "0" * 64}), encoding="utf-8")
        try:
            charger_marque(Path(tmp))
            verifier(False, "garde-fou : une empreinte fausse doit refuser")
        except Refus:
            verifier(True, "garde-fou : une sauvegarde alteree sur le disque refuse d'ecrire")
        (Path(tmp) / "sauvegarde_ok.json").write_text(
            json.dumps({"fichier": "flash.bin", "sha256": empreinte(b"\xff" * 16)}), encoding="utf-8"
        )
        verifier(charger_marque(Path(tmp))["sha256"] == empreinte(b"\xff" * 16), "garde-fou : sauvegarde intacte acceptee")

    # carte simulee : l'essai complet et surtout la restauration
    ancien_mac = os.environ.get("MERIDIEN_MAC_7B")
    try:
        _simulation(verifier)
    finally:
        if ancien_mac is None:
            os.environ.pop("MERIDIEN_MAC_7B", None)
        else:
            os.environ["MERIDIEN_MAC_7B"] = ancien_mac

    print("AUTOTEST " + ("OK" if ok else "ECHEC"))
    return 0 if ok else 1


# --- Ligne de commande --------------------------------------------------------------------------------------------------------
def heure_limite(texte: str) -> dt.datetime:
    h, m = (int(x) for x in texte.split(":"))
    maintenant = dt.datetime.now()
    limite = maintenant.replace(hour=h, minute=m, second=0, microsecond=0)
    return limite if limite > maintenant else limite + dt.timedelta(days=1)


def principal(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sous = p.add_subparsers(dest="commande", required=True)
    sous.add_parser("autotest", help="verifie les fonctions pures, sans carte")
    s = sous.add_parser("identifier", help="lit la MAC du port donne et la compare a MERIDIEN_MAC_7B")
    s.add_argument("--port", required=True)
    for nom in ("sauvegarde", "flasher", "restaurer", "essai"):
        s = sous.add_parser(nom)
        s.add_argument("--port", required=True)
        s.add_argument("--dossier", required=True, type=Path, help="dossier de la sauvegarde de la flash")
        if nom in ("flasher", "essai"):
            s.add_argument("--build", type=Path, default=Path("build"))
        if nom == "essai":
            s.add_argument("--journaux", type=Path, default=None, help="dossier des journaux (defaut : <dossier>/essai)")
            s.add_argument("--limite", default="06:30", help="heure limite HH:MM avant de restaurer (defaut 06:30)")
            s.add_argument("--resets", type=int, default=10)
    s = sous.add_parser("table", help="affiche la table de partitions et l'otadata d'un fichier de sauvegarde")
    s.add_argument("fichier", type=Path)
    a = p.parse_args(argv)

    if a.commande == "autotest":
        return autotest()
    if a.commande == "table":
        donnees = a.fichier.read_bytes()
        afficher_table(
            decoder_table(donnees[DECALAGE_TABLE : DECALAGE_TABLE + TAILLE_TABLE]),
            decoder_otadata(donnees[DECALAGE_OTADATA : DECALAGE_OTADATA + TAILLE_OTADATA]),
        )
        return 0
    try:
        verifier_carte(a.port)
        if a.commande == "identifier":
            return 0
        if a.commande == "sauvegarde":
            etape_sauvegarde(a.port, a.dossier)
            return 0
        if a.commande == "flasher":
            etape_flasher(a.port, a.build, a.dossier)
            return 0
        if a.commande == "restaurer":
            return 0 if etape_restaurer(a.port, a.dossier) else 3
        if a.commande == "essai":
            journaux = a.journaux or (a.dossier / "essai")
            return etape_essai(a.port, a.build, a.dossier, journaux, heure_limite(a.limite), a.resets)
    except Refus as e:
        print(f"[REFUS] {e}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(principal())
