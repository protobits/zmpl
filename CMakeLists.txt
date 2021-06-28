zephyr_library()
zephyr_include_directories(include)
zephyr_library_sources(src/zmpl.c)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/zmpl_commands.cmake)

zephyr_linker_sources(RODATA include/zmpl_ro.ld)