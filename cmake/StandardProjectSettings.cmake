# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(
    STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui, ccmake
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

find_program(CCACHE ccache)
if(CCACHE)
  message("using ccache")
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
else()
  message("ccache not found cannot use")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_IPO
       "Enable Iterprocedural Optimization, aka Link Time Optimization (LTO)"
       OFF)

if(ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result OUTPUT output)
  if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(SEND_ERROR "IPO is not supported: ${output}")
  endif()
endif()

function(set_project_options project_name)
  option(${NAMESPACE}_ENABLE_AVX "Enable AVX optimization for release build." OFF)
  option(${NAMESPACE}_ENABLE_AVX2 "Enable AVX2 optimization for release build." OFF)

  if(${NAMESPACE}_ENABLE_AVX2)
    set(MSVC_VECTOR_OPTIMIZATIONS /arch:AVX2)
    set(CLANG_VECTOR_OPTIMIZATIONS -mavx2)
  elseif(${NAMESPACE}_ENABLE_AVX)
    set(MSVC_VECTOR_OPTIMIZATIONS /arch:AVX)
    set(CLANG_VECTOR_OPTIMIZATIONS -mavx)
  endif()

  if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.14)
    set(PROJECT_OPTIONS ${MSVC_VECTOR_OPTIMIZATIONS} /external:W0 /external:anglebrackets /analyze:external-)
    set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "/external:I ")
  elseif(MSVC)
    set(PROJECT_OPTIONS ${MSVC_VECTOR_OPTIMIZATIONS})
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(PROJECT_OPTIONS ${CLANG_VECTOR_OPTIMIZATIONS})
  else()
    set(PROJECT_OPTIONS ${CLANG_VECTOR_OPTIMIZATIONS})
  endif()

  target_compile_options(${project_name} INTERFACE ${PROJECT_OPTIONS})
endfunction()

