cmake_minimum_required(VERSION 3.25)

project(Curvable VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CURVABLE_BUILD_TESTS "Build unit tests" ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()

add_subdirectory(libs/JUCE)

# ---------------------------------------------------------------------------
# curvable_core: 与 JUCE 无关的纯 C++ 逻辑（model / engine），便于单元测试
# ---------------------------------------------------------------------------
add_library(curvable_core STATIC
    src/engine/PitchBend.cpp
)
target_include_directories(curvable_core PUBLIC src)
target_compile_features(curvable_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(curvable_core PUBLIC /utf-8)
endif()

# ---------------------------------------------------------------------------
# 插件
# ---------------------------------------------------------------------------
set(CURVABLE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CURVABLE_FORMATS AU)
endif()

juce_add_plugin(Curvable
    PRODUCT_NAME "Curvable"
    COMPANY_NAME "asahanada"
    COMPANY_WEBSITE "https://github.com/asahanadadesu/curvable"
    BUNDLE_ID "io.github.asahanadadesu.curvable"
    # 四字符代码一经发布不要再改，否则宿主会把它当成另一个插件，旧工程将找不到它
    PLUGIN_MANUFACTURER_CODE Ashn
    PLUGIN_CODE Crvb
    FORMATS ${CURVABLE_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Curvable PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
)

target_compile_definitions(Curvable PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    CURVABLE_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(Curvable
    PRIVATE
        curvable_core
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# 测试
# ---------------------------------------------------------------------------
if(CURVABLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(libs/Catch2)
    add_subdirectory(tests)
endif()
