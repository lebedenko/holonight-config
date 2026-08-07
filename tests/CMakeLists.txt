add_executable(holonight_config_tests
    test_main.cpp
    test_appearance.cpp
    test_codec.cpp
    test_path.cpp
    test_store.cpp
)
target_link_libraries(holonight_config_tests PRIVATE HoloNight::ConfigTestSupport)
target_compile_features(holonight_config_tests PRIVATE cxx_std_20)
add_test(NAME holonight_config_tests COMMAND holonight_config_tests)

configure_file(consumer_test.cmake.in consumer_test.cmake @ONLY)
add_test(NAME holonight_config_consumer_test
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/consumer_test.cmake)
