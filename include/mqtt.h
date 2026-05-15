#ifndef MQTT_H
#define MQTT_H

#include <mosquitto.h>
#include "sensors.h"

struct mosquitto *mqtt_init(const char *host, int port);
int mqtt_publish_sensors(struct mosquitto *mosq, const SensorData *data);
int mqtt_publish_photo(struct mosquitto *mosq, const char *filename);
void mqtt_cleanup(struct mosquitto *mosq);

#endif // MQTT_H