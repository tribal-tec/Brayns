# Copyright (c) 2015-2017, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

set(OSPRAY_MODULE_DEFLECT_LINK_LIBRARIES Deflect ${OSPRAY_LIBRARIES})

set(OSPRAY_MODULE_DEFLECT_SOURCES
  DeflectPixelOp.cpp
  ModuleInit.cpp
)

set(OSPRAY_MODULE_DEFLECT_HEADERS
  DeflectPixelOp.h
)

common_library(ospray_module_deflect)
