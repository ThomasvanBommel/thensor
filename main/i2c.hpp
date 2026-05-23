#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>

#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS 1000

static i2c_master_bus_handle_t i2c_bus;

esp_err_t i2c_init() {
    i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_19,
        .scl_io_num = GPIO_NUM_20,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true }
    };

    return i2c_new_master_bus(&config, &i2c_bus);
}

class I2CDevice {
private:
    i2c_master_dev_handle_t _handle;
    const uint16_t _address;

public:
    I2CDevice(uint16_t address) : _address(address) {}

    esp_err_t init() {
        i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = _address,
            .scl_speed_hz = 400000
        };

        return i2c_master_bus_add_device(i2c_bus, &config, &_handle);
    }

    esp_err_t write(const uint8_t* data, size_t len) {
        return i2c_master_transmit(_handle, data, len, I2C_MASTER_TIMEOUT_MS);
    }

    esp_err_t read(uint8_t* data, size_t len) {
        return i2c_master_receive(_handle, data, len, I2C_MASTER_TIMEOUT_MS);
    }
};