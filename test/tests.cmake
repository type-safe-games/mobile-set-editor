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
