#include "log.h"
#include "i2c.hpp"
#include "aht21.hpp"

#include <nvs_flash.h>
#include <esp_matter.h>

#include <driver/gpio.h>
#include <freertos/semphr.h>

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

#define I2C_MASTER_SCL_IO GPIO_NUM_20
#define I2C_MASTER_SDA_IO GPIO_NUM_19
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

#define AHT21_ADDR 0x38
// i2c_master_dev_handle_t AHT21;
AHT21 aht21;

static void temperature_sensor_update(uint16_t endpoint_id, float temp, void* data) {
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, temp]() {
        attribute_t* attr = attribute::get(endpoint_id,
                                           TemperatureMeasurement::Id, 
                                           TemperatureMeasurement::Attributes::MeasuredValue::Id);
        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attr, &val);
        LOGI("Updating temperature sensor: endpoint=%u, temp=%.2f", endpoint_id, temp);
        val.val.i16 = static_cast<int16_t>(temp * 100);
        attribute::update(endpoint_id, 
                          TemperatureMeasurement::Id, 
                          TemperatureMeasurement::Attributes::MeasuredValue::Id, 
                          &val);
    });
}

static void humidity_sensor_update(uint16_t endpoint_id, float rh, void* data) {
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, rh]() {
        attribute_t* attr = attribute::get(endpoint_id,
                                           RelativeHumidityMeasurement::Id, 
                                           RelativeHumidityMeasurement::Attributes::MeasuredValue::Id);
        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attr, &val);
        LOGI("Updating humidity sensor: endpoint=%u, rh=%.2f", endpoint_id, rh);
        val.val.u16 = static_cast<uint16_t>(rh * 100);
        attribute::update(endpoint_id, 
                          RelativeHumidityMeasurement::Id, 
                          RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, 
                          &val);
    });
}

static void sensor_update(void* args) {
    for(;;) {
        float temperature, humidity;
        esp_err_t err = aht21.measure(&temperature, &humidity);

        if(err != ESP_OK) {
            LOGE("Failed to read from AHT21 sensor: %s", esp_err_to_name(err));
        } else {
            LOGI("Sensor readings: Temperature=%.2f °C, Humidity=%.2f %%", temperature, humidity);
            humidity_sensor_update(2, humidity, NULL); // TODO: change hard-coded id
            temperature_sensor_update(1, temperature, NULL);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
        // uint8_t buf1[] = {  0xAC, 0x33, 0x00 };
        // ABORTIF(i2c_master_transmit(AHT21, buf1, sizeof(buf1), I2C_MASTER_TIMEOUT_MS) != ESP_OK, 
        //         "Failed to transmit I2C data to start measurement");
        // LOGD("Transmitted I2C data to start measurement");

        // vTaskDelay(pdMS_TO_TICKS(80)); // Wait for measurement to be taken

        // for(;;) {
        //     uint8_t read_buf1[1];
        //     i2c_master_receive(AHT21, read_buf1, sizeof(read_buf1), I2C_MASTER_TIMEOUT_MS);
        //     LOGD("Measurement in progress, sensor status: 0x%02X", read_buf1[0]);
        //     if (!(read_buf1[0] & 0x80)) break;
        //     vTaskDelay(pdMS_TO_TICKS(20));
        // }
        
        // uint8_t read_buf2[6];
        // ABORTIF(i2c_master_receive(AHT21, read_buf2, sizeof(read_buf2), I2C_MASTER_TIMEOUT_MS) != ESP_OK, 
        //         "Failed to receive I2C data for measurement");
        // LOGD("Received measurement data from sensor");

        // uint32_t srh = (read_buf2[1] << 12) | (read_buf2[2] << 4) | (read_buf2[3] >> 4);
        // float rh = (srh / 1048576.0) * 100.0;

        // uint32_t st = ((read_buf2[3] & 0x0F) << 16) | read_buf2[4] << 8 | read_buf2[5];
        // float t = (st / 1048576.0) * 200.0 - 50.0;

        // humidity_sensor_update(2, rh, NULL); // TODO: change hard-coded id
        // temperature_sensor_update(1, t, NULL);

        // LOGI("Sensor readings: Temperature=%.2f °C, Humidity=%.2f %%", t, rh);
        // vTaskDelay(pdMS_TO_TICKS(5000));  
    }
}

static void event_cb(const ChipDeviceEvent* event, intptr_t arg) {
    switch(event->Type) {
        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete: {
            LOGI("Commissioning complete");
            break;
        }

        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired: {
            LOGI("Commissioning failed: fail-safe timer expired");
            break;
        }

        case chip::DeviceLayer::DeviceEventType::kFabricRemoved: {
            LOGI("Fabric removed successfully");
            // TODO: open commissioning window ?
            break;
        }

        case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized: {
            LOGI("BLE deinitialized");
            break;
        }

        default:
            LOGD("Unhandled event: type=%u", event->Type);
            break;
    }
}

static esp_err_t identity_cb(identification::callback_type_t t, uint16_t endpt_id, uint8_t eff_id, 
                             uint8_t eff_variant, void* priv_data) {
    LOGI("Identification callback: type:%u, eff:%u, variant:%u", t, eff_id, eff_variant);
    return ESP_OK;
}

static esp_err_t attr_upd_cb(attribute::callback_type_t t, uint16_t endpt_id, uint32_t cluster_id, 
                             uint32_t attr_id, esp_matter_attr_val_t* val, void* priv_data) {
    return ESP_OK;
}

static SemaphoreHandle_t factory_reset_sem = NULL;
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    xSemaphoreGiveFromISR(factory_reset_sem, NULL);
}
void factory_reset_task(void* p) {
    for(;;) {
        if (xSemaphoreTake(factory_reset_sem, portMAX_DELAY) == pdTRUE) {
            LOGW("Boot button pressed");
            vTaskDelay(pdMS_TO_TICKS(3000)); // Wait for 3 seconds
            if (gpio_get_level(GPIO_NUM_9) == 0) { // Still pressed after 3 seconds
                LOGW("Factory resetting...");
                esp_matter::factory_reset();
            } else {
                LOGI("Aborting factory reset, button released");
            }
        }
    }
}

// void battery_measurement_task(void* p) {
//     adc_oneshot_unit_handle_t adc_handle;
//     adc_oneshot_unit_init_cfg_t adc_config = {
//         .unit_id = ADC_UNIT_1,
//         // .ulp_mode = ADC_ULP_MODE_DISABLE,
//     };
//     ABORTIF(adc_oneshot_new_unit(&adc_config, &adc_handle) != ESP_OK, "Failed to initialize ADC");

//     adc_oneshot_chan_cfg_t chan_config = {
//         .atten = ADC_ATTEN_DB_12,
//         .bitwidth = ADC_BITWIDTH_12,
//     };
//     ABORTIF(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &chan_config) != ESP_OK, "Failed to configure ADC channel");

//     adc_cali_handle_t adc_cali_handle;
//     adc_cali_curve_fitting_config_t cali_config = {
//         .unit_id = ADC_UNIT_1,
//         .chan = ADC_CHANNEL_0,
//         .atten = ADC_ATTEN_DB_12,
//         .bitwidth = ADC_BITWIDTH_12,
//     };
//     ABORTIF(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle) != ESP_OK, "Failed to create ADC calibration scheme");
    
//     for(;;) {
//         int raw, val;
//         if(adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &raw) != ESP_OK) {
//             LOGE("Failed to read ADC");
//             continue;
//         }
//         if(adc_cali_raw_to_voltage(adc_cali_handle, raw, &val) != ESP_OK) {
//             LOGE("Failed to calibrate ADC reading");
//             continue;
//         }
//         LOGI("Battery voltage raw reading: %d, calibrated: %d", raw, val*2);
//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }
// }

extern "C" void app_main() {
    esp_log_level_set(TAG, LOG_LOCAL_LEVEL);
    LOGI("Thensor v1.0 [%s]", log_level_name(LOG_LOCAL_LEVEL));

    // uint8_t data[2];
    // i2c_master_bus_handle_t bus;
    // // i2c_master_dev_handle_t dev;
    // i2c_master_bus_config_t bus_cfg = {
    //     .i2c_port = I2C_MASTER_NUM,
    //     .sda_io_num = I2C_MASTER_SDA_IO,
    //     .scl_io_num = I2C_MASTER_SCL_IO,
    //     .clk_source = I2C_CLK_SRC_DEFAULT,
    //     .glitch_ignore_cnt = 7,
    //     .flags = {
    //         .enable_internal_pullup = true,
    //     },
    // };

    // ABORTIF(i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK, "Failed to initialize I2C bus");
    // LOGD("Initialized I2C bus");

    // i2c_device_config_t dev_cfg = {
    //     .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    //     .device_address = AHT21_ADDR,
    //     .scl_speed_hz = I2C_MASTER_FREQ_HZ
    // };
    // ABORTIF(i2c_master_bus_add_device(bus, &dev_cfg, &AHT21) != ESP_OK, "Failed to add I2C device");
    // LOGD("Added I2C device: address=0x%02X", AHT21_ADDR);

    // vTaskDelay(pdMS_TO_TICKS(100)); // Wait for sensor to power up

    // uint8_t buf[] = { 0x71 };
    // ABORTIF(i2c_master_transmit(AHT21, buf, sizeof(buf), I2C_MASTER_TIMEOUT_MS) != ESP_OK, 
    //         "Failed to transmit I2C data");
    // LOGD("Transmitted I2C data to init sensor");

    // uint8_t read_buf[1];
    // i2c_master_receive(AHT21, read_buf, sizeof(read_buf), I2C_MASTER_TIMEOUT_MS);
    // ABORTIF((read_buf[0] & 0x18) != 0x18, "AHT21 registers not initialized [0x1B, 0x1C, & 0x1E]");
    // LOGD("Received sensor response: 0x%02X", read_buf[0]);
    
    // vTaskDelay(pdMS_TO_TICKS(10)); // Wait for sensor to be ready after init

    ABORTIF(i2c_init() != ESP_OK, "Failed to initialize I2C");
    ABORTIF(aht21.init() != ESP_OK, "Failed to initialize AHT21 sensor");

    nvs_flash_init();
    LOGD("Initialized NVS");
    
    node::config_t node_config;
    node_t* node = node::create(&node_config, attr_upd_cb, identity_cb);
    ABORTIF(node == nullptr, "Failed to create node");
    LOGD("Created node");

    temperature_sensor::config_t temp_sensor_config;
    endpoint_t* temp_sensor_ep = temperature_sensor::create(node, &temp_sensor_config, 0, NULL);
    ABORTIF(temp_sensor_ep == nullptr, "Failed to create temperature sensor endpoint");
    LOGD("Created temperature sensor endpoint");

    humidity_sensor::config_t humidity_sensor_config;
    endpoint_t* humidity_sensor_ep = humidity_sensor::create(node, &humidity_sensor_config, 0,NULL);
    ABORTIF(humidity_sensor_ep == nullptr, "Failed to create humidity sensor endpoint");
    LOGD("Created humidity sensor endpoint");

    LOGD("Starting Matter");
    ESP_ERROR_CHECK(esp_matter::start(event_cb));
    LOGI("Started Matter");

    factory_reset_sem = xSemaphoreCreateBinary();

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_9),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_9, gpio_isr_handler, NULL);
    xTaskCreate(factory_reset_task, "factory_reset_task", 4096, NULL, 5, NULL);
    LOGI("Factory reset button configured on GPIO9");

    xTaskCreate(sensor_update, "sensor_update_task", 4096, NULL, 5, NULL);
    LOGI("Sensor update task started");

    // xTaskCreate(battery_measurement_task, "battery_measurement_task", 4096, NULL, 5, NULL);
    // LOGI("Battery measurement task started");
}