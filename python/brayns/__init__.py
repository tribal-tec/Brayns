#!/usr/bin/env python
# -*- coding: utf-8 -*-
# pylint: disable=R0801,E1101

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
Python package allowing remote control of visualization applications through HTTP REST API.
"""

__all__ = ['Visualizer', 'SIMULATION_DEFAULT_RANGE', 'Brayns', 'Application']
from .version import VERSION
from .application import Application
from .visualizer import Visualizer
from .brayns import Brayns
from .settings import SIMULATION_DEFAULT_RANGE, DEFAULT_SAURON_URI
from .utils import inherit_docstring_from, simulation_control_points
