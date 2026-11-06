#!/usr/bin/env python3
"""Embarque la page de réglages (web/app/) dans le firmware : écrit un fichier C++ qui contient chaque fichier compressé en
gzip (niveau 9, date fixée à zéro pour que deux compilations donnent les mêmes octets) et la table chemin -> données.

    python tools/embarquer_web.py web/app build/esp-idf/main/web_fichiers.cpp

Lancé par main/CMakeLists.txt à chaque modification d'un fichier de web/app. Bibliothèque standard seulement.
Le serveur (main/serveur_web.cpp) répond toujours en gzip (« Content-Encoding: gzip ») : tous les navigateurs l'acceptent, et
l'appareil n'a pas à décompresser. L'étiquette de cache (ETag) est l'empreinte SHA-256 des octets compressés.
"""

import gzip
import hashlib
import io
import sys
from pathlib import Path

TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".txt": "text/plain; charset=utf-8",
}


def compresser(octets: bytes) -> bytes:
    sortie = io.BytesIO()
    with gzip.GzipFile(fileobj=sortie, mode="wb", compresslevel=9, mtime=0) as g:
        g.write(octets)
    return sortie.getvalue()


def ecrire(dossier: Path, sortie: Path) -> None:
    fichiers = sorted(p for p in dossier.rglob("*") if p.is_file())
    if not fichiers:
        raise SystemExit(f"aucun fichier dans {dossier}")
    lignes = [
        "// GÉNÉRÉ par tools/embarquer_web.py à partir de web/app/ : ne pas modifier, ne pas versionner.",
        '#include "web_fichiers.h"',
        "",
        "namespace ecran {",
        "namespace {",
    ]
    entrees = []
    total_gz = 0
    total_brut = 0
    for i, chemin in enumerate(fichiers):
        brut = chemin.read_bytes()
        gz = compresser(brut)
        url = "/" + chemin.relative_to(dossier).as_posix()
        type_mime = TYPES.get(chemin.suffix.lower())
        if type_mime is None:
            raise SystemExit(f"type inconnu pour {url} : l'ajouter à TYPES dans tools/embarquer_web.py")
        etag = hashlib.sha256(gz).hexdigest()[:16]
        total_gz += len(gz)
        total_brut += len(brut)
        octets = ",".join(str(b) for b in gz)
        lignes.append(f"const unsigned char kDonnees{i}[] = {{{octets}}}; // {url}, {len(brut)} octets bruts")
        entrees.append(f'    {{"{url}", "{type_mime}", kDonnees{i}, sizeof(kDonnees{i}), {len(brut)}, "\\"{etag}\\""}},')
    lignes += [
        "} // namespace",
        "",
        "const FichierWeb kFichiersWeb[] = {",
        *entrees,
        "};",
        "const unsigned kNombreFichiersWeb = sizeof(kFichiersWeb) / sizeof(kFichiersWeb[0]);",
        f"const unsigned kPoidsGzipWeb = {total_gz};",
        f"const unsigned kPoidsBrutWeb = {total_brut};",
        "} // namespace ecran",
        "",
    ]
    texte = "\n".join(lignes)
    # N'écrit que si le contenu change : évite de recompiler pour rien.
    if sortie.exists() and sortie.read_text(encoding="utf-8") == texte:
        return
    sortie.parent.mkdir(parents=True, exist_ok=True)
    sortie.write_text(texte, encoding="utf-8", newline="\n")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    ecrire(Path(sys.argv[1]).resolve(), Path(sys.argv[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
