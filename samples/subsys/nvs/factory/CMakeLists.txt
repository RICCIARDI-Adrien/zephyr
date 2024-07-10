# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Check whether the mandatory variables have been defined
if(NOT DEFINED FACTORY_FLASH_CONTROLLER_OVERLAY)
  message(FATAL_ERROR "You must specify the FACTORY_FLASH_CONTROLLER_OVERLAY variable, \
  see README for an example.")
endif()
if(NOT DEFINED FACTORY_NVS_CONTENT_FILE)
  message(FATAL_ERROR "You must specify the FACTORY_NVS_CONTENT_FILE variable, \
  see README for an example.")
endif()
if(NOT DEFINED FACTORY_NVS_PARTITION_DT_LABEL)
  message(FATAL_ERROR "You must specify the FACTORY_NVS_PARTITION_DT_LABEL variable, \
  see README for an example.")
endif()

set(DTC_OVERLAY_FILE remove_default_flash_controller.overlay ${FACTORY_FLASH_CONTROLLER_OVERLAY})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nvs)

target_sources(app PRIVATE src/main.c ${FACTORY_NVS_CONTENT_FILE})
target_compile_definitions(app PRIVATE FACTORY_NVS_PARTITION_DT_LABEL=${FACTORY_NVS_PARTITION_DT_LABEL})
target_include_directories(app PRIVATE include)
