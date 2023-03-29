#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

list(APPEND mcuboot_OVERLAY_CONFIG
  "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay-rsa.conf"
  )
set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ciphersuites)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
# NORDIC SDK APP END
