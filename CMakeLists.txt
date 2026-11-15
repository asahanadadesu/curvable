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
    src/model/Project.cpp
    src/model/NoteEditing.cpp
    src/model/PitchCurve.cpp
    src/engine/PitchBend.cpp
    src/engine/Schedule.cpp
    src/engine/SketchPlayer.cpp
    src/engine/PlaybackEngine.cpp
    src/engine/VoiceAllocator.cpp
    src/hosting/FLDatabaseParser.cpp
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

# 依赖 JUCE 的模型代码（插件本体与单元测试共用）
set(CURVABLE_JUCE_MODEL_SOURCES
    ${PROJECT_SOURCE_DIR}/src/model/ProjectSerializer.cpp
    ${PROJECT_SOURCE_DIR}/src/model/CurvePresetLibrary.cpp
    ${PROJECT_SOURCE_DIR}/src/io/MidiExchange.cpp
    ${PROJECT_SOURCE_DIR}/src/i18n/Translations.cpp
)

# 托管其他插件的代码（插件本体与托管冒烟测试共用）
set(CURVABLE_HOSTING_SOURCES
    src/hosting/HostingContext.cpp
    src/hosting/HostedInstrument.cpp
)

set(CURVABLE_JUCE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>
    CURVABLE_VERSION_STRING="${PROJECT_VERSION}"
)

target_sources(Curvable PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/AutomationSlots.cpp
    src/ui/SlotsPanel.cpp
    src/ui/SynthBrowser.cpp
    src/ui/Theme.cpp
    src/ui/CurvableLookAndFeel.cpp
    src/ui/PianoRoll.cpp
    ${CURVABLE_JUCE_MODEL_SOURCES}
    ${CURVABLE_HOSTING_SOURCES}
)

target_compile_definitions(Curvable PUBLIC ${CURVABLE_JUCE_DEFINITIONS})

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

# ---------------------------------------------------------------------------
# 托管冒烟测试：加载一个真实的音源插件，检查发声与状态保存/恢复
# 用法：curvable_hosting_smoke <插件路径>
# 配置时指定 -DCURVABLE_SMOKE_PLUGIN=<插件路径> 则注册为 ctest 测试
# ---------------------------------------------------------------------------
option(CURVABLE_BUILD_SMOKE_TEST "Build the plugin-hosting smoke test" ON)
set(CURVABLE_SMOKE_PLUGIN "" CACHE FILEPATH "Synth plugin used by the hosting smoke test (optional)")

if(CURVABLE_BUILD_SMOKE_TEST)
    juce_add_console_app(curvable_hosting_smoke PRODUCT_NAME "curvable_hosting_smoke")
    target_sources(curvable_hosting_smoke PRIVATE
        tests/smoke/HostingSmokeTest.cpp
        ${CURVABLE_HOSTING_SOURCES}
    )
    target_include_directories(curvable_hosting_smoke PRIVATE src)
    target_compile_definitions(curvable_hosting_smoke PRIVATE
        ${CURVABLE_JUCE_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1
    )
    target_link_libraries(curvable_hosting_smoke
        PRIVATE
            curvable_core
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
    if(MSVC)
        target_compile_options(curvable_hosting_smoke PRIVATE /utf-8)
    endif()

    if(CURVABLE_BUILD_TESTS AND CURVABLE_SMOKE_PLUGIN)
        add_test(NAME hosting_smoke COMMAND curvable_hosting_smoke "${CURVABLE_SMOKE_PLUGIN}")
    endif()
endif()
