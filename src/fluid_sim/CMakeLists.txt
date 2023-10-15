file(RELATIVE_PATH PROJECT_REL_PATH ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    *.h
    *.hpp
    *.inl
    *.cpp)
list(REMOVE_ITEM SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)


file(GLOB_RECURSE INCLUDE_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}/*.h
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}/*.hpp
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}/*.inl)
source_group(TREE ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH} FILES ${INCLUDE_FILES})

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

add_library(${APPLICATION_NAME}_lib OBJECT ${SRC_FILES} ${INCLUDE_FILES} ${EXTERN_SOURCES} ${TOP_FILES})
target_link_libraries(${APPLICATION_NAME}_lib PUBLIC ${APPLICATION_NAME}_options ${APPLICATION_NAME}_warnings fmt::fmt spdlog::spdlog cereal::cereal glm::glm imgui::imgui)
target_link_libraries(${APPLICATION_NAME}_lib PRIVATE glfw)
target_include_directories(${APPLICATION_NAME}_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(${APPLICATION_NAME}_lib PROPERTIES FOLDER "${APPLICATION_NAME}")
set_project_static_analyzer(${APPLICATION_NAME}_lib)


add_executable(${APPLICATION_NAME} ${TOP_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(${APPLICATION_NAME} PUBLIC ${APPLICATION_NAME}_options ${APPLICATION_NAME}_warnings $<TARGET_OBJECTS:${APPLICATION_NAME}_lib> fmt::fmt spdlog::spdlog cereal::cereal glm::glm imgui::imgui)
target_link_libraries(${APPLICATION_NAME} PRIVATE glfw)
target_include_directories(${APPLICATION_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include/${PROJECT_REL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR})
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APPLICATION_NAME})
set_target_properties(${APPLICATION_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_target_properties(${APPLICATION_NAME} PROPERTIES FOLDER "${APPLICATION_NAME}")
set_project_static_analyzer(${APPLICATION_NAME})
