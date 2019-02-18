cmake_minimum_required (VERSION 3.4)

project(audiorw)

set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#Adding cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/modules/)

#######################
## Internal Libraries
#######################

include_directories(include)
file(GLOB PROJECT_SOURCES src/*.cpp)

#######################
## External Libraries
#######################

# FFMPEG
find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIR})
set(LIBS ${LIBS} ${FFMPEG_LIBRARIES})

####################
## Library Creation
####################

add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
set(LIBS ${LIBS} ${PROJECT_NAME})

################
## Installation
################

install(TARGETS ${PROJECT_NAME}
        DESTINATION lib)
install(DIRECTORY include/
        DESTINATION include)

#####################################
## Unit Tests
#####################################

option(BUILD_EXAMPLES "BUILD_EXAMPLES" OFF)
if (BUILD_EXAMPLES)
  # from list of files we'll create examples example_name.cpp -> example_name
  file(GLOB EXAMPLE_SOURCES example/*.cpp)
  foreach(_example_file ${EXAMPLE_SOURCES})
      get_filename_component(_example_name ${_example_file} NAME_WE)
      add_executable(${_example_name} ${_example_file})
      target_link_libraries(${_example_name} ${LIBS})
  endforeach()
endif()
