# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

set(FACTORY_BASE_DIR ${ZEPHYR_BASE}/samples/subsys/nvs/factory)

ExternalProject_Add(nvs_factory_tool
  SOURCE_DIR ${FACTORY_BASE_DIR}
  CMAKE_ARGS
  -DFACTORY_FLASH_CONTROLLER_OVERLAY=${ZEPHYR_BASE}/samples/sysbuild/hello_world/nucleo_f091rc_flash.overlay
  -DFACTORY_NVS_CONTENT_FILE=${FACTORY_BASE_DIR}/src/demo_nvs_content.c
  -DFACTORY_NVS_PARTITION_DT_LABEL=storage_partition
  -DBOARD=native_sim
  INSTALL_COMMAND ""
  TEST_COMMAND zephyr/zephyr.exe --flash=${APPLICATION_BINARY_DIR}/nvs_area.bin &&
  xxd -i ${APPLICATION_BINARY_DIR}/nvs_area.bin > ${APPLICATION_BINARY_DIR}/nvs_area.h
  TEST_AFTER_INSTALL TRUE
)
add_dependencies(${DEFAULT_IMAGE} nvs_factory_tool)
