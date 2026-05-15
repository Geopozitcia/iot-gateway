#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/camera.h"

#define CAPTURE_CMD   "rpicam-jpeg --nopreview -o %s 2>/dev/null"
#define VIDEO_CMD     "rpicam-vid --nopreview -t %d -o %s 2>/dev/null"

int camera_init(void) {
    int ret = system("which rpicam-jpeg > /dev/null 2>&1");
    if (ret != 0) {
        fprintf(stderr, "rpicam-jpeg не найден\n");
        return -1;
    }
    return 0;
}

int camera_capture(const char *filename) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), CAPTURE_CMD, filename);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Ошибка съёмки фото\n");
        return -1;
    }
    return 0;
}

