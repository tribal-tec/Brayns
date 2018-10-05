#!/usr/bin/env python
# -*- coding: utf-8 -*-

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

"""Client that connects to a remote running Brayns instance which provides the supported API."""

import base64
import io

from PIL import Image
import rockets

from .api_generator import build_api
from .utils import in_notebook, set_http_protocol, HTTP_METHOD_GET, HTTP_STATUS_OK, SCHEMA_ENDPOINT
from .version import MINIMAL_VERSION
from . import utils


def _obtain_registry(url):
    """Obtain the registry of exposed objects and RPCs from Brayns."""
    status = utils.http_request(HTTP_METHOD_GET, url, 'registry')
    if status.code != HTTP_STATUS_OK:
        raise Exception('Failed to obtain registry from Brayns')
    return status.contents


class Client(rockets.Client):
    """Client that connects to a remote running Brayns instance which provides the supported API."""

    def __init__(self, url, loop=None):
        """
        Create a new client instance by connecting to the given URL.

        :param str url: a string 'hostname:port' to connect to a running brayns instance
        """
        self._http_url = set_http_protocol(url) + '/'
        super(Client, self).__init__(url, subprotocols=['rockets'], loop=loop)
        self._check_version()
        self._build_api()

        if in_notebook():
            self._add_widgets()  # pragma: no cover

    def http_url(self):
        """Return the HTTP URL"""
        return self._http_url

    def __str__(self):
        """Return a pretty-print on the currently connected Brayns instance."""
        # pylint: disable=E1101
        version = 'unknown'
        if self.version:
            version = '.'.join(str(x) for x in [self.version.major, self.version.minor,
                                                self.version.patch])
        return "Brayns version {0} running on {1}".format(version, self.http_url())

    # pylint: disable=W0613,W0622,E1101
    def image(self, size, format='jpg', animation_parameters=None, camera=None, quality=None,
              renderer=None, samples_per_pixel=None, call_async=False):
        """
        Request a snapshot from Brayns and return a PIL image.

        :param tuple size: (width,height) of the resulting image
        :param str format: image type as recognized by FreeImage
        :param object animation_parameters: animation params to use instead of current params
        :param object camera: camera to use instead of current camera
        :param int quality: compression quality between 1 (worst) and 100 (best)
        :param object renderer: renderer to use instead of current renderer
        :param int samples_per_pixel: samples per pixel to increase render quality
        :return: the PIL image of the current rendering, None on error obtaining the image
        :rtype: :py:class:`~PIL.Image.Image`
        """
        args = locals()
        del args['self']
        result = self.snapshot(response_timeout=None,
                               **{k: v for k, v in args.items() if v})

        if call_async:
            from ipywidgets import FloatProgress, Label, HBox, VBox, Button
            from IPython.display import display

            progress = FloatProgress(min=0, max=1, value=0)
            label = Label(value='')
            button = Button(description='Cancel')
            box = VBox([label, HBox([progress, button])])
            display(box)

            def _on_progress(value):
                progress.value = value.amount
                label.value = value.operation

            def show_image(data):
                if not data:
                    return
                if data and 'data' in data:
                    data = data['data']
                missing_padding = len(data) % 4
                if missing_padding != 0:
                    data += b'=' * (4 - missing_padding)
                from IPython.display import display
                import ipywidgets as widgets
                image = widgets.Image(format='jpg')
                image.value = base64.b64decode(data)
                display(image)

            def _on_done(task):
                try:
                    show_image(task.result())
                except rockets.RequestError as e:
                    print("Error", e.code, e.message)
                except ConnectionRefusedError as e:
                    print(e)
                except Exception as e:
                    print(e)
                finally:
                    box.close()

            result.add_progress_callback(_on_progress)
            result.add_done_callback(_on_done)

            def _on_cancel(b):
                result.cancel()
            button.on_click(_on_cancel)

            return

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

    def set_colormap(self, colormap='magma', colormap_size=256, intensity=1, opacity=1,
                     data_range=(0, 256)):
        """
        Set a colormap to Brayns.

        :param str colormap: color palette to use from matplotlib and seaborn
        :param int colormap_size: the number of colors to use to control precision
        :param int intensity: value to amplify the color values
        :param int opacity: opacity for colormap values
        :param typle data_range: data range on which values the colormap should be applied
        """
        import seaborn as sns
        palette = sns.color_palette(colormap, colormap_size)
        palette_size = len(palette)
        contributions = []
        diffuses = []
        # pylint: disable=E1101
        tf = self.transfer_function
        for i in range(0, palette_size):
            color = palette[i]
            diffuses.append([intensity * color[0], intensity * color[1], intensity * color[2],
                             opacity])
            contributions.append(0)
        tf.diffuse = diffuses
        tf.contribution = contributions
        tf.range = data_range
        tf.commit()

    def open_ui(self):
        """Open the Brayns UI in a new page of the default system browser."""
        import webbrowser
        url = 'https://bbp-brayns.epfl.ch?host=' + self.http_url()
        webbrowser.open(url)

    def _check_version(self):
        """Check if the Brayns' version is sufficient enough."""
        status = utils.http_request(HTTP_METHOD_GET, self.http_url(), 'version')
        if status.code != HTTP_STATUS_OK:
            raise Exception('Cannot obtain version from Brayns')

        version = '.'.join(str(x) for x in [status.contents['major'], status.contents['minor'],
                                            status.contents['patch']])

        import semver
        if semver.match(version, '<{0}'.format(MINIMAL_VERSION)):
            raise Exception('Brayns does not satisfy minimal required version; '
                            'needed {0}, got {1}'.format(MINIMAL_VERSION, version))

    def _build_api(self):
        """Fetch the registry and all schemas from the remote running Brayns to build the API."""
        registry = _obtain_registry(self.http_url())
        endpoints = {x.replace(SCHEMA_ENDPOINT, '') for x in registry}

        # batch request all schemas from all endpoints
        requests = list()
        for endpoint in endpoints:
            requests.append(rockets.Request('schema', {'endpoint': endpoint}))
        schemas = self.batch(requests)

        schemas_dict = dict()
        for request, schema in zip(requests, schemas):
            schemas_dict[request.params['endpoint']] = schema
        build_api(self, registry, schemas_dict)

    def _add_widgets(self):  # pragma: no cover
        """Add functions to the Brayns object to provide widgets for appropriate properties."""
        self._add_show_function()
        self._add_animation_slider()

    def _add_show_function(self):  # pragma: no cover
        """Add show() function for live streaming."""
        def function_builder():
            """Wrapper for returning the show() function."""
            def show():
                """Show the live rendering of Brayns."""
                from IPython.display import display
                import ipywidgets as widgets
                from rx import Observer
                import base64

                brayns = self

                class StreamObserver(Observer):
                    def __init__(self):
                        self.image = widgets.Image(format='jpg')
                        task = brayns.async_request('image-jpeg')

                        def _on_done(value):
                            self.image.value = base64.b64decode(value.result()['data'])
                            display(self.image)
                        task.add_done_callback(_on_done)

                    def on_next(self, value):
                        self.image.value = value

                    def on_completed(self):
                        self.image.close()

                    def on_error(self, error):
                        print("Error Occurred: {0}".format(error))
                        self.image.close()

                brayns.as_observable() \
                    .filter(lambda value: isinstance(value, (bytes, bytearray, memoryview))) \
                    .subscribe(StreamObserver())

            return show

        setattr(self, 'show', function_builder())

    def _add_animation_slider(self):  # pragma: no cover
        """Add animation_slider() function for animation_parameters control."""
        def function_builder():
            """Wrapper for returning the animation_slider() function."""
            def animation_slider():
                """.Show slider to control animation"""
                from IPython.display import display
                import ipywidgets as widgets
                from rx import Observer
                import base64

                brayns = self

                class StreamObserver(Observer):
                    def __init__(self):
                        self.button = widgets.ToggleButton(value=brayns.animation_parameters.delta != 0,
                                                        icon='play' if brayns.animation_parameters.delta == 0
                                                                    else 'pause')
                        self.button.layout = widgets.Layout(width='40px')

                        def on_button_change(change):
                            """Callback after play/pause button update to send delta for animation."""
                            self.button.icon = 'pause' if change['new'] else 'play'
                            brayns.set_animation_parameters(delta=1 if change['new'] else 0)

                        self.button.observe(on_button_change, names='value')

                        self.slider = widgets.IntSlider(min=brayns.animation_parameters.start,
                                                        max=brayns.animation_parameters.end,
                                                        value=brayns.animation_parameters.current)

                        def on_value_change(change):
                            """Callback after slider update to send current for animation."""
                            brayns.set_animation_parameters(current=change['new'])

                        self.value_change = on_value_change

                        self.slider.observe(self.value_change, names='value')

                        self.box = widgets.HBox([self.button, self.slider])
                        display(self.box)

                    def on_next(self, value):
                        self.slider.unobserve(self.value_change, names='value')
                        params = value['params']
                        self.slider.min = params['start']
                        self.slider.max = params['end']
                        self.slider.value = params['current']
                        self.slider.observe(self.value_change, names='value')

                    def on_completed(self):
                        self.box.close()

                    def on_error(self, error):
                        print("Error Occurred: {0}".format(error))
                        self.box.close()

                def _animation_filter(value):
                    return 'method' in value and value['method'] == 'set-animation-parameters'
                brayns.json_stream().filter(_animation_filter) \
                    .subscribe(StreamObserver())

            return animation_slider

        setattr(self, 'animation_slider', function_builder())
