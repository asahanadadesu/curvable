# 单元测试：Catch2 + JUCE 控制台程序（可以测试依赖 JUCE 的序列化代码）
juce_add_console_app(curvable_tests PRODUCT_NAME "curvable_tests")

target_sources(curvable_tests PRIVATE
    PitchBendTests.cpp
    FLDatabaseParserTests.cpp
    ProjectTests.cpp
    SketchPlayerTests.cpp
    PlaybackEngineTests.cpp
    ScheduleExchangeTests.cpp
    ProjectSerializerTests.cpp
    NoteEditingTests.cpp
    RollViewTests.cpp
    PitchCurveTests.cpp
    VoiceAllocatorTests.cpp
    ExpressionTests.cpp
    GlideTests.cpp
    ${CURVABLE_JUCE_MODEL_SOURCES}
)

target_include_directories(curvable_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(curvable_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if(MSVC)
    target_compile_options(curvable_tests PRIVATE /utf-8)
endif()

target_link_libraries(curvable_tests
    PRIVATE
        curvable_core
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

include(${PROJECT_SOURCE_DIR}/libs/Catch2/extras/Catch.cmake)
catch_discover_tests(curvable_tests)
