#pragma once

#include "i2c.hpp"

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

class AHT21 : I2CDevice {
private:
    static const uint16_t ADDRESS = 0x38;

public:
    AHT21() : I2CDevice(ADDRESS) {}

    esp_err_t init() {
        esp_err_t err = I2CDevice::init();
        if(err != ESP_OK) return err;

        vTaskDelay(pdMS_TO_TICKS(100));
        err = I2CDevice::write((const uint8_t[]){ 0x71 }, 1);
        if(err != ESP_OK) return err;

        uint8_t read_buf[1];
        err = I2CDevice::read(read_buf, sizeof(read_buf));
        if(err != ESP_OK) return err;

        if((read_buf[0] & 0x18) != 0x18)
            return ESP_ERR_INVALID_RESPONSE;

        vTaskDelay(pdMS_TO_TICKS(10)); // Wait for sensor to be ready after init
        return ESP_OK;
    }

    esp_err_t measure(float* temperature, float* humidity) {
        esp_err_t err = I2CDevice::write((const uint8_t[]){ 0xAC, 0x33, 0x00 }, 3);
        if(err != ESP_OK) return err;

        vTaskDelay(pdMS_TO_TICKS(80)); // Wait for measurement to be taken

        for(;;) {
            uint8_t read_buf[1];
            err = I2CDevice::read(read_buf, sizeof(read_buf));
            if(err != ESP_OK) return err;
            
            if (!(read_buf[0] & 0x80)) break;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        uint8_t read_buf[6];
        err = I2CDevice::read(read_buf, sizeof(read_buf));
        if(err != ESP_OK) return err;

        uint32_t raw = (read_buf[1] << 12) | (read_buf[2] << 4) | (read_buf[3] >> 4);
        *humidity = raw / 10485.76f; //(srh / 1048576.0) * 100.0

        raw = ((read_buf[3] & 0x0F) << 16) | (read_buf[4] << 8) | read_buf[5];
        *temperature = raw / 5242.88f - 50.0f; //(st / 1048576.0) * 200.0 - 50.0

        return ESP_OK;
    }
};
