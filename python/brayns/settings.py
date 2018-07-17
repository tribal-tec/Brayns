#!/usr/bin/env python
# -*- coding: utf-8 -*-
# pylint: disable=R0801

# Copyright (c) 2016-2018, Blue Brain Project
#                          Raphael Dumusc <raphael.dumusc@epfl.ch>
#                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
#                          Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3.0 as published
# by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
# All rights reserved. Do not distribute without further notice.

"""
Settings and default values for the viztools
"""

import getpass

DEFAULT_ALLOCATOR_URI = 'https://visualization-dev.humanbrainproject.eu/viz'
DEFAULT_SAURON_URI = 'http://brayns.ocp.bbp.epfl.ch'

DEFAULT_ALLOCATOR_API_VERSION = 'v1'
DEFAULT_ALLOCATOR_NAME = 'rendering-resource-manager'
DEFAULT_ALLOCATOR_SESSION_PREFIX = 'session'
DEFAULT_ALLOCATOR_CONFIG_PREFIX = 'config'
DEFAULT_ALLOCATOR_NB_NODES = 1
DEFAULT_ALLOCATOR_NB_CPUS = 1
DEFAULT_ALLOCATOR_NB_GPUS = 0
DEFAULT_ALLOCATOR_TIME = '0:05:00'
DEFAULT_ALLOCATOR_EXCLUSIVE = False

DEFAULT_RENDERER = 'brayns_generic'
DEFAULT_APPLICATION = 'viztools_any'
DEFAULT_SESSION = getpass.getuser()

SESSION_MAX_CONNECTION_ATTEMPTS = 5

SESSION_ID_QUERY_PARAM_NAME = 'session_id'

SIMULATION_DEFAULT_RANGE = [-92.0915, 49.5497]
