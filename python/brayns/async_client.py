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

import asyncio
import rockets

from .api_generator import build_api
from .base import BaseClient
from .utils import obtain_registry, convert_snapshot_response_to_PIL, SCHEMA_ENDPOINT
from . import utils


class aobject(object):
    """Inheriting from this class allows to define an async __init__."""

    async def __new__(cls, *a, **kw):
        """Allow to create objects by calling `await MyClass(params)`"""
        instance = super().__new__(cls)
        await instance.__init__(*a, **kw)
        return instance


class AsyncClient(BaseClient, aobject):
    """Client that connects to a remote running Brayns instance which provides the supported API."""

    async def __init__(self, url, loop=None):
        """
        Create a new client instance by connecting to the given URL.

        :param str url: a string 'hostname:port' to connect to a running Brayns instance
        :param asyncio.AbstractEventLoop loop: Event loop where this client should run in
        """
        super().__init__(url)

        self.rockets_client = rockets.AsyncClient(url, subprotocols=['rockets'], loop=loop)
        await self._build_api()

        super()._setup_notifications()

    async def _build_api(self):
        """Fetch the registry and all schemas from the remote running Brayns to build the API."""
        registry = obtain_registry(self.http_url)
        endpoints = {x.replace(SCHEMA_ENDPOINT, '') for x in registry}

        # batch request all schemas from all endpoints
        requests = list()
        for endpoint in endpoints:
            requests.append(rockets.Request('schema', {'endpoint': endpoint}))
        schemas = await self.rockets_client.batch(requests)

        schemas_dict = dict()
        for request in requests:
            # pylint: disable=protected-access,cell-var-from-loop
            response = list(filter(lambda x, request_id=request.request_id(): x._id == request_id,
                                   schemas))
            # pylint: enable=protected-access,cell-var-from-loop
            schemas_dict[request.params['endpoint']] = response[0].result
        build_api(self, registry, schemas_dict)

    # pylint: disable=W0613,W0622,E1101
    async def image(self, size, format='jpg', animation_parameters=None, camera=None, quality=None,
                    renderer=None, samples_per_pixel=None):
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
        result = self.snapshot(**{k: v for k, v in args.items() if v})

        future = asyncio.get_event_loop().create_future()

        def _on_done(task):
            try:
                if task.exception():  # pragma: no cover
                    print("image() failed:", task.exception())
                else:
                    image = convert_snapshot_response_to_PIL(task.result())
                    if utils.in_notebook():  # pragma: no cover
                        if image:
                            from IPython.display import display
                            display(image)
                    else:
                        future.set_result(image)
            except rockets.RequestError as e:  # pragma: no cover
                print("Error", e.code, e.message)
            except ConnectionRefusedError as e:  # pragma: no cover
                print(e)

        result.add_done_callback(_on_done)
        if utils.in_notebook():  # pragma: no cover
            return None
        return future
