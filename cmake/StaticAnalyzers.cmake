option(${NAMESPACE}_ENABLE_CPPCHECK "Enable static analysis with cppcheck" OFF)
option(${NAMESPACE}_ENABLE_CLANG_TIDY "Enable static analysis with clang-tidy" OFF)
cmake_dependent_option(${NAMESPACE}_ENABLE_VS_STATICANALYSIS "Enable Visual Studio static analyzer" OFF "MSVC" OFF)
if(${NAMESPACE}_ENABLE_CPPCHECK)
  find_program(${NAMESPACE}_CPPCHECK cppcheck)
  if(${NAMESPACE}_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${CPPCHECK} --suppress=missingInclude --enable=all
                           --inconclusive -i ${CMAKE_SOURCE_DIR}/imgui/lib)
  else()
    message(SEND_ERROR "cppcheck requested but executable not found")
  endif()
endif()

if(${NAMESPACE}_ENABLE_CLANG_TIDY)
  find_program(CLANGTIDY clang-tidy)
  if(CLANGTIDY)
    set(CMAKE_CXX_CLANG_TIDY ${CLANGTIDY};-header-filter=.;)
  else()
    if (NOT MSVC)
      message(SEND_ERROR "clang-tidy requested but executable not found")
    endif()
  endif()
endif()


function(set_project_static_analyzer project_name)
  if (MSVC AND (${NAMESPACE}_ENABLE_CLANG_TIDY OR ${NAMESPACE}_ENABLE_VS_STATICANALYSIS))
    set_target_properties(${project_name} PROPERTIES VS_GLOBAL_RunCodeAnalysis true)
  endif()

  if (MSVC AND ${NAMESPACE}_ENABLE_CLANG_TIDY)
    set_target_properties(${project_name} PROPERTIES
      VS_GLOBAL_EnableClangTidyCodeAnalysis true
      VS_GLOBAL_ClangTidyChecks -header-filter=.)
  endif()

  if (MSVC AND ${NAMESPACE}_ENABLE_VS_STATICANALYSIS)
    set_target_properties(${project_name} PROPERTIES VS_GLOBAL_EnableMicrosoftCodeAnalysis true)
  endif()
endfunction()
