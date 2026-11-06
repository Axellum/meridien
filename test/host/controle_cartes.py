"""Contrôles croisés des cartes (boards/<carte>/ et components/bsp_<carte>/), sans compilation ni carte. Lus par lancer.py.

Ce que ces contrôles attrapent, qu'aucun test C++ ne voit :
  - une carte sans son dossier de réglages, sans sa couche carte, ou dont le sdkconfig pointe une autre table de partitions ;
  - une table de partitions qui déborde de la flash, dont deux partitions se chevauchent, dont les applications ne sont pas alignées
    sur 64 Ko, ou dont l'otadata n'a pas la taille imposée par ESP-IDF (0x2000) ;
  - des broches SDIO / de remise à zéro du C6 qui diffèrent entre boards/waveshare-7b/sdkconfig.defaults.board et
    components/bsp_waveshare_7b/include/ws7b_pur.h (deux copies d'un même fait : elles doivent rester d'accord) ;
  - une adresse MAC, une adresse IP du réseau privé ou un chemin du PC d'Axel écrits dans les fichiers de la 7B (règle du dépôt :
    aucun identifiant de matériel ni chemin local dans git).
Non couvert : que les valeurs soient celles de la carte (voir docs/carte-7b.md et docs/mesures/7b-*.md).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]

# Tailles de flash lues dans le sdkconfig de la carte : CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
_FLASH = re.compile(r"^CONFIG_ESPTOOLPY_FLASHSIZE_(\d+)MB=y", re.M)
_MAC = re.compile(r"\b(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b")
_IP_PRIVEE = re.compile(r"\b(?:192\.168|10\.\d{1,3})\.\d{1,3}\.\d{1,3}\b")
_CHEMIN_PC = re.compile(r"\b[A-Za-z]:\\(?:Users|AuxFilsDesIdees|Windows)\b|\bE:\\")


def _entier(texte: str) -> int:
    texte = texte.strip()
    if texte.lower().endswith("k"):
        return int(texte[:-1], 0) * 1024
    if texte.lower().endswith("m"):
        return int(texte[:-1], 0) * 1024 * 1024
    return int(texte, 0)


def lire_partitions(chemin: Path) -> list[dict]:
    """Lit un partitions.csv d'ESP-IDF (champs : nom, type, sous-type, décalage, taille, drapeaux)."""
    lignes = []
    for brut in chemin.read_text(encoding="utf-8").splitlines():
        ligne = brut.split("#", 1)[0].strip()
        if not ligne:
            continue
        champs = [c.strip() for c in ligne.split(",")]
        if len(champs) < 5:
            raise ValueError(f"{chemin.name} : ligne illisible : {brut!r}")
        lignes.append({"nom": champs[0], "type": champs[1], "sous_type": champs[2],
                       "decalage": _entier(champs[3]), "taille": _entier(champs[4])})
    return lignes


def _controle_partitions(carte: str, defauts: str, table: Path, erreurs: list[str]) -> None:
    m = _FLASH.search(defauts)
    if not m:
        erreurs.append(f"{carte} : taille de flash absente de sdkconfig.defaults.board (CONFIG_ESPTOOLPY_FLASHSIZE_<n>MB=y)")
        return
    flash = int(m.group(1)) * 1024 * 1024
    for cle in ("CONFIG_PARTITION_TABLE_CUSTOM_FILENAME", "CONFIG_PARTITION_TABLE_FILENAME"):
        attendu = f'{cle}="boards/{carte}/partitions.csv"'
        if attendu not in defauts:
            erreurs.append(f"{carte} : le sdkconfig doit contenir {attendu}")
    try:
        parts = lire_partitions(table)
    except ValueError as e:
        erreurs.append(str(e))
        return
    if not parts:
        erreurs.append(f"{carte} : table de partitions vide")
        return
    precedent_fin = 0x9000  # 0x8000 = la table elle-même (0x1000 octets), les données commencent à 0x9000
    noms = set()
    for p in parts:
        if p["nom"] in noms:
            erreurs.append(f"{carte} : partition « {p['nom']} » déclarée deux fois")
        noms.add(p["nom"])
        if p["decalage"] < precedent_fin:
            erreurs.append(f"{carte} : « {p['nom']} » (0x{p['decalage']:x}) chevauche la précédente ou la table (fin 0x{precedent_fin:x})")
        fin = p["decalage"] + p["taille"]
        if fin > flash:
            erreurs.append(f"{carte} : « {p['nom']} » finit à 0x{fin:x}, au-delà de la flash ({flash // (1024 * 1024)} Mo)")
        alignement = 0x10000 if p["type"] == "app" else 0x1000
        if p["decalage"] % alignement:
            erreurs.append(f"{carte} : « {p['nom']} » : décalage 0x{p['decalage']:x} non aligné sur 0x{alignement:x}")
        if p["type"] == "data" and p["sous_type"] == "ota" and p["taille"] != 0x2000:
            erreurs.append(f"{carte} : otadata doit faire 0x2000 octets (ESP-IDF), pas 0x{p['taille']:x}")
        precedent_fin = max(precedent_fin, fin)
    apps = {p["nom"]: p for p in parts if p["type"] == "app"}
    if not ({"ota_0", "ota_1"} <= apps.keys()):
        erreurs.append(f"{carte} : deux emplacements d'application ota_0 et ota_1 sont attendus (retour arrière de l'OTA)")
    elif apps["ota_0"]["taille"] != apps["ota_1"]["taille"]:
        erreurs.append(f"{carte} : ota_0 et ota_1 doivent avoir la même taille")
    if not any(p["type"] == "data" and p["sous_type"] == "nvs" for p in parts):
        erreurs.append(f"{carte} : aucune partition nvs (les réglages et les clés y vivent)")
    if not any(p["type"] == "data" and p["sous_type"] == "ota" for p in parts):
        erreurs.append(f"{carte} : aucune partition otadata")


def _controle_7b_coherence(defauts: str, erreurs: list[str]) -> None:
    """Les broches du C6 sont écrites deux fois (ws7b_pur.h pour le C, sdkconfig pour esp_hosted) : elles doivent concorder."""
    pur = (RACINE / "components" / "bsp_waveshare_7b" / "include" / "ws7b_pur.h").read_text(encoding="utf-8")

    def broche(nom: str) -> int:
        m = re.search(rf"^#define {nom}\s+(\d+)", pur, re.M)
        if not m:
            raise ValueError(f"{nom} introuvable dans ws7b_pur.h")
        return int(m.group(1))

    attendu = {
        "CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CLK_SLOT_1": broche("WS7B_PIN_C6_SDIO_CLK"),
        "CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CMD_SLOT_1": broche("WS7B_PIN_C6_SDIO_CMD"),
        "CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D0_SLOT_1": broche("WS7B_PIN_C6_SDIO_D0"),
        "CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D1_4BIT_BUS_SLOT_1": broche("WS7B_PIN_C6_SDIO_D1"),
        "CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D2_4BIT_BUS_SLOT_1": broche("WS7B_PIN_C6_SDIO_D2"),
        "CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D3_4BIT_BUS_SLOT_1": broche("WS7B_PIN_C6_SDIO_D3"),
        "CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE": broche("WS7B_PIN_C6_RESET"),
    }
    for cle, valeur in attendu.items():
        m = re.search(rf"^{cle}=(\d+)", defauts, re.M)
        if not m:
            erreurs.append(f"waveshare-7b : {cle} absent de sdkconfig.defaults.board")
        elif int(m.group(1)) != valeur:
            erreurs.append(f"waveshare-7b : {cle}={m.group(1)} mais ws7b_pur.h dit {valeur}")
    for indispensable in ("CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y", "CONFIG_ESP_HOSTED_SDIO_SLOT_1=y", "CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y"):
        if indispensable not in defauts:
            erreurs.append(f"waveshare-7b : {indispensable} manque")
    # Jamais de mise à jour du C6 (décision d'Axel du 16/10/2026) : aucune option de mise à jour ne doit être activée.
    if re.search(r"^CONFIG_ESP_HOSTED_(?:OTA|SLAVEOTA|.*FW_UPDATE)\w*=y", defauts, re.M):
        erreurs.append("waveshare-7b : une option de mise à jour du C6 est activée (interdit)")


def _controle_hygiene_7b(erreurs: list[str]) -> None:
    """Pas d'identifiant de matériel (MAC), d'adresse du réseau privé ni de chemin du PC dans les fichiers de la 7B."""
    cibles: list[Path] = []
    for dossier in (RACINE / "components" / "bsp_waveshare_7b", RACINE / "boards" / "waveshare-7b"):
        cibles += [p for p in dossier.rglob("*") if p.is_file() and p.suffix in {".c", ".h", ".txt", ".yml", ".csv", ".board", ".md"}]
    docs = RACINE / "docs"
    cibles += [p for p in (docs / "carte-7b.md", *docs.glob("mesures/7b-*.md")) if p.is_file()]
    for fichier in cibles:
        texte = fichier.read_text(encoding="utf-8", errors="replace")
        joli = fichier.relative_to(RACINE).as_posix()
        for nom, motif in (("adresse MAC", _MAC), ("adresse IP privée", _IP_PRIVEE), ("chemin du PC", _CHEMIN_PC)):
            m = motif.search(texte)
            if m:
                ligne = texte.count("\n", 0, m.start()) + 1
                erreurs.append(f"{joli}:{ligne} : {nom} écrite dans le dépôt (« {m.group(0)} »)")


def controle_cartes() -> int:
    """Rend le nombre d'erreurs (0 = tout est cohérent). Affiche un résumé."""
    erreurs: list[str] = []
    cartes = sorted(p.name for p in (RACINE / "boards").iterdir() if p.is_dir())
    for carte in cartes:
        dossier = RACINE / "boards" / carte
        defauts_f = dossier / "sdkconfig.defaults.board"
        table = dossier / "partitions.csv"
        bsp = RACINE / "components" / f"bsp_{carte.replace('-', '_')}"
        if not defauts_f.is_file():
            erreurs.append(f"{carte} : boards/{carte}/sdkconfig.defaults.board manque")
            continue
        if not table.is_file():
            erreurs.append(f"{carte} : boards/{carte}/partitions.csv manque")
            continue
        if not (bsp / "CMakeLists.txt").is_file():
            erreurs.append(f"{carte} : {bsp.relative_to(RACINE).as_posix()}/CMakeLists.txt manque (couche carte absente)")
        defauts = defauts_f.read_text(encoding="utf-8")
        _controle_partitions(carte, defauts, table, erreurs)
        if carte == "waveshare-7b":
            try:
                _controle_7b_coherence(defauts, erreurs)
            except (ValueError, OSError) as e:
                erreurs.append(f"waveshare-7b : {e}")
            _controle_hygiene_7b(erreurs)
    print("\ncontrôles des cartes (" + ", ".join(cartes) + ") :", "OK" if not erreurs else f"{len(erreurs)} erreur(s)")
    for e in erreurs:
        print("  ERREUR", e)
    return len(erreurs)


if __name__ == "__main__":
    sys.exit(1 if controle_cartes() else 0)
