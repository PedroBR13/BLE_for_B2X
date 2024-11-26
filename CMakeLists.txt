# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_uart)

# Include main application source files
target_sources(app PRIVATE src/main.c)

# Add modules source file
target_sources(app PRIVATE src/gnss_module.c src/scan_module.c src/beacon_module.c)

# If you have a separate include directory for headers, you can add it like this:
target_include_directories(app PRIVATE include)