# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)

add_custom_command(
  OUTPUT include/factory_nvs.h
  COMMAND ${APPLICATION_SOURCE_DIR}/generate-factory-nvs.sh
  COMMAND
    ZEPHYR_BASE=${ZEPHYR_BASE}
    APPLICATION_BINARY_DIR=${APPLICATION_BINARY_DIR}
    make -C ${APPLICATION_SOURCE_DIR}/nvs_factory_tool
  COMMAND ${APPLICATION_SOURCE_DIR}/nvs_factory_tool/nvs-factory-tool
  DEPENDS zephyr_generated_headers
)
target_sources(app PRIVATE include/factory_nvs.h)
target_include_directories(app PRIVATE ${APPLICATION_BINARY_DIR}/include)
