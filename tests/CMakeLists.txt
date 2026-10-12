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
