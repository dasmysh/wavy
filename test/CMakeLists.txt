file(GLOB TOP_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/.runsettings)
source_group(" " FILES ${TOP_FILES})
add_library(catch_main STATIC catch_main.cpp ${TOP_FILES})
target_link_libraries(catch_main PUBLIC Catch2::Catch2 Catch2::Catch2WithMain)

add_executable(tests tests.cpp ${TOP_FILES})
target_link_libraries(tests PRIVATE ${APPLICATION_NAME}_warnings ${APPLICATION_NAME}_options catch_main)
set_project_static_analyzer(tests)

include(CTest)
include(Catch)

# automatically discover tests that are defined in catch based test files you
# can modify the unittests. TEST_PREFIX to whatever you want, or use different
# for different binaries
catch_discover_tests(
  tests
  TEST_PREFIX
  "unittests."
  EXTRA_ARGS
  -s
  --reporter=xml
  --out=tests.xml)

# Add a file containing a set of constexpr tests
add_executable(constexpr_tests constexpr_tests.cpp)
target_link_libraries(constexpr_tests ${APPLICATION_NAME}_warnings ${APPLICATION_NAME}_options catch_main)
set_project_static_analyzer(constexpr_tests)

catch_discover_tests(
  constexpr_tests
  TEST_PREFIX
  "constexpr."
  EXTRA_ARGS
  -s
  --reporter=xml
  --out=constexpr.xml)

# Disable the constexpr portion of the test, and build again this allows us to
# have an executable that we can debug when things go wrong with the constexpr
# testing
add_executable(relaxed_constexpr_tests constexpr_tests.cpp)
target_link_libraries(relaxed_constexpr_tests PRIVATE ${APPLICATION_NAME}_warnings ${APPLICATION_NAME}_options catch_main)
target_compile_definitions(relaxed_constexpr_tests PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE)
set_project_static_analyzer(relaxed_constexpr_tests)

catch_discover_tests(
  relaxed_constexpr_tests
  TEST_PREFIX
  "relaxed_constexpr."
  EXTRA_ARGS
  -s
  --reporter=xml
  --out=relaxed_constexpr.xml)
