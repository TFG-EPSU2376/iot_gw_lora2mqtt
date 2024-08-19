set(component_srcs)

idf_component_register(SRCS "main.c" "${component_srcs}"
                       INCLUDE_DIRS "."
                       REQUIRES lora mqtt_ )