# Copyright (c) 2022 R. Rietbroek
# SPDX-License-Identifier: Apache-2.0
# modified from original CMakeLists.txt to also include lz4 files for stream support
# This file reuses the supplied source code which can be found in the LZ4 module directory 

if(CONFIG_LZ4STREAM)

  #tap into existing lz4 source code
  set(LZ4_DIR ${ZEPHYR_CURRENT_MODULE_DIR})

  zephyr_library()

  zephyr_include_directories(${LZ4_DIR}/lib)

  #add additional (in addition to lz4.c)  source files for stream support of lz4 files
  #note lz4 files are copied from from git@github.com:zephyrproject-rtos/lz4.git
  zephyr_library_sources(
    ${LZ4_DIR}/lib/lz4.c
    ${LZ4_DIR}/lib/lz4hc.c
    ${LZ4_DIR}/lib/xxhash.c
    ${LZ4_DIR}/lib/lz4frame.c
    ${LZ4_DIR}/lib/lz4file.c
  )

endif()
