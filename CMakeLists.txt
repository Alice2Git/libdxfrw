cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(libdxfrw VERSION 0.6.3 LANGUAGES CXX)

option(LIBDXFRW_BUILD_TESTS "Build the unit tests" ON)
option(LIBDXFRW_BUILD_TOOLS "Build the dwg2dxf and dwg2text converters" ON)
option(LIBDXFRW_STATIC_RUNTIME "MinGW: link the C/C++ runtime statically into the executables" ON)
option(LIBDXFRW_DEBUG_OUTPUT "Keep the debug messages (dxfRW/dwgR::setDebug); reading is about twice as slow" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

file(GLOB libdxfrw_sources src/*.cpp)
file(GLOB libdxfrw_intern_sources src/intern/*.cpp)
file(GLOB libdxfrw_headers src/*.h)
file(GLOB libdxfrw_intern_headers src/intern/*.h)

# The library has no external dependencies (iconv is no longer used, see DXFRW_C_PATCHES.md).
add_library(dxfrw STATIC ${libdxfrw_sources} ${libdxfrw_intern_sources})
target_include_directories(dxfrw PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)
if(NOT LIBDXFRW_DEBUG_OUTPUT)
  target_compile_definitions(dxfrw PRIVATE DRW_NO_DEBUG)
endif()
if(MSVC)
  target_compile_options(dxfrw PRIVATE /utf-8)
  target_compile_definitions(dxfrw PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS dxfrw
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
# Public headers; the intern/ headers are needed by applications that use the UTF-8 path helpers
install(FILES ${libdxfrw_headers} DESTINATION include)
install(FILES ${libdxfrw_intern_headers} DESTINATION include/intern)
install(FILES COPYING README.md DXFRW_C_PATCHES.md DESTINATION share/doc/libdxfrw)

function(libdxfrw_static_runtime target)
  if(MINGW AND LIBDXFRW_STATIC_RUNTIME)
    target_link_options(${target} PRIVATE -static $<$<CONFIG:Release>:-s>)
  endif()
endfunction()

# Converters
if(LIBDXFRW_BUILD_TOOLS)
  add_executable(dwg2dxf dwg2dxf/main.cpp dwg2dxf/dx_iface.cpp)
  target_link_libraries(dwg2dxf PRIVATE dxfrw)
  libdxfrw_static_runtime(dwg2dxf)

  add_executable(dwg2text dwg2text/main.cpp dwg2text/dx_iface.cpp)
  target_link_libraries(dwg2text PRIVATE dxfrw)
  libdxfrw_static_runtime(dwg2text)

  install(TARGETS dwg2dxf dwg2text RUNTIME DESTINATION bin)
endif()

# Tests
if(LIBDXFRW_BUILD_TESTS)
  enable_testing()
  set(libdxfrw_tests
    basic:BasicTests entities:EntityTests polylines:PolylineTests text:TextTests tables:TableTests
    blocks:BlockTests versions:VersionTests errors:ErrorTests dimensions:DimensionTests
    annotations:AnnotationTests attributes:AttributeTests class_underflow:ClassUnderflowTests
    dwg_classes:DWGClassParsingTests)
  foreach(entry ${libdxfrw_tests})
    string(REPLACE ":" ";" parts ${entry})
    list(GET parts 0 name)
    list(GET parts 1 test_name)
    add_executable(test_${name} tests/test_${name}.cpp)
    target_include_directories(test_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(test_${name} PRIVATE dxfrw)
    libdxfrw_static_runtime(test_${name})
    add_test(NAME ${test_name} COMMAND test_${name})
  endforeach()
endif()
