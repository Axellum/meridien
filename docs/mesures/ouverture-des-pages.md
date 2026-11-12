# Ouverture des pages : polices partagées, anneau du toucher, ciel — mesure au banc du PC

> **Summary (EN)** — Every page used to create its own TinyTTF fonts (home screen, Details, Sea, Settings, Alerts, boot screen): one size could exist up to three times, each with its own glyph cache filled cold. `main/ui_ressources.{h,cpp}` now hands out **one instance per (font file, pixel size)** for the whole device. On the PC bench, the home screen then Details, Sea, Settings and Alerts opened twice each use **14 font instances instead of 20** (16 instead of 20 at the 7B size) and **13 KB less glyph-bitmap memory** at 1280×720 (181 → 168 KB; 124 → 116 KB at 1024×600), the memory the device puts in PSRAM. Opening time is **not measurably different on the PC**: building a page costs 0.05–0.2 ms against 1.5–15 ms for its first full render, and cold rasterisation is a small part of it. The touch ring already repaints only its old and new box (checked by the bench, no code change). Refilling the sky in place is not worth it. 1524 of 1524 renders identical pixel for pixel; a deliberately wrong registry (kerning off) breaks 333 of 341. **Not measured on a board.**

## Ce qui change

- **Registre de polices** (`main/ui_ressources.h`, `.cpp`, LVGL pur) : `ressources::police(ttf, taille, px)` rend la police déjà créée pour ce fichier et cette taille, ou la crée (`lv_tiny_ttf_create_data_ex`, crénage normal, cache de 128 glyphes : exactement ce que faisait `lv_tiny_ttf_create_data`). Jamais détruite : les pages se rouvrent et TinyTTF se libère mal. Branché dans l'accueil (`ui_principal.cpp`), Détails, Mer, Alertes, Réglages et installation (`ui_commun.cpp`) et l'écran de démarrage (`ui_bonjour.cpp`). La roue (`ui_roue.cpp`) garde ses trois polices : hors du périmètre de ce lot.
- Les pages reprennent leurs polices au registre **à chaque ouverture** (une recherche dans une table de ≤ 32 entrées) : une page rouverte à une autre taille d'écran a la bonne taille (avant, Alertes et Réglages gardaient celle de la première ouverture).
- **Scène de banc `ouvertures`** (`tools/rendu/scenes/scene_ouvertures.cpp`) : l'accueil, puis chaque page ouverte deux fois comme sur l'appareil ; avec `BANC_CHRONO=1`, construction, premier rendu complet, mémoire des images de glyphes et glyphes servis par police. Variante `anneau` : vérifie ce que l'anneau du toucher fait repeindre.

## Résultats au banc (constatés)

Banc du PC, `tools/rendu/banc.cpp` passé **temporairement** à `kLignesParTampon = 28`, langue `fr`, `main` `ddc2f42` (avec la seule scène de mesure ajoutée) contre cette branche. Médiane de 11 exécutions alternées, µs du PC. « Glyphes » = octets des images A8 de TinyTTF après l'ouverture (cumul depuis l'accueil).

| Taille | Instances TinyTTF | Glyphes après les cinq écrans |
| --- | --- | --- |
| 1280×720 | 20 → **14** | 181 161 → **167 920 o** (−13,2 Ko, −7 %) |
| 1024×600 (7B) | 20 → **16** | 124 346 → **115 621 o** (−8,7 Ko, −7 %) |
| 720×1280 | 20 → **14** | 180 725 → **167 484 o** (−13,2 Ko, −7 %) |

Sur l'appareil s'y ajoutent les cinq polices de l'écran de démarrage (72, 128, 44, 38 et 30 px) : 44 et 30 px sont désormais les mêmes instances que celles des Réglages, des Alertes et de Détails.

Temps d'ouverture à 1280×720 (construction / premier rendu complet, médianes) :

| Page | `main` 1re ouverture | branche 1re | `main` 2e | branche 2e |
| --- | --- | --- | --- | --- |
| Détails | 154 / 4 009 | 158 / 4 072 | 146 / 3 378 | 145 / 3 634 |
| Mer | 59 / 9 790 | 60 / 12 606 | 49 / 9 123 | 49 / 8 937 |
| Réglages | 197 / 1 992 | 189 / 1 952 | 166 / 1 550 | 152 / 1 626 |
| Alertes | 215 / 6 677 | 219 / 7 026 | 199 / 5 581 | 202 / 6 134 |

**Pas de gain de temps mesurable sur le PC** : les écarts vont dans les deux sens et atteignent ±20 % entre deux ouvertures au code identique (PC partagé avec d'autres compilations). Ce que disent ces chiffres :

- **construire** une page coûte 0,05 à 0,2 ms, son **premier rendu** 1,5 à 15 ms : garder les objets d'une page entre deux ouvertures (RAP-06) ne gagnerait au plus que la construction, en gardant en mémoire un écran fermé. Non fait.
- la **rastérisation à froid** des glyphes (1re ouverture moins la 2e, 0,3 à 1 ms) ne concerne que la première ouverture : chaque page gardait déjà ses polices d'une ouverture à l'autre. Le registre l'évite pour les tailles partagées seulement.

**Dimensionnement du cache** : sur les sept langues et les trois tailles, une police partagée sert au plus **72 glyphes différents** (22 px à 1024×600, en français et en turc), sous les 128 du cache : aucun glyphe n'est chassé puis rastérisé à nouveau pendant ces parcours. Non mesuré : le clavier et les sous-pages des réglages (la scène n'ouvre que la liste).

## Anneau du toucher (RAP-13)

`.\tools\rendre.ps1 -Scene ouvertures -Variante anneau` : un vrai pointeur LVGL sur l'accueil (sans alerte, pour que le doigt arrive à l'écran et non à la pastille).

| Taille | Doigt immobile, 5 lectures | Pas de 1 px |
| --- | --- | --- |
| 1280×720 | 0 zone | **7 832 px** = l'ancienne et la nouvelle boîte de l'anneau (88 px), 0,85 % de l'écran |
| 1024×600 | 0 zone | 4 970 px (boîte de 70 px) |
| 720×1280 | 0 zone | 7 832 px |

LVGL le fait déjà : `lv_obj_set_x/y` ne changent rien si la valeur est la même, et un déplacement n'invalide que l'ancienne et la nouvelle boîte. **Aucun changement de `ui_principal.cpp`** ; la variante sort en ECHEC si cela change. Piège rencontré : pendant le rendu, `lv_refr.c` (`get_max_row`) envoie `LV_EVENT_INVALIDATE_AREA` avec une zone `0,0-0,h` pour arrondir la hauteur de ses tampons ; ce n'est pas une invalidation, la scène ne note donc les zones qu'en dehors du rendu.

## Changement de ciel (RAP-14) : non fait

Le ciel de l'accueil change quand le moment du jour ou l'ambiance change (quelques fois par jour). Chaque pixel du fond change alors : l'image doit être remplie à nouveau et l'écran repeint en entier, quoi qu'on fasse. Remplir l'image existante au lieu d'en créer une neuve n'économiserait qu'une allocation et, quelques millisecondes durant, 1,8 Mo de PSRAM en double. Gain trop faible pour toucher au code : non fait.

## Rendu identique (banc du PC)

Tampons de 28 lignes, toutes les scènes, langues et tailles par défaut (dont 720×1280) : **1524 rendus sur 1524 identiques au pixel** (`ImageChops.difference(...).getbbox()` vide), `main` `ddc2f42` contre cette branche : `principal` (162), `principal_themes` (111), `details` (180), `mer` (189), `roue` (50, ses deux tailles), `reglages` (342), `installation` (96), `parcours` (42), `alertes` (81), `auto` (234), `fuseaux` (14), `bonjour` (4), `hote_appareil` (10), `temoin` (3), `ouvertures` (6). Avant la fusion de #39, la même comparaison sans l'accueil branché avait donné 1513 sur 1513.

Contre-épreuve : registre créé **sans crénage** (`LV_FONT_KERNING_NONE`, fichier restauré après) : **333 rendus sur 341 diffèrent** (`principal`, `details`, `mer`, `reglages`, `roue` en `fr`) ; les 8 autres n'ont pas de paire de lettres crénée.

## Non vérifié sur la carte (protocole)

Rien n'a été flashé. Pour le Tab5 (et la 7B quand elle sera essayée), avant/après sur le même binaire hormis ce lot :

1. Profil LVGL de l'ouverture de chaque page (Détails, Mer, Réglages, Alertes), 1re et 2e ouverture après un démarrage, comme le profil de l'accueil du 08/11/2026 : durée du premier rendu complet.
2. PSRAM utilisée (`heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`) avant le démarrage de l'accueil, après l'accueil, puis après chaque page ouverte : l'écart attendu est de l'ordre des −13 Ko du banc (images de glyphes), plus les structures des six instances TinyTTF en moins.
3. Mémoire interne libre au même moment : inchangée attendue (les images de glyphes sont en PSRAM depuis le 07/11/2026).

Vu sur : banc du PC (Windows, clang 23.1.5 llvm-mingw, LVGL 9.5.0), aucune carte, branche `claude/rapidite-polices-pages-726178`, le 12/11/2026.
