// Banc d'essai de l'interface Wi-Fi (recherche, essai avec retour arrière), piloté depuis la console série du PC. TEMPORAIRE : à
// retirer avant la phase 2, comme « memoire » et « piles » de commande_serie.cpp. Il sert à mesurer sur la carte, sans l'écran de
// réglages (qui n'est pas encore fusionné), ce que le C6 transmet pour un mot de passe faux, les durées de l'essai et du retour.
// Jamais d'identifiant au journal : seulement des résultats, des durées, des longueurs et des raisons de coupure.
#pragma once

namespace ecran {

// Reconnaît « wifi_scan », « wifi_faux », « wifi_faux_abandon », « wifi_inconnu » et « wifi_bon » (voir banc_wifi.cpp) et lance
// l'essai dans une tâche à part (un seul à la fois). Rend vrai si la ligne était l'une d'elles.
bool banc_wifi_lancer(const char *ligne);

} // namespace ecran
