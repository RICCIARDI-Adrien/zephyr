# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(test_power_management)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
  src/main.c
  src/watchdog.c
)
