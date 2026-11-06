"""Dossier de l'installation ESP-IDF utilisée en lecture seule par les outils de test (compilateur croisé RISC-V).

Ce dossier est propre à chaque PC : il n'est jamais écrit dans le dépôt. Ordre de recherche (le même que
tools/idf.ps1) : la variable d'environnement MERIDIEN_IDF_RACINE, puis tools/local.ps1 (ignoré par git, modèle
tools/local.example.ps1) de ce dossier de travail, puis celui du dossier principal du dépôt (utile dans un worktree git).
"""
import os
import re
import subprocess
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]


def _lire_local(chemin):
    try:
        texte = chemin.read_text(encoding="utf-8")
    except OSError:
        return None
    trouve = re.search(r"^\s*\$IdfRacine\s*=\s*'([^']+)'", texte, re.MULTILINE)
    if trouve and not trouve.group(1).startswith("<"):
        return Path(trouve.group(1))
    return None


def racine_idf():
    """Retourne le dossier de l'installation ESP-IDF (Path), ou None si rien n'est configuré sur ce PC."""
    if os.environ.get("MERIDIEN_IDF_RACINE"):
        return Path(os.environ["MERIDIEN_IDF_RACINE"])
    candidats = [RACINE / "tools" / "local.ps1"]
    try:
        commun = subprocess.run(
            ["git", "-C", str(RACINE), "rev-parse", "--path-format=absolute", "--git-common-dir"],
            capture_output=True, text=True, timeout=30,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        commun = ""
    if commun:
        candidats.append(Path(commun).parent / "tools" / "local.ps1")
    for chemin in candidats:
        racine = _lire_local(chemin)
        if racine:
            return racine
    return None


def outil_croise(nom):
    """Chemin d'un outil du compilateur croisé RISC-V d'ESP-IDF (ex. « g++ », « objcopy »), ou None."""
    racine = racine_idf()
    if not racine:
        return None
    trouves = sorted(racine.glob(f"tools/riscv32-esp-elf/*/riscv32-esp-elf/bin/riscv32-esp-elf-{nom}.exe"))
    return str(trouves[-1]) if trouves else None
