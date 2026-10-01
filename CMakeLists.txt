cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(atv1)

# A versão 1.4.1 do componente ativa ESP-NN internamente no ESP32-S3.
# O -U mantém a implementação de referência que foi validada no Wokwi.
idf_component_get_property(tflm_lib espressif__esp-tflite-micro COMPONENT_LIB)
if(DEFINED tflm_lib)
    if(TARGET ${tflm_lib})
        target_compile_options(${tflm_lib} PRIVATE -UESP_NN)
    endif()
endif()
