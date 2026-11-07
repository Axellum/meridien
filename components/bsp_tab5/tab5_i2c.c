// Petits peripheriques du bus I2C commun du Tab5 (GPIO31/32) : ES8388, ES7210, INA226, RX8130. Acces par registre de 8 bits.
// Le pilote I2C maitre d'ESP-IDF serialise lui-meme les transactions du bus (le tactile, les expandeurs et ces puces le
// partagent) : aucun verrou ici. Les erreurs sont rendues telles quelles ; c'est l'appelant qui decide de les journaliser.
#include "esp_check.h"

#include "tab5_internal.h"

#define TAB5_I2C_HZ 400000
#define TAB5_I2C_TIMEOUT_MS 100

esp_err_t tab5_i2c_attach(uint8_t address, i2c_master_dev_handle_t *dev) {
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = TAB5_I2C_HZ,
    };
    return i2c_master_bus_add_device(tab5_i2c_bus(), &cfg, dev);
}

esp_err_t tab5_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, TAB5_I2C_TIMEOUT_MS);
}

esp_err_t tab5_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *data, size_t len) {
    // Registre puis donnees dans UNE transaction (pas de tampon dynamique : 16 octets suffisent, les puces d'ici n'en ecrivent
    // jamais plus de 7 d'un bloc).
    uint8_t trame[17];
    if (len > sizeof(trame) - 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    trame[0] = reg;
    for (size_t i = 0; i < len; i++) {
        trame[1 + i] = data[i];
    }
    return i2c_master_transmit(dev, trame, len + 1, TAB5_I2C_TIMEOUT_MS);
}

esp_err_t tab5_i2c_read_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *value) {
    return tab5_i2c_read_reg(dev, reg, value, 1);
}

esp_err_t tab5_i2c_write_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value) {
    return tab5_i2c_write_reg(dev, reg, &value, 1);
}

esp_err_t tab5_i2c_update_u8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t actuel = 0;
    ESP_RETURN_ON_ERROR(tab5_i2c_read_u8(dev, reg, &actuel), "tab5_i2c", "lecture avant mise a jour");
    return tab5_i2c_write_u8(dev, reg, (uint8_t)((actuel & ~mask) | (value & mask)));
}

bool tab5_i2c_probe(uint8_t address) {
    return i2c_master_probe(tab5_i2c_bus(), address, TAB5_I2C_TIMEOUT_MS) == ESP_OK;
}
