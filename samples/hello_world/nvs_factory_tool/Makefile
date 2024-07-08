CC = gcc
CFLAGS = -W -Wall

BINARY = nvs-factory-tool
NVS_FACTORY_TOOL_SOURCES += main.c nvs_kconfig.c

all: $(BINARY)

$(BINARY): $(NVS_FACTORY_TOOL_SOURCES)
	$(CC) $(CFLAGS) $(NVS_FACTORY_TOOL_SOURCES) \
	-I $(ZEPHYR_BASE)/include \
	-I $(APPLICATION_BINARY_DIR)/zephyr/include/generated \
	-I $(CURDIR) \
	-I $(ZEPHYR_BASE)/subsys/fs/nvs \
	-o $(BINARY)
