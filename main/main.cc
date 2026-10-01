#include <cstdint>

#include "bmp180.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2cdev.h"

#include "altitude_model.h"

namespace {

constexpr char kTag[] = "extra_altitude";
constexpr gpio_num_t kSdaGpio = GPIO_NUM_9;
constexpr gpio_num_t kSclGpio = GPIO_NUM_8;
constexpr TickType_t kMeasurementInterval = pdMS_TO_TICKS(2000);

}  // namespace

extern "C" void app_main(void) {
  bmp180_dev_t sensor = {};

  ESP_ERROR_CHECK(i2cdev_init());
  ESP_ERROR_CHECK(
      bmp180_init_desc(&sensor, I2C_NUM_0, kSdaGpio, kSclGpio));
  ESP_ERROR_CHECK(bmp180_init(&sensor));

  if (!altitude_model_init()) {
    ESP_LOGE(kTag, "Nao foi possivel inicializar o modelo");
    ESP_ERROR_CHECK(bmp180_free_desc(&sensor));
    return;
  }

  ESP_LOGI(kTag, "BMP180 pronto: SDA=GPIO%d, SCL=GPIO%d",
           static_cast<int>(kSdaGpio), static_cast<int>(kSclGpio));

  while (true) {
    float temperature_c = 0.0f;
    uint32_t pressure_pa = 0;

    const esp_err_t measurement_result = bmp180_measure(
        &sensor, &temperature_c, &pressure_pa,
        BMP180_MODE_ULTRA_HIGH_RESOLUTION);

    if (measurement_result != ESP_OK) {
      ESP_LOGE(kTag, "Falha na leitura do BMP180: %s",
               esp_err_to_name(measurement_result));
      vTaskDelay(kMeasurementInterval);
      continue;
    }

    float altitude_m = 0.0f;
    if (!altitude_model_predict(temperature_c,
                                static_cast<float>(pressure_pa),
                                &altitude_m)) {
      ESP_LOGE(kTag, "Falha ao estimar a altitude");
      vTaskDelay(kMeasurementInterval);
      continue;
    }

    ESP_LOGI(kTag,
             "Temperatura: %.2f C | Pressao: %lu Pa (%.2f hPa) | "
             "Altitude estimada: %.2f m",
             static_cast<double>(temperature_c),
             static_cast<unsigned long>(pressure_pa),
             static_cast<double>(pressure_pa / 100.0f),
             static_cast<double>(altitude_m));

    vTaskDelay(kMeasurementInterval);
  }
}
