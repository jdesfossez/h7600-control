add_library(h7600_core STATIC
    src/Protocol.cpp
    src/Sigfile.cpp
    src/ScreenDump.cpp
    src/RequestTracker.cpp
    src/PresetCatalog.cpp
    src/ParamSender.cpp
    src/ModuleDb.cpp
    src/Patch.cpp)
target_include_directories(h7600_core PUBLIC include)
target_link_libraries(h7600_core PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(h7600_core PUBLIC cxx_std_20)
set_target_properties(h7600_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(h7600_core PRIVATE -Wall -Wextra)
endif()
