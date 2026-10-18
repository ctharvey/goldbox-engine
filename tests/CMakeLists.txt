add_executable(goldbox_tests journal_test.cpp)
add_executable(goldbox_audio_tests audio_test.cpp)
target_link_libraries(goldbox_audio_tests PRIVATE goldbox_formats)
if(GOLDBOX_BUILD_RENDERER)
    target_include_directories(goldbox_audio_tests PRIVATE ${PROJECT_SOURCE_DIR}/runtime)
    target_link_libraries(goldbox_audio_tests PRIVATE SDL3::SDL3)
    target_compile_definitions(goldbox_audio_tests PRIVATE GOLDBOX_RENDERER)
endif()
add_test(NAME goldbox_audio_tests COMMAND goldbox_audio_tests)
add_executable(goldbox_party_tests party_test.cpp)
target_link_libraries(goldbox_party_tests PRIVATE goldbox_core)
add_test(NAME goldbox_party_tests COMMAND goldbox_party_tests
    ${PROJECT_SOURCE_DIR}/rules/adnd-first-edition-creation.json)
target_link_libraries(goldbox_tests PRIVATE goldbox_core)
add_test(NAME goldbox_journal_test COMMAND goldbox_tests)

add_executable(goldbox_mod_tests mod_loader_test.cpp)
target_link_libraries(goldbox_mod_tests PRIVATE goldbox_core)
add_test(NAME goldbox_mod_loader_test COMMAND goldbox_mod_tests)

add_test(NAME goldbox_cli_enhanced
    COMMAND goldbox_cli
        --content ${PROJECT_SOURCE_DIR}/examples/content/demo-campaign/journals.json
        --mods ${PROJECT_SOURCE_DIR}/examples/mods
        --journal 17 --mode enhanced)
set_tests_properties(goldbox_cli_enhanced PROPERTIES PASS_REGULAR_EXPRESSION "Old Tower \\(Enhanced\\)")

add_test(NAME goldbox_cli_classic
    COMMAND goldbox_cli
        --content ${PROJECT_SOURCE_DIR}/examples/content/demo-campaign/journals.json
        --mods ${PROJECT_SOURCE_DIR}/examples/mods
        --journal 17 --mode classic)
set_tests_properties(goldbox_cli_classic PROPERTIES PASS_REGULAR_EXPRESSION "A locked door blocks the passage")

add_executable(goldbox_dax_tests dax_test.cpp)
target_link_libraries(goldbox_dax_tests PRIVATE goldbox_formats)
add_test(NAME goldbox_dax_tests COMMAND goldbox_dax_tests)

add_executable(goldbox_game_profile_tests game_profile_test.cpp)
target_link_libraries(goldbox_game_profile_tests PRIVATE goldbox_core)
add_test(NAME goldbox_game_profile_tests COMMAND goldbox_game_profile_tests
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json
    ${PROJECT_SOURCE_DIR}/games/curse-of-the-azure-bonds/profile.json)

add_executable(goldbox_ecl_tests ecl_test.cpp)
target_link_libraries(goldbox_ecl_tests PRIVATE goldbox_formats)
add_test(NAME goldbox_ecl_tests COMMAND goldbox_ecl_tests)

add_executable(goldbox_geo_tests geo_test.cpp)
target_link_libraries(goldbox_geo_tests PRIVATE goldbox_formats)
add_test(NAME goldbox_geo_tests COMMAND goldbox_geo_tests)
add_executable(goldbox_graphics_tests graphics_test.cpp)
target_link_libraries(goldbox_graphics_tests PRIVATE goldbox_formats)
add_test(NAME goldbox_graphics_tests COMMAND goldbox_graphics_tests)
add_executable(goldbox_image_sequence_tests image_sequence_test.cpp)
target_link_libraries(goldbox_image_sequence_tests PRIVATE goldbox_formats)
add_test(NAME goldbox_image_sequence_tests COMMAND goldbox_image_sequence_tests)
add_executable(goldbox_image_host_tests image_host_test.cpp)
target_link_libraries(goldbox_image_host_tests PRIVATE goldbox_core)
if(GOLDBOX_BUILD_RENDERER)
    target_link_libraries(goldbox_image_host_tests PRIVATE goldbox_runtime SDL3::SDL3)
    target_compile_definitions(goldbox_image_host_tests PRIVATE GOLDBOX_RENDERER=1)
endif()
add_test(NAME goldbox_image_host_tests COMMAND goldbox_image_host_tests)
set_tests_properties(goldbox_image_host_tests PROPERTIES TIMEOUT 20
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME goldbox_rng_debug_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rng_debug_test.py $<TARGET_FILE:goldbox_rng_probe>)
set_tests_properties(goldbox_rng_debug_tests PROPERTIES TIMEOUT 30)
add_executable(goldbox_gameplay_random_tests gameplay_random_test.cpp)
find_package(Boost 1.75 CONFIG REQUIRED)
target_include_directories(goldbox_gameplay_random_tests PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(goldbox_gameplay_random_tests PRIVATE goldbox_core)
add_test(NAME goldbox_gameplay_random_tests COMMAND goldbox_gameplay_random_tests
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
set_tests_properties(goldbox_gameplay_random_tests PROPERTIES TIMEOUT 20)
add_test(NAME goldbox_rng_original_integration_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rng_original_integration_test.py $<TARGET_FILE:goldbox_rng_probe>)
set_tests_properties(goldbox_rng_original_integration_tests PROPERTIES
    TIMEOUT 120 SKIP_RETURN_CODE 77 LABELS "private-assets")
add_executable(goldbox_party_memory_tests party_memory_test.cpp)
target_link_libraries(goldbox_party_memory_tests PRIVATE goldbox_core)
add_test(NAME goldbox_party_memory_tests COMMAND goldbox_party_memory_tests
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
if(GOLDBOX_BUILD_RENDERER)
    add_executable(goldbox_renderer_tests renderer_test.cpp)
    target_link_libraries(goldbox_renderer_tests PRIVATE goldbox_runtime SDL3::SDL3)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(goldbox_renderer_tests PRIVATE GOLDBOX_RENDER_CALL_TESTS=1)
        target_link_options(goldbox_renderer_tests PRIVATE
            "-Wl,--wrap=SDL_RenderPresent" "-Wl,--wrap=SDL_SetTextureColorMod")
    endif()
    add_test(NAME goldbox_renderer_tests COMMAND goldbox_renderer_tests ${CMAKE_CURRENT_BINARY_DIR}/renderer-save.json)
    set_tests_properties(goldbox_renderer_tests PROPERTIES TIMEOUT 20
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()
add_executable(goldbox_ecl_dispatch_tests ecl_dispatch_test.cpp)
target_link_libraries(goldbox_ecl_dispatch_tests PRIVATE goldbox_formats)
add_test(NAME goldbox_ecl_dispatch_tests COMMAND goldbox_ecl_dispatch_tests)
add_executable(goldbox_ecl_vm_tests ecl_vm_test.cpp)
target_link_libraries(goldbox_ecl_vm_tests PRIVATE goldbox_core)
add_test(NAME goldbox_ecl_vm_tests COMMAND goldbox_ecl_vm_tests)
add_executable(goldbox_exploration_tests exploration_test.cpp)
target_link_libraries(goldbox_exploration_tests PRIVATE goldbox_core)
add_test(NAME goldbox_exploration_tests COMMAND goldbox_exploration_tests)
add_executable(goldbox_campaign_startup_tests campaign_startup_test.cpp)
target_link_libraries(goldbox_campaign_startup_tests PRIVATE goldbox_core)
add_test(NAME goldbox_campaign_startup_tests COMMAND goldbox_campaign_startup_tests
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
set_tests_properties(goldbox_campaign_startup_tests PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
add_test(NAME goldbox_pool_startup_reference_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/pool_startup_reference_test.py)
set_tests_properties(goldbox_pool_startup_reference_tests PROPERTIES TIMEOUT 10)
add_test(NAME goldbox_exploration_cli_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/exploration_cli_test.py $<TARGET_FILE:goldbox_explore>
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
add_test(NAME goldbox_harness_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/harness_test.py $<TARGET_FILE:goldbox_explore>
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
set_tests_properties(goldbox_harness_tests PROPERTIES TIMEOUT 120)
add_test(NAME goldbox_compare_game_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/compare_game_test.py $<TARGET_FILE:goldbox_explore>
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
set_tests_properties(goldbox_compare_game_tests PROPERTIES TIMEOUT 300)
add_test(NAME goldbox_scripted_harness_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/scripted_harness_test.py $<TARGET_FILE:goldbox_explore>
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
set_tests_properties(goldbox_scripted_harness_tests PROPERTIES TIMEOUT 120)
add_test(NAME goldbox_reference_input_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/reference_input_test.py)
set_tests_properties(goldbox_reference_input_tests PROPERTIES TIMEOUT 120)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME goldbox_wsl_supervisor_tests COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/wsl_engine_test.py)
    set_tests_properties(goldbox_wsl_supervisor_tests PROPERTIES TIMEOUT 120)
endif()
add_test(NAME goldbox_installation_scanner_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/installation_scanner_test.py $<TARGET_FILE:goldbox_cli>
    ${PROJECT_SOURCE_DIR}/games/pool-of-radiance/profile.json)
