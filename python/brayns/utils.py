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

"""
The visualizer is the remote rendering resource in charge of rendering datasets
"""

import sys
from collections import OrderedDict
import requests


HTTP_METHOD_PUT = 'PUT'
HTTP_METHOD_GET = 'GET'
HTTP_METHOD_DELETE = 'DELETE'
HTTP_METHOD_POST = 'POST'
HTTP_STATUS_OK = 200

HTTP_PREFIX = 'http://'
HTTPS_PREFIX = 'https://'
WS_PREFIX = 'ws://'
WSS_PREFIX = 'wss://'

WS_PATH = '/ws'


class Status(object):
    """
    Holds the execution status of an HTTP request
    """
    def __init__(self, code, contents):
        self.code = code
        self.contents = contents


# pylint: disable=R0912
def http_request(method, url, command, body=None, query_params=None):  # pragma: no cover
    """
    Perform http requests to the given URL and return the applications' response.

    :param method: the type of HTTP request, PUT or GET are supported
    :param url: the URL of the applications' http server
    :param body: optional body for PUT requests
    :param command: the type of HTTP command to be executed on the target app
    :param query_params: the query params to append to the request url
    :return: JSON-encoded response of the request
    """
    full_url = url
    request = None
    full_url = full_url + command
    try:
        if method == HTTP_METHOD_POST:
            if body == '':
                request = requests.post(full_url, params=query_params)
            else:
                request = requests.post(full_url, data=body, params=query_params)
        elif method == HTTP_METHOD_PUT:
            if body == '':
                request = requests.put(full_url, params=query_params)
            else:
                request = requests.put(full_url, data=body, params=query_params)
        elif method == HTTP_METHOD_GET:
            request = requests.get(full_url, params=query_params)
            if request.status_code == 502:
                raise requests.exceptions.ConnectionError('Bad Gateway 502')
        elif method == HTTP_METHOD_DELETE:
            if body == '':
                request = requests.delete(full_url, params=query_params)
            else:
                request = requests.delete(full_url, data=body, params=query_params)
        js = ''
        if request.content:
            if request.status_code == 200:
                js = request.json(object_pairs_hook=OrderedDict)
            else:
                js = request.text
        response = Status(request.status_code, js)
        request.close()
    except requests.exceptions.ConnectionError:
        raise Exception('ERROR: Failed to connect to Application, did you start it with the '
                        '--http-server command line option?')
    return response


def in_notebook():
    """
    Returns ``True`` if the module is running in IPython kernel,
    ``False`` if in IPython shell or other Python shell.
    """
    return 'ipykernel' in sys.modules


def simulation_control_points(colormap_size, data_range):
    """
    Return a dict of control points to create a default simulation colormap
    :param colormap_size: the number of colors to use to control precision
    :param data_range: data range on which values the colormap should be applied
    :return: dict of points
    """
    delta = colormap_size / (data_range[1] - data_range[0])
    points = dict()
    points[int((-80.0 - data_range[0]) * delta)] = [1.0, 0.0, 0.0, 0.0]
    points[int((-77.181205 - data_range[0]) * delta)] = [0.023529, 0.023529, 0.6549020, 0.05]
    points[int((-72.06669 - data_range[0]) * delta)] = [0.141176, 0.529412, 0.9607843, 0.16]
    points[int((-70.2 - data_range[0]) * delta)] = [0.388235, 0.345098, 0.7137255, 0.22]
    points[int((-67.4 - data_range[0]) * delta)] = [0.960784, 0.0, 0.0196078, 0.3]
    points[int((-50.67785 - data_range[0]) * delta)] = [0.858824, 0.674510, 0.0000000, 0.4]
    points[int((-31.47 - data_range[0]) * delta)] = [0.964706, 1.000000, 0.6313725, 0.8]
    points[int((-10.0 - data_range[0]) * delta)] = [1.0, 1.0, 1.0, 1.0]
    return sorted(points.items())


def set_http_protocol(url):
    """
    Sets the http protocol to the url if it is not present. If not protocol is specified in the url,
     http is used
    :param url: Url to be checked
    :return: url with protocol
    """
    if url.find(HTTP_PREFIX) == -1 and url.find(HTTPS_PREFIX) == -1:
        return HTTP_PREFIX + url
    return url


def set_ws_protocol(url):
    """
    Sets the WebSocket protocol according to the resource url
    :return: ws for http, wss for https
    """
    if url.find(HTTPS_PREFIX) != -1:
        return WSS_PREFIX + url[len(HTTPS_PREFIX):]
    return WS_PREFIX + url[len(HTTP_PREFIX):]
