// Compteur de démarrages (NVS) et cause du dernier redémarrage.
#pragma once

#include <cstdint>

namespace ecran {

struct BootInfo {
    uint32_t boot_count;      // numéro de ce démarrage (1 au premier), gardé en NVS
    bool counter_persistent;  // faux si la NVS était illisible et a dû être reformatée ou n'a pas pu écrire
    const char *reset_reason; // cause du redémarrage, en français
};

// Initialise la NVS, incrémente et enregistre le compteur, lit la cause du redémarrage. À appeler une fois, au début.
BootInfo boot_info_load();

} // namespace ecran
