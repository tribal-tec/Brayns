#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
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
The visualizer is the remote rendering resource in charge of rendering datasets
"""

import base64
import io

from PIL import Image
from .application import Application
from .utils import in_notebook


class Visualizer(Application):
    """
    The visualizer class provides specialization like widgets, image display and image streaming
    for applications that expose according APIs.
    """

    def __init__(self, resource=None):
        """
        Create a new visualizer instance by connecting to the given resource
        :param resource: can be a string 'hostname:port' to connect to a known application, a
                         ResourceAllocator instance or None for default allocation.
        """

        super(Visualizer, self).__init__(resource)

        if in_notebook():
            self._add_widgets()  # pragma: no cover

    # pylint: disable=W0613,W0622,E1101
    def image(self, size, format='jpg', quality=None, samples_per_pixel=None):
        """
        Requests a snapshot from the application and returns a PIL image
        :param size: tuple (width,height) for the resulting image
        :param format: image type as recognized by ImageMagick
        :param quality: compression quality between 1 (worst) and 100 (best)
        :param samples_per_pixel: samples per pixel to increase render quality
        :return: the PIL image of the current rendering, None on error obtaining the image
        """

        args = locals()
        del args['self']
        result = self.snapshot(response_timeout=None, **{k: v for k, v in args.items() if v})

        # error case: invalid request/parameters
        if 'code' in result:
            print(result['message'])
            return None

        # https://stackoverflow.com/a/9807138
        data = result['data']
        missing_padding = len(data) % 4
        if missing_padding != 0:
            data += b'=' * (4 - missing_padding)
        return Image.open(io.BytesIO(base64.b64decode(data)))

    def _add_widgets(self):  # pragma: no cover
        """ Add functions to the visualizer to provide widgets for appropriate properties """

        if self._ws_connected:
            self._add_show_function()

        if hasattr(self, 'animation_parameters'):
            self._add_simulation_slider()

    def _add_show_function(self):  # pragma: no cover
        """ Add show() function for live streaming """

        def function_builder():
            """ Wrapper for returning the visualizer.show() function """

            def show():
                """ Shows the live rendering of the application """

                self._setup_websocket()
                if self._ws_connected:
                    # pylint: disable=F0401,E1101
                    from IPython.display import display
                    import ipywidgets as widgets
                    image = widgets.Image(format='jpg')
                    image.value = base64.b64decode(self.image_jpeg()['data'])
                    display(image)

                    def image_update(data=None, close=False):
                        """
                        Update callback when we receive a new image or when the websocket was closed
                        """
                        if close:
                            image.close()
                        elif data is not None:
                            image.value = data

                    self._update_callback['image-jpeg'] = image_update
                    return

            return show

        setattr(self, 'show', function_builder())

    def _add_simulation_slider(self):  # pragma: no cover
        """ Add simulation_slider() function for animation_parameters control """

        def function_builder():
            """ Wrapper for returning the visualizer.simulation_slider() function """

            def simulation_slider():
                """ Show slider to control simulation """

                # pylint: disable=F0401,E1101
                import ipywidgets as widgets
                from IPython.display import display

                button = widgets.ToggleButton(value=self.animation_parameters.delta != 0,
                                              icon='play' if self.animation_parameters.delta == 0
                                              else 'pause')
                button.layout = widgets.Layout(width='40px')

                def on_button_change(change):
                    """ Callback after play/pause button update to send delta for animation """
                    button.icon = 'pause' if change['new'] else 'play'
                    self.set_animation_parameters(delta=1 if change['new'] else 0)
                button.observe(on_button_change, names='value')

                slider = widgets.IntSlider(min=self.animation_parameters.start,
                                           max=self.animation_parameters.end,
                                           value=self.animation_parameters.current)

                def on_value_change(change):
                    """ Callback after slider update to send current for animation """
                    self.set_animation_parameters(current=change['new'])
                slider.observe(on_value_change, names='value')

                w = widgets.HBox([button, slider])
                display(w)

                def slider_update(data=None, close=False):
                    """
                    Update callback when we receive animation_parameters update or when the
                    websocket was closed
                    """
                    if close:
                        w.close()
                    elif data is not None:
                        slider.unobserve(on_value_change, names='value')
                        slider.min = data.start
                        slider.max = data.end
                        slider.value = data.current
                        slider.observe(on_value_change, names='value')

                self._update_callback['animation-parameters'] = slider_update

            return simulation_slider

        setattr(self, 'simulation_slider', function_builder())

    def _add_show_progress(self):  # pragma: no cover
        """ Add show_progress() function for lexis/data/progress display """

        def function_builder():
            """ Wrapper for returning the visualizer.show_progress() function """

            def show_progress():
                """ Show progress bar and message for current operation """

                # pylint: disable=F0401,E1101
                from ipywidgets import FloatProgress, Label, VBox
                from IPython.display import display

                progress = FloatProgress(min=0, max=1, value=self.progress.amount)
                label = Label(value=self.progress.operation)
                box = VBox([label, progress])
                display(box)

                def progress_update(data=None, close=False):
                    """
                    Update callback when we receive a progress update or when the websocket was
                    closed
                    """
                    if close:
                        box.close()
                    elif data is not None:
                        progress.value = data.amount
                        label.value = data.operation

                self._update_callback['progress'] = progress_update

            return show_progress

        setattr(self, 'show_progress', function_builder())
