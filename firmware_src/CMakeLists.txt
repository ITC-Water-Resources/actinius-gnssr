#
# Copyright (c) 2019 Nordic Semiconductor
# Modified by R.Rietbroek (r.rietbroek@utwente.nl)
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.13.1)

set(ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/lz4stream) 

list(APPEND mcuboot_OVERLAY_CONFIG
  "${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_overlay-rsa.conf"
  )
set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml)

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/actinius_icarus.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gnssr_logger)

zephyr_library_sources(src/main.c src/featherw_datalogger.c)

zephyr_library_sources_ifdef(
  CONFIG_SUPL_CLIENT_LIB
  src/supl_support.c
)
