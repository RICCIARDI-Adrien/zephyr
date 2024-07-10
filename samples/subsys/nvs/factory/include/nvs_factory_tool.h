/*
 * Copyright (C) 2024 BayLibre.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_NVS_FACTORY_TOOL_H
#define H_NVS_FACTORY_TOOL_H

#include <stddef.h>
#include <stdint.h>

struct nvs_factory_tool_item_t {
	uint16_t id;
	const void *data;
	uint16_t size;
};

extern struct nvs_factory_tool_item_t nvs_factory_tool_items[];
extern size_t nvs_factory_tool_items_count;

#endif
