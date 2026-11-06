// Bouchon PC de esp_rom_sys.h : board.h en a besoin pour board_restart(). Le banc ne redémarre rien (définition dans banc.cpp).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void esp_rom_software_reset_system(void);

#ifdef __cplusplus
}
#endif
