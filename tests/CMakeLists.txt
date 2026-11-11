function(adderall_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} PRIVATE adderall_core)
  target_compile_definitions(${name} PRIVATE
    ADDERALL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    ADDERALL_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME ${name} COMMAND ${name})
endfunction()

adderall_test(test_date)
adderall_test(test_raster)
adderall_test(test_board)
adderall_test(test_format)
adderall_test(test_store)
adderall_test(test_day)
adderall_test(test_habits)
adderall_test(test_measure)
adderall_test(test_map)
