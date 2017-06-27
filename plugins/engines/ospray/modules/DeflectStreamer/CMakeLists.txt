# Copyright (c) 2015-2017, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

set(OSPRAY_MODULE_DEFLECT_LINK_LIBRARIES
  PUBLIC Deflect Lexis ${OSPRAY_LIBRARIES})

set(OSPRAY_MODULE_DEFLECT_SOURCES
    OSPRayPixelOperations.cpp
    DeflectStreamer.cpp
)

set(OSPRAY_MODULE_DEFLECT_PUBLIC_HEADERS
  OSPRayPixelOperations.h
)

common_library(ospray_module_deflect)
