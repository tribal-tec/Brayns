# Copyright (c) 2015-2017, EPFL/Blue Brain Project
# All rights reserved. Do not distribute without permission.
# Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>

add_subdirectory(parameters)
add_subdirectory(common)
add_subdirectory(io)

set(BRAYNS_PUBLIC_HEADERS Brayns.h PluginAPI.h)

set(BRAYNS_SOURCES Brayns.cpp PluginAPI.cpp)

set(BRAYNS_LINK_LIBRARIES
    PUBLIC braynsParameters braynsCommon braynsIO braynsPlugins)

if(TARGET Lunchbox)
  list(APPEND BRAYNS_LINK_LIBRARIES PRIVATE Lunchbox)
endif()

common_library(brayns)


#set(BRAYNS_PLUGIN_API_PUBLIC_HEADERS PluginAPI.h)
#set(BRAYNS_PLUGIN_API_SOURCES PluginAPI.cpp)
#set(BRAYNS_PLUGIN_API_LINK_LIBRARIES PUBLIC brayns)
#common_library(braynsPluginAPI)
