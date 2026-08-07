cmake_minimum_required(VERSION 3.25)

project(HoloNightConfig VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(BUILD_TESTING "Build tests" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TOMLPLUSPLUS REQUIRED IMPORTED_TARGET tomlplusplus)

add_library(holonight_config SHARED
    src/appearance.cpp
    src/codec.cpp
    src/path.cpp
    src/store.cpp
)
add_library(HoloNight::Config ALIAS holonight_config)

target_compile_features(holonight_config PUBLIC cxx_std_20)
target_include_directories(holonight_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(holonight_config PRIVATE PkgConfig::TOMLPLUSPLUS)
set_target_properties(holonight_config PROPERTIES
    EXPORT_NAME Config
    POSITION_INDEPENDENT_CODE ON
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(holonight_config PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

install(TARGETS holonight_config
    EXPORT HoloNightConfigTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN "test_support.h" EXCLUDE
)
install(EXPORT HoloNightConfigTargets
    FILE HoloNightConfigTargets.cmake
    NAMESPACE HoloNight::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HoloNightConfig
)

configure_package_config_file(
    cmake/HoloNightConfigConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightConfigConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HoloNightConfig
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightConfigConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightConfigConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HoloNightConfigConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HoloNightConfig
)

if(BUILD_TESTING)
    enable_testing()
    add_library(holonight_config_test_support STATIC src/test_support.cpp)
    add_library(HoloNight::ConfigTestSupport ALIAS holonight_config_test_support)
    target_compile_features(holonight_config_test_support PUBLIC cxx_std_20)
    target_include_directories(holonight_config_test_support
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(holonight_config_test_support PUBLIC holonight_config)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(holonight_config_test_support PRIVATE
            -Wall -Wextra -Wpedantic -Werror
        )
    endif()
    add_subdirectory(tests)
endif()

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE HOLONIGHT_CONFIG_FORMAT_FILES CONFIGURE_DEPENDS
        include/*.h src/*.cpp tests/*.cpp)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${HOLONIGHT_CONFIG_FORMAT_FILES}
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${HOLONIGHT_CONFIG_FORMAT_FILES}
        VERBATIM
    )
endif()
