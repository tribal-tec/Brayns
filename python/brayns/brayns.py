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

from .visualizer import Visualizer
from .settings import DEFAULT_BRAYNS_UI_URI
from .utils import simulation_control_points


class Brayns(Visualizer):
    """
    Visualizer for large-scale and interactive ray-tracing of neurons
    """

    def set_colormap(self, colormap='magma', colormap_size=256, intensity=1, opacity=1,
                     data_range=(0, 256)):
        """
        Set a colormap to Brayns.
        :param colormap: color palette to use from matplotlib and seaborn
        :param colormap_size: the number of colors to use to control precision
        :param intensity: value to amplify the color values
        :param opacity: opacity for colormap values
        :param data_range: data range on which values the colormap should be applied
        """
        import seaborn as sns
        palette = sns.color_palette(colormap, colormap_size)
        palette_size = len(palette)
        contributions = []
        diffuses = []
        emissions = []
        tf = self.transfer_function
        for i in range(0, palette_size):
            color = palette[i]
            diffuses.append([intensity * color[0], intensity * color[1], intensity * color[2],
                             opacity])
            contributions.append(0)
        tf.diffuse = diffuses
        tf.emission = emissions
        tf.contribution = contributions
        tf.range = data_range
        tf.commit()

    # pylint: disable = R0914
    def set_simulation_colormap(self, colormap_size=1024, intensity=1, opacity=0.8,
                                data_range=(-80.0, -10.0), ambient_light=None):
        """
        Set the default simulation colormap to Brayns.
        :param colormap_size: the number of colors to use to control precision
        :param intensity: value to amplify the color values
        :param opacity: opacity for colormap values
        :param data_range: data range on which values the colormap should be applied
        :param ambient_light: ambient light as a base for the diffuse colors
        """
        if ambient_light is None:
            ambient_light = [0.25, 0.25, 0.25]

        x = []
        reds = []
        greens = []
        blues = []
        for p in simulation_control_points(colormap_size, data_range):
            x.append(p[0])
            reds.append(p[1][0])
            greens.append(p[1][1])
            blues.append(p[1][2])

        # pylint: disable = E0611
        from scipy.interpolate import interp1d

        # interpolate; default is linear
        reds = interp1d(x, reds)
        greens = interp1d(x, greens)
        blues = interp1d(x, blues)

        tf = self.transfer_function
        tf.contribution = []
        tf.diffuse = []
        tf.emission = []

        for i in range(colormap_size):
            light_intensity = 0
            if i > colormap_size * 3 / 4:
                light_intensity = 0.2 * float(i) / float(colormap_size)

            color = [ambient_light[0] + reds(i) * intensity,
                     ambient_light[1] + greens(i) * intensity,
                     ambient_light[2] + blues(i) * intensity,
                     opacity]

            tf.contribution.append(0)
            tf.diffuse.append(color)
            tf.emission.append([light_intensity] * 3)

        tf.range = data_range
        tf.commit()

    def open_ui(self):
        """
        Open the Brayns UI in a new page of the default system browser
        """
        import webbrowser
        url = DEFAULT_BRAYNS_UI_URI + '/?host=' + self.url()
        webbrowser.open(url)
