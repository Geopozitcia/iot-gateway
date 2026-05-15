#ifndef SENSORS_H
#define SENSORS_H

typedef struct {
    double temperature;
    double pressure;
} SensorData;

int sensors_init(void);
int sensors_read_bmp280(int fd, SensorData *data);

int sensors_read_motion(void);

void sensors_close(int fd);

#endif