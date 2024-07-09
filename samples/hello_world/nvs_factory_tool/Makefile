CC = gcc
CFLAGS = -W -Wall

BINARY = nvs-factory-tool
NVS_FACTORY_TOOL_SOURCES += \
	crc8_kconfig.c \
	crc32_kconfig.c \
	main.c \
	nvs_kconfig.c \
	nvs_stubs.c

all: $(BINARY)

$(BINARY): $(NVS_FACTORY_TOOL_SOURCES)
	$(CC) $(CFLAGS) $(NVS_FACTORY_TOOL_SOURCES) \
	-I $(ZEPHYR_BASE)/include \
	-I $(APPLICATION_BINARY_DIR)/zephyr/include/generated \
	-I $(CURDIR) \
	-I $(ZEPHYR_BASE)/subsys/fs/nvs \
	-I $(ZEPHYR_BASE) \
	-o $(BINARY)
