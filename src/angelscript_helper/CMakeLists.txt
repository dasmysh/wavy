file(RELATIVE_PATH PROJECT_REL_PATH ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    *.h
    *.hpp
    *.inl
    *.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

file(GLOB_RECURSE INCLUDE_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}/*.h
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}/*.hpp
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}/*.inl)
source_group(TREE ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH} FILES ${INCLUDE_FILES})


file(GLOB_RECURSE MODULE_FILES CONFIGURE_DEPENDS
    *.cppm
    *.ixx)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${MODULE_FILES})

file(GLOB TOP_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/Readme.md
    ${PROJECT_SOURCE_DIR}/.clang-tidy
    ${PROJECT_SOURCE_DIR}/.clang-format
    ${PROJECT_SOURCE_DIR}/.editorconfig
    ${CMAKE_BINARY_DIR}/glm.natvis
    ${CMAKE_BINARY_DIR}/imgui.natvis
    ${CMAKE_BINARY_DIR}/imgui.gdb
    ${CMAKE_BINARY_DIR}/imgui.natstepfilter)
source_group(" " FILES ${TOP_FILES})

add_library(angelscript_helper STATIC ${SRC_FILES} ${INCLUDE_FILES} ${TOP_FILES})
target_sources(angelscript_helper PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_FILES})
target_link_libraries(angelscript_helper PUBLIC ${APPLICATION_NAME}_options ${APPLICATION_NAME}_warnings fmt::fmt spdlog::spdlog Angelscript::angelscript)
target_include_directories(angelscript_helper PUBLIC
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(angelscript_helper PROPERTIES FOLDER "${APPLICATION_NAME}")
set_project_static_analyzer(angelscript_helper)
