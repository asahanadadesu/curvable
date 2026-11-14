add_executable(curvable_tests
    PitchBendTests.cpp
    FLDatabaseParserTests.cpp
)
target_link_libraries(curvable_tests PRIVATE curvable_core Catch2::Catch2WithMain)

include(${PROJECT_SOURCE_DIR}/libs/Catch2/extras/Catch.cmake)
catch_discover_tests(curvable_tests)
