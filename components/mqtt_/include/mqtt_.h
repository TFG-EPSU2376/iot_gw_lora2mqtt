void mqtt_app_start(char *mqtt_broker, int mqtt_port, char *client_id);
void init_mqtt(char *mqtt_broker, int mqtt_port, char *client_id);
void send_mqtt_message(const char *topic, const char *message);
void get_current_time_iso8601(char *buffer, size_t buffer_size);