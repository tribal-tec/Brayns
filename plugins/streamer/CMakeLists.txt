# Copyright (c) 2015-2018, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(Streamer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HighFive QUIET REQUIRED)

set(STREAMER_SOURCES
  streamer.cpp
)

set(STREAMER_HEADERS
  streamer.h
)

add_library(braynsStreamer SHARED ${STREAMER_SOURCES} ${STREAMER_HEADERS})
target_link_libraries(braynsStreamer
  PUBLIC braynsCommon braynsPluginAPI
  PRIVATE braynsEngine avcodec avformat avutil swscale avfilter avdevice
)
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
