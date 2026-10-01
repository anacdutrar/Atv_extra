#include "altitude_model.h"

#include <cmath>
#include <cstdint>

#include "esp_log.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "extra_altitude_normalization.h"
#include "model_data.h"

namespace {

constexpr char kTag[] = "altitude_model";
constexpr int kExpectedInputCount = 2;
constexpr int kTensorArenaSize = 16 * 1024;

alignas(16) uint8_t tensor_arena[kTensorArenaSize];

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
bool initialized = false;

int tensor_element_count(const TfLiteTensor* tensor) {
  if (tensor == nullptr || tensor->dims == nullptr) {
    return 0;
  }

  int count = 1;
  for (int i = 0; i < tensor->dims->size; ++i) {
    count *= tensor->dims->data[i];
  }
  return count;
}

int8_t quantize_int8(float value, const TfLiteTensor* tensor) {
  float quantized =
      std::round(value / tensor->params.scale) + tensor->params.zero_point;

  if (quantized < -128.0f) {
    quantized = -128.0f;
  } else if (quantized > 127.0f) {
    quantized = 127.0f;
  }

  return static_cast<int8_t>(quantized);
}

}  // namespace

bool altitude_model_init() {
  if (initialized) {
    return true;
  }

  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(kTag, "Schema do modelo: %d; schema suportado: %d",
             model->version(), TFLITE_SCHEMA_VERSION);
    return false;
  }

  // As ativacoes ReLU das camadas Dense sao fundidas em FULLY_CONNECTED.
  static tflite::MicroMutableOpResolver<1> resolver;
  if (resolver.AddFullyConnected() != kTfLiteOk) {
    ESP_LOGE(kTag, "Falha ao registrar FULLY_CONNECTED");
    return false;
  }

  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(kTag, "AllocateTensors falhou; aumente kTensorArenaSize");
    return false;
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  if (input == nullptr || output == nullptr || input->type != kTfLiteInt8 ||
      output->type != kTfLiteInt8) {
    ESP_LOGE(kTag, "O modelo deve possuir entrada e saida int8");
    return false;
  }

  if (tensor_element_count(input) != kExpectedInputCount ||
      tensor_element_count(output) != 1) {
    ESP_LOGE(kTag, "Dimensoes inesperadas: entrada=%d, saida=%d",
             tensor_element_count(input), tensor_element_count(output));
    return false;
  }

  if (input->params.scale <= 0.0f || output->params.scale <= 0.0f) {
    ESP_LOGE(kTag, "Escala de quantizacao invalida");
    return false;
  }

  initialized = true;
  ESP_LOGI(kTag,
           "Modelo carregado (%d bytes), entrada int8 (scale=%.8f, zp=%d), "
           "saida int8 (scale=%.8f, zp=%d)",
           g_model_len, static_cast<double>(input->params.scale),
           input->params.zero_point, static_cast<double>(output->params.scale),
           output->params.zero_point);
  return true;
}

bool altitude_model_predict(float temperature_c, float pressure_pa,
                            float* altitude_m) {
  if (!initialized || altitude_m == nullptr || !std::isfinite(temperature_c) ||
      !std::isfinite(pressure_pa)) {
    return false;
  }

  const float normalized_temperature =
      (temperature_c - kAltitudeInputMean[0]) / kAltitudeInputStd[0];
  const float normalized_pressure =
      (pressure_pa - kAltitudeInputMean[1]) / kAltitudeInputStd[1];

  input->data.int8[0] = quantize_int8(normalized_temperature, input);
  input->data.int8[1] = quantize_int8(normalized_pressure, input);

  if (interpreter->Invoke() != kTfLiteOk) {
    ESP_LOGE(kTag, "Falha durante a inferencia");
    return false;
  }

  // O cast para int32_t evita overflow ao subtrair o zero point de um int8.
  const int32_t quantized_output =
      static_cast<int32_t>(output->data.int8[0]);
  const int32_t output_zero_point =
      static_cast<int32_t>(output->params.zero_point);
  const float normalized_altitude =
      static_cast<float>(quantized_output - output_zero_point) *
      output->params.scale;

  *altitude_m =
      normalized_altitude * kAltitudeTargetStd + kAltitudeTargetMean;
  return std::isfinite(*altitude_m);
}
