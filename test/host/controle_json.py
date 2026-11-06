#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des requêtes que construit le noyau de l'assistant (requetes_openai.hpp).

    python test/host/controle_json.py              # compile, extrait les octets produits, les relit avec Python
    python test/host/controle_json.py --cpp F.cpp  # garde en plus le programme C++ généré

Pourquoi : les tests C++ de test/host/ comparent le corps produit à du texte écrit par la même main que le code ;
ici on fait relire les octets RÉELLEMENT produits par des analyseurs qui n'ont rien à voir avec le noyau :
  - le corps de chat et de synthèse vocale passe par json.loads (strict, UTF-8 strict, clés dupliquées refusées,
    NaN refusé) et le contenu relu est comparé à ce qu'on avait mis (les « attendus » viennent de Python, pas du noyau) ;
  - le multipart (STT) passe par le module `email` de Python, qui doit retrouver chaque champ, le nom du fichier,
    son type et l'audio octet pour octet ;
  - l'adresse passe par urllib.parse en plus d'une table d'attendus écrite à la main.

Méthode : ce script génère un .cpp qui construit chaque cas À LA COMPILATION (constexpr) dans une section nommée,
le compile avec le g++ croisé RISC-V d'ESP-IDF (`-c`, sans projet ESP-IDF), extrait la section avec `objcopy` et lit
les octets. Aucun programme n'est exécuté (pas de compilateur PC) : ce sont les octets que l'évaluation
constexpr du compilateur a fabriqués. Lecture seule sur H:\\idf. Sortie : code 0 si tout est conforme.
"""
import argparse
import email
import email.policy
import glob
import json
import re
import struct
import subprocess
import sys
import tempfile
import urllib.parse
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
INCLUDE_NOYAU = RACINE / "components" / "core" / "include"
TAILLE_DONNEES = 8192
TAILLE_SLOT = 12 + TAILLE_DONNEES  # idx, n, ok (uint32 petit-boutiste) + données


def outil(nom):
    trouves = sorted(glob.glob(r"H:\idf\tools\riscv32-esp-elf\*\riscv32-esp-elf\bin\riscv32-esp-elf-" + nom + ".exe"))
    if not trouves:
        sys.exit("outil croisé introuvable : riscv32-esp-elf-" + nom + " (H:\\idf\\tools\\riscv32-esp-elf)")
    return trouves[-1]


def lit(b):
    """Littéral de chaîne C++ pour des octets quelconques (octal sur 3 chiffres : jamais ambigu)."""
    sortie = []
    for x in b:
        if 32 <= x < 127 and chr(x) not in '"\\?':
            sortie.append(chr(x))
        else:
            sortie.append("\\%03o" % x)
    return '"' + "".join(sortie) + '"'


# ---------------------------------------------------------------------------------------------------------------
# Les cas. Les « attendus » sont écrits ici, indépendamment du noyau.
# ---------------------------------------------------------------------------------------------------------------
S = lambda t: t.encode("utf-8")  # noqa: E731

HOSTILE = (b'Il a dit "bonjour" \\ C:\\dossier\\fichier\nligne 2\r\tTab \x01\x1f\x7f\x00 </script> '
           + S("é à ç € œ ") + b"\xe2\x80\xa8" + b" " + b"\xf0\x9f\x98\x80" + S(" fin"))

# (octets envoyés, texte que json.loads doit rendre). Le noyau remplace CHAQUE octet invalide par U+FFFD.
INVALIDES = [
    (b"a\xffb", "a\ufffdb"),
    (b"x\xc0\x80y", "x\ufffd\ufffdy"),
    (b"\x80", "\ufffd"),
    (b"\xed\xa0\x80", "\ufffd\ufffd\ufffd"),  # surrogate U+D800
    (b"fin\xe2\x82", "fin\ufffd\ufffd"),  # caractère coupé en fin de texte
    (b"\xf4\x90\x80\x80", "\ufffd\ufffd\ufffd\ufffd"),  # > U+10FFFF
    (b"\xf5", "\ufffd"),
    (b"\xe0\x80\x80", "\ufffd\ufffd\ufffd"),  # sur-longue
    (b"\xf0\x9f\x98", "\ufffd\ufffd\ufffd"),  # émoji coupé
]

SYSTEME = S("Tu es l'assistant de l'écran. Réponds en français, en une ou deux phrases courtes.")


def fenetre_attendue(roles_contenus, budget):
    """Fenêtre d'historique attendue, réécrite ici d'après la SPÉCIFICATION (octets du contenu, dernier message
    toujours gardé, la fenêtre commence par un message d'utilisateur)."""
    n = len(roles_contenus)
    gardes = []
    total = 0
    for i in range(n - 1, -1, -1):
        taille = len(roles_contenus[i][1])
        if i < n - 1 and total + taille > budget:
            break
        total += taille
        gardes.insert(0, i)
    while len(gardes) > 1 and roles_contenus[gardes[0]][0] != "user":
        gardes.pop(0)
    return gardes


def historique_type():
    return [
        ("user", S("Quelle heure est-il ?")),
        ("assistant", S("Il est quatorze heures trente.")),
        ("user", S("Et demain, il fera beau ?")),
        ("assistant", S("Demain : soleil, 21 °C.")),
        ("user", S("Merci, et après-demain ?")),
    ]


CAS = []  # chaque cas : dict(type=..., ...)


def chat(nom, modele, systeme, msgs, budget=4096, cap=TAILLE_DONNEES, ok=True, attendus=None):
    CAS.append(dict(type="chat", nom=nom, modele=modele, systeme=systeme, msgs=msgs, budget=budget, cap=cap, ok=ok,
                    attendus=attendus))


def tts(nom, modele, voix, texte, fmt="pcm", cap=TAILLE_DONNEES, ok=True, attendu_texte=None):
    CAS.append(dict(type="tts", nom=nom, modele=modele, voix=voix, texte=texte, fmt=fmt, cap=cap, ok=ok,
                    attendu_texte=attendu_texte))


def url(nom, base, route, attendu):
    CAS.append(dict(type="url", nom=nom, base=base, route=route, attendu=attendu))


def frontiere(nom, graine):
    CAS.append(dict(type="frontiere", nom=nom, graine=graine))


def stt(nom, front, modele=b"", langue=b"", fichier=b"audio.wav", mime=b"audio/wav", ok_debut=True, ok_ct=True,
        audio=None):
    CAS.append(dict(type="stt", nom=nom, front=front, modele=modele, langue=langue, fichier=fichier, mime=mime,
                    ok_debut=ok_debut, ok_ct=ok_ct, audio=audio))


# --- chat ---
chat("chat : modèle + système + une question", S("gpt-4o-mini"), SYSTEME, [("user", S("Quelle heure est-il ?"))])
chat("chat : sans modèle ni système", b"", b"", [("user", S("Bonjour"))])
chat("chat : contenu hostile (guillemets, \\, contrôles, NUL, U+2028, émoji)", S("m"), SYSTEME, [("user", HOSTILE)])
chat("chat : système hostile", S("m"), HOSTILE, [("user", S("ok"))])
chat("chat : modèle hostile", HOSTILE, b"", [("user", S("ok"))])
for i, (brut, voulu) in enumerate(INVALIDES):
    chat("chat : UTF-8 invalide #%d" % i, S("m"), b"", [("user", brut)], attendus=[("user", voulu)])
for budget in (0, 10, 30, 60, 100, 4096):
    chat("chat : fenêtre, budget %d" % budget, S("m"), SYSTEME, historique_type(), budget=budget)
chat("chat : la dernière question dépasse seule le budget", S("m"), b"", [("assistant", S("réponse")), ("user", b"x" * 500)],
     budget=50)
chat("chat : historique qui commence par une réponse orpheline", S("m"), b"",
     [("assistant", S("orpheline")), ("assistant", S("encore")), ("user", S("question"))], budget=4096)
chat("chat : long contenu (3000 octets)", S("m"), SYSTEME, [("user", ("é" * 1500).encode("utf-8"))], budget=100000)
chat("chat : débordement du tampon (60 octets)", S("m"), SYSTEME, [("user", S("Bonjour"))], cap=60, ok=False)
chat("chat : aucun message", S("m"), SYSTEME, [], ok=False)

# --- synthèse vocale ---
tts("tts : complet", S("tts-1"), S("alloy"), S("Bonjour, il est quatorze heures."), "wav")
tts("tts : sans modèle ni voix", b"", b"", S("Bonjour"), "pcm")
for f in ("pcm", "wav", "mp3", "opus"):
    tts("tts : format " + f, S("m"), S("v"), S("texte"), f)
tts("tts : texte hostile", S("m"), S("v"), HOSTILE, "mp3")
for i, (brut, voulu) in enumerate(INVALIDES[:5]):
    tts("tts : UTF-8 invalide #%d" % i, b"", b"", brut, "pcm", attendu_texte=voulu)
tts("tts : texte vide refusé", S("m"), S("v"), b"", "pcm", ok=False)
tts("tts : débordement du tampon (30 octets)", S("m"), S("v"), S("Bonjour tout le monde"), "pcm", cap=30, ok=False)

# --- adresse ---
url("url : base avec /v1", "http://192.168.1.10:1234/v1", "chat/completions", "http://192.168.1.10:1234/v1/chat/completions")
url("url : slashs de jonction en trop", "http://h:1234/v1/", "/chat/completions", "http://h:1234/v1/chat/completions")
url("url : /v1 manquant, non corrigé", "http://h:1234", "chat/completions", "http://h:1234/chat/completions")
url("url : /v1 doublé", "http://h:1234/v1", "v1/chat/completions", "http://h:1234/v1/chat/completions")
url("url : OpenAI", "https://api.openai.com/v1", "audio/speech", "https://api.openai.com/v1/audio/speech")
url("url : Gemini (v1beta/openai)", "https://generativelanguage.googleapis.com/v1beta/openai/", "chat/completions",
    "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions")
url("url : transcription", "http://192.168.1.10:8000/v1", "audio/transcriptions",
    "http://192.168.1.10:8000/v1/audio/transcriptions")
url("url : schéma ftp refusé", "ftp://h/v1", "models", None)
url("url : espace refusé", "http://h/v1 x", "models", None)
url("url : paramètres refusés", "http://h/v1?api=1", "models", None)
url("url : fragment refusé", "http://h/v1#x", "models", None)
url("url : pas de schéma", "h:1234/v1", "models", None)

# --- multipart (STT) ---
for g in (0, 0x0123456789ABCDEF, 0xFFFFFFFFFFFFFFFF, 0xDEADBEEF):
    frontiere("frontière graine %#x" % g, g)
F1 = "ecran-0123456789abcdef"
AUDIO = bytes(range(256)) * 3 + b"\r\n--ecran-0123456789abcde" + b"\r\n\r\n" + bytes(range(255, -1, -1))
stt("stt : complet", F1, b"whisper-1", b"fr", audio=AUDIO)
stt("stt : sans modèle ni langue", F1, audio=AUDIO)
stt("stt : nom de fichier et type choisis", F1, b"m", b"fr", b"enregistrement_01.mp3", b"audio/mpeg", audio=AUDIO)
stt("stt : audio vide", F1, b"m", b"fr", audio=b"")
stt("stt : frontière avec espace refusée", "ecran 1", b"m", ok_debut=False, ok_ct=False)
stt("stt : frontière vide refusée", "", b"m", ok_debut=False, ok_ct=False)
stt("stt : frontière de 71 caractères refusée", "a" * 71, b"m", ok_debut=False, ok_ct=False)
stt("stt : guillemet dans le modèle refusé", F1, b'mo"del', ok_debut=False)
stt("stt : saut de ligne dans la langue refusé", F1, b"m", b"fr\r\nX: y", ok_debut=False)
stt("stt : nom de fichier avec « / » refusé", F1, b"m", b"", b"../x.wav", ok_debut=False)
stt("stt : valeur qui imite la frontière refusée", F1, b"avant" + F1.encode() + b"apres", ok_debut=False)
stt("stt : type MIME avec espace refusé", F1, b"m", b"", b"a.wav", b"audio/wav; x", ok_debut=False)


# ---------------------------------------------------------------------------------------------------------------
# Génération du programme C++
# ---------------------------------------------------------------------------------------------------------------
def generer_cpp():
    L = []
    L.append("// GÉNÉRÉ par test/host/controle_json.py : ne pas éditer.")
    L.append('#include "ecran/requetes_openai.hpp"')
    L.append("using namespace ecran::assistant;")
    L.append("namespace {")
    L.append("struct Sortie { std::uint32_t idx; std::uint32_t n; std::uint32_t ok; char d[%d]; };" % TAILLE_DONNEES)
    L.append("template <std::size_t N> constexpr std::string_view V(const char (&a)[N]) { return std::string_view(a, N - 1); }")
    L.append("template <typename F> constexpr Sortie faire(std::uint32_t idx, std::size_t cap, F&& f) {")
    L.append("    Sortie s{}; s.idx = idx; Tampon t(s.d, cap); s.ok = f(t) ? 1U : 0U; s.n = static_cast<std::uint32_t>(t.taille()); return s; }")
    compteur = {"donnees": 0, "slot": 0}

    def donnee(b):
        compteur["donnees"] += 1
        nom = "D%d" % compteur["donnees"]
        L.append("constexpr char %s[] = %s;" % (nom, lit(b)))
        return "V(%s)" % nom

    def slot(cap, appel):
        """Un emplacement de la section .sorties : `appel` est l'expression C++ qui écrit dans `t`."""
        i = compteur["slot"]
        compteur["slot"] += 1
        L.append('__attribute__((used, section(".sorties"))) constexpr Sortie S%d = faire(%d, %d, [](Tampon& t) { return %s; });'
                 % (i, i, cap, appel))

    for c in CAS:
        t = c["type"]
        if t == "chat":
            vues = ["Message{Role::%s, %s}" % ({"user": "Utilisateur", "assistant": "Assistant"}[r], donnee(m)) for r, m in c["msgs"]]
            hist = "nullptr"
            if vues:
                L.append("constexpr Message M%d[] = {%s};" % (compteur["slot"], ", ".join(vues)))
                hist = "M%d" % compteur["slot"]
            opts = "OptionsChat{%s, %s, %s, %d, %d}" % (donnee(c["modele"]), donnee(c["systeme"]), hist, len(vues), c["budget"])
            slot(c["cap"], "corps_chat(t, %s)" % opts)
        elif t == "tts":
            fmt = {"pcm": "Pcm", "wav": "Wav", "mp3": "Mp3", "opus": "Opus"}[c["fmt"]]
            opts = "OptionsTts{%s, %s, %s, FormatAudio::%s}" % (donnee(c["modele"]), donnee(c["voix"]), donnee(c["texte"]), fmt)
            slot(c["cap"], "corps_tts(t, %s)" % opts)
        elif t == "url":
            slot(512, "assembler_url(t, %s, %s)" % (donnee(S(c["base"])), donnee(S(c["route"]))))
        elif t == "frontiere":
            slot(64, "ecrire_frontiere(t, UINT64_C(%d))" % c["graine"])
        elif t == "stt":
            f = donnee(S(c["front"]))
            opts = "OptionsStt{%s, %s, %s, %s}" % (donnee(c["modele"]), donnee(c["langue"]), donnee(c["fichier"]), donnee(c["mime"]))
            slot(1024, "type_contenu_multipart(t, %s)" % f)
            slot(2048, "debut_multipart(t, %s, %s)" % (f, opts))
            slot(256, "fin_multipart(t, %s)" % f)
    L.append("}  // namespace")
    L.append("int main() { return 0; }")
    return "\n".join(L) + "\n", compteur["slot"]


# ---------------------------------------------------------------------------------------------------------------
# Compilation, extraction
# ---------------------------------------------------------------------------------------------------------------
def compiler_et_extraire(cpp_texte, nb_slots, garder):
    gpp = outil("g++")
    objcopy = outil("objcopy")
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        src = tmp / "controle.cpp"
        obj = tmp / "controle.o"
        src.write_text(cpp_texte, encoding="utf-8", newline="\n")
        if garder:
            Path(garder).write_text(cpp_texte, encoding="utf-8", newline="\n")
        r = subprocess.run([gpp, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-toplevel-reorder", "-c",
                            "-I", str(INCLUDE_NOYAU), str(src), "-o", str(obj)], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        if r.returncode != 0:
            sys.exit("ÉCHEC de compilation du programme de contrôle :\n" + r.stdout + r.stderr)
        brut = tmp / "sorties.bin"
        r = subprocess.run([objcopy, "--dump-section", ".sorties=" + str(brut), str(obj)], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        if r.returncode != 0 or not brut.exists():
            sys.exit("extraction de la section .sorties impossible :\n" + r.stdout + r.stderr)
        donnees = brut.read_bytes()
    if len(donnees) != nb_slots * TAILLE_SLOT:
        sys.exit("section .sorties de %d octets, %d attendus (%d cas)" % (len(donnees), nb_slots * TAILLE_SLOT, nb_slots))
    sorties = []
    for i in range(nb_slots):
        base = i * TAILLE_SLOT
        idx, n, ok = struct.unpack_from("<III", donnees, base)
        if idx != i:
            sys.exit("ordre des sorties inattendu : emplacement %d contient l'indice %d" % (i, idx))
        sorties.append((bool(ok), donnees[base + 12: base + 12 + n]))
    return sorties


# ---------------------------------------------------------------------------------------------------------------
# Vérification avec des analyseurs indépendants
# ---------------------------------------------------------------------------------------------------------------
class Echec(Exception):
    pass


def sans_doublon(paires):
    cles = [k for k, _ in paires]
    if len(cles) != len(set(cles)):
        raise Echec("clé dupliquée dans l'objet : %r" % cles)
    return dict(paires)


def refuser_constante(nom):
    raise Echec("constante non JSON : " + nom)


def relire_json(corps):
    try:
        texte = corps.decode("utf-8")  # strict : refuse surrogates, sur-longues, octets isolés
    except UnicodeDecodeError as e:
        raise Echec("le corps n'est pas de l'UTF-8 valide : %s" % e)
    try:
        return json.loads(texte, object_pairs_hook=sans_doublon, parse_constant=refuser_constante, strict=True)
    except json.JSONDecodeError as e:
        raise Echec("json.loads refuse le corps : %s" % e)


def verifier_chat(c, ok, corps):
    if not c["ok"]:
        if ok:
            raise Echec("devait être refusé (ok=faux), le noyau a répondu ok=vrai")
        return "refus conforme"
    if not ok:
        raise Echec("ok=faux inattendu")
    obj = relire_json(corps)
    attendu = {}
    if c["modele"]:
        attendu["model"] = c["modele"].decode("utf-8")
    attendu["stream"] = True
    messages = []
    if c["systeme"]:
        messages.append({"role": "system", "content": c["systeme"].decode("utf-8")})
    gardes = fenetre_attendue(c["msgs"], c["budget"])
    for i in gardes:
        role, contenu = c["msgs"][i]
        if c["attendus"] is not None:
            texte = c["attendus"][i][1]
        else:
            texte = contenu.decode("utf-8")
        messages.append({"role": role, "content": texte})
    attendu["messages"] = messages
    if obj != attendu:
        raise Echec("contenu relu différent de l'attendu\n  relu    : %r\n  attendu : %r" % (obj, attendu))
    if obj["stream"] is not True:
        raise Echec("« stream » n'est pas le booléen true")
    for interdit in ("max_tokens", "max_completion_tokens", "temperature"):
        if interdit in obj:
            raise Echec("champ non voulu : " + interdit)
    return "%d message(s) relus, %d octets" % (len(messages), len(corps))


def verifier_tts(c, ok, corps):
    if not c["ok"]:
        if ok:
            raise Echec("devait être refusé, le noyau a répondu ok=vrai")
        return "refus conforme"
    if not ok:
        raise Echec("ok=faux inattendu")
    obj = relire_json(corps)
    attendu = {}
    if c["modele"]:
        attendu["model"] = c["modele"].decode("utf-8")
    attendu["input"] = c["attendu_texte"] if c["attendu_texte"] is not None else c["texte"].decode("utf-8")
    if c["voix"]:
        attendu["voice"] = c["voix"].decode("utf-8")
    attendu["response_format"] = c["fmt"]
    if obj != attendu:
        raise Echec("contenu relu différent de l'attendu\n  relu    : %r\n  attendu : %r" % (obj, attendu))
    return "%d octets" % len(corps)


def verifier_url(c, ok, corps):
    if c["attendu"] is None:
        if ok:
            raise Echec("adresse invalide acceptée : %r" % corps)
        return "refus conforme"
    if not ok:
        raise Echec("adresse valide refusée")
    texte = corps.decode("ascii")
    if texte != c["attendu"]:
        raise Echec("adresse %r au lieu de %r" % (texte, c["attendu"]))
    p = urllib.parse.urlsplit(texte)
    if p.scheme not in ("http", "https") or not p.netloc or p.query or p.fragment:
        raise Echec("urllib ne reconnaît pas une adresse http(s) propre : %r" % (p,))
    if "//" in p.path or "/v1/v1" in p.path:
        raise Echec("chemin avec « // » ou « /v1/v1 » : " + p.path)
    route = c["route"].strip("/")
    if route.startswith("v1/") and c["base"].rstrip("/").endswith("/v1"):
        route = route[3:]
    if not p.path.endswith("/" + route):
        raise Echec("le chemin ne se termine pas par la route : " + p.path)
    return texte


def verifier_frontiere(c, ok, corps):
    if not ok:
        raise Echec("ok=faux inattendu")
    attendu = "ecran-%016x" % c["graine"]
    if corps.decode("ascii") != attendu:
        raise Echec("frontière %r au lieu de %r" % (corps, attendu))
    return attendu


def verifier_stt(c, sorties):
    (ok_ct, ct), (ok_debut, debut), (ok_fin, fin) = sorties
    if ok_ct != c["ok_ct"]:
        raise Echec("type de contenu : ok=%s, attendu %s" % (ok_ct, c["ok_ct"]))
    if ok_debut != c["ok_debut"]:
        raise Echec("début multipart : ok=%s, attendu %s" % (ok_debut, c["ok_debut"]))
    if not ok_ct or not ok_debut:
        return "refus conforme"
    if not ok_fin:
        raise Echec("fin multipart refusée")
    audio = c["audio"] if c["audio"] is not None else b""
    # Grammaire stricte (RFC 7578 / 2046) : le module `email` est tolérant (il accepte un champ sans la ligne vide
    # réglementaire), donc on vérifie aussi la forme exacte des octets.
    f = re.escape(c["front"].encode("ascii"))
    champ = rb'--' + f + rb'\r\nContent-Disposition: form-data; name="(?:model|language|response_format)"\r\n\r\n[^\r\n]*\r\n'
    entete_fichier = (rb'--' + f + rb'\r\nContent-Disposition: form-data; name="file"; filename="[A-Za-z0-9._-]+"\r\n'
                      rb'Content-Type: [a-z0-9/+.-]+\r\n\r\n')
    if not re.fullmatch(rb'(?:' + champ + rb')*' + entete_fichier, debut):
        raise Echec("début de multipart hors grammaire RFC 7578 : %r" % debut[:200])
    if not re.fullmatch(rb'\r\n--' + f + rb'--\r\n', fin):
        raise Echec("fin de multipart hors grammaire : %r" % fin)
    if ct != b"multipart/form-data; boundary=" + c["front"].encode("ascii"):
        raise Echec("Content-Type inattendu : %r" % ct)
    brut = b"Content-Type: " + ct + b"\r\nMIME-Version: 1.0\r\n\r\n" + debut + audio + fin
    msg = email.message_from_bytes(brut, policy=email.policy.HTTP)
    if not msg.is_multipart() or msg.get_content_type() != "multipart/form-data":
        raise Echec("le module email ne reconnaît pas un multipart/form-data : %r" % ct)
    if msg.get_boundary() != c["front"]:
        raise Echec("frontière relue %r au lieu de %r" % (msg.get_boundary(), c["front"]))
    champs = {}
    fichier = None
    for part in msg.iter_parts():
        if part.get_content_disposition() != "form-data":
            raise Echec("partie sans form-data : %r" % part.get_content_disposition())
        nom = part.get_param("name", header="content-disposition")
        if part.get_filename() is not None:
            fichier = (nom, part.get_filename(), part.get_content_type(), part.get_payload(decode=True))
        else:
            champs[nom] = bytes(part.get_payload(decode=True)).decode("utf-8")
    attendus = {"response_format": "json"}
    if c["modele"]:
        attendus["model"] = c["modele"].decode("utf-8")
    if c["langue"]:
        attendus["language"] = c["langue"].decode("utf-8")
    if champs != attendus:
        raise Echec("champs relus %r au lieu de %r" % (champs, attendus))
    if fichier is None:
        raise Echec("aucune partie « file »")
    if fichier[0] != "file" or fichier[1] != c["fichier"].decode() or fichier[2] != c["mime"].decode():
        raise Echec("partie file relue %r" % (fichier[:3],))
    if fichier[3] != audio:
        raise Echec("audio relu (%d octets) différent de l'audio envoyé (%d octets)" % (len(fichier[3]), len(audio)))
    return "%d champs + fichier de %d octets relus" % (len(champs), len(audio))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cpp", help="garde le programme C++ généré à cet endroit")
    ap.add_argument("--include", help="dossier d'include du noyau à contrôler (par défaut components/core/include) ; sert "
                                      "à prouver que le contrôle échoue sur un noyau abîmé")
    args = ap.parse_args()
    if args.include:
        global INCLUDE_NOYAU
        INCLUDE_NOYAU = Path(args.include)

    cpp, nb_slots = generer_cpp()
    sorties = compiler_et_extraire(cpp, nb_slots, args.cpp)
    echecs = 0
    verifies = 0
    pos = 0
    for c in CAS:
        n = 3 if c["type"] == "stt" else 1
        mes = sorties[pos:pos + n]
        pos += n
        try:
            if c["type"] == "chat":
                detail = verifier_chat(c, *mes[0])
            elif c["type"] == "tts":
                detail = verifier_tts(c, *mes[0])
            elif c["type"] == "url":
                detail = verifier_url(c, *mes[0])
            elif c["type"] == "frontiere":
                detail = verifier_frontiere(c, *mes[0])
            else:
                detail = verifier_stt(c, mes)
            verifies += 1
            print("  ok   %-72s %s" % (c["nom"], detail))
        except Echec as e:
            echecs += 1
            print("  ÉCHEC %-71s %s" % (c["nom"], e))
    print("\n%d cas, %d conformes, %d échec(s). Octets relus : produits par le noyau à la compilation (g++ croisé, -c), "
          "analysés par json.loads / email / urllib.parse. Rien n'a été exécuté sur carte." % (len(CAS), verifies, echecs))
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
