zephyr_library()
zephyr_include_directories(include)
zephyr_library_sources(src/zmpl.c)

zephyr_linker_sources(RODATA include/zmpl_ro.ld)
