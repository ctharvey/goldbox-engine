add_executable(goldbox_tests journal_test.cpp)
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
