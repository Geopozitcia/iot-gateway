#include "../include/camera.h"
#include "../include/mqtt.h"
#include "../include/sensors.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MQTT_HOST "192.168.1.65"
#define MQTT_PORT 1883
#define POLL_INTERVAL 1
#define COOLDOWN 10
#define SENSOR_INTERVAL 60
#define MOTION_CONFIRM_CNT 3
#define MOTION_CONFIRM_SEC 3

// Буфер для данных при потере связи
#define BUFFER_MAX 64

typedef struct {
    char payload[256];
    time_t timestamp;
} BufferedMessage;

static BufferedMessage sensor_buffer[BUFFER_MAX];
static int buffer_count = 0;
static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    printf("\nЗавершение работы\n");
    running = 0;
}

static void generate_photo_filename(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "photos/%Y%m%d_%H%M%S.jpg", t);
}

static void cleanup_photos(const char *dir, int max_files) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ls -t %s/*.jpg 2>/dev/null | tail -n +%d | xargs rm -f", dir,
             max_files + 1);
    system(cmd);
}

static void buffer_push(const char *payload) {
    if (buffer_count >= BUFFER_MAX) {
        memmove(&sensor_buffer[0], &sensor_buffer[1],
                sizeof(BufferedMessage) * (BUFFER_MAX - 1));
        buffer_count = BUFFER_MAX - 1;
    }
    strncpy(sensor_buffer[buffer_count].payload, payload, 255);
    sensor_buffer[buffer_count].timestamp = time(NULL);
    buffer_count++;
    printf("Данные буферизованы (в буфере: %d)\n", buffer_count);
}

static void buffer_flush(struct mosquitto *mosq) {
    if (buffer_count == 0)
        return;
    printf("Отправка буферизованных данных (%d сообщений)...\n", buffer_count);
    for (int i = 0; i < buffer_count; i++) {
        int ret = mosquitto_publish(mosq, NULL, "iot/sensors",
                                    strlen(sensor_buffer[i].payload),
                                    sensor_buffer[i].payload, 1, false);
        if (ret != MOSQ_ERR_SUCCESS) {
            printf("Ошибка отправки буферизованных данных\n");
            memmove(&sensor_buffer[0], &sensor_buffer[i],
                    sizeof(BufferedMessage) * (buffer_count - i));
            buffer_count -= i;
            return;
        }
    }
    buffer_count = 0;
    printf("Буфер очищен\n");
}

static void publish_sensors_buffered(struct mosquitto *mosq,
                                     const SensorData *data) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", t);

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"timestamp\":\"%s\",\"temperature\":%.2f,\"pressure\":%.2f}",
             timestamp, data->temperature, data->pressure);

    if (buffer_count > 0) {
        buffer_flush(mosq);
    }

    int ret = mosquitto_publish(mosq, NULL, "iot/sensors", strlen(payload),
                                payload, 1, false);

    if (ret != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Нет связи с брокером. Данные будут буферизированы\n");
        buffer_push(payload);
    } else {
        printf("Отправлено: %s\n", payload);
    }
}

static int motion_confirmed(void) {
    sleep(1);
    return sensors_read_motion();
}

int main(void) {
    printf("Приложение запущено\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int fd = sensors_init();
    if (fd < 0) {
        fprintf(stderr, "Ошибка инициализации датчиков\n");
        return 1;
    }

    if (camera_init() < 0) {
        fprintf(stderr, "Ошибка инициализации камеры\n");
        sensors_close(fd);
        return 1;
    }

    struct mosquitto *mosq = mqtt_init(MQTT_HOST, MQTT_PORT);
    if (!mosq) {
        fprintf(stderr, "Ошибка подключения к MQTT\n");
        sensors_close(fd);
        return 1;
    }

    printf("Все компоненты инициализированы. Ожидание движения...\n");

    time_t last_trigger = 0;
    time_t last_sensor_send = 0;

    while (running) {
        time_t now = time(NULL);

        if ((now - last_sensor_send) >= SENSOR_INTERVAL) {
            SensorData data;
            if (sensors_read_bmp280(fd, &data) == 0) {
                publish_sensors_buffered(mosq, &data);
                last_sensor_send = now;
            }
        }

        int motion = sensors_read_motion();
        if (motion == 1 && (now - last_trigger) >= COOLDOWN) {
            printf("[%ld] Первичное срабатывание датчика.\n", now);

            if (motion_confirmed()) {
                printf("[%ld] Движение подтверждено\n", now);
                last_trigger = time(NULL);

                SensorData data;
                if (sensors_read_bmp280(fd, &data) == 0) {
                    publish_sensors_buffered(mosq, &data);
                }

                char photo_filename[64];
                generate_photo_filename(photo_filename, sizeof(photo_filename));
                if (camera_capture(photo_filename) == 0) {
                    cleanup_photos("photos", 30);
                    mqtt_publish_photo(mosq, photo_filename);
                }
            } else {
                printf("[%ld] Игнорирование ложного движения\n", now);
            }
        }

        sleep(POLL_INTERVAL);
    }

    sensors_close(fd);
    mqtt_cleanup(mosq);
    printf("Приложение остановлено\n");
    return 0;
}