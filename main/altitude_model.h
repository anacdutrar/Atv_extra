#pragma once

// Inicializa o interpretador TFLite Micro e valida os tensores do modelo.
bool altitude_model_init();

// Executa a inferencia usando temperatura em graus Celsius e pressao em Pa.
// A altitude calculada e devolvida em metros.
bool altitude_model_predict(float temperature_c, float pressure_pa,
                            float* altitude_m);
