# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

set(FACTORY_BASE_DIR ${ZEPHYR_BASE}/samples/subsys/nvs/factory)
set(INCLUDE_OUTPUT_DIR ${APPLICATION_BINARY_DIR}/include)

ExternalProject_Add(nvs_factory_tool
  SOURCE_DIR ${FACTORY_BASE_DIR}
  CMAKE_ARGS
  -DFACTORY_BINARY_DIR=${APPLICATION_BINARY_DIR}/factory
  -DFACTORY_NVS_CONTENT_FILE=${FACTORY_BASE_DIR}/src/demo_nvs_content.c
  -DFACTORY_NVS_PARTITION_DT_LABEL=storage_partition
  -DBOARD=native_sim
  -DFACTORY_FLASH_ERASE_VALUE=0xFF
  -DFACTORY_FLASH_ERASE_BLOCK_SIZE=0x800
  -DFACTORY_FLASH_WRITE_BLOCK_SIZE=0x2
  -DFACTORY_NVS_PARTITION_SIZE=0x1800
  INSTALL_COMMAND ""
  TEST_COMMAND zephyr/zephyr.exe --flash=${APPLICATION_BINARY_DIR}/nvs_area.bin &&
  mkdir -p ${INCLUDE_OUTPUT_DIR} &&
  xxd -i ${APPLICATION_BINARY_DIR}/nvs_area.bin > ${INCLUDE_OUTPUT_DIR}/nvs_area.h &&
  sed -i -e "1d" ${INCLUDE_OUTPUT_DIR}/nvs_area.h &&
  sed -i -e "1i __in_section(nvs_partition, static, var) unsigned char nvs_area[] = {" ${INCLUDE_OUTPUT_DIR}/nvs_area.h
  TEST_AFTER_INSTALL TRUE
)
add_dependencies(${DEFAULT_IMAGE} nvs_factory_tool)
