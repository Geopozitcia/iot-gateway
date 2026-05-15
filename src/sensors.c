#include "sensors.h"
#include <fcntl.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BMP280_ADDR 0x76
#define BMP280_REG_ID 0xD0
#define BMP280_REG_CTRL 0xF4
#define BMP280_REG_CONF 0xF5
#define BMP280_REG_DATA 0xF7
#define HC_SR501_GPIO 24

int sensors_init(void) {
    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) {
        perror("Ошибка открытия I2C шины");
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, BMP280_ADDR) < 0) {
        perror("Ошибка подключения к BMP280");
        close(fd);
        return -1;
    }

    uint8_t chip_id = i2c_smbus_read_byte_data(fd, BMP280_REG_ID);
    if (chip_id != 0x60 && chip_id != 0x58) {
        fprintf(stderr, "Неверный ID датчика: 0x%02X\n", chip_id);
        close(fd);
        return -1;
    }

    i2c_smbus_write_byte_data(fd, BMP280_REG_CTRL, 0x27);
    i2c_smbus_write_byte_data(fd, BMP280_REG_CONF, 0xA0);

    return fd;
}

// Чтение калибровочных коэффициентов из датчика
static void bmp280_read_calibration(int fd, uint16_t *dig_T1, int16_t *dig_T2,
                                    int16_t *dig_T3, uint16_t *dig_P1,
                                    int16_t *dig_P2, int16_t *dig_P3,
                                    int16_t *dig_P4, int16_t *dig_P5,
                                    int16_t *dig_P6, int16_t *dig_P7,
                                    int16_t *dig_P8, int16_t *dig_P9) {

    *dig_T1 = i2c_smbus_read_word_data(fd, 0x88);
    *dig_T2 = (int16_t)i2c_smbus_read_word_data(fd, 0x8A);
    *dig_T3 = (int16_t)i2c_smbus_read_word_data(fd, 0x8C);
    *dig_P1 = i2c_smbus_read_word_data(fd, 0x8E);
    *dig_P2 = (int16_t)i2c_smbus_read_word_data(fd, 0x90);
    *dig_P3 = (int16_t)i2c_smbus_read_word_data(fd, 0x92);
    *dig_P4 = (int16_t)i2c_smbus_read_word_data(fd, 0x94);
    *dig_P5 = (int16_t)i2c_smbus_read_word_data(fd, 0x96);
    *dig_P6 = (int16_t)i2c_smbus_read_word_data(fd, 0x98);
    *dig_P7 = (int16_t)i2c_smbus_read_word_data(fd, 0x9A);
    *dig_P8 = (int16_t)i2c_smbus_read_word_data(fd, 0x9C);
    *dig_P9 = (int16_t)i2c_smbus_read_word_data(fd, 0x9E);
}

int sensors_read_bmp280(int fd, SensorData *data) {
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5;
    int16_t dig_P6, dig_P7, dig_P8, dig_P9;

    bmp280_read_calibration(fd, &dig_T1, &dig_T2, &dig_T3, &dig_P1, &dig_P2,
                            &dig_P3, &dig_P4, &dig_P5, &dig_P6, &dig_P7,
                            &dig_P8, &dig_P9);

    uint8_t buf[6];
    for (int i = 0; i < 6; i++) {
        int val = i2c_smbus_read_byte_data(fd, BMP280_REG_DATA + i);
        if (val < 0) {
            perror("Ошибка чтения данных BMP280");
            return -1;
        }
        buf[i] = (uint8_t)val;
    }

    int32_t adc_P =
        ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T =
        ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);

    // Компенсация температуры
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * dig_T2) >> 11;
    int32_t var2 = (((((adc_T >> 4) - (int32_t)dig_T1) *
                      ((adc_T >> 4) - (int32_t)dig_T1)) >>
                     12) *
                    dig_T3) >>
                   14;
    int32_t t_fine = var1 + var2;
    data->temperature = (double)((t_fine * 5 + 128) >> 8) / 100.0;

    // Компенсация давления
    int64_t p_var1 = (int64_t)t_fine - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)dig_P3) >> 8) +
             ((p_var1 * (int64_t)dig_P2) << 12);
    p_var1 = (((int64_t)1 << 47) + p_var1) * (int64_t)dig_P1 >> 33;

    if (p_var1 == 0)
        return -1;

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p_var2) * 3125) / p_var1;
    p_var1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    p_var2 = ((int64_t)dig_P8 * p) >> 19;
    p = ((p + p_var1 + p_var2) >> 8) + ((int64_t)dig_P7 << 4);
    data->pressure = (double)p / 25600.0;

    return 0;
}

int sensors_read_motion(void) {
    FILE *fp = popen("gpioget --chip gpiochip0 24", "r");
    if (!fp)
        return -1;

    char buf[32];
    fgets(buf, sizeof(buf), fp);
    pclose(fp);

    if (strstr(buf, "active") && !strstr(buf, "inactive"))
        return 1;
    return 0;
}

void sensors_close(int fd) { close(fd); }