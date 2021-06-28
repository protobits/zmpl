# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../)

find_package(Zephyr HINTS CMAKE_CURRENT_LIST_DIR/../zephyr $ENV{ZEPHYR_BASE})
project(zmpl_test)
target_sources(app PRIVATE src/main.c)