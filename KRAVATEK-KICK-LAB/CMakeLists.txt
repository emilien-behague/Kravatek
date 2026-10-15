cmake_minimum_required(VERSION 3.22)
project(KravatekKickLab VERSION 0.2.0 LANGUAGES C CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(KRAVATEK_BUILD_APP "Build the JUCE standalone application" ON)
option(KRAVATEK_BUILD_TESTS "Build DSP and playback tests" ON)
option(KRAVATEK_ENABLE_SANITIZERS "Enable ASan and UBSan with GCC/Clang" OFF)
set(KRAVATEK_JUCE_PATH "" CACHE PATH "Existing checkout of the pinned JUCE version")

find_package(Threads REQUIRED)
add_library(kravatek_dsp STATIC
    src/dsp/KickSynth.cpp src/dsp/Analysis.cpp src/dsp/Variations.cpp src/dsp/Playback.cpp src/dsp/WavWriter.cpp
    src/genetic/Genome.cpp src/genetic/LayerRenderer.cpp src/genetic/Generator.cpp src/genetic/Features.cpp)
target_include_directories(kravatek_dsp PUBLIC src)
target_link_libraries(kravatek_dsp PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(kravatek_dsp PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(kravatek_dsp PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
if(KRAVATEK_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(kravatek_dsp PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(kravatek_dsp PUBLIC -fsanitize=address,undefined)
endif()

add_executable(kravatek-render src/cli/Main.cpp)
target_link_libraries(kravatek-render PRIVATE kravatek_dsp)

if(KRAVATEK_BUILD_TESTS)
    enable_testing()
    add_executable(kravatek-tests tests/DspTests.cpp)
    target_link_libraries(kravatek-tests PRIVATE kravatek_dsp)
    add_executable(krava-genetic-tests tests/GeneticTests.cpp)
    target_link_libraries(krava-genetic-tests PRIVATE kravatek_dsp)
    foreach(check IN ITEMS genetic-identity genetic-prng genetic-layers genetic-modes genetic-measures genetic-100)
        add_test(NAME ${check} COMMAND krava-genetic-tests ${check})
        set_tests_properties(${check} PROPERTIES TIMEOUT 300 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/test-output")
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-output")
    foreach(check IN ITEMS oscillator pitch reference reproducibility variations sample-rates
            invalid-input envelope wav playback retrigger concurrency sound-families dna-locks saturation looping parameter-effects filter-stability exploration analysis)
        add_test(NAME ${check} COMMAND kravatek-tests ${check})
        set_tests_properties(${check} PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/test-output" TIMEOUT 60)
    endforeach()
    add_test(NAME cli-export COMMAND kravatek-render --seed 123456789 --pitch-end 52 --decay 320
        --output "${CMAKE_BINARY_DIR}/test-output/cli-kick.wav")
    add_test(NAME cli-invalid COMMAND kravatek-render --sample-rate nan)
    set_tests_properties(cli-invalid PROPERTIES WILL_FAIL TRUE)
endif()

if(KRAVATEK_BUILD_APP)
    if(MINGW)
        message(FATAL_ERROR "JUCE 8.0.16 does not support MinGW. Use Visual Studio 2022 / MSVC for the Windows app.")
    endif()
    # Modules-only uses ordinary CMake targets and avoids a host-tool bootstrap
    # when cross-compiling. No generated JuceHeader or Projucer project is needed.
    set(JUCE_MODULES_ONLY ON CACHE BOOL "Use JUCE as CMake modules" FORCE)
    if(KRAVATEK_JUCE_PATH)
        add_subdirectory("${KRAVATEK_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ea77444750de45e036e0279bb0291144bf8e3688)
        FetchContent_MakeAvailable(JUCE)
    endif()
    add_executable(KravatekKickLab WIN32 src/app/Main.cpp src/app/MainComponent.cpp
        src/app/EnvelopeEditor.cpp src/app/PresetState.cpp)
    if(WIN32)
        target_sources(KravatekKickLab PRIVATE src/app/Windows.rc)
    endif()
    target_compile_definitions(KravatekKickLab PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_JACK=0
        JUCE_APPLICATION_NAME_STRING="KRAVATEK KICK LAB"
        JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(KravatekKickLab PRIVATE kravatek_dsp
        juce::juce_gui_basics juce::juce_audio_devices)
    if(MSVC)
        target_compile_options(KravatekKickLab PRIVATE /utf-8 /Zc:__cplusplus)
    endif()
    install(TARGETS KravatekKickLab kravatek-render RUNTIME DESTINATION .)
endif()
