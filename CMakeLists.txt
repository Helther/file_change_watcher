cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(project_name less_test)
project(
  project_name
  VERSION 1.0
  DESCRIPTION "тестовое задание"
  HOMEPAGE_URL "www.github.com/Helther"
  LANGUAGES CXX C)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(project_warnings INTERFACE)

include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

include_directories(src)
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE HEADERS ${SRC_DIR}/*.h*)
file(GLOB_RECURSE SOURCES ${SRC_DIR}/*.cpp)

add_executable(${project_name} ${SOURCES} ${HEADERS})
add_compile_definitions(APP_NAME="${project_name}")
target_link_libraries(${project_name} PRIVATE
        project_options
	project_warnings
)
