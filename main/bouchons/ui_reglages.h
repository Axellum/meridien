// BOUCHON — ne remplace PAS le vrai fichier. Le vrai `main/ui_reglages.h` (et `ui_reglages.cpp`) est écrit par la session
// `reglages-ecran` (ADR-0007 : réglages à l'écran et installation guidée). Tant qu'il n'existe pas, ce dossier donne à l'écran
// principal de quoi se compiler : le bouton « Réglages » appelle `ui_reglages_ouvrir()`, dont le corps provisoire
// (ui_reglages_bouchon.cpp) affiche « Réglages : bientôt » deux secondes. main/CMakeLists.txt ne compile ce bouchon et n'ajoute
// ce dossier au chemin d'inclusion que si `main/ui_reglages.h` n'existe pas : dès que le vrai fichier est là, ce dossier est
// ignoré (et à supprimer). Nom global, sans espace de noms : c'est la signature convenue.
#pragma once

// Ouvre l'écran des réglages. À appeler dans la tâche LVGL, verrou pris.
void ui_reglages_ouvrir();
