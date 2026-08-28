cmake_minimum_required(VERSION 3.15)
project(AudioPlayerApp VERSION 1.0.0)

# Clang কম্পাইলার ব্যবহার করলে স্বয়ংক্রিয়ভাবে lld লিঙ্কার সেট করবে
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    #    add_link_options(-fuse-ld=lld)
    add_compile_options(-flto)
    add_link_options(-fuse-ld=mold -flto)
endif ()

# Release Mode এবং Size Optimization Flags
set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Os -flto -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections -Wl,--strip-all)

# JUCE যুক্ত করুন
add_subdirectory(JUCE)

juce_add_gui_app(AudioPlayerApp
        PRODUCT_NAME "JUCE Audio Player"
)

# -------------------------------------------------------------
# FAAD2 (libfaad) Library Linking (FFmpeg-এর বিকল্প)
# -------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(FAAD REQUIRED faad2)

# Source Files
target_sources(AudioPlayerApp PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/AacAudioFormat.cpp
)

# Header Inclusion Path
target_include_directories(AudioPlayerApp PRIVATE
        ${FAAD_INCLUDE_DIRS}
)

# Dynamic / Static Link Libraries
target_link_libraries(AudioPlayerApp PRIVATE
        ${FAAD_LIBRARIES}

        # System Libraries (Linux)
        pthread
        m

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

juce_generate_juce_header(AudioPlayerApp)
