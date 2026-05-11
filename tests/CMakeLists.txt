find_package(GTest REQUIRED)

function(librtsp_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name}
    PRIVATE
      ${PROJECT_NAME}
      GTest::gtest
      GTest::gtest_main
  )
  add_test(NAME ${name} COMMAND ${name})
endfunction()

librtsp_add_test(event_loop_test)
librtsp_add_test(timer_test)
