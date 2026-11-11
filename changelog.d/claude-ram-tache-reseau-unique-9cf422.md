### Changed

- **One network task for the data instead of four: 12 kB less internal RAM reserved for stacks at rest, 24 kB once a sea
  spot is chosen.** The weather, the Vigie alerts, the sea of the chosen spot and the list of spots no longer each own a
  task with a 12 kB stack in internal RAM (weather and alerts for the whole life of the device, the sea as soon as a spot
  is chosen, the list on demand): one task, `donnees`, runs them one step at a
  time ([main/tache_donnees.h](main/tache_donnees.h)). They never made two requests at once anyway (common TLS lock). Each
  client keeps its own rhythm, its retry after a failure and its reaction to the settings; who runs and when is decided by
  the pure core ([ecran/ordonnanceur_reseau.hpp](components/core/include/ecran/ordonnanceur_reseau.hpp), 20 tests, 20
  mutations): the most overdue first, the list of spots before the others (someone is waiting in front of the screen),
  no starvation. The city search keeps its own short-lived task: it never queues behind the data, only behind the TLS
  lock, as before. Not tested on a board.
- **Two fixed variables of `main/` moved from internal RAM to PSRAM: −7,480 bytes of internal `.bss` on the Tab5 and the
  7B (`idf.py size`, CI).** The settings host (`hote_appareil_esp.cpp`, 5.0 kB) and the detailed weather read by the home
  screen (`ecran_principal.cpp`, 2.1 kB) are now allocated once in PSRAM; 0.4 kB of `atexit` tables go away with them. `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` is deliberately **not** enabled: it would also
  move the whole `.bss` of lwIP (the TCP/IP stack of Wi-Fi and TLS) to PSRAM. Not tested on a board.
