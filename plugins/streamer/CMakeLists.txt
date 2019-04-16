# Copyright (c) 2015-2018, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(Streamer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FFMPEG REQUIRED)
find_package(Lunchbox REQUIRED)
find_package(ospray 1.7 REQUIRED)
find_package(NvPipe)

list(APPEND CMAKE_MODULE_PATH ${OSPRAY_CMAKE_ROOT})
include(macros)
OSPRAY_CONFIGURE_MPI()

set(STREAMER_SOURCES
  streamer.cpp
)

set(STREAMER_HEADERS
  streamer.h
)

add_library(braynsStreamer SHARED ${STREAMER_SOURCES} ${STREAMER_HEADERS})
target_include_directories(braynsStreamer PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(braynsStreamer
  PUBLIC braynsCommon braynsPluginAPI
  PRIVATE braynsEngine ${FFMPEG_LIBRARIES} Lunchbox ${OSPRAY_COMMON_LIBRARY} ${OSPRAY_MPI_LIBRARIES}
)
if(TARGET NvPipe)
  target_link_libraries(braynsStreamer PRIVATE NvPipe)
  target_compile_definitions(braynsStreamer PRIVATE USE_NVPIPE)
endif()
set_target_properties(braynsStreamer PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION})

install(TARGETS braynsStreamer
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

if(TARGET Brayns-all)
  add_dependencies(Brayns-all braynsStreamer)
  add_dependencies(Brayns-install braynsStreamer)
endif()
