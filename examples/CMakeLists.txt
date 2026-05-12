foreach(_example test_pattern_server test_pattern_client)
  add_executable(${_example} ${_example}.cpp)
  target_link_libraries(${_example} PRIVATE ${PROJECT_NAME})
endforeach()
