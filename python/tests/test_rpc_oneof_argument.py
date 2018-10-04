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

from nose.tools import assert_true
from mock import patch
import brayns

from .mocks import *


def test_rpc_one_of_parameter():
    with patch('rockets.AsyncClient.connected', new=mock_connected), \
         patch('brayns.utils.http_request', new=mock_http_request), \
         patch('rockets.Client.batch', new=mock_batch), \
         patch('rockets.Client.notify', new=mock_rpc_notify):
        app = brayns.Client('localhost:8200')
        import inspect
        assert_true(inspect.getdoc(app.set_camera).startswith(TEST_RPC_ONEOF_PARAMETER['description']))
        assert_true(hasattr(app, 'PerspectiveCamera'))
        param = app.PerspectiveCamera()
        param.fov = 10.2
        app.set_camera(param)


def test_rpc_one_of_parameter_weird_casings():
    with patch('rockets.AsyncClient.connected', new=mock_connected), \
         patch('brayns.utils.http_request', new=mock_http_request), \
         patch('rockets.Client.batch', new=mock_batch), \
         patch('rockets.Client.request', new=mock_rpc_request):
        app = brayns.Client('localhost:8200')
        assert_true(hasattr(app, 'StereofullMode'))
        assert_true(hasattr(app, 'MonoFullMode'))
        assert_true(hasattr(app, 'TruthfulMode'))
        assert_true(hasattr(app, 'MeaningFulMode'))


if __name__ == '__main__':
    import nose
    nose.run(defaultTest=__name__)
