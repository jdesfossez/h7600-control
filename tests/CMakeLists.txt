add_executable(h7600_tests
    ProtocolTests.cpp
    SigfileTests.cpp
    ScreenDumpTests.cpp
    RequestTrackerTests.cpp
    PresetsCatalogTests.cpp
    ParamSenderTests.cpp)
target_link_libraries(h7600_tests PRIVATE h7600_core Catch2::Catch2WithMain)
target_compile_definitions(h7600_tests PRIVATE
    H7600_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    H7600_RESOURCES_DIR="${PROJECT_SOURCE_DIR}/resources")
include(Catch)
catch_discover_tests(h7600_tests)
