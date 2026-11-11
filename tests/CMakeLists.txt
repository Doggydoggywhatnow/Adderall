function(adderall_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} PRIVATE adderall_core)
  target_compile_definitions(${name} PRIVATE
    ADDERALL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
  add_test(NAME ${name} COMMAND ${name})
endfunction()

adderall_test(test_date)
adderall_test(test_raster)
adderall_test(test_board)
