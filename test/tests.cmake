# Use CTest for tests
enable_testing()
set(test_dir "${PROJECT_SOURCE_DIR}/test")
set(CTEST_OUTPUT_ON_FAILURE 1)

# Validate locales
file(GLOB locales data/*.mse-locale)
add_test(
  NAME "validate-locales"
  COMMAND perl "${test_dir}/locale/validate_locale.pl" "${PROJECT_SOURCE_DIR}/src" ${locales}
)

# Scripting language tests
add_test(
  NAME script-functions
  COMMAND magicseteditor ${test_dir}/script/script-functions.mse-script
)

# Rendering tests
# TODO

# Test fixture (ADR 0008): a tiny free Game and Style that public CI can render.
# Tests see only the fixture's bundled fonts and template packages.
set(FIXTURE_FONTS_DIR "${test_dir}/fixture/fonts")
set(FIXTURE_FONT_CACHE_DIR "${PROJECT_BINARY_DIR}/fixture-fontconfig-cache")
configure_file("${test_dir}/fixture/fonts.conf.in" "${PROJECT_BINARY_DIR}/fixture-fonts.conf" @ONLY)
set(fixture_env
  "FONTCONFIG_FILE=${PROJECT_BINARY_DIR}/fixture-fonts.conf"
  "MSE_TEMPLATES_DIR=${test_dir}/fixture/data"
)
set(fixture_set "${test_dir}/fixture/sets/fixture-cards.mse-set")

add_test(
  NAME fixture-render
  COMMAND ${CMAKE_COMMAND} -DMSE=$<TARGET_FILE:magicseteditor> -DSET=${fixture_set}
          -DCOMMANDS=${test_dir}/fixture/render-all.mse-cli -DWORKDIR=${PROJECT_BINARY_DIR}/test-output
          -P ${test_dir}/run_cli.cmake
)
set_tests_properties(fixture-render PROPERTIES ENVIRONMENT "${fixture_env}")

# Layout dump (ADR 0010): deterministic, and read from the drawing pass
add_test(
  NAME layout-dump
  COMMAND ${CMAKE_COMMAND} -DMSE=$<TARGET_FILE:magicseteditor> -DSET=${fixture_set}
          -DCOMMANDS=${test_dir}/layout/layout-dump.mse-cli -DWORKDIR=${PROJECT_BINARY_DIR}/test-output
          -P ${test_dir}/run_cli.cmake
)
set_tests_properties(layout-dump PROPERTIES ENVIRONMENT "${fixture_env}")

# The facade (ADR 0009), headless on the desktop host, against the fixture. Renders are also compared
# with the fixture's Golden images when the private goldens are there (ADR 0008, ADR 0010).
set(FIXTURE_GOLDENS_DIR "${PROJECT_SOURCE_DIR}/templates/goldens/images/fixture/fixture-cards"
    CACHE PATH "Golden images of the Test fixture (optional)")
add_test(
  NAME facade
  COMMAND facade_tests ${test_dir}/fixture ${PROJECT_BINARY_DIR}/test-output/facade ${FIXTURE_GOLDENS_DIR}
)
set_tests_properties(facade PROPERTIES ENVIRONMENT "${fixture_env}")
