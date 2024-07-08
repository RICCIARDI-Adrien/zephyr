#include <nvs_factory_tool.h>

// OTP layer
uint16_t otp_item_id_0_data = 0x1234;
uint32_t otp_item_id_3_data = 0x1234;
uint8_t otp_item_id_27349_data[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

struct nvs_factory_item_t otp_items[] = {
	{ 12, "test string OTP", sizeof("test string OTP") },
	{ 3, &otp_item_id_3_data, sizeof(otp_item_id_3_data) },
	{ 0, &otp_item_id_0_data, sizeof(otp_item_id_0_data) },
	{ 27349, &otp_item_id_27349_data, sizeof(otp_item_id_27349_data) }
};

// Manufacturing layer

// User layer

struct nvs_factory_layer_t layers[] = {
	// OTP layer
	{ "image-0", otp_items, sizeof(otp_items) / sizeof(otp_items[0]) }
};
