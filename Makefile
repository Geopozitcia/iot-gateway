CC = gcc
CFLAGS = -Wall -Wextra -I include
LIBS = -lmosquitto -li2c

SRC = src/main.c src/sensors.c src/camera.c src/mqtt.c
OBJ = $(SRC:.c=.o)
TARGET = iot-gateway

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) src/*.o test_sensors test_camera test_mqtt

.PHONY: all clean