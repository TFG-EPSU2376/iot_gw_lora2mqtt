set(component_srcs)

idf_component_register(SRCS "mqtt_.c" "${component_srcs}"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS "include"
                       REQUIRES wifi nvs_flash mqtt)
target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/client.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/client.key" TEXT)