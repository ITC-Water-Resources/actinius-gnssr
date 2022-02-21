# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml)
list(APPEND mcuboot_OVERLAY_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay-rsa.conf")
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/actinius_icarus.overlay)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gnssr)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
