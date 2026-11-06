# Modele de tools\local.ps1. Ce fichier-ci est suivi par git ; local.ps1, lui, ne l'est pas (.gitignore) : c'est la
# que vivent les chemins propres a votre PC. Copier ce fichier en tools\local.ps1, puis adapter la valeur ci-dessous.
#
# IdfRacine : dossier de l'installation d'ESP-IDF 5.5.5 qu'utilise tools\idf.ps1. Il doit avoir la disposition de
# l'installation faite par ESPHome : frameworks\5.5.5, penvs\5.5.5\Scripts\python.exe, tools\cmake, tools\ninja,
# tools\ccache, tools\riscv32-esp-elf. Avec l'installeur officiel d'Espressif la disposition est autre : utiliser
# alors son export.ps1 et idf.py directement, sans tools\idf.ps1.
#
# Autre moyen, sans fichier : definir la variable d'environnement MERIDIEN_IDF_RACINE (elle passe avant local.ps1).
$IdfRacine = '<A ADAPTER : dossier de l''installation ESP-IDF>'
