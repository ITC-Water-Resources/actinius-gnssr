#
# Copyright (c) 2019 Nordic Semiconductor
# Modified by R.Rietbroek (r.rietbroek@utwente.nl)
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.13.1)

set(ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/lz4stream) 

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/actinius_icarus.overlay)

#uncomment the following to use the external sim
  #set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/actinius_use_externalsim.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gnssr_logger)

zephyr_library_sources(src/main.c src/featherw_datalogger.c src/config.c src/led_buttons.c src/modem.c src/gnss.c)

zephyr_library_sources_ifdef(
  CONFIG_UPLOAD_CLIENT
  src/uploadclient.c
)

zephyr_library_sources_ifdef(
  CONFIG_SUPL_CLIENT_LIB
  src/supl_support.c
)
