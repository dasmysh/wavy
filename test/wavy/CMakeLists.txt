file(RELATIVE_PATH PROJECT_REL_PATH ${PROJECT_SOURCE_DIR}/test ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB_RECURSE TEST_SRC_FILES CONFIGURE_DEPENDS
    *.h
    *.hpp
    *.inl
    *.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TEST_SRC_FILES})

add_executable(${APPLICATION_NAME}_tests ${TEST_SRC_FILES} ${TOP_FILES})
target_link_libraries(${APPLICATION_NAME}_tests PRIVATE ${APPLICATION_NAME}_warnings ${APPLICATION_NAME}_options catch_main $<TARGET_OBJECTS:${APPLICATION_NAME}_lib> fmt::fmt spdlog::spdlog cereal::cereal cppcoro::cppcoro Eigen3::Eigen glm::glm sfml-graphics ImGui-SFML::ImGui-SFML)
target_include_directories(${APPLICATION_NAME}_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(${APPLICATION_NAME}_tests PROPERTIES FOLDER "tests")
set_project_static_analyzer(${APPLICATION_NAME}_tests)

catch_discover_tests(
  ${APPLICATION_NAME}_tests
  TEST_PREFIX
  "unittests."
  EXTRA_ARGS
  -s
  --reporter=xml
  --out=${APPLICATION_NAME}_tests.xml)
