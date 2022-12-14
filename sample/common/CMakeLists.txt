cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})

if (NOT ${ENVSDK} STREQUAL "")
    # Convert path from Windows
    file(TO_CMAKE_PATH ${ENVSDK} SDK)
else()
    execute_process(
        COMMAND bash -c "egrep '^\\s*SDKRoot' $HOME/.Playdate/config"
        COMMAND head -n 1
        COMMAND cut -c9-
        OUTPUT_VARIABLE SDK
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if (NOT EXISTS ${SDK})
    message(FATAL_ERROR "SDK Path not found; set ENV value PLAYDATE_SDK_PATH")
    return()
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

set(PLAYDATE_GAME_NAME ${GAME_NAME})
set(PLAYDATE_GAME_DEVICE ${GAME_NAME}_DEVICE)

project(${PLAYDATE_GAME_NAME} C ASM)

function(print_list)
  foreach(msg ${ARGV})
    message("${msg}")
  endforeach()
endfunction()

file(GLOB_RECURSE SOURCE RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src" src/*.c ../../src/*.c ../common/*.c)
list(TRANSFORM SOURCE PREPEND "src/")

if (TOOLCHAIN STREQUAL "armgcc")
    add_executable(${PLAYDATE_GAME_DEVICE} ${SDK}/C_API/buildsupport/setup.c ${SOURCE})
    target_compile_options(${PLAYDATE_GAME_DEVICE} PUBLIC -save-temps=obj -v)
    target_include_directories(${PLAYDATE_GAME_DEVICE} PUBLIC ../../src ../common)
else()
    add_library(${PLAYDATE_GAME_NAME} SHARED ${SOURCE})
    #target_compile_options(${PLAYDATE_GAME_NAME} PUBLIC -S)
    target_include_directories(${PLAYDATE_GAME_NAME} PUBLIC ../../src ../common)

    enable_testing()

    list(FILTER SOURCE EXCLUDE REGEX "main\\.c$")
    file(GLOB TEST_SOURCE test/*.c)

    add_compile_options(-D UNIT_TEST)
    foreach(test_src IN ITEMS ${TEST_SOURCE})
        get_filename_component(TEST_NAME ${test_src} NAME_WE)
        add_executable(${TEST_NAME} ${SOURCE} ${test_src})
        target_include_directories(${TEST_NAME} PUBLIC src)
        add_test(NAME ${test_src} COMMAND ${TEST_NAME})
    endforeach()
endif()


include(${SDK}/C_API/buildsupport/playdate_game.cmake)

