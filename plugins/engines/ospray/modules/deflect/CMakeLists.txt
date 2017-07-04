# Copyright (c) 2017, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Daniel Nachbaur <daniel.nachbaur@epfl.ch>
#
# This file is part of DeflectPixelOp <https://github.com/BlueBrain/DeflectPixelOp>

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(DeflectPixelOp VERSION 1.0.0)
set(DeflectPixelOp_VERSION_ABI 1)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
  ${CMAKE_SOURCE_DIR}/CMake/common ${PROJECT_SOURCE_DIR}/Pydoxine/CMake)
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/CMake/common/Common.cmake)
  message(FATAL_ERROR "CMake/common missing, run: git submodule update --init")
endif()

include(Common)

common_find_package(Deflect REQUIRED)
common_find_package(OpenMP)
common_find_package(ospray REQUIRED SYSTEM)
common_find_package_post()

set(OSPRAY_MODULE_DEFLECT_OMIT_LIBRARY_HEADER ON)
set(OSPRAY_MODULE_DEFLECT_OMIT_VERSION_HEADERS ON)
set(OSPRAY_MODULE_DEFLECT_HEADERS DeflectPixelOp.h)
set(OSPRAY_MODULE_DEFLECT_SOURCES DeflectPixelOp.cpp ModuleInit.cpp)
set(OSPRAY_MODULE_DEFLECT_LINK_LIBRARIES PUBLIC Deflect ${OSPRAY_LIBRARIES})
common_library(ospray_module_deflect)

set(DOXYGEN_MAINPAGE_MD README.md)
set(DOXYGEN_EXTRA_INPUT "${PROJECT_SOURCE_DIR}/README.md")
include(DoxygenRule)
