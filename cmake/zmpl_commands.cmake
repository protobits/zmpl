function(zmpl_define_topics)
  set(sections)
  foreach(topic ${ARGN})
    string(REPLACE "/" "_" section_name ${topic})
    list(APPEND sections "\t__start___zmpl_subscribers${section_name} = .\;\n\tKEEP(*(__zmpl_subscribers${section_name}))\;\n\t__stop___zmpl_subscribers${section_name} = .\;")
  endforeach()
  string(JOIN "\n" ZMPL_SUBSCRIBER_SECTIONS ${sections})

  configure_file(
    ${ZEPHYR_ZMPL_CMAKE_DIR}/cmake/zmpl_linker_script.ld.in
    ${CMAKE_CURRENT_BINARY_DIR}/zmpl_linker_script.ld @ONLY)

  zephyr_linker_sources(RWDATA ${CMAKE_CURRENT_BINARY_DIR}/zmpl_linker_script.ld)
endfunction()