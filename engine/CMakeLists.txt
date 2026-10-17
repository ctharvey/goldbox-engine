find_package(Boost 1.75 CONFIG REQUIRED)

add_library(goldbox_core STATIC
    src/boost_json.cpp
    src/json_support.cpp
    src/journal.cpp
    src/content.cpp
    src/mods.cpp
    src/game_profile.cpp
    src/ecl_vm.cpp
    src/exploration.cpp
    src/image_presentation.cpp
    src/party.cpp
    src/party_memory.cpp
)
target_include_directories(goldbox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_features(goldbox_core PUBLIC cxx_std_20)
target_link_libraries(goldbox_core PUBLIC goldbox_formats)
if(MSVC)
    target_compile_options(goldbox_core PRIVATE /W4)
else()
    target_compile_options(goldbox_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
