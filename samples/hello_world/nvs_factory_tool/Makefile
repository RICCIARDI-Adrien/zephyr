CC = gcc
CFLAGS = -W -Wall

BINARY = nvs-factory-tool

all: $(BINARY)

$(BINARY): main.c
	$(CC) $(CFLAGS) main.c \
	-I $(ZEPHYR_BASE)/include \
	-I $(APPLICATION_BINARY_DIR)/zephyr/include/generated \
	-o $(BINARY)
