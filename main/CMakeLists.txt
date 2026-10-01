idf_component_register(
    SRCS
        "main.cc"
        "altitude_model.cc"
        "model_data.cc"
    INCLUDE_DIRS "."
    REQUIRES bmp180
)
