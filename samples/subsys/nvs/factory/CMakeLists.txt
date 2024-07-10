# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Check if mandatory build files have been provided
if(NOT DEFINED ENV{FACTORY_FLASH_OVERLAY})
  message(FATAL_ERROR "You must specify the FACTORY_FLASH_OVERLAY environment variable, \
  see README for an example.")
endif()

set(DTC_OVERLAY_FILE remove_default_flash_controller.overlay $ENV{FACTORY_FLASH_OVERLAY})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nvs)

target_sources(app PRIVATE src/main.c)
