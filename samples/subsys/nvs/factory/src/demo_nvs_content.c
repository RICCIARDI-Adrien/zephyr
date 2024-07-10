/*
 * Copyright (C) 2024 BayLibre.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <nvs_factory_tool.h>

uint16_t item_id_0_data = 0x1234;
uint32_t item_id_3_data = 0x5678;
uint8_t item_id_27349_data[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

struct nvs_factory_tool_item_t nvs_factory_tool_items[] = {
	{ 12, "test string", sizeof("test string") },
	{ 3, &item_id_3_data, sizeof(item_id_3_data) },
	{ 0, &item_id_0_data, sizeof(item_id_0_data) },
	{ 27349, &item_id_27349_data, sizeof(item_id_27349_data) }
};

size_t nvs_factory_tool_items_count = sizeof(nvs_factory_tool_items) /
				      sizeof(struct nvs_factory_tool_item_t);
