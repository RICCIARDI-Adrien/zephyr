#ifndef H_NVS_FACTOY_TOOL_H
#define H_NVS_FACTOY_TOOL_H

#include <stddef.h>
#include <stdint.h>

struct nvs_factory_item_t {
	uint16_t id;
	const void *data;
	uint16_t size;
};

struct nvs_factory_layer_t {
	char *storage_partition_device_tree_label;
	struct nvs_factory_item_t *items;
	size_t items_count;
};

extern struct nvs_factory_layer_t layers[];

#endif
