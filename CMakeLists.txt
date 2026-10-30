cmake_minimum_required(VERSION 3.15)
project(AudioPlayerApp VERSION 1.0.0)

# -------------------------------------------------------------
# Compiler ও Linker Flags (Clang & Optimization)
# -------------------------------------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-flto)
    add_link_options(-fuse-ld=mold -flto)
endif ()

# Release Mode এবং Size Optimization Flags
set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Os -flto -ffunction-sections -fdata-sections)

if (NOT MSVC AND NOT WIN32)
    add_link_options(-Wl,--gc-sections -Wl,--strip-all)
endif ()

# Export compile_commands.json for Zed editor / clangd LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------------------------------------------
# FAAD2 (libfaad) FetchContent কনফিগারেশন
# -------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_SOURCE_DIR_FAAD2 "${CMAKE_BINARY_DIR}/_deps/faad2-src" CACHE PATH "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    faad2
    SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/faad2-src"
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FAAD_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_CLI OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(faad2)

# -------------------------------------------------------------
# JUCE সাবমডিউল যুক্তকরণ
# -------------------------------------------------------------
add_subdirectory(JUCE)

juce_add_gui_app(AudioPlayerApp
    PRODUCT_NAME "JUCE Audio Player"
    ICON_BIG "res/player.webp"
)

# -------------------------------------------------------------
# Binary Data for Embedded App Icon
# -------------------------------------------------------------
juce_add_binary_data(AudioPlayerResources
    SOURCES
        res/player.webp
)

# -------------------------------------------------------------
# Source Files
# -------------------------------------------------------------
target_sources(AudioPlayerApp PRIVATE
    src/Main.cpp
    src/MainComponent.cpp
    lib/AacAudioFormat.cpp
)

# -------------------------------------------------------------
# Header Inclusion Paths
# -------------------------------------------------------------
target_include_directories(AudioPlayerApp PRIVATE
    src
    lib
    ${faad2_SOURCE_DIR}/include
)

# -------------------------------------------------------------
# Libraries Linking
# -------------------------------------------------------------
target_link_libraries(AudioPlayerApp PRIVATE
    AudioPlayerResources
    faad

    # JUCE Modules
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

# Windows প্ল্যাটফর্মের জন্য অতিরিক্ত সিস্টেম লাইব্রেরি
if (WIN32)
    target_link_libraries(AudioPlayerApp PRIVATE
        mfplat
        mfreadwrite
        mfuuid
    )
endif ()

# Linux প্ল্যাটফর্মের জন্য অতিরিক্ত সিস্টেম লাইব্রেরি
if (UNIX AND NOT APPLE)
    target_link_libraries(AudioPlayerApp PRIVATE
        pthread
        m
    )
endif ()

juce_generate_juce_header(AudioPlayerApp)
