cmake_minimum_required(VERSION 3.25)
project(HoloNightConfigConsumer LANGUAGES CXX)

find_package(HoloNightConfig CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE HoloNight::Config)
target_compile_features(consumer PRIVATE cxx_std_20)
