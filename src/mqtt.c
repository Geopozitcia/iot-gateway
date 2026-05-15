#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mosquitto.h>
#include "../include/mqtt.h"

#define MQTT_TOPIC_SENSORS "iot/sensors"
#define MQTT_TOPIC_PHOTO   "iot/photo"
#define MQTT_QOS           1
#define MQTT_KEEPALIVE     30
#define MQTT_TOPIC_VIDEO "iot/video"


static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc) {
    (void)userdata;
    (void)mosq;
    if (rc != 0) {
        fprintf(stderr, "Разрыв MQTT соединения (rc=%d), автопереподключение...\n", rc);
    }
}

struct mosquitto *mqtt_init(const char *host, int port) {
    mosquitto_lib_init();

    struct mosquitto *mosq = mosquitto_new("iot-gateway", true, NULL);
    if (!mosq) {
        fprintf(stderr, "Ошибка создания MQTT клиента\n");
        return NULL;
    }

    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_reconnect_delay_set(mosq, 2, 10, true);

    if (mosquitto_connect(mosq, host, port, MQTT_KEEPALIVE) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Ошибка подключения к MQTT брокеру %s:%d\n", host, port);
        mosquitto_destroy(mosq);
        return NULL;
    }

    if (mosquitto_loop_start(mosq) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Ошибка запуска loop\n");
        mosquitto_destroy(mosq);
        return NULL;
    }

    printf("Подключено к MQTT брокеру %s:%d\n", host, port);
    return mosq;
}

int mqtt_publish_sensors(struct mosquitto *mosq, const SensorData *data) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", t);

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"timestamp\":\"%s\",\"temperature\":%.2f,\"pressure\":%.2f}",
        timestamp, data->temperature, data->pressure);

    int ret = mosquitto_publish(mosq, NULL, MQTT_TOPIC_SENSORS,
                                strlen(payload), payload, MQTT_QOS, false);
    if (ret != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Ошибка отправки данных датчиков: %s\n", mosquitto_strerror(ret));
        return -1;
    }

    printf("Отправлено: %s\n", payload);
    return 0;
}

int mqtt_publish_photo(struct mosquitto *mosq, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Ошибка открытия файла фото: %s\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *buf = malloc(size);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    fread(buf, 1, size, fp);
    fclose(fp);

    int ret = mosquitto_publish(mosq, NULL, MQTT_TOPIC_PHOTO,
                                size, buf, MQTT_QOS, false);
    free(buf);

    if (ret != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Ошибка отправки фото: %s\n", mosquitto_strerror(ret));
        return -1;
    }

    printf("Фото отправлено (%ld байт)\n", size);
    return 0;
}


void mqtt_cleanup(struct mosquitto *mosq) {
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}