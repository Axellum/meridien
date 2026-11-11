### Fixed

- **The time is asked again after the Wi-Fi comes back**: the direct SNTP request (`sntp_rapide`) now runs at every new IP
  address, never twice at once, and a failed request is retried after 10 s, 20 s, 40 s… up to 10 minutes. Before, it ran once
  per boot, and after a Wi-Fi loss the time rested on lwIP's slower SNTP alone (the Waveshare 7B firmware reads no battery clock to fall
  back on). The log now says `heure reçue N ms après l'IP`. The packet check and the retry rules are in the pure core and
  tested on the PC. Not tested on board.
